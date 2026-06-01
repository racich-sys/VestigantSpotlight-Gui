## V0_9_27 continuation

Current baseline after V0_9_27 should be a more usable iOS Spotlight investigator workflow: compact normal-mode parsing is stable, the GUI C2026 issue from V0_9_26_1 is incorporated via V0_9_26_2, and new communication-centric review surfaces have been added. Next validation should check whether the new communication/attachment views make iOS Spotlight evidence review practical and whether specific records need better dbStr/property-name mapping. Continue using the saved thin-upload review workflow before changing code.

## V0_9_26_2 - MSVC oversized SQL literal compile fix

- Fixed Windows/MSVC compile failure `src\\db\\case_db.cpp(...): error C2026: string too big, trailing characters truncated` by splitting the affected SQL raw-string block into smaller runtime-joined fragments.
- Preserved V0_9_26 SQL/view behavior and chat-app attribution changes.
- Added validation note that all raw SQL literal fragments in `case_db.cpp` should remain below conservative MSVC-safe size thresholds.

V0.9.26.2 update: chat-app review now distinguishes explicit bundle/domain/external-id attribution from plain keyword/link mentions, adds `classification_evidence`, and adds `iOS - Spotlight Chat App Attribution Summary`. Use this summary before treating WhatsApp/Signal/Telegram terms as app attribution. The build-log/thin-upload review workflow is now documented for repeatable future iterations.

## Active continuation after V0_9_25

- Next validation focus: confirm Windows/MSVC build, reused-cache completion, absence of nested `Upload/` duplication in thin ZIPs, and usefulness of the new high-value Spotlight text-context views.
- Continue preserving Spotlight/CoreSpotlight as the primary evidence source; FFS/app DB materialization remains support/diagnostic unless explicitly requested.
- If high-value context is still too broad, next pass should add investigator keyword-list search and app/domain/entity grouping rather than re-enabling broad raw property persistence.

## Active continuation after V0_9_24

- Treat `parser_limits_and_suppression_summary.csv` as the first reference when row counts appear even or unexpectedly low/high.
- Continue keeping iOS CoreSpotlight normal mode compact: broad native/dbStr/property persistence, full FFS inventory, and broad app DB parsed-record persistence remain support/diagnostic-only.
- Next validation focus: confirm V0_9_24 Windows/MSVC build, confirm reused-cache completion, and compare `raw_records`, `raw_key_values`, `raw_date_candidates`, `parser_limits_and_suppression_summary.csv`, Missing From FFS high-value summaries, and the new Spotlight text-context sample.
- Future development: if counts are still hard to interpret, add per-table suppression counters directly from parser runtime instead of only deriving ratios during export.

## V0_9_17 - iOS CoreSpotlight output and database bloat control

V0_9_17 fixes the V0_9_16 large-case regression where successful iOS dbStr/property decoding generated too many low-level raw key/value rows and massive support CSVs during normal investigator/reuse-cache runs. Normal iOS runs now keep high-value raw key/value rows by default, preserve date candidates for provenance, and move full native/dbStr/support exports behind explicit diagnostic/support modes. Added `--diagnostic-full-native-db` for bounded support runs that intentionally need full native key/value persistence.

## V0_9_17 - iOS CoreSpotlight dbStr dictionary decoding and Apple field coverage

- Added clean-room iOS CoreSpotlight dbStr map loading for `index.spotlightV2` evidence folders. The parser now looks for `dbStr-1`, `dbStr-2`, `dbStr-4`, and `dbStr-5` map components beside `store.db` / `.store.db` and records whether each map was found, parsed, or missing.
- Added native diagnostics tables for `native_dbstr_map_inventory`, `native_category_dictionary`, and `native_index_dictionary_summary`.
- Added GUI/export review surfaces: `iOS - Spotlight dbStr Map Inventory`, `iOS - Spotlight Dictionary Coverage`, and `iOS - Spotlight Apple Field Coverage`.
- Added Apple-public-field semantic grouping for recovered Spotlight/CoreSpotlight field names. Apple developer documentation is used only as a public semantic taxonomy, not as private on-disk Store-V2 schema.
- Yogesh Khatri `spotlight_parser` / `mac_apt` behavior is treated as a reference only; this version implements independent C++ parsing logic and does not copy GPL source.
- Reuse-cache scripts continue to use `--skip-container-hash` by default for development/test iteration against large iOS FFS ZIPs.
- Legacy V7 import remains compiled but deprecated/hidden from normal workflow; physical removal remains a later cleanup once active Spotlight/CoreSpotlight paths remain stable.

