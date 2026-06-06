# V1.0.29 Validation Notes

## Base reviewed

- Base source: `VestigantSpotlightInv_V1_0_28_2.zip`
- Uploaded build log: `V1_0_28_2_build.log`
- Uploaded thin ZIP: `Upload_Thin_MacOS_AFF4_V1_0_28_2.zip`

## Observations

- V1.0.28.2 binaries linked and reported `Vestigant Spotlight v1.0.28.2`.
- The V1.0.28.2 PowerShell build wrapper failed after build because it still checked for `1.0.27`.
- The uploaded V1.0.28.2 thin ZIP existed and did not contain the denied raw filenames checked during review.

## Implemented in V1.0.29

- Corrected versioned scripts for V1.0.29.
- Closed parent-side redirected process log handle immediately after successful `CreateProcessW`.
- Replaced process-wide `SetDllDirectoryW`/`LoadLibraryW` with `LoadLibraryExW` secure search flags.
- Added `WM_SETREDRAW` suspension around Win32 review ListView bulk population.
- Added 50 MB size cap for dynamically globbed thin-upload export CSVs in C++.
- Added corresponding export CSV size cap in standalone PowerShell thin-upload helper.
- Updated continuity and tracker files.

## Local validation

Passed:

```text
g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp
g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/apfs_diagnostic_exporter.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
cmake -S . -B build-cmake-validate
```

Static checks passed for:

- V1.0.29 build-wrapper version regex.
- Absence of `SetDllDirectoryW`.
- Presence of `LoadLibraryExW`.
- Parent log-handle closure after child creation.
- `WM_SETREDRAW` review-list guard.
- C++ and PowerShell thin-upload export CSV size caps.
- Existing `sqliteColumnText` null guard.
- Existing decmpfs 512 MB expected-size cap.

## Not validated here

- Windows/MSVC full build.
- Windows GUI runtime.
- Windows AFF4 dynamic-load runtime behavior.
- V1.0.29 macOS AFF4/APFS thin run.
- V1.0.29 iOS run.
