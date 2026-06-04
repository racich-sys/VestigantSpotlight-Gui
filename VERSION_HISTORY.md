# Version History

## V0_9_57 - Windows GUI forward-declaration compile hotfix

V0_9_57 is a focused Windows/MSVC GUI build hotfix after V0_9_56 reached the GUI compile stage and failed with `C3861: setReviewSummary identifier not found` in `src\gui\win32_gui.cpp`. The fix adds a forward declaration for `setReviewSummary(const std::wstring&)` before the custom view-set helper functions that call it. No parser, ingest, cache, ZIP, FFS inventory, app DB, export, or forensic interpretation behavior was intentionally changed.

## V0_9_57 - Windows MSVC batch-label build hotfix

V0_9_57 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55

GUI-focused V1 readiness update. Added supplied Vestigant logo branding, cleaned the Case Information / Build Processing layout, added live elapsed-time processing telemetry to the bottom Case Information log pane, added case-persisted Custom view sets for macOS/iOS investigation tabs, and repaired tag-management schema availability for older existing cases. No parser, ZIP staging, cache, FFS inventory, app DB classification, or forensic interpretation behavior was intentionally changed.

## V0_9_54

V1 production cleanup. Removed visible developer/testing controls from the investigator GUI, removed legacy V7 importer source/build paths, and moved GUI review-view SQL ownership into the database layer while preserving older case compatibility.
