# Spotlight2 / Vestigant Spotlight Continuation Handoff

## Current packaged version

- Current source package: `VestigantSpotlightInv_V1_0_27.zip`
- Version string: `1.0.27`
- Base version reviewed before this package: `1.0.26.1`
- V1.0.26.1 Windows/MSVC build status: passed and reported `Vestigant Spotlight v1.0.26.1`.
- V1.0.26.1 macOS AFF4/APFS thin status: thin ZIP generated successfully and reviewed.

## V1.0.26.1 validation notes

- Denied raw thin-upload filenames were not present in the uploaded thin ZIP.
- Case and additional-output inventory files used relative paths and did not expose full local `Q:\`, `D:\`, or `T:\` paths.
- AFF4/APFS staged Store-V2 parser counts remained stable: `raw_records=25000`, `raw_key_values=2181`, `raw_date_candidates=25000`.
- External comparison remained broadly consistent with prior baseline: `external_file_count=4123`, `vestigant_file_count=8986`, `file_match_rows=2213`, `external_only_rows=1424`, `vestigant_only_rows=6710`.

## V1.0.27 changes

- Added Win32 Job Object wrapping to hidden process launches in `app_runner.cpp` so child processes are killed with the parent on timeout/cleanup when assignment succeeds.
- Added bounded SQLite busy retry handlers to GUI review and GUI export-worker read connections.
- Added generated ZIP deny-list self-check to `tools/Create-SourceProbeUploadZip.ps1`.
- Updated roadmap/checklist/suggestions tracking files.

## Standard paths

- Downloaded source ZIPs: `D:\Downloads`
- Extracted source root: `T:\`
- Current extraction target: `T:\VestigantSpotlightInv_V1_0_27`
- macOS AFF4/APFS source: `O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`
- External macOS Store-V2 reference: `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`
- V1.0.27 case root: `Q:\SpotlightCase\TestMacOS_AFF4_V1_0_27`
- V1.0.27 thin ZIP output: `D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_27.zip`

## Build and run commands

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_0_27.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_0_27" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_0_27.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_27\scripts\Build-V1_0_27.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_27\scripts\Run-V1_0_27-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

## Next recommended work

After V1.0.27 builds and the thin ZIP validates, the next safest substantive version is moving the first APFS diagnostic writer family into `apfs_diagnostic_exporter.cpp`. Do not combine APFS traversal rewrites, iOS bplist unflattening, and GUI state refactoring in one version.
