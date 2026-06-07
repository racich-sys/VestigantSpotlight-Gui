## V1.2.1 update

- Scope: documentation/package hygiene release.
- Consolidated standalone development notes into `docs/CONSOLIDATED_DEVELOPMENT_NOTES.md`.
- Consolidated standalone validation logs/notes into `validation/CONSOLIDATED_VALIDATION_LOGS_AND_NOTES.md`.
- Removed the now-consolidated standalone note/log files from the active package.
- Added `docs/SUPPORT_DIAGNOSTIC_TOOLS_REGISTER.md` to track retained support/diagnostic tools and their retention rationale.
- No support/diagnostic tools were deleted in this version because each remains tied to active AFF4/APFS validation, iOS support, general packaging/staging, or on-demand troubleshooting.
- No AFF4/APFS extraction, iOS parsing, GUI behavior, Store-V2 parser behavior, or SQLite schema behavior was intentionally changed.

- Reviewed uploaded `V1_1_10_1_build.log`: Windows/MSVC build completed successfully, CLI/tests/GUI linked, and `Vestigant Spotlight v1.1.10.1` was reported.
- Reviewed uploaded `Upload_Thin_MacOS_AFF4_V1_1_10_1.zip`: AFF4/APFS run completed source-probe workflow; staged Store-V2 parse/enrichment produced 25,000 artifacts.
- External compare summary remained stable against the prior V1.1.9/V1.1.10.1 class: 4,123 external files, 8,986 Vestigant staged files, 2,213 file matches, 1,424 external-only rows, 6,710 Vestigant-only rows, and 486 relative-path size mismatches.
- Remaining mismatch diagnostics stayed at 486 rows: 4 `DATA_FORK_SIZE_DISAGREES_WITH_EXTERNAL` and 482 `NO_EXACT_COPYOUT_CANDIDATE`.

## TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build.
- iOS: not required.
- Reason: V1.2.1 changes Win32 GUI review-grid rendering behavior and documentation only. The V1.1.11 build and AFF4/APFS thin output were reviewed before this version; no extraction/traversal/copy-out/decompression/parser code changed.
- Trigger for escalating AFF4/APFS to full test: any next change to live APFS traversal, copy-out, decompression, extent handling, path reconstruction, external compare logic, or Store-V2 staging behavior.
- Trigger for iOS testing: any next change to iOS ZIP staging, CoreSpotlight parsing, FFS lookup, app DB parsing, bplist/NSKeyedArchiver handling, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_2_1_build.log` and `Upload_Thin_MacOS_AFF4_V1_2_1.zip`.

## Standard V1.2.1 build command block

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_2_1.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_2_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_2_1.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_2_1\scripts\Build-V1_2_1.ps1
```

## Standard V1.2.1 AFF4/APFS thin command

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_2_1\scripts\Run-V1_2_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```


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

V1.0.18 validation summary

Inputs reviewed:
- V1_0_17_build.log
- Upload_Thin_MacOS_AFF4_V1_0_17.zip
- V1.0.17 source package

V1.0.17 observed build status:
- MSVC build completed successfully.
- Apple/lzfse source was detected and VESTIGANT_HAS_LZFSE was enabled.
- Third-party Apple/lzfse decoder sources compiled and linked.

V1.0.17 observed AFF4/APFS metrics:
- Copy-out rows: 9,902
- Copied files: 9,235
- Normalized staged files: 8,986
- Staged bytes: 1,368,577,744
- Valid parsed Store-V2 databases: 2
- Parsed raw records: 25,000
- Apple/lzfse codec status: APPLE_LZFSE_REFERENCE_CODEC_ENABLED
- decmpfs/resource-fork rows in the current test run: 0

V1.0.18 changes validated in this environment:
- app_runner.cpp C++20 syntax check with VESTIGANT_HAS_LZFSE=1: PASS
- ios_app_db_parser.cpp C++20 syntax check: PASS
- lzfse_codec.cpp C++20 syntax check: PASS
- APFS parser module syntax checks: PASS
- GUI view registry syntax check: PASS
- tests/main.cpp syntax check: PASS
- CMake configure: PASS
- Linux build progressed through the vendored Apple/lzfse sources and existing modules, then timed out during the very large app_runner.cpp compile; no compile error was observed before timeout.

Not verified here:
- Windows/MSVC V1.0.18 build
- Win32 GUI runtime
- Live AFF4/APFS V1.0.18 run


## V1.0.25

- Added shared GUI view/export helper module (`src/gui/gui_view_helpers.h/.cpp`) to remove duplicated SQL/view helper logic between the Win32 GUI and `GuiExportWorker`.
- No APFS traversal, Store-V2 parsing, iOS parsing, schema, or GUI view behavior was intentionally changed.
- Windows/MSVC validation is pending.
