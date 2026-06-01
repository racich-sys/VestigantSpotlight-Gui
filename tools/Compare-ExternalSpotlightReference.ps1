param(
    [Parameter(Mandatory=$true)][string]$CaseRoot,
    [Parameter(Mandatory=$true)][string]$ExternalSpotlightRoot,
    [string]$VestigantStagedRoot = "",
    [Alias("ExternalCompareOutRoot")][string]$CompareOutputRoot = "",
    [string]$OutputPrefix = "aff4_apfs_external_spotlight",
    [switch]$SkipHash,
    [int]$MaxManifestFiles = 200000,
    [int64]$MinimumFreeBytes = 268435456
)

# V0_8_59_HOTFIX: supports separate comparison output root and wrapper stale-script detection.
$ErrorActionPreference = "Stop"

if ($CaseRoot -eq "-CaseRoot" -or $ExternalSpotlightRoot -eq "-ExternalSpotlightRoot") {
    throw "Parameter binding failed: CaseRoot or ExternalSpotlightRoot was received as a literal parameter token. Use the V0_8_59 runner or direct positional invocation: Compare-ExternalSpotlightReference.ps1 <CaseRoot> <ExternalSpotlightRoot> <VestigantStagedRoot> <CompareOutputRoot>."
}

function Ensure-Directory {
    param([Parameter(Mandatory=$true)][string]$PathValue)
    New-Item -ItemType Directory -Force -Path $PathValue | Out-Null
    if (!(Test-Path -LiteralPath $PathValue)) { throw "Directory was not created: $PathValue" }
}

function Escape-Csv {
    param([AllowNull()][object]$Value)
    $s = if ($null -eq $Value) { "" } else { [string]$Value }
    if ($s.Contains('"') -or $s.Contains(',') -or $s.Contains("`n") -or $s.Contains("`r")) {
        return '"' + $s.Replace('"','""') + '"'
    }
    return $s
}

