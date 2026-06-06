# V1.1.1 Validation Notes

## Inputs reviewed

- Uploaded `V1_0_31_build.log`: Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.31`.
- Uploaded `Upload_Thin_MacOS_AFF4_V1_0_31.zip`: generated successfully; denied raw thin-upload filenames were absent.
- Uploaded review markdown: requested database-lifetime cleanup, codec relocation, intake isolation, rerun-plan relocation, AFF4 stream inventory relocation, APFS path/leaf helpers, GUI state work, NSKeyedArchiver work, and NXSB parser relocation.

## Changes validated locally

- Single `CaseDatabase db` declaration remains in `src/app/app_runner.cpp`.
- Decmpfs/resource-fork reconstruction helpers are declared/implemented in `src/codec/lzfse_codec.*`.
- `parseApfsNxSuperblock()` is declared/implemented in `src/parsers/apfs_volume_reader.*`.
- `runAff4StreamInventory()` is declared/implemented in `src/parsers/apfs_aff4_reader.*`.
- `writeAff4ApfsV1DiagnosticRerunPlan()` is declared/implemented in `src/parsers/apfs_diagnostic_exporter.*`.

## Local checks

- C++20 syntax checks passed for changed and dependent files.
- Linux CMake configure/build completed.
- CLI version check reported `Vestigant Spotlight v1.1.1`.
- Local self-test passed.

## Not validated here

- Windows/MSVC build.
- Windows GUI runtime.
- Windows guarded AFF4 dynamic-load runtime.
- V1.1.1 macOS AFF4/APFS thin run.
- Current iOS run.

## Risk notes

- APFS path/leaf helper APIs are not wired into live evidence output.
- NSKeyedArchiver output was not added because a placeholder resolver would risk misleading interpretation.
- The AFF4/APFS dynamic load probe remains large; full worker extraction is still deferred until comparator evidence is available.
