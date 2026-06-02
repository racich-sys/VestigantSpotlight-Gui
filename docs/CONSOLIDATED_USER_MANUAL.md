# Vestigant Spotlight Consolidated User Manual

Version: 0.9.34

## Purpose

Vestigant Spotlight parses and reviews macOS Spotlight Store-V2 and iOS CoreSpotlight data as forensic evidence.  The primary focus is the Spotlight/CoreSpotlight index itself.  FFS inventory, app databases, and active-file comparison are supporting/corroborating context, not the main parser goal.

## Standard Windows build

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V0_9_34.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_9_34" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V0_9_34.zip -DestinationPath T:\ -Force
& "T:\VestigantSpotlightInv_V0_9_34\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath "D:\Downloads\V0_9_34_build.log"
& "T:\VestigantSpotlightInv_V0_9_34\build-msvc\Release\VestigantSpotlightCli.exe" --version
& "T:\VestigantSpotlightInv_V0_9_34\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V0_9_34\build-msvc\selftest_out"
```

## Standard iOS reuse-cache test

```powershell
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_34\scripts\Run-V0_9_34-iOS-ReuseCache-CLI-AndZip.ps1" `
  -InputZip "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip" `
  -ReuseCache "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4" `
  -CaseRoot "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_34_ReusedCache" `
  -OutZip "D:\Downloads\Upload_Thin_iOS_GUI_V0_9_34_ReusedCache_Check.zip"
```

## iOS investigator start path

1. `iOS - Investigator Overview` — compact start page showing the recommended review order.
2. `iOS - Case Quality Dashboard` — run quality, parser diagnostics, missing-FFS and compact-mode signals.
3. `iOS - Direct User Message Review` — direct Apple Messages/SMS/RCS/iMessage entries recovered from CoreSpotlight.
4. `iOS - Direct User Message Thread Summary` — identify high-volume/notable thread or contact buckets.
5. `iOS - Spotlight Message Body Review` and `iOS - User-Focus Message Body Review` — broader communication/mail/message context.
6. `iOS - Normalized Spotlight Timeline` / `iOS - Timeline Month Summary` — chronology triage with date provenance cautions.
7. `iOS - Parser Diagnostics Action Summary` — visible unsupported/corrupt/unparsed decode buckets.

## Interpreting compact normal iOS mode

Normal iOS investigator mode deliberately avoids full one-row-per-property persistence to prevent DB/WAL explosion.  `raw_key_values` and `raw_date_candidates` are compact investigative/provenance tables, not complete raw property dumps.  Use `exports/parser_limits_and_suppression_summary.csv` and `CASE_REVIEW_SUMMARY.txt` to confirm limits and suppressions for each run.

## Thin upload review

Thin upload ZIPs are intended for review/debugging and may contain samples of large CSVs.  Full local case exports remain in the case folder unless a view/export is explicitly sample-only.  See `docs/THIN_UPLOAD_REVIEW_WORKFLOW.md`.

## Current documentation set

- `HELP.md`: short start page.
- `docs/CONSOLIDATED_USER_MANUAL.md`: this manual.
- `docs/CONSOLIDATED_VERSION_HISTORY.md`: consolidated release history.
- `docs/PROJECT_ROADMAP_AND_CONTINUATION.md`: active roadmap and continuation notes.
- `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md`: testing-source transition and AFF4/APFS roadmap.
- `docs/PACKAGE_CLEANUP_SUMMARY.md`: production-package cleanup notes.
