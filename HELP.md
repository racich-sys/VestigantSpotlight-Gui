# Vestigant Spotlight Help — V1.1.10.1

## Current build

- Current source version: `1.1.10.1`.
- Current source ZIP: `VestigantSpotlightInv_V1_1_10_1.zip`.
- V1.1.10.1 is a documentation/script command hotfix on V1.1.10.

## Build

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_1_10_1.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_10_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_10_1.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Build-V1_1_10_1.ps1
```

## Launch GUI

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Launch-V1_1_10_1-GUI.ps1
```

## macOS AFF4/APFS thin regression

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

See `docs/NEW_CHAT_CONTINUATION_GUIDE.md` for the full continuation package and upload requirements.
