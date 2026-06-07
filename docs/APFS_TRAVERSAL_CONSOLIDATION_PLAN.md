# APFS Traversal Consolidation Plan - V1.3.2

## Purpose

V1.3.2 starts the transition from duplicated APFS B-tree traversal logic toward shared, testable helpers that can later support a common BlockReader-style APFS reader for direct-map AFF4, dynamic/libaff4, and future raw-image paths.

## Implemented in V1.3.2

- Kept horizontal leaf loading centralized through `aff4ApfsLoadNextLeafForProbe(...)`.
- Added an explicit source comment that next-leaf footer parsing should remain centralized rather than duplicated at direct-map/dynamic call sites.
- Reused next-leaf output buffers at all current `aff4ApfsLoadNextLeafForProbe(...)` call sites instead of allocating a fresh `std::vector<unsigned char>` for every leaf transition.
- Reused the direct filesystem-tree diagnostic node buffer across pending-node traversal.
- Reused the root-tree node probe buffer across lookup rows.

## Deferred

- Full `ApfsVolumeReader` BlockReader abstraction.
- Replacing all AFF4 worker inline B-tree parsing in one release.
- Any changed forensic interpretation of APFS records.

## Next milestone candidates

1. Move fixed/generic B-tree key/value decoding wrappers into a shared APFS traversal utility module.
2. Add a small APFS traversal parity test that feeds identical nodes through direct-map and shared helpers.
3. Define a bounded BlockReader interface that returns block-size-aligned node buffers and explicit read status.
4. Route one low-risk diagnostic traversal through the shared interface before moving live extraction.