## V0_9_15 - Spotlight entity review, parser-target triage, and V0_9_11_1 GUI fix carry-forward

- Reviewed V0_9_11 reused-cache thin upload: CLI run completed successfully with 344,445 Spotlight records, 21,472 recovered human-text/key-value rows, 344,445 Spotlight date candidates, and V0_9_11 Spotlight-first timeline/reference exports.
- Carried forward V0_9_11_1 GUI compile fix (`execGuiSql` in GUI SQL helpers) so CLI, tests, and GUI should all build from this baseline.
- Added `iOS - Spotlight Entity Review` / `ios_spotlight_entity_review.csv`, a Spotlight-first normalized entity view that classifies recovered CoreSpotlight text as URL/web, file/attachment, account/email, communication-app, message/communication text, or other Spotlight text.
- Added `iOS - Spotlight Entity Summary` / `ios_spotlight_entity_summary.csv`, grouped counts by entity type, source store, source field, date semantic class, and supporting FFS/app context.
- Added `iOS - Spotlight Native Parser Targets` / `ios_spotlight_native_parser_targets.csv`, a focused target list for deeper native CoreSpotlight decoding: records without recovered text and generic `__native_core_probe_string_*` fields needing property-name mapping.
- Reuse-cache development scripts continue to pass `--skip-container-hash` by default. Full source hashing remains available for final/forensic runs.
- Legacy V7 import remains compiled but is still deprecated/hidden from normal workflow; physical removal remains a separate cleanup after the active iOS Spotlight review path remains stable.

## V0_9_11_1 continuation note

V0_9_11 CLI/tests built but GUI failed because split GUI SQL blocks used `exec(...)` instead of `execGuiSql(...)`. V0_9_11_1 fixes that narrow GUI build blocker. Continue to treat Spotlight/CoreSpotlight as the primary evidence source; FFS/app database correlation remains supporting context. Legacy V7 import removal remains deferred to a separate cleanup build after this path is stable.

## V0_9_11_1 - Spotlight investigative value/date review surfaces

- Reviewed V0_9_10 build/thin upload: MSVC build and reused-cache run completed successfully.
- Added Spotlight-first review exports and GUI views for high-value timeline, file references, URL references, account/contact references, and decode-gap summary.
- Preserved date provenance fields so investigative values can be traced back to raw Spotlight date candidates and source Store-V2 records.
- Kept FFS/app database context secondary to Spotlight evidence.
- Reuse-cache development script continues to use `--skip-container-hash` by default.
- Legacy V7 import remains compiled for compatibility but should be removed/hard-disabled in a separate cleanup once the current iOS Spotlight review path remains stable.

## V0_9_11_1 continuation note - after V0_9_7_2 thin upload

V0_9_7_2 built far enough to run but failed at SQLite open/schema creation because the C2026 string-splitting fix accidentally left literal `R"SQL(` text inside SQL sent to SQLite. V0_9_11_1 removes those nested raw-string markers. Primary direction remains Spotlight/CoreSpotlight-first parsing and investigator review. FFS/app database comparison remains supporting context. Legacy V7 import should be removed/hard-disabled later after this active iOS Spotlight review path stabilizes; do not spend the immediate fix cycle on V7 removal unless it blocks current work.

## V0_9_11_1 continuation note

Primary goal remains Spotlight/CoreSpotlight-first investigation. V0_9_7_1 CLI run completed and produced useful Spotlight-first review exports, but GUI failed to compile due to an oversized SQL raw string in `win32_gui.cpp`. V0_9_11_1 fixes that compile blocker and preserves the successful CLI/reuse-cache workflow. Reuse-cache test scripts should skip full container hash by default; final forensic/preservation runs may force hashing when needed. Thin-upload CSVs ending in `_sample5000.csv` are diagnostic samples only; normal local investigation exports must remain full/untruncated.


## V0_9_11_1 roadmap update - Spotlight database first

Primary focus is deeper iOS/macOS Spotlight/CoreSpotlight parsing and investigator visibility into native records, fields, decoded text, decode coverage, and gaps. FFS/app-database comparison remains important corroboration but is secondary to Spotlight as the evidence source.

# V0_9_6 update

V0_9_6 adds reusable iOS source-intake/cache support for very large iOS FFS ZIP testing. Use `--reuse-ios-cache <completed-case-folder>` or, in the GUI, select `iOS/CoreSpotlight` and place the prior completed case folder in the `Evidence root / iOS cache case` field. This is intended for parser/enrichment iteration against the same source ZIP after a successful baseline run such as `Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4`. Reuse mode writes `source_cache_manifest.json` and `logs\ios_reuse_cache.log` and avoids re-listing/re-extracting the 250+ GiB ZIP.

