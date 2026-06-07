# Quick Start — V1.1.7.1

## V1.1.9 update

- Current generated source package: V1.1.9.
- Validated baseline reviewed before this version: V1.1.8 Windows/MSVC build and macOS AFF4/APFS thin output.
- Main change: guarded live APFS OMAP horizontal leaf traversal with bounded next-leaf transitions.
- Source-package `.md`, `.txt`, and `.ps1` file review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.md`.


## 1. Extract source

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_1_7_1.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_7_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_7_1.zip -DestinationPath T:\ -Force
```

## 2. Build

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Build-V1_1_7_1.ps1
```

## 3. Launch GUI

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Launch-V1_1_7_1-GUI.ps1
```

## 4. macOS AFF4/APFS regression

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Run-V1_1_7_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

## 5. Continue in a new chat

Upload the newest source ZIP, build log, thin ZIP, and any review notes. Tell the new chat to read:

- `docs/NEW_CHAT_CONTINUATION_GUIDE.md`
- `docs/WORKFLOW_LEDGER.md`
- `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`
- `docs/ROADMAP_CHECKLIST.md`
- `docs/FULL_VERSION_HISTORY.md`

## V1.1.8 Update

- `BaselineVersionHistory.md` is now the append-only version-history baseline in `docs/FULL_VERSION_HISTORY.md` and `VERSION_HISTORY.md`.
- Windows long-path evidence writes were added for APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction output paths.
- SQLite WAL checkpoint/truncate is requested before upload packaging.
- Logger writes are mutex-protected for concurrent GUI/export/ingest paths.
- APFS decmpfs reconstruction remains bounded; the expected-output safety cap is now 256 MiB.

