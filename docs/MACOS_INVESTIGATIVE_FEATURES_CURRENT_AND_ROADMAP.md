# macOS Investigative Features: Current and Anticipated

## Current implemented macOS/AFF4/APFS features

### Evidence source handling

- Folder and ZIP source workflows for ordinary exported Spotlight Store-V2 sources.
- Staged AFF4/APFS source option for a single explicitly selected AFF4 evidence file.
- Single-AFF4 policy in the development wrapper to avoid recursive evidence discovery.
- Reader-tool integration path for `aff4-cpp-lite` under `T:\VestigantReaderTools\aff4-cpp-lite`.

### AFF4/APFS discovery and reconstruction

- Direct AFF4 map/index/data reader path for selected AFF4 files.
- APFS container-superblock and volume-superblock discovery.
- APFS volume role/name reporting, including macOS Data/System-style volume context when resolvable.
- APFS root-tree, inode, directory-record, file-extent, xattr, and logical-directory evidence capture in support mode.
- Store-V2 namespace discovery from APFS directory records.
- Store-V2 file copy-out from APFS file extents.
- Synthetic-zero sparse-region handling with explicit provenance.
- Logical-size trimming using inode/dstream size where available.
- Immutable per-target APFS copy-out source folders to avoid duplicate-name overwrite.
- Normalized `ExtractedSpotlight/StagedStoreV2` investigator-facing staging.
- External reference comparison against `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`.
- Candidate dual-process comparison between raw copy-out candidates and normalized staged Store-V2 selections.

### Store-V2 parsing and enrichment

- Native Store-V2 parser handoff for APFS-staged stores.
- Bounded CoreFields parse mode for AFF4/APFS-staged Store-V2 validation.
- Raw record extraction, key/value extraction, and date-candidate extraction.
- Timeline event creation from parsed Store-V2 records.
- Parent/inode link enrichment where Spotlight data provides enough identifiers.
- Artifact rows for parsed Store-V2 items.

### GUI/investigator review surface

- macOS and iOS investigation view separation through the GUI view registry.
- Stronger typed view routing through `ViewPlatform` rather than view-name substring routing.
- View help text centralized with the view registry.
- Case provenance summary views.
- External comparison outputs that identify matches, external-only files, Vestigant-only files, size mismatches, and hash matches at different paths.

### Compression support now staged for validation

- Decmpfs zlib/plain resource-fork reconstruction path.
- Apple/lzfse vendored source tree for LZVN/LZFSE decode validation.
- LZVN/LZFSE decode adapter using the Apple reference implementation when compiled.
- Codec-enabled smoke test vector in the test executable.

## Near-term macOS investigative roadmap

### Extraction correctness

- Run V1.0.17 against the AFF4/APFS test image with Apple/lzfse enabled.
- Confirm LZVN/LZFSE rows reduce external-only and size-mismatch cache files.
- Add row-level decode failure inventory for any compressed files that still cannot be reconstructed.
- Promote lower-bound APFS directory iterator only after diagnostic comparison proves it matches or improves current staging.
- Add direct support for xattr stream-backed resource forks where the current xattr probe is suppressed in normal mode.

### Store-V2 completeness

- Increase Store-V2 parser completeness beyond bounded CoreFields validation once APFS staging parity improves.
- Add parser completeness summary per Store-V2 group.
- Identify missing component classes by comparing staged groups to external reference groups.
- Track whether each Store-V2 component came from ordinary data fork, synthetic sparse reconstruction, decmpfs inline data, or decmpfs resource fork.

### Investigator-focused artifact views

- macOS Spotlight file/artifact overview: paths, names, extensions, bundle IDs, domains, URLs, and content-type hints.
- Download/source review using Spotlight fields such as `kMDItemWhereFroms` when recovered.
- Last-used/use-evidence review using Spotlight use-date fields where present.
- Date provenance review showing the source field and confidence for each interpreted timestamp.
- External-device evidence review based on path/volume/device hints contained in Spotlight metadata.
- Missing/deleted candidate review comparing Spotlight-indexed artifacts against the APFS-staged current filesystem inventory.
- Store-V2 group health dashboard showing component completeness and parsing confidence.
- Hash/path comparison dashboard against external reference extraction when provided.

### GUI cleanup and modularization

- Extract remaining GUI database SQL into a `ReviewDatabaseHelper` module.
- Move review thread lifecycle into a small query manager class.
- Continue shrinking `app_runner.cpp` by moving APFS copy-out row construction into parser-level modules.
- Preserve iOS functionality while moving any remaining app DB row insertion coupling behind parser-independent row sinks.

## Longer-term macOS roadmap

- Full AFF4/APFS handoff into normal `discoverStores()` rather than source-probe wrapper mode.
- Read-only source profile for raw IMG/DD/APFS when the AFF4 pipeline is stable.
- LZFSE/LZVN vector corpus using real APFS decmpfs resource forks and expected logical outputs.
- Store-V2 property dictionary expansion for richer field names and less generic raw-value reporting.
- Timeline fusion with unified logs, FSEvents, quarantine/download metadata, and volume/device mount artifacts when available.
- Production-mode suppression of internal structural diagnostics by default, with support-mode opt-in only.
