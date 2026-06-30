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
  "BuildAndRun-$token-FromDownloadedZip.ps1",
  "BuildAndRunThin-$token-FromDownloadedZip.ps1",
  "BuildAndRunAff4Probe-$token-FromDownloadedZip.ps1",
  "scripts\BuildAndRun-$token-FromDownloadedZip.ps1",
  "scripts\BuildAndRunThin-$token-FromDownloadedZip.ps1",
  "scripts\BuildAndRunAff4Probe-$token-FromDownloadedZip.ps1",
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
RequireContains "src\parsers\native_storedb_parser.cpp" @('native_parse_store_persistence_mode', 'macos_storev2', 'ios_corespotlight_compact', 'writeProgressPair(parent.parent_path() / progressPath.filename())', 'utf8FromByteRange', 'const auto storeDir = pathString(store.storePath.parent_path())')
RequireContains "src\app\app_runner.cpp" @('native_kv_persistence_macos_storev2', 'native_parse_configuration', 'NativePersistenceMode::MacOSStoreV2', 'NativePersistenceMode::IosCoreSpotlightCompact', 'SevenZipEntryRecord', 'native_cpp_7z_slt_inventory_parser_fast_record_state_v1_6_64')
RequireContains "src\parsers\aff4_probe_worker.cpp" @('VOLUME_OMAP_VERTICAL_CYCLE_DETECTED', 'literalLen > srcSize - pos', 'literalLen > expectedOutputSize - outPos', 'matchLen > expectedOutputSize - outPos', 'direct_map_stream_select', 'No AFF4 stream base with both /idx and /map entries')
RequireContains "src\parsers\apfs_volume_reader.cpp" @('NXSB_FOUND_BLOCK_SIZE_REJECTED', 'outside the accepted APFS power-of-two range 4096..65536', 'rejected before allocation/use')
RequireContains "src\parsers\ios_app_db_parser.cpp" @('<invalid_dictionary_bounds>', '<dictionary_too_large>', 'callCount > 2500U', '<seen_uid_', 'UTF-16LE fallback', 'LIMIT ? OFFSET ?', 'sqlite3_bind_int64(st, 2, offset)', 'IDENTITY_PROMOTION_SUPPRESSED_FOR_COREDUET_INTERACTIONC=True')
RequireContains "src\gui\win32_gui.cpp" @('schemaEnsuredPaths_', 'Folder path unavailable', 'Folder path too long', 'vw_ios_spotlight_comms_missing_from_ffs', 'isArtifactCheckedNoThrow', 'clearCheckedArtifactIdsNoThrow', 'gReviewThread.detach()', 'worker.detach()', 'Full extraction / no guardrails', 'opt.reuseIosCache.clear()', 'raw evidence source is fully enumerated/extracted', 'cache reuse is disabled', 'aff4SourceSelected', 'opt.mode = "source-probe"', 'opt.strictSingleAff4 = true', 'opt.enableAff4DynamicProbe = true', 'normal-run unsupported-container failure path')
RequireContains "src\db\case_db.cpp" @('vw_ios_spotlight_comms_missing_from_ffs', 'vw_active_file_comparison_validation_checks', 'vw_ios_coreduet_interactionc_validation_checks')
RequireContains "src\enrich_sql\sqlite_enrichment.cpp" @('MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE', 'investigative lead only', 'COMPLETED_IOS_FFS_EXACT_PATH_AND_REFERENCE_LOOKUP', 'active_comparison_reference_materialize_start', 'active_comparison_complete')
RequireContains "src\export_sql\sqlite_exporter.cpp" @('active_file_comparison_validation_checks_sample.csv', 'ios_coreduet_interactionc_validation_checks_sample.csv', 'writeCsvFieldFast(std::ofstream& out, const char* text, int byteLen = -1)', 'sqlite3_column_bytes(stmt, i)', '[NUL]', 'reuseFullExportAsSample', 'export_upload_sample_reuse', 'V1.6.115 avoids duplicate SQL materialization', 'prepareThinIosExportCaches', 'export_thin_ios_normalized_timeline_cache_complete', 'export_thin_ios_communication_record_cache_complete', 'export_thin_ios_message_text_cache_complete', 'export_thin_ios_text_context_cache_complete', 'temp_export_ios_communication_record_review', 'temp_export_ios_message_text_review', 'temp_export_ios_text_context_review', 'ExternalSourceSha256', 'temp_export_ios_entity_summary')

