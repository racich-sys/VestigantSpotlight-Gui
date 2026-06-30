param(
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V1_6_115.zip",
  [string]$DestinationRoot = "T:\",
  [string]$BuildLog = "D:\Downloads\V1_6_115_build.log",
  [string]$ReaderToolsRoot = ""
)

$ErrorActionPreference = "Stop"
# Release marker regex: 1\.6\.112


if (!(Test-Path -LiteralPath $ZipPath)) {
  throw "Downloaded source ZIP not found: $ZipPath"
}

$SourceRoot = Join-Path $DestinationRoot "VestigantSpotlightInv_V1_6_115"
Write-Host "Source ZIP: $ZipPath"
Write-Host "Destination source root: $SourceRoot"
Write-Host "Build log: $BuildLog"
Write-Host "Thin/test mode: downloaded source ZIP hash display skipped. Use full validation hashing only when requested."

if (Test-Path -LiteralPath $SourceRoot) {
  Write-Host "Removing prior extracted source folder: $SourceRoot"
  Remove-Item -LiteralPath $SourceRoot -Recurse -Force
}

Write-Host "Expanding downloaded source ZIP to $DestinationRoot"
Expand-Archive -LiteralPath $ZipPath -DestinationPath $DestinationRoot -Force

$BuildScript = Join-Path $SourceRoot "scripts\Build-V1_6_115.ps1"
if (!(Test-Path -LiteralPath $BuildScript)) {
  throw "Build script was not found after extraction: $BuildScript"
}

Set-Location -LiteralPath $SourceRoot
Write-Host "Running build script from extracted source folder: $SourceRoot"
$buildArgs = @("-ExecutionPolicy", "Bypass", "-File", $BuildScript, "-ZipPath", $ZipPath, "-SourceRoot", $SourceRoot, "-BuildLog", $BuildLog)
if (![string]::IsNullOrWhiteSpace($ReaderToolsRoot)) { $buildArgs += @("-ReaderToolsRoot", $ReaderToolsRoot) }
powershell @buildArgs
if ($LASTEXITCODE -ne 0) {
  throw "Extracted-source build script failed with exit code $LASTEXITCODE. Log: $BuildLog"
}

Write-Host "Build completed. Upload log: $BuildLog"
