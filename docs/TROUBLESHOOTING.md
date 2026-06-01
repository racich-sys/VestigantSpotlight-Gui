## V0_9_17 troubleshooting update

If property/category dictionaries are still zero, first check `ios_spotlight_dbstr_map_inventory.csv` / `native_dbstr_map_inventory.csv`. Missing map files usually means the entire `index.spotlightV2` folder was not staged or extracted. Parse errors should be reviewed by map ID and source path before changing entity/date interpretation views.

## V0_9_15 - Spotlight entity review, parser-target triage, and V0_9_11_1 GUI fix carry-forward

- Reviewed V0_9_11 reused-cache thin upload: CLI run completed successfully with 344,445 Spotlight records, 21,472 recovered human-text/key-value rows, 344,445 Spotlight date candidates, and V0_9_11 Spotlight-first timeline/reference exports.
- Carried forward V0_9_11_1 GUI compile fix (`execGuiSql` in GUI SQL helpers) so CLI, tests, and GUI should all build from this baseline.
- Added `iOS - Spotlight Entity Review` / `ios_spotlight_entity_review.csv`, a Spotlight-first normalized entity view that classifies recovered CoreSpotlight text as URL/web, file/attachment, account/email, communication-app, message/communication text, or other Spotlight text.
- Added `iOS - Spotlight Entity Summary` / `ios_spotlight_entity_summary.csv`, grouped counts by entity type, source store, source field, date semantic class, and supporting FFS/app context.
- Added `iOS - Spotlight Native Parser Targets` / `ios_spotlight_native_parser_targets.csv`, a focused target list for deeper native CoreSpotlight decoding: records without recovered text and generic `__native_core_probe_string_*` fields needing property-name mapping.
- Reuse-cache development scripts continue to pass `--skip-container-hash` by default. Full source hashing remains available for final/forensic runs.
- Legacy V7 import remains compiled but is still deprecated/hidden from normal workflow; physical removal remains a separate cleanup after the active iOS Spotlight review path remains stable.


## V0_9_11_1

- Added `iOS - Spotlight Investigative Item Date Evidence` to directly link recovered CoreSpotlight text/value rows to raw Spotlight date candidates from the same Store-V2 record.
- Added `iOS - Spotlight Date Field Summary` to summarize raw Spotlight date fields, parse methods, semantic classifications, date ranges, and reporting cautions.
- Added CSV exports `ios_spotlight_investigative_item_date_evidence.csv` and `ios_spotlight_date_field_summary.csv`.
- Preserved cautious date language: `Last_Updated` remains CoreSpotlight metadata/index timing unless another decoded field supports created/modified/accessed/opened/used semantics.
- Reuse-cache test scripts continue to skip full container hashing by default for large development/test runs.

- Removed silent `LIMIT 5000` caps from several normal/core exports. Thin upload and `exports/upload_samples` files may still be bounded, but normal investigation exports should be full/chunked rather than silently truncated.

# V0_9_6 update

V0_9_6 adds reusable iOS source-intake/cache support for very large iOS FFS ZIP testing. Use `--reuse-ios-cache <completed-case-folder>` or, in the GUI, select `iOS/CoreSpotlight` and place the prior completed case folder in the `Evidence root / iOS cache case` field. This is intended for parser/enrichment iteration against the same source ZIP after a successful baseline run such as `Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4`. Reuse mode writes `source_cache_manifest.json` and `logs\ios_reuse_cache.log` and avoids re-listing/re-extracting the 250+ GiB ZIP.

# V0_9_6 Large iOS ZIP staging update

- Active baseline reset from user-uploaded V0_9_3.
- V0_9_6 addresses apparent hangs during stage_zip_source on very large iOS FFS ZIP files by streaming 7-Zip inventory output to CSV and writing heartbeat/progress logs.
- New runtime diagnostics: logs/ios_zip_stage_heartbeat.log and logs/ios_zip_inventory_progress.tsv.
- Targeted app database extraction now starts before the full FFS inventory stream and duplicate extraction is skipped.

# Vestigant Spotlight V0_9_1 Troubleshooting

## iOS content appears to exist only as generic string probes

This is currently expected for many iOS CoreSpotlight records. The parser recovers generic bounded strings as `__native_core_probe_string_N` rows, but formal CoreSpotlight property/category names, display names, and content types are still incomplete parser-roadmap work.

Use these outputs first:

- `exports/ios_store_parse_summary.csv`
- `exports/ios_string_probe_category_summary.csv`
- `exports/ios_string_probe_values.csv`
- `exports/ios_record_string_probe_summary.csv`
- `exports/ios_timeline_index_updates.csv`
- `exports/ios_app_parsed_record_summary.csv`
- `exports/ios_app_parsed_records.csv`

Do not report iOS `Last_Updated` as user activity or usage without another decoded field supporting that interpretation.

## Parsed app records are empty

`ios_app_parsed_records.csv` depends on staged iOS SQLite app databases that can be opened read-only and contain supported table families with rows. Empty output usually means one of these conditions is true:

- The FFS ZIP did not contain a known app database path.
- The database was staged but the relevant tables had zero rows.
- The table schema is not yet covered by the generic parser.
- The database requires an app-specific parser or additional extraction handling.

