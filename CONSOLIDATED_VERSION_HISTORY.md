# V1.0.17

- Added optional Apple/lzfse LZFSE/LZVN codec integration path.
- Added `src/codec/lzfse_codec.h/.cpp` with safe no-output behavior when the codec is not compiled in.
- Added `tools/Prepare-LzfseThirdParty.ps1` to explicitly vendor and manifest Apple/lzfse source under `third_party/lzfse`.
- Updated CMake and no-CMake MSVC build scripts to compile the Apple decoder sources only when the vetted source tree is present.
- Updated APFS decmpfs resource-fork reconstruction so compression types 8/12 call the Apple codec adapter when available and record explicit decode/skipped statuses when unavailable or failed.
- Updated direct AFF4/APFS copy-out to prefer inode data-stream logical size over raw extent-chain end where available.
- Added validation/status documentation for logical-size trim and optional codec integration.

# V1.0.15

- Added AFF4/APFS Store-V2 candidate dual-process comparison.
- New outputs:
  - `aff4_apfs_storev2_candidate_dual_process_compare.csv`
  - `aff4_apfs_storev2_candidate_dual_process_compare_summary.json`
  - `AFF4_APFS_STOREV2_CANDIDATE_DUAL_PROCESS_COMPARE.md`
- The compare output audits raw APFS copy-out candidates against normalized `StagedStoreV2` selections.
- Added packaging and wrapper validation for the new compare outputs.
- Added LZFSE/LZVN source review documentation explaining why APFS structural documentation is authoritative for locating compressed content but not sufficient by itself to enable production codec output.
- Kept normal-mode AFF4/APFS structural diagnostics suppressed while keeping copy-out/staging/parser/enrichment/external-compare outputs enabled.

# V1.0.14

- Moved iOS app DB row parsing into `src/parsers/ios_app_db_parser.cpp`.
- Corrected AFF4/APFS normal-mode logging around suppressed diagnostics.
- Preserved Store-V2 staged parser handoff.
