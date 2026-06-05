# V1.0.19

- GUI-only iOS review hotfix.
- Stabilized View Set combo box visibility/z-order after resize in investigation tabs.
- Removed clipped splitter text/control artifact above Selected Row Details.
- Increased separation between the main result grid and detail pane.
- Populates Selected Row Details immediately for the first selected/clicked row.
- Keeps checkbox-click detail focus on the clicked row.
- No intended extraction, parser, codec, schema, or Store-V2 pipeline changes from V1.0.18.

# V1.0.18 Release Notes

V1.0.18 is a cleanup and GUI-responsiveness release on top of the Apple/lzfse-enabled V1.0.17 baseline.

## Changes

- Added an `IosAppDbParser` class facade and routed app-runner iOS app database parsing through that parser facade.
- Converted filtered-view CSV export in the Win32 GUI to a background worker so large exports do not block the UI message loop.
- Added `WM_EXPORT_FILTERED_RESULT` handling to return export status to the UI thread.
- Added a single-export guard for filtered exports.
- Updated thin-upload packaging so heavy APFS structural diagnostics are only included when diagnostic outputs are explicitly requested.
- Updated macOS investigative feature documentation and modularization cleanup documentation.

## Notes

The actual V1.0.17 tree did not contain duplicate GUI `ViewSpec/views()` definitions in `win32_gui.cpp`; the registry was already centralized in `view_registry.h/.cpp`. The actual V1.0.17 tree also did not contain the specialized iOS row parser bodies in `app_runner.cpp`; V1.0.18 adds the class facade requested by review notes while keeping the existing free-function compatibility API.

## Next work

- Run V1.0.18 on Windows/MSVC and upload the build and AFF4 thin output.
- Add `ReviewDatabaseHelper` and `ReviewQueryManager` modules.
- Run APFS lower-bound B-tree iterator as a comparator against current staged output before promoting it to live extraction.
