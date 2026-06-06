# V1.0.25 Thin Upload Security and iOS Performance Hardening

V1.0.25 is a narrow hardening build after the V1.0.24.1 Windows/MSVC build passed.

## Implemented

- Removed raw AFF4/iOS extraction tool logs and generated extraction helper scripts from the Thin Upload copy lists.
- Kept the high-level run logs in Thin Upload: `run_status.txt`, `last_stage.txt`, `run_progress.tsv`, `last_progress.tsv`, and `VestigantSpotlight.log`.
- Replaced hardcoded export CSV bundle lists with dynamic copying of regular `.csv` files directly under the `exports` folder, plus the existing `exports/upload_samples` directory.
- Replaced `countCsvDataRows` line-by-line string allocation with binary chunk newline counting.
- Reworked staged iOS app-database output path normalization to use `std::filesystem::path::lexically_normal()` plus per-path-component sanitization.
- Hardened Windows hidden process execution paths used for AFF4 stream inventory and ZIP PowerShell staging so they use `CreateProcessW` directly with inherited stdout/stderr log handles rather than `cmd.exe /C` shell redirection.

## Not changed

- APFS/AFF4 traversal, copy-out, staging, Store-V2 parsing, and lower-bound iterator behavior are unchanged.
- iOS CoreSpotlight parsing, app DB parser behavior, schema, and GUI views are unchanged.
- APFS diagnostic writer bodies remain in `app_runner.cpp`; this remains a future modularization target.
- The dynamic AFF4/APFS probe lambda structure remains unchanged.
- Case database open/close lifetime remains unchanged.

## Validation status

Local validation confirmed C++20 syntax for `src/app/app_runner.cpp`. Windows/MSVC build and runtime validation are still required.
