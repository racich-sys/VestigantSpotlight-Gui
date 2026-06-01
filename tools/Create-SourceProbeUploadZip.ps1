param(
    [Parameter(Mandatory=$true)][string]$CaseRoot,
    [string]$ReaderToolsRoot = "T:\VestigantReaderTools\aff4-cpp-lite",
    [string]$ZipPath = "",
    [string]$AdditionalOutputRoot = "",
    [string]$UploadWorkRoot = "",
    [switch]$IncludeLogsTailOnly
)

$ErrorActionPreference = "Stop"

function Copy-FirstExistingCaseFile {
    param(
        [Parameter(Mandatory=$true)][string]$RelativeName,
        [string]$OutputName = ""
    )

    if ([string]::IsNullOrWhiteSpace($OutputName)) { $OutputName = $RelativeName }

    $candidates = @(
        (Join-Path $CaseRoot $RelativeName),
        (Join-Path (Join-Path $CaseRoot "logs") $RelativeName),
        (Join-Path (Join-Path $CaseRoot "Upload") $RelativeName)
    )
    if (![string]::IsNullOrWhiteSpace($AdditionalOutputRoot)) {
        $candidates += (Join-Path $AdditionalOutputRoot $RelativeName)
    }

    foreach ($p in $candidates) {
        if (Test-Path -LiteralPath $p) {
            $dest = Join-Path $UploadRoot $OutputName
            $destParent = Split-Path -Parent $dest
            if ($destParent) { New-Item -ItemType Directory -Force -Path $destParent | Out-Null }

            if ($IncludeLogsTailOnly -and ($RelativeName -ieq "VestigantSpotlight.log")) {
                Get-Content -LiteralPath $p -Tail 250 | Set-Content -LiteralPath (Join-Path $UploadRoot "VestigantSpotlight_tail250.log") -Encoding UTF8
            } else {
                Copy-Item -LiteralPath $p -Destination $dest -Force
            }
            return $true
        }
    }
    return $false
}

if (!(Test-Path -LiteralPath $CaseRoot)) {
    throw "Case root not found: $CaseRoot"
}

if ([string]::IsNullOrWhiteSpace($ZipPath)) {
    $caseName = Split-Path -Leaf $CaseRoot
    if ([string]::IsNullOrWhiteSpace($caseName)) { $caseName = "Vestigant_SourceProbe" }
    $ZipPath = Join-Path "D:\Downloads" ("Upload_Thin_" + $caseName + ".zip")
}

if ([string]::IsNullOrWhiteSpace($UploadWorkRoot)) {
    $UploadRoot = Join-Path $CaseRoot "Upload_Thin"
} else {
    $UploadRoot = $UploadWorkRoot
}
Remove-Item -LiteralPath $UploadRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $ZipPath -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $UploadRoot | Out-Null

