# Spotlight2 Roadmap Checklist

## Current stability gate

- [x] V1.0.24.1: Windows/MSVC build passed after GUI helper ambiguity hotfix.
- [x] V1.0.25: Windows/MSVC build passed.
- [x] V1.0.26: Windows/MSVC build passed and reported `Vestigant Spotlight v1.0.26`.
- [x] V1.0.26.1: Windows/MSVC build passed and reported `Vestigant Spotlight v1.0.26.1`.
- [x] V1.0.26.1: macOS AFF4/APFS thin ZIP generated and reviewed.
- [x] V1.0.26.1: Thin ZIP excludes denied raw logs and inventories.
- [x] V1.0.27: Windows/MSVC build passed and reported `Vestigant Spotlight v1.0.27`.
- [x] V1.0.27: macOS AFF4/APFS thin ZIP generated and reviewed.
- [x] V1.0.28: Windows/MSVC build attempted; failed on missing `asciiLower` declaration after APFS writer relocation.
- [x] V1.0.28.2: binaries linked and reported `Vestigant Spotlight v1.0.28.2`; wrapper failed stale version check.
- [x] V1.0.28.2: macOS AFF4/APFS thin ZIP generated and reviewed.
- [x] V1.0.29: Windows/MSVC build passed and reported `Vestigant Spotlight v1.0.29`.
- [x] V1.0.29: macOS AFF4/APFS thin ZIP generated and reviewed.
- [x] V1.0.30: Windows/MSVC build passed and reported `Vestigant Spotlight v1.0.30`.
- [x] V1.0.30: macOS AFF4/APFS thin ZIP generated and reviewed.
- [x] V1.1.0.1: Windows/MSVC build passed and reported `Vestigant Spotlight v1.1.0.1`.
- [x] V1.1.0.1: macOS AFF4/APFS thin ZIP generated and reviewed.
- [ ] V1.1.1: Windows/MSVC build pending.
- [ ] V1.1.1: macOS AFF4/APFS thin ZIP pending.
- [x] V1.1.1: Local Linux CMake build/self-test passed before packaging.

## Thin upload / redaction

- [x] V1.0.25: Removed major raw AFF4/iOS tool logs from in-app thin upload copy list.
- [x] V1.0.25: Dynamic top-level `exports/*.csv` copying added.
- [x] V1.0.26: Added deny-list policy to in-app and standalone thin-upload helper.
- [x] V1.0.26: Converted case/additional inventories to relative paths.
- [x] V1.0.26.1: Fixed Windows PowerShell `[char]'\\'` packaging crash.
- [x] V1.0.26.1: Added packaging-only wrapper for an already-completed AFF4/APFS case.
- [x] V1.0.27: Added generated-ZIP deny-list self-check.
- [x] V1.0.29: Added 50 MB cap for dynamically copied top-level export CSVs.
- [ ] Decide whether `VestigantSpotlight.log` should be full, tail-only by default, or redacted summary-only.

## GUI review and export

- [x] V1.0.19: Fixed iOS GUI dropdown/detail-pane layout issues.
- [x] V1.0.22: Moved Export Page and Export Filtered SQL/CSV backend logic into `GuiExportWorker`.
- [x] V1.0.24 / V1.0.24.1: Added shared GUI view/export helper module and fixed MSVC ambiguity.
- [x] V1.0.27: Added bounded SQLite busy retry handling for GUI review/export read connections.
- [x] V1.0.29: Suspended ListView redraw during bulk review-page population.
- [x] V1.1.1: Added `case_sensitive_like=OFF` to GUI read/export connections while preserving current search semantics.
- [x] V1.0.30: Registered GUI export worker threads and joined them on `WM_DESTROY` instead of detaching export threads.
- [x] V1.1.1: Main GUI ingest/build worker is tracked and joined on `WM_DESTROY` instead of detached.
- [ ] Confirm Export Page, Export Filtered, Export Checked, and Export Tagged in live Windows GUI.
- [ ] Keep iOS and macOS views separated by `ViewPlatform`, not string-prefix checks.
- [ ] Consider gradual `VestigantMainWindow` state object only after current exports are stable.

## APFS / AFF4 extraction and diagnostics

