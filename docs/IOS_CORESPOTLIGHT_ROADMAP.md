## V0_9_17 iOS CoreSpotlight roadmap update

Immediate focus is validating folder-level `index.spotlightV2` parsing: `store.db` / `.store.db` plus `dbStr-1`, `dbStr-2`, `dbStr-4`, and `dbStr-5` map components. The next thin upload should be checked for map files found/missing, property/category row counts, generic probe reduction, and Apple-public-field coverage.

## V0_9_15 - Spotlight entity review, parser-target triage, and V0_9_11_1 GUI fix carry-forward

- Reviewed V0_9_11 reused-cache thin upload: CLI run completed successfully with 344,445 Spotlight records, 21,472 recovered human-text/key-value rows, 344,445 Spotlight date candidates, and V0_9_11 Spotlight-first timeline/reference exports.
- Carried forward V0_9_11_1 GUI compile fix (`execGuiSql` in GUI SQL helpers) so CLI, tests, and GUI should all build from this baseline.
- Added `iOS - Spotlight Entity Review` / `ios_spotlight_entity_review.csv`, a Spotlight-first normalized entity view that classifies recovered CoreSpotlight text as URL/web, file/attachment, account/email, communication-app, message/communication text, or other Spotlight text.
- Added `iOS - Spotlight Entity Summary` / `ios_spotlight_entity_summary.csv`, grouped counts by entity type, source store, source field, date semantic class, and supporting FFS/app context.
- Added `iOS - Spotlight Native Parser Targets` / `ios_spotlight_native_parser_targets.csv`, a focused target list for deeper native CoreSpotlight decoding: records without recovered text and generic `__native_core_probe_string_*` fields needing property-name mapping.
- Reuse-cache development scripts continue to pass `--skip-container-hash` by default. Full source hashing remains available for final/forensic runs.
- Legacy V7 import remains compiled but is still deprecated/hidden from normal workflow; physical removal remains a separate cleanup after the active iOS Spotlight review path remains stable.

## V0_9_11_1 - Spotlight investigative value/date review surfaces

- Reviewed V0_9_10 build/thin upload: MSVC build and reused-cache run completed successfully.
- Added Spotlight-first review exports and GUI views for high-value timeline, file references, URL references, account/contact references, and decode-gap summary.
- Preserved date provenance fields so investigative values can be traced back to raw Spotlight date candidates and source Store-V2 records.
- Kept FFS/app database context secondary to Spotlight evidence.
- Reuse-cache development script continues to use `--skip-container-hash` by default.
- Legacy V7 import remains compiled for compatibility but should be removed/hard-disabled in a separate cleanup once the current iOS Spotlight review path remains stable.


## V0_9_11_1

- Added `iOS - Spotlight Investigative Item Date Evidence` to directly link recovered CoreSpotlight text/value rows to raw Spotlight date candidates from the same Store-V2 record.
- Added `iOS - Spotlight Date Field Summary` to summarize raw Spotlight date fields, parse methods, semantic classifications, date ranges, and reporting cautions.
- Added CSV exports `ios_spotlight_investigative_item_date_evidence.csv` and `ios_spotlight_date_field_summary.csv`.
- Preserved cautious date language: `Last_Updated` remains CoreSpotlight metadata/index timing unless another decoded field supports created/modified/accessed/opened/used semantics.
- Reuse-cache test scripts continue to skip full container hashing by default for large development/test runs.


## V0_9_11_1 roadmap update - Spotlight database first

Primary focus is deeper iOS/macOS Spotlight/CoreSpotlight parsing and investigator visibility into native records, fields, decoded text, decode coverage, and gaps. FFS/app-database comparison remains important corroboration but is secondary to Spotlight as the evidence source.

# V0_9_6 update

V0_9_6 adds reusable iOS source-intake/cache support for very large iOS FFS ZIP testing. Use `--reuse-ios-cache <completed-case-folder>` or, in the GUI, select `iOS/CoreSpotlight` and place the prior completed case folder in the `Evidence root / iOS cache case` field. This is intended for parser/enrichment iteration against the same source ZIP after a successful baseline run such as `Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4`. Reuse mode writes `source_cache_manifest.json` and `logs\ios_reuse_cache.log` and avoids re-listing/re-extracting the 250+ GiB ZIP.

# V0_9_6 Large iOS ZIP staging update

- Active baseline reset from user-uploaded V0_9_3.
- V0_9_6 addresses apparent hangs during stage_zip_source on very large iOS FFS ZIP files by streaming 7-Zip inventory output to CSV and writing heartbeat/progress logs.
- New runtime diagnostics: logs/ios_zip_stage_heartbeat.log and logs/ios_zip_inventory_progress.tsv.
- Targeted app database extraction now starts before the full FFS inventory stream and duplicate extraction is skipped.

# iOS CoreSpotlight Roadmap

This is an active roadmap track, separate from the macOS Store-V2 parser.

## Input assumptions

- iOS evidence will usually arrive as full-file-system ZIP/folder/container output from forensic tools such as GrayKey, Cellebrite, Verakey, or similar.
- The iOS route should not be forced through macOS Store-V2 parsing.
- The first iOS goal is discovery and triage of CoreSpotlight artifacts, especially `index.db` and metadata payloads.

