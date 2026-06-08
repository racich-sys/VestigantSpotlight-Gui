param(
  [Parameter(Mandatory=$true)][string]$SourceRoot,
  [Parameter(Mandatory=$true)][string]$Version
)
$ErrorActionPreference = "Stop"
$escaped = [regex]::Escape($Version)
$appInfo = Join-Path $SourceRoot "src\core\app_info.cpp"
if (!(Test-Path $appInfo)) { throw "Missing app_info.cpp: $appInfo" }
$appText = Get-Content -LiteralPath $appInfo -Raw
if ($appText -notmatch [regex]::Escape('return "' + $Version + '"')) {
  throw "app_info.cpp does not report version $Version"
}
$scriptName = "Build-V$($Version.Replace('.', '_')).ps1"
$scriptPath = Join-Path $SourceRoot ("scripts\" + $scriptName)
if (!(Test-Path $scriptPath)) { throw "Missing expected build script: $scriptPath" }
$scriptText = Get-Content -LiteralPath $scriptPath -Raw
$regexText = $Version.Replace('.', '\.')
if ($scriptText -notmatch [regex]::Escape($regexText)) {
  throw "$scriptName does not check expected version regex $regexText"
}
$badPriorChecks = Select-String -Path $scriptPath -Pattern '1\\\.4\\\.1|1\\\.4\\\.0|1\\\.3\\\.' -SimpleMatch -ErrorAction SilentlyContinue
if ($badPriorChecks) { throw "$scriptName appears to contain stale prior-version check text" }
Write-Host "PASS: release version consistency for $Version"
