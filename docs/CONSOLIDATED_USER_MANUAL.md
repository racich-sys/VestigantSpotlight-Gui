# Vestigant Spotlight User Manual

Version: 0.9.33

## Purpose

Vestigant Spotlight is a forensic review application for macOS Spotlight Store-V2 and iOS CoreSpotlight data. The primary evidence source is Spotlight/CoreSpotlight itself. File-system inventory, app databases, WhatsApp/SMS/CallHistory, and active-data comparisons are supporting context only.

Normal iOS runs are intentionally compact. They parse all available native records by default, but they do not persist every native/dbStr/property value into the case database. Full native/property materialization is a diagnostic/support mode because earlier versions produced very large SQLite databases and export stalls.

## Standard workflow

1. Extract the release ZIP under `T:\`.
2. Run `build_windows_msvc.bat`.
3. Confirm the CLI version and self-test.
4. Run the reuse-cache iOS script for the large test source or use the GUI.
5. Review `CASE_REVIEW_SUMMARY.txt`, `exports/parser_limits_and_suppression_summary.csv`, and the iOS Investigation views before drawing conclusions.
6. Upload the build log and thin upload bundle for review.

## Build from PowerShell

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V0_9_33.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_9_33" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V0_9_33.zip -DestinationPath T:\ -Force
& "T:\VestigantSpotlightInv_V0_9_33\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath "D:\Downloads\V0_9_33_build.log"
& "T:\VestigantSpotlightInv_V0_9_33\build-msvc\Release\VestigantSpotlightCli.exe" --version
& "T:\VestigantSpotlightInv_V0_9_33\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V0_9_33\build-msvc\selftest_out"
```

## iOS CoreSpotlight reuse-cache run

```powershell
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_33\scripts\Run-V0_9_33-iOS-ReuseCache-CLI-AndZip.ps1" `
  -InputZip "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip" `
  -ReuseCache "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4" `
  -CaseRoot "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_33_ReusedCache" `
  -OutZip "D:\Downloads\Upload_Thin_iOS_GUI_V0_9_33_ReusedCache_Check.zip"
```

The reuse-cache script passes `--skip-container-hash` by default to avoid rereading very large ZIP sources for development iterations. For final forensic reporting, run without skip-container-hash or run an explicit container/source hash workflow and preserve the hash with the case.

## Important iOS review views

Start with these views in the iOS Investigation tab:

- `iOS - Spotlight Communication Summary`: counts by communication class, bundle, domain, and content type.
- `iOS - Spotlight Message Body Review`: record-level message, mail, call, and chat context. V0.9.33 extracts message/mail body, subject, snippet, and thread/contact text from compact same-record Spotlight context.
- `iOS - User-Focus Message Body Review`: filters out conversation placeholders and likely noise while preserving the full review view elsewhere.
- `iOS - Message Contact/Thread Summary`: groups message/domain handle, suggested contact, and thread/title context.
- `iOS - Spotlight Message Media Review`: media/photo/video records saved from or associated with messages.
- `iOS - Missing From FFS Summary` and `iOS - High-Value Missing From FFS`: Spotlight references that may no longer resolve in the file-system lookup.
- `iOS - Normalized Spotlight Timeline`: timeline rows built from Spotlight dates and message/media context.
- `iOS - Timeline Anomaly Summary`: basic review flags such as dates after parse-run time or unusually old dates.
- `iOS - Parser Diagnostics Summary`: counts and samples of native parser failures, partial decode errors, and decode attempts.
- `iOS - Parser Diagnostics Detail Sample`: sampled unparsed/unsupported/native failure detail for parser gap review.
- `iOS - Case Provenance Summary`: parser version, case metadata, evidence-source path/kind, and hash/deferred-hash context.

## Interpreting compact counts

Even-looking row counts do not automatically mean record parsing was capped. Check `exports/parser_limits_and_suppression_summary.csv`.

Normal iOS mode usually means:

- native record parsing is unlimited unless `--max-native-records` was used;
- `raw_key_values` is compact and not a full property dump;
- date candidates are compact and avoid cross-product expansion;
- full FFS inventory is not materialized unless `--materialize-ios-ffs-inventory` is used;
- full app database parsed records are not materialized unless `--materialize-ios-app-db-records` or a support/diagnostic profile is used;
- thin upload files are samples and not always full case exports.

## Parser diagnostics / unparsed data

Non-zero parser diagnostics are not automatically fatal. They identify unsupported, corrupt, or partially decoded regions/values that should remain visible to the examiner. Review both the summary and detail sample before claiming the parser saw everything available in the source.

## Noise reduction

Noise-reduction summaries are non-destructive. They help separate likely marketing/short-code rows and conversation placeholders from user-review candidates. They do not delete or hide the full evidence from full views.

## macOS Spotlight / AFF4/APFS status

macOS Store-V2, AFF4/APFS staging, and external compare remain supported areas but are lower priority than current iOS CoreSpotlight investigative views. The core macOS evidence goals remain downloads/WhereFroms, LastUsed/app usage, path reconstruction, and Spotlight-to-file comparison when AFF4/APFS file-system enumeration is available.

## Support and diagnostic modes

Use support/diagnostic modes only when a focused parser or review question requires additional data. Examples include full native property persistence, full FFS inventory materialization, broad app database parsed-record export, and heavy object/date/reference review exports. These modes may create large SQLite databases and CSV outputs.

## Troubleshooting

If the run stalls or stops writing, use the matching `Collect-V0_9_33-DBBloat-State.ps1` script to collect status, logs, DB/WAL sizes, and recent outputs before stopping or rerunning. If Windows Defender is the only process reading the original large ZIP, collect state first and then consider controlled exclusions on the test system only.

If MSVC reports `C2026: string too big`, the likely source is an oversized SQL raw-string literal. The current validation check should keep large SQL fragments below conservative MSVC-safe sizes, but any new large SQL block should be split or moved into the database schema layer.

### V0_9_33 iOS investigator start path

For iOS CoreSpotlight cases, start with `iOS - Investigator Overview`. It lists the recommended review surfaces and current row counts. For communications, use `iOS - Direct User Message Thread Summary` first to identify high-volume or notable contacts/threads, then open `iOS - Direct User Message Review` for row-level Spotlight message text and validation locators. Use `iOS - Timeline Month Summary` to identify date ranges before opening row-level timeline samples.

These views are Spotlight-first. The rows are evidence from the Spotlight/CoreSpotlight index and should be corroborated with SMS.db, app databases, or FFS lookup when those support sources are available. Normal iOS mode remains compact: it does not persist every raw native property or materialize full FFS/app DB tables by default.

## V0_9_33 roadmap and testing-source guidance

Use `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md` for the current development roadmap. It explains when to continue using the reuse-cache workflow, when to switch to a fresh full iOS FFS ZIP workflow, when to run selective support/correlation materialization, and how the macOS AFF4/APFS work should resume after the iOS investigator views are stable.

For normal investigator review, start with:

1. `iOS - Investigator Overview`
2. `iOS - Case Quality Dashboard`
3. `iOS - Direct User Message Review`
4. `iOS - Direct User Message Thread Summary`
5. `iOS - High-Value Missing From FFS`
6. `iOS - Normalized Spotlight Timeline`
7. `iOS - Parser Diagnostics Action Summary`

Reuse-cache runs remain appropriate for fast parser/view iteration. After several successful reuse-cache runs, run a fresh full ZIP workflow to verify staging and slim FFS lookup creation directly from the source ZIP.
