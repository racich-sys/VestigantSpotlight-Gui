param(
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V0_9_31.zip",
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V0_9_31",
  [string]$BuildLog = "D:\Downloads\V0_9_31_build.log",
  [switch]$CleanExtract
)

$ErrorActionPreference = "Stop"

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

& "$SourceRoot\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath $BuildLog
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE. Log: $BuildLog" }

& "$SourceRoot\build-msvc\Release\VestigantSpotlightCli.exe" --version
Write-Host "Build log: $BuildLog"
