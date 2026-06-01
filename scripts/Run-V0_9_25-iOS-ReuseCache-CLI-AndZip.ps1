param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V0_9_25",
  [string]$InputZip = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$ReuseCache = "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4",
  [string]$CaseRoot = "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_25_ReusedCache",
  [string]$OutZip = "D:\Downloads\Upload_Thin_iOS_GUI_V0_9_25_ReusedCache_Check.zip"
)

$ErrorActionPreference = "Stop"

Remove-Item -LiteralPath $CaseRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $CaseRoot | Out-Null

& "$SourceRoot\build-msvc\Release\VestigantSpotlightCli.exe" `
  --mode run `
  --profile ios `
  --input $InputZip `
  --out $CaseRoot `
  --reuse-ios-cache $ReuseCache `
  --skip-container-hash `
  --experimental-full-native-values `
  --export-profile investigator `
  --verbose

powershell -ExecutionPolicy Bypass -File "$SourceRoot\scripts\Package-V0_9_25-iOS-ThinUpload.ps1" -CaseRoot $CaseRoot -OutZip $OutZip
