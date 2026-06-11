# V1.6.6.6 Continuation Guide

Upload or reference these files in a new chat:

- `VestigantSpotlightInv_V1_6_6_6.zip`
- `VestigantSpotlightInv_V1_6_6_6.zip.sha256`
- `V1_6_6_5_to_V1_6_6_6.patch`
- `V1_6_6_6_linux_validation.log`
- `V1_6_6_6_static_audit.log`
- `Upload_Thin_iOS_CoreSpotlight_V1_6_6_6.zip` when available
- `V1_6_6_6_build.log` when available

Current state:

V1.6.6.6 was produced after reviewing the V1.6.6.5 iOS thin bundle. The thin run completed successfully. Source inspection found that most queued forensic directives were already present in V1.6.6.5; the concrete gap fixed in V1.6.6.6 was the missing GUI bootstrap copy of `vw_ios_spotlight_comms_missing_from_ffs`.

Run next:

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_6_6.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_6_6" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_6_6.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_6\scripts\Build-V1_6_6_6.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_6\scripts\Run-V1_6_6_6-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

AFF4/APFS thin/full is not required unless the Windows build, shared schema initialization, or APFS/AFF4 validation checks regress.
