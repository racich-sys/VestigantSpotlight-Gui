# Spotlight2 / Vestigant Spotlight Continuation Handoff

## Current packaged version

- Current source package: `VestigantSpotlightInv_V1_0_31.zip`
- Version string: `1.0.31`
- Base version reviewed before this package: `V1.0.30`
- V1.0.30 Windows/MSVC build log was uploaded and reviewed. It compiled and linked CLI, tests, and GUI, and reported `Vestigant Spotlight v1.0.30`.
- V1.0.30 macOS AFF4/APFS thin ZIP was uploaded and reviewed. Denied raw filenames were absent and the AFF4/APFS staged Store-V2 baseline remained stable.

## User shorthand

If a future request begins with `repeat`, treat it as: review all uploaded/copied information, identify the newest source/build/thin evidence, continue to the next version in the same project style, implement as many safe outstanding roadmap/suggestion items as possible, update the handoff/roadmap/suggestions tracker, package artifacts, and provide concrete PowerShell commands.

## V1.0.31 scope

V1.0.31 is a bounded intake/performance modularization release.

Implemented:

- Added `src/ingest/evidence_intake.h/.cpp`.
- Moved behavior-preserving evidence-intake helper logic out of `app_runner.cpp`:
  - CSV data-row counting.
  - iOS ZIP path normalization.
  - iOS app database staging path sanitization.
  - iOS database category/app/domain/protection/container hint helpers.
- Added temporary SQLite PRAGMAs around regenerable iOS CSV fallback ingestion and restored WAL/NORMAL afterward.
- Added `PRAGMA case_sensitive_like=OFF` for GUI review/export read connections while preserving existing broad search semantics.
- Updated roadmap/checklist/suggestions tracker and documented the `repeat` shorthand.

Not changed:

- APFS traversal.
- AFF4 read semantics.
- APFS copy-out/staging.
- Store-V2 parser.
- SQLite schema.
- GUI platform separation.
- APFS reverse path resolver.
- NSKeyedArchiver parser.
- Full evidence intake orchestration movement.
- Database lifetime model in `runApplication`.

## Standard paths

- Source ZIPs: `D:\Downloads`
- Extract source under: `T:\`
- Current source extraction: `T:\VestigantSpotlightInv_V1_0_31`
- Case outputs: `Q:\SpotlightCase`
- macOS AFF4/APFS source: `O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`
- External Store-V2 reference: `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`

## Commands

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_0_31.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_0_31" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_0_31.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_31\scripts\Build-V1_0_31.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_31\scripts\Run-V1_0_31-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

## Next recommended work

After V1.0.31 builds and the thin ZIP validates, the next safest substantive work is one of:

1. Validate V1.0.31 on an iOS run because the CSV fallback import settings and intake helper module affect iOS paths.
2. Move any remaining report-only APFS helpers out of `app_runner.cpp`.
3. Add iOS bplist/NSKeyedArchiver parser primitives only after a real bounded object model exists.
4. Add passive APFS path-resolution comparator outputs only after catalog parent/name lookup is validated.

Do not combine APFS traversal replacement, NSKeyedArchiver unflattening, GUI global-state encapsulation, and database lifetime rewrites in one version.
