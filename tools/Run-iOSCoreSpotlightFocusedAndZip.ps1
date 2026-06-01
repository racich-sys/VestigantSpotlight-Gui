param(
    [string]$InputRoot = "D:\Downloads\iOS_CoreSpotlight_Focused_Extracts\EXTRACTION_FFS",
    [string]$Out = "Q:\SpotlightCase\V0_8_75_iOS_CoreSpotlight",
    [string]$ZipPath = "D:\Downloads\Upload_Thin_V0_8_75_iOS_CoreSpotlight.zip",
    [string]$UploadWorkRoot = "D:\Downloads\V0_8_75_iOS_UploadWork",
    [switch]$CleanOut,
    [switch]$FullScan,
    [switch]$NoClipboardOrExplorer
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Cli = Join-Path $RepoRoot "build-msvc\Release\VestigantSpotlightCli.exe"
$UploadTool = Join-Path $PSScriptRoot "Create-SourceProbeUploadZip.ps1"

if (!(Test-Path -LiteralPath $Cli)) { throw "CLI binary not found. Build this source tree first: $Cli" }
if (!(Test-Path -LiteralPath $InputRoot)) { throw "InputRoot not found: $InputRoot" }
if (!(Test-Path -LiteralPath $UploadTool)) { throw "Upload tool not found: $UploadTool" }

$version = (& $Cli --version 2>&1 | Out-String).Trim()
Write-Host "Verified built CLI: $version"

if (Test-Path -LiteralPath $Out) {
    $existing = @(Get-ChildItem -LiteralPath $Out -Force -ErrorAction SilentlyContinue | Select-Object -First 1)
    if ($existing.Count -gt 0) {
        if ($CleanOut) { Remove-Item -LiteralPath $Out -Recurse -Force }
        else { throw "Case output exists and is not empty: $Out. Use -CleanOut or choose a new -Out path." }
    }
}
New-Item -ItemType Directory -Force -Path $Out | Out-Null

$args = @(
    "--mode", "diagnostics",
    "--profile", "ios",
    "--input", $InputRoot,
    "--out", $Out,
    "--full-scan",
    "--export-profile", "diagnostics",
    "--verbose"
)

Write-Host "Running iOS CoreSpotlight diagnostics..."
& $Cli @args
if ($LASTEXITCODE -ne 0) { throw "VestigantSpotlightCli failed with exit code $LASTEXITCODE" }

& $UploadTool `
  -CaseRoot $Out `
  -ZipPath $ZipPath `
  -UploadWorkRoot $UploadWorkRoot `
  -IncludeLogsTailOnly

if (!$NoClipboardOrExplorer) {
    try { Set-Clipboard -Value $ZipPath } catch {}
    try { explorer.exe /select,$ZipPath | Out-Null } catch {}
}
