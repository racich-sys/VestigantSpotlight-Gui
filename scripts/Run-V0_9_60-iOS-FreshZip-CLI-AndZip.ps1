param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V0_9_60",
  [string]$InputZip = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$CaseRoot = "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_60_FreshZip",
  [string]$OutZip = "D:\Downloads\Upload_Thin_iOS_GUI_V0_9_60_FreshZip_Check.zip"
)

$ErrorActionPreference = "Stop"

$Cli = Join-Path $SourceRoot "build-msvc\Release\VestigantSpotlightCli.exe"
if (!(Test-Path -LiteralPath $Cli)) { throw "CLI not found. Build first: $Cli" }
if (!(Test-Path -LiteralPath $InputZip)) { throw "Input ZIP not found: $InputZip" }

Remove-Item -LiteralPath $CaseRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $CaseRoot | Out-Null

& $Cli `
  --mode run `
  --profile ios `
  --input $InputZip `
  --out $CaseRoot `
  --skip-container-hash `
  --experimental-full-native-values `
  --export-profile investigator `
  --verbose
if ($LASTEXITCODE -ne 0) { throw "VestigantSpotlightCli failed with exit code $LASTEXITCODE" }

powershell -ExecutionPolicy Bypass -File "$SourceRoot\scripts\Package-V0_9_60-iOS-ThinUpload.ps1" -CaseRoot $CaseRoot -OutZip $OutZip
