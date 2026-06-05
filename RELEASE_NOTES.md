## V1.0.11

- Continued APFS/AFF4 modularization without changing the already-working Store-V2 staging pipeline from V1.0.8.
- Moved APFS B-tree table-of-contents key/value decoding into `src/parsers/apfs_aff4_reader.*` and left `app_runner.cpp` with thin compatibility wrappers.
- Updated iOS app database table processing to use `IosAppDbTableParseDecision` from `src/parsers/ios_app_db_parser.*` for parser routing, reducing app-runner-local classification branching.
- Kept full iOS row parsers in `app_runner.cpp` for now because they still depend on local SQLite row-binding and timestamp helper state.
- Corrected stale AFF4 run-status wording: AFF4/APFS is no longer reported as unimplemented; it is described as an active guarded staged pipeline.
- Preserved diagnostic CSVs because V1.0.8 still needs support outputs for external-compare mismatch analysis and before promoting AFF4/APFS to ordinary `discoverStores()` ingest.
- Did not add LZFSE/LZVN; this remains blocked on vetted source, MSVC/Linux integration, and test vectors.

Validation performed in the coding environment:

- CMake configure: PASS.
- New parser modules compiled before timeout: PASS.
- Full Linux build reached `app_runner.cpp` and timed out due to compilation time; no compile error was observed before timeout.
- Windows/MSVC build still requires validation on the Windows system.

## V1.0.8

- Added `src/parsers/ios_app_db_parser.h/.cpp` for iOS app database table classification, special-parser routing, and KnowledgeC snippet assembly.
- Added `src/parsers/apfs_aff4_reader.h/.cpp` with a callback-driven APFS lower-bound directory-iterator scaffold and directory-record decoder.
- Added parser module smoke tests and build integration for CMake and MSVC no-CMake builds.
- Preserved V1.0.7 live AFF4/APFS copy-out behavior; live traversal replacement is delayed until iterator parity can be benchmarked.


## V1.0.7

- Added a dedicated `src/parsers/apfs_volume_reader.*` module boundary for future APFS lower-bound B-tree iterator work.
- Added APFS key, Store-V2 component, copy-status, and staging path helper functions with smoke-test coverage.
- Updated MSVC/CMake builds to include the APFS module.
- Corrected AFF4/APFS direct copy-out counting and staging classification for `COPIED_DIRECT_INDEXED_EXTENT_CHAIN` and `COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS`.
- Preserved V1.0.6 AFF4/APFS extraction behavior while reducing the risk of adding the full B-tree iterator directly into `app_runner.cpp`.

## V1.0.6 - MSVC preview-status helper scope hotfix

V1.0.6 fixes a Windows/MSVC compile-scope issue introduced in V1.0.5. The direct AFF4/APFS copy-out code referenced `directPreviewStatusForBytes()` before the helper was visible to MSVC. The helper is now available at file scope before both the guarded/indexed and direct AFF4/APFS copy-out paths. No AFF4/APFS runtime behavior was intentionally changed beyond making the V1.0.5 target-index/copy-out work compile on Windows.

## V1.0.5 - APFS target-index correlation and guarded direct Store-V2 copy-out attempt

V1.0.5 responds to the V1.0.4 AFF4/APFS result where APFS root-tree traversal completed without node-limit skips and found Store-V2 namespace candidates, but still staged zero Store-V2 files. It adds a direct APFS filesystem-tree index during the exhausted B-tree traversal, correlates Store-V2 target child IDs to indexed INODE and FILE_EXTENT records, writes a logical directory-walk report, and attempts guarded direct copy-out when usable extents are found. Sparse logical gaps and zero physical extents are written only with explicit synthetic-zero provenance and SHA-256 hashing. The primary application binary no longer contains a self-test route or fake evidence-generation path.

Delayed items: a true lower-bound APFS directory iterator is still targeted for a dedicated APFS module; V1.0.5 uses an exhausted indexed traversal bridge. LZFSE/LZVN remains delayed until vetted codec source and cross-platform test vectors are added. Diagnostic CSV suppression for normal production mode remains delayed until AFF4/APFS extraction is promoted from source-probe to normal StoreInfo discovery.

## V1.0.4 - AFF4/APFS direct traversal limit cleanup and Store-V2 namespace seeding

V1.0.4 fixes the stale build-script version check from V1.0.3 and cleans up the direct AFF4/APFS traversal behavior. The direct APFS root-tree scan now terminates by queue exhaustion and visited-node cycle protection instead of the prior node/record/depth hard caps. It also records direct directory entries independent from the bounded upload name-sample CSV and uses those entries to recursively seed Store-V2 child copy-attempt rows with group and APFS path context. Full target-guided INODE/FILE_EXTENT copy-out is deferred to V1.0.5 and should be moved out of `app_runner.cpp` into a dedicated APFS lookup module before implementation.

