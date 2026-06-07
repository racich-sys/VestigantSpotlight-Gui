## V1_1_9

- Promoted guarded live AFF4/APFS OMAP horizontal leaf traversal for APFS OMAP target lookups and volume root-tree lookups.
- Added bounded next-leaf transition handling with cycle detection, cancellation checks, and transition limits.
- Reviewed every `.md`, `.txt`, and `.ps1` source-package file and recorded decisions in `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.*`.
- Removed obsolete root-level prior-version package manifests while preserving append-only version history under `docs/`.
- No Store-V2 parser schema, iOS parser behavior, or GUI view semantics were intentionally changed.

# Baseline Version History

## V1_1_8

- Uses the user-provided `BaselineVersionHistory.md` as the append-only baseline version history going forward.
- Adds Windows long-path support helpers and routes APFS/AFF4 Store-V2 evidence copy-out/reconstruction writes through long-path-capable file writing on Windows.
- Forces a WAL checkpoint/truncate before upload packaging and changes `CaseDatabase::close()` to request `SQLITE_CHECKPOINT_TRUNCATE` rather than `FULL`.
- Adds mutex protection to `Logger` file/message writes for concurrent GUI/export/ingest logging paths.
- Tightens APFS decmpfs expected-output safety cap from 512 MiB to 256 MiB.
- Preserves V1.1.7.1 AFF4 probe-worker architecture; both dynamic and direct AFF4/APFS probe bodies remain outside `app_runner.cpp`.
- No live APFS traversal replacement, iOS parser semantic change, Store-V2 schema change, or GUI view behavior change.


## V1_1_7_1

- Build hotfix for V1.1.7 after the AFF4 dynamic probe relocation: moved/exposed missing worker-local helpers required by `src/parsers/aff4_probe_worker.cpp`.
- Package cleanup: removed obsolete version-specific scripts and root-level old package artifacts/manifests while preserving append-only history in `docs/`.
- Preserved both large AFF4/APFS probe bodies outside `app_runner.cpp`.
- Added append-only full version history baseline and new-chat continuation guide.
- Added/updated new-chat continuation guide, source-package layout notes, and workflow ledger instructions.
- Added worker-local helper boundary for known blocking AFF4 layout detection, reader tool resolution, and Win32 error reporting.
- Cleaned active source package by removing obsolete version-specific scripts and root-level old package manifests.

## V1_1_7

- Moved the AFF4/libaff4 dynamic-load APFS probe body out of `src/app/app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp` as `Aff4ProbeWorker::executeDynamicLoadProbe(...)`.
- `app_runner.cpp` now delegates both AFF4/APFS probe paths to `Aff4ProbeWorker`, removing the remaining `writeAff4CppLiteDynamicLoadProbe(...)` implementation from the orchestrator.
- Added cancellation propagation into shared APFS OMAP traversal helper calls so direct-map and dynamic-load probe paths can observe investigator cancellation during APFS B-tree walks.
- No live APFS interpretation, copy-out/staging rules, Store-V2 parsing, iOS parsing, or SQLite schema changes.

## V1_1_6_1

- Build hotfix for V1.1.6 after the direct-map probe worker split.
- Corrected V1.1.6.1 build-wrapper version check.
- Added missing Windows-only `wideProcessPath(...)` helper to `src/parsers/aff4_probe_worker.cpp`.
- Windows/MSVC build and macOS AFF4/APFS thin output were later reviewed as passing; V1.1.6.1 became the current stable baseline before V1.1.7 work.

## V1_1_6

- Added `src/parsers/aff4_probe_worker.h` and wired it into CMake/MSVC build lists.
- Moved the direct-map AFF4/APFS probe body from `src/app/app_runner.cpp` into the new `src/parsers/aff4_probe_worker.cpp` module.
- Updated app runner call sites to delegate direct-map probe execution through `Aff4ProbeWorker::executeDirectMapReaderProbe(...)`.
- Left the libaff4 dynamic-load probe in `app_runner.cpp` pending a larger dependency split; this is explicitly tracked rather than hidden.

## V1_1_5_1

- Changed focused iOS 7-Zip extraction log redirection to UTF-8 `Out-File`.
- Added case-directory writability preflight before normal logging/database setup.
- Added thin-upload size/policy guard for `exports/upload_samples` in C++ and PowerShell packagers.
- Wrapped APFS staged Store-V2 diagnostic sample exports in localized error handling.
- Propagated ingest cancellation into guarded AFF4 dynamic/direct probe entry points and selected expensive bounded loops.

## V1_1_5

- Changed focused iOS 7-Zip extraction log redirection to UTF-8 `Out-File`.
- Added case-directory writability preflight before normal logging/database setup.
- Added thin-upload size/policy guard for `exports/upload_samples` in C++ and PowerShell packagers.
- Wrapped APFS staged Store-V2 diagnostic sample exports in localized error handling.
- Propagated ingest cancellation into guarded AFF4 dynamic/direct probe entry points and selected expensive bounded loops.

## V1_1_4

- Repeat-cycle hardening release after V1.1.3 validation.
- Added safer GUI checked-artifact snapshot helpers for export/page-load requests.
- Updated workflow ledger, roadmap, and suggestions tracker for the next AFF4/APFS monolith and comparator work.
- Added bplist offset-table/top-object-offset metadata to existing bounded bplist context summaries without claiming full NSKeyedArchiver graph decoding.
- Strengthened GUI ingest double-click protection with an atomic compare/exchange gate.

## V1_1_3

- Repeat-cycle hardening: export cancellation callbacks, orphan purge transaction, secure RichEdit load, APFS iterator scaffolding update, and workflow ledger updates.

## V1_1_2

- Added repeat-cycle workflow ledger.
- Added native parser bulk SQLite PRAGMAs with restoration.
- Added bounded bplist trailer validation metadata to bplist/NSKeyedArchiver context output.
- Added GUI ingest cancellation token/control and runApplication safe cancellation checkpoints.
- Hardened AFF4 dependent DLL search.
- Freed GUI logo bitmap during shutdown.

## V1_1_1

- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- Moved cache-SQLite iOS inventory helpers and referenced-path lookup import into `src/ingest/evidence_intake.cpp`.
- Moved iOS inventory import orchestration from `app_runner.cpp` into `EvidenceIntake::importIosInventoryCsvs(...)`.
- Added `EvidenceIntake::importReferencedIosPathLookupFromReuseCache(...)` and preserved run-status reporting through callback injection.
- No APFS live traversal replacement, AFF4 read semantic change, Store-V2 parser change, SQLite schema change, or new forensic interpretation output was intentionally added.
- Cleared the V1.1.0.1 `apfs_aff4_reader.cpp` C4100 warning without changing behavior.
- Replaced detached GUI main ingest worker with a tracked `gIngestThread` that is joined during `WM_DESTROY`.

## V1_1_0_1

- Moved APFS NX superblock parsing into `src/parsers/apfs_volume_reader.cpp/.h`.
- Moved `writeAff4ApfsV1DiagnosticRerunPlan()` into `src/parsers/apfs_diagnostic_exporter.cpp/.h`.
- Preserved V1.0.31 evidence-intake helper module, iOS CSV fallback PRAGMAs, and GUI LIKE PRAGMA behavior.
- Moved APFS decmpfs/resource-fork reconstruction and bounded zlib/deflate helpers into `src/codec/lzfse_codec.cpp/.h`.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker to reflect the broader `repeat` workflow.
- Added non-live APFS path/next-leaf helper API scaffolding in `ApfsVolumeReader` for future comparator work; live extraction was not changed.
- Moved AFF4 stream inventory classification/reporting into `src/parsers/apfs_aff4_reader.cpp/.h` with callback-injected tool lookup and process execution.
- No APFS live traversal replacement, AFF4 read semantic change, Store-V2 parser change, SQLite schema change, or new forensic interpretation output was intentionally added.
- Opened `CaseDatabase` once in `runApplication()` and reused that handle through AFF4/raw and general workflow.

## V1_1_0

- Moved APFS NX superblock parsing into `src/parsers/apfs_volume_reader.cpp/.h`.
- Moved `writeAff4ApfsV1DiagnosticRerunPlan()` into `src/parsers/apfs_diagnostic_exporter.cpp/.h`.
- Preserved V1.0.31 evidence-intake helper module, iOS CSV fallback PRAGMAs, and GUI LIKE PRAGMA behavior.
- Moved APFS decmpfs/resource-fork reconstruction and bounded zlib/deflate helpers into `src/codec/lzfse_codec.cpp/.h`.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker to reflect the broader `repeat` workflow.
- Added non-live APFS path/next-leaf helper API scaffolding in `ApfsVolumeReader` for future comparator work; live extraction was not changed.
- Moved AFF4 stream inventory classification/reporting into `src/parsers/apfs_aff4_reader.cpp/.h` with callback-injected tool lookup and process execution.
- No APFS live traversal replacement, AFF4 read semantic change, Store-V2 parser change, SQLite schema change, or new forensic interpretation output was intentionally added.
- Opened `CaseDatabase` once in `runApplication()` and reused that handle through AFF4/raw and general workflow.

## V1_0_31

- Added `src/ingest/evidence_intake.h/.cpp` as a behavior-preserving intake helper module boundary.
- Documented the user `repeat` shorthand and updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- Added temporary SQLite PRAGMAs around regenerable iOS CSV fallback import, with WAL/NORMAL restore after import or rollback.
- No APFS traversal, AFF4 read, Store-V2 parser, SQLite schema, or forensic interpretation behavior was intentionally changed.
- Added `PRAGMA case_sensitive_like=OFF` to GUI review/export read connections while preserving current broad search semantics.
- Moved CSV row counting, iOS ZIP path normalization, app database staging path sanitization, and iOS app database hint/category helpers out of `app_runner.cpp`.

## V1_0_30

- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- Moved iOS app database record-inventory orchestration into `IosAppDbParser::parseRecordInventories(...)`.
- Added GUI export thread registry and joined active export workers during `WM_DESTROY` instead of detaching Export Page/Filtered/Checked/Tagged workers.
- Reviewed V1.0.29 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Reduced `app_runner.cpp` iOS app DB inventory function to a delegating wrapper with status callback preservation.
- No AFF4/APFS traversal, copy-out, Store-V2 parsing, iOS CoreSpotlight schema, or forensic interpretation changes.

## V1_0_29

- V1.0.29 is a narrow stability and hardening release after V1.0.28.2 successfully linked binaries but the versioned PowerShell build wrapper still checked for `1.0.27`.
- No AFF4 read semantics changed.
- Updated continuation, roadmap, and suggestions/fixes tracking files.
- No APFS reverse-path walker or NSKeyedArchiver unflattener was added.
- Added the same 50 MB export CSV cap to the standalone thin-upload PowerShell helper.
- Added a 50 MB cap for dynamically globbed thin-upload export CSVs in the C++ upload bundler.
- Corrected versioned build/launch/run scripts for V1.0.29 so the post-build CLI version check expects `1.0.29`.
- Suspended Win32 ListView redraw during bulk row population to reduce GUI freezes on large review pages.
- Closed the parent process copy of redirected subprocess log handles immediately after successful `CreateProcessW`.

## V1_0_28_2

- Updated continuation, roadmap, and suggestions/fixes tracking docs.
- Preserved the existing runner helper used by dynamic AFF4/APFS probe code.
- Scoped the APFS diagnostic exporter copy of `isLikelyStoreV2GroupDirectoryName()` to the exporter translation unit.
- V1.0.28.2 is a narrow build/link hotfix for V1.0.28.1. It fixes the MSVC `LNK2005` duplicate symbol for `isLikelyStoreV2GroupDirectoryName` after APFS diagnostic writer relocation.
- No AFF4 read changes.
- No iOS parser changes.
- No GUI behavior changes.
- No SQLite schema changes.
- No APFS traversal changes.

## V1_0_28_1

- V1.0.28.1 is a modularization release. It moves the main APFS/AFF4 diagnostic/report writer bodies out of the app runner and into the APFS diagnostic exporter module.
- Updated continuity files.
- Moved main APFS diagnostic/report writer functions into `src/parsers/apfs_diagnostic_exporter.cpp`.
- Expanded `src/parsers/apfs_diagnostic_exporter.h` with declarations for the moved writer functions.
- Local Linux C++20 syntax checks were run for changed translation units. Windows/MSVC validation is required.
- Reduced `src/app/app_runner.cpp` by approximately 1,800 lines.
- No iOS parser changes.
- No GUI behavior changes.
- No SQLite schema changes.

## V1_0_28

- V1.0.28 is a modularization release. It moves the main APFS/AFF4 diagnostic/report writer bodies out of the app runner and into the APFS diagnostic exporter module.
- Updated continuity files.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- Moved main APFS diagnostic/report writer functions into `src/parsers/apfs_diagnostic_exporter.cpp`.
- Local Linux C++20 syntax checks were run for changed translation units. Windows/MSVC validation is required.
- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.
- Kept APFS traversal, Store-V2 parsing, iOS parsing, SQLite schema, GUI behavior, and live extraction behavior unchanged.
- Moved the main APFS/AFF4 diagnostic writer families from `src/app/app_runner.cpp` into `src/parsers/apfs_diagnostic_exporter.cpp`.

## V1_0_27

- V1.0.27 is a narrow hardening release after V1.0.26.1 built successfully and the macOS AFF4/APFS thin ZIP was generated and reviewed.
- Updated continuity files.
- temp-store/cache PRAGMAs remain unchanged.
- Added resilient SQLite busy retry handling for GUI review database connections.
- Added a thin-upload ZIP deny-list self-check to `tools/Create-SourceProbeUploadZip.ps1`.
- Added Windows Job Object wrapping to hidden external process launches in `app_runner.cpp`.
- The AFF4/APFS run reached `complete_source_probe`.
- `Upload_Thin_MacOS_AFF4_V1_0_26_1.zip` was present and reviewed.
- redirected process execution also uses the same Job Object helper.

## V1_0_26_1

- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.
- Added `scripts/Package-V1_0_26_1-macOS-AFF4-ThinFromExistingCase.ps1` for packaging an already-completed V1.0.26 AFF4/APFS case without rerunning the probe.
- Fixed `tools/Create-SourceProbeUploadZip.ps1` so `Get-RelativePathForThinInventory` no longer uses `[char]'\\'`, which Windows PowerShell treats as a two-character string and rejects.
- Reused the robust relative-path helper for `ExtractedSpotlight` copy paths.
- Reviewed the uploaded V1.0.26 build log; the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.
- Reviewed the user-reported wrapper output showing the AFF4/APFS probe and external comparison completed before packaging failed.
- V1.0.26.1 is a thin-upload packaging hotfix after the V1.0.26 AFF4/APFS run and external comparison completed but the thin ZIP failed during PowerShell relative-path inventory generation.

## V1_0_26

- Updated exact AFF4/ZIP byte reads on Windows to use 64-bit `_fseeki64`.
- Updated thin-upload inventory text files to report relative paths instead of full local paths.
- Added matching in-app thin-upload deny-list policy for raw AFF4/iOS tool logs and full file-inventory CSVs.
- Added bounded hidden Windows subprocess waits to avoid indefinite hangs from prompted/wedged external tools.
- Fixed the remaining thin-upload raw-log leak in the standalone source-probe upload tool by denying raw tool outputs and full raw file inventories.
- No APFS traversal, Store-V2 parsing, iOS parsing, GUI schema, or APFS diagnostic writer movement is included in this version.

## V1_0_25

