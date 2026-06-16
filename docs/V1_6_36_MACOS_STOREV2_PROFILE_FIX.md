# V1.6.38 macOS Store-V2 Profile Fix

## Evidence trigger

A macOS folder Spotlight run against `E:/test second/Spotlight/.Spotlight-V100` was registered as a macOS folder Spotlight source and discovered a valid `.Spotlight-V100/Store-V2/<GUID>/store.db` group. However, the last written run status reported `native_kv_persistence_filtered` with an iOS CoreSpotlight-specific message before the parse appeared to stall.

## Fixes

- Added an explicit native persistence mode to `NativeStoreDbParser`:
  - `MacOSStoreV2`
  - `IosCoreSpotlightCompact`
  - `AutoPathSensitive`
- `app_runner.cpp` now selects the native persistence mode from the requested source profile.
- macOS profile now writes `native_kv_persistence_macos_storev2` and disables iOS CoreSpotlight compact filtering.
- iOS profile continues to write iOS CoreSpotlight compact persistence status.
- Auto profile remains path-sensitive.
- Native parser progress writes now mirror from `logs/run_progress.tsv` to the case-root `run_progress.tsv` and `last_progress.tsv` so long native parses do not appear silent when the root status files are monitored.
- A new `native_parse_configuration` status line records decode mode and parser limits before the native parser call.

## Guardrails retained

- iOS CoreSpotlight compact filtering remains available for iOS profile runs.
- macOS Store-V2 parsing does not use iOS CoreSpotlight compact-filter status or mode.
- No fuzzy active filesystem matching was added.
- Missing-from-FFS rows remain investigative leads only, not deletion proof.
