param(
    [string]$Aff4Input = "O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4",
    [string]$Out = "Q:\SpotlightCase\V0_8_75_SingleAff4Probe",
    [string]$ReaderToolsRoot = "T:\VestigantReaderTools\aff4-cpp-lite",
    [string]$ZipPath = "D:\Downloads\Upload_Thin_V0_8_75_SingleAff4Probe.zip",
    [switch]$SkipUploadZip,
    [switch]$ForceContainerHash,
    [switch]$FullScan,
    [switch]$EnableAff4DynamicProbe,
    [switch]$EnableAff4VirtualApfsProbe,
    [switch]$EnableAff4StreamInventory,
    [switch]$IncludeLogsTailOnly,
    [switch]$CleanOut,
    [switch]$NoClipboardOrExplorer,
    [string]$ExternalSpotlightRoot = "",
    [string]$ExternalCompareOutRoot = "",
    [string]$UploadWorkRoot = "",
    [switch]$SkipExternalSpotlightHash,
    [int]$CliTimeoutMinutes = 30
)

$ErrorActionPreference = "Stop"

function Assert-NoWildcardPath {
    param(
        [Parameter(Mandatory=$true)][string]$PathValue,
        [Parameter(Mandatory=$true)][string]$Description
    )
    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        throw "$Description is blank. Provide one explicit path."
    }
    if ($PathValue.Contains("*") -or $PathValue.Contains("?")) {
        throw "$Description contains a wildcard. This helper intentionally probes one explicit AFF4 file only: $PathValue"
    }
}

function Ensure-Directory {
    param([Parameter(Mandatory=$true)][string]$PathValue)
    if ([string]::IsNullOrWhiteSpace($PathValue)) { throw "Cannot create a blank directory path." }
    New-Item -ItemType Directory -Force -Path $PathValue | Out-Null
    if (!(Test-Path -LiteralPath $PathValue)) { throw "Directory was not created: $PathValue" }
}

function Ensure-ParentDirectory {
    param([Parameter(Mandatory=$true)][string]$PathValue)
    $parent = Split-Path -Parent $PathValue
    if ([string]::IsNullOrWhiteSpace($parent)) { return }
    Ensure-Directory -PathValue $parent
}