- Changed `countCsvDataRows()` from line-by-line `std::getline()` allocation to binary chunk newline counting.
- Removed raw AFF4/iOS extraction tool logs and generated extraction helper scripts from Thin Upload copy lists.
- Added direct Windows `CreateProcessW` helpers with stdout/stderr log redirection for selected hidden tool/script execution paths.
- Changed staged iOS app-database output path normalization to use `std::filesystem::path::lexically_normal()` plus per-component sanitization.
- Apple/lzfse codec behavior.
- APFS diagnostic writer locations.
- GUI export worker behavior from V1.0.24.1.
- Kept the existing `exports/upload_samples` recursive copy.
- iOS CoreSpotlight parser behavior, app DB parser behavior, schema, or GUI views.

## V1_0_24_1

- Windows/MSVC build hotfix for V1.0.24 GUI view helper modularization.
- Updated versioned PowerShell build/launch/AFF4 wrapper scripts for V1.0.24.1.
- Confirmed the local wrapper was removed and only the shared helper implementation remains.
- Removed the stale anonymous-namespace `buildWhere(const ViewSpec&, const std::string&)` wrapper from `src/gui/win32_gui.cpp`.
- Updated the review-page SQL assembly path to explicitly call `vestigant::spotlight::buildWhere(v, search, capturedFilterColumn, capturedFilterValue)`.
- SQLite schema.
- Apple/lzfse codec behavior.
- APFS diagnostic writer locations.
- iOS CoreSpotlight extraction/parsing or iOS GUI view behavior.

## V1_0_24

- Updated V1.0.24 build/launch/AFF4 wrapper scripts.
- Updated versioned PowerShell build/launch/AFF4 wrapper scripts for V1.0.24.
- Added `src/parsers/apfs_diagnostic_models.h` as the shared APFS/AFF4 diagnostic row-model header.
- Moved APFS diagnostic row/summary structs out of `src/app/app_runner.cpp` without changing extraction, Store-V2 parsing, iOS parsing, GUI view behavior, Apple/lzfse behavior, or schema.
- SQLite schema.
- Apple/lzfse codec behavior.
- GUI export worker behavior from V1.0.22.
- APFS/AFF4 diagnostic model modularization.
- Static source review of the V1.0.22 baseline.

## V1_0_23

- Updated V1.0.23 build/launch/AFF4 wrapper scripts.
- Updated versioned PowerShell build/launch/AFF4 wrapper scripts for V1.0.23.
- Added `src/parsers/apfs_diagnostic_models.h` as the shared APFS/AFF4 diagnostic row-model header.
- Moved APFS diagnostic row/summary structs out of `src/app/app_runner.cpp` without changing extraction, Store-V2 parsing, iOS parsing, GUI view behavior, Apple/lzfse behavior, or schema.
- SQLite schema.
- Apple/lzfse codec behavior.
- GUI export worker behavior from V1.0.22.
- APFS/AFF4 diagnostic model modularization.
- Static source review of the V1.0.22 baseline.

## V1_0_22

- Added `WM_EXPORT_PAGE_RESULT` and a current-page export guard.
- Moved filtered-view export backend work from `win32_gui.cpp` into `GuiExportWorker`.
- Added `GuiViewExportRequest` to snapshot review view/export parameters before background export.
- SQLite schema.
- Store-V2 parser behavior.
- macOS AFF4/APFS extraction.
- Apple/lzfse codec behavior.
- iOS CoreSpotlight extraction.
- GUI export modularization and responsiveness cleanup.

## V1_0_20

- Added `src/gui/gui_export_worker.h/.cpp`.
- Added `WM_EXPORT_CHECKED_RESULT` and `WM_EXPORT_TAGGED_RESULT` completion handling.
- Added single-export guards for checked/tagged exports to prevent overlapping exports.
- Added `src/parsers/apfs_diagnostic_exporter.h/.cpp` as the first APFS diagnostic-export policy module.
- Moved checked-artifact and tagged-artifact CSV/support-file export logic out of `win32_gui.cpp` and into `GuiExportWorker`.
- Fixed synthetic APFS directory iterator name handling so null-terminated APFS directory record names stop at `NUL` rather than becoming a trailing underscore in the module smoke test.
- Converted checked-artifact export to a background worker thread with UI-thread completion posting.
- Centralized the AFF4/APFS structural-diagnostic gating expression in `shouldWriteAff4ApfsStructuralDiagnostics(...)`.
- Optimized `normalizeDetailValueForTable()` by reserving output capacity and using `push_back()` rather than repeated string concatenation.

## V1_0_19

- Removed clipped splitter text/control artifact above Selected Row Details.
- GUI-only iOS review hotfix.
- Keeps checkbox-click detail focus on the clicked row.
- Increased separation between the main result grid and detail pane.
- Populates Selected Row Details immediately for the first selected/clicked row.
- Stabilized View Set combo box visibility/z-order after resize in investigation tabs.
- No intended extraction, parser, codec, schema, or Store-V2 pipeline changes from V1.0.18.

## V1_0_18

- V1.0.18 is a cleanup and GUI-responsiveness release on top of the Apple/lzfse-enabled V1.0.17 baseline.
- Added a single-export guard for filtered exports.
- Added `WM_EXPORT_FILTERED_RESULT` handling to return export status to the UI thread.
- Updated macOS investigative feature documentation and modularization cleanup documentation.
- Added an `IosAppDbParser` class facade and routed app-runner iOS app database parsing through that parser facade.
- Updated thin-upload packaging so heavy APFS structural diagnostics are only included when diagnostic outputs are explicitly requested.
- Add `ReviewDatabaseHelper` and `ReviewQueryManager` modules.
- Run APFS lower-bound B-tree iterator as a comparator against current staged output before promoting it to live extraction.
- Converted filtered-view CSV export in the Win32 GUI to a background worker so large exports do not block the UI message loop.

## V1_0_17

- Added optional Apple/lzfse LZFSE/LZVN codec integration path.
- Added macOS investigative feature inventory and roadmap documentation.
- Added a codec smoke test using a known Apple/lzfse-produced LZVN vector.
- Added AFF4/APFS copy-out summary fields for codec status and decmpfs LZVN/LZFSE row counts.
- Added validation/status documentation for logical-size trim and optional codec integration.
- Added `src/codec/lzfse_codec.h/.cpp` with safe no-output behavior when the codec is not compiled in.
- Updated direct AFF4/APFS copy-out to prefer inode data-stream logical size over raw extent-chain end where available.
- Added `tools/Prepare-LzfseThirdParty.ps1` to explicitly vendor and manifest Apple/lzfse source under `third_party/lzfse`.
- Updated CMake and no-CMake MSVC build scripts to compile the Apple decoder sources only when the vetted source tree is present.

## V1_0_16

- Added optional Apple/lzfse LZFSE/LZVN codec integration path.
- Added validation/status documentation for logical-size trim and optional codec integration.
- Added `src/codec/lzfse_codec.h/.cpp` with safe no-output behavior when the codec is not compiled in.
- Updated direct AFF4/APFS copy-out to prefer inode data-stream logical size over raw extent-chain end where available.
- Added `tools/Prepare-LzfseThirdParty.ps1` to explicitly vendor and manifest Apple/lzfse source under `third_party/lzfse`.
- Updated CMake and no-CMake MSVC build scripts to compile the Apple decoder sources only when the vetted source tree is present.
- Updated APFS decmpfs resource-fork reconstruction so compression types 8/12 call the Apple codec adapter when available and record explicit decode/skipped statuses when unavailable or failed.

## V1_0_15

- Added AFF4/APFS Store-V2 candidate dual-process comparison.
- Added packaging and wrapper validation for the new compare outputs.
- Added LZFSE/LZVN source review documentation explaining why APFS structural documentation is authoritative for locating compressed content but not sufficient by itself to enable production codec output.
- New outputs.
- The compare output audits raw APFS copy-out candidates against normalized `StagedStoreV2` selections.
- Kept normal-mode AFF4/APFS structural diagnostics suppressed while keeping copy-out/staging/parser/enrichment/external-compare outputs enabled.

## V1_0_14

- Added opt-in AFF4/APFS structural diagnostic CSV output mode.
- Removed low-risk duplicated iOS parser wrapper functions from `app_runner.cpp`.
- Added `--aff4-apfs-diagnostic-outputs` / `--diagnostic-apfs-csvs` for full support runs.
- Updated wrapper validation so suppressed diagnostics do not block normal external comparison.
- Added callback-driven `ApfsVolumeReader::enumerateDirectory()` lower-bound iterator implementation for isolated APFS directory walk testing.
- Full iOS row parser migration awaits parser-independent row sink.
- Live APFS traversal replacement awaits iterator parity benchmarks.
- Confirmed GUI view registry ownership remains centralized in `view_registry`.
- Normalized investigator-facing Store-V2 files remain under `ExtractedSpotlight/StagedStoreV2/<group>/...`.

## V1_0_13

- Added opt-in AFF4/APFS structural diagnostic CSV output mode.
- Removed low-risk duplicated iOS parser wrapper functions from `app_runner.cpp`.
- Added `--aff4-apfs-diagnostic-outputs` / `--diagnostic-apfs-csvs` for full support runs.
- Updated wrapper validation so suppressed diagnostics do not block normal external comparison.
- Added callback-driven `ApfsVolumeReader::enumerateDirectory()` lower-bound iterator implementation for isolated APFS directory walk testing.
- Full iOS row parser migration awaits parser-independent row sink.
- Live APFS traversal replacement awaits iterator parity benchmarks.
- Confirmed GUI view registry ownership remains centralized in `view_registry`.
- Prevents duplicate same-name Store-V2 candidates from overwriting the file selected by normalized staging.

## V1_0_12

- Added opt-in AFF4/APFS structural diagnostic CSV output mode.
- Removed low-risk duplicated iOS parser wrapper functions from `app_runner.cpp`.
- Added `--aff4-apfs-diagnostic-outputs` / `--diagnostic-apfs-csvs` for full support runs.
- Updated wrapper validation so suppressed diagnostics do not block normal external comparison.
- Added callback-driven `ApfsVolumeReader::enumerateDirectory()` lower-bound iterator implementation for isolated APFS directory walk testing.
- Corrected stale AFF4 run-status wording: AFF4/APFS is no longer reported as unimplemented; it is described as an active guarded staged pipeline.
- Moved APFS B-tree table-of-contents key/value decoding into `src/parsers/apfs_aff4_reader.*` and left `app_runner.cpp` with thin compatibility wrappers.
- Preserved diagnostic CSVs because V1.0.8 still needs support outputs for external-compare mismatch analysis and before promoting AFF4/APFS to ordinary `discoverStores()` ingest.
- Updated iOS app database table processing to use `IosAppDbTableParseDecision` from `src/parsers/ios_app_db_parser.*` for parser routing, reducing app-runner-local classification branching.

## V1_0_11

- Corrected stale AFF4 run-status wording: AFF4/APFS is no longer reported as unimplemented; it is described as an active guarded staged pipeline.
- Moved APFS B-tree table-of-contents key/value decoding into `src/parsers/apfs_aff4_reader.*` and left `app_runner.cpp` with thin compatibility wrappers.
- Preserved diagnostic CSVs because V1.0.8 still needs support outputs for external-compare mismatch analysis and before promoting AFF4/APFS to ordinary `discoverStores()` ingest.
- Updated iOS app database table processing to use `IosAppDbTableParseDecision` from `src/parsers/ios_app_db_parser.*` for parser routing, reducing app-runner-local classification branching.
- CMake configure: PASS.
- New parser modules compiled before timeout: PASS.
- Did not add LZFSE/LZVN; this remains blocked on vetted source, MSVC/Linux integration, and test vectors.
- Continued APFS/AFF4 modularization without changing the already-working Store-V2 staging pipeline from V1.0.8.
- Full Linux build reached `app_runner.cpp` and timed out due to compilation time; no compile error was observed before timeout.

## V1_0_10

- Corrected stale AFF4 run-status wording: AFF4/APFS is no longer reported as unimplemented; it is described as an active guarded staged pipeline.
- Moved APFS B-tree table-of-contents key/value decoding into `src/parsers/apfs_aff4_reader.*` and left `app_runner.cpp` with thin compatibility wrappers.
- Preserved diagnostic CSVs because V1.0.8 still needs support outputs for external-compare mismatch analysis and before promoting AFF4/APFS to ordinary `discoverStores()` ingest.
- Updated iOS app database table processing to use `IosAppDbTableParseDecision` from `src/parsers/ios_app_db_parser.*` for parser routing, reducing app-runner-local classification branching.
- CMake configure: PASS.
- New parser modules compiled before timeout: PASS.
- Did not add LZFSE/LZVN; this remains blocked on vetted source, MSVC/Linux integration, and test vectors.
- Continued APFS/AFF4 modularization without changing the already-working Store-V2 staging pipeline from V1.0.8.
- Full Linux build reached `app_runner.cpp` and timed out due to compilation time; no compile error was observed before timeout.

## V1_0_9

- Corrected stale AFF4 run-status wording: AFF4/APFS is no longer reported as unimplemented; it is described as an active guarded staged pipeline.
- Moved APFS B-tree table-of-contents key/value decoding into `src/parsers/apfs_aff4_reader.*` and left `app_runner.cpp` with thin compatibility wrappers.
- Preserved diagnostic CSVs because V1.0.8 still needs support outputs for external-compare mismatch analysis and before promoting AFF4/APFS to ordinary `discoverStores()` ingest.
- Updated iOS app database table processing to use `IosAppDbTableParseDecision` from `src/parsers/ios_app_db_parser.*` for parser routing, reducing app-runner-local classification branching.
- CMake configure: PASS.
- New parser modules compiled before timeout: PASS.
- Did not add LZFSE/LZVN; this remains blocked on vetted source, MSVC/Linux integration, and test vectors.
- Continued APFS/AFF4 modularization without changing the already-working Store-V2 staging pipeline from V1.0.8.
- Full Linux build reached `app_runner.cpp` and timed out due to compilation time; no compile error was observed before timeout.

## V1_0_8

- Added parser module smoke tests and build integration for CMake and MSVC no-CMake builds.
- Preserved V1.0.7 live AFF4/APFS copy-out behavior; live traversal replacement is delayed until iterator parity can be benchmarked.
- Added `src/parsers/apfs_aff4_reader.h/.cpp` with a callback-driven APFS lower-bound directory-iterator scaffold and directory-record decoder.
- Added `src/parsers/ios_app_db_parser.h/.cpp` for iOS app database table classification, special-parser routing, and KnowledgeC snippet assembly.

## V1_0_7

- Updated MSVC/CMake builds to include the APFS module.
- Added APFS key, Store-V2 component, copy-status, and staging path helper functions with smoke-test coverage.
- Added a dedicated `src/parsers/apfs_volume_reader.*` module boundary for future APFS lower-bound B-tree iterator work.
- Preserved V1.0.6 AFF4/APFS extraction behavior while reducing the risk of adding the full B-tree iterator directly into `app_runner.cpp`.
- Corrected AFF4/APFS direct copy-out counting and staging classification for `COPIED_DIRECT_INDEXED_EXTENT_CHAIN` and `COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS`.

## V1_0_6

- MSVC preview-status helper scope hotfix.

## V1_0_5

- APFS target-index correlation and guarded direct Store-V2 copy-out attempt.

## V1_0_4

- AFF4/APFS target correlation and production cleanup.

## V1_0_3

- AFF4/APFS target correlation and production cleanup.

## V1_0_2

- AFF4/APFS target correlation and production cleanup.

## V1_0_1

