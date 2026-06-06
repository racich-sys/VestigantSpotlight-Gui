# V1.0.25

Thin Upload security and iOS staging performance hardening after the V1.0.24.1 Windows/MSVC build succeeded.

## Changed

- Removed raw AFF4/iOS extraction tool logs and generated extraction helper scripts from Thin Upload copy lists.
- Kept high-level run logs in Thin Upload: `run_status.txt`, `last_stage.txt`, `run_progress.tsv`, `last_progress.tsv`, and `VestigantSpotlight.log`.
- Replaced hardcoded export CSV bundle lists with dynamic copying of regular top-level `exports/*.csv` files.
- Kept the existing `exports/upload_samples` recursive copy.
- Changed `countCsvDataRows()` from line-by-line `std::getline()` allocation to binary chunk newline counting.
- Changed staged iOS app-database output path normalization to use `std::filesystem::path::lexically_normal()` plus per-component sanitization.
- Added direct Windows `CreateProcessW` helpers with stdout/stderr log redirection for selected hidden tool/script execution paths.

## Not changed

- macOS AFF4/APFS traversal, copy-out, staging, Store-V2 parsing, or external-compare behavior.
- APFS diagnostic writer locations.
- iOS CoreSpotlight parser behavior, app DB parser behavior, schema, or GUI views.
- Apple/lzfse codec behavior.
- GUI export worker behavior from V1.0.24.1.

## Validation

- Reviewed uploaded `V1_0_24_1_build.log`; Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.24.1`.
- Reviewed uploaded `Upload_Thin_MacOS_AFF4_V1_0_24_1.zip`; the run reached `complete_source_probe` and staged/parsed the expected AFF4/APFS Store-V2 baseline.
- Ran local C++20 syntax check for `src/app/app_runner.cpp`; it passed.
- Windows/MSVC V1.0.25 validation is still required.
