# Troubleshooting — V1.1.7.1

## V1.1.9 update

- Current generated source package: V1.1.9.
- Validated baseline reviewed before this version: V1.1.8 Windows/MSVC build and macOS AFF4/APFS thin output.
- Main change: guarded live APFS OMAP horizontal leaf traversal with bounded next-leaf transitions.
- Source-package `.md`, `.txt`, and `.ps1` file review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.md`.


## Build script not found

Confirm the ZIP extracted to:

```text
T:\VestigantSpotlightInv_V1_1_7_1
```

and that the source root contains:

```text
build_windows_msvc.bat
build_windows_msvc_nocmake.bat
scripts\Build-V1_1_7_1.ps1
```

## Unexpected CLI version after build

Open the relevant `scripts\Build-*.ps1` file and confirm the version gate matches the source package version. For V1.1.7.1 it must match:

```text
1.1.7.1
```

## Missing helper compile errors in `aff4_probe_worker.cpp`

V1.1.7 moved the dynamic AFF4/APFS probe into `aff4_probe_worker.cpp`. V1.1.7.1 fixes the known missing helper errors for:

```text
shouldSkipLibAff4DynamicProbeForKnownBlockingLayout
findToolCandidate
lastWindowsErrorString
```

## Thin ZIP contains raw logs or inventories

The thin upload should not contain raw full inventories or raw tool logs. Review:

```text
docs/THIN_UPLOAD_REVIEW_WORKFLOW.md
```

Denied examples include:

```text
aff4_stream_inventory_raw.txt
ios_focused_zip_extract.log
ios_focused_zip_extract_7z.log
ios_focused_zip_extract.ps1
ios_ffs_file_inventory.csv
image_file_inventory.csv
```

## AFF4/APFS probe appears to hang

Use the GUI Cancel Ingest button where available. V1.1.5 and later propagate cancellation into more AFF4/APFS loops, but some lower-level reader/tool calls may still need additional cancellation checkpoints.

## New-chat continuation uncertainty

Use `docs/NEW_CHAT_CONTINUATION_GUIDE.md`. It lists the current baseline, paths, required external files, and repeat-process expectations.

## V1.1.8 Update

- `BaselineVersionHistory.md` is now the append-only version-history baseline in `docs/FULL_VERSION_HISTORY.md` and `VERSION_HISTORY.md`.
- Windows long-path evidence writes were added for APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction output paths.
- SQLite WAL checkpoint/truncate is requested before upload packaging.
- Logger writes are mutex-protected for concurrent GUI/export/ingest paths.
- APFS decmpfs reconstruction remains bounded; the expected-output safety cap is now 256 MiB.

