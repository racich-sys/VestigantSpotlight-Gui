## V0_9_27 - record-centric iOS Spotlight communications review

- Reviewed V0_9_26_1 thin output: CLI run completed successfully with 344,445 raw iOS Spotlight records, 982,230 compact key/value rows, 336,037 compact date candidates, and complete_success.
- Incorporated V0_9_26_2 GUI C2026 oversized SQL literal fix as the base for V0_9_27.
- Added record-centric iOS Spotlight communication review views/exports that pivot compact Spotlight fields into investigator columns for Messages/SMS/RCS/iMessage, Mail/email, phone/FaceTime calls, WhatsApp/Signal/Telegram context, contacts, calendar/invitations, and URL/web context.
- Added attachment/media-focused Spotlight reference review for communications.
- Added communication summary output and upload samples so investigators can start from compact high-value communication surfaces instead of broad raw key/value tables.
- Preserved compact normal iOS mode; full FFS/app DB materialization and full native DB persistence remain support/diagnostic options only.

## V0_9_26_2 - MSVC oversized SQL literal compile fix

- Fixed Windows/MSVC compile failure `src\\db\\case_db.cpp(...): error C2026: string too big, trailing characters truncated` by splitting the affected SQL raw-string block into smaller runtime-joined fragments.
- Preserved V0_9_26 SQL/view behavior and chat-app attribution changes.
- Added validation note that all raw SQL literal fragments in `case_db.cpp` should remain below conservative MSVC-safe size thresholds.

## V0_9_26_2 - chat-app attribution refinement and repeatable thin-review workflow

- Reviewed V0_9_25 build/thin output before patching: build completed, run reached complete_success, raw_records=344445, raw_key_values=982230, raw_date_candidates=336037.
- Refined iOS Spotlight text-context classification to separate explicit chat-app bundle/domain/external-id attribution from plain text/link mentions. This avoids false positives such as location names containing “Signal” being promoted as Signal app evidence.
- Added `classification_evidence` to Spotlight text-context review and priority summary views.
- Added `vw_ios_spotlight_chat_app_attribution_summary` plus export/sample `ios_spotlight_chat_app_attribution_summary.csv` / `_sample.csv`.
- Added GUI view `iOS - Spotlight Chat App Attribution Summary`.
- Tightened generic human-text category matching for WhatsApp/Signal/Telegram so app-name keywords alone do not inflate chat-app categories.
- Preserved compact normal iOS mode, parser limits reporting, and Spotlight-first review behavior.
- Documented the repeatable build-log/thin-upload review method so future cycles can reuse the same process.

## V0_9_25 - high-value Spotlight text triage and cleaner thin uploads

- Reviewed V0_9_24 build/thin results: Windows/MSVC build completed, run reached complete_success, and parser limits report was present.
- Added priority/category scoring to same-record iOS Spotlight text context so communications, email, chat, URL, call/contact, and calendar contexts surface before generic text.
- Added GUI views `iOS - High-Value Spotlight Text Context` and `iOS - Spotlight Text Context Priority Summary`.
- Added exports/samples `ios_spotlight_high_value_text_context_review_sample.csv` and `ios_spotlight_text_context_priority_summary.csv`.
- Fixed thin upload packaging to avoid recursively including prior `Upload` folders and to downsample oversized generic upload_samples while preserving high-value samples.
- Kept normal iOS mode compact; full native/dbStr/property and broad FFS/app DB materialization remain diagnostic/support options.
- Validation: Linux build/self-test attempted in this environment; Windows/MSVC validation required.

## V0_9_24 - parser limits transparency and Spotlight text-context review

- Reviewed the V0_9_23_1 thin upload: the run completed successfully and the DB/export-stall issues remained resolved.
- Added `parser_limits_and_suppression_summary.csv`, a machine-readable report that explains native record limits, key/value persistence suppression, date candidate reduction, thin-upload sampling, FFS/app DB materialization status, and DB/WAL guardrail settings for each run.
- Persisted relevant run options into `case_info` so later validation can distinguish unlimited native record parsing from deliberately compact raw key/value/date persistence.
- Added `iOS - Spotlight Text Context Review` and `ios_spotlight_text_context_review_sample.csv` so investigators can directly review compact same-record Spotlight text retained in normal iOS mode.
- Added the same text-context sample to thin upload packaging.
- Added a limits/suppression section to `CASE_REVIEW_SUMMARY.txt` and updated the upload README/review index to point to the new report.
- Preserved the compact Spotlight-first default profile: full native/dbStr/property persistence, broad app DB materialization, and full FFS inventory exports remain explicit support/diagnostic choices.
- Linux validation: build completed, CLI version returned `Vestigant Spotlight v0.9.24`, and self-test passed. Windows/MSVC validation remains required.

