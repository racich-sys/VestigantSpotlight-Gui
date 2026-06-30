param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_115",
  [string]$ReaderToolsRoot = "",
  [switch]$RequireAff4ReaderTools,
  [switch]$NoPathSearch
)

$ErrorActionPreference = "Stop"

function Add-UniquePath {
  param([System.Collections.Generic.List[string]]$List, [string]$PathValue)
  if ([string]::IsNullOrWhiteSpace($PathValue)) { return }
  try { $full = [System.IO.Path]::GetFullPath($PathValue) } catch { $full = $PathValue }
  if ((Test-Path -LiteralPath $full -PathType Container) -and -not $List.Contains($full)) { [void]$List.Add($full) }
}

function Find-FirstFileInRoots {
  param([string[]]$Patterns, [System.Collections.Generic.List[string]]$Roots)
  foreach ($root in $Roots) {
    foreach ($pattern in $Patterns) {
      $direct = Join-Path $root $pattern
      if (Test-Path -LiteralPath $direct -PathType Leaf) { return (Get-Item -LiteralPath $direct) }
      $found = Get-ChildItem -LiteralPath $root -Recurse -File -Filter $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
      if ($found) { return $found }
    }
  }
  return $null
}

function Copy-FoundFile {
  param(
    [string]$LogicalName,
    [string[]]$Patterns,
    [bool]$Required,
    [string]$DestinationRoot,
    [System.Collections.Generic.List[string]]$Roots,
    [System.Collections.Generic.List[object]]$Rows
  )
  $found = Find-FirstFileInRoots -Patterns $Patterns -Roots $Roots
  if ($found) {
    $dest = Join-Path $DestinationRoot $found.Name
    Copy-Item -LiteralPath $found.FullName -Destination $dest -Force
    [void]$Rows.Add([pscustomobject]@{
      logical_name = $LogicalName
      file_name = $found.Name
      source_path = $found.FullName
      copied_to = $dest
      required = [string]$Required
      status = "FOUND"
    })
    return $true
  }
  [void]$Rows.Add([pscustomobject]@{
    logical_name = $LogicalName
    file_name = ($Patterns -join '|')
    source_path = ""
    copied_to = ""
    required = [string]$Required
    status = if ($Required) { "MISSING_REQUIRED" } else { "MISSING_OPTIONAL" }
  })
  return $false
}

if (!(Test-Path -LiteralPath $SourceRoot -PathType Container)) { throw "SourceRoot not found: $SourceRoot" }
$ReleaseRoot = Join-Path $SourceRoot "build-msvc\Release"
if (!(Test-Path -LiteralPath $ReleaseRoot -PathType Container)) { throw "Release folder not found. Build first: $ReleaseRoot" }

$ResourcesRoot = Join-Path $ReleaseRoot "resources"
$ReaderOut = Join-Path $ResourcesRoot "reader_tools"
$ApfsOut = Join-Path $ResourcesRoot "apfs_tools"
$DocsOut = Join-Path $ResourcesRoot "docs"
New-Item -ItemType Directory -Force -Path $ReaderOut, $ApfsOut, $DocsOut | Out-Null

# Copy non-binary application resources needed by the GUI, including the logo.
$SourceResources = Join-Path $SourceRoot "resources"
if (Test-Path -LiteralPath $SourceResources -PathType Container) {
  Get-ChildItem -LiteralPath $SourceResources -File -ErrorAction SilentlyContinue |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $ResourcesRoot $_.Name) -Force }
}

$roots = New-Object 'System.Collections.Generic.List[string]'
Add-UniquePath -List $roots -PathValue $ReaderToolsRoot
Add-UniquePath -List $roots -PathValue (Join-Path $SourceRoot "resources\reader_tools")