# V0_9_6 Large iOS ZIP staging update

- Active baseline reset from user-uploaded V0_9_3.
- V0_9_6 addresses apparent hangs during stage_zip_source on very large iOS FFS ZIP files by streaming 7-Zip inventory output to CSV and writing heartbeat/progress logs.
- New runtime diagnostics: logs/ios_zip_stage_heartbeat.log and logs/ios_zip_inventory_progress.tsv.
- Targeted app database extraction now starts before the full FFS inventory stream and duplicate extraction is skipped.

# Current continuation note - V0_8_71

Version: `0.8.71`

Continue from V0_8_71 unless the user explicitly says otherwise. Do not restart from older versions.

## Current roadmap position

The project remains in APFS/AFF4 extraction validation and iOS CoreSpotlight intake validation. Do not shift to GUI, licensing, or broad investigator-view work until the APFS/iOS validation loop is stable.

## V0_8_70 review result that drove V0_8_71

V0_8_70 closed the known duplicate same-path Cache candidate selection issue: `MATCH_RELATIVE_PATH_AND_HASH` increased from 10,112 to 10,119 and `RELATIVE_PATH_SIZE_MISMATCH` decreased from 103 to 96. The remaining 96 mismatches no longer had an exact hash/size candidate already available in copy-out, so the next review needs better extraction/reconstruction diagnostics rather than another selector-only fix.

The iOS V0_8_70 run showed that intake/parsing worked but diagnostics export failed on an absent `native_decode_errors` table. The focused iOS ZIP contained 14 valid `store.db` / `.store.db` files across 7 CoreSpotlight store groups, so future iOS review should focus on multi-store discovery/selection and useful string/date/category exports.

## V0_8_71 changes to validate

1. APFS external-compare runs should now include:
   - `aff4_apfs_remaining_mismatch_diagnostics.csv`
   - `aff4_apfs_remaining_mismatch_diagnostics_summary.json`
   - `aff4_apfs_remaining_mismatch_diagnostics.md`

2. iOS diagnostics runs should no longer fail exporting `native_decode_errors.csv`.

3. iOS focused ZIP runs should use `--full-scan` and should discover 14 valid database candidates / 7 selected primary stores for the current focused extracts ZIP.

4. iOS focused exports should include:
   - `exports/ios_store_parse_summary.csv`
   - `exports/ios_string_probe_category_summary.csv`
   - `exports/ios_string_probe_values.csv`
   - `exports/ios_timeline_index_updates.csv`

## Required validation workflow when new outputs are uploaded

For `Upload_Thin_*_ExternalCompare.zip`, reuse this method: unpack to a temp review folder; inventory key files; read `aff4_apfs_external_spotlight_compare_summary.json`; parse `aff4_apfs_external_spotlight_file_compare.csv` status counts; parse/correlate `aff4_apfs_spotlight_file_copy_out.csv`, `aff4_apfs_extracted_storev2_stage_files.csv`, `aff4_apfs_spotlight_xattr_probe.csv`, and `aff4_apfs_remaining_mismatch_diagnostics.csv`; then classify remaining issues before creating another build.

For iOS CoreSpotlight uploads, reuse this method: inventory focused report and extracts ZIPs; enumerate ZIP contents; identify CoreSpotlight `store.db`/`.store.db` files by path and validity; inspect `store_inventory.csv`, `store_selection.csv`, and the new `exports/ios_*` CSVs; then determine which categories/fields are useful for investigation before building deeper parser logic.

## APFS PowerShell command

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V0_8_71.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_8_71" -Recurse -Force -ErrorAction SilentlyContinue

Expand-Archive `
  -LiteralPath .\VestigantSpotlightInv_V0_8_71.zip `
  -DestinationPath T:\ `
  -Force

& "T:\VestigantSpotlightInv_V0_8_71\build_windows_msvc.bat" 2>&1 |
  Tee-Object -FilePath "D:\Downloads\V0_8_71_build.log"

& "T:\VestigantSpotlightInv_V0_8_71\build-msvc\Release\VestigantSpotlightCli.exe" --version

& "T:\VestigantSpotlightInv_V0_8_71\tools\Run-SingleAff4SourceProbeAndZip.ps1" `
  -Aff4Input "O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4" `
  -Out "T:\SpotlightCase\V0_8_71_ExternalCompare" `
  -ReaderToolsRoot "T:\VestigantReaderTools\aff4-cpp-lite" `
  -ZipPath "D:\Downloads\Upload_Thin_V0_8_71_ExternalCompare.zip" `
  -EnableAff4VirtualApfsProbe `
  -ExternalSpotlightRoot "T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2" `
  -ExternalCompareOutRoot "T:\V0_8_71_ExternalCompare_CompareOnly" `
  -UploadWorkRoot "D:\Downloads\V0_8_71_UploadWork" `
  -NoClipboardOrExplorer `
  -CleanOut
```

