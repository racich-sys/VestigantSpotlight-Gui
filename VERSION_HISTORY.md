## V1.1.2

- Added repeat-cycle workflow ledger.
- Added GUI ingest cancellation token/control and runApplication safe cancellation checkpoints.
- Hardened AFF4 dependent DLL search.
- Freed GUI logo bitmap during shutdown.
- Added native parser bulk SQLite PRAGMAs with restoration.
- Added bounded bplist trailer validation metadata to bplist/NSKeyedArchiver context output.

# Version History

## V1.1.1

- Reviewed V1.1.0.1 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Moved iOS inventory import orchestration from `app_runner.cpp` into `EvidenceIntake::importIosInventoryCsvs(...)`.
- Moved cache-SQLite iOS inventory helpers and referenced-path lookup import into `src/ingest/evidence_intake.cpp`.
- Added `EvidenceIntake::importReferencedIosPathLookupFromReuseCache(...)` and preserved run-status reporting through callback injection.
- Replaced detached GUI main ingest worker with a tracked `gIngestThread` that is joined during `WM_DESTROY`.
- Cleared the V1.1.0.1 `apfs_aff4_reader.cpp` C4100 warning without changing behavior.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- Local syntax checks, Linux CMake build, CLI version check, and self-test passed.
- Windows/MSVC build and AFF4/APFS/iOS runtime validation remain required.
- No APFS live traversal replacement, AFF4 read semantic change, Store-V2 parser change, SQLite schema change, or new forensic interpretation output was intentionally added.

## V1.0.30

- Reviewed V1.0.29 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Moved iOS app database record-inventory orchestration into `IosAppDbParser::parseRecordInventories(...)`.
- Reduced `app_runner.cpp` iOS app DB inventory function to a delegating wrapper with status callback preservation.
- Added GUI export thread registry and joined active export workers during `WM_DESTROY` instead of detaching Export Page/Filtered/Checked/Tagged workers.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- No AFF4/APFS traversal, copy-out, Store-V2 parsing, iOS CoreSpotlight schema, or forensic interpretation changes.


V1.0.29 is a narrow hardening release after V1.0.28.2 linked successfully but the PowerShell build wrapper still checked for the stale `1.0.27` version string.

## Changes

- Corrected the versioned PowerShell build wrapper to expect `1.0.29`.
- Closed the parent process copy of redirected subprocess log handles immediately after child process creation.
- Replaced global DLL-directory mutation with `LoadLibraryExW` secure per-module loading for the guarded AFF4 dynamic probe.
- Suspended Win32 ListView redraw during bulk review grid population.
- Added a 50 MB cap for dynamically copied thin-upload export CSVs in both the C++ upload bundler and the standalone PowerShell thin-upload helper.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.

## Validation

- Local C++20 syntax checks passed for `src/app/app_runner.cpp`, `src/parsers/apfs_diagnostic_exporter.cpp`, `src/gui/gui_export_worker.cpp`, and `src/core/app_info.cpp`.
- CMake configure completed with Apple/lzfse detected.
- Windows/MSVC validation remains required.

# V1.0.28.2

V1.0.28.2 is a narrow build/link hotfix after the V1.0.28.1 MSVC build failed with duplicate `isLikelyStoreV2GroupDirectoryName` symbols between `app_runner.obj` and `apfs_diagnostic_exporter.obj`.

## Changes

- Scoped the APFS diagnostic exporter copy of `isLikelyStoreV2GroupDirectoryName()` to the exporter translation unit.
- Updated continuation, roadmap, and suggestions tracker files.
- No extraction, parser, schema, GUI, or forensic interpretation behavior changed.

## Validation

- Local syntax checks were run for `src/parsers/apfs_diagnostic_exporter.cpp`, `src/app/app_runner.cpp`, and `src/core/app_info.cpp`.
- A local object-symbol check confirmed `apfs_diagnostic_exporter.o` no longer exports a public `isLikelyStoreV2GroupDirectoryName` symbol.
- Windows/MSVC validation remains required.

