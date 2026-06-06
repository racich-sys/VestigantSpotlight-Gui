# V1.0.24.1

Windows/MSVC build hotfix for V1.0.24 GUI view helper modularization.

## Changed

- Removed the stale anonymous-namespace `buildWhere(const ViewSpec&, const std::string&)` wrapper from `src/gui/win32_gui.cpp`.
- Updated the review-page SQL assembly path to explicitly call `vestigant::spotlight::buildWhere(v, search, capturedFilterColumn, capturedFilterValue)`.
- Updated versioned PowerShell build/launch/AFF4 wrapper scripts for V1.0.24.1.

## Not changed

- macOS AFF4/APFS traversal, copy-out, staging, Store-V2 parsing, or external-compare behavior.
- APFS diagnostic writer locations.
- iOS CoreSpotlight extraction/parsing or iOS GUI view behavior.
- Apple/lzfse codec behavior.
- SQLite schema.
- GUI export worker backend logic other than the compile-time caller disambiguation.

## Validation

- Reviewed uploaded `V1_0_24_build.log` and confirmed the failure was `C2668` ambiguous `buildWhere` overload resolution in `win32_gui.cpp`.
- Confirmed the local wrapper was removed and only the shared helper implementation remains.
- Rebuilt and ran the controlled SQLite backend smoke test for Export Current Page and Export Filtered against the V1.0.24.1 source.
- Windows/MSVC validation is still required on the target Windows environment.
