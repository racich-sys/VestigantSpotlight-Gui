## Current package source review: V1_1_11

# V1.1.11 Source Docs/Scripts Review

- Reviewed uploaded V1.1.10.1 build log and thin ZIP before changes.
- Reviewed source documentation/script layout.
- Consolidated 47 standalone docs/source-review note files into `docs/CONSOLIDATED_DEVELOPMENT_NOTES.md`.
- Consolidated 70 standalone validation log/note files into `validation/CONSOLIDATED_VALIDATION_LOGS_AND_NOTES.md`.
- Retained all support/diagnostic tools and recorded retention decisions in `docs/SUPPORT_DIAGNOSTIC_TOOLS_REGISTER.md`.
- Removed only files whose content was consolidated into active aggregate documents.

---

# Consolidated Development Notes
This file consolidates prior standalone development-note files into one active-package reference. Individual prior-version note files were removed from the active package after consolidation to reduce package clutter; append-only version history remains preserved separately.
## Consolidation index
- `V1_1_9_LIVE_APFS_LEAF_TRAVERSAL_AND_SOURCE_REVIEW.md`
- `V1_1_8_WINDOWS_PATH_AND_HISTORY_BASELINE.md`
- `V1_1_7_AFF4_PROBE_WORKER_DYNAMIC_RELOCATION.md`
- `V1_1_7_1_BUILD_HOTFIX_AND_PACKAGE_CLEANUP.md`
- `V1_1_6_AFF4_PROBE_WORKER_DIRECT_MAP_SPLIT.md`
- `V1_1_6_1_AFF4_WORKER_WINDOWS_BUILD_HOTFIX.md`
- `V1_1_5_AFF4_CANCEL_AND_UPLOAD_GUARDS.md`
- `V1_1_5_1_BUILD_HOTFIX.md`
- `V1_1_4_BPLIST_AND_GUI_STATE_HARDENING.md`
- `V1_1_3_EXPORT_CANCEL_AND_PURGE_HARDENING.md`
- `V1_1_2_CANCELLATION_DLL_AND_BPLIST_HARDENING.md`
- `V1_1_1_EVIDENCE_INTAKE_AND_GUI_INGEST_THREAD.md`
- `V1_1_10_SOURCE_PACKAGE_CLEANUP.md`
- `V1_1_0_ORCHESTRATOR_MODULARIZATION_AND_DB_LIFETIME.md`
- `V1_1_0_1_PACKAGING_HOTFIX.md`
- `V1_0_7_APFS_MODULE_REFACTOR.md`
- `V1_0_6_APFS_BTREE_ITERATOR_AND_DIRECT_COPYOUT.md`
- `V1_0_4_AFF4_APFS_REVIEW_AND_LIMIT_CLEANUP.md`
- `V1_0_31_EVIDENCE_INTAKE_HELPERS_AND_IOS_IMPORT_PERFORMANCE.md`
- `V1_0_30_IOS_DB_AND_GUI_EXPORT_LIFECYCLE.md`
- `V1_0_2_AFF4_APFS_EXTRACTION_AND_CLEANUP.md`
- `V1_0_29_BUILD_SCRIPT_AND_LOW_RISK_HARDENING.md`
- `V1_0_29_BUILD_HOTFIX.md`
- `V1_0_28_1_APFS_DIAGNOSTIC_EXPORTER_RELOCATION.md`
- `V1_0_27_PROCESS_AND_GUI_SQLITE_HARDENING.md`
- `V1_0_26_THIN_UPLOAD_AND_IO_HARDENING.md`
- `V1_0_25_THIN_UPLOAD_SECURITY_AND_IOS_PERFORMANCE.md`
- `V1_0_25_GUI_VIEW_HELPERS_BUILD_HOTFIX.md`
- `V1_0_23_APFS_DIAGNOSTIC_MODEL_HEADER.md`
- `V1_0_22_GUI_EXPORT_WORKER_MODULARIZATION.md`
- `V1_0_21_GUI_BUILD_HOTFIX.md`
- `V1_0_20_GUI_EXPORT_AND_APFS_DIAGNOSTIC_MODULARIZATION.md`
- `V1_0_18_MODULARIZATION_CLEANUP_PLAN.md`
- `V1_0_18_LZFSE_LZVN_OPTIONAL_VENDOR_INTEGRATION.md`
- `V1_0_18_GUI_EXPORT_AND_MODULARIZATION_CLEANUP.md`
- `V1_0_18_DIRECT_LOGICAL_SIZE_TRIM.md`
- `V1_0_18_APPLE_LZFSE_VENDOR_ENABLEMENT.md`
- `V1_0_15_STOREV2_DUAL_PROCESS_COMPARE.md`
- `V1_0_15_PRODUCTION_MODULARIZATION_REVIEW.md`
- `V1_0_15_LZFSE_LZVN_SOURCE_REVIEW.md`
- `V1_0_15_COPYOUT_SOURCE_ISOLATION.md`
- `V1_0_12_STRUCTURAL_CLEANUP_AND_APFS_DIAGNOSTIC_IO_REVIEW.md`
- `V1_0_10_GUI_VIEW_REGISTRY_REFACTOR.md`
- `SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10_1.md`
- `SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.md`

---

## Source: `V1_1_9_LIVE_APFS_LEAF_TRAVERSAL_AND_SOURCE_REVIEW.md`

# V1.1.9.1 Live APFS Leaf Traversal and Source Review

## Scope

V1.1.9.1 promotes a bounded live APFS B-tree horizontal leaf traversal path inside the guarded AFF4/APFS probe worker. It also reviews the source package `.md`, `.txt`, and `.ps1` files for current-roadmap relevance.

## Implemented

- Added bounded next-leaf traversal to shared APFS OMAP target resolution.
- Added bounded next-leaf traversal to the dynamic/libaff4 APFS OMAP target lookup path.
- Added bounded next-leaf traversal to dynamic/libaff4 APFS volume root-tree lookup.
- Added cycle detection, transition cap, cancellation checks, and diagnostic notes for next-leaf transitions.
- Updated `apfsReadNextLeafOidFromBtreeInfoFooter()` documentation to reflect live guarded use.
- Reviewed all `.md`, `.txt`, and `.ps1` files and recorded decisions in `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9_1.csv`.

## Safety gates

- The traversal is bounded to 256 horizontal leaf transitions per lookup.
- Cycles stop traversal.
- Unsafe next-leaf offsets stop traversal.
- Invalid/non-leaf next nodes stop traversal.
- Existing cancellation callbacks are checked while scanning leaf entries.

## Not implemented

- Full APFS absolute path reconstruction.
- Full NSKeyedArchiver UID graph decoding.
- Win32 virtual ListView conversion.

Those remain roadmap items.

## Source: `V1_1_8_WINDOWS_PATH_AND_HISTORY_BASELINE.md`

# V1.1.8 Windows Path and History Baseline

## Scope

V1.1.8 starts from the validated V1.1.7.1 source/build/thin baseline and the user-provided `BaselineVersionHistory.md`.

## Changes

- Replaced the in-package full history with the user-cleaned baseline and appended V1.1.8.
- Added Windows long-path helpers in `src/core/path_utils.*`.
- Routed APFS/AFF4 Store-V2 evidence copy-out and decmpfs reconstruction writes through long-path-capable file output on Windows.
- Changed `CaseDatabase::close()` to request `SQLITE_CHECKPOINT_TRUNCATE`.
- Added an explicit WAL checkpoint/truncate status marker before upload packaging.
- Added mutex protection around `Logger` writes.
- Lowered the APFS decmpfs expected-output cap to 256 MiB.

## Deferred

- Live APFS horizontal leaf promotion.
- Full NSKeyedArchiver UID graph decoding.
- Win32 virtual list-view conversion.
- Complete EvidenceIntake ZIP-staging relocation.

## Source: `V1_1_7_AFF4_PROBE_WORKER_DYNAMIC_RELOCATION.md`

# V1.1.7 AFF4 Probe Worker Dynamic Relocation

## Scope

V1.1.7 completes the major Tracker #17 modularization step that remained after V1.1.6. The dynamic libaff4 APFS probe body was physically moved from `src/app/app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp`.

## Implemented

- Added `Aff4ProbeWorker::executeDynamicLoadProbe(...)`.
- Replaced the app-runner call to `writeAff4CppLiteDynamicLoadProbe(...)` with `Aff4ProbeWorker::executeDynamicLoadProbe(...)`.
- Removed the dynamic probe body from `app_runner.cpp`.
- Kept direct-map probe worker introduced earlier.
- Added cancellation callback support to the shared APFS OMAP traversal helper and passed the appropriate direct/dynamic cancellation closures.

## Not changed

- No AFF4/APFS traversal semantics were intentionally changed.
- No copy-out/staging rules were changed.
- No Store-V2 parser behavior was changed.
- No iOS parser behavior was changed.
- No schema changes were made.

## Validation

Local Linux validation passed: syntax checks, CMake build, CLI version check, and self-test. Windows/MSVC and AFF4 thin-output parity still require user-side validation.

## Source: `V1_1_7_1_BUILD_HOTFIX_AND_PACKAGE_CLEANUP.md`

# V1.1.7.1 Build Hotfix and Package Cleanup

## Purpose

V1.1.7 physically moved both major AFF4/APFS probe bodies into `src/parsers/aff4_probe_worker.cpp`, but the Windows/MSVC build exposed missing helper dependencies left behind in `app_runner.cpp`.

## Fixed

- Added worker-local helper boundary for:
  - `shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(...)`
  - `findToolCandidate(...)`
  - `lastWindowsErrorString(...)`
- Kept helper scope local to `aff4_probe_worker.cpp` to avoid new public API and ODR/linker risk.
- Preserved both probe bodies outside `app_runner.cpp`.

## Cleaned

- Removed old version-specific scripts from `scripts/`.
- Removed old root-level package manifests/deleted-files manifests.
- Added a source package cleanup policy.
- Added a new-chat continuation guide.
- Added full append-only version history baseline files.

## Not changed

- APFS traversal semantics.
- AFF4 read behavior.
- Store-V2 parser.
- iOS parser.
- SQLite schema.
- GUI behavior.

## Source: `V1_1_6_AFF4_PROBE_WORKER_DIRECT_MAP_SPLIT.md`

# V1.1.6 AFF4 Probe Worker Direct-Map Split

V1.1.6 begins the Tracker #17 God-closure remediation by physically moving the `writeAff4DirectMapReaderProbe` body out of `src/app/app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp`.

## Implemented

- New module: `src/parsers/aff4_probe_worker.h/.cpp`.
- New entry point: `Aff4ProbeWorker::executeDirectMapReaderProbe(...)`.
- App runner delegates direct-map probe calls to the worker.
- Build systems include `src/parsers/aff4_probe_worker.cpp`.

## Deferred

`writeAff4CppLiteDynamicLoadProbe(...)` remains in `app_runner.cpp`. A full physical extraction was started, but the function depends on many app-runner-local structs/helpers and needs a separate dependency boundary pass. V1.1.6 records this in `docs/WORKFLOW_LEDGER.md` so the same dependency discovery is not repeated.

## Forensic behavior

No live APFS interpretation, AFF4 byte-read semantics, copy-out decisions, or Store-V2 parsing logic were intentionally changed.

