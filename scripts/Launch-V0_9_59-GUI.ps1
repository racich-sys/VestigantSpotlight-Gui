param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V0_9_59"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $SourceRoot "build-msvc\Release\VestigantSpotlight.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "GUI executable not found. Build first: $exe" }
Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent)
Write-Host "Started: $exe"
Write-Host "Open an existing completed case database from the Case Information tab, then test the bottom Selected Row Metadata pane."
