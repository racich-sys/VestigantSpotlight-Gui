# Vestigant Spotlight Help

Current version: 0.9.43

## Start here

Use `docs/CONSOLIDATED_USER_MANUAL.md` as the primary manual.  Use `docs/CONSOLIDATED_VERSION_HISTORY.md` for the full restored version history and development-process narrative.

## What changed in V0_9_37

V0_9_37 restores the older V0_9 development history into one consolidated version-history document.  The V0_9_34 cleanup removed stale separate fragments from the production ZIP, but it also left the consolidated history too shallow.  V0_9_37 keeps the package clean while preserving the historical substance in a single maintained file.

No parser, schema, GUI, export, or forensic interpretation behavior was intentionally changed from V0_9_34.

## Build

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V0_9_43.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_9_43" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V0_9_43.zip -DestinationPath T:\ -Force
& "T:\VestigantSpotlightInv_V0_9_43\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath "D:\Downloads\V0_9_43_build.log"
& "T:\VestigantSpotlightInv_V0_9_43\build-msvc\Release\VestigantSpotlightCli.exe" --version
& "T:\VestigantSpotlightInv_V0_9_43\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V0_9_43\build-msvc\selftest_out"
```

## iOS reuse-cache test

```powershell
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_43\scripts\Run-V0_9_43-iOS-ReuseCache-CLI-AndZip.ps1" `
  -InputZip "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip" `
  -ReuseCache "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4" `
  -CaseRoot "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_43_ReusedCache" `
  -OutZip "D:\Downloads\Upload_Thin_iOS_GUI_V0_9_43_ReusedCache_Check.zip"
```

## Key documents

- `docs/CONSOLIDATED_USER_MANUAL.md`
- `docs/CONSOLIDATED_VERSION_HISTORY.md`
- `docs/PROJECT_ROADMAP_AND_CONTINUATION.md`
- `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md`
- `docs/THIN_UPLOAD_REVIEW_WORKFLOW.md`

## V0_9_37 - Missing From FFS text visibility

V0_9_37 addresses the user-reported issue that some Spotlight CSV reports did not show recovered Spotlight text/content.  It adds row-level Missing From FFS text detail and text coverage exports, exposes the same views in the GUI, increases compact same-record text context retention for reference-bearing iOS records, and documents when text is unavailable or suppressed by compact mode.



## V0_9_42 - Missing From FFS text visibility guardrail fix

V0_9_37 improved Missing From FFS text visibility but over-expanded same-record text context and hit the SQLite 5 GiB guardrail during native parse.  V0_9_42 keeps the text-detail views/exports but restores a bounded normal-mode text-context budget and fixes fatal guardrail propagation so runs stop cleanly if a guardrail is ever hit.

### V0_9_42 V1-readiness note

V0_9_42 tightens normal iOS compact-mode text storage to keep Missing From FFS text visibility without exceeding the DB guardrail on the current large iOS source. GUI review-page loads are now tracked and cancellable instead of detached.
