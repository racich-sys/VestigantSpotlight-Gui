# V1.1.7 AFF4 Probe Worker Dynamic Relocation

## Scope

V1.1.7 completes the major Tracker #17 modularization step that remained after V1.1.6. The dynamic libaff4 APFS probe body was physically moved from `src/app/app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp`.

## Implemented

- Added `Aff4ProbeWorker::executeDynamicLoadProbe(...)`.
- Replaced the app-runner call to `writeAff4CppLiteDynamicLoadProbe(...)` with `Aff4ProbeWorker::executeDynamicLoadProbe(...)`.
- Removed the dynamic probe body from `app_runner.cpp`.
- Kept direct-map probe worker introduced earlier.
- Added cancellation callback support to the shared APFS OMAP traversal helper and passed the appropriate direct/dynamic cancellation closures.

## Not changed

- No AFF4/APFS traversal semantics were intentionally changed.
- No copy-out/staging rules were changed.
- No Store-V2 parser behavior was changed.
- No iOS parser behavior was changed.
- No schema changes were made.

## Validation

Local Linux validation passed: syntax checks, CMake build, CLI version check, and self-test. Windows/MSVC and AFF4 thin-output parity still require user-side validation.
