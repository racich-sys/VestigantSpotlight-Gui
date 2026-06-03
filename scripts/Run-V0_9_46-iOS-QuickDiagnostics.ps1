Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$SourceRoot = "T:\VestigantSpotlightInv_V0_9_46"
$Tool = "$SourceRoot\tools\Collect-iOSCoreSpotlightQuickDiagnostics.ps1"
$InputZip = "T:\0202_0024-IT002\00008132-000269523699001C_files_full.zip"

$ReportRoot = "D:\Downloads\iOS_CoreSpotlight_QuickDiagnostics_V0_9_46"
$ReportZip = "D:\Downloads\iOS_CoreSpotlight_QuickDiagnostics_V0_9_46.zip"
$EvidenceRoot = "D:\Downloads\iOS_CoreSpotlight_MinimalEvidence_V0_9_46"
$EvidenceZip = "D:\Downloads\iOS_CoreSpotlight_MinimalEvidence_V0_9_46.zip"

if (!(Test-Path -LiteralPath $Tool)) { throw "Quick diagnostic tool not found: $Tool" }

& $Tool `
  -InputZip $InputZip `
  -ReportRoot $ReportRoot `
  -ReportZip $ReportZip `
  -EvidenceRoot $EvidenceRoot `
  -EvidenceZip $EvidenceZip `
  -IncludeStoreFiles `
  -IncludeCacheTextSamples `
  -MaxCacheTextSamplesPerProtectionClass 25 `
  -NoClipboardOrExplorer

Write-Host "Upload these files if they were created:"
Write-Host "  $ReportZip"
Write-Host "  $EvidenceZip"
