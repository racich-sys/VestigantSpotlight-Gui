param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_41",
  [string]$ZipInput = "E:\test second\Spotlight\Spotlight-V100.zip",
  [string]$CaseRoot = "Q:\SpotlightCase\TestMacOS_ZippedSpotlight_Thin_V1_6_41",
  [string]$ThinZipPath = "D:\Downloads\Upload_Thin_MacOS_ZippedSpotlight_V1_6_41.zip",
  [string]$UploadWorkRoot = "D:\Downloads\Upload_Thin_MacOS_ZippedSpotlight_V1_6_41_UploadWork",
  [string]$RunLog = "D:\Downloads\V1_6_41_macos_zipped_spotlight_thin.log",
  [int]$MaxNativeRecords = 50000,
  [int]$MaxNativeBlocks = 200000,
  [int]$TimeoutMinutes = 30,
  [switch]$CleanOut,
  [switch]$ForceContainerHash,
  [switch]$PackageOnly
)

$ErrorActionPreference = "Stop"

function Write-LogLine {
  param([string]$Message)
  $line = "{0} {1}" -f ([DateTime]::UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ")), $Message
  Write-Host $line
  Add-Content -LiteralPath $RunLog -Value $line
}

function Quote-NativeArg {
  param([string]$Value)
  if ($null -eq $Value) { return '""' }
  if ($Value -notmatch '[\s"`$&|<>^]') { return $Value }
  return '"' + ($Value -replace '"','\"') + '"'
}

function Copy-FileIfExists {
  param([string]$Path, [string]$DestinationDirectory)
  if (Test-Path -LiteralPath $Path) {
    New-Item -ItemType Directory -Force -Path $DestinationDirectory | Out-Null
    Copy-Item -LiteralPath $Path -Destination (Join-Path $DestinationDirectory (Split-Path -Leaf $Path)) -Force -ErrorAction SilentlyContinue
  }
}

function Copy-TreeFiltered {
  param([string]$Path, [string]$DestinationDirectory, [int64]$MaxBytes = 52428800)
  if (!(Test-Path -LiteralPath $Path)) { return }
  New-Item -ItemType Directory -Force -Path $DestinationDirectory | Out-Null
  Get-ChildItem -LiteralPath $Path -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
    $ext = $_.Extension.ToLowerInvariant()
    if ($ext -in @('.sqlite','.db','.db-wal','.db-shm','.wal','.shm')) { return }
    if ($_.Length -gt $MaxBytes) { return }
    $relative = $_.FullName.Substring((Resolve-Path -LiteralPath $Path).Path.Length).TrimStart('\')
    $target = Join-Path $DestinationDirectory $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $target -Force -ErrorAction SilentlyContinue
  }
}

function Invoke-NativeLogged {
  param(
    [string]$ExePath,
    [string[]]$Arguments,
    [string]$LogPath,
    [int]$TimeoutMinutes
  )
  $stdout = Join-Path (Split-Path -Parent $LogPath) "V1_6_41_macos_zipped_spotlight_stdout.txt"
  $stderr = Join-Path (Split-Path -Parent $LogPath) "V1_6_41_macos_zipped_spotlight_stderr.txt"
  Remove-Item -LiteralPath $stdout,$stderr -Force -ErrorAction SilentlyContinue
  $argString = ($Arguments | ForEach-Object { Quote-NativeArg $_ }) -join ' '
  Add-Content -LiteralPath $LogPath -Value "COMMAND: $ExePath $argString"
  $proc = Start-Process -FilePath $ExePath -ArgumentList $argString -RedirectStandardOutput $stdout -RedirectStandardError $stderr -NoNewWindow -PassThru
  $timeoutMs = [int64]$TimeoutMinutes * 60 * 1000
  $finished = $proc.WaitForExit([int]([Math]::Min($timeoutMs, [int]::MaxValue)))
  $timedOut = $false
  if (-not $finished) {
    $timedOut = $true
    Write-LogLine "CLI timeout reached after ${TimeoutMinutes} minutes; terminating process id=$($proc.Id)"
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
  }
  if (Test-Path -LiteralPath $stdout) {
    Add-Content -LiteralPath $LogPath -Value "`n--- STDOUT ---"
    Get-Content -LiteralPath $stdout -ErrorAction SilentlyContinue | Add-Content -LiteralPath $LogPath
  }
  if (Test-Path -LiteralPath $stderr) {
    Add-Content -LiteralPath $LogPath -Value "`n--- STDERR ---"
    Get-Content -LiteralPath $stderr -ErrorAction SilentlyContinue | Add-Content -LiteralPath $LogPath
  }
  if ($timedOut) { return 124 }
  $code = $proc.ExitCode
  if ($null -eq $code -or ([string]$code).Length -eq 0) {
    Add-Content -LiteralPath $LogPath -Value "Native process returned a blank ExitCode after WaitForExit; treating as diagnostic code -998."
    return -998
  }
  return [int]$code
}

Remove-Item -LiteralPath $RunLog -Force -ErrorAction SilentlyContinue
Write-LogLine "Starting V1.6.41.1 macOS zipped Spotlight thin test"
Write-LogLine "SourceRoot=$SourceRoot"
Write-LogLine "ZipInput=$ZipInput"
Write-LogLine "CaseRoot=$CaseRoot"
Write-LogLine "ThinZipPath=$ThinZipPath"

$cli = Join-Path $SourceRoot "build-msvc\Release\VestigantSpotlightCli.exe"
if (!(Test-Path -LiteralPath $cli)) { throw "CLI executable not found. Build V1.6.41.1 first: $cli" }
if (!(Test-Path -LiteralPath $ZipInput)) { throw "Spotlight ZIP not found: $ZipInput" }

if ($CleanOut) {
  Remove-Item -LiteralPath $CaseRoot -Recurse -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $UploadWorkRoot -Recurse -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $ThinZipPath -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath ($ThinZipPath + ".sha256.txt") -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Force -Path $CaseRoot | Out-Null
New-Item -ItemType Directory -Force -Path $UploadWorkRoot | Out-Null

# ZIP pre-inventory, kept small and text-only.
$inventoryDir = Join-Path $UploadWorkRoot "zip_preinventory"
New-Item -ItemType Directory -Force -Path $inventoryDir | Out-Null
$zipSummary = Join-Path $inventoryDir "zip_inventory_summary.txt"
$zipSample = Join-Path $inventoryDir "zip_entries_sample.csv"
try {
  Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
  $zip = [System.IO.Compression.ZipFile]::OpenRead($ZipInput)
  try {
    $entries = $zip.Entries
    $total = $entries.Count
    $spotlight = @($entries | Where-Object { $_.FullName -match '(^|/)\.Spotlight-V100(/|$)|(^|/)Spotlight-V100(/|$)' })
    $storev2 = @($entries | Where-Object { $_.FullName -match '(^|/)Store-V2(/|$)' })
    $storedb = @($entries | Where-Object { $_.FullName -match '(^|/)(\.store\.db|store\.db)$' })
    $dbstr = @($entries | Where-Object { $_.FullName -match '(^|/)dbStr' })
    $dbhdr = @($entries | Where-Object { $_.FullName -match '(^|/)dbHdr' })
    @(
      "zip_input=$ZipInput",
      "entry_count=$total",
      "spotlight_path_entries=$($spotlight.Count)",
      "store_v2_entries=$($storev2.Count)",
      "store_db_entries=$($storedb.Count)",
      "dbStr_entries=$($dbstr.Count)",
      "dbHdr_entries=$($dbhdr.Count)"
    ) | Set-Content -LiteralPath $zipSummary
    'full_name,compressed_length,length' | Set-Content -LiteralPath $zipSample
    $entries | Select-Object -First 500 | ForEach-Object {
      $name = ($_.FullName -replace '"','""')
      '"{0}",{1},{2}' -f $name, $_.CompressedLength, $_.Length
    } | Add-Content -LiteralPath $zipSample
  } finally {
    $zip.Dispose()
  }
  Write-LogLine "ZIP pre-inventory complete"
} catch {
  "ZIP pre-inventory failed: $($_.Exception.Message)" | Set-Content -LiteralPath $zipSummary
  Write-LogLine "ZIP pre-inventory failed: $($_.Exception.Message)"
}

$args = @(
  "--mode", "run",
  "--profile", "macos",
  "--input", $ZipInput,
  "--out", $CaseRoot,
  "--case-name", "MacOS_ZippedSpotlight_Thin_V1_6_41",
  "--investigator", "V1.6.41.1 macOS zipped Spotlight thin validation",
  "--decode-core-native-values",
  "--export-profile", "diagnostics",
  "--max-native-records", [string]$MaxNativeRecords,
  "--max-native-blocks", [string]$MaxNativeBlocks,
  "--verbose"
)
if ($ForceContainerHash) {
  $args += "--force-container-hash"
} else {
  $args += "--skip-container-hash"
}

if ($PackageOnly) {
  Write-LogLine "PackageOnly specified; skipping CLI launch and packaging existing CaseRoot diagnostics"
  $exitCode = -997
} else {
  Write-LogLine "Launching CLI with macOS profile"
  $exitCode = Invoke-NativeLogged -ExePath $cli -Arguments $args -LogPath $RunLog -TimeoutMinutes $TimeoutMinutes
  Write-LogLine "CLI exit code=$exitCode"
}

# Package focused evidence even on failure/timeout.
$caseCopy = Join-Path $UploadWorkRoot "case_focused_files"
New-Item -ItemType Directory -Force -Path $caseCopy | Out-Null
foreach ($rel in @(
  "case_info.json", "case_summary.csv", "evidence_sources.csv", "evidence_source_readiness.csv",
  "source_partition_probe.csv", "source_probe_signatures.csv", "SOURCE_INTAKE_PLAN.md",
  "store_inventory.csv", "store_selection.csv", "active_file_comparison_readiness.csv",
  "image_inventory_readiness.csv", "image_file_inventory.csv", "run_status.txt", "last_stage.txt",
  "run_progress.tsv", "last_progress.tsv", "EXPORT_INDEX.csv"
)) {
  Copy-FileIfExists -Path (Join-Path $CaseRoot $rel) -DestinationDirectory $caseCopy
}
Copy-TreeFiltered -Path (Join-Path $CaseRoot "logs") -DestinationDirectory (Join-Path $caseCopy "logs") -MaxBytes 52428800
Copy-TreeFiltered -Path (Join-Path $CaseRoot "exports") -DestinationDirectory (Join-Path $caseCopy "exports") -MaxBytes 52428800

$treeFile = Join-Path $UploadWorkRoot "case_file_tree.txt"
if (Test-Path -LiteralPath $CaseRoot) {
  Get-ChildItem -LiteralPath $CaseRoot -Recurse -File -ErrorAction SilentlyContinue |
    Sort-Object FullName |
    ForEach-Object { "{0}`t{1}" -f $_.Length, $_.FullName } |
    Set-Content -LiteralPath $treeFile
}

Copy-FileIfExists -Path $RunLog -DestinationDirectory $UploadWorkRoot
Copy-FileIfExists -Path (Join-Path (Split-Path -Parent $RunLog) "V1_6_41_macos_zipped_spotlight_stdout.txt") -DestinationDirectory $UploadWorkRoot
Copy-FileIfExists -Path (Join-Path (Split-Path -Parent $RunLog) "V1_6_41_macos_zipped_spotlight_stderr.txt") -DestinationDirectory $UploadWorkRoot

$summary = Join-Path $UploadWorkRoot "WRAPPER_SUMMARY.txt"
@(
  "wrapper=Run-V1_6_41-macOS-ZippedSpotlight-Thin-AndZip.ps1",
  "expected_profile=macos",
  "expected_native_stage=native_kv_persistence_macos_storev2",
  "unexpected_native_stage=native_kv_persistence_ios_corespotlight_compact",
  "zip_input=$ZipInput",
  "case_root=$CaseRoot",
  "cli_exit_code=$exitCode",
  "timeout_minutes=$TimeoutMinutes",
  "max_native_records=$MaxNativeRecords",
  "max_native_blocks=$MaxNativeBlocks",
  "thin_zip=$ThinZipPath"
) | Set-Content -LiteralPath $summary

if ($exitCode -ne 0) {
  "CLI exited with code $exitCode. Thin diagnostic bundle was still packaged." | Set-Content -LiteralPath (Join-Path $UploadWorkRoot "CLI_FAILURE_OR_TIMEOUT_NOTE.txt")
}

if (Test-Path -LiteralPath $ThinZipPath) { Remove-Item -LiteralPath $ThinZipPath -Force }
$thinZipParent = Split-Path -Parent $ThinZipPath
if (![string]::IsNullOrWhiteSpace($thinZipParent)) { New-Item -ItemType Directory -Force -Path $thinZipParent | Out-Null }
Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
[System.IO.Compression.ZipFile]::CreateFromDirectory($UploadWorkRoot, $ThinZipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
if (!(Test-Path -LiteralPath $ThinZipPath)) { throw "Thin ZIP was not created: $ThinZipPath" }
$hash = Get-FileHash -LiteralPath $ThinZipPath -Algorithm SHA256
$hash.Hash | Set-Content -LiteralPath ($ThinZipPath + ".sha256.txt")
Write-LogLine "Thin upload ZIP ready: $ThinZipPath"
Write-LogLine "Thin upload SHA256: $($hash.Hash)"
Write-Host "Upload these files:"
Write-Host "  $ThinZipPath"
Write-Host "  $RunLog"
Write-Host "  $($ThinZipPath).sha256.txt"
