# Vestigant Spotlight Release Notes

Current version: 0.9.44

## V0_9_46

- Reviewed V0_9_45 build, reuse-cache thin output, and fresh-ZIP thin output.
- Confirmed V0_9_45 completed both reuse-cache and fresh-ZIP runs.
- Confirmed fresh-ZIP inventory now reports nonzero FFS rows and a much smaller app database candidate set.
- Tightened app database categorization so generic `signals` and `history` database names do not become Signal or Chrome/Web evidence without stronger path/app identifiers.
- Updated stale VERSION/VERSION.txt/CMake project metadata so the build banner matches the package version.

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