# V1.0.28.1

- Build hotfix for the V1.0.28 Windows/MSVC failure in `src\app\app_runner.cpp` where `asciiLower` was used before declaration after APFS diagnostic writer relocation.
- Added a forward declaration for the existing runner-local helper.
- Kept V1.0.28 APFS diagnostic writer relocation intact.
- No APFS traversal, AFF4 reads, copy-out/staging, Store-V2 parsing, iOS parsing, schema, or GUI behavior was intentionally changed.

# V1.0.28.1

V1.0.28.1 is a thin-upload packaging hotfix after the V1.0.26 AFF4/APFS run and external comparison completed but the thin ZIP failed during PowerShell relative-path inventory generation.

## Changed

- Fixed `tools/Create-SourceProbeUploadZip.ps1` so `Get-RelativePathForThinInventory` no longer uses `[char]'\\'`, which Windows PowerShell treats as a two-character string and rejects.
- Reused the robust relative-path helper for `ExtractedSpotlight` copy paths.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- Added `scripts/Package-V1_0_28_1-macOS-AFF4-ThinFromExistingCase.ps1` for packaging an already-completed V1.0.26 AFF4/APFS case without rerunning the probe.
- Added `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, and `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.

## Not changed

- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.

## Validation

- Reviewed the uploaded V1.0.26 build log; the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.
- Reviewed the user-reported wrapper output showing the AFF4/APFS probe and external comparison completed before packaging failed.
- Local syntax/text checks were performed for modified C++ and PowerShell packaging files. Windows/MSVC V1.0.28.1 validation remains required.

# V1.0.26

- Fixed the remaining thin-upload raw-log leak in the standalone source-probe upload tool by denying raw tool outputs and full raw file inventories.
- Added matching in-app thin-upload deny-list policy for raw AFF4/iOS tool logs and full file-inventory CSVs.
- Updated thin-upload inventory text files to report relative paths instead of full local paths.
- Added bounded hidden Windows subprocess waits to avoid indefinite hangs from prompted/wedged external tools.
- Updated exact AFF4/ZIP byte reads on Windows to use 64-bit `_fseeki64`.
- No APFS traversal, Store-V2 parsing, iOS parsing, GUI schema, or APFS diagnostic writer movement is included in this version.

# V1.0.25

- Fixed the V1.0.24 Windows/MSVC `C2668` ambiguous `buildWhere` compile failure in `src/gui/win32_gui.cpp`.
- Removed the stale local `buildWhere` wrapper left behind after creating `src/gui/gui_view_helpers.h/.cpp`.
- Explicitly routed review-page SQL `WHERE` assembly through the shared `vestigant::spotlight::buildWhere(...)` helper using captured filter state.
- No APFS/AFF4 traversal, Store-V2 parsing, iOS parsing, schema, GUI views, or diagnostic writer behavior was intentionally changed.
- Updated V1.0.25 build/launch/AFF4 wrapper scripts.

# V1.0.18

- Vendored the uploaded Apple/lzfse source tree under `third_party/lzfse`.
- Enabled codec-aware builds when Apple/lzfse is present.
- Added a codec smoke test using a known Apple/lzfse-produced LZVN vector.
- Added AFF4/APFS copy-out summary fields for codec status and decmpfs LZVN/LZFSE row counts.
- Added macOS investigative feature inventory and roadmap documentation.

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

## V1.0.28.1

- Reviewed V1.0.27 Windows/MSVC build and macOS AFF4/APFS thin output.
- Moved the main APFS/AFF4 diagnostic writer families from `src/app/app_runner.cpp` into `src/parsers/apfs_diagnostic_exporter.cpp`.
- Expanded `src/parsers/apfs_diagnostic_exporter.h` with typed writer declarations.
- Kept APFS traversal, Store-V2 parsing, iOS parsing, SQLite schema, GUI behavior, and live extraction behavior unchanged.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