# Release Notes

## V1.0.4 - AFF4/APFS target correlation and production cleanup

V1.0.4 continues the macOS AFF4/APFS transition from diagnostic probe toward extraction. It removes the named hard traversal/copy caps from the Spotlight filesystem-tree target scan, adds scanned-INODE target correlation, records APFS absolute path context for Store-V2 attempts, adds explicit sparse/zero-region copy-out provenance, removes the app-runner self-test/fake case data, removes deprecated GUI native-decoding toggles, and purges stale V0.9 production-package artifacts.

Delayed items: vetted LZFSE/LZVN integration, full `discoverStores()` promotion, and diagnostic-view separation are documented with benchmarks in `docs/V1_0_4_AFF4_APFS_EXTRACTION_AND_CLEANUP.md`.

# Vestigant Spotlight Release Notes

## V1.0.1 - AFF4/APFS direct filesystem-tree target scan

V1.0.1 is a focused macOS AFF4/APFS diagnostic follow-up to V1.0.0. The V1.0.0 run confirmed that the direct AFF4 map reader reached APFS container metadata, checkpoints, volume superblocks, volume object maps, and root-tree lookup rows, but did not stage Store-V2 because the APFS filesystem namespace scan was still too shallow.

Changes:

- Added a bounded direct AFF4/APFS filesystem-tree target scan starting from resolved volume root-tree objects.
- Resolves non-root APFS B-tree child nodes through each volume OMAP where possible.
- Prioritizes likely Data volumes during target scanning.
- Records namespace-name samples and Spotlight target hits for `.Spotlight-V100` / `Store-V2` style paths.
- Always writes explicit target scan, inode probe, xattr probe, file-extent probe, copy-out, and staging output files, even when no target is found, so the upload bundle clearly distinguishes no-hit/incomplete states from missing-output packaging errors.
- Keeps actual file copy-out gated until inode, xattr, dstream, file extent, sparse/gap, zero-block, decmpfs, and resource-fork provenance can be recorded defensibly.

Validation summary:

- Linux CMake configure/build passed.
- `VestigantSpotlightTests` self-test passed.
- CLI version check reports `Vestigant Spotlight v1.0.1`.
- Static raw-string/long-line check passed for MSVC C2026 risk.
- Windows/MSVC build, GUI runtime, and live AFF4/APFS target scan still require validation on the Windows evidence workstation.


# Vestigant Spotlight V1.0.0 Notes

V1.0.0 starts the post-0.9.x V1 line. It keeps the stable V0_9_60 GUI/iOS baseline and moves the next work back toward macOS Spotlight investigation from AFF4/APFS forensic images.

## What changed in V1.0.0

- Version identifiers were advanced to `1.0.0` in `VERSION`, `VERSION.txt`, `CMakeLists.txt`, and `src/core/app_info.cpp`.
- Added a V1 AFF4/APFS diagnostic rerun artifact set written during AFF4 source-probe runs:
  - `AFF4_APFS_V1_DIAGNOSTIC_RERUN_PLAN.md`
  - `aff4_apfs_v1_diagnostic_checklist.csv`
  - `aff4_apfs_v1_diagnostic_plan_summary.json`
- Added those V1 AFF4/APFS diagnostic files to the thin-upload review index and upload bundle copy list.
- Updated the single-AFF4 probe wrapper defaults for the V1.0.0 case/output names.
- Added concrete V1.0.0 PowerShell scripts:
  - `scripts/Build-V1_0_0.ps1`
  - `scripts/Launch-V1_0_0-GUI.ps1`
  - `scripts/Run-V1_0_0-macOS-AFF4-Probe-AndZip.ps1`
  - copy/paste command text files for GUI and AFF4/APFS testing.
- Kept AFF4/APFS and Raw IMG/DD source-type paths visible as staged/experimental workflows rather than hiding them.

## V1.0.0 decision rule

Do not make broad APFS reconstruction changes from the old V0_8_69 external-compare bundle alone. First run a fresh V1.0.0 strict single-AFF4 diagnostic against:

`O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`

Then review the new thin upload to classify the next fix as one of:

- AFF4 container/virtual read access,
- APFS checkpoint or object-map resolution,
- APFS filesystem root-tree namespace traversal,
- Spotlight target inode/xattr/extent correlation,
- sparse/gap/zero-physical-block reconstruction policy,
- decmpfs/resource-fork handling,
- staged Store-V2 parsing/enrichment,
- external-reference comparison,
- macOS investigator views.

