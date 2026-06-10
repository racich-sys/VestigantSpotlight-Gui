## V1.6.6.5 - iOS native CoreSpotlight probe review bridge

V1.6.6.5 was built after review of the uploaded V1.6.6.4 Windows build log and iOS thin bundle. The Windows build log showed a completed V1.6.6.4 build. The iOS thin bundle reached `complete_success`, with 6 valid stores, 344,445 raw records, 22,569 raw key/value rows, 344,445 artifacts, 228,699 usage evidence rows, and 277,823 timeline events.

The V1.6.6.4 thin run showed that timeout-prone V1.6.6.2 exports were fixed, but compact native probe strings were still not flowing into the main iOS text-context and communication review surfaces. `ios_string_probe_category_summary.csv` contained 9,591 message/app string probes and 933 email/account probes, while message/text-context review exports remained empty.

Implemented:
- high-signal `__native_core_probe_string_*` values can now feed same-record text context;
- iOS text-context review exposes `source_field_name` and classifies native SMS/iMessage/mail/account/file-reference probes;
- iOS communication/message review views expose `native_probe_context_count` and `native_probe_context_sample`;
- new communication buckets identify `SPOTLIGHT_MESSAGE_OR_ATTACHMENT_TEXT_PROBE` and `SPOTLIGHT_MAIL_OR_ACCOUNT_TEXT_PROBE`;
- `vw_ios_spotlight_investigator_overview` now uses lightweight base/probe counts to avoid the V1.6.6.4 46-second slow overview export;
- self-test coverage now includes native CoreSpotlight probe-to-text-context and communication-review checks.

Validation performed here:
- Linux CMake build: PASS.
- CLI version: `Vestigant Spotlight v1.6.6.5`.
- Self-test: PASS.
- Windows/MSVC build: not run here.

## V1_6_6_5

- Reviewed the V1.6.6.3 wrapper/readiness failure reported from the Windows build output before source changes.
- Fixed current Windows build wrapper versioning and moved preflight checks after source extraction / clean extraction to avoid validating stale extracted source.
- Updated Win32 GUI bootstrap communication/identity/frequency views so generic `KNOWLEDGEC_EVENTS` is not a positive communication predicate; `KNOWLEDGEC_COMMUNICATION_INTENT` and provenance markers are used instead.
- Reworked release-readiness checks to scan both `src/db/case_db.cpp` and `src/gui/win32_gui.cpp`, while permitting explicit suppression guards that exclude generic KnowledgeC/device-app rows.
- Local validation: Linux CMake build PASS; CLI version reports `Vestigant Spotlight v1.6.6.5`; self-test PASS; static current-wrapper/text and KnowledgeC promotional predicate audits PASS.
- Test determination: run Windows/MSVC build first, then iOS thin; AFF4/APFS thin/full is not required unless build/shared schema behavior regresses.

## V1_6_6_3

- Reviewed uploaded `Upload_Thin_iOS_CoreSpotlight_V1_6_6_2.zip` before source changes.
- The V1.6.6.2 thin run reached `complete_success`, but three identity/communication exports timed out: `ios_identity_pivot_frequency.csv`, `ios_communication_candidate_promotion_sample.csv`, and `ios_spotlight_communication_not_observed_native_sample.csv`.
- V1.6.6.3 makes those outputs thin-safe in minimal mode: direct base-table summaries/samples replace timeout-prone joined graph materialization, while support/full profiles retain the richer joined views under timeout protection.
- Corrected the current Windows build wrapper CLI-version validation to expect `1.6.6.3`.
- Local validation: Linux CMake build PASS; CLI version reports `Vestigant Spotlight v1.6.6.3`; self-test PASS.
- Test determination: run iOS thin for V1.6.6.3; AFF4/APFS thin/full is not required unless build/shared schema behavior regresses.

## V1_6_6_2

