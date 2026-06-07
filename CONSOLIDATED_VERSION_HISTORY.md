## V1_2_0

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
- Reason: V1.2.0 changes Win32 GUI review-grid rendering and current-package documentation/scripts only. V1.1.11 AFF4/APFS thin output was reviewed before the change, and no extraction/traversal/copy-out/decompression/parser code was intentionally changed.
- Trigger for escalating AFF4/APFS to full test: any next change to live APFS traversal, copy-out, decompression, extent handling, path reconstruction, external compare logic, or Store-V2 staging behavior.
- Trigger for iOS testing: any next change to iOS ZIP staging, CoreSpotlight parsing, FFS lookup, app DB parsing, bplist/NSKeyedArchiver handling, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_2_0_build.log` and `Upload_Thin_MacOS_AFF4_V1_2_0.zip`.

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
