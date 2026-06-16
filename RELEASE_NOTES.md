# V1.6.35

- Loads external `dbStr-*` maps for macOS Store-V2 databases when native v2 headers have zero in-header dictionary pointers and adjacent map components exist.
- Removes iOS-only gating from external dbStr map loading; component presence now controls this path.
- Renames the log/failure phase to generic Store-V2 terminology instead of iOS-specific text.
- Carries forward V1.6.34 native path-probe promotion for GUI display names/paths.

# V1.6.35

- Promotes macOS Store-V2 native path probe candidates from `raw_key_values` into artifact display/path fields when artifact names are placeholder or path context is weak.
- Adds run-status and parser metric markers for native path probe promotion.
- Preserves V1.6.33 parent-inode path-apply skip optimization and V1.6.32 advisory release-readiness build workflow.

# Vestigant Spotlight 1.6.35

## macOS zipped Spotlight thin performance fix

- Reviewed V1.6.32 macOS zipped Spotlight thin evidence. The run reached `complete_success`, used macOS Store-V2 persistence, parsed 50,000 bounded records, and produced 47,928 artifacts.
- Added a guard to skip the parent-inode path apply UPDATE when link analysis reports `new_reconstructed_paths=0`.
- This targets the observed no-op enrichment phase that consumed several minutes and ended with `artifacts_updated=0`.

# Vestigant Spotlight 1.6.35

## Release-preflight hardening

- Fixed the recurring build-blocking release-readiness failure pattern by making release-readiness advisory from the build wrapper.
- Kept PowerShell wrapper compatibility and MSVC raw-string risk checks as fatal preflight checks.
- Replaced stale literal CLI version checking with a dynamic check based on the root `VERSION` file.
- Kept V1.6.31 macOS Store-V2 persistence/profile fixes.

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

# Vestigant Spotlight Investigator V1.6.35

## Validation fix
- V1.6.27 thin completed and produced the new active-comparison/CoreDuet validation samples. Eight active-comparison checks passed and one remained REVIEW because the GUI-layer Missing-from-FFS view re-created `vw_ios_spotlight_missing_from_ffs_candidates` with blank `ffs_lookup_source`.
- V1.6.35 updates the GUI/review-layer Missing-from-FFS view to carry lookup source from full iOS FFS inventory or slim path lookup into exported reference rows.
- V1.6.35 also hardens orphan/missing candidate materialization so `orphan_reason` uses a nonblank lookup-source fallback.
- Missing-from-FFS rows remain investigative leads only, not deletion proof.

# Vestigant Spotlight Investigator V1.6.35

## Validation hardening
- Verified from V1.6.26 thin evidence that active filesystem comparison completed and produced 7,766 materialized Missing-from-FFS reference candidates, but one validation check remained REVIEW because reference-view lookup-source reporting was not populated in the exported validation surface.
- Added additional active filesystem comparison validation checks for candidate-summary reconciliation, unsafe deletion-language absence, materialized-reference provenance, and high-value message-attachment candidate visibility.
- Added CoreDuet interactionC validation checks to reconcile parsed rows to canonical ZINTERACTIONS rows, confirm join tables are not promoted as standalone events, confirm Phone: identity promotion remains suppressed, and confirm contextual guardrail notes are present.
- Added bounded upload enforcement for the new active-comparison and CoreDuet validation samples.
- Missing-from-FFS rows remain investigative leads only, not deletion proof.

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

- Validated V1.6.29.4 build and iOS CoreSpotlight thin upload evidence.
- Hardened GUI checked-artifact state locking to avoid concurrent `std::set` read/write races.
- Replaced synchronous review-query thread join in `loadReviewPage()` with stale-request invalidation and worker detach.
- Changed export worker registration to avoid accumulating thread handles for the life of the process.
- Made SQLite CSV export length-aware using `sqlite3_column_bytes()` and visible `[NUL]`/control-byte placeholders.
- Rejected suspicious APFS NXSB block sizes before APFS traversal/allocation use.
- Retained V1.6.29 AFF4 LZ4/bplist/pagination hardening and iOS active-comparison/CoreDuet validation guardrails.

## V1.6.35

- Fixes misleading macOS folder Spotlight native parse status that reported iOS CoreSpotlight compact persistence during macOS Store-V2 runs.
- Adds explicit native persistence modes: macOS Store-V2, iOS CoreSpotlight compact, and auto path-sensitive.
- macOS profile now disables iOS CoreSpotlight compact filtering by construction.
- Adds `native_parse_configuration` and `native_parse_store_persistence_mode` progress/status visibility.
- Mirrors native parser progress from `logs/run_progress.tsv` to root `run_progress.tsv` / `last_progress.tsv` so long native parses do not appear silent when root progress files are monitored.
