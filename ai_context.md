# ai_context.md

## Project Overview
Vestigant Spotlight is a Windows C++ forensic parser and investigator GUI for macOS Spotlight Store-V2 and iOS CoreSpotlight/app database evidence, with AFF4/APFS and iOS FFS ZIP workflows. The project focus is now shifting from extraction stability to identity-centric communication, frequency, timeline, and usage attribution.

## Tech Stack
- C++17, CMake, MSVC on Windows 10/11 x64.
- Win32 GUI, SQLite, PowerShell 5.1-compatible build/test wrappers.
- Primary validation artifacts are Windows build logs plus iOS/AFF4 thin upload bundles.

## Current State
- Current version: V1.6.3.
- V1.5.0.2 iOS thin completed successfully with stable extraction, timeline promotion, usage evidence promotion, and export pipeline.
- V1.6.3 adds identity graph views/exports that link identity hints to threads, records, URLs, apps, record categories, and dated activity.
- Keychain material and protected-data candidate inventory exists as presence/correlation only; no decryption claims are made.

## Next Steps / Active Goal
Transform extracted iOS Spotlight/AppDB evidence into identity-centric investigative views: who/what identifier, what activity, which app, which thread/reference, first/last seen, and frequency. Continue modularizing schema/view code to avoid MSVC raw string failures and late export/view mismatches.

## Roadmap / Implemented History
- V1.4.x: iOS timeline and usage evidence promotion began working at scale.
- V1.5.0.2: stabilized identity rollup/thread activity matrix exports and release checks.
- V1.6.3: added identity graph summary, identity graph edges, and identity activity timeline views/exports. Added schema modularization plan and maintained raw string/version/wrapper release checks.

## Known Bugs
- `case_db.cpp` still contains too many schema/view definitions and should be modularized into `src/db/schema/*` modules.
- Identity graph output is correlation evidence and still requires investigator review before attribution.

## The "Do Not Do" List (Graveyard)
- Do not add large monolithic SQL/raw-string blocks to `case_db.cpp`; this repeatedly caused MSVC C2026. Split SQL and modularize schema files.
- Do not provide a release ZIP until version consistency, raw-string-size, wrapper compatibility, and exported-view query checks pass.
- Do not label missing native DB records as deleted/destroyed without corroboration. Use cautious wording such as "present in CoreSpotlight; not observed in parsed native database set."
- Do not rely on static logs alone for heartbeat; thin wrappers should report process CPU/RAM, DB/output growth, and last progress tail.


---

## Prior Context History

## Project Overview
Vestigant Spotlight / Spotlight2 is a Windows-first C++ forensic analysis application for extracting, parsing, staging, and reviewing macOS Spotlight Store-V2 and iOS CoreSpotlight/app-database evidence. The project prioritizes defensible provenance, investigator-facing SQLite/CSV/GUI review surfaces, and cautious interpretation rather than unsupported intent conclusions.

## Tech Stack
- Language: C++20.
- Primary target: Windows 10/11 x64, MSVC / Visual Studio 2022 toolchain.
- GUI: native Win32 API with ListView/RichEdit/common controls.
- Database: SQLite case database with generated views and CSV exports.
- Scripting: Windows PowerShell wrappers for build, AFF4/APFS thin runs, iOS CoreSpotlight thin runs, upload packaging, diagnostics, and GitHub setup.
- Build systems: CMake plus no-CMake MSVC batch wrappers.
- Evidence/container helpers: 7-Zip for iOS FFS ZIP inventory/extraction; AFF4/APFS reader/probe code in native C++ plus optional external AFF4 tooling where explicitly configured.
- Compression/codec support: Apple/lzfse optional integration path for LZFSE/LZVN; guarded decompression-size limits.

