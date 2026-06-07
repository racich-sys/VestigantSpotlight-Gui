# Suggestions and Fixes Tracker

## V1.1.9 update

- Current generated source package: V1.1.9.
- Validated baseline reviewed before this version: V1.1.8 Windows/MSVC build and macOS AFF4/APFS thin output.
- Main change: guarded live APFS OMAP horizontal leaf traversal with bounded next-leaf transitions.
- Source-package `.md`, `.txt`, and `.ps1` file review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.md`.


This file tracks review suggestions, proposed fixes, implementation status, and validation status across versions. Keep it current in every future package.

| ID | Suggestion / issue | Proposed fix | Status | Implemented in | Validation status / notes |
|---:|---|---|---|---|---|
| 1 | Thin Upload may include raw tool logs and raw inventories with sensitive paths. | Deny raw extraction logs/scripts and full raw inventory CSVs from thin bundle. | [x] Implemented | V1.0.25, strengthened V1.0.26/V1.0.26.1/V1.0.27 | V1.0.27 thin ZIP reviewed; denied raw filenames absent. |
| 2 | Standalone thin-upload helper still risked including raw files. | Apply same deny-list policy in `tools/Create-SourceProbeUploadZip.ps1`. | [x] Implemented | V1.0.26 | V1.0.27 thin ZIP reviewed. |
| 3 | `Get-RelativePathForThinInventory` used `[char]'\\'`, causing Windows PowerShell packaging failure. | Replace with Uri-based relative path helper compatible with Windows PowerShell 5.1. | [x] Implemented | V1.0.26.1 | V1.0.26.1 and V1.0.27 thin ZIPs generated. |
| 4 | Thin-upload inventory text may leak full local paths. | Write relative paths for case, additional output, and reader-tools inventories. | [x] Implemented | V1.0.26, strengthened V1.0.26.1 | V1.0.26.1 thin ZIP reviewed; no full local prefixes in inventory files. |
| 5 | `cmd.exe /C` command execution can mishandle shell metacharacters. | Use direct `CreateProcessW` paths for selected hidden tool execution. | [x] Partially implemented | V1.0.25/V1.0.26 | Further review needed for generated script internals. |
| 6 | Hidden external processes can hang indefinitely. | Add bounded wait and terminate on timeout. | [x] Implemented | V1.0.26 | Windows build passed; forced timeout behavior not separately tested. |
| 7 | Hidden external process parents can leave child processes orphaned on timeout. | Wrap Win32 process launches in kill-on-close Job Objects and terminate the job on timeout. | [x] Implemented | V1.0.27 | V1.0.27 build passed; forced timeout behavior not separately tested. |
| 8 | Large AFF4/ZIP byte reads risk offset truncation on Windows. | Use `_wfopen_s` and `_fseeki64` for Windows exact-byte reads. | [x] Implemented | V1.0.26 | Windows build passed. |
| 9 | `countCsvDataRows` was line-allocation heavy for huge CSVs. | Count newlines in binary chunks. | [x] Implemented | V1.0.25 | Subsequent builds passed. |
| 10 | iOS staged DB path normalization used repeated string replacement. | Use `std::filesystem::path::lexically_normal()` and component sanitization. | [x] Implemented | V1.0.25 | Subsequent builds passed. |
| 11 | Thin Upload hardcoded export CSV names. | Dynamically copy regular top-level `exports/*.csv`. | [x] Implemented | V1.0.25 | Continue reviewing that high-volume raw files remain denied. |
| 12 | GUI/export helper logic duplicated across Win32 GUI and export worker. | Add `gui_view_helpers.h/.cpp` shared module. | [x] Implemented | V1.0.24, hotfixed V1.0.24.1 | V1.0.24.1 and later builds passed. |
| 13 | GUI read connections can fail on transient SQLite WAL locks. | Add bounded custom `sqlite3_busy_handler` retry loops for GUI review/export read connections. | [x] Implemented | V1.0.27 | V1.0.27 build passed; live GUI validation still needed. |
| 14 | Export Checked/Tagged could block GUI. | Run checked/tagged export through background worker and post completion to UI thread. | [x] Implemented before current review | V1.0.22/V1.0.24 series | Source review confirms checked/tagged exports are threaded in V1.0.27. |
| 15 | APFS diagnostic row models were inside `app_runner.cpp`. | Move row structs to `apfs_diagnostic_models.h`. | [x] Implemented | V1.0.23 | Later builds passed. |
| 16 | APFS diagnostic CSV writer bodies still bloat `app_runner.cpp`. | Move writer families to `apfs_diagnostic_exporter.cpp` incrementally. | [x] Mostly implemented | V1.0.28 | Main writer families moved; exact signature scan and rerun plan remain pending. |
| 17 | `writeAff4CppLiteDynamicLoadProbe` remains a large dynamic-loader/APFS lambda monolith. | Extract worker/class only after tests and APFS comparator output are stable. | [ ] Deferred | Not yet | High risk; do not combine with other changes. |
| 18 | APFS extent copy-out logic should move into `ApfsVolumeReader`. | Add class extraction API and comparator validation first. | [ ] Deferred | Not yet | Must not replace live traversal until parity or improvement is proven. |
| 19 | decmpfs/resource-fork reconstruction logic is hard to test inside runner. | Move codec-specific decode helpers into codec module. | [x] Implemented | V1.1.1 | Moved bounded zlib/deflate and decmpfs reconstruction helpers into `src/codec/lzfse_codec.cpp/.h`; local syntax and Linux CMake build/self-test passed. |
| 20 | iOS app DB inventory parsing should be fully inside parser module. | Move remaining DB inventory orchestration into `ios_app_db_parser.cpp`. | [x] Implemented | V1.0.30 | `IosAppDbParser::parseRecordInventories(...)` now owns iOS app database table enumeration, row counting, parser selection, and parsed-record insertion; `app_runner.cpp` delegates. |
| 21 | SQLite handle thrashing in `runApplication`. | Open `CaseDatabase` once and pass references explicitly. | [x] Implemented | V1.1.1 | Runner now opens one `CaseDatabase` handle and reuses it through AFF4/raw and general workflow; Windows runtime validation pending. |
| 22 | GUI global state can cause thread-safety hazards. | Gradually introduce window-associated state object. | [ ] Deferred | Not yet | Large Win32 refactor; avoid until existing worker paths validated. |
| 23 | GUI LIKE searches can full-scan large tables. | Evaluate FTS5 or narrower search strategy. | [ ] Backlog | Not yet | Requires schema/query design, not a hotfix. |
| 24 | Add a running handoff file for future chats. | Add and maintain `docs/CONTINUATION_HANDOFF.md`. | [x] Implemented | V1.0.26.1, updated V1.0.28.2 | Must be updated every version. |
| 25 | Continue checklist roadmap with completed items checked off. | Add and maintain `docs/ROADMAP_CHECKLIST.md`. | [x] Implemented | V1.0.26.1, updated V1.0.28.2 | Must be updated every version. |
| 26 | Track suggestions/code fixes with checkboxes and version implemented. | Add and maintain this file. | [x] Implemented | V1.0.26.1, updated V1.0.28.2 | Must be updated every version. |
| 27 | Generated thin ZIP should fail closed if denied raw filenames are accidentally copied. | Add post-`Compress-Archive` ZIP entry deny-list assertion. | [x] Implemented | V1.0.27 | V1.0.27 thin ZIP reviewed. |
| 28 | APFS absolute logical path resolution needs reverse catalog walking. | Add only after validated B-tree lookup/value parsing supports parent/name reconstruction. | [~] Scaffolding only | V1.1.1 | Added non-live `ApfsVolumeReader::resolveAbsolutePath(...)` helper API for future comparator work; not wired into staged forensic output. |
| 29 | Evidence intake/staging should be isolated from app runner. | Create `EvidenceIntake` module after current hardening validates. | [ ] Deferred | Not yet | Broad refactor; do not combine with APFS writer relocation. |
| 30 | iOS NSKeyedArchiver bplist unflattening is needed for better item context. | Add real bounded bplist object/UID model before emitting interpreted fields. | [ ] Deferred | Not yet | Placeholder JSON would be misleading; needs focused parser work. |
| 31 | True APFS B-tree iterator / next-leaf traversal requested. | Add comparator outputs first; replace live extraction only after parity or improvement. | [~] Helper improved | V1.1.1, strengthened V1.1.3 | Non-live footer helper and iterator fallback scaffolding exist in `ApfsVolumeReader`; live AFF4 extraction unchanged pending comparator parity. |
| 32 | V1.0.28 MSVC build failed because `asciiLower` was defined after earlier runner call sites following APFS writer relocation. | Add forward declaration for existing runner-local helper near the top of `app_runner.cpp`, before the anonymous-namespace runner code. | [x] Implemented | V1.0.28.1 | Windows/MSVC still needs validation after subsequent linker hotfix. |
| 33 | V1.0.28.1 MSVC link failed with `LNK2005` because `isLikelyStoreV2GroupDirectoryName()` existed in both `app_runner.cpp` and `apfs_diagnostic_exporter.cpp`. | Make the exporter-side helper translation-unit local while preserving the runner helper for AFF4/APFS probe code. | [x] Implemented | V1.0.28.2 | Requires Windows/MSVC build validation. |

| 34 | V1.0.28.2 build wrapper checked for stale `1.0.27` even though binaries linked as `1.0.28.2`. | Regenerate versioned scripts and make V1.0.29+ build wrappers expect their own version. | [x] Implemented | V1.0.29, preserved V1.0.30/V1.1.1 | V1.0.30 Windows/MSVC build passed; V1.1.1 wrapper expects `1.1.1`. |
| 35 | Redirected child-process log handle remained open in parent until process completion. | Close parent `logHandle` immediately after successful `CreateProcessW`; child retains inherited stdout/stderr handle. | [x] Implemented | V1.0.29 | V1.0.29 Windows/MSVC build passed. |
| 36 | AFF4 dynamic probe used process-wide DLL directory mutation. | Replace `SetDllDirectoryW`/`LoadLibraryW` with `LoadLibraryExW` and secure per-module search flags. | [x] Implemented | V1.0.29 | V1.0.29 Windows/MSVC build and macOS AFF4/APFS thin run passed. |
| 37 | Review ListView bulk population repainted per inserted row. | Suspend redraw with `WM_SETREDRAW` during row insertion and invalidate once after completion. | [x] Implemented | V1.0.29 | V1.0.29 Windows/MSVC build passed; live GUI runtime validation still needed. |
| 38 | Dynamically globbed thin-upload export CSVs could include oversized custom exports. | Cap dynamic export CSV copies at 50 MB and record size-limit redaction in manifest; mirror cap in PowerShell helper. | [x] Implemented | V1.0.29 | V1.0.29 thin ZIP generated and denied raw files were absent; oversized-export behavior not separately forced. |
| 39 | `sqliteColumnText` null/BLOB safety requested. | Verify `sqliteColumnText` returns empty string on `nullptr`. | [x] Already present | Before V1.0.30 | Confirmed in source during V1.0.30 review. |
| 40 | decmpfs expected-size overflow requested. | Verify 512 MB bounded reconstruction cap exists before allocation/reconstruction. | [x] Already present | Before V1.0.30 | Confirmed in source during V1.0.30 review. |
| 41 | Detached GUI export worker threads can be killed during app close, risking partial CSV output. | Register export worker threads and join them during `WM_DESTROY`; avoid `.detach()` for Export Page, Export Filtered, Export Checked, and Export Tagged. | [x] Implemented | V1.0.30 | Local static review passed; Windows GUI runtime validation pending. |
| 42 | iOS app DB record-inventory orchestration still kept table enumeration and parsed-record insertion in `app_runner.cpp`. | Add `IosAppDbParser::parseRecordInventories(...)` and keep runner as a narrow delegating wrapper with status callback. | [x] Implemented | V1.0.30 | C++20 syntax checks passed for parser and app runner; Windows/MSVC validation pending. |

| 43 | Evidence intake/staging should be isolated from `app_runner.cpp`. | Start a behavior-preserving `src/ingest/evidence_intake.*` module and move intake helper/import functions there before attempting full staging movement. | [x] Substantially implemented | V1.1.1 | CSV row counting, iOS ZIP path helpers, iOS CSV/cache import, and referenced-path lookup import moved into `EvidenceIntake`; full `stageZipEvidenceSource` orchestration remains pending. |
| 44 | iOS CSV inventory ingestion can be slow for large fallback CSV imports. | Apply temporary regenerable-intake SQLite PRAGMAs around the streaming CSV import and restore WAL/NORMAL afterward. | [x] Implemented | V1.1.1 | Local syntax and Linux CMake build/self-test passed; Windows/MSVC and current iOS runtime validation pending. |
| 45 | GUI LIKE behavior should be made more resilient before deeper search redesign. | Add `PRAGMA case_sensitive_like=OFF` to GUI read/export SQLite connections while leaving current broad investigator search behavior unchanged. | [x] Implemented | V1.1.1 | `gui_export_worker.cpp` syntax passed; `win32_gui.cpp` requires Windows/MSVC validation. |
| 46 | User requested `repeat` shorthand for future continuation cycles. | Document that `repeat` means review uploaded/copied info, continue from newest version, implement safe roadmap/suggestion items, update trackers, package, and provide commands. | [x] Implemented | V1.1.1 | Recorded in `docs/CONTINUATION_HANDOFF.md` and memory. |
| 47 | Remaining AFF4 stream inventory logic lived in `app_runner.cpp`. | Move inventory classification/reporting to `apfs_aff4_reader.cpp` while injecting tool lookup/process runners from the orchestrator. | [x] Implemented | V1.1.1 | Local syntax and Linux CMake build/self-test passed; Windows AFF4 thin validation pending. |
| 48 | `writeAff4ApfsV1DiagnosticRerunPlan()` was report-formatting logic still in `app_runner.cpp`. | Move it to `apfs_diagnostic_exporter.cpp/.h`. | [x] Implemented | V1.1.1 | Local syntax and Linux CMake build/self-test passed. |
| 49 | `parseApfsNxSuperblock()` was filesystem parsing logic inside the runner. | Move NXSB parsing into `apfs_volume_reader.cpp/.h`. | [x] Implemented | V1.1.1 | Local syntax and Linux CMake build/self-test passed. |
| 50 | The `repeat` workflow should allow larger coordinated version jumps. | On repeat, review inputs and implement a broader but reversible set of safe roadmap items with repeated validation. | [x] Implemented | V1.1.1 | Documented in continuation handoff and applied to the V1.1.1 package. |

| 51 | GUI main ingest/build worker was detached. | Track the ingest thread and join it during `WM_DESTROY` to reduce abrupt SQLite WAL interruption risk. | [x] Implemented | V1.1.1 | `win32_gui.cpp` now owns `gIngestThread`, guards starts with `gIngestActive`, and joins the worker on shutdown. |
| 52 | V1.1.0.1 AFF4 stream inventory callback warning. | Preserve callback signature but mark platform-specific unused callback with `(void)`. | [x] Implemented | V1.1.1 | Suppresses the MSVC C4100-style warning without changing AFF4 stream inventory behavior. |
| 53 | Full `stageZipEvidenceSource(...)` remains in `app_runner.cpp`. | Move only after V1.1.1 import-boundary relocation validates on Windows and iOS. | [ ] Pending | Not yet | Next intake modularization target. |

| 54 | Repeat workflow was re-solving prior packaging/build failures. | Add `docs/WORKFLOW_LEDGER.md` and require it as first review artifact each repeat cycle. | [x] Implemented | V1.1.2 | Tracks current baseline, prior failures, validation commands, and next candidate work. |
| 55 | GUI ingest thread is joined but cannot be cancelled safely by the investigator. | Add GUI cancel token/control and safe cancellation checkpoints in `runApplication`. | [x] Implemented | V1.1.2 | Adds `Cancel Ingest` button, optional atomic token, and stage checkpoints; cancellation inside deep APFS/native parser loops remains future work. |
| 56 | AFF4 dependent DLL search remained exposed to implicit dependency search behavior. | Use `SetDefaultDllDirectories`, `AddDllDirectory`, and `LoadLibraryExW` with default/user/DLL-load-dir flags. | [x] Implemented | V1.1.2 | Windows runtime validation pending. |
| 57 | GUI logo bitmap GDI handle was not explicitly freed. | Delete `gLogoBitmap` during `WM_DESTROY`. | [x] Implemented | V1.1.2 | Font cleanup preserved. |
| 58 | Native Store-V2 parser can be slow during regenerable bulk inserts. | Apply temporary bulk SQLite PRAGMAs around `NativeStoreDbParser::parseStores` and restore WAL/NORMAL afterward/error. | [x] Implemented | V1.1.2 | Runtime validation pending; schema unchanged. |
| 59 | bplist/NSKeyedArchiver context lacked trailer validation metadata. | Add bounded bplist trailer parser summary to existing context without emitting full object-graph interpretation. | [x] Implemented | V1.1.2 | Provides validation metadata while continuing to state that full NSKeyedArchiver decode is not implemented. |

| 35 | GUI export loops should honor shutdown/cancel. | Add export-worker cancellation callbacks and check before long SQLite scans/support exports. | [x] Implemented | V1.1.3 | Local syntax/build/self-test passed; Windows GUI runtime validation pending. |
| 36 | Orphan-source row purge uses many auto-committed DELETE statements. | Wrap purge loop in one transaction while preserving per-table warnings. | [x] Implemented | V1.1.3 | Local syntax/build/self-test passed; Windows runtime validation pending. |
| 37 | RichEdit DLL load should be constrained to System32. | Use `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_SYSTEM32)` and free the module on destroy. | [x] Implemented | V1.1.3 | Source review and syntax/build validation passed; Windows GUI runtime validation pending. |


| 60 | bplist trailer metadata still did not expose top-object physical offset mapping. | Add offset-table validation and top-object relative offset to bounded bplist context summaries without claiming full NSKeyedArchiver decoding. | [x] Implemented | V1.1.4 | Local syntax/build validation pending in this package. |
| 61 | GUI checked-artifact/export request construction still read mutable checked-state globals directly. | Add checked-state snapshot helpers and use snapshots when constructing review/export requests. | [x] Implemented | V1.1.4 | Compile validation pending; full window-state encapsulation remains future work. |
| 62 | Rapid repeated Build / Process Case clicks should be rejected before a second ingest worker can be created. | Use `gIngestActive.compare_exchange_strong` in the guarded ingest launcher. | [x] Implemented | V1.1.4 | Windows GUI runtime validation pending. |
| 63 | `writeAff4CppLiteDynamicLoadProbe` remains the largest monolith. | Extract through an `aff4_probe_worker` boundary only after a dedicated compile/test pass. | [ ] Pending | Not yet | Highest-risk remaining modularization item. |

| 64 | Cancel Ingest did not reach deep AFF4/APFS probes. | Pass the existing cancellation token into AFF4 dynamic/direct probe entry points and check it in selected bounded loops. | [x] Implemented | V1.1.5 | Local syntax validation passed; Windows long-run cancellation behavior pending. |
| 65 | Thin upload recursively copied `exports/upload_samples` without the dynamic export size guard. | Replace recursive copy with explicit per-file policy/size-guard handling; mirror nested policy in the standalone PowerShell packager. | [x] Implemented | V1.1.5 | Thin ZIP validation pending. |
| 66 | Focused iOS 7-Zip extraction logs used default PowerShell redirection encoding. | Pipe extraction output to `Out-File -Encoding UTF8` for targeted app DB and focused CoreSpotlight extraction logs. | [x] Implemented | V1.1.5 | Windows iOS ZIP runtime validation pending. |
| 67 | APFS staged Store-V2 diagnostic sample CSV exports could abort final probe summary writing. | Wrap diagnostic sample export group in localized try/catch and record run status on failure. | [x] Implemented | V1.1.5 | Local syntax validation passed. |
| 68 | Case directory writability was not explicitly checked before logger/database setup. | Add a small preflight write/remove probe before starting normal run status/logging. | [x] Implemented | V1.1.5 | Windows read-only path validation pending. |
| 69 | `writeAff4CppLiteDynamicLoadProbe` remains the main app-runner monolith. | Extract to `aff4_probe_worker.cpp` as a dedicated high-risk version; do not combine with live APFS traversal changes. | [ ] Pending | Not yet | Highest-priority modularization target after V1.1.5 validates. |
| 70 | V1.1.5 cancellation branch returned `false` from an `ApfsOmapTargetResolution` lambda. | Return populated cancellation-status `ApfsOmapTargetResolution out` instead. | [x] Implemented | V1.1.5.1 | Build hotfix; Windows/MSVC validation pending. |

| 71 | Tracker #17: `writeAff4DirectMapReaderProbe` remained inside `app_runner.cpp`. | Move the direct-map probe body into `src/parsers/aff4_probe_worker.cpp` and delegate through `Aff4ProbeWorker`. | [x] Implemented | V1.1.6 | Linux syntax/build/self-test passed locally; Windows/MSVC pending. |
| 72 | Tracker #17: `writeAff4CppLiteDynamicLoadProbe` remains inside `app_runner.cpp`. | Perform separate dependency-boundary pass before physical extraction to avoid duplicating app-runner-local probe helpers unsafely. | [ ] Deferred | Pending | Direct-map split was completed first; dynamic-load function requires a larger worker context. |


## V1.1.6.1 build-hotfix note

V1.1.6 moved the direct-map AFF4/APFS probe into `src/parsers/aff4_probe_worker.cpp`, but the MSVC build exposed a Windows-only missing helper: `wideProcessPath`. V1.1.6.1 adds a local Windows helper in the worker and corrects the versioned build script gate. This is recorded as a repeat-process pitfall: after moving code from `app_runner.cpp`, grep for Windows-only helper dependencies that Linux syntax checks cannot see.


## V1.1.7 update

- Baseline: validated V1.1.6.1.
- Completed Tracker #17 major step: moved `writeAff4CppLiteDynamicLoadProbe(...)` from `app_runner.cpp` into `Aff4ProbeWorker::executeDynamicLoadProbe(...)` in `src/parsers/aff4_probe_worker.cpp`.
- `writeAff4DirectMapReaderProbe(...)` had already been moved in V1.1.6; both large AFF4/APFS probe bodies now live in `aff4_probe_worker.cpp`.
- Added cancellation checks into the shared APFS OMAP traversal helper so the direct-map and dynamic-load paths can stop during B-tree traversal.
- Remaining related work: compare V1.1.7 Windows build and thin output against V1.1.6.1; then clean duplicated/unused helper functions and continue moving staged probe workers only after parity is confirmed.

| 40 | V1.1.7 dynamic AFF4/APFS probe relocation left helper functions in `app_runner.cpp`, causing Windows/MSVC missing identifier errors. | Add worker-local helper boundary for known blocking AFF4 layout detection, reader tool lookup, and Win32 error formatting. | [x] Implemented | V1.1.7.1 | Local C++ syntax checks passed; Windows/MSVC validation pending. |
| 41 | Active source package contained many obsolete version-specific scripts and root-level package manifests. | Keep only current version scripts plus generic scripts; preserve history in append-only docs. | [x] Implemented | V1.1.7.1 | Package cleanup manifest records deleted files. |
| 42 | Version history risked fragmentation/truncation. | Add `docs/FULL_VERSION_HISTORY.md` and append-only policy baseline from uploaded workflow/history docs. | [x] Implemented | V1.1.7.1 | Future packages must append to this history. |
| 43 | New chat continuation still required reconstructing setup from chat history. | Add `docs/NEW_CHAT_CONTINUATION_GUIDE.md` with current paths, required external files, commands, and repeat workflow. | [x] Implemented | V1.1.7.1 | Use as first artifact in new chat. |

## V1.1.8 Update

- `BaselineVersionHistory.md` is now the append-only version-history baseline in `docs/FULL_VERSION_HISTORY.md` and `VERSION_HISTORY.md`.
- Windows long-path evidence writes were added for APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction output paths.
- SQLite WAL checkpoint/truncate is requested before upload packaging.
- Logger writes are mutex-protected for concurrent GUI/export/ingest paths.
- APFS decmpfs reconstruction remains bounded; the expected-output safety cap is now 256 MiB.

