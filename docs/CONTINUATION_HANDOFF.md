# Spotlight2 / Vestigant Spotlight Continuation Handoff

## Current packaged version

- Current source package: `VestigantSpotlightInv_V1_1_2.zip`
- Version string: `1.1.2`
- Base version reviewed before this package: `V1.1.2`
- V1.1.0.1 Windows/MSVC build log was uploaded and reviewed. It compiled and linked CLI, tests, and GUI, reported `Vestigant Spotlight v1.1.0.1`, and only showed the known C4100 warning for `shellRunner` in `apfs_aff4_reader.cpp`.
- V1.1.0.1 macOS AFF4/APFS thin ZIP was uploaded and reviewed. Denied raw filenames were absent and the AFF4/APFS staged Store-V2 baseline remained stable.

## User shorthand

If a future request begins with `repeat`, treat it as: review all uploaded/copied information, identify the newest source/build/thin evidence, continue to the next version in the same project style, implement as many safe outstanding roadmap/suggestion items as possible, update the handoff/roadmap/suggestions tracker, package artifacts, and provide concrete PowerShell commands. The expected cadence for `repeat` is broader than a one-line hotfix when the current version builds: spend more time, make a larger coordinated but reviewable step, and run repeated checks before packaging.

## V1.1.2 scope

V1.1.2 is a broader repeat-cycle release focused on evidence-intake isolation and GUI ingest-thread safety. It preserves extraction semantics while moving more iOS inventory import logic out of `app_runner.cpp`.

Implemented:

- Moved `importIosInventoryCsvs(...)` from `app_runner.cpp` into `EvidenceIntake::importIosInventoryCsvs(...)`.
- Moved cache-SQLite iOS FFS/app-database inventory import helpers and referenced-path lookup import into `src/ingest/evidence_intake.cpp`.
- Added `EvidenceIntake::importReferencedIosPathLookupFromReuseCache(...)` and updated `app_runner.cpp` to call it through the intake module.
- Preserved run-status reporting through callback injection so `app_runner.cpp` keeps orchestration/progress ownership while intake owns import mechanics.
- Changed Win32 GUI main ingest/build worker from `std::thread(worker).detach()` to a tracked `gIngestThread` with guarded start and `WM_DESTROY` join.
- Cleared the V1.1.0.1 AFF4 stream inventory C4100 warning using platform-aware `(void)` annotations while preserving the callback signature.
- Updated roadmap/checklist/suggestions tracker and documented this version.

Not changed:

- APFS live traversal replacement.
- AFF4 read semantics.
- APFS copy-out/staging decisions.
- Store-V2 parser semantics.
- SQLite schema.
- GUI platform separation.
- NSKeyedArchiver emitted interpretation.
- Full GUI global-state encapsulation.
- Full `stageZipEvidenceSource(...)` relocation.


## V1.1.2 workflow-ledger note

V1.1.2 adds `docs/WORKFLOW_LEDGER.md`. Future `repeat` cycles should read that ledger before editing so prior fixes, failed attempts, packaging pitfalls, and validation gates are not rediscovered each time.

V1.1.2 implements safe cancellation plumbing, dependent DLL search hardening, GUI logo bitmap cleanup, native parser bulk PRAGMAs, and bounded bplist trailer validation metadata.

## Standard paths

- Source ZIPs: `D:\Downloads`
- Extract source under: `T:\`
- Current source extraction: `T:\VestigantSpotlightInv_V1_1_2`
- Case outputs: `Q:\SpotlightCase`
- macOS AFF4/APFS source: `O:9_0142-IT001\disk3 2024-10-01 10-43-409_0142-IT001.aff4`
- External Store-V2 reference: `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`

## Commands

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_1_2.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_2" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_2.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_2\scripts\Build-V1_1_2.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_2\scripts\Run-V1_1_2-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

## Next recommended validation

1. Windows/MSVC build for V1.1.2.
2. macOS AFF4/APFS thin run for V1.1.2.
3. Current iOS run because V1.1.2 moved the iOS inventory import boundary into `EvidenceIntake`.

## Next recommended work after V1.1.2 validates

1. Compare V1.1.2 AFF4/APFS thin output to V1.1.0.1 baseline.
2. Continue reducing `writeAff4CppLiteDynamicLoadProbe` only through smaller callable helpers or a dedicated worker with parity output.
3. Move `stageZipEvidenceSource(...)` only after the V1.1.2 intake-import relocation validates.
4. Add passive APFS path-resolution comparator CSVs before using path reconstruction in live staging.
5. Build a real bounded bplist object model before emitting NSKeyedArchiver unflattened investigator fields.
6. Defer full Win32 GUI global-state encapsulation until current worker paths have been validated.
