param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_41",
  [string]$InputZipOrFolder = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$CaseRoot = "Q:\SpotlightCase\ProductionIOS_CoreSpotlight_V1_6_41",
  [string]$ZipPath = "D:\Downloads\Upload_Production_iOS_CoreSpotlight_V1_6_41.zip",
  [switch]$CleanOut,
  [switch]$NoClipboardOrExplorer,
  [switch]$MaterializeIosSupportDb
)

$ErrorActionPreference = "Stop"
$runner = Join-Path $SourceRoot "tools\Run-IosCoreSpotlightFocusedZip.ps1"
if (!(Test-Path -LiteralPath $runner)) { throw "iOS CoreSpotlight runner not found: $runner" }

$args = @{
  InputZipOrFolder = $InputZipOrFolder
  Out = $CaseRoot
  ZipPath = $ZipPath
  RunMode = "run"
  ExportProfile = "investigator"
  ForceContainerHash = $true
  FullNativeValues = $true
}
if ($CleanOut) { $args.CleanOut = $true }
if ($NoClipboardOrExplorer) { $args.NoClipboardOrExplorer = $true }
if ($MaterializeIosSupportDb) { $args.MaterializeIosSupportDb = $true }

& $runner @args
if ($LASTEXITCODE -ne 0) {
  $failedZip = [System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($ZipPath), ([System.IO.Path]::GetFileNameWithoutExtension($ZipPath) + "_FAILED.zip"))
  if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $failedZip -Force -ErrorAction SilentlyContinue
    Move-Item -LiteralPath $ZipPath -Destination $failedZip -Force
    Write-Warning "iOS production run failed; upload ZIP renamed to: $failedZip"
  }
  throw "iOS CoreSpotlight V1.6.41.1 production wrapper failed with exit code $LASTEXITCODE"
}
Write-Host "iOS CoreSpotlight production upload ZIP: $ZipPath"
Write-Host "Review exports/ios_production_readiness_summary.csv in the local case folder before reporting."
