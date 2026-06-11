param([string]$SourceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")))
$ErrorActionPreference = 'Stop'
function RequireText($Path, $Pattern, $Description) {
  if (!(Test-Path -LiteralPath $Path)) { throw "Missing file for check: $Path" }
  $text = Get-Content -LiteralPath $Path -Raw
  if ($text -notmatch $Pattern) { throw "Missing required forensic directive implementation: $Description ($Pattern) in $Path" }
  Write-Host "Verified: $Description"
}
$aff4 = Join-Path $SourceRoot 'src\parsers\aff4_probe_worker.cpp'
$ios = Join-Path $SourceRoot 'src\parsers\ios_app_db_parser.cpp'
$caseDb = Join-Path $SourceRoot 'src\db\case_db.cpp'
$views = Join-Path $SourceRoot 'src\gui\view_registry.cpp'
$win32 = Join-Path $SourceRoot 'src\gui\win32_gui.cpp'
RequireText $aff4 'visitedGuidedNodes' 'APFS guided traversal visited-node set'
RequireText $aff4 'GUIDED_INODE_LOOKUP_CYCLE_DETECTED' 'APFS guided inode cycle marker'
RequireText $aff4 'GUIDED_FILE_EXTENT_LOOKUP_CYCLE_DETECTED' 'APFS guided file extent cycle marker'
RequireText $ios 'ripBplistStrings' 'iOS embedded bplist string ripping'
RequireText $ios 'resolveUid' 'iOS bounded NSKeyedArchiver UID reconstruction helper'
RequireText $ios 'tel:' 'iOS tel: identity recovery'
RequireText $ios 'mailto:' 'iOS mailto: identity recovery'
RequireText $ios 'NOTES_RECORDS' 'iOS Notes routing'
RequireText $ios 'LOCATION_RECORDS' 'iOS Location routing'
RequireText $ios 'zcontent' 'iOS Notes/content column catcher'
RequireText $ios 'payload' 'iOS payload column catcher'
RequireText $caseDb 'vw_ios_spotlight_comms_missing_from_ffs' 'CoreSpotlight communication not matched to native DB case schema view'
RequireText $win32 'vw_ios_spotlight_comms_missing_from_ffs' 'CoreSpotlight communication not matched to native DB GUI bootstrap view'
RequireText $views 'vw_ios_spotlight_comms_missing_from_ffs' 'CoreSpotlight communication missing/native DB GUI registration'
Write-Host 'Forensic directive verification passed.'
