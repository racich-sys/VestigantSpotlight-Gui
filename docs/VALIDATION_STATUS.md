# V1.0.26.1

V1.0.26.1 is a thin-upload packaging hotfix after the V1.0.26 AFF4/APFS run and external comparison completed but the thin ZIP failed during PowerShell relative-path inventory generation.

## Changed

- Fixed `tools/Create-SourceProbeUploadZip.ps1` so `Get-RelativePathForThinInventory` no longer uses `[char]'\\'`, which Windows PowerShell treats as a two-character string and rejects.
- Reused the robust relative-path helper for `ExtractedSpotlight` copy paths.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- Added `scripts/Package-V1_0_26_1-macOS-AFF4-ThinFromExistingCase.ps1` for packaging an already-completed V1.0.26 AFF4/APFS case without rerunning the probe.
- Added `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, and `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.

## Not changed

- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.

## Validation

- Reviewed the uploaded V1.0.26 build log; the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.
- Reviewed the user-reported wrapper output showing the AFF4/APFS probe and external comparison completed before packaging failed.
- Local syntax/text checks were performed for modified C++ and PowerShell packaging files. Windows/MSVC V1.0.26.1 validation remains required.

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
