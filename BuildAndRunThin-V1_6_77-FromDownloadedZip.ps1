param(
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V1_6_77.zip",
  [string]$DestinationRoot = "T:\",
  [string]$BuildLog = "D:\Downloads\V1_6_77_build.log",
  [string]$InputZipOrFolder = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$ThinCaseRoot = "Q:\SpotlightCase\TestIOS_CoreSpotlight_V1_6_77",
  [string]$ThinZipPath = "D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_77.zip",
  [string]$ChatUploadBundlePath = "D:\Downloads\Upload_To_Chat_V1_6_77_Results.zip",
  [string]$ChatUploadBundleWorkRoot = "D:\Downloads\Upload_To_Chat_V1_6_77_Results_Work",
  [int]$MinimumCaseRootFreeGB = 25,
  [int]$MinimumBundleRootFreeGB = 5,
  [switch]$SkipFreeSpacePreflight,
  [switch]$CleanOut,
  [switch]$NoClipboardOrExplorer,
  [switch]$FullDiagnostics,
  [switch]$NoCsvExports,
  [switch]$UseFastLocalCaseRoot,
  [string]$FastLocalRoot = "D:\Downloads\SpotlightCase",
  [string]$ExternalSourceSha256 = "",
  [string]$ExternalSourceHashNote = "",
  [string]$ReuseIosCache = "",
  [switch]$AutoReusePriorIosCache,
  [switch]$DisableAutoReusePriorIosCache
)

$ErrorActionPreference = "Stop"

function Get-FreeSpaceInfoForPath {
  param([string]$Path)
  $root = [System.IO.Path]::GetPathRoot($Path)
  if ([string]::IsNullOrWhiteSpace($root)) { throw "Cannot determine drive root for path: $Path" }
  $drive = New-Object System.IO.DriveInfo($root)
  if (!$drive.IsReady) { throw "Drive is not ready for path: $Path root=$root" }
  [pscustomobject]@{
    Root = $root
    AvailableBytes = [int64]$drive.AvailableFreeSpace
    TotalBytes = [int64]$drive.TotalSize
    AvailableGB = [math]::Round($drive.AvailableFreeSpace / 1GB, 2)
    TotalGB = [math]::Round($drive.TotalSize / 1GB, 2)
  }
}

function Assert-FreeSpaceForPath {
  param(
    [string]$Path,
    [int]$MinimumGB,
    [string]$Purpose
  )
  $info = Get-FreeSpaceInfoForPath -Path $Path
  Write-Host ("Free-space preflight for {0}: path={1} root={2} available={3} GB total={4} GB required={5} GB" -f $Purpose, $Path, $info.Root, $info.AvailableGB, $info.TotalGB, $MinimumGB)
  if ($info.AvailableBytes -lt ([int64]$MinimumGB * 1GB)) {
    throw ("Free-space preflight failed for {0}: path={1} root={2} available={3} GB required={4} GB. Free old case folders or rerun with -UseFastLocalCaseRoot -FastLocalRoot <fast volume path>. Use -SkipFreeSpacePreflight only if you intentionally want to bypass this check." -f $Purpose, $Path, $info.Root, $info.AvailableGB, $MinimumGB)
  }
}

$script:IosReuseCacheValidationReason = ""
$script:IosReuseCacheWrapperLog = ""
$script:IosReuseCacheWrapperLogEntries = New-Object System.Collections.Generic.List[string]

