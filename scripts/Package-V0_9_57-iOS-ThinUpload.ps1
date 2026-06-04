param(
  [string]$CaseRoot = "Q:\SpotlightCase\TestiOS_V0_9_57",
  [string]$OutZip = "D:\Downloads\Upload_Thin_iOS_GUI_V0_9_57_Check.zip",
  [string]$Work = "D:\Downloads\Upload_Thin_iOS_GUI_V0_9_57_Check"
)

$ErrorActionPreference = "Stop"

if (!(Test-Path -LiteralPath $CaseRoot)) { throw "CaseRoot not found: $CaseRoot" }

Remove-Item -LiteralPath $Work -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Work | Out-Null
$SkipLog = Join-Path $Work "THIN_UPLOAD_SKIPPED_LARGE_FILES.txt"
"Large CSVs are sampled in this thin upload. Full files remain in the local case folder: $CaseRoot" | Set-Content -LiteralPath $SkipLog -Encoding UTF8

$LargeCsvNames = @(
  "ios_ffs_file_inventory.csv",
  "ios_spotlight_referenced_paths.csv",
  "ios_spotlight_human_text_values.csv",
  "ios_spotlight_human_text_rollup.csv",
  "ios_spotlight_missing_from_ffs_text_detail.csv",
  "ios_spotlight_missing_from_ffs_text_coverage_summary.csv",
  "ios_spotlight_missing_from_ffs_text_detail.csv",
  "ios_spotlight_missing_from_ffs_candidates.csv",
  "ios_database_residency_candidates.csv",
  "ios_spotlight_object_identity.csv",
  "ios_spotlight_to_ffs_object_links.csv",
  "ios_spotlight_to_app_db_record_links.csv",
  "ios_timeline_index_updates.csv",
  "ios_string_probe_values.csv",
  "ios_record_investigation_hints.csv",
  "ios_record_string_probe_summary.csv",
  "ios_app_parsed_records.csv",
  "ios_app_live_activity_timeline.csv",
  "ios_communications_review_records.csv",
  "ios_spotlight_communication_candidates.csv",
  "ios_apple_messages_parsed_records.csv",
  "ios_whatsapp_parsed_records.csv",
  "ios_keychain_material_inventory.csv",
  "ios_keychain_support_reference_inventory.csv",
  "ios_spotlight_high_value_timeline.csv",
  "ios_spotlight_file_reference_review.csv",
  "ios_spotlight_url_reference_review.csv",
  "ios_spotlight_account_contact_reference_review.csv"
)
$MaxCopyBytes = 30MB
$SampleLines = 5001
$UploadSampleMaxBytes = 1MB
$UploadSampleLines = 1001
$KeepLargeUploadSampleNames = @(
  "ios_spotlight_high_value_text_context_review_sample.csv",
  "ios_spotlight_missing_from_ffs_high_value_candidates_sample.csv",
  "ios_spotlight_missing_from_ffs_text_detail_sample.csv",
  "ios_spotlight_missing_from_ffs_text_coverage_summary_sample.csv",
  "ios_spotlight_missing_from_ffs_candidates_sample.csv",
  "ios_spotlight_communication_record_review_sample.csv",
  "ios_spotlight_attachment_reference_review_sample.csv",
  "ios_spotlight_communication_summary_sample.csv",
  "parser_limits_and_suppression_summary.csv"
)

function Copy-ThinFile([System.IO.FileInfo]$File, [string]$RelativePath) {
  $Dest = Join-Path $Work $RelativePath
  New-Item -ItemType Directory -Force -Path (Split-Path $Dest -Parent) | Out-Null
  $leaf = $File.Name
  if (($LargeCsvNames -contains $leaf) -and ($File.Length -gt $MaxCopyBytes)) {
    $sampleRel = $RelativePath -replace '\.csv$', "_sample_first5000.csv"
    $sampleDest = Join-Path $Work $sampleRel
    New-Item -ItemType Directory -Force -Path (Split-Path $sampleDest -Parent) | Out-Null
    Get-Content -LiteralPath $File.FullName -TotalCount $SampleLines | Set-Content -LiteralPath $sampleDest -Encoding UTF8
    Add-Content -LiteralPath $SkipLog -Encoding UTF8 -Value ("SAMPLED,{0},{1},{2}" -f $RelativePath,$File.Length,$sampleRel)
    return
  }
  if (($RelativePath -match '(^|[\\/])exports[\\/]upload_samples[\\/]') -and ($File.Extension -ieq ".csv") -and ($File.Length -gt $UploadSampleMaxBytes) -and -not ($KeepLargeUploadSampleNames -contains $leaf)) {
    Get-Content -LiteralPath $File.FullName -TotalCount $UploadSampleLines | Set-Content -LiteralPath $Dest -Encoding UTF8
    Add-Content -LiteralPath $SkipLog -Encoding UTF8 -Value ("RESAMPLED_UPLOAD_SAMPLE,{0},{1},first1000_rows_kept" -f $RelativePath,$File.Length)
    return
  }
  Copy-Item -LiteralPath $File.FullName -Destination $Dest -Force
}

