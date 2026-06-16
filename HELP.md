# Vestigant Spotlight Investigator Help - V1.6.37.1

V1.6.37.1 fixes lookup-source reporting for Missing-from-FFS validation. Review `active_file_comparison_validation_checks_sample.csv` after the thin run. The expected result is that `reference_view_lookup_source_populated` changes from REVIEW to PASS. Missing rows remain lead-only and not deletion proof.

# Vestigant Spotlight Investigator Help - V1.6.37.1

V1.6.37.1 is a validation-hardening release after V1.6.26 active-comparison validation. Use the new validation samples before retiring additional guardrails:

- exports/upload_samples/active_file_comparison_validation_checks_sample.csv
- exports/upload_samples/active_file_comparison_candidate_summary_sample.csv
- exports/upload_samples/orphaned_deleted_candidates_focus.csv
- exports/upload_samples/ios_coreduet_interactionc_validation_checks_sample.csv

All Missing-from-FFS candidate rows are investigative leads only. They do not prove deletion without corroborating filesystem, acquisition, application, or timeline evidence.

# V1.6.37.1 Release Notes

## Purpose

V1.6.25 validated the active filesystem comparison pipeline and materialized 7,766 Missing-from-FFS reference candidates. V1.6.37.1 adds additional validation surfaces so the next thin run can prove count consistency, lead-only language, lookup-source preservation, and candidate categorization without manually joining CSVs.

## Triggering evidence from V1.6.25 thin

- Build completed cleanly and reported `Vestigant Spotlight v1.6.25`.
- Thin run ended with `complete_success`.
- `active_file_comparison_runs_sample.csv` showed `run_status=COMPLETED_IOS_FFS_EXACT_PATH_AND_REFERENCE_LOOKUP`, `image_file_count=2245783`, `missing_candidate_count=7766`, and `not_checked_count=344445`.
- `orphaned_deleted_candidates_sample.csv` showed `MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE` rows with lead-only language.
- The Missing-from-FFS candidate sample exposed blank `ffs_lookup_source`, so V1.6.37.1 normalizes blank lookup-source values to `lookup_available_no_matching_path`.

## Changed in V1.6.37.1

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
