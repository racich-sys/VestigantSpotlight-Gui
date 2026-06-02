# Vestigant Spotlight Consolidated Version History

Current version: 0.9.42

This is the maintained version history for the production package.  It intentionally replaces many separate per-version note fragments with one chronological history, while preserving the important historical development process.  V0_9_37 restored and summarized historical details from the uploaded V0_9_3 documentation archive (`Docs.zip`) after V0_9_34 cleanup removed too much detail from the consolidated history.

## Documentation policy going forward

- Keep one primary user manual: `docs/CONSOLIDATED_USER_MANUAL.md`.
- Keep one primary version history: this file.
- Keep one active roadmap/continuation file: `docs/PROJECT_ROADMAP_AND_CONTINUATION.md`, with detailed testing-source/AFF4 notes in `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md`.
- Avoid reintroducing many root-level `V0_*_CHANGE_VALIDATION_NOTE.txt` fragments into the production ZIP.
- Preserve the historical substance by aggregating it here.

## Major development phases

### Phase 1 - iOS CoreSpotlight and app-context foundation, V0_9_0 through V0_9_4

The V0_9 branch began with iOS CoreSpotlight and iOS full-filesystem ZIP workflows producing a usable baseline.  Early runs processed 5 valid iOS CoreSpotlight stores, 37,719 raw Spotlight records, roughly 407,826 iOS FFS inventory rows, about 2,898 app database inventory rows, 32 extracted/opened app databases, and 109,592 parsed app database records.  The early dataset included SMS.db but the live message/chat/attachment/handle tables were empty; no actual WhatsApp ChatStorage.sqlite / ContactsV2.sqlite / WhatsApp CallHistory.sqlite was present.

### Phase 2 - Large iOS ZIP staging and reuse-cache performance, V0_9_4 through V0_9_6

The project then targeted the large iOS WhatsApp FFS ZIP workflow.  It moved from in-memory 7-Zip entry enumeration toward streaming ZIP inventory, heartbeat/progress logs, targeted CoreSpotlight/app database extraction, and reuse-cache acceleration.  V0_9_6 introduced cache-SQLite fast-path import and streaming CSV fallback so reuse-cache tests did not need to re-import very large FFS/app inventory CSVs into memory.

### Phase 3 - Spotlight-first iOS GUI and review workflow hardening, V0_9_10 through V0_9_15

After reuse-cache reliability improved, the iOS GUI was refocused on Spotlight/CoreSpotlight review rather than broad FFS/app database inventory.  Default iOS views were reduced and ordered around Spotlight summaries, entities, dates, FFS overlap, parser targets, file/URL/account references, referenced paths, missing-FFS candidates, and parser diagnostics.  GUI interaction was tuned for large tables by reducing default page sizes, avoiding expensive initial sort expressions, and using the help panel rather than intrusive balloon tooltips.

### Phase 4 - iOS dbStr/native decoding and DB/WAL bloat control, V0_9_17 through V0_9_23_1

V0_9_17 introduced clean-room C++ iOS CoreSpotlight dbStr map loading for `index.spotlightV2` folders, treating the full Spotlight folder as the evidence unit rather than `store.db` alone.  It attempted dbStr-1, dbStr-2, dbStr-4, and dbStr-5 map loading for property/category/index dictionaries.  That improved decoding directionally but led to massive DB/WAL and CSV growth in V0_9_15 through V0_9_17.  V0_9_18 through V0_9_23_1 corrected this by making normal iOS investigator mode compact, filtering broad raw native/dbStr/property persistence, moving heavy exports to diagnostic/support modes, adding guardrails, adding slim FFS lookup, and prioritizing high-value Missing From FFS review.

### Phase 5 - Investigator-facing iOS Spotlight review, V0_9_24 through V0_9_32

Once bloat and export stalls were under control, development focused on making Spotlight evidence review usable: parser limits/suppression reporting, text context review, high-value text prioritization, chat-app attribution, communication/message/media views, message body extraction, parser diagnostics, case provenance, normalized timelines, Plaso/L2T timeline sample exports, investigator overview, direct user message review, direct message thread summary, timeline month summary, and case quality dashboards.