$Wanted = @(
    "run_status.txt",
    "last_stage.txt",
    "last_progress.tsv",
    "run_progress.tsv",
    "VestigantSpotlight.log",
    "case_path_manifest.txt",
    "source_probe_run_validation_note.txt",
    "case_info.json",
    "case_summary.csv",
    "case_summary.json",
    "evidence_sources.csv",
    "store_inventory.csv",
    "store_selection.csv",
    "parser_coverage.csv",
    "parser_failures.csv",
    "field_inventory.csv",
    "native_parse_summary.json",
    "native_parser_run_summary.json",
    "source_probe_summary.json",
    "source_probe_signatures.csv",
    "source_partition_probe.csv",
    "source_inventory.csv",
    "source_probe_inventory.csv",
    "evidence_source_readiness.csv",
    "image_file_inventory.csv",
    "image_inventory_readiness.csv",
    "active_file_comparison_readiness.csv",
    "reader_tool_readiness.csv",
    "aff4_cpp_lite_reader_readiness.csv",
    "aff4_cpp_lite_integration_readiness.csv",
    "aff4_cpp_lite_dynamic_load_probe.csv", "aff4_virtual_apfs_probe.csv", "aff4_virtual_apfs_probe_summary.json", "AFF4_VIRTUAL_APFS_PROBE.md", "aff4_apfs_container_superblock.csv", "aff4_apfs_container_superblock_summary.json", "aff4_apfs_checkpoint_descriptor_scan.csv", "AFF4_APFS_CONTAINER_VIEW.md", "aff4_apfs_volume_superblocks.csv", "aff4_apfs_volume_superblocks_summary.json", "AFF4_APFS_VOLUME_SUPERBLOCK_PROBE.md", "aff4_apfs_checkpoint_map.csv", "aff4_apfs_checkpoint_mapped_object_probe.csv", "aff4_apfs_checkpoint_map_summary.json", "AFF4_APFS_CHECKPOINT_MAP_PROBE.md", "aff4_apfs_object_id_probe.csv", "aff4_apfs_btree_node_probe.csv", "aff4_apfs_object_resolution_probe_summary.json", "AFF4_APFS_OBJECT_RESOLUTION_PROBE.md", "aff4_apfs_omap_phys_probe.csv", "aff4_apfs_omap_btree_root_probe.csv", "aff4_apfs_omap_lookup_probe.csv", "aff4_apfs_omap_btree_toc_probe.csv", "aff4_apfs_omap_leaf_kv_decode.csv", "aff4_apfs_omap_leaf_lookup_results.csv", "aff4_apfs_resolved_volume_superblocks.csv", "aff4_apfs_resolved_volume_superblocks_summary.json", "AFF4_APFS_RESOLVED_VOLUME_SUPERBLOCKS.md", "aff4_apfs_volume_omap_probe.csv", "AFF4_APFS_VOLUME_OMAP_PROBE.md", "aff4_apfs_volume_root_tree_lookup.csv", "aff4_apfs_volume_root_tree_lookup_summary.json", "AFF4_APFS_VOLUME_ROOT_TREE_LOOKUP.md", "aff4_apfs_root_tree_node_probe.csv", "aff4_apfs_root_tree_record_sample.csv", "aff4_apfs_root_tree_node_probe_summary.json", "AFF4_APFS_ROOT_TREE_NODE_PROBE.md", "aff4_apfs_root_tree_child_node_probe.csv", "aff4_apfs_root_tree_child_record_sample.csv", "aff4_apfs_root_tree_child_node_probe_summary.json", "AFF4_APFS_ROOT_TREE_CHILD_NODE_PROBE.md", "aff4_apfs_root_tree_descendant_node_probe.csv", "aff4_apfs_root_tree_descendant_record_sample.csv", "aff4_apfs_root_tree_descendant_node_probe_summary.json", "AFF4_APFS_ROOT_TREE_DESCENDANT_NODE_PROBE.md", "aff4_apfs_filesystem_namespace_seed.csv", "aff4_apfs_filesystem_namespace_seed_summary.json", "AFF4_APFS_FILESYSTEM_NAMESPACE_SEED.md", "aff4_apfs_spotlight_target_scan.csv", "aff4_apfs_spotlight_name_scan_sample.csv", "aff4_apfs_spotlight_copy_attempt.csv", "aff4_apfs_spotlight_file_extent_probe.csv", "aff4_apfs_spotlight_file_extent_probe_summary.json", "AFF4_APFS_SPOTLIGHT_FILE_EXTENT_PROBE.md", "aff4_apfs_spotlight_file_copy_out.csv", "aff4_apfs_spotlight_file_copy_out_summary.json", "AFF4_APFS_SPOTLIGHT_FILE_COPY_OUT.md", "aff4_apfs_extracted_storev2_stage_groups.csv", "aff4_apfs_extracted_storev2_stage_files.csv", "aff4_apfs_extracted_storev2_stage_summary.json", "AFF4_APFS_EXTRACTED_STOREV2_STAGE.md", "aff4_apfs_staged_storev2_parser_probe.csv", "aff4_apfs_staged_storev2_parser_probe_summary.json", "AFF4_APFS_STAGED_STOREV2_PARSER_PROBE.md", "aff4_apfs_staged_storev2_enrichment_probe_summary.json", "AFF4_APFS_STAGED_STOREV2_ENRICHMENT_PROBE.md", "aff4_apfs_staged_storev2_artifacts_sample.csv", "aff4_apfs_staged_storev2_timeline_sample.csv", "aff4_apfs_staged_storev2_raw_key_values_sample.csv", "aff4_apfs_staged_storev2_raw_date_candidates_sample.csv", "aff4_apfs_staged_storev2_raw_failures_sample.csv", "aff4_apfs_external_spotlight_external_manifest.csv", "aff4_apfs_external_spotlight_vestigant_manifest.csv", "aff4_apfs_external_spotlight_file_compare.csv", "aff4_apfs_external_spotlight_storev2_group_compare.csv", "aff4_apfs_external_spotlight_compare_summary.json", "AFF4_APFS_EXTERNAL_SPOTLIGHT_COMPARE.md", "aff4_apfs_remaining_mismatch_diagnostics.csv", "aff4_apfs_remaining_mismatch_diagnostics_summary.json", "aff4_apfs_remaining_mismatch_diagnostics.md", "aff4_apfs_spotlight_inode_probe.csv", "aff4_apfs_spotlight_inode_probe_summary.json",
        "aff4_apfs_spotlight_xattr_probe.csv",
        "aff4_apfs_spotlight_xattr_probe_summary.json",
        "AFF4_APFS_SPOTLIGHT_XATTR_PROBE.md", "AFF4_APFS_SPOTLIGHT_INODE_PROBE.md", "aff4_apfs_spotlight_target_scan_summary.json", "AFF4_APFS_SPOTLIGHT_TARGET_SCAN.md", "aff4_apfs_omap_probe_summary.json", "AFF4_APFS_OMAP_PROBE.md", "AFF4_APFS_OMAP_TOC_PROBE.md", "AFF4_APFS_OMAP_LEAF_KV_DECODE.md",
    "aff4_stream_inventory.csv",
    "aff4_stream_inventory_raw.txt",
    "aff4_zip_probe_summary.json",
    "aff4_zip_central_directory.csv",
    "aff4_direct_map_reader_probe.csv",
    "aff4_direct_map_reader_probe_summary.json",
    "AFF4_DIRECT_MAP_READER_PROBE.md",
    "aff4_direct_sqlite_candidate_carve.csv",
    "aff4_direct_sqlite_candidate_carve_summary.json",
    "AFF4_DIRECT_SQLITE_CANDIDATE_CARVE.md",
    "aff4_apfs_exact_file_signature_scan.csv",
    "aff4_apfs_exact_file_signature_scan_summary.json",
    "AFF4_APFS_EXACT_FILE_SIGNATURE_SCAN.md",
    "AFF4_ZIP_SINGLE_FILE_PROBE.md",
    "SOURCE_INTAKE_PLAN.md",
    "AFF4_APFS_READER_PLAN.md",
    "AFF4_CPP_LITE_RANDOM_ACCESS_PLAN.md",
    "AFF4_CPP_LITE_DYNAMIC_LOAD_PROBE.md",
    "AFF4_DIRECT_MAP_READER_REQUIRED.md",
    "AFF4_STREAM_SELECTION_PLAN.md",
    "exports/ios_store_parse_summary.csv",
    "exports/ios_string_probe_category_summary.csv",
    "exports/ios_string_probe_values.csv",
    "exports/ios_record_string_probe_summary.csv",
    "exports/ios_timeline_index_updates.csv",
    "exports/native_decode_attempts.csv",
    "exports/native_decode_errors.csv",
    "exports/native_partial_decode_errors.csv",
    "exports/parser_coverage_summary.csv",
    "exports/upload_samples/upload_table_counts.csv",
    "exports/upload_samples/upload_samples_manifest.csv",
    "IOS_CORESPOTLIGHT_PLAN.md",
    "INVESTIGATOR_UI_GUIDE.md"
)