- Added a bounded direct AFF4/APFS filesystem-tree target scan starting from resolved volume root-tree objects.
- Prioritizes likely Data volumes during target scanning.
- CLI version check reports `Vestigant Spotlight v1.0.1`.
- Resolves non-root APFS B-tree child nodes through each volume OMAP where possible.
- Records namespace-name samples and Spotlight target hits for `.Spotlight-V100` / `Store-V2` style paths.
- Keeps actual file copy-out gated until inode, xattr, dstream, file extent, sparse/gap, zero-block, decmpfs, and resource-fork provenance can be recorded defensibly.
- Always writes explicit target scan, inode probe, xattr probe, file-extent probe, copy-out, and staging output files, even when no target is found, so the upload bundle clearly distinguishes no-hit/incomplete states from missing-output packaging errors.
- Static raw-string/long-line check passed for MSVC C2026 risk.

## V1_0_0

- Added concrete V1.0.0 PowerShell scripts.
- Updated the single-AFF4 probe wrapper defaults for the V1.0.0 case/output names.
- Added a V1 AFF4/APFS diagnostic rerun artifact set written during AFF4 source-probe runs.
- Added those V1 AFF4/APFS diagnostic files to the thin-upload review index and upload bundle copy list.
- macOS investigator views.
- external-reference comparison,.
- decmpfs/resource-fork handling,.
- AFF4 container/virtual read access,.
- staged Store-V2 parsing/enrichment,.

## V0_9_60

- Legacy V7-only schema tables/indexes were removed from new case initialization.
- The V1 GUI source selector now exposes only fully implemented Folder and ZIP intake paths. AFF4/APFS and raw image support remain roadmap items and are not presented as clickable V1 options.
- Key changes.
- Current version: 0.9.60.
- Duplicate AFF4/APFS child/descendant root-tree probe output writers were consolidated into one traversal-output writer.
- View loading now shows an explicit marquee progress indicator above the investigation grid and a loading message in the details pane so long SQLite view loads are not mistaken for hangs.
- V0_9_60 is a V1 production-readiness cleanup after V0_9_57 compiled and ran on Windows. It improves the processing workflow and review workflow without changing parser interpretation logic.
- The Case Information bottom log is now the live processing log. It clears at run start, timestamps messages, mirrors run/progress status, and emits periodic heartbeat messages while processing continues.

## V0_9_59

- Legacy V7-only schema tables/indexes were removed from new case initialization.
- The V1 GUI source selector now exposes only fully implemented Folder and ZIP intake paths. AFF4/APFS and raw image support remain roadmap items and are not presented as clickable V1 options.
- Key changes.
- Current version: 0.9.59.
- Duplicate AFF4/APFS child/descendant root-tree probe output writers were consolidated into one traversal-output writer.
- View loading now shows an explicit marquee progress indicator above the investigation grid and a loading message in the details pane so long SQLite view loads are not mistaken for hangs.
- V0_9_59 is a V1 production-readiness cleanup after V0_9_57 compiled and ran on Windows. It improves the processing workflow and review workflow without changing parser interpretation logic.
- The Case Information bottom log is now the live processing log. It clears at run start, timestamps messages, mirrors run/progress status, and emits periodic heartbeat messages while processing continues.

## V0_9_58

- Legacy V7-only schema tables/indexes were removed from new case initialization.
- The V1 GUI source selector now exposes only fully implemented Folder and ZIP intake paths. AFF4/APFS and raw image support remain roadmap items and are not presented as clickable V1 options.
- Key changes.
- Current version: 0.9.58.
- Duplicate AFF4/APFS child/descendant root-tree probe output writers were consolidated into one traversal-output writer.
- View loading now shows an explicit marquee progress indicator above the investigation grid and a loading message in the details pane so long SQLite view loads are not mistaken for hangs.
- V0_9_58 is a V1 production-readiness cleanup after V0_9_57 compiled and ran on Windows. It improves the processing workflow and review workflow without changing parser interpretation logic.
- The Case Information bottom log is now the live processing log. It clears at run start, timestamps messages, mirrors run/progress status, and emits periodic heartbeat messages while processing continues.

## V0_9_57

- Windows MSVC batch-label build hotfix.
- No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.
- Next Windows checks.
- V0_9_57 fresh-ZIP run.
- V0_9_57 reuse-cache run.
- Run reuse-cache and fresh-ZIP scripts.
- Windows GUI forward-declaration compile hotfix.
- V0_9_57 pre-patch review metrics from uploaded V0_9_45 thin results.

## V0_9_56

- Next Windows checks.
- V0_9_56 fresh-ZIP run.
- V0_9_56 reuse-cache run.
- Run reuse-cache and fresh-ZIP scripts.
- V0_9_56 pre-patch review metrics from uploaded V0_9_45 thin results.
- Build V0_9_56 and verify the build banner says source version 0.9.56 and binary version says v0.9.56.
- Confirm fresh-ZIP app database row count remains 5528, with substantially fewer SIGNAL and CHROME_WEB category labels.

## V0_9_55

- V0_9_55 is a GUI-focused V1 readiness release built on the stable V0_9_54 parser/intake baseline. It does not intentionally change parser, cache, ZIP staging, FFS inventory, or app database classification behavior.
- Added live elapsed-time status updates while processing runs.
- Added Custom view set support for both MacOS and iOS investigation tabs.
- Added view-set controls: Move Up, Move Down, Hide, Save Set, and Reset Set.
- Confirmed removed V1-blocker GUI controls and V7 importer references remain absent.
- Added Vestigant branding to the Case Information workflow using the supplied green Vestigant logo.
- Added a visible Tags button on the investigation toolbar to switch directly to the Tags / Notes workflow.
- Added processing telemetry messages for elapsed processing time and estimated throughput when a measurable file source size is available.

## V0_9_54

- C++20 syntax checks passed for changed core files.
- Removed the GUI `Skip preservation (temporary testing only)` checkbox.
- Removed visible Legacy V7 workflow messaging from the GUI and CLI help.
- Removed `src/parsers/v7_output_importer.cpp` and `.h` from the package.
- Removed V7 importer compilation from CMake and the Windows MSVC batch build.
- Removed import-v7 / legacy-v7 CLI option parsing and runApplication import/compare branches.
- Preserved the V0_9_53 two-column selected-row details ListView and investigation-tab-only visibility behavior.
- Moved GUI SQL review-view creation from `src/gui/win32_gui.cpp` into `CaseDatabase::ensureGuiReviewViews()` in `src/db/case_db.cpp`.

## V0_9_53

- Updated version metadata/scripts/docs to V0_9_53.
- Added a platform-scoped View Set selector for MacOS and iOS investigation tabs.
- Added review_view_preferences schema table for future per-case custom view ordering/visibility.
- This is a GUI-only change; no ingest, parser, cache, or export behavior was intentionally changed.
- No parser, ingest, cache, export, ZIP, FFS inventory, app DB, or forensic interpretation logic changed.
- Added a bottom read-only `Selected Row Metadata / All Fields` details pane to the shared investigation grid.
- Added an existing-case GUI test note/script so GUI-only feature testing can proceed without rerunning ingest.
- Added existing-case schema/view upgrade on open so newly added SQL views can appear without re-ingest when the case DB is writable.

## V0_9_52

- Updated version metadata/scripts/docs to V0_9_52.
- Added a platform-scoped View Set selector for MacOS and iOS investigation tabs.
- Added review_view_preferences schema table for future per-case custom view ordering/visibility.
- This is a GUI-only change; no ingest, parser, cache, or export behavior was intentionally changed.
- Added a bottom read-only `Selected Row Metadata / All Fields` details pane to the shared investigation grid.
- Added an existing-case GUI test note/script so GUI-only feature testing can proceed without rerunning ingest.
- Added existing-case schema/view upgrade on open so newly added SQL views can appear without re-ingest when the case DB is writable.
- A draggable splitter was added above the details pane so the investigator can adjust the grid/detail height while reviewing results.

## V0_9_51

- Updated version metadata/scripts/docs to V0_9_51.
- Added a platform-scoped View Set selector for MacOS and iOS investigation tabs.
- Added review_view_preferences schema table for future per-case custom view ordering/visibility.
- This is a GUI-only change; no ingest, parser, cache, or export behavior was intentionally changed.
- Added a bottom read-only `Selected Row Metadata / All Fields` details pane to the shared investigation grid.
- Added an existing-case GUI test note/script so GUI-only feature testing can proceed without rerunning ingest.
- Added existing-case schema/view upgrade on open so newly added SQL views can appear without re-ingest when the case DB is writable.
- A draggable splitter was added above the details pane so the investigator can adjust the grid/detail height while reviewing results.

## V0_9_50

- Updated version metadata/scripts/docs to V0_9_50.
- Added a platform-scoped View Set selector for MacOS and iOS investigation tabs.
- Added review_view_preferences schema table for future per-case custom view ordering/visibility.
- Added a bottom read-only `Selected Row Metadata / All Fields` details pane to the shared investigation grid.
- Added an existing-case GUI test note/script so GUI-only feature testing can proceed without rerunning ingest.
- Added existing-case schema/view upgrade on open so newly added SQL views can appear without re-ingest when the case DB is writable.
- Added grouped RichEdit selected-row details pane with text/content first, dates second, then paths, people/apps, status, provenance, counts, and other fields.
- Artifacts: 344,445.

## V0_9_49

- Updated version metadata/scripts/docs to V0_9_49.
- Added a bottom read-only `Selected Row Metadata / All Fields` details pane to the shared investigation grid.
- Added an existing-case GUI test note/script so GUI-only feature testing can proceed without rerunning ingest.
- Artifacts: 344,445.
- Raw records: 344,445.
- Timeline events: 336,037.
- Status: `complete_success`.
- Raw key/value rows: 982,668.

## V0_9_48

- Changed-file Linux `g++ -fsyntax-only` checks passed.
- Added KnowledgeC/CoreDuet ZOBJECT + ZSTRUCTUREDMETADATA joined parsing where available.
- Updated GUI iOS view registry entries, export package output, scripts, version metadata, and validation documentation.
- Preserved the bplist feature as bounded string discovery only; it does not claim full NSKeyedArchiver object-graph decoding.
- Added deleted/recoverable Apple Messages table row extraction for tables already classified as MESSAGE_DELETED_OR_RECOVERABLE.
- No LZFSE/LZVN codec was added; that remains deferred until vetted codec source is included and build-system integration can be validated.
- Changed normal iOS mode to parse only already-extracted high-value app databases for investigator summaries. Full broad app DB materialization remains opt-in.
- Added investigator review views and exports for KnowledgeC/CoreDuet interaction summaries/events. Normal mode still skips broad app database record materialization by default.

## V0_9_47

- Changed-file Linux `g++ -fsyntax-only` checks passed.
- Updated GUI iOS view registry entries, export package output, scripts, version metadata, and validation documentation.
- Preserved the bplist feature as bounded string discovery only; it does not claim full NSKeyedArchiver object-graph decoding.
- Added investigator review views and exports for KnowledgeC/CoreDuet interaction summaries/events. Normal mode still skips broad app database record materialization by default.
- Added an explicit `vw_investigator_time_anomalies` triage view/export that compares available Spotlight-derived usage/download/update fields and includes an interpretation warning.
- Added specialized KnowledgeC `ZOBJECT` parsing scaffolding for `/app/inFocus`, `/document/open`, and `/app/intents` streams when support/full app database record materialization is enabled.
- Added KnowledgeC/CoreDuet database classification for `knowledgeC.db`, `interactionC.db`, and `globalKnowledge.db`, plus targeted extraction patterns for future support-mode app database materialization.
- iOS investigative-value update.

## V0_9_46

- Updated stale VERSION/VERSION.txt/CMake project metadata so the build banner matches the package version.
- Confirmed V0_9_45 completed both reuse-cache and fresh-ZIP runs.
- Reviewed V0_9_45 build, reuse-cache thin output, and fresh-ZIP thin output.
- Confirmed fresh-ZIP inventory now reports nonzero FFS rows and a much smaller app database candidate set.
- Tightened app database categorization so generic `signals` and `history` database names do not become Signal or Chrome/Web evidence without stronger path/app identifiers.

## V0_9_45

- Removed generic filename-only *history* matching from Chrome/Web classification.
- Updated quick diagnostic helper classification logic to remain consistent with the main fresh-ZIP workflow.
- Updated VERSION, VERSION.txt, CMake project version, app version, scripts, and current documentation references to 0.9.46.
- Preserved normal iOS mode constraints: no broad FFS materialization and no broad app DB record materialization by default.
- GUI launch.
- Review findings.
- complete_success.
- V0_9_45 fresh-ZIP thin.

## V0_9_44

- Updated version metadata, scripts, help, validation notes, and roadmap to V0_9_44.
- Added a warning status if a raw 7-Zip listing exists but the C++ parser produces zero inventory records.
- Fixed the fresh-ZIP 7-Zip raw inventory handoff by avoiding Windows PowerShell UTF-16 redirection for `7z l -slt` output.
- Added native C++ raw-listing line normalization that can decode older UTF-16LE/UTF-16BE PowerShell-redirection logs as a fallback.
- Fresh ZIP FFS inventory recovery and native parser efficiency.
- Raw-string size scan found no oversized raw-string literals above the configured threshold.
- Improved bounded high-value probe deduplication and non-ASCII preservation for fallback/CoreSpotlight probes.
- Hardened `cleanDecodedString` so trailing null/space padding does not prevent removal of the CoreSpotlight `0x16 0x02` trailer marker.

## V0_9_43

- V0_9_43 reviews the uploaded V0_9_42 build log and reuse-cache thin output. V0_9_42 built successfully and the reuse-cache run reached `complete_success`, so this release moves from performance-only work to bounded iOS investigative value.
- Ran changed-file C++ syntax checks for `native_storedb_parser.cpp`, `case_db.cpp`, and `sqlite_exporter.cpp`.
- Bplist / NSKeyedArchiver discovery scaffold.
- Reviewed V0_9_42 build log and reuse-cache thin output.
- Ran a SQLite view smoke test for the new VSQL33 bplist views.
- Adds normal CSV exports and upload samples for the new summary/detail views.
- Ran MSVC C2026 raw-string size risk check on the major SQL/GUI/export files.
- Confirmed V0_9_42 build succeeded and the reuse-cache run reached `complete_success`.

## V0_9_42

- Added a fresh-ZIP Stage B run script.
- Preserved compact iOS Spotlight normal mode and current investigator views.
- V1-readiness performance pass.
- Increased sequential hash-read buffers.
- Improved generated 7-Zip FFS inventory parsing for future actual-ZIP testing.
- Optimized CSV export writing to reduce string allocation and small-write overhead.
- Reviewed V0_9_39 build/thin results; Windows build and reuse-cache run completed successfully.

## V0_9_41

- Added a fresh-ZIP Stage B run script.
- Preserved compact iOS Spotlight normal mode and current investigator views.
- V1-readiness performance pass.
- Increased sequential hash-read buffers.
- Improved generated 7-Zip FFS inventory parsing for future actual-ZIP testing.
- Optimized CSV export writing to reduce string allocation and small-write overhead.
- Reviewed V0_9_39 build/thin results; Windows build and reuse-cache run completed successfully.

## V0_9_40

- Added a fresh-ZIP Stage B run script.
- Preserved compact iOS Spotlight normal mode and current investigator views.
- V1-readiness performance pass.
- Increased sequential hash-read buffers.
- Improved generated 7-Zip FFS inventory parsing for future actual-ZIP testing.
- Optimized CSV export writing to reduce string allocation and small-write overhead.
- Reviewed V0_9_39 build/thin results; Windows build and reuse-cache run completed successfully.

## V0_9_39

