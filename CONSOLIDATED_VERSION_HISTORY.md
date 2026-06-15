# Consolidated Version History

## V1.6.28
V1.6.28 follows V1.6.27 thin validation. The thin showed active comparison and CoreDuet validation exports working, but the lookup-source validation check remained REVIEW. V1.6.28 updates the later GUI/review Missing-from-FFS view definition and the materialized candidate reason builder to preserve a nonblank lookup source.

# Consolidated Version History

## V1.6.28
V1.6.28 validates the V1.6.26 active-comparison work by adding reconciliation checks and bounded validation samples for active comparison and CoreDuet interactionC. The V1.6.26 thin showed active comparison completed with 7,766 materialized reference candidates; V1.6.28 adds validation surfaces to confirm those rows reconcile to summary and reference views and preserve lead-only wording.

# V1.6.28 Release Notes

## Purpose

V1.6.25 validated the active filesystem comparison pipeline and materialized 7,766 Missing-from-FFS reference candidates. V1.6.28 adds additional validation surfaces so the next thin run can prove count consistency, lead-only language, lookup-source preservation, and candidate categorization without manually joining CSVs.

## Triggering evidence from V1.6.25 thin

- Build completed cleanly and reported `Vestigant Spotlight v1.6.25`.
- Thin run ended with `complete_success`.
- `active_file_comparison_runs_sample.csv` showed `run_status=COMPLETED_IOS_FFS_EXACT_PATH_AND_REFERENCE_LOOKUP`, `image_file_count=2245783`, `missing_candidate_count=7766`, and `not_checked_count=344445`.
- `orphaned_deleted_candidates_sample.csv` showed `MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE` rows with lead-only language.
- The Missing-from-FFS candidate sample exposed blank `ffs_lookup_source`, so V1.6.28 normalizes blank lookup-source values to `lookup_available_no_matching_path`.

## Changed in V1.6.28

- Adds `vw_active_file_comparison_validation_checks`.
- Adds `vw_active_file_comparison_candidate_summary`.
- Exports full CSVs for both validation views.
- Adds bounded upload samples:
  - `active_file_comparison_validation_checks_sample.csv`
  - `active_file_comparison_candidate_summary_sample.csv`
  - `orphaned_deleted_candidates_focus.csv`
- Updates active-comparison readiness next-action text for all `COMPLETED_IOS_FFS%` run statuses.
- Normalizes blank Missing-from-FFS lookup source values to `lookup_available_no_matching_path`.

## Guardrails retained

- Missing rows remain investigative leads only, not deletion proof.
- No fuzzy matching.
- AFF4/APFS image-backed inode/parent comparison remains pending.