## Current State
- Current package: V1.6.3, based on validated V1.4.1/V1.4.0 iOS existence/frequency work plus continued roadmap implementation.
- Current functional milestone: V1.6.3 strengthens iOS investigator-value generation by re-promoting parsed app-database records after Store-V2 enrichment so timeline_events and usage_evidence are not cleared by the enrichment pass.
- V1.6.3 also adds URL frequency and attachment/file-reference frequency views/exports, and adjusts the Case tab action-row layout so Build and Cancel buttons do not overlap.
- V1.6.3: Adds iOS identity-linked activity views/exports to connect phone/email/account/thread/user identity hints with parsed activities, plus Triage-style wrapper heartbeat logging for iOS and AFF4 thin runs. Thin wrappers must show dynamic process/resource/output growth rather than relying only on static status logs.

- V1.3.7 verified the prior critical stability/parser fixes: GUI SQLite pooled connection without instance-lifetime mutex deadlock, APFS guided lookup cycle detection, embedded bplist string ripping, Notes/Location routing, and widened iOS text/path column catchers.
- V1.3.6 direct thin runs were reported successful for both iOS and AFF4/APFS; V1.3.6.1 contained a thin-safeguard hotfix but was not used as the tested baseline.
- iOS thin stability improved after the V1.3.2.x export-stall fixes; minimal/thin mode avoids expensive full diagnostic exports by default.
- AFF4/APFS can stage coherent Store-V2 groups and parse 25,000 artifacts in thin runs, with external comparison still showing path/exact-copyout gaps.

## Next Steps / Active Goal
Validate V1.6.3 on Windows/MSVC and iOS thin. Confirm that iOS timeline_event_count and usage_evidence_count now reflect parsed app-database dated records after enrichment, and review new URL and attachment/file-reference frequency outputs. Whenever a build, thin, full, or targeted validation test is needed, every response/package handoff must include the exact PowerShell command block(s) needed to run that test, including Set-Location, Get-FileHash, Remove-Item, Expand-Archive, build command, and the applicable thin/full test wrapper commands. After validation, continue increasing iOS investigator value by improving communication identity/frequency, timeline/usage promotion, keychain plist intake, NSKeyedArchiver reconstruction depth, and selected app database routes while preserving thin-run performance.

