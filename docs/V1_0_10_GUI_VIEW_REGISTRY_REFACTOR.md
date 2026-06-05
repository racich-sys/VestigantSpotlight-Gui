# V1.0.11 GUI View Registry Refactor

## Purpose

V1.0.11 begins reducing `src/gui/win32_gui.cpp` God-file risk without changing the investigator workflow. The highest-risk GUI issue was macOS/iOS view routing based on display-name substring checks inside the GUI file. That routing is now centralized in a dedicated view registry module.

## Implemented

- Added `src/gui/view_registry.h` and `src/gui/view_registry.cpp`.
- Moved `ViewSpec` and the full `views()` registry out of `win32_gui.cpp`.
- Added `ViewPlatform` enum: `MacOS`, `iOS`, `Shared`, `Auto`.
- Added platform inference and sort-priority assignment in the view registry, not in tab-routing code.
- Updated `win32_gui.cpp` tab filtering to use `ViewSpec::platform` rather than raw display-name substring checks.
- Moved `viewHelpText()` out of `win32_gui.cpp` into the view registry module.
- Added `view_registry.cpp` to the CMake Windows GUI target and no-CMake MSVC GUI link path.

## Also implemented

- Updated the AFF4/APFS wrapper so nonzero source-probe exits attempt to create a `_FAILED.zip` partial diagnostic upload before throwing. This addresses the V1.0.9 failure mode where the wrapper stopped before producing an upload bundle.

## Deferred

The following recommendations remain staged for later iterations:

1. `MainWindow` class extraction for Win32 handles and state. Benchmark: no global HWND or review-state variables remain in `win32_gui.cpp`, and `staticWndProc` dispatches through a window-owned instance.
2. `ReviewDatabaseHelper` extraction. Benchmark: export, tag, checked-artifact, and paged-query SQL no longer lives in `win32_gui.cpp`.
3. Review query lifecycle manager. Benchmark: all background review queries are owned by a single manager object with request cancellation, join, and SQLite progress-handler state.
4. Full APFS lower-bound iterator promotion. Benchmark: iterator output for `/.Spotlight-V100/Store-V2` matches or improves the current logical directory walk and staged Store-V2 output.

## Forensic note

V1.0.11 is intended as a low-risk structural refactor. It does not change AFF4/APFS extraction logic except the wrapper's failure packaging behavior.
