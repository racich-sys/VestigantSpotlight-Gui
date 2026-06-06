param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_0_25",
  [string]$Aff4Input = "O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4",
  [string]$CaseRoot = "Q:\SpotlightCase\TestMacOS_AFF4_V1_0_25",
  [string]$ReaderToolsRoot = "T:\VestigantReaderTools\aff4-cpp-lite",
  [string]$ZipPath = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_25.zip",
  [string]$ExternalSpotlightRoot = "T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2",
  [switch]$SkipExternalCompare,
  [switch]$CleanOut,
  [switch]$ForceContainerHash,
  [switch]$NoClipboardOrExplorer,
  [switch]$DiagnosticOutputs
)

$ErrorActionPreference = "Stop"
$runner = Join-Path $SourceRoot "tools\Run-SingleAff4SourceProbeAndZip.ps1"
if (!(Test-Path -LiteralPath $runner)) { throw "AFF4 runner not found: $runner" }

$args = @{
  Aff4Input = $Aff4Input
  Out = $CaseRoot
  ReaderToolsRoot = $ReaderToolsRoot
  ZipPath = $ZipPath
  EnableAff4VirtualApfsProbe = $true
  CleanOut = $CleanOut
  CliTimeoutMinutes = 90
}
if ($ForceContainerHash) { $args.ForceContainerHash = $true }
if ($NoClipboardOrExplorer) { $args.NoClipboardOrExplorer = $true }
if ($DiagnosticOutputs) { $args.DiagnosticOutputs = $true }
if (!$SkipExternalCompare -and (Test-Path -LiteralPath $ExternalSpotlightRoot)) {
  $args.ExternalSpotlightRoot = $ExternalSpotlightRoot
  $args.ExternalCompareOutRoot = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_25_ExternalCompare"
} elseif (!$SkipExternalCompare) {
  Write-Warning "External Spotlight reference was not found, so the AFF4 probe will run without external comparison: $ExternalSpotlightRoot"
}

& $runner @args
if ($LASTEXITCODE -ne 0) { throw "AFF4/APFS V1.0.25 probe wrapper failed with exit code $LASTEXITCODE" }
Write-Host "AFF4/APFS thin upload ZIP: $ZipPath"