## Source: `V1_1_6_1_AFF4_WORKER_WINDOWS_BUILD_HOTFIX.md`

# V1.1.6.1 AFF4 Worker Windows Build Hotfix

V1.1.6 physically moved `writeAff4DirectMapReaderProbe(...)` into `src/parsers/aff4_probe_worker.cpp`. The Linux validation passed because the affected code path was under `_WIN32`, but MSVC correctly reported that `wideProcessPath(...)` was not available in the new translation unit.

V1.1.6.1 adds a local Windows-only path widening helper to the AFF4 probe worker. It mirrors the UTF-8/ACP fallback behavior used by the app runner and keeps the helper translation-unit local to avoid new exported symbols.

No APFS traversal, AFF4 read, Store-V2 parser, iOS parser, SQLite schema, copy-out, or GUI behavior was changed.

## Source: `V1_1_5_AFF4_CANCEL_AND_UPLOAD_GUARDS.md`

# V1.1.5 AFF4 Cancellation and Upload Guard Hardening

V1.1.5 is a repeat-cycle hardening release based on the V1.1.4 Windows/MSVC build and macOS AFF4/APFS thin-output baseline.

## Implemented

- Propagated the existing ingest cancellation token into the guarded AFF4/libaff4 dynamic-load probe and the direct AFF4 map-reader probe.
- Added bounded cancellation checks inside selected expensive APFS/AFF4 probe loops so Cancel Ingest can return sooner during long scans.
- Added an early case-directory writability probe before logger/database initialization.
- Replaced PowerShell redirection for targeted app database extraction and focused CoreSpotlight extraction logs with explicit UTF-8 `Out-File` writes.
- Replaced recursive thin-upload copying of `exports/upload_samples` with explicit per-file policy handling and the existing 50 MB size guard.
- Applied the same nested upload-samples size policy to `tools/Create-SourceProbeUploadZip.ps1`.
- Wrapped APFS staged Store-V2 diagnostic sample CSV exports in a localized try/catch so diagnostic-sample failure does not suppress final probe summaries.

## Not implemented

- Full `writeAff4CppLiteDynamicLoadProbe(...)` extraction into `aff4_probe_worker.cpp`.
- Full `stageZipEvidenceSource(...)` relocation into `EvidenceIntake`.
- Live APFS horizontal leaf traversal replacement.
- Live APFS absolute path reconstruction.
- Full NSKeyedArchiver UID graph decoding.

Those remain tracked as separate high-risk/focused targets. V1.1.5 does not alter live Store-V2 parsing semantics or APFS copy-out interpretation.

## Source: `V1_1_5_1_BUILD_HOTFIX.md`

# V1.1.5.1 Build Hotfix

V1.1.5.1 fixes the V1.1.5 MSVC compile failure in `src/app/app_runner.cpp`.

The V1.1.5 cancellation propagation inserted a `return false;` inside a lambda returning `ApfsOmapTargetResolution`. V1.1.5.1 returns a populated cancellation-status `ApfsOmapTargetResolution` instead.

No live forensic extraction behavior was otherwise changed.

## Source: `V1_1_4_BPLIST_AND_GUI_STATE_HARDENING.md`

# V1.1.4 Bplist and GUI State Hardening

V1.1.4 is a repeat-cycle hardening release based on the V1.1.3 build/thin baseline.

## Implemented

- Extended bounded CoreSpotlight bplist context metadata with offset-table validation details and the top-object relative offset where valid.
- Added checked-artifact snapshot helpers for review/export request construction.
- Replaced the ingest-start load/store gate with an atomic compare/exchange gate to reject repeated start requests before a second worker can be created.

## Not implemented

- Full NSKeyedArchiver UID graph decoding.
- Live APFS path reconstruction.
- Live APFS B-tree horizontal traversal replacement.
- Dynamic AFF4/APFS probe monolith extraction.

Those remain tracked for dedicated versions.

## Source: `V1_1_3_EXPORT_CANCEL_AND_PURGE_HARDENING.md`

# V1.1.3 Export Cancellation and Purge Hardening

V1.1.3 is a repeat-cycle hardening release built from V1.1.2.

## Implemented

- GUI export workers now accept cancellation callbacks and check for shutdown before long SQLite export scans.
- Export Current Page, Export Filtered, Export Checked, and Export Tagged pass a shutdown-aware callback from the Win32 GUI.
- Orphan source-row cleanup now runs inside a single SQLite transaction, while still logging per-table purge warnings.
- RichEdit is loaded from System32 via `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_SYSTEM32)` when available and freed in `WM_DESTROY`.
- `ApfsVolumeReader` next-leaf helper scaffolding is improved for comparator work but is not wired into live AFF4/APFS staged extraction.

## Explicit non-changes

- No live APFS traversal replacement.
- No new forensic APFS path interpretation.
- No full NSKeyedArchiver UID graph decode.
- No SQLite schema changes.

## Source: `V1_1_2_CANCELLATION_DLL_AND_BPLIST_HARDENING.md`

# V1.1.2 Cancellation, DLL Search, Native Parse, and Bplist Hardening

## Scope

V1.1.2 is a broader repeat-cycle hardening release based on the validated V1.1.1 baseline. It addresses safe cancellation plumbing, dependent DLL search hardening, GUI resource cleanup, native Store-V2 parse performance, and bounded bplist trailer validation.

## Implemented

- Added GUI `Cancel Ingest` button and `gCancelIngestRequested` token.
- Added optional `std::atomic_bool` cancellation token to `runApplication(...)`.
- Added safe cancellation checkpoints around source probing, staging, discovery, native parse, and enrichment entry.
- Hardened AFF4 dynamic loading by using `SetDefaultDllDirectories(...)`, `AddDllDirectory(...)`, and `LoadLibraryExW(...)` with user/default search directories.
- Freed `gLogoBitmap` on `WM_DESTROY`.
- Applied temporary bulk SQLite PRAGMAs around native Store-V2 parse inserts and restored WAL/NORMAL settings afterward, including exception restoration.
- Added bounded bplist trailer validation metadata to the existing bplist context string.
- Added `docs/WORKFLOW_LEDGER.md` to avoid rediscovering prior build/package failures.

## Deliberately unchanged

- No AFF4/APFS read semantics changed.
- No live APFS traversal replacement.
- No live APFS path reconstruction.
- No full NSKeyedArchiver object graph decode is emitted.
- No SQLite schema changes.

## Validation required

- Windows/MSVC build.
- Windows GUI launch and Cancel Ingest button smoke test.
- macOS AFF4/APFS thin run.
- iOS run when practical because native bplist context metadata changed.

## Source: `V1_1_1_EVIDENCE_INTAKE_AND_GUI_INGEST_THREAD.md`

# V1.1.1 Evidence Intake and GUI Ingest Thread

V1.1.1 continues the broader repeat-cycle refactor after V1.1.0.1 built and produced a macOS AFF4/APFS thin ZIP.

## Implemented

- `importIosInventoryCsvs(...)` is now `EvidenceIntake::importIosInventoryCsvs(...)`.
- Cache-SQLite iOS inventory import helpers and referenced-path lookup import moved to the intake module.
- `app_runner.cpp` now delegates referenced-path lookup import through `EvidenceIntake::importReferencedIosPathLookupFromReuseCache(...)`.
- Run-status/progress logging remains controlled by the orchestrator through callback injection.
- Win32 GUI Build/Process Case no longer launches the main ingest worker with `.detach()`.
- The GUI tracks the ingest thread and joins it on `WM_DESTROY` to reduce risk of abrupt SQLite WAL interruption during app close.
- The AFF4 stream inventory callback signature was preserved and the platform-specific unused callback warning was suppressed.

## Deferred

- Full `stageZipEvidenceSource(...)` relocation.
- Dynamic AFF4/APFS probe worker extraction.
- Live APFS path reconstruction or B-tree iterator replacement.
- NSKeyedArchiver unflattened output.
- Full Win32 global-state object migration.

## Validation

- C++20 syntax checks passed for changed/dependent files.
- Linux CMake build passed.
- CLI version check reported `Vestigant Spotlight v1.1.1`.
- Local self-test passed.
- Windows/MSVC build, GUI runtime, AFF4 thin, and iOS runtime validation are still required.

## Source: `V1_1_10_SOURCE_PACKAGE_CLEANUP.md`

# V1.1.10 Source Package Cleanup

V1.1.10 uses V1.1.9.1 as the base and performs a package-hygiene pass only.

## Changes

- Regenerated current version metadata and V1.1.10 wrapper scripts.
- Reviewed all `.md`, `.txt`, and `.ps1` files in the source package.
- Removed obsolete root-level package manifests from the prior generated package.
- Removed stale source-review inventory files that were replaced by `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.*`.
- Preserved append-only version history and historical validation material.

## Not changed

- No AFF4/APFS traversal, copy-out, decompression, staging, Store-V2 parsing, external comparison, iOS parsing, GUI review behavior, or SQLite schema behavior was intentionally changed.

## Source: `V1_1_0_ORCHESTRATOR_MODULARIZATION_AND_DB_LIFETIME.md`

# V1.1.1 Orchestrator Modularization and Database Lifetime

## Purpose

V1.1.1 is a broader `repeat`-cycle release. It targets the remaining production-readiness concerns that can be moved safely without changing live APFS extraction semantics or emitting new unvalidated forensic interpretations.

## Implemented

- `runApplication()` now opens `CaseDatabase` once and reuses that handle through AFF4/raw and general processing.
- APFS decmpfs/resource-fork reconstruction helpers were moved to the codec module.
- APFS NX superblock parsing was moved to `apfs_volume_reader.cpp/.h`.
- AFF4 stream inventory classification/reporting was moved to `apfs_aff4_reader.cpp/.h` with callback injection for process execution.
- `writeAff4ApfsV1DiagnosticRerunPlan()` was moved to `apfs_diagnostic_exporter.cpp/.h`.
- Non-live APFS path/leaf helper APIs were added for future comparator work.

## Not implemented

- Live APFS traversal replacement.
- APFS staged path substitution using the new path helper.
- Full `writeAff4CppLiteDynamicLoadProbe` worker extraction.
- Full evidence staging/import orchestration movement.
- NSKeyedArchiver unflattened output.
- Win32 GUI global state rewrite.

## Validation

- C++20 syntax checks were run for changed and dependent source files.
- Linux CMake configure/build completed.
- CLI version check reported `Vestigant Spotlight v1.1.1`.
- Local self-test passed.

Windows/MSVC and live macOS/iOS runtime validation remain required.

## Source: `V1_1_0_1_PACKAGING_HOTFIX.md`

# V1.1.1 Packaging Hotfix

## Scope

V1.1.1 restores the root build scripts that were accidentally omitted from the V1.1.0 full source ZIP.

## Fixed

- Restored `build_windows_msvc.bat`.
- Restored `build_windows_msvc_nocmake.bat`.
- Restored `build_linux_test.sh`.
- Regenerated V1.1.1 versioned PowerShell wrappers.

## Not changed

No application source behavior was changed beyond version metadata and documentation/tracking updates.

## Validation needed

- Windows/MSVC build.
- macOS AFF4/APFS thin run.

## Source: `V1_0_7_APFS_MODULE_REFACTOR.md`

# V1.0.7 APFS Module Refactor and AFF4/APFS Thin-Run Review