# Documentation checks are advisory. Missing docs should not prevent an MSVC build from starting.
WarnIfMissing "ai_context.md" @($expected, 'first-fix reference')
WarnIfMissing "docs\PROJECT_REFERENCE_V1_6_115.md" @($expected, 'red-team', 'date-count metric cache', 'Markdown files consolidated')
WarnIfMissing "docs\START_CONTINUATION_CHAT.md" @($expected, 'Continue the Vestigant Spotlight / Spotlight2 Project from V1.6.115', 'enrichment_metrics_date_count_cache_complete')

RequireContains "src\enrich_sql\sqlite_enrichment.cpp" @('enrichment_timeline_insert_start', 'enrichment_timeline_candidate_cache_complete', 'enrichment_timeline_insert_artifact_id_complete', 'enrichment_timeline_insert_complete', 'temp_timeline_date_candidates', 'enrichment_metrics_date_count_cache_start', 'enrichment_metrics_date_count_cache_complete', 'temp_record_date_counts', 'export_finalization_case_review_summary_start', 'export_finalization_investigator_dashboard_start', 'export_finalization_export_index_complete', 'export_case_summary_counts_complete', 'export_case_summary_communication_start', 'export_case_summary_top_date_fields_complete')
RequireContains "BuildAndRunThin-V1_6_115-FromDownloadedZip.ps1" @('UseFastLocalCaseRoot', 'FastLocalRoot', 'TestIOS_CoreSpotlight_V1_6_115', 'IosReuseCacheWrapperLogEntries', 'Flush-IosReuseCacheWrapperLog', 'preserves entries if -CleanOut removed the initial case log')
RequireContains "scripts\BuildAndRunThin-V1_6_115-FromDownloadedZip.ps1" @('UseFastLocalCaseRoot', 'FastLocalRoot', 'TestIOS_CoreSpotlight_V1_6_115', 'IosReuseCacheWrapperLogEntries', 'Flush-IosReuseCacheWrapperLog', 'preserves entries if -CleanOut removed the initial case log')
RequireContains "BuildAndRunAff4Probe-V1_6_115-FromDownloadedZip.ps1" @('Aff4Path', 'FullNoGuardrails', 'Run-SingleAff4SourceProbeAndZip.ps1', 'EnableAff4DynamicProbe', 'EnableAff4VirtualApfsProbe')
RequireContains "scripts\BuildAndRunAff4Probe-V1_6_115-FromDownloadedZip.ps1" @('Aff4Path', 'FullNoGuardrails', 'Run-SingleAff4SourceProbeAndZip.ps1', 'EnableAff4DynamicProbe', 'EnableAff4VirtualApfsProbe')
RequireContains "tools\Run-SingleAff4SourceProbeAndZip.ps1" @('Reader tools: not supplied', 'dynamic AFF4 probe will rely on environment/PATH', 'if (![string]::IsNullOrWhiteSpace($ReaderToolsRoot))')
RequireContains "scripts\Run-V1_6_115-macOS-AFF4-Probe-AndZip.ps1" @('FullNoGuardrails', 'RequireStoreV2ValidationOutputs', 'Partial AFF4/APFS diagnostic uploads are allowed')

RequireContains "BuildAndRun-V1_6_115-FromDownloadedZip.ps1" @('IOSCoreSpotlightThin', 'AFF4Probe', 'BuildOnly', 'SelectedWorkflow', 'BuildAndRunThin-V1_6_115-FromDownloadedZip.ps1', 'BuildAndRunAff4Probe-V1_6_115-FromDownloadedZip.ps1', 'documentation placeholder')
RequireContains "scripts\BuildAndRun-V1_6_115-FromDownloadedZip.ps1" @('IOSCoreSpotlightThin', 'AFF4Probe', 'BuildOnly', 'SelectedWorkflow', 'BuildAndRunThin-V1_6_115-FromDownloadedZip.ps1', 'BuildAndRunAff4Probe-V1_6_115-FromDownloadedZip.ps1', 'documentation placeholder')
RequireContains "scripts\Build-V1_6_115.ps1" @('VestigantSpotlightTests.exe', 'Running required self-test', 'Self-test completed', 'SkipSelfTest', 'Invoke-SelfTestProcessToLog', 'stderr alone is not a failure')

Write-Host "Release readiness advisory passed for $expected"

RequireContains "src\app\app_runner.cpp" @('original_container_external_hash_recorded', 'EXTERNAL_HASH_RECORDED', 'externalSourceSha256')
RequireContains "src\cli\main.cpp" @('--external-source-sha256', '--external-source-hash-note')
RequireContains "src\db\case_db.cpp" @('external_source_sha256', 'PRODUCTION_READY_EXTERNAL_HASH_RECORDED')
RequireContains "tools\Run-IosCoreSpotlightFocusedZip.ps1" @('ExternalSourceSha256', '--external-source-sha256')
