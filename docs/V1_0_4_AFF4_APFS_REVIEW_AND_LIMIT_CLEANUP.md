# V1.0.4 AFF4/APFS Review and Limit Cleanup

## Review input

Reviewed the V1.0.3 AFF4/APFS thin upload and build log. V1.0.3 built the CLI, test binary, and GUI, but `Build-V1_0_3.ps1` incorrectly checked for version `1.0.1` after a successful `1.0.3` build. The AFF4/APFS run completed and produced external comparison outputs, but Store-V2 staging remained zero.

## Observed V1.0.3 metrics

- AFF4 direct map reader status: `DIRECT_MAP_READER_SMOKE_OK`.
- Map entries total: `318594`.
- APFS container superblock parsed: yes.
- APFS volumes resolved: `6`.
- Root-tree nodes visited: `49517`.
- Root-tree records scanned: `2000000`.
- Directory records decoded: `539471`.
- Spotlight target hits: `1034`.
- Nodes skipped by traversal limit: `45279`.
- Target inode rows: `0`.
- Target file extent rows: `0`.
- Staged Store-V2 files: `0`.
- External reference files: `4123`.
- Vestigant staged files: `0`.

## Implemented in V1.0.4

1. Fixed the Windows build script version check to require `1.0.4` instead of the stale `1.0.1` pattern.
2. Removed the direct AFF4/APFS root-tree node, record, and depth caps from the active traversal loop. Traversal now ends by queue exhaustion and visited-node cycle protection.
3. Kept only a diagnostic upload sample cap for `aff4_apfs_spotlight_name_scan_sample.csv`; this cap does not limit target discovery or directory-record collection.
4. Added complete direct-directory record collection independent from the name-sample CSV.
5. Added direct recursive Store-V2 namespace seeding from discovered Store-V2 directories and top-level component names, preserving group root, group name, relative path, and APFS path context in copy-attempt notes.
6. Corrected strict single-AFF4 policy reporting in the direct AFF4/APFS output writers from `false` to `true`.

## Deferred

Target-guided direct INODE/FILE_EXTENT copy-out remains deferred. The first attempted implementation of full direct guided lookup made `app_runner.cpp` compile impractically slowly in the Linux validation environment. This should be implemented in the next iteration by moving APFS B-tree target lookup code out of `app_runner.cpp` into a small reusable module, such as `src/apfs/apfs_btree_lookup.cpp`, with focused unit tests. Do not continue expanding `app_runner.cpp` for this logic.

## Next benchmark for V1.0.5

A V1.0.5 run should meet these benchmarks before full ingest promotion:

- `nodes_skipped_by_limit` must be `0`.
- `records_scanned` should exceed the old V1.0.3 cap of `2000000`, unless the queue exhausts earlier.
- `copy_attempt_rows` should exceed the old V1.0.3 value of `1034` if recursive Store-V2 child seeding succeeds.
- `aff4_apfs_spotlight_copy_attempt.csv` should include recursive Store-V2 `rel_path=` and `apfs_absolute_path=` notes for ordinary child files.
- Direct guided inode lookup should be moved into a separate APFS module and should produce nonzero `target_inode_hits` before attempting file byte copy-out.