Use `ios_app_database_inventory.csv`, `ios_app_database_record_inventory.csv`, and `ios_app_database_record_summary.csv` to decide whether the issue is discovery, staging, table coverage, or row parsing.

## Database residency export is smaller than before

V0_9_1 aggregates matching app-database inventory before exporting `ios_database_residency_candidates.csv`. This is expected and improves speed. A status of `POTENTIAL_PARSED_APP_RECORDS_AVAILABLE` means parsed records exist for a relevant database family, but it is still only a review lead until exact row-level correlation is implemented.

## iOS timeline export is shorter than `timeline_event_count`

In V0_8_84, `ios_timeline_index_updates.csv` was capped at 20,000 rows. V0_9_1 removes that cap for the main export. The upload sample remains intentionally capped at 5,000 rows.

## Store summary says every record has a file name or path

In V0_8_84, the exported `ios_store_parse_summary.csv` counted placeholder values such as `------NONAME------` and `/` as file-name/path values. V0_9_1 exports the view-backed summary with placeholder counts separated from meaningful file-name/path counts.

## Quick diagnostics are preferred when full GUI ingest is not needed

Run:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V0_9_1\scripts\Run-V0_9_1-iOS-QuickDiagnostics.ps1
```

Upload:

```text
D:\Downloads\iOS_CoreSpotlight_QuickDiagnostics_V0_9_1.zip
D:\Downloads\iOS_CoreSpotlight_MinimalEvidence_V0_9_1.zip
```


## V0_9_1 iOS investigation exports

V0_9_1 adds three iOS-first pivot exports and GUI views for investigator review:

- `ios_protection_class_summary.csv`: summarizes record counts, string-probe counts, selected database counts, and date ranges by iOS protection class.
- `ios_artifact_hint_summary.csv`: groups decoded string probes into investigator-oriented buckets such as Mail AttachmentData paths, iCloud/CloudDocs, Google Drive/Docs, Microsoft Teams/OneDrive, Zoom, map links, calendar invitations, web links, iOS file paths, email/account text, and message/attachment text.
- `ios_record_investigation_hints.csv`: provides per-record protection class, primary investigation hint, hint categories, string-probe count, and index-update timing without embedding raw string samples in that rollup. Raw decoded string values remain in `ios_string_probe_values.csv`.

`Last_Updated` remains metadata/index update timing and must not be treated as usage without supporting decoded fields.


## V0_9_1 note

V0_9_1 fixes the iOS FFS/app database ZIP-entry inventory failure observed in V0_8_88_1 by avoiding Windows path APIs for iOS ZIP entry names. The expected result is nonzero FFS inventory rows and, where relevant databases exist, nonzero app database inventory/table-count rows. Full iOS inventory CSVs may be large; the thin upload script samples oversized CSVs and leaves the complete files in the local case folder.

## V0_9_1 iOS app-database interpretation update

V0_9_1 adds `ios_apple_messages_database_status.csv` and `ios_app_live_activity_timeline.csv`. The Apple Messages status export distinguishes an SMS.db that is present but has zero live message/chat/attachment rows from a parser failure. Database-residency candidates are now aggregated by database family and exclude WAL/SHM artifacts to reduce duplicate leads.



## V0_9_1 source-scoped iOS object linking

V0_9_1 adds active-source cleanup before export and three iOS object-linking exports: `ios_spotlight_object_identity.csv`, `ios_spotlight_to_ffs_object_links.csv`, and `ios_spotlight_to_app_db_record_links.csv`. These views are intended to help link CoreSpotlight IDs/path fragments/string probes back to FFS paths and parsed app database families. Correlation remains conservative unless exact app database row matching is available.


## V0_9_1 update

V0_9_1 adds GUI hover explanations for MacOS and iOS investigation view lists. Hover over a view name in either investigation tab to see what the view shows and the main interpretation limits.

V0_9_1 also improves Apple Messages/SMS.db handling. Messages are parsed with a schema-specific join across message, handle, chat, and attachment tables where present. Empty live SMS.db tables remain reported as empty; Spotlight message-like text should still be treated as a Spotlight-only candidate unless a matching app database record is parsed. Recoverable/deleted Messages-related tables are classified cautiously and do not by themselves prove deletion.

## V0_9_1 WhatsApp / keychain update

V0_9_1 adds the first schema-aware iOS WhatsApp review layer. The parser uses the uploaded WhatsApp/iLEAPP reference material as the trusted local schema reference for iOS WhatsApp databases and targets `ChatStorage.sqlite`, `ContactsV2.sqlite`, and WhatsApp `CallHistory.sqlite` where those databases are present in the iOS FFS ZIP. New GUI/export views include `iOS - WhatsApp Database Status`, `iOS - WhatsApp Parsed Records`, and `iOS - WhatsApp Parsed Summary`.

V0_9_1 also adds an inventory-only `iOS - Keychain Material Inventory` view and export. Keychain/keybag presence is reported as acquisition context only; the tool does not yet claim WhatsApp decryption or key extraction.

V0_9_1 tightens WhatsApp database classification so unrelated files whose filenames contain `whatsapp`, such as web assets or SVG logos, are not treated as WhatsApp app databases.

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
