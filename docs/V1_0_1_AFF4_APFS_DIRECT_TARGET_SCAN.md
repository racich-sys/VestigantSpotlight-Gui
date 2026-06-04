# Vestigant Spotlight V1.0.1 - AFF4/APFS Direct Target Scan Notes

V1.0.1 follows the V1.0.0 AFF4/APFS diagnostic rerun. The V1.0.0 run showed that the direct AFF4 map reader could parse the AFF4 ZIP central directory, decode guarded LZ4 chunks, find APFS NXSB data, resolve APFS checkpoints, resolve six APFS volume superblocks, and read root-tree lookup rows. The remaining blocker was that the diagnostic path did not yet traverse enough APFS filesystem-tree namespace records to find `.Spotlight-V100` / `Store-V2` targets.

## V1.0.1 change

This version adds a bounded direct AFF4/APFS filesystem-tree target scan. It walks APFS B-tree nodes starting from resolved volume root-tree objects, resolves non-root child nodes through each volume OMAP where possible, prioritizes likely Data volumes, and records namespace-name samples and Spotlight target hits.

The implementation remains diagnostic-first. It does not perform unverifiable APFS file reconstruction. Copy-out remains gated until inode, xattr, data-stream, file-extent, sparse/gap, zero-block, decmpfs, and resource-fork status can be recorded explicitly.

## New/expanded outputs

The AFF4/APFS source-probe run now writes explicit outputs for these phases even when no target is found:

- `aff4_apfs_spotlight_target_scan.csv`
- `aff4_apfs_spotlight_target_scan_summary.json`
- `aff4_apfs_spotlight_inode_probe.csv`
- `aff4_apfs_spotlight_inode_probe_summary.json`
- `aff4_apfs_spotlight_xattr_probe.csv`
- `aff4_apfs_spotlight_xattr_probe_summary.json`
- `aff4_apfs_spotlight_file_extent_probe.csv`
- `aff4_apfs_spotlight_file_extent_probe_summary.json`
- `aff4_apfs_spotlight_file_copy_out.csv`
- `aff4_apfs_spotlight_file_copy_out_summary.json`
- `aff4_apfs_extracted_storev2_stage_groups.csv`
- `aff4_apfs_extracted_storev2_stage_files.csv`
- `aff4_apfs_extracted_storev2_stage_summary.json`

## Review decision after the next run

If the target scan finds `.Spotlight-V100` or `Store-V2` rows, the next change should implement inode/xattr/file-extent correlation and copy-out with explicit reconstruction status.

If the target scan still finds no hits, the next change should focus on APFS directory-entry decoding, APFS root-tree traversal correctness, or a target-guided lookup path for `.Spotlight-V100` and `Store-V2` names.