## Validation performed in this package

- Reviewed uploaded `V0_9_60_build.log`; it shows CLI, tests, and GUI linked successfully and the built binary reported `Vestigant Spotlight v0.9.60`.
- Reviewed the old `Upload_Thin_V0_8_69_ExternalCompare.zip` enough to confirm historical AFF4/APFS extraction reached staged Store-V2 parsing/enrichment and external comparison, but it is old and should not be treated as current V1 truth.
- Performed a Linux CMake build and ran the included self-test for V1.0.0 in this environment.
- Performed static checks for common MSVC C2026 raw-string risk.

Windows/MSVC GUI build and the live AFF4 image probe still require execution on the user's Windows machine with access to the standard O:, Q:, T:, and D: paths.

---

# Vestigant Spotlight V0_9_60 Notes

V0_9_60 is a V1 production-readiness cleanup after V0_9_57 compiled and ran on Windows. It improves the processing workflow and review workflow without changing parser interpretation logic.

Key changes:
- The Case Information bottom log is now the live processing log. It clears at run start, timestamps messages, mirrors run/progress status, and emits periodic heartbeat messages while processing continues.
- View loading now shows an explicit marquee progress indicator above the investigation grid and a loading message in the details pane so long SQLite view loads are not mistaken for hangs.
- The V1 GUI source selector now exposes only fully implemented Folder and ZIP intake paths. AFF4/APFS and raw image support remain roadmap items and are not presented as clickable V1 options.
- Legacy V7-only schema tables/indexes were removed from new case initialization.
- CLI/operator self-test mode is deprecated; the automated test executable uses an internal automated self-test path.
- Duplicate AFF4/APFS child/descendant root-tree probe output writers were consolidated into one traversal-output writer.

Validation summary:
- Linux CMake configure/build passed.
- VestigantSpotlightTests passed.
- C++20 syntax checks passed for modified non-Windows translation units.
- Windows/MSVC GUI compile and runtime validation remain required.

# Vestigant Spotlight Release Notes

Current version: 0.9.60

## V0_9_60 - Windows MSVC batch-label build hotfix

V0_9_60 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55 - V1 GUI polish, custom view sets, processing telemetry, and tag-management repair

V0_9_55 is a GUI-focused V1 readiness release built on the stable V0_9_54 parser/intake baseline. It does not intentionally change parser, cache, ZIP staging, FFS inventory, or app database classification behavior.

Changes:

- Added Vestigant branding to the Case Information workflow using the supplied green Vestigant logo.
- Cleaned the Case Information / Build Processing layout to reduce visual clutter and make the workflow more investigator-focused.
- Reused the bottom Case Information log area as the live processing log/status pane.
- Added processing telemetry messages for elapsed processing time and estimated throughput when a measurable file source size is available.
- Added live elapsed-time status updates while processing runs.
- Added Custom view set support for both MacOS and iOS investigation tabs.
- Added view-set controls: Move Up, Move Down, Hide, Save Set, and Reset Set.
- Custom view sets are stored in the case database through `review_view_preferences`.
- Added a visible Tags button on the investigation toolbar to switch directly to the Tags / Notes workflow.
- Added existing-case GUI schema repair for tag tables, checked-row persistence, notes, and view preferences. This is intended to fix older cases where Manage Tags / tag actions were unavailable because the GUI interaction tables did not yet exist.
- Retained the V0_9_54 V1 production cleanup: Skip preservation, Self-test, Max records, Blocks/store, and legacy V7 GUI/CLI paths remain removed.
- Retained the two-column selected-row details table and investigation-tab-only visibility behavior.

Validation summary:

- Linux CMake configure/build passed for core library, CLI, and tests.
- `VestigantSpotlightTests` self-test passed.
- Static balance check passed for `src/gui/win32_gui.cpp`.
- Confirmed removed V1-blocker GUI controls and V7 importer references remain absent.
- Windows/MSVC GUI build and runtime GUI validation are still required.

## V0_9_60 - Staged AFF4/raw source restoration and MB telemetry

V0_9_60 restores AFF4/APFS image and Raw IMG/DD image choices in the GUI source-type selector as staged image workflows. Folder and ZIP remain the production intake paths; AFF4/APFS is kept visible for macOS forensic images, and Raw IMG/DD is kept visible for raw/external media such as exFAT devices attached to Macs that may contain Spotlight indexes. Processing telemetry now displays throughput in decimal MB and MB/s rather than MiB/MiB/s for investigator readability. No parser interpretation logic was intentionally changed.
