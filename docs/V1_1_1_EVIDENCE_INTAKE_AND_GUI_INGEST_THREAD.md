# V1.1.1 Evidence Intake and GUI Ingest Thread

V1.1.1 continues the broader repeat-cycle refactor after V1.1.0.1 built and produced a macOS AFF4/APFS thin ZIP.

## Implemented

- `importIosInventoryCsvs(...)` is now `EvidenceIntake::importIosInventoryCsvs(...)`.
- Cache-SQLite iOS inventory import helpers and referenced-path lookup import moved to the intake module.
- `app_runner.cpp` now delegates referenced-path lookup import through `EvidenceIntake::importReferencedIosPathLookupFromReuseCache(...)`.
- Run-status/progress logging remains controlled by the orchestrator through callback injection.
- Win32 GUI Build/Process Case no longer launches the main ingest worker with `.detach()`.
- The GUI tracks the ingest thread and joins it on `WM_DESTROY` to reduce risk of abrupt SQLite WAL interruption during app close.
- The AFF4 stream inventory callback signature was preserved and the platform-specific unused callback warning was suppressed.

## Deferred

- Full `stageZipEvidenceSource(...)` relocation.
- Dynamic AFF4/APFS probe worker extraction.
- Live APFS path reconstruction or B-tree iterator replacement.
- NSKeyedArchiver unflattened output.
- Full Win32 global-state object migration.

## Validation

- C++20 syntax checks passed for changed/dependent files.
- Linux CMake build passed.
- CLI version check reported `Vestigant Spotlight v1.1.1`.
- Local self-test passed.
- Windows/MSVC build, GUI runtime, AFF4 thin, and iOS runtime validation are still required.