function Normalize-RelativePath {
    param([Parameter(Mandatory=$true)][string]$PathValue)
    return $PathValue.Replace('\', '/').TrimStart('/').ToLowerInvariant()
}

function Strip-FirstRelativeComponent {
    param([Parameter(Mandatory=$true)][string]$PathValue)
    $p = $PathValue.Replace('\','/').TrimStart('/')
    $idx = $p.IndexOf('/')
    if ($idx -lt 0) { return $p.ToLowerInvariant() }
    return $p.Substring($idx + 1).ToLowerInvariant()
}

function Resolve-ComparableSpotlightRoot {
    param([Parameter(Mandatory=$true)][string]$Root)
    $item = Get-Item -LiteralPath $Root -Force
    if (!$item.PSIsContainer) { throw "Spotlight comparison root must be a directory: $Root" }
    $candidates = @(
        (Join-Path $item.FullName "Store-V2"),
        (Join-Path (Join-Path $item.FullName ".Spotlight-V100") "Store-V2"),
        $item.FullName
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            $hasStore = @(Get-ChildItem -LiteralPath $candidate -Recurse -File -Filter "store.db" -ErrorAction SilentlyContinue | Select-Object -First 1)
            $hasDotStore = @(Get-ChildItem -LiteralPath $candidate -Recurse -File -Filter ".store.db" -ErrorAction SilentlyContinue | Select-Object -First 1)
            $hasDbStr = @(Get-ChildItem -LiteralPath $candidate -Recurse -File -Filter "dbStr-*" -ErrorAction SilentlyContinue | Select-Object -First 1)
            if ($hasStore.Count -gt 0 -or $hasDotStore.Count -gt 0 -or $hasDbStr.Count -gt 0) { return (Get-Item -LiteralPath $candidate -Force).FullName }
        }
    }
    return $item.FullName
}

function Make-Relative {
    param(
        [Parameter(Mandatory=$true)][string]$Root,
        [Parameter(Mandatory=$true)][string]$FullName
    )
    $rootFull = (Get-Item -LiteralPath $Root -Force).FullName.TrimEnd('\','/')
    if ($FullName.Length -le $rootFull.Length) { return "" }
    return $FullName.Substring($rootFull.Length).TrimStart('\','/')
}

function Get-FreeBytesForPath {
    param([Parameter(Mandatory=$true)][string]$PathValue)
    $root = [System.IO.Path]::GetPathRoot((Resolve-Path -LiteralPath $PathValue).Path)
    if ([string]::IsNullOrWhiteSpace($root) -or $root.StartsWith("\\")) { return [int64]::MaxValue }
    $driveName = $root.Substring(0,1)
    $drive = Get-PSDrive -Name $driveName -ErrorAction SilentlyContinue
    if ($null -eq $drive) { return [int64]::MaxValue }
    return [int64]$drive.Free
}

function Assert-OutputFreeSpace {
    param([Parameter(Mandatory=$true)][string]$OutputRoot)
    Ensure-Directory -PathValue $OutputRoot
    $freeBytes = Get-FreeBytesForPath -PathValue $OutputRoot
    if ($freeBytes -lt $MinimumFreeBytes) {
        throw "Comparison output root has insufficient free space. OutputRoot=$OutputRoot FreeBytes=$freeBytes MinimumFreeBytes=$MinimumFreeBytes. Use -CompareOutputRoot on a drive with more space."
    }
}

function New-CsvWriter {
    param([Parameter(Mandatory=$true)][string]$PathValue, [Parameter(Mandatory=$true)][string[]]$Headers)
    $parent = Split-Path -Parent $PathValue
    if ($parent) { Ensure-Directory -PathValue $parent }
    $enc = New-Object System.Text.UTF8Encoding($false)
    $writer = New-Object System.IO.StreamWriter($PathValue, $false, $enc)
    $writer.WriteLine(($Headers -join ','))
    return $writer
}

function Write-CsvRow {
    param([Parameter(Mandatory=$true)]$Writer, [Parameter(Mandatory=$true)][object[]]$Values)
    $Writer.WriteLine((($Values | ForEach-Object { Escape-Csv $_ }) -join ','))
}

function Build-Manifest {
    param(
        [Parameter(Mandatory=$true)][string]$Root,
        [Parameter(Mandatory=$true)][string]$Label,
        [Parameter(Mandatory=$true)][string]$OutputCsv
    )
    $rows = New-Object System.Collections.Generic.List[object]
    $headers = @("source_label","relative_path","normalized_relative_path","file_name","normalized_file_name","size_bytes","sha256","hash_status","absolute_path")
    $writer = New-CsvWriter -PathValue $OutputCsv -Headers $headers
    try {
        $files = Get-ChildItem -LiteralPath $Root -Recurse -File -ErrorAction SilentlyContinue | Sort-Object FullName | Select-Object -First $MaxManifestFiles
        foreach ($file in $files) {
            $rel = Make-Relative -Root $Root -FullName $file.FullName
            $hash = ""
            $hashStatus = "SKIPPED"
            if (!$SkipHash) {
                try {
                    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                    $hashStatus = "OK"
                } catch {
                    $hashStatus = "HASH_ERROR: $($_.Exception.Message)"
                }
            }
            $row = [pscustomobject]@{
                source_label = $Label
                relative_path = $rel.Replace('\\','/')
                normalized_relative_path = (Normalize-RelativePath $rel)
                file_name = $file.Name
                normalized_file_name = $file.Name.ToLowerInvariant()
                size_bytes = [int64]$file.Length
                sha256 = $hash
                hash_status = $hashStatus
                absolute_path = $file.FullName
            }
            $rows.Add($row) | Out-Null
            Write-CsvRow -Writer $writer -Values @($row.source_label,$row.relative_path,$row.normalized_relative_path,$row.file_name,$row.normalized_file_name,$row.size_bytes,$row.sha256,$row.hash_status,$row.absolute_path)
        }
    } finally {
        $writer.Close()
    }
    return $rows
}

function Add-ToMapList {
    param($Map, [string]$Key, $Value)
    if ([string]::IsNullOrWhiteSpace($Key)) { return }
    if (!$Map.ContainsKey($Key)) { $Map[$Key] = New-Object System.Collections.Generic.List[object] }
    $Map[$Key].Add($Value) | Out-Null
}

function Increment-Count {
    param($Map, [string]$Key)
    if (!$Map.ContainsKey($Key)) { $Map[$Key] = 0 }
    $Map[$Key] = [int]$Map[$Key] + 1
}

function Write-FileCompare {
    param($ExternalRows, $VestigantRows, [string]$OutputCsv)
    $vestByRel = @{}
    $vestByHash = @{}
    $vestByNameSize = @{}
    foreach ($v in $VestigantRows) {
        $vestByRel[$v.normalized_relative_path] = $v
        Add-ToMapList -Map $vestByHash -Key $v.sha256 -Value $v
        Add-ToMapList -Map $vestByNameSize -Key ($v.normalized_file_name + '|' + $v.size_bytes) -Value $v
    }
    $matchedVest = @{}
    $statusCounts = @{}
    $headers = @("status","reason","external_relative_path","vestigant_relative_path","external_file_name","vestigant_file_name","external_size","vestigant_size","external_sha256","vestigant_sha256","external_path","vestigant_path")
    $writer = New-CsvWriter -PathValue $OutputCsv -Headers $headers
    try {
        foreach ($e in $ExternalRows) {
            $status = "EXTERNAL_ONLY"
            $reason = "No matching Vestigant file by relative path, SHA256, or name+size."
            $v = $null
            if ($vestByRel.ContainsKey($e.normalized_relative_path)) {
                $v = $vestByRel[$e.normalized_relative_path]
                if ($e.sha256 -and $v.sha256 -and $e.sha256 -eq $v.sha256) { $status = "MATCH_RELATIVE_PATH_AND_HASH"; $reason = "Relative path and SHA256 match." }
                elseif ($e.size_bytes -eq $v.size_bytes) { $status = "MATCH_RELATIVE_PATH_SIZE_ONLY"; $reason = "Relative path and size match; hash differs or was skipped." }
                else { $status = "RELATIVE_PATH_SIZE_MISMATCH"; $reason = "Relative path matched but size differs." }
            } else {
                $strippedExternalRel = Strip-FirstRelativeComponent $e.relative_path
                if ($vestByRel.ContainsKey($strippedExternalRel)) {
                    $v = $vestByRel[$strippedExternalRel]
                    if ($e.sha256 -and $v.sha256 -and $e.sha256 -eq $v.sha256) { $status = "MATCH_UUID_STRIPPED_RELATIVE_PATH_AND_HASH"; $reason = "External Store-V2 group folder was stripped and remaining relative path plus SHA256 match." }
                    elseif ($e.size_bytes -eq $v.size_bytes) { $status = "MATCH_UUID_STRIPPED_RELATIVE_PATH_SIZE_ONLY"; $reason = "External Store-V2 group folder was stripped and remaining relative path plus size match; hash differs or was skipped." }
                    else { $status = "UUID_STRIPPED_RELATIVE_PATH_SIZE_MISMATCH"; $reason = "External Store-V2 group folder was stripped and remaining relative path matched, but size differs." }
                }
            }
            if ($status -eq "EXTERNAL_ONLY") {
                if ($e.sha256 -and $vestByHash.ContainsKey($e.sha256)) {
                    $v = $vestByHash[$e.sha256][0]
                    $status = "MATCH_HASH_DIFFERENT_PATH"
                    $reason = "SHA256 matched, but normalized relative paths differ. This is expected when Vestigant stages groups by APFS parent object ID."
                } else {
                    $k = $e.normalized_file_name + '|' + $e.size_bytes
                    if ($vestByNameSize.ContainsKey($k)) {
                        $v = $vestByNameSize[$k][0]
                        $status = "MATCH_NAME_SIZE_ONLY"
                        $reason = "File name and size matched; hash unavailable or different. Review before relying on this match."
                    }
                }
            }
            $vestRel = ""; $vestName = ""; $vestSize = ""; $vestHash = ""; $vestPath = ""
            if ($v) {
                $matchedVest[$v.absolute_path] = $true
                $vestRel = $v.relative_path
                $vestName = $v.file_name
                $vestSize = $v.size_bytes
                $vestHash = $v.sha256
                $vestPath = $v.absolute_path
            }
            Increment-Count -Map $statusCounts -Key $status
            Write-CsvRow -Writer $writer -Values @($status,$reason,$e.relative_path,$vestRel,$e.file_name,$vestName,$e.size_bytes,$vestSize,$e.sha256,$vestHash,$e.absolute_path,$vestPath)
        }
        foreach ($v in $VestigantRows) {
            if (!$matchedVest.ContainsKey($v.absolute_path)) {
                Increment-Count -Map $statusCounts -Key "VESTIGANT_ONLY"
                Write-CsvRow -Writer $writer -Values @("VESTIGANT_ONLY","No external reference file matched by relative path, SHA256, or name+size.","",$v.relative_path,"",$v.file_name,"",$v.size_bytes,"",$v.sha256,"",$v.absolute_path)
            }
        }
    } finally {
        $writer.Close()
    }
    return $statusCounts
}

function Group-StoreV2Files {
    param($Rows)
    $groups = @{}
    foreach ($r in $Rows) {
        $name = $r.file_name.ToLowerInvariant()
        if ($name -ne "store.db" -and $name -ne ".store.db" -and $name -notlike "dbstr-*" -and $name -notlike "dbhdr-*" -and $name -notlike "tmp.spotlight*") { continue }
        $parent = Split-Path -Parent $r.relative_path
        if ([string]::IsNullOrWhiteSpace($parent)) { $parent = "." }
        $key = Normalize-RelativePath $parent
        if (!$groups.ContainsKey($key)) {
            $groups[$key] = [pscustomobject]@{ group_key=$key; relative_path=$parent.Replace('\\','/'); files=New-Object System.Collections.Generic.List[object]; store_hash=""; store_count=0; component_count=0; total_bytes=[int64]0 }
        }
        $groups[$key].files.Add($r) | Out-Null
        $groups[$key].component_count++
        $groups[$key].total_bytes += [int64]$r.size_bytes
        if ($name -eq "store.db" -or $name -eq ".store.db") { $groups[$key].store_count++; if (!$groups[$key].store_hash -and $r.sha256) { $groups[$key].store_hash = $r.sha256 } }
    }
    return $groups
}

function Write-GroupCompare {
    param($ExternalRows, $VestigantRows, [string]$OutputCsv)
    $eg = Group-StoreV2Files -Rows $ExternalRows
    $vg = Group-StoreV2Files -Rows $VestigantRows
    $vgByStoreHash = @{}
    foreach ($k in $vg.Keys) { if ($vg[$k].store_hash) { $vgByStoreHash[$vg[$k].store_hash] = $vg[$k] } }
    $matchedVestGroups = @{}
    $statusCounts = @{}
    $headers = @("status","reason","external_group","vestigant_group","external_has_store_db","vestigant_has_store_db","external_component_count","vestigant_component_count","external_total_bytes","vestigant_total_bytes","external_store_sha256","vestigant_store_sha256")
    $writer = New-CsvWriter -PathValue $OutputCsv -Headers $headers
    try {
        foreach ($ek in $eg.Keys) {
            $e = $eg[$ek]
            $v = $null
            $status = "EXTERNAL_GROUP_ONLY"
            $reason = "No Vestigant group matched by store.db/.store.db SHA256 or group path."
            if ($e.store_hash -and $vgByStoreHash.ContainsKey($e.store_hash)) {
                $v = $vgByStoreHash[$e.store_hash]
                $matchedVestGroups[$v.group_key] = $true
                if ($e.component_count -eq $v.component_count -and $e.total_bytes -eq $v.total_bytes) { $status="GROUP_STORE_HASH_COMPONENTS_MATCH"; $reason="store.db/.store.db SHA256, component count, and total bytes match." }
                else { $status="GROUP_STORE_HASH_PARTIAL_COMPONENT_DIFF"; $reason="store.db/.store.db SHA256 matched, but component count or total bytes differ." }
            } elseif ($vg.ContainsKey($ek)) {
                $v = $vg[$ek]
                $matchedVestGroups[$v.group_key] = $true
                $status = "GROUP_PATH_MATCH"
                $reason = "Normalized group path matched."
            }
            $vestGroup = ""; $vestHasStore = $false; $vestComponentCount = 0; $vestTotalBytes = 0; $vestStoreHash = ""
            if ($v) {
                $vestGroup = $v.relative_path
                $vestHasStore = ($v.store_count -gt 0)
                $vestComponentCount = $v.component_count
                $vestTotalBytes = $v.total_bytes
                $vestStoreHash = $v.store_hash
            }
            Increment-Count -Map $statusCounts -Key $status
            Write-CsvRow -Writer $writer -Values @($status,$reason,$e.relative_path,$vestGroup,($e.store_count -gt 0),$vestHasStore,$e.component_count,$vestComponentCount,$e.total_bytes,$vestTotalBytes,$e.store_hash,$vestStoreHash)
        }
        foreach ($vk in $vg.Keys) {
            if (!$matchedVestGroups.ContainsKey($vk)) {
                $v = $vg[$vk]
                Increment-Count -Map $statusCounts -Key "VESTIGANT_GROUP_ONLY"
                Write-CsvRow -Writer $writer -Values @("VESTIGANT_GROUP_ONLY","No external reference group matched by store.db/.store.db SHA256 or group path.","",$v.relative_path,$false,($v.store_count -gt 0),0,$v.component_count,0,$v.total_bytes,"",$v.store_hash)
            }
        }
    } finally {
        $writer.Close()
    }
    $statusCounts["__row_count"] = [int]($statusCounts.Values | Measure-Object -Sum).Sum
    return $statusCounts
}

function Get-CountValue {
    param($Map, [string]$Key)
    if ($Map.ContainsKey($Key)) { return [int]$Map[$Key] }
    return 0
}

function Convert-StatusCountsToJson {
    param($Map, [string[]]$SkipKeys = @())
    $parts = New-Object System.Collections.Generic.List[string]
    foreach ($k in ($Map.Keys | Sort-Object)) {
        if ($SkipKeys -contains $k) { continue }
        $parts.Add('    "' + $k + '": ' + ([int]$Map[$k])) | Out-Null
    }
    return ($parts -join ",`n")
}

if (!(Test-Path -LiteralPath $CaseRoot)) { throw "Case root not found: $CaseRoot" }
if (!(Test-Path -LiteralPath $ExternalSpotlightRoot)) { throw "External Spotlight root not found: $ExternalSpotlightRoot" }
if ([string]::IsNullOrWhiteSpace($VestigantStagedRoot)) { $VestigantStagedRoot = Join-Path $CaseRoot "ExtractedSpotlight\StagedStoreV2" }
if (!(Test-Path -LiteralPath $VestigantStagedRoot)) { throw "Vestigant staged Store-V2 root not found: $VestigantStagedRoot" }
if ([string]::IsNullOrWhiteSpace($CompareOutputRoot)) { $CompareOutputRoot = $CaseRoot }
Assert-OutputFreeSpace -OutputRoot $CompareOutputRoot

$externalComparableRoot = Resolve-ComparableSpotlightRoot -Root $ExternalSpotlightRoot
$vestigantComparableRoot = (Get-Item -LiteralPath $VestigantStagedRoot -Force).FullName
$compareOutputRootFull = (Get-Item -LiteralPath $CompareOutputRoot -Force).FullName

$externalManifestCsv = Join-Path $compareOutputRootFull ($OutputPrefix + "_external_manifest.csv")
$vestigantManifestCsv = Join-Path $compareOutputRootFull ($OutputPrefix + "_vestigant_manifest.csv")
$fileCompareCsv = Join-Path $compareOutputRootFull ($OutputPrefix + "_file_compare.csv")
$groupCompareCsv = Join-Path $compareOutputRootFull ($OutputPrefix + "_storev2_group_compare.csv")
$summaryJson = Join-Path $compareOutputRootFull ($OutputPrefix + "_compare_summary.json")
$mdPath = Join-Path $compareOutputRootFull "AFF4_APFS_EXTERNAL_SPOTLIGHT_COMPARE.md"

$externalRows = Build-Manifest -Root $externalComparableRoot -Label "external_reference" -OutputCsv $externalManifestCsv
$vestigantRows = Build-Manifest -Root $vestigantComparableRoot -Label "vestigant_aff4_apfs_staged" -OutputCsv $vestigantManifestCsv
$fileStatusCounts = Write-FileCompare -ExternalRows $externalRows -VestigantRows $vestigantRows -OutputCsv $fileCompareCsv
$groupStatusCounts = Write-GroupCompare -ExternalRows $externalRows -VestigantRows $vestigantRows -OutputCsv $groupCompareCsv

$matchCount = 0
foreach ($k in $fileStatusCounts.Keys) { if ($k -like "MATCH*") { $matchCount += [int]$fileStatusCounts[$k] } }
$totalFileRows = [int]($fileStatusCounts.Values | Measure-Object -Sum).Sum
$mismatchCount = $totalFileRows - $matchCount

$fileStatusJson = Convert-StatusCountsToJson -Map $fileStatusCounts
$groupStatusJson = Convert-StatusCountsToJson -Map $groupStatusCounts -SkipKeys @("__row_count")

@"
{
  "generated_utc": "$((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))",
  "case_root": "$(($CaseRoot).Replace('\','\\'))",
  "compare_output_root": "$(($compareOutputRootFull).Replace('\','\\'))",
  "external_spotlight_root_input": "$(($ExternalSpotlightRoot).Replace('\','\\'))",
  "external_comparable_root": "$(($externalComparableRoot).Replace('\','\\'))",
  "vestigant_comparable_root": "$(($vestigantComparableRoot).Replace('\','\\'))",
  "hashing_enabled": $((!$SkipHash).ToString().ToLowerInvariant()),
  "external_file_count": $($externalRows.Count),
  "vestigant_file_count": $($vestigantRows.Count),
  "file_compare_rows": $totalFileRows,
  "file_match_rows": $matchCount,
  "file_nonmatch_rows": $mismatchCount,
  "external_only_rows": $(Get-CountValue $fileStatusCounts "EXTERNAL_ONLY"),
  "vestigant_only_rows": $(Get-CountValue $fileStatusCounts "VESTIGANT_ONLY"),
  "hash_different_path_rows": $(Get-CountValue $fileStatusCounts "MATCH_HASH_DIFFERENT_PATH"),
  "group_compare_rows": $(Get-CountValue $groupStatusCounts "__row_count"),
  "file_status_counts": {
$fileStatusJson
  },
  "group_status_counts": {
$groupStatusJson
  },
  "outputs": [
    "$(Split-Path -Leaf $externalManifestCsv)",
    "$(Split-Path -Leaf $vestigantManifestCsv)",
    "$(Split-Path -Leaf $fileCompareCsv)",
    "$(Split-Path -Leaf $groupCompareCsv)"
  ]
}
"@ | Set-Content -LiteralPath $summaryJson -Encoding UTF8

@"
# AFF4/APFS External Spotlight Comparison

Version scope: V0_8_59 comparison harness.

## Purpose

This test-only comparison uses a Spotlight folder copied by an external/reference tool as a validation reference and compares it to the Spotlight files staged by Vestigant's AFF4/APFS extraction path.

## Inputs

- External input: $ExternalSpotlightRoot
- External comparable root: $externalComparableRoot
- Vestigant staged root: $vestigantComparableRoot
- Comparison output root: $compareOutputRootFull
- Hashing enabled: $((!$SkipHash).ToString())

## Summary

- External files: $($externalRows.Count)
- Vestigant files: $($vestigantRows.Count)
- File compare rows: $totalFileRows
- File match rows: $matchCount
- File nonmatch rows: $mismatchCount
- External-only rows: $(Get-CountValue $fileStatusCounts "EXTERNAL_ONLY")
- Vestigant-only rows: $(Get-CountValue $fileStatusCounts "VESTIGANT_ONLY")
- Hash matches with different paths: $(Get-CountValue $fileStatusCounts "MATCH_HASH_DIFFERENT_PATH")
- Group compare rows: $(Get-CountValue $groupStatusCounts "__row_count")

## Outputs

- $(Split-Path -Leaf $externalManifestCsv)
- $(Split-Path -Leaf $vestigantManifestCsv)
- $(Split-Path -Leaf $fileCompareCsv)
- $(Split-Path -Leaf $groupCompareCsv)
- $(Split-Path -Leaf $summaryJson)

## Interpretation

MATCH_HASH_DIFFERENT_PATH is expected when Vestigant normalizes Store-V2 groups by APFS parent object ID rather than preserving the external tool's folder naming. EXTERNAL_ONLY rows identify files present in the reference copy that were not matched in Vestigant output by relative path, SHA256, or file name plus size. This is a validation comparison, not a deleted/missing-file classification.
"@ | Set-Content -LiteralPath $mdPath -Encoding UTF8

Write-Host "External Spotlight comparison written: $summaryJson"
Get-Item -LiteralPath $summaryJson | Select-Object FullName, Length
