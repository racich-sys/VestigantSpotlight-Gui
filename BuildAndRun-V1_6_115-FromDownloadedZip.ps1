param(
  [ValidateSet("Auto", "IOSCoreSpotlightThin", "AFF4Probe", "BuildOnly")]
  [string]$Workflow = "Auto",
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V1_6_115.zip",
  [string]$DestinationRoot = "T:\",
  [string]$BuildLog = "D:\Downloads\V1_6_115_build.log",

  # iOS thin workflow defaults.
  [string]$InputZipOrFolder = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$ThinCaseRoot = "Q:\SpotlightCase\TestIOS_CoreSpotlight_V1_6_115",
  [string]$ThinZipPath = "D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_115.zip",
  [string]$ChatUploadBundlePath = "D:\Downloads\Upload_To_Chat_V1_6_115_Results.zip",
  [string]$ChatUploadBundleWorkRoot = "D:\Downloads\Upload_To_Chat_V1_6_115_Results_Work",
  [int]$MinimumCaseRootFreeGB = 25,
  [int]$MinimumBundleRootFreeGB = 5,
  [switch]$SkipFreeSpacePreflight,
  [switch]$UseFastLocalCaseRoot,
  [string]$FastLocalRoot = "D:\Downloads\SpotlightCase",
  [string]$ExternalSourceSha256 = "",
  [string]$ExternalSourceHashNote = "",
  [string]$ReuseIosCache = "",
  [switch]$AutoReusePriorIosCache,
  [switch]$DisableAutoReusePriorIosCache,
  [switch]$FullDiagnostics,
  [switch]$NoCsvExports,

  # AFF4 probe workflow defaults.
  [string]$Aff4Path = "",
  [string]$Aff4SearchRoot = "T:\",
  [string]$Aff4NameHint = "0202_0024-IT003",
  [string]$Aff4CaseRoot = "Q:\SpotlightCase\TestMacOS_AFF4_V1_6_115",
  [string]$ReaderToolsRoot = "",
  [string]$Aff4ZipPath = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_115.zip",
  [switch]$FullNoGuardrails,
  [switch]$PressureTestMode,
  [switch]$EnableStreamInventory,
  [switch]$FullNativeValues,
  [int]$MaxNativeRecords = 0,
  [int]$MaxNativeBlocks = 0,
  [int]$CliTimeoutMinutes = 180,

  # Shared controls.
  [switch]$CleanOut,
  [switch]$NoClipboardOrExplorer
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
  param([string]$Path, [int]$MinimumGB, [string]$Purpose)
  $info = Get-FreeSpaceInfoForPath -Path $Path
  Write-Host ("Free-space preflight for {0}: path={1} root={2} available={3} GB total={4} GB required={5} GB" -f $Purpose, $Path, $info.Root, $info.AvailableGB, $info.TotalGB, $MinimumGB)
  if ($info.AvailableBytes -lt ([int64]$MinimumGB * 1GB)) {
    throw ("Free-space preflight failed for {0}: path={1} root={2} available={3} GB required={4} GB. Free old case folders or rerun with -UseFastLocalCaseRoot -FastLocalRoot <fast volume path>. Use -SkipFreeSpacePreflight only if you intentionally want to bypass this check." -f $Purpose, $Path, $info.Root, $info.AvailableGB, $MinimumGB)
  }
}

function Assert-Leaf {
  param([Parameter(Mandatory=$true)][string]$PathValue, [Parameter(Mandatory=$true)][string]$Description)
  if (!(Test-Path -LiteralPath $PathValue -PathType Leaf)) { throw "$Description not found: $PathValue" }
}

function Resolve-Aff4PathFromSearch {
  param([string]$SearchRoot, [string]$NameHint)
  if ([string]::IsNullOrWhiteSpace($SearchRoot)) { return "" }
  if (!(Test-Path -LiteralPath $SearchRoot -PathType Container)) { return "" }
  Write-Host "Searching for AFF4 under $SearchRoot with hint '$NameHint'..."
  $matches = @(Get-ChildItem -LiteralPath $SearchRoot -Filter "*.aff4" -Recurse -File -ErrorAction SilentlyContinue | Where-Object { [string]::IsNullOrWhiteSpace($NameHint) -or $_.FullName -like ("*" + $NameHint + "*") } | Sort-Object FullName)
  if ($matches.Count -eq 0) { return "" }
  if ($matches.Count -gt 1) {
    Write-Warning "Multiple AFF4 files matched; using the first sorted path. Matching count: $($matches.Count)"
    $preview = ($matches | Select-Object -First 10 FullName, Length, LastWriteTime | Format-Table -AutoSize | Out-String)
    Write-Host $preview
  }
  return $matches[0].FullName
}