Upload after APFS run:

```text
D:\Downloads\V0_8_71_build.log
D:\Downloads\Upload_Thin_V0_8_71_ExternalCompare.zip
```

## iOS PowerShell command

```powershell
Set-Location D:\Downloads

& "T:\VestigantSpotlightInv_V0_8_71\tools\Extract-iOSCoreSpotlightFromFFSZips.ps1" `
  -ZipFolder "T:\iOS_Extraction" `
  -ExtractRoot "D:\Downloads\iOS_CoreSpotlight_Focused_Extracts" `
  -ReportRoot "D:\Downloads\iOS_CoreSpotlight_Focused_Report" `
  -ReportZip "D:\Downloads\iOS_CoreSpotlight_Focused_Report.zip" `
  -EvidenceZip "D:\Downloads\iOS_CoreSpotlight_Focused_Extracts.zip" `
  -SevenZip "C:\Program Files\7-Zip\7z.exe"

Get-FileHash "D:\Downloads\iOS_CoreSpotlight_Focused_Report.zip" -Algorithm SHA256
Get-FileHash "D:\Downloads\iOS_CoreSpotlight_Focused_Extracts.zip" -Algorithm SHA256

& "T:\VestigantSpotlightInv_V0_8_71\tools\Run-IosCoreSpotlightFocusedZip.ps1" `
  -InputZipOrFolder "D:\Downloads\iOS_CoreSpotlight_Focused_Extracts.zip" `
  -Out "T:\SpotlightCase\V0_8_71_iOS_CoreSpotlight" `
  -ZipPath "D:\Downloads\Upload_Thin_V0_8_71_iOS_CoreSpotlight.zip" `
  -CleanOut `
  -NoClipboardOrExplorer

