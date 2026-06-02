# V0_9_37 Review Notes

## Inputs reviewed

- V0_9_34 Windows/MSVC build log.
- V0_9_34 iOS reused-cache thin upload.
- V0_9_35 documentation/history repair package, which was not separately run.

## Result of V0_9_34 thin review

V0_9_34 built successfully, but the reused-cache run ended in `failed_exception` during normal export because `ios_spotlight_missing_from_ffs_summary.csv` ordered by `investigative_priority_sort` while the active `vw_ios_spotlight_missing_from_ffs_summary` view in the run did not expose that column.  The run had already completed native parsing and enrichment before the export failure.

Observed V0_9_34 run metrics:

- raw_records: 344,445
- raw_key_values: 982,230
- raw_date_candidates: 336,037
- slim FFS path lookup rows imported from cache: 1,592,440
- full FFS inventory materialized: false
- app DB records materialized: false

## Implemented in V0_9_37

- Carried forward the V0_9_35 consolidated documentation/history repair.
- Fixed Missing From FFS export ordering so normal export no longer depends on a potentially stale summary-view sort column.
- Synchronized the GUI fallback Missing From FFS candidate/summary SQL so it exposes `missing_candidate_category`, `investigative_priority`, `investigative_priority_sort`, and `investigative_reason`.
- Added/kept high-value Missing From FFS fallback views in the GUI schema path.
- Updated version metadata and scripts to V0_9_37.

## Validation focus

V0_9_37 should be validated by Windows/MSVC build and the standard iOS reuse-cache run. The primary pass/fail item is whether the run completes past `ios_spotlight_missing_from_ffs_summary.csv` export and produces the thin upload.
