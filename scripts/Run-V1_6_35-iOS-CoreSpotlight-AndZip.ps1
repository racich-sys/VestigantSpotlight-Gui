param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_35",
  [string]$InputZipOrFolder = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$CaseRoot = "Q:\SpotlightCase\TestIOS_CoreSpotlight_V1_6_35",
  [string]$ZipPath = "D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_35.zip",
  [switch]$CleanOut,
  [switch]$NoClipboardOrExplorer,
  [switch]$FullDiagnostics,
  [switch]$NoCsvExports
)

$ErrorActionPreference = "Stop"
$runner = Join-Path $SourceRoot "tools\Run-IosCoreSpotlightFocusedZip.ps1"
if (!(Test-Path -LiteralPath $runner)) { throw "iOS CoreSpotlight runner not found: $runner" }

$args = @{
  InputZipOrFolder = $InputZipOrFolder
  Out = $CaseRoot
  ZipPath = $ZipPath
  RunMode = "diagnostics"
  ExportProfile = "minimal"
}
if ($CleanOut) { $args.CleanOut = $true }
if ($NoClipboardOrExplorer) { $args.NoClipboardOrExplorer = $true }
if ($FullDiagnostics) { $args.FullDiagnostics = $true }
if ($NoCsvExports) { $args.NoCsvExports = $true }

& $runner @args
if ($LASTEXITCODE -ne 0) {
  if (Test-Path -LiteralPath $ZipPath) {
    Write-Warning "iOS CoreSpotlight thin run did not complete cleanly; upload ZIP retained for diagnostics: $ZipPath"
    Write-Warning "Upload this incomplete-run diagnostic bundle for review instead of rerunning blindly."
    $global:LASTEXITCODE = 0
  } else {
    throw "iOS CoreSpotlight V1.6.35 thin wrapper failed with exit code $LASTEXITCODE and did not create an upload ZIP."
  }
}
Write-Host "iOS CoreSpotlight thin upload ZIP: $ZipPath"
