# V1.0.8 Modularization Review

## Inputs reviewed

- V1.0.7 Windows build log: MSVC compiled the APFS volume reader, app runner, CLI, tests, and GUI; the built binary reported Vestigant Spotlight v1.0.7.
- V1.0.7 AFF4/APFS thin upload: AFF4/APFS copy-out is now producing nonzero staged Store-V2 outputs.
- User modularization suggestions: move iOS app database parser decisions and APFS/AFF4 traversal mechanics out of `app_runner.cpp` before implementing a true APFS lower-bound B-tree iterator or LZFSE/LZVN decompression.

## V1.0.7 benchmark summary from the uploaded thin output

- `aff4_apfs_spotlight_file_copy_out_summary.json`: 9,902 copy-out rows; 9,166 copied files; 1,428,004,864 copied bytes.
- `aff4_apfs_extracted_storev2_stage_summary.json`: 11 staged Store-V2 groups; 884 staged files; 1,409,941,504 staged bytes.
- `aff4_apfs_logical_directory_walk_summary.json`: 917,903 indexed inode records; 918,384 indexed file-extent records; 16,539 materialized target inode rows; 27,374 materialized target file-extent rows.
- `aff4_apfs_external_spotlight_compare_summary.json`: 4,123 external reference files; 9,222 Vestigant staged files; 3 direct match rows; 1,855 external-only rows; 6,954 Vestigant-only rows.

## Implemented in V1.0.8

### iOS app database parser module boundary

Added:

- `src/parsers/ios_app_db_parser.h`
- `src/parsers/ios_app_db_parser.cpp`

Moved the low-risk, table-classification and special-parser routing logic out of `app_runner.cpp` into the parser module:

- record category classification for Messages, WhatsApp, KnowledgeC/CoreDuet, browser, mail, calendar, contacts, and generic SQLite support tables;
- WhatsApp special-parser decision;
- Apple Messages special-parser decision;
- KnowledgeC special-parser decision;
- KnowledgeC snippet assembly;
- table parse-decision struct for future migration of row parsers.

`app_runner.cpp` now calls the new module wrappers for these decisions. The row extraction functions are still in `app_runner.cpp` because they remain tightly coupled to the current `CaseDatabase::Statement` insertion object and the local `IosAppDbInv` structure.

### APFS/AFF4 reader module boundary

Added:

- `src/parsers/apfs_aff4_reader.h`
- `src/parsers/apfs_aff4_reader.cpp`

The new module contains the lower-bound directory-iterator API shape requested by the user, but with callback injection rather than direct AFF4 reads. This lets the future live iterator reuse the current guarded AFF4/APFS read functions without moving all low-level reader state in one risky release.

The module includes:

- APFS B-tree key/value location structure;
- lower-bound iterator result/benchmark structure;
- callback hooks for leaf location, node read, key/value decode, and next-leaf resolution;
- `getDirectoryContents(parentInodeId)` iterator scaffold with cycle detection and next-leaf transition counters;
- `decodeDirectoryRecord()` for APFS Type 9 directory entries.

### Build integration and tests

- Added the two new modules to CMake.
- Added the two new modules to the MSVC no-CMake common-object manifest.
- Added smoke tests for iOS app database classification/routing.
- Added smoke tests for APFS/AFF4 directory-record decoding and missing-callback guard behavior.

## Delayed intentionally

### Full iOS row-parser migration

The following functions remain in `app_runner.cpp` for now:

- `parseAppleMessagesSmsDbMessageRows`
- `parseAppleMessagesSmsDbParticipantRows`
- `parseAppleMessagesSmsDbAttachmentRows`
- `parseWhatsAppIosTableRows`
- `parseKnowledgeCIosZObjectRows`
- `parseIosAppDbTableRows`

Benchmark for moving them in V1.0.9/V1.1: introduce a parser-independent insertion interface or row sink so the iOS module does not need to depend on app-runner local types.

### Live APFS lower-bound iterator wiring

The lower-bound iterator API exists in `ApfsAff4Reader`, but the live AFF4/APFS reader still uses the current exhausted traversal/indexed-copy pipeline. This avoids changing traversal semantics in the same release that adds module boundaries.

Benchmarks before replacing the live traversal:

- current V1.0.7 copy-out count and staged-byte count must be preserved or improved;
- the new iterator must enumerate `/.Spotlight-V100/Store-V2` on the same AFF4 image;
- iterator output must match current logical namespace rows for known directories;
- next-leaf transition, malformed-node, and cycle-stop counters must be emitted in support diagnostics.

### LZFSE/LZVN

Still delayed. It requires vetted `third_party/lzfse/` source, MSVC/Linux build integration, and known-good test vectors before enabling production extraction.

## Cleanup status

- No `V0_9_*` run/package/collect scripts were present in the clean V1.0.8 package.
- No `V0_9_*` validation summaries were present.
- No `docs/codex_notes/CHANGES_Codex_*.md` files were present.
- No `Run-SelfTest.ps1`, app-runner `selfTest()`, or `/Users/alice/Documents/...` fake-evidence route was present.
