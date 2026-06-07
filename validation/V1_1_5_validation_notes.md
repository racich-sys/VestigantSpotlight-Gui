# V1.1.5 Validation Notes

## Baseline reviewed

- V1.1.4 Windows/MSVC build log: completed and reported `Vestigant Spotlight v1.1.4`.
- V1.1.4 macOS AFF4/APFS thin ZIP: generated successfully; denied raw upload filenames were absent.
- Current review: requested deeper cancellation propagation, upload-sample size guards, UTF-8 extraction logs, local diagnostic sample export error handling, and continued tracking of the dynamic AFF4/APFS probe monolith.

## Changes made

- Propagated the existing ingest cancellation token to `writeAff4CppLiteDynamicLoadProbe(...)` and `writeAff4DirectMapReaderProbe(...)`.
- Added cancellation checks in selected expensive bounded AFF4/APFS loops.
- Added case-directory writability preflight before logger/database setup.
- Replaced recursive `exports/upload_samples` thin-upload copying with explicit policy/size-guarded copy handling.
- Added nested upload-sample size-policy parity to `tools/Create-SourceProbeUploadZip.ps1`.
- Changed targeted app database and focused CoreSpotlight 7-Zip extraction logs to UTF-8 `Out-File`.
- Wrapped APFS staged Store-V2 diagnostic sample CSV exports in a localized try/catch.
- Updated handoff, roadmap, suggestion tracker, workflow ledger, version history, and release notes.

## Local validation

- C++20 syntax checks passed for changed/dependent source files.
- Linux CMake configure passed.
- Linux CMake build passed.
- CLI version check returned `Vestigant Spotlight v1.1.5`.
- Local self-test passed.

## Not validated here

- Windows/MSVC V1.1.5 build.
- Windows GUI runtime.
- GUI Cancel Ingest behavior during long AFF4/APFS probe loops.
- V1.1.5 macOS AFF4/APFS thin run.
- Current iOS runtime parity.

## Deferred

- Full `writeAff4CppLiteDynamicLoadProbe(...)` extraction to `aff4_probe_worker.cpp`.
- Full `stageZipEvidenceSource(...)` relocation to `EvidenceIntake`.
- Live APFS horizontal leaf traversal replacement and absolute path reconstruction.
- Full NSKeyedArchiver UID graph decoding.