### Phase 6 - Documentation/roadmap/package consolidation, V0_9_33 through V0_9_37

V0_9_33 added a detailed roadmap and testing-source timeline.  V0_9_34 performed the first clean production-package pass, removing stale fragments and old scripts, but compressed too much historical detail.  V0_9_35 restored the historical development story into this single consolidated history while keeping the production package clean.  V0_9_36 fixed a Missing From FFS export-schema mismatch while retaining the V0_9_35 documentation repair.  V0_9_37 improves Missing From FFS text visibility.

---

## Chronological version history

### V0_9_42

- Reviewed V0_9_39 build/thin results. Windows/MSVC build succeeded and the iOS reuse-cache run reached `complete_success` with stable compact-mode counts.
- Added CSV export performance improvements using direct SQLite text pointers, streaming CSV escaping, and a 1 MiB output buffer to reduce allocations and small writes during large exports.
- Increased Windows sequential SHA256 file-read buffer to 4 MiB and non-Windows hashing buffer to 1 MiB.
- Improved the generated iOS FFS ZIP inventory workflow by dumping `7z l -slt` output to a raw text file and parsing it without an external-process PowerShell pipeline or per-line regex matching.
- Added `Run-V0_9_42-iOS-FreshZip-CLI-AndZip.ps1` for Stage B actual-source ZIP testing after the reuse-cache path validates.
- Preserved compact iOS Spotlight normal mode and deferred broad GUI/parser refactors that are not required for V1 usability.

### V0_9_39

- Reviewed V0_9_37 build/thin results.  Windows/MSVC build succeeded, but the iOS reuse-cache run failed during native parse at the SQLite 5 GiB guardrail.
- Determined the regression was caused by V0_9_37's larger same-record Spotlight text context budget rather than broad row-count expansion.
- Preserved Missing From FFS text visibility while restoring normal-mode context to 1,800 bytes / 8 fields / 320 bytes per field sample.
- Fixed fatal native guardrail propagation so size guardrail hits stop cleanly and do not continue into secondary `COMMIT` errors.
- Added V0_9_39 review notes and updated scripts/version metadata.

### V0_9_37

- Reviewed V0_9_36 build/thin results; Windows/MSVC build succeeded and the run reached `complete_success` with stable compact iOS counts.
- Increased compact same-record Spotlight text context retained for reference-bearing iOS records from 1,800 bytes / 5 fields to 4,096 bytes / 12 fields so Missing From FFS and message/media reports have more visible content.
- Added `vw_ios_spotlight_missing_from_ffs_text_detail` and `vw_ios_spotlight_missing_from_ffs_text_coverage_summary`.
- Added normal exports `ios_spotlight_missing_from_ffs_text_detail.csv` and `ios_spotlight_missing_from_ffs_text_coverage_summary.csv` so missing-from-FFS content review does not require manual SQLite searching.
- Made full Missing From FFS candidates and high-value candidates normal exports instead of support-only exports because the candidate set is the primary investigator surface for potential deleted/unresolved Spotlight-indexed data.
- Added GUI views for Missing From FFS text detail and text coverage.

### V0_9_36

- Built from the V0_9_35 documentation/history repair baseline.
- Reviewed V0_9_34 build/thin results and fixed `ios_spotlight_missing_from_ffs_summary.csv` export ordering that referenced a stale/missing `investigative_priority_sort` column.
- Synchronized the GUI fallback Missing From FFS SQL with the CaseDatabase-managed view shape.
- Preserved the V0_9_35 consolidated documentation/history work.

### V0_9_35

- Reviewed the user-uploaded historical `Docs.zip` from the V0_9_3 documentation set.
- Restored historical V0_9 development detail into this consolidated version history rather than reintroducing many stale per-version note fragments.
- Updated `docs/CONSOLIDATED_USER_MANUAL.md` to explain the documentation model, standard workflows, iOS review path, compact-mode interpretation, and where historical process information now lives.
- No parser, schema, GUI, export, or forensic interpretation behavior was intentionally changed from V0_9_34.

