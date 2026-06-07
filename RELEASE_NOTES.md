
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
