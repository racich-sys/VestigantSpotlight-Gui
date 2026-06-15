# AI Context - Vestigant Spotlight V1.6.28

Current baseline: V1.6.28.

Latest validated prior evidence: V1.6.27 Windows build succeeded and V1.6.27 iOS thin completed. Active comparison produced 7,766 materialized candidates and CoreDuet interactionC validation checks all passed. The remaining active-comparison validation gap was `reference_view_lookup_source_populated=REVIEW` because exported reference rows still had blank lookup-source values through the GUI/review view path. V1.6.28 fixes that path.

Next validation: build V1.6.28 and run the V1.6.28 iOS thin. Confirm `active_file_comparison_validation_checks_sample.csv` shows `reference_view_lookup_source_populated=PASS` and candidate reasons no longer include `lookup=` with an empty value.

# AI Context - Vestigant Spotlight V1.6.28

Current baseline: V1.6.28.

Latest validated prior evidence: V1.6.26 Windows build succeeded and V1.6.26 iOS thin completed. Active iOS FFS exact-path plus Spotlight recovered-reference comparison completed and materialized 7,766 Missing-from-FFS reference candidates. V1.6.26 introduced validation checks; one reference lookup-source check was REVIEW in the thin, so V1.6.28 hardens validation surfaces and sample enforcement.

Next required validation:
1. Build V1.6.28 on Windows/MSVC.
2. Run the V1.6.28 iOS CoreSpotlight thin.
3. Upload `V1_6_28_build.log` and `Upload_Thin_iOS_CoreSpotlight_V1_6_28.zip`.
4. Review the new active-comparison and CoreDuet validation samples.

Guardrail status: Missing-from-FFS candidates remain investigative leads only, not deletion proof. No blind guardrail removal.

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
