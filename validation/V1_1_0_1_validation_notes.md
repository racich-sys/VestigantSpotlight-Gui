# V1.1.1 validation notes

## Reason

The V1.1.0 ZIP extracted successfully and SHA256 matched, but `scripts/Build-V1_1_0.ps1` failed because `build_windows_msvc.bat` was absent from the package root.

## Changes

- Restored root `build_windows_msvc.bat`.
- Restored root `build_windows_msvc_nocmake.bat`.
- Restored root `build_linux_test.sh`.
- Updated version metadata and wrappers to V1.1.1.

## Not changed

No parsing, APFS traversal, AFF4 reads, iOS processing, GUI behavior, Store-V2 parsing, or SQLite schema logic changed.

## Local checks

- Verified restored root build scripts exist in source tree.
- Verified version metadata points to 1.1.1.
- Linux CMake configure passed.
- Linux CMake build passed.
- CLI version check returned `Vestigant Spotlight v1.1.1`.
- Local self-test passed.

## Pending

- Windows/MSVC build.
- macOS AFF4/APFS thin run.
