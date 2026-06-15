# Vestigant Spotlight Investigator V1.6.29.4

## Compile hotfix

- Fixes MSVC compile error in `src/parsers/aff4_probe_worker.cpp` by replacing `appendProbeNote(...)` with the in-file helper `aff4ApfsAppendProbeNote(...)` in OMAP vertical-cycle handling.
- Hardens `Build-V1_6_29_4.ps1` so it fails before version probing if the CLI executable was not produced or if the build log contains compiler/linker errors.
- Carries forward V1.6.29.3 packaging and readiness fixes.

# Vestigant Spotlight Investigator V1.6.29.4

## Hotfix

- Adds the required `docs/V1_6_29_CODE_REVIEW_VALIDATION_HARDENING.md` document that was missing from the V1.6.29.2 ZIP.
- Carries forward V1.6.29.2 stable release-readiness checks and all V1.6.29 code-review hardening.

# Vestigant Spotlight Investigator V1.6.29.4

## Hotfix

- Replaces the brittle V1.6.29.1 release-readiness roadmap prose check with stable source/docs markers.
- Keeps version, wrapper, raw-string, and code-review hardening checks strict.
- Carries forward V1.6.29.1 and V1.6.29 fixes.

# Vestigant Spotlight Investigator V1.6.29.4

## Hotfix

- Fixes the V1.6.29 release-readiness script stale escaped VERSION regex that still checked `^1\.6\.28\s*$`.
- Carries forward the corrected fatal preflight behavior: wrapper compatibility, MSVC raw-string risk, and release-readiness failures stop before MSVC starts.
- Carries forward V1.6.29 code-review hardening.

# Vestigant Spotlight Investigator V1.6.29.4

## Validation and stability hardening

- Validated V1.6.29.4 build/thin evidence before source changes. V1.6.29.4 active-comparison and CoreDuet validation samples passed.
- Implemented user-supplied code-review hardening for APFS OMAP cycle detection, AFF4 LZ4 overflow checks, NSKeyedArchiver/bplist bounds and expansion caps, Unicode bplist fallback ripping, generic iOS app database pagination, GUI schema-open churn reduction, and folder-picker path warnings.
- Split oversized SQL raw strings and corrected V1.6.29.4 build/readiness version pinning.
- Preserved Missing-from-FFS and CoreDuet interpretation guardrails.

See `docs/V1_6_29_4_CODE_REVIEW_VALIDATION_HARDENING.md` for the detailed issue-by-issue audit.

# Consolidated Version History

## V1.6.29.4
V1.6.29.4 follows V1.6.27 thin validation. The thin showed active comparison and CoreDuet validation exports working, but the lookup-source validation check remained REVIEW. V1.6.29.4 updates the later GUI/review Missing-from-FFS view definition and the materialized candidate reason builder to preserve a nonblank lookup source.

# Consolidated Version History

## V1.6.29.4
V1.6.29.4 validates the V1.6.26 active-comparison work by adding reconciliation checks and bounded validation samples for active comparison and CoreDuet interactionC. The V1.6.26 thin showed active comparison completed with 7,766 materialized reference candidates; V1.6.29.4 adds validation surfaces to confirm those rows reconcile to summary and reference views and preserve lead-only wording.

# V1.6.29.4 Release Notes

## Purpose

V1.6.25 validated the active filesystem comparison pipeline and materialized 7,766 Missing-from-FFS reference candidates. V1.6.29.4 adds additional validation surfaces so the next thin run can prove count consistency, lead-only language, lookup-source preservation, and candidate categorization without manually joining CSVs.

## Triggering evidence from V1.6.25 thin

- Build completed cleanly and reported `Vestigant Spotlight v1.6.25`.
- Thin run ended with `complete_success`.
- `active_file_comparison_runs_sample.csv` showed `run_status=COMPLETED_IOS_FFS_EXACT_PATH_AND_REFERENCE_LOOKUP`, `image_file_count=2245783`, `missing_candidate_count=7766`, and `not_checked_count=344445`.
- `orphaned_deleted_candidates_sample.csv` showed `MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE` rows with lead-only language.
- The Missing-from-FFS candidate sample exposed blank `ffs_lookup_source`, so V1.6.29.4 normalizes blank lookup-source values to `lookup_available_no_matching_path`.

## Changed in V1.6.29.4

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
