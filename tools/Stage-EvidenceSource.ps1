<#
Vestigant Spotlight V0_7_13 evidence source stager.

Purpose:
- Stage direct folders and ZIP evidence sources into a controlled working evidence folder.
- Hash file-based evidence sources using SHA256.
- Create deterministic manifests for folder/staged evidence.
- Detect Store-V2 and likely iOS CoreSpotlight locations without forcing parser routing.
- Write JSON, CSV, and SQL inventory outputs that can be imported into a case database.

This is intentionally conservative. IMG/DD/AFF4 are registered as unsupported source types in this build.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath,

    [Parameter(Mandatory=$true)]
    [string]$CaseOut,

    [string]$EvidenceSourceId,

    [switch]$NoCopyFolder,

    [switch]$Force
)

$ErrorActionPreference = "Stop"

function New-SafeId {
    $bytes = New-Object byte[] 8
    $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create(); try { $rng.GetBytes($bytes) } finally { if ($rng) { $rng.Dispose() } }
    return "evidence_" + ([BitConverter]::ToString($bytes).Replace("-", "").ToLowerInvariant())
}

function ConvertTo-JsonSafeString([object]$Value) {
    if ($null -eq $Value) { return $null }
    return [string]$Value
}

function Get-FileSha256([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Escape-Csv([string]$Value) {
    if ($null -eq $Value) { return "" }
    $v = $Value.Replace('"','""')
    return '"' + $v + '"'
}

function SqlQuote([object]$Value) {
    if ($null -eq $Value) { return "NULL" }
    $s = [string]$Value
    return "'" + $s.Replace("'", "''") + "'"
}

function Write-Log([string]$Message) {
    $ts = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
    Write-Host "[$ts] $Message"
}

function New-Manifest([string]$Root, [string]$ManifestPath) {
    if (!(Test-Path -LiteralPath $Root -PathType Container)) {
        throw "Manifest root not found: $Root"
    }
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
    $rows = New-Object System.Collections.Generic.List[string]
    $rows.Add("relative_path,size_bytes,last_write_utc,sha256")

    $files = Get-ChildItem -LiteralPath $Root -Recurse -File -Force | Sort-Object FullName
    $count = 0
    foreach ($f in $files) {
        $full = [System.IO.Path]::GetFullPath($f.FullName)
        $rel = $full.Substring($rootFull.Length).TrimStart('\')
        $hash = Get-FileSha256 -Path $f.FullName
        $rows.Add((Escape-Csv $rel) + "," + $f.Length + "," + (Escape-Csv $f.LastWriteTimeUtc.ToString("yyyy-MM-ddTHH:mm:ss.fffffffZ")) + "," + $hash)
        $count++
    }

    $manifestDir = Split-Path -Parent $ManifestPath
    if (!(Test-Path -LiteralPath $manifestDir)) { New-Item -ItemType Directory -Path $manifestDir -Force | Out-Null }
    [System.IO.File]::WriteAllLines($ManifestPath, $rows, (New-Object System.Text.UTF8Encoding $false))
    $manifestHash = Get-FileSha256 -Path $ManifestPath
    return [pscustomobject]@{
        file_count = $count
        manifest_path = $ManifestPath
        manifest_sha256 = $manifestHash
    }
}

function Detect-SourceType([string]$ResolvedPath) {
    if (Test-Path -LiteralPath $ResolvedPath -PathType Container) { return "folder" }
    $ext = [System.IO.Path]::GetExtension($ResolvedPath).ToLowerInvariant()
    switch ($ext) {
        ".zip" { return "zip" }
        ".img" { return "flat_image_unsupported" }
        ".dd" { return "flat_image_unsupported" }
        ".aff4" { return "aff4_unsupported" }
        default { return "file_unsupported" }
    }
}

function Find-StoreV2([string]$Root) {
    if (!(Test-Path -LiteralPath $Root -PathType Container)) { return @() }
    return @(Get-ChildItem -LiteralPath $Root -Directory -Recurse -Force -ErrorAction SilentlyContinue | Where-Object { $_.Name -ieq "Store-V2" } | Select-Object -ExpandProperty FullName)
}

function Find-CoreSpotlightCandidates([string]$Root) {
    if (!(Test-Path -LiteralPath $Root -PathType Container)) { return @() }
    $candidates = New-Object System.Collections.Generic.List[string]

    Get-ChildItem -LiteralPath $Root -Directory -Recurse -Force -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "(?i)CoreSpotlight|corespotlight|com\.apple\.corespotlight" } |
        ForEach-Object { $candidates.Add($_.FullName) }

    Get-ChildItem -LiteralPath $Root -File -Recurse -Force -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ieq "index.db" -or $_.FullName -match "(?i)CoreSpotlight|corespotlight|com\.apple\.corespotlight" } |
        ForEach-Object { $candidates.Add($_.FullName) }

    return @($candidates | Sort-Object -Unique)
}

$resolved = Resolve-Path -LiteralPath $SourcePath -ErrorAction Stop
$sourceFull = $resolved.Path
if (!(Test-Path -LiteralPath $CaseOut)) { New-Item -ItemType Directory -Path $CaseOut -Force | Out-Null }

if ([string]::IsNullOrWhiteSpace($EvidenceSourceId)) { $EvidenceSourceId = New-SafeId }
$stageRoot = Join-Path $CaseOut ("EvidenceStaging\" + $EvidenceSourceId)
$inventoryRoot = Join-Path $CaseOut "EvidenceInventory"

if ((Test-Path -LiteralPath $stageRoot) -and !$Force) {
    throw "Stage root already exists. Use -Force to replace: $stageRoot"
}
if (Test-Path -LiteralPath $stageRoot) { Remove-Item -LiteralPath $stageRoot -Recurse -Force }
New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null
New-Item -ItemType Directory -Path $inventoryRoot -Force | Out-Null

$sourceType = Detect-SourceType -ResolvedPath $sourceFull
$sourceItem = Get-Item -LiteralPath $sourceFull -Force
$originalSha256 = if ($sourceItem.PSIsContainer) { $null } else { Get-FileSha256 -Path $sourceFull }
$originalSize = if ($sourceItem.PSIsContainer) { $null } else { $sourceItem.Length }
$startedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")

Write-Log "Source: $sourceFull"
Write-Log "Type: $sourceType"
Write-Log "Stage root: $stageRoot"

$normalizedRoot = $null
$sourceNotes = ""
$unsupportedReason = $null

switch ($sourceType) {
    "folder" {
        if ($NoCopyFolder) {
            $normalizedRoot = $sourceFull
            $sourceNotes = "Folder source referenced directly because -NoCopyFolder was set. Parser should preserve original provenance and avoid modifying source."
        } else {
            $normalizedRoot = Join-Path $stageRoot "folder_copy"
            New-Item -ItemType Directory -Path $normalizedRoot -Force | Out-Null
            Write-Log "Copying folder source into controlled staging folder"
            $robocopyArgs = @($sourceFull, $normalizedRoot, "/E", "/COPY:DAT", "/R:1", "/W:1", "/NFL", "/NDL", "/NP")
            & robocopy @robocopyArgs | Out-Host
            $robocopyExitCode = $LASTEXITCODE
            if ($robocopyExitCode -gt 7) {
                throw "Robocopy failed with exit code $robocopyExitCode using compatibility options: /E /COPY:DAT /R:1 /W:1 /NFL /NDL /NP"
            }
            $sourceNotes = "Folder source copied into controlled staging folder."
        }
    }
    "zip" {
        $normalizedRoot = Join-Path $stageRoot "extracted"
        New-Item -ItemType Directory -Path $normalizedRoot -Force | Out-Null
        Write-Log "Extracting ZIP into controlled staging folder"
        Expand-Archive -LiteralPath $sourceFull -DestinationPath $normalizedRoot -Force
        $sourceNotes = "ZIP extracted into controlled staging folder using PowerShell Expand-Archive."
    }
    "flat_image_unsupported" {
        $normalizedRoot = $null
        $unsupportedReason = "Flat IMG/DD filesystem extraction is intentionally not implemented in V0_7_13. Register source only."
        $sourceNotes = $unsupportedReason
    }
    "aff4_unsupported" {
        $normalizedRoot = $null
        $unsupportedReason = "AFF4 extraction is intentionally not implemented in V0_7_13. Register source only."
        $sourceNotes = $unsupportedReason
    }
    default {
        $normalizedRoot = $null
        $unsupportedReason = "Unsupported source type in V0_7_13. Register source only."
        $sourceNotes = $unsupportedReason
    }
}

$manifest = $null
$sourceManifest = $null
if ($sourceType -eq "folder") {
    $sourceManifestPath = Join-Path $inventoryRoot ($EvidenceSourceId + "_original_folder_manifest.csv")
    Write-Log "Creating original folder manifest"
    $sourceManifest = New-Manifest -Root $sourceFull -ManifestPath $sourceManifestPath
}
if ($normalizedRoot -and (Test-Path -LiteralPath $normalizedRoot -PathType Container)) {
    $manifestPath = Join-Path $inventoryRoot ($EvidenceSourceId + "_staged_manifest.csv")
    Write-Log "Creating staged evidence manifest"
    $manifest = New-Manifest -Root $normalizedRoot -ManifestPath $manifestPath
}

$storeV2 = if ($normalizedRoot) { Find-StoreV2 -Root $normalizedRoot } else { @() }
$coreSpotlight = if ($normalizedRoot) { Find-CoreSpotlightCandidates -Root $normalizedRoot } else { @() }

$parserRoute = "unknown"
if ($storeV2.Count -gt 0) { $parserRoute = "mac_store_v2_candidate" }
elseif ($coreSpotlight.Count -gt 0) { $parserRoute = "ios_corespotlight_candidate" }
elseif ($unsupportedReason) { $parserRoute = "unsupported_registered_only" }
else { $parserRoute = "no_supported_spotlight_artifacts_detected" }

$completedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")

$inventory = [ordered]@{
    evidence_source_id = $EvidenceSourceId
    source_type = $sourceType
    original_path = $sourceFull
    original_size = $originalSize
    original_sha256 = $originalSha256
    directory_manifest_path = if ($sourceManifest) { $sourceManifest.manifest_path } else { $null }
    directory_manifest_sha256 = if ($sourceManifest) { $sourceManifest.manifest_sha256 } else { $null }
    directory_manifest_file_count = if ($sourceManifest) { $sourceManifest.file_count } else { $null }
    container_member_path = $null
    extracted_path = $normalizedRoot
    extracted_sha256 = if ($manifest) { $manifest.manifest_sha256 } else { $null }
    extracted_manifest_path = if ($manifest) { $manifest.manifest_path } else { $null }
    extracted_manifest_file_count = if ($manifest) { $manifest.file_count } else { $null }
    parser_route = $parserRoute
    source_notes = $sourceNotes
    unsupported_reason = $unsupportedReason
    detected_store_v2_paths = $storeV2
    detected_corespotlight_candidates = $coreSpotlight
    started_utc = $startedUtc
    completed_utc = $completedUtc
}

$jsonPath = Join-Path $inventoryRoot ($EvidenceSourceId + "_source_inventory.json")
$csvPath = Join-Path $inventoryRoot ($EvidenceSourceId + "_source_inventory.csv")
$sqlPath = Join-Path $inventoryRoot ($EvidenceSourceId + "_source_inventory.sql")

($inventory | ConvertTo-Json -Depth 8) | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$csvHeader = "evidence_source_id,source_type,original_path,original_size,original_sha256,directory_manifest_sha256,container_member_path,extracted_path,extracted_sha256,parser_route,source_notes,unsupported_reason,detected_store_v2_count,detected_corespotlight_count,started_utc,completed_utc"
$csvRow = @(
    Escape-Csv $EvidenceSourceId,
    Escape-Csv $sourceType,
    Escape-Csv $sourceFull,
    $originalSize,
    Escape-Csv $originalSha256,
    Escape-Csv $(if ($sourceManifest) { $sourceManifest.manifest_sha256 } else { $null }),
    Escape-Csv $null,
    Escape-Csv $normalizedRoot,
    Escape-Csv $(if ($manifest) { $manifest.manifest_sha256 } else { $null }),
    Escape-Csv $parserRoute,
    Escape-Csv $sourceNotes,
    Escape-Csv $unsupportedReason,
    $storeV2.Count,
    $coreSpotlight.Count,
    Escape-Csv $startedUtc,
    Escape-Csv $completedUtc
) -join ","
Set-Content -LiteralPath $csvPath -Value ($csvHeader + [Environment]::NewLine + $csvRow) -Encoding UTF8

$sql = @"
CREATE TABLE IF NOT EXISTS evidence_sources (
    evidence_source_id TEXT PRIMARY KEY,
    source_type TEXT,
    original_path TEXT,
    original_size INTEGER,
    original_sha256 TEXT,
    directory_manifest_path TEXT,
    directory_manifest_sha256 TEXT,
    directory_manifest_file_count INTEGER,
    container_member_path TEXT,
    extracted_path TEXT,
    extracted_sha256 TEXT,
    extracted_manifest_path TEXT,
    extracted_manifest_file_count INTEGER,
    parser_route TEXT,
    source_notes TEXT,
    unsupported_reason TEXT,
    detected_store_v2_count INTEGER,
    detected_corespotlight_count INTEGER,
    started_utc TEXT,
    completed_utc TEXT
);

INSERT OR REPLACE INTO evidence_sources (
    evidence_source_id, source_type, original_path, original_size, original_sha256,
    directory_manifest_path, directory_manifest_sha256, directory_manifest_file_count,
    container_member_path, extracted_path, extracted_sha256, extracted_manifest_path,
    extracted_manifest_file_count, parser_route, source_notes, unsupported_reason,
    detected_store_v2_count, detected_corespotlight_count, started_utc, completed_utc
) VALUES (
    $(SqlQuote $EvidenceSourceId),
    $(SqlQuote $sourceType),
    $(SqlQuote $sourceFull),
    $(if ($null -eq $originalSize) { "NULL" } else { $originalSize }),
    $(SqlQuote $originalSha256),
    $(SqlQuote $(if ($sourceManifest) { $sourceManifest.manifest_path } else { $null })),
    $(SqlQuote $(if ($sourceManifest) { $sourceManifest.manifest_sha256 } else { $null })),
    $(if ($sourceManifest) { $sourceManifest.file_count } else { "NULL" }),
    NULL,
    $(SqlQuote $normalizedRoot),
    $(SqlQuote $(if ($manifest) { $manifest.manifest_sha256 } else { $null })),
    $(SqlQuote $(if ($manifest) { $manifest.manifest_path } else { $null })),
    $(if ($manifest) { $manifest.file_count } else { "NULL" }),
    $(SqlQuote $parserRoute),
    $(SqlQuote $sourceNotes),
    $(SqlQuote $unsupportedReason),
    $($storeV2.Count),
    $($coreSpotlight.Count),
    $(SqlQuote $startedUtc),
    $(SqlQuote $completedUtc)
);
"@
Set-Content -LiteralPath $sqlPath -Value $sql -Encoding UTF8

$reportPath = Join-Path $inventoryRoot ($EvidenceSourceId + "_detection_report.txt")
@"
Vestigant Spotlight V0_7_13 Evidence Source Detection Report
Evidence Source ID: $EvidenceSourceId
Source Type: $sourceType
Original Path: $sourceFull
Original SHA256: $originalSha256
Directory Manifest SHA256: $(if ($sourceManifest) { $sourceManifest.manifest_sha256 } else { "" })
Staged Path: $normalizedRoot
Staged Manifest SHA256: $(if ($manifest) { $manifest.manifest_sha256 } else { "" })
Parser Route: $parserRoute
Unsupported Reason: $unsupportedReason

Detected Store-V2 paths:
$($storeV2 -join [Environment]::NewLine)

Detected iOS/CoreSpotlight candidates:
$($coreSpotlight -join [Environment]::NewLine)

Inventory JSON: $jsonPath
Inventory CSV: $csvPath
Inventory SQL: $sqlPath
"@ | Set-Content -LiteralPath $reportPath -Encoding UTF8

Write-Log "Inventory JSON: $jsonPath"
Write-Log "Inventory CSV:  $csvPath"
Write-Log "Inventory SQL:  $sqlPath"
Write-Log "Detection report: $reportPath"
Write-Log "Parser route: $parserRoute"

[pscustomobject]$inventory