## A full list of what has been implemented, how and what version the implementation took place in (Roadmap)
- V0.4.x: Initial Store-V2 preservation, parser scaffolding, SQLite case model, early GUI/review/export structure, and V7 bridge/hydration workflows.
- V0.5.x: Native-first transition, conservative core probe, mounted-volume candidates, date-candidate identity correction, and V7 de-emphasis.
- V0.6.x: Parent-inode/same-folder analysis, native decoder diagnostics, crash-safe parsing, invalid timestamp handling, and guarded full-native parsing.
- V0.7.x: Investigator dashboard, thin upload model, targeted exports, persistent tags/checked artifacts, GUI investigation redesign, evidence staging, ZIP intake, iOS readiness scaffolding, and source-probe inventory for AFF4/raw readiness.
- V0.8.x: AFF4/source signature probing, large-file shared-read hashing, AFF4 stream inventory harness, iOS focused CoreSpotlight ZIP extraction, iOS app database inventory and parsed app-record scaffolding, iOS GUI containment fixes.
- V0.9.x: iOS CoreSpotlight-focused investigator views, compact normal mode, FFS slim lookup, Missing From FFS views, communication/text-context views, KnowledgeC/CoreDuet scaffolding, parser diagnostics, DB/WAL guardrails, fresh-ZIP inventory fixes, GUI details/view-set improvements, V1 readiness cleanup, and legacy V7 removal.
- V1.0.0-V1.0.8: AFF4/APFS staged Store-V2 extraction path, APFS target scanning, Store-V2 copy-out, APFS reader module boundary, direct copy-out counting/staging fixes, parser module smoke tests.
- V1.0.9-V1.0.17: AFF4/APFS wording cleanup, APFS B-tree decode modularization, diagnostic output gating, lower-bound iterator scaffolding, LZFSE/LZVN optional codec integration, logical-size trim, decmpfs/resource-fork handling.
- V1.0.18-V1.0.31: GUI export worker modularization, background export workers, APFS diagnostic exporter/model relocation, thin-upload safeguards, subprocess/job-object hardening, evidence intake module, iOS app DB parser modularization, database-handle reuse.
- V1.1.0-V1.1.11: APFS/AFF4 worker extraction from app_runner, repeat workflow ledger, GUI cancellation/thread joins, APFS probe worker modularization, documentation/validation consolidation, V1.1.9.1 wrapper/warning hotfix, and V1.1.10/11 package hygiene.
- V1.2.0: Win32 investigation grid moved to LVS_OWNERDATA virtual ListView with LVN_GETDISPINFO, reducing GUI memory pressure.
- V1.2.1: Additional APFS guided lookup buffer reuse and next-leaf buffer reuse.
- V1.3.0-V1.3.1: Group A stability/architecture milestone; APFS buffer reuse continuation, export thread audit, GUI DB access audit, Case tab ingest-time control safeguards, and roadmap reorganization.
- V1.3.2: AFF4/APFS progress visibility; GUI GB-of-GB status estimate; iOS communication frequency view; KnowledgeC /app/activity and /item/interactions stream coverage; cautious communication-intent provenance.
- V1.3.2.1: iOS stack-overflow hotfix; larger Windows stack reserve; app DB inventory progress markers; failed thin ZIP renamed *_FAILED.zip.
- V1.3.2.2: iOS thin mode switched away from diagnostics-heavy export behavior; heavy full exports suppressed in thin mode; full diagnostics left opt-in.
- V1.3.2.3: CSV export cancellation/progress support; long sqlite3_step loops made cancellable; GUI SQLite read connection pooling/serialization; additional APFS branch-child buffer hoisting.
- V1.3.3: Lightweight base-table sample exports for thin iOS instead of expensive view-backed samples; cautious device-owner/Trash/LSQuarantine provenance markers.
- V1.3.4: APFS WhereFroms surfaced as download-origin metadata; bounded NSKeyedArchiver/bplist top-object sample reconstruction; cautious iOS provenance markers; deleted/tombstone GUI routing keywords.
- V1.3.5: iOS communication identity expansion, KnowledgeC communication-intent target hints, WEB_DOWNLOADS parsing, protected-data/keychain-material candidate correlation, and keychain plist filename recognition in keychain material inventory.
- V1.3.6: GUI SQLite deadlock-risk fix, APFS guided B-tree cycle detection, bplist ASCII string ripping, Notes/Location routing, widened iOS text/path catchers.
- V1.3.6.1: iOS thin FFS-materialization safeguard for disk-full class; not used as the tested baseline because V1.3.6 direct thin succeeded after disk cleanup.
- V1.3.7: Verification-focused release confirming the five requested fixes: local-scope ReadOnlyDb locking, APFS cycle guards, ripBplistStrings use, Notes/Location routing, widened catchers.
- V1.4.0: iOS existence/frequency milestone: communication existence, identity/thread frequency, temporal frequency, source coverage, and promotion of dated parsed app records into timeline_events/usage_evidence.
- V1.4.1: Added root ai_context.md. Initial package mistakenly left app_info.cpp at 1.4.0; corrected V1.4.1 fixed binary version metadata.
- V1.6.3: Re-promotes parsed iOS app DB timeline/usage evidence after Store-V2 enrichment, adds URL frequency and attachment/file-reference frequency views/exports, and cleans up Case tab Build/Cancel button layout.

## Known Bugs
- V1.6.3 requires Windows/MSVC build and iOS thin validation before it becomes the next validated baseline.
- Confirm in V1.6.3 thin that timeline_event_count and usage_evidence_count increase above zero after post-enrichment iOS app DB promotion.
- AFF4/APFS path reconstruction remains incomplete: prior thin results had many Vestigant-only files, external-only files, size mismatches, and NO_EXACT_COPYOUT_CANDIDATE rows.
- APFS exact object/path attribution remains the largest AFF4/APFS correctness gap.
- Keychain plist intake is partially recognized as material inventory/candidate correlation, but no validated decryption workflow is implemented or claimed.
- Full NSKeyedArchiver semantic graph reconstruction is not complete; current logic is bounded/sampled and string-focused.
- Optional live ingest preview remains roadmap-only; it should be throttled, read-only, default-off, and skip refresh when DB is busy.
- The Case tab buttons/autosave behavior improved during ingest but should continue to be tested under GUI runs.

