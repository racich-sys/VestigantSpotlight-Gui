# V1.0.23 Validation Notes

## Baseline reviewed

- Source baseline: `VestigantSpotlightInv_V1_0_22.zip`
- Build log reviewed: `V1_0_22_build.log`
- V1.0.22 Windows/MSVC log reported `Vestigant Spotlight v1.0.22` and contained no build errors.

## V1.0.23 scope

- Added `src/parsers/apfs_diagnostic_models.h`.
- Moved APFS/AFF4 diagnostic row/summary structs out of `src/app/app_runner.cpp` into the new shared model header.
- Updated version files, CMake project version, app version, release notes, and versioned PowerShell wrappers to V1.0.23.

## Explicitly not changed

- macOS AFF4/APFS traversal, extraction, copy-out, staging, external compare, or Store-V2 parsing.
- iOS CoreSpotlight extraction/parsing or iOS GUI view behavior.
- Apple/lzfse codec behavior.
- SQLite schema.
- GUI export worker behavior from V1.0.22.

## Local validation performed

- Verified `VERSION`, `VERSION.txt`, `CMakeLists.txt`, and `src/core/app_info.cpp` report V1.0.23.
- Verified `src/app/app_runner.cpp` includes `parsers/apfs_diagnostic_models.h`.
- Verified APFS diagnostic row/summary structs exist in `src/parsers/apfs_diagnostic_models.h` and were removed from `app_runner.cpp`.
- Ran standalone syntax check for `src/parsers/apfs_diagnostic_models.h` using GCC/C++20.
- Ran GCC/C++20 `-fsyntax-only` check for `src/app/app_runner.cpp` with the same source include paths and `VESTIGANT_HAS_LZFSE=1`.
  - Result: syntax check completed successfully.
  - Warnings observed were pre-existing unused-parameter warnings in `stageZipEvidenceSource`.
- Attempted Linux CMake build/self-test.
  - Debug/Release builds progressed through the large `app_runner.cpp` translation unit and showed only warnings before the execution environment timed out.
  - No compile error was observed before timeout.

## Required external validation

- Windows/MSVC build using `scripts\Build-V1_0_23.ps1`.
- GUI launch smoke test using `scripts\Launch-V1_0_23-GUI.ps1`.
- Optional macOS AFF4/APFS thin run using `scripts\Run-V1_0_23-macOS-AFF4-Probe-AndZip.ps1`.
