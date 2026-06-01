# Codex AFF4 Direct APFS Root-Tree Update - V0.8.99

## What changed

- Extended the direct AFF4 ZIP map/index/data reader so it no longer stops after locating APFS container metadata.
- Resolved APFS volume superblocks through the container OMAP without calling the blocking `AFF4_open` path.
- Parsed each resolved volume's APFS object map and volume OMAP B-tree root.
- Resolved each volume's `apfs_root_tree_oid` through the volume OMAP.
- Added root-tree node/key sampling and a filesystem namespace seed report for the direct reader path.
- Preserved the existing guarded libaff4 skip behavior for BlackBag/LZ4 APFS `DiscontiguousImage` layouts.

## Verified on the supplied AFF4 path

The focused probe completed in about 11 seconds against:

`O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`

Observed diagnostic progress:

- APFS NXSB found: true
- Resolved APFS volume rows: 6
- Volume OMAP rows parsed: 6
- Volume root-tree lookup rows: 6
- Root-tree B-tree headers read: 5
- Root-tree record samples: 58
- Filesystem namespace seed rows: 55

## Still pending

- Guided APFS child-node traversal from the sampled root-tree branch records.
- Full namespace/path reconstruction for `.Spotlight-V100` and `Store-V2`.
- File extent copying from APFS into the staged Spotlight parser.