- Added safer SQLite close behavior using `sqlite3_close_v2()` fallback/usage.
- Fixed the V0_9_38 SQLite guardrail failure by tightening compact-mode native/text value storage.
- Added minimal GUI review-page lifecycle hardening: review loads are tracked, cancellable through SQLite progress callbacks, and joined on cancel/shutdown.
- Missing From FFS text visibility guardrail fix.
- Updates version metadata and scripts to V0_9_39.
- Confirmed source syntax and static raw-string checks.
- Reviewed the V0_9_37 Windows build log and thin upload.
- Preserves the V0_9_37 Missing From FFS text-detail views/exports.

## V0_9_38

- Updates version metadata and scripts to V0_9_38.
- Confirmed source syntax and static raw-string checks.
- Reviewed the V0_9_37 Windows build log and thin upload.
- Preserves the V0_9_37 Missing From FFS text-detail views/exports.
- Windows/MSVC build and the standard iOS reuse-cache run remain required.
- Confirmed the failure class was DB-size guardrail caused by larger text context values rather than row-count explosion.
- Keeps row-level text visibility/status columns so missing-from-FFS candidates show recovered Spotlight text where compact mode has it.
- Restores normal-mode Spotlight text context to a bounded investigator-safe size: 1,800 bytes, 8 fields, and 320 bytes per field sample.

## V0_9_37

- V0_9_37 is a documentation-history repair release after V0_9_34 cleanup compressed too much historical detail.
- Updated version metadata and scripts to V0_9_37.
- Confirmed version metadata was updated to 0.9.37.
- No parser, schema, GUI, export, or forensic interpretation behavior was intentionally changed from V0_9_34.
- Windows/MSVC build validation remains required because only documentation/version/script metadata changed in this packaging environment.
- Updated `docs/CONSOLIDATED_USER_MANUAL.md` to explain the current documentation model, standard workflows, iOS review start path, compact-mode interpretation, diagnostics, and AFF4/APFS roadmap location.
- Confirmed ZIP/patch integrity and SHA256 files.
- Confirmed production package still avoids reintroducing root-level historical fragments.

## V0_9_36

- V0_9_36 is a documentation-history repair release after V0_9_34 cleanup compressed too much historical detail.
- Updated version metadata and scripts to V0_9_36.
- Confirmed version metadata was updated to 0.9.36.
- No parser, schema, GUI, export, or forensic interpretation behavior was intentionally changed from V0_9_34.
- Windows/MSVC build validation remains required because only documentation/version/script metadata changed in this packaging environment.
- Updated `docs/CONSOLIDATED_USER_MANUAL.md` to explain the current documentation model, standard workflows, iOS review start path, compact-mode interpretation, diagnostics, and AFF4/APFS roadmap location.
- Confirmed ZIP/patch integrity and SHA256 files.
- Confirmed production package still avoids reintroducing root-level historical fragments.

## V0_9_35

- No parser, schema, GUI, export, or forensic interpretation behavior was intentionally changed from V0_9_34.
- Updated `docs/CONSOLIDATED_USER_MANUAL.md` to explain the documentation model, standard workflows, iOS review path, compact-mode interpretation, and where historical process information now lives.
- V0_9_35 documentation/history repair package, which was not separately run.
- Reviewed the user-uploaded historical `Docs.zip` from the V0_9_3 documentation set.
- Restored historical V0_9 development detail into this consolidated version history rather than reintroducing many stale per-version note fragments.

## V0_9_34

- V0_9_34 is a cleanup/consolidation release after the V0_9_33 build and thin upload completed successfully. The release keeps parser behavior stable and focuses on production-package hygiene.
- V0_9_34 reviewed the V0_9_31 build/thin result and found the run completed successfully with stable compact-mode counts. This release focuses on making the iOS Spotlight review workflow more usable for investigators.
- Added iOS - Investigator Overview as a start-here GUI view.
- Added normal investigator exports for these views and smoke-test coverage.
- Removed superseded old V0_7/V0_8 documentation fragments from the production ZIP.
- Corrected VERSION and VERSION.txt so the source package version is no longer stale.
- Added iOS - Timeline Month Summary for compact timeline triage by month/category/anomaly.
- Preserved compact normal iOS mode and did not reintroduce full raw property, FFS, or app DB materialization.

## V0_9_33

- Added GUI view `iOS - Parser Diagnostics Detail Sample`.
- Added normal export `parser_diagnostics_detail_sample.csv`.
- Preserved compact normal iOS mode; full native/dbStr/property persistence and broad FFS/app DB materialization remain support/diagnostic options.
- Added parser diagnostics detail view/export so native failures and partial decode errors are visible at record/sample level, not only as summary counts.
- C++ syntax checks for modified files.
- SQLite schema smoke test expanded to include the new diagnostics detail view.
- Raw-string fragment length check to reduce recurrence of MSVC C2026 oversized literal failures.
- Improved compact iOS Spotlight message/body review extraction from same-record Spotlight text context, including mail/message title, snippet, description, supporting text, and suggested contact/thread context where present.

## V0_9_32

- Added normal investigator exports and schema-smoke coverage for the new views.
- Added `iOS - Investigator Overview` as a start-here view for iOS Spotlight review.
- Added `iOS - Timeline Month Summary` for compact timeline triage by month/category/anomaly.
- Added GUI view and export `iOS - Direct User Message Thread Summary` / `ios_spotlight_direct_user_message_thread_summary.csv`.
- Updated the consolidated manual/version history to describe the recommended iOS review starting path.
- Added `iOS - Direct User Message Thread Summary` to group direct messages by available contact/thread/handle context.
- Added `iOS - Direct User Message Review` for direct Apple Messages/SMS/RCS/iMessage text recovered from compact Spotlight context.

## V0_9_31

- Added GUI view `iOS - Parser Diagnostics Detail Sample`.
- Added normal export `parser_diagnostics_detail_sample.csv`.
- Added `ios_spotlight_message_contact_thread_detail_sample.csv` for bounded representative thread/handle examples.
- Preserved compact normal iOS mode; full native/dbStr/property persistence and broad FFS/app DB materialization remain support/diagnostic options.
- Added parser diagnostics detail view/export so native failures and partial decode errors are visible at record/sample level, not only as summary counts.
- Added bounded contact/thread detail sampling, message body focus summary, parser diagnostics action summary, Plaso/L2T timeline sample, and case quality dashboard views/exports.
- C++ syntax checks for modified files.

## V0_9_30

- Added normal export `parser_diagnostics_detail_sample.csv`.
- Preserved compact normal iOS mode; full native/dbStr/property persistence and broad FFS/app DB materialization remain support/diagnostic options.
- Added parser diagnostics detail view/export so native failures and partial decode errors are visible at record/sample level, not only as summary counts.
- Added `iOS - Parser Diagnostics Detail Sample` and `parser_diagnostics_detail_sample.csv` so unsupported/unparsed native parser details are visible, not only summarized.
- C++ syntax checks for modified files.
- SQLite schema smoke test expanded to include the new diagnostics detail view.
- Raw-string fragment length check to reduce recurrence of MSVC C2026 oversized literal failures.
- Improved `iOS - Spotlight Message Body Review` so message/mail body, subject, snippet, supporting text, and thread/contact context are extracted from compact same-record Spotlight context when available.

## V0_9_29

- Added normalized iOS Spotlight timeline and timeline anomaly summary with source-field/date-provenance cautions.
- Preserved compact normal iOS mode; full FFS/app DB/native-property materialization remains support/diagnostic only.
- Added schema smoke coverage to the self-test for key iOS Spotlight review/diagnostic views so SQL/view regressions are caught earlier.
- Added message body/subject extraction, contact/thread summary, user-focus message review, and non-destructive noise-reduction summaries for iOS Spotlight communications.
- Added case/provenance and parser diagnostics summary views/exports to make parser version, source context, raw failures, partial decode errors, and native decode attempt status visible.
- iOS message body review, timeline/provenance, and diagnostics.

## V0_9_28

- Added attachment/media-focused Spotlight reference review for communications.
- Added `investigator_visible_text`, `message_domain_handle_or_chat`, and `mail_participants` to communication review output.
- Preserved compact normal iOS mode; full FFS/app DB materialization and full native DB persistence remain support/diagnostic options only.
- Fixed V0_9_27 Windows GUI compile failure caused by a duplicate/dead SQL block in `src/gui/win32_gui.cpp` that referenced `execGuiSqlParts` outside its scope.
- Added communication summary output and upload samples so investigators can start from compact high-value communication surfaces instead of broad raw key/value tables.
- Corrected message title extraction to include `kMDItemAppEntityTitle`, so many iOS Messages rows show direct recovered message text rather than only `------NONAME------`.
- Split media saved/shared from Messages into `MEDIA_SAVED_OR_SHARED_FROM_MESSAGES`, avoiding over-classification of Photos/mobile-slideshow assets as direct message body records.
- Added iOS Spotlight Message Text Review and Message Media Review views/exports to make recovered Spotlight communications, message text, mail/call/chat context, and message-adjacent media easier to review.

## V0_9_27

- Added attachment/media-focused Spotlight reference review for communications.
- Preserved compact normal iOS mode; full FFS/app DB materialization and full native DB persistence remain support/diagnostic options only.
- Added communication summary output and upload samples so investigators can start from compact high-value communication surfaces instead of broad raw key/value tables.
- record-centric iOS Spotlight communications review.
- Incorporated V0_9_26_2 GUI C2026 oversized SQL literal fix as the base for V0_9_27.
- Reviewed V0_9_26_1 thin output: CLI run completed successfully with 344,445 raw iOS Spotlight records, 982,230 compact key/value rows, 336,037 compact date candidates, and complete_success.

## V0_9_26_2

- Added GUI view `iOS - Spotlight Chat App Attribution Summary`.
- Preserved V0_9_26 SQL/view behavior and chat-app attribution changes.
- Added `classification_evidence` to Spotlight text-context review and priority summary views.
- Preserved compact normal iOS mode, parser limits reporting, and Spotlight-first review behavior.
- Added validation note that all raw SQL literal fragments in `case_db.cpp` should remain below conservative MSVC-safe size thresholds.
- Fixed Windows/MSVC compile failure `src\\db\\case_db.cpp(...): error C2026: string too big, trailing characters truncated` by splitting the affected SQL raw-string block into smaller runtime-joined fragments.
- MSVC oversized SQL literal compile fix.
- chat-app attribution refinement and repeatable thin-review workflow.

## V0_9_26_1

- Added GUI view `iOS - Spotlight Chat App Attribution Summary`.
- Preserved V0_9_26 SQL/view behavior and chat-app attribution changes.
- Added `classification_evidence` to Spotlight text-context review and priority summary views.
- Preserved compact normal iOS mode, parser limits reporting, and Spotlight-first review behavior.
- Added validation note that all raw SQL literal fragments in `case_db.cpp` should remain below conservative MSVC-safe size thresholds.
- Fixed Windows/MSVC compile failure `src\\db\\case_db.cpp(...): error C2026: string too big, trailing characters truncated` by splitting the affected SQL raw-string block into smaller runtime-joined fragments.
- MSVC oversized SQL literal compile fix.
- chat-app attribution refinement and repeatable thin-review workflow.

## V0_9_26

- Added GUI view `iOS - Spotlight Chat App Attribution Summary`.
- Added `classification_evidence` to Spotlight text-context review and priority summary views.
- Preserved compact normal iOS mode, parser limits reporting, and Spotlight-first review behavior.
- chat-app attribution refinement and repeatable thin-review workflow.
- Documented the repeatable build-log/thin-upload review method so future cycles can reuse the same process.
- Tightened generic human-text category matching for WhatsApp/Signal/Telegram so app-name keywords alone do not inflate chat-app categories.
- Refined iOS Spotlight text-context classification to separate explicit chat-app bundle/domain/external-id attribution from plain text/link mentions. This avoids false positives such as location names containing "Signalâ€ being promoted as Signal app evidence.

## V0_9_25

- Added GUI views `iOS - High-Value Spotlight Text Context` and `iOS - Spotlight Text Context Priority Summary`.
- Fixed thin upload packaging to avoid recursively including prior `Upload` folders and to downsample oversized generic upload_samples while preserving high-value samples.
- Added priority/category scoring to same-record iOS Spotlight text context so communications, email, chat, URL, call/contact, and calendar contexts surface before generic text.
- high-value Spotlight text triage and cleaner thin uploads.
- Kept normal iOS mode compact; full native/dbStr/property and broad FFS/app DB materialization remain diagnostic/support options.
- Reviewed V0_9_24 build/thin results: Windows/MSVC build completed, run reached complete_success, and parser limits report was present.

## V0_9_24

- Added the same text-context sample to thin upload packaging.
- Added a limits/suppression section to `CASE_REVIEW_SUMMARY.txt` and updated the upload README/review index to point to the new report.
- Added `iOS - Spotlight Text Context Review` and `ios_spotlight_text_context_review_sample.csv` so investigators can directly review compact same-record Spotlight text retained in normal iOS mode.
- Preserved the compact Spotlight-first default profile: full native/dbStr/property persistence, broad app DB materialization, and full FFS inventory exports remain explicit support/diagnostic choices.
- parser limits transparency and Spotlight text-context review.
- Reviewed the V0_9_23_1 thin upload: the run completed successfully and the DB/export-stall issues remained resolved.

## V0_9_23_1

- Added high-value Missing From FFS GUI views and exports while preserving full candidate views for support review.
- Normalized repeated slashes in iOS path references before FFS lookup and improved missing-row lookup-source reporting.
- Prioritized iOS Missing From FFS review by separating high/medium-value user-document, message attachment, and content URL references from likely thumbnail/brand/cache-only path noise.

## V0_9_23

- Added high-value Missing From FFS GUI views and exports while preserving full candidate views for support review.
- Normalized repeated slashes in iOS path references before FFS lookup and improved missing-row lookup-source reporting.
- Prioritized iOS Missing From FFS review by separating high/medium-value user-document, message attachment, and content URL references from likely thumbnail/brand/cache-only path noise.

## V0_9_22

- Added V0_9_22 concrete build/reuse-cache/state scripts.
- Added GUI view `iOS - Spotlight Missing From FFS Summary`.
- Preserved Spotlight-first compact mode and support/diagnostic-only full row exports.
- Added `ios_ffs_path_lookup`, a slim path-only FFS lookup populated from reuse cache/CSV when full FFS inventory is not materialized.
- Updated Missing From FFS views to use either full FFS inventory or the slim lookup and to avoid false missing classifications when no lookup is available.

## V0_9_21

- Added V0_9_21 concrete build, reuse-cache, packaging, and stopped-state scripts.
- Added preferred raw_record_id mapping in human text views to avoid cross-multiplying same-object raw_key_values.
- Added compact object/inode diagnostic summary to evaluate whether repeated raw records represent the same object/inode.
- Normal iOS investigator export now avoids full ios_spotlight_record_review/date_provenance/investigative_items/object_inode_summary materialization.
- Linux validation: build and self-test passed; Windows/MSVC validation pending.

## V0_9_20

- Updated Missing From FFS GUI column registry to expose Spotlight text context/status.
- Added SQL export heartbeat progress so long-running queries write status before the first row returns.
- Fixed V0_9_20 state collector ZIP creation to avoid the non-fatal Resolve-Path warning seen during V0_9_19 stopped-state collection.
- Added iOS Spotlight Object/Inode Summary to help determine whether multiple Spotlight records or table rows represent the same inode/object/store identifier.
- Keeps normal investigator mode Spotlight-first and moves full FFS/app correlation detail exports to support/diagnostic mode by default.
- Reworked iOS Spotlight Record Review to stay anchored to one row per raw_record_id and avoid full FFS/app-residency joins in the default export.
- Adds SQLite export heartbeat progress using `sqlite3_progress_handler` so long-running query execution writes progress before the first row is returned.
- Reworks `vw_ios_spotlight_record_review` to stay one row per `raw_record_id`, collapse date provenance per record, and avoid full FFS/app-residency joins in the record-review export.