$copied = New-Object System.Collections.Generic.List[string]
foreach ($name in $Wanted) {
    if (Copy-FirstExistingCaseFile -RelativeName $name) {
        $copied.Add($name) | Out-Null
    }
}

$CandidateDir = Join-Path $CaseRoot "Aff4DirectSqliteCandidates"
if (Test-Path -LiteralPath $CandidateDir) {
    $CandidateUploadDir = Join-Path $UploadRoot "Aff4DirectSqliteCandidates"
    New-Item -ItemType Directory -Force -Path $CandidateUploadDir | Out-Null
    $budgetBytes = 24MB
    $usedBytes = 0L
    $copiedCandidateCount = 0
    Get-ChildItem -LiteralPath $CandidateDir -File -Filter "*.db" -ErrorAction SilentlyContinue |
        Sort-Object Length, Name |
        ForEach-Object {
            if ($copiedCandidateCount -lt 8 -and (($usedBytes + $_.Length) -le $budgetBytes)) {
                Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $CandidateUploadDir $_.Name) -Force
                $usedBytes += $_.Length
                $copiedCandidateCount += 1
            }
        }
    if ($copiedCandidateCount -gt 0) {
        $copied.Add("Aff4DirectSqliteCandidates/*.db") | Out-Null
    }
}

if (![string]::IsNullOrWhiteSpace($AdditionalOutputRoot) -and (Test-Path -LiteralPath $AdditionalOutputRoot)) {
    Get-ChildItem -LiteralPath $AdditionalOutputRoot -Recurse -File -ErrorAction SilentlyContinue |
        Select-Object FullName, Length, LastWriteTime |
        Format-Table -AutoSize |
        Out-String |
        Set-Content -LiteralPath (Join-Path $UploadRoot "additional_output_file_inventory.txt") -Encoding UTF8
    $copied.Add("additional_output_file_inventory.txt") | Out-Null
}

