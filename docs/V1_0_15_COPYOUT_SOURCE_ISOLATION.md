# V1.0.15 Copy-Out Source Isolation

## Issue

V1.0.12 successfully copied APFS Store-V2 candidate files and staged 884 normalized Store-V2 component rows, but the external comparison still showed many `RELATIVE_PATH_SIZE_MISMATCH` rows. Review of the thin upload showed a provenance mismatch: `aff4_apfs_extracted_storev2_stage_files.csv` could report a large selected copy-out row while the actual normalized staged file at the same relative path was only 4096 bytes.

The root cause was raw APFS copy-out path collision. Multiple APFS candidate rows with the same component name were written into `ExtractedSpotlight/StagedStoreV2/Ungrouped/<component>`. Later duplicate rows could overwrite the file that an earlier, better-scored staging row referenced.

## Fix

V1.0.15 writes raw APFS copy-out rows to unique immutable per-target folders under:

`ExtractedSpotlight/ApfsCopyOutByTarget/seq_<target_sequence>_fid_<child_file_id>_parent_<parent_object_id>_<group>/...`

The normalized Store-V2 stage still writes investigator-facing files under:

`ExtractedSpotlight/StagedStoreV2/<group>/...`

This keeps copy-out provenance separate from normalized Store-V2 staging and prevents same-name duplicate rows from overwriting the source selected by the staging scorer.

## Expected benchmark

After V1.0.15, `aff4_apfs_extracted_storev2_stage_files.csv`, the actual files under `ExtractedSpotlight/StagedStoreV2`, and `aff4_apfs_external_spotlight_vestigant_manifest.csv` should agree on staged file sizes/hashes for the same relative path. The number of `RELATIVE_PATH_SIZE_MISMATCH` rows should fall, especially for BADA95B6 Store-V2 components such as `0.indexArrays`.
