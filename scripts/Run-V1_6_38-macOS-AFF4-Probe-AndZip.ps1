param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_38",
  [string]$Aff4Path = "O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4",
  [string]$CaseRoot = "Q:\SpotlightCase\TestMacOS_AFF4_V1_6_38",
  [string]$ReaderToolsRoot = "T:\VestigantReaderTools\aff4-cpp-lite",
  [string]$ExternalSpotlightRoot = "T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2",
  [string]$ExternalCompareOutRoot = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_38_ExternalCompare",
  [string]$UploadWorkRoot = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_38_UploadWork",
  [string]$ZipPath = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_38.zip",
  [switch]$CleanOut,
  [switch]$DiagnosticOutputs,
  [switch]$ForceContainerHash,
  [int]$CliTimeoutMinutes = 90
)

$ErrorActionPreference = "Stop"

function Assert-PathExists {
  param(
    [Parameter(Mandatory=$true)][string]$PathValue,
    [Parameter(Mandatory=$true)][string]$Description,
    [switch]$Leaf
  )
  if ($Leaf) {
    if (!(Test-Path -LiteralPath $PathValue -PathType Leaf)) { throw "$Description not found: $PathValue" }
  } else {
    if (!(Test-Path -LiteralPath $PathValue)) { throw "$Description not found: $PathValue" }
  }
}

$Cli = Join-Path $SourceRoot "build-msvc\Release\VestigantSpotlightCli.exe"
$Runner = Join-Path $SourceRoot "tools\Run-SingleAff4SourceProbeAndZip.ps1"
Assert-PathExists -PathValue $Cli -Description "CLI binary" -Leaf
Assert-PathExists -PathValue $Runner -Description "Single AFF4 probe wrapper" -Leaf
Assert-PathExists -PathValue $Aff4Path -Description "AFF4 input" -Leaf
Assert-PathExists -PathValue $ReaderToolsRoot -Description "AFF4 reader tools folder"
Assert-PathExists -PathValue $ExternalSpotlightRoot -Description "External Store-V2 reference folder"

if ($CleanOut) {
  Remove-Item -LiteralPath $CaseRoot -Recurse -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $ZipPath -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $ExternalCompareOutRoot -Recurse -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $UploadWorkRoot -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Force -Path $CaseRoot | Out-Null

$runnerArgs = @(
  "-Aff4Input", $Aff4Path,
  "-Out", $CaseRoot,
  "-ReaderToolsRoot", $ReaderToolsRoot,
  "-ZipPath", $ZipPath,
  "-ExternalSpotlightRoot", $ExternalSpotlightRoot,
  "-ExternalCompareOutRoot", $ExternalCompareOutRoot,
  "-UploadWorkRoot", $UploadWorkRoot,
  "-EnableAff4VirtualApfsProbe",
  "-EnableAff4DynamicProbe",
  "-IncludeLogsTailOnly",
  "-CliTimeoutMinutes", $CliTimeoutMinutes
)
if ($CleanOut) { $runnerArgs += "-CleanOut" }
if ($DiagnosticOutputs) { $runnerArgs += "-DiagnosticOutputs" }
if ($ForceContainerHash) { $runnerArgs += "-ForceContainerHash" }

powershell -ExecutionPolicy Bypass -File $Runner @runnerArgs
if ($LASTEXITCODE -ne 0) { throw "AFF4/APFS V1.6.38 probe wrapper failed with exit code $LASTEXITCODE. Review case folder: $CaseRoot" }
Assert-PathExists -PathValue $ZipPath -Description "Thin upload ZIP" -Leaf

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
try {
  $entries = @{}
  foreach ($entry in $zip.Entries) { $entries[$entry.FullName.Replace('/','\')] = $entry }
  function Read-ZipEntryText {
    param([Parameter(Mandatory=$true)][string]$EntryName)
    $key = $EntryName.Replace('/','\')
    if (!$entries.ContainsKey($key)) { throw "Expected ZIP entry missing: $EntryName" }
    $stream = $entries[$key].Open()
    try {
      $reader = New-Object System.IO.StreamReader($stream, [System.Text.Encoding]::UTF8, $true)
      try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
    } finally { $stream.Dispose() }
  }
  $runStatus = Read-ZipEntryText -EntryName "run_status.txt"
  if ($runStatus -match "aff4_dynamic_load_probe message=skipped by default") {
    throw "Thin ZIP is source-probe-only: AFF4 dynamic/direct reader path did not run."
  }
  foreach ($name in @(
    "aff4_apfs_spotlight_file_copy_out_summary.json",
    "aff4_apfs_spotlight_file_copy_out.csv",
    "aff4_apfs_extracted_storev2_stage_summary.json",
    "aff4_apfs_extracted_storev2_stage_files.csv",
    "aff4_apfs_staged_storev2_parser_probe_summary.json",
    "aff4_apfs_staged_storev2_enrichment_probe_summary.json",
    "aff4_apfs_staged_storev2_field_inventory_sample.csv",
    "aff4_apfs_staged_storev2_parser_coverage_summary_sample.csv",
    "aff4_apfs_staged_storev2_path_reconstruction_sample.csv",
    "aff4_apfs_staged_storev2_path_reconstruction_metrics_sample.csv"
  )) {
    if (!$entries.ContainsKey($name.Replace('/','\'))) { throw "Thin ZIP is missing required AFF4/APFS Store-V2 validation output: $name" }
  }
  $parserSummary = Read-ZipEntryText -EntryName "aff4_apfs_staged_storev2_parser_probe_summary.json"
  if ($parserSummary -notmatch '"parse_probe_status"\s*:\s*"PARSE_PROBE_COMPLETED"') { throw "Parser summary does not report PARSE_PROBE_COMPLETED." }
  if ($parserSummary -notmatch '"native_decode_mode"\s*:\s*"FullValues"') { throw "Parser summary does not report default FullValues validation mode." }
  Write-Host "Verified thin ZIP contains AFF4/APFS Store-V2 extraction, staging, parsing, enrichment, and FullValues validation outputs."
} finally { $zip.Dispose() }

Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256 | Tee-Object -FilePath ($ZipPath + ".sha256.txt")
Write-Host "AFF4/APFS thin upload ZIP: $ZipPath"
