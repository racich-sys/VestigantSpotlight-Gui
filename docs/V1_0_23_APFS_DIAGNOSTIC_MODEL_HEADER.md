# V1.0.23 APFS Diagnostic Model Header

V1.0.23 is a narrow source-maintainability release. It introduces a shared APFS/AFF4 diagnostic model header and moves the APFS diagnostic row/summary structs out of `src/app/app_runner.cpp`.

## Changed

- Added `src/parsers/apfs_diagnostic_models.h`.
- Moved APFS diagnostic row/summary structs into the shared header.
- Included the shared APFS diagnostic model header from `app_runner.cpp`.
- Updated versioned build/launch/AFF4 wrapper scripts for V1.0.23.

## Not changed

- macOS AFF4/APFS traversal or extraction logic.
- APFS copy-out/staging behavior.
- APFS lower-bound iterator behavior.
- iOS CoreSpotlight extraction or parsing.
- Apple/lzfse codec behavior.
- Store-V2 parser behavior.
- SQLite schema or GUI view definitions.

## Follow-up scope

After Windows/MSVC validation, the next safe APFS cleanup step is moving one small family of diagnostic CSV writer bodies from `app_runner.cpp` into `apfs_diagnostic_exporter.cpp` using the shared row-model header.
