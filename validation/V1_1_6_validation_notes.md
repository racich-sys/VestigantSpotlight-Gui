# V1.1.6 Validation Notes

## Scope

V1.1.6 begins the Tracker #17 AFF4/APFS probe-worker extraction by physically moving the direct-map probe body out of `src/app/app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp`.

## Baseline reviewed

- Uploaded `V1_1_5_1_build.log`: Windows/MSVC build completed and reported `Vestigant Spotlight v1.1.5.1`.
- Uploaded `Upload_Thin_MacOS_AFF4_V1_1_5_1.zip`: generated successfully, denied raw upload filenames absent, and AFF4/APFS Store-V2 baseline counts remained present.

## Implemented

- Added `src/parsers/aff4_probe_worker.h`.
- Added `src/parsers/aff4_probe_worker.cpp`.
- Moved `writeAff4DirectMapReaderProbe(...)` body into `Aff4ProbeWorker::executeDirectMapReaderProbe(...)`.
- Updated `src/app/app_runner.cpp` to delegate direct-map probe calls to the new worker.
- Added the worker source to CMake and MSVC build lists.
- Updated workflow ledger, roadmap, handoff, and suggestions tracker.

## Deferred

`writeAff4CppLiteDynamicLoadProbe(...)` remains in `app_runner.cpp`. A full extraction attempt exposed a large dependency surface on app-runner-local APFS helper structs/functions and status helpers. The next safe step is a dedicated dependency-boundary pass rather than copying hidden dependencies blindly.

## Local validation

- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/aff4_probe_worker.cpp`: PASS
- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp`: PASS
- Linux CMake configure/build: PASS
- CLI version check: `Vestigant Spotlight v1.1.6`
- Local self-test: PASS

## Not validated

- Windows/MSVC V1.1.6 build.
- Windows GUI runtime.
- V1.1.6 macOS AFF4/APFS thin run.