## V0_9_19

- V0_9_19 refinement: normal mode keeps a slim FFS normalized-path index in the active DB instead of omitting FFS rows entirely. This preserves Missing From FFS/Spotlight-to-FFS correlation while avoiding the full FFS inventory row bloat.

## V0_9_18

- Added V0_9_18 build/run/collection scripts with concrete standard paths.
- Added SQLite DB/WAL guardrails, parser heartbeat progress, periodic transaction commits, and WAL checkpoint/truncate attempts during long native parses.
- Fixed the V0_9_17 iOS CoreSpotlight normal-mode DB/WAL bloat class by making default native key/value and date-candidate persistence compact and Spotlight-investigator-focused.
- Added safer diagnostic-full-native behavior: full raw native DB persistence remains explicit and defaults to a bounded record sample unless --max-native-records is explicitly set.
- Added stricter iOS raw_key_values filtering, bounded per-record date provenance, suppression of derived/ranking/component date candidates, and support-only raw usage/detail exports.

## V0_9_17

- Fixed Visual Studio discovery to avoid repeated CheckVsPath batch-label errors.
- Fixed V0_9_15 iOS output-control regression that generated very large normal investigator exports.
- Added native diagnostics tables for `native_dbstr_map_inventory`, `native_category_dictionary`, and `native_index_dictionary_summary`.
- Added GUI/export review surfaces: `iOS - Spotlight dbStr Map Inventory`, `iOS - Spotlight Dictionary Coverage`, and `iOS - Spotlight Apple Field Coverage`.
- Added Apple-public-field semantic grouping for recovered Spotlight/CoreSpotlight field names. Apple developer documentation is used only as a public semantic taxonomy, not as private on-disk Store-V2 schema.
- Reduced verbose native parser block logging and export progress-status churn.
- Reuse-cache scripts continue to use `--skip-container-hash` by default for development/test iteration against large iOS FFS ZIPs.
- Reworked iOS Spotlight value/date evidence to use aggregated record-level date provenance instead of value x date candidate expansion.

## V0_9_16

- Fixed Visual Studio discovery to avoid repeated CheckVsPath batch-label errors.
- Fixed V0_9_15 iOS output-control regression that generated very large normal investigator exports.
- Reduced verbose native parser block logging and export progress-status churn.
- Reworked iOS Spotlight value/date evidence to use aggregated record-level date provenance instead of value x date candidate expansion.
- Made broad FFS/app DB/native diagnostic exports opt-in through diagnostics/support/full export profiles or --diagnostic-full-native-exports.

## V0_9_15

- Added native diagnostics tables for `native_dbstr_map_inventory`, `native_category_dictionary`, and `native_index_dictionary_summary`.
- Added GUI/export review surfaces: `iOS - Spotlight dbStr Map Inventory`, `iOS - Spotlight Dictionary Coverage`, and `iOS - Spotlight Apple Field Coverage`.
- Added Apple-public-field semantic grouping for recovered Spotlight/CoreSpotlight field names. Apple developer documentation is used only as a public semantic taxonomy, not as private on-disk Store-V2 schema.
- Reuse-cache scripts continue to use `--skip-container-hash` by default for development/test iteration against large iOS FFS ZIPs.
- Yogesh Khatri `spotlight_parser` / `mac_apt` behavior is treated as a reference only; this version implements independent C++ parsing logic and does not copy GPL source.
- Legacy V7 import remains compiled but deprecated/hidden from normal workflow; physical removal remains a later cleanup once active Spotlight/CoreSpotlight paths remain stable.

## V0_9_14

- Added `iOS - Spotlight Entity Summary` / `ios_spotlight_entity_summary.csv`, grouped counts by entity type, source store, source field, date semantic class, and supporting FFS/app context.
- Spotlight entity review, parser-target triage, and V0_9_11_1 GUI fix carry-forward.
- Carried forward V0_9_11_1 GUI compile fix (`execGuiSql` in GUI SQL helpers) so CLI, tests, and GUI should all build from this baseline.
- Reuse-cache development scripts continue to pass `--skip-container-hash` by default. Full source hashing remains available for final/forensic runs.
- Legacy V7 import remains compiled but is still deprecated/hidden from normal workflow; physical removal remains a separate cleanup after the active iOS Spotlight review path remains stable.
- Reviewed V0_9_11 reused-cache thin upload: CLI run completed successfully with 344,445 Spotlight records, 21,472 recovered human-text/key-value rows, 344,445 Spotlight date candidates, and V0_9_11 Spotlight-first timeline/reference exports.

## V0_9_13

- Added `iOS - Spotlight Entity Summary` / `ios_spotlight_entity_summary.csv`, grouped counts by entity type, source store, source field, date semantic class, and supporting FFS/app context.
- Spotlight entity review, parser-target triage, and V0_9_11_1 GUI fix carry-forward.
- Carried forward V0_9_11_1 GUI compile fix (`execGuiSql` in GUI SQL helpers) so CLI, tests, and GUI should all build from this baseline.
- Reuse-cache development scripts continue to pass `--skip-container-hash` by default. Full source hashing remains available for final/forensic runs.
- Legacy V7 import remains compiled but is still deprecated/hidden from normal workflow; physical removal remains a separate cleanup after the active iOS Spotlight review path remains stable.
- Reviewed V0_9_11 reused-cache thin upload: CLI run completed successfully with 344,445 Spotlight records, 21,472 recovered human-text/key-value rows, 344,445 Spotlight date candidates, and V0_9_11 Spotlight-first timeline/reference exports.

## V0_9_12

- Added `iOS - Spotlight Entity Summary` / `ios_spotlight_entity_summary.csv`, grouped counts by entity type, source store, source field, date semantic class, and supporting FFS/app context.
- Spotlight entity review, parser-target triage, and V0_9_11_1 GUI fix carry-forward.
- Carried forward V0_9_11_1 GUI compile fix (`execGuiSql` in GUI SQL helpers) so CLI, tests, and GUI should all build from this baseline.
- Reuse-cache development scripts continue to pass `--skip-container-hash` by default. Full source hashing remains available for final/forensic runs.
- Legacy V7 import remains compiled but is still deprecated/hidden from normal workflow; physical removal remains a separate cleanup after the active iOS Spotlight review path remains stable.
- Reviewed V0_9_11 reused-cache thin upload: CLI run completed successfully with 344,445 Spotlight records, 21,472 recovered human-text/key-value rows, 344,445 Spotlight date candidates, and V0_9_11 Spotlight-first timeline/reference exports.

## V0_9_11_1

- Runtime SQL split fix after V0_9_7_2 thin upload.
- Added date provenance columns to the Spotlight-first record review surface.
- Preserved V0_9_11 Spotlight-first iOS review views and reuse-cache workflow.
- Added the new date provenance CSV to normal exports and thin-upload required export collection.
- Fixed the iOS keyword surface source label typo from CORESPOPTLIGHT_TEXT to CORESPOTLIGHT_TEXT.
- Added Spotlight decode coverage summary, field coverage summary, human text category summary, record review, and decode-gap views.
- Fixed GUI compile error `C3861: exec identifier not found` by changing split GUI SQL view creation blocks to use `execGuiSql(...)`.
- Proactively split large SQL raw string literals in `src\db\case_db.cpp` and `src\gui\win32_gui.cpp` below the MSVC-sensitive range.

## V0_9_11

- Runtime SQL split fix after V0_9_7_2 thin upload.
- Added date provenance columns to the Spotlight-first record review surface.
- Fixed the iOS keyword surface source label typo from CORESPOPTLIGHT_TEXT to CORESPOTLIGHT_TEXT.
- Added the new date provenance CSV to normal exports and thin-upload required export collection.
- Added Spotlight decode coverage summary, field coverage summary, human text category summary, record review, and decode-gap views.
- Proactively split large SQL raw string literals in `src\db\case_db.cpp` and `src\gui\win32_gui.cpp` below the MSVC-sensitive range.
- Updated generated reuse-cache CLI packaging script to pass `--skip-container-hash` by default for development/test reuse-cache runs.
- Preserved date provenance fields so investigative values can be traced back to raw Spotlight date candidates and source Store-V2 records.

## V0_9_10

- Runtime SQL split fix after V0_9_7_2 thin upload.
- Added date provenance columns to the Spotlight-first record review surface.
- Added the new date provenance CSV to normal exports and thin-upload required export collection.
- Fixed the iOS keyword surface source label typo from CORESPOPTLIGHT_TEXT to CORESPOTLIGHT_TEXT.
- Added Spotlight decode coverage summary, field coverage summary, human text category summary, record review, and decode-gap views.
- Proactively split large SQL raw string literals in `src\db\case_db.cpp` and `src\gui\win32_gui.cpp` below the MSVC-sensitive range.
- Updated generated reuse-cache CLI packaging script to pass `--skip-container-hash` by default for development/test reuse-cache runs.
- Fixed MSVC GUI build failure `src\gui\win32_gui.cpp(2580): error C2026: string too big` by splitting oversized embedded SQL raw string literals.

## V0_9_9

- Runtime SQL split fix after V0_9_7_2 thin upload.
- Added date provenance columns to the Spotlight-first record review surface.
- Fixed the iOS keyword surface source label typo from CORESPOPTLIGHT_TEXT to CORESPOTLIGHT_TEXT.
- Added the new date provenance CSV to normal exports and thin-upload required export collection.
- Added Spotlight decode coverage summary, field coverage summary, human text category summary, record review, and decode-gap views.
- Proactively split large SQL raw string literals in `src\db\case_db.cpp` and `src\gui\win32_gui.cpp` below the MSVC-sensitive range.
- Updated generated reuse-cache CLI packaging script to pass `--skip-container-hash` by default for development/test reuse-cache runs.
- Fixed MSVC GUI build failure `src\gui\win32_gui.cpp(2580): error C2026: string too big` by splitting oversized embedded SQL raw string literals.

## V0_9_8

- Runtime SQL split fix after V0_9_7_2 thin upload.
- Added date provenance columns to the Spotlight-first record review surface.
- Added the new date provenance CSV to normal exports and thin-upload required export collection.
- Fixed the iOS keyword surface source label typo from CORESPOPTLIGHT_TEXT to CORESPOTLIGHT_TEXT.
- Added Spotlight decode coverage summary, field coverage summary, human text category summary, record review, and decode-gap views.
- Proactively split large SQL raw string literals in `src\db\case_db.cpp` and `src\gui\win32_gui.cpp` below the MSVC-sensitive range.
- Updated generated reuse-cache CLI packaging script to pass `--skip-container-hash` by default for development/test reuse-cache runs.
- Fixed MSVC GUI build failure `src\gui\win32_gui.cpp(2580): error C2026: string too big` by splitting oversized embedded SQL raw string literals.

## V0_9_7_3

- Runtime SQL split fix after V0_9_7_2 thin upload.
- Fixed the iOS keyword surface source label typo from CORESPOPTLIGHT_TEXT to CORESPOTLIGHT_TEXT.
- Added Spotlight decode coverage summary, field coverage summary, human text category summary, record review, and decode-gap views.
- Proactively split large SQL raw string literals in `src\db\case_db.cpp` and `src\gui\win32_gui.cpp` below the MSVC-sensitive range.
- Updated generated reuse-cache CLI packaging script to pass `--skip-container-hash` by default for development/test reuse-cache runs.
- Fixed MSVC GUI build failure `src\gui\win32_gui.cpp(2580): error C2026: string too big` by splitting oversized embedded SQL raw string literals.
- Fixed V0_9_7_2 runtime failure during `open_sqlite`: SQLite received a literal `R"SQL(` marker from the prior raw-string split and failed near `R`.
- Removed nested raw-string delimiter text from the generated SQL blocks in `src/db/case_db.cpp` and `src/gui/win32_gui.cpp` while preserving the Spotlight-first iOS review views.

## V0_9_7_2

- Fixed the iOS keyword surface source label typo from CORESPOPTLIGHT_TEXT to CORESPOTLIGHT_TEXT.
- Added Spotlight decode coverage summary, field coverage summary, human text category summary, record review, and decode-gap views.
- Proactively split large SQL raw string literals in `src\db\case_db.cpp` and `src\gui\win32_gui.cpp` below the MSVC-sensitive range.
- Updated generated reuse-cache CLI packaging script to pass `--skip-container-hash` by default for development/test reuse-cache runs.
- Fixed MSVC GUI build failure `src\gui\win32_gui.cpp(2580): error C2026: string too big` by splitting oversized embedded SQL raw string literals.
- Added Spotlight-first iOS review views and exports so investigator review starts with CoreSpotlight records, recovered text values, decode coverage, field coverage, and decode gaps.
- GUI compile fix / reuse-cache script speed default.
- Spotlight-first iOS review and decode coverage focus.

## V0_9_7_1

- Fixed the iOS keyword surface source label typo from CORESPOPTLIGHT_TEXT to CORESPOTLIGHT_TEXT.
- Added Spotlight decode coverage summary, field coverage summary, human text category summary, record review, and decode-gap views.
- Added Spotlight-first iOS review views and exports so investigator review starts with CoreSpotlight records, recovered text values, decode coverage, field coverage, and decode gaps.
- Spotlight-first iOS review and decode coverage focus.
- Reaffirmed that FFS/app-database correlation is supporting context; the primary evidence surface is Spotlight/CoreSpotlight parsing and review.

## V0_9_7

- Fixed the iOS keyword surface source label typo from CORESPOPTLIGHT_TEXT to CORESPOTLIGHT_TEXT.
- Added Spotlight decode coverage summary, field coverage summary, human text category summary, record review, and decode-gap views.
- Added Spotlight-first iOS review views and exports so investigator review starts with CoreSpotlight records, recovered text values, decode coverage, field coverage, and decode gaps.
- Spotlight-first iOS review and decode coverage focus.
- Reaffirmed that FFS/app-database correlation is supporting context; the primary evidence surface is Spotlight/CoreSpotlight parsing and review.

## V0_9_6

- New runtime diagnostics: logs/ios_zip_stage_heartbeat.log and logs/ios_zip_inventory_progress.tsv.
- Targeted app database extraction now starts before the full FFS inventory stream and duplicate extraction is skipped.
- V0_9_6 addresses apparent hangs during stage_zip_source on very large iOS FFS ZIP files by streaming 7-Zip inventory output to CSV and writing heartbeat/progress logs.

## V0_9_5

- New runtime diagnostics: logs/ios_zip_stage_heartbeat.log and logs/ios_zip_inventory_progress.tsv.
- Targeted app database extraction now starts before the full FFS inventory stream and duplicate extraction is skipped.
- V0_9_5 addresses apparent hangs during stage_zip_source on very large iOS FFS ZIP files by streaming 7-Zip inventory output to CSV and writing heartbeat/progress logs.
- Forensic interpretation remains conservative: Spotlight communication candidates are candidate strings only unless independently supported by parsed app database rows, FFS path matches, or stronger corroboration.
- V0_9_5 also improves WhatsApp status reporting. If no iOS WhatsApp ChatStorage/Contacts/CallHistory database is found in the FFS inventory, the WhatsApp status view now returns an explicit `WHATSAPP_DB_NOT_FOUND` row instead of an empty result set.

## V0_9_4

- New runtime diagnostics: logs/ios_zip_stage_heartbeat.log and logs/ios_zip_inventory_progress.tsv.
- Targeted app database extraction now starts before the full FFS inventory stream and duplicate extraction is skipped.
- V0_9_4 addresses apparent hangs during stage_zip_source on very large iOS FFS ZIP files by streaming 7-Zip inventory output to CSV and writing heartbeat/progress logs.