## V1.0.6 thin-run findings

The V1.0.6 Windows build completed successfully and produced CLI, test, and GUI binaries reporting version 1.0.6. The AFF4/APFS run demonstrated a major extraction improvement:

- APFS root-tree traversal visited 94,796 nodes and scanned 3,833,445 records.
- 940,070 directory records were decoded.
- 16,539 target inode rows were materialized.
- 27,374 target FILE_EXTENT rows were materialized.
- 9,902 copy-out rows were produced.
- 9,084 rows copied through direct indexed extent chains.
- 82 rows copied with recorded synthetic zero regions.
- The external comparison found 8,338 Vestigant staged files and 1 exact relative-path/hash match against the external Store-V2 reference.

The remaining extraction gap is not basic AFF4 access. The tool is reading AFF4-backed APFS metadata and copying many files. The remaining blocker is correctness/completeness: path selection, exact Store-V2 component selection, logical-size trimming, and mismatch reduction against the external reference.

## Implemented in V1.0.7

- Added `src/parsers/apfs_volume_reader.h` and `src/parsers/apfs_volume_reader.cpp` as the dedicated APFS module boundary.
- Added APFS key helpers: search-key creation, object-id extraction, record-type extraction, and record-type labels.
- Added Store-V2 component classification helpers.
- Added APFS-safe path-component sanitizer.
- Added APFS copy-status classification helpers.
- Added an explicit `ApfsVolumeReader` class shell with the production-facing APIs planned for the lower-bound B-tree iterator:
  - `enumerateDirectory()`
  - `resolvePathToInode()`
  - `extractFileToDisk()`
- Added APFS module smoke tests to `VestigantSpotlightTests`.
- Added the new parser module to CMake and no-CMake MSVC build manifests.
- Fixed AFF4/APFS stage/count classification so V1.0.6 direct copy statuses are treated as successful copies:
  - `COPIED_DIRECT_INDEXED_EXTENT_CHAIN`
  - `COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS`
- Updated staging status labels for files copied with synthetic zero provenance.

## Deliberately not implemented in V1.0.7

The full lower-bound B-tree leaf-jumping iterator was not wired to live AFF4 reads in this version. The current app-runner APFS code already works well enough to copy thousands of files, and a full iterator refactor should not be combined with new extraction behavior in the same release. V1.0.7 creates the module boundary, validates helper behavior, and fixes reporting/counting around the current direct copy-out path.

LZFSE/LZVN decompression remains delayed until a vetted source tree and known-good test vectors are added.

## V1.0.8 benchmarks

Move the APFS iterator into the new module only when these conditions are met:

1. `ApfsVolumeReader` receives an injected block-reader/OMAP resolver rather than owning AFF4 globals.
2. Unit tests cover lower-bound key navigation with synthetic B-tree nodes.
3. The iterator records these metrics: lower-bound lookups, leaf nodes visited, next-leaf transitions, cycle stops, malformed-node stops, and directory entries returned.
4. A live AFF4 run returns the same or better `.Spotlight-V100/Store-V2` directory-entry coverage as V1.0.6.
5. External comparison improves from V1.0.6 baseline: external-only rows and relative-path size mismatches decrease, while exact hash/path matches increase.

## Source: `V1_0_6_APFS_BTREE_ITERATOR_AND_DIRECT_COPYOUT.md`

# V1.0.6 APFS B-Tree Iterator and Direct AFF4 Copy-Out Work

## Implemented

1. Added direct AFF4/APFS record indexing during the exhausted filesystem B-tree traversal. The direct path now records decoded INODE and FILE_EXTENT records from the same APFS root-tree walk used to find Spotlight directory records.
2. Added target materialization from the decoded directory namespace: Store-V2 recursive namespace rows are correlated to indexed inode records, private data-stream IDs, and file-extent rows.
3. Added guarded direct AFF4/APFS copy-out for matched Store-V2 rows where extents are ordered and readable. Sparse logical gaps and zero physical extents are written as explicit synthetic zero regions and recorded in copy-out notes.
4. Added a logical directory-walk report that follows the discovered APFS namespace Root -> .Spotlight-V100 -> Store-V2 -> children, with object IDs and path context. This is a production bridge toward a formal lower-bound iterator.
5. Kept AFF4/APFS diagnostic outputs available for source-probe/support runs, but documented that production ingest should later hide these behind diagnostic/support mode after direct Store-V2 extraction is promoted into normal store discovery.

## Delayed

A complete APFS lower-bound iterator with repeated seek continuation across non-sibling-linked APFS leaf nodes is still pending. APFS B-tree nodes are not sibling linked in the reference material, so a directory spanning multiple leaves cannot safely be continued by following a simple next-leaf pointer. The next benchmark is a reusable `ApfsFsTreeIterator` module that supports lower-bound seeks by full key bytes and repeated continuation.

LZFSE/LZVN decompression remains delayed until a vetted third-party source tree, MSVC/Linux build integration, and known-good decmpfs test vectors are included. No decompression stub is used.

## Source: `V1_0_4_AFF4_APFS_REVIEW_AND_LIMIT_CLEANUP.md`

# V1.0.4 AFF4/APFS Review and Limit Cleanup

## Review input

Reviewed the V1.0.3 AFF4/APFS thin upload and build log. V1.0.3 built the CLI, test binary, and GUI, but `Build-V1_0_3.ps1` incorrectly checked for version `1.0.1` after a successful `1.0.3` build. The AFF4/APFS run completed and produced external comparison outputs, but Store-V2 staging remained zero.

## Observed V1.0.3 metrics

- AFF4 direct map reader status: `DIRECT_MAP_READER_SMOKE_OK`.
- Map entries total: `318594`.
- APFS container superblock parsed: yes.
- APFS volumes resolved: `6`.
- Root-tree nodes visited: `49517`.
- Root-tree records scanned: `2000000`.
- Directory records decoded: `539471`.
- Spotlight target hits: `1034`.
- Nodes skipped by traversal limit: `45279`.
- Target inode rows: `0`.
- Target file extent rows: `0`.
- Staged Store-V2 files: `0`.
- External reference files: `4123`.
- Vestigant staged files: `0`.

## Implemented in V1.0.4

1. Fixed the Windows build script version check to require `1.0.4` instead of the stale `1.0.1` pattern.
2. Removed the direct AFF4/APFS root-tree node, record, and depth caps from the active traversal loop. Traversal now ends by queue exhaustion and visited-node cycle protection.
3. Kept only a diagnostic upload sample cap for `aff4_apfs_spotlight_name_scan_sample.csv`; this cap does not limit target discovery or directory-record collection.
4. Added complete direct-directory record collection independent from the name-sample CSV.
5. Added direct recursive Store-V2 namespace seeding from discovered Store-V2 directories and top-level component names, preserving group root, group name, relative path, and APFS path context in copy-attempt notes.
6. Corrected strict single-AFF4 policy reporting in the direct AFF4/APFS output writers from `false` to `true`.

## Deferred

Target-guided direct INODE/FILE_EXTENT copy-out remains deferred. The first attempted implementation of full direct guided lookup made `app_runner.cpp` compile impractically slowly in the Linux validation environment. This should be implemented in the next iteration by moving APFS B-tree target lookup code out of `app_runner.cpp` into a small reusable module, such as `src/apfs/apfs_btree_lookup.cpp`, with focused unit tests. Do not continue expanding `app_runner.cpp` for this logic.

## Next benchmark for V1.0.5

A V1.0.5 run should meet these benchmarks before full ingest promotion:

- `nodes_skipped_by_limit` must be `0`.
- `records_scanned` should exceed the old V1.0.3 cap of `2000000`, unless the queue exhausts earlier.
- `copy_attempt_rows` should exceed the old V1.0.3 value of `1034` if recursive Store-V2 child seeding succeeds.
- `aff4_apfs_spotlight_copy_attempt.csv` should include recursive Store-V2 `rel_path=` and `apfs_absolute_path=` notes for ordinary child files.
- Direct guided inode lookup should be moved into a separate APFS module and should produce nonzero `target_inode_hits` before attempting file byte copy-out.

## Source: `V1_0_31_EVIDENCE_INTAKE_HELPERS_AND_IOS_IMPORT_PERFORMANCE.md`

# V1.0.31 Evidence Intake Helpers and iOS CSV Import Performance

V1.0.31 advances the evidence-intake modularization roadmap without changing APFS/AFF4 extraction physics or SQLite schema.

## Implemented

- Added `src/ingest/evidence_intake.h/.cpp`.
- Moved reusable intake helper logic out of `app_runner.cpp`:
  - `countCsvDataRows`.
  - iOS ZIP path normalization and basename/extension helpers.
  - iOS database category/app/domain/protection/container hint helpers.
  - iOS app database staging path sanitization.
- Added temporary bulk-import SQLite PRAGMAs for regenerable iOS CSV fallback ingestion.
- Restored WAL/NORMAL settings after commit or rollback.
- Added `case_sensitive_like=OFF` to GUI review/export read-only connections.

## Deliberately not implemented

- Full `stageZipEvidenceSource` movement.
- Full `importIosInventoryCsvs` movement.
- APFS reverse path reconstruction.
- NSKeyedArchiver UID/object graph unflattening.
- GUI global state encapsulation.

These remain tracked but should be separate, focused versions.

## Source: `V1_0_30_IOS_DB_AND_GUI_EXPORT_LIFECYCLE.md`

# V1.0.30 iOS DB Parser Boundary and GUI Export Lifecycle

## Purpose

V1.0.30 advances two review suggestions that were safe to implement without changing APFS/AFF4 extraction physics or SQLite schema:

1. Move remaining iOS app database record-inventory orchestration out of the application runner and into the parser module.
2. Stop detaching GUI export threads and join active export workers during window destruction.

## Implemented changes

- Added `IosAppDbParser::parseRecordInventories(...)` in `src/parsers/ios_app_db_parser.cpp`.
- Added `IosAppDbStatusWriter` callback support so the parser module can preserve existing run-status behavior without depending directly on `app_runner.cpp` internals.
- Reduced `app_runner.cpp::parseIosAppDatabaseRecordInventories(...)` to a delegating wrapper.
- Added GUI export thread registry helpers in `src/gui/win32_gui.cpp`:
  - `registerExportThread(...)`
  - `joinExportThreadsNoThrow()`
  - `postExportResult(...)`
- Replaced export `.detach()` calls for:
  - Export Current Page
  - Export Filtered View
  - Export Checked
  - Export Tagged
- Joined registered export threads during `WM_DESTROY` after review-thread shutdown.

## Explicit non-goals

V1.0.30 does not change:

- AFF4/APFS read or traversal logic.
- APFS copy-out or Store-V2 staging.
- iOS CoreSpotlight parsing semantics.
- SQLite schema.
- GUI platform separation.
- APFS reverse path walker.
- NSKeyedArchiver bplist object interpretation.

## Validation still required

- Windows/MSVC build.
- GUI runtime export testing with application close during/after export.
- Current iOS thin or focused parser run to confirm parser-module delegation preserves output counts.

## Source: `V1_0_2_AFF4_APFS_EXTRACTION_AND_CLEANUP.md`

# Vestigant Spotlight V1.0.4 - AFF4/APFS Extraction and Repository Cleanup

## Immediate review inputs

