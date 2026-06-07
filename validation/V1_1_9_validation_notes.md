# V1.1.9 Validation Notes

## Baseline reviewed

- V1.1.8 Windows/MSVC build log: build completed and binary reported Vestigant Spotlight v1.1.8.
- V1.1.8 macOS AFF4/APFS thin ZIP: generated successfully; denied raw upload filenames were absent; AFF4/APFS Store-V2 baseline counts remained stable.

## Local validation performed

- C++20 syntax check: `src/parsers/aff4_probe_worker.cpp`.
- C++20 syntax check: `src/app/app_runner.cpp`.
- C++20 syntax check: `src/parsers/apfs_volume_reader.cpp`.
- C++20 syntax check: `src/gui/gui_export_worker.cpp`.
- C++20 syntax check: `src/core/app_info.cpp`.
- Linux CMake configure/build.
- CLI version check returned `Vestigant Spotlight v1.1.9`.
- Local self-test passed.

## Required external validation

- Windows/MSVC V1.1.9 build.
- V1.1.9 macOS AFF4/APFS thin run.
- Compare external-reference counts and mismatch diagnostics against V1.1.8.
- Review whether next-leaf traversal changes staged Store-V2 file counts or mismatch classes.
