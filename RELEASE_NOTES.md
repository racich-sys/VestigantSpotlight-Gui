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

## V0_9_11_1 - GUI compile helper fix

- Fixed GUI MSVC compile failure where split SQL blocks in `src/gui/win32_gui.cpp` called `exec(...)` instead of the GUI helper `execGuiSql(...)`.
- Preserved V0_9_11 Spotlight-first iOS review views and reuse-cache workflow.

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

## V0_9_11_1 - Spotlight date provenance review

- Added iOS Spotlight date provenance view/export so each Spotlight/CoreSpotlight record can be reviewed with the raw date source field, raw value, parse method, date type, and validation hint.
- Added date provenance columns to the Spotlight-first record review surface.
- Added the new date provenance CSV to normal exports and thin-upload required export collection.
- Reuse-cache development scripts continue to skip full source-container hashing by default for large validated FFS ZIPs.
- Legacy V7 import remains present but marked for later removal after the active Spotlight/CoreSpotlight review workflow stabilizes.

## V0_9_11_1 - Runtime SQL split fix after V0_9_7_2 thin upload

- Fixed V0_9_7_2 runtime failure during `open_sqlite`: SQLite received a literal `R"SQL(` marker from the prior raw-string split and failed near `R`.
- Removed nested raw-string delimiter text from the generated SQL blocks in `src/db/case_db.cpp` and `src/gui/win32_gui.cpp` while preserving the Spotlight-first iOS review views.
- Verified no raw SQL segment in `case_db.cpp` or `win32_gui.cpp` exceeds 7,000 characters and no raw SQL segment content contains embedded `R"SQL(` delimiter text.
- Reuse-cache development/test scripts continue to pass `--skip-container-hash` by default.
- Legacy V7 import remains deprecated and hidden from normal workflow. Removal is planned after the current iOS Spotlight/CoreSpotlight review path is stable enough that schema/enrichment dependencies can be safely removed.

## V0_9_11_1 - GUI MSVC string-literal fix and reuse-cache script hash skip

- Fixed MSVC GUI build failure `src\gui\win32_gui.cpp(2580): error C2026: string too big` by splitting oversized embedded SQL raw string literals.
- Proactively split large SQL raw string literals in `src\db\case_db.cpp` and `src\gui\win32_gui.cpp` below the MSVC-sensitive range.
- Updated generated reuse-cache CLI packaging script to pass `--skip-container-hash` by default for development/test reuse-cache runs.
- Reviewed V0_9_7_1 thin upload: run completed successfully; Spotlight-first exports were generated; thin upload sampling is explicitly marked and full local CSVs remain in the case folder.


## V0_9_11_1 - Spotlight-first iOS review and decode coverage focus

- Added Spotlight-first iOS review views and exports so investigator review starts with CoreSpotlight records, recovered text values, decode coverage, field coverage, and decode gaps.
- Added Spotlight decode coverage summary, field coverage summary, human text category summary, record review, and decode-gap views.
- Fixed the iOS keyword surface source label typo from CORESPOPTLIGHT_TEXT to CORESPOTLIGHT_TEXT.
- Reaffirmed that FFS/app-database correlation is supporting context; the primary evidence surface is Spotlight/CoreSpotlight parsing and review.

# V0_9_6 update

V0_9_6 adds reusable iOS source-intake/cache support for very large iOS FFS ZIP testing. Use `--reuse-ios-cache <completed-case-folder>` or, in the GUI, select `iOS/CoreSpotlight` and place the prior completed case folder in the `Evidence root / iOS cache case` field. This is intended for parser/enrichment iteration against the same source ZIP after a successful baseline run such as `Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4`. Reuse mode writes `source_cache_manifest.json` and `logs\ios_reuse_cache.log` and avoids re-listing/re-extracting the 250+ GiB ZIP.

# V0_9_6 Large iOS ZIP staging update

