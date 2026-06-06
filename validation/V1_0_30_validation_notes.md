# V1.0.31 Validation Notes

## Base reviewed

- Base source: `VestigantSpotlightInv_V1_0_29.zip`
- Uploaded build log: `V1_0_29_build.log`
- Uploaded macOS AFF4/APFS thin ZIP: `Upload_Thin_MacOS_AFF4_V1_0_29.zip`

## Uploaded V1.0.29 findings

- Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.29`.
- macOS AFF4/APFS thin ZIP was created and reviewed.
- Denied raw thin-upload filenames were absent:
  - `aff4_stream_inventory_raw.txt`
  - `ios_focused_zip_extract.log`
  - `ios_focused_zip_extract_7z.log`
  - `ios_focused_zip_extract.ps1`
  - `ios_ffs_file_inventory.csv`
  - `image_file_inventory.csv`
- AFF4/APFS staged Store-V2 baseline remained stable:
  - `raw_record_count=25000`
  - `raw_key_value_count=2181`
  - `raw_date_candidate_count=25000`
  - `artifact_count=25000`
  - external file count `4123`
  - Vestigant staged file count `8986`
  - external-only rows `1424`
  - Vestigant-only rows `6710`
  - hash-different-path rows `431`

## V1.0.31 change scope

Implemented:

1. Moved iOS app database record-inventory orchestration into `src/parsers/ios_app_db_parser.cpp`.
2. Added `IosAppDbParser::parseRecordInventories(...)` with a status-writer callback to preserve run-status behavior.
3. Reduced `app_runner.cpp::parseIosAppDatabaseRecordInventories(...)` to a delegating wrapper.
4. Added GUI export thread registration for Export Page, Export Filtered, Export Checked, and Export Tagged.
5. Removed `.detach()` from those four GUI export workers and joined registered export threads during `WM_DESTROY`.
6. Updated continuation, roadmap, suggestions/fixes tracker, versioned scripts, and release/history notes.

Not changed:

- AFF4/APFS traversal.
- APFS copy-out or staging.
- Store-V2 parser behavior.
- SQLite schema.
- iOS CoreSpotlight parser schema or interpretation.
- APFS reverse path reconstruction.
- NSKeyedArchiver/bplist interpretation.
- `runApplication()` database lifetime.

## Local validation pass 1

- `src/parsers/ios_app_db_parser.cpp`: C++20 syntax pass after resolving local `toLower` / path-utils name collision.
- `src/app/app_runner.cpp`: C++20 syntax pass.
- `src/gui/gui_export_worker.cpp`: C++20 syntax pass.
- `src/core/app_info.cpp`: C++20 syntax pass.

`src/gui/win32_gui.cpp` cannot be syntax-checked in this Linux container because it is explicitly Windows-only and includes Win32 headers.

## Local validation pass 2

- `src/parsers/ios_app_db_parser.cpp`: C++20 syntax pass.
- `src/app/app_runner.cpp`: C++20 syntax pass.
- `src/parsers/apfs_diagnostic_exporter.cpp`: C++20 syntax pass.
- `src/gui/gui_export_worker.cpp`: C++20 syntax pass.
- `src/core/app_info.cpp`: C++20 syntax pass.
- CMake configure pass.
- Static review pass confirmed:
  - `Build-V1_0_31.ps1` expects `1.0.31`.
  - app runner delegates iOS DB record inventory to parser module.
  - old app-runner `sqliteScalarCount`/`sqliteColumnList` helpers were removed.
  - parser module owns `IosAppDbParser::parseRecordInventories(...)`.
  - GUI export thread registry is present.
  - four GUI export workers no longer call `.detach()` directly.

## Validation still required

- Windows/MSVC V1.0.31 build.
- Windows GUI runtime test.
- GUI export close/shutdown behavior during and after large exports.
- V1.0.31 macOS AFF4/APFS thin run.
- Current iOS parser run to confirm iOS module delegation preserves counts.
