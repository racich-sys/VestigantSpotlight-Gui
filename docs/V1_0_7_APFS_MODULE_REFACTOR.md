# V1.0.7 APFS Module Refactor and AFF4/APFS Thin-Run Review

## V1.0.6 thin-run findings

The V1.0.6 Windows build completed successfully and produced CLI, test, and GUI binaries reporting version 1.0.6. The AFF4/APFS run demonstrated a major extraction improvement:

- APFS root-tree traversal visited 94,796 nodes and scanned 3,833,445 records.
- 940,070 directory records were decoded.
- 16,539 target inode rows were materialized.
- 27,374 target FILE_EXTENT rows were materialized.
- 9,902 copy-out rows were produced.
- 9,084 rows copied through direct indexed extent chains.
- 82 rows copied with recorded synthetic zero regions.
- The external comparison found 8,338 Vestigant staged files and 1 exact relative-path/hash match against the external Store-V2 reference.

The remaining extraction gap is not basic AFF4 access. The tool is reading AFF4-backed APFS metadata and copying many files. The remaining blocker is correctness/completeness: path selection, exact Store-V2 component selection, logical-size trimming, and mismatch reduction against the external reference.

## Implemented in V1.0.7

- Added `src/parsers/apfs_volume_reader.h` and `src/parsers/apfs_volume_reader.cpp` as the dedicated APFS module boundary.
- Added APFS key helpers: search-key creation, object-id extraction, record-type extraction, and record-type labels.
- Added Store-V2 component classification helpers.
- Added APFS-safe path-component sanitizer.
- Added APFS copy-status classification helpers.
- Added an explicit `ApfsVolumeReader` class shell with the production-facing APIs planned for the lower-bound B-tree iterator:
  - `enumerateDirectory()`
  - `resolvePathToInode()`
  - `extractFileToDisk()`
- Added APFS module smoke tests to `VestigantSpotlightTests`.
- Added the new parser module to CMake and no-CMake MSVC build manifests.
- Fixed AFF4/APFS stage/count classification so V1.0.6 direct copy statuses are treated as successful copies:
  - `COPIED_DIRECT_INDEXED_EXTENT_CHAIN`
  - `COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS`
- Updated staging status labels for files copied with synthetic zero provenance.

## Deliberately not implemented in V1.0.7

The full lower-bound B-tree leaf-jumping iterator was not wired to live AFF4 reads in this version. The current app-runner APFS code already works well enough to copy thousands of files, and a full iterator refactor should not be combined with new extraction behavior in the same release. V1.0.7 creates the module boundary, validates helper behavior, and fixes reporting/counting around the current direct copy-out path.

LZFSE/LZVN decompression remains delayed until a vetted source tree and known-good test vectors are added.

## V1.0.8 benchmarks

Move the APFS iterator into the new module only when these conditions are met:

1. `ApfsVolumeReader` receives an injected block-reader/OMAP resolver rather than owning AFF4 globals.
2. Unit tests cover lower-bound key navigation with synthetic B-tree nodes.
3. The iterator records these metrics: lower-bound lookups, leaf nodes visited, next-leaf transitions, cycle stops, malformed-node stops, and directory entries returned.
4. A live AFF4 run returns the same or better `.Spotlight-V100/Store-V2` directory-entry coverage as V1.0.6.
5. External comparison improves from V1.0.6 baseline: external-only rows and relative-path size mismatches decrease, while exact hash/path matches increase.