## V0_9_3

- Updated user documentation, release notes, validation notes, and roadmap material.
- Added GUI views for.
- V0_9_3 implemented changes.
- Source version strings updated to 0.9.3.
- Added iOS Web History Review and Summary views/exports.
- Added unified iOS communications review views and exports.
- Added the new CSVs to the thin-upload required export list.
- Added iOS Contact Identity Review and Summary views/exports.

## V0_9_2

- Added GUI views for.
- Added unified iOS communications review views and exports.
- Updated GUI hover text for the new iOS communications/keychain views.
- Added exports and thin-upload packaging support for the new communications and keychain-support views.
- Added iOS - Keychain Support References for lower-priority keychain-named framework/code references outside core keychain/keybag locations.
- Preserved cautious forensic language: Spotlight communication candidates are not treated as live messages/calls/chats unless supported by parsed app database rows or exact file/database links.
- V0_9_2 changes.
- Known limitations.

## V0_9_1

- Added schema-specific SMS.db participant parsing for `handle` and `chat` tables.
- Updated scripts and documentation paths to V0_9_1 / `Q:\SpotlightCase\TestiOS_V0_9_1`.
- Added cautious handling for recoverable/deleted Messages-related tables by classifying them as `MESSAGE_DELETED_OR_RECOVERABLE` for investigator review without asserting deletion.
- Added mouse-hover explanations for the left-side review view list used by both MacOS Investigation View and iOS Investigation View. The help text explains what each view shows, key interpretation limits, and cautious residency/date language.
- GUI launches and the MacOS/iOS review view list shows hover explanations.
- `ios_app_database_record_inventory.csv` still reports SMS.db table counts.
- `VestigantSpotlightCli.exe --version` reports `Vestigant Spotlight v0.9.1`.
- Focus: iOS Apple Messages/SMS.db parsing improvements and GUI review-view hover explanations.

## V0_9_0

- Added schema-specific SMS.db participant parsing for `handle` and `chat` tables.
- Updated scripts and documentation paths to V0_9_0 / `Q:\SpotlightCase\TestiOS_V0_9_0`.
- Added cautious handling for recoverable/deleted Messages-related tables by classifying them as `MESSAGE_DELETED_OR_RECOVERABLE` for investigator review without asserting deletion.
- Added mouse-hover explanations for the left-side review view list used by both MacOS Investigation View and iOS Investigation View. The help text explains what each view shows, key interpretation limits, and cautious residency/date language.
- GUI launches and the MacOS/iOS review view list shows hover explanations.
- `ios_app_database_record_inventory.csv` still reports SMS.db table counts.
- `VestigantSpotlightCli.exe --version` reports `Vestigant Spotlight v0.9.0`.
- Focus: iOS Apple Messages/SMS.db parsing improvements and GUI review-view hover explanations.

## V0_8_99

- Added platform-aware review-view filtering in the Win32 GUI.
- Updated `vw_ios_spotlight_residency_summary` so file-presence counts come from the all-path object-link view, not only the missing-candidates view.
- Fixed `vw_ios_spotlight_to_ffs_object_links` so it reports all resolvable iOS Spotlight path references and their FFS residency status, including `PRESENT_AS_FILE_IN_FFS` matches.
- GUI iOS ingest completes.
- Windows/MSVC build completes.
- Focus: GUI tab containment hotfix for iOS investigation views.
- The MacOS Investigation View no longer lists iOS-only views whose display names begin with `iOS`.
- Switching back to the MacOS Investigation View resets the visible review list to non-iOS/macOS views.

## V0_8_98_1

- Kept iOS views in the iOS Investigation tab and kept release/history documentation consolidated.
- Preserved the main V0_8_89 iOS FFS ZIP inventory fixes: safe slash-based ZIP-entry parsing, 7-Zip `l -slt` inventory, sampled thin-upload CSV handling, and consolidated release/history docs.
- Added platform-aware review-view filtering in the Win32 GUI.
- Updated build/run/package scripts and documentation to V0_8_98_1 paths.
- Updated quick diagnostic PowerShell with the same safe ZIP-entry parsing logic.
- Preserved the V0_8_90 parsed app database row extraction behavior; no intended Store-V2 parser behavior change.
- Added table/view/export `ios_app_database_record_summary.csv` for summarized database-resident artifact families.
- Added `extracted_path` to `ios_app_database_inventory` so later parsing can operate on staged database copies rather than the source ZIP.

## V0_8_92

- Kept iOS views in the iOS Investigation tab and kept release/history documentation consolidated.
- Preserved the main V0_8_89 iOS FFS ZIP inventory fixes: safe slash-based ZIP-entry parsing, 7-Zip `l -slt` inventory, sampled thin-upload CSV handling, and consolidated release/history docs.
- Updated build/run/package scripts and documentation to V0_8_92 paths.
- Updated quick diagnostic PowerShell with the same safe ZIP-entry parsing logic.
- Preserved the V0_8_90 parsed app database row extraction behavior; no intended Store-V2 parser behavior change.
- Added table/view/export `ios_app_database_record_inventory.csv` for SQLite table names, row counts, sample columns, and table categories.
- Added `extracted_path` to `ios_app_database_inventory` so later parsing can operate on staged database copies rather than the source ZIP.

## V0_8_89_1

- Added `ios_app_parsed_records` to the case database with source database, table, category, timestamp, participant/contact, URL, title, file path, identifier, snippet, status, and provenance fields.
- Current limitation.
- Windows/MSVC build completes and the CLI reports `Vestigant Spotlight v0.8.89.1`.
- Full iOS GUI ingest logs `parsed_app_records=<nonzero>` when supported staged app databases contain target rows.
- Thin upload packaging now includes the parsed app-record summary and samples the potentially large parsed-record CSV.
- Focus: move the iOS app-database stage from table counts toward investigator-reviewable records while keeping conservative forensic wording.
- This is a generic, schema-tolerant parser pass. It does not yet implement app-specific deleted/live semantics, message-thread reconstruction, attachment file carving, or exact Spotlight-to-app-row correlation.
- Database residency candidates may report `POTENTIAL_PARSED_APP_RECORDS_AVAILABLE`; this means relevant parsed app records exist for the database family, not that a specific CoreSpotlight string has been row-matched.

## V0_8_89

- Kept iOS views in the iOS Investigation tab and kept release/history documentation consolidated.
- Updated quick diagnostic PowerShell with the same safe ZIP-entry parsing logic.
- Added table/view/export `ios_app_database_record_inventory.csv` for SQLite table names, row counts, sample columns, and table categories.
- Added `extracted_path` to `ios_app_database_inventory` so later parsing can operate on staged database copies rather than the source ZIP.
- Updated quick iOS diagnostics to use the same 7-Zip inventory approach and to collect FFS/app-database inventory without requiring a full GUI ingest.
- Updated thin upload packaging so very large iOS CSVs are sampled in the upload ZIP while full CSVs remain in the local case folder. This is intended to prevent the FFS inventory from making the upload bundle excessively large.
- Fixed the iOS FFS ZIP inventory failure seen in V0_8_88_1 where the generated extractor logged `ios_ffs_inventory_7z_list_error=Exception calling "GetFileName" ... Illegal characters in path` and therefore imported zero FFS/app database inventory rows.

## V0_8_88_1

- Kept iOS views in the iOS Investigation tab and kept release/history documentation consolidated.
- Added table/view/export `ios_app_database_record_inventory.csv` for SQLite table names, row counts, sample columns, and table categories.
- Added `extracted_path` to `ios_app_database_inventory` so later parsing can operate on staged database copies rather than the source ZIP.
- Updated quick iOS diagnostics to use the same 7-Zip inventory approach and to collect FFS/app-database inventory without requiring a full GUI ingest.
- Limitations.
- CoreSpotlight dbStr/property-map decoding remains a future parser phase.
- Database residency candidates now distinguish database-family presence from the presence of relevant record tables.

## V0_8_88

- Kept iOS views in the iOS Investigation tab and kept release/history documentation consolidated.
- Added table/view/export `ios_app_database_record_inventory.csv` for SQLite table names, row counts, sample columns, and table categories.
- Added `extracted_path` to `ios_app_database_inventory` so later parsing can operate on staged database copies rather than the source ZIP.
- Updated quick iOS diagnostics to use the same 7-Zip inventory approach and to collect FFS/app-database inventory without requiring a full GUI ingest.
- Limitations.
- CoreSpotlight dbStr/property-map decoding remains a future parser phase.
- Database residency candidates now distinguish database-family presence from the presence of relevant record tables.

## V0_8_87

- V0_8_87 upload review.
- iOS CoreSpotlight ingest still completed successfully.
- Windows/MSVC build succeeded and produced GUI, CLI, and test binaries.
- Parsed iOS outputs remained populated: 5 selected store groups/databases, 37,719 raw records, 15,104 raw key/value rows, 37,719 artifacts, and 37,719 index-timeline rows.

## V0_8_86

- Added iOS investigator pivots for protection class summary, artifact hint summary, and per-record investigation hints. Added corresponding GUI views, exports, upload packaging, and consolidated release/history documentation.

## V0_8_85_1

- Compile hotfix for V0_8_85. Split oversized SQLite SQL raw string literals in `src\db\case_db.cpp` to avoid MSVC `C2026`.

## V0_8_85

- Added iOS record string-probe summary export/view, corrected iOS store parse summary placeholder accounting, and removed caps from main iOS timeline and string-probe exports.

## V0_8_84

- Changed iOS Store-V2 database selection to parse one primary database per CoreSpotlight group, preferring `store.db` while preserving `.store.db` alternates in inventory/hash outputs.

## V0_8_83

- Fixed quick-diagnostic ZIP packaging issues and added redacted iOS investigation summaries/domain URL summaries and cleaner Upload packaging for iOS CSVs.

## V0_8_82

- Consolidated release/history documentation and added fast iOS diagnostic collection intended to avoid full GUI ingest when only ZIP/CoreSpotlight inventory is needed.

## V0_8_81

- Added iOS string-probe fallback preservation and improved iOS tab status reporting after focused CoreSpotlight parsing.

## V0_8_80

- Manual, quick start, troubleshooting, and release notes updated.
- Version references updated to 0.8.80.
- V0_8_80 changes.
- Windows/MSVC build.
- Issue classification.
- Full GUI iOS FFS ZIP run.
- Full APFS external comparison.
- Expected V0_8_80 success indicators.

## V0_8_79

- Release notes/manual quick-start/troubleshooting docs updated for V0_8_79.
- V0_8_79 moved past that and reached focused iOS extraction.
- V0_8_79 changes.
- Issue classification.
- Reviewed uploaded V0_8_79 iOS GUI attempt.
- Expected V0_8_79 GUI iOS success indicators.
- logs/ios_focused_zip_extract.log should exist.
- focused iOS extraction reached, but .NET ZIP enumeration did not match entries.

## V0_8_78

- Root cause classification.
- GUI/source-profile routing issue.
- V0_8_78 issue was generic ZIP staging / profile routing.
- V0_8_78 logs show generic ZIP staging, not the iOS focused ZIP extraction route.
- The GUI run likely remained profile=auto, so the ZIP source followed generic ZIP staging.
- Bounded byte signature scan did not see CoreSpotlight within the first 64 MiB, so auto profile was not promoted before staging.

## V0_8_59

- Workflow.
- The uploaded focused evidence contains two samples.
- Parse the focused ZIP/folder with `--profile ios --decode-core-native-values`.
- Compare store discovery, store selection, raw record counts, parser failures, and high-value field coverage by protection class.
- Add an iOS-specific CoreSpotlight parser route only if the existing Store-V2 parser cannot parse enough of the focused `.store.db` data.
- Next parser work: decode iOS CoreSpotlight metadata values beyond header/core-probe rows, incorporate BundleInfo app identity context, and preserve protection-class labels as first-class review columns.
- `1f1ce9a08328644d471f4f90ae79ef81c4e22164_files_full` with populated `NSFileProtectionComplete`, `NSFileProtectionCompleteUnlessOpen`, and `NSFileProtectionCompleteUntilFirstUserAuthentication` CoreSpotlight indexes.
- `EXTRACTION_FFS` with populated `NSFileProtectionComplete`, `NSFileProtectionCompleteUnlessOpen`, `NSFileProtectionCompleteUntilFirstUserAuthentication`, `NSFileProtectionCompleteWhenUserInactive`, `Priority`, and SpotlightKnowledge-related files.

## V0_8_13_2

- Build-helper hotfix for aff4-cpp-lite under VS2022.
- Preserves retargeting from Windows SDK 8.1/v140 to installed Windows 10/11 SDK/v143.
- Restores the legacy native `lz4.1.3.1.2` package required by `libaff4.vcxproj` before MSBuild.
- No intended parser, GUI, source-probe, APFS/HFS, or database behavior changes.

## V0_8_13_1

- Added explicit MSBuild properties for `WindowsTargetPlatformVersion` and `PlatformToolset`.
- Updated `tools/Build-Aff4CppLite-VS2022.ps1` to retarget aff4-cpp-lite Windows projects from legacy Windows SDK 8.1 / v140 settings to an installed Windows 10/11 SDK and VS2022 v143 by default.
- No parser, source-probe, export, GUI, or database behavior changes.

## V0_8_12

- Added controlled AFF4 stream inventory harness for source-probe mode when `aff4imager` is available.
- Keeps AFF4 stream extraction, APFS/HFS/HFS+ filesystem enumeration, Spotlight staging from images, and active-file comparison classification unimplemented until a selected stream can be validated.
- Adds stream-inventory files to the thin Upload bundle and review index.

## V0_8_10

- Added Windows shared-read file access for source hashing and source-probe byte scanning, using read/share modes intended for large AFF4/image files.
- Source-probe `bytes_scanned` now reports actual bytes read.
- Replaced whole-file-in-memory SHA256 with streaming SHA256 for large evidence containers.
- Source-probe now logs concrete open/read warnings when a file can be stat'ed but not read.
- Full ZIP Spotlight parsing remains available as an optional regression test, but is no longer the default iteration test for AFF4/APFS reader work.
- No AFF4 stream enumeration, APFS filesystem enumeration, or Spotlight extraction from AFF4/APFS is claimed in this version.

## V0_8_9_1

- Added validation scripts using the user's AFF4 test image path: `K:\test\0109_0142-IT001.aff4`.
- Compile hotfix for V0.8.9 MSVC C2026 in `src\db\case_db.cpp`; split the large SQL/view initialization block into smaller raw-string fragments.
- No intended database schema, parser, source-probe, export, GUI, AFF4/APFS readiness, or active-file-comparison behavior changes.

## V0_8_9

- Added minimal/full exports and upload samples for image inventory and active-file comparison readiness.
- Added GUI investigation views for image inventory sources, image file inventory, active comparison readiness, and Spotlight-vs-image comparison.
- Prioritized AFF4 + APFS as the main image-source path for Spotlight-containing forensic images.
- Active-file comparison remains readiness/data-model support until a validated AFF4/APFS reader populates `image_file_inventory`.
- No AFF4 stream enumeration, APFS filesystem enumeration, or Spotlight extraction from images is claimed in this version.

## V0_8_8

- Added APFS/HFS/HFS+ signature checks at reported partition offsets when possible.
- Added raw-image partition readiness probing for MBR/protective MBR and GPT entries.
- Added source-probe inventory/signature/partition CSV exports to the minimal/investigator export profile and upload samples.
- Added SQLite source-probe inventory tables/views and GUI view entries for source intake, signature hits, and partition readiness.
- AFF4 stream enumeration, raw filesystem enumeration, APFS/HFS/HFS+ extraction, and Spotlight extraction from image files remain explicitly unsupported.