## The "Do Not Do" List (Graveyard)

- Repeated build-wrapper version-check mismatches are release-blocking. V1.6.3 adds pre-package verification and must check that Build-V1_6_3.ps1 expects 1.6.3 while app_info.cpp reports 1.6.3. Correct fix: before packaging, run/static-check that app_info.cpp, Build-V*.ps1 version regex, script filenames, and package folder name all match the release version. This is release-blocking.
- Do not use generic AFF4 intake for macOS AFF4/APFS thin validation. Use the guarded AFF4/APFS probe wrapper; generic intake can end as failed_unsupported_container while still creating a misleading upload folder.
- Do not treat a created Upload.zip as success. Check run_status.txt, last_stage.txt, case_summary.json, and complete_success before accepting a thin package.
- Do not bump wrapper/script checks without also bumping core app version metadata in src/core/app_info.cpp; V1.4.1 initially failed because the script expected 1.4.1 while the binary still reported 1.4.0.
- Do not tell the investigator a test is required without providing the exact PowerShell command(s) to run it; this was added as a standing workflow rule in V1.4.1 context maintenance.
- Do not upload 1 GB raw 7-Zip iOS inventory logs. Upload compact logs, summaries, candidate inventories, and export file-size listings instead.
- Do not run iOS thin using full diagnostics unless intentionally testing support/full outputs; diagnostics can materialize millions of FFS rows and fill the disk.
- Do not export heavyweight iOS review views in thin mode by querying full complex views with LIMIT; use lightweight base-table sample SQL.
- Do not keep a GUI SQLite mutex locked for an entire query/lifetime of ReadOnlyDb; lock only for opening/closing/replacing the pooled connection.
- Do not create local APFS node/leaf vectors inside hot B-tree depth loops when reusable buffers can be hoisted.
- Do not allow APFS guided lookups to traverse without a visited-node/cycle guard.
- Do not add automatic “exfiltration” or “destruction” conclusions from WhereFroms, KnowledgeC, Trash paths, quarantine flags, or tombstone indicators. Preserve provenance and cautious labels only.
- Do not claim keychain-assisted decryption merely because *_keychain.plist exists. Inventory and correlate first; implement decryption only with validated key mapping, target, crypto method, and provenance.
- Do not rely on timestamps written after Ctrl+C as proof a long export was progressing normally; buffered/cancellation unwind can write late progress lines.
- Do not use OCR as a fallback unless absolutely necessary. Prefer native parser/string/bplist extraction for iOS Spotlight/app DB content.
- Do not share bundled font files or assume local Windows paths are accessible unless uploaded or present in the sandbox.


## Release Gate Addendum - MSVC string literal safety
- Before packaging every version, run `tools/Verify-MsvcStringLiteralRisk.ps1` or equivalent static review to prevent MSVC C2026 (`string too big, trailing characters truncated`).
- Do not place very large SQL/view definitions in one raw C++ string literal. Split large SQL with `joinSql({ R"SQL(... )SQL", R"SQL(... )SQL" })` or separate `exec(...)` calls.
- Graveyard: V1.6.3 initially failed in `src/db/case_db.cpp` because newly added iOS identity/frequency SQL enlarged an existing raw literal enough to trigger MSVC C2026. Correct fix: split the SQL raw literal and add a release gate; do not just rerun the build or assume Linux compilation catches it.


## V1.6.3 Wrapper Compatibility Hotfix

### Current State
- V1.6.3 fixes the thin-wrapper heartbeat launcher so it works under Windows PowerShell 5.1/.NET Framework, where `System.Diagnostics.ProcessStartInfo.ArgumentList` can be null/unavailable.

