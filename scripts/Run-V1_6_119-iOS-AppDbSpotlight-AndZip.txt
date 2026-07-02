param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_119",
  [string]$InputZipOrFolder = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$CaseRoot = "Q:\SpotlightCase\AppDbSpotlightIOS_CoreSpotlight_V1_6_119",
  [string]$ZipPath = "D:\Downloads\Upload_iOS_AppDbSpotlight_V1_6_119.zip",
  [int]$MaxNativeRecords = 25000,
  [switch]$CleanOut,
  [switch]$NoClipboardOrExplorer,
  [switch]$NoPressureTestMode,
  [string]$ExternalSourceSha256 = "",
  [string]$ExternalSourceHashNote = "",
  [string]$ReuseIosCache = ""
)

$ErrorActionPreference = "Stop"
if ($MaxNativeRecords -le 0) { throw "MaxNativeRecords must be positive for this bounded iOS app-DB Spotlight validation script." }
$runner = Join-Path $SourceRoot "tools\Run-IosCoreSpotlightFocusedZip.ps1"
$validator = Join-Path $SourceRoot "tools\Verify-iOSSpotlightValidationOutputs.ps1"
if (!(Test-Path -LiteralPath $runner -PathType Leaf)) { throw "iOS CoreSpotlight runner not found: $runner" }
if (!(Test-Path -LiteralPath $validator -PathType Leaf)) { throw "iOS validation-output verifier not found: $validator" }

$args = @{
  InputZipOrFolder = $InputZipOrFolder
  Out = $CaseRoot
  ZipPath = $ZipPath
  RunMode = "diagnostics"
  ExportProfile = "support"
  FullNativeValues = $true
  DiagnosticFullNativeDb = $true
  MaxNativeRecords = $MaxNativeRecords
  MaterializeIosSupportDb = $true
}
if (!$NoPressureTestMode) { $args.PressureTestMode = $true }
if ($CleanOut) { $args.CleanOut = $true }
if ($NoClipboardOrExplorer) { $args.NoClipboardOrExplorer = $true }
if (![string]::IsNullOrWhiteSpace($ExternalSourceSha256)) { $args.ExternalSourceSha256 = $ExternalSourceSha256 }
if (![string]::IsNullOrWhiteSpace($ExternalSourceHashNote)) { $args.ExternalSourceHashNote = $ExternalSourceHashNote }
if (![string]::IsNullOrWhiteSpace($ReuseIosCache)) { $args.ReuseIosCache = $ReuseIosCache }

& $runner @args
$runnerExit = $LASTEXITCODE
if (!(Test-Path -LiteralPath $ZipPath -PathType Leaf)) {
  throw "iOS App DB Spotlight validation did not create an upload ZIP. RunnerExitCode=$runnerExit ZipPath=$ZipPath"
}

$verifyArgs = @("-ExecutionPolicy", "Bypass", "-File", $validator, "-ZipPath", $ZipPath, "-RequireAppDbSpotlightExports")
if ($runnerExit -ne 0) { $verifyArgs += "-AllowIncompleteRun" }
powershell @verifyArgs
if ($LASTEXITCODE -ne 0) { throw "iOS App DB Spotlight output verification failed with exit code $LASTEXITCODE. ZIP: $ZipPath" }

if ($runnerExit -ne 0) {
  Write-Warning "iOS App DB Spotlight runner exited with code $runnerExit but produced a diagnostic ZIP: $ZipPath"
  exit $runnerExit
}
Write-Host "iOS App DB Spotlight validation upload ZIP: $ZipPath"
Write-Host "Review exports/ios_app_db_spotlight_flag_candidates*.csv and exports/ios_app_db_spotlight_enabled_summary.csv. Rows are app-declared indicators only."