V0.9.22 update: V0_9_21 completed successfully without DB/WAL or export stall. The object/inode diagnostic showed one Spotlight record per object for the test stores, so the remaining issue was not same-inode record splitting. V0_9_22 keeps compact normal mode but adds a slim iOS FFS path lookup table so Missing From FFS views can make real present/missing determinations without materializing full FFS inventory rows. It also adds compact Missing From FFS summary/sample exports and improves GUI status wording so missing-file candidates include Spotlight text context and explicitly report whether the FFS path lookup was available.

## V0_9_22
- Added `ios_ffs_path_lookup`, a slim path-only FFS lookup populated from reuse cache/CSV when full FFS inventory is not materialized.
- Updated Missing From FFS views to use either full FFS inventory or the slim lookup and to avoid false missing classifications when no lookup is available.
- Added `ios_spotlight_missing_from_ffs_summary.csv` as a normal compact export and a bounded `ios_spotlight_missing_from_ffs_candidates_sample.csv` upload sample.
- Added GUI view `iOS - Spotlight Missing From FFS Summary`.
- Preserved Spotlight-first compact mode and support/diagnostic-only full row exports.
- Added V0_9_22 concrete build/reuse-cache/state scripts.

V0.9.21 update: V0_9_20 stopped-state review showed the database was no longer the primary failure; the run stalled during normal export SQL materialization at ios_spotlight_record_review.csv after completing ios_spotlight_object_inode_summary as a large split export. V0_9_21 moves full broad per-record/per-object Spotlight exports to support/diagnostic mode by default, adds compact ios_spotlight_object_inode_diagnostic_summary.csv for object/inode record-splitting analysis, adds object-key indexes, and fixes raw_key_values-to-raw_records human-text mapping so multiple raw records sharing the same object key do not multiply Spotlight text rows during review views.

## V0_9_21
- Normal iOS investigator export now avoids full ios_spotlight_record_review/date_provenance/investigative_items/object_inode_summary materialization.
- Added compact object/inode diagnostic summary to evaluate whether repeated raw records represent the same object/inode.
- Added preferred raw_record_id mapping in human text views to avoid cross-multiplying same-object raw_key_values.
- Added V0_9_21 concrete build, reuse-cache, packaging, and stopped-state scripts.
- Linux validation: build and self-test passed; Windows/MSVC validation pending.



V0.9.20 update: V0_9_18 validation showed the 5 GiB SQLite guardrail fired at 150,000 parsed items with ~1.4M raw_key_values and ~463k raw_date_candidates, confirming remaining bloat was still raw table materialization. V0_9_20 further reduces normal iOS CoreSpotlight raw persistence to reference-only key/value rows, suppresses Last_Updated date candidate rows in normal mode because raw_records.last_updated_utc already preserves that timing, propagates SQLite guardrail aborts as fatal instead of continuing after a failed guardrail, and adds compact same-record Spotlight text context to the iOS Missing From FFS Candidates GUI/view where available.

## V0_9_20

- Fixed the V0_9_17 iOS CoreSpotlight normal-mode DB/WAL bloat class by making default native key/value and date-candidate persistence compact and Spotlight-investigator-focused.
- Added stricter iOS raw_key_values filtering, bounded per-record date provenance, suppression of derived/ranking/component date candidates, and support-only raw usage/detail exports.
- Added SQLite DB/WAL guardrails, parser heartbeat progress, periodic transaction commits, and WAL checkpoint/truncate attempts during long native parses.
- Added safer diagnostic-full-native behavior: full raw native DB persistence remains explicit and defaults to a bounded record sample unless --max-native-records is explicitly set.
- Added V0_9_20 build/run/collection scripts with concrete standard paths.

## V0_9_17 - iOS CoreSpotlight output and database bloat control

