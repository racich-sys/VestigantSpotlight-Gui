param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_6_5",
  [string]$InputZipOrFolder = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$CaseRoot = "Q:\SpotlightCase\TestIOS_CoreSpotlight_V1_6_6_5",
  [string]$ZipPath = "D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_6_5.zip",
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
}
if ($CleanOut) { $args.CleanOut = $true }
if ($NoClipboardOrExplorer) { $args.NoClipboardOrExplorer = $true }
if ($FullDiagnostics) { $args.FullDiagnostics = $true }
if ($NoCsvExports) { $args.NoCsvExports = $true }

& $runner @args
if ($LASTEXITCODE -ne 0) {
  $failedZip = [System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($ZipPath), ([System.IO.Path]::GetFileNameWithoutExtension($ZipPath) + "_FAILED.zip"))
  if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $failedZip -Force -ErrorAction SilentlyContinue
    Move-Item -LiteralPath $ZipPath -Destination $failedZip -Force
    Write-Warning "iOS CoreSpotlight run failed; upload ZIP renamed to: $failedZip"
  }
  throw "iOS CoreSpotlight V1.6.6.5 wrapper failed with exit code $LASTEXITCODE"
}
Write-Host "iOS CoreSpotlight thin upload ZIP: $ZipPath"