function Resolve-CaseFile {
    param([Parameter(Mandatory=$true)][string]$RelativeName)
    $candidates = @(
        (Join-Path $Out $RelativeName),
        (Join-Path (Join-Path $Out "logs") $RelativeName),
        (Join-Path (Join-Path $Out "Upload") $RelativeName),
        (Join-Path (Join-Path $Out "Upload_Thin") $RelativeName)
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

function Assert-RequiredCaseOutputFile {
    param(
        [Parameter(Mandatory=$true)][string]$RelativeName,
        [Parameter(Mandatory=$true)][string]$Description
    )
    $resolved = Resolve-CaseFile -RelativeName $RelativeName
    if ([string]::IsNullOrWhiteSpace($resolved)) {
        throw "Required output missing after source-probe: $Description. Checked root/logs/Upload/Upload_Thin under $Out for $RelativeName"
    }
    $requiredItem = Get-Item -LiteralPath $resolved -Force
    if (!$requiredItem.PSIsContainer -and $requiredItem.Length -le 0) {
        throw "Required output exists but is empty after source-probe: $Description ($resolved)"
    }
    return $resolved
}

function Assert-RequiredOutputFile {
    param(
        [Parameter(Mandatory=$true)][string]$PathValue,
        [Parameter(Mandatory=$true)][string]$Description
    )
    if (!(Test-Path -LiteralPath $PathValue)) {
        throw "Required output missing after source-probe: $Description ($PathValue)"
    }
    $requiredItem = Get-Item -LiteralPath $PathValue -Force
    if (!$requiredItem.PSIsContainer -and $requiredItem.Length -le 0) {
        throw "Required output exists but is empty after source-probe: $Description ($PathValue)"
    }
}

function Assert-ZipContainsAnyRequired {
    param(
        [Parameter(Mandatory=$true)][string]$ZipFile,
        [Parameter(Mandatory=$true)][string[]]$EntryNames
    )
    Assert-RequiredOutputFile -PathValue $ZipFile -Description "thin upload ZIP"
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($ZipFile)
    try {
        $names = @{}
        foreach ($entry in $zip.Entries) { $names[$entry.FullName.Replace('\\','/')] = $true }
        foreach ($name in $EntryNames) {
            if (!$names.ContainsKey($name)) {
                throw "Thin upload ZIP is missing expected entry: $name in $ZipFile"
            }
        }
    } finally {
        $zip.Dispose()
    }
}

function Write-PathManifest {
    param([string]$Stage)
    $manifest = Join-Path $Out "case_path_manifest.txt"
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("Vestigant single-AFF4 source-probe path manifest") | Out-Null
    $lines.Add("Generated UTC: $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))") | Out-Null
    $lines.Add("Stage: $Stage") | Out-Null
    $lines.Add("RepoRoot: $RepoRoot") | Out-Null
    $lines.Add("CLI: $Cli") | Out-Null
    $lines.Add("AFF4 input: $Aff4Input") | Out-Null
    $lines.Add("Case output: $Out") | Out-Null
    $lines.Add("Case logs folder: $(Join-Path $Out 'logs')") | Out-Null
    $lines.Add("Reader tools: $ReaderToolsRoot") | Out-Null
    $lines.Add("Upload zip: $ZipPath") | Out-Null
    $lines.Add("External Spotlight root: $ExternalSpotlightRoot") | Out-Null
    $lines.Add("External compare output root: $ExternalCompareOutRoot") | Out-Null
    $lines.Add("Upload work root: $UploadWorkRoot") | Out-Null
    $lines.Add("") | Out-Null
    $check = @(
        "run_status.txt",
        "last_stage.txt",
        "VestigantSpotlight.log",
        "aff4_apfs_spotlight_target_scan.csv",
        "aff4_apfs_spotlight_inode_probe.csv",
        "aff4_apfs_spotlight_file_extent_probe.csv"
    )
    foreach ($name in $check) {
        $resolved = Resolve-CaseFile -RelativeName $name
        if ([string]::IsNullOrWhiteSpace($resolved)) {
            $lines.Add("MISSING: $name") | Out-Null
        } else {
            $item = Get-Item -LiteralPath $resolved -Force
            $lines.Add("FOUND: $name -> $resolved ($($item.Length) bytes)") | Out-Null
        }
    }
    $lines | Set-Content -LiteralPath $manifest -Encoding UTF8
}

function ConvertTo-ProcessArgumentString {
    param([Parameter(Mandatory=$true)][string[]]$ArgumentList)
    $quoted = New-Object System.Collections.Generic.List[string]
    foreach ($arg in $ArgumentList) {
        if ($null -eq $arg) {
            $quoted.Add('""') | Out-Null
        } elseif ($arg -notmatch '[\s"]') {
            $quoted.Add($arg) | Out-Null
        } else {
            $quoted.Add('"' + ($arg -replace '"', '\"') + '"') | Out-Null
        }
    }
    return ($quoted -join " ")
}

Assert-NoWildcardPath -PathValue $Aff4Input -Description "AFF4 input path"
Assert-NoWildcardPath -PathValue $Out -Description "Case output path"
Assert-NoWildcardPath -PathValue $ReaderToolsRoot -Description "Reader tools path"
Assert-NoWildcardPath -PathValue $ZipPath -Description "Upload ZIP path"
if (![string]::IsNullOrWhiteSpace($ExternalSpotlightRoot)) { Assert-NoWildcardPath -PathValue $ExternalSpotlightRoot -Description "External Spotlight root path" }
if (![string]::IsNullOrWhiteSpace($ExternalCompareOutRoot)) { Assert-NoWildcardPath -PathValue $ExternalCompareOutRoot -Description "External comparison output path" }
if (![string]::IsNullOrWhiteSpace($UploadWorkRoot)) { Assert-NoWildcardPath -PathValue $UploadWorkRoot -Description "Upload work root path" }

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Cli = Join-Path $RepoRoot "build-msvc\Release\VestigantSpotlightCli.exe"
$UploadTool = Join-Path $PSScriptRoot "Create-SourceProbeUploadZip.ps1"
$ExternalCompareTool = Join-Path $PSScriptRoot "Compare-ExternalSpotlightReference.ps1"
$RemainingMismatchDiagnosticsTool = Join-Path $PSScriptRoot "Create-ApfsRemainingMismatchDiagnostics.ps1"

if (!(Test-Path -LiteralPath $Cli)) {
    throw "CLI binary not found. Build this source tree first: $Cli"
}
$VersionFile = Join-Path $RepoRoot "VERSION"
$ExpectedVersion = ""
if (Test-Path -LiteralPath $VersionFile) {
    $ExpectedVersion = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
}
$CliVersionOutput = (& $Cli --version 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Unable to read CLI version from: $Cli"
}
if (![string]::IsNullOrWhiteSpace($ExpectedVersion) -and ($CliVersionOutput -notmatch [regex]::Escape($ExpectedVersion))) {
    throw "Built CLI version mismatch. Expected VERSION=$ExpectedVersion from $VersionFile but CLI reported: $CliVersionOutput. Rebuild this exact source tree before running the probe."
}
Write-Host "Verified built CLI version: $CliVersionOutput"

if (!(Test-Path -LiteralPath $Aff4Input)) {
    throw "Single AFF4 test image not found: $Aff4Input"
}
$item = Get-Item -LiteralPath $Aff4Input -Force
if ($item.PSIsContainer) {
    throw "AFF4 input points to a directory. This helper does not search folders or drives for AFF4 files. Provide the exact .aff4 file path: $Aff4Input"
}
if ([System.IO.Path]::GetExtension($item.FullName) -ine ".aff4") {
    throw "AFF4 input must end in .aff4 for this single-test helper. Provided: $($item.FullName)"
}
if (!(Test-Path -LiteralPath $ReaderToolsRoot)) {
    throw "Reader tools folder not found: $ReaderToolsRoot"
}
if (![string]::IsNullOrWhiteSpace($ExternalSpotlightRoot) -and !(Test-Path -LiteralPath $ExternalSpotlightRoot)) {
    throw "External Spotlight root not found: $ExternalSpotlightRoot"
}

if (Test-Path -LiteralPath $Out) {
    $existingChildren = @(Get-ChildItem -LiteralPath $Out -Force -ErrorAction SilentlyContinue | Select-Object -First 1)
    if ($existingChildren.Count -gt 0) {
        if ($CleanOut) {
            Write-Host "Removing existing case output folder because -CleanOut was supplied: $Out"
            Remove-Item -LiteralPath $Out -Recurse -Force
        } else {
            throw "Case output folder already exists and is not empty: $Out. Use a new -Out path, delete it manually, or rerun this helper with -CleanOut for a development probe rerun."
        }
    }
}
Ensure-Directory -PathValue $Out
Ensure-Directory -PathValue (Join-Path $Out "logs")
Ensure-Directory -PathValue (Join-Path $Out "Upload_Thin")
Ensure-ParentDirectory -PathValue $ZipPath
Write-PathManifest -Stage "pre-run"

Write-Host "Single AFF4 source-probe policy active."
Write-Host "No recursive AFF4 discovery will be performed."
Write-Host "AFF4 input: $($item.FullName)"
Write-Host "Case output: $Out"
Write-Host "Reader tools: $ReaderToolsRoot"
if ($EnableAff4VirtualApfsProbe) { Write-Host "AFF4 virtual APFS probe enabled through guarded libaff4 dynamic reads for this one explicit AFF4 file." }
Write-Host "CLI timeout: $CliTimeoutMinutes minute(s)"

$args = @(
    "--mode", "source-probe",
    "--input", $item.FullName,
    "--out", $Out,
    "--reader-tools", $ReaderToolsRoot,
    "--strict-single-aff4",
    "--verbose"
)
if ($ForceContainerHash) { $args += "--force-container-hash" }
if ($FullScan) { $args += "--full-scan" }
if ($EnableAff4DynamicProbe -or $EnableAff4VirtualApfsProbe) { $args += "--enable-aff4-dynamic-probe" }
if ($EnableAff4StreamInventory) { $args += "--enable-aff4-stream-inventory" }

$ProbeTimedOut = $false
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $Cli
$psi.Arguments = ConvertTo-ProcessArgumentString -ArgumentList $args
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$proc = New-Object System.Diagnostics.Process
$proc.StartInfo = $psi
if (!$proc.Start()) {
    throw "Unable to start Vestigant source-probe process: $Cli"
}
if (!$proc.WaitForExit([Math]::Max(1, $CliTimeoutMinutes) * 60 * 1000)) {
    $ProbeTimedOut = $true
    try { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue } catch {}
    $timeoutMessage = "Vestigant source-probe exceeded timeout of $CliTimeoutMinutes minute(s) and was stopped. Partial diagnostics will be packaged when available."
    Write-Warning $timeoutMessage
    Add-Content -LiteralPath (Join-Path $Out "run_status.txt") -Value "$((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')) stage=wrapper_timeout message=$timeoutMessage"
    Add-Content -LiteralPath (Join-Path $Out "run_progress.tsv") -Value "$((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))`t-1`twrapper_timeout`t$timeoutMessage"
    $exitCode = 124
} else {
    $exitCode = $proc.ExitCode
}
if ($exitCode -ne 0 -and !$ProbeTimedOut) {
    Write-PathManifest -Stage "failed-run"
    throw "Vestigant source-probe failed with exit code $exitCode. Review case output folder: $Out"
}

Write-PathManifest -Stage "post-run"

$logPath = Resolve-CaseFile -RelativeName "VestigantSpotlight.log"
if ([string]::IsNullOrWhiteSpace($logPath)) {
    $note = Join-Path $Out "source_probe_run_validation_note.txt"
    @(
        "WARNING: VestigantSpotlight.log was not found at case root, logs, Upload, or Upload_Thin after source-probe.",
        "The parser process returned exit code 0 and required APFS outputs are validated separately.",
        "Case output: $Out",
        "Generated UTC: $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))"
    ) | Set-Content -LiteralPath $note -Encoding UTF8
    Write-Warning "VestigantSpotlight.log was not found. Continuing because the process returned exit code 0 and required APFS outputs will be validated. A validation note was written: $note"
} elseif (![string]::IsNullOrWhiteSpace($ExpectedVersion)) {
    $logText = Get-Content -LiteralPath $logPath -Raw
    $versionPattern = "app_version=" + [regex]::Escape($ExpectedVersion)
    if ($logText -notmatch $versionPattern) {
        Write-Warning "Run log exists at $logPath but does not contain exact app_version marker $ExpectedVersion. Continuing because preflight already verified the built CLI version and required APFS outputs will be validated."
    }
}

$expectedRunOutputs = @(
    "aff4_apfs_spotlight_target_scan.csv",
    "aff4_apfs_spotlight_target_scan_summary.json",
    "aff4_apfs_spotlight_file_extent_probe.csv",
    "aff4_apfs_spotlight_file_extent_probe_summary.json",
    "aff4_apfs_spotlight_inode_probe.csv",
    "aff4_apfs_spotlight_inode_probe_summary.json",
        "aff4_apfs_spotlight_xattr_probe.csv",
        "aff4_apfs_spotlight_xattr_probe_summary.json",
        "AFF4_APFS_SPOTLIGHT_XATTR_PROBE.md",
    "AFF4_APFS_SPOTLIGHT_INODE_PROBE.md",
    "aff4_apfs_spotlight_file_copy_out.csv",
    "aff4_apfs_spotlight_file_copy_out_summary.json",
    "AFF4_APFS_SPOTLIGHT_FILE_COPY_OUT.md",
    "aff4_apfs_extracted_storev2_stage_groups.csv",
    "aff4_apfs_extracted_storev2_stage_files.csv",
    "aff4_apfs_extracted_storev2_stage_summary.json",
    "AFF4_APFS_EXTRACTED_STOREV2_STAGE.md",
    "aff4_apfs_staged_storev2_parser_probe.csv",
    "aff4_apfs_staged_storev2_parser_probe_summary.json",
    "AFF4_APFS_STAGED_STOREV2_PARSER_PROBE.md",
    "aff4_apfs_staged_storev2_enrichment_probe_summary.json",
    "AFF4_APFS_STAGED_STOREV2_ENRICHMENT_PROBE.md",
    "aff4_apfs_staged_storev2_artifacts_sample.csv",
    "aff4_apfs_staged_storev2_timeline_sample.csv",
    "aff4_apfs_staged_storev2_raw_key_values_sample.csv",
    "aff4_apfs_staged_storev2_raw_date_candidates_sample.csv",
    "aff4_apfs_staged_storev2_raw_failures_sample.csv"
)
$missingRunOutputs = New-Object System.Collections.Generic.List[string]
foreach ($relative in $expectedRunOutputs) {
    try {
        [void](Assert-RequiredCaseOutputFile -RelativeName $relative -Description $relative)
    } catch {
        $missingRunOutputs.Add($relative) | Out-Null
    }
}
if ($missingRunOutputs.Count -gt 0) {
    Write-Warning "AFF4/APFS copy-out outputs are incomplete. This is acceptable for a guarded/partial diagnostic upload. Missing: $($missingRunOutputs -join ', ')"
    Add-Content -LiteralPath (Join-Path $Out "run_status.txt") -Value "$((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')) stage=wrapper_partial_aff4_outputs message=missing=$($missingRunOutputs -join ';')"
} else {
    Write-Host "Verified expected AFF4/APFS probe outputs in: $Out"
}

$EffectiveExternalCompareOutRoot = ""
$CanRunExternalCompare = !$ProbeTimedOut -and $missingRunOutputs.Count -eq 0 -and
    (Test-Path -LiteralPath (Join-Path $Out "aff4_apfs_spotlight_file_copy_out.csv")) -and
    (Test-Path -LiteralPath (Join-Path $Out "aff4_apfs_extracted_storev2_stage_files.csv"))
if (![string]::IsNullOrWhiteSpace($ExternalSpotlightRoot) -and !$CanRunExternalCompare) {
    Write-Warning "Skipping external Spotlight comparison because AFF4/APFS copy-out did not complete. The thin upload will contain the AFF4 reader diagnostics needed for the next fix."
    Add-Content -LiteralPath (Join-Path $Out "run_status.txt") -Value "$((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')) stage=wrapper_external_compare_skipped message=aff4_apfs_copy_out_incomplete"
}
if (![string]::IsNullOrWhiteSpace($ExternalSpotlightRoot) -and $CanRunExternalCompare) {
    if (!(Test-Path -LiteralPath $ExternalCompareTool)) {
        throw "External Spotlight comparison helper not found: $ExternalCompareTool"
    }
    $compareToolText = Get-Content -LiteralPath $ExternalCompareTool -Raw
    if ($compareToolText -notmatch "CompareOutputRoot" -or $compareToolText -notmatch "V0_8_59_HOTFIX") {
        throw "External Spotlight comparison helper appears stale or unpatched: $ExternalCompareTool. Remove the existing source folder and expand the current package cleanly."
    }
    if ([string]::IsNullOrWhiteSpace($ExternalCompareOutRoot)) {
        $zipParent = Split-Path -Parent $ZipPath
        if ([string]::IsNullOrWhiteSpace($zipParent)) { $zipParent = "D:\Downloads" }
        $zipBase = [System.IO.Path]::GetFileNameWithoutExtension($ZipPath)
        if ([string]::IsNullOrWhiteSpace($zipBase)) { $zipBase = "VestigantSpotlight_ExternalCompare" }
        $EffectiveExternalCompareOutRoot = Join-Path $zipParent ($zipBase + "_ExternalCompare")
    } else {
        $EffectiveExternalCompareOutRoot = $ExternalCompareOutRoot
    }
    Ensure-Directory -PathValue $EffectiveExternalCompareOutRoot
    Write-Host "Running external Spotlight reference comparison against: $ExternalSpotlightRoot"
    Write-Host "External comparison output root: $EffectiveExternalCompareOutRoot"
    # Use positional parameters here intentionally. The V0_8_51 wrapper failure showed
    # that stale/mixed script copies could pass named tokens as positional values
    # (CaseRoot="-CaseRoot"). The preflight above confirms this is the hotfixed helper,
    # and the positional call avoids repeating that argument-binding failure mode.
    if ($SkipExternalSpotlightHash) {
        & $ExternalCompareTool $Out $ExternalSpotlightRoot "" $EffectiveExternalCompareOutRoot "aff4_apfs_external_spotlight" -SkipHash
    } else {
        & $ExternalCompareTool $Out $ExternalSpotlightRoot "" $EffectiveExternalCompareOutRoot "aff4_apfs_external_spotlight"
    }
    if ($LASTEXITCODE -ne 0) { throw "External Spotlight comparison helper failed with exit code $LASTEXITCODE" }
    if (Test-Path -LiteralPath $RemainingMismatchDiagnosticsTool) {
        Write-Host "Creating APFS remaining-mismatch diagnostics for external comparison output."
        & $RemainingMismatchDiagnosticsTool -CaseRoot $Out -CompareRoot $EffectiveExternalCompareOutRoot -OutputPrefix "aff4_apfs_remaining_mismatch"
        if ($LASTEXITCODE -ne 0) { throw "APFS remaining-mismatch diagnostics helper failed with exit code $LASTEXITCODE" }
    } else {
        Write-Warning "APFS remaining-mismatch diagnostics helper not found: $RemainingMismatchDiagnosticsTool"
    }
}


if (!$SkipUploadZip) {
    if (!(Test-Path -LiteralPath $UploadTool)) {
        throw "Upload packaging helper not found: $UploadTool"
    }
    $EffectiveUploadWorkRoot = $UploadWorkRoot
    if ([string]::IsNullOrWhiteSpace($EffectiveUploadWorkRoot)) {
        $zipParent = Split-Path -Parent $ZipPath
        if ([string]::IsNullOrWhiteSpace($zipParent)) { $zipParent = "D:\Downloads" }
        $zipBase = [System.IO.Path]::GetFileNameWithoutExtension($ZipPath)
        if ([string]::IsNullOrWhiteSpace($zipBase)) { $zipBase = "VestigantSpotlight_Upload" }
        $EffectiveUploadWorkRoot = Join-Path $zipParent ($zipBase + "_UploadWork")
    }
    $uploadArgs = @{
        CaseRoot = $Out
        ReaderToolsRoot = $ReaderToolsRoot
        ZipPath = $ZipPath
        UploadWorkRoot = $EffectiveUploadWorkRoot
        IncludeLogsTailOnly = $IncludeLogsTailOnly
    }
    if (![string]::IsNullOrWhiteSpace($EffectiveExternalCompareOutRoot)) { $uploadArgs["AdditionalOutputRoot"] = $EffectiveExternalCompareOutRoot }
    & $UploadTool @uploadArgs
    $expectedZipEntries = @(
        "run_status.txt",
        "aff4_apfs_spotlight_target_scan.csv",
        "aff4_apfs_spotlight_file_extent_probe.csv",
        "aff4_apfs_spotlight_inode_probe.csv",
        "aff4_apfs_spotlight_inode_probe_summary.json",
        "aff4_apfs_spotlight_xattr_probe.csv",
        "aff4_apfs_spotlight_xattr_probe_summary.json",
        "AFF4_APFS_SPOTLIGHT_XATTR_PROBE.md",
        "AFF4_APFS_SPOTLIGHT_INODE_PROBE.md",
        "case_path_manifest.txt",
        "UPLOAD_MANIFEST.txt",
        "aff4_apfs_spotlight_file_copy_out.csv",
        "aff4_apfs_extracted_storev2_stage_groups.csv",
        "aff4_apfs_extracted_storev2_stage_summary.json",
        "aff4_apfs_staged_storev2_parser_probe.csv",
        "aff4_apfs_staged_storev2_parser_probe_summary.json",
        "aff4_apfs_staged_storev2_enrichment_probe_summary.json",
        "aff4_apfs_staged_storev2_artifacts_sample.csv"
    )
    if (![string]::IsNullOrWhiteSpace($ExternalSpotlightRoot) -and $CanRunExternalCompare) {
        $expectedZipEntries += "aff4_apfs_external_spotlight_compare_summary.json"
        $expectedZipEntries += "AFF4_APFS_EXTERNAL_SPOTLIGHT_COMPARE.md"
        $expectedZipEntries += "aff4_apfs_remaining_mismatch_diagnostics.csv"
        $expectedZipEntries += "aff4_apfs_remaining_mismatch_diagnostics_summary.json"
    }
    $expectedZipEntries = @($expectedZipEntries | Where-Object {
        (Test-Path -LiteralPath (Join-Path $Out $_)) -or
        (![string]::IsNullOrWhiteSpace($EffectiveExternalCompareOutRoot) -and (Test-Path -LiteralPath (Join-Path $EffectiveExternalCompareOutRoot $_)))
    })
    Assert-ZipContainsAnyRequired -ZipFile $ZipPath -EntryNames $expectedZipEntries
    Write-Host "Verified thin upload ZIP contains expected AFF4/APFS outputs: $ZipPath"
    if (!$NoClipboardOrExplorer) {
        try { Set-Clipboard -Value $ZipPath -ErrorAction Stop } catch { Write-Warning "Could not copy upload ZIP path to clipboard: $($_.Exception.Message)" }
        try { Start-Process explorer.exe "/select,`"$ZipPath`"" } catch { Write-Warning "Could not open Explorer to the upload ZIP: $($_.Exception.Message)" }
        Write-Host "Thin upload ZIP path copied to clipboard and selected in Explorer when supported: $ZipPath"
    }
}

if ($ProbeTimedOut) {
    throw "Vestigant source-probe timed out after $CliTimeoutMinutes minute(s). A partial diagnostic upload ZIP was created when packaging succeeded: $ZipPath"
}