Reviewed the V1.0.1 Windows build log and the V1.0.1 macOS AFF4/APFS thin output. V1.0.1 built successfully and accessed the selected AFF4/APFS image deeply enough to resolve APFS volume metadata and find `.Spotlight-V100` / `Store-V2` namespace entries. The remaining blocker was not AFF4 access; it was target object correlation and copy-out.

## Implemented in V1.0.4

### AFF4/APFS extraction movement

- Removed the hardcoded Spotlight traversal caps named in the review:
  - `kMaxSpotlightScanNodes`
  - `kMaxSpotlightDecodedNameSamples`
  - `kMaxSpotlightScanRecordsPerNode`
  - `kMaxSpotlightScanDepth`
  - `kMaxCopyOutFiles`
- Kept structural cycle protection through visited-node / visited-directory sets rather than the old fixed depth/node caps.
- Added APFS directory-entry parent mapping and path-context reconstruction for Store-V2 copy attempts.
  - Target rows now carry reconstructed APFS absolute path context in notes, for example expected Data-volume paths beginning `/System/Volumes/Data/...`.
- Added traversal-time INODE caching and later target correlation.
  - Previously scanned INODE rows are now correlated back to Store-V2 targets before guided private-id/dstream extent lookup.
- Added zero-fill assembly handling for sparse logical gaps and zero physical extents.
  - Sparse or zero regions are written as synthetic zero bytes instead of automatically skipping the file.
  - Output rows record synthetic zero counts and validation status.
  - Output hashes are calculated over the reconstructed logical byte stream.
- Forced GUI runs to keep preservation and native metadata decoding enabled.
- Removed GUI controls/handles for deprecated core/full-native toggles.
- Removed the app-runner self-test function and fake `/Users/alice/...` data from the primary binary.
- Replaced the test executable with a schema/view smoke test that does not generate fake forensic case records.
- Purged stale V0.9 run/package/collect scripts, V0.9 validation artifacts, and old Codex change notes from the production package.

## Partially implemented or delayed

### LZFSE/LZVN static integration

Not implemented in V1.0.4. A vetted Apple/reference `lzfse` source tree was not present in the uploaded project source, and adding a decompression stub would be forensically unsafe. Current decmpfs reconstruction support remains limited to already implemented plain/zlib and limited marker-handling paths.

Benchmark for implementation:

- Add a vendored `third_party/lzfse/` source tree with license text.
- Build it on Linux and MSVC from the project CMake and MSVC batch build.
- Add unit vectors for at least: raw uncompressed decmpfs, zlib decmpfs, LZVN, LZFSE, malformed payload, truncated resource fork.
- Require extracted output size and SHA256 match for known-good external extraction samples before enabling production staging.

### Full transition from source-probe to normal ingest

Not completed in V1.0.4. The code now moves closer to copy-out, but the AFF4/APFS path still remains diagnostic-forward until V1.0.4 output proves that target INODE, XATTR, FILE_EXTENT, sparse/zero provenance, and staged Store-V2 files are valid.

Benchmark for promotion into `discoverStores()`:

- `aff4_apfs_spotlight_inode_probe_summary.json` shows nonzero target inode hits.
- `aff4_apfs_spotlight_file_extent_probe_summary.json` shows nonzero target extent rows.
- `aff4_apfs_spotlight_file_copy_out_summary.json` shows copied files and no unexplained read/write failures.
- `aff4_apfs_extracted_storev2_stage_summary.json` shows staged Store-V2 groups/files.
- External comparison shows material improvement against `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`.
- Staged Store-V2 files parse through `NativeStoreDbParser` and produce case DB artifacts without corrupt/false-positive files.

### Diagnostic view cleanup

Only package/script cleanup was done in V1.0.4. SQLite diagnostic views were not removed broadly because several of them are currently used to report parser coverage and evidence limitations. Removing them without a view registry/support-export split could reduce forensic transparency.

Benchmark for production cleanup:

- Classify each view as `investigator`, `case_quality`, `support`, or `internal_diagnostic`.
- GUI should show only `investigator` and selected `case_quality` views by default.
- Support/internal views should be generated only when an explicit support/diagnostic export is requested.
- No user-facing View Set should show views whose names include `diagnostic` unless support mode is active.

### Remaining APFS copy-out safeguards

The per-file copy-out memory/size gate remains at 512 MB. This is retained to avoid creating massive memory buffers or unexpectedly large outputs during the first post-correlation AFF4 run.

Benchmark for relaxing this:

- Convert all copy-out to streaming reads/writes with progress counters.
- Confirm no single-file memory allocation scales with the file's logical size.
- Confirm timeout/progress UI reflects active AFF4/APFS copy-out throughput.
- Add large-file test case with sparse regions and multi-extent layout.

## Next expected validation

Run the V1.0.4 AFF4/APFS probe against the same explicit AFF4 file and upload:

- `D:\Downloads\V1_0_4_build.log`
- `D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_4.zip`

Primary review metrics:

- target inode hits > 0
- target FILE_EXTENT rows > 0
- copied files > 0, or clear no-match statuses explaining why
- synthetic zero rows recorded only when sparse/zero regions are present
- staged Store-V2 groups/files > 0
- native parser probe artifacts > 0 if Store-V2 staging succeeds

## Source: `V1_0_29_BUILD_SCRIPT_AND_LOW_RISK_HARDENING.md`

# V1.0.29 Build Script and Low-Risk Hardening

## Purpose

V1.0.29 fixes the stale V1.0.28.2 build-wrapper version gate and applies low-risk hardening items from the current review without changing APFS traversal, Store-V2 parsing, iOS parsing, schema, or forensic interpretation.

## Implemented

- Build wrapper now expects `1.0.29`.
- Redirected child-process log handle is closed in the parent immediately after successful process creation.
- AFF4 dynamic probe uses `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`.
- Win32 review ListView redraw is suspended during bulk row insertion and restored after population.
- Dynamically globbed thin-upload export CSVs are capped at 50 MB.
- Standalone thin-upload PowerShell helper applies the same cap for export CSVs.

## Deferred

- APFS absolute path reverse walker.
- True APFS next-leaf iterator replacement.
- iOS NSKeyedArchiver unflattener.
- Evidence intake module relocation.
- Full Win32 GUI global-state encapsulation.
- Joined GUI export-thread lifecycle.

Those are still tracked as future work because they affect extraction, parser semantics, or GUI lifecycle and should not be combined with a build-wrapper/hardening release.

## Source: `V1_0_29_BUILD_HOTFIX.md`

# V1.0.28.2 Build Hotfix

V1.0.28.2 is a narrow linker hotfix after the V1.0.28.1 MSVC build failed with `LNK2005` for `vestigant::spotlight::isLikelyStoreV2GroupDirectoryName`.

## Root cause

The APFS diagnostic writer relocation copied a helper named `isLikelyStoreV2GroupDirectoryName()` into `src/parsers/apfs_diagnostic_exporter.cpp` while the original orchestrator helper remained in `src/app/app_runner.cpp`. The copied helper had external linkage, so MSVC linked two definitions of the same symbol.

## Fix

The exporter-side helper is now local to `apfs_diagnostic_exporter.cpp` by placing it in an anonymous namespace around that helper only. The existing `app_runner.cpp` helper remains unchanged for the dynamic AFF4/APFS probe logic.

## Scope boundaries

No APFS traversal, AFF4 reads, copy-out/staging behavior, Store-V2 parsing, iOS parsing, SQLite schema, GUI behavior, or forensic interpretation changed in this hotfix.

## Source: `V1_0_28_1_APFS_DIAGNOSTIC_EXPORTER_RELOCATION.md`

# V1.0.28.1 APFS Diagnostic Exporter Relocation

## Scope

V1.0.28.1 moves the main APFS/AFF4 diagnostic/report writer bodies out of `src/app/app_runner.cpp` and into `src/parsers/apfs_diagnostic_exporter.cpp`.

Moved writer families include:

- container superblock / checkpoint descriptor outputs
- volume superblock outputs
- resolved volume outputs
- volume root-tree lookup outputs
- root-tree node and traversal probe outputs
- filesystem namespace seed outputs
- Spotlight target/inode/xattr/file-extent/file-copy-out outputs
- Store-V2 copy-out versus staged-candidate comparison outputs
- extracted Store-V2 stage outputs
- checkpoint-map outputs

## Non-goals

V1.0.28.1 does not change:

- AFF4 dynamic reads
- APFS traversal
- APFS copy-out/staging decisions
- Store-V2 parser behavior
- iOS parsing
- SQLite schema
- GUI behavior

## Remaining work

The exact-file-signature scan and V1 diagnostic rerun-plan writers still remain in `app_runner.cpp`. They should be moved only after V1.0.28.1 builds and the macOS AFF4/APFS thin output validates.

## Source: `V1_0_27_PROCESS_AND_GUI_SQLITE_HARDENING.md`

# V1.0.27 Process and GUI SQLite Hardening

V1.0.27 is a narrow hardening release after V1.0.26.1 built successfully and the macOS AFF4/APFS thin ZIP was generated and reviewed.

## Evidence reviewed before patching

- `V1_0_26_1_build.log` showed the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26.1`.
- `Upload_Thin_MacOS_AFF4_V1_0_26_1.zip` was present and reviewed.
- The V1.0.26.1 thin ZIP did not contain the denied raw log/inventory names:
  - `aff4_stream_inventory_raw.txt`
  - `ios_focused_zip_extract.log`
  - `ios_focused_zip_extract_7z.log`
  - `ios_focused_zip_extract.ps1`
  - `ios_ffs_file_inventory.csv`
  - `image_file_inventory.csv`
- The V1.0.26.1 case/additional inventories used relative paths rather than full `Q:\`, `D:\`, or `T:\` paths.
- The AFF4/APFS run reached `complete_source_probe`.
- APFS staged Store-V2 parse baseline remained: `raw_records=25000`, `raw_key_values=2181`, `raw_date_candidates=25000`.
- External comparison summary remained: `external_file_count=4123`, `vestigant_file_count=8986`, `file_match_rows=2213`, `external_only_rows=1424`, `vestigant_only_rows=6710`, `hash_different_path_rows=431`, and `RELATIVE_PATH_SIZE_MISMATCH=486`.

## Implemented changes

1. Added Windows Job Object wrapping to hidden external process launches in `app_runner.cpp`.
   - `runShellCommandNoWindow()` now creates a kill-on-close Job Object when available.
   - redirected process execution also uses the same Job Object helper.
   - on timeout, `TerminateJobObject()` is used when a job was assigned; otherwise the parent process is terminated as fallback.
   - this is intended to prevent orphaned child processes from locking evidence files.

2. Added resilient SQLite busy retry handling for GUI review database connections.
   - `win32_gui.cpp` now installs a bounded custom `sqlite3_busy_handler` for `ReadOnlyDb` connections.
   - `gui_export_worker.cpp` now installs a matching portable busy handler for export-worker database connections.
   - temp-store/cache PRAGMAs remain unchanged.

3. Added a thin-upload ZIP deny-list self-check to `tools/Create-SourceProbeUploadZip.ps1`.
   - after `Compress-Archive`, the script opens the generated ZIP and fails if any denied raw filenames are present.

4. Updated continuity files:
   - `docs/CONTINUATION_HANDOFF.md`
   - `docs/ROADMAP_CHECKLIST.md`
   - `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`

## Intentionally deferred

- APFS absolute-path reverse catalog walker: deferred because the proposed implementation is pseudocode and would require validated APFS B-tree lookup/value parsing before being forensically safe.
- Evidence intake/staging extraction module: deferred because moving ZIP staging and iOS inventory import is broad and should not be combined with process/GUI hardening.
- iOS NSKeyedArchiver unflattener: deferred until a real bplist object model/UID parser is present; returning placeholder JSON would create misleading investigative output.
- APFS diagnostic writer relocation: still a good next target, but not combined with V1.0.27.

## Validation performed locally

- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp`
- `g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp`
- `g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp`

