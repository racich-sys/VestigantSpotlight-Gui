# V1.1.3 Validation Notes

## Scope

V1.1.3 is a repeat-cycle hardening release based on V1.1.2. It implements shutdown-aware GUI export cancellation, orphan-source purge transaction wrapping, secure RichEdit loading, and non-live APFS next-leaf iterator scaffolding improvements.

## Baseline reviewed

- `V1_1_2_build.log`: Windows/MSVC build completed and reported `Vestigant Spotlight v1.1.2`.
- `Upload_Thin_MacOS_AFF4_V1_1_2.zip`: generated successfully; denied raw upload filenames were absent.
- V1.1.2 AFF4/APFS staged Store-V2 baseline remained stable: `raw_records=25000`, `raw_key_values=2181`, `raw_date_candidates=25000`, `artifact_count=25000`.

## Implemented

- Added `GuiViewExportRequest::shouldCancel`.
- Added cancellation checks to export current-page, export filtered, checked export, tagged export, and support CSV writer loops.
- Win32 GUI now passes `gShuttingDown` cancellation callbacks into export workers.
- `purgeOrphanSourceRows(...)` now attempts one SQLite transaction around the table purge loop.
- RichEdit is loaded via `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_SYSTEM32)` and freed on `WM_DESTROY`.
- `ApfsVolumeReader::enumerateDirectory(...)` can use the non-live APFS footer helper when no injected next-leaf reader is supplied.
- Workflow ledger, handoff, roadmap checklist, and suggestions/fixes tracker updated.

## Not changed

- No live AFF4/APFS extraction behavior change.
- No live APFS staged path substitution.
- No full NSKeyedArchiver UID graph decoding.
- No SQLite schema changes.
- No dynamic probe worker extraction yet.

## Local validation

- C++20 syntax checks passed for changed/dependent files.
- Linux CMake configure/build passed.
- CLI version check returned `Vestigant Spotlight v1.1.3`.
- Local self-test passed.
