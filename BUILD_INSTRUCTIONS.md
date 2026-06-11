# Current Build and Test Instructions — V1_6_6_6

## Build

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_6_6.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_6_6" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_6_6.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_6\scripts\Build-V1_6_6_6.ps1
```

Expected CLI version after build:

```text
Vestigant Spotlight v1.6.6.6
```

## Launch GUI

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_6\scripts\Launch-V1_6_6_6-GUI.ps1
```

## iOS thin validation

Run iOS thin after the Windows build passes because V1.6.6.6 updates the GUI bootstrap iOS communication/native-DB comparison view and release-readiness checks.

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_6\scripts\Run-V1_6_6_6-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Optional no-CSV run:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_6\scripts\Run-V1_6_6_6-iOS-CoreSpotlight-AndZip.ps1 -CleanOut -NoCsvExports
```

Upload next:

```text
V1_6_6_6_build.log
Upload_Thin_iOS_CoreSpotlight_V1_6_6_6.zip
```

## AFF4/APFS test determination

AFF4/APFS full or thin validation is not required for V1.6.6.6 unless the Windows build, shared schema initialization, or APFS/AFF4 validation checks regress. V1.6.6.6 audits APFS cycle guards but does not modify APFS traversal, copy-out, decompression, source-reader, APFS filesystem reading, or Store-V2 staging behavior.
