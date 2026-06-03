# Vestigant Spotlight Release Notes

Current version: 0.9.47

## V0_9_47 - iOS investigative-value update

V0_9_47 reviews the uploaded V0_9_46 Windows/MSVC build log, reuse-cache thin upload, fresh-ZIP thin upload, and the new parser recommendations. The V0_9_46 build completed successfully, and both normal validation runs reached `complete_success`. No build/runtime hotfix was required.

Changes:

- Replaced the crude bounded bplist printable-token fallback with a conservative native `bplist00` object-string extractor for ASCII and UTF-16BE string objects, with size/object caps and fallback scanning for damaged/truncated blobs.
- Preserved the bplist feature as bounded string discovery only; it does not claim full NSKeyedArchiver object-graph decoding.
- Added KnowledgeC/CoreDuet database classification for `knowledgeC.db`, `interactionC.db`, and `globalKnowledge.db`, plus targeted extraction patterns for future support-mode app database materialization.
- Added specialized KnowledgeC `ZOBJECT` parsing scaffolding for `/app/inFocus`, `/document/open`, and `/app/intents` streams when support/full app database record materialization is enabled.
- Added investigator review views and exports for KnowledgeC/CoreDuet interaction summaries/events. Normal mode still skips broad app database record materialization by default.
- Added an explicit `vw_investigator_time_anomalies` triage view/export that compares available Spotlight-derived usage/download/update fields and includes an interpretation warning.
- Updated GUI iOS view registry entries, export package output, scripts, version metadata, and validation documentation.

Validation here:

- Reviewed V0_9_46 build log and confirmed source/banner/binary version consistency.
- Reviewed V0_9_46 reuse-cache and fresh-ZIP thin outputs; both completed successfully.
- Confirmed fresh-ZIP inventory remained at 2,245,783 files and 5,528 app database candidates.
- Confirmed V0_9_46 already extracted/staged high-value databases but classified KnowledgeC/CoreDuet targets as generic database candidates.
- Changed-file Linux `g++ -fsyntax-only` checks passed.
- SQLite smoke test for new V0_9_47 views passed.
- Raw-string size scan found no oversized raw-string literals above the configured threshold.

Windows/MSVC validation still required:

- Build V0_9_47 with `scripts\Build-V0_9_47.ps1`.
- Run the reuse-cache validation.
- Run Stage B fresh-ZIP validation.
- For KnowledgeC parsed rows, run a support/full materialization profile after confirming normal-mode stability.

## V0_9_44 - Fresh ZIP FFS inventory recovery and native parser efficiency

V0_9_44 reviews the uploaded V0_9_43 build log, reuse-cache thin upload, and Stage B fresh-ZIP thin upload. The Windows/MSVC build succeeded and the Stage B fresh-ZIP run reached `complete_success`, but the fresh-ZIP iOS FFS/app-database inventory CSVs contained zero rows even though CoreSpotlight stores were extracted and parsed. The run status showed `ios_ffs_inventory_cpp_parser_complete files=0 app_databases=0 raw_records=0`, which indicates the raw 7-Zip inventory handoff did not parse correctly.

Changes:

- Fixed the fresh-ZIP 7-Zip raw inventory handoff by avoiding Windows PowerShell UTF-16 redirection for `7z l -slt` output.
- Added native C++ raw-listing line normalization that can decode older UTF-16LE/UTF-16BE PowerShell-redirection logs as a fallback.
- Added a warning status if a raw 7-Zip listing exists but the C++ parser produces zero inventory records.
- Converted the native metadata item parser to support zero-copy bounded item parsing from the decompressed metadata block payload, reducing per-item heap copying during native parse.
- Improved bounded high-value probe deduplication and non-ASCII preservation for fallback/CoreSpotlight probes.
- Hardened `cleanDecodedString` so trailing null/space padding does not prevent removal of the CoreSpotlight `0x16 0x02` trailer marker.
- Updated version metadata, scripts, help, validation notes, and roadmap to V0_9_44.

Validation here:

- Reviewed V0_9_43 fresh-ZIP thin output: run completed, 6 valid stores, 344,445 raw records, 982,668 raw key/value rows, 336,037 date candidates, 438 bplist/NSKeyedArchiver detail rows, but 0 FFS inventory rows.
- `src/app/app_runner.cpp` passed Linux `g++ -fsyntax-only` with warnings only.
- `src/parsers/native_storedb_parser.cpp` passed Linux `g++ -fsyntax-only`.
- Raw-string size scan found no oversized raw-string literals above the configured threshold.
- Full Linux build was attempted; it progressed through native parser compilation and app_runner syntax, but full link/build was not completed in the available runtime window.

Windows/MSVC validation still required:

- Build V0_9_44 with `scripts\Build-V0_9_44.ps1`.
- Run the reuse-cache validation.
- Re-run Stage B fresh-ZIP validation and confirm `ios_ffs_inventory_cpp_parser_complete` reports nonzero `raw_records` / `files`, and that `ios_ffs_file_inventory.csv` and `ios_app_database_inventory.csv` contain rows.
