# V0_9_37 Review Notes

## User issue

The user reported that some Spotlight CSV reports did not show recovered Spotlight text/content, especially for potential Missing From FFS data.  The content of potentially missing indexed data is vital and should not require manual SQLite searching.

## Review of V0_9_36

V0_9_36 built successfully on Windows/MSVC and the iOS reuse-cache thin upload reached `complete_success`.  Counts stayed stable in compact mode: 344,445 raw records, 982,230 compact raw key/value rows, 336,037 compact date candidates, 1,592,440 slim FFS lookup rows, and no broad app DB materialization.

## Implemented in V0_9_37

- Increased same-record compact Spotlight text context for reference-bearing iOS records from 1,800 bytes / 5 fields to 4,096 bytes / 12 fields.
- Added `vw_ios_spotlight_missing_from_ffs_text_detail` for row-level Missing From FFS review with visible text/content preview, text visibility status, validation locators, Spotlight record locator, source field, path, priority, and FFS lookup status.
- Added `vw_ios_spotlight_missing_from_ffs_text_coverage_summary` to show whether Missing From FFS candidates have visible same-record Spotlight text.
- Added normal exports `ios_spotlight_missing_from_ffs_text_detail.csv` and `ios_spotlight_missing_from_ffs_text_coverage_summary.csv`.
- Made full Missing From FFS candidates and high-value candidates normal exports because this is a small/high-value investigator surface in the current workflow.
- Added GUI view registrations for Missing From FFS text detail and coverage.

## Interpretation

Normal compact mode still does not persist every raw native/dbStr/property value.  V0_9_37 improves investigator visibility for Missing From FFS candidates by surfacing the best same-record Spotlight text context available in compact mode.  If a row still reports `NO_SAME_RECORD_TEXT_RECOVERED_IN_COMPACT_MODE`, the record may primarily contain path/reference identifiers, or text may be in native fields not yet decoded or not retained by compact-mode filtering.

## Validation summary

Vestigant Spotlight V0_9_37 validation checks

Inputs reviewed:
- V0_9_36_build.log: Windows/MSVC build completed; binary version Vestigant Spotlight v0.9.36.
- Upload_Thin_iOS_GUI_V0_9_36_ReusedCache_Check.zip: run reached complete_success; stable compact-mode counts; Missing From FFS candidates include same-record text context but text visibility needed stronger/exported detail.

Implemented:
- Increased compact iOS Spotlight same-record text context from 1800 bytes / 5 fields to 4096 bytes / 12 fields for reference-bearing records.
- Added vw_ios_spotlight_missing_from_ffs_text_detail and vw_ios_spotlight_missing_from_ffs_text_coverage_summary.
- Added normal exports for ios_spotlight_missing_from_ffs_text_detail.csv and ios_spotlight_missing_from_ffs_text_coverage_summary.csv.
- Moved full Missing From FFS candidate/high-value candidate exports into normal profile because these are vital investigator outputs and were small in the reviewed run.
- Added GUI registrations and schema smoke coverage for the new views.

src/db/case_db.cpp: raw string count=92 max_body_bytes=7557
src/gui/win32_gui.cpp: raw string count=63 max_body_bytes=9055

Syntax/build validation:
- g++ syntax checks passed for src/db/case_db.cpp, src/export_sql/sqlite_exporter.cpp, and tests/main.cpp.
- Linux CMake build completed successfully.
- CLI version returned Vestigant Spotlight v0.9.37.
- Linux self-test passed.

Windows/MSVC validation still required because GUI builds and string-literal handling are MSVC-sensitive.
