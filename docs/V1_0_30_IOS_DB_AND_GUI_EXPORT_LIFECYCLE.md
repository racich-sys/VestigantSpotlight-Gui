# V1.0.30 iOS DB Parser Boundary and GUI Export Lifecycle

## Purpose

V1.0.30 advances two review suggestions that were safe to implement without changing APFS/AFF4 extraction physics or SQLite schema:

1. Move remaining iOS app database record-inventory orchestration out of the application runner and into the parser module.
2. Stop detaching GUI export threads and join active export workers during window destruction.

## Implemented changes

- Added `IosAppDbParser::parseRecordInventories(...)` in `src/parsers/ios_app_db_parser.cpp`.
- Added `IosAppDbStatusWriter` callback support so the parser module can preserve existing run-status behavior without depending directly on `app_runner.cpp` internals.
- Reduced `app_runner.cpp::parseIosAppDatabaseRecordInventories(...)` to a delegating wrapper.
- Added GUI export thread registry helpers in `src/gui/win32_gui.cpp`:
  - `registerExportThread(...)`
  - `joinExportThreadsNoThrow()`
  - `postExportResult(...)`
- Replaced export `.detach()` calls for:
  - Export Current Page
  - Export Filtered View
  - Export Checked
  - Export Tagged
- Joined registered export threads during `WM_DESTROY` after review-thread shutdown.

## Explicit non-goals

V1.0.30 does not change:

- AFF4/APFS read or traversal logic.
- APFS copy-out or Store-V2 staging.
- iOS CoreSpotlight parsing semantics.
- SQLite schema.
- GUI platform separation.
- APFS reverse path walker.
- NSKeyedArchiver bplist object interpretation.

## Validation still required

- Windows/MSVC build.
- GUI runtime export testing with application close during/after export.
- Current iOS thin or focused parser run to confirm parser-module delegation preserves output counts.
