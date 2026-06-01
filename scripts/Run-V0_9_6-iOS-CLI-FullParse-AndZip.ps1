$ErrorActionPreference = "Stop"

$SourceRoot = "T:\VestigantSpotlightInv_V0_9_6"
$Cli = "$SourceRoot\build-msvc\Release\VestigantSpotlightCli.exe"
$CaseRoot = "Q:\SpotlightCase\TestiOS_V0_9_6_CLI"
$InputZip = "T:\0202_0024-IT002\00008132-000269523699001C_files_full.zip"
$OutZip = "D:\Downloads\Upload_Thin_iOS_CLI_V0_9_6_Check.zip"
$Work = "D:\Downloads\Upload_Thin_iOS_CLI_V0_9_6_Check"

if (!(Test-Path -LiteralPath $Cli)) { throw "CLI not found. Run scripts\Build-V0_9_6.ps1 first: $Cli" }
if (!(Test-Path -LiteralPath $InputZip)) { throw "Input ZIP not found: $InputZip" }

Remove-Item -LiteralPath $CaseRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $CaseRoot | Out-Null

& $Cli `
  --mode run `
  --profile ios `
  --input $InputZip `
  --out $CaseRoot `
  --experimental-full-native-values `
  --export-profile investigator `
  --verbose
if ($LASTEXITCODE -ne 0) { throw "VestigantSpotlightCli failed with exit code $LASTEXITCODE" }

& "$SourceRoot\scripts\Package-V0_9_6-iOS-ThinUpload.ps1" -CaseRoot $CaseRoot -OutZip $OutZip -Work $Work