- [x] V1.0.18: Last dual thin validation baseline for macOS AFF4/APFS and iOS.
- [x] V1.0.23: Moved APFS diagnostic row models into `apfs_diagnostic_models.h`.
- [x] V1.0.28: Moved the main APFS diagnostic/report writer families into `apfs_diagnostic_exporter.cpp`.
- [x] V1.0.28.1: Added build hotfix for `asciiLower` declaration ordering after writer relocation.
- [x] V1.0.28.2: Fixed APFS diagnostic exporter duplicate linker symbol (`isLikelyStoreV2GroupDirectoryName`).
- [x] V1.0.29: Switched AFF4 dynamic probe loading to per-module `LoadLibraryExW` secure search flags.
- [x] V1.1.1: Moved AFF4 stream inventory classification/reporting into `apfs_aff4_reader.cpp` with callback-injected tool/process execution.
- [x] V1.1.1: Moved APFS NX superblock parsing into `apfs_volume_reader.cpp`.
- [x] V1.1.1: Moved `writeAff4ApfsV1DiagnosticRerunPlan()` into `apfs_diagnostic_exporter.cpp`.
- [ ] Move remaining APFS exact signature scan writer out of `app_runner.cpp` after V1.1.1 validates.
- [ ] Do not replace live APFS traversal with lower-bound iterator until comparator parity/improvement is proven.
- [ ] Preserve sparse gap, zero physical block, unresolved read failure, partial reconstruction, and hash provenance.
- [ ] Add APFS lower-bound iterator comparator outputs before changing live extraction.
- [x] V1.1.1: Added non-live APFS path/leaf helper API scaffolding for future comparator work.
- [ ] Add APFS absolute path reconstruction to live/staged outputs only after validated catalog parent/name lookup comparator exists.

## iOS investigative mode

- [x] Preserve iOS CoreSpotlight-first workflow.
- [x] Preserve parsed app records, KnowledgeC, WhatsApp, Apple Messages, bplist/NSKeyedArchiver, Missing From FFS, and text-context views.
- [x] V1.0.30: Moved iOS app database record-inventory orchestration into `ios_app_db_parser.cpp` and left `app_runner.cpp` as a delegating wrapper.
- [ ] Validate current iOS thin output on a current post-V1.0.30 build.
- [ ] Keep normal iOS mode compact; do not reintroduce broad FFS database dumping by default.
- [ ] Add real bounded NSKeyedArchiver UID/object unflattening only after a real bplist object model exists.

## Performance and stability

- [x] V1.0.25: Faster CSV row counting via binary chunk newline count.
- [x] V1.0.25: Safer iOS app database staging path normalization.
- [x] V1.1.1: Added `src/ingest/evidence_intake.*` and moved intake helper functions out of `app_runner.cpp`.
- [x] V1.1.1: Moved iOS CSV/cache inventory import and referenced-path lookup import into `EvidenceIntake`.
- [ ] Move full `stageZipEvidenceSource(...)` after V1.1.1 intake-import validation.
- [x] V1.0.26: Hidden process timeout added.
- [x] V1.0.26: Large-offset AFF4/ZIP byte reads hardened on Windows with `_fseeki64`.
- [x] V1.0.27: Added Win32 Job Object wrapping for hidden external process trees.
- [x] V1.0.29: Closed redirected subprocess log handle in the parent immediately after child creation.
- [x] V1.1.1: Added temporary SQLite PRAGMAs around regenerable iOS CSV fallback import and restore WAL/NORMAL after import.
- [x] V1.1.1: Moved decmpfs/resource-fork reconstruction helpers into the codec module for focused future testing.
- [ ] Validate V1.1.1 iOS CSV import runtime counts on a current iOS thin/test run.
- [x] V1.1.1: Opened `CaseDatabase` once in `runApplication()` and reused the handle through AFF4/raw and general workflow.
- [ ] Validate V1.1.1 database-lifetime cleanup on Windows/MSVC and a current iOS run.
- [x] V1.1.1: Suppressed platform-specific AFF4 stream inventory callback warning while preserving callback signature.

## Documentation / continuity

- [x] V1.0.26.1: Added `docs/CONTINUATION_HANDOFF.md`.
- [x] V1.0.26.1: Added `docs/ROADMAP_CHECKLIST.md`.
- [x] V1.0.26.1: Added `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.
- [x] V1.0.28: Updated all three continuity files.
- [x] V1.0.28.2: Updated all three continuity files and added build-hotfix note.
- [x] V1.0.29: Updated all three continuity files.
- [x] V1.0.30: Updated all three continuity files.
- [x] V1.1.1: Updated all three continuity files and documented the broader `repeat` shorthand.
- [ ] Update these three files in every future package.
