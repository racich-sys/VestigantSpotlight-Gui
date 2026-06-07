## V1.1.10 update

- Current generated source package: V1.1.10.
- Base used for changes: V1.1.9.1.
- Scope: source-package documentation/script cleanup and current-version wrapper regeneration only.
- Removed only clearly obsolete active-package clutter; ambiguous historical notes/scripts were retained for user approval before any future removal.
- Source-package `.md`, `.txt`, and `.ps1` review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.md`.
- No AFF4/APFS extraction, iOS parsing, GUI behavior, or SQLite schema behavior was intentionally changed.

# Vestigant Spotlight V0_9_60 Notes

## V1.1.10 update

- Current generated source package: V1.1.10.
- Validated baseline reviewed before this version: V1.1.8 Windows/MSVC build and macOS AFF4/APFS thin output.
- Main change: guarded live APFS OMAP horizontal leaf traversal with bounded next-leaf transitions.
- Source-package `.md`, `.txt`, and `.ps1` file review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.md`.


V0_9_60 is a V1 production-readiness cleanup after V0_9_57 compiled and ran on Windows. It improves the processing workflow and review workflow without changing parser interpretation logic.

Key changes:
- The Case Information bottom log is now the live processing log. It clears at run start, timestamps messages, mirrors run/progress status, and emits periodic heartbeat messages while processing continues.
- View loading now shows an explicit marquee progress indicator above the investigation grid and a loading message in the details pane so long SQLite view loads are not mistaken for hangs.
- The V1 GUI source selector now exposes only fully implemented Folder and ZIP intake paths. AFF4/APFS and raw image support remain roadmap items and are not presented as clickable V1 options.
- Legacy V7-only schema tables/indexes were removed from new case initialization.
- CLI/operator self-test mode is deprecated; the automated test executable uses an internal automated self-test path.
- Duplicate AFF4/APFS child/descendant root-tree probe output writers were consolidated into one traversal-output writer.

Validation summary:
- Linux CMake configure/build passed.
- VestigantSpotlightTests passed.
- C++20 syntax checks passed for modified non-Windows translation units.
- Windows/MSVC GUI compile and runtime validation remain required.

# Vestigant Spotlight Consolidated User Manual

Version: 0.9.60

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
Get-FileHash .\VestigantSpotlightInv_V0_9_60.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_9_60" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V0_9_60.zip -DestinationPath T:\ -Force
& "T:\VestigantSpotlightInv_V0_9_60\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath "D:\Downloads\V0_9_60_build.log"
& "T:\VestigantSpotlightInv_V0_9_60\build-msvc\Release\VestigantSpotlightCli.exe" --version
& "T:\VestigantSpotlightInv_V0_9_60\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V0_9_60\build-msvc\selftest_out"
```

## Standard iOS reuse-cache test

Use this for fast parser/view/export iteration while the large source ZIP and known-good cache remain unchanged.

```powershell
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_60\scripts\Run-V0_9_60-iOS-ReuseCache-CLI-AndZip.ps1" `
  -InputZip "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip" `
  -ReuseCache "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4" `
  -CaseRoot "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_60_ReusedCache" `
  -OutZip "D:\Downloads\Upload_Thin_iOS_GUI_V0_9_60_ReusedCache_Check.zip"
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

If a run stalls or stops writing, use the matching `Collect-V0_9_60-DBBloat-State.ps1` script before stopping/rerunning:

```powershell
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_60\scripts\Collect-V0_9_60-DBBloat-State.ps1" `
  -CaseRoot "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_60_ReusedCache" `
  -OutZip "D:\Downloads\Upload_State_V0_9_37_NoWrites_Stopped_Check.zip" `
  -StopVestigant
```

If MSVC reports `C2026: string too big`, the likely source is an oversized SQL raw-string literal.  Current validation checks keep raw SQL fragments below prior risk thresholds, but new large SQL should be split or moved further into the database schema layer.

## What to upload after each test

Upload:

- `D:\Downloads\V0_9_60_build.log`
- `D:\Downloads\Upload_Thin_iOS_GUI_V0_9_60_ReusedCache_Check.zip`
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

## V0_9_55 existing-case GUI feature test

Because reuse-cache and fresh-ZIP ingest paths are now completing successfully, GUI/view/export-only changes can be tested against an existing completed case. Build V0_9_55, start the GUI, open the existing case database from the Case Information tab, then use the macOS or iOS Investigation tab. Select any row in the results grid. The bottom `Selected Row Metadata / All Fields` pane should populate with the selected row's fields in vertical form so long text, metadata, and provenance values can be reviewed without horizontal scrolling.

Rerun reuse-cache only when a change creates new parser output rows. Rerun fresh-ZIP only when a change affects source ZIP inventory, staging, cache creation, or app database extraction.

## V0_9_60 - Windows MSVC batch-label build hotfix

V0_9_60 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_60 - Staged AFF4/raw source restoration and MB telemetry

V0_9_60 restores AFF4/APFS image and Raw IMG/DD image choices in the GUI source-type selector as staged image workflows. Folder and ZIP remain the production intake paths; AFF4/APFS is kept visible for macOS forensic images, and Raw IMG/DD is kept visible for raw/external media such as exFAT devices attached to Macs that may contain Spotlight indexes. Processing telemetry now displays throughput in decimal MB and MB/s rather than MiB/MiB/s for investigator readability. No parser interpretation logic was intentionally changed.

## V1.1.8 Update

- `BaselineVersionHistory.md` is now the append-only version-history baseline in `docs/FULL_VERSION_HISTORY.md` and `VERSION_HISTORY.md`.
- Windows long-path evidence writes were added for APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction output paths.
- SQLite WAL checkpoint/truncate is requested before upload packaging.
- Logger writes are mutex-protected for concurrent GUI/export/ingest paths.
- APFS decmpfs reconstruction remains bounded; the expected-output safety cap is now 256 MiB.


## V1.1.10.1 command-block documentation hotfix

- Corrected current-version build documentation to include the full extraction/build PowerShell block requested by the user.
- Corrected current-version macOS AFF4/APFS thin test documentation to include `Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut`.
- Updated `docs/NEW_CHAT_CONTINUATION_GUIDE.md` so new chats started from the newest upload include the full commands.
- No source parser/extraction behavior changed.

### TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build, because wrappers/docs changed and APFS behavior did not.
- iOS: not required, because no iOS intake/parser/schema/view code changed.
- Trigger for full AFF4/APFS test: any future change to traversal, copy-out, decompression, staging, external compare, or Store-V2 parse behavior.