Windows/MSVC build and Windows GUI runtime validation are still required for V1.0.27.

## Source: `V1_0_26_THIN_UPLOAD_AND_IO_HARDENING.md`

# V1.0.26 Thin Upload and I/O Hardening

V1.0.26 is a narrow hardening release after the V1.0.25 Windows/MSVC build and macOS AFF4/APFS thin run were reviewed.

## Evidence reviewed

- `V1_0_25_build.log` showed a successful Windows/MSVC build and binary version check for `Vestigant Spotlight v1.0.25`.
- `Upload_Thin_MacOS_AFF4_V1_0_25.zip` reached `complete_source_probe` and preserved the V1.0.25 AFF4/APFS staged Store-V2 baseline.
- The V1.0.25 thin ZIP still contained `aff4_stream_inventory_raw.txt` through the standalone thin upload PowerShell tool path. That raw output remains useful locally but is not appropriate for the thin-upload package.

## Changes

- Added a deny-list policy for thin-upload helper paths:
  - `aff4_stream_inventory_raw.txt`
  - `ios_focused_zip_extract.log`
  - `ios_focused_zip_extract_7z.log`
  - `ios_focused_zip_extract.ps1`
  - `ios_ffs_file_inventory.csv`
  - `image_file_inventory.csv`
- Applied the same policy to the in-app upload-bundle copier and to `tools/Create-SourceProbeUploadZip.ps1`.
- Updated PowerShell-generated `case_file_inventory.txt` and `additional_output_file_inventory.txt` to use relative paths rather than full absolute paths.
- Added a bounded 12-hour wait for hidden Windows subprocess launches so a prompted or wedged external tool is terminated instead of hanging indefinitely.
- Updated exact AFF4 ZIP byte reads on Windows to use `_wfopen_s` plus `_fseeki64`, avoiding 32-bit-offset truncation risk in large AFF4/ZIP containers.

## Not changed

- APFS traversal and APFS Store-V2 staging are unchanged.
- Store-V2 parsing is unchanged.
- iOS CoreSpotlight parsing and app database parsing are unchanged.
- SQLite schema and GUI view definitions are unchanged.
- APFS diagnostic writer bodies were not moved in this version.

## Source: `V1_0_25_THIN_UPLOAD_SECURITY_AND_IOS_PERFORMANCE.md`

# V1.0.25 Thin Upload Security and iOS Performance Hardening

V1.0.25 is a narrow hardening build after the V1.0.24.1 Windows/MSVC build passed.

## Implemented

- Removed raw AFF4/iOS extraction tool logs and generated extraction helper scripts from the Thin Upload copy lists.
- Kept the high-level run logs in Thin Upload: `run_status.txt`, `last_stage.txt`, `run_progress.tsv`, `last_progress.tsv`, and `VestigantSpotlight.log`.
- Replaced hardcoded export CSV bundle lists with dynamic copying of regular `.csv` files directly under the `exports` folder, plus the existing `exports/upload_samples` directory.
- Replaced `countCsvDataRows` line-by-line string allocation with binary chunk newline counting.
- Reworked staged iOS app-database output path normalization to use `std::filesystem::path::lexically_normal()` plus per-path-component sanitization.
- Hardened Windows hidden process execution paths used for AFF4 stream inventory and ZIP PowerShell staging so they use `CreateProcessW` directly with inherited stdout/stderr log handles rather than `cmd.exe /C` shell redirection.

## Not changed

- APFS/AFF4 traversal, copy-out, staging, Store-V2 parsing, and lower-bound iterator behavior are unchanged.
- iOS CoreSpotlight parsing, app DB parser behavior, schema, and GUI views are unchanged.
- APFS diagnostic writer bodies remain in `app_runner.cpp`; this remains a future modularization target.
- The dynamic AFF4/APFS probe lambda structure remains unchanged.
- Case database open/close lifetime remains unchanged.

## Validation status

Local validation confirmed C++20 syntax for `src/app/app_runner.cpp`. Windows/MSVC build and runtime validation are still required.

## Source: `V1_0_25_GUI_VIEW_HELPERS_BUILD_HOTFIX.md`

# V1.0.24.1 GUI View Helpers Build Hotfix - Historical Note

This file is retained only as a historical note from the V1.0.24.1 build hotfix sequence. The current V1.0.25 scope is documented in `docs/V1_0_25_THIN_UPLOAD_SECURITY_AND_IOS_PERFORMANCE.md`.

## Source: `V1_0_23_APFS_DIAGNOSTIC_MODEL_HEADER.md`

# V1.0.23 APFS Diagnostic Model Header

V1.0.23 is a narrow source-maintainability release. It introduces a shared APFS/AFF4 diagnostic model header and moves the APFS diagnostic row/summary structs out of `src/app/app_runner.cpp`.

## Changed

- Added `src/parsers/apfs_diagnostic_models.h`.
- Moved APFS diagnostic row/summary structs into the shared header.
- Included the shared APFS diagnostic model header from `app_runner.cpp`.
- Updated versioned build/launch/AFF4 wrapper scripts for V1.0.23.

## Not changed

- macOS AFF4/APFS traversal or extraction logic.
- APFS copy-out/staging behavior.
- APFS lower-bound iterator behavior.
- iOS CoreSpotlight extraction or parsing.
- Apple/lzfse codec behavior.
- Store-V2 parser behavior.
- SQLite schema or GUI view definitions.

## Follow-up scope

After Windows/MSVC validation, the next safe APFS cleanup step is moving one small family of diagnostic CSV writer bodies from `app_runner.cpp` into `apfs_diagnostic_exporter.cpp` using the shared row-model header.

## Source: `V1_0_22_GUI_EXPORT_WORKER_MODULARIZATION.md`

# V1.0.22 GUI Export Worker Modularization

V1.0.22 continues the production-readiness cleanup after V1.0.21 confirmed the Windows GUI build hotfix.

## Implemented

- Moved current-page review export SQL/CSV backend execution out of `win32_gui.cpp` and into `GuiExportWorker`.
- Moved filtered-view review export SQL/CSV backend execution out of `win32_gui.cpp` and into `GuiExportWorker`.
- Added `GuiViewExportRequest` so the UI layer snapshots view, filter, sort, page, and checked-artifact state before launching a background export.
- Added a dedicated export-page completion message and guard so current-page export cannot be double-started.
- Kept Win32 dialog prompting and button enable/disable behavior in the GUI layer.
- Left checked/tagged export support in the existing worker backend.

## Non-goals

No extraction, Store-V2 parsing, APFS copy-out, Apple/lzfse, iOS CoreSpotlight, or database schema behavior was changed.

## Validation expectation

The GUI should remain responsive during Export Page, Export Filtered, Export Checked, and Export Tagged operations.

## Source: `V1_0_21_GUI_BUILD_HOTFIX.md`

# V1.0.22 GUI Build Hotfix

V1.0.20 intentionally moved export/database work out of `win32_gui.cpp`, but the small Win32 ListView rendering helpers for the Selected Row Details pane were accidentally removed from the GUI translation unit.

V1.0.22 restores only these local UI rendering helpers:

- `ensureDetailsListColumns()`
- `resizeDetailsListColumns()`
- `clearDetailsList()`
- `addDetailsListRow()`

These functions are not database/export business logic. They operate directly on the `gRowDetails` ListView handle and therefore remain appropriate in the GUI file until a future `MainWindow`/detail-pane class extraction.

No macOS AFF4/APFS extraction, iOS CoreSpotlight extraction, Apple/lzfse codec behavior, Store-V2 parsing, or database schema behavior changed.

## Source: `V1_0_20_GUI_EXPORT_AND_APFS_DIAGNOSTIC_MODULARIZATION.md`

# V1.0.20 GUI Export and APFS Diagnostic Modularization

## Purpose

V1.0.20 reduces long-term maintenance risk without changing evidence extraction behavior.

## GUI export worker

`win32_gui.cpp` previously owned UI event handling, SQLite export SQL, CSV formatting, support-file export, and status reporting for checked/tagged exports.  V1.0.20 moves the backend work for checked and tagged exports into `GuiExportWorker`.

The GUI still owns dialog prompts and status messages.  The worker owns read-only database access, CSV output, support CSV output, and support manifest creation.

## Threading behavior

Checked-artifact export and tagged-artifact export now run on detached background threads.  Completion is posted back to the UI thread through `WM_EXPORT_CHECKED_RESULT` and `WM_EXPORT_TAGGED_RESULT`.

This matches the prior filtered-export pattern and prevents large checked/tagged exports from freezing the Win32 message loop.

## APFS diagnostic export policy

`apfs_diagnostic_exporter.h/.cpp` now owns the policy for deciding whether heavy AFF4/APFS structural diagnostic CSVs are written.  Copy-out and staging evidence outputs remain outside this policy and continue to run in normal mode.

Full movement of APFS diagnostic CSV writer bodies is deferred until the associated row structs are moved out of `app_runner.cpp` into shared APFS diagnostic model headers.

## Validation checklist

- Build V1.0.20 under MSVC.
- Open the GUI.
- Export filtered view and confirm the GUI remains responsive.
- Export checked artifacts and confirm the GUI remains responsive.
- Export tagged artifacts and confirm the GUI remains responsive.
- Confirm generated support manifests and support CSVs are present.
- Confirm macOS/iOS extraction outputs are unchanged unless a separate extraction test is run.

## Source: `V1_0_18_MODULARIZATION_CLEANUP_PLAN.md`

# V1.0.18 Modularization and Cleanup Plan

## Current structure after V1.0.18

Current line counts from this package:

```text
src/codec/lzfse_codec.cpp           73
src/parsers/apfs_aff4_reader.cpp   242
src/parsers/apfs_volume_reader.cpp 289
src/gui/view_registry.cpp          323
src/parsers/ios_app_db_parser.cpp  898
src/gui/win32_gui.cpp             3301
src/app/app_runner.cpp           15083
```

The previous modularization work created real module boundaries, but `app_runner.cpp` and `win32_gui.cpp` are still too large. The highest-risk future work should move logic out by capability, not by broad mechanical deletion.

## Production cleanup priority

### 1. APFS/AFF4 module migration

Move from `app_runner.cpp` into `src/parsers/apfs_aff4_reader.cpp` and `src/parsers/apfs_volume_reader.cpp`:

- APFS file-copy row construction.
- Direct indexed FILE_EXTENT chain assembly.
- decmpfs xattr/resource-fork lookup and reconstruction.
- lower-bound directory iterator comparator and promotion logic.
- APFS copy-out status classification.

Acceptance benchmark:

