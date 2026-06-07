# Quick Start — V1.1.7.1

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