function Assert-DirectoryOrBlank {
  param([string]$PathValue, [string]$Description)
  if (![string]::IsNullOrWhiteSpace($PathValue) -and !(Test-Path -LiteralPath $PathValue -PathType Container)) { throw "$Description not found: $PathValue" }
}

function Copy-IfExistsToBundle {
  param([string]$Path, [string]$Name)
  if (![string]::IsNullOrWhiteSpace($Path) -and (Test-Path -LiteralPath $Path)) {
    $dest = Join-Path $ChatUploadBundleWorkRoot $Name
    $destParent = Split-Path -Parent $dest
    if (!(Test-Path -LiteralPath $destParent)) { New-Item -ItemType Directory -Path $destParent -Force | Out-Null }
    Copy-Item -LiteralPath $Path -Destination $dest -Force
  }
}

function Write-TextLogSummaryToBundle {
  param([string]$Path, [string]$Name, [int]$HeadLines = 80, [int]$TailLines = 80)
  if (![string]::IsNullOrWhiteSpace($Path) -and (Test-Path -LiteralPath $Path)) {
    $dest = Join-Path $ChatUploadBundleWorkRoot $Name
    $destParent = Split-Path -Parent $dest
    if (!(Test-Path -LiteralPath $destParent)) { New-Item -ItemType Directory -Path $destParent -Force | Out-Null }
    $item = Get-Item -LiteralPath $Path
    @(
      "Summary for large log file intentionally not copied in full.",
      "Source: $Path",
      "LengthBytes: $($item.Length)",
      "LastWriteTimeUtc: $($item.LastWriteTimeUtc.ToString('o'))",
      "",
      "--- FIRST $HeadLines LINES ---"
    ) | Set-Content -LiteralPath $dest -Encoding UTF8
    try { Get-Content -LiteralPath $Path -TotalCount $HeadLines -ErrorAction Stop | Add-Content -LiteralPath $dest -Encoding UTF8 } catch { "Unable to read head: $_" | Add-Content -LiteralPath $dest -Encoding UTF8 }
    "" | Add-Content -LiteralPath $dest -Encoding UTF8
    "--- LAST $TailLines LINES ---" | Add-Content -LiteralPath $dest -Encoding UTF8
    try { Get-Content -LiteralPath $Path -Tail $TailLines -ErrorAction Stop | Add-Content -LiteralPath $dest -Encoding UTF8 } catch { "Unable to read tail: $_" | Add-Content -LiteralPath $dest -Encoding UTF8 }
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
  param([string]$CacheRoot, [string]$SourcePath)
  $script:IosReuseCacheValidationReason = ""
  if ([string]::IsNullOrWhiteSpace($CacheRoot)) { $script:IosReuseCacheValidationReason = "cache path is blank"; return $false }
  if (!(Test-Path -LiteralPath $CacheRoot -PathType Container)) { $script:IosReuseCacheValidationReason = "cache folder not found: $CacheRoot"; return $false }
  foreach ($name in @("source_cache_manifest.json", "ios_input_store_entry_inventory.csv", "ios_ffs_file_inventory.csv", "ios_app_database_inventory.csv")) {
    $r = Join-Path $CacheRoot $name
    if (!(Test-Path -LiteralPath $r)) { $script:IosReuseCacheValidationReason = "missing required cache file: $r"; return $false }
  }
  $stageRoot = Join-Path (Join-Path (Join-Path $CacheRoot "EvidenceStaging") "zip_source") "extracted"
  if (!(Test-Path -LiteralPath $stageRoot -PathType Container)) { $script:IosReuseCacheValidationReason = "missing required cache folder: $stageRoot"; return $false }
  try {
    $manifest = Get-Content -LiteralPath (Join-Path $CacheRoot "source_cache_manifest.json") -Raw | ConvertFrom-Json
    $sourceItem = Get-Item -LiteralPath $SourcePath -ErrorAction Stop
    $manifestPathRaw = [string]$manifest.source_path
    $manifestPath = Normalize-IosReuseSourcePath -Path $manifestPathRaw
    $currentPath = Normalize-IosReuseSourcePath -Path $SourcePath
    $manifestSize = [int64]$manifest.source_size_bytes
    $currentSize = [int64]$sourceItem.Length
    if ($manifestPath -ne $currentPath) { $script:IosReuseCacheValidationReason = "source path mismatch after normalization: manifest='$manifestPathRaw' normalized_manifest='$manifestPath' current='$SourcePath' normalized_current='$currentPath'"; return $false }
    if ($manifestSize -ne $currentSize) { $script:IosReuseCacheValidationReason = "source size mismatch: manifest=$manifestSize current=$currentSize"; return $false }
    $script:IosReuseCacheValidationReason = "source path and size matched; cache is usable"
    return $true
  } catch {
    $script:IosReuseCacheValidationReason = "exception while validating cache manifest: $_"
    return $false
  }
}

function Resolve-IosReuseCache {
  param([string]$ExplicitCache, [string]$CaseRoot, [string]$SourcePath, [switch]$AutoPrior)
  if (![string]::IsNullOrWhiteSpace($ExplicitCache)) {
    Write-IosReuseCacheWrapperLog "explicit cache supplied: $ExplicitCache"
    if (!(Test-Path -LiteralPath $ExplicitCache -PathType Container)) { throw "Explicit ReuseIosCache path was not found: $ExplicitCache" }
    if (!(Test-IosReuseCacheSourceMatch -CacheRoot $ExplicitCache -SourcePath $SourcePath)) {
      Write-Warning "Explicit ReuseIosCache did not pass source manifest/path/size validation. Continuing because the cache was explicitly supplied: $ExplicitCache. Reason: $script:IosReuseCacheValidationReason"
      Write-IosReuseCacheWrapperLog "explicit cache validation warning: $script:IosReuseCacheValidationReason"
    } else { Write-IosReuseCacheWrapperLog "explicit cache validation passed: $script:IosReuseCacheValidationReason" }
    return $ExplicitCache
  }
  if (!$AutoPrior) { $script:IosReuseCacheValidationReason = "automatic prior-cache selection disabled"; Write-IosReuseCacheWrapperLog $script:IosReuseCacheValidationReason; return "" }
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

function Invoke-ProcessAndRequireSuccess {
  param([string[]]$ArgumentList, [string]$FailureMessage)
  powershell @ArgumentList
  if ($LASTEXITCODE -ne 0) { throw "$FailureMessage Exit code: $LASTEXITCODE" }
}

if (!(Test-Path -LiteralPath $ZipPath)) { throw "Downloaded source ZIP not found: $ZipPath" }

$SelectedWorkflow = $Workflow
if ($SelectedWorkflow -eq "Auto") {
  if (![string]::IsNullOrWhiteSpace($Aff4Path)) { $SelectedWorkflow = "AFF4Probe" }
  else { $SelectedWorkflow = "IOSCoreSpotlightThin" }
}

$SourceRoot = Join-Path $DestinationRoot "VestigantSpotlightInv_V1_6_115"
if ($UseFastLocalCaseRoot -and $SelectedWorkflow -eq "IOSCoreSpotlightThin") {
  $ThinCaseRoot = Join-Path $FastLocalRoot "TestIOS_CoreSpotlight_V1_6_115"
  Write-Host "Using fast local thin case root: $ThinCaseRoot"
}

Write-Host "Vestigant Spotlight V1.6.115 automated workflow"
Write-Host "Selected workflow: $SelectedWorkflow"
Write-Host "Source ZIP: $ZipPath"
Write-Host "Destination source root: $SourceRoot"
Write-Host "Build log: $BuildLog"
Write-Host "Thin/test mode: downloaded source ZIP hash display skipped. Use full validation hashing only when requested."

if ($SelectedWorkflow -eq "IOSCoreSpotlightThin" -and !$SkipFreeSpacePreflight) {
  Assert-FreeSpaceForPath -Path $ThinCaseRoot -MinimumGB $MinimumCaseRootFreeGB -Purpose "thin case root"
  Assert-FreeSpaceForPath -Path $ChatUploadBundlePath -MinimumGB $MinimumBundleRootFreeGB -Purpose "chat upload bundle destination"
} elseif ($SelectedWorkflow -eq "IOSCoreSpotlightThin") {
  Write-Warning "Skipping free-space preflight by request. SQLite may fail later with database/disk-full errors if the case-root volume is low."
}

# AFF4 path resolution is intentionally deferred until after build/self-test so the one-click workflow always builds first.

if (Test-Path -LiteralPath $SourceRoot) {
  Write-Host "Removing prior extracted source folder: $SourceRoot"
  Remove-Item -LiteralPath $SourceRoot -Recurse -Force
}
Write-Host "Expanding downloaded source ZIP to $DestinationRoot"
Expand-Archive -LiteralPath $ZipPath -DestinationPath $DestinationRoot -Force

$BuildScript = Join-Path $SourceRoot "scripts\Build-V1_6_115.ps1"
if (!(Test-Path -LiteralPath $BuildScript)) { throw "Build script was not found after extraction: $BuildScript" }
Set-Location -LiteralPath $SourceRoot
Write-Host "Running build and required self-test from extracted source folder: $SourceRoot"
$buildArgs = @("-ExecutionPolicy", "Bypass", "-File", $BuildScript, "-ZipPath", $ZipPath, "-SourceRoot", $SourceRoot, "-BuildLog", $BuildLog)
if (![string]::IsNullOrWhiteSpace($ReaderToolsRoot)) { $buildArgs += @("-ReaderToolsRoot", $ReaderToolsRoot) }
powershell @buildArgs
if ($LASTEXITCODE -ne 0) { throw "Build/self-test failed with exit code $LASTEXITCODE. Log: $BuildLog" }

if ($SelectedWorkflow -eq "BuildOnly") {
  Write-Host "BuildOnly workflow completed. Upload build log if review is needed: $BuildLog"
  exit 0
}

if ($SelectedWorkflow -eq "AFF4Probe") {
  if ([string]::IsNullOrWhiteSpace($Aff4Path)) {
    $Aff4Path = Resolve-Aff4PathFromSearch -SearchRoot $Aff4SearchRoot -NameHint $Aff4NameHint
    if (![string]::IsNullOrWhiteSpace($Aff4Path)) { Write-Host "Auto-resolved AFF4 input: $Aff4Path" }
  }
  if ([string]::IsNullOrWhiteSpace($Aff4Path)) { throw "AFF4Probe workflow requires -Aff4Path <path-to-aff4>, or a searchable -Aff4SearchRoot with a matching .aff4 file. SearchRoot=$Aff4SearchRoot NameHint=$Aff4NameHint" }
  if ((Normalize-IosReuseSourcePath -Path $Aff4Path) -eq (Normalize-IosReuseSourcePath -Path "R:\path\to\evidence.aff4")) {
    throw "AFF4Path is still the documentation placeholder. Replace it with the exact .aff4 evidence file path before running AFF4Probe."
  }
  Assert-Leaf -PathValue $Aff4Path -Description "AFF4 input"
  Assert-DirectoryOrBlank -PathValue $ReaderToolsRoot -Description "Reader tools folder"
  $Runner = Join-Path $SourceRoot "tools\Run-SingleAff4SourceProbeAndZip.ps1"
  Assert-Leaf -PathValue $Runner -Description "AFF4 source-probe runner"
  $runnerArgs = @(
    "-ExecutionPolicy", "Bypass", "-File", $Runner,
    "-Aff4Input", $Aff4Path,
    "-Out", $Aff4CaseRoot,
    "-ZipPath", $Aff4ZipPath,
    "-IncludeLogsTailOnly",
    "-CliTimeoutMinutes", $CliTimeoutMinutes,
    "-NoClipboardOrExplorer"
  )
  if ($CleanOut) { $runnerArgs += "-CleanOut" }
  if (![string]::IsNullOrWhiteSpace($ReaderToolsRoot)) { $runnerArgs += @("-ReaderToolsRoot", $ReaderToolsRoot) }
  if ($FullNoGuardrails) {
    $runnerArgs += "-FullScan"
    $runnerArgs += "-EnableAff4DynamicProbe"
    $runnerArgs += "-EnableAff4VirtualApfsProbe"
    $runnerArgs += "-DiagnosticOutputs"
  }
  # Thin/trial runs do not hash source containers; full validation must use a separate confirmed hash workflow.
  $runnerArgs += "-SkipContainerHash"
  if ($PressureTestMode -or $FullNoGuardrails) { $runnerArgs += "-PressureTestMode" }
  if ($FullNativeValues -or $PressureTestMode -or $FullNoGuardrails) { $runnerArgs += "-FullNativeValues" } else { $runnerArgs += "-DecodeCoreNativeValues" }
  if ($MaxNativeRecords -ge 0) { $runnerArgs += @("-MaxNativeRecords", ([string]$MaxNativeRecords)) }
  if ($MaxNativeBlocks -gt 0) { $runnerArgs += @("-MaxNativeBlocks", ([string]$MaxNativeBlocks)) }
  if ($EnableStreamInventory) { $runnerArgs += "-EnableAff4StreamInventory" }
  powershell @runnerArgs
  $runnerExit = $LASTEXITCODE
  $summaryPath = "D:\Downloads\V1_6_115_AFF4_WRAPPER_RUN_SUMMARY.txt"
  @(
    "Vestigant Spotlight V1.6.115 automated AFF4 wrapper run summary",
    "Created: $((Get-Date).ToString('o'))",
    "Workflow: $SelectedWorkflow",
    "ZipPath: $ZipPath",
    "SourceRoot: $SourceRoot",
    "Aff4Path: $Aff4Path",
    "Aff4SearchRoot: $Aff4SearchRoot",
    "Aff4NameHint: $Aff4NameHint",
    "CaseRoot: $Aff4CaseRoot",
    "ReaderToolsRoot: $ReaderToolsRoot",
    "Aff4ZipPath: $Aff4ZipPath",
    "BuildLog: $BuildLog",
    "CleanOut: $CleanOut",
    "FullNoGuardrails: $FullNoGuardrails",
    "ForceContainerHash: False",
    "SkipContainerHash: True",
    "PressureTestMode: $($PressureTestMode -or $FullNoGuardrails)",
    "FullNativeValues: $($FullNativeValues -or $PressureTestMode -or $FullNoGuardrails)",
    "MaxNativeRecords: $MaxNativeRecords",
    "MaxNativeBlocks: $MaxNativeBlocks",
    "EnableStreamInventory: $EnableStreamInventory",
    "CliTimeoutMinutes: $CliTimeoutMinutes",
    "RunnerExitCode: $runnerExit"
  ) | Set-Content -LiteralPath $summaryPath -Encoding UTF8
  if ($runnerExit -ne 0) {
    $emergencyZip = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_115_FAILED_WRAPPER_RESCUE.zip"
    $emergencyWork = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_115_FAILED_WRAPPER_RESCUE_Work"
    try {
      $uploadTool = Join-Path $SourceRoot "tools\Create-SourceProbeUploadZip.ps1"
      if ((Test-Path -LiteralPath $Aff4CaseRoot) -and (Test-Path -LiteralPath $uploadTool)) {
        $emergencyArgs = @{
          CaseRoot = $Aff4CaseRoot
          ZipPath = $emergencyZip
          UploadWorkRoot = $emergencyWork
          IncludeLogsTailOnly = $true
          IncludeStructuralDiagnostics = $true
        }
        if (![string]::IsNullOrWhiteSpace($ReaderToolsRoot)) { $emergencyArgs["ReaderToolsRoot"] = $ReaderToolsRoot }
        & $uploadTool @emergencyArgs
        Write-Warning "AFF4 runner failed, but an emergency diagnostic bundle was created: $emergencyZip"
      }
    } catch {
      Write-Warning "AFF4 emergency diagnostic bundle creation failed: $($_.Exception.Message)"
    }
    throw "AFF4 source-probe runner failed with exit code $runnerExit. Review case folder: $Aff4CaseRoot and summary: $summaryPath"
  }
  Assert-Leaf -PathValue $Aff4ZipPath -Description "AFF4 upload ZIP"
  $IdentifierAudit = Join-Path $SourceRoot "tools\Verify-ThinIdentifierCsvPrecision.ps1"
  if (Test-Path -LiteralPath $IdentifierAudit) {
    Write-Host "Running thin identifier CSV precision audit: $IdentifierAudit"
    powershell -ExecutionPolicy Bypass -File $IdentifierAudit -ZipPath $Aff4ZipPath
    if ($LASTEXITCODE -ne 0) { throw "Thin identifier CSV precision audit failed with exit code $LASTEXITCODE. Upload ZIP: $Aff4ZipPath" }
  }
  Remove-Item -LiteralPath ($Aff4ZipPath + ".sha256.txt") -Force -ErrorAction SilentlyContinue
  Write-Host "AFF4 workflow completed. Upload: $Aff4ZipPath"
  Write-Host "Thin/test mode: upload ZIP SHA256 sidecar intentionally not generated. Full validation can hash later if needed."
  Write-Host "AFF4 wrapper summary: $summaryPath"
  exit 0
}

# iOS CoreSpotlight thin workflow.
$script:IosReuseCacheWrapperLog = Join-Path (Join-Path $ThinCaseRoot "logs") "ios_reuse_cache_wrapper.log"
Write-IosReuseCacheWrapperLog "automated wrapper reuse-cache validation log initialized"
$AutoReusePriorIosCacheEffective = $AutoReusePriorIosCache.IsPresent -or !$DisableAutoReusePriorIosCache.IsPresent
if ($AutoReusePriorIosCacheEffective) { Write-Host "Automatic prior iOS reuse-cache selection is enabled." }
else { Write-Host "Automatic prior iOS reuse-cache selection is disabled." }
$ResolvedReuseIosCache = Resolve-IosReuseCache -ExplicitCache $ReuseIosCache -CaseRoot $ThinCaseRoot -SourcePath $InputZipOrFolder -AutoPrior:$AutoReusePriorIosCacheEffective
if (![string]::IsNullOrWhiteSpace($ResolvedReuseIosCache)) { Write-Host "Resolved iOS reuse cache: $ResolvedReuseIosCache" }

$ThinScript = Join-Path $SourceRoot "scripts\Run-V1_6_115-iOS-CoreSpotlight-AndZip.ps1"
if (!(Test-Path -LiteralPath $ThinScript)) { throw "Thin run script was not found after extraction: $ThinScript" }
$thinArgList = @(
  "-ExecutionPolicy", "Bypass", "-File", $ThinScript,
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
  } else { throw "Thin wrapper failed with exit code $LASTEXITCODE and did not create upload ZIP: $ThinZipPath" }
}

Write-IosReuseCacheWrapperLog "finalizing automated wrapper reuse-cache log after thin run; preserves entries if -CleanOut removed the initial case log"
Flush-IosReuseCacheWrapperLog

Write-Host "Creating single chat upload bundle: $ChatUploadBundlePath"
if (Test-Path -LiteralPath $ChatUploadBundleWorkRoot) { Remove-Item -LiteralPath $ChatUploadBundleWorkRoot -Recurse -Force }
New-Item -ItemType Directory -Path $ChatUploadBundleWorkRoot -Force | Out-Null
Copy-IfExistsToBundle -Path $BuildLog -Name "V1_6_115_build.log"
Copy-IfExistsToBundle -Path $ThinZipPath -Name "Upload_Thin_iOS_CoreSpotlight_V1_6_115.zip"
foreach ($name in @("case_info.json", "case_summary.json", "source_cache_manifest.json", "run_progress.tsv", "run_status.txt", "last_stage.txt", "last_progress.tsv", "THIN_PERFORMANCE_SUMMARY.md", "thin_performance_summary.csv", "wrapper_heartbeat.log", "VestigantSpotlight_tail250.log", "EXPORT_INDEX.csv")) {
  Copy-IfExistsToBundle -Path (Join-Path $ThinCaseRoot $name) -Name (Join-Path "case_root" $name)
}
foreach ($name in @("VestigantSpotlight.log", "ios_focused_zip_extract.log", "ios_focused_zip_extract_7z.log", "ios_zip_stage_heartbeat.log", "ios_app_database_extract_7z.log", "ios_reuse_cache.log", "ios_reuse_cache_wrapper.log")) {
  Copy-IfExistsToBundle -Path (Join-Path (Join-Path $ThinCaseRoot "logs") $name) -Name (Join-Path "case_logs" $name)
}
Write-TextLogSummaryToBundle -Path (Join-Path (Join-Path $ThinCaseRoot "logs") "ios_ffs_7z_inventory_raw_slt.txt") -Name (Join-Path "case_logs" "ios_ffs_7z_inventory_raw_slt_summary.txt")

@"
Vestigant Spotlight V1.6.115 automated wrapper run summary
Created: $(Get-Date -Format o)
Workflow: $SelectedWorkflow
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
Vestigant Spotlight V1.6.115 automated chat upload bundle
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
Write-Host "iOS workflow completed. Build log: $BuildLog"
Write-Host "Thin ZIP: $ThinZipPath"
Write-Host "Single chat upload bundle: $ChatUploadBundlePath"
