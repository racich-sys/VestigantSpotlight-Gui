# V1.0.25 Validation Notes

## Baseline reviewed

- Uploaded `V1_0_24_1_build.log` showed a successful Windows/MSVC build.
- Uploaded `Upload_Thin_MacOS_AFF4_V1_0_24_1.zip` reached `complete_source_probe`.
- V1.0.24.1 AFF4/APFS staged Store-V2 parser metrics remained consistent with the prior baseline: `raw_records=25000`, `raw_key_values=2181`, `raw_date_candidates=25000`, `staged_groups=11`, `staged_files=8986`, `staged_bytes=1368577744`.

## Changes validated locally

- Confirmed version fields now report `1.0.25`.
- Confirmed `src/app/app_runner.cpp` no longer copies the following raw files into Thin Upload lists:
  - `aff4_stream_inventory_raw.txt`
  - `ios_focused_zip_extract.log`
  - `ios_focused_zip_extract_7z.log`
  - `ios_focused_zip_extract.ps1`
- Confirmed Thin Upload export copying now iterates regular `.csv` files directly under `caseDir/exports` instead of using a hardcoded required export manifest.
- Confirmed `countCsvDataRows()` now uses binary chunk newline counting.
- Confirmed `extractedIosAppDbPathForZipEntryCpp()` now uses `lexically_normal()` and per-component sanitization.
- Confirmed Windows helper process calls for AFF4 stream inventory and ZIP PowerShell staging now use direct `CreateProcessW` process execution with stdout/stderr redirected to log handles.
- Ran `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp`; result: passed.

## Not validated here

- Windows/MSVC full build.
- Windows GUI runtime.
- V1.0.25 macOS AFF4/APFS thin run.
- V1.0.25 iOS CoreSpotlight thin run.

## Deferred review items

The following review items were intentionally deferred because they are larger architectural changes and should not be mixed with this security/performance hardening build:

- Moving APFS diagnostic writer bodies into `apfs_diagnostic_exporter.cpp`.
- Moving decmpfs resource-fork reconstruction into the codec module.
- Moving iOS app database record inventory orchestration fully into `IosAppDbParser`.
- Moving dynamic AFF4/APFS probe lambdas into `ApfsVolumeReader`.
- Refactoring the Win32 GUI global state into a window-associated class.
- Refactoring `runApplication()` database lifetime to a single long-lived `CaseDatabase` object.
