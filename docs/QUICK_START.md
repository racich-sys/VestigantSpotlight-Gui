# V1.3.0 Standard Commands

## Build

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_3_0.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_3_0" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_3_0.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_3_0\scripts\Build-V1_3_0.ps1
```

## AFF4/APFS thin-create

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_3_0\scripts\Run-V1_3_0-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

---

# Quick Start — V1.1.10.1

## 1. Extract source and build

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_1_10_1.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_10_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_10_1.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Build-V1_1_10_1.ps1
```

## 2. Launch GUI

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Launch-V1_1_10_1-GUI.ps1
```

## 3. macOS AFF4/APFS thin regression

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

Expected output:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_1_10_1.zip
```

## 4. Continue in a new chat

Upload the newest source ZIP, build log, thin ZIP, `BaselineVersionHistory.md` if newer than the copy in this package, and any review notes. Tell the new chat to read:

- `docs/NEW_CHAT_CONTINUATION_GUIDE.md`
- `docs/WORKFLOW_LEDGER.md`
- `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`
- `docs/ROADMAP_CHECKLIST.md`
- `docs/FULL_VERSION_HISTORY.md`
