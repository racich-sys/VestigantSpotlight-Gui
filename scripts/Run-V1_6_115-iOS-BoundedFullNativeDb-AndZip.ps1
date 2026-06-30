param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_115",
  [string]$InputZipOrFolder = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$CaseRoot = "Q:\SpotlightCase\BoundedFullNativeIOS_CoreSpotlight_V1_6_115",
  [string]$ZipPath = "D:\Downloads\Upload_BoundedFullNative_iOS_CoreSpotlight_V1_6_115.zip",
  [int]$MaxNativeRecords = 25000,
  [switch]$CleanOut,
  [switch]$NoClipboardOrExplorer,
  [string]$ExternalSourceSha256 = "",
  [string]$ExternalSourceHashNote = "",
  [string]$ReuseIosCache = ""
)

$ErrorActionPreference = "Stop"
if ($MaxNativeRecords -le 0) { throw "MaxNativeRecords must be a positive bounded validation value. Use the CLI directly only after bounded validation evidence supports an unbounded run." }
$runner = Join-Path $SourceRoot "tools\Run-IosCoreSpotlightFocusedZip.ps1"
if (!(Test-Path -LiteralPath $runner)) { throw "iOS CoreSpotlight runner not found: $runner" }

$args = @{
  InputZipOrFolder = $InputZipOrFolder
  Out = $CaseRoot
  ZipPath = $ZipPath
  RunMode = "diagnostics"
  ExportProfile = "diagnostics"
  ForceContainerHash = $true
  FullNativeValues = $true
  DiagnosticFullNativeDb = $true
  MaxNativeRecords = $MaxNativeRecords
  MaterializeIosSupportDb = $true
}
if ($CleanOut) { $args.CleanOut = $true }
if ($NoClipboardOrExplorer) { $args.NoClipboardOrExplorer = $true }
if (![string]::IsNullOrWhiteSpace($ExternalSourceSha256)) { $args.ExternalSourceSha256 = $ExternalSourceSha256 }
if (![string]::IsNullOrWhiteSpace($ExternalSourceHashNote)) { $args.ExternalSourceHashNote = $ExternalSourceHashNote }
if (![string]::IsNullOrWhiteSpace($ReuseIosCache)) { $args.ReuseIosCache = $ReuseIosCache }

& $runner @args
if ($LASTEXITCODE -ne 0) {
  $failedZip = [System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($ZipPath), ([System.IO.Path]::GetFileNameWithoutExtension($ZipPath) + "_FAILED.zip"))
  if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $failedZip -Force -ErrorAction SilentlyContinue
    Move-Item -LiteralPath $ZipPath -Destination $failedZip -Force
    Write-Warning "iOS bounded full-native DB validation failed; upload ZIP renamed to: $failedZip"
  }
  throw "iOS CoreSpotlight V1.6.115 bounded full-native DB wrapper failed with exit code $LASTEXITCODE"
}
Write-Host "iOS CoreSpotlight bounded full-native DB upload ZIP: $ZipPath"
Write-Host "Use parser_limits_and_suppression_summary.csv to decide whether MaxNativeRecords can be raised or removed in a later run."
