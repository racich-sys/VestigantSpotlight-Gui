# Vestigant Spotlight Help — V1.1.7.1

## V1.1.9 update

- Current generated source package: V1.1.9.
- Validated baseline reviewed before this version: V1.1.8 Windows/MSVC build and macOS AFF4/APFS thin output.
- Main change: guarded live APFS OMAP horizontal leaf traversal with bounded next-leaf transitions.
- Source-package `.md`, `.txt`, and `.ps1` file review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.md`.


## Primary documentation

Start with these files:

- `docs/NEW_CHAT_CONTINUATION_GUIDE.md`
- `docs/CONSOLIDATED_USER_MANUAL.md`
- `docs/WORKFLOW_LEDGER.md`
- `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`
- `docs/ROADMAP_CHECKLIST.md`
- `docs/FULL_VERSION_HISTORY.md`

## Current technical status

- Current source version: `1.1.7.1`.
- V1.1.6.1 is the most recently reviewed Windows/MSVC + macOS AFF4/APFS thin-validated stable baseline.
- V1.1.7 moved both major AFF4/APFS probe bodies out of `app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp`.
- V1.1.7.1 fixes the Windows/MSVC helper-boundary failure from that move and cleans active source-package layout.

## Build

Use `BUILD_INSTRUCTIONS.md` or:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Build-V1_1_7_1.ps1
```

## GUI

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Launch-V1_1_7_1-GUI.ps1
```

## Thin upload safety

Thin upload packages deny raw inventory/log artifacts and enforce size limits on dynamic export samples. Review `docs/THIN_UPLOAD_REVIEW_WORKFLOW.md` and `docs/SOURCE_PACKAGE_CLEANUP_POLICY.md`.

## V1.1.8 Update

- `BaselineVersionHistory.md` is now the append-only version-history baseline in `docs/FULL_VERSION_HISTORY.md` and `VERSION_HISTORY.md`.
- Windows long-path evidence writes were added for APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction output paths.
- SQLite WAL checkpoint/truncate is requested before upload packaging.
- Logger writes are mutex-protected for concurrent GUI/export/ingest paths.
- APFS decmpfs reconstruction remains bounded; the expected-output safety cap is now 256 MiB.

