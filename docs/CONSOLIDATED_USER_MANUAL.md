# Vestigant Spotlight Consolidated User Manual

Version: 0.9.46

## Purpose

Vestigant Spotlight parses and reviews macOS Spotlight Store-V2 and iOS CoreSpotlight data as forensic evidence.  The primary evidence source is the Spotlight/CoreSpotlight index itself.  File-system inventory, app databases, WhatsApp/SMS/CallHistory, and active-data comparisons are supporting/corroborating context, not the main parser goal.

Normal iOS investigator mode is intentionally compact.  It parses native CoreSpotlight records by default, but it does not persist every native/dbStr/property value into the case database.  Full native/property materialization, full FFS inventory, and broad app database parsed-record materialization are diagnostic/support modes because earlier V0_9 runs produced very large SQLite DB/WAL files and export stalls.

## Current documentation model

The production package now keeps historical information in a small maintained documentation set instead of many separate version fragments:

- `docs/CONSOLIDATED_USER_MANUAL.md` - this manual.
- `docs/CONSOLIDATED_VERSION_HISTORY.md` - chronological development history and process narrative.
- `docs/PROJECT_ROADMAP_AND_CONTINUATION.md` - active roadmap and continuation notes.
- `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md` - testing-source transition and AFF4/APFS roadmap.
- `docs/THIN_UPLOAD_REVIEW_WORKFLOW.md` - repeatable review workflow for build logs/thin uploads.
- `HELP.md`, `RELEASE_NOTES.md`, and `VERSION_HISTORY.md` - top-level entry points that point back to the consolidated documents.

V0_9_37 restored historical version details from the uploaded V0_9_3 documentation archive into the consolidated version history without reintroducing dozens of stale root-level note files.

## Standard Windows build

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V0_9_46.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_9_46" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V0_9_46.zip -DestinationPath T:\ -Force
& "T:\VestigantSpotlightInv_V0_9_46\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath "D:\Downloads\V0_9_46_build.log"
& "T:\VestigantSpotlightInv_V0_9_46\build-msvc\Release\VestigantSpotlightCli.exe" --version
& "T:\VestigantSpotlightInv_V0_9_46\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V0_9_46\build-msvc\selftest_out"
```

## Standard iOS reuse-cache test

Use this for fast parser/view/export iteration while the large source ZIP and known-good cache remain unchanged.

```powershell
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_46\scripts\Run-V0_9_46-iOS-ReuseCache-CLI-AndZip.ps1" `
  -InputZip "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip" `
  -ReuseCache "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4" `
  -CaseRoot "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_46_ReusedCache" `
  -OutZip "D:\Downloads\Upload_Thin_iOS_GUI_V0_9_46_ReusedCache_Check.zip"
```

The reuse-cache script passes `--skip-container-hash` by default to avoid rereading very large ZIP sources during development iterations.  For final forensic reporting, run an explicit container/source hash workflow and preserve the hash with the case.

## iOS investigator start path

Start with these GUI views in the iOS Investigation tab:

1. `iOS - Investigator Overview` - start-here overview and recommended review order.
2. `iOS - Case Quality Dashboard` - parser diagnostics, provenance, missing-FFS and compact-mode signals.
3. `iOS - Direct User Message Thread Summary` - identify high-volume or notable thread/contact buckets.
4. `iOS - Direct User Message Review` - row-level direct Apple Messages/SMS/RCS/iMessage evidence recovered from CoreSpotlight.
5. `iOS - Spotlight Message Body Review` and `iOS - User-Focus Message Body Review` - broader communication/mail/message context.
6. `iOS - Spotlight Message Media Review` - media/photo/video records saved from or associated with messages.
7. `iOS - High-Value Missing From FFS` and `iOS - Missing From FFS Summary` - indexed references that may no longer resolve in the FFS lookup.
8. `iOS - Normalized Spotlight Timeline` and `iOS - Timeline Month Summary` - chronological triage with date provenance cautions.
9. `iOS - Parser Diagnostics Action Summary` and `iOS - Parser Diagnostics Detail Sample` - unsupported, corrupt, or partially decoded native parser data.
10. `iOS - Case Provenance Summary` - parser version, case metadata, evidence-source path/kind, and hash/deferred-hash context.

## Interpreting compact normal iOS mode

Even-looking counts do not necessarily mean record parsing was capped.  Check `exports/parser_limits_and_suppression_summary.csv` and `CASE_REVIEW_SUMMARY.txt`.

Normal iOS mode usually means:

- native record parsing is unlimited unless `--max-native-records` was used;
- `raw_key_values` is compact and not a full property dump;
- date candidates are compact and avoid cross-product expansion;
- full FFS inventory is not materialized unless `--materialize-ios-ffs-inventory` is used;
- full app database parsed records are not materialized unless `--materialize-ios-app-db-records` or a support/diagnostic profile is used;
- thin-upload files are samples and not always full case exports.

