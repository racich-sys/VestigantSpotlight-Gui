# V1.0.22 GUI Export Worker Modularization

V1.0.22 continues the production-readiness cleanup after V1.0.21 confirmed the Windows GUI build hotfix.

## Implemented

- Moved current-page review export SQL/CSV backend execution out of `win32_gui.cpp` and into `GuiExportWorker`.
- Moved filtered-view review export SQL/CSV backend execution out of `win32_gui.cpp` and into `GuiExportWorker`.
- Added `GuiViewExportRequest` so the UI layer snapshots view, filter, sort, page, and checked-artifact state before launching a background export.
- Added a dedicated export-page completion message and guard so current-page export cannot be double-started.
- Kept Win32 dialog prompting and button enable/disable behavior in the GUI layer.
- Left checked/tagged export support in the existing worker backend.

## Non-goals

No extraction, Store-V2 parsing, APFS copy-out, Apple/lzfse, iOS CoreSpotlight, or database schema behavior was changed.

## Validation expectation

The GUI should remain responsive during Export Page, Export Filtered, Export Checked, and Export Tagged operations.