- Reviewed all 63 `.md` and `.txt` files from the uploaded V1.6.6.1 source package before making source changes.
- Hardened iOS KnowledgeC/CoreDuet parsing so fallback device/app state streams no longer populate `contact_or_participant`; they are classified as `KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY` and marked with `IDENTITY_PROMOTION_SUPPRESSED=True`.
- Updated communication/identity SQLite view predicates so generic `KNOWLEDGEC_EVENTS` no longer auto-promotes into communication, identity, or frequency evidence. Communication-intent rows remain surfaced through `KNOWLEDGEC_COMMUNICATION_INTENT` and explicit provenance markers.
- Added `runKnowledgeCIdentitySuppressionSmokeTest` to validate synthetic KnowledgeC device/generic/communication-intent rows against generated SQLite views.
- Updated current build/run wrappers, release-readiness checks, release notes, quick start, continuation handoff, and `ai_context.md` to V1.6.6.2.
- Local validation: Linux CMake build PASS; CLI version reports `Vestigant Spotlight v1.6.6.2`; self-test PASS. Windows/MSVC build and iOS thin validation remain required.
- Test determination: iOS thin required; AFF4/APFS thin/full not required unless shared schema initialization or build behavior regresses, because no AFF4/APFS code changed.

## V1_6_3

- Documentation/context bootstrap release.
- Added root-level `ai_context.md` as the living source-of-truth context file for future sessions.
- No code behavior, parser behavior, schema, GUI workflow, or forensic interpretation changes are intended in this package.

## V1_3_7

- Prioritized iOS communication identity and KnowledgeC expansion before broader roadmap items.
- Added WEB_DOWNLOADS to the iOS app database target parse categories so Safari/Chrome/WebKit download tables are parsed rather than only inventoried.
- Expanded KnowledgeC communication-intent parsing to promote bounded serialized interaction target hints into contact/participant context when personHandle/emailAddress/name indicators are present.
- Added iOS protected-data decryption candidate view/export that correlates parsed app databases with available keychain/keybag/keychain-plist material without claiming decryption.
- Added sibling/decrypted keychain plist filenames to keychain material inventory logic where present in the FFS inventory.
- Preserved cautious interpretation wording: no automatic exfiltration or destruction conclusions are asserted.
- Linux CMake build, CLI version check, and self-test passed locally; Windows/MSVC build and iOS thin validation are required.

## V1_3_4

- Stability: retained V1.3.2.3 CSV cancellation/progress and GUI SQLite pooled read connection behavior after review.
- APFS: added cautious WhereFroms XATTR surfacing as download-origin metadata without asserting exfiltration.
- iOS: added bounded binary plist / NSKeyedArchiver top-object graph sample reconstruction in the existing bplist context output.
- iOS: added safe provenance markers for device-owner contact indicators, Trash path components, Quarantine metadata references, and Spotlight deleted/expired references.
- GUI: improved deleted/tombstone review-view routing keywords for macOS views without adding unsupported intent conclusions.
- Not implemented: automatic exfiltration/destruction conclusions; full NSKeyedArchiver semantic decoder; full APFS parser unification.
- Validation: Linux CMake build, CLI version check, and self-test passed. Windows/MSVC build and iOS thin validation are required.

TEST SCOPE DECISION
- AFF4/APFS: thin only if validating WhereFroms/APFS XATTR output; otherwise not required for iOS-only validation.
- iOS: thin required because bplist/NSKeyedArchiver context output and iOS parsed-record provenance changed.
- Full test trigger: full AFF4/APFS only if APFS traversal/copy-out behavior is changed later; full iOS only if keychain-assisted decryption or full NSKeyedArchiver semantic decoding is added.

## V1_3_3

- iOS thin/export stability release after V1.3.2.2 completed but showed expensive sample exports could appear stalled.
- Replaced thin-mode `ios_spotlight_record_review_sample.csv`, date provenance sample, and investigative-item samples with lightweight base-table SQL instead of querying heavy investigator views with `LIMIT`.
- Preserved full heavy exports for explicit support/full diagnostic profiles.
- Added cautious iOS parsed-record provenance markers for device-owner contact rows, Trash path components, and LSQuarantine string references without asserting exfiltration/destruction intent.
- Confirmed prior V1.3.2.3 export cancellation and GUI read-connection pooling remain present.