## Milestones

1. Evidence-source profile for iOS full-file-system ZIP/folder inputs.
2. CoreSpotlight locator output: candidate paths, file sizes, hashes, and app/container context.
3. Separate parser route for CoreSpotlight SQLite `index.db`.
4. Table/column inventory and parser coverage reporting.
5. Safe binary plist / NSKeyedArchiver decoding with per-record isolation.
6. Normalization of item IDs, bundle IDs, domain identifiers, protection classes, dates, snippets/text, ranking fields, and app attribution.
7. iOS Investigation View outputs for communications, app documents, Safari/web, cloud apps, app bundle/domain pivots, and deleted/index-only content.
8. Later active-vs-indexed comparison against iOS filesystem inventory.

## Guardrails

- Do not claim deleted/missing status until there is a reliable filesystem inventory for the same extraction.
- Preserve raw source fields and decode errors.
- Keep the iOS parser separate from macOS Store-V2 code paths unless shared utility functions are clearly safe and generic.

## V0_9_1 status

The iOS app-database path has moved beyond table/count inventory. The build now stages known SQLite app databases from the iOS FFS ZIP and attempts read-only generic row parsing for high-value table families, producing `ios_app_parsed_records.csv` and `ios_app_parsed_record_summary.csv`.

This is still a correlation-assist layer, not final app-specific interpretation. The next highest-value work is exact app schema handling for SMS/Messages, CallHistory, Safari History, Calendar, Contacts, and major third-party chat/cloud apps, followed by defensible Spotlight-to-filesystem/app-row correlation.


## V0_8_59 intake/parser starter

The first implemented iOS path is focused-folder ingestion from FFS ZIP extractions. Large FFS ZIPs are enumerated and selectively extracted with 7-Zip instead of the .NET ZIP central-directory API because the provided 12 GB and 35 GB Zip64 samples returned zero entries through the .NET path.

Workflow:

1. Run `tools/Extract-iOSCoreSpotlightFromFFSZips.ps1 -ZipFolder T:\iOS_Extraction` to extract only `private/var/mobile/Library/Spotlight/CoreSpotlight` and `BundleInfo` content.
2. Run `tools/Run-iOSCoreSpotlightFocusedAndZip.ps1 -InputRoot <focused_sample_folder>` to execute diagnostics and create a thin upload package.
3. Review `store_selection.csv`, `native_decode_attempts.csv`, `raw_failures_sample.csv`, and timeline/artifact samples.

Current parser status: the existing Store-V2 native parser can parse iOS `index.spotlightV2/store.db` records at a header/core-probe level. This is not yet a full iOS CoreSpotlight structured-value parser, and property/category dictionaries are still usually zero in the tested samples.

Next parser work: decode iOS CoreSpotlight metadata values beyond header/core-probe rows, incorporate BundleInfo app identity context, and preserve protection-class labels as first-class review columns.

## V0_8_59 iOS CoreSpotlight starter

The first iOS implementation path accepts focused CoreSpotlight folders extracted from FFS ZIPs. Full FFS ZIPs can be large and may not enumerate through .NET ZIP APIs, so the workflow uses 7-Zip central-directory/listing and focused extraction before parser execution.

### Current validated sample structure

The uploaded focused evidence contains two samples:

- `1f1ce9a08328644d471f4f90ae79ef81c4e22164_files_full` with populated `NSFileProtectionComplete`, `NSFileProtectionCompleteUnlessOpen`, and `NSFileProtectionCompleteUntilFirstUserAuthentication` CoreSpotlight indexes.
- `EXTRACTION_FFS` with populated `NSFileProtectionComplete`, `NSFileProtectionCompleteUnlessOpen`, `NSFileProtectionCompleteUntilFirstUserAuthentication`, `NSFileProtectionCompleteWhenUserInactive`, `Priority`, and SpotlightKnowledge-related files.

### V0_8_59 parser requirement

Each `CoreSpotlight/<protection-class>/index.spotlightV2` directory must be treated as a separate logical store group. Grouping only by the parent folder name `index.spotlightV2` is incorrect because it collapses unrelated protection classes and samples.

### Near-term next steps

1. Parse the focused ZIP/folder with `--profile ios --decode-core-native-values`.
2. Compare store discovery, store selection, raw record counts, parser failures, and high-value field coverage by protection class.
3. Add an iOS-specific CoreSpotlight parser route only if the existing Store-V2 parser cannot parse enough of the focused `.store.db` data.


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

## V0_9_60 iOS CoreSpotlight update

- Added bounded `bplist00` ASCII/UTF-16BE object-string extraction for CoreSpotlight bplist/NSKeyedArchiver context rows.
- Added KnowledgeC/CoreDuet database identification and support-mode parser scaffolding for `/app/inFocus`, `/document/open`, and `/app/intents` streams.
- Added iOS GUI/export surfaces for KnowledgeC summaries/events and investigator time-anomaly triage.
- Normal mode remains compact and Spotlight-first; support/full profiles should be used for broader app database materialization.

