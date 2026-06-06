# V1.0.31 Validation Notes

## Reviewed inputs

- `V1_0_30_build.log`: Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.30`.
- `Upload_Thin_MacOS_AFF4_V1_0_30.zip`: thin ZIP generated; denied raw filenames were absent; AFF4/APFS Store-V2 staged baseline remained stable.

## Local validation performed

Two C++20 syntax-check passes were run for:

- `src/ingest/evidence_intake.cpp`
- `src/app/app_runner.cpp`
- `src/parsers/ios_app_db_parser.cpp`
- `src/parsers/apfs_diagnostic_exporter.cpp`
- `src/gui/gui_export_worker.cpp`
- `src/core/app_info.cpp`

Linux CMake configure and full build completed.

Local CLI version check returned `Vestigant Spotlight v1.0.31`.

Local self-test passed.

## Not validated here

- Windows/MSVC V1.0.31 build.
- Windows GUI runtime.
- Windows GUI search behavior.
- V1.0.31 macOS AFF4/APFS thin run.
- Current iOS run to validate CSV fallback import PRAGMAs and intake-helper module output parity.
