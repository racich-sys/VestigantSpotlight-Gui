param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_101",
  [Parameter(Mandatory=$true)][string]$Aff4Path,
  [string]$CaseRoot = "Q:\SpotlightCase\TestMacOS_AFF4_V1_6_101",
  [string]$ReaderToolsRoot = "",
  [string]$ExternalSpotlightRoot = "",
  [string]$ExternalCompareOutRoot = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_101_ExternalCompare",
  [string]$UploadWorkRoot = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_101_UploadWork",
  [string]$ZipPath = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_101.zip",
  [switch]$CleanOut,
  [switch]$DiagnosticOutputs,
  [switch]$ForceContainerHash,
  [switch]$FullNoGuardrails,
  [switch]$PressureTestMode,
  [switch]$EnableStreamInventory,
  [switch]$RequireStoreV2ValidationOutputs,
  [switch]$FullNativeValues,
  [int]$MaxNativeRecords = 0,
  [int]$MaxNativeBlocks = 0,
  [int]$CliTimeoutMinutes = 180
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
if (![string]::IsNullOrWhiteSpace($ReaderToolsRoot)) { Assert-PathExists -PathValue $ReaderToolsRoot -Description "AFF4 reader tools folder" }
if (![string]::IsNullOrWhiteSpace($ExternalSpotlightRoot)) { Assert-PathExists -PathValue $ExternalSpotlightRoot -Description "External Store-V2 reference folder" }

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
  "-ZipPath", $ZipPath,
  "-UploadWorkRoot", $UploadWorkRoot,
  "-IncludeLogsTailOnly",
  "-CliTimeoutMinutes", $CliTimeoutMinutes
)
if (![string]::IsNullOrWhiteSpace($ReaderToolsRoot)) { $runnerArgs += @("-ReaderToolsRoot", $ReaderToolsRoot) }
if (![string]::IsNullOrWhiteSpace($ExternalSpotlightRoot)) {
  $runnerArgs += @("-ExternalSpotlightRoot", $ExternalSpotlightRoot)
  $runnerArgs += @("-ExternalCompareOutRoot", $ExternalCompareOutRoot)
}
if ($CleanOut) { $runnerArgs += "-CleanOut" }
if ($DiagnosticOutputs -or $FullNoGuardrails) { $runnerArgs += "-DiagnosticOutputs" }
if ($ForceContainerHash) {
  $runnerArgs += "-ForceContainerHash"
} else {
  $runnerArgs += "-SkipContainerHash"
  if ($PressureTestMode -or $FullNoGuardrails) { $runnerArgs += "-PressureTestMode" }
}
if ($FullNoGuardrails) {
  $runnerArgs += "-FullScan"
  $runnerArgs += "-EnableAff4DynamicProbe"
  $runnerArgs += "-EnableAff4VirtualApfsProbe"
}
if ($FullNativeValues -or $PressureTestMode -or $FullNoGuardrails) { $runnerArgs += "-FullNativeValues" } else { $runnerArgs += "-DecodeCoreNativeValues" }
if ($MaxNativeRecords -ge 0) { $runnerArgs += @("-MaxNativeRecords", ([string]$MaxNativeRecords)) }
if ($MaxNativeBlocks -gt 0) { $runnerArgs += @("-MaxNativeBlocks", ([string]$MaxNativeBlocks)) }
if ($EnableStreamInventory) { $runnerArgs += "-EnableAff4StreamInventory" }

powershell -ExecutionPolicy Bypass -File $Runner @runnerArgs
if ($LASTEXITCODE -ne 0) { throw "AFF4/APFS V1.6.101 probe wrapper failed with exit code $LASTEXITCODE. Review case folder: $CaseRoot" }
Assert-PathExists -PathValue $ZipPath -Description "Thin upload ZIP" -Leaf

if ($RequireStoreV2ValidationOutputs) {
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
      "aff4_apfs_staged_storev2_enrichment_probe_summary.json"
    )) {
      if (!$entries.ContainsKey($name.Replace('/','\'))) { throw "Thin ZIP is missing required AFF4/APFS Store-V2 validation output: $name" }
    }
    $parserSummary = Read-ZipEntryText -EntryName "aff4_apfs_staged_storev2_parser_probe_summary.json"
    if ($parserSummary -notmatch '"parse_probe_status"\s*:\s*"PARSE_PROBE_COMPLETED"') { throw "Parser summary does not report PARSE_PROBE_COMPLETED." }
    Write-Host "Verified thin ZIP contains required AFF4/APFS Store-V2 validation outputs."
  } finally { $zip.Dispose() }
} else {
  Write-Host "Partial AFF4/APFS diagnostic uploads are allowed for this wrapper run. Use -RequireStoreV2ValidationOutputs only for strict validation."
}

Remove-Item -LiteralPath ($ZipPath + ".sha256.txt") -Force -ErrorAction SilentlyContinue
Write-Host "AFF4/APFS thin upload ZIP: $ZipPath"
Write-Host "Thin/test mode: upload ZIP SHA256 sidecar intentionally not generated. Full validation can hash later if needed."
