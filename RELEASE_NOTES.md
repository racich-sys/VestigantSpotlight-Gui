# Vestigant Spotlight Investigator V1.6.28

## Validation fix
- V1.6.27 thin completed and produced the new active-comparison/CoreDuet validation samples. Eight active-comparison checks passed and one remained REVIEW because the GUI-layer Missing-from-FFS view re-created `vw_ios_spotlight_missing_from_ffs_candidates` with blank `ffs_lookup_source`.
- V1.6.28 updates the GUI/review-layer Missing-from-FFS view to carry lookup source from full iOS FFS inventory or slim path lookup into exported reference rows.
- V1.6.28 also hardens orphan/missing candidate materialization so `orphan_reason` uses a nonblank lookup-source fallback.
- Missing-from-FFS rows remain investigative leads only, not deletion proof.

# Vestigant Spotlight Investigator V1.6.28

## Validation hardening
- Verified from V1.6.26 thin evidence that active filesystem comparison completed and produced 7,766 materialized Missing-from-FFS reference candidates, but one validation check remained REVIEW because reference-view lookup-source reporting was not populated in the exported validation surface.
- Added additional active filesystem comparison validation checks for candidate-summary reconciliation, unsafe deletion-language absence, materialized-reference provenance, and high-value message-attachment candidate visibility.
- Added CoreDuet interactionC validation checks to reconcile parsed rows to canonical ZINTERACTIONS rows, confirm join tables are not promoted as standalone events, confirm Phone: identity promotion remains suppressed, and confirm contextual guardrail notes are present.
- Added bounded upload enforcement for the new active-comparison and CoreDuet validation samples.
- Missing-from-FFS rows remain investigative leads only, not deletion proof.

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
