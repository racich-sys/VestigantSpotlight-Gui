# V1.1.9 Live APFS Leaf Traversal and Source Review

## Scope

V1.1.9 promotes a bounded live APFS B-tree horizontal leaf traversal path inside the guarded AFF4/APFS probe worker. It also reviews the source package `.md`, `.txt`, and `.ps1` files for current-roadmap relevance.

## Implemented

- Added bounded next-leaf traversal to shared APFS OMAP target resolution.
- Added bounded next-leaf traversal to the dynamic/libaff4 APFS OMAP target lookup path.
- Added bounded next-leaf traversal to dynamic/libaff4 APFS volume root-tree lookup.
- Added cycle detection, transition cap, cancellation checks, and diagnostic notes for next-leaf transitions.
- Updated `apfsReadNextLeafOidFromBtreeInfoFooter()` documentation to reflect live guarded use.
- Reviewed all `.md`, `.txt`, and `.ps1` files and recorded decisions in `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.csv`.

## Safety gates

- The traversal is bounded to 256 horizontal leaf transitions per lookup.
- Cycles stop traversal.
- Unsafe next-leaf offsets stop traversal.
- Invalid/non-leaf next nodes stop traversal.
- Existing cancellation callbacks are checked while scanning leaf entries.

## Not implemented

- Full APFS absolute path reconstruction.
- Full NSKeyedArchiver UID graph decoding.
- Win32 virtual ListView conversion.

Those remain roadmap items.
