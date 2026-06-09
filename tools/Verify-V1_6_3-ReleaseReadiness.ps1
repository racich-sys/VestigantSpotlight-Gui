param([string]$SourceRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = "Stop"
$expected = "1.6.3"
$appInfo = Get-Content -LiteralPath (Join-Path $SourceRoot "src\core\app_info.cpp") -Raw
if ($appInfo -notmatch [regex]::Escape($expected)) { throw "app_info.cpp does not contain expected version $expected" }
$build = Get-Content -LiteralPath (Join-Path $SourceRoot "scripts\Build-V1_6_3.ps1") -Raw
if ($build -notmatch "1\\.6\\.2\\.1") { throw "Build wrapper does not check 1.6.3" }
$ios = Join-Path $SourceRoot "scripts\Run-V1_6_3-iOS-CoreSpotlight-AndZip.ps1"
if (!(Test-Path -LiteralPath $ios)) { throw "Missing iOS thin wrapper: $ios" }
$raw = & powershell -ExecutionPolicy Bypass -File (Join-Path $SourceRoot "tools\Verify-MsvcStringLiteralRisk.ps1") -SourceRoot $SourceRoot
Write-Host $raw
Write-Host "Release readiness check passed for $expected"
