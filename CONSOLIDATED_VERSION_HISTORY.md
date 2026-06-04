# Consolidated Version History

## V0_9_57 - Windows GUI forward-declaration compile hotfix

V0_9_57 is a focused Windows/MSVC GUI build hotfix after V0_9_56 reached the GUI compile stage and failed with `C3861: setReviewSummary identifier not found` in `src\gui\win32_gui.cpp`. The fix adds a forward declaration for `setReviewSummary(const std::wstring&)` before the custom view-set helper functions that call it. No parser, ingest, cache, ZIP, FFS inventory, app DB, export, or forensic interpretation behavior was intentionally changed.

## V0_9_57 - Windows MSVC batch-label build hotfix

V0_9_57 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55

V0_9_55 is a GUI-focused V1 readiness release. It adds Vestigant branding, improves the Case Information / Build Processing layout, adds elapsed-time processing telemetry, routes processing text status into the bottom Case Information log pane, adds case-persisted Custom view sets, and repairs tag-management schema setup for older existing cases.

## V0_9_54

V1 production cleanup removed visible testing/developer controls and legacy V7 workflow code, and moved GUI review-view SQL creation into the database layer.
