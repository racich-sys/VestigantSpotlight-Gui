# V1.0.6 APFS B-Tree Iterator and Direct AFF4 Copy-Out Work

## Implemented

1. Added direct AFF4/APFS record indexing during the exhausted filesystem B-tree traversal. The direct path now records decoded INODE and FILE_EXTENT records from the same APFS root-tree walk used to find Spotlight directory records.
2. Added target materialization from the decoded directory namespace: Store-V2 recursive namespace rows are correlated to indexed inode records, private data-stream IDs, and file-extent rows.
3. Added guarded direct AFF4/APFS copy-out for matched Store-V2 rows where extents are ordered and readable. Sparse logical gaps and zero physical extents are written as explicit synthetic zero regions and recorded in copy-out notes.
4. Added a logical directory-walk report that follows the discovered APFS namespace Root -> .Spotlight-V100 -> Store-V2 -> children, with object IDs and path context. This is a production bridge toward a formal lower-bound iterator.
5. Kept AFF4/APFS diagnostic outputs available for source-probe/support runs, but documented that production ingest should later hide these behind diagnostic/support mode after direct Store-V2 extraction is promoted into normal store discovery.

## Delayed

A complete APFS lower-bound iterator with repeated seek continuation across non-sibling-linked APFS leaf nodes is still pending. APFS B-tree nodes are not sibling linked in the reference material, so a directory spanning multiple leaves cannot safely be continued by following a simple next-leaf pointer. The next benchmark is a reusable `ApfsFsTreeIterator` module that supports lower-bound seeks by full key bytes and repeated continuation.

LZFSE/LZVN decompression remains delayed until a vetted third-party source tree, MSVC/Linux build integration, and known-good decmpfs test vectors are included. No decompression stub is used.
