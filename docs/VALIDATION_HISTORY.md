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