### V0_9_34

- Reviewed V0_9_33 build/thin results; Windows/MSVC build succeeded and iOS reuse-cache run reached `complete_success`.
- Corrected stale `VERSION` and `VERSION.txt` metadata.
- Performed the first safe production-package cleanup pass.
- Removed old root-level per-version fragments, old V0_7/V0_8 test-command fragments, and older version-specific PowerShell wrappers from the production ZIP.
- Kept current scripts plus generic utility/GitHub scripts.
- Added package cleanup summary/manifest and V0_9_34 review notes.
- Preserved parser behavior and compact normal iOS mode.

### V0_9_33

- Reviewed V0_9_32-related items together, including build log, thin upload, separately uploaded/replaced thin upload, and the roadmap/testing-source/AFF4-APFS request.
- Added `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md` and V0_9_33 review notes.
- Documented when to stay on reuse-cache testing, when to run fresh full iOS FFS ZIP testing, when to enable selective support/correlation materialization, when to test alternate iOS sources, and how to resume AFF4/APFS work.
- No major parser behavior change; this was a planning/documentation/test-strategy release.

### V0_9_32

- Reviewed V0_9_31 build/thin results; build and run completed successfully with 6 stores, 344,445 raw records, 982,230 compact key/value rows, and 336,037 compact date candidates.
- Added `iOS - Investigator Overview` and `ios_spotlight_investigator_overview.csv` as a start-here surface.
- Added direct user message review and direct user message thread summary for Apple Messages/SMS/RCS/iMessage text recovered from Spotlight compact context.
- Added timeline month summary for compact chronology triage.
- Added schema-smoke coverage for the new views.
- Preserved compact normal iOS mode.

### V0_9_31

- Reviewed V0_9_30 build/thin results; GUI/CLI/self-test built and the reuse-cache run completed.
- Replaced overly granular `ios_spotlight_message_contact_summary.csv` with compact bucketed summary output.
- Added bounded message contact/thread detail sample and message body focus summary.
- Added parser diagnostics action summary and compact case quality dashboard.
- Added Plaso/L2T-compatible timeline sample export.
- Continued incorporating useful DFIR suggestions without adding heavy backburner items such as Relativity load files or NSRL ingestion.

### V0_9_30

- Reviewed V0_9_29 build/thin results and kept stable compact iOS behavior.
- Consolidated stalled/scattered help into `docs/CONSOLIDATED_USER_MANUAL.md` and version history into `docs/CONSOLIDATED_VERSION_HISTORY.md`.
- Improved compact iOS Spotlight message/body review extraction from same-record text context.
- Added parser diagnostics detail sample so unsupported/unparsed native failures are visible at record/sample level.
- Preserved compact normal iOS mode.

### V0_9_29

- Reviewed V0_9_28 build/thin upload; build completed, GUI linked, reuse-cache run reached `complete_success`, and compact counts remained stable.
- Added schema/view smoke-test coverage for key iOS and diagnostics views.
- Added parser diagnostics summary, case provenance summary, normalized iOS Spotlight timeline, timeline anomaly summary, message body review, user-focus message review, message contact summary, and non-destructive noise-reduction summary.
- Preserved compact normal iOS mode and kept full FFS/app DB/native-property materialization in support/diagnostic paths.

### V0_9_28

- Fixed V0_9_27 Windows GUI compile failure caused by a duplicate SQL block in `win32_gui.cpp` referencing `execGuiSqlParts` outside its scope.
- Added iOS Spotlight Message Text Review and Message Media Review surfaces.
- Added investigator-visible text, message-domain/handle/chat context, and mail participant columns.
- Included `kMDItemAppEntityTitle` in title/text extraction.
- Separated message-adjacent media saved/shared from Messages from direct Apple Messages/SMS/RCS/iMessage records.

### V0_9_27

- Incorporated the V0_9_26_2 GUI SQL-literal splitting fix.
- Added record-centric iOS Spotlight communications investigator review, communication summary, and attachment/media reference review.
- Pivoted compact Spotlight fields into investigator-facing columns such as title/name, snippet, bundle ID, domain identifier, message service, account identifier, callback URL, content URL, attachment/media path, message identifier, mailbox/thread, saved-from-app, same-record Spotlight text context, and validation locator.