V0_9_17 fixes the V0_9_16 large-case regression where successful iOS dbStr/property decoding generated too many low-level raw key/value rows and massive support CSVs during normal investigator/reuse-cache runs. Normal iOS runs now keep high-value raw key/value rows by default, preserve date candidates for provenance, and move full native/dbStr/support exports behind explicit diagnostic/support modes. Added `--diagnostic-full-native-db` for bounded support runs that intentionally need full native key/value persistence.

## V0_9_17 validation status

Validated in sandbox: parser/database/export/app syntax checks, Linux build, CLI version check, Linux self-test, and SQL raw-string safety scan. Not validated in sandbox: Windows/MSVC build, Win32 GUI launch, and full large-case reused-cache runtime.

## V0_9_15 - Spotlight entity review, parser-target triage, and V0_9_11_1 GUI fix carry-forward

- Reviewed V0_9_11 reused-cache thin upload: CLI run completed successfully with 344,445 Spotlight records, 21,472 recovered human-text/key-value rows, 344,445 Spotlight date candidates, and V0_9_11 Spotlight-first timeline/reference exports.
- Carried forward V0_9_11_1 GUI compile fix (`execGuiSql` in GUI SQL helpers) so CLI, tests, and GUI should all build from this baseline.
- Added `iOS - Spotlight Entity Review` / `ios_spotlight_entity_review.csv`, a Spotlight-first normalized entity view that classifies recovered CoreSpotlight text as URL/web, file/attachment, account/email, communication-app, message/communication text, or other Spotlight text.
- Added `iOS - Spotlight Entity Summary` / `ios_spotlight_entity_summary.csv`, grouped counts by entity type, source store, source field, date semantic class, and supporting FFS/app context.
- Added `iOS - Spotlight Native Parser Targets` / `ios_spotlight_native_parser_targets.csv`, a focused target list for deeper native CoreSpotlight decoding: records without recovered text and generic `__native_core_probe_string_*` fields needing property-name mapping.
- Reuse-cache development scripts continue to pass `--skip-container-hash` by default. Full source hashing remains available for final/forensic runs.
- Legacy V7 import remains compiled but is still deprecated/hidden from normal workflow; physical removal remains a separate cleanup after the active iOS Spotlight review path remains stable.

## V0_9_11_1 validation status

Packaging validation: checked SQL helper usage and raw string segment safety. Windows/MSVC build remains required.

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

- Removed silent `LIMIT 5000` caps from several normal/core exports. Thin upload and `exports/upload_samples` files may still be bounded, but normal investigation exports should be full/chunked rather than silently truncated.

# V0_9_6 update

V0_9_6 adds reusable iOS source-intake/cache support for very large iOS FFS ZIP testing. Use `--reuse-ios-cache <completed-case-folder>` or, in the GUI, select `iOS/CoreSpotlight` and place the prior completed case folder in the `Evidence root / iOS cache case` field. This is intended for parser/enrichment iteration against the same source ZIP after a successful baseline run such as `Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4`. Reuse mode writes `source_cache_manifest.json` and `logs\ios_reuse_cache.log` and avoids re-listing/re-extracting the 250+ GiB ZIP.

# V0_9_6 Large iOS ZIP staging update

- Active baseline reset from user-uploaded V0_9_3.
- V0_9_6 addresses apparent hangs during stage_zip_source on very large iOS FFS ZIP files by streaming 7-Zip inventory output to CSV and writing heartbeat/progress logs.
- New runtime diagnostics: logs/ios_zip_stage_heartbeat.log and logs/ios_zip_inventory_progress.tsv.
- Targeted app database extraction now starts before the full FFS inventory stream and duplicate extraction is skipped.

# Validation Status - V0_8_71

Validated in the build sandbox:

- Linux CMake build: passed.
- CLI version check: `Vestigant Spotlight v0.8.71`.
- Linux self-test: passed.
- Bounded iOS diagnostics run with `--max-native-records 1000`: passed and generated `exports/ios_*` files.

Requires user Windows validation:

- MSVC build using `build_windows_msvc.bat`.
- Full APFS/AFF4 external compare against the active AFF4 image.
- Full iOS CoreSpotlight focused ZIP run without sandbox timeout.

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
