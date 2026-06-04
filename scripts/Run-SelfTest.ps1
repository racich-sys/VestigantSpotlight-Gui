param(
  [string]$SourceRoot = (Split-Path -Parent $PSScriptRoot),
  [string]$Out = "Q:\SpotlightCase\SelfTest_V0_9_60"
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $SourceRoot 'build-msvc\Release\VestigantSpotlightTests.exe'
if (!(Test-Path -LiteralPath $exe)) { throw "VestigantSpotlightTests.exe not found. Build first: $exe" }
& $exe $Out
if ($LASTEXITCODE -ne 0) { throw "VestigantSpotlightTests.exe failed with exit code $LASTEXITCODE" }
