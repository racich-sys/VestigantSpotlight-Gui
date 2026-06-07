# V1.1.3 Export Cancellation and Purge Hardening

V1.1.3 is a repeat-cycle hardening release built from V1.1.2.

## Implemented

- GUI export workers now accept cancellation callbacks and check for shutdown before long SQLite export scans.
- Export Current Page, Export Filtered, Export Checked, and Export Tagged pass a shutdown-aware callback from the Win32 GUI.
- Orphan source-row cleanup now runs inside a single SQLite transaction, while still logging per-table purge warnings.
- RichEdit is loaded from System32 via `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_SYSTEM32)` when available and freed in `WM_DESTROY`.
- `ApfsVolumeReader` next-leaf helper scaffolding is improved for comparator work but is not wired into live AFF4/APFS staged extraction.

## Explicit non-changes

- No live APFS traversal replacement.
- No new forensic APFS path interpretation.
- No full NSKeyedArchiver UID graph decode.
- No SQLite schema changes.
