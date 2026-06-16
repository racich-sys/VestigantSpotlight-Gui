param(
    [Parameter(Mandatory=$true)][string]$InputZipOrFolder,
    [string]$Out = "Q:\SpotlightCase\iOS_CoreSpotlight",
    [string]$ZipPath = "D:\Downloads\Upload_Thin_iOS_CoreSpotlight.zip",
    [switch]$CleanOut,
    [switch]$NoClipboardOrExplorer,
    [switch]$FullDiagnostics,
    [ValidateSet("diagnostics","run")][string]$RunMode = "diagnostics",
    [ValidateSet("minimal","investigator","diagnostics","support","full")][string]$ExportProfile = "",
    [switch]$ForceContainerHash,
    [switch]$FullNativeValues,
    [switch]$DiagnosticFullNativeDb,
    [int]$MaxNativeRecords = 0,
    [int]$MaxNativeBlocks = 0,
    [switch]$MaterializeIosSupportDb,
    [switch]$NoCsvExports
)

$ErrorActionPreference = "Stop"

function Format-SizeMB {
    param([double]$Bytes)
    if ($Bytes -le 0) { return "0.0" }
    return ("{0:N1}" -f ($Bytes / 1MB))
}

function Get-FolderSizeBytesSafe {
    param([string]$PathValue)
    try {
        if ([string]::IsNullOrWhiteSpace($PathValue) -or !(Test-Path -LiteralPath $PathValue)) { return 0 }
        $sum = 0L
        Get-ChildItem -LiteralPath $PathValue -Recurse -File -Force -ErrorAction SilentlyContinue | ForEach-Object { $sum += [int64]$_.Length }
        return $sum
    } catch { return 0 }
}

function Get-LastMeaningfulLinesSafe {
    param([string]$CaseRoot, [int]$Count = 5)
    $candidates = @(
        (Join-Path $CaseRoot "run_progress.tsv"),
        (Join-Path (Join-Path $CaseRoot "logs") "run_progress.tsv"),
        (Join-Path $CaseRoot "run_status.txt"),
        (Join-Path (Join-Path $CaseRoot "logs") "run_status.txt")
    )
    foreach ($candidate in $candidates) {
        try {
            if (Test-Path -LiteralPath $candidate) { return @(Get-Content -LiteralPath $candidate -Tail $Count -ErrorAction SilentlyContinue) }
        } catch {}
    }
    return @()
}


function ConvertTo-ProcessArgumentString {
    param([Parameter(Mandatory=$true)][string[]]$ArgumentList)
    $quoted = New-Object System.Collections.Generic.List[string]
    foreach ($arg in $ArgumentList) {
        if ($null -eq $arg) {
            [void]$quoted.Add('""')
        } elseif ($arg -notmatch '[\s"]') {
            [void]$quoted.Add($arg)
        } else {
            [void]$quoted.Add('"' + ($arg -replace '"', '\\"') + '"')
        }
    }
    return ($quoted -join " ")
}

function Start-ProcessWithTriageHeartbeat {
    param(
        [Parameter(Mandatory=$true)][string]$ExePath,
        [Parameter(Mandatory=$true)][string[]]$ArgumentList,
        [Parameter(Mandatory=$true)][string]$CaseRoot,
        [int]$IntervalSeconds = 60,
        [int]$TimeoutMinutes = 0
    )
    $heartbeatPath = Join-Path (Join-Path $CaseRoot "logs") "wrapper_heartbeat.log"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $heartbeatPath) | Out-Null
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $ExePath
    $psi.Arguments = ConvertTo-ProcessArgumentString -ArgumentList $ArgumentList
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $false
    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    if (!$proc.Start()) { throw "Unable to start process: $ExePath" }
    $started = Get-Date
    while (!$proc.HasExited) {
        Start-Sleep -Seconds ([Math]::Max(5,$IntervalSeconds))
        try { $proc.Refresh() } catch {}
        $now = Get-Date
        $elapsed = New-TimeSpan -Start $started -End $now
        $caseDb = Join-Path $CaseRoot "VestigantSpotlight.case.sqlite"
        $dbMb = 0.0
        try { if (Test-Path -LiteralPath $caseDb) { $dbMb = (Get-Item -LiteralPath $caseDb).Length / 1MB } } catch {}
        $outMb = (Get-FolderSizeBytesSafe -PathValue $CaseRoot) / 1MB
        $cpu = 0.0
        $ws = 0.0
        try { $cpu = $proc.TotalProcessorTime.TotalSeconds; $ws = $proc.WorkingSet64 / 1MB } catch {}
        $stamp = $now.ToString('HH:mm:ss')
        $line = ("{0} - Heartbeat: process_id={1} cpu_seconds={2:N1} working_set_mb={3:N1} case_db_mb={4:N1} output_mb={5:N1} elapsed={6}" -f $stamp,$proc.Id,$cpu,$ws,$dbMb,$outMb,$elapsed.ToString('hh\:mm\:ss'))
        Write-Host $line
        Add-Content -LiteralPath $heartbeatPath -Value $line -Encoding UTF8
        $status = "{0} - HeartbeatStatus: status=running" -f $stamp
        Write-Host $status
        Add-Content -LiteralPath $heartbeatPath -Value $status -Encoding UTF8
        $tail = Get-LastMeaningfulLinesSafe -CaseRoot $CaseRoot -Count 5
        foreach ($t in $tail) {
            $tailLine = "{0} - HeartbeatTail: {1}" -f $stamp, $t
            Write-Host $tailLine
            Add-Content -LiteralPath $heartbeatPath -Value $tailLine -Encoding UTF8
        }
        if ($TimeoutMinutes -gt 0 -and $elapsed.TotalMinutes -ge $TimeoutMinutes) {
            $timeoutLine = "{0} - HeartbeatStatus: timeout_minutes={1}; stopping process" -f $stamp,$TimeoutMinutes
            Write-Warning $timeoutLine
            Add-Content -LiteralPath $heartbeatPath -Value $timeoutLine -Encoding UTF8
            try { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue } catch {}
            return 124
        }
    }
    try { $proc.WaitForExit() } catch {}
    return $proc.ExitCode
}

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptRoot
$Cli = Join-Path $ProjectRoot "build-msvc\Release\VestigantSpotlightCli.exe"
$UploadScript = Join-Path $ScriptRoot "Create-SourceProbeUploadZip.ps1"

