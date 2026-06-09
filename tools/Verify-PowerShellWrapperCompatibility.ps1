param(
    [string]$SourceRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = "Stop"
$bad = @()
$pattern = '\.ArgumentList' + '\.Add\('
foreach ($rootName in @("tools", "scripts")) {
    $root = Join-Path $SourceRoot $rootName
    if (!(Test-Path -LiteralPath $root)) { continue }
    Get-ChildItem -LiteralPath $root -Filter "*.ps1" -File -Recurse | ForEach-Object {
        if ($_.Name -eq "Verify-PowerShellWrapperCompatibility.ps1") { return }
        $text = Get-Content -LiteralPath $_.FullName -Raw
        if ($text -match $pattern) { $bad += $_.FullName }
    }
}
if ($bad.Count -gt 0) {
    throw "PowerShell wrapper compatibility failure: ProcessStartInfo ArgumentList Add pattern is not safe on Windows PowerShell 5.1: $($bad -join '; ')"
}
Write-Host "PowerShell wrapper compatibility check passed."
