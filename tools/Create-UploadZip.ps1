<#
Creates a Vestigant Spotlight upload ZIP.
V0_7_17: defaults to a focused review profile instead of all non-database files.
Profiles:
  Focused  - logs/status/progress, summaries, export index, key investigator CSVs, upload_samples
  Standard - Focused plus HTML review/dashboard files and standard investigator exports
  FullThin - all non-database/non-staging files; can still be large
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$CaseOut,

    [string]$ZipPath,

    [ValidateSet("Focused", "Standard", "FullThin")]
    [string]$Profile = "Focused"
)

$ErrorActionPreference = "Stop"

if (!(Test-Path -LiteralPath $CaseOut -PathType Container)) {
    throw "Case output folder not found: $CaseOut"
}

$caseRoot = (Resolve-Path -LiteralPath $CaseOut).Path
if ([string]::IsNullOrWhiteSpace($ZipPath)) {
    $leaf = Split-Path -Leaf $caseRoot
    $suffix = if ($Profile -eq "Focused") { "_FOCUSED" } elseif ($Profile -eq "Standard") { "_STANDARD" } else { "_FULL_THIN" }
    $ZipPath = Join-Path $caseRoot ("Upload_" + $leaf + $suffix + ".zip")
}

$zipFull = [System.IO.Path]::GetFullPath($ZipPath)
$temp = Join-Path ([System.IO.Path]::GetTempPath()) ("VestigantUpload_" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temp -Force | Out-Null

$copied = 0
$skipped = 0

function Should-SkipCommonFile {
    param([System.IO.FileInfo]$File)
    $excludeExt = @(".sqlite", ".db", ".db-wal", ".db-shm", ".sqlite-wal", ".sqlite-shm")
    $fileFull = [System.IO.Path]::GetFullPath($File.FullName)
    if ($fileFull.Equals($zipFull, [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
    if ($File.Name -like "Upload_*.zip") { return $true }
    if ($File.Name -like "*.case.sqlite") { return $true }
    if ($File.FullName -match "\\EvidenceStaging\\") { return $true }
    foreach ($ext in $excludeExt) {
        if ($File.Name.ToLowerInvariant().EndsWith($ext)) { return $true }
    }
    return $false
}

function Copy-CaseFile {
    param([string]$RelativePath)
    $src = Join-Path $caseRoot $RelativePath
    if (!(Test-Path -LiteralPath $src -PathType Leaf)) { return }
    $dst = Join-Path $temp $RelativePath
    $dstDir = Split-Path -Parent $dst
    if (!(Test-Path -LiteralPath $dstDir)) { New-Item -ItemType Directory -Path $dstDir -Force | Out-Null }
    Copy-Item -LiteralPath $src -Destination $dst -Force
    $script:copied++
}

function Copy-CaseDirectory {
    param([string]$RelativePath)
    $src = Join-Path $caseRoot $RelativePath
    if (!(Test-Path -LiteralPath $src -PathType Container)) { return }
    Get-ChildItem -LiteralPath $src -Recurse -File -Force | ForEach-Object {
        if (Should-SkipCommonFile $_) { $script:skipped++; return }
        $rel = $_.FullName.Substring($caseRoot.Length).TrimStart('\')
        $dst = Join-Path $temp $rel
        $dstDir = Split-Path -Parent $dst
        if (!(Test-Path -LiteralPath $dstDir)) { New-Item -ItemType Directory -Path $dstDir -Force | Out-Null }
        Copy-Item -LiteralPath $_.FullName -Destination $dst -Force
        $script:copied++
    }
}

try {
    if ($Profile -eq "FullThin") {
        Get-ChildItem -LiteralPath $caseRoot -Recurse -File -Force | ForEach-Object {
            if (Should-SkipCommonFile $_) { $script:skipped++; return }
            $rel = $_.FullName.Substring($caseRoot.Length).TrimStart('\')
            $dst = Join-Path $temp $rel
            $dstDir = Split-Path -Parent $dst
            if (!(Test-Path -LiteralPath $dstDir)) { New-Item -ItemType Directory -Path $dstDir -Force | Out-Null }
            Copy-Item -LiteralPath $_.FullName -Destination $dst -Force
            $script:copied++
        }
    } else {
        $focusedFiles = @(
            "run_status.txt",
            "last_stage.txt",
            "run_progress.tsv",
            "last_progress.tsv",
            "case_summary.json",
            "CASE_REVIEW_SUMMARY.txt",
            "EXPORT_INDEX.csv",
            "logs\VestigantSpotlight.log",
            "logs\run_status.txt",
            "logs\last_stage.txt",
            "logs\run_progress.tsv",
            "logs\last_progress.tsv",
            "logs\FATAL_CRASH.txt",
            "exports\EXPORT_INDEX.csv",
            "exports\object_usage_summary.csv",
            "exports\usage_timeline_attributed.csv",
            "exports\usage_event_detail_attributed_raw.csv",
            "exports\artifact_dates_wide.csv",
  "exports\date_field_attribution.csv",
            "exports\date_field_inventory.csv",
            "exports\snapshot_date_warnings.csv",
            "exports\usage_evidence.csv"
        )
        foreach ($rel in $focusedFiles) { Copy-CaseFile $rel }
        Copy-CaseDirectory "exports\upload_samples"

        if ($Profile -eq "Standard") {
            $standardFiles = @(
                "investigator_dashboard.html",
                "review_index.html",
                "exports\content_type_summary.csv",
                "exports\store_content_type_summary.csv",
                "exports\timeline_date_source_summary.csv"
            )
            foreach ($rel in $standardFiles) { Copy-CaseFile $rel }
        }
    }

    if ($copied -lt 1) {
        throw "No eligible files were found to include in upload ZIP. CaseOut=$caseRoot Skipped=$skipped Profile=$Profile"
    }

    if (Test-Path -LiteralPath $zipFull) { Remove-Item -LiteralPath $zipFull -Force }

    $items = Get-ChildItem -LiteralPath $temp -Force
    if ($items.Count -lt 1) {
        throw "Temporary upload staging folder is empty: $temp"
    }

    Compress-Archive -Path (Join-Path $temp "*") -DestinationPath $zipFull -Force

    if (!(Test-Path -LiteralPath $zipFull -PathType Leaf)) {
        throw "Compress-Archive completed but ZIP was not found: $zipFull"
    }

    $zipItem = Get-Item -LiteralPath $zipFull
    if ($zipItem.Length -le 0) {
        throw "Upload ZIP was created but is empty: $zipFull"
    }

    Write-Host "Created Vestigant upload ZIP: $zipFull"
    Write-Host "Profile: $Profile"
    Write-Host "Included files: $copied"
    Write-Host "Skipped files: $skipped"
    Write-Host "ZIP bytes: $($zipItem.Length)"
} finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
