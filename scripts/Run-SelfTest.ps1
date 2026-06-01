$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'build-msvc\Release\VestigantSpotlightCli.exe'
if (!(Test-Path $exe)) { throw "Build first: build_windows_msvc.bat" }
$out = Join-Path $root 'selftest_out_win'
& $exe --mode self-test --out $out --verbose
Write-Host "Self-test output: $out"