TEST SCOPE DECISION
- AFF4/APFS: not required.
- iOS: thin required.
- Reason: changes affect iOS thin export SQL and generic iOS app database parsed-record provenance only.
- Trigger for escalating to full iOS test: iOS thin still stalls, crashes, or missing expected communication/app DB outputs.
- Required next uploaded artifacts: `V1_3_3_build.log` and `Upload_Thin_iOS_CoreSpotlight_V1_3_3.zip` or stopped-state logs if interrupted.

## V1_3_2_3

- iOS stack-overflow hotfix after V1.3.2 iOS thin crashed with Windows structured exception `0xc00000fd` immediately after native 7-Zip inventory parsing.
- Increased Windows/MSVC executable stack reserve for CLI, tests, and GUI to reduce stack exhaustion risk during very large iOS ZIP/app-database processing.
- Added iOS app-database record-inventory stage/progress markers around candidate enumeration, transaction start, per-database progress, and per-table parse warnings.
- Updated the iOS thin wrapper so failed runs rename any generated upload ZIP to `_FAILED.zip` rather than leaving a normal-looking thin result.
- No AFF4/APFS extraction semantics, Store-V2 parsing, or iOS forensic interpretation logic was intentionally changed.

TEST SCOPE DECISION
- AFF4/APFS: not required for this hotfix.
- iOS: thin required.
- Reason: V1.3.2.3 targets the iOS ZIP/app-database stack-overflow failure and failed-upload labeling behavior only.
- Trigger for escalating iOS to full test: repeat `0xc00000fd`, zero parsed records after successful inventory import, malformed communication-frequency view, or severe slowdown after database inventory.
- Required next uploaded artifacts: `V1_3_2_3_build.log` and `Upload_Thin_iOS_CoreSpotlight_V1_3_2_3.zip` or `_FAILED.zip` with logs if it fails.

## V1_3_2

- Priority order implemented as requested: (1) AFF4/APFS progress visibility, (3) iOS communication/identity analysis, then (2) Case tab stability follow-through.
- Added AFF4 direct-map progress heartbeats during long-running AFF4/APFS direct reader stages so the GUI no longer remains stuck at `open_sqlite` while resource monitor shows AFF4 reads.
- Updated GUI heartbeat/status display to include a stage-percent-derived `GB of GB processed` estimate when the source file size and progress percent are available.
- Added iOS communication identity derivation for parsed app/KnowledgeC records: identity-bound communication markers, thread-volume markers, deleted/expired Spotlight communication markers, and intent-target hints are preserved in provenance/contact fields when present in parsed snippets/metadata.
- Expanded KnowledgeC parsing to include `/app/activity` and `/item/interactions` streams and to tag share/message intents with cautious `COMMUNICATION_INTENT` provenance rather than unsupported exfiltration conclusions.
- Added `vw_ios_communication_frequency` and GUI/export registration for `iOS - Communication Frequency & Volume` to group committed parsed records by thread/contact/source identifier.
- Added `ios_communication_frequency.csv` to investigator exports.
- Added Safari/Chrome/WebKit download table categorization as `WEB_DOWNLOADS` without asserting exfiltration.
- Preserved conservative forensic language: new iOS fields surface communication/thread/intent evidence and provenance, not final conclusions about user intent.

TEST SCOPE DECISION
- AFF4/APFS: thin required.
- iOS: thin required before relying on new communication-frequency and KnowledgeC intent outputs.
- Reason: V1.3.2 changes AFF4 progress reporting only, but it also changes iOS parser categorization/provenance and adds a new iOS review/export view.
- Trigger for escalating AFF4/APFS to full test: any regression in staged Store-V2 counts, remaining mismatch diagnostics, or a repeat of status/progress stalling without heartbeats.
- Trigger for escalating iOS to full test: incorrect counts in `vw_ios_communication_frequency`, parser slowdown, malformed provenance/contact extraction, or missing KnowledgeC rows.
- Required next uploaded artifacts: `V1_3_2_build.log`, `Upload_Thin_MacOS_AFF4_V1_3_2.zip`, and an iOS thin output if the iOS communication view is tested.

