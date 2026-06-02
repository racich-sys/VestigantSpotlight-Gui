# V0_9_39 Review Notes

## Input reviewed

V0_9_39 was prepared after reviewing:

- `V0_9_38_build.log`
- `Upload_Thin_iOS_GUI_V0_9_38_ReusedCache_Check.zip`
- external V1-readiness suggestions focusing on GUI thread safety, SQL/schema ownership, and safe SQLite close behavior.

## Finding

V0_9_38 built successfully, but the reuse-cache run did not complete.  It reached native parsing and stopped at the SQLite size guardrail:

- `parsed_items=340000`
- `raw_key_values=978712`
- `raw_date_candidates=332422`
- DB size slightly above the 5 GiB guardrail
- WAL size 0

The row counts were not explosive; the failure was caused by normal compact-mode text/reference values still occupying too much SQLite space.  V0_9_37 and V0_9_38 intentionally improved Missing From FFS text visibility, but the stored value sizes still need to be bounded more tightly for the current large iOS source.

## Changes in V0_9_39

- Tightened normal iOS compact-mode native value storage:
  - default stored native value preview reduced from 4096 bytes to 1024 bytes;
  - same-record investigator text context reduced from 1800 bytes / 8 fields to 1200 bytes / 6 fields;
  - individual same-record text field samples reduced from 320 bytes to 240 bytes.
- Raised the default DB/WAL guardrail from 5 GiB to 6 GiB because the stable compact case is very close to the previous arbitrary guardrail while still far below the earlier 20+ GiB bloat failure class.
- Added minimal GUI review-thread lifecycle hardening:
  - the review-page loader is now a tracked thread instead of a detached thread;
  - SQLite progress handler cancels long review queries when the user cancels, changes views, or closes the GUI;
  - shutdown joins the review thread before process exit.
- Added safer SQLite close behavior:
  - `CaseDatabase::close()` falls back to `sqlite3_close_v2()` if `sqlite3_close()` reports `SQLITE_BUSY`;
  - GUI read-only DB helper uses `sqlite3_close_v2()`.

## Deferred

- Moving all GUI fallback SQL out of `win32_gui.cpp` remains on the roadmap.  It is worthwhile, but moving thousands of lines of view SQL in the same release as parser-size and thread-safety changes would be riskier than useful for the immediate V1 goal.
- Full Win32 global-state refactoring and app_runner monolith breakdown remain V2/backlog work unless they block V1 stability.