## Parser diagnostics and unparsed data

Parser diagnostics are evidence of parser behavior and source condition.  Non-zero diagnostics are not automatically fatal.  They identify unsupported, corrupt, partially decoded, or intentionally suppressed decode regions/values that should remain visible to the examiner.  Review both the summary and detail sample before claiming the parser saw every available record/value.

## Noise reduction

Noise-reduction summaries are non-destructive.  They help separate likely marketing/short-code/cache/placeholder rows from user-review candidates.  They do not delete or hide the full evidence from full views.

## macOS Spotlight / AFF4/APFS status

macOS Store-V2, AFF4/APFS staging, and external compare remain active roadmap areas but are lower priority than current iOS CoreSpotlight investigator views.  The core macOS evidence goals remain:

- downloads and WhereFroms/referrer evidence;
- LastUsed/app usage evidence;
- path reconstruction and CNID/inode parent relationships;
- Store-V2 extraction from AFF4/APFS without relying on Windows mounting;
- comparison of extracted Store-V2 folders against trusted external reference extractions where available.

Use `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md` for when to switch from reuse-cache iOS testing to fresh full FFS ZIP testing, support/correlation materialization, alternate iOS sources, and renewed AFF4/APFS testing.

## Support and diagnostic modes

Use support/diagnostic modes only when a focused parser or review question requires additional data.  Examples include full native property persistence, full FFS inventory materialization, broad app database parsed-record export, and heavy object/date/reference review exports.  These modes may create large SQLite databases and CSV outputs.

## Thin upload review

Thin upload ZIPs are for review/debugging and may contain samples of large CSVs.  Full local case exports remain in the case folder unless a view/export is explicitly sample-only.  See `docs/THIN_UPLOAD_REVIEW_WORKFLOW.md` for the review workflow to use after each build log/thin upload.

## Troubleshooting

If a run stalls or stops writing, use the matching `Collect-V0_9_46-DBBloat-State.ps1` script before stopping/rerunning:

```powershell
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_46\scripts\Collect-V0_9_46-DBBloat-State.ps1" `
  -CaseRoot "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_46_ReusedCache" `
  -OutZip "D:\Downloads\Upload_State_V0_9_37_NoWrites_Stopped_Check.zip" `
  -StopVestigant
```

If MSVC reports `C2026: string too big`, the likely source is an oversized SQL raw-string literal.  Current validation checks keep raw SQL fragments below prior risk thresholds, but new large SQL should be split or moved further into the database schema layer.

## What to upload after each test

Upload:

- `D:\Downloads\V0_9_46_build.log`
- `D:\Downloads\Upload_Thin_iOS_GUI_V0_9_46_ReusedCache_Check.zip`
- stopped-state ZIP/SHA256 only if the run stalls, DB/WAL grows unexpectedly, or no writes occur for a long period.


## V0_9_37 note

V0_9_37 carries forward the V0_9_35 consolidated documentation/history repair and fixes the V0_9_34 Missing From FFS summary export failure.

## V0_9_37 - Missing From FFS text visibility

V0_9_37 addresses the user-reported issue that some Spotlight CSV reports did not show recovered Spotlight text/content.  It adds row-level Missing From FFS text detail and text coverage exports, exposes the same views in the GUI, increases compact same-record text context retention for reference-bearing iOS records, and documents when text is unavailable or suppressed by compact mode.



## V0_9_42 - Missing From FFS text visibility guardrail fix

V0_9_37 improved Missing From FFS text visibility but over-expanded same-record text context and hit the SQLite 5 GiB guardrail during native parse.  V0_9_42 keeps the text-detail views/exports but restores a bounded normal-mode text-context budget and fixes fatal guardrail propagation so runs stop cleanly if a guardrail is ever hit.

### V0_9_42 V1-readiness testing note

After V0_9_42 is validated with the standard reuse-cache script, run the new Stage B fresh-ZIP script against the same large iOS FFS ZIP. This tests actual ZIP enumeration/staging and the new non-regex 7-Zip inventory path without changing the parser's compact normal-mode assumptions. If Stage B succeeds, later cycles can selectively test support/correlation materialization under guardrails.

## V0_9_42 - Native C++ 7-Zip inventory parser

V0_9_42 reviewed the successful V0_9_41 reuse-cache run and carries forward the V1-readiness performance work. The CSV exporter fast path remains in place. The iOS focused ZIP workflow now lets 7-Zip dump `-slt` output to raw text and then rebuilds FFS/app database inventory CSVs using native C++ parsing rather than the PowerShell raw-listing parser. This is intended to make the Stage B fresh-ZIP test faster and closer to the 60-120 MB/s target where hardware permits.