### Known Bugs / Recently Fixed
- V1.4.3 heartbeat wrappers failed before CLI launch with `You cannot call a method on a null-valued expression` because they attempted `$psi.ArgumentList.Add(...)`.

### The "Do Not Do" List (Graveyard)
- Do not use `ProcessStartInfo.ArgumentList.Add(...)` in project PowerShell wrappers. It is not safe on the Windows PowerShell version used in this test environment. Use a quoted `$psi.Arguments` string constructed by `ConvertTo-ProcessArgumentString` instead.
- Before providing any build/thin command, verify wrapper scripts for Windows PowerShell 5.1 compatibility, version consistency, and stale script names.

## V1.6.3 Build-Hygiene Hotfix
- V1.4.3.2 failed Windows/MSVC build with C2026 in `src/db/case_db.cpp` at a newly enlarged SQL raw string. The secondary `VestigantSpotlightCli.exe not recognized` error occurred only because the compile failed before the executable was produced.
- Correct fix in V1.6.3: split oversized SQL raw literals below MSVC limits and treat raw-string size checks as release-blocking.
- Permanent rule: before providing a package or build command, verify app version metadata, build-script regex, wrapper names, package folder/version, PowerShell 5.1 compatibility, and oversized C++ string literals.

## V1.6.3 Update

### Project Overview
Vestigant Spotlight parses and reviews macOS Spotlight Store-V2 and iOS CoreSpotlight/AppDB evidence for forensic investigation, with a focus on provenance-backed activity, identity, frequency, and timeline review.

### Current State
V1.4.3.3 validated that iOS extraction, timeline promotion, and usage-evidence promotion are working at scale. V1.6.3 advances from extraction to identity/activity correlation and strengthens release hygiene around modular code and MSVC string-literal safety.

### Next Steps / Active Goal
Transform extracted Spotlight/AppDB data into identity, communication, frequency, and attribution evidence while keeping schema/view code modular enough to avoid recurring MSVC C2026 build failures.

### Roadmap Additions Implemented in V1.6.3
- Added `iOS - Identity Entity Rollup` to group phone/email/account/thread/user-like identifiers across apps, categories, timestamps, and source tables.
- Added `iOS - Identity Thread Activity Matrix` to link identity keys to thread/record/activity keys with app/category provenance.
- Added corresponding CSV exports for the two V1.5 identity-correlation views.
- Split large C++ raw SQL literals across source files so release checks can prevent MSVC C2026 oversized string-literal failures.
- Kept Triage-style heartbeat wrappers and version consistency checks as release-blocking expectations.

### Known Bugs
- Keychain plist intake is still inventory/correlation only; no decryption workflow is claimed.
- Some identity keys are fallback keys and require row-level review before attribution.
- AFF4/APFS path matching remains incomplete for some extracted Store-V2 content.

### The "Do Not Do" List (Graveyard)
- Do not add new large monolithic SQL blocks to `case_db.cpp`; split schema/view SQL into smaller literals or modular helper files.
- Do not provide a release ZIP until app version metadata, build-script version checks, wrapper script names, and `ai_context.md` agree.
- Do not diagnose `VestigantSpotlightCli.exe not recognized` as a path problem when compile failed first; it is secondary.
- Do not label Spotlight-only communications as deleted/destructed without corroborating evidence; use “present in CoreSpotlight / not observed in parsed native DB set.”


## V1.6.3 Hotfix Note

- V1.5.0 iOS thin parsed and exported most outputs but failed during `ios_identity_entity_rollup.csv` because `vw_ios_identity_entity_rollup` referenced `activity_thread_or_record_id` from `vw_ios_identity_activity_linkage`, which is an aggregate view and does not expose that detail-level column.
- Fix: `vw_ios_identity_entity_rollup` now derives directly from `ios_app_parsed_records` in a local CTE so all referenced columns exist and rollup counts are not dependent on a limited/sample detail view.
- Release-blocking check: any new view/export pair must be checked for column availability before release; failed late-export views should be treated as build/release blockers even when parsing succeeds.

