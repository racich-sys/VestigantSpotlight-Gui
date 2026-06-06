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

# Build Notes - v0.3.5

From Developer PowerShell or ordinary PowerShell with Visual Studio 2022 installed:

```powershell
cd T:\VestigantSpotlightInv_V0_3_2
.\build_windows_msvc.bat
```

The script does not require CMake. It locates Visual Studio 2022 Community/Professional/Enterprise/BuildTools and compiles directly with `cl.exe`.

Required Visual Studio components:

```text
Desktop development with C++
MSVC v143 x64/x86 build tools
Windows 10 or Windows 11 SDK
```

Runtime database dependency uses Windows `winsqlite3.dll` via `winsqlite3.lib`.


## V1.0.25

- Thin Upload security/performance hardening.
- Raw AFF4/iOS extraction tool logs and generated extraction helper scripts are no longer copied into Thin Upload.
- Top-level export CSVs are copied dynamically from `exports/*.csv`.
- `countCsvDataRows()` and staged iOS app DB output path normalization were optimized/hardened.
- Windows/MSVC validation is pending.
