V1.0.15 validation summary

Changed areas:
- src/app/app_runner.cpp
- tools/Create-SourceProbeUploadZip.ps1
- tools/Run-SingleAff4SourceProbeAndZip.ps1
- scripts/Build-V1_0_15.ps1
- scripts/Run-V1_0_15-macOS-AFF4-Probe-AndZip.ps1
- docs/V1_0_15_STOREV2_DUAL_PROCESS_COMPARE.md
- docs/V1_0_15_LZFSE_LZVN_SOURCE_REVIEW.md

Implemented:
- Added Store-V2 dual-process candidate comparison between AFF4/APFS copy-out candidates and normalized staging selections.
- Added CSV/JSON/Markdown outputs for the compare.
- Added compare outputs to thin-upload packaging and wrapper expected-output checks.
- Updated source/scripts/version metadata to 1.0.15.
- Documented LZFSE/LZVN source status and the production benchmark required before enabling codec output.

Checks performed in Linux sandbox:
- g++ C++20 syntax compile of src/app/app_runner.cpp: PASS.
- g++ C++20 syntax compile of src/parsers/apfs_volume_reader.cpp, src/parsers/apfs_aff4_reader.cpp, src/parsers/ios_app_db_parser.cpp, src/gui/view_registry.cpp, and tests/main.cpp: PASS.
- CMake configure: PASS.
- CMake build progressed through all common modules into app_runner.cpp and timed out during the very large optimized app_runner.cpp compile; no compile error was observed before timeout.

Not verified here:
- Windows/MSVC V1.0.15 build.
- Win32 GUI runtime.
- Live AFF4/APFS V1.0.15 run.


# V1.0.15

- Added AFF4/APFS Store-V2 candidate dual-process comparison.
- New outputs:
  - `aff4_apfs_storev2_candidate_dual_process_compare.csv`
  - `aff4_apfs_storev2_candidate_dual_process_compare_summary.json`
  - `AFF4_APFS_STOREV2_CANDIDATE_DUAL_PROCESS_COMPARE.md`
- The compare output audits raw APFS copy-out candidates against normalized `StagedStoreV2` selections.
- Added packaging and wrapper validation for the new compare outputs.
- Added LZFSE/LZVN source review documentation explaining why APFS structural documentation is authoritative for locating compressed content but not sufficient by itself to enable production codec output.
- Kept normal-mode AFF4/APFS structural diagnostics suppressed while keeping copy-out/staging/parser/enrichment/external-compare outputs enabled.

# V1.0.14

- Moved iOS app DB row parsing into `src/parsers/ios_app_db_parser.cpp`.
- Corrected AFF4/APFS normal-mode logging around suppressed diagnostics.
- Preserved Store-V2 staged parser handoff.
