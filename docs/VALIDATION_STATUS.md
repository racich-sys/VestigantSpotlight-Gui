# Validation Status

## V0_9_57 - Windows GUI forward-declaration compile hotfix

V0_9_57 is a focused Windows/MSVC GUI build hotfix after V0_9_56 reached the GUI compile stage and failed with `C3861: setReviewSummary identifier not found` in `src\gui\win32_gui.cpp`. The fix adds a forward declaration for `setReviewSummary(const std::wstring&)` before the custom view-set helper functions that call it. No parser, ingest, cache, ZIP, FFS inventory, app DB, export, or forensic interpretation behavior was intentionally changed.

Current version: 0.9.57

## V0_9_57 - Windows MSVC batch-label build hotfix

V0_9_57 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55 local validation

Passed in this environment:

- Static balance check for `src/gui/win32_gui.cpp`.
- CMake configure.
- Linux CMake build for core library, CLI, and tests.
- `VestigantSpotlightTests` self-test.
- Verified removed V1-blocker GUI controls remain absent from GUI source.
- Verified legacy V7 importer source/build references remain absent.

Not validated in this environment:

- Windows/MSVC GUI compile and link.
- Runtime GUI logo/layout behavior.
- Runtime custom view set save/hide/move/reset behavior.
- Runtime tag-management repair on an existing case database.
- Runtime processing telemetry during ingest.

## Expected Windows validation

- Build banner reports source version `0.9.55`.
- Binary reports `Vestigant Spotlight v0.9.55`.
- Case Information tab shows the logo/header and cleaner processing layout.
- Tags button opens Tags / Notes and tags can be created/applied/removed.
- Custom view sets persist for macOS and iOS tabs.
- Bottom Case Information log shows elapsed-time processing status during runs.