## V1_3_2

- Base reviewed before changes: V1.3.0 source package, `V1_3_0_build.log`, `Upload_Thin_MacOS_AFF4_V1_3_0.zip`, and uploaded refined roadmap suggestions.
- Windows/MSVC V1.3.0 build completed successfully and reported `Vestigant Spotlight v1.3.0`.
- V1.3.0 thin AFF4/APFS package continued to contain coherent Store-V2 staged groups; no regression was identified before this stability pass.
- Completed remaining APFS next-leaf buffer hoisting in three OMAP/root-tree horizontal-leaf traversal paths in `src/parsers/aff4_probe_worker.cpp`; the next-leaf scratch buffer is now allocated outside the bounded depth loop and reused.
- Hardened the Case Information tab during active ingest: case-location/case-database/open/save mutations are blocked or deferred while the ingest worker owns the case database, case-mutation controls are disabled during processing, and autosave now reports `Autosave deferred during ingest` instead of a misleading failure when processing is active.
- Updated roadmap/tracker documentation so Groups B/C and later suggestions remain tracked as future APFS completeness, iOS reconstruction, and APFS unification work.
- No new forensic interpretation labels were added. No iOS parser behavior, SQLite schema, Store-V2 parser behavior, decompression behavior, or APFS copy-out semantics were intentionally changed.

## TEST SCOPE DECISION

- AFF4/APFS: thin required after Windows build.
- iOS: not required.
- Reason: V1.3.2 touches APFS traversal-buffer allocation and Win32 GUI case-tab runtime behavior, but does not change copy-out semantics, decoder behavior, Store-V2 parsing, iOS parsing, or database schema.
- Trigger for escalating AFF4/APFS to full test: any regression in staged group/file counts, remaining mismatch diagnostics, parser failures, decompression failures, or case-tab processing behavior in the thin run.
- Trigger for iOS testing: any later change to iOS ZIP staging, CoreSpotlight parsing, app DB parsing, bplist/NSKeyedArchiver reconstruction, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_3_2_build.log` and `Upload_Thin_MacOS_AFF4_V1_3_2.zip`.

## V1_3_2

- Scope: coordinated Win32 GUI runtime hardening release after V1.1.11 validation.
- Reviewed uploaded `V1_1_11_build.log`: Windows/MSVC build completed successfully, CLI/tests/GUI linked, and `Vestigant Spotlight v1.1.11` was reported.
- Reviewed uploaded `Upload_Thin_MacOS_AFF4_V1_1_11.zip`: AFF4/APFS run completed with 25,000 artifacts, 8,986 staged files, 4,123 external reference files, and 486 remaining relative-path size mismatches.
- Implemented owner-data/virtual rendering for the main Win32 investigation `WC_LISTVIEWW` grid using `LVS_OWNERDATA` plus an `LVN_GETDISPINFOW` callback backed by the current page cache.
- Updated checked-row and visible-tag refresh behavior to redraw cached owner-data rows instead of pushing per-cell strings into the ListView control.
- Updated the selected-row details pane to read from the same current-page cache so details remain populated under virtual ListView rendering.
- Preserved previously implemented Windows hardening already present in V1.1.11: long-path portable binary writes, SQLite busy timeout/WAL checkpointing, ingest thread guard, GUI GDI cleanup, logger mutex protection, and bounded LZFSE/LZVN decode behavior.
- Did not change AFF4/APFS extraction, APFS traversal, Store-V2 parsing, iOS parsing, evidence interpretation, or SQLite schema behavior.

## TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build.
- iOS: not required.
- Reason: V1.3.2 changes Win32 GUI review-grid rendering and current-package documentation/scripts only. V1.1.11 AFF4/APFS thin output was reviewed before the change, and no extraction/traversal/copy-out/decompression/parser code was intentionally changed.
- Trigger for escalating AFF4/APFS to full test: any next change to live APFS traversal, copy-out, decompression, extent handling, path reconstruction, external compare logic, or Store-V2 staging behavior.
- Trigger for iOS testing: any next change to iOS ZIP staging, CoreSpotlight parsing, FFS lookup, app DB parsing, bplist/NSKeyedArchiver handling, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_3_2_build.log` and `Upload_Thin_MacOS_AFF4_V1_3_2.zip`.

