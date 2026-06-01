Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = "T:\VestigantSpotlight"
$Version = "V0_9_6"
$Branch = "release/$Version"
Set-Location $RepoRoot
git checkout main
git pull
git checkout -b $Branch
Write-Host "Created branch: $Branch"
