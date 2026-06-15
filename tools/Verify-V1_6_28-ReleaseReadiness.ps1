param([string]$SourceRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = "Stop"
$expected = "1.6.28"
$expectedToken = "V1_6_28"
function ReadText($Relative) {
  $p = Join-Path $SourceRoot $Relative
  if (!(Test-Path -LiteralPath $p)) { throw "Missing required file: $Relative" }
  return Get-Content -LiteralPath $p -Raw
}
$appInfo = ReadText "src\core\app_info.cpp"
if ($appInfo -notmatch [regex]::Escape('return "' + $expected + '"')) { throw "app_info.cpp does not contain expected version $expected" }
if ((ReadText "VERSION") -notmatch '^1\.6\.28\s*$') { throw "VERSION is not $expected" }
if ((ReadText "VERSION.txt") -notmatch '^1\.6\.28\s*$') { throw "VERSION.txt is not $expected" }
if ((ReadText "CMakeLists.txt") -notmatch 'VERSION 1\.6\.28') { throw "CMakeLists.txt project version is not $expected" }
$build = ReadText "scripts\Build-V1_6_28.ps1"
if ($build -notmatch [regex]::Escape('Verify-V1_6_28-ReleaseReadiness.ps1')) { throw "Build wrapper does not call current release-readiness script" }
$cliVersionRegexPin = [regex]::Escape('1.6.28')
$cliVersionLiteralPin = [regex]::Escape('1.6.28')
if (($build -notmatch $cliVersionRegexPin) -and ($build -notmatch $cliVersionLiteralPin)) { throw "Build wrapper CLI version validation is not pinned to $expected" }
$staleRegex = ('1\.6\.' + '6\.6|' + 'V1_6_' + '6_6')
if ($build -match $staleRegex) { throw "Build wrapper contains stale previous-version reference" }
foreach ($script in @("scripts\Run-V1_6_28-iOS-CoreSpotlight-AndZip.ps1", "scripts\Run-V1_6_28-iOS-Production-AndZip.ps1", "scripts\Launch-V1_6_28-GUI.ps1")) {
  $txt = ReadText $script
  if ($txt -match $staleRegex) { throw "$script contains stale previous-version reference" }
}
$prod = ReadText "scripts\Run-V1_6_28-iOS-Production-AndZip.ps1"
foreach ($needle in @('RunMode = "run"', 'ExportProfile = "investigator"', 'ForceContainerHash = $true', 'FullNativeValues = $true', 'ios_production_readiness_summary.csv')) {
  if ($prod -notmatch [regex]::Escape($needle)) { throw "Production iOS wrapper missing: $needle" }
}
$runner = ReadText "tools\Run-IosCoreSpotlightFocusedZip.ps1"
foreach ($needle in @('ForceContainerHash', '--force-container-hash', 'FullNativeValues', '--experimental-full-native-values', 'MaterializeIosSupportDb', '--materialize-ios-support-db', 'RunMode', 'PRODUCTION_PERFORMANCE_SUMMARY.md')) {
  if ($runner -notmatch [regex]::Escape($needle)) { throw "iOS focused runner missing production switch support: $needle" }
}
foreach ($needle in @('requiredUploadSamples', 'artifacts_sample.csv', 'raw_key_values_sample.csv', 'timeline_events_sample.csv', 'usage_evidence_sample.csv', 'ios_coreduet_interactionc_database_status_sample.csv', 'ios_coreduet_interactionc_summary_sample.csv', 'ios_coreduet_interactionc_events_sample.csv', 'parser_coverage_summary_sample.csv', 'missing required bounded validation samples')) {
  if ($runner -notmatch [regex]::Escape($needle)) { throw "iOS focused runner missing V1.6.28 upload-sample ZIP validation marker: $needle" }
}
foreach ($needle in @('DiagnosticFullNativeDb', '--diagnostic-full-native-db', 'MaxNativeRecords', '--max-native-records')) {
  if ($runner -notmatch [regex]::Escape($needle)) { throw "iOS focused runner missing bounded full-native validation marker: $needle" }
}
foreach ($needle in @('incomplete_run_diagnostic_bundle', 'CLI exit code was $cliExit', 'missing_required_upload_samples=', 'instead of failing post-upload validation')) {
  if ($runner -notmatch [regex]::Escape($needle)) { throw "iOS focused runner missing V1.6.28 incomplete-run diagnostic preservation marker: $needle" }
}
$thinWrapper = ReadText 'scripts\Run-V1_6_28-iOS-CoreSpotlight-AndZip.ps1'
foreach ($needle in @('upload ZIP retained for diagnostics', 'Upload this incomplete-run diagnostic bundle for review', '$global:LASTEXITCODE = 0')) {
  if ($thinWrapper -notmatch [regex]::Escape($needle)) { throw "Thin wrapper missing V1.6.28 diagnostic upload retention marker: $needle" }
}
foreach ($script in @('scripts\Run-V1_6_28-iOS-ValidationSupport-AndZip.ps1', 'scripts\Run-V1_6_28-iOS-BoundedFullNativeDb-AndZip.ps1')) {
  $txt = ReadText $script
  if ($txt -match $staleRegex) { throw "$script contains stale previous-version reference" }
}
$validationSupport = ReadText 'scripts\Run-V1_6_28-iOS-ValidationSupport-AndZip.ps1'
foreach ($needle in @('ExportProfile = "support"', 'ForceContainerHash = $true', 'FullNativeValues = $true', 'MaterializeIosSupportDb = $true')) {
  if ($validationSupport -notmatch [regex]::Escape($needle)) { throw "Validation support wrapper missing: $needle" }
}
$boundedFullNative = ReadText 'scripts\Run-V1_6_28-iOS-BoundedFullNativeDb-AndZip.ps1'
foreach ($needle in @('DiagnosticFullNativeDb = $true', 'MaxNativeRecords = $MaxNativeRecords', 'ExportProfile = "diagnostics"')) {
  if ($boundedFullNative -notmatch [regex]::Escape($needle)) { throw "Bounded full-native wrapper missing: $needle" }
}
foreach ($doc in @('docs\IOS_INVESTIGATION_VALIDATION_WORKFLOW.md', 'docs\GUARDRAIL_RETIREMENT_PLAN.md', 'docs\START_CONTINUATION_CHAT.md')) {
  $null = ReadText $doc
}

$activeFsRoadmap = ReadText 'docs\ACTIVE_FILESYSTEM_COMPARISON_ROADMAP.md'
foreach ($needle in @('V1.6.28 implements Phase 1', 'ios_ffs_file_inventory', 'ios_ffs_path_lookup', 'PRESENT_IN_IOS_FFS_EXACT_PATH', 'MISSING_FROM_IOS_FFS_EXACT_PATH_CANDIDATE', 'investigative lead only')) {
  if ($activeFsRoadmap -notmatch [regex]::Escape($needle)) { throw "ACTIVE_FILESYSTEM_COMPARISON_ROADMAP.md missing V1.6.28 active filesystem marker: $needle" }
}
$enrichSql = ReadText 'src\enrich_sql\sqlite_enrichment.cpp'
if ($enrichSql -match [regex]::Escape('tabled in v0.6.4')) { throw 'sqlite_enrichment.cpp still contains stale v0.6.4 active filesystem comparison wording' }

foreach ($needle in @('PRESENT_IN_IOS_FFS_EXACT_PATH', 'MISSING_FROM_IOS_FFS_EXACT_PATH_CANDIDATE', 'COMPLETED_IOS_FFS_EXACT_PATH_LOOKUP', 'IOS_FFS_EXACT_PATH_LOOKUP', 'temp_ios_active_ffs_paths')) {
  if ($enrichSql -notmatch [regex]::Escape($needle)) { throw "sqlite_enrichment.cpp missing V1.6.28 iOS active filesystem marker: $needle" }
}
$caseDbTextForActive = ReadText 'src\db\case_db.cpp'
foreach ($needle in @('latest_active_runs', 'PRESENT_IN_IOS_FFS_EXACT_PATH', 'MISSING_FROM_IOS_FFS_EXACT_PATH_CANDIDATE', 'IOS_FFS_EXACT_PATH_LOOKUP')) {
  if ($caseDbTextForActive -notmatch [regex]::Escape($needle)) { throw "case_db.cpp missing V1.6.28 active filesystem marker: $needle" }
}

$cliMain = ReadText 'src\cli\main.cpp'
if ($cliMain -match [regex]::Escape('tabled for this build')) { throw 'cli/main.cpp still contains stale active filesystem comparison wording' }


$iosParser = ReadText "src\parsers\ios_app_db_parser.cpp"
foreach ($needle in @('if (t == "zinteractions") return "COREDUET_INTERACTIONS"', '"COREDUET_INTERACTIONS"', 'summarizeSqliteRowValues', 'parsed_coreduet_interactionc_generic_row', 'schema_tolerant_v1_6_24', 'no_direct_communication_conclusion=True', 'IDENTITY_PROMOTION_SUPPRESSED_FOR_COREDUET_INTERACTIONC=True')) {
  if ($iosParser -notmatch [regex]::Escape($needle)) { throw "ios_app_db_parser.cpp missing V1.6.28 interactionC parser marker: $needle" }
}


if ($iosParser -match [regex]::Escape('contains(t, "interaction") && !contains(t, "recipient")')) { throw "ios_app_db_parser.cpp still has broad interaction-table promotion; V1.6.28 should target canonical ZINTERACTIONS only" }

$hashCpp = ReadText "src\core\hash.cpp"
foreach ($needle in @('sha256FileWithProgress', 'progressCallback', 'bytesRead += static_cast<std::uintmax_t>(got)')) {
  if ($hashCpp -notmatch [regex]::Escape($needle)) { throw "hash.cpp missing production hash progress support: $needle" }
}
if ([regex]::Matches($hashCpp, 'std::vector<unsigned char> buffer\(4 \* 1024 \* 1024\);').Count -ne 1) { throw "hash.cpp has duplicate or missing Windows hash buffer declaration" }
$appRunner = ReadText "src\app\app_runner.cpp"
foreach ($needle in @('original_container_hash_start', 'original_container_hash_progress', 'original_container_hash_complete', 'sha256FileWithProgress')) {
  if ($appRunner -notmatch [regex]::Escape($needle)) { throw "app_runner.cpp missing hash progress status: $needle" }
}
$uploadTool = ReadText "tools\Create-SourceProbeUploadZip.ps1"
foreach ($needle in @('exports/ios_production_readiness_summary.csv', 'production_performance_summary.csv', 'PRODUCTION_PERFORMANCE_SUMMARY.md', 'exports/EXPORT_INDEX.csv', 'wrapper_heartbeat.log')) {
  if ($uploadTool -notmatch [regex]::Escape($needle)) { throw "upload tool missing production bundle artifact: $needle" }
}

$guiExportWorker = ReadText "src\gui\gui_export_worker.cpp"
foreach ($needle in @('std::wstring joinPath', 'std::wstring sanitizeExportName', 'void ensureFolderExistsNoThrow')) {
  if ($guiExportWorker -notmatch [regex]::Escape($needle)) { throw "GUI export worker is not self-contained for helper: $needle" }
}
$iosParser = ReadText "src\parsers\ios_app_db_parser.cpp"
foreach ($needle in @('looksLikeAppleAbsoluteTimeNumeric', 'IDENTITY_PROMOTION_SUPPRESSED=True', 'KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY', 'KNOWLEDGEC_COMMUNICATION_INTENT', 'resolveUid', 'mailto:', 'tel:')) {
  if ($iosParser -notmatch [regex]::Escape($needle)) { throw "iOS parser missing guardrail/recovery marker: $needle" }
}
$caseDb = ReadText "src\db\case_db.cpp"
foreach ($needle in @('KNOWLEDGEC_COMMUNICATION_INTENT', 'KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY', 'IDENTITY_PROMOTION_SUPPRESSED=True', 'vw_ios_spotlight_comms_missing_from_ffs', 'vw_ios_production_readiness_summary', 'PRODUCTION_READY_HASH_RECORDED')) {
  if ($caseDb -notmatch [regex]::Escape($needle)) { throw "case_db.cpp missing V1.6.28 required view/guardrail: $needle" }
}
if ($caseDb -match "KNOWLEDGEC_EVENTS'\) THEN 'parsed_communication_category") { throw "Generic KNOWLEDGEC_EVENTS still auto-promotes as parsed communication category" }
function Test-NoGenericKnowledgeCInPromotionalSql {
  param([Parameter(Mandatory=$true)][string]$Text,[Parameter(Mandatory=$true)][string]$Label)
  $promotionalViews = @('vw_ios_communication_frequency','vw_ios_communication_existence_evidence','vw_ios_communication_identity_frequency','vw_ios_communication_temporal_frequency','vw_ios_communication_source_coverage','vw_ios_identity_activity_linkage','vw_ios_identity_activity_detail_sample','vw_ios_identity_entity_rollup')
  foreach ($view in $promotionalViews) {
    $escapedView = [regex]::Escape($view)
    $m = [regex]::Match($Text, "CREATE VIEW(?: IF NOT EXISTS)?\s+$escapedView\s+AS(?<body>.*?);", [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if (!$m.Success) { continue }
    $body = $m.Groups['body'].Value
    $withoutSuppressionGuards = [regex]::Replace($body, "AND\s+NOT\s*\(\s*record_category\s+IN\s*\([^)]*KNOWLEDGEC_EVENTS[^)]*\).*?\)", "", [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if ($withoutSuppressionGuards -match "record_category\s+IN\s*\([^)]*KNOWLEDGEC_EVENTS[^)]*\)") { throw "$Label has generic KNOWLEDGEC_EVENTS in promotional predicate for $view" }
  }
}
Test-NoGenericKnowledgeCInPromotionalSql -Text $caseDb -Label "case_db.cpp"
$guiText = ReadText "src\gui\win32_gui.cpp"
foreach ($needle in @('LRESULT CALLBACK ReviewListSubclassProc', 'LRESULT CALLBACK ReviewDetailsSplitterSubclassProc', 'SetWindowSubclass(gList, ReviewListSubclassProc', 'SetWindowSubclass(gRowDetailsSplitter, ReviewDetailsSplitterSubclassProc')) {
  if ($guiText -notmatch [regex]::Escape($needle)) { throw "win32_gui.cpp missing V1.6.28 GUI subclass hotfix marker: $needle" }
}
Test-NoGenericKnowledgeCInPromotionalSql -Text $guiText -Label "win32_gui.cpp"
foreach ($needle in @('vw_ios_spotlight_comms_missing_from_ffs', 'vw_ios_production_readiness_summary', 'PRODUCTION_READY_HASH_RECORDED', 'COMMUNICATION_PRESENT_IN_SPOTLIGHT_NOT_MATCHED_TO_NATIVE_APP_DB', 'deletion, app removal, encryption/inaccessibility, parser coverage limits', 'iOS - Production Readiness Summary')) {
  if ($guiText -notmatch [regex]::Escape($needle)) { throw "win32_gui.cpp missing iOS production/native-DB view coverage: $needle" }
}
$registry = ReadText "src\gui\view_registry.cpp"
foreach ($needle in @('vw_ios_production_readiness_summary', 'iOS - Production Readiness Summary', 'source_field_name', 'native_probe_context_count', 'native_probe_context_sample')) {
  if ($registry -notmatch [regex]::Escape($needle)) { throw "view_registry.cpp missing production/native probe column: $needle" }
}

foreach ($needle in @('vw_ios_coreduet_interactionc_database_status', 'vw_ios_coreduet_interactionc_summary', 'vw_ios_coreduet_interactionc_event_review', 'iOS - CoreDuet interactionC Database Status')) {
  if ($registry -notmatch [regex]::Escape($needle)) { throw "view_registry.cpp missing V1.6.28 CoreDuet interactionC workflow marker: $needle" }
}
foreach ($needle in @('vw_ios_coreduet_interactionc_database_status', 'vw_ios_coreduet_interactionc_summary', 'vw_ios_coreduet_interactionc_event_review', 'INTERACTIONC_DB_OR_COREDUET_PEOPLE_PATH')) {
  if ($caseDb -notmatch [regex]::Escape($needle)) { throw "case_db.cpp missing V1.6.28 CoreDuet interactionC workflow marker: $needle" }
}
$exporter = ReadText "src\export_sql\sqlite_exporter.cpp"
foreach ($needle in @('ios_coreduet_interactionc_database_status.csv', 'ios_coreduet_interactionc_summary.csv', 'ios_coreduet_interactionc_database_status_sample.csv', 'ios_coreduet_interactionc_summary_sample.csv')) {
  if ($exporter -notmatch [regex]::Escape($needle)) { throw "sqlite_exporter.cpp missing V1.6.28 CoreDuet interactionC export marker: $needle" }
}
$nativeParser = ReadText "src\parsers\native_storedb_parser.cpp"
$exporter = ReadText "src\export_sql\sqlite_exporter.cpp"
foreach ($needle in @('__native_probe_', '__native_core_probe_string_', '_candidate_', 'incorrectly inserted into raw_date_candidates')) {
  if ($nativeParser -notmatch [regex]::Escape($needle)) { throw "native_storedb_parser.cpp missing V1.6.28 date-candidate alias guard: $needle" }
}
foreach ($needle in @('extractFirstEmailCandidate', 'isLikelyEmailCandidate', 'isEmailLocalPartChar', 'isEmailDomainChar')) {
  if ($nativeParser -notmatch [regex]::Escape($needle)) { throw "native_storedb_parser.cpp missing V1.6.28 semantic email alias precision marker: $needle" }
}
foreach ($needle in @('__native_probe_email_candidate_%', "v NOT LIKE '%file:%'", "v NOT LIKE '%/%'", "v NOT LIKE '% %'")) {
  if ($caseDb -notmatch [regex]::Escape($needle)) { throw "case_db.cpp missing V1.6.28 iOS email-category precision marker: $needle" }
}
foreach ($needle in @('__native_probe_email_candidate_%', "v NOT LIKE '%file:%'", "v NOT LIKE '%/%'", "v NOT LIKE '% %'")) {
  if ($exporter -notmatch [regex]::Escape($needle)) { throw "sqlite_exporter.cpp missing V1.6.28 iOS email-category export precision marker: $needle" }
}
foreach ($needle in @('lower.compare(end, 8, "https://")', 'lower.compare(end, 7, "http://")', 'lower.compare(end, 7, "file://")')) {
  if ($nativeParser -notmatch [regex]::Escape($needle)) { throw "native_storedb_parser.cpp missing V1.6.28 URL/path concatenation guard marker: $needle" }
}
foreach ($needle in @('isUsefulDerivedBasenameCandidate', 'highBytes', 'filenameSafe', 'punctuationNoise', 'Keep raw __native_core_probe_string_## evidence untouched')) {
  if ($nativeParser -notmatch [regex]::Escape($needle)) { throw "native_storedb_parser.cpp missing V1.6.28 basename precision marker: $needle" }
}
$enrich = ReadText "src\enrich_sql\sqlite_enrichment.cpp"
foreach ($needle in @('temp_parent_inode_path_candidates', 'ANCESTOR_PATH_PLUS_CHILD_CHAIN', 'RELATIVE_PARENT_INODE_NAME_CHAIN', 'PARENT_INODE_CHAIN_RECONSTRUCTION', 'parent_inode_links_with_new_reconstructed_path', 'parent_inode_links_with_existing_path_context')) {
  if ($enrich -notmatch [regex]::Escape($needle)) { throw "sqlite_enrichment.cpp missing V1.6.28 parent-path reconstruction marker: $needle" }
}
if ($appRunner -notmatch [regex]::Escape('aff4_apfs_staged_storev2_path_reconstruction_sample.csv')) { throw "app_runner.cpp missing V1.6.28 AFF4 path reconstruction sample export" }
if ($appRunner -notmatch [regex]::Escape('aff4_apfs_staged_storev2_path_reconstruction_metrics_sample.csv')) { throw "app_runner.cpp missing V1.6.28 AFF4 path reconstruction metrics sample export" }
if ($uploadTool -notmatch [regex]::Escape('aff4_apfs_staged_storev2_path_reconstruction_sample.csv')) { throw "upload tool missing V1.6.28 AFF4 path reconstruction sample" }
if ($uploadTool -notmatch [regex]::Escape('aff4_apfs_staged_storev2_path_reconstruction_metrics_sample.csv')) { throw "upload tool missing V1.6.28 AFF4 path reconstruction metrics sample" }

$tests = ReadText "tests\main.cpp"
foreach ($needle in @('runKnowledgeCIdentitySuppressionSmokeTest', 'runIosCoreProbeTextContextSmokeTest', 'runIosProductionReadinessSmokeTest', 'runParentInodePathReconstructionSmokeTest', 'vw_ios_production_readiness_summary')) {
  if ($tests -notmatch [regex]::Escape($needle)) { throw "Self-test missing V1.6.28 coverage: $needle" }
}
$exporter = ReadText "src\export_sql\sqlite_exporter.cpp"
foreach ($needle in @('ios_production_readiness_summary.csv', 'FORCE_CONTAINER_HASH_REQUESTED', 'THIN_PROFILE_DIRECT_IDENTITY_PIVOT', 'THIN_PROFILE_DIRECT_CANDIDATE_SAMPLE', 'THIN_PROFILE_NOT_EVALUATED')) {
  if ($exporter -notmatch [regex]::Escape($needle)) { throw "sqlite_exporter.cpp missing V1.6.28 production/thin guard: $needle" }
}
$raw = & powershell -ExecutionPolicy Bypass -File (Join-Path $SourceRoot "tools\Verify-MsvcStringLiteralRisk.ps1") -SourceRoot $SourceRoot
Write-Host $raw
$forensic = Join-Path $SourceRoot "tools\Verify-V1_6_28-ForensicDirectives.ps1"
powershell -ExecutionPolicy Bypass -File $forensic -SourceRoot $SourceRoot
Write-Host "Release readiness check passed for $expected"