Add-UniquePath -List $roots -PathValue (Join-Path $SourceRoot "resources\reader_tools\include")
Add-UniquePath -List $roots -PathValue (Join-Path $SourceRoot "resources\reader_tools\x64\Release")
Add-UniquePath -List $roots -PathValue (Join-Path $SourceRoot "resources\aff4_cpp_lite")
Add-UniquePath -List $roots -PathValue (Join-Path $SourceRoot "reader_tools")
Add-UniquePath -List $roots -PathValue $env:VESTIGANT_READER_TOOLS
Add-UniquePath -List $roots -PathValue $env:VESTIGANT_AFF4_CPP_LITE_ROOT
Add-UniquePath -List $roots -PathValue "T:\VestigantReaderTools\aff4-cpp-lite"
Add-UniquePath -List $roots -PathValue "T:\VestigantReaderTools\aff4-cpp-lite\x64\Release"
Add-UniquePath -List $roots -PathValue "C:\VestigantReaderTools\aff4-cpp-lite"
Add-UniquePath -List $roots -PathValue "C:\Tools\aff4-cpp-lite"

if (!$NoPathSearch) {
  foreach ($entry in (($env:PATH -split ';') | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) {
    Add-UniquePath -List $roots -PathValue $entry
  }
}

$rows = New-Object 'System.Collections.Generic.List[object]'
$requiredOk = $true
$requiredOk = (Copy-FoundFile -LogicalName "AFF4_CPP_LITE_LIBRARY_DLL" -Patterns @("libaff4.dll") -Required $true -DestinationRoot $ReaderOut -Roots $roots -Rows $rows) -and $requiredOk
$requiredOk = (Copy-FoundFile -LogicalName "AFF4_CPP_LITE_DEPENDENCY_ZLIB_DLL" -Patterns @("zlib1.dll") -Required $true -DestinationRoot $ReaderOut -Roots $roots -Rows $rows) -and $requiredOk
$requiredOk = (Copy-FoundFile -LogicalName "AFF4_CPP_LITE_DEPENDENCY_SNAPPY_DLL" -Patterns @("snappy.dll") -Required $true -DestinationRoot $ReaderOut -Roots $roots -Rows $rows) -and $requiredOk
$requiredOk = (Copy-FoundFile -LogicalName "AFF4_CPP_LITE_DEPENDENCY_RAPTOR2_DLL" -Patterns @("raptor2.dll") -Required $true -DestinationRoot $ReaderOut -Roots $roots -Rows $rows) -and $requiredOk

# Accept either legacy or modern OpenSSL names. These remain marked optional in the manifest because some static/custom libaff4 builds do not need both DLLs.
[void](Copy-FoundFile -LogicalName "AFF4_CPP_LITE_DEPENDENCY_OPENSSL_CRYPTO_DLL" -Patterns @("libcrypto*.dll", "libeay32.dll") -Required $false -DestinationRoot $ReaderOut -Roots $roots -Rows $rows)
[void](Copy-FoundFile -LogicalName "AFF4_CPP_LITE_DEPENDENCY_OPENSSL_SSL_DLL" -Patterns @("libssl*.dll", "ssleay32.dll") -Required $false -DestinationRoot $ReaderOut -Roots $roots -Rows $rows)
[void](Copy-FoundFile -LogicalName "AFF4_CPP_LITE_DEPENDENCY_ZLIB_COMPAT_DLL" -Patterns @("zlib.dll") -Required $false -DestinationRoot $ReaderOut -Roots $roots -Rows $rows)
$requiredOk = (Copy-FoundFile -LogicalName "MSVC_RUNTIME_MSVCP140_DLL" -Patterns @("MSVCP140.dll") -Required $true -DestinationRoot $ReaderOut -Roots $roots -Rows $rows) -and $requiredOk
$requiredOk = (Copy-FoundFile -LogicalName "MSVC_RUNTIME_VCRUNTIME140_DLL" -Patterns @("VCRUNTIME140.dll") -Required $true -DestinationRoot $ReaderOut -Roots $roots -Rows $rows) -and $requiredOk
$requiredOk = (Copy-FoundFile -LogicalName "MSVC_RUNTIME_VCRUNTIME140_1_DLL" -Patterns @("VCRUNTIME140_1.dll") -Required $true -DestinationRoot $ReaderOut -Roots $roots -Rows $rows) -and $requiredOk
[void](Copy-FoundFile -LogicalName "AFF4_CPP_LITE_LIBRARY_IMPORT_LIB" -Patterns @("libaff4.lib") -Required $false -DestinationRoot $ReaderOut -Roots $roots -Rows $rows)
[void](Copy-FoundFile -LogicalName "AFF4_CPP_LITE_C_HEADER" -Patterns @("aff4-c.h") -Required $false -DestinationRoot $ReaderOut -Roots $roots -Rows $rows)
[void](Copy-FoundFile -LogicalName "AFF4_CONTAINER_READER_FALLBACK" -Patterns @("aff4-info.exe", "aff4-info") -Required $false -DestinationRoot $ReaderOut -Roots $roots -Rows $rows)
[void](Copy-FoundFile -LogicalName "AFF4_CONTAINER_EXTRACT_FALLBACK" -Patterns @("aff4-extract.exe", "aff4-extract") -Required $false -DestinationRoot $ReaderOut -Roots $roots -Rows $rows)
[void](Copy-FoundFile -LogicalName "AFF4_IMAGER_TOOL" -Patterns @("aff4imager.exe") -Required $false -DestinationRoot $ReaderOut -Roots $roots -Rows $rows)
[void](Copy-FoundFile -LogicalName "APFS_INFO_READER" -Patterns @("fsapfsinfo.exe", "fsapfsinfo") -Required $false -DestinationRoot $ApfsOut -Roots $roots -Rows $rows)
[void](Copy-FoundFile -LogicalName "APFS_MOUNT_OR_EXPORT_READER" -Patterns @("fsapfsmount.exe", "fsapfsmount") -Required $false -DestinationRoot $ApfsOut -Roots $roots -Rows $rows)

$ManifestPath = Join-Path $ReaderOut "reader_tools_manifest.csv"
$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -Path $ManifestPath
$BundledManifest = Join-Path $SourceRoot "resources\reader_tools\bundled_reader_tools_manifest.csv"
if (Test-Path -LiteralPath $BundledManifest -PathType Leaf) { Copy-Item -LiteralPath $BundledManifest -Destination (Join-Path $ReaderOut "bundled_reader_tools_manifest.csv") -Force }
$BundledOriginalManifest = Join-Path $SourceRoot "resources\reader_tools\reader_tools_manifest_original.csv"
if (Test-Path -LiteralPath $BundledOriginalManifest -PathType Leaf) { Copy-Item -LiteralPath $BundledOriginalManifest -Destination (Join-Path $ReaderOut "reader_tools_manifest_original.csv") -Force }
$BundledNote = Join-Path $SourceRoot "resources\reader_tools\READER_TOOLS_BUNDLE_NOTE.txt"
if (Test-Path -LiteralPath $BundledNote -PathType Leaf) { Copy-Item -LiteralPath $BundledNote -Destination (Join-Path $ReaderOut "READER_TOOLS_BUNDLE_NOTE.txt") -Force }

$runtimeStatus = if ($requiredOk) { "PORTABLE_RUNTIME_READY" } else { "PORTABLE_RUNTIME_INCOMPLETE" }
$statusPath = Join-Path $ResourcesRoot "portable_runtime_status.txt"
$statusLines = New-Object 'System.Collections.Generic.List[string]'
$statusLines.Add("Status: $runtimeStatus") | Out-Null
$statusLines.Add("Created: $((Get-Date).ToUniversalTime().ToString('o'))") | Out-Null
$statusLines.Add("ReleaseRoot: $ReleaseRoot") | Out-Null
$statusLines.Add("ReaderToolsRootParameter: $ReaderToolsRoot") | Out-Null
$statusLines.Add("SearchRoots:") | Out-Null
$roots | ForEach-Object { $statusLines.Add("  $_") | Out-Null }
$statusLines.Add("") | Out-Null
$statusLines.Add("RequiredRuntimeFiles:") | Out-Null
foreach ($rel in @(
  "resources\reader_tools\libaff4.dll",
  "resources\reader_tools\zlib1.dll",
  "resources\reader_tools\snappy.dll",
  "resources\reader_tools\raptor2.dll",
  "resources\reader_tools\MSVCP140.dll",
  "resources\reader_tools\VCRUNTIME140.dll",
  "resources\reader_tools\VCRUNTIME140_1.dll"
)) {
  $fp = Join-Path $ReleaseRoot $rel
  if (Test-Path -LiteralPath $fp -PathType Leaf) {
    $item = Get-Item -LiteralPath $fp
    $hash = ""
    try { $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $fp).Hash } catch { $hash = "HASH_UNAVAILABLE" }
    $statusLines.Add(("  FOUND {0} bytes={1} sha256={2}" -f $rel,$item.Length,$hash)) | Out-Null
  } else {
    $statusLines.Add("  MISSING $rel") | Out-Null
  }
}
$statusLines | Set-Content -LiteralPath $statusPath -Encoding UTF8

