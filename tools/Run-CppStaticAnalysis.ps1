[CmdletBinding()]
param(
    [string]$SourceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [switch]$Strict
)
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$clangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue
if (-not $clangTidy) {
    Write-Host "clang-tidy not found; static analysis skipped. Install clang-tidy and re-run with -Strict to gate warnings."
    if ($Strict) { throw "clang-tidy not found while -Strict was requested." }
    exit 0
}

$files = @(
    Join-Path $SourceRoot 'src\parsers\aff4_probe_worker.cpp'
    Join-Path $SourceRoot 'src\parsers\native_storedb_parser.cpp'
    Join-Path $SourceRoot 'src\enrich_sql\sqlite_enrichment.cpp'
    Join-Path $SourceRoot 'src\app\app_runner.cpp'
) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }

if ($files.Count -eq 0) { throw "No target C++ files found under $SourceRoot" }

$failed = $false
foreach ($file in $files) {
    Write-Host "clang-tidy: $file"
    & $clangTidy.Path $file -- -I (Join-Path $SourceRoot 'src')
    if ($LASTEXITCODE -ne 0) { $failed = $true }
}

if ($failed -and $Strict) { throw "clang-tidy reported issues under -Strict." }
if ($failed) { Write-Warning "clang-tidy reported issues; rerun with -Strict to make this fatal." }