- Active baseline reset from user-uploaded V0_9_3.
- V0_9_6 addresses apparent hangs during stage_zip_source on very large iOS FFS ZIP files by streaming 7-Zip inventory output to CSV and writing heartbeat/progress logs.
- New runtime diagnostics: logs/ios_zip_stage_heartbeat.log and logs/ios_zip_inventory_progress.tsv.
- Targeted app database extraction now starts before the full FFS inventory stream and duplicate extraction is skipped.

# Vestigant Spotlight Release Notes / Consolidated History

## V0_9_1

Focus: iOS Apple Messages/SMS.db parsing improvements and GUI review-view hover explanations.

Changes:
- Added mouse-hover explanations for the left-side review view list used by both MacOS Investigation View and iOS Investigation View. The help text explains what each view shows, key interpretation limits, and cautious residency/date language.
- Improved the Apple Messages/SMS.db parser path. Message rows now use a schema-specific join pattern across `message`, `handle`, `chat_message_join`/`chat`, `message_attachment_join`, and `attachment` when the tables are present, following the same high-level table-join approach used by iLEAPP.
- Added schema-specific SMS.db participant parsing for `handle` and `chat` tables.
- Reclassified Apple Messages join/support tables so `chat_message_join` and `message_attachment_join` are not misrepresented as standalone message records.
- Added cautious handling for recoverable/deleted Messages-related tables by classifying them as `MESSAGE_DELETED_OR_RECOVERABLE` for investigator review without asserting deletion.
- Updated scripts and documentation paths to V0_9_1 / `Q:\SpotlightCase\TestiOS_V0_9_1`.

Validation status in package: source-level/static checks only in this environment. Windows/MSVC build and GUI runtime validation must be run on the user's Windows workstation.

Expected validation indicators:
- `VestigantSpotlightCli.exe --version` reports `Vestigant Spotlight v0.9.1`.
- GUI launches and the MacOS/iOS review view list shows hover explanations.
- iOS GUI ingest completes against `T:\0202_0024-IT002\00008132-000269523699001C_files_full.zip`.
- `ios_app_database_record_inventory.csv` still reports SMS.db table counts.
- If a future SMS.db has live message/handle/chat/attachment rows, `ios_apple_messages_parsed_records.csv` shows `parsed_apple_messages_smsdb_message_joined`, `parsed_apple_messages_smsdb_handle`, or `parsed_apple_messages_smsdb_chat` rows.

## V0_8_99

Focus: iOS object-linking/export correctness after V0_8_98_1 GUI validation.

Changes:
- Fixed `vw_ios_spotlight_to_ffs_object_links` so it reports all resolvable iOS Spotlight path references and their FFS residency status, including `PRESENT_AS_FILE_IN_FFS` matches.
- Kept `vw_ios_spotlight_missing_from_ffs_candidates` limited to absent/unresolved path references only.
- Updated `vw_ios_spotlight_residency_summary` so file-presence counts come from the all-path object-link view, not only the missing-candidates view.
- Hardened iOS app-database extraction path handling by sanitizing invalid Windows path characters before checking extracted database paths. This prevents misleading `ios_ffs_inventory_error=Illegal characters in path` warnings when the inventory itself populated.
- No intended Store-V2 parser behavior change from V0_8_98_1.

Expected validation indicators:
- Windows/MSVC build completes.
- GUI iOS ingest completes.
- `ios_spotlight_to_ffs_object_links.csv` has nonzero rows.
- `ios_spotlight_to_ffs_object_links.csv` includes `PRESENT_AS_FILE_IN_FFS` rows when Spotlight path references match FFS ZIP entries.
- `ios_spotlight_missing_from_ffs_candidates.csv` remains limited to absent/unresolved path references.
- `ios_focused_zip_extract.log` should no longer contain a misleading `ios_ffs_inventory_error=Illegal characters in path` when inventory rows were written.

# V0_8_99 Build Script Hotfix

- Converted Windows batch and PowerShell scripts to CRLF line endings.
- Fixes Windows batch error: `The system cannot find the batch label specified - TryVsPath`.
- No intended parser, database, GUI, or export behavior change from V0_8_93_3.

