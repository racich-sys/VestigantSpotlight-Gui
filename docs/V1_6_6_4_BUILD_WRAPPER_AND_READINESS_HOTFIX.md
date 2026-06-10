# V1.6.6.4 Build Wrapper and Release Readiness Hotfix

## Trigger

The V1.6.6.3 Windows build wrapper reached the pre-build release-readiness stage and stopped with:

```text
Generic KNOWLEDGEC_EVENTS still appears in communication/identity view predicates; use KNOWLEDGEC_COMMUNICATION_INTENT plus provenance markers instead
```

The user reported that the compile itself did not fail, but the wrapper output showed the release-readiness script threw before the build flow could complete normally.

## Review performed before changes

V1.6.6.4 reviewed the active source, wrappers, and text package from V1.6.6.3 with focus on:

- current version strings in `VERSION`, `VERSION.txt`, `CMakeLists.txt`, and `src/core/app_info.cpp`;
- Windows PowerShell build/run wrappers under `scripts/`;
- release-readiness and forensic-directive checks under `tools/`;
- current `.md` and `.txt` build, handoff, validation, and user-facing instructions;
- communication/identity view SQL in `src/db/case_db.cpp` and the Win32 GUI bootstrap SQL in `src/gui/win32_gui.cpp`;
- self-test coverage in `tests/main.cpp`.

## Implemented

- Updated the current Windows build wrapper to V1.6.6.4 and corrected the final CLI version assertion to `1.6.6.4`.
- Moved build-wrapper preflight checks so source extraction/clean extraction happens before wrapper compatibility, raw-string, and release-readiness checks. This prevents `-CleanExtract` from validating stale extracted source.
- Updated the Win32 GUI preview/bootstrap communication views so generic `KNOWLEDGEC_EVENTS` is not included as a communication/identity/frequency predicate. The GUI bootstrap SQL now uses `KNOWLEDGEC_COMMUNICATION_INTENT` and provenance markers, matching the main case schema.
- Reworked the release-readiness check so it distinguishes disallowed promotional predicates from allowed suppression guards that explicitly exclude generic KnowledgeC/device-app activity rows.
- Added release-readiness coverage for the Win32 GUI bootstrap SQL, not just `case_db.cpp`.
- Updated current text/wrapper/version references to V1.6.6.4.

## Validation performed locally

- Linux CMake configure/build: PASS.
- CLI version: `Vestigant Spotlight v1.6.6.4`.
- Self-test: PASS.
- Static wrapper/current-text audit: PASS.
- Static KnowledgeC promotional predicate audit: PASS for `src/db/case_db.cpp` and `src/gui/win32_gui.cpp`.
- PowerShell/MSVC execution: not run in this Linux environment.

## Test scope decision

Run the Windows/MSVC build wrapper first. The V1.6.6.4 change directly targets the release-readiness failure that occurred before the Windows build flow completed normally.

Run iOS thin after the Windows build passes. This remains required from V1.6.6.3 because the previous iOS thin validation target was to confirm the three V1.6.6.2 export timeouts are gone.

AFF4/APFS thin or full testing is not required for V1.6.6.4 unless Windows build/shared schema initialization regresses, because this version does not change AFF4/APFS traversal, copy-out, decompression, AFF4 source reading, APFS filesystem reading, or Store-V2 staging behavior.
