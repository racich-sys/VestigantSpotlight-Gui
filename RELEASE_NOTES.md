# V1.0.23

APFS/AFF4 diagnostic model modularization.

## Changed

- Added `src/parsers/apfs_diagnostic_models.h` as the shared row-model header for APFS/AFF4 diagnostic summaries and CSV row models.
- Moved APFS diagnostic row/summary structs out of `src/app/app_runner.cpp`.
- Kept APFS diagnostic writer bodies in `app_runner.cpp` for this version to keep the change narrow and build-verifiable.
- Updated versioned PowerShell build/launch/AFF4 wrapper scripts for V1.0.23.

## Not changed

- macOS AFF4/APFS traversal, copy-out, staging, Store-V2 parsing, or external-compare behavior.
- iOS CoreSpotlight extraction/parsing or iOS GUI view behavior.
- Apple/lzfse codec behavior.
- SQLite schema.
- GUI export worker behavior from V1.0.22.

## Validation

- Static source review of the V1.0.22 baseline.
- Local Linux build/self-test attempted for the V1.0.23 source after modularization.
- Windows/MSVC validation is still required on the target Windows environment.