$checkScript = @'
$ReleaseRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Required = @(
  "VestigantSpotlight.exe",
  "VestigantSpotlightCli.exe",
  "resources\reader_tools\libaff4.dll",
  "resources\reader_tools\zlib1.dll",
  "resources\reader_tools\snappy.dll",
  "resources\reader_tools\raptor2.dll",
  "resources\reader_tools\MSVCP140.dll",
  "resources\reader_tools\VCRUNTIME140.dll",
  "resources\reader_tools\VCRUNTIME140_1.dll"
)
$Advisory = @()
$ok = $true
foreach ($rel in $Required) {
  $p = Join-Path $ReleaseRoot $rel
  if (Test-Path -LiteralPath $p -PathType Leaf) { Write-Host "[OK] $rel" }
  else { Write-Host "[MISSING] $rel" -ForegroundColor Red; $ok = $false }
}
$status = Join-Path $ReleaseRoot "resources\portable_runtime_status.txt"
if (Test-Path -LiteralPath $status) { Get-Content -LiteralPath $status | ForEach-Object { Write-Host $_ } }
foreach ($rel in $Advisory) {
  $p = Join-Path $ReleaseRoot $rel
  if (Test-Path -LiteralPath $p -PathType Leaf) { Write-Host "[OK] $rel" }
  else { Write-Host "[ADVISORY MISSING] $rel - target machine must have the Microsoft VC++ runtime installed or these DLLs copied beside the app." -ForegroundColor Yellow }
}
if (!$ok) { throw "Portable runtime is incomplete. Provide reader tools or rerun Stage-PortableRelease.ps1 with -ReaderToolsRoot." }
Write-Host "Portable runtime check passed."
'@
Set-Content -LiteralPath (Join-Path $ReleaseRoot "Check-PortableRuntime.ps1") -Encoding UTF8 -Value $checkScript

