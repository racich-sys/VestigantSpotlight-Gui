param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_7_1",
  [string]$Aff4Path = "O:\0109_0142-IT001.aff4",
  [string]$CaseRoot = "Q:\SpotlightCase\TestMacOS_AFF4_V1_6_7_1",
  [string]$ReaderToolsRoot = "T:\VestigantReaderTools\aff4-cpp-lite",
  [string]$ZipPath = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_7_1.zip",
  [switch]$CleanOut,
  [switch]$DiagnosticOutputs,
  [switch]$ForceContainerHash
)

$ErrorActionPreference = "Stop"
$Cli = Join-Path $SourceRoot "build-msvc\Release\VestigantSpotlightCli.exe"
$UploadTool = Join-Path $SourceRoot "tools\Create-SourceProbeUploadZip.ps1"
if (!(Test-Path -LiteralPath $Cli)) { throw "CLI binary not found. Build this source tree first: $Cli" }
if (!(Test-Path -LiteralPath $Aff4Path)) { throw "AFF4 input not found: $Aff4Path" }
if (!(Test-Path -LiteralPath $UploadTool)) { throw "Upload packaging helper not found: $UploadTool" }
if ($CleanOut) { Remove-Item -LiteralPath $CaseRoot -Recurse -Force -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path $CaseRoot | Out-Null

$args = @("--mode","source-probe","--profile","macos","--input",$Aff4Path,"--out",$CaseRoot,"--reader-tools",$ReaderToolsRoot,"--strict-single-aff4","--enable-aff4-dynamic-probe","--enable-aff4-stream-inventory","--verbose")
if ($DiagnosticOutputs) { $args += "--aff4-apfs-diagnostic-outputs" }
if ($ForceContainerHash) { $args += "--force-container-hash" }
& $Cli @args
if ($LASTEXITCODE -ne 0) { throw "AFF4/APFS V1.6.7.1 probe CLI failed with exit code $LASTEXITCODE" }
& $UploadTool -CaseRoot $CaseRoot -ZipPath $ZipPath -UploadWorkRoot "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_7_1_UploadWork" -ReaderToolsRoot $ReaderToolsRoot -IncludeLogsTailOnly
if ($LASTEXITCODE -ne 0) { throw "AFF4/APFS V1.6.7.1 probe upload packaging failed with exit code $LASTEXITCODE" }
Write-Host "AFF4/APFS thin upload ZIP: $ZipPath"
