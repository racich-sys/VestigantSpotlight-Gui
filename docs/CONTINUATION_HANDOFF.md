# Spotlight2 / Vestigant Spotlight Continuation Handoff

## Current packaged version

- Current source package: `VestigantSpotlightInv_V1_0_28_2.zip`
- Version string: `1.0.28.2`
- Base version reviewed before this package: `1.0.28.1`
- V1.0.28.1 Windows/MSVC build status: failed during link with `LNK2005` because `isLikelyStoreV2GroupDirectoryName` existed in both `app_runner.obj` and `apfs_diagnostic_exporter.obj`.
- V1.0.28.2 scope: linker hotfix only.

## V1.0.28.2 changes

- Scoped the APFS diagnostic exporter copy of `isLikelyStoreV2GroupDirectoryName()` to the exporter translation unit.
- Preserved the runner helper used by dynamic AFF4/APFS probe code.
- Kept the V1.0.28 APFS diagnostic writer relocation intact.
- Did not change APFS traversal, AFF4 reads, copy-out/staging, Store-V2 parsing, iOS parsing, schema, GUI behavior, or forensic interpretation.
- Added `docs/V1_0_28_2_BUILD_HOTFIX.md`.
- Updated this handoff file, roadmap checklist, and suggestions/fixes tracker.

## Standard paths

- Downloaded source ZIPs: `D:\Downloads`
- Extracted source root: `T:\`
- Current extraction target: `T:\VestigantSpotlightInv_V1_0_28_2`
- macOS AFF4/APFS source: `O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`
- External macOS Store-V2 reference: `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`
- V1.0.28.2 case root: `Q:\SpotlightCase\TestMacOS_AFF4_V1_0_28_2`
- V1.0.28.2 thin ZIP output: `D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_28_2.zip`

## Build and run commands

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_0_28_2.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_0_28_2" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_0_28_2.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_28_2\scripts\Build-V1_0_28_2.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_28_2\scripts\Run-V1_0_28_2-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

## Next recommended work

After V1.0.28.2 builds and the thin ZIP validates, the next safest work is either:

1. move the remaining APFS exact-signature/rerun report writers out of `app_runner.cpp`; or
2. create an `EvidenceIntake` skeleton/module boundary without changing runtime behavior.

Do not combine APFS traversal rewrites, iOS NSKeyedArchiver unflattening, GUI state encapsulation, and database lifetime rewrites in one version.
