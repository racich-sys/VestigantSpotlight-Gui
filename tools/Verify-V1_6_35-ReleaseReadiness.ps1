param([string]$SourceRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = "Stop"

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
function WarnIfMissing($Relative, [string[]]$Needles) {
  try { $txt = ReadText $Relative } catch { Write-Warning $_.Exception.Message; return }
  foreach ($needle in $Needles) {
    if ($txt -notmatch [regex]::Escape($needle)) { Write-Warning "$Relative missing advisory marker: $needle" }
  }
}

$expected = (ReadText "VERSION").Trim()
if ([string]::IsNullOrWhiteSpace($expected)) { throw "VERSION is empty" }
$expectedRegex = [regex]::Escape($expected)

if ((ReadText "VERSION.txt").Trim() -ne $expected) { throw "VERSION.txt is not $expected" }
if ((ReadText "CMakeLists.txt") -notmatch ("VERSION\s+" + $expectedRegex)) { throw "CMakeLists.txt project version is not $expected" }
RequireContains "src\core\app_info.cpp" @('return "' + $expected + '"')

$token = "V" + ($expected -replace '\.', '_')
$buildScript = "scripts\Build-$token.ps1"
$null = ReadText $buildScript
foreach ($script in @(
  "scripts\Run-$token-iOS-CoreSpotlight-AndZip.ps1",
  "scripts\Run-$token-iOS-ValidationSupport-AndZip.ps1",
  "scripts\Run-$token-iOS-BoundedFullNativeDb-AndZip.ps1",
  "scripts\Run-$token-iOS-Production-AndZip.ps1",
  "scripts\Launch-$token-GUI.ps1"
)) {
  $null = ReadText $script
}

# Fatal source-code safety markers that represent actual build/run behavior.
RequireContains "src\parsers\native_storedb_parser.h" @('NativePersistenceMode', 'MacOSStoreV2', 'IosCoreSpotlightCompact')
RequireContains "src\parsers\native_storedb_parser.cpp" @('native_parse_store_persistence_mode', 'macos_storev2', 'ios_corespotlight_compact', 'writeProgressPair(parent.parent_path() / progressPath.filename())')
RequireContains "src\app\app_runner.cpp" @('native_kv_persistence_macos_storev2', 'native_parse_configuration', 'NativePersistenceMode::MacOSStoreV2', 'NativePersistenceMode::IosCoreSpotlightCompact')
RequireContains "src\parsers\aff4_probe_worker.cpp" @('VOLUME_OMAP_VERTICAL_CYCLE_DETECTED', 'literalLen > srcSize - pos', 'literalLen > expectedOutputSize - outPos', 'matchLen > expectedOutputSize - outPos')
RequireContains "src\parsers\apfs_volume_reader.cpp" @('NXSB_FOUND_BLOCK_SIZE_REJECTED', 'outside the accepted APFS power-of-two range 4096..65536', 'rejected before allocation/use')
RequireContains "src\parsers\ios_app_db_parser.cpp" @('<invalid_dictionary_bounds>', '<dictionary_too_large>', 'callCount > 2500U', '<seen_uid_', 'UTF-16LE fallback', 'LIMIT ? OFFSET ?', 'sqlite3_bind_int64(st, 2, offset)', 'IDENTITY_PROMOTION_SUPPRESSED_FOR_COREDUET_INTERACTIONC=True')
RequireContains "src\gui\win32_gui.cpp" @('schemaEnsuredPaths_', 'Folder path unavailable', 'Folder path too long', 'vw_ios_spotlight_comms_missing_from_ffs', 'isArtifactCheckedNoThrow', 'clearCheckedArtifactIdsNoThrow', 'gReviewThread.detach()', 'worker.detach()')
RequireContains "src\db\case_db.cpp" @('vw_ios_spotlight_comms_missing_from_ffs', 'vw_active_file_comparison_validation_checks', 'vw_ios_coreduet_interactionc_validation_checks')
RequireContains "src\enrich_sql\sqlite_enrichment.cpp" @('MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE', 'investigative lead only', 'COMPLETED_IOS_FFS_EXACT_PATH_AND_REFERENCE_LOOKUP')
RequireContains "src\export_sql\sqlite_exporter.cpp" @('active_file_comparison_validation_checks_sample.csv', 'ios_coreduet_interactionc_validation_checks_sample.csv', 'writeCsvFieldFast(std::ofstream& out, const char* text, int byteLen = -1)', 'sqlite3_column_bytes(stmt, i)', '[NUL]')

# Documentation checks are advisory. Missing docs should not prevent an MSVC build from starting.
WarnIfMissing "docs\START_CONTINUATION_CHAT.md" @($expected)
WarnIfMissing "docs\V1_6_29_CODE_REVIEW_VALIDATION_HARDENING.md" @('V1.6.29')
WarnIfMissing ("docs\" + $token + "_RELEASE_PREFLIGHT_HARDENING.md") @($expected)
WarnIfMissing "RELEASE_NOTES.md" @($expected)
WarnIfMissing "ai_context.md" @($expected)
WarnIfMissing "KNOWN_ISSUES.md" @($expected)
WarnIfMissing "BUILD_INSTRUCTIONS.md" @($expected)

Write-Host "Release readiness advisory passed for $expected"
