param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_119",
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightPortable_V1_6_119.zip",
  [switch]$RequireReady
)

$ErrorActionPreference = "Stop"
$ReleaseRoot = Join-Path $SourceRoot "build-msvc\Release"
if (!(Test-Path -LiteralPath $ReleaseRoot -PathType Container)) { throw "Release folder not found: $ReleaseRoot" }
$Check = Join-Path $ReleaseRoot "Check-PortableRuntime.ps1"
if (!(Test-Path -LiteralPath $Check -PathType Leaf)) { throw "Portable runtime check script not found: $Check" }

& powershell -ExecutionPolicy Bypass -File $Check
$checkExit = $LASTEXITCODE
if ($checkExit -ne 0 -and $RequireReady) { throw "Portable runtime check failed; refusing to export portable ZIP because -RequireReady was specified." }
if ($checkExit -ne 0) { Write-Warning "Portable runtime check failed; exporting diagnostic ZIP anyway." }

$parent = Split-Path -Parent $ZipPath
if (![string]::IsNullOrWhiteSpace($parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
if (Test-Path -LiteralPath $ZipPath -PathType Leaf) { Remove-Item -LiteralPath $ZipPath -Force }
if (Test-Path -LiteralPath ($ZipPath + ".sha256.txt") -PathType Leaf) { Remove-Item -LiteralPath ($ZipPath + ".sha256.txt") -Force }

# Use .NET ZipFile instead of Compress-Archive. Some Windows PowerShell builds emit a
# NativeCommandError/Resolve-Path failure when Compress-Archive validates a destination
# ZIP that does not exist yet. CreateFromDirectory only requires the source directory to
# exist, so this is safer for one-click build pipelines.
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($ReleaseRoot, $ZipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)

if (!(Test-Path -LiteralPath $ZipPath -PathType Leaf)) { throw "Portable Release ZIP was not created: $ZipPath" }
$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $ZipPath
$hash | Format-List | Out-String | Set-Content -LiteralPath ($ZipPath + ".sha256.txt") -Encoding UTF8
Write-Host "Portable Release ZIP: $ZipPath"
Write-Host "Portable Release ZIP SHA256: $($hash.Hash)"
