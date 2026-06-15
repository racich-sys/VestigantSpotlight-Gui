param([string]$SourceRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = "Stop"
$expected = "1.6.29.4"
$expectedToken = "V1_6_29_4"
function ReadText($Relative) {
  $p = Join-Path $SourceRoot $Relative
  if (!(Test-Path -LiteralPath $p)) { throw "Missing required file: $Relative" }
  return Get-Content -LiteralPath $p -Raw
}
function RequireContains($Relative, [string[]]$Needles) {
  $txt = ReadText $Relative
  foreach ($needle in $Needles) {
    if ($txt -notmatch [regex]::Escape($needle)) { throw "$Relative missing required marker: $needle" }
  }
}
function RequireNotContains($Relative, [string[]]$Needles) {
  $txt = ReadText $Relative
  foreach ($needle in $Needles) {
    if ($txt -match [regex]::Escape($needle)) { throw "$Relative contains stale/forbidden marker: $needle" }
  }
}

if ((ReadText "VERSION") -notmatch '^1\.6\.29\.4\s*$') { throw "VERSION is not $expected" }
if ((ReadText "VERSION.txt") -notmatch '^1\.6\.29\.4\s*$') { throw "VERSION.txt is not $expected" }
if ((ReadText "CMakeLists.txt") -notmatch 'VERSION 1\.6\.29\.4') { throw "CMakeLists.txt project version is not $expected" }
RequireContains "src\core\app_info.cpp" @('return "1.6.29.4"')

$build = ReadText "scripts\Build-V1_6_29_4.ps1"
foreach ($needle in @('Verify-V1_6_29_4-ReleaseReadiness.ps1', 'Invoke-CheckedPreflightScript', '${LASTEXITCODE}', '1\.6\.29\.4')) {
  if ($build -notmatch [regex]::Escape($needle)) { throw "Build wrapper missing required marker: $needle" }
}
foreach ($script in @('scripts\Run-V1_6_29_4-iOS-CoreSpotlight-AndZip.ps1', 'scripts\Run-V1_6_29_4-iOS-ValidationSupport-AndZip.ps1', 'scripts\Run-V1_6_29_4-iOS-BoundedFullNativeDb-AndZip.ps1', 'scripts\Run-V1_6_29_4-iOS-Production-AndZip.ps1', 'scripts\Launch-V1_6_29_4-GUI.ps1')) {
  $null = ReadText $script
}

RequireContains "src\parsers\aff4_probe_worker.cpp" @('VOLUME_OMAP_VERTICAL_CYCLE_DETECTED', 'literalLen > srcSize - pos', 'literalLen > expectedOutputSize - outPos', 'matchLen > expectedOutputSize - outPos')
RequireContains "src\parsers\ios_app_db_parser.cpp" @('<invalid_dictionary_bounds>', '<dictionary_too_large>', 'callCount > 2500U', '<seen_uid_', 'UTF-16LE fallback', 'LIMIT ? OFFSET ?', 'sqlite3_bind_int64(st, 2, offset)', 'IDENTITY_PROMOTION_SUPPRESSED_FOR_COREDUET_INTERACTIONC=True')
RequireContains "src\gui\win32_gui.cpp" @('schemaEnsuredPaths_', 'Folder path unavailable', 'Folder path too long', 'vw_ios_spotlight_comms_missing_from_ffs')
RequireContains "src\db\case_db.cpp" @('vw_ios_spotlight_comms_missing_from_ffs', 'vw_active_file_comparison_validation_checks', 'vw_ios_coreduet_interactionc_validation_checks')
RequireContains "src\enrich_sql\sqlite_enrichment.cpp" @('MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE', 'investigative lead only', 'COMPLETED_IOS_FFS_EXACT_PATH_AND_REFERENCE_LOOKUP')
RequireContains "src\export_sql\sqlite_exporter.cpp" @('active_file_comparison_validation_checks_sample.csv', 'ios_coreduet_interactionc_validation_checks_sample.csv')

foreach ($doc in @('docs\START_CONTINUATION_CHAT.md', 'docs\V1_6_29_CODE_REVIEW_VALIDATION_HARDENING.md', 'RELEASE_NOTES.md', 'ai_context.md', 'KNOWN_ISSUES.md', 'BUILD_INSTRUCTIONS.md')) {
  $null = ReadText $doc
}

foreach ($relative in @('scripts', 'tools', 'src')) {
  Get-ChildItem -LiteralPath (Join-Path $SourceRoot $relative) -Recurse -File | ForEach-Object {
    if ($_.Name -eq 'Verify-V1_6_29_4-ReleaseReadiness.ps1') { return }
    $txt = Get-Content -LiteralPath $_.FullName -Raw -ErrorAction SilentlyContinue
    if ($txt -match '1\.6\.28|1\\\.6\\\.28|V1_6_28') { throw "$($_.FullName) contains stale V1.6.28 marker" }
    if ($txt -match 'VestigantSpotlightInv_V1_6_29\.zip|V1_6_29_build\.log|Upload_Thin_iOS_CoreSpotlight_V1_6_29\.zip') { throw "$($_.FullName) contains stale V1.6.29 artifact name" }
  }
}

Write-Host "Release readiness passed for $expected"