### V0_9_26_2

- Fixed Windows/MSVC GUI compile failure in `src/gui/win32_gui.cpp` caused by oversized SQL raw-string literals.
- Added `execGuiSqlParts(...)` runtime SQL assembly helper and split oversized GUI SQL fragments.
- Preserved V0_9_26_1 GUI review SQL/view behavior.

### V0_9_26_1

- Fixed Windows/MSVC compile failure in `src/db/case_db.cpp` caused by oversized SQL raw-string literals.
- Split the affected SQL into smaller runtime-joined fragments via `joinSql(...)`.
- Preserved V0_9_26 SQL/view behavior.
- CLI and self-test built after this fix; GUI still needed V0_9_26_2.

### V0_9_26

- Reviewed V0_9_25 build/thin output; run completed successfully and compact counts remained stable.
- Refined iOS Spotlight text-context classification so app attribution was stricter.
- Separated explicit chat-app evidence from plain keyword/link mentions.
- Added `classification_evidence` to Spotlight text-context views.
- Added `iOS - Spotlight Chat App Attribution Summary` and related exports.
- Added repeatable thin-upload review workflow documentation.

### V0_9_25

- Reviewed V0_9_24 build/thin results; build completed, run reached `complete_success`, and parser limits report was present.
- Added priority/category scoring to same-record iOS Spotlight text context.
- Added high-value Spotlight text context review and priority summary views/exports.
- Fixed thin upload packaging to avoid recursively including prior `Upload` folders and to downsample oversized generic upload samples while preserving high-value review samples.

### V0_9_24

- Added `exports/parser_limits_and_suppression_summary.csv` to explain native record/block limits, compact raw key/value/date persistence, full native DB/FFS/app DB materialization flags, thin-upload sampling, and ratio metrics.
- Persisted run options into `case_info`.
- Added same-record iOS Spotlight text context review.
- Added run-limits/suppression status to `CASE_REVIEW_SUMMARY.txt`.
- Made compact/even-looking counts explainable.

### V0_9_23_1

- Fixed V0_9_23 Windows/MSVC compile failure in `case_db.cpp` due to oversized SQL raw-string literals.
- Refactored oversized SQL literal sequences into smaller fragments passed through `joinSql(...)`.
- Preserved V0_9_23 SQL/view behavior.

### V0_9_23

- Added priority/category columns to Missing From FFS candidates.
- Added high-value Missing From FFS views and summaries.
- Normalized repeated slashes before FFS lookup.
- Fixed missing-row `ffs_lookup_source` reporting.
- Kept full Missing From FFS views while sorting high-value message attachments, content URLs, app documents, and media references ahead of thumbnail/cache noise.

### V0_9_22

- Added slim `ios_ffs_path_lookup` so Missing From FFS could use cached FFS path evidence without full FFS inventory materialization.
- Updated Missing From FFS logic to use full inventory when available or slim lookup otherwise.
- Prevented false missing classification when no FFS lookup exists.
- Added text context/status and lookup source/status fields to Missing From FFS views.
- Added missing-from-FFS summary and bounded candidate sample.

### V0_9_21

- Moved heavyweight `ios_spotlight_record_review.csv` and related object/date/reference exports out of normal investigator mode.
- Added compact object/inode diagnostic summary and GUI view.
- Reworked human-text and object/inode summaries to reduce multiplication across records/dates/references.
- Added indexes supporting object/inode grouping and record-order access.
- Confirmed the active issue had shifted from DB/WAL bloat to heavy SQL/view materialization.

### V0_9_20

- Diagnosed V0_9_19 no-writes/stall as export/query materialization rather than DB/WAL bloat.
- Reworked `vw_ios_spotlight_record_review` to remain one row per raw Spotlight record.
- Collapsed date provenance per record and removed default full FFS/app residency joins from record review.
- Added object/inode summary export to investigate possible same-object/inode splitting.
- Added SQL export heartbeat/progress using SQLite progress handler.

