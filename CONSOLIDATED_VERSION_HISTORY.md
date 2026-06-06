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