- `app_runner.cpp` no longer constructs APFS copy-out rows directly.
- Existing V1.0.16/V1.0.18 AFF4/APFS thin metrics do not regress.
- External compare output still runs automatically.

### 2. macOS Store-V2 investigation module

Create a small module for macOS-specific investigator summaries:

```text
src/investigate/macos_spotlight_views.cpp
src/investigate/macos_spotlight_views.h
```

Move high-level field interpretation and confidence labeling out of SQL/GUI-only code.

Acceptance benchmark:

- macOS views show field provenance and confidence.
- Date/use/source interpretations are traceable to Store-V2 field/key/value provenance.

### 3. GUI review database helper

Create:

```text
src/gui/review_db_helper.cpp
src/gui/review_db_helper.h
```

Move raw SQL out of `win32_gui.cpp` for:

- view page loading,
- filtered export,
- checked artifact persistence,
- tag read/write,
- table/column introspection.

Acceptance benchmark:

- `win32_gui.cpp` stops constructing most raw SQL strings.
- view registry remains the single source for table/column metadata.
- iOS/macOS tab separation continues to use `ViewPlatform`.

### 4. GUI thread/query lifecycle manager

Create:

```text
src/gui/review_query_manager.cpp
src/gui/review_query_manager.h
```

Encapsulate request sequence IDs, cancellation, join, and worker result handoff.

Acceptance benchmark:

- rapid view switching does not overlap stale query writes into the active list view.
- SQLite busy errors remain recoverable and visible.

### 5. iOS parser row sink

`ios_app_db_parser.cpp` now owns most row parsing, but the parser interface should still be made more independent from app-runner insertion details.

Acceptance benchmark:

- Apple Messages, WhatsApp, and KnowledgeC parser output can be unit-tested without opening a full case database.
- `app_runner.cpp` only enumerates candidate databases and passes a row sink.

## Cleanup policy

- Keep V1 build/run scripts for current supported workflows.
- Do not reintroduce V0.9 one-off scripts into production packages.
- Keep support/diagnostic APFS CSVs opt-in, except copy-out/stage/parser/external-compare outputs required for investigator validation.
- Do not delete diagnostic paths until an equivalent support-mode workflow remains available.

## Source: `V1_0_18_LZFSE_LZVN_OPTIONAL_VENDOR_INTEGRATION.md`

# V1.0.18 LZFSE/LZVN Optional Vendor Integration

## Source decision

V1.0.18 recognizes Apple/lzfse as the vetted codec source for future APFS decmpfs LZFSE/LZVN reconstruction. The project does not fetch source during normal builds; it requires the source to be explicitly vendored under:

```text
third_party/lzfse/
```

This keeps forensic builds reproducible and reviewable.

## Build behavior

If `third_party/lzfse/src/lzfse.h` and the expected decoder C files are present, both CMake and `build_windows_msvc.bat` define:

```text
VESTIGANT_HAS_LZFSE=1
```

and compile the Apple decoder sources into the project.

If the source is absent, the code still builds. LZFSE/LZVN decode attempts return:

```text
LZFSE_CODEC_NOT_COMPILED
```

and emit no reconstructed bytes.

## Vendor helper