if (Test-Path -LiteralPath $ReaderToolsRoot) {
    $manifest = Join-Path $ReaderToolsRoot "reader_tools_manifest.csv"
    if (Test-Path -LiteralPath $manifest) {
        Copy-Item -LiteralPath $manifest -Destination (Join-Path $UploadRoot "reader_tools_manifest.csv") -Force
        $copied.Add("reader_tools_manifest.csv") | Out-Null
    }

    Get-ChildItem -LiteralPath $ReaderToolsRoot -Recurse -File -ErrorAction SilentlyContinue |
        Select-Object FullName, Length, LastWriteTime |
        Format-Table -AutoSize |
        Out-String |
        Set-Content -LiteralPath (Join-Path $UploadRoot "reader_tools_file_inventory.txt") -Encoding UTF8
    $copied.Add("reader_tools_file_inventory.txt") | Out-Null
}

Get-ChildItem -LiteralPath $CaseRoot -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notlike "*\Upload_Thin\*" } |
    Select-Object FullName, Length, LastWriteTime |
    Format-Table -AutoSize |
    Out-String |
    Set-Content -LiteralPath (Join-Path $UploadRoot "case_file_inventory.txt") -Encoding UTF8
$copied.Add("case_file_inventory.txt") | Out-Null

# Copy ExtractedSpotlight small development artifacts into the thin upload when present.
$ExtractedSpotlightRoot = Join-Path $CaseRoot "ExtractedSpotlight"
if (Test-Path -LiteralPath $ExtractedSpotlightRoot) {
    $extractCopied = 0
    $extractBytes = 0L
    Get-ChildItem -LiteralPath $ExtractedSpotlightRoot -Recurse -File -ErrorAction SilentlyContinue |
        Sort-Object FullName |
        ForEach-Object {
            if ($extractCopied -ge 300) { return }
            if ($_.Length -gt 67108864) { return }
            if (($extractBytes + $_.Length) -gt 134217728) { return }
            $rel = $_.FullName.Substring($ExtractedSpotlightRoot.Length).TrimStart([char]'\',[char]'/')
            $dest = Join-Path (Join-Path $UploadRoot "ExtractedSpotlight") $rel
            $destParent = Split-Path -Parent $dest
            if ($destParent) { New-Item -ItemType Directory -Force -Path $destParent | Out-Null }
            Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
            $extractCopied++
            $extractBytes += $_.Length
        }
    if ($extractCopied -gt 0) { $copied.Add("ExtractedSpotlight/ copied_files=$extractCopied bytes=$extractBytes") | Out-Null }
}

$manifestPath = Join-Path $UploadRoot "UPLOAD_MANIFEST.txt"
@(
    "Vestigant source-probe upload bundle",
    "Generated UTC: $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "CaseRoot: $CaseRoot",
    "ReaderToolsRoot: $ReaderToolsRoot",
    "ZipPath: $ZipPath",
    "AdditionalOutputRoot: $AdditionalOutputRoot",
    "UploadWorkRoot: $UploadRoot",
    "",
    "Copied files:",
    ($copied | Sort-Object -Unique | ForEach-Object { "- $_" })
) | Set-Content -LiteralPath $manifestPath -Encoding UTF8

$files = Get-ChildItem -LiteralPath $UploadRoot -File -Force
if ($files.Count -eq 0) {
    throw "No source-probe files were copied into $UploadRoot. Check CaseRoot: $CaseRoot"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ZipPath) | Out-Null
Compress-Archive -Path (Join-Path $UploadRoot "*") -DestinationPath $ZipPath -Force

if (!(Test-Path -LiteralPath $ZipPath)) {
    throw "Compress-Archive did not create: $ZipPath"
}

Write-Host "Created upload package: $ZipPath"
Get-Item -LiteralPath $ZipPath | Select-Object FullName, Length
Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256
