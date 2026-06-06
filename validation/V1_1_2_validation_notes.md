# V1.1.2 Validation Notes

## Baseline reviewed

- Source baseline: `VestigantSpotlightInv_V1_1_1.zip`.
- Build baseline: `V1_1_1_build.log`, which reported `Vestigant Spotlight v1.1.1`.
- Thin baseline: `Upload_Thin_MacOS_AFF4_V1_1_1.zip`; denied raw upload filenames were absent.
- Review prompt focus: ingest cancellation, dependent DLL loading hardening, GDI cleanup, dynamic-load probe modularization, APFS next-leaf work, bplist trailer parsing, native parser bulk PRAGMAs, and EvidenceIntake completion.

## Changes implemented

- Added `docs/WORKFLOW_LEDGER.md` as the first repeat-cycle state file.
- Added optional `std::atomic_bool` cancellation token to `runApplication`.
- Added GUI `Cancel Ingest` button and `gCancelIngestRequested` token.
- Added safe cancellation checkpoints around startup/source probing/staging/discovery/native parse/enrichment boundaries.
- Hardened AFF4 DLL loading path with `SetDefaultDllDirectories`, `AddDllDirectory`, and `LoadLibraryExW` using DLL/user/default search flags.
- Freed `gLogoBitmap` in `WM_DESTROY`.
- Applied temporary native Store-V2 bulk SQLite PRAGMAs around `NativeStoreDbParser::parseStores` and restored WAL/NORMAL settings after success/error.
- Added bounded bplist trailer validation metadata to existing bplist/NSKeyedArchiver context output.
- Updated continuation, roadmap, and suggestions/fixes tracking docs.

## Deferred

- Full dynamic-load probe extraction to `aff4_probe_worker.cpp`.
- Full `stageZipEvidenceSource(...)` relocation.
- Live APFS B-tree next-leaf traversal replacement.
- Live APFS absolute path reconstruction.
- Full NSKeyedArchiver UID graph decoding.

## Local validation performed

- C++20 syntax pass 1: `app_runner.cpp`, `native_storedb_parser.cpp`, `evidence_intake.cpp`, `apfs_aff4_reader.cpp`, `app_info.cpp`.
- C++20 syntax pass 2 with clang++: `app_runner.cpp`, `native_storedb_parser.cpp`, `evidence_intake.cpp`, `app_info.cpp`.
- Linux CMake configure/build completed.
- CLI version check returned `Vestigant Spotlight v1.1.2`.
- Local self-test passed.

## Not validated locally

- Windows/MSVC build.
- Windows GUI runtime.
- GUI Cancel Ingest behavior during long AFF4/iOS runs.
- V1.1.2 macOS AFF4/APFS thin run.
- Current iOS runtime parity.
