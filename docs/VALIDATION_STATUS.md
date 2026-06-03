# Vestigant Spotlight Validation Status

Current version: 0.9.46

## Reviewed V0_9_43 results

- Windows/MSVC build log: successful.
- Stage B fresh-ZIP run: `complete_success`.
- Fresh-ZIP case counts: 6 valid stores, 344,445 raw records, 982,668 raw key/value rows, 336,037 raw date candidates, 344,445 artifacts, 336,037 timeline events.
- V0_9_43 bplist discovery output: 2 summary rows and 438 detail rows.
- Fresh-ZIP defect: `ios_ffs_file_inventory.csv` and `ios_app_database_inventory.csv` had zero data rows; run status reported `ios_ffs_inventory_cpp_parser_complete files=0 app_databases=0 raw_records=0`.


## V0_9_46 validation performed here

- Reviewed uploaded V0_9_44 build log: Windows/MSVC build completed and produced CLI, GUI, and self-test binaries.
- Reviewed V0_9_44 reuse-cache thin output: run reached complete_success with 344,445 raw records and compact normal-mode behavior preserved.
- Reviewed V0_9_44 fresh-ZIP thin output: run reached complete_success and native C++ 7-Zip inventory parsed 2,245,783 file entries, 131,610 app-database candidates, and 2,245,783 raw records.
- Classified remaining defect: app-database inventory over-classified non-database files under SMS/app paths because broad path-family checks did not require database-like file names.
- Applied focused C++/PowerShell helper fixes to require database-like names for app DB inventory classification and preserve extracted database paths for native inventory rows.

## V0_9_44 validation performed here

- `src/app/app_runner.cpp`: Linux syntax check passed with warnings only.
- `src/parsers/native_storedb_parser.cpp`: Linux syntax check passed.
- Raw-string size scan: no oversized literals found above the configured threshold.
- Full Linux build: attempted, but the full build/link did not complete in the available runtime window.

## Required Windows validation

1. Run `scripts\Build-V0_9_46.ps1`.
2. Confirm CLI reports `Vestigant Spotlight v0.9.44`.
3. Run `VestigantSpotlightTests.exe`.
4. Run reuse-cache test and confirm `complete_success`.
5. Re-run Stage B fresh-ZIP test and confirm FFS/app-database inventory rows are nonzero.
