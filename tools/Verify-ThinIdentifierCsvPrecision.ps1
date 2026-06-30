param(
  [Parameter(Mandatory=$true)][string]$ZipPath,
  [string]$WorkRoot = "",
  [int]$MaxSamples = 25
)

$ErrorActionPreference = "Stop"
if (!(Test-Path -LiteralPath $ZipPath -PathType Leaf)) { throw "Thin upload ZIP not found: $ZipPath" }
if ([string]::IsNullOrWhiteSpace($WorkRoot)) {
  $safeName = [System.IO.Path]::GetFileNameWithoutExtension($ZipPath)
  $WorkRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("VestigantThinIdentifierAudit_" + $safeName + "_" + [guid]::NewGuid().ToString("N"))
}
if (Test-Path -LiteralPath $WorkRoot) { Remove-Item -LiteralPath $WorkRoot -Recurse -Force }
New-Item -ItemType Directory -Path $WorkRoot -Force | Out-Null
Expand-Archive -LiteralPath $ZipPath -DestinationPath $WorkRoot -Force

$scientificNotation = '^[+-]?(?:\d+\.?\d*|\d*\.\d+)[eE][+-]?\d+$'
$targets = @{
  'aff4_apfs_staged_storev2_path_reconstruction_sample.csv' = @('artifact_id','raw_record_id','inode_num','parent_inode_num','store_id')
  'aff4_apfs_staged_storev2_timeline_sample.csv' = @('timeline_event_id','artifact_id','inode_num','parent_inode_num')
  'aff4_apfs_staged_storev2_points_of_interest_sample.csv' = @('poi_id','artifact_id','inode_num','parent_inode_num')
  'aff4_apfs_staged_storev2_points_of_interest_validation_sample.csv' = @('poi_id','artifact_id','inode_num','parent_inode_num')
  'aff4_apfs_staged_storev2_high_priority_validation_queue.csv' = @('poi_id','artifact_id','inode_num','parent_inode_num')
  'aff4_apfs_staged_storev2_high_priority_validation_evidence_packet.csv' = @('poi_id','artifact_id','inode_num','parent_inode_num')
  'aff4_apfs_staged_storev2_unresolved_after_resolution_sample.csv' = @('unresolved_id','artifact_id','inode_num','parent_inode_num','store_id')
  'aff4_apfs_staged_storev2_raw_key_values_sample.csv' = @('raw_kv_id','inode_num','store_id','parent_inode_num')
  'aff4_apfs_staged_storev2_raw_date_candidates_sample.csv' = @('raw_date_id','inode_num','store_id')
  'aff4_apfs_staged_storev2_field_inventory_sample.csv' = @('field_inventory_id')
  'aff4_apfs_staged_storev2_artifacts_sample.csv' = @('artifact_id','inode_num','parent_inode_num')
  'aff4_apfs_logical_directory_walk.csv' = @('inode_num','parent_inode_num','filesystem_object_id','parent_filesystem_object_id')
  'aff4_apfs_spotlight_file_copy_out.csv' = @('inode_num','parent_inode_num','filesystem_object_id','parent_filesystem_object_id')
  'aff4_apfs_unresolved_spotlight_object_resolution_probe.csv' = @('inode_num','parent_inode_num','filesystem_object_id','parent_filesystem_object_id')
  'active_file_comparison_readiness.csv' = @('spotlight_artifact_count','image_inventory_rows')
}

$failures = New-Object System.Collections.Generic.List[string]
$checkedValues = 0L
foreach ($name in $targets.Keys) {
  $file = Get-ChildItem -LiteralPath $WorkRoot -Recurse -File -Filter $name -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($null -eq $file) { continue }
  $columns = $targets[$name]
  $rowIndex = 0L
  Import-Csv -LiteralPath $file.FullName | ForEach-Object {
    $rowIndex++
    foreach ($col in $columns) {
      $prop = $_.PSObject.Properties[$col]
      if ($null -eq $prop) { continue }
      $value = [string]$prop.Value
      if ([string]::IsNullOrWhiteSpace($value)) { continue }
      $checkedValues++
      if ($value.Trim() -match $scientificNotation) {
        if ($failures.Count -lt $MaxSamples) {
          [void]$failures.Add(("{0}: row={1} column={2} value={3}" -f $name, $rowIndex, $col, $value))
        }
      }
    }
  }
}

if ($failures.Count -gt 0) {
  Write-Error ("Scientific-notation forensic identifier values found in thin upload. Samples:`n" + ($failures -join "`n"))
  exit 2
}
Write-Host "Thin identifier CSV precision audit passed. Checked values: $checkedValues"
try { Remove-Item -LiteralPath $WorkRoot -Recurse -Force -ErrorAction SilentlyContinue } catch { }
exit 0