## V0_8_7_2

- Preserved the V0.8.7.1 CLI/source-probe compile fixes.
- Corrected stale package `VERSION` and `VERSION.txt` metadata from 0.8.3 to 0.8.7.2.
- Added `NOMINMAX` and local macro cleanup around Windows header inclusion in app/CLI code paths.
- Fixed remaining GUI MSVC compile failure in `src\gui\win32_gui.cpp` caused by ambiguous `std::max` overloads between `LONG` and `int`.
- Compile hotfix for V0.8.7 MSVC failures in `src\app\app_runner.cpp` around `std::min`, `std::max`, and `std::numeric_limits<...>::max()` after Windows header macro expansion.
- No parser or export behavior changes.
- No intended parser, enrichment, export, GUI review, tagging, checked-state, or source-probe behavior changes.

## V0_8_7_1

- Corrected stale package `VERSION` and `VERSION.txt` metadata from 0.8.3 to 0.8.7.1.
- Added `NOMINMAX` and local macro cleanup around Windows header inclusion in app/CLI code paths.
- Compile hotfix for V0.8.7 MSVC failures in `src\app\app_runner.cpp` around `std::min`, `std::max`, and `std::numeric_limits<...>::max()` after Windows header macro expansion.
- No intended parser, enrichment, export, GUI review, tagging, checked-state, or source-probe behavior changes.

## V0_8_7

- Added bounded source signature probing for AFF4/raw/image-style source intake.
- Source readiness CSV now includes probe byte count, truncation status, signature count, filesystem hints, and Spotlight/CoreSpotlight path hints.
- Source intake plan now lists detected source signatures and explicitly separates low-confidence byte hints from validated container/filesystem extraction.
- No intended changes to native Store-V2 parsing, enrichment, tagging, GUI review, or targeted export SQL.
- No AFF4 stream enumeration, raw partition parsing, APFS/HFS/HFS+ filesystem enumeration, or image extraction is claimed in this version.

## V0_8_6

- Added AFF4 and raw flat-image identification/registration scaffolding.
- Added source-probe/source-intake mode for evidence source readiness reporting without parser/enrichment execution.
- AFF4/raw inputs are hashed/registered as original containers but remain unsupported for extraction until future container/filesystem reader layers are implemented.
- Included source-intake outputs in the focused Upload bundle.
- No intended changes to native Store-V2 parsing, enrichment, tagging, or targeted export SQL.

## V0_8_5

- Targeted export and upload polish release after V0.8.4 compiled and ran successfully with full native metadata values.
- Updated targeted export documentation/examples for artifact, checked, and tag workflows.
- Added targeted modes for persisted checked artifacts and selected tag exports: `-Mode checked` and `-Mode tag -TagName <name>`.
- Preserved V0.8.1 full-validation guardrails, V0.8.2 async investigation loading, V0.8.3 persistent checked-artifact storage/export behavior, and V0.8.4 GUI layout/autosave behavior.
- Targeted exports now include header rows consistently.
- `-Mode artifact` now accepts multiple artifact IDs separated by commas, semicolons, or whitespace.

## V0_8_4

- GUI layout and autosave release focused on safe presentation-layer improvements after V0.8.3 compiled and ran correctly.
- Added an autosave status indicator beside the manual Save Case Info button.
- Increased default window size and added a minimum window size to reduce clipped controls.
- Added Segoe UI font application and a Common Controls v6 manifest dependency for more current Windows visual styling.
- Preserved V0.8.1 full-validation guardrails, V0.8.2 async investigation loading, and V0.8.3 persistent checked-artifact storage/export behavior.
- Added resize handling so the tab frame, investigation result grid, status text, log area, Tags / Notes list, and iOS planning panels expand when the main window is enlarged.
- Added case-information autosave: Case Name, Case Number, Investigator, Company, Case Location, and Case Database changes are saved back to the case SQLite database after a short idle delay and again on close.

## V0_8_3

- Added checked-artifact upload sample support through `checked_artifacts_focus.csv`.
- Added Review-tab buttons to open the Case Folder, Upload Folder, and Logs folder directly.
- Preserved full-validation guardrails and async investigation-view behavior from V0.8.1/V0.8.2.
- Added persistent checked-artifact storage in the SQLite case database via `gui_checked_artifacts`.
- Added `vw_checked_artifacts` and MacOS Investigation View entry `Investigator - Checked Artifacts`.
- Fixed the V0.8.2 MSVC `/` escape warning and removed duplicate checked-artifact count text from async page-load summaries.

## V0_8_2

- Added V0.8.2 release notes, patch summary, validation notes, and full-validation PowerShell script.
- Added Cancel Load button for long investigation-view loads.
- Added background MacOS Investigation View page loading so SQLite page queries no longer execute directly on the UI thread.
- Preserved existing one-extra-row pagination, checked-row state, and tag cell display when applying background-loaded results.
- Added stale-request cancellation for view/search/page/sort changes; older page-load results are discarded if a newer request exists.

## V0_8_1

- Added V0.8.1 release notes, patch summary, validation notes, and full-validation PowerShell script.
- Added GUI warning before running investigator/full export workflows with full native metadata disabled.
- Fixed overlapping MacOS Investigation View buttons for Dashboard / Review Index / checked export controls.
- Added run-status warnings when investigator/full exports are requested without full native metadata values.
- Added CLI `--full-validation` / `--investigator-full-validation` shortcut for full native metadata plus investigator exports.
- Clarified GUI checkbox wording for full native metadata values.

## V0_8_0

- Larger investigator workflow release after the validated 0.7.24.x parent-inode/path baseline.
- Added selected-tag export with raw support CSVs and a support manifest.
- Added tagged-artifact, artifact-note, and export-ready investigator views.
- Added upload samples for tagged/noted/export-ready artifacts and current investigator tag/note tables.
- Expanded checked-artifact export to include raw key/value, raw date, usage, and timeline support detail.

## V0_7_24_3

- Updated thin Upload review index to link to included bounded sample files where full optional diagnostics are not generated.
- Polished thin Upload manifest status handling.
- Reclassified absent optional diagnostics as `OPTIONAL_NOT_GENERATED` and absent crash logs as `NO_FATAL_CRASH_LOG`.
- No parser/schema changes.
- Kept V0_7_24_1 header-only root placeholder protections and full-native validation summary fields.

## V0_7_24_2

- Kept V0_7_24_1 header-only root placeholder protections and full-native validation summary fields.

## V0_7_24_1

- Added `native_decode_mode`, `metadata_values_decoded`, raw record count, raw key/value count, and raw date candidate count to case summary output.
- Added explicit warning status when metadata key/value rows are absent so path/name/usage/WhereFroms validation is not mistaken for a full native-values run.
- Expanded parent-inode status reporting with matched parent links and child-name counts.
- Expanded `native_parse_complete` status to include raw key/value and date candidate counts.
- Bugfix hardening after the first v0.7.24 validation upload.

## V0_7_24

- Added full export `path_reconstruction_applied.csv`.
- Added GUI views for path reconstruction and same-folder grouping.
- Added `vw_path_reconstruction` and `vw_same_folder_groups` database review views.
- Added parent-inode path reconstruction hardening after the successful V0_7_23_6 run.
- Initialized native artifact path attribution fields (`path_source`, `path_status`, `spotlight_display_path`, `normalized_mac_path`) during artifact materialization.
- Applied parent-inode reconstructed path candidates back onto weak artifact path rows when the artifact only had a file name, no usable path, or no directory context.

## V0_7_23_6

- Preserved the V0_7_23_5 materialized artifact_dates_wide export fix.
- Added granular enrichment progress/status markers around raw date attribution and artifact_date_summary materialization.
- Reconstructed the intended artifact-date status hardening build after the V0_7_23_6 download failed.
- No roadmap features were advanced in this patch.

## V0_7_23_5

- Updated GUI stage labels to display readable progress descriptions.
- Added export SQL execution status and early CSV header flush for visible progress.
- Added explicit enrichment-complete, export query, upload sample, export complete, and focused Upload-folder progress/status stages.
- Targeted GUI ingest-progress/status fix after V0_7_23_2 testing showed the GUI could appear stuck at enrichment/90% while backend logs had already moved to export.
- Fixed object_usage_summary export stall by simplifying the object usage view to use enriched date association fields and usage-bearing candidates instead of re-entering full date-attribution/date-wide views.
- Kept V0_7_23 object/date association hardening and V0_7_23_2 MSVC compile fixes intact.

## V0_7_23_4

- Updated GUI stage labels to display readable progress descriptions.
- Added export SQL execution status and early CSV header flush for visible progress.
- Added explicit enrichment-complete, export query, upload sample, export complete, and focused Upload-folder progress/status stages.
- Targeted GUI ingest-progress/status fix after V0_7_23_2 testing showed the GUI could appear stuck at enrichment/90% while backend logs had already moved to export.
- Fixed object_usage_summary export stall by simplifying the object usage view to use enriched date association fields and usage-bearing candidates instead of re-entering full date-attribution/date-wide views.
- Kept V0_7_23 object/date association hardening and V0_7_23_2 MSVC compile fixes intact.

## V0_7_23_3

- Updated GUI stage labels to display readable progress descriptions.
- Added explicit enrichment-complete, export query, upload sample, export complete, and focused Upload-folder progress/status stages.
- Targeted GUI ingest-progress/status fix after V0_7_23_2 testing showed the GUI could appear stuck at enrichment/90% while backend logs had already moved to export.
- Kept V0_7_23 object/date association hardening and V0_7_23_2 MSVC compile fixes intact.

## V0_7_23_2

- Patch release for MSVC compile failure in V0_7_23.
- Split oversized `case_db.cpp` raw SQL string literal at view boundaries.
- Updated `build_windows_msvc.bat` to stop immediately after common object compile failures.

## V0_7_23_1

- Patch release for MSVC compile failure in V0_7_23.
- Split oversized `case_db.cpp` raw SQL string literal at view boundaries.
- Updated `build_windows_msvc.bat` to stop immediately after common object compile failures.

## V0_7_23

- Populates raw date candidate object-association fields during SQLite enrichment.
- Retains V0_7_22 review performance/export behavior and Folder/ZIP evidence-source semantics.
- Fixes GUI review filtering duplicate local variable and adds schema migration for raw date association fields before review indexes are created.
- Adds object-centric date summary view and expands date/event attribution with association status, confidence, object context, and snapshot warning detail.

## V0_7_20

- Explicit Folder/ZIP/AFF4/raw source-type guidance, with AFF4/raw blocked until implemented.
- GUI workflow/performance polish.
- Larger ingest status/log presentation.
- Additional SQLite indexes for date/timeline/artifact review views.

## V0_7_19

- Added integrated ZIP staging before Store-V2 discovery.
- Corrected preservation semantics for already-containerized sources.
- Added explicit source selector: Folder, ZIP, AFF4 future, Raw IMG/DD future.
- Changed investigation view paging to avoid unconditional COUNT(*) over large views.
- Enlarged ingest status/log area.
- GUI intake clarity/performance build.
- Hidden legacy V7 views from the GUI view list.
- Hid legacy V7 GUI input path while preserving CLI legacy mode.

## V0_7_18

- Added integrated ZIP staging before Store-V2 discovery.
- Added explicit source selector: Folder, ZIP, AFF4 future, Raw IMG/DD future.
- Changed investigation view paging to avoid unconditional COUNT(*) over large views.
- Enlarged ingest status/log area.
- GUI intake clarity/performance build.
- Hid legacy V7 GUI input path while preserving CLI legacy mode.
- Reaffirmed date/timeline events must remain associated with artifact/object context where possible.

## V0_7_17

- Preserved object-centric Usage Timeline, Object Usage Summary, raw usage-event detail view, and all V0_7_15.x build hotfixes.
- Added parser progress output files for long Store-V2 parses.
- Added GUI ingest progress bar and percentage/status monitoring.
- Added CLI logging that explicitly records unlimited native record/block settings.
- Added focused/standard/full-thin upload helper profiles, with Focused as the default.
- Added forced case output location selection before GUI ingest can begin when the case location is blank.
- Added an ingest status indicator on the Case Information tab, including live polling of `logs/run_status.txt` stage lines while ingest is running.
- Changed GUI defaults from bounded diagnostics to full raw Spotlight processing: Process Raw Spotlight Evidence mode, blank/unlimited max records, blank/unlimited blocks per store, and Investigator export profile.

## V0_7_16

- Preserved object-centric Usage Timeline, Object Usage Summary, raw usage-event detail view, and all V0_7_15.x build hotfixes.
- Added CLI logging that explicitly records unlimited native record/block settings.
- Added forced case output location selection before GUI ingest can begin when the case location is blank.
- Added an ingest status indicator on the Case Information tab, including live polling of `logs/run_status.txt` stage lines while ingest is running.
- Changed GUI defaults from bounded diagnostics to full raw Spotlight processing: Process Raw Spotlight Evidence mode, blank/unlimited max records, blank/unlimited blocks per store, and Investigator export profile.
- No parser logic, iOS parser, IMG/AFF4, APFS, HFS, or HFS+ extraction changes.

## V0_7_15_7

- Updated minimal exports.
- Added GUI view `Investigator - Usage Event Details (Raw)`.
- Added `vw_usage_event_detail_attributed` for raw per-date/per-field usage event detail when the investigator needs the supporting rows.
- Preserved artifact-level tagging behavior. Tagging a usage row should apply to the artifact/object, not to each repeated usage-date row.
- Reworked `vw_usage_timeline_attributed` to return one row per usage-bearing artifact/object.
- Kept the investigator-facing view name `Investigator - Usage Timeline`, but its rows now summarize the object and aggregate its usage metadata.
- V0_7_15_7 changes the investigator-facing Usage Timeline view to be object-centric so the same artifact/file is not shown repeatedly as separate tag targets merely because Spotlight exposed multiple usage/date fields for the same object.
- No parser logic changes.

## V0_7_15_5

- Preserved the successful V0_7_15_4 CLI/test compile and `case_db.cpp` string-size hotfix.
- Preserved Visual Studio environment detection fix from 0.7.15.3 and response-file linker fix from 0.7.15.2.
- Fixed MSVC `C2026: string too big` in `src\gui\win32_gui.cpp` by splitting the large GUI review-view SQL raw string into smaller SQLite execution blocks.
- No parser, GUI behavior, export, staging, object-usage, IMG/AFF4, APFS, HFS, or HFS+ logic changes.

## V0_7_15_2

- Windows no-CMake build hotfix.
- Links CLI/tests/GUI with explicit response files so CaseDatabase, SqlStatement, and CaseStore implementation objects are included reliably.
- No parser, GUI, staging, or database behavior changes.

## V0_7_15_1

- Added per-source compile progress so the build does not appear frozen at CLI creation.
- Build-system hotfix for V0_7_15.
- Reworked the no-CMake MSVC batch build to compile common source files once into object files.
- No parser, GUI, export, or database behavior changes.

## V0_7_15

- Added GUI view `Investigator - Object Usage Summary`.
- Added default/full export `object_usage_summary.csv`.
- Added upload sample `object_usage_summary_focus.csv`.
- Added object-centric usage summary view `vw_object_usage_summary`.
- Updated dashboard usage section to show one row per artifact/object with filename/path and fused usage metadata.
- Carried forward the visible blue `OPEN CASE` button change.

## V0_7_14

- V0_7_14 should build on this by preparing the iOS CoreSpotlight intake route, including iOS evidence source tables, CoreSpotlight locator/reporting, and an iOS parser interface skeleton.