Use:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\Prepare-LzfseThirdParty.ps1 -LzfseZip D:\Downloads\lzfse-master.zip -ExpectedSha256 <sha256>
```

The helper unpacks the source tree, verifies required files, and writes a vendor manifest. The `-ExpectedSha256` parameter should be used for final forensic builds.

## Runtime integration point

The existing APFS decmpfs resource-fork reconstruction path now calls `decodeAppleLzfseOrLzvnChunk()` for compression types 8 and 12 when the codec is compiled in. Decode failures are explicit and non-fatal to the process; they are recorded as skipped/failed reconstruction statuses.

## Remaining benchmark before production claim

Before claiming complete LZFSE/LZVN support, run a codec-enabled build against known-good compressed/decompressed vectors and against the external Spotlight reference comparison. The required evidence is:

- codec source SHA-256/vendor manifest;
- MSVC and Linux build pass;
- LZFSE vector pass;
- LZVN vector pass;
- decmpfs inline/resource-fork decode tests;
- AFF4/APFS external comparison hash parity improvement.

## Source: `V1_0_18_GUI_EXPORT_AND_MODULARIZATION_CLEANUP.md`

# V1.0.18 GUI Export and Modularization Cleanup

## Purpose

V1.0.18 addresses the V1.0.17 review notes that focused on incomplete modularization and Win32 GUI responsiveness.

## Verified structure

The reviewed V1.0.17 tree did not contain a duplicate `ViewSpec` or duplicate `views()` implementation in `src/gui/win32_gui.cpp`. The only production definitions are in `src/gui/view_registry.h` and `src/gui/view_registry.cpp`.

The reviewed V1.0.17 tree also did not contain local copies of the specialized Apple Messages, WhatsApp, KnowledgeC, or generic iOS app database row parser bodies in `src/app/app_runner.cpp`; those bodies were already in `src/parsers/ios_app_db_parser.cpp`.

## Implemented cleanup

- Added an `IosAppDbParser` class facade in `src/parsers/ios_app_db_parser.h/.cpp`.
- Updated `parseIosAppDatabaseRecordInventories()` to call `IosAppDbParser::buildTableParseDecision()`, `IosAppDbParser::isTargetRecordCategory()`, and `IosAppDbParser::parseTable()`.
- Kept the existing free-function parser API as a compatibility layer during the transition.
- Converted `exportFilteredView()` to a background worker thread so large CSV exports do not run on the Win32 message loop thread.
- Added `WM_EXPORT_FILTERED_RESULT` completion handling to marshal export status back to the UI thread.
- Added a single-export guard so the user cannot start multiple filtered exports at the same time.
- Snapshot checked artifact IDs before starting the worker to avoid reading mutable UI state from the background export thread.
- Added `IncludeStructuralDiagnostics` to `Create-SourceProbeUploadZip.ps1` so normal thin uploads omit heavy structural APFS CSVs unless the wrapper is run with diagnostic output enabled.

## Deferred cleanup

- Move remaining SQLite review/export/tag logic into a future `ReviewDatabaseHelper` module.
- Move review-page background query lifecycle into a future `ReviewQueryManager` module.
- Continue reducing `app_runner.cpp` by moving AFF4/APFS copy-out/staging implementation into APFS parser/orchestration modules.
- Run the lower-bound APFS B-tree iterator as a comparator before replacing the current live extraction path.

## Source: `V1_0_18_DIRECT_LOGICAL_SIZE_TRIM.md`

# V1.0.18 Direct AFF4/APFS Logical-Size Trim

## Problem observed in V1.0.15

The V1.0.15 AFF4/APFS copy-out path successfully staged thousands of Store-V2-related files, but the external comparison still showed many `RELATIVE_PATH_SIZE_MISMATCH` rows.

Review of representative mismatch diagnostics showed a repeatable pattern:

- copy-out status was successful;
- staging source validation matched the copied extent-chain output;
- the staged Vestigant file was larger than the external reference file by a small block-aligned amount;
- the copy-out logical size source was `direct_indexed_file_extent_end`.

This indicates that direct APFS copy-out sometimes wrote the allocated/file-extent end rather than the file's logical data-stream size.

## V1.0.18 change

For direct indexed APFS copy-out rows, V1.0.18 now prefers inode-derived logical size when available:

1. `INO_EXT_TYPE_DSTREAM.size.direct_index`
2. `j_inode_val.uncompressed_size.direct_index`
3. fallback to `direct_indexed_file_extent_end`

The copy loop is bounded by that logical size. If the final file is shorter than the extent-chain end because it was trimmed to the inode data-stream size, the validation status records that provenance instead of treating it as an unexplained mismatch.

## Forensic handling

This change does not hide uncertainty. The row keeps:

- `logical_size_bytes`
- `logical_size_source`
- copy-out status
- validation status
- SHA-256 of the emitted bytes
- synthetic-zero provenance when used

The intended benchmark is a reduction in `RELATIVE_PATH_SIZE_MISMATCH` rows, especially for Store-V2 component files whose V1.0.15 mismatch was caused by `direct_indexed_file_extent_end` output.

## Source: `V1_0_18_APPLE_LZFSE_VENDOR_ENABLEMENT.md`

# V1.0.18 Apple/lzfse Vendor Enablement

## Source accepted for this build

The project now vendors the uploaded Apple/lzfse source package under:

```text
third_party/lzfse/
```

Vendor manifest:

```text
third_party/lzfse/VESTIGANT_VENDOR_MANIFEST.txt
```

Recorded ZIP hash:

```text
23855f54ff38ff2f679f79730d20df970dcd3f6cd5ad33505fcdc4220b3ab158  lzfse-master.zip
```

The repository reviewed for source provenance is:

```text
https://github.com/lzfse/lzfse
```

Observed latest commit from GitHub connector review:

```text
8ca039302ee20ae9ee39d2d00ab0d6f652352a10  Add const to tables in lzfse_internal.h
```

## Build behavior

When `third_party/lzfse/src/lzfse.h` exists, both the CMake and no-CMake MSVC build paths define:

```text
VESTIGANT_HAS_LZFSE=1
```

The build compiles these Apple/lzfse decoder sources into the common object set:

```text
third_party/lzfse/src/lzfse_decode.c
third_party/lzfse/src/lzfse_decode_base.c
third_party/lzfse/src/lzfse_fse.c
third_party/lzfse/src/lzvn_decode_base.c
```

## Runtime behavior

The AFF4/APFS decmpfs resource-fork reconstruction path now uses the Apple decoder adapter for compression types:

```text
8  LZVN_RSRC
12 LZFSE_RSRC
```

Successful rows are expected to use statuses such as:

```text
COPIED_DECOMPFS_RESOURCE_FORK_LZVN
COPIED_DECOMPFS_RESOURCE_FORK_LZFSE
```

Failures remain explicit. The tool must not silently emit uncertain decompressed data. Examples:

```text
DECOMPFS_LZFSE_LZVN_DECODE_FAILED
LZFSE_EXPECTED_OUTPUT_SIZE_UNSAFE
LZFSE_DECODE_OUTPUT_EXCEEDED_EXPECTED_CHUNK_SIZE
```

## Validation benchmark for the next AFF4 run

The next thin upload should be checked for:

1. `aff4_apfs_spotlight_file_copy_out_summary.json` reports `APPLE_LZFSE_REFERENCE_CODEC_ENABLED`.
2. LZVN/LZFSE resource-fork rows appear where compressed cache files are present.
3. `RELATIVE_PATH_SIZE_MISMATCH` and `EXTERNAL_ONLY` cache-file counts fall if the mismatches were caused by compressed resource forks.
4. The external comparison should move from size-only mismatch toward relative-path hash matches for decmpfs cache files.
5. Any failed decode must remain visible in CSV/summary output with inode/file/relative-path provenance.

## Source: `V1_0_15_STOREV2_DUAL_PROCESS_COMPARE.md`

# V1.0.15 Store-V2 Candidate Dual-Process Compare

## Purpose

V1.0.15 adds a deterministic support output that compares two AFF4/APFS Store-V2 processing stages:

1. the raw APFS copy-out candidate set (`aff4_apfs_spotlight_file_copy_out.csv`), and
2. the normalized investigator-facing Store-V2 staging set (`ExtractedSpotlight/StagedStoreV2` and `aff4_apfs_extracted_storev2_stage_files.csv`).

This is not yet a replacement for the live extraction path. It is a parity and regression guard designed to catch duplicate APFS candidates, candidate scoring drift, synthetic-zero provenance, decmpfs/resource-fork reconstruction rows, and cases where the staged row differs from the best scored copy-out row for the same Store-V2 component.

## New outputs

- `aff4_apfs_storev2_candidate_dual_process_compare.csv`
- `aff4_apfs_storev2_candidate_dual_process_compare_summary.json`
- `AFF4_APFS_STOREV2_CANDIDATE_DUAL_PROCESS_COMPARE.md`

## Key statuses

- `STAGED_SELECTED_BEST_COPYOUT_CANDIDATE`: normalized staging chose the same row as the best-scored copy-out candidate.
- `STAGED_SELECTED_BEST_COPYOUT_SEQUENCE`: normalized staging chose a row with the same source sequence as the best candidate.
- `STAGED_ROW_DIFFERS_FROM_BEST_COPYOUT_CANDIDATE`: staging selected a different row than the highest-scored candidate for the same Store-V2 key.
- `BEST_COPYOUT_CANDIDATE_NOT_STAGED`: a copy-out candidate existed but did not enter normalized staging.
- `NO_COPIED_OR_STAGED_ROW`: no usable row existed for that key.

## Why this is useful

V1.0.14 staged 8,986 files and parsed 25,000 raw Store-V2 records, but external comparison still showed many size/path mismatches. The new compare output gives a deterministic way to review whether mismatches are caused by copy-out, staging selection, duplicate candidate collisions, or external reference differences.

## Source: `V1_0_15_PRODUCTION_MODULARIZATION_REVIEW.md`

# V1.0.15 Production Modularization Review

## Implemented

- Preserved the V1.0.14 iOS parser modularization: Apple Messages, WhatsApp, KnowledgeC, and generic iOS app DB row parsing are delegated through `iosAppDbParseTable(...)`.
- Preserved the centralized GUI view registry; `ViewSpec` and `views()` remain owned by `src/gui/view_registry.*`.
- Kept AFF4/APFS structural diagnostics suppressed by default while keeping copy-out, normalized staging, parser, enrichment, and external comparison outputs active.
- Added a Store-V2 dual-process candidate compare to audit raw copy-out candidate rows against normalized staged rows.

## Not implemented

- The live AFF4/APFS traversal was not replaced with the lower-bound iterator in this iteration. V1.0.14 already staged files and parsed records; replacing the live path before comparator evidence would risk regression.
- LZFSE/LZVN decompression was not enabled because no vetted codec implementation and known-good vectors were present in the provided repository inputs.

## Next benchmarks

- Run V1.0.15 and review `aff4_apfs_storev2_candidate_dual_process_compare_summary.json`.
- Prioritize any `STAGED_ROW_DIFFERS_FROM_BEST_COPYOUT_CANDIDATE` rows.
- Compare summary counts against `aff4_apfs_external_spotlight_compare_summary.json`.
- Implement the lower-bound APFS iterator as a diagnostic comparator only after candidate selection is stable.

## Source: `V1_0_15_LZFSE_LZVN_SOURCE_REVIEW.md`

# V1.0.15 LZFSE/LZVN Source Review

## Finding

The uploaded Apple File System Reference is an authoritative source for APFS on-disk structures, including container/volume mount flow, object maps, file-system record types, directory records, extended attributes, inode records, file extents, data streams, sparse-file indicators, and resource forks.

The uploaded APFS slide deck identifies transparent file compression, `com.apple.decmpfs`, inline compressed data for small files, and `com.apple.ResourceFork` for larger compressed content. The live code already records decmpfs/resource-fork status and recognizes known compression type labels.

## What was not added

V1.0.15 does not add production LZFSE/LZVN decompression. The reason is narrow: the APFS filesystem documentation is sufficient to justify decoding the structures that point to compressed data, but it is not by itself a codec implementation and it does not provide known-good decode vectors for this toolchain.

## Required benchmark before enabling codec output

LZFSE/LZVN should be enabled when all of the following are present in the repository:

1. a vendored or system-linked vetted codec implementation with license notes;
2. MSVC and Linux build integration;
3. known-good LZFSE and LZVN compressed/decompressed test vectors;
4. tests for decmpfs inline data and resource-fork backed data;
5. output statuses that distinguish fully decoded, skipped unsupported codec, partial resource fork, sparse/gap, and read failure.

Until that benchmark is met, V1.0.15 keeps the forensic-safe behavior: detect and record decmpfs/LZFSE/LZVN provenance, but do not emit plausible reconstructed files from an unverified codec path.

## Source: `V1_0_15_COPYOUT_SOURCE_ISOLATION.md`

# V1.0.15 Copy-Out Source Isolation

## Issue

V1.0.12 successfully copied APFS Store-V2 candidate files and staged 884 normalized Store-V2 component rows, but the external comparison still showed many `RELATIVE_PATH_SIZE_MISMATCH` rows. Review of the thin upload showed a provenance mismatch: `aff4_apfs_extracted_storev2_stage_files.csv` could report a large selected copy-out row while the actual normalized staged file at the same relative path was only 4096 bytes.

The root cause was raw APFS copy-out path collision. Multiple APFS candidate rows with the same component name were written into `ExtractedSpotlight/StagedStoreV2/Ungrouped/<component>`. Later duplicate rows could overwrite the file that an earlier, better-scored staging row referenced.

## Fix

V1.0.15 writes raw APFS copy-out rows to unique immutable per-target folders under:

`ExtractedSpotlight/ApfsCopyOutByTarget/seq_<target_sequence>_fid_<child_file_id>_parent_<parent_object_id>_<group>/...`

The normalized Store-V2 stage still writes investigator-facing files under:

`ExtractedSpotlight/StagedStoreV2/<group>/...`

This keeps copy-out provenance separate from normalized Store-V2 staging and prevents same-name duplicate rows from overwriting the source selected by the staging scorer.

## Expected benchmark

After V1.0.15, `aff4_apfs_extracted_storev2_stage_files.csv`, the actual files under `ExtractedSpotlight/StagedStoreV2`, and `aff4_apfs_external_spotlight_vestigant_manifest.csv` should agree on staged file sizes/hashes for the same relative path. The number of `RELATIVE_PATH_SIZE_MISMATCH` rows should fall, especially for BADA95B6 Store-V2 components such as `0.indexArrays`.

## Source: `V1_0_12_STRUCTURAL_CLEANUP_AND_APFS_DIAGNOSTIC_IO_REVIEW.md`

# V1.0.12 Structural Cleanup and APFS Diagnostic-I/O Review

## Purpose

V1.0.12 continues the V1 modularization work while preserving the currently successful AFF4/APFS staged Store-V2 extraction path. The goal is to reduce monolithic coupling without rewriting the working copy-out path in the same iteration.

## Review findings

- `src/gui/win32_gui.cpp` no longer defines `ViewSpec` or `views()`. Those definitions are owned by `src/gui/view_registry.h/.cpp`, so the suspected GUI ODR duplicate was not present in the reviewed V1.0.11 tree.
- `src/app/app_runner.cpp` still owns the specialized Apple Messages, WhatsApp, and KnowledgeC row emitters. Full migration is delayed until a parser-independent row sink is added.
- Normal AFF4/APFS source-probe runs were still writing many structural diagnostic CSVs. This was the highest-impact safe performance fix for this iteration.

## Implemented

- Added `RunOptions::aff4ApfsDiagnosticOutputs` and CLI flag `--aff4-apfs-diagnostic-outputs` / `--diagnostic-apfs-csvs`.
- Standard V1.0.12 AFF4/APFS wrapper runs no longer pass `--verbose` by default.
- Heavy structural AFF4/APFS CSV outputs are now gated behind `--aff4-apfs-diagnostic-outputs`, `--verbose`, or `--diagnostic-full-native-db`.
- Copy-out, staging, native Store-V2 parser probe, enrichment samples, and external comparison outputs remain enabled in normal source-probe mode.
- The APFS remaining-mismatch diagnostics tool now treats `aff4_apfs_spotlight_xattr_probe.csv` as optional so normal-mode suppressed diagnostic CSVs do not block external comparison.
- Removed low-risk duplicated iOS parser wrapper functions from `app_runner.cpp` and routed KnowledgeC snippet creation through `ios_app_db_parser`.
- Upgraded `ApfsVolumeReader::enumerateDirectory()` from a placeholder to a callback-driven lower-bound iterator that can be tested independently and later bound to the live AFF4/APFS block reader.

## Delayed

- Full migration of Apple Messages, WhatsApp, KnowledgeC, and generic table row emission is delayed until a parser-independent row sink exists.
- Live AFF4/APFS traversal is not replaced by `ApfsVolumeReader` yet. The benchmark before replacement is iterator output parity with the current staged Store-V2 output.
- LZFSE/LZVN remains delayed pending vetted source, MSVC/Linux integration, and known-good vectors.

## Operator usage

Normal faster source-probe:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_12\scripts\Run-V1_0_12-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

Full diagnostic source-probe:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_12\scripts\Run-V1_0_12-macOS-AFF4-Probe-AndZip.ps1 -CleanOut -DiagnosticOutputs
```

## Source: `V1_0_10_GUI_VIEW_REGISTRY_REFACTOR.md`

# V1.0.11 GUI View Registry Refactor

## Purpose

V1.0.11 begins reducing `src/gui/win32_gui.cpp` God-file risk without changing the investigator workflow. The highest-risk GUI issue was macOS/iOS view routing based on display-name substring checks inside the GUI file. That routing is now centralized in a dedicated view registry module.

## Implemented

- Added `src/gui/view_registry.h` and `src/gui/view_registry.cpp`.
- Moved `ViewSpec` and the full `views()` registry out of `win32_gui.cpp`.
- Added `ViewPlatform` enum: `MacOS`, `iOS`, `Shared`, `Auto`.
- Added platform inference and sort-priority assignment in the view registry, not in tab-routing code.
- Updated `win32_gui.cpp` tab filtering to use `ViewSpec::platform` rather than raw display-name substring checks.
- Moved `viewHelpText()` out of `win32_gui.cpp` into the view registry module.
- Added `view_registry.cpp` to the CMake Windows GUI target and no-CMake MSVC GUI link path.

## Also implemented

- Updated the AFF4/APFS wrapper so nonzero source-probe exits attempt to create a `_FAILED.zip` partial diagnostic upload before throwing. This addresses the V1.0.9 failure mode where the wrapper stopped before producing an upload bundle.

## Deferred

The following recommendations remain staged for later iterations:

1. `MainWindow` class extraction for Win32 handles and state. Benchmark: no global HWND or review-state variables remain in `win32_gui.cpp`, and `staticWndProc` dispatches through a window-owned instance.
2. `ReviewDatabaseHelper` extraction. Benchmark: export, tag, checked-artifact, and paged-query SQL no longer lives in `win32_gui.cpp`.
3. Review query lifecycle manager. Benchmark: all background review queries are owned by a single manager object with request cancellation, join, and SQLite progress-handler state.
4. Full APFS lower-bound iterator promotion. Benchmark: iterator output for `/.Spotlight-V100/Store-V2` matches or improves the current logical directory walk and staged Store-V2 output.

