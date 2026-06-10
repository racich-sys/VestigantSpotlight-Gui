# Continuation Handoff - V1.6.6.5

Current package: `VestigantSpotlightInv_V1_6_6_5.zip`.

## What changed most recently

V1.6.6.5 bridges compact native CoreSpotlight probe strings into iOS text-context and communication review views. The triggering evidence was the uploaded V1.6.6.4 iOS thin bundle: the run completed successfully and had 9,591 message/app string probes plus 933 email/account probes, but the main communication/text-context review exports still produced zero rows.

## Current validation

- Linux CMake build: PASS.
- CLI version: `Vestigant Spotlight v1.6.6.5`.
- Self-test: PASS.
- Windows/MSVC build: not run here.

## Next required run

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_6_5.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_6_5" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_6_5.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_5\scripts\Build-V1_6_6_5.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_5\scripts\Run-V1_6_6_5-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Upload:
- `V1_6_6_5_build.log`
- `Upload_Thin_iOS_CoreSpotlight_V1_6_6_5.zip`
