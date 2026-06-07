# V1.3.0 Local Validation Notes

## Scope

V1.3.0 is a stability/architecture milestone based on V1.2.1. It implements Group A work only and does not add new APFS/iOS forensic interpretations.

## Local checks performed in packaging environment

- Version/script metadata updated to 1.3.0 / V1_3_0.
- Verified build/thin wrapper command blocks were regenerated for V1.3.0.
- Grepped AFF4 worker for remaining local `nextNode` allocations at APFS horizontal leaf call sites; none remained.
- Verified current GUI export workers are registered through `registerExportThread(...)`; no `.detach()` calls were present in `win32_gui.cpp`.
- Created APFS traversal consolidation and GUI database access audit documentation.

## Required Windows validation

Run the V1.3.0 Windows/MSVC build and then AFF4/APFS thin test. A full AFF4/APFS test should be decided after the thin output is reviewed.

## Linux build attempt

A local Linux CMake build was started. It configured successfully and compiled through `src/parsers/aff4_probe_worker.cpp` without compile errors before the environment timeout terminated the build during the next translation unit. Warnings were existing unused-helper warnings in non-Windows packaging context; no new syntax error was observed before timeout.