### V0_9_19

- Continued reducing normal-mode DB materialization after V0_9_18 still hit the SQLite guardrail.
- Stopped full FFS inventory and broad app DB parsed-record materialization by default.
- Kept a slim normalized-path FFS index for Missing From FFS / Spotlight-to-FFS correlation.
- Reduced normal `raw_key_values` to reference-only capped rows and suppressed duplicated `Last_Updated` date candidates.
- Added explicit support/diagnostic flags for materializing iOS FFS/app DB/support databases.
- Added same-record Spotlight text-context sample to Missing From FFS views.

### V0_9_18

- Responded to V0_9_17 DB/WAL bloat by disabling broad raw native/dbStr/property materialization by default for iOS CoreSpotlight normal mode.
- Added compact derived-row handling, stricter date-candidate reduction, DB/WAL guardrails, parser heartbeat/progress, and WAL checkpoint/truncate controls.
- Moved large raw/support exports to diagnostic/support modes.
- Added `Collect-V0_9_18-DBBloat-State.ps1`.

### V0_9_17

- Introduced clean-room C++ dbStr map loading for iOS CoreSpotlight `index.spotlightV2` folders, using public Apple Core Spotlight docs for semantic taxonomy and Yogesh Khatri's work as reference only.
- Attempted to load dbStr map data/offset/header files for property, category, and index dictionaries.
- Added dbStr map inventory, dictionary coverage, and Apple field coverage views/exports.
- Added GUI views for dbStr/dictionary/Apple-field coverage.
- Subsequent V0_9_17/V0_9_18 review found this increased raw key/value/date materialization too much for normal mode.

### V0_9_16 / V0_9_15

- V0_9_15 introduced directional iOS dbStr/property decoding but caused massive expansion: tens of millions of raw key/value and date/export rows, very large SQLite DB/WAL files, and long exports.
- V0_9_16 reduced some exports but still created large DB/WAL files.
- These versions established the need for compact normal iOS mode and explicit diagnostic full-native modes.

### V0_9_14 / V0_9_13

- Reviewed V0_9_12/V0_9_13 build/thin outputs and GUI full-intake behavior.
- Hardened Visual Studio discovery and reduced noisy batch-label failures.
- Deferred or skipped source/container hashing by default for large iOS development/reuse-cache runs while preserving opt-in full hash workflows for final/forensic runs.
- Improved GUI iOS review responsiveness with faster preview ordering and smaller default page sizes.
- Hid/suppressed legacy V7 exports from normal upload samples.
- Focused the iOS Investigation default view list on Spotlight/CoreSpotlight review and direct FFS overlap.
- Replaced intrusive balloon tooltips with help-panel updates and reduced initial page size.

### V0_9_11_1

- Fixed V0_9_11 GUI compile issue caused by split GUI SQL blocks using the database helper name `exec(...)` instead of the GUI helper `execGuiSql(...)`.
- Preserved Spotlight-first iOS review views and exports.
- Preserved reuse-cache scripts with `--skip-container-hash` by default.

### V0_9_10 / V0_9_12

- Uploaded historical notes indicate that V0_9_10 and V0_9_12 build/thin uploads completed successfully.
- V0_9_12 generated Spotlight-first outputs including Spotlight Entity Review/Summary and parser-target exports.
- No standalone detailed V0_9_10 or V0_9_12 change note was present in the uploaded V0_9_3 documentation archive.

### V0_9_6

- Reviewed V0_9_5 build/runtime results; V0_9_5 successfully reused the V0_9_4 large iOS FFS cache but still had a long inventory import gap and high memory pressure.
- Added cache-SQLite fast path for `--reuse-ios-cache`, attaching the prior cache case database and copying inventory rows directly.
- Added streaming CSV fallback when cache SQLite is missing/incompatible.
- Added status/progress messages for cache DB import and streaming fallback.
- Preserved cache provenance and manifest linkage.

### V0_9_5

