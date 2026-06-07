# Spotlight2 Workflow Ledger

## V1.1.9 update

- Current generated source package: V1.1.9.
- Validated baseline reviewed before this version: V1.1.8 Windows/MSVC build and macOS AFF4/APFS thin output.
- Main change: guarded live APFS OMAP horizontal leaf traversal with bounded next-leaf transitions.
- Source-package `.md`, `.txt`, and `.ps1` file review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.md`.


## Current cycle — V1.1.7.1

- Active package under development: `VestigantSpotlightInv_V1_1_7_1.zip`
- Baseline source reviewed: `VestigantSpotlightInv_V1_1_7.zip`
- V1.1.7 Windows/MSVC build failure reviewed: missing helpers in `src/parsers/aff4_probe_worker.cpp` after dynamic AFF4/APFS probe relocation.
- Root cause: `writeAff4CppLiteDynamicLoadProbe(...)` was physically moved to `aff4_probe_worker.cpp`, but these app-runner-local helpers were not moved/exposed:
  - `shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(...)`
  - `findToolCandidate(...)`
  - `lastWindowsErrorString(...)`
- V1.1.7.1 fix: define worker-local helper boundary in `aff4_probe_worker.cpp` and preserve internal linkage.
- Package cleanup: old version-specific scripts and root-level old package artifacts/manifests removed; append-only history preserved in `docs/`.
- Next validation gate: Windows/MSVC `V1_1_7_1_build.log`, then macOS AFF4/APFS thin ZIP.

## Current known-good baseline

- V1.1.6.1: Windows/MSVC build passed and macOS AFF4/APFS thin output reviewed.
- V1.1.7: local Linux validation passed, but Windows/MSVC failed due missing worker helpers.
- V1.1.7.1: generated as build hotfix and package cleanup; Windows/MSVC validation pending.

## Repeat workflow checklist

1. Review newest uploaded source ZIP, build log, and thin output before editing.
2. Read this file, `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`, and `docs/FULL_VERSION_HISTORY.md`.
3. Prefer larger coordinated work only when changes are behavior-preserving, reversible, and compile-checkable.
4. Do not emit placeholder forensic interpretations. Comparator/scaffolding is acceptable when clearly not wired into live output.
5. Keep append-only version history. Do not truncate old versions.
6. Package full source ZIP, patch ZIP, unified diff, SHA256 files, validation notes, and concrete PowerShell commands.

## Known packaging pitfalls to avoid

- Do not omit root build scripts from full source ZIP.
- Do not leave stale version checks in `scripts/Build-*.ps1`.
- Do not keep old version-specific wrappers in `scripts/` once a current wrapper exists, unless specifically needed for migration.
- Do not delete old version history; consolidate it into append-only docs.
- When moving a function out of `app_runner.cpp`, move or expose all app-runner-local helper dependencies at the same time.

---

# Spotlight2 Workflow Ledger

## Purpose

This ledger is the first file to review during every `repeat` cycle. It records the working baseline, successful and failed attempts, current pitfalls, and validation gates so the next pass does not rediscover the same workflow issues.

## Current cycle

- Active package under development: `VestigantSpotlightInv_V1_1_4.zip`
- Baseline source reviewed: `VestigantSpotlightInv_V1_1_3.zip`
- Baseline Windows/MSVC build reviewed: `V1_1_3_build.log`
- Baseline macOS AFF4/APFS thin ZIP reviewed: `Upload_Thin_MacOS_AFF4_V1_1_3.zip`
- Baseline status: V1.1.3 built successfully and generated a thin ZIP with denied raw upload files absent.

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
- PowerShell `[char]'\'` relative-path bug.
- Unscoped anonymous/local helpers colliding with shared helper modules.
- Treating pseudocode APFS path/leaf traversal as live forensic output before comparator validation.

## V1.1.3 implementation notes

Implemented in this cycle:

- Added cooperative cancellation callbacks to GUI export worker requests and checked/tagged support exports.
- Threaded export loops now abort on GUI shutdown before continuing long SQLite scans.
- Added secure System32 loading for `Msftedit.dll` when RichEdit is available, with cleanup in `WM_DESTROY`.
- Wrapped orphan-source-row purge deletes in one transaction while preserving per-table warning behavior.
- Strengthened non-live APFS next-leaf helper scaffolding and allowed `ApfsVolumeReader::enumerateDirectory(...)` to use the footer helper when no injected next-leaf callback exists.
- Retained existing V1.1.2 bplist trailer validation; full NSKeyedArchiver graph decoding remains deferred.

Deferred in this cycle:

- Full `writeAff4CppLiteDynamicLoadProbe` extraction to `aff4_probe_worker.cpp`.
- Full `stageZipEvidenceSource(...)` relocation.
- Live APFS horizontal leaf traversal replacement in the AFF4 extraction path.
- Live APFS absolute path reconstruction.
- Full NSKeyedArchiver UID graph unflattening.

## Next candidate work

- Move `stageZipEvidenceSource(...)` only after V1.1.3 validates under Windows/MSVC and thin packaging.
- Extract `writeAff4CppLiteDynamicLoadProbe(...)` by first moving state structs and callback interfaces, then behavior unchanged.
- Add APFS B-tree/absolute-path comparator CSVs before live staging changes.
- Extend bplist parser from trailer/object-string discovery toward a bounded UID graph model.

## V1.1.4 implementation notes

Implemented in this cycle:

