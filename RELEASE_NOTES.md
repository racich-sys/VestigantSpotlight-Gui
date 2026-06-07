## V1.2.0 update

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
- Reason: V1.2.0 changes Win32 GUI review-grid rendering behavior and documentation only. The V1.1.11 build and AFF4/APFS thin output were reviewed before this version; no extraction/traversal/copy-out/decompression/parser code changed.
- Trigger for escalating AFF4/APFS to full test: any next change to live APFS traversal, copy-out, decompression, extent handling, path reconstruction, external compare logic, or Store-V2 staging behavior.
- Trigger for iOS testing: any next change to iOS ZIP staging, CoreSpotlight parsing, FFS lookup, app DB parsing, bplist/NSKeyedArchiver handling, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_2_0_build.log` and `Upload_Thin_MacOS_AFF4_V1_2_0.zip`.

## Standard V1.2.0 build command block

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_2_0.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_2_0" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_2_0.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_2_0\scripts\Build-V1_2_0.ps1
```

## Standard V1.2.0 AFF4/APFS thin command

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_2_0\scripts\Run-V1_2_0-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```


# V1.1.10.1 Release Notes

- Documentation/script-command hotfix on V1.1.10.
- Corrected the package-facing build instructions to the full pattern: `Set-Location D:\Downloads`, hash the ZIP, remove `T:\VestigantSpotlightInv_V1_1_10_1`, expand to `T:\`, then run `scripts\Build-V1_1_10_1.ps1`.
- Corrected macOS AFF4/APFS thin instructions to `scripts\Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut`.
- Updated `docs/NEW_CHAT_CONTINUATION_GUIDE.md` so a new chat can continue from the newest source upload without relying on prior chat history.
- No parser, extraction, GUI, Store-V2, iOS, schema, or forensic interpretation behavior was intentionally changed.

## V1_1_10

- Used V1.1.9.1 as the base.
- Performed a documentation/script/source-package hygiene pass.
- Regenerated V1.1.10 build, GUI launch, AFF4/APFS thin-run, and package-existing-case wrappers.
- Removed only clearly obsolete active-package artifacts: stale root-level V1.1.9 manifest/patch files and stale V1.1.9 source-review inventory files replaced by V1.1.10 review files.
- Preserved append-only version history and historical validation notes.
- No AFF4/APFS traversal, copy-out, staging, Store-V2 parsing, iOS parsing, GUI behavior, or SQLite schema behavior was intentionally changed.

## V1_1_10

- Promoted guarded live AFF4/APFS OMAP horizontal leaf traversal for APFS OMAP target lookups and volume root-tree lookups.
- Added bounded next-leaf transition handling with cycle detection, cancellation checks, and transition limits.
- Reviewed every `.md`, `.txt`, and `.ps1` source-package file and recorded decisions in `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.*`.
- Removed obsolete root-level prior-version package manifests while preserving append-only version history under `docs/`.
- No Store-V2 parser schema, iOS parser behavior, or GUI view semantics were intentionally changed.

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

# Vestigant Spotlight v1.0.30 Release Notes

## V1.0.30

- Reviewed V1.0.29 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Moved iOS app database record-inventory orchestration into `IosAppDbParser::parseRecordInventories(...)`.
- Reduced `app_runner.cpp` iOS app DB inventory function to a delegating wrapper with status callback preservation.
- Added GUI export thread registry and joined active export workers during `WM_DESTROY` instead of detaching Export Page/Filtered/Checked/Tagged workers.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- No AFF4/APFS traversal, copy-out, Store-V2 parsing, iOS CoreSpotlight schema, or forensic interpretation changes.


## Summary

V1.0.29 is a narrow stability and hardening release after V1.0.28.2 successfully linked binaries but the versioned PowerShell build wrapper still checked for `1.0.27`.

## Changes

- Corrected versioned build/launch/run scripts for V1.0.29 so the post-build CLI version check expects `1.0.29`.
- Closed the parent process copy of redirected subprocess log handles immediately after successful `CreateProcessW`.
- Replaced process-wide `SetDllDirectoryW`/`LoadLibraryW` use for the AFF4 dynamic probe with per-module `LoadLibraryExW` secure DLL search flags.
- Suspended Win32 ListView redraw during bulk row population to reduce GUI freezes on large review pages.
- Added a 50 MB cap for dynamically globbed thin-upload export CSVs in the C++ upload bundler.
- Added the same 50 MB export CSV cap to the standalone thin-upload PowerShell helper.
- Updated continuation, roadmap, and suggestions/fixes tracking files.

## Not changed

- No APFS traversal changes.
- No AFF4 read semantics changed.
- No copy-out/staging changes.
- No Store-V2 parser changes.
- No iOS parser changes.
- No SQLite schema changes.
- No GUI view/platform separation changes.
- No APFS reverse-path walker or NSKeyedArchiver unflattener was added.

## Validation status

Local syntax/configuration checks passed for the modified source files. Windows/MSVC build and runtime testing remain required.

## V1.1.7.1

- Build hotfix for V1.1.7 after Windows/MSVC exposed missing helper dependencies in `src/parsers/aff4_probe_worker.cpp`.
- Added worker-local helper boundary for known blocking AFF4 layout detection, reader tool resolution, and Win32 error reporting.
- Preserved both large AFF4/APFS probe bodies outside `app_runner.cpp`.
- Cleaned active source package by removing obsolete version-specific scripts and root-level old package manifests.
- Added append-only full version history baseline and new-chat continuation guide.

## V1.1.8 Update

- `BaselineVersionHistory.md` is now the append-only version-history baseline in `docs/FULL_VERSION_HISTORY.md` and `VERSION_HISTORY.md`.
- Windows long-path evidence writes were added for APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction output paths.
- SQLite WAL checkpoint/truncate is requested before upload packaging.
- Logger writes are mutex-protected for concurrent GUI/export/ingest paths.
- APFS decmpfs reconstruction remains bounded; the expected-output safety cap is now 256 MiB.