# Vestigant Spotlight Release Notes / Consolidated History

## V0_8_99

Focus: GUI tab containment hotfix for iOS investigation views.

Changes:
- Added platform-aware review-view filtering in the Win32 GUI.
- The MacOS Investigation View no longer lists iOS-only views whose display names begin with `iOS`.
- iOS review buttons repopulate the visible view list in iOS mode before loading their target view, so iOS results remain inside the iOS Investigation View.
- Switching back to the MacOS Investigation View resets the visible review list to non-iOS/macOS views.
- No intended parser, database schema, or export behavior change from V0_8_93.



## V0_8_99

Focus: iOS parsed app database records upload visibility and review packaging after V0_8_90 GUI validation.

Changes:
- Fixed app-generated Upload bundle copy list so `ios_app_parsed_records.csv` and `ios_app_parsed_record_summary.csv` are copied into `Upload/exports` when generated.
- Preserved the V0_8_90 parsed app database row extraction behavior; no intended Store-V2 parser behavior change.
- Updated build/run/package scripts and documentation to V0_8_99 paths.

Validation target:
- Windows/MSVC build completes.
- GUI iOS ingest completes.
- `exports/ios_app_parsed_records.csv` has nonzero rows when parsed app DB rows exist.
- `Upload/exports/ios_app_parsed_records.csv` and `Upload/exports/ios_app_parsed_record_summary.csv` are present in the thin upload bundle.

Current limitation:
- Parsed app DB records remain generic/dynamic-schema extraction. Specific message/call/browser/application row parsers and record-level Spotlight-to-app-row matching remain future phases.

## V0_8_99

Focus: merge useful Codex V0_8_89_1 work while keeping the active iOS investigation path forward.

Changes:
- Added generic read-only parsed-row extraction for staged iOS app SQLite databases. New outputs include `ios_app_parsed_records.csv` and `ios_app_parsed_record_summary.csv`, with matching GUI views for Parsed App Records and Parsed App Summary.
- Added conservative app-database row extraction for high-value table families: messages, message attachments, participants, calls, web/Safari history, mail, calendar, contacts, and chat-style tables. Parsed rows are review leads, not proof of usage/deletion or exact Spotlight-to-row correlation.
- Improved iOS database residency candidates so they can report when parsed app records are available for a matching database family.
- Integrated Codex AFF4/APFS direct-reader progress as a guarded diagnostic path for BlackBag/LZ4 `aff4:DiscontiguousImage` layouts: pre-open AFF4 guard, direct AFF4 ZIP map/index/data reader, APFS container/volume/root-tree resolution, filesystem namespace seed outputs, and bounded Spotlight target/copy-attempt diagnostics where available.
- Preserved the main V0_8_89 iOS FFS ZIP inventory fixes: safe slash-based ZIP-entry parsing, 7-Zip `l -slt` inventory, sampled thin-upload CSV handling, and consolidated release/history docs.

Limitations:
- iOS app parsed rows are generic schema-driven extractions. App-specific deleted/live semantics, thread reconstruction, attachment reconstruction, and exact Spotlight-to-row matching remain future work.
- CoreSpotlight dbStr/property/category map decoding remains incomplete.
- AFF4/APFS direct traversal remains diagnostic/experimental. It can reach APFS metadata/root-tree/target-name levels on the supplied BlackBag/LZ4 AFF4 path, but the mature V0_8_74 copy-out/staging behavior still needs full direct-reader parity.

## V0_8_99

Focus: move the iOS app-database stage from table counts toward investigator-reviewable records while keeping conservative forensic wording.

