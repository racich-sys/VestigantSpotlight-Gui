
## V1.6.38 - CSV default, source-profile filtering, unresolved-label path guard

- GUI processing now defaults to `Exclude CSV exports` checked. SQLite case output remains the default review artifact unless CSV exports are explicitly enabled.
- Non-iOS ZIP profiles now record that iOS FFS/app-database parser stages were skipped.
- macOS-profile exports now skip `ios_*` CSV export calls rather than writing large groups of zero-row iOS CSVs.
- Unresolved Store-V2 review labels are no longer accepted as valid filename/path components for parent-inode path reconstruction.
- Added `docs/V1_6_38_CSV_DEFAULT_AND_SOURCE_PROFILE_FILTERING.md`.


## V1.6.37.1 macOS unresolved Store-V2 object labels

- Added explicit unresolved object labels for macOS Store-V2 records that still lack structured names after dictionary/path-probe enrichment.
- Labels are forensic review handles, not asserted filenames.
- Added parser metric `unresolved_identifier_label_artifacts`.

# V1.6.37.1

MacOS Store-V2 path enrichment hotfix. V1.6.37.1 promotes `__native_probe_file_path_candidate_%` values to artifact display/path fields when raw record headers lack usable names or paths.

# 1.6.38

V1.6.37.1 follows the V1.6.32 macOS zipped Spotlight thin review. The source parsed successfully, but enrichment spent several minutes in a no-op parent-inode path apply step. This version skips that UPDATE when `new_reconstructed_paths=0`.

# 1.6.38

Release-preflight hardening. The build wrapper no longer treats release-readiness documentation/static marker assertions as fatal compile gates. Fatal preflight remains in place for PowerShell wrapper compatibility and MSVC raw-string literal risk.

# Vestigant Spotlight Investigator V1.6.37.1

## Compile hotfix

- Fixes MSVC compile error in `src/parsers/aff4_probe_worker.cpp` by replacing `appendProbeNote(...)` with the in-file helper `aff4ApfsAppendProbeNote(...)` in OMAP vertical-cycle handling.
- Hardens `Build-V1_6_38.ps1` so it fails before version probing if the CLI executable was not produced or if the build log contains compiler/linker errors.
- Carries forward V1.6.29.3 packaging and readiness fixes.

# Vestigant Spotlight Investigator V1.6.37.1

## Hotfix

- Adds the required `docs/V1_6_29_CODE_REVIEW_VALIDATION_HARDENING.md` document that was missing from the V1.6.29.2 ZIP.
- Carries forward V1.6.29.2 stable release-readiness checks and all V1.6.29 code-review hardening.

# Vestigant Spotlight Investigator V1.6.37.1

## Hotfix

- Replaces the brittle V1.6.29.1 release-readiness roadmap prose check with stable source/docs markers.
- Keeps version, wrapper, raw-string, and code-review hardening checks strict.
- Carries forward V1.6.29.1 and V1.6.29 fixes.

# Vestigant Spotlight Investigator V1.6.37.1

## Hotfix

- Fixes the V1.6.29 release-readiness script stale escaped VERSION regex that still checked `^1\.6\.28\s*$`.
- Carries forward the corrected fatal preflight behavior: wrapper compatibility, MSVC raw-string risk, and release-readiness failures stop before MSVC starts.
- Carries forward V1.6.29 code-review hardening.

# Vestigant Spotlight Investigator V1.6.37.1

## Validation and stability hardening

- Validated V1.6.37.1 build/thin evidence before source changes. V1.6.37.1 active-comparison and CoreDuet validation samples passed.
- Implemented user-supplied code-review hardening for APFS OMAP cycle detection, AFF4 LZ4 overflow checks, NSKeyedArchiver/bplist bounds and expansion caps, Unicode bplist fallback ripping, generic iOS app database pagination, GUI schema-open churn reduction, and folder-picker path warnings.
- Split oversized SQL raw strings and corrected V1.6.37.1 build/readiness version pinning.
- Preserved Missing-from-FFS and CoreDuet interpretation guardrails.

See `docs/V1_6_38_CODE_REVIEW_VALIDATION_HARDENING.md` for the detailed issue-by-issue audit.

# Consolidated Version History

## V1.6.37.1
V1.6.37.1 follows V1.6.27 thin validation. The thin showed active comparison and CoreDuet validation exports working, but the lookup-source validation check remained REVIEW. V1.6.37.1 updates the later GUI/review Missing-from-FFS view definition and the materialized candidate reason builder to preserve a nonblank lookup source.

# Consolidated Version History

## V1.6.37.1
V1.6.37.1 validates the V1.6.26 active-comparison work by adding reconciliation checks and bounded validation samples for active comparison and CoreDuet interactionC. The V1.6.26 thin showed active comparison completed with 7,766 materialized reference candidates; V1.6.37.1 adds validation surfaces to confirm those rows reconcile to summary and reference views and preserve lead-only wording.

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

## V1.6.37.1

V1.6.37.1 validates V1.6.29.4 and applies additional source-grounded hardening: checked-artifact mutex coverage, stale review-query detach instead of UI-thread join, export worker handle accumulation reduction, length-aware SQLite CSV export, and APFS NXSB block-size rejection before allocation/use.

## V1.6.37.1

Adds profile-aware native persistence mode selection after a macOS `.Spotlight-V100` folder run showed an iOS CoreSpotlight persistence status. Also mirrors native parser progress to root progress files for long parse visibility.