function Write-IosReuseCacheWrapperLog {
  param([string]$Message)
  try {
    $line = ("{0}`t{1}" -f (Get-Date -Format o), $Message)
    [void]$script:IosReuseCacheWrapperLogEntries.Add($line)
    if ([string]::IsNullOrWhiteSpace($script:IosReuseCacheWrapperLog)) { return }
    $dir = Split-Path -Parent $script:IosReuseCacheWrapperLog
    if (!(Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $line | Add-Content -LiteralPath $script:IosReuseCacheWrapperLog -Encoding UTF8
  } catch { }
}

function Flush-IosReuseCacheWrapperLog {
  try {
    if ([string]::IsNullOrWhiteSpace($script:IosReuseCacheWrapperLog)) { return }
    $dir = Split-Path -Parent $script:IosReuseCacheWrapperLog
    if (!(Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $script:IosReuseCacheWrapperLogEntries | Set-Content -LiteralPath $script:IosReuseCacheWrapperLog -Encoding UTF8
  } catch { }
}

function Normalize-IosReuseSourcePath {
  param([string]$Path)
  if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
  $s = ([string]$Path).Trim().Replace('/','\')
  try { return ([System.IO.Path]::GetFullPath($s).TrimEnd('\')).ToLowerInvariant() }
  catch { return $s.TrimEnd('\').ToLowerInvariant() }
}

function Test-IosReuseCacheSourceMatch {
  param(
    [string]$CacheRoot,
    [string]$SourcePath
  )
  $script:IosReuseCacheValidationReason = ""
  if ([string]::IsNullOrWhiteSpace($CacheRoot)) {
    $script:IosReuseCacheValidationReason = "cache path is blank"
    return $false
  }
  if (!(Test-Path -LiteralPath $CacheRoot -PathType Container)) {
    $script:IosReuseCacheValidationReason = "cache folder not found: $CacheRoot"
    return $false
  }
  $required = @(
    "source_cache_manifest.json",
    "ios_input_store_entry_inventory.csv",
    "ios_ffs_file_inventory.csv",
    "ios_app_database_inventory.csv"
  )
  foreach ($name in $required) {
    $r = Join-Path $CacheRoot $name
    if (!(Test-Path -LiteralPath $r)) {
      $script:IosReuseCacheValidationReason = "missing required cache file: $r"
      return $false
    }
  }
  $stageRoot = Join-Path (Join-Path (Join-Path $CacheRoot "EvidenceStaging") "zip_source") "extracted"
  if (!(Test-Path -LiteralPath $stageRoot -PathType Container)) {
    $script:IosReuseCacheValidationReason = "missing required cache folder: $stageRoot"
    return $false
  }
  try {
    $manifest = Get-Content -LiteralPath (Join-Path $CacheRoot "source_cache_manifest.json") -Raw | ConvertFrom-Json
    $sourceItem = Get-Item -LiteralPath $SourcePath -ErrorAction Stop
    $manifestPathRaw = [string]$manifest.source_path
    $manifestPath = Normalize-IosReuseSourcePath -Path $manifestPathRaw
    $currentPath = Normalize-IosReuseSourcePath -Path $SourcePath
    $manifestSize = [int64]$manifest.source_size_bytes
    $currentSize = [int64]$sourceItem.Length
    if ($manifestPath -ne $currentPath) {
      $script:IosReuseCacheValidationReason = "source path mismatch after normalization: manifest='$manifestPathRaw' normalized_manifest='$manifestPath' current='$SourcePath' normalized_current='$currentPath'"
      return $false
    }
    if ($manifestSize -ne $currentSize) {
      $script:IosReuseCacheValidationReason = "source size mismatch: manifest=$manifestSize current=$currentSize"
      return $false
    }
    $script:IosReuseCacheValidationReason = "source path and size matched; cache is usable"
    return $true
  } catch {
    $script:IosReuseCacheValidationReason = "exception while validating cache manifest: $_"
    return $false
  }
}

function Resolve-IosReuseCache {
  param(
    [string]$ExplicitCache,
    [string]$CaseRoot,
    [string]$SourcePath,
    [switch]$AutoPrior
  )
  if (![string]::IsNullOrWhiteSpace($ExplicitCache)) {
    Write-IosReuseCacheWrapperLog "explicit cache supplied: $ExplicitCache"
    if (!(Test-Path -LiteralPath $ExplicitCache -PathType Container)) { throw "Explicit ReuseIosCache path was not found: $ExplicitCache" }
    if (!(Test-IosReuseCacheSourceMatch -CacheRoot $ExplicitCache -SourcePath $SourcePath)) {
      Write-Warning "Explicit ReuseIosCache did not pass source manifest/path/size validation. Continuing because the cache was explicitly supplied: $ExplicitCache. Reason: $script:IosReuseCacheValidationReason"
      Write-IosReuseCacheWrapperLog "explicit cache validation warning: $script:IosReuseCacheValidationReason"
    } else {
      Write-IosReuseCacheWrapperLog "explicit cache validation passed: $script:IosReuseCacheValidationReason"
    }
    return $ExplicitCache
  }
  if (!$AutoPrior) {
    $script:IosReuseCacheValidationReason = "automatic prior-cache selection disabled"
    Write-IosReuseCacheWrapperLog $script:IosReuseCacheValidationReason
    return ""
  }
  $parent = Split-Path -Parent $CaseRoot
  $candidate = Join-Path $parent "TestIOS_CoreSpotlight_V1_6_61"
  Write-IosReuseCacheWrapperLog "auto candidate: $candidate"
  if (Test-IosReuseCacheSourceMatch -CacheRoot $candidate -SourcePath $SourcePath) {
    Write-Host "Auto-selected prior iOS source cache: $candidate"
    Write-IosReuseCacheWrapperLog "auto cache selected: $candidate; $script:IosReuseCacheValidationReason"
    return $candidate
  }
  Write-Warning "AutoReusePriorIosCache was requested, but the expected prior cache was not usable or did not match source path/size: $candidate. Reason: $script:IosReuseCacheValidationReason"
  Write-IosReuseCacheWrapperLog "auto cache rejected: $candidate; $script:IosReuseCacheValidationReason"
  return ""
}

if (!(Test-Path -LiteralPath $ZipPath)) {
  throw "Downloaded source ZIP not found: $ZipPath"
}

$SourceRoot = Join-Path $DestinationRoot "VestigantSpotlightInv_V1_6_77"
if ($UseFastLocalCaseRoot) {
  $ThinCaseRoot = Join-Path $FastLocalRoot "TestIOS_CoreSpotlight_V1_6_77"
  Write-Host "Using fast local thin case root: $ThinCaseRoot"
}
$script:IosReuseCacheWrapperLog = Join-Path (Join-Path $ThinCaseRoot "logs") "ios_reuse_cache_wrapper.log"
Write-IosReuseCacheWrapperLog "wrapper reuse-cache validation log initialized"
$AutoReusePriorIosCacheEffective = $AutoReusePriorIosCache.IsPresent -or !$DisableAutoReusePriorIosCache.IsPresent
if ($AutoReusePriorIosCacheEffective) { Write-Host "Automatic prior iOS reuse-cache selection is enabled." }
else { Write-Host "Automatic prior iOS reuse-cache selection is disabled." }
$ResolvedReuseIosCache = Resolve-IosReuseCache -ExplicitCache $ReuseIosCache -CaseRoot $ThinCaseRoot -SourcePath $InputZipOrFolder -AutoPrior:$AutoReusePriorIosCacheEffective
if (![string]::IsNullOrWhiteSpace($ResolvedReuseIosCache)) { Write-Host "Resolved iOS reuse cache: $ResolvedReuseIosCache" }
Write-Host "Source ZIP: $ZipPath"
Write-Host "Destination source root: $SourceRoot"
Write-Host "Build log: $BuildLog"
Write-Host "Thin upload ZIP: $ThinZipPath"
if (!$SkipFreeSpacePreflight) {
  Assert-FreeSpaceForPath -Path $ThinCaseRoot -MinimumGB $MinimumCaseRootFreeGB -Purpose "thin case root"
  Assert-FreeSpaceForPath -Path $ChatUploadBundlePath -MinimumGB $MinimumBundleRootFreeGB -Purpose "chat upload bundle destination"
} else {
  Write-Warning "Skipping free-space preflight by request. SQLite may fail later with database/disk-full errors if the case-root volume is low."
}
Write-Host "SHA256 of downloaded ZIP:"
Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256 | Format-List

if (Test-Path -LiteralPath $SourceRoot) {
  Write-Host "Removing prior extracted source folder: $SourceRoot"
  Remove-Item -LiteralPath $SourceRoot -Recurse -Force
}

Write-Host "Expanding downloaded source ZIP to $DestinationRoot"
Expand-Archive -LiteralPath $ZipPath -DestinationPath $DestinationRoot -Force

$BuildScript = Join-Path $SourceRoot "scripts\Build-V1_6_77.ps1"
if (!(Test-Path -LiteralPath $BuildScript)) {
  throw "Build script was not found after extraction: $BuildScript"
}

Set-Location -LiteralPath $SourceRoot
Write-Host "Running build script from extracted source folder: $SourceRoot"
powershell -ExecutionPolicy Bypass -File $BuildScript -ZipPath $ZipPath -SourceRoot $SourceRoot -BuildLog $BuildLog
if ($LASTEXITCODE -ne 0) {
  throw "Extracted-source build script failed with exit code $LASTEXITCODE. Log: $BuildLog"
}

$ThinScript = Join-Path $SourceRoot "scripts\Run-V1_6_77-iOS-CoreSpotlight-AndZip.ps1"
if (!(Test-Path -LiteralPath $ThinScript)) {
  throw "Thin run script was not found after extraction: $ThinScript"
}

$thinArgList = @(
  "-ExecutionPolicy", "Bypass",
  "-File", $ThinScript,
  "-SourceRoot", $SourceRoot,
  "-InputZipOrFolder", $InputZipOrFolder,
  "-CaseRoot", $ThinCaseRoot,
  "-ZipPath", $ThinZipPath
)
if ($CleanOut) { $thinArgList += "-CleanOut" }
if ($NoClipboardOrExplorer) { $thinArgList += "-NoClipboardOrExplorer" }
if ($FullDiagnostics) { $thinArgList += "-FullDiagnostics" }
if ($NoCsvExports) { $thinArgList += "-NoCsvExports" }
if (![string]::IsNullOrWhiteSpace($ExternalSourceSha256)) { $thinArgList += @("-ExternalSourceSha256", $ExternalSourceSha256) }
if (![string]::IsNullOrWhiteSpace($ExternalSourceHashNote)) { $thinArgList += @("-ExternalSourceHashNote", $ExternalSourceHashNote) }
if (![string]::IsNullOrWhiteSpace($ResolvedReuseIosCache)) { $thinArgList += @("-ReuseIosCache", $ResolvedReuseIosCache) }

Write-Host "Running iOS CoreSpotlight thin wrapper from extracted source folder: $SourceRoot"
powershell @thinArgList
if ($LASTEXITCODE -ne 0) {
  if (Test-Path -LiteralPath $ThinZipPath) {
    Write-Warning "Thin run returned exit code $LASTEXITCODE but produced an upload ZIP. Upload it for diagnostic review: $ThinZipPath"
    $global:LASTEXITCODE = 0
  } else {
    throw "Thin wrapper failed with exit code $LASTEXITCODE and did not create upload ZIP: $ThinZipPath"
  }
}

function Copy-IfExistsToBundle {
  param(
    [string]$Path,
    [string]$Name
  )
  if (![string]::IsNullOrWhiteSpace($Path) -and (Test-Path -LiteralPath $Path)) {
    $dest = Join-Path $ChatUploadBundleWorkRoot $Name
    $destParent = Split-Path -Parent $dest
    if (!(Test-Path -LiteralPath $destParent)) { New-Item -ItemType Directory -Path $destParent -Force | Out-Null }
    Copy-Item -LiteralPath $Path -Destination $dest -Force
  }
}

function Write-TextLogSummaryToBundle {
  param(
    [string]$Path,
    [string]$Name,
    [int]$HeadLines = 80,
    [int]$TailLines = 80
  )
  if (![string]::IsNullOrWhiteSpace($Path) -and (Test-Path -LiteralPath $Path)) {
    $dest = Join-Path $ChatUploadBundleWorkRoot $Name
    $destParent = Split-Path -Parent $dest
    if (!(Test-Path -LiteralPath $destParent)) { New-Item -ItemType Directory -Path $destParent -Force | Out-Null }
    $item = Get-Item -LiteralPath $Path
    @(
      "Summary for large log file intentionally not copied in full."
      "Source: $Path"
      "LengthBytes: $($item.Length)"
      "LastWriteTimeUtc: $($item.LastWriteTimeUtc.ToString('o'))"
      ""
      "--- FIRST $HeadLines LINES ---"
    ) | Set-Content -LiteralPath $dest -Encoding UTF8
    try { Get-Content -LiteralPath $Path -TotalCount $HeadLines -ErrorAction Stop | Add-Content -LiteralPath $dest -Encoding UTF8 } catch { "Unable to read head: $_" | Add-Content -LiteralPath $dest -Encoding UTF8 }
    "" | Add-Content -LiteralPath $dest -Encoding UTF8
    "--- LAST $TailLines LINES ---" | Add-Content -LiteralPath $dest -Encoding UTF8
    try { Get-Content -LiteralPath $Path -Tail $TailLines -ErrorAction Stop | Add-Content -LiteralPath $dest -Encoding UTF8 } catch { "Unable to read tail: $_" | Add-Content -LiteralPath $dest -Encoding UTF8 }
  }
}

Write-IosReuseCacheWrapperLog "finalizing wrapper reuse-cache log after thin run; preserves entries if -CleanOut removed the initial case log"
Flush-IosReuseCacheWrapperLog

Write-Host "Creating single chat upload bundle: $ChatUploadBundlePath"
if (Test-Path -LiteralPath $ChatUploadBundleWorkRoot) { Remove-Item -LiteralPath $ChatUploadBundleWorkRoot -Recurse -Force }
New-Item -ItemType Directory -Path $ChatUploadBundleWorkRoot -Force | Out-Null

Copy-IfExistsToBundle -Path $BuildLog -Name "V1_6_77_build.log"
Copy-IfExistsToBundle -Path $ThinZipPath -Name "Upload_Thin_iOS_CoreSpotlight_V1_6_77.zip"

$caseFiles = @(
  "case_info.json",
  "case_summary.json",
  "source_cache_manifest.json",
  "run_progress.tsv",
  "run_status.txt",
  "last_stage.txt",
  "last_progress.tsv",
  "THIN_PERFORMANCE_SUMMARY.md",
  "thin_performance_summary.csv",
  "wrapper_heartbeat.log",
  "VestigantSpotlight_tail250.log",
  "EXPORT_INDEX.csv"
)
foreach ($name in $caseFiles) {
  Copy-IfExistsToBundle -Path (Join-Path $ThinCaseRoot $name) -Name (Join-Path "case_root" $name)
}

$logFiles = @(
  "VestigantSpotlight.log",
  "ios_focused_zip_extract.log",
  "ios_focused_zip_extract_7z.log",
  "ios_zip_stage_heartbeat.log",
  "ios_app_database_extract_7z.log",
  "ios_reuse_cache.log",
  "ios_reuse_cache_wrapper.log"
)
foreach ($name in $logFiles) {
  Copy-IfExistsToBundle -Path (Join-Path (Join-Path $ThinCaseRoot "logs") $name) -Name (Join-Path "case_logs" $name)
}
Write-TextLogSummaryToBundle -Path (Join-Path (Join-Path $ThinCaseRoot "logs") "ios_ffs_7z_inventory_raw_slt.txt") -Name (Join-Path "case_logs" "ios_ffs_7z_inventory_raw_slt_summary.txt")

@"
Vestigant Spotlight V1.6.77 wrapper run summary
Created: $(Get-Date -Format o)
ZipPath: $ZipPath
SourceRoot: $SourceRoot
InputZipOrFolder: $InputZipOrFolder
ThinCaseRoot: $ThinCaseRoot
ThinZipPath: $ThinZipPath
ChatUploadBundlePath: $ChatUploadBundlePath
UseFastLocalCaseRoot: $($UseFastLocalCaseRoot.IsPresent)
FastLocalRoot: $FastLocalRoot
MinimumCaseRootFreeGB: $MinimumCaseRootFreeGB
MinimumBundleRootFreeGB: $MinimumBundleRootFreeGB
SkipFreeSpacePreflight: $($SkipFreeSpacePreflight.IsPresent)
AutoReusePriorIosCacheParameter: $($AutoReusePriorIosCache.IsPresent)
DisableAutoReusePriorIosCache: $($DisableAutoReusePriorIosCache.IsPresent)
AutoReusePriorIosCacheEffective: $AutoReusePriorIosCacheEffective
ExplicitReuseIosCache: $ReuseIosCache
ResolvedReuseIosCache: $ResolvedReuseIosCache
ReuseCacheValidationReason: $script:IosReuseCacheValidationReason
ReuseCacheWrapperLog: $script:IosReuseCacheWrapperLog
"@ | Set-Content -LiteralPath (Join-Path $ChatUploadBundleWorkRoot "WRAPPER_RUN_SUMMARY.txt") -Encoding UTF8

@"
Vestigant Spotlight V1.6.77 chat upload bundle
Created: $(Get-Date -Format o)
Build log: $BuildLog
Thin ZIP: $ThinZipPath
Case root: $ThinCaseRoot

Upload this ZIP to ChatGPT when the run completes:
$ChatUploadBundlePath
"@ | Set-Content -LiteralPath (Join-Path $ChatUploadBundleWorkRoot "README_UPLOAD_THIS_BUNDLE.txt") -Encoding UTF8

if (Test-Path -LiteralPath $ChatUploadBundlePath) { Remove-Item -LiteralPath $ChatUploadBundlePath -Force }
Compress-Archive -Path (Join-Path $ChatUploadBundleWorkRoot "*") -DestinationPath $ChatUploadBundlePath -Force

if (!$NoClipboardOrExplorer) {
  try { Set-Clipboard -Value $ChatUploadBundlePath } catch { Write-Warning "Unable to copy chat upload bundle path to clipboard: $_" }
  try { explorer.exe /select,"$ChatUploadBundlePath" | Out-Null } catch { Write-Warning "Unable to open Explorer to chat upload bundle: $_" }
}

Write-Host "Build completed. Build log: $BuildLog"
Write-Host "Thin run completed. Thin ZIP: $ThinZipPath"
Write-Host "Single chat upload bundle: $ChatUploadBundlePath"
