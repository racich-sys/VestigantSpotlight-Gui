param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_38"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $SourceRoot "build-msvc\Release\VestigantSpotlight.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "GUI executable not found. Build first: $exe" }
Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent)
Write-Host "Started: $exe"
Write-Host "Suggested smoke test: iOS tab shows Production Readiness Summary first; iOS native-DB mismatch view is present; macOS tab does not show iOS views."
