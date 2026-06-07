# V1.1.6.1 Validation Notes

## Scope

V1.1.6.1 is a narrow Windows/MSVC build hotfix for V1.1.6.

The V1.1.6 direct-map AFF4/APFS probe worker split moved Windows-only file-read code into `src/parsers/aff4_probe_worker.cpp`. MSVC then reported that `wideProcessPath(...)` was not defined in the new translation unit.

## Change

- Added a translation-unit-local Windows-only path widening helper in `src/parsers/aff4_probe_worker.cpp`.
- Corrected the versioned V1.1.6.1 build/run wrappers.
- Updated version, handoff, workflow ledger, roadmap, and suggestions tracker.

## Not changed

- No AFF4/APFS traversal behavior changed.
- No Store-V2 parser behavior changed.
- No iOS parser behavior changed.
- No SQLite schema changed.
- No GUI behavior changed.
- No copy-out/staging semantics changed.

## Local validation

Passed:

- Static check: no bad `return false` cancellation branch remains from V1.1.5.
- Static check: `wideProcessPath(...)` exists in `aff4_probe_worker.cpp` under `_WIN32`.
- Static check: `Build-V1_1_6_1.ps1` checks for `1.1.6.1`.
- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/aff4_probe_worker.cpp`
- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp`
- `g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp`

Attempted:

- Linux CMake configure/build. Configure succeeded and build progressed through changed/dependent translation units. The container timed out during later existing build steps with no compile error shown before timeout.

Not validated:

- Windows/MSVC full build.
- Windows AFF4/APFS direct-map runtime.
- V1.1.6.1 macOS AFF4/APFS thin run.
