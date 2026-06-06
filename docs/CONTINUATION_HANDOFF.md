# Spotlight2 / Vestigant Spotlight Continuation Handoff

## Current packaged version

- Current source package: `VestigantSpotlightInv_V1_1_0_1.zip`
- Version string: `1.1.0.1`
- Base version reviewed before this package: `V1.0.31`
- V1.0.31 Windows/MSVC build log was uploaded and reviewed. It compiled and linked CLI, tests, and GUI, and reported `Vestigant Spotlight v1.0.31`.
- V1.0.31 macOS AFF4/APFS thin ZIP was uploaded and reviewed. Denied raw filenames were absent and the AFF4/APFS staged Store-V2 baseline remained stable.

## User shorthand

If a future request begins with `repeat`, treat it as: review all uploaded/copied information, identify the newest source/build/thin evidence, continue to the next version in the same project style, implement as many safe outstanding roadmap/suggestion items as possible, update the handoff/roadmap/suggestions tracker, package artifacts, and provide concrete PowerShell commands. The expected cadence for `repeat` is broader than a one-line hotfix when the current version builds: spend more time, make a larger coordinated but reviewable step, and run repeated checks before packaging.

## V1.1.0.1 scope

V1.1.0.1 is a larger orchestrator modularization and database-lifetime cleanup release. It preserves extraction semantics while moving testable logic out of `app_runner.cpp`.

Implemented:

- Opened `CaseDatabase` once in `runApplication()` and reused that handle through the AFF4/raw and general workflow instead of reopening the WAL database in separate blocks.
- Moved decmpfs/resource-fork reconstruction helpers from `app_runner.cpp` into `src/codec/lzfse_codec.cpp/.h`:
  - `ApfsDeflateBitReader` / `ApfsHuffmanTree`
  - bounded zlib inflater
  - decmpfs compression label helper
  - decmpfs resource-fork reconstruction helpers
- Moved APFS NX superblock parsing into `src/parsers/apfs_volume_reader.cpp/.h`.
- Moved AFF4 stream inventory classification/report generation into `src/parsers/apfs_aff4_reader.cpp/.h` using callback injection for tool lookup and process execution.
- Moved `writeAff4ApfsV1DiagnosticRerunPlan()` into `src/parsers/apfs_diagnostic_exporter.cpp/.h`.
- Added APFS path/leaf helper API scaffolding in `ApfsVolumeReader` for future comparator work, but did not wire it into live forensic output.
- Preserved V1.0.31 `EvidenceIntake` helper module, iOS CSV fallback PRAGMAs, and GUI read/export LIKE PRAGMA behavior.
- Updated roadmap/checklist/suggestions tracker and documented the broader `repeat` workflow.

Not changed:

- APFS live traversal replacement.
- AFF4 read semantics.
- APFS copy-out/staging decisions.
- Store-V2 parser semantics.
- SQLite schema.
- GUI platform separation.
- NSKeyedArchiver emitted interpretation.
- Full GUI global-state encapsulation.

## Standard paths

- Source ZIPs: `D:\Downloads`
- Extract source under: `T:\`
- Current source extraction: `T:\VestigantSpotlightInv_V1_1_0_1`
- Case outputs: `Q:\SpotlightCase`
- macOS AFF4/APFS source: `O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`
- External Store-V2 reference: `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`

## Commands

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_1_0_1.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_0_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_0_1.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_0_1\scripts\Build-V1_1_0_1.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_0_1\scripts\Run-V1_1_0_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

## Next recommended validation

1. Windows/MSVC build for V1.1.0.1.
2. macOS AFF4/APFS thin run for V1.1.0.1.
3. Current iOS run after V1.1.0.1 because prior V1.0.31 moved iOS intake helpers and V1.1.0.1 preserved that boundary while changing database lifetime.

## Next recommended work after V1.1.0.1 validates

1. Compare V1.1.0.1 AFF4/APFS thin output to V1.0.31 baseline.
2. Continue reducing `writeAff4CppLiteDynamicLoadProbe` only through smaller callable helpers or a dedicated worker with parity output.
3. Add passive APFS path-resolution comparator CSVs before using path reconstruction in live staging.
4. Build a real bounded bplist object model before emitting NSKeyedArchiver unflattened investigator fields.
5. Defer full Win32 GUI global-state encapsulation until export/thread paths have been validated.
