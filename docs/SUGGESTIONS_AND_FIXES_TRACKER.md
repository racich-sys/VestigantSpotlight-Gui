# Suggestions and Fixes Tracker

This file tracks review suggestions, proposed fixes, implementation status, and validation status across versions. Keep it current in every future package.

| ID | Suggestion / issue | Proposed fix | Status | Implemented in | Validation status / notes |
|---:|---|---|---|---|---|
| 1 | Thin Upload may include raw tool logs and raw inventories with sensitive paths. | Deny raw extraction logs/scripts and full raw inventory CSVs from thin bundle. | [x] Implemented | V1.0.25, strengthened V1.0.26 | V1.0.26 build passed; V1.0.26 thin packaging failed after probe completion due PowerShell char bug, fixed in V1.0.27. |
| 2 | Standalone thin-upload helper still risked including raw files. | Apply same deny-list policy in `tools/Create-SourceProbeUploadZip.ps1`. | [x] Implemented | V1.0.26 | Needs V1.0.27 packaging-only ZIP validation. |
| 3 | `Get-RelativePathForThinInventory` used `[char]'\\'`, causing Windows PowerShell packaging failure. | Replace with Uri-based relative path helper compatible with Windows PowerShell 5.1. | [x] Implemented | V1.0.27 | User observed failure after V1.0.26 probe/external compare completed; V1.0.27 validation pending. |
| 4 | Thin-upload inventory text may leak full local paths. | Write relative paths for case, additional output, and reader-tools inventories. | [x] Implemented | V1.0.26, strengthened V1.0.27 | Needs generated ZIP review. |
| 5 | `cmd.exe /C` command execution can mishandle shell metacharacters. | Use direct `CreateProcessW` paths for selected hidden tool execution. | [x] Partially implemented | V1.0.25/V1.0.26 | Further review needed for remaining generated PowerShell script shell use. |
| 6 | Hidden external processes can hang indefinitely. | Add bounded wait and terminate on timeout. | [x] Implemented | V1.0.26 | MSVC build passed; runtime timeout behavior not yet forced/tested. |
| 7 | Large AFF4/ZIP byte reads risk offset truncation on Windows. | Use `_wfopen_s` and `_fseeki64` for Windows exact-byte reads. | [x] Implemented | V1.0.26 | MSVC build passed. |
| 8 | `countCsvDataRows` was line-allocation heavy for huge CSVs. | Count newlines in binary chunks. | [x] Implemented | V1.0.25 | MSVC build passed in V1.0.25 and V1.0.26. |
| 9 | iOS staged DB path normalization used repeated string replacement. | Use `std::filesystem::path::lexically_normal()` and component sanitization. | [x] Implemented | V1.0.25 | MSVC build passed. |
| 10 | Thin Upload hardcoded export CSV names. | Dynamically copy regular top-level `exports/*.csv`. | [x] Implemented | V1.0.25 | Needs continued review that no raw/high-volume exports are unintentionally copied. |
| 11 | GUI/export helper logic duplicated across Win32 GUI and export worker. | Add `gui_view_helpers.h/.cpp` shared module. | [x] Implemented | V1.0.24, hotfixed V1.0.24.1 | V1.0.24.1 build passed. |
| 12 | APFS diagnostic row models were inside `app_runner.cpp`. | Move row structs to `apfs_diagnostic_models.h`. | [x] Implemented | V1.0.23 | V1.0.24.1 and later builds passed. |
| 13 | APFS diagnostic CSV writer bodies still bloat `app_runner.cpp`. | Move writer families to `apfs_diagnostic_exporter.cpp` incrementally. | [ ] Pending | Not yet | Next safe substantive target after V1.0.27 packaging validates. |
| 14 | `writeAff4CppLiteDynamicLoadProbe` remains a large dynamic-loader/APFS lambda monolith. | Extract worker/class only after tests and APFS comparator output are stable. | [ ] Deferred | Not yet | High risk; do not combine with other changes. |
| 15 | APFS extent copy-out logic should move into `ApfsVolumeReader`. | Add class extraction API and comparator validation first. | [ ] Deferred | Not yet | Must not replace live traversal until parity or improvement is proven. |
| 16 | decmpfs/resource-fork reconstruction logic is hard to test inside runner. | Move codec-specific decode helpers into codec module later. | [ ] Deferred | Not yet | High risk; requires focused codec tests. |
| 17 | iOS app DB inventory parsing should be fully inside parser module. | Move remaining DB inventory orchestration into `ios_app_db_parser.cpp`. | [ ] Pending review | Not yet | Confirm actual current boundary before moving. |
| 18 | SQLite handle thrashing in `runApplication`. | Open `CaseDatabase` once and pass references explicitly. | [ ] Deferred | Not yet | Broad lifetime refactor; do only after current runs stable. |
| 19 | GUI global state can cause thread-safety hazards. | Gradually introduce window-associated state object. | [ ] Deferred | Not yet | Large Win32 refactor; avoid until existing worker paths validated. |
| 20 | GUI LIKE searches can full-scan large tables. | Evaluate FTS5 or narrower search strategy. | [ ] Backlog | Not yet | Requires schema/query design, not a hotfix. |
| 21 | Add a running handoff file for future chats. | Add and maintain `docs/CONTINUATION_HANDOFF.md`. | [x] Implemented | V1.0.27 | Must be updated every version. |
| 22 | Continue checklist roadmap with completed items checked off. | Add and maintain `docs/ROADMAP_CHECKLIST.md`. | [x] Implemented | V1.0.27 | Must be updated every version. |
| 23 | Track suggestions/code fixes with checkboxes and version implemented. | Add and maintain this file. | [x] Implemented | V1.0.27 | Must be updated every version. |
| 24 | Hidden external process parents can leave child processes orphaned on timeout. | Wrap Win32 process launches in kill-on-close Job Objects and terminate the job on timeout. | [x] Implemented | V1.0.27 | Local syntax checks passed; Windows/MSVC/runtime validation pending. |
| 25 | GUI read connections can fail on transient SQLite WAL locks. | Add bounded custom `sqlite3_busy_handler` retry loops for GUI review/export read connections. | [x] Implemented | V1.0.27 | Local export-worker syntax check passed; Windows GUI validation pending. |
| 26 | Generated thin ZIP should fail closed if denied raw filenames are accidentally copied. | Add post-`Compress-Archive` ZIP entry deny-list assertion. | [x] Implemented | V1.0.27 | PowerShell execution pending on Windows. |
| 27 | APFS absolute logical path resolution needs reverse catalog walking. | Add only after validated B-tree lookup/value parsing supports parent/name reconstruction. | [ ] Deferred | Not yet | Proposed code is pseudocode and not safe to add directly. |
| 28 | Evidence intake/staging should be isolated from app runner. | Create `EvidenceIntake` module after current hardening validates. | [ ] Deferred | Not yet | Broad refactor; do not combine with process/GUI hardening. |
| 29 | iOS NSKeyedArchiver bplist unflattening is needed for better item context. | Add real bounded bplist object/UID model before emitting interpreted fields. | [ ] Deferred | Not yet | Placeholder JSON would be misleading; needs focused parser work. |
