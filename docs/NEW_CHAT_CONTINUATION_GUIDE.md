# New Chat Continuation Guide — Vestigant Spotlight / Spotlight2

This source package is intended to be self-contained for continuing the project in a new ChatGPT project chat.

## Current active source baseline

- Newest source package: `VestigantSpotlightInv_V1_1_7_1.zip`
- Current version: `1.1.7.1`
- Most recently validated baseline before this package: `V1.1.6.1` with Windows/MSVC build and macOS AFF4/APFS thin output.
- V1.1.7 moved both large AFF4/APFS probe bodies out of `app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp`.
- V1.1.7.1 fixes the Windows/MSVC helper-boundary failure from that relocation and cleans source package layout.

## Standard local paths

- Downloaded source ZIPs: `D:\Downloads`
- Source extraction root: `T:\`
- Current source extraction: `T:\VestigantSpotlightInv_V1_1_7_1`
- Case output root: `Q:\SpotlightCase`
- macOS AFF4/APFS test source:
  `O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`
- External macOS Store-V2 reference:
  `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`
- Reader tools:
  `T:\VestigantReaderTools\aff4-cpp-lite`

## Required external files for full regression

Upload or make available when continuing in a new chat:

1. Current source ZIP: `VestigantSpotlightInv_V1_1_7_1.zip`.
2. Current Windows/MSVC build log after running: `V1_1_7_1_build.log`.
3. Current macOS AFF4/APFS thin output after running: `Upload_Thin_MacOS_AFF4_V1_1_7_1.zip`.
4. Optional iOS CoreSpotlight thin output from the latest iOS regression, if available.
5. External compare files if not already inside the thin ZIP:
   - `aff4_apfs_external_spotlight_compare_summary.json`
   - `aff4_apfs_external_spotlight_file_compare.csv`
   - `aff4_apfs_remaining_mismatch_diagnostics.csv/json`

## First files to read in every repeat cycle

1. `docs/WORKFLOW_LEDGER.md`
2. `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`
3. `docs/ROADMAP_CHECKLIST.md`
4. `docs/CONTINUATION_HANDOFF.md`
5. `docs/FULL_VERSION_HISTORY.md`
6. Latest `validation/V*_validation_notes.md`

## Repeat workflow

When the user says `repeat`:

1. Review uploaded build log and thin ZIP first.
2. Confirm whether the newest uploaded/generated version built and whether thin output exists.
3. Read the workflow ledger and suggestions tracker before editing.
4. Implement as many safe, effective roadmap/suggestion items as possible.
5. Avoid placeholder forensic output. Do not wire unverified APFS path reconstruction or NSKeyedArchiver interpretation into live evidence views.
6. Run at least two local review/syntax/build validation passes where feasible.
7. Package full source ZIP, patch ZIP, unified diff, SHA256 files, patch manifest, validation notes, and concrete PowerShell commands.
8. Update append-only history and workflow docs.

## Build command

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_1_7_1.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_7_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_7_1.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Build-V1_1_7_1.ps1
```

## macOS AFF4/APFS thin regression command

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Run-V1_1_7_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

Expected output:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_1_7_1.zip
```

## Current high-priority remaining work

- Validate V1.1.7.1 under Windows/MSVC.
- Run macOS AFF4/APFS thin regression and compare to V1.1.6.1 baseline.
- After build/thin parity, split internals of `Aff4ProbeWorker` into smaller private helpers to reduce lambda capture scope.
- Continue comparator-first APFS horizontal leaf traversal and absolute path reconstruction work.
- Continue iOS bplist/NSKeyedArchiver decoding only when field/object associations can be validated.
