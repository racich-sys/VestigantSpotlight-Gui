# V1.6.35

- Fixed macOS Store-V2 native dictionary loading for stores that use external `dbStr-*` maps rather than in-header dictionary block pointers.
- The uploaded macOS `store.db`/`.store.db` showed zero dictionary block pointers while adjacent `dbStr` files contained property/category dictionaries; V1.6.35 now loads those maps for macOS Store-V2.

# V1.6.35

- Fixed macOS Store-V2 GUI review rows showing `------NONAME------` despite native path probe candidates being present in parsed key/value rows.
- Added native path probe promotion before timeline materialization so GUI and exports can use recovered macOS path evidence.

# 1.6.35

- Added parent-inode path-apply skip when no new reconstructed path candidates exist.
- Added explicit run-status markers for skipped no-op path apply.
- Kept V1.6.32 build-preflight hardening and V1.6.31 macOS Store-V2 persistence behavior.

# 1.6.35

- Hardened build wrapper preflight behavior so brittle release-readiness documentation/version-marker checks cannot prevent MSVC compilation from starting.
- Build wrapper now reads expected version from `VERSION` and uses that value for post-build CLI version verification.
- Release-readiness remains available as an advisory validation script.

# Vestigant Spotlight Investigator V1.6.35

## Compile hotfix

- Fixes MSVC compile error in `src/parsers/aff4_probe_worker.cpp` by replacing `appendProbeNote(...)` with the in-file helper `aff4ApfsAppendProbeNote(...)` in OMAP vertical-cycle handling.
- Hardens `Build-V1_6_35.ps1` so it fails before version probing if the CLI executable was not produced or if the build log contains compiler/linker errors.
- Carries forward V1.6.29.3 packaging and readiness fixes.

# Vestigant Spotlight Investigator V1.6.35

## Hotfix

- Adds the required `docs/V1_6_29_CODE_REVIEW_VALIDATION_HARDENING.md` document that was missing from the V1.6.29.2 ZIP.
- Carries forward V1.6.29.2 stable release-readiness checks and all V1.6.29 code-review hardening.

# Vestigant Spotlight Investigator V1.6.35

## Hotfix

- Replaces the brittle V1.6.29.1 release-readiness roadmap prose check with stable source/docs markers.
- Keeps version, wrapper, raw-string, and code-review hardening checks strict.
- Carries forward V1.6.29.1 and V1.6.29 fixes.

# Vestigant Spotlight Investigator V1.6.35

## Hotfix

- Fixes the V1.6.29 release-readiness script stale escaped VERSION regex that still checked `^1\.6\.28\s*$`.
- Carries forward the corrected fatal preflight behavior: wrapper compatibility, MSVC raw-string risk, and release-readiness failures stop before MSVC starts.
- Carries forward V1.6.29 code-review hardening.

# Vestigant Spotlight Investigator V1.6.35

## Validation and stability hardening

- Validated V1.6.35 build/thin evidence before source changes. V1.6.35 active-comparison and CoreDuet validation samples passed.
- Implemented user-supplied code-review hardening for APFS OMAP cycle detection, AFF4 LZ4 overflow checks, NSKeyedArchiver/bplist bounds and expansion caps, Unicode bplist fallback ripping, generic iOS app database pagination, GUI schema-open churn reduction, and folder-picker path warnings.
- Split oversized SQL raw strings and corrected V1.6.35 build/readiness version pinning.
- Preserved Missing-from-FFS and CoreDuet interpretation guardrails.

See `docs/V1_6_35_CODE_REVIEW_VALIDATION_HARDENING.md` for the detailed issue-by-issue audit.

# Version History

## V1.6.35
- Fixed GUI/review-layer Missing-from-FFS lookup-source propagation so active validation can pass `reference_view_lookup_source_populated`.
- Hardened orphan/missing candidate materialization with nonblank lookup-source fallback in candidate reasons.
- Preserved lead-only Missing-from-FFS interpretation.

# Version History

## V1.6.35
- Added active-comparison validation checks for candidate-summary reconciliation, unsafe deletion-language absence, reference-candidate materialization, and message-attachment candidate visibility.
- Added CoreDuet interactionC validation checks for canonical ZINTERACTIONS reconciliation, join-table suppression, phone-label suppression, and contextual guardrail notes.
- Added new full and bounded validation exports for CoreDuet interactionC checks.
- Added required bounded validation samples to the focused iOS thin wrapper.
- Preserved lead-only Missing-from-FFS guardrails.

# V1.6.35 Release Notes

## Purpose

V1.6.25 validated the active filesystem comparison pipeline and materialized 7,766 Missing-from-FFS reference candidates. V1.6.35 adds additional validation surfaces so the next thin run can prove count consistency, lead-only language, lookup-source preservation, and candidate categorization without manually joining CSVs.

## Triggering evidence from V1.6.25 thin

- Build completed cleanly and reported `Vestigant Spotlight v1.6.25`.
- Thin run ended with `complete_success`.
- `active_file_comparison_runs_sample.csv` showed `run_status=COMPLETED_IOS_FFS_EXACT_PATH_AND_REFERENCE_LOOKUP`, `image_file_count=2245783`, `missing_candidate_count=7766`, and `not_checked_count=344445`.
- `orphaned_deleted_candidates_sample.csv` showed `MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE` rows with lead-only language.
- The Missing-from-FFS candidate sample exposed blank `ffs_lookup_source`, so V1.6.35 normalizes blank lookup-source values to `lookup_available_no_matching_path`.

## Changed in V1.6.35

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

## V1.6.35

Validation/hardening release after V1.6.29.4. Adds GUI checked-state locking, non-blocking stale review query handoff, export worker detachment, length-aware CSV export for embedded NUL/control bytes, and APFS NXSB block-size rejection before use.

## V1.6.35

MacOS Store-V2 profile fix. The native parser now has explicit persistence modes and macOS profile uses macOS Store-V2 persistence instead of iOS CoreSpotlight compact status/mode. Native parser progress is mirrored to root progress files.