## V0_7_13_1

- Fixed `tools\Stage-EvidenceSource.ps1` on systems where Robocopy rejects `/DCOPY:DAT`.
- Removed `/MT:8` from the default folder-copy command because older Robocopy builds may not support multithreaded copy syntax.
- Run folder staging again against.
- The staging script now uses conservative Robocopy options.
- Expected result: controlled staging completes and writes inventory outputs under.
- V0_7_13_1 is a script-only compatibility hotfix for the V0_7_13 evidence-source staging workflow.
- No GUI logic changes.
- No IMG/AFF4 extraction.

## V0_7_13

- Added evidence-source staging and ZIP intake helpers under `tools/`.
- Added APFS/HFS/HFS+ design warning for later IMG/AFF4 research builds.
- Added explicit unsupported registration for IMG/DD/RAW and AFF4 sources.
- Added source inventory outputs for JSON, CSV, SQL, and text detection reports.
- Added deterministic manifests and SHA256 provenance records for folder/ZIP staging.
- No parser or GUI behavior changes.

## V0_7_12

- Added an iOS Investigation View readiness tab.
- Added evidence-source staging roadmap documentation for ZIP, iOS FFS ZIP, IMG/DD, and AFF4 sources.
- Added right-click tag submenus with available tag names for row, selected-row, and checked-row tag actions.
- Renamed the investigation tab to MacOS Investigation View.
- No parser changes.

## V0_7_11

- Added left-side investigation view list.
- Added database-backed tag and note tables.
- Added existing Tags as the second Investigation View column.
- Added persistent checked-row workflow to the Investigation View.
- Added older-case review-view upgrade and available-view filtering.
- Added right-click row tagging actions and checked-row bulk tag operations.
- Added artifact-based tag propagation across views where artifact association is available.
- GUI workflow redesign to Case Information / Investigation View / Tags-Notes.

## V0_7_8

- Review usability release: explicit date attribution, snapshot/index-date warnings, minimal export profile default, and first-pass GUI sorting/filtering.

## V0_7_7

- Preserved thin-upload and local-only database model.
- Added GUI review buttons for dashboard and review index.
- Added database-backed investigator SQLite views for GUI review.
- Updated Win32 GUI process controls for diagnostics/full-native bounded parsing.

## V0_7_6

- Added bounded investigator_dashboard.html for immediate Mac Spotlight review.
- Dashboard is included in thin Upload; full SQLite and large CSVs remain local-only.
- Dashboard includes usage, recent activity, WhereFroms, content type, parent-inode/folder, volume, and date-field pivots.

## V0_7_5

- Corrected generated targeted-export README examples to avoid ambiguous PowerShell script paths.
- Added Upload-specific review index that links only thin-upload files actually copied into Upload.
- Retained verified Upload ZIP helper and database-exclusion safeguards from v0.7.4.
- Improved targeted-export helper so it can be launched from either the case root or the copied Upload folder.

## V0_7_4

- Updated review/readme outputs to describe the verified ZIP workflow.
- Added generated `Create-UploadZip.ps1` helper to create and verify Upload ZIP archives without wildcard `Compress-Archive` behavior.
- Copied the ZIP helper into the thin Upload bundle for reuse.

## V0_7_3

- Added `TARGETED_EXPORT_README.txt`.
- Added generated `Export-SpotlightTargetedData.ps1` local SQLite slice exporter.
- Added bounded investigator pivot CSVs for content types, store/content types, folder activity, recent activity, and volume root indicators.
- Kept the full SQLite database local-only and continued the thin-upload model.

## V0_7_2

- Fixed v0.7.1 thin-upload SQL failure in `timeline_usage_focus.csv`.
- Changed thin-upload sample generation to non-fatal so review files and Upload bundle are still generated if a sample export fails.
- Reduced Upload bundle contents so large full-case CSV exports and SQLite databases remain local by default.

## V0_7_1

- Added CASE_REVIEW_SUMMARY.txt and review_index.html.
- Added spotlight_case.db convenience copy of the SQLite case database.
- Mac Spotlight Investigator MVP.
- Active filesystem comparison remains disabled.

## V0_7_0

- Added CASE_REVIEW_SUMMARY.txt and review_index.html.
- Added spotlight_case.db convenience copy of the SQLite case database.
- Mac Spotlight Investigator MVP.
- Active filesystem comparison remains disabled.

## V0_6_7_1

- Safe full-native enrichment cleanup: string trailer stripping, parsed-UTC-only timeline rows, timeline rejected-candidate export, and native usage-summary hydration. Active filesystem comparison remains tabled.

## V0_6_7

- Changed native metadata block limit to per-store behavior.
- Preserved fatal crash logs under `Upload/logs/FATAL_CRASH.txt`.
- Added guarded date conversion, invalid-parameter crash logging, native block limit, CLI aliases, and automatic Upload bundle.
- Sanitized CSV exports for invalid UTF-8/control bytes.
- Enforced native record limits during full-native parsing.
- Crash-safe diagnostic patch for full-native parsing beyond the 5,000-record sample.

## V0_6_6_1

- Safe full-native enrichment cleanup: string trailer stripping, parsed-UTC-only timeline rows, timeline rejected-candidate export, and native usage-summary hydration. Active filesystem comparison remains tabled.

## V0_6_6

- Changed native metadata block limit to per-store behavior.
- Preserved fatal crash logs under `Upload/logs/FATAL_CRASH.txt`.
- Added guarded date conversion, invalid-parameter crash logging, native block limit, CLI aliases, and automatic Upload bundle.
- Sanitized CSV exports for invalid UTF-8/control bytes.
- Enforced native record limits during full-native parsing.
- Crash-safe diagnostic patch for full-native parsing beyond the 5,000-record sample.

## V0_6_5_1

- Added guarded date conversion, invalid-parameter crash logging, native block limit, CLI aliases, and automatic Upload bundle.
- Crash-safe diagnostic patch for full-native parsing beyond the 5,000-record sample.
- Safe full-native enrichment cleanup: string trailer stripping, parsed-UTC-only timeline rows, timeline rejected-candidate export, and native usage-summary hydration. Active filesystem comparison remains tabled.

## V0_6_5

- Safe full-native enrichment cleanup: string trailer stripping, parsed-UTC-only timeline rows, timeline rejected-candidate export, and native usage-summary hydration. Active filesystem comparison remains tabled.

## V0_6_4_1

- Added `native_parse_call` run status marker with decode mode.
- Added guarded timestamp conversion for experimental decoded date values.
- Added Windows invalid-parameter crash logging to `logs/FATAL_CRASH.txt`.
- Added `--native-item-limit` and `--native-block-limit` for controlled full-native diagnostics.
- Active filesystem comparison remains tabled.
- Normal run mode still uses preservation-first workflow.
- `--mode diagnostics` still skips 7z preservation by default.
- Deleted/orphaned filesystem-delta classification remains disabled.

## V0_6_4

- Safe cleanup and full-native diagnostic staging.
- Keeps deleted/orphaned filesystem-delta classification disabled.
- Keeps V0.6.3 diagnostic exports and fast diagnostic no-archive default.
- Documents the recommended full-native diagnostics command and upload set.
- Keeps experimental full-native structured decoding as the next diagnostic target.
- Removes stale filesystem live/missing warning text now that active filesystem comparison is tabled.

## V0_6_3

- Added native property dictionary and native decode attempt outputs.
- Added `--preserve` for diagnostics mode when archive-first behavior must be tested.
- Added diagnostics mode that skips 7z preservation by default for faster parser testing.
- Added decoder diagnostic exports for property names, value-type coverage, high-value candidates, field hit summaries, and store-level decode attempts.
- Diagnostic mode and native decoder visibility.
- Tabled active filesystem comparison and evidence-root deleted/orphaned classification.

## V0_6_2

- Added parent-inode quality summary export.
- Added child-name availability metrics for parent-inode and same-folder analysis.
- Parent-inode same-folder reporting hygiene.
- Preserves v0.6.0 parent-inode relationship analysis.
- Parent-inode reporting safety and path-candidate correctness.
- Improved same-folder group export so placeholder names are not treated as valid child names.
- Prevents placeholder child names such as `------NONAME------` from being used to construct path candidates.
- Does not change native parser decoding, decompression, evidence preservation, V7 behavior, or date parsing.

## V0_6_1

- Preserves v0.6.0 parent-inode relationship analysis.
- Parent-inode reporting safety and path-candidate correctness.
- Prevents placeholder child names such as `------NONAME------` from being used to construct path candidates.
- Does not change native parser decoding, decompression, evidence preservation, V7 behavior, or date parsing.
- Adds parser coverage metrics for parent-inode links with reconstructed paths and parent-inode matches missing child names.

## V0_6_0

- Adds a GUI review view for parent-inode links.
- Parent-inode relationship and same-folder grouping.
- Adds conservative native parent-inode relationship analysis.
- Does not enable full structured Spotlight decoding or V7 hydration.
- Adds parser coverage metrics for parent-inode links and same-folder groups.
- Links child `parent_inode_num` to parent `inode_num` within the same source/store context.

## V0_5_9

- No parser/decompression/native decoder behavior changed.
- Export schema hotfix.
- Date-candidate identity correction.
- Keeps V7 disabled in normal processing.
- Reporting hygiene and explicit date multiplicity buckets.
- Does not enable full structured Spotlight value decoding.
- Updates remaining active v0.5.6 log/UI text to current versioning.
- Does not change parser walking, preservation, decompression, or V7 behavior.

## V0_5_8

- No parser/decompression/native decoder behavior changed.
- Export schema hotfix.
- Summarizes date-source coverage from `raw_date_candidates` so the full date candidate identity key remains available.

## V0_5_7

- Build/version verification.
- Adds `VestigantSpotlightCli.exe --version`.
- Retains v0.5.6 date-candidate identity corrections.
- Updates CMake project version and runtime app version to v0.5.7.
- Updates Windows build script to print source version and verify the compiled CLI version.

## V0_5_6

- Date-candidate identity correction.
- Keeps V7 disabled in normal processing.
- Does not enable full structured Spotlight value decoding.
- Fixes the misleading v0.5.5 result where records sharing an inode but having different `store_id` values could appear to have two dates.
- Corrects date-candidate multiplicity metrics to join native date candidates back to raw records using the full native record identity: `source_id`, `store_guid`, `source_db`, `inode_num`, and `store_id`.

## V0_5_5

- Added `raw_date_candidates.csv`.
- Added `date_field_inventory.csv`.
- Added date field inventory export.
- Added GUI Raw Date Candidates view.
- Added `timeline_date_source_summary.csv`.
- Added timeline source-field summary export.
- Added `date_candidate_summary_by_record.csv`.
- Added per-record date candidate summary export.

## V0_5_4

- Reporting stabilization.
- Keeps V7 disabled in normal processing.
- Keeps full structured Spotlight value decoding disabled.
- Reporting-quality stabilization after the successful v0.5.3 core-probe run.
- Adds `detection_source_field` and `detection_source_value` to the GUI external/mounted-volume candidate view.
- Deduplicates mounted-volume candidates generated from multiple native probe fields for the same artifact/path.
- Prefers `__native_probe_mounted_volume_path` over generic `__native_core_probe_string_*` fields as the detection source when the same mounted-volume path is found more than once.

## V0_5_3

- Fixed stale active log text.
- Added detection source fields for mounted-volume candidates.
- Added `EXPORT_INDEX.csv` in the case root and exports folder.
- Native probe export and mounted-volume promotion.
- Did not enable full structured Spotlight value decoding.
- Conservative native-first stabilization pass after successful v0.5.2 default/core-probe runs.

## V0_5_2

- Moved structured metadata value walking behind `--experimental-full-native-values` only.
- Changed core decode flag to safe core-probe behavior: stable headers plus bounded high-value raw string/path probes only.
- Core-probe crash hotfix.
- GUI no longer enables core native value decoding automatically.
- Hotfix for crash during optional v0.5.1 `--decode-core-native-values` run.
- CMake MSVC build flags now use `/EHa` for consistency with structured exception handling.

## V0_5_1

- Added additional safety caps around native value reads.
- Native crash hotfix.
- Kept V7 out of normal processing.
- Returned default native parser mode to stable header-only behavior.
- Kept guarded high-value native decoding behind `--decode-core-native-values`.

## V0_5_0

- Removed V7 from normal hydration/enrichment.
- Added store-selection export and mounted-volume candidate logic.
- Added primary database selection so each store group selects valid `store.db` over `.store.db` while preserving and inventorying both.
- Native-first transition.
- Known issue: v0.5.0 crashed during early core-field native value decoding in user testing.

## V0_4_5

- V7 output was imported for enrichment.
- Stable preservation and V7-hydrated workflow.
- Native parser ran in conservative header-only mode.
- Evidence preservation succeeded and produced verified static archive.
- Known issue: V7 dependency and duplicate parsing of `store.db` plus `.store.db`.
- Introduced case outputs, preservation manifests, store inventory, native block/header scanning, SQLite case database, and review/export structure. Known focus: move away from V7 dependency and improve native parser coverage.
- Earlier C++ GUI/investigator iterations. Focused on discovery, preservation, GUI workflow, and initial parser integration. Known instability in early versions included crashes, logging gaps, and evidence-container targeting problems.

## V0_4_4

- V0.4.4 is a review-correctness and diagnostics release.
- Adds diagnostic exports.
- Adds path-specific artifact columns.
- Adds usage summary artifact columns.
- artifacts are marked `NOT_CHECKED` or `*_NOT_CHECKED`.
- Fixes live/missing classification when no evidence root is supplied.
- orphan/deleted candidates are not generated without a comparison root.
- Preserves the raw V7 fullpath value while also surfacing a cleaner display path.

## V0_4_3

- V7 output is imported before SQLite enrichment when `--v7-output` is supplied.
- Additional timeline events are created from imported V7 high-value date fields.
- `import-v7` mode now re-runs enrichment after importing V7 output into an existing case.
- Artifacts are hydrated from V7 key/value and fullpath output when native `raw_key_values` is empty.
- Usage evidence is populated from imported V7 date candidates for `kMDItemUsedDates` and `kMDItemLastUsedDate`.
- V0.4.3 uses imported V7 parser output as an enrichment bridge while the native C++ metadata value decoder is still being hardened.
- Hydrated fields include file name, display name, full path, content type, content type tree, where-froms, authors, creator, sizes, and index text snippets when available.
- This is still a bridge build. It does not claim full native value-decoder parity. It makes processed cases reviewable using the known-good V7 metadata layer while the native parser is expanded field-family by field-family.

## V0_4_2

- Timeline.
- Artifacts.
- Raw Records.
- Store Groups.
- Usage Evidence.
- Processing Log.
- Field Inventory.
- Parser Coverage.

## V0_4_1

- .\build-msvc\Release\VestigantSpotlightCli.exe `.
- Timeline.
- Artifacts.
- --verbose.
- Raw Records.
- Store Groups.
- --mode run `.
- Usage Evidence.

## V0_4_0

- Deduplicates only at the artifact review layer so source-level records are preserved.
- Adds export files for.
- Adds parser coverage tables.
- Run the default header-only path.
- Adds `source_copy_comparison` to classify records as.
- Run a separate test with `--decode-core-native-values`.
- Confirm raw record/source-copy counts match expectations.
- Keeps stable conservative header-only decoding as the default.

## V0_3_9

- Keeps preserved staging as the parser source after preservation.
- 7z add/test logs.
- 7z executable path.
- archive destination.
- archive source directory.
- Clarifies case summary terminology.
- Adds explicit preservation logging for.
- staged source file count and byte count.
