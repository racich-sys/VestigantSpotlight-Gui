# Spotlight2 / Vestigant Spotlight Continuation Handoff

## Current packaged version

- Current source package: `VestigantSpotlightInv_V1_0_26_1.zip`
- Version string: `1.0.26.1`
- Base version reviewed before this package: `1.0.26`
- V1.0.26 Windows/MSVC build status: passed and reported `Vestigant Spotlight v1.0.26`.
- V1.0.26 macOS AFF4/APFS run status: AFF4 probe and external comparison completed, but thin ZIP packaging failed in `Create-SourceProbeUploadZip.ps1` during relative-path inventory generation.

## Immediate V1.0.26.1 purpose

V1.0.26.1 is a packaging hotfix plus continuity-documentation release. It does not change APFS traversal, AFF4 reads, Store-V2 parsing, iOS parsing, database schema, or GUI behavior.

## Failure being fixed

The V1.0.26 macOS AFF4 wrapper reached external comparison and then failed with:

```text
Get-RelativePathForThinInventory : Cannot convert value "\\" to type "System.Char".
```

Cause: `tools/Create-SourceProbeUploadZip.ps1` used `[char]'\\'`, which is a two-character string in Windows PowerShell. The error occurred while creating `additional_output_file_inventory.txt`, after the expensive probe work had already completed.

## V1.0.26.1 changes

- Replaced fragile PowerShell `[char]'\\'` relative-path trimming with a Windows PowerShell 5.1-compatible `System.Uri.MakeRelativeUri` implementation.
- Reused the same relative-path helper for `ExtractedSpotlight` copy paths.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full absolute local paths.
- Added `scripts/Package-V1_0_26_1-macOS-AFF4-ThinFromExistingCase.ps1` so a completed V1.0.26 case can be packaged without rerunning the 90-minute AFF4/APFS probe.
- Added this handoff file, a roadmap checklist, and a suggestions/fixes tracker for future chat continuity.

## Standard paths

- Downloaded source ZIPs: `D:\Downloads`
- Extracted source root: `T:\`
- Current extraction target: `T:\VestigantSpotlightInv_V1_0_26_1`
- macOS AFF4/APFS source: `O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`
- External macOS Store-V2 reference: `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`
- Existing V1.0.26 case root from the failed packaging run: `Q:\SpotlightCase\TestMacOS_AFF4_V1_0_26`
- Expected V1.0.26 external comparison output root: `D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_26_ExternalCompare`
- Packaging-only V1.0.26.1 thin ZIP output: `D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_26_1.zip`

## Fastest next command after extracting V1.0.26.1

Use this if the V1.0.26 AFF4/APFS run completed and only thin packaging failed:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_26_1\scripts\Package-V1_0_26_1-macOS-AFF4-ThinFromExistingCase.ps1
```

This should package the existing `Q:\SpotlightCase\TestMacOS_AFF4_V1_0_26` output and existing external-compare directory into `D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_26_1.zip` without rerunning the AFF4 probe.

## Full validation command sequence

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_0_26_1.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_0_26_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_0_26_1.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_26_1\scripts\Build-V1_0_26_1.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_26_1\scripts\Package-V1_0_26_1-macOS-AFF4-ThinFromExistingCase.ps1
```

## Next likely version after V1.0.26.1

If V1.0.26.1 builds and produces the thin ZIP, the next narrow substantive version should be one of:

1. Move the first APFS diagnostic writer family from `app_runner.cpp` into `apfs_diagnostic_exporter.cpp`; or
2. Continue thin-upload security cleanup by validating the final ZIP contents against the denied raw inventory/log names; or
3. Add a thin-upload self-check that fails if denied filenames are present in the generated ZIP.

Do not combine APFS traversal rewrites, DB lifetime rewrites, and GUI global-state refactors in one version.
