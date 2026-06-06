# Spotlight2 Workflow Ledger

## Purpose

This ledger is the first file to review during every `repeat` cycle. It records the working baseline, successful and failed attempts, current pitfalls, and validation gates so the next pass does not rediscover the same workflow issues.

## Current cycle

- Active package under development: `VestigantSpotlightInv_V1_1_2.zip`
- Baseline source reviewed: `VestigantSpotlightInv_V1_1_1.zip`
- Baseline Windows/MSVC build reviewed: `V1_1_1_build.log`
- Baseline macOS AFF4/APFS thin ZIP reviewed: `Upload_Thin_MacOS_AFF4_V1_1_1.zip`
- Baseline status: V1.1.1 built successfully and generated a thin ZIP with denied raw upload files absent.

## Repeat workflow checklist

1. Review newest uploaded source ZIP, build log, and thin output before editing.
2. Read `docs/WORKFLOW_LEDGER.md`, `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, and `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.
3. Prefer larger coordinated work only when changes are behavior-preserving, reversible, and compile-checkable.
4. Do not emit placeholder forensic interpretations. Comparator/scaffolding is acceptable when clearly not wired into live output.
5. Run two source-review/validation passes before packaging.
6. Package full source ZIP, patch ZIP, unified diff, SHA256 files, patch manifest, validation notes, and exact PowerShell commands.

## Known prior failure modes to avoid

- Stale version regex in versioned build scripts.
- Full source ZIP missing root build scripts.
- Copied helpers causing duplicate linker symbols after modularization.
- Helper definitions moved below earlier call sites without forward declarations.
- Thin upload accidentally including raw AFF4/iOS inventories or absolute-path-heavy logs.
- PowerShell `[char]'\\'` relative-path bug.
- Unscoped anonymous/local helpers colliding with shared helper modules.

## V1.1.2 implementation notes

Implemented in this cycle:

- Added GUI ingest cancellation token plumbing and a visible `Cancel Ingest` control.
- Added safe cancellation checkpoints in `runApplication(...)`.
- Hardened AFF4 dependent DLL search by combining `SetDefaultDllDirectories`, `AddDllDirectory`, and `LoadLibraryExW`.
- Freed the GUI logo bitmap in `WM_DESTROY` to avoid a GDI handle leak.
- Added temporary regenerable bulk SQLite PRAGMAs around native Store-V2 parsing.
- Added bounded bplist trailer validation metadata to the existing bplist/NSKeyedArchiver context output without claiming full object-graph decode.

Deferred in this cycle:

- Full `writeAff4CppLiteDynamicLoadProbe` extraction to `aff4_probe_worker.cpp`.
- Full `stageZipEvidenceSource(...)` relocation.
- Live APFS horizontal leaf traversal replacement.
- Live APFS absolute path reconstruction.
- Full NSKeyedArchiver UID graph unflattening.

## Next candidate work

- Move `stageZipEvidenceSource(...)` only after V1.1.2 validates.
- Extract `writeAff4CppLiteDynamicLoadProbe(...)` by first moving parameter/state structs and callbacks, then moving behavior unchanged.
- Add APFS B-tree/absolute-path comparator CSVs before live staging changes.
- Extend bplist parser from trailer/object-string discovery toward a bounded UID graph model.
