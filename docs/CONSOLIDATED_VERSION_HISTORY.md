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

---

# V1.0.27

V1.0.27 is a thin-upload packaging hotfix after the V1.0.26 AFF4/APFS run and external comparison completed but the thin ZIP failed during PowerShell relative-path inventory generation.

## Changed

- Fixed `tools/Create-SourceProbeUploadZip.ps1` so `Get-RelativePathForThinInventory` no longer uses `[char]'\\'`, which Windows PowerShell treats as a two-character string and rejects.
- Reused the robust relative-path helper for `ExtractedSpotlight` copy paths.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- Added `scripts/Package-V1_0_27-macOS-AFF4-ThinFromExistingCase.ps1` for packaging an already-completed V1.0.26 AFF4/APFS case without rerunning the probe.
- Added `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, and `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.

## Not changed

- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.

## Validation

- Reviewed the uploaded V1.0.26 build log; the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.
- Reviewed the user-reported wrapper output showing the AFF4/APFS probe and external comparison completed before packaging failed.
- Local syntax/text checks were performed for modified C++ and PowerShell packaging files. Windows/MSVC V1.0.27 validation remains required.

## V1.0.26

- Thin-upload redaction and hidden-process/large-offset I/O hardening added. Windows/MSVC validation pending.

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

# Vestigant Spotlight Consolidated Version History

## V0_9_60 - Windows GUI forward-declaration compile hotfix

V0_9_60 is a focused Windows/MSVC GUI build hotfix after V0_9_56 reached the GUI compile stage and failed with `C3861: setReviewSummary identifier not found` in `src\gui\win32_gui.cpp`. The fix adds a forward declaration for `setReviewSummary(const std::wstring&)` before the custom view-set helper functions that call it. No parser, ingest, cache, ZIP, FFS inventory, app DB, export, or forensic interpretation behavior was intentionally changed.

Current version: 0.9.60

## V0_9_60 - Windows MSVC batch-label build hotfix

V0_9_60 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55

V0_9_55 reviewed the uploaded V0_9_48 reuse-cache thin output and the investigator-details-pane design request. The V0_9_48 reuse-cache run completed successfully with 6 valid CoreSpotlight stores, 344,445 raw records, 982,668 raw key/value rows, 336,037 date candidates, 525,409 parsed app database records, a populated investigator super timeline sample, and 101 time-anomaly rows.

This release focuses on V1 investigator usability rather than ingest/parsing changes. It adds a bottom read-only **Selected Row Metadata / All Fields** details pane to the shared investigation grid used by both the macOS and iOS investigation tabs. Selecting a row now shows the view name, row number, artifact ID where available, check/tag state, and every visible result column vertically so investigators can review long Spotlight/app metadata values without horizontal scrolling. The pane supports vertical scrolling and copy/select behavior through a native Win32 multiline edit control.

No parser, staging, FFS inventory, or app database classification behavior was intentionally changed in V0_9_55. Existing completed cases can be used to test this GUI feature. Reuse-cache/fresh-ZIP ingest reruns are only needed if later changes affect parser output, cache behavior, or staging.

## V0_9_45

V0_9_45 reviewed V0_9_44 Windows build plus reuse-cache and Stage B fresh-ZIP thin outputs. Both runs completed successfully. The fresh-ZIP native 7-Zip inventory parser now produced nonzero FFS inventory rows, but it over-classified many non-database files as app databases because several path-based rules did not require a database-like filename. This release narrows iOS app database inventory classification to database-like file names before applying Messages, Signal, Telegram, CallHistory, WebKit, Safari, Chrome/Google, Mail, Calendar, Contacts, Keychain, WhatsApp, and other database-family labels. It also preserves targeted extraction paths in the native C++ inventory CSV when the extracted database exists under EvidenceStaging/ios_app_databases, so future support-mode app DB record parsing has usable paths after a fresh ZIP run.

## V0_9_44

V0_9_44 reviewed V0_9_43 Windows build, reuse-cache thin output, and Stage B fresh-ZIP thin output. The fresh-ZIP path completed but produced an empty FFS/app-database inventory, showing the native C++ parser did not parse the raw 7-Zip listing. This release changes the PowerShell helper to use `cmd.exe` redirection for raw `7z l -slt` output, adds native fallback decoding for UTF-16 raw listing lines, reports a warning if the raw listing parses to zero rows, and preserves the compact normal iOS mode.

Additional parser improvements include zero-copy bounded metadata item parsing from decompressed metadata-block payloads, improved fallback probe deduplication/non-ASCII preservation, and safer removal of CoreSpotlight string trailer markers after trimming padding.

## V0_9_43

Added bounded iOS CoreSpotlight bplist / NSKeyedArchiver marker discovery. The release detected likely binary plist / NSKeyedArchiver values, extracted bounded printable-token context, stored compact synthetic context rows, and exposed summary/detail views and CSV exports. It intentionally did not perform full NSKeyedArchiver graph decoding.

## Earlier V0_9 history

Earlier V0_9 releases focused on compact normal iOS mode, DB/WAL guardrails, iOS investigator views, message/media/text review surfaces, parser limits/suppression reporting, GUI stability, CSV export optimization, reuse-cache workflows, and initial Stage B fresh-ZIP preparation. Preserve prior detailed history from earlier packages if litigation/reporting requires exact version-by-version reconstruction.

## V0_9_55

- Refined the selected-row metadata pane into a two-column Field / Value layout so field identifiers remain on the left and metadata values appear on the right.
- Added a draggable splitter above the details pane so investigators can resize the grid/detail split during review.
- Preserved the V0_9_50 grouped review order: text first, dates second, then paths, people/apps, status, provenance, counts, and other fields.
- GUI-only update; no parser, cache, ZIP, FFS, or app database behavior changed.

## V0_9_55
- GUI-only selected-row metadata pane revision.
- Converted the bottom details pane from formatted text to a true two-column Field / Metadata ListView table.
- Added section-row background shading for field groups.
- Kept drag-resizable details area.
- Explicitly hides the row-details controls on non-investigation tabs.

## V0_9_60 - Staged AFF4/raw source restoration and MB telemetry

V0_9_60 restores AFF4/APFS image and Raw IMG/DD image choices in the GUI source-type selector as staged image workflows. Folder and ZIP remain the production intake paths; AFF4/APFS is kept visible for macOS forensic images, and Raw IMG/DD is kept visible for raw/external media such as exFAT devices attached to Macs that may contain Spotlight indexes. Processing telemetry now displays throughput in decimal MB and MB/s rather than MiB/MiB/s for investigator readability. No parser interpretation logic was intentionally changed.


## V1.0.25

- Added shared GUI view/export helper module (`src/gui/gui_view_helpers.h/.cpp`) to remove duplicated SQL/view helper logic between the Win32 GUI and `GuiExportWorker`.
- No APFS traversal, Store-V2 parsing, iOS parsing, schema, or GUI view behavior was intentionally changed.
- Windows/MSVC validation is pending.
