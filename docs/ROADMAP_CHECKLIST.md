# Spotlight2 Roadmap Checklist

## Current stability gate

- [x] V1.0.24.1: Windows/MSVC build passed after GUI helper ambiguity hotfix.
- [x] V1.0.25: Windows/MSVC build passed.
- [x] V1.0.26: Windows/MSVC build passed and reported `Vestigant Spotlight v1.0.26`.
- [x] V1.0.26.1: Windows/MSVC build passed and reported `Vestigant Spotlight v1.0.26.1`.
- [x] V1.0.26.1: macOS AFF4/APFS thin ZIP generated and reviewed.
- [x] V1.0.26.1: Thin ZIP excludes denied raw logs and inventories.
- [ ] V1.0.27: Windows/MSVC build pending.
- [ ] V1.0.27: macOS AFF4/APFS thin ZIP pending.

## Thin upload / redaction

- [x] V1.0.25: Removed major raw AFF4/iOS tool logs from in-app thin upload copy list.
- [x] V1.0.25: Dynamic top-level `exports/*.csv` copying added.
- [x] V1.0.26: Added deny-list policy to in-app and standalone thin-upload helper.
- [x] V1.0.26: Converted case/additional inventories to relative paths.
- [x] V1.0.27: Fixed Windows PowerShell `[char]'\\'` packaging crash.
- [x] V1.0.27: Added packaging-only wrapper for an already-completed AFF4/APFS case.
- [x] V1.0.27: Add generated-ZIP deny-list self-check.
- [ ] Decide whether `VestigantSpotlight.log` should be full, tail-only by default, or redacted summary-only.

## GUI review and export

- [x] V1.0.19: Fixed iOS GUI dropdown/detail-pane layout issues.
- [x] V1.0.22: Moved Export Page and Export Filtered SQL/CSV backend logic into `GuiExportWorker`.
- [x] V1.0.24 / V1.0.24.1: Added shared GUI view/export helper module and fixed MSVC ambiguity.
- [ ] Confirm Export Page, Export Filtered, Export Checked, and Export Tagged in live Windows GUI.
- [ ] Keep iOS and macOS views separated by `ViewPlatform`, not string-prefix checks.
- [ ] Consider gradual `VestigantMainWindow` state object only after current exports are stable.

## APFS / AFF4 extraction and diagnostics

- [x] V1.0.18: Last dual thin validation baseline for macOS AFF4/APFS and iOS.
- [x] V1.0.23: Moved APFS diagnostic row models into `apfs_diagnostic_models.h`.
- [ ] Move first APFS diagnostic writer family into `apfs_diagnostic_exporter.cpp`.
- [ ] Move remaining APFS diagnostic writer families after each build validates.
- [ ] Do not replace live APFS traversal with lower-bound iterator until comparator parity/improvement is proven.
- [ ] Preserve sparse gap, zero physical block, unresolved read failure, partial reconstruction, and hash provenance.
- [ ] Add APFS lower-bound iterator comparator outputs before changing live extraction.

## iOS investigative mode

- [x] Preserve iOS CoreSpotlight-first workflow.
- [x] Preserve parsed app records, KnowledgeC, WhatsApp, Apple Messages, bplist/NSKeyedArchiver, Missing From FFS, and text-context views.
- [ ] Validate current iOS thin output on a current post-V1.0.26 build.
- [ ] Keep normal iOS mode compact; do not reintroduce broad FFS database dumping by default.

## Performance and stability

- [x] V1.0.27: Add Win32 Job Object wrapping for hidden external process trees.
- [x] V1.0.27: Add bounded SQLite busy retry handling for GUI review/export read connections.
- [x] V1.0.25: Faster CSV row counting via binary chunk newline count.
- [x] V1.0.25: Safer iOS app database staging path normalization.
- [x] V1.0.26: Hidden process timeout added.
- [x] V1.0.26: Large-offset AFF4/ZIP byte reads hardened on Windows with `_fseeki64`.
- [ ] Evaluate SQLite ingestion PRAGMAs in a controlled dev-only import phase.
- [ ] Evaluate database open/close lifetime cleanup only after current runs are stable.

## Documentation / continuity

- [x] V1.0.27: Added `docs/CONTINUATION_HANDOFF.md`.
- [x] V1.0.27: Added `docs/ROADMAP_CHECKLIST.md`.
- [x] V1.0.27: Added `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.
- [ ] Update these three files in every future package.
