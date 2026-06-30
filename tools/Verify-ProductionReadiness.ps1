param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_115",
  [string]$ExpectedVersion = "1.6.115"
)

$ErrorActionPreference = "Stop"
$issues = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]

function Add-Issue([string]$msg) { $issues.Add($msg) | Out-Null }
function Add-Warn([string]$msg) { $warnings.Add($msg) | Out-Null }
function Test-RelFile([string]$rel, [bool]$required = $true) {
  $p = Join-Path $SourceRoot $rel
  if (Test-Path -LiteralPath $p -PathType Leaf) { return $true }
  if ($required) { Add-Issue "Missing required file: $rel" } else { Add-Warn "Missing optional file: $rel" }
  return $false
}

$versionPath = Join-Path $SourceRoot "VERSION"
if (Test-Path -LiteralPath $versionPath -PathType Leaf) {
  $v = (Get-Content -LiteralPath $versionPath -Raw).Trim()
  if ($v -ne $ExpectedVersion) { Add-Issue "VERSION mismatch: expected $ExpectedVersion but found $v" }
} else { Add-Issue "Missing VERSION file" }

$mdFiles = Get-ChildItem -LiteralPath $SourceRoot -Recurse -File -Filter "*.md" -ErrorAction SilentlyContinue |
  Where-Object { $_.FullName -notmatch "\\build-" }
if ($mdFiles.Count -ne 5) { Add-Issue "Expected exactly 5 active Markdown files but found $($mdFiles.Count)" }

Test-RelFile "Run-V1_6_115-AfterDownload.ps1" | Out-Null
Test-RelFile "Build-V1_6_115-FromDownloadedZip.ps1" | Out-Null
Test-RelFile "Launch-V1_6_115-GUI.ps1" | Out-Null
Test-RelFile "POWERSHELL_COMMANDS_V1_6_115.txt" | Out-Null
Test-RelFile "tools\Stage-PortableRelease.ps1" | Out-Null
Test-RelFile "tools\Verify-PortableReleaseLayout.ps1" | Out-Null
Test-RelFile "tools\Export-PortableReleaseZip.ps1" | Out-Null
Test-RelFile "tools\Verify-ThinIdentifierCsvPrecision.ps1" | Out-Null
Test-RelFile "resources\reader_tools\libaff4.dll" | Out-Null
Test-RelFile "resources\reader_tools\zlib1.dll" | Out-Null
Test-RelFile "resources\reader_tools\snappy.dll" | Out-Null
Test-RelFile "resources\reader_tools\raptor2.dll" | Out-Null
Test-RelFile "resources\reader_tools\MSVCP140.dll" | Out-Null
Test-RelFile "resources\reader_tools\VCRUNTIME140.dll" | Out-Null
Test-RelFile "resources\reader_tools\VCRUNTIME140_1.dll" | Out-Null

$release = Join-Path $SourceRoot "build-msvc\Release"
if (Test-Path -LiteralPath $release -PathType Container) {
  foreach ($rel in @(
    "VestigantSpotlight.exe",
    "VestigantSpotlightCli.exe",
    "Check-PortableRuntime.ps1",
    "Launch-VestigantSpotlight-GUI.ps1",
    "Run-AFF4Probe-Portable.ps1",
    "resources\reader_tools\libaff4.dll",
    "resources\reader_tools\zlib1.dll",
    "resources\reader_tools\snappy.dll",
    "resources\reader_tools\raptor2.dll",
    "resources\reader_tools\MSVCP140.dll",
    "resources\reader_tools\VCRUNTIME140.dll",
    "resources\reader_tools\VCRUNTIME140_1.dll"
  )) {
    $p = Join-Path $release $rel
    if (!(Test-Path -LiteralPath $p -PathType Leaf)) { Add-Issue "Release folder missing: $rel" }
  }
  $statusPath = Join-Path $release "resources\production_readiness_status.txt"
  $lines = @()
  $lines += "ProductionReadinessVersion=$ExpectedVersion"
  $lines += "Created=$(Get-Date -Format o)"
  $status = if ($issues.Count -eq 0) { 'PRODUCTION_READINESS_CHECK_PASSED' } else { 'PRODUCTION_READINESS_CHECK_FAILED' }
  $lines += "Status=$status"
  $lines += "IssueCount=$($issues.Count)"
  $lines += "WarningCount=$($warnings.Count)"
  if ($issues.Count) { $lines += "Issues:"; $lines += $issues }
  if ($warnings.Count) { $lines += "Warnings:"; $lines += $warnings }
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $statusPath) | Out-Null
  Set-Content -LiteralPath $statusPath -Encoding UTF8 -Value $lines
} else {
  Add-Warn "Release folder not present yet; source-level readiness only was checked."
}

foreach ($w in $warnings) { Write-Warning $w }
if ($issues.Count -gt 0) {
  Write-Host "Production readiness check failed with $($issues.Count) issue(s)."
  foreach ($i in $issues) { Write-Host "ISSUE: $i" }
  exit 1
}
Write-Host "Production readiness check passed for $ExpectedVersion."
exit 0
