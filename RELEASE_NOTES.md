# Vestigant Spotlight V0_9_59 Notes

V0_9_59 is a V1 production-readiness cleanup after V0_9_57 compiled and ran on Windows. It improves the processing workflow and review workflow without changing parser interpretation logic.

Key changes:
- The Case Information bottom log is now the live processing log. It clears at run start, timestamps messages, mirrors run/progress status, and emits periodic heartbeat messages while processing continues.
- View loading now shows an explicit marquee progress indicator above the investigation grid and a loading message in the details pane so long SQLite view loads are not mistaken for hangs.
- The V1 GUI source selector now exposes only fully implemented Folder and ZIP intake paths. AFF4/APFS and raw image support remain roadmap items and are not presented as clickable V1 options.
- Legacy V7-only schema tables/indexes were removed from new case initialization.
- CLI/operator self-test mode is deprecated; the automated test executable uses an internal automated self-test path.
- Duplicate AFF4/APFS child/descendant root-tree probe output writers were consolidated into one traversal-output writer.

Validation summary:
- Linux CMake configure/build passed.
- VestigantSpotlightTests passed.
- C++20 syntax checks passed for modified non-Windows translation units.
- Windows/MSVC GUI compile and runtime validation remain required.

# Vestigant Spotlight Release Notes

Current version: 0.9.59

## V0_9_59 - Windows MSVC batch-label build hotfix

V0_9_59 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55 - V1 GUI polish, custom view sets, processing telemetry, and tag-management repair

V0_9_55 is a GUI-focused V1 readiness release built on the stable V0_9_54 parser/intake baseline. It does not intentionally change parser, cache, ZIP staging, FFS inventory, or app database classification behavior.

Changes:

- Added Vestigant branding to the Case Information workflow using the supplied green Vestigant logo.
- Cleaned the Case Information / Build Processing layout to reduce visual clutter and make the workflow more investigator-focused.
- Reused the bottom Case Information log area as the live processing log/status pane.
- Added processing telemetry messages for elapsed processing time and estimated throughput when a measurable file source size is available.
- Added live elapsed-time status updates while processing runs.
- Added Custom view set support for both MacOS and iOS investigation tabs.
- Added view-set controls: Move Up, Move Down, Hide, Save Set, and Reset Set.
- Custom view sets are stored in the case database through `review_view_preferences`.
- Added a visible Tags button on the investigation toolbar to switch directly to the Tags / Notes workflow.
- Added existing-case GUI schema repair for tag tables, checked-row persistence, notes, and view preferences. This is intended to fix older cases where Manage Tags / tag actions were unavailable because the GUI interaction tables did not yet exist.
- Retained the V0_9_54 V1 production cleanup: Skip preservation, Self-test, Max records, Blocks/store, and legacy V7 GUI/CLI paths remain removed.
- Retained the two-column selected-row details table and investigation-tab-only visibility behavior.

Validation summary:

- Linux CMake configure/build passed for core library, CLI, and tests.
- `VestigantSpotlightTests` self-test passed.
- Static balance check passed for `src/gui/win32_gui.cpp`.
- Confirmed removed V1-blocker GUI controls and V7 importer references remain absent.
- Windows/MSVC GUI build and runtime GUI validation are still required.
