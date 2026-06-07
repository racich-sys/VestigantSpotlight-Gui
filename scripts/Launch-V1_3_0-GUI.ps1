param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_3_0"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $SourceRoot "build-msvc\Release\VestigantSpotlight.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "GUI executable not found. Build first: $exe" }
Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent)
Write-Host "Started: $exe"
Write-Host "Suggested smoke test: source selector still shows Folder, ZIP, AFF4/APFS image (staged), and Raw IMG/DD image (staged); bottom Case Information pane remains readable; MacOS/iOS details panes stay in their investigation tabs."
