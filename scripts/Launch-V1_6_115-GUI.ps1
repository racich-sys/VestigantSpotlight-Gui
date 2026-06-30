param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_115"
)

$ErrorActionPreference = "Stop"
$ReleaseRoot = Join-Path $SourceRoot "build-msvc\Release"
$exe = Join-Path $ReleaseRoot "VestigantSpotlight.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "GUI executable not found. Build first: $exe" }
$ReaderTools = Join-Path $ReleaseRoot "resources\reader_tools"
if (Test-Path -LiteralPath $ReaderTools -PathType Container) {
  $env:VESTIGANT_READER_TOOLS = $ReaderTools
  Write-Host "Portable reader tools exported for this GUI launch: $ReaderTools"
} else {
  Write-Warning "Portable reader tools folder was not found: $ReaderTools. AFF4/APFS may require manual reader-tool selection."
}
Start-Process -FilePath $exe -WorkingDirectory $ReleaseRoot
Write-Host "Started: $exe"
Write-Host "Copyable portable folder: $ReleaseRoot"
Write-Host "Suggested check: powershell -ExecutionPolicy Bypass -File `"$ReleaseRoot\Check-PortableRuntime.ps1`""
