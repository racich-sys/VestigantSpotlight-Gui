# V1.0.29 Build Script and Low-Risk Hardening

## Purpose

V1.0.29 fixes the stale V1.0.28.2 build-wrapper version gate and applies low-risk hardening items from the current review without changing APFS traversal, Store-V2 parsing, iOS parsing, schema, or forensic interpretation.

## Implemented

- Build wrapper now expects `1.0.29`.
- Redirected child-process log handle is closed in the parent immediately after successful process creation.
- AFF4 dynamic probe uses `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`.
- Win32 review ListView redraw is suspended during bulk row insertion and restored after population.
- Dynamically globbed thin-upload export CSVs are capped at 50 MB.
- Standalone thin-upload PowerShell helper applies the same cap for export CSVs.

## Deferred

- APFS absolute path reverse walker.
- True APFS next-leaf iterator replacement.
- iOS NSKeyedArchiver unflattener.
- Evidence intake module relocation.
- Full Win32 GUI global-state encapsulation.
- Joined GUI export-thread lifecycle.

Those are still tracked as future work because they affect extraction, parser semantics, or GUI lifecycle and should not be combined with a build-wrapper/hardening release.
