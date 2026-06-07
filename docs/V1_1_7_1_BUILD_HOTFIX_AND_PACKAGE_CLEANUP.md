# V1.1.7.1 Build Hotfix and Package Cleanup

## Purpose

V1.1.7 physically moved both major AFF4/APFS probe bodies into `src/parsers/aff4_probe_worker.cpp`, but the Windows/MSVC build exposed missing helper dependencies left behind in `app_runner.cpp`.

## Fixed

- Added worker-local helper boundary for:
  - `shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(...)`
  - `findToolCandidate(...)`
  - `lastWindowsErrorString(...)`
- Kept helper scope local to `aff4_probe_worker.cpp` to avoid new public API and ODR/linker risk.
- Preserved both probe bodies outside `app_runner.cpp`.

## Cleaned

- Removed old version-specific scripts from `scripts/`.
- Removed old root-level package manifests/deleted-files manifests.
- Added a source package cleanup policy.
- Added a new-chat continuation guide.
- Added full append-only version history baseline files.

## Not changed

- APFS traversal semantics.
- AFF4 read behavior.
- Store-V2 parser.
- iOS parser.
- SQLite schema.
- GUI behavior.

