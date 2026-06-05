# V1.0.9 Modularization Review

## Input review

The V1.0.8 Windows build log uploaded by the user shows that the new parser modules compiled under MSVC, and that the CLI, test executable, and GUI executable were linked successfully with version `Vestigant Spotlight v1.0.8`.

The V1.0.8 AFF4/APFS thin upload showed that the guarded AFF4/APFS pipeline is now past the original extraction blocker: Store-V2 files are being staged and parsed. The relevant measured metrics from the uploaded thin package were:

- `aff4_apfs_extracted_storev2_stage_files.csv`: 884 staged Store-V2 rows.
- `aff4_apfs_spotlight_file_copy_out.csv`: 9,902 copy-out rows.
- `aff4_apfs_spotlight_file_extent_probe.csv`: 27,374 file-extent probe rows.
- `aff4_apfs_spotlight_inode_probe.csv`: 16,539 inode-probe rows.
- `aff4_apfs_logical_directory_walk.csv`: 16,363 logical directory walk rows.
- staged Store-V2 parser probe: 12 selected databases, 2 valid stores, 25,000 parsed/raw records, 2,181 key/value rows, and 25,000 date candidates.
- external comparison: 4,123 external files, 9,222 Vestigant files, 11,077 compare rows, 3 match rows, and 11,074 non-match rows.

## Implemented in V1.0.9

1. Added shared APFS B-tree key/value decoder functions to `src/parsers/apfs_aff4_reader.*`:
   - `apfsAff4DecodeFixedKvAbs`
   - `apfsAff4DecodeGenericBtreeKvAbs`

2. Converted the app-runner-local `aff4ApfsFixedKvAbsForProbe` and `aff4GenericBtreeKvAbsForProbe` implementations into thin wrappers around the parser module.

3. Updated iOS app DB table routing to use `IosAppDbTableParseDecision` from `src/parsers/ios_app_db_parser.*`. This keeps WhatsApp, Apple Messages, and KnowledgeC special-parser selection in the iOS parser module while leaving row extraction unchanged.

4. Corrected stale AFF4 source status text. The run status now describes AFF4/APFS as an active guarded metadata and Store-V2 staging pipeline rather than `container/filesystem extraction not implemented`.

## Not implemented in V1.0.9

1. Full iOS row parser migration was not completed. The remaining Apple Messages, WhatsApp, KnowledgeC, and generic SQLite row parsers still rely on local `SqlStatement` binding and timestamp helper functions in `app_runner.cpp`. The next safe step is to introduce an `IosParsedRecord` sink interface, then move these functions without changing row semantics.

2. Full AFF4/APFS structural extraction was not moved out of `app_runner.cpp`. V1.0.9 moved the shared B-tree TOC decode logic first. The next safe APFS step is to move NXSB parsing and inode extended-field decoding into typed parser-module functions with tests.

3. The true APFS lower-bound iterator was not promoted into live extraction. It should not replace the current working extraction path until a diagnostic run proves that iterator enumeration for `/.Spotlight-V100/Store-V2` matches the existing staged namespace output.

4. LZFSE/LZVN was not added. The requirement remains a vetted `third_party/lzfse/` source tree, MSVC/Linux build integration, and known-good decompression vectors before production use.

## V1.0.10 benchmarks

- Move iOS parsed-row emission behind an `IosParsedRecordSink` and migrate at least Apple Messages message/attachment/participant parsers out of `app_runner.cpp`.
- Move `parseApfsNxSuperblock` and `decodeApfsInodeExtendedFieldsForProbe` into parser modules with tests.
- Add an APFS iterator diagnostic CSV that compares lower-bound iterator output against the current `aff4_apfs_logical_directory_walk.csv` namespace output for Store-V2.
- Maintain or improve V1.0.8 AFF4/APFS metrics: nonzero staged Store-V2 rows, nonzero parsed raw records, and external comparison output.
