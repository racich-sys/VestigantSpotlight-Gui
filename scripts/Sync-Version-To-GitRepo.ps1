param(
  [string]$Version = "V1_0_23",
  [string]$VersionRoot = "T:\VestigantSpotlightInv_V1_0_23",
  [string]$RepoRoot = "T:\VestigantSpotlight",
  [switch]$Commit,
  [switch]$Push
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (!(Test-Path -LiteralPath $VersionRoot)) { throw "Version folder not found: $VersionRoot" }
if (!(Test-Path -LiteralPath $RepoRoot)) { throw "Repo folder not found: $RepoRoot" }

$RequiredVersionFiles = @(
  "build_windows_msvc.bat",
  "src\app\app_runner.cpp",
  "src\db\case_db.cpp",
  "src\gui\win32_gui.cpp",
  ".github\workflows\windows-msvc-build.yml"
)
foreach ($Rel in $RequiredVersionFiles) {
  $Path = Join-Path $VersionRoot $Rel
  if (!(Test-Path -LiteralPath $Path)) { throw "Version source is missing required file: $Path" }
}

$SyncLog = "D:\Downloads\Sync_$Version`_ToGitRepo.log"
robocopy $VersionRoot $RepoRoot /MIR `
  /XD ".git" "build-msvc" "build" "build-linux" ".vs" `
  /XF "*.zip" "*.7z" "*.rar" "*.sha256" "*.aff4" "*.E01" "*.Ex01" "*.img" "*.dd" "*.raw" `
  /R:1 /W:1 /TEE /LOG:$SyncLog
$RoboExit = $LASTEXITCODE
if ($RoboExit -gt 7) { throw "Robocopy failed with exit code $RoboExit. See $SyncLog" }

Set-Location $RepoRoot

foreach ($Rel in $RequiredVersionFiles) {
  if (!(Test-Path -LiteralPath (Join-Path $RepoRoot $Rel))) { throw "Post-sync repo is missing required file: $Rel" }
}

git status --short
Write-Host "Review the diff before committing: git diff --stat; git diff --cached --stat after git add -A"

if ($Commit) {
  git add -A
  git diff --cached --stat
  git commit -m "Update Vestigant Spotlight to $Version"
  if ($Push) { git push --set-upstream origin main }
} else {
  Write-Host "No commit made. To commit after review: git add -A; git diff --cached --stat; git commit -m 'Update Vestigant Spotlight to $Version'; git push --set-upstream origin main"
}
