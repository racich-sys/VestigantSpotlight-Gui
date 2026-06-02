# Vestigant Spotlight Help

Current version: 0.9.36

## Start here

Use `docs/CONSOLIDATED_USER_MANUAL.md` as the primary manual.  Use `docs/CONSOLIDATED_VERSION_HISTORY.md` for the full restored version history and development-process narrative.

## What changed in V0_9_36

V0_9_36 restores the older V0_9 development history into one consolidated version-history document.  The V0_9_34 cleanup removed stale separate fragments from the production ZIP, but it also left the consolidated history too shallow.  V0_9_36 keeps the package clean while preserving the historical substance in a single maintained file.

No parser, schema, GUI, export, or forensic interpretation behavior was intentionally changed from V0_9_34.

## Build

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V0_9_36.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_9_36" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V0_9_36.zip -DestinationPath T:\ -Force
& "T:\VestigantSpotlightInv_V0_9_36\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath "D:\Downloads\V0_9_36_build.log"
& "T:\VestigantSpotlightInv_V0_9_36\build-msvc\Release\VestigantSpotlightCli.exe" --version
& "T:\VestigantSpotlightInv_V0_9_36\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V0_9_36\build-msvc\selftest_out"
```

## iOS reuse-cache test

```powershell
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_36\scripts\Run-V0_9_36-iOS-ReuseCache-CLI-AndZip.ps1" `
  -InputZip "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip" `
  -ReuseCache "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4" `
  -CaseRoot "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_36_ReusedCache" `
  -OutZip "D:\Downloads\Upload_Thin_iOS_GUI_V0_9_36_ReusedCache_Check.zip"
```

## Key documents

- `docs/CONSOLIDATED_USER_MANUAL.md`
- `docs/CONSOLIDATED_VERSION_HISTORY.md`
- `docs/PROJECT_ROADMAP_AND_CONTINUATION.md`
- `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md`
- `docs/THIN_UPLOAD_REVIEW_WORKFLOW.md`