## V1_1_11

- Scope: documentation/package hygiene release.
- Consolidated standalone development notes into `docs/CONSOLIDATED_DEVELOPMENT_NOTES.md`.
- Consolidated standalone validation logs/notes into `validation/CONSOLIDATED_VALIDATION_LOGS_AND_NOTES.md`.
- Removed the now-consolidated standalone note/log files from the active package.
- Added `docs/SUPPORT_DIAGNOSTIC_TOOLS_REGISTER.md` to track retained support/diagnostic tools and their retention rationale.
- No support/diagnostic tools were deleted in this version because each remains tied to active AFF4/APFS validation, iOS support, general packaging/staging, or on-demand troubleshooting.
- No AFF4/APFS extraction, iOS parsing, GUI behavior, Store-V2 parser behavior, or SQLite schema behavior was intentionally changed.

- Reviewed uploaded `V1_1_10_1_build.log`: Windows/MSVC build completed successfully, CLI/tests/GUI linked, and `Vestigant Spotlight v1.1.10.1` was reported.
- Reviewed uploaded `Upload_Thin_MacOS_AFF4_V1_1_10_1.zip`: AFF4/APFS run completed source-probe workflow; staged Store-V2 parse/enrichment produced 25,000 artifacts.
- External compare summary remained stable against the prior V1.1.9/V1.1.10.1 class: 4,123 external files, 8,986 Vestigant staged files, 2,213 file matches, 1,424 external-only rows, 6,710 Vestigant-only rows, and 486 relative-path size mismatches.
- Remaining mismatch diagnostics stayed at 486 rows: 4 `DATA_FORK_SIZE_DISAGREES_WITH_EXTERNAL` and 482 `NO_EXACT_COPYOUT_CANDIDATE`.

## TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build.
- iOS: not required.
- Reason: V1.1.11 changes documentation/package hygiene only. The V1.1.10.1 build and AFF4/APFS thin output were reviewed before this version; no extraction/traversal/copy-out/decompression/parser code changed.
- Trigger for escalating AFF4/APFS to full test: any next change to live APFS traversal, copy-out, decompression, extent handling, path reconstruction, external compare logic, or Store-V2 staging behavior.
- Trigger for iOS testing: any next change to iOS ZIP staging, CoreSpotlight parsing, FFS lookup, app DB parsing, bplist/NSKeyedArchiver handling, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_1_11_build.log` and `Upload_Thin_MacOS_AFF4_V1_1_11.zip`.


## V1_1_10_1

- Documentation/script-command hotfix on V1.1.10.
- Updated build, quick-start, help, and new-chat continuation instructions to use the full extract/build PowerShell block for `VestigantSpotlightInv_V1_1_10_1.zip`.
- Updated macOS AFF4/APFS thin regression instructions to use `Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut`.
- Regenerated current-version PowerShell wrappers for V1.1.10.1.
- No AFF4/APFS extraction, iOS parsing, GUI behavior, SQLite schema, Store-V2 parser, or forensic interpretation behavior was intentionally changed.

## V1_1_10

- Used V1.1.9.1 as the base.
- Reviewed all source-package `.md`, `.txt`, and `.ps1` files and recorded decisions in `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.*`.
- Regenerated V1.1.10 build, GUI launch, AFF4/APFS thin-run, and package-existing-case wrappers.
- Removed obsolete active-package clutter: stale root-level V1.1.9 manifest/patch files and stale V1.1.9 source-review inventory files replaced by V1.1.10 review files.
- Preserved append-only version history and historical validation notes.
- No AFF4/APFS traversal, copy-out, staging, Store-V2 parsing, external comparison, iOS parsing, GUI behavior, or SQLite schema behavior was intentionally changed.

## V1.1.5.1

- Propagated ingest cancellation into guarded AFF4 dynamic/direct probe entry points and selected expensive bounded loops.
- Added case-directory writability preflight before normal logging/database setup.
- Added thin-upload size/policy guard for `exports/upload_samples` in C++ and PowerShell packagers.
- Changed focused iOS 7-Zip extraction log redirection to UTF-8 `Out-File`.
- Wrapped APFS staged Store-V2 diagnostic sample exports in localized error handling.


## V1.1.4

- Repeat-cycle hardening release after V1.1.3 validation.
- Added bplist offset-table/top-object-offset metadata to existing bounded bplist context summaries without claiming full NSKeyedArchiver graph decoding.
- Added safer GUI checked-artifact snapshot helpers for export/page-load requests.
- Strengthened GUI ingest double-click protection with an atomic compare/exchange gate.
- Updated workflow ledger, roadmap, and suggestions tracker for the next AFF4/APFS monolith and comparator work.

## V1.1.3

Repeat-cycle hardening: export cancellation callbacks, orphan purge transaction, secure RichEdit load, APFS iterator scaffolding update, and workflow ledger updates.

## V1.1.2

- Added repeat-cycle workflow ledger.
- Added GUI ingest cancellation token/control and runApplication safe cancellation checkpoints.
- Hardened AFF4 dependent DLL search.
- Freed GUI logo bitmap during shutdown.
- Added native parser bulk SQLite PRAGMAs with restoration.
- Added bounded bplist trailer validation metadata to bplist/NSKeyedArchiver context output.

# Version History

## V1.0.30

- Reviewed V1.0.29 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Moved iOS app database record-inventory orchestration into `IosAppDbParser::parseRecordInventories(...)`.
- Reduced `app_runner.cpp` iOS app DB inventory function to a delegating wrapper with status callback preservation.
- Added GUI export thread registry and joined active export workers during `WM_DESTROY` instead of detaching Export Page/Filtered/Checked/Tagged workers.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- No AFF4/APFS traversal, copy-out, Store-V2 parsing, iOS CoreSpotlight schema, or forensic interpretation changes.


V1.0.27 is a thin-upload packaging hotfix after the V1.0.26 AFF4/APFS run and external comparison completed but the thin ZIP failed during PowerShell relative-path inventory generation.

## Changed

- Fixed `tools/Create-SourceProbeUploadZip.ps1` so `Get-RelativePathForThinInventory` no longer uses `[char]'\\'`, which Windows PowerShell treats as a two-character string and rejects.
- Reused the robust relative-path helper for `ExtractedSpotlight` copy paths.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- Added `scripts/Package-V1_0_27-macOS-AFF4-ThinFromExistingCase.ps1` for packaging an already-completed V1.0.26 AFF4/APFS case without rerunning the probe.
- Added `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, and `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.

## Not changed

- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.

## Validation

- Reviewed the uploaded V1.0.26 build log; the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.
- Reviewed the user-reported wrapper output showing the AFF4/APFS probe and external comparison completed before packaging failed.
- Local syntax/text checks were performed for modified C++ and PowerShell packaging files. Windows/MSVC V1.0.27 validation remains required.

# V1.0.26

- Fixed the remaining thin-upload raw-log leak in the standalone source-probe upload tool by denying raw tool outputs and full raw file inventories.
- Added matching in-app thin-upload deny-list policy for raw AFF4/iOS tool logs and full file-inventory CSVs.
- Updated thin-upload inventory text files to report relative paths instead of full local paths.
- Added bounded hidden Windows subprocess waits to avoid indefinite hangs from prompted/wedged external tools.
- Updated exact AFF4/ZIP byte reads on Windows to use 64-bit `_fseeki64`.
- No APFS traversal, Store-V2 parsing, iOS parsing, GUI schema, or APFS diagnostic writer movement is included in this version.

# V1.0.25

- Thin Upload security/performance hardening after V1.0.24.1 build success.
- Removed raw AFF4/iOS extraction tool logs and generated extraction helper scripts from Thin Upload copy lists.
- Replaced hardcoded export CSV bundle lists with dynamic copying of regular top-level `exports/*.csv` files plus the existing `exports/upload_samples` directory.
- Optimized `countCsvDataRows()` with binary chunk newline counting.
- Reworked staged iOS app-database path normalization using `std::filesystem::path::lexically_normal()` plus per-component sanitization.
- Added direct Windows `CreateProcessW` helpers with redirected stdout/stderr handles for selected hidden AFF4 stream inventory and ZIP PowerShell staging calls.
- No APFS traversal, APFS copy-out, Store-V2 parsing, iOS parsing, schema, or GUI view behavior was intentionally changed.

# V1.0.24.1

- Fixed the V1.0.24 Windows/MSVC `C2668` ambiguous `buildWhere` compile failure in `src/gui/win32_gui.cpp`.
- Removed the stale local `buildWhere` wrapper left behind after creating `src/gui/gui_view_helpers.h/.cpp`.
- Explicitly routed review-page SQL `WHERE` assembly through the shared `vestigant::spotlight::buildWhere(...)` helper using captured filter state.
- No APFS/AFF4 traversal, Store-V2 parsing, iOS parsing, schema, GUI views, or diagnostic writer behavior was intentionally changed.
- Updated V1.0.24.1 build/launch/AFF4 wrapper scripts.

# V1.0.18

- Added optional Apple/lzfse LZFSE/LZVN codec integration path.
- Added `src/codec/lzfse_codec.h/.cpp` with safe no-output behavior when the codec is not compiled in.
- Added `tools/Prepare-LzfseThirdParty.ps1` to explicitly vendor and manifest Apple/lzfse source under `third_party/lzfse`.
- Updated CMake and no-CMake MSVC build scripts to compile the Apple decoder sources only when the vetted source tree is present.
- Updated APFS decmpfs resource-fork reconstruction so compression types 8/12 call the Apple codec adapter when available and record explicit decode/skipped statuses when unavailable or failed.
- Updated direct AFF4/APFS copy-out to prefer inode data-stream logical size over raw extent-chain end where available.
- Added validation/status documentation for logical-size trim and optional codec integration.

# V1.0.15

- Added AFF4/APFS Store-V2 candidate dual-process comparison.
- New outputs:
  - `aff4_apfs_storev2_candidate_dual_process_compare.csv`
  - `aff4_apfs_storev2_candidate_dual_process_compare_summary.json`
  - `AFF4_APFS_STOREV2_CANDIDATE_DUAL_PROCESS_COMPARE.md`
- The compare output audits raw APFS copy-out candidates against normalized `StagedStoreV2` selections.
- Added packaging and wrapper validation for the new compare outputs.
- Added LZFSE/LZVN source review documentation explaining why APFS structural documentation is authoritative for locating compressed content but not sufficient by itself to enable production codec output.
- Kept normal-mode AFF4/APFS structural diagnostics suppressed while keeping copy-out/staging/parser/enrichment/external-compare outputs enabled.

# V1.0.14

- Moved iOS app DB row parsing into `src/parsers/ios_app_db_parser.cpp`.
- Corrected AFF4/APFS normal-mode logging around suppressed diagnostics.
- Preserved Store-V2 staged parser handoff.

# V1.0.28.2

V1.0.28.2 relocates the main APFS/AFF4 diagnostic/report writer bodies out of `app_runner.cpp` and into `apfs_diagnostic_exporter.cpp`.

## Changed

- Added typed APFS diagnostic writer declarations to `src/parsers/apfs_diagnostic_exporter.h`.
- Moved main APFS diagnostic/report writer families into `src/parsers/apfs_diagnostic_exporter.cpp`.
- Reduced `src/app/app_runner.cpp` by approximately 1,800 lines.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.

## Not changed

- No APFS traversal changes.
- No AFF4 dynamic reader changes.
- No APFS copy-out/staging changes.
- No Store-V2 parser changes.
- No iOS parser changes.
- No SQLite schema changes.
- No GUI behavior changes.
