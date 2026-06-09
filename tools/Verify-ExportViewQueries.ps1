param([string]$SourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')))
$ErrorActionPreference = 'Stop'
$caseDb = Get-Content -LiteralPath (Join-Path $SourceRoot 'src\db\case_db.cpp') -Raw
$exporter = Get-Content -LiteralPath (Join-Path $SourceRoot 'src\export_sql\sqlite_exporter.cpp') -Raw
if ($caseDb -notmatch 'AS identity_kind' -or $caseDb -notmatch 'AS activity_thread_or_record_id') {
  throw 'Identity detail sample view does not expose identity_kind/activity_thread_or_record_id.'
}
if ($exporter -match 'vw_ios_identity_thread_activity_matrix' -and $caseDb -notmatch 'CREATE VIEW vw_ios_identity_thread_activity_matrix') {
  throw 'Exporter references identity thread activity matrix but case_db.cpp does not define it.'
}
Write-Host 'Export/view consistency static checks passed.'