## Forensic note

V1.0.11 is intended as a low-risk structural refactor. It does not change AFF4/APFS extraction logic except the wrapper's failure packaging behavior.

## Source: `SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10_1.md`

# Source Docs/Scripts Review — V1_1_10_1

Reviewed current documentation and PowerShell wrappers after the user's command-block correction request.

## Result

- Current build command block now matches the requested pattern: set location, hash ZIP, remove prior T:\ extraction, expand ZIP to T:\, and run the versioned build wrapper.
- Current AFF4/APFS thin command now uses the versioned `Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut` wrapper.
- New-chat continuation guide now includes the same command blocks.
- No ambiguous files were removed.

## Source: `SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.md`

# V1.1.10 Source Documentation / Text / PowerShell Review

Reviewed every `.md`, `.txt`, and `.ps1` file present in the V1.1.10 source package after using V1.1.9.1 as the base.

## Summary

- Reviewed files: 143
- Removed only clearly obsolete root-level package artifacts/manifests and stale per-package source-review inventory files.
- Preserved append-only version history files and historical validation notes.
- Preserved support scripts unless they were versioned active wrappers superseded by regenerated V1.1.10 wrappers.
- Detailed inventory: `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv`.

## Removed as clearly unnecessary active-package clutter

- `V1_1_9_DELETED_FILES_MANIFEST.md`
- `V1_1_9_patch_manifest.txt`
- `V1_1_9_patch_manifest.txt.sha256`
- `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.md`
- `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.csv`

## Items deliberately not removed without approval

- Historical `docs/V1_*` implementation notes: useful for audit/history, but could be archived later.
- Historical `validation/V1_*` logs/notes: useful validation trail, but could be consolidated later.
- GitHub setup/project scripts: useful if the repository/project automation is still used; removal requires confirmation.
- Legacy/source-staging helper tools for iOS, AFF4, ZIP, and upload packaging: retained because they support current or diagnostic workflows.

## CSV source-review inventory: `SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10_1.csv`

```csv
path,action,notes
BUILD_INSTRUCTIONS.md,updated,Full extract/build command block corrected
docs/NEW_CHAT_CONTINUATION_GUIDE.md,updated,New-chat continuation commands corrected
docs/QUICK_START.md,updated,Quick start commands corrected
HELP.md,updated,Current commands corrected
```

## CSV source-review inventory: `SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.csv`

```csv
relative_path,bytes,decision,reason
.github/pull_request_template.md,518,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
BUILD_INSTRUCTIONS.md,2393,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
CMakeLists.txt,2990,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
CONSOLIDATED_VERSION_HISTORY.md,8296,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
HELP.md,2130,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
KNOWN_ISSUES.md,6140,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
RELEASE_NOTES.md,5318,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
VERSION.txt,7,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
VERSION_HISTORY.md,133066,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/BUILD_NOTES.md,6066,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/BaselineVersionHistory.md,133066,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/CONSOLIDATED_USER_MANUAL.md,14714,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/CONSOLIDATED_VERSION_HISTORY.md,9486,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/CONTINUATION_HANDOFF.md,12259,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md,12171,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/EVIDENCE_SOURCE_STAGING_ROADMAP.md,2457,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/EVIDENCE_SOURCE_STAGING_WORKFLOW.md,2323,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/FILESYSTEM_INVENTORY_SQLITE_PLAN.md,2323,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/FULL_VERSION_HISTORY.md,133066,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/GITHUB_PROJECT_SETUP.md,970,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/IOS_CORESPOTLIGHT_ROADMAP.md,18332,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/MACOS_INVESTIGATIVE_FEATURES_CURRENT_AND_ROADMAP.md,3313,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/NEW_CHAT_CONTINUATION_GUIDE.md,3967,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/PACKAGE_CLEANUP_SUMMARY.md,8926,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/PROJECT_ROADMAP_AND_CONTINUATION.md,13864,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/QUICK_START.md,2043,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/README.md,7328,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/ROADMAP_CHECKLIST.md,14607,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/SOURCE_PACKAGE_CLEANUP_POLICY.md,1055,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/SUGGESTIONS_AND_FIXES_TRACKER.md,25058,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/THIN_UPLOAD_REVIEW_WORKFLOW.md,1783,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/TROUBLESHOOTING.md,2476,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/USER_MANUAL.md,1322,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/V1_0_10_GUI_VIEW_REGISTRY_REFACTOR.md,2171,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_12_STRUCTURAL_CLEANUP_AND_APFS_DIAGNOSTIC_IO_REVIEW.md,2753,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_15_COPYOUT_SOURCE_ISOLATION.md,1674,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_15_LZFSE_LZVN_SOURCE_REVIEW.md,1698,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_15_PRODUCTION_MODULARIZATION_REVIEW.md,1407,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_15_STOREV2_DUAL_PROCESS_COMPARE.md,1862,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_18_APPLE_LZFSE_VENDOR_ENABLEMENT.md,2200,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_18_DIRECT_LOGICAL_SIZE_TRIM.md,1692,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_18_GUI_EXPORT_AND_MODULARIZATION_CLEANUP.md,2168,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_18_LZFSE_LZVN_OPTIONAL_VENDOR_INTEGRATION.md,1910,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_18_MODULARIZATION_CLEANUP_PLAN.md,3354,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_20_GUI_EXPORT_AND_APFS_DIAGNOSTIC_MODULARIZATION.md,1813,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_21_GUI_BUILD_HOTFIX.md,774,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_22_GUI_EXPORT_WORKER_MODULARIZATION.md,1081,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_23_APFS_DIAGNOSTIC_MODEL_HEADER.md,1018,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_25_GUI_VIEW_HELPERS_BUILD_HOTFIX.md,254,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_25_THIN_UPLOAD_SECURITY_AND_IOS_PERFORMANCE.md,1689,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_26_THIN_UPLOAD_AND_IO_HARDENING.md,1799,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_27_PROCESS_AND_GUI_SQLITE_HARDENING.md,3431,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_28_1_APFS_DIAGNOSTIC_EXPORTER_RELOCATION.md,1060,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_29_BUILD_HOTFIX.md,939,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_29_BUILD_SCRIPT_AND_LOW_RISK_HARDENING.md,1237,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_2_AFF4_APFS_EXTRACTION_AND_CLEANUP.md,5744,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_30_IOS_DB_AND_GUI_EXPORT_LIFECYCLE.md,1693,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_31_EVIDENCE_INTAKE_HELPERS_AND_IOS_IMPORT_PERFORMANCE.md,1046,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_4_AFF4_APFS_REVIEW_AND_LIMIT_CLEANUP.md,2965,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_6_APFS_BTREE_ITERATOR_AND_DIRECT_COPYOUT.md,1864,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_0_7_APFS_MODULE_REFACTOR.md,3412,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_0_1_PACKAGING_HOTFIX.md,512,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_0_ORCHESTRATOR_MODULARIZATION_AND_DB_LIFETIME.md,1471,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_1_EVIDENCE_INTAKE_AND_GUI_INGEST_THREAD.md,1489,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_2_CANCELLATION_DLL_AND_BPLIST_HARDENING.md,1592,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_3_EXPORT_CANCEL_AND_PURGE_HARDENING.md,935,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_4_BPLIST_AND_GUI_STATE_HARDENING.md,763,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_5_1_BUILD_HOTFIX.md,365,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_5_AFF4_CANCEL_AND_UPLOAD_GUARDS.md,1574,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_6_1_AFF4_WORKER_WINDOWS_BUILD_HOTFIX.md,668,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_6_AFF4_PROBE_WORKER_DIRECT_MAP_SPLIT.md,987,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_7_1_BUILD_HOTFIX_AND_PACKAGE_CLEANUP.md,994,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_7_AFF4_PROBE_WORKER_DYNAMIC_RELOCATION.md,1146,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_8_WINDOWS_PATH_AND_HISTORY_BASELINE.md,920,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/V1_1_9_LIVE_APFS_LEAF_TRAVERSAL_AND_SOURCE_REVIEW.md,1325,KEEP,Version-scoped design/review note retained as historical implementation context.
docs/VALIDATION_HISTORY.md,7431,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/VALIDATION_STATUS.md,6580,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/VERSION_HISTORY_APPEND_ONLY_POLICY.md,948,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
docs/WORKFLOW_LEDGER.md,13312,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
scripts/Build-V1_1_10.ps1,2091,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/Create-GitHubLabels.ps1,1157,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/Create-GitHubProjectIssues.ps1,2700,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/Initialize-GitHubRepo.ps1,1018,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/Launch-V1_1_10-GUI.ps1,604,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/New-ReleaseBranch.ps1,260,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/Package-GitHubValidationBundle.ps1,1208,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/Package-V1_1_10-macOS-AFF4-ThinFromExistingCase.ps1,1826,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/Run-Local-WindowsBuild.ps1,400,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/Run-V1_1_10-macOS-AFF4-Probe-AndZip.ps1,1770,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/Run-V1_1_10-macOS-AFF4-Probe-AndZip.txt,769,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
scripts/Sync-Version-To-GitRepo.ps1,1853,KEEP,Current or support PowerShell workflow retained; no ambiguous script deletion without user approval.
third_party/lzfse/CMakeLists.txt,4231,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
third_party/lzfse/README.md,2582,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
third_party/lzfse/VESTIGANT_VENDOR_MANIFEST.txt,500,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Build-Aff4CppLite-VS2022.ps1,19842,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Collect-iOSCoreSpotlightQuickDiagnostics.ps1,15428,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Compare-ExternalSpotlightReference.ps1,22329,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Create-ApfsRemainingMismatchDiagnostics.ps1,9717,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Create-SourceProbeUploadZip.ps1,21309,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Create-UploadZip.ps1,6241,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Extract-IosCoreSpotlightFromZips.ps1,3739,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Extract-iOSCoreSpotlightFromFFSZips.ps1,3862,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Prepare-LzfseThirdParty.ps1,2040,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Probe-Aff4DirectMapScan.ps1,5041,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Run-IosCoreSpotlightFocusedZip.ps1,2975,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Run-SingleAff4SourceProbeAndZip.ps1,27806,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Run-iOSCoreSpotlightFocusedAndZip.ps1,2045,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Search-SpotlightKeywordExports.ps1,6609,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
tools/Stage-EvidenceSource.ps1,15022,KEEP,"Current source-package documentation/script needed for build, workflow, validation history, source tooling, or append-only project context."
validation/V1_0_10_validation_summary.txt,767,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_12_validation_summary.txt,1220,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_15_validation_summary.txt,1351,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_18_validation_summary.txt,1303,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_20_validation_summary.txt,645,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_21_validation_summary.txt,868,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_22_validation_summary.txt,552,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_23_validation_notes.md,2037,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_25_patch_manifest.txt,1615,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_25_validation_notes.md,2252,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_26_1_validation_notes.md,1671,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_26_patch_manifest.txt,1169,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_26_validation_notes.md,1378,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_27_validation_notes.md,1449,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_29_validation_notes.md,2025,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_30_validation_notes.md,3361,KEEP,Historical validation note/log retained as audit trail; not active runtime input.
validation/V1_0_31_validation_notes.md,953,KEEP,Histo

[TRUNCATED]

```
