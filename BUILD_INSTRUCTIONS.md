# Build Instructions — Vestigant Spotlight V1.1.7.1

## V1.1.9 update

- Current generated source package: V1.1.9.
- Validated baseline reviewed before this version: V1.1.8 Windows/MSVC build and macOS AFF4/APFS thin output.
- Main change: guarded live APFS OMAP horizontal leaf traversal with bounded next-leaf transitions.
- Source-package `.md`, `.txt`, and `.ps1` file review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.md`.


## Windows/MSVC build

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_1_7_1.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_7_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_7_1.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Build-V1_1_7_1.ps1
```

Expected successful version output:

```text
Vestigant Spotlight v1.1.7.1
```

## Optional self-test

The build script prints the test command. The direct form is:

```powershell
& "T:\VestigantSpotlightInv_V1_1_7_1\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V1_1_7_1\build-msvc\selftest_out"
```

## GUI launch

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Launch-V1_1_7_1-GUI.ps1
```

## macOS AFF4/APFS thin regression

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Run-V1_1_7_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

Expected thin output:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_1_7_1.zip
```

## Source package cleanup note

This package intentionally keeps only current version-specific scripts in `scripts/`. Older version details are preserved in append-only documentation, not as old executable wrappers in the active source root.

## V1.1.8 Update

- `BaselineVersionHistory.md` is now the append-only version-history baseline in `docs/FULL_VERSION_HISTORY.md` and `VERSION_HISTORY.md`.
- Windows long-path evidence writes were added for APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction output paths.
- SQLite WAL checkpoint/truncate is requested before upload packaging.
- Logger writes are mutex-protected for concurrent GUI/export/ingest paths.
- APFS decmpfs reconstruction remains bounded; the expected-output safety cap is now 256 MiB.

