## V0_9_27 - iOS Spotlight communications investigator workflow

Start iOS Spotlight review with these GUI views/exports:

1. `iOS - Spotlight Communication Summary` / `exports/ios_spotlight_communication_summary.csv` for counts by Messages, Mail, calls, chat-app context, contacts, calendar, and URL/web context.
2. `iOS - Spotlight Communications Investigator Review` / `exports/ios_spotlight_communication_record_review_sample.csv` for record-centric columns including title/name, snippet/description, account, service, phone/callback, URL/content reference, attachment path, Spotlight date, raw record locator, and same-record Spotlight text context.
3. `iOS - Spotlight Attachment/Media References` / `exports/ios_spotlight_attachment_reference_review_sample.csv` for communication-linked attachment/media/content references.
4. `exports/parser_limits_and_suppression_summary.csv` before interpreting even-looking compact row counts.

Normal iOS mode remains compact and Spotlight-first. App DB and FFS evidence is corroborating/support context and is not materialized broadly unless support/diagnostic flags are used.

## V0_9_26_2 - MSVC oversized SQL literal compile fix

- Fixed Windows/MSVC compile failure `src\\db\\case_db.cpp(...): error C2026: string too big, trailing characters truncated` by splitting the affected SQL raw-string block into smaller runtime-joined fragments.
- Preserved V0_9_26 SQL/view behavior and chat-app attribution changes.
- Added validation note that all raw SQL literal fragments in `case_db.cpp` should remain below conservative MSVC-safe size thresholds.

V0.9.26.2 update: chat-app review now distinguishes explicit bundle/domain/external-id attribution from plain keyword/link mentions, adds `classification_evidence`, and adds `iOS - Spotlight Chat App Attribution Summary`. Use this summary before treating WhatsApp/Signal/Telegram terms as app attribution. The build-log/thin-upload review workflow is now documented for repeatable future iterations.

V0.9.25 update: V0_9_25 adds priority-ranked iOS Spotlight text-context views. For investigator review, start with `iOS - High-Value Spotlight Text Context`, `iOS - Spotlight Text Context Priority Summary`, `iOS - High-Value Missing From FFS`, and `exports/parser_limits_and_suppression_summary.csv`. Generic text-context and probe samples are still compact review surfaces, not full raw property dumps. Thin uploads now exclude nested prior Upload folders and resample oversized generic support samples.

V0.9.24 update: V0_9_24 adds explicit run-limit and suppression reporting after the V0_9_23_1 run completed successfully but still produced deliberately compact/even-looking counts. Review `exports/parser_limits_and_suppression_summary.csv` before interpreting row counts. It distinguishes unlimited native Spotlight record parsing from intentionally compact raw key/value/date persistence, thin-upload sampling, and support-only FFS/app DB materialization. V0_9_24 also adds `iOS - Spotlight Text Context Review` so investigators can see the compact same-record Spotlight text retained in normal iOS mode.

## V0_9_24
- New report: `exports/parser_limits_and_suppression_summary.csv`.
- New GUI view: `iOS - Spotlight Text Context Review`.
- New normal export: `ios_spotlight_text_context_review_sample.csv`.
- CASE_REVIEW_SUMMARY now includes a run limits/suppression section.
- Thin upload packaging now includes the limits report and text-context sample.
- Normal iOS mode remains compact and Spotlight-first; full native/dbStr/property persistence and broad app DB/FFS materialization remain explicit support/diagnostic choices.

V0.9.21 update: V0_9_20 stopped-state review showed the database was no longer the primary failure; the run stalled during normal export SQL materialization at ios_spotlight_record_review.csv after completing ios_spotlight_object_inode_summary as a large split export. V0_9_21 moves full broad per-record/per-object Spotlight exports to support/diagnostic mode by default, adds compact ios_spotlight_object_inode_diagnostic_summary.csv for object/inode record-splitting analysis, adds object-key indexes, and fixes raw_key_values-to-raw_records human-text mapping so multiple raw records sharing the same object key do not multiply Spotlight text rows during review views.

## V0_9_21
- Normal iOS investigator export now avoids full ios_spotlight_record_review/date_provenance/investigative_items/object_inode_summary materialization.
- Added compact object/inode diagnostic summary to evaluate whether repeated raw records represent the same object/inode.
- Added preferred raw_record_id mapping in human text views to avoid cross-multiplying same-object raw_key_values.
- Added V0_9_21 concrete build, reuse-cache, packaging, and stopped-state scripts.
- Linux validation: build and self-test passed; Windows/MSVC validation pending.



V0.9.20 update: V0_9_18 validation showed the 5 GiB SQLite guardrail fired at 150,000 parsed items with ~1.4M raw_key_values and ~463k raw_date_candidates, confirming remaining bloat was still raw table materialization. V0_9_20 further reduces normal iOS CoreSpotlight raw persistence to reference-only key/value rows, suppresses Last_Updated date candidate rows in normal mode because raw_records.last_updated_utc already preserves that timing, propagates SQLite guardrail aborts as fatal instead of continuing after a failed guardrail, and adds compact same-record Spotlight text context to the iOS Missing From FFS Candidates GUI/view where available.

## V0_9_20

