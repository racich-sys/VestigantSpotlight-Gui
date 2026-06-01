# Codex AFF4/APFS Hang Guard - V0.8.99

This update fixes the observed single-AFF4 source-probe hang against the BlackBag-style APFS AFF4 image:

`O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`

## What Changed

- Added a pre-open AFF4 metadata guard before the dynamic `libaff4` `AFF4_open` probe.
- The guard reads `information.turtle` directly from the AFF4 ZIP container and detects the known blocking layout:
  - BlackBag APFS container image
  - `aff4:DiscontiguousImage`
  - LZ4-backed AFF4 ImageStream
  - AFF4 map/index/data stream layout
- When detected, the program skips the blocking dynamic `AFF4_open` call and writes explicit diagnostics instead of freezing.
- Added `AFF4_DIRECT_MAP_READER_REQUIRED.md` to the generated case/upload package so the next engineering step is clear.
- Updated the single-AFF4 PowerShell wrapper with a CLI timeout.
- Updated the wrapper to package partial diagnostics even when APFS Spotlight copy-out is not yet available.
- Updated the wrapper to skip external Spotlight comparison unless AFF4/APFS Store-V2 copy-out actually completed.
- Replaced the wrapper's `Start-Process` launch path with a .NET process launch path to avoid Windows sessions that expose duplicate `Path/PATH` environment variables.

## Current Result

The same AFF4 source-probe path now completes cleanly in seconds and records:

- `aff4_dynamic_load_probe_guard`
- `SKIPPED_KNOWN_BLOCKING_LAYOUT`
- `wrapper_partial_aff4_outputs`
- `wrapper_external_compare_skipped`

## Important Limitation

This fixes the hang and preserves diagnostic upload creation. It does not yet implement full AFF4/APFS Spotlight copy-out for this BlackBag/LZ4 discontiguous image layout.

V0.8.99 direct-map follow-up adds a native AFF4 ZIP map/index/data smoke reader. The reader decodes bounded LZ4 image chunks without calling `AFF4_open` and now reports decoded SQLite/CoreSpotlight signature hits from the reconstructed stream. The next engineering step is to turn those virtual offsets into validated APFS path/file extents or, where APFS metadata remains unavailable, carve and validate bounded SQLite/CoreSpotlight candidates from direct virtual reads.

The next needed feature is a direct AFF4 map reader that decodes:

- `information.turtle`
- AFF4 map stream
- AFF4 index stream
- LZ4 compressed image chunks
- APFS reads over the reconstructed sparse virtual image

Only after that direct reader exists can the tool copy `.Spotlight-V100` Store-V2 files from this AFF4 and run a meaningful external Spotlight comparison.