Changes:
- Added generic read-only row parsing for staged iOS SQLite app databases when known high-value tables are present. Current target families include messages, message attachments, participants, calls, Safari/web history, mail, calendar, contacts, and chat-style tables.
- Added `ios_app_parsed_records.csv` and `ios_app_parsed_record_summary.csv`, plus matching GUI views, so investigators can review extracted app-database rows instead of only table/count inventory.
- Added `ios_app_parsed_records` to the case database with source database, table, category, timestamp, participant/contact, URL, title, file path, identifier, snippet, status, and provenance fields.
- Reduced `ios_database_residency_candidates.csv` expansion by aggregating database/table/parsed-record matches before export. This keeps the pivot useful on large full-file-system ZIPs without multiplying each Spotlight candidate by every matching table-count row.
- Thin upload packaging now includes the parsed app-record summary and samples the potentially large parsed-record CSV.

Expected validation indicators:
- Windows/MSVC build completes and the CLI reports `Vestigant Spotlight v0.8.99`.
- Self-test passes.
- Full iOS GUI ingest logs `parsed_app_records=<nonzero>` when supported staged app databases contain target rows.
- `ios_app_parsed_record_summary.csv` shows which databases/tables produced parsed rows.
- Database residency candidates may report `POTENTIAL_PARSED_APP_RECORDS_AVAILABLE`; this means relevant parsed app records exist for the database family, not that a specific CoreSpotlight string has been row-matched.

Current limitation:
- This is a generic, schema-tolerant parser pass. It does not yet implement app-specific deleted/live semantics, message-thread reconstruction, attachment file carving, or exact Spotlight-to-app-row correlation.

## V0_8_99

Focus: iOS investigation inventory reliability after V0_8_88_1 GUI validation.

Changes:
- Fixed the iOS FFS ZIP inventory failure seen in V0_8_88_1 where the generated extractor logged `ios_ffs_inventory_7z_list_error=Exception calling "GetFileName" ... Illegal characters in path` and therefore imported zero FFS/app database inventory rows.
- Replaced Windows `IO.Path.GetFileName()` parsing for ZIP entry names with safe slash-based ZIP-entry leaf/extension parsing. This avoids Windows path-character restrictions when reading iOS ZIP central-directory entries.
- Skips archive metadata records from `7z l -slt` before treating output as ZIP entries.
- Updated quick diagnostic PowerShell with the same safe ZIP-entry parsing logic.
- Updated thin upload packaging so very large iOS CSVs are sampled in the upload ZIP while full CSVs remain in the local case folder. This is intended to prevent the FFS inventory from making the upload bundle excessively large.
- No intended Store-V2 parser behavior change from V0_8_88_1.

Expected validation indicators:
- Windows/MSVC build completes.
- GUI iOS ingest completes.
- `ios_ffs_file_inventory.csv` has nonzero rows for the 39 GiB iOS FFS ZIP.
- `ios_app_database_inventory.csv` has nonzero rows if known app databases are present in the ZIP.
- `ios_app_database_record_inventory.csv` and `ios_app_database_record_summary.csv` populate table/count inventory for extracted SQLite databases that can be opened read-only.

Current limitation:
- V0_8_99 still performs table/count inventory for app databases. It does not yet parse message bodies, call rows, WhatsApp rows, Signal rows, Telegram rows, Safari history rows, or other content-level app database records.


## V0_8_99

Focus: make iOS Phase 2/6 inventory usable on large iOS FFS ZIPs and add first record-table inventory for local app databases.

Changes:
- Replaced ZIP64-sensitive .NET-only FFS ZIP inventory with a 7-Zip `l -slt` central-directory inventory path, with .NET fallback when 7-Zip is unavailable.
- Focused iOS ZIP staging now writes a full `ios_ffs_file_inventory.csv` from the FFS ZIP entry listing, not only from staged CoreSpotlight files.
- Known local app database candidates are extracted to `EvidenceStaging/ios_app_databases` for limited record-table inventory.
- Added `extracted_path` to `ios_app_database_inventory` so later parsing can operate on staged database copies rather than the source ZIP.
- Added table/view/export `ios_app_database_record_inventory.csv` for SQLite table names, row counts, sample columns, and table categories.
- Added table/view/export `ios_app_database_record_summary.csv` for summarized database-resident artifact families.
- Database residency candidates now distinguish database-family presence from the presence of relevant record tables.
- Updated quick iOS diagnostics to use the same 7-Zip inventory approach and to collect FFS/app-database inventory without requiring a full GUI ingest.
- Kept iOS views in the iOS Investigation tab and kept release/history documentation consolidated.

