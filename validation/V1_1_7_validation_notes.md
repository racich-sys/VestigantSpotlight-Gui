# V1.1.7 Validation Notes

## Baseline

Started from validated V1.1.6.1.

## Scope

- Moved `writeAff4CppLiteDynamicLoadProbe(...)` from `app_runner.cpp` into `Aff4ProbeWorker::executeDynamicLoadProbe(...)`.
- Both large AFF4/APFS probe bodies now live in `src/parsers/aff4_probe_worker.cpp`.
- Added cancellation callback support to shared APFS OMAP traversal helper calls used by direct-map and dynamic-load probe paths.

## Local validation

- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/aff4_probe_worker.cpp`: PASS
- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp`: PASS
- Linux CMake configure/build: PASS
- CLI version check: `Vestigant Spotlight v1.1.7`
- Local self-test: PASS

## Not validated here

- Windows/MSVC full build.
- Windows GUI runtime.
- V1.1.7 macOS AFF4/APFS thin run.