- Added bplist offset-table validation metadata to the existing bounded CoreSpotlight bplist context output, including offset-table byte count and top-object relative offset where valid.
- Added GUI checked-artifact snapshot helpers used by page load/export request construction to avoid direct long-lived references to mutable checked-state containers.
- Strengthened the GUI ingest launch gate with `compare_exchange_strong` so repeated/double-click `Build / Process Case` requests are dropped before a second worker can be created.
- Preserved V1.1.3 live APFS extraction behavior; APFS reverse-path and live horizontal-leaf substitution remain deferred until comparator evidence exists.

Deferred in this cycle:

- Full `writeAff4CppLiteDynamicLoadProbe` extraction to `aff4_probe_worker.cpp`.
- Full `stageZipEvidenceSource(...)` relocation.
- Live APFS absolute path reconstruction and live B-tree next-leaf traversal replacement.
- Full NSKeyedArchiver UID graph decoding.


## V1.1.5 implementation notes

Baseline reviewed:

- `V1_1_4_build.log`: Windows/MSVC build completed and binary version reported `Vestigant Spotlight v1.1.4`.
- `Upload_Thin_MacOS_AFF4_V1_1_4.zip`: generated successfully; denied raw upload filenames were absent.
- Current review requested deeper cancellation propagation, dynamic probe extraction, upload-sample size guarding, UTF-8 PowerShell extraction logs, and local diagnostic sample export error handling.

Implemented in this cycle:

- AFF4 dynamic/direct probe signatures now accept the existing ingest cancellation token.
- Selected expensive bounded APFS/AFF4 loops now check cancellation.
- Case output directory writability is probed before logger/database initialization.
- Thin-upload `exports/upload_samples` copying is policy-guarded and size-capped.
- Standalone thin-upload helper applies the nested upload-samples size policy.
- Focused iOS 7-Zip extraction logs use UTF-8 `Out-File` rather than default PowerShell redirection.
- APFS staged Store-V2 diagnostic sample CSV export failures are caught locally.

Deferred in this cycle:

- Full `aff4_probe_worker.cpp` extraction.
- Full `stageZipEvidenceSource(...)` relocation.
- Live APFS horizontal leaf substitution and path reconstruction.
- Full NSKeyedArchiver UID/object graph decoding.

## V1.1.5.1 workflow entry

- Baseline: V1.1.5 package.
- Trigger: MSVC C2440 at `src/app/app_runner.cpp(5760)` during V1.1.5 build.
- Root cause: cancellation propagation returned `false` inside a lambda returning `ApfsOmapTargetResolution`.
- Fix: return the local `ApfsOmapTargetResolution out` with cancellation status fields populated.
- Validation required: Windows/MSVC build and macOS AFF4/APFS thin run.
- Do not repeat: when adding cancellation checks inside typed lambdas/functions, return the function's declared type, not a generic boolean sentinel.

## V1.1.6 workflow entry

Baseline reviewed: V1.1.5.1 build log and macOS AFF4 thin ZIP. Build reported `Vestigant Spotlight v1.1.5.1`; thin ZIP existed and denied raw upload files were absent.

Requested focus: Tracker #17 God-closure remediation for `writeAff4CppLiteDynamicLoadProbe` and `writeAff4DirectMapReaderProbe`.

Attempted: full physical extraction of both functions into `src/parsers/aff4_probe_worker.cpp`. The dynamic-load function exposed a large dependency surface on app-runner-local helper structs/functions (`Aff4DynamicProbeRow`, `ApfsOmapTargetResolution`, `ApfsDirectoryRecordEntry`, ZIP helpers, status helpers, and APFS decode helpers). Moving it safely requires a dedicated worker context or a staged helper-boundary module.

Completed: physical extraction of `writeAff4DirectMapReaderProbe` into `Aff4ProbeWorker::executeDirectMapReaderProbe(...)`; app-runner call sites now delegate to the worker.

Do not repeat: do not create a wrapper-only `aff4_probe_worker` and claim Tracker #17 is fixed. The direct-map body is now physically moved. The dynamic-load body is still pending and must be handled in a separate dependency-boundary pass.


## V1.1.6.1 build-hotfix note

V1.1.6 moved the direct-map AFF4/APFS probe into `src/parsers/aff4_probe_worker.cpp`, but the MSVC build exposed a Windows-only missing helper: `wideProcessPath`. V1.1.6.1 adds a local Windows helper in the worker and corrects the versioned build script gate. This is recorded as a repeat-process pitfall: after moving code from `app_runner.cpp`, grep for Windows-only helper dependencies that Linux syntax checks cannot see.


## V1.1.7 update

- Baseline: validated V1.1.6.1.
- Completed Tracker #17 major step: moved `writeAff4CppLiteDynamicLoadProbe(...)` from `app_runner.cpp` into `Aff4ProbeWorker::executeDynamicLoadProbe(...)` in `src/parsers/aff4_probe_worker.cpp`.
- `writeAff4DirectMapReaderProbe(...)` had already been moved in V1.1.6; both large AFF4/APFS probe bodies now live in `aff4_probe_worker.cpp`.
- Added cancellation checks into the shared APFS OMAP traversal helper so the direct-map and dynamic-load paths can stop during B-tree traversal.
- Remaining related work: compare V1.1.7 Windows build and thin output against V1.1.6.1; then clean duplicated/unused helper functions and continue moving staged probe workers only after parity is confirmed.

## V1.1.8 Update

- `BaselineVersionHistory.md` is now the append-only version-history baseline in `docs/FULL_VERSION_HISTORY.md` and `VERSION_HISTORY.md`.
- Windows long-path evidence writes were added for APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction output paths.
- SQLite WAL checkpoint/truncate is requested before upload packaging.
- Logger writes are mutex-protected for concurrent GUI/export/ingest paths.
- APFS decmpfs reconstruction remains bounded; the expected-output safety cap is now 256 MiB.

