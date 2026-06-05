# V1.0.20 GUI Export and APFS Diagnostic Modularization

## Purpose

V1.0.20 reduces long-term maintenance risk without changing evidence extraction behavior.

## GUI export worker

`win32_gui.cpp` previously owned UI event handling, SQLite export SQL, CSV formatting, support-file export, and status reporting for checked/tagged exports.  V1.0.20 moves the backend work for checked and tagged exports into `GuiExportWorker`.

The GUI still owns dialog prompts and status messages.  The worker owns read-only database access, CSV output, support CSV output, and support manifest creation.

## Threading behavior

Checked-artifact export and tagged-artifact export now run on detached background threads.  Completion is posted back to the UI thread through `WM_EXPORT_CHECKED_RESULT` and `WM_EXPORT_TAGGED_RESULT`.

This matches the prior filtered-export pattern and prevents large checked/tagged exports from freezing the Win32 message loop.

## APFS diagnostic export policy

`apfs_diagnostic_exporter.h/.cpp` now owns the policy for deciding whether heavy AFF4/APFS structural diagnostic CSVs are written.  Copy-out and staging evidence outputs remain outside this policy and continue to run in normal mode.

Full movement of APFS diagnostic CSV writer bodies is deferred until the associated row structs are moved out of `app_runner.cpp` into shared APFS diagnostic model headers.

## Validation checklist

- Build V1.0.20 under MSVC.
- Open the GUI.
- Export filtered view and confirm the GUI remains responsive.
- Export checked artifacts and confirm the GUI remains responsive.
- Export tagged artifacts and confirm the GUI remains responsive.
- Confirm generated support manifests and support CSVs are present.
- Confirm macOS/iOS extraction outputs are unchanged unless a separate extraction test is run.
