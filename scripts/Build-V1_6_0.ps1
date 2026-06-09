param(
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V1_6_0.zip",
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_0",
  [string]$BuildLog = "D:\Downloads\V1_6_0_build.log",
  [switch]$CleanExtract
)

$ErrorActionPreference = "Stop"

$WrapperCompat = Join-Path $SourceRoot "tools\Verify-PowerShellWrapperCompatibility.ps1"
if (Test-Path -LiteralPath $WrapperCompat) { powershell -ExecutionPolicy Bypass -File $WrapperCompat -SourceRoot $SourceRoot }

$MsvcStringCheck = Join-Path $SourceRoot "tools\Verify-MsvcStringLiteralRisk.ps1"
if (Test-Path -LiteralPath $MsvcStringCheck) { powershell -ExecutionPolicy Bypass -File $MsvcStringCheck -SourceRoot $SourceRoot }

$ReleaseReadiness = Join-Path $SourceRoot "tools\Verify-V1_6_0-ReleaseReadiness.ps1"
if (Test-Path -LiteralPath $ReleaseReadiness) { powershell -ExecutionPolicy Bypass -File $ReleaseReadiness -SourceRoot $SourceRoot }


if (!(Test-Path -LiteralPath $SourceRoot)) {
  if (!(Test-Path -LiteralPath $ZipPath)) { throw "SourceRoot and source ZIP not found. SourceRoot=$SourceRoot ZIP=$ZipPath" }
  Set-Location D:\Downloads
  Get-FileHash $ZipPath -Algorithm SHA256 | Format-List
  Expand-Archive -LiteralPath $ZipPath -DestinationPath "T:\" -Force
} elseif ($CleanExtract) {
  if (!(Test-Path -LiteralPath $ZipPath)) { throw "Source ZIP not found for CleanExtract: $ZipPath" }
  Set-Location D:\Downloads
  Get-FileHash $ZipPath -Algorithm SHA256 | Format-List
  Remove-Item -LiteralPath $SourceRoot -Recurse -Force -ErrorAction SilentlyContinue
  Expand-Archive -LiteralPath $ZipPath -DestinationPath "T:\" -Force
}

if (!(Test-Path -LiteralPath "$SourceRoot\build_windows_msvc.bat")) { throw "Build script not found: $SourceRoot\build_windows_msvc.bat" }

Remove-Item -LiteralPath "$SourceRoot\build-msvc\obj\win32_gui.obj" -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$SourceRoot\build-msvc\obj\app_info.obj" -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$SourceRoot\build-msvc\Release\VestigantSpotlight.exe" -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$SourceRoot\build-msvc\Release\VestigantSpotlightCli.exe" -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$SourceRoot\build-msvc\Release\VestigantSpotlightTests.exe" -Force -ErrorAction SilentlyContinue

& "$SourceRoot\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath $BuildLog
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE. Log: $BuildLog" }

$version = (& "$SourceRoot\build-msvc\Release\VestigantSpotlightCli.exe" --version 2>&1 | Out-String).Trim()
if ($version -notmatch "1\.6\.0") { throw "Unexpected CLI version after build: $version" }
Write-Host $version
Write-Host "Build log: $BuildLog"