$Patterns = @(
  "run_status.txt",
  "last_stage.txt",
  "run_progress.tsv",
  "last_progress.tsv",
  "VestigantSpotlight.log",
  "source_cache_manifest.json",
  "ios_reuse_cache.log",
  "ios_zip_stage_heartbeat.log",
  "ios_zip_inventory_progress.tsv",
  "case_summary.json",
  "case_summary.csv",
  "source_probe_summary.json",
  "ios_input_store_entry_inventory.csv",
  "ios_zip_entry_probe.csv",
  "store_inventory.csv",
  "store_selection.csv",
  "ios_focused_zip_extract.log",
  "ios_focused_zip_extract_7z.log",
  "ios_app_database_extract_7z.log",
  "CASE_REVIEW_SUMMARY.txt",
  "EXPORT_INDEX.csv",
  "parser_limits_and_suppression_summary.csv",
  "ios_spotlight_text_context_priority_summary.csv",
  "ios_spotlight_chat_app_attribution_summary.csv",
  "ios_spotlight_high_value_text_context_review_sample.csv",
  "ios_spotlight_text_context_priority_summary_sample.csv",
  "ios_spotlight_chat_app_attribution_summary_sample.csv",
  "ios_spotlight_high_value_text_context_review_sample.csv",
  "ios_spotlight_text_context_review_sample.csv",
  "ios_store_parse_summary.csv",
  "ios_protection_class_summary.csv",
  "ios_artifact_hint_summary.csv",
  "ios_record_investigation_hints.csv",
  "ios_string_probe_values.csv",
  "ios_record_string_probe_summary.csv",
  "ios_string_probe_category_summary.csv",
  "ios_timeline_index_updates.csv",
  "ios_domain_url_summary.csv",
  "ios_redacted_investigation_summary.csv",
  "ios_ffs_file_inventory.csv",
  "ios_app_database_inventory.csv",
  "ios_app_database_record_inventory.csv",
  "ios_app_database_record_summary.csv",
  "ios_app_parsed_records.csv",
  "ios_app_parsed_record_summary.csv",
  "ios_apple_messages_parsed_records.csv",
  "ios_apple_messages_parsed_summary.csv",
  "ios_apple_messages_database_status.csv",
  "ios_app_live_activity_timeline.csv",
  "ios_communications_review_records.csv",
  "ios_communications_review_summary.csv",
  "ios_spotlight_communication_candidates.csv",
  "ios_whatsapp_database_status.csv",
  "ios_whatsapp_parsed_records.csv",
  "ios_whatsapp_parsed_summary.csv",
  "ios_keychain_material_inventory.csv",
  "ios_keychain_support_reference_inventory.csv",
  "ios_spotlight_referenced_paths.csv",
  "ios_spotlight_human_text_values.csv",
  "ios_spotlight_human_text_rollup.csv",
  "ios_spotlight_missing_from_ffs_candidates.csv",
  "ios_spotlight_residency_summary.csv",
  "ios_database_residency_candidates.csv",
  "ios_spotlight_object_identity.csv",
  "ios_spotlight_to_ffs_object_links.csv",
  "ios_spotlight_to_app_db_record_links.csv",
  "ios_spotlight_high_value_timeline.csv",
  "ios_spotlight_file_reference_review.csv",
  "ios_spotlight_url_reference_review.csv",
  "ios_spotlight_account_contact_reference_review.csv",
  "ios_spotlight_decode_gap_summary.csv",
  "content_type_summary.csv",
  "store_content_type_summary.csv",
  "parser_coverage_summary_sample.csv",
  "raw_key_values_sample.csv",
  "native_key_values_high_value_sample.csv",
  "*.json",
  "*.csv",
  "*.log",
  "*.txt"
)

$Seen = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($Pattern in $Patterns) {
  Get-ChildItem -LiteralPath $CaseRoot -Recurse -File -Filter $Pattern -ErrorAction SilentlyContinue |
    ForEach-Object {
      $Rel = $_.FullName.Substring($CaseRoot.Length).TrimStart('\','/')
      if ($Rel -match '(^|[\\/])Upload([\\/]|$)') { return }
      if ($Rel -match '(^|[\\/])Upload_Thin') { return }
      if ($Rel -match '(^|[\\/])build-msvc([\\/]|$)') { return }
      if ($Rel -match '(^|[\\/])EvidenceStaging[\\/].*([\\/]Cache[\\/].*\.txt|\.store\.db|store\.db)$') { return }
      if ($Seen.Contains($Rel)) { return }
      [void]$Seen.Add($Rel)
      Copy-ThinFile -File $_ -RelativePath $Rel
    }
}

Remove-Item -LiteralPath $OutZip -Force -ErrorAction SilentlyContinue
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($Work, $OutZip, [System.IO.Compression.CompressionLevel]::Optimal, $false)
Get-Item -LiteralPath $OutZip | Select-Object FullName,Length,LastWriteTime
Get-FileHash -LiteralPath $OutZip -Algorithm SHA256
try { Set-Clipboard -Value $OutZip } catch {}
try { explorer.exe /select,$OutZip | Out-Null } catch {}
