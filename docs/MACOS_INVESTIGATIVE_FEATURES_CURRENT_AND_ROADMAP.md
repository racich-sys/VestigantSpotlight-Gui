# macOS Investigative Features - Current and Roadmap

## Current macOS investigative and extraction features

- Folder and ZIP ingestion for existing `.Spotlight-V100/Store-V2` content.
- Explicit single-AFF4 source-probe workflow for one selected forensic image.
- AFF4 direct-map reader path for BlackBag-style APFS discontiguous image streams.
- APFS container, checkpoint, object-map, volume, root-tree, inode, directory-record, xattr, and file-extent diagnostics in support mode.
- Direct APFS Store-V2 copy-out from AFF4 without mounting the source filesystem.
- Synthetic-zero provenance tracking for sparse or zero-physical-region reconstruction.
- Direct extent-chain trimming to inode/data-stream logical size where available.
- Immutable raw APFS copy-out locations under `ExtractedSpotlight/ApfsCopyOutByTarget` and normalized investigator-facing staging under `ExtractedSpotlight/StagedStoreV2`.
- Native Store-V2 parser handoff for staged Store-V2 candidates.
- Store-V2 raw records, raw key/value rows, date candidates, timeline rows, artifact rows, and bounded validation parsing.
- External reference comparison against a known extracted `.Spotlight-V100/Store-V2` tree.
- Candidate dual-process comparison to identify whether the normalized staged Store-V2 component is the best available APFS copy-out candidate.
- Apple/lzfse codec integration when the vetted source tree is vendored under `third_party/lzfse`.
- Centralized GUI view registry with macOS/iOS/shared platform routing.
- Current GUI export improvement: filtered-view CSV export now runs in a background worker.

## Current macOS investigator-facing views and outputs

- Store/native parse summary and parser limits.
- Raw Spotlight records and raw key/value records.
- Raw date candidates with source/provenance fields.
- Timeline/artifact outputs generated from parsed Store-V2 records.
- AFF4/APFS copy-out summary, Store-V2 staging summary, and candidate-selection audit outputs.
- External compare outputs: relative path/hash matches, hash matches under different paths, relative-path size mismatches, external-only, and Vestigant-only rows.
- Remaining mismatch diagnostics for APFS Store-V2 comparison.

## Anticipated macOS investigative features

- Full lower-bound APFS B-tree directory iterator comparator, then eventual promotion to the live extraction path after parity is demonstrated.
- More complete APFS path reconstruction from parent-child directory records and inode parents.
- LZFSE/LZVN decmpfs/resource-fork reconstruction validation against external reference hashes.
- Store-V2 completeness scoring by group and component.
- Better reconciliation for `MATCH_HASH_DIFFERENT_PATH`, `EXTERNAL_ONLY`, and `VESTIGANT_ONLY` comparison statuses.
- Spotlight-first views for files known only to Spotlight, files present in both Spotlight and APFS copy-out, and missing/deleted candidates.
- First-used, last-used, last-opened, downloaded-from, and source URL views when the parsed Store-V2 fields support those interpretations.
- External-device and volume/path/device-hint views when Spotlight contains those fields.
- Confidence and provenance fields for every interpreted date/path/use claim.
- GUI modularization: `ReviewDatabaseHelper`, `ReviewQueryManager`, and eventually `MainWindow` state encapsulation.
