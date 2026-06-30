param([string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_115")
$ErrorActionPreference = "Stop"
$ReleaseRoot = Join-Path $SourceRoot "build-msvc\Release"
$Required = @(
  "VestigantSpotlight.exe",
  "VestigantSpotlightCli.exe",
  "VestigantSpotlightTests.exe",
  "resources\vestigant_logo.bmp",
  "resources\reader_tools\reader_tools_manifest.csv",
  "resources\reader_tools\libaff4.dll",
  "resources\reader_tools\zlib1.dll",
  "resources\reader_tools\snappy.dll",
  "resources\reader_tools\raptor2.dll",
  "resources\reader_tools\MSVCP140.dll",
  "resources\reader_tools\VCRUNTIME140.dll",
  "resources\reader_tools\VCRUNTIME140_1.dll",
  "Check-PortableRuntime.ps1",
  "Launch-VestigantSpotlight-GUI.ps1",
  "Run-AFF4Probe-Portable.ps1"
)
$missing = @()
foreach ($rel in $Required) {
  $p = Join-Path $ReleaseRoot $rel
  if (!(Test-Path -LiteralPath $p -PathType Leaf)) { $missing += $rel }
}
if ($missing.Count -gt 0) {
  Write-Warning "Portable release layout is incomplete. Missing: $($missing -join ', ')"
  exit 2
}
Write-Host "Portable release layout check passed: $ReleaseRoot"
