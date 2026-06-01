Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = "T:\VestigantSpotlight"
$Log = "D:\Downloads\VestigantSpotlight_LocalBuild.log"
Set-Location $RepoRoot
.\build_windows_msvc.bat 2>&1 | Tee-Object -FilePath $Log
.\build-msvc\Release\VestigantSpotlightCli.exe --version
.\build-msvc\Release\VestigantSpotlightTests.exe ".\build-msvc\selftest_out"
Write-Host "Build log: $Log"
