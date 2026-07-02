param(
  [string]$DownloadRoot = "D:\Downloads",
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V1_6_119.zip",
  [string]$DestinationRoot = "T:\",
  [string]$BuildLog = "D:\Downloads\V1_6_119_build.log",
  [string]$InputZipOrFolder = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$CaseRootBase = "Q:\SpotlightCase\iOS_TestMatrix_V1_6_119",
  [string]$BundlePath = "D:\Downloads\Upload_iOS_TestMatrix_V1_6_119.zip",
  [int]$MaxNativeRecords = 25000,
  [switch]$CleanOut,
  [switch]$NoClipboardOrExplorer,
  [switch]$SkipBuild,
  [switch]$SkipThin,
  [switch]$SkipAppDbSpotlight,
  [switch]$IncludeValidationSupport,
  [switch]$IncludeBoundedFullNative,
  [switch]$IncludeProduction,
  [string]$ExternalSourceSha256 = "",
  [string]$ExternalSourceHashNote = "",
  [string]$ReuseIosCache = ""
)

$ErrorActionPreference = "Stop"
if (!(Test-Path -LiteralPath $ZipPath -PathType Leaf)) { throw "Source ZIP not found: $ZipPath" }
if (!(Test-Path -LiteralPath $InputZipOrFolder)) { throw "InputZipOrFolder not found: $InputZipOrFolder" }

$SourceRoot = Join-Path $DestinationRoot "VestigantSpotlightInv_V1_6_119"
Write-Host "Vestigant Spotlight V1.6.119 iOS test-matrix after-download wrapper"
Write-Host "Source ZIP: $ZipPath"
Write-Host "Source root: $SourceRoot"
Write-Host "Input: $InputZipOrFolder"

if ($SkipBuild -and (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
  Write-Warning "SkipBuild requested; preserving existing source root: $SourceRoot"
} else {
  if (Test-Path -LiteralPath $SourceRoot) { Remove-Item -LiteralPath $SourceRoot -Recurse -Force }
  Expand-Archive -LiteralPath $ZipPath -DestinationPath $DestinationRoot -Force
}

if (!$SkipBuild) {
  $BuildScript = Join-Path $SourceRoot "scripts\Build-V1_6_119.ps1"
  if (!(Test-Path -LiteralPath $BuildScript -PathType Leaf)) { throw "Build script not found after extraction: $BuildScript" }
  powershell -ExecutionPolicy Bypass -File $BuildScript -ZipPath $ZipPath -SourceRoot $SourceRoot -BuildLog $BuildLog
  if ($LASTEXITCODE -ne 0) { throw "V1.6.119 build/self-test failed with exit code $LASTEXITCODE. Log: $BuildLog" }
} else {
  Write-Warning "Skipping build by request. Existing binaries under $SourceRoot\build-msvc\Release must already be present."
}

$MatrixScript = Join-Path $SourceRoot "scripts\Run-V1_6_119-iOS-TestMatrix-AndZip.ps1"
if (!(Test-Path -LiteralPath $MatrixScript -PathType Leaf)) { throw "iOS test matrix script not found after extraction: $MatrixScript" }
$matrixArgs = @(
  "-ExecutionPolicy", "Bypass", "-File", $MatrixScript,
  "-SourceRoot", $SourceRoot,
  "-InputZipOrFolder", $InputZipOrFolder,
  "-CaseRootBase", $CaseRootBase,
  "-BundlePath", $BundlePath,
  "-MaxNativeRecords", ([string]$MaxNativeRecords),
  "-NoClipboardOrExplorer"
)
if ($CleanOut) { $matrixArgs += "-CleanOut" }
if ($SkipThin) { $matrixArgs += "-SkipThin" }
if ($SkipAppDbSpotlight) { $matrixArgs += "-SkipAppDbSpotlight" }
if ($IncludeValidationSupport) { $matrixArgs += "-IncludeValidationSupport" }
if ($IncludeBoundedFullNative) { $matrixArgs += "-IncludeBoundedFullNative" }
if ($IncludeProduction) { $matrixArgs += "-IncludeProduction" }
if (![string]::IsNullOrWhiteSpace($ExternalSourceSha256)) { $matrixArgs += @("-ExternalSourceSha256", $ExternalSourceSha256) }
if (![string]::IsNullOrWhiteSpace($ExternalSourceHashNote)) { $matrixArgs += @("-ExternalSourceHashNote", $ExternalSourceHashNote) }
if (![string]::IsNullOrWhiteSpace($ReuseIosCache)) { $matrixArgs += @("-ReuseIosCache", $ReuseIosCache) }

powershell @matrixArgs
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) { throw "V1.6.119 iOS test matrix failed with exit code $exitCode. Upload bundle if present: $BundlePath" }
if (!$NoClipboardOrExplorer) {
  try { Set-Clipboard -Value $BundlePath } catch {}
  try { explorer.exe /select,"$BundlePath" | Out-Null } catch {}
}
Write-Host "V1.6.119 iOS test matrix completed. Upload: $BundlePath"