- Follow-up to V0_9_18 DB guardrail failure during large iOS CoreSpotlight reuse-cache validation.
- Normal iOS Spotlight-first mode now references/counts cached FFS inventory instead of inserting millions of file rows into the active case DB.
- Normal mode skips broad app database parsed-record materialization unless explicitly requested.
- Added support flags: `--materialize-ios-ffs-inventory`, `--materialize-ios-app-db-records`, and `--materialize-ios-support-db`.
- Tightened default iOS `raw_key_values` to reference-only rows and reduced `raw_date_candidates`; full native/dbStr/property persistence remains diagnostics-only.
- DB/WAL guardrail exceptions now abort cleanly instead of being caught as recoverable item/store parse failures.

## V0_9_17 - iOS CoreSpotlight output and database bloat control

V0_9_17 fixes the V0_9_16 large-case regression where successful iOS dbStr/property decoding generated too many low-level raw key/value rows and massive support CSVs during normal investigator/reuse-cache runs. Normal iOS runs now keep high-value raw key/value rows by default, preserve date candidates for provenance, and move full native/dbStr/support exports behind explicit diagnostic/support modes. Added `--diagnostic-full-native-db` for bounded support runs that intentionally need full native key/value persistence.

## V0_9_17 user note

After running V0_9_17, open the iOS Investigation tab and review the new Spotlight dictionary views before broader FFS/app database support views. The intended validation question is whether the iOS CoreSpotlight folder includes usable `dbStr-*` maps and whether recovered records now receive named property/category coverage.

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

# Vestigant Spotlight V0_9_1 User Manual Addendum

V0_9_1 is an iOS CoreSpotlight investigation build. The current validation source is:

```text
T:\0202_0024-IT002\00008132-000269523699001C_files_full.zip
```

The current GUI case output convention is:

```text
Q:\SpotlightCase\TestiOS_V0_9_1
```

## iOS interpretation rules

- `Last_Updated` is metadata/index-update timing, not user file usage by itself.
- `__native_core_probe_string_N` rows are recovered string payloads from iOS Store-V2 records.
- Formal iOS property/category mapping is still roadmap work.
- `ios_string_probe_values.csv` contains raw case-review strings.
- `ios_record_string_probe_summary.csv` rolls string probes up per CoreSpotlight record for faster review.
- `ios_redacted_investigation_summary.csv` is safer for high-level sharing because raw strings are not included.

## Main iOS review exports

- `ios_store_parse_summary.csv`
- `ios_string_probe_category_summary.csv`
- `ios_string_probe_values.csv`
- `ios_record_string_probe_summary.csv`
- `ios_domain_url_summary.csv`
- `ios_redacted_investigation_summary.csv`
- `ios_timeline_index_updates.csv`
- `ios_app_parsed_records.csv`
- `ios_app_parsed_record_summary.csv`

## GUI review views

Use the iOS tab or review view list:

- iOS - Store Parse Summary
- iOS - String Category Summary
- iOS - String Probe Values
- iOS - Record String Probe Summary
- iOS - Index Update Timeline
- iOS - Parsed Artifacts
- iOS - Parsed App Records
- iOS - Parsed App Record Summary

## Fast diagnostic path

Use the quick diagnostic script when the goal is to provide evidence for parser/discovery review without running the entire GUI ingest workflow:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V0_9_1\scripts\Run-V0_9_1-iOS-QuickDiagnostics.ps1
```


## V0_9_1 iOS investigation exports

V0_9_1 adds three iOS-first pivot exports and GUI views for investigator review:

- `ios_protection_class_summary.csv`: summarizes record counts, string-probe counts, selected database counts, and date ranges by iOS protection class.
- `ios_artifact_hint_summary.csv`: groups decoded string probes into investigator-oriented buckets such as Mail AttachmentData paths, iCloud/CloudDocs, Google Drive/Docs, Microsoft Teams/OneDrive, Zoom, map links, calendar invitations, web links, iOS file paths, email/account text, and message/attachment text.
- `ios_record_investigation_hints.csv`: provides per-record protection class, primary investigation hint, hint categories, string-probe count, and index-update timing without embedding raw string samples in that rollup. Raw decoded string values remain in `ios_string_probe_values.csv`.

`Last_Updated` remains metadata/index update timing and must not be treated as usage without supporting decoded fields.

## V0_9_1 parsed app-database records

V0_9_1 adds a generic read-only parser for staged iOS SQLite app databases discovered in the full-file-system ZIP. It currently targets investigator-relevant table families such as messages, message attachments, participants, calls, Safari/web history, mail, calendar, contacts, and chat-style records.

The parser writes `ios_app_parsed_records.csv` and `ios_app_parsed_record_summary.csv`. These records are intended as leads for review and correlation. A parsed app-database record does not by itself prove deletion, usage, or a one-to-one match with a CoreSpotlight string.


## V0_9_1 note

V0_9_1 fixes the iOS FFS/app database ZIP-entry inventory failure observed in V0_8_88_1 by avoiding Windows path APIs for iOS ZIP entry names. The expected result is nonzero FFS inventory rows and, where relevant databases exist, nonzero app database inventory/table-count rows. V0_9_1 then attempts generic app-database row parsing for supported tables. Full iOS inventory CSVs may be large; the thin upload script samples oversized CSVs and leaves the complete files in the local case folder.

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
