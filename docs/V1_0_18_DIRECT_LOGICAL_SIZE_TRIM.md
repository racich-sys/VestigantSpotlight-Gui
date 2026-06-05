# V1.0.18 Direct AFF4/APFS Logical-Size Trim

## Problem observed in V1.0.15

The V1.0.15 AFF4/APFS copy-out path successfully staged thousands of Store-V2-related files, but the external comparison still showed many `RELATIVE_PATH_SIZE_MISMATCH` rows.

Review of representative mismatch diagnostics showed a repeatable pattern:

- copy-out status was successful;
- staging source validation matched the copied extent-chain output;
- the staged Vestigant file was larger than the external reference file by a small block-aligned amount;
- the copy-out logical size source was `direct_indexed_file_extent_end`.

This indicates that direct APFS copy-out sometimes wrote the allocated/file-extent end rather than the file's logical data-stream size.

## V1.0.18 change

For direct indexed APFS copy-out rows, V1.0.18 now prefers inode-derived logical size when available:

1. `INO_EXT_TYPE_DSTREAM.size.direct_index`
2. `j_inode_val.uncompressed_size.direct_index`
3. fallback to `direct_indexed_file_extent_end`

The copy loop is bounded by that logical size. If the final file is shorter than the extent-chain end because it was trimmed to the inode data-stream size, the validation status records that provenance instead of treating it as an unexplained mismatch.

## Forensic handling

This change does not hide uncertainty. The row keeps:

- `logical_size_bytes`
- `logical_size_source`
- copy-out status
- validation status
- SHA-256 of the emitted bytes
- synthetic-zero provenance when used

The intended benchmark is a reduction in `RELATIVE_PATH_SIZE_MISMATCH` rows, especially for Store-V2 component files whose V1.0.15 mismatch was caused by `direct_indexed_file_extent_end` output.
