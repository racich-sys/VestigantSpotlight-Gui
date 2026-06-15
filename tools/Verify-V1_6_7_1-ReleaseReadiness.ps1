param([string]$SourceRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = "Stop"
$expected = "1.6.7.1"
$expectedToken = "V1_6_7_1"
function ReadText($Relative) {
  $p = Join-Path $SourceRoot $Relative
  if (!(Test-Path -LiteralPath $p)) { throw "Missing required file: $Relative" }
  return Get-Content -LiteralPath $p -Raw
}
$appInfo = ReadText "src\core\app_info.cpp"
if ($appInfo -notmatch [regex]::Escape('return "' + $expected + '"')) { throw "app_info.cpp does not contain expected version $expected" }
if ((ReadText "VERSION") -notmatch '^1\.6\.7\.1\s*$') { throw "VERSION is not $expected" }
if ((ReadText "VERSION.txt") -notmatch '^1\.6\.7\.1\s*$') { throw "VERSION.txt is not $expected" }
if ((ReadText "CMakeLists.txt") -notmatch 'VERSION 1\.6\.7\.1') { throw "CMakeLists.txt project version is not $expected" }
$build = ReadText "scripts\Build-V1_6_7_1.ps1"
if ($build -notmatch [regex]::Escape('Verify-V1_6_7_1-ReleaseReadiness.ps1')) { throw "Build wrapper does not call current release-readiness script" }
if ($build -notmatch '1\.6\.7\.1') { throw "Build wrapper CLI version validation is not pinned to $expected" }
$staleRegex = ('1\.6\.' + '6\.6|' + 'V1_6_' + '6_6')
if ($build -match $staleRegex) { throw "Build wrapper contains stale previous-version reference" }
foreach ($script in @("scripts\Run-V1_6_7_1-iOS-CoreSpotlight-AndZip.ps1", "scripts\Run-V1_6_7_1-iOS-Production-AndZip.ps1", "scripts\Launch-V1_6_7_1-GUI.ps1")) {
  $txt = ReadText $script
  if ($txt -match $staleRegex) { throw "$script contains stale previous-version reference" }
}
$prod = ReadText "scripts\Run-V1_6_7_1-iOS-Production-AndZip.ps1"
foreach ($needle in @('RunMode = "run"', 'ExportProfile = "investigator"', 'ForceContainerHash = $true', 'FullNativeValues = $true', 'ios_production_readiness_summary.csv')) {
  if ($prod -notmatch [regex]::Escape($needle)) { throw "Production iOS wrapper missing: $needle" }
}
$runner = ReadText "tools\Run-IosCoreSpotlightFocusedZip.ps1"
foreach ($needle in @('ForceContainerHash', '--force-container-hash', 'FullNativeValues', '--experimental-full-native-values', 'MaterializeIosSupportDb', '--materialize-ios-support-db', 'RunMode', 'PRODUCTION_PERFORMANCE_SUMMARY.md')) {
  if ($runner -notmatch [regex]::Escape($needle)) { throw "iOS focused runner missing production switch support: $needle" }
}

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
  if ($caseDb -notmatch [regex]::Escape($needle)) { throw "case_db.cpp missing V1.6.7.1 required view/guardrail: $needle" }
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
Test-NoGenericKnowledgeCInPromotionalSql -Text $guiText -Label "win32_gui.cpp"
foreach ($needle in @('vw_ios_spotlight_comms_missing_from_ffs', 'vw_ios_production_readiness_summary', 'PRODUCTION_READY_HASH_RECORDED', 'COMMUNICATION_PRESENT_IN_SPOTLIGHT_NOT_MATCHED_TO_NATIVE_APP_DB', 'deletion, app removal, encryption/inaccessibility, parser coverage limits', 'iOS - Production Readiness Summary')) {
  if ($guiText -notmatch [regex]::Escape($needle)) { throw "win32_gui.cpp missing iOS production/native-DB view coverage: $needle" }
}
$registry = ReadText "src\gui\view_registry.cpp"
foreach ($needle in @('vw_ios_production_readiness_summary', 'iOS - Production Readiness Summary', 'source_field_name', 'native_probe_context_count', 'native_probe_context_sample')) {
  if ($registry -notmatch [regex]::Escape($needle)) { throw "view_registry.cpp missing production/native probe column: $needle" }
}
$tests = ReadText "tests\main.cpp"
foreach ($needle in @('runKnowledgeCIdentitySuppressionSmokeTest', 'runIosCoreProbeTextContextSmokeTest', 'runIosProductionReadinessSmokeTest', 'vw_ios_production_readiness_summary')) {
  if ($tests -notmatch [regex]::Escape($needle)) { throw "Self-test missing V1.6.7.1 coverage: $needle" }
}
$exporter = ReadText "src\export_sql\sqlite_exporter.cpp"
foreach ($needle in @('ios_production_readiness_summary.csv', 'FORCE_CONTAINER_HASH_REQUESTED', 'THIN_PROFILE_DIRECT_IDENTITY_PIVOT', 'THIN_PROFILE_DIRECT_CANDIDATE_SAMPLE', 'THIN_PROFILE_NOT_EVALUATED')) {
  if ($exporter -notmatch [regex]::Escape($needle)) { throw "sqlite_exporter.cpp missing V1.6.7.1 production/thin guard: $needle" }
}
$raw = & powershell -ExecutionPolicy Bypass -File (Join-Path $SourceRoot "tools\Verify-MsvcStringLiteralRisk.ps1") -SourceRoot $SourceRoot
Write-Host $raw
$forensic = Join-Path $SourceRoot "tools\Verify-V1_6_7_1-ForensicDirectives.ps1"
powershell -ExecutionPolicy Bypass -File $forensic -SourceRoot $SourceRoot
Write-Host "Release readiness check passed for $expected"
