## V1.0.13

## V1.0.13

- Added opt-in AFF4/APFS structural diagnostic CSV output mode.
- Normal AFF4/APFS source-probe runs now suppress heavy structural APFS diagnostic CSVs while keeping copy-out, staging, parser, enrichment, and external-comparison outputs.
- Added `--aff4-apfs-diagnostic-outputs` / `--diagnostic-apfs-csvs` for full support runs.
- Added callback-driven `ApfsVolumeReader::enumerateDirectory()` lower-bound iterator implementation for isolated APFS directory walk testing.
- Removed low-risk duplicated iOS parser wrapper functions from `app_runner.cpp`.
- Confirmed GUI view registry ownership remains centralized in `view_registry`.
- Updated wrapper validation so suppressed diagnostics do not block normal external comparison.

Delayed:
- Full iOS row parser migration awaits parser-independent row sink.
- Live APFS traversal replacement awaits iterator parity benchmarks.
- LZFSE/LZVN remains pending vetted codec and test vectors.

## V1.0.13

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

V1.0.7: Added dedicated APFS module boundary and fixed direct AFF4/APFS copy-status/staging classification.

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

# Validation Status

## V0_9_60 - Windows GUI forward-declaration compile hotfix

V0_9_60 is a focused Windows/MSVC GUI build hotfix after V0_9_56 reached the GUI compile stage and failed with `C3861: setReviewSummary identifier not found` in `src\gui\win32_gui.cpp`. The fix adds a forward declaration for `setReviewSummary(const std::wstring&)` before the custom view-set helper functions that call it. No parser, ingest, cache, ZIP, FFS inventory, app DB, export, or forensic interpretation behavior was intentionally changed.

Current version: 0.9.60

## V0_9_60 - Windows MSVC batch-label build hotfix

V0_9_60 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55 local validation

Passed in this environment:

- Static balance check for `src/gui/win32_gui.cpp`.
- CMake configure.
- Linux CMake build for core library, CLI, and tests.
- `VestigantSpotlightTests` self-test.
- Verified removed V1-blocker GUI controls remain absent from GUI source.
- Verified legacy V7 importer source/build references remain absent.

Not validated in this environment:

- Windows/MSVC GUI compile and link.
- Runtime GUI logo/layout behavior.
- Runtime custom view set save/hide/move/reset behavior.
- Runtime tag-management repair on an existing case database.
- Runtime processing telemetry during ingest.

## Expected Windows validation

- Build banner reports source version `0.9.55`.
- Binary reports `Vestigant Spotlight v0.9.55`.
- Case Information tab shows the logo/header and cleaner processing layout.
- Tags button opens Tags / Notes and tags can be created/applied/removed.
- Custom view sets persist for macOS and iOS tabs.
- Bottom Case Information log shows elapsed-time processing status during runs.

## V0_9_60 - Staged AFF4/raw source restoration and MB telemetry

V0_9_60 restores AFF4/APFS image and Raw IMG/DD image choices in the GUI source-type selector as staged image workflows. Folder and ZIP remain the production intake paths; AFF4/APFS is kept visible for macOS forensic images, and Raw IMG/DD is kept visible for raw/external media such as exFAT devices attached to Macs that may contain Spotlight indexes. Processing telemetry now displays throughput in decimal MB and MB/s rather than MiB/MiB/s for investigator readability. No parser interpretation logic was intentionally changed.