- Uploaded historical notes indicate V0_9_5 built successfully and reused the V0_9_4 large iOS FFS cache.
- The key observed limitation was slow in-memory re-import of large cached inventory CSVs, which drove V0_9_6.
- No standalone detailed V0_9_5 change note was present in the uploaded V0_9_3 documentation archive.

### V0_9_4

- Large iOS FFS ZIP staging no longer built a complete in-memory `ArrayList` of every 7-Zip entry before writing FFS inventory.
- Streamed `7z l -slt` output into FFS/app database inventory CSVs with progress flushes.
- Added heartbeat/progress logs for large ZIP staging.
- Started targeted app database extraction before full FFS inventory stream and skipped duplicate extraction.
- Added unified iOS communications review records/summary and Spotlight communication candidates.
- Improved WhatsApp database status so no-WhatsApp datasets report explicit `WHATSAPP_DB_NOT_FOUND`.
- Tightened keychain material reporting and separated keychain support references.
- Added contact identity, web history, calendar review, and unified keyword search surfaces.

### V0_9_3 / V0_9_2

- V0_9_2 review reached `complete_success` on 5 valid iOS CoreSpotlight stores with 37,719 raw records, 407,826 FFS inventory rows, 2,898 app DB inventory rows, 32 opened app DBs, and 109,592 parsed app DB records.
- Communications review contained Phone/FaceTime Call History records in the dataset.
- SMS.db existed but live message/chat/attachment/handle rows were empty.
- WhatsApp database status correctly reported no WhatsApp database found.
- V0_9_3/V0_9_4 planning focused on adding contact identity, web history, calendar, unified keyword search, and better communications/keychain review surfaces.

### V0_9_1

- Reviewed V0_9_0 thin iOS GUI upload; V0_9_0 had reached `complete_success`.
- Added schema-aware iOS WhatsApp parser hooks using uploaded WhatsApp/iLEAPP references as schema references.
- Targeted WhatsApp iOS table families such as ZWAMESSAGE, ZWAMEDIAITEM, ZWACHATSESSION, ZWAADDRESSBOOKCONTACT, ZWAPROFILEPUSHNAME, ZWAGROUPMEMBER, and call-related ZWA tables when present.
- Added WhatsApp database status, parsed-record, and parsed-summary views/exports.
- Added keychain/keybag inventory context without claiming decryption or key extraction.
- Improved app database discovery/classification to avoid treating non-database WhatsApp-named assets as WhatsApp databases.

### V0_9_0

- Baseline iOS GUI/thin run for the V0_9 branch.
- Reached `complete_success` in the uploaded thin bundle.
- Established the iOS case metrics that drove V0_9_1: 5 valid stores, 37,719 raw records, 15,104 raw key/value rows, 407,826 FFS inventory rows, 2,899 app database inventory rows, 28 opened extracted databases, and 109,592 parsed app records.
- SMS.db was present but live SMS tables were empty in the dataset.

## Earlier macOS/AFF4/APFS and V0_7/V0_8 work

The earlier V0_7 and V0_8 work established the macOS Store-V2 parser, AFF4/APFS staging/external compare direction, raw evidence-source staging, ZIP intake, GUI investigation tabs, thin upload bundles, and validation workflow.  Those old fragments are no longer kept as separate production-package documents, but the active roadmap preserves the remaining macOS/AFF4/APFS work:

- continue native Store-V2 decoding for macOS and iOS;
- preserve source provenance and validation locators;
- resume AFF4/APFS image enumeration/extraction after iOS investigator views remain stable;
- compare extracted Store-V2 folders against external reference extractions where available;
- keep active-file comparison secondary to the main Spotlight/CoreSpotlight evidence review goal.

## V0_9_42 - Native C++ 7-Zip inventory parser

V0_9_42 reviewed the successful V0_9_41 reuse-cache run and carries forward the V1-readiness performance work. The CSV exporter fast path remains in place. The iOS focused ZIP workflow now lets 7-Zip dump `-slt` output to raw text and then rebuilds FFS/app database inventory CSVs using native C++ parsing rather than the PowerShell raw-listing parser. This is intended to make the Stage B fresh-ZIP test faster and closer to the 60-120 MB/s target where hardware permits.
