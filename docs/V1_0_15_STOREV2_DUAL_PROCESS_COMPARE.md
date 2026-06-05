# V1.0.15 Store-V2 Candidate Dual-Process Compare

## Purpose

V1.0.15 adds a deterministic support output that compares two AFF4/APFS Store-V2 processing stages:

1. the raw APFS copy-out candidate set (`aff4_apfs_spotlight_file_copy_out.csv`), and
2. the normalized investigator-facing Store-V2 staging set (`ExtractedSpotlight/StagedStoreV2` and `aff4_apfs_extracted_storev2_stage_files.csv`).

This is not yet a replacement for the live extraction path. It is a parity and regression guard designed to catch duplicate APFS candidates, candidate scoring drift, synthetic-zero provenance, decmpfs/resource-fork reconstruction rows, and cases where the staged row differs from the best scored copy-out row for the same Store-V2 component.

## New outputs

- `aff4_apfs_storev2_candidate_dual_process_compare.csv`
- `aff4_apfs_storev2_candidate_dual_process_compare_summary.json`
- `AFF4_APFS_STOREV2_CANDIDATE_DUAL_PROCESS_COMPARE.md`

## Key statuses

- `STAGED_SELECTED_BEST_COPYOUT_CANDIDATE`: normalized staging chose the same row as the best-scored copy-out candidate.
- `STAGED_SELECTED_BEST_COPYOUT_SEQUENCE`: normalized staging chose a row with the same source sequence as the best candidate.
- `STAGED_ROW_DIFFERS_FROM_BEST_COPYOUT_CANDIDATE`: staging selected a different row than the highest-scored candidate for the same Store-V2 key.
- `BEST_COPYOUT_CANDIDATE_NOT_STAGED`: a copy-out candidate existed but did not enter normalized staging.
- `NO_COPIED_OR_STAGED_ROW`: no usable row existed for that key.

## Why this is useful

V1.0.14 staged 8,986 files and parsed 25,000 raw Store-V2 records, but external comparison still showed many size/path mismatches. The new compare output gives a deterministic way to review whether mismatches are caused by copy-out, staging selection, duplicate candidate collisions, or external reference differences.
