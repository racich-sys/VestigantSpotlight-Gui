param(
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V1_6_72.zip",
  [string]$DestinationRoot = "T:\",
  [string]$BuildLog = "D:\Downloads\V1_6_72_build.log"
)

$ErrorActionPreference = "Stop"

if (!(Test-Path -LiteralPath $ZipPath)) {
  throw "Downloaded source ZIP not found: $ZipPath"
}

$SourceRoot = Join-Path $DestinationRoot "VestigantSpotlightInv_V1_6_72"
Write-Host "Source ZIP: $ZipPath"
Write-Host "Destination source root: $SourceRoot"
Write-Host "Build log: $BuildLog"
Write-Host "SHA256 of downloaded ZIP:"
Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256 | Format-List

if (Test-Path -LiteralPath $SourceRoot) {
  Write-Host "Removing prior extracted source folder: $SourceRoot"
  Remove-Item -LiteralPath $SourceRoot -Recurse -Force
}

Write-Host "Expanding downloaded source ZIP to $DestinationRoot"
Expand-Archive -LiteralPath $ZipPath -DestinationPath $DestinationRoot -Force

$BuildScript = Join-Path $SourceRoot "scripts\Build-V1_6_72.ps1"
if (!(Test-Path -LiteralPath $BuildScript)) {
  throw "Build script was not found after extraction: $BuildScript"
}

Set-Location -LiteralPath $SourceRoot
Write-Host "Running build script from extracted source folder: $SourceRoot"
powershell -ExecutionPolicy Bypass -File $BuildScript -ZipPath $ZipPath -SourceRoot $SourceRoot -BuildLog $BuildLog
if ($LASTEXITCODE -ne 0) {
  throw "Extracted-source build script failed with exit code $LASTEXITCODE. Log: $BuildLog"
}

Write-Host "Build completed. Upload log: $BuildLog"
