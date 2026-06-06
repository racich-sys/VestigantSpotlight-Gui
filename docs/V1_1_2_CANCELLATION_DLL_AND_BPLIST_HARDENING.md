# V1.1.2 Cancellation, DLL Search, Native Parse, and Bplist Hardening

## Scope

V1.1.2 is a broader repeat-cycle hardening release based on the validated V1.1.1 baseline. It addresses safe cancellation plumbing, dependent DLL search hardening, GUI resource cleanup, native Store-V2 parse performance, and bounded bplist trailer validation.

## Implemented

- Added GUI `Cancel Ingest` button and `gCancelIngestRequested` token.
- Added optional `std::atomic_bool` cancellation token to `runApplication(...)`.
- Added safe cancellation checkpoints around source probing, staging, discovery, native parse, and enrichment entry.
- Hardened AFF4 dynamic loading by using `SetDefaultDllDirectories(...)`, `AddDllDirectory(...)`, and `LoadLibraryExW(...)` with user/default search directories.
- Freed `gLogoBitmap` on `WM_DESTROY`.
- Applied temporary bulk SQLite PRAGMAs around native Store-V2 parse inserts and restored WAL/NORMAL settings afterward, including exception restoration.
- Added bounded bplist trailer validation metadata to the existing bplist context string.
- Added `docs/WORKFLOW_LEDGER.md` to avoid rediscovering prior build/package failures.

## Deliberately unchanged

- No AFF4/APFS read semantics changed.
- No live APFS traversal replacement.
- No live APFS path reconstruction.
- No full NSKeyedArchiver object graph decode is emitted.
- No SQLite schema changes.

## Validation required

- Windows/MSVC build.
- Windows GUI launch and Cancel Ingest button smoke test.
- macOS AFF4/APFS thin run.
- iOS run when practical because native bplist context metadata changed.
