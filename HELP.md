## V1.2.0 update

- Scope: documentation/package hygiene release.
- Consolidated standalone development notes into `docs/CONSOLIDATED_DEVELOPMENT_NOTES.md`.
- Consolidated standalone validation logs/notes into `validation/CONSOLIDATED_VALIDATION_LOGS_AND_NOTES.md`.
- Removed the now-consolidated standalone note/log files from the active package.
- Added `docs/SUPPORT_DIAGNOSTIC_TOOLS_REGISTER.md` to track retained support/diagnostic tools and their retention rationale.
- No support/diagnostic tools were deleted in this version because each remains tied to active AFF4/APFS validation, iOS support, general packaging/staging, or on-demand troubleshooting.
- No AFF4/APFS extraction, iOS parsing, GUI behavior, Store-V2 parser behavior, or SQLite schema behavior was intentionally changed.

- Reviewed uploaded `V1_1_10_1_build.log`: Windows/MSVC build completed successfully, CLI/tests/GUI linked, and `Vestigant Spotlight v1.1.10.1` was reported.
- Reviewed uploaded `Upload_Thin_MacOS_AFF4_V1_1_10_1.zip`: AFF4/APFS run completed source-probe workflow; staged Store-V2 parse/enrichment produced 25,000 artifacts.
- External compare summary remained stable against the prior V1.1.9/V1.1.10.1 class: 4,123 external files, 8,986 Vestigant staged files, 2,213 file matches, 1,424 external-only rows, 6,710 Vestigant-only rows, and 486 relative-path size mismatches.
- Remaining mismatch diagnostics stayed at 486 rows: 4 `DATA_FORK_SIZE_DISAGREES_WITH_EXTERNAL` and 482 `NO_EXACT_COPYOUT_CANDIDATE`.

## TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build.
- iOS: not required.
- Reason: V1.2.0 changes Win32 GUI review-grid rendering behavior and documentation only. The V1.1.11 build and AFF4/APFS thin output were reviewed before this version; no extraction/traversal/copy-out/decompression/parser code changed.
- Trigger for escalating AFF4/APFS to full test: any next change to live APFS traversal, copy-out, decompression, extent handling, path reconstruction, external compare logic, or Store-V2 staging behavior.
- Trigger for iOS testing: any next change to iOS ZIP staging, CoreSpotlight parsing, FFS lookup, app DB parsing, bplist/NSKeyedArchiver handling, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_2_0_build.log` and `Upload_Thin_MacOS_AFF4_V1_2_0.zip`.

## Standard V1.2.0 build command block

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_2_0.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_2_0" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_2_0.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_2_0\scripts\Build-V1_2_0.ps1
```

## Standard V1.2.0 AFF4/APFS thin command

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_2_0\scripts\Run-V1_2_0-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```


# Vestigant Spotlight Help — V1.1.10.1

## Current build

- Current source version: `1.1.10.1`.
- Current source ZIP: `VestigantSpotlightInv_V1_1_10_1.zip`.
- V1.1.10.1 is a documentation/script command hotfix on V1.1.10.

## Build

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_1_10_1.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_10_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_10_1.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Build-V1_1_10_1.ps1
```

## Launch GUI

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Launch-V1_1_10_1-GUI.ps1
```

## macOS AFF4/APFS thin regression

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

See `docs/NEW_CHAT_CONTINUATION_GUIDE.md` for the full continuation package and upload requirements.