if (!(Test-Path -LiteralPath $Cli)) { throw "CLI binary not found. Build this source tree first: $Cli" }
if (!(Test-Path -LiteralPath $InputZipOrFolder)) { throw "Input not found: $InputZipOrFolder" }
if ($CleanOut) { Remove-Item -LiteralPath $Out -Recurse -Force -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path $Out | Out-Null

# Record what CoreSpotlight stores exist inside the submitted focused ZIP/folder
# before the CLI stages it. This makes discovery gaps visible in thin uploads.
$InventoryPath = Join-Path $Out "ios_input_store_entry_inventory.csv"
"entry_type,length,last_write_time,full_name" | Set-Content -LiteralPath $InventoryPath -Encoding UTF8
if (Test-Path -LiteralPath $InputZipOrFolder -PathType Leaf) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($InputZipOrFolder)
    try {
        $zip.Entries |
          Where-Object { $_.FullName -match '(^|[\\/])(\.store\.db|store\.db)$' } |
          Sort-Object FullName |
          ForEach-Object {
              $leaf = [System.IO.Path]::GetFileName($_.FullName.Replace('/','\'))
              $line = ('{0},{1},{2},{3}' -f $leaf,$_.Length,$_.LastWriteTime.UtcDateTime.ToString('o'),('"' + ($_.FullName -replace '"','""') + '"'))
              Add-Content -LiteralPath $InventoryPath -Value $line -Encoding UTF8
          }
    } finally { $zip.Dispose() }
} else {
    Get-ChildItem -LiteralPath $InputZipOrFolder -Recurse -File -Force |
      Where-Object { $_.Name -in @('store.db','.store.db') } |
      Sort-Object FullName |
      ForEach-Object {
          $line = ('{0},{1},{2},{3}' -f $_.Name,$_.Length,$_.LastWriteTimeUtc.ToString('o'),('"' + ($_.FullName -replace '"','""') + '"'))
          Add-Content -LiteralPath $InventoryPath -Value $line -Encoding UTF8
      }
}


$EffectiveExportProfile = if (![string]::IsNullOrWhiteSpace($ExportProfile)) { $ExportProfile } elseif ($FullDiagnostics) { "diagnostics" } else { "minimal" }
Write-Host "iOS run mode: $RunMode"
Write-Host "iOS export profile: $EffectiveExportProfile (FullDiagnostics=$($FullDiagnostics.IsPresent))"
Write-Host "iOS full native metadata values: $($FullNativeValues.IsPresent)"
Write-Host "iOS diagnostic full native DB persistence: $($DiagnosticFullNativeDb.IsPresent)"
if ($MaxNativeRecords -gt 0) { Write-Host "iOS max native records override: $MaxNativeRecords" }
if ($MaxNativeBlocks -gt 0) { Write-Host "iOS max native blocks override: $MaxNativeBlocks" }

$cliArgs = @(
  "--mode", $RunMode,
  "--profile", "ios",
  "--input", $InputZipOrFolder,
  "--out", $Out,
  "--full-scan",
  "--decode-core-native-values",
  "--export-profile", $EffectiveExportProfile,
  "--verbose"
)
if ($ForceContainerHash) { $cliArgs += "--force-container-hash" }
if ($FullNativeValues) { $cliArgs += "--experimental-full-native-values" }
if ($DiagnosticFullNativeDb) { $cliArgs += "--diagnostic-full-native-db" }
if ($MaxNativeRecords -gt 0) { $cliArgs += @("--max-native-records", [string]$MaxNativeRecords) }
if ($MaxNativeBlocks -gt 0) { $cliArgs += @("--max-native-blocks", [string]$MaxNativeBlocks) }
if ($MaterializeIosSupportDb) { $cliArgs += "--materialize-ios-support-db" }
if ($NoCsvExports) { $cliArgs += "--no-csv-exports" }
$cliExit = Start-ProcessWithTriageHeartbeat -ExePath $Cli -ArgumentList $cliArgs -CaseRoot $Out -IntervalSeconds 60
$global:LASTEXITCODE = $cliExit
if ($cliExit -ne 0) { Write-Warning "iOS CLI exited with code $cliExit; upload bundle will still be attempted for diagnostics." }

try {
  $perfScript = Join-Path $ScriptRoot "Generate-ThinPerformanceSummary.ps1"
  if (Test-Path -LiteralPath $perfScript) {
    if ($RunMode -eq "run" -and $EffectiveExportProfile -eq "investigator") {
      & $perfScript -CaseRoot $Out -SlowExportSeconds 30 -ReportTitle "Production Performance Summary" -OutputPrefix "production_performance_summary" -OutputMarkdownName "PRODUCTION_PERFORMANCE_SUMMARY.md"
    } else {
      & $perfScript -CaseRoot $Out -SlowExportSeconds 30
    }
  }
} catch {
  Write-Warning "Unable to generate performance summary: $($_.Exception.Message)"
}

& $UploadScript `
  -CaseRoot $Out `
  -ZipPath $ZipPath `
  -UploadWorkRoot "D:\Downloads\iOS_CoreSpotlight_UploadWork" `
  -IncludeLogsTailOnly

Get-Item $ZipPath | Select-Object FullName,Length
Get-FileHash $ZipPath -Algorithm SHA256

# V1.6.17 iOS validation fix: fail fast if the focused thin ZIP omits bounded row-level upload samples.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$requiredUploadSamples = @(
  "exports/upload_samples/upload_table_counts.csv",
  "exports/upload_samples/upload_samples_manifest.csv",
  "exports/upload_samples/artifacts_sample.csv",
  "exports/upload_samples/raw_key_values_sample.csv",
  "exports/upload_samples/timeline_events_sample.csv",
  "exports/upload_samples/usage_evidence_sample.csv",
  "exports/upload_samples/active_file_comparison_validation_checks_sample.csv",
  "exports/upload_samples/active_file_comparison_candidate_summary_sample.csv",
  "exports/upload_samples/orphaned_deleted_candidates_focus.csv",
  "exports/upload_samples/ios_coreduet_interactionc_database_status_sample.csv",
  "exports/upload_samples/ios_coreduet_interactionc_validation_checks_sample.csv",
  "exports/upload_samples/ios_coreduet_interactionc_summary_sample.csv",
  "exports/upload_samples/ios_coreduet_interactionc_events_sample.csv"
)
if ($EffectiveExportProfile -in @("support", "full", "diagnostics")) {
  $requiredUploadSamples += "exports/upload_samples/parser_coverage_summary_sample.csv"
  $requiredUploadSamples += "exports/upload_samples/ios_record_investigation_hints_sample.csv"
}
$zipForValidation = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
try {
  $zipEntrySet = @{}
  foreach ($entry in $zipForValidation.Entries) { $zipEntrySet[$entry.FullName.Replace('\','/')] = $true }
  $missingRequiredUploadSamples = @()
  foreach ($required in $requiredUploadSamples) {
    if (!$zipEntrySet.ContainsKey($required)) { $missingRequiredUploadSamples += $required }
  }
  if ($missingRequiredUploadSamples.Count -gt 0) {
    $missingMessage = "iOS thin upload ZIP is missing required bounded validation samples: $($missingRequiredUploadSamples -join ', ')"
    if ($cliExit -ne 0) {
      Write-Warning ("$missingMessage. CLI exit code was $cliExit, so this upload is being preserved as an incomplete-run diagnostic bundle instead of failing post-upload validation.")
      try {
        $diagnosticStatusPath = Join-Path $Out "focused_zip_validation_status.txt"
        Set-Content -LiteralPath $diagnosticStatusPath -Value @(
          "status=incomplete_run_diagnostic_bundle",
          "cli_exit_code=$cliExit",
          "missing_required_upload_samples=$($missingRequiredUploadSamples -join ';')",
          "note=V1.6.40.1.1 preserves incomplete-run upload ZIPs for review when the CLI exits before bounded exports are generated."
        ) -Encoding UTF8
      } catch {}
    } else {
      throw $missingMessage
    }
  }
} finally {
  $zipForValidation.Dispose()
}

if (!$NoClipboardOrExplorer) {
    try { Set-Clipboard -Value $ZipPath } catch {}
    try { explorer.exe /select,"$ZipPath" } catch {}
}
