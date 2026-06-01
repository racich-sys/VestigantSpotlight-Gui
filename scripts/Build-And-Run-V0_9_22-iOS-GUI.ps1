Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Set-Location D:\Downloads

$Zip = "D:\Downloads\VestigantSpotlightInv_V0_9_22.zip"
$SourceRoot = "T:\VestigantSpotlightInv_V0_9_22"
$BuildLog = "D:\Downloads\V0_9_22_build.log"
$Exe = "$SourceRoot\build-msvc\Release\VestigantSpotlight.exe"

Get-FileHash -LiteralPath $Zip -Algorithm SHA256 | Format-List
Remove-Item -LiteralPath $SourceRoot -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath $Zip -DestinationPath T:\ -Force
& "$SourceRoot\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath $BuildLog
& "$SourceRoot\build-msvc\Release\VestigantSpotlightCli.exe" --version

Write-Host "Launching GUI. Use these settings:"
Write-Host "  Source type: ZIP"
Write-Host "  Profile: iOS/CoreSpotlight"
Write-Host "  Input: T:\0202_0024-IT002\00008132-000269523699001C_files_full.zip"
Write-Host "  Case location: Q:\SpotlightCase\TestiOS_V0_9_22"
Write-Host "  Mode: Process Raw Spotlight Evidence"
Write-Host "  Full native values: enabled"
Write-Host "  Export profile: Investigator"
Start-Process -FilePath $Exe
