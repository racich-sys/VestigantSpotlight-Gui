# V1.0.18 GUI Export and Modularization Cleanup

## Purpose

V1.0.18 addresses the V1.0.17 review notes that focused on incomplete modularization and Win32 GUI responsiveness.

## Verified structure

The reviewed V1.0.17 tree did not contain a duplicate `ViewSpec` or duplicate `views()` implementation in `src/gui/win32_gui.cpp`. The only production definitions are in `src/gui/view_registry.h` and `src/gui/view_registry.cpp`.

The reviewed V1.0.17 tree also did not contain local copies of the specialized Apple Messages, WhatsApp, KnowledgeC, or generic iOS app database row parser bodies in `src/app/app_runner.cpp`; those bodies were already in `src/parsers/ios_app_db_parser.cpp`.

## Implemented cleanup

- Added an `IosAppDbParser` class facade in `src/parsers/ios_app_db_parser.h/.cpp`.
- Updated `parseIosAppDatabaseRecordInventories()` to call `IosAppDbParser::buildTableParseDecision()`, `IosAppDbParser::isTargetRecordCategory()`, and `IosAppDbParser::parseTable()`.
- Kept the existing free-function parser API as a compatibility layer during the transition.
- Converted `exportFilteredView()` to a background worker thread so large CSV exports do not run on the Win32 message loop thread.
- Added `WM_EXPORT_FILTERED_RESULT` completion handling to marshal export status back to the UI thread.
- Added a single-export guard so the user cannot start multiple filtered exports at the same time.
- Snapshot checked artifact IDs before starting the worker to avoid reading mutable UI state from the background export thread.
- Added `IncludeStructuralDiagnostics` to `Create-SourceProbeUploadZip.ps1` so normal thin uploads omit heavy structural APFS CSVs unless the wrapper is run with diagnostic output enabled.

## Deferred cleanup

- Move remaining SQLite review/export/tag logic into a future `ReviewDatabaseHelper` module.
- Move review-page background query lifecycle into a future `ReviewQueryManager` module.
- Continue reducing `app_runner.cpp` by moving AFF4/APFS copy-out/staging implementation into APFS parser/orchestration modules.
- Run the lower-bound APFS B-tree iterator as a comparator before replacing the current live extraction path.
