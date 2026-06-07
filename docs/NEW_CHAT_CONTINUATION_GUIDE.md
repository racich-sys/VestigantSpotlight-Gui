# New Chat Continuation Guide — Vestigant Spotlight / Spotlight2

This package is intended to be self-contained for continuing the project in a new ChatGPT project chat.

## Current active source baseline

- Newest source package: `VestigantSpotlightInv_V1_2_0.zip`
- Current version: `1.2.0`
- Base used for changes: V1.1.10.1.
- Scope: documentation/package hygiene; consolidated notes and validation logs/notes; support/diagnostic tools retained and tracked.
- No AFF4/APFS extraction, iOS parsing, GUI behavior, Store-V2 parser behavior, or SQLite schema behavior was intentionally changed.

## Required uploads for a new chat

1. Current source ZIP: `VestigantSpotlightInv_V1_2_0.zip`.
2. Current Windows/MSVC build log after running: `V1_2_0_build.log`.
3. Current macOS AFF4/APFS thin output after running: `Upload_Thin_MacOS_AFF4_V1_2_0.zip`.
4. `BaselineVersionHistory.md` only if a newer append-only baseline exists outside this package.
5. Optional iOS CoreSpotlight thin output if iOS work is being touched.
6. External compare files if not already inside the thin ZIP.

## First files to read in every repeat cycle

1. `docs/WORKFLOW_LEDGER.md`
2. `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`
3. `docs/ROADMAP_CHECKLIST.md`
4. `docs/CONTINUATION_HANDOFF.md`
5. `docs/BaselineVersionHistory.md`
6. `docs/CONSOLIDATED_DEVELOPMENT_NOTES.md`
7. `validation/CONSOLIDATED_VALIDATION_LOGS_AND_NOTES.md`
8. `docs/SUPPORT_DIAGNOSTIC_TOOLS_REGISTER.md`

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


## Standard local paths

- Downloaded source ZIPs: `D:\Downloads`
- Source extraction root: `T:\`
- Current source extraction: `T:\VestigantSpotlightInv_V1_2_0`
- Case output root: `Q:\SpotlightCase`
- macOS AFF4/APFS test source: `O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`
- External macOS Store-V2 reference: `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`
- Reader tools: `T:\VestigantReaderTools\aff4-cpp-lite`

## Repeat workflow

When the user says `repeat`, review uploaded build/thin artifacts first, validate the current source state, make the largest safe coordinated change set, update workflow docs and append-only history, package the full source and patch artifacts, and state test scope for AFF4/APFS and iOS.

## TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build.
- iOS: not required.
- Reason: V1.2.0 changes Win32 GUI review-grid rendering behavior and documentation only. The V1.1.11 build and AFF4/APFS thin output were reviewed before this version; no extraction/traversal/copy-out/decompression/parser code changed.
- Trigger for escalating AFF4/APFS to full test: any next change to live APFS traversal, copy-out, decompression, extent handling, path reconstruction, external compare logic, or Store-V2 staging behavior.
- Trigger for iOS testing: any next change to iOS ZIP staging, CoreSpotlight parsing, FFS lookup, app DB parsing, bplist/NSKeyedArchiver handling, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_2_0_build.log` and `Upload_Thin_MacOS_AFF4_V1_2_0.zip`.

