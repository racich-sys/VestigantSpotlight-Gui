# V1.0.15 LZFSE/LZVN Source Review

## Finding

The uploaded Apple File System Reference is an authoritative source for APFS on-disk structures, including container/volume mount flow, object maps, file-system record types, directory records, extended attributes, inode records, file extents, data streams, sparse-file indicators, and resource forks.

The uploaded APFS slide deck identifies transparent file compression, `com.apple.decmpfs`, inline compressed data for small files, and `com.apple.ResourceFork` for larger compressed content. The live code already records decmpfs/resource-fork status and recognizes known compression type labels.

## What was not added

V1.0.15 does not add production LZFSE/LZVN decompression. The reason is narrow: the APFS filesystem documentation is sufficient to justify decoding the structures that point to compressed data, but it is not by itself a codec implementation and it does not provide known-good decode vectors for this toolchain.

## Required benchmark before enabling codec output

LZFSE/LZVN should be enabled when all of the following are present in the repository:

1. a vendored or system-linked vetted codec implementation with license notes;
2. MSVC and Linux build integration;
3. known-good LZFSE and LZVN compressed/decompressed test vectors;
4. tests for decmpfs inline data and resource-fork backed data;
5. output statuses that distinguish fully decoded, skipped unsupported codec, partial resource fork, sparse/gap, and read failure.

Until that benchmark is met, V1.0.15 keeps the forensic-safe behavior: detect and record decmpfs/LZFSE/LZVN provenance, but do not emit plausible reconstructed files from an unverified codec path.
