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
- [ ] V1.0.28.2: Windows/MSVC build pending.
- [ ] V1.0.28.2: macOS AFF4/APFS thin ZIP pending.

## Thin upload / redaction

- [x] V1.0.25: Removed major raw AFF4/iOS tool logs from in-app thin upload copy list.
- [x] V1.0.25: Dynamic top-level `exports/*.csv` copying added.
- [x] V1.0.26: Added deny-list policy to in-app and standalone thin-upload helper.
- [x] V1.0.26: Converted case/additional inventories to relative paths.
- [x] V1.0.26.1: Fixed Windows PowerShell `[char]'\\'` packaging crash.
- [x] V1.0.26.1: Added packaging-only wrapper for an already-completed AFF4/APFS case.
- [x] V1.0.27: Added generated-ZIP deny-list self-check.
- [ ] Decide whether `VestigantSpotlight.log` should be full, tail-only by default, or redacted summary-only.

## GUI review and export

- [x] V1.0.19: Fixed iOS GUI dropdown/detail-pane layout issues.
- [x] V1.0.22: Moved Export Page and Export Filtered SQL/CSV backend logic into `GuiExportWorker`.
- [x] V1.0.24 / V1.0.24.1: Added shared GUI view/export helper module and fixed MSVC ambiguity.
- [x] V1.0.27: Added bounded SQLite busy retry handling for GUI review/export read connections.
- [ ] Confirm Export Page, Export Filtered, Export Checked, and Export Tagged in live Windows GUI.
- [ ] Keep iOS and macOS views separated by `ViewPlatform`, not string-prefix checks.
- [ ] Consider gradual `VestigantMainWindow` state object only after current exports are stable.

## APFS / AFF4 extraction and diagnostics

- [x] V1.0.18: Last dual thin validation baseline for macOS AFF4/APFS and iOS.
- [x] V1.0.23: Moved APFS diagnostic row models into `apfs_diagnostic_models.h`.
- [x] V1.0.28: Moved the main APFS diagnostic/report writer families into `apfs_diagnostic_exporter.cpp`.
- [x] V1.0.28.1: Added build hotfix for `asciiLower` declaration ordering after writer relocation.
- [ ] Move remaining APFS exact signature/rerun report writers out of `app_runner.cpp` after V1.0.28.2 validates.
- [ ] Do not replace live APFS traversal with lower-bound iterator until comparator parity/improvement is proven.
- [ ] Preserve sparse gap, zero physical block, unresolved read failure, partial reconstruction, and hash provenance.
- [ ] Add APFS lower-bound iterator comparator outputs before changing live extraction.
- [ ] Add APFS absolute path reconstruction only after validated catalog parent/name lookup exists.

## iOS investigative mode

- [x] Preserve iOS CoreSpotlight-first workflow.
- [x] Preserve parsed app records, KnowledgeC, WhatsApp, Apple Messages, bplist/NSKeyedArchiver, Missing From FFS, and text-context views.
- [ ] Validate current iOS thin output on a current post-V1.0.27 build.
- [ ] Keep normal iOS mode compact; do not reintroduce broad FFS database dumping by default.
- [ ] Add real bounded NSKeyedArchiver UID/object unflattening only after a real bplist object model exists.

## Performance and stability

- [x] V1.0.25: Faster CSV row counting via binary chunk newline count.
- [x] V1.0.25: Safer iOS app database staging path normalization.
- [x] V1.0.26: Hidden process timeout added.
- [x] V1.0.26: Large-offset AFF4/ZIP byte reads hardened on Windows with `_fseeki64`.
- [x] V1.0.27: Added Win32 Job Object wrapping for hidden external process trees.
- [ ] Evaluate SQLite ingestion PRAGMAs in a controlled dev-only import phase.
- [ ] Evaluate database open/close lifetime cleanup only after current runs are stable.

## Documentation / continuity

- [x] V1.0.26.1: Added `docs/CONTINUATION_HANDOFF.md`.
- [x] V1.0.26.1: Added `docs/ROADMAP_CHECKLIST.md`.
- [x] V1.0.26.1: Added `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.
- [x] V1.0.28: Updated all three continuity files.
- [x] V1.0.28.2: Updated all three continuity files and added build-hotfix note.
- [ ] Update these three files in every future package.
- [x] V1.0.28.2: Fixed APFS diagnostic exporter duplicate linker symbol from V1.0.28.1 (`isLikelyStoreV2GroupDirectoryName`).
- [ ] V1.0.28.2: Windows/MSVC build pending.
- [ ] V1.0.28.2: macOS AFF4/APFS thin ZIP pending.

