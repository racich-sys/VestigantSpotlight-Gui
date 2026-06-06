Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$CurrentVersionRoot = "T:\VestigantSpotlightInv_V1_1_0_1"
$RepoRoot = "T:\VestigantSpotlight"
$GitHubUserOrOrg = "racich-sys"
$RepoName = "VestigantSpotlight"
$RemoteUrl = "https://github.com/$GitHubUserOrOrg/$RepoName.git"

if (!(Test-Path -LiteralPath $CurrentVersionRoot)) { throw "Current source folder not found: $CurrentVersionRoot" }

Remove-Item -LiteralPath $RepoRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $RepoRoot | Out-Null
Copy-Item -LiteralPath (Join-Path $CurrentVersionRoot "*") -Destination $RepoRoot -Recurse -Force

Set-Location $RepoRoot
if (!(Test-Path -LiteralPath ".git")) { git init }
git branch -M main
git add .
git status
git commit -m "Initial Vestigant Spotlight import"

$ExistingOrigin = git remote get-url origin 2>$null
if ($LASTEXITCODE -eq 0 -and $ExistingOrigin) { git remote set-url origin $RemoteUrl } else { git remote add origin $RemoteUrl }
git push -u origin main