Limitations:
- V0_8_99 performs app database table/count inventory, not content-level parsing of SMS.db, CallHistory, WhatsApp, Signal, Telegram, Safari, or other app database rows.
- `POTENTIAL_RECORD_TABLE_PRESENT_IN_APP_DB` means a likely relevant table exists and has rows; it does not yet prove a specific Spotlight string corresponds to a specific live app database row.
- `SPOTLIGHT_ONLY_FILE_MISSING_OR_UNRESOLVED` means the normalized path was absent from the enumerated FFS ZIP inventory. It does not prove user deletion.
- CoreSpotlight dbStr/property-map decoding remains a future parser phase.

## V0_8_87

Added the first iOS investigation phase scaffolding: iOS tab routing cleanup, initial FFS ZIP inventory CSVs, local app-database inventory CSVs, Spotlight referenced-path extraction, missing-from-FFS candidates, and database-residency candidate views/exports. V0_8_87 validation showed CoreSpotlight parsing still worked, but FFS/app database inventory remained empty on the large iOS ZIP; V0_8_99 fixes that by using 7-Zip entry listing and staged database extraction.

## V0_8_86

Added iOS investigator pivots for protection class summary, artifact hint summary, and per-record investigation hints. Added corresponding GUI views, exports, upload packaging, and consolidated release/history documentation.

## V0_8_85_1

Compile hotfix for V0_8_85. Split oversized SQLite SQL raw string literals in `src\db\case_db.cpp` to avoid MSVC `C2026`.

## V0_8_85

Added iOS record string-probe summary export/view, corrected iOS store parse summary placeholder accounting, and removed caps from main iOS timeline and string-probe exports.

## V0_8_84

Changed iOS Store-V2 database selection to parse one primary database per CoreSpotlight group, preferring `store.db` while preserving `.store.db` alternates in inventory/hash outputs.

## V0_8_83

Fixed quick-diagnostic ZIP packaging issues and added redacted iOS investigation summaries/domain URL summaries and cleaner Upload packaging for iOS CSVs.

## V0_8_82

Consolidated release/history documentation and added fast iOS diagnostic collection intended to avoid full GUI ingest when only ZIP/CoreSpotlight inventory is needed.

## V0_8_81

Added iOS string-probe fallback preservation and improved iOS tab status reporting after focused CoreSpotlight parsing.

## Earlier V0_8.x summary

Earlier V0_8.x builds moved the project toward iOS CoreSpotlight ZIP intake, macOS AFF4/APFS readiness, external Store-V2 comparison, upload/thin-bundle workflow, and GUI parity. Legacy V7 import remains deprecated for normal workflows unless a current dependency requires it.

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

## v0.9.20

- Fixes V0_9_19 no-writes/stall failure class where export reached `ios_spotlight_record_review.csv` and did not complete.
- Reworks `vw_ios_spotlight_record_review` to stay one row per `raw_record_id`, collapse date provenance per record, and avoid full FFS/app-residency joins in the record-review export.
- Adds `vw_ios_spotlight_object_inode_summary` and `ios_spotlight_object_inode_summary.csv` to evaluate whether multiple Spotlight records share the same inode/object/store identifier.
- Keeps normal investigator mode Spotlight-first and moves full FFS/app correlation detail exports to support/diagnostic mode by default.
- Adds SQLite export heartbeat progress using `sqlite3_progress_handler` so long-running query execution writes progress before the first row is returned.
- Updates the iOS Missing From FFS GUI column registry to show same-record Spotlight text context/status.
- Replaces the V0_9_20 state collector ZIP creation path to avoid the non-fatal `Resolve-Path` warning seen in V0_9_19 state collection.
