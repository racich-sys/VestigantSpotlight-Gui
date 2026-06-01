Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = "T:\VestigantSpotlight"
$OutRoot = "D:\Downloads\VestigantSpotlight_GitHubValidation"
$OutZip = "D:\Downloads\VestigantSpotlight_GitHubValidation.zip"
Remove-Item -LiteralPath $OutRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $OutZip -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $OutRoot | Out-Null
$Files = @("build-msvc\selftest_out", "VERSION_HISTORY.md", "RELEASE_NOTES.md", "KNOWN_ISSUES.md", "HELP.md")
foreach ($File in $Files) {
  $Path = Join-Path $RepoRoot $File
  if (Test-Path -LiteralPath $Path) {
    $Dest = Join-Path $OutRoot $File
    New-Item -ItemType Directory -Force -Path (Split-Path $Dest -Parent) | Out-Null
    Copy-Item -LiteralPath $Path -Destination $Dest -Recurse -Force
  }
}
if (Test-Path -LiteralPath "D:\Downloads\VestigantSpotlight_LocalBuild.log") { Copy-Item -LiteralPath "D:\Downloads\VestigantSpotlight_LocalBuild.log" -Destination (Join-Path $OutRoot "VestigantSpotlight_LocalBuild.log") -Force }
Compress-Archive -Path (Join-Path $OutRoot "*") -DestinationPath $OutZip -Force
Get-FileHash -LiteralPath $OutZip -Algorithm SHA256