Get-FileHash "D:\Downloads\Upload_Thin_V0_8_71_iOS_CoreSpotlight.zip" -Algorithm SHA256
```

Upload after iOS run:

```text
D:\Downloads\iOS_CoreSpotlight_Focused_Report.zip
D:\Downloads\iOS_CoreSpotlight_Focused_Extracts.zip
D:\Downloads\Upload_Thin_V0_8_71_iOS_CoreSpotlight.zip
```

---


## Added in v0.8.74

- APFS/AFF4: increased bounded ResourceFork stream copy and decmpfs reconstruction caps to improve coverage of remaining BADA95B6 Cache `.txt` resource-fork targets.
- APFS/AFF4: prioritized Cache `.txt` ZLIB_RSRC reconstruction targets before unsupported resource-fork codecs.
- Diagnostics: replaced APFS remaining-mismatch diagnostic script with robust path normalization and classification.
- Keyword search: added a first-stage investigator keyword-search PowerShell script (`tools\Search-SpotlightKeywordExports.ps1`) for macOS/iOS case exports. Future versions should move this into a native CLI/GUI workflow and add source-app/database residency correlation.


## V0_8_99 iOS investigation exports

V0_8_99 adds three iOS-first pivot exports and GUI views for investigator review:

- `ios_protection_class_summary.csv`: summarizes record counts, string-probe counts, selected database counts, and date ranges by iOS protection class.
- `ios_artifact_hint_summary.csv`: groups decoded string probes into investigator-oriented buckets such as Mail AttachmentData paths, iCloud/CloudDocs, Google Drive/Docs, Microsoft Teams/OneDrive, Zoom, map links, calendar invitations, web links, iOS file paths, email/account text, and message/attachment text.
- `ios_record_investigation_hints.csv`: provides per-record protection class, primary investigation hint, hint categories, string-probe count, and index-update timing without embedding raw string samples in that rollup. Raw decoded string values remain in `ios_string_probe_values.csv`.

`Last_Updated` remains metadata/index update timing and must not be treated as usage without supporting decoded fields.


## V0_8_99 note

V0_8_99 fixes the iOS FFS/app database ZIP-entry inventory failure observed in V0_8_88_1 by avoiding Windows path APIs for iOS ZIP entry names. The expected result is nonzero FFS inventory rows and, where relevant databases exist, nonzero app database inventory/table-count rows. Full iOS inventory CSVs may be large; the thin upload script samples oversized CSVs and leaves the complete files in the local case folder.

## V0_8_99 iOS app-database interpretation update

V0_8_99 adds `ios_apple_messages_database_status.csv` and `ios_app_live_activity_timeline.csv`. The Apple Messages status export distinguishes an SMS.db that is present but has zero live message/chat/attachment rows from a parser failure. Database-residency candidates are now aggregated by database family and exclude WAL/SHM artifacts to reduce duplicate leads.



## V0_8_99 source-scoped iOS object linking

V0_8_99 adds active-source cleanup before export and three iOS object-linking exports: `ios_spotlight_object_identity.csv`, `ios_spotlight_to_ffs_object_links.csv`, and `ios_spotlight_to_app_db_record_links.csv`. These views are intended to help link CoreSpotlight IDs/path fragments/string probes back to FFS paths and parsed app database families. Correlation remains conservative unless exact app database row matching is available.

## V0_9_6 communications and keychain review update

V0_9_6 was created after reviewing the V0_9_1 thin iOS GUI upload. The V0_9_1 run completed successfully and confirmed that the iOS FFS inventory, app database inventory, app database parsing, keychain inventory, Spotlight-to-FFS links, and Spotlight-to-app-DB candidate links are being produced.

V0_9_6 adds a communications-centered iOS review layer: `ios_communications_review_records.csv`, `ios_communications_review_summary.csv`, and `ios_spotlight_communication_candidates.csv`. The corresponding GUI views are `iOS - Communications Review Records`, `iOS - Communications Review Summary`, and `iOS - Spotlight Communication Candidates`. These views aggregate parsed communication-related app database rows such as Apple Messages/SMS, WhatsApp when present, Phone/FaceTime call history, and message/chat/call-like records.

V0_9_6 also improves WhatsApp status reporting. If no iOS WhatsApp ChatStorage/Contacts/CallHistory database is found in the FFS inventory, the WhatsApp status view now returns an explicit `WHATSAPP_DB_NOT_FOUND` row instead of an empty result set.

V0_9_6 tightens keychain review. Core keychain/keybag material remains in `iOS - Keychain Material Inventory`, while lower-priority keychain-named framework/code references are separated into `iOS - Keychain Support References`. This prevents app framework names such as SAMKeychain from being mixed with core keychain material.

Forensic interpretation remains conservative: Spotlight communication candidates are candidate strings only unless independently supported by parsed app database rows, FFS path matches, or stronger corroboration.


## V0_9_6 iOS investigator pivot / keyword-surface update

V0_9_6 was created after reviewing the V0_9_2 iOS GUI thin upload. The V0_9_2 run completed successfully and produced the new communications/keychain views. The run confirmed 5 valid iOS CoreSpotlight stores, 37,719 raw Spotlight records, 407,826 imported FFS inventory rows, 2,898 iOS app database inventory rows, 32 opened extracted app databases, 109,592 parsed app database records, 550 communications review records, 228 Spotlight communication candidates, 51 core keychain/keybag material rows, and 739 lower-priority keychain support-reference rows.

V0_9_6 adds additional iOS Investigation tab views and exports intended to make the parsed app database data more usable without forcing the investigator into the full generic parsed-record table:

- `iOS - Contact Identity Review` / `ios_contact_identity_records.csv`
- `iOS - Contact Identity Summary` / `ios_contact_identity_summary.csv`
- `iOS - Web History Review` / `ios_web_history_review_records.csv`
- `iOS - Web History Summary` / `ios_web_history_review_summary.csv`
- `iOS - Calendar Review` / `ios_calendar_review_records.csv`
- `iOS - Calendar Summary` / `ios_calendar_review_summary.csv`
- `iOS - Unified Keyword Search Surface` / `ios_investigation_keyword_surface.csv`

The unified keyword surface combines CoreSpotlight human-readable strings, parsed app database rows, selected high-value FFS paths, and app database inventory rows into one searchable review view. It is intended for keyword/name/domain/phone/path/app pivots using the GUI search box, while preserving source type, source location, timestamp, path/URL/contact fields, residency context, and interpretation limits.

Contact, calendar, and web review views are triage views over parsed local app database records. They are not deletion findings. Cache/FTS/contact-token tables can duplicate or tokenize records, and calendar rows can represent support rows rather than user events. Use the source database path, table name, primary key, provenance, and timestamp source columns before reporting.

WhatsApp status remains `WHATSAPP_DB_NOT_FOUND` in the current test dataset because no iOS WhatsApp `ChatStorage.sqlite`, `ContactsV2.sqlite`, or WhatsApp `CallHistory.sqlite` was present. WhatsApp parsing still needs validation on a dataset containing those databases.
