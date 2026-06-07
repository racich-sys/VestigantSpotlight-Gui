# V1.1.7.1 Validation Notes

## Purpose

V1.1.7.1 is a build hotfix and package cleanup release after V1.1.7 moved both large AFF4/APFS probe bodies into `src/parsers/aff4_probe_worker.cpp`.

## Build failure addressed

Windows/MSVC reported missing identifiers in `aff4_probe_worker.cpp`:

- `shouldSkipLibAff4DynamicProbeForKnownBlockingLayout`
- `findToolCandidate`
- `lastWindowsErrorString`

Root cause: the V1.1.7 dynamic probe relocation moved code that depended on app-runner-local helpers without moving/exposing those helper boundaries.

## Fix implemented

- Added worker-local helper implementations in `src/parsers/aff4_probe_worker.cpp`.
- Preserved internal linkage to avoid exporting new public API or creating ODR/linker risk.
- Left `app_runner.cpp` helpers in place where still used by orchestration/other code.

## Cleanup implemented

- Removed obsolete version-specific build/run/launch/package scripts from `scripts/`.
- Removed old root-level package/deleted-files manifests from the active source root.
- Added `docs/NEW_CHAT_CONTINUATION_GUIDE.md`.
- Added `docs/SOURCE_PACKAGE_CLEANUP_POLICY.md`.
- Added `docs/FULL_VERSION_HISTORY.md` and append-only version history policy files.
- Updated top-level `BUILD_INSTRUCTIONS.md`, `HELP.md`, and troubleshooting/quick-start docs.

## Local validation performed

- C++20 syntax check: `src/parsers/aff4_probe_worker.cpp` — PASS.
- C++20 syntax check: `src/app/app_runner.cpp` — PASS.
- C++20 syntax check: `src/core/app_info.cpp` — PASS.
- Linux CMake configure/build — PASS.
- CLI version check — `Vestigant Spotlight v1.1.7.1`.
- Local self-test — PASS.

## Not validated here

- Windows/MSVC full build.
- Windows GUI runtime.
- V1.1.7.1 macOS AFF4/APFS thin run.

## Notes

Linux build still reports unused-function warnings in `aff4_probe_worker.cpp` because several helpers are Windows-only or preserved as part of the staged AFF4 probe boundary. Do not remove them until Windows/MSVC and AFF4 thin parity are confirmed.