$launchScript = @'
$ReleaseRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Join-Path $ReleaseRoot "VestigantSpotlight.exe"
if (!(Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "VestigantSpotlight.exe not found: $Exe" }
$ReaderTools = Join-Path $ReleaseRoot "resources\reader_tools"
if (Test-Path -LiteralPath $ReaderTools -PathType Container) {
  $env:VESTIGANT_READER_TOOLS = $ReaderTools
}
Start-Process -FilePath $Exe -WorkingDirectory $ReleaseRoot
'@
Set-Content -LiteralPath (Join-Path $ReleaseRoot "Launch-VestigantSpotlight-GUI.ps1") -Encoding UTF8 -Value $launchScript

$runScript = @'
param(
  [Parameter(Mandatory=$true)][string]$Aff4Path,
  [Parameter(Mandatory=$true)][string]$CaseRoot,
  [string]$ZipPath = "D:\Downloads\Upload_Thin_MacOS_AFF4_Portable.zip",
  [switch]$FullNoGuardrails,
  [switch]$PressureTestMode
)
$ReleaseRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Cli = Join-Path $ReleaseRoot "VestigantSpotlightCli.exe"
if (!(Test-Path -LiteralPath $Cli -PathType Leaf)) { throw "CLI not found: $Cli" }
$ReaderTools = Join-Path $ReleaseRoot "resources\reader_tools"
$args = @("--mode", "source-probe", "--profile", "macos", "--input", $Aff4Path, "--out", $CaseRoot, "--strict-single-aff4", "--skip-container-hash")
if (Test-Path -LiteralPath $ReaderTools -PathType Container) { $args += @("--reader-tools", $ReaderTools); $env:VESTIGANT_READER_TOOLS = $ReaderTools }
if ($FullNoGuardrails) { $args += @("--full-scan", "--enable-aff4-dynamic-probe", "--aff4-apfs-diagnostic-outputs") }
if ($PressureTestMode -or $FullNoGuardrails) { $args += "--pressure-test" }
$args += @("--experimental-full-native-values", "--max-native-records", "0")
Write-Host "Executing: $Cli $($args -join ' ')"
& $Cli @args
$exit = $LASTEXITCODE
if ($exit -eq 0 -and ![string]::IsNullOrWhiteSpace($ZipPath)) {
  $zipParent = Split-Path -Parent $ZipPath
  if (![string]::IsNullOrWhiteSpace($zipParent)) { New-Item -ItemType Directory -Force -Path $zipParent | Out-Null }
  if (Test-Path -LiteralPath $ZipPath -PathType Leaf) { Remove-Item -LiteralPath $ZipPath -Force }
  $thin = Join-Path $CaseRoot "Upload_Thin"
  if (Test-Path -LiteralPath $thin -PathType Container) {
    Compress-Archive -LiteralPath (Join-Path $thin "*") -DestinationPath $ZipPath -Force
    Write-Host "Portable AFF4 thin ZIP created from Upload_Thin: $ZipPath"
  } else {
    $stage = Join-Path $CaseRoot "PortableUploadStage"
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue }
    New-Item -ItemType Directory -Force -Path $stage | Out-Null
    Get-ChildItem -LiteralPath $CaseRoot -File -Include *.csv,*.txt,*.md,*.json,*.tsv -ErrorAction SilentlyContinue | Copy-Item -Destination $stage -Force
    Compress-Archive -LiteralPath (Join-Path $stage "*") -DestinationPath $ZipPath -Force
    Write-Host "Portable AFF4 diagnostic ZIP created from case-root text artifacts: $ZipPath"
  }
}
exit $exit
'@
Set-Content -LiteralPath (Join-Path $ReleaseRoot "Run-AFF4Probe-Portable.ps1") -Encoding UTF8 -Value $runScript

$doc = @"
# Portable Runtime

This Release folder is intended to be copyable to another Windows machine.

Run `Check-PortableRuntime.ps1` after copying. AFF4/APFS extraction needs the files under `resources\reader_tools`.

Status at staging time: $runtimeStatus
"@
Set-Content -LiteralPath (Join-Path $DocsOut "RUNTIME_DEPENDENCY_CHECKLIST.md") -Encoding UTF8 -Value $doc

Write-Host "Portable release staging status: $runtimeStatus"
Write-Host "ReleaseRoot: $ReleaseRoot"
Write-Host "Manifest: $ManifestPath"
Write-Host "Status: $statusPath"
if (!$requiredOk) {
  $msg = "Portable runtime is incomplete; AFF4/APFS on a clean machine will require reader tools. Review $ManifestPath."
  if ($RequireAff4ReaderTools) { throw $msg }
  Write-Warning $msg
}
