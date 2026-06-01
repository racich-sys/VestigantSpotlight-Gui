param(
    [Parameter(Mandatory=$true)][string]$CaseRoot,
    [string[]]$Keywords = @(),
    [string]$KeywordFile = "",
    [string]$OutCsv = "",
    [int]$ContextChars = 120,
    [int]$MaxHitsPerFile = 0
)

$ErrorActionPreference = "Stop"

function Escape-Csv {
    param([AllowNull()][object]$Value)
    $s = if ($null -eq $Value) { "" } else { [string]$Value }
    if ($s.Contains('"') -or $s.Contains(',') -or $s.Contains("`n") -or $s.Contains("`r")) { return '"' + $s.Replace('"','""') + '"' }
    return $s
}

function Get-MatchContext {
    param([string]$Value, [string]$Needle, [int]$Width)
    if ([string]::IsNullOrEmpty($Value) -or [string]::IsNullOrEmpty($Needle)) { return "" }
    $idx = $Value.IndexOf($Needle, [System.StringComparison]::OrdinalIgnoreCase)
    if ($idx -lt 0) { return "" }
    $start = [Math]::Max(0, $idx - $Width)
    $len = [Math]::Min($Value.Length - $start, $Needle.Length + (2 * $Width))
    $ctx = $Value.Substring($start, $len)
    if ($start -gt 0) { $ctx = "..." + $ctx }
    if (($start + $len) -lt $Value.Length) { $ctx = $ctx + "..." }
    return $ctx
}

$terms = New-Object System.Collections.Generic.List[string]
foreach ($k in $Keywords) {
    if (![string]::IsNullOrWhiteSpace($k)) { $terms.Add($k.Trim()) | Out-Null }
}
if (![string]::IsNullOrWhiteSpace($KeywordFile)) {
    if (!(Test-Path -LiteralPath $KeywordFile -PathType Leaf)) { throw "Keyword file not found: $KeywordFile" }
    foreach ($line in Get-Content -LiteralPath $KeywordFile) {
        $v = ([string]$line).Trim()
        if ($v -and !$v.StartsWith('#')) { $terms.Add($v) | Out-Null }
    }
}
$terms = @($terms | Sort-Object -Unique)
if ($terms.Count -eq 0) { throw "No keywords supplied. Use -Keywords or -KeywordFile." }
if (!(Test-Path -LiteralPath $CaseRoot -PathType Container)) { throw "Case root not found: $CaseRoot" }
if ([string]::IsNullOrWhiteSpace($OutCsv)) { $OutCsv = Join-Path $CaseRoot "investigator_keyword_hits.csv" }

$csvFiles = New-Object System.Collections.Generic.List[System.IO.FileInfo]
$preferred = @(
    "exports\\ios_string_probe_values.csv",
    "exports\\ios_timeline_index_updates.csv",
    "exports\\artifact_dates_wide.csv",
    "exports\\timeline_events.csv",
    "exports\\artifacts.csv",
    "exports\\native_decode_attempts.csv",
    "exports\\native_decode_errors.csv",
    "store_inventory.csv",
    "store_selection.csv",
    "aff4_apfs_extracted_storev2_stage_files.csv",
    "aff4_apfs_spotlight_file_copy_out.csv",
    "aff4_apfs_spotlight_xattr_probe.csv"
)
foreach ($rel in $preferred) {
    $p = Join-Path $CaseRoot $rel
    if (Test-Path -LiteralPath $p -PathType Leaf) { $csvFiles.Add((Get-Item -LiteralPath $p)) | Out-Null }
}
$exportDir = Join-Path $CaseRoot "exports"
if (Test-Path -LiteralPath $exportDir -PathType Container) {
    Get-ChildItem -LiteralPath $exportDir -Filter *.csv -File -Recurse | ForEach-Object {
        if (!($csvFiles.FullName -contains $_.FullName)) { $csvFiles.Add($_) | Out-Null }
    }
}
Get-ChildItem -LiteralPath $CaseRoot -Filter *.csv -File | ForEach-Object {
    if (!($csvFiles.FullName -contains $_.FullName)) { $csvFiles.Add($_) | Out-Null }
}

$headers = @(
    "keyword","source_file","row_number","field_name","match_context","matched_value_sample",
    "source_id","store_guid","source_db","record_id","raw_record_id","raw_kv_id","inode_num","store_id","parent_inode_num",
    "file_name","full_path","content_type","last_updated_utc","residency_status"
)
$enc = New-Object System.Text.UTF8Encoding($false)
$writer = New-Object System.IO.StreamWriter($OutCsv, $false, $enc)
$totalHits = 0
try {
    $writer.WriteLine(($headers -join ','))
    foreach ($file in $csvFiles) {
        $fileHits = 0
        $relFile = try { [System.IO.Path]::GetRelativePath($CaseRoot, $file.FullName) } catch { $file.FullName }
        $rowNumber = 0
        try {
            Import-Csv -LiteralPath $file.FullName | ForEach-Object {
                $rowNumber++
                $row = $_
                foreach ($prop in $row.PSObject.Properties) {
                    $val = [string]$prop.Value
                    if ([string]::IsNullOrEmpty($val)) { continue }
                    foreach ($term in $terms) {
                        if ($val.IndexOf($term, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) { continue }
                        $ctx = Get-MatchContext -Value $val -Needle $term -Width $ContextChars
                        $sample = if ($val.Length -gt 500) { $val.Substring(0,500) + "..." } else { $val }
                        $residency = "UNRESOLVED_INSUFFICIENT_PROVENANCE"
                        if ($row.PSObject.Properties.Name -contains 'resident_file_status') { $residency = [string]$row.resident_file_status }
                        elseif ($row.PSObject.Properties.Name -contains 'existence_status') { $residency = [string]$row.existence_status }
                        elseif ($row.PSObject.Properties.Name -contains 'record_state') { $residency = [string]$row.record_state }
                        $values = @(
                            $term,$relFile,$rowNumber,$prop.Name,$ctx,$sample,
                            $row.source_id,$row.store_guid,$row.source_db,$row.record_id,$row.raw_record_id,$row.raw_kv_id,$row.inode_num,$row.store_id,$row.parent_inode_num,
                            $row.file_name,$row.full_path,$row.content_type,$row.last_updated_utc,$residency
                        )
                        $writer.WriteLine((($values | ForEach-Object { Escape-Csv $_ }) -join ','))
                        $totalHits++
                        $fileHits++
                        if ($MaxHitsPerFile -gt 0 -and $fileHits -ge $MaxHitsPerFile) { throw "__MAX_HITS_PER_FILE_REACHED__" }
                    }
                }
            }
        } catch {
            if ([string]$_.Exception.Message -ne "__MAX_HITS_PER_FILE_REACHED__") {
                Write-Warning "Keyword search skipped/partially read $($file.FullName): $($_.Exception.Message)"
            }
        }
    }
} finally {
    $writer.Close()
}

$summaryPath = [System.IO.Path]::ChangeExtension($OutCsv, ".summary.json")
[ordered]@{
    generated_utc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    case_root = $CaseRoot
    keyword_count = $terms.Count
    keywords = $terms
    searched_csv_count = $csvFiles.Count
    total_hits = $totalHits
    output_csv = $OutCsv
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "Keyword search hits written: $OutCsv"
Write-Host "Keyword search summary written: $summaryPath"
