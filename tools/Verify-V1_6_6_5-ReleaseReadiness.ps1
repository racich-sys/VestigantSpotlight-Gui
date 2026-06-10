param([string]$SourceRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = "Stop"
$expected = "1.6.6.5"
$appInfo = Get-Content -LiteralPath (Join-Path $SourceRoot "src\core\app_info.cpp") -Raw
if ($appInfo -notmatch [regex]::Escape($expected)) { throw "app_info.cpp does not contain expected version $expected" }
$build = Get-Content -LiteralPath (Join-Path $SourceRoot "scripts\Build-V1_6_6_5.ps1") -Raw
if ($build -notmatch "1\\.6\\.6\\.5") { throw "Build wrapper does not reference 1.6.6.5" }
if ($build -notmatch '\$version -notmatch "1\\.6\\.6\\.5"') { throw "Build wrapper CLI version validation is not pinned to 1.6.6.5" }
$ios = Join-Path $SourceRoot "scripts\Run-V1_6_6_5-iOS-CoreSpotlight-AndZip.ps1"
if (!(Test-Path -LiteralPath $ios)) { throw "Missing iOS thin wrapper: $ios" }
$iosText = Get-Content -LiteralPath $ios -Raw
if ($iosText -notmatch "NoCsvExports") { throw "iOS thin wrapper does not expose -NoCsvExports" }

$guiExportWorker = Get-Content -LiteralPath (Join-Path $SourceRoot "src\gui\gui_export_worker.cpp") -Raw
foreach ($needle in @("std::wstring joinPath", "std::wstring sanitizeExportName", "void ensureFolderExistsNoThrow")) {
  if ($guiExportWorker -notmatch [regex]::Escape($needle)) { throw "GUI export worker is not self-contained for helper: $needle" }
}
$iosParser = Get-Content -LiteralPath (Join-Path $SourceRoot "src\parsers\ios_app_db_parser.cpp") -Raw
foreach ($needle in @("looksLikeAppleAbsoluteTimeNumeric", "IDENTITY_PROMOTION_SUPPRESSED=True", "KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY", "KNOWLEDGEC_COMMUNICATION_INTENT")) {
  if ($iosParser -notmatch [regex]::Escape($needle)) { throw "iOS parser missing V1.6.6.5 identity/KnowledgeC guardrail: $needle" }
}


$caseDb = Get-Content -LiteralPath (Join-Path $SourceRoot "src\db\case_db.cpp") -Raw
foreach ($needle in @("KNOWLEDGEC_COMMUNICATION_INTENT", "KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY", "IDENTITY_PROMOTION_SUPPRESSED=True")) {
  if ($caseDb -notmatch [regex]::Escape($needle)) { throw "case_db.cpp missing V1.6.6.5 KnowledgeC classification guardrail: $needle" }
}
if ($caseDb -match "KNOWLEDGEC_EVENTS'\) THEN 'parsed_communication_category") { throw "Generic KNOWLEDGEC_EVENTS still auto-promotes as parsed communication category" }

function Test-NoGenericKnowledgeCInPromotionalSql {
  param(
    [Parameter(Mandatory=$true)][string]$Text,
    [Parameter(Mandatory=$true)][string]$Label
  )
  $promotionalViews = @(
    "vw_ios_communication_frequency",
    "vw_ios_communication_existence_evidence",
    "vw_ios_communication_identity_frequency",
    "vw_ios_communication_temporal_frequency",
    "vw_ios_communication_source_coverage",
    "vw_ios_identity_activity_linkage",
    "vw_ios_identity_activity_detail_sample",
    "vw_ios_identity_entity_rollup"
  )
  foreach ($view in $promotionalViews) {
    $escapedView = [regex]::Escape($view)
    $m = [regex]::Match($Text, "CREATE VIEW(?: IF NOT EXISTS)?\s+$escapedView\s+AS(?<body>.*?);", [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if (!$m.Success) { continue }
    $body = $m.Groups["body"].Value
    $withoutSuppressionGuards = [regex]::Replace($body, "AND\s+NOT\s*\(\s*record_category\s+IN\s*\([^)]*KNOWLEDGEC_EVENTS[^)]*\).*?\)", "", [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if ($withoutSuppressionGuards -match "record_category\s+IN\s*\([^)]*KNOWLEDGEC_EVENTS[^)]*\)") {
      throw "$Label has generic KNOWLEDGEC_EVENTS in promotional predicate for $view; use KNOWLEDGEC_COMMUNICATION_INTENT plus provenance markers instead"
    }
  }
}
Test-NoGenericKnowledgeCInPromotionalSql -Text $caseDb -Label "case_db.cpp"
$guiText = Get-Content -LiteralPath (Join-Path $SourceRoot "src\gui\win32_gui.cpp") -Raw
Test-NoGenericKnowledgeCInPromotionalSql -Text $guiText -Label "win32_gui.cpp"

foreach ($needle in @("__native_core_probe_string_%", "native_probe_context_sample", "SPOTLIGHT_MESSAGE_OR_ATTACHMENT_TEXT_PROBE", "SPOTLIGHT_MAIL_OR_ACCOUNT_TEXT_PROBE", "10_string_probes")) {
  if ($caseDb -notmatch [regex]::Escape($needle)) { throw "case_db.cpp missing V1.6.6.5 native CoreSpotlight probe review coverage: $needle" }
}
$nativeParser = Get-Content -LiteralPath (Join-Path $SourceRoot "src\parsers\native_storedb_parser.cpp") -Raw
foreach ($needle in @("coreProbeField", "looksLikeForensicReferenceValue", 'fieldLabel = "__native_core_probe_string"')) {
  if ($nativeParser -notmatch [regex]::Escape($needle)) { throw "native_storedb_parser.cpp missing V1.6.6.5 core probe text-context guard: $needle" }
}
$registry = Get-Content -LiteralPath (Join-Path $SourceRoot "src\gui\view_registry.cpp") -Raw
foreach ($needle in @("source_field_name", "native_probe_context_count", "native_probe_context_sample")) {
  if ($registry -notmatch [regex]::Escape($needle)) { throw "view_registry.cpp missing V1.6.6.5 native probe review column: $needle" }
}

$tests = Get-Content -LiteralPath (Join-Path $SourceRoot "tests\main.cpp") -Raw
foreach ($needle in @("runKnowledgeCIdentitySuppressionSmokeTest", "runIosCoreProbeTextContextSmokeTest", "SPOTLIGHT_MESSAGE_OR_ATTACHMENT_TEXT_PROBE", "KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY", "KNOWLEDGEC_COMMUNICATION_INTENT")) {
  if ($tests -notmatch [regex]::Escape($needle)) { throw "Self-test missing V1.6.6.5 KnowledgeC identity suppression coverage: $needle" }
}


$exporter = Get-Content -LiteralPath (Join-Path $SourceRoot "src\export_sql\sqlite_exporter.cpp") -Raw
foreach ($needle in @("THIN_PROFILE_DIRECT_IDENTITY_PIVOT", "THIN_PROFILE_DIRECT_CANDIDATE_SAMPLE", "THIN_PROFILE_NOT_EVALUATED")) {
  if ($exporter -notmatch [regex]::Escape($needle)) { throw "sqlite_exporter.cpp missing V1.6.6.5 thin export timeout guard: $needle" }
}

$raw = & powershell -ExecutionPolicy Bypass -File (Join-Path $SourceRoot "tools\Verify-MsvcStringLiteralRisk.ps1") -SourceRoot $SourceRoot
Write-Host $raw

$forensic = Join-Path $SourceRoot "tools\Verify-V1_6_6_5-ForensicDirectives.ps1"
if (Test-Path -LiteralPath $forensic) { powershell -ExecutionPolicy Bypass -File $forensic -SourceRoot $SourceRoot }

Write-Host "Release readiness check passed for $expected"
