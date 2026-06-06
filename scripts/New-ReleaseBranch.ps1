Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = "T:\VestigantSpotlight"
$Version = "V1_0_31"
$Branch = "release/$Version"
Set-Location $RepoRoot
git checkout main
git pull
git checkout -b $Branch
Write-Host "Created branch: $Branch"
