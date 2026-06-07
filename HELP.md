# Vestigant Spotlight Help — V1.1.7.1

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
