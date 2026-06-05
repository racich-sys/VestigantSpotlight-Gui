param(
    [Parameter(Mandatory=$true)][string]$CaseRoot,
    [Parameter(Mandatory=$true)][string]$CompareRoot,
    [string]$OutputPrefix = "aff4_apfs_remaining_mismatch",
    [string[]]$StatusFilter = @("RELATIVE_PATH_SIZE_MISMATCH", "UUID_STRIPPED_RELATIVE_PATH_SIZE_MISMATCH")
)

$ErrorActionPreference = "Stop"

function Require-File { param([string]$PathValue) if (!(Test-Path -LiteralPath $PathValue -PathType Leaf)) { throw "Required file not found: $PathValue" } return $PathValue }
function Escape-Csv { param([AllowNull()][object]$Value) $s = if ($null -eq $Value) { "" } else { [string]$Value }; if ($s.Contains('"') -or $s.Contains(',') -or $s.Contains("`n") -or $s.Contains("`r")) { return '"' + $s.Replace('"','""') + '"' }; return $s }
function Normalize-RelPath { param([AllowNull()][string]$PathValue) if ([string]::IsNullOrWhiteSpace($PathValue)) { return "" }; $p = $PathValue.Replace([char]92,[char]47).TrimStart('/'); $marker='ExtractedSpotlight/StagedStoreV2/'; $idx=$p.IndexOf($marker,[System.StringComparison]::OrdinalIgnoreCase); if ($idx -ge 0) { $p=$p.Substring($idx+$marker.Length) }; return $p.Trim('/').ToLowerInvariant() }
function StoreKeyFromParts { param([string]$GroupName,[string]$StoreRel) $g=if($GroupName){$GroupName.Replace([char]92,[char]47).Trim('/')}else{""}; $r=if($StoreRel){$StoreRel.Replace([char]92,[char]47).Trim('/')}else{""}; if($g -and $r){return Normalize-RelPath ($g+'/'+$r)}; if($r){return Normalize-RelPath $r}; return Normalize-RelPath $g }
function Add-List { param([hashtable]$Map,[string]$Key,$Value) if([string]::IsNullOrWhiteSpace($Key)){return}; if(!$Map.ContainsKey($Key)){ $Map[$Key] = New-Object System.Collections.ArrayList }; [void]$Map[$Key].Add($Value) }
function Join-Values { param($Rows,[string]$Column) if($null -eq $Rows){return ""}; return (($Rows | ForEach-Object { $_.$Column } | Where-Object { ![string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique) -join ';') }
function Parse-DecmpfsAlgorithm { param([string]$Hex) if([string]::IsNullOrWhiteSpace($Hex)){return ""}; $bytes=New-Object System.Collections.Generic.List[int]; foreach($tok in ($Hex -split '[^0-9A-Fa-f]+')){ if($tok.Length -eq 2){ try{ $bytes.Add([Convert]::ToInt32($tok,16)) | Out-Null }catch{} } }; if($bytes.Count -lt 8){return ""}; if($bytes[0] -eq 0x66 -and $bytes[1] -eq 0x70 -and $bytes[2] -eq 0x6d -and $bytes[3] -eq 0x63){ $alg=[int]($bytes[4] -bor ($bytes[5] -shl 8) -bor ($bytes[6] -shl 16) -bor ($bytes[7] -shl 24)); switch($alg){3{"3_ZLIB_ATTR"}4{"4_ZLIB_RSRC"}7{"7_LZVN_ATTR"}8{"8_LZVN_RSRC"}9{"9_PLAIN_ATTR"}10{"10_PLAIN_RSRC"}11{"11_LZFSE_ATTR"}12{"12_LZFSE_RSRC"}13{"13_LZBITMAP_ATTR"}14{"14_LZBITMAP_RSRC"}default{"${alg}_UNKNOWN"}} } else { return "" } }

$FileComparePath = Require-File (Join-Path $CompareRoot "aff4_apfs_external_spotlight_file_compare.csv")
$StagePath = Require-File (Join-Path $CaseRoot "aff4_apfs_extracted_storev2_stage_files.csv")
$CopyOutPath = Require-File (Join-Path $CaseRoot "aff4_apfs_spotlight_file_copy_out.csv")
$XattrPath = Join-Path $CaseRoot "aff4_apfs_spotlight_xattr_probe.csv"
$OutputCsv = Join-Path $CompareRoot ($OutputPrefix + "_diagnostics.csv")
$OutputJson = Join-Path $CompareRoot ($OutputPrefix + "_diagnostics_summary.json")
$OutputMd = Join-Path $CompareRoot ($OutputPrefix + "_diagnostics.md")

$stageByRel=@{}; Import-Csv -LiteralPath $StagePath | ForEach-Object { Add-List $stageByRel (Normalize-RelPath $_.staged_relative_path) $_; Add-List $stageByRel (StoreKeyFromParts $_.storev2_group_name $_.storev2_relative_path) $_ }
$copyByRel=@{}; $copyByChild=@{}; Import-Csv -LiteralPath $CopyOutPath | ForEach-Object { Add-List $copyByRel (StoreKeyFromParts $_.storev2_group_name $_.storev2_relative_path) $_; Add-List $copyByChild ([string]$_.target_child_file_id) $_ }
$xattrByFile=@{}; if (Test-Path -LiteralPath $XattrPath -PathType Leaf) { Import-Csv -LiteralPath $XattrPath | ForEach-Object { Add-List $xattrByFile ([string]$_.file_object_id) $_ } }

$headers=@("status","external_relative_path","external_size","vestigant_size","external_sha256","vestigant_sha256","stage_match_count","staged_child_file_id","staged_parent_object_id","staged_source_copy_status","staged_source_validation_status","stage_status","stage_notes","copyout_candidate_count","copyout_exact_hash_candidate_count","copyout_exact_size_candidate_count","copyout_statuses","copyout_validation_statuses","copyout_logical_size_sources","copyout_child_file_ids","copyout_resourcefork_or_compressed_candidate_count","xattr_names","decmpfs_algorithm_labels","resourcefork_stream_ids","resourcefork_stream_sizes","resourcefork_storage","diagnostic_classification")
$writer = New-Object System.IO.StreamWriter($OutputCsv,$false,(New-Object System.Text.UTF8Encoding($false)))
$rowCount=0; $statusCounts=@{}; $classCounts=@{}
try {
    $writer.WriteLine(($headers -join ','))
    $allRows = Import-Csv -LiteralPath $FileComparePath
    foreach($cmp in $allRows){
        if(!($StatusFilter -contains [string]$cmp.status)){ continue }
        $rel=Normalize-RelPath $cmp.external_relative_path
        $stageRows=@(); if($stageByRel.ContainsKey($rel)){ $stageRows=@($stageByRel[$rel]) }
        $stage=if($stageRows.Count -gt 0){$stageRows[0]}else{$null}
        $child=if($stage){[string]$stage.child_file_id}else{""}
        $copyRows=@(); if($copyByRel.ContainsKey($rel)){ $copyRows=@($copyByRel[$rel]) } elseif($child -and $copyByChild.ContainsKey($child)){ $copyRows=@($copyByChild[$child]) }
        $xattrs=@(); if($child -and $xattrByFile.ContainsKey($child)){ $xattrs=@($xattrByFile[$child]) }
        $exactHash=@($copyRows | Where-Object { $_.output_sha256 -and $_.output_sha256 -eq $cmp.external_sha256 }).Count
        $exactSize=@($copyRows | Where-Object { [string]$_.output_size_bytes -eq [string]$cmp.external_size }).Count
        $rsrcOrCompressed=@($copyRows | Where-Object { $_.copy_status -match 'RSRC|COMPRESSED|PARTIAL|DECOMPFS' -or $_.validation_status -match 'RSRC|COMPRESSED|PARTIAL|DECOMPFS' }).Count
        $algLabels=(($xattrs | Where-Object { $_.xattr_name -eq 'com.apple.decmpfs' } | ForEach-Object { Parse-DecmpfsAlgorithm $_.xdata_preview_hex } | Where-Object { $_ } | Sort-Object -Unique) -join ';')
        $xNames=Join-Values $xattrs "xattr_name"
        $classification="NO_EXACT_COPYOUT_CANDIDATE"
        if($exactHash -gt 0){$classification="EXACT_HASH_COPYOUT_AVAILABLE_STAGING_SELECTION"}
        elseif($exactSize -gt 0){$classification="EXACT_SIZE_COPYOUT_AVAILABLE_REVIEW_HASH"}
        elseif($algLabels -match '4_ZLIB_RSRC'){$classification="DECOMPFS_ZLIB_RSRC_RECONSTRUCTION_TARGET"}
        elseif($algLabels -match '8_LZVN_RSRC|12_LZFSE_RSRC|14_LZBITMAP_RSRC'){$classification="DECOMPFS_UNSUPPORTED_CODEC_RESOURCE_FORK_TARGET"}
        elseif($xNames -match 'com.apple.decmpfs' -and $xNames -match 'com.apple.ResourceFork'){$classification="DECOMPFS_RESOURCE_FORK_RECONSTRUCTION_TARGET"}
        elseif($rsrcOrCompressed -gt 0){$classification="PARTIAL_COMPRESSED_OR_RESOURCE_FORK_TARGET"}
        elseif($copyRows.Count -eq 0){$classification="NO_COPYOUT_ROW_FOR_RELATIVE_PATH"}
        elseif((Join-Values $copyRows "validation_status") -match 'TRIMMED_TO_INODE_LOGICAL_SIZE'){$classification="DATA_FORK_SIZE_DISAGREES_WITH_EXTERNAL"}
        $statusCounts[[string]$cmp.status] = 1 + [int]$statusCounts[[string]$cmp.status]
        $classCounts[$classification] = 1 + [int]$classCounts[$classification]
        $rowCount++
        $values=@($cmp.status,$cmp.external_relative_path,$cmp.external_size,$cmp.vestigant_size,$cmp.external_sha256,$cmp.vestigant_sha256,$stageRows.Count,$child,$(if($stage){$stage.parent_object_id}else{""}),$(if($stage){$stage.source_copy_status}else{""}),$(if($stage){$stage.source_validation_status}else{""}),$(if($stage){$stage.stage_status}else{""}),$(if($stage){$stage.notes}else{""}),$copyRows.Count,$exactHash,$exactSize,(Join-Values $copyRows "copy_status"),(Join-Values $copyRows "validation_status"),(Join-Values $copyRows "logical_size_source"),(Join-Values $copyRows "target_child_file_id"),$rsrcOrCompressed,$xNames,$algLabels,(Join-Values ($xattrs | Where-Object { $_.xattr_name -eq 'com.apple.ResourceFork' }) "xdata_stream_id"),(Join-Values ($xattrs | Where-Object { $_.xattr_name -eq 'com.apple.ResourceFork' }) "xdata_stream_size"),(Join-Values ($xattrs | Where-Object { $_.xattr_name -eq 'com.apple.ResourceFork' }) "xattr_storage"),$classification)
        $writer.WriteLine((($values | ForEach-Object { Escape-Csv $_ }) -join ','))
    }
} finally { $writer.Close() }

$summary=[ordered]@{ generated_utc=(Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'); case_root=$CaseRoot; compare_root=$CompareRoot; status_filter=$StatusFilter; diagnostic_row_count=$rowCount; xattr_probe_available=(Test-Path -LiteralPath $XattrPath -PathType Leaf); by_status=@(); by_classification=@() }
foreach($k in ($statusCounts.Keys | Sort-Object)){ $summary.by_status += [ordered]@{ status=$k; count=$statusCounts[$k] } }
foreach($k in ($classCounts.Keys | Sort-Object)){ $summary.by_classification += [ordered]@{ diagnostic_classification=$k; count=$classCounts[$k] } }
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
@("# APFS Remaining Mismatch Diagnostics","","Generated UTC: $($summary.generated_utc)","Case root: $CaseRoot","Compare root: $CompareRoot","Rows: $rowCount","","## Classification counts",($summary.by_classification | ForEach-Object { "- $($_.diagnostic_classification): $($_.count)" })) | Set-Content -LiteralPath $OutputMd -Encoding UTF8
Write-Host "APFS remaining mismatch diagnostics written: $OutputCsv"
Write-Host "APFS remaining mismatch summary written: $OutputJson"
