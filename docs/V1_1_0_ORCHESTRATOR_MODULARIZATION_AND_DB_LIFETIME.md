# V1.1.1 Orchestrator Modularization and Database Lifetime

## Purpose

V1.1.1 is a broader `repeat`-cycle release. It targets the remaining production-readiness concerns that can be moved safely without changing live APFS extraction semantics or emitting new unvalidated forensic interpretations.

## Implemented

- `runApplication()` now opens `CaseDatabase` once and reuses that handle through AFF4/raw and general processing.
- APFS decmpfs/resource-fork reconstruction helpers were moved to the codec module.
- APFS NX superblock parsing was moved to `apfs_volume_reader.cpp/.h`.
- AFF4 stream inventory classification/reporting was moved to `apfs_aff4_reader.cpp/.h` with callback injection for process execution.
- `writeAff4ApfsV1DiagnosticRerunPlan()` was moved to `apfs_diagnostic_exporter.cpp/.h`.
- Non-live APFS path/leaf helper APIs were added for future comparator work.

## Not implemented

- Live APFS traversal replacement.
- APFS staged path substitution using the new path helper.
- Full `writeAff4CppLiteDynamicLoadProbe` worker extraction.
- Full evidence staging/import orchestration movement.
- NSKeyedArchiver unflattened output.
- Win32 GUI global state rewrite.

## Validation

- C++20 syntax checks were run for changed and dependent source files.
- Linux CMake configure/build completed.
- CLI version check reported `Vestigant Spotlight v1.1.1`.
- Local self-test passed.

Windows/MSVC and live macOS/iOS runtime validation remain required.
