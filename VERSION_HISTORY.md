
## V1.6.41 - iOS Unicode, LZ4 hardening, and communication review safety

- Hardened native Store-V2 LZ4 raw-block decompression against integer-overflow bounds bypasses by converting addition-based checks to subtraction/capacity checks and by guarding length accumulation.
- Widened native CoreSpotlight high-value string probing to preserve high-bit UTF-8, tabs, CR, and LF while still treating exact NUL bytes as delimiters.
- Widened iOS bplist string ripping to preserve high-bit UTF-8 and CR/LF/TAB; UTF-16 fallback remains bounded.
- Added bounded UID/object-table hint expansion to the internal bplist/NSKeyedArchiver decoder while retaining recursion, object-count, and JSON-size caps.
- Confirmed existing iOS communication anti-join view `vw_ios_spotlight_comms_missing_from_ffs` remains available in both core schema and GUI review schema.
- Confirmed existing `tel:` and `mailto:` fallback identity recovery remains present in `deriveIosCommunicationFields`.


## V1.6.41 - MSVC raw-string safety hotfix

- Split oversized SQL raw-string blocks in `src/db/case_db.cpp` after the V1.6.41 iOS index-update timeline view change.
- No parser, enrichment, or GUI behavior change from V1.6.41.
- Local raw-string audit found no raw-string body over 5,000 characters.


## V1.6.41.1 - CSV default, source-profile filtering, unresolved-label path guard

- GUI processing now defaults to `Exclude CSV exports` checked. SQLite case output remains the default review artifact unless CSV exports are explicitly enabled.
- Non-iOS ZIP profiles now record that iOS FFS/app-database parser stages were skipped.
- macOS-profile exports now skip `ios_*` CSV export calls rather than writing large groups of zero-row iOS CSVs.
- Unresolved Store-V2 review labels are no longer accepted as valid filename/path components for parent-inode path reconstruction.
- Added `docs/V1_6_41_CSV_DEFAULT_AND_SOURCE_PROFILE_FILTERING.md`.


## V1.6.37.1 macOS unresolved Store-V2 object labels

- Added explicit unresolved object labels for macOS Store-V2 records that still lack structured names after dictionary/path-probe enrichment.
- Labels are forensic review handles, not asserted filenames.
- Added parser metric `unresolved_identifier_label_artifacts`.

# V1.6.37.1

- Fixed macOS Store-V2 GUI review rows showing `------NONAME------` despite native path probe candidates being present in parsed key/value rows.
- Added native path probe promotion before timeline materialization so GUI and exports can use recovered macOS path evidence.

# 1.6.41

- Added parent-inode path-apply skip when no new reconstructed path candidates exist.
- Added explicit run-status markers for skipped no-op path apply.
- Kept V1.6.32 build-preflight hardening and V1.6.31 macOS Store-V2 persistence behavior.

# 1.6.41

- Hardened build wrapper preflight behavior so brittle release-readiness documentation/version-marker checks cannot prevent MSVC compilation from starting.
- Build wrapper now reads expected version from `VERSION` and uses that value for post-build CLI version verification.
- Release-readiness remains available as an advisory validation script.

# Vestigant Spotlight Investigator V1.6.37.1

## Compile hotfix

- Fixes MSVC compile error in `src/parsers/aff4_probe_worker.cpp` by replacing `appendProbeNote(...)` with the in-file helper `aff4ApfsAppendProbeNote(...)` in OMAP vertical-cycle handling.
- Hardens `Build-V1_6_41.ps1` so it fails before version probing if the CLI executable was not produced or if the build log contains compiler/linker errors.
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

See `docs/V1_6_41_CODE_REVIEW_VALIDATION_HARDENING.md` for the detailed issue-by-issue audit.

# Version History

## V1.6.37.1
- Fixed GUI/review-layer Missing-from-FFS lookup-source propagation so active validation can pass `reference_view_lookup_source_populated`.
- Hardened orphan/missing candidate materialization with nonblank lookup-source fallback in candidate reasons.
- Preserved lead-only Missing-from-FFS interpretation.

# Version History

## V1.6.37.1
- Added active-comparison validation checks for candidate-summary reconciliation, unsafe deletion-language absence, reference-candidate materialization, and message-attachment candidate visibility.
- Added CoreDuet interactionC validation checks for canonical ZINTERACTIONS reconciliation, join-table suppression, phone-label suppression, and contextual guardrail notes.
- Added new full and bounded validation exports for CoreDuet interactionC checks.
- Added required bounded validation samples to the focused iOS thin wrapper.
- Preserved lead-only Missing-from-FFS guardrails.

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

Validation/hardening release after V1.6.29.4. Adds GUI checked-state locking, non-blocking stale review query handoff, export worker detachment, length-aware CSV export for embedded NUL/control bytes, and APFS NXSB block-size rejection before use.

## V1.6.37.1

MacOS Store-V2 profile fix. The native parser now has explicit persistence modes and macOS profile uses macOS Store-V2 persistence instead of iOS CoreSpotlight compact status/mode. Native parser progress is mirrored to root progress files.
