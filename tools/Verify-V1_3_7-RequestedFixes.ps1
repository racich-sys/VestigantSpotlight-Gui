$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$gui = Join-Path $root 'src\gui\win32_gui.cpp'
$aff4 = Join-Path $root 'src\parsers\aff4_probe_worker.cpp'
$ios = Join-Path $root 'src\parsers\ios_app_db_parser.cpp'

function Require-Match($Path, $Pattern, $Name) {
  if (!(Select-String -Path $Path -Pattern $Pattern -Quiet)) { throw "Missing required implementation: $Name" }
  Write-Host "PASS: $Name"
}
function Require-NoMatch($Path, $Pattern, $Name) {
  if (Select-String -Path $Path -Pattern $Pattern -Quiet) { throw "Forbidden stale implementation still present: $Name" }
  Write-Host "PASS: $Name"
}

Require-NoMatch $gui 'unique_lock<std::mutex>\s+lock_' 'ReadOnlyDb does not hold mutex for instance lifetime'
Require-Match $gui 'class ReadOnlyDb' 'ReadOnlyDb present'
Require-Match $gui 'closePoolNoThrow' 'ReadOnlyDb pool close hook present'
Require-Match $aff4 'visitedGuidedNodes' 'APFS guided visited-node cycle guard present'
Require-Match $aff4 'GUIDED_INODE_LOOKUP_CYCLE_DETECTED' 'APFS guided inode cycle status present'
Require-Match $aff4 'GUIDED_FILE_EXTENT_LOOKUP_CYCLE_DETECTED' 'APFS guided file extent cycle status present'
Require-Match $ios 'ripBplistStrings' 'iOS bplist string ripper present'
Require-Match $ios 'NOTES_RECORDS' 'iOS Notes routing present'
Require-Match $ios 'LOCATION_RECORDS' 'iOS Location routing present'
Require-Match $ios 'zcontent' 'iOS zcontent text column catcher present'
Require-Match $ios 'payload' 'iOS payload text column catcher present'
Require-Match $ios 'zlocalpath' 'iOS zlocalpath path column catcher present'
Write-Host 'All V1.3.7 requested-fix checks passed.'
