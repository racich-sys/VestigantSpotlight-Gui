# ai_context.md

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
- Current package: V1.4.2, based on validated V1.4.1/V1.4.0 iOS existence/frequency work plus continued roadmap implementation.
- Current functional milestone: V1.4.2 strengthens iOS investigator-value generation by re-promoting parsed app-database records after Store-V2 enrichment so timeline_events and usage_evidence are not cleared by the enrichment pass.
- V1.4.2 also adds URL frequency and attachment/file-reference frequency views/exports, and adjusts the Case tab action-row layout so Build and Cancel buttons do not overlap.
- V1.3.7 verified the prior critical stability/parser fixes: GUI SQLite pooled connection without instance-lifetime mutex deadlock, APFS guided lookup cycle detection, embedded bplist string ripping, Notes/Location routing, and widened iOS text/path column catchers.
- V1.3.6 direct thin runs were reported successful for both iOS and AFF4/APFS; V1.3.6.1 contained a thin-safeguard hotfix but was not used as the tested baseline.
- iOS thin stability improved after the V1.3.2.x export-stall fixes; minimal/thin mode avoids expensive full diagnostic exports by default.
- AFF4/APFS can stage coherent Store-V2 groups and parse 25,000 artifacts in thin runs, with external comparison still showing path/exact-copyout gaps.

## Next Steps / Active Goal
Validate V1.4.2 on Windows/MSVC and iOS thin. Confirm that iOS timeline_event_count and usage_evidence_count now reflect parsed app-database dated records after enrichment, and review new URL and attachment/file-reference frequency outputs. Whenever a build, thin, full, or targeted validation test is needed, every response/package handoff must include the exact PowerShell command block(s) needed to run that test, including Set-Location, Get-FileHash, Remove-Item, Expand-Archive, build command, and the applicable thin/full test wrapper commands. After validation, continue increasing iOS investigator value by improving communication identity/frequency, timeline/usage promotion, keychain plist intake, NSKeyedArchiver reconstruction depth, and selected app database routes while preserving thin-run performance.

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
- V1.4.2: Re-promotes parsed iOS app DB timeline/usage evidence after Store-V2 enrichment, adds URL frequency and attachment/file-reference frequency views/exports, and cleans up Case tab Build/Cancel button layout.

## Known Bugs
- V1.4.2 requires Windows/MSVC build and iOS thin validation before it becomes the next validated baseline.
- Confirm in V1.4.2 thin that timeline_event_count and usage_evidence_count increase above zero after post-enrichment iOS app DB promotion.
- AFF4/APFS path reconstruction remains incomplete: prior thin results had many Vestigant-only files, external-only files, size mismatches, and NO_EXACT_COPYOUT_CANDIDATE rows.
- APFS exact object/path attribution remains the largest AFF4/APFS correctness gap.
- Keychain plist intake is partially recognized as material inventory/candidate correlation, but no validated decryption workflow is implemented or claimed.
- Full NSKeyedArchiver semantic graph reconstruction is not complete; current logic is bounded/sampled and string-focused.
- Optional live ingest preview remains roadmap-only; it should be throttled, read-only, default-off, and skip refresh when DB is busy.
- The Case tab buttons/autosave behavior improved during ingest but should continue to be tested under GUI runs.

## The "Do Not Do" List (Graveyard)

- V1.4.2 initial package repeated the build-wrapper version-check mismatch: Build-V1_4_2.ps1 still checked 1.4.1 while app_info.cpp reported 1.4.2. Correct fix: before packaging, run/static-check that app_info.cpp, Build-V*.ps1 version regex, script filenames, and package folder name all match the release version. This is release-blocking.
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