## V1.6.3 Export/View Schema Hotfix

- V1.5.0.1 failed late in iOS thin export because `vw_ios_identity_thread_activity_matrix` referenced `identity_kind` and `activity_thread_or_record_id` through a detail sample view that did not expose those columns.
- Correct fix in V1.6.3: expose identity classification and thread/record id columns directly in the detail sample view before downstream matrix exports query them.
- Release-blocker rule: any SQLite view added or changed for export must be prepare-tested with its actual export query, not only created successfully. `SELECT * FROM <view> LIMIT 1` is insufficient if export uses explicit ORDER BY or column names.
- Modularization rule: shared view column definitions must remain synchronized with GUI registry and exporter SQL; prefer single registry/definition sources where feasible.


## V1.6.3 Thin Export Performance Rule

- V1.6.1 thin showed that `ios_spotlight_identity_context_links.csv` can remain actively running for a long period because the full identity-context view materializes a broad CoreSpotlight join before returning rows.
- Correct fix: thin/minimal mode must not fully export new identity/frequency views unless they are proven bounded and fast. Use summary views or base-table samples with explicit LIMITs. Full exports remain available only through FullDiagnostics/support profiles.
- Release blocker: every new investigator export must be classified as required, optional sample, or full diagnostics, and thin wrappers must use the bounded version.

## V1.6.3 Thin Export Performance Hotfix
- V1.6.1 thin heartbeat showed `ios_identity_all_activity_links_sample.csv` consuming sustained CPU and SQLite progress for more than 30 minutes while output size was flat. This proves the sample was still backed by a heavy joined view and was not suitable for thin/minimal mode.
- Correct fix in V1.6.3: thin/minimal mode no longer exports the full all-activity identity links view or a sample derived from that view. It emits a lightweight bounded summary directly from `ios_app_parsed_records`; full joined identity link exports remain FullDiagnostics/support-only.
- Release rule: no export named `*_sample.csv` may use a broad joined identity/frequency view unless it has been proven bounded and fast on the standard iOS FFS test source.


## V1.6.3 Full-Investigation Export Guardrails
- V1.6.3 treats full-investigation stalling as a release-blocking workflow risk. Thin/minimal identity exports must use direct base-table summaries and bounded samples, not joined identity graph views that can materialize large intermediate result sets.
- Full/support/diagnostic exports now have a per-export SQL timeout guard. A single expensive export should time out, write a timeout notice, and allow the run to continue rather than making the whole investigation appear hung.
- Full joined identity graph exports remain available in FullDiagnostics/support/full profiles, but they must not be required for normal thin validation or routine investigator review.
- Future work: materialize indexed identity-link tables during enrichment so full identity graph exports query precomputed tables rather than expensive ad hoc joins.

## V1.6.3 Thin Performance Evidence Update
- Thin uploads now include `thin_performance_summary.csv` and `THIN_PERFORMANCE_SUMMARY.md` when generated.
- These files summarize export durations and flag slow or incomplete exports from `run_progress.tsv`.
- Future performance claims about stalls or slow exports should cite heartbeat/progress logs and the thin performance summary when available.

## V1.6.3 Identity/Communication Promotion Update
- V1.6.3 adds a dedicated identity pivot surface that promotes phone/email/account/thread/name-like indicators from parsed iOS app database records and CoreSpotlight key/value context into investigator-facing views.
- V1.6.3 adds communication candidate promotion views so records containing communication categories, thread identifiers, author/recipient/phone/email markers, or message-app text cues are visible even when they cannot be conclusively classified as messages.
- V1.6.3 adds a conservative CoreSpotlight-not-observed-in-native view. Wording must remain lead-only: absence from parsed native app DBs may reflect deletion, encryption/inaccessibility, unsupported parser coverage, missing app data, or unmatched identifiers.
- V1.6.3 preserves the thin-export performance rule: bounded base-table summaries and samples in thin/minimal mode; expensive joined exports remain FullDiagnostics/support-only.
