# V1.1.10.1 Validation Notes

## Scope

Documentation/script-command hotfix on V1.1.10.

## Local validation performed

- Confirmed current-version scripts exist for V1.1.10.1.
- Confirmed `VERSION`, `VERSION.txt`, `CMakeLists.txt`, and `src/core/app_info.cpp` report 1.1.10.1.
- Confirmed `docs/NEW_CHAT_CONTINUATION_GUIDE.md`, `BUILD_INSTRUCTIONS.md`, `docs/QUICK_START.md`, and `HELP.md` include the full requested extract/build command block.
- Confirmed macOS AFF4/APFS thin command is documented as `Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut`.

## Not run here

- Windows/MSVC build was not run in this environment.
- AFF4/APFS thin run was not run in this environment.
- iOS run was not run because no iOS code changed.

## TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build.
- iOS: not required.
- Reason: documentation/script wrapper correction only; no extraction/parser/schema behavior changed.
