# Current Build and Test Instructions — V1_6_6_5

## Build

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_6_5.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_6_5" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_6_5.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_5\scripts\Build-V1_6_6_5.ps1
```

Expected CLI version after build:

```text
Vestigant Spotlight v1.6.6.5
```

## GUI launch

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_5\scripts\Launch-V1_6_6_5-GUI.ps1
```

## iOS thin validation

Run iOS thin after the Windows build passes. This remains required to confirm the V1.6.6.3 thin-export timeout fixes and the V1.6.6.5 release-readiness/GUI predicate hotfix.

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_5\scripts\Run-V1_6_6_5-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Optional no-CSV run:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_5\scripts\Run-V1_6_6_5-iOS-CoreSpotlight-AndZip.ps1 -CleanOut -NoCsvExports
```

Upload after iOS thin:

```text
D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_6_5.zip
```

## AFF4/APFS test scope

AFF4/APFS full or thin validation is not required for V1.6.6.5 unless shared schema initialization, Windows/MSVC build behavior, or unrelated AFF4/APFS code regresses. V1.6.6.5 changes wrappers, release-readiness checks, and iOS/GUI communication predicate guardrails only.

If AFF4/APFS thin is needed anyway:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_5\scripts\Run-V1_6_6_5-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

## Upload next

- `V1_6_6_5_build.log`
- `Upload_Thin_iOS_CoreSpotlight_V1_6_6_5.zip`
- `Upload_Thin_MacOS_AFF4_V1_6_6_5.zip` only if AFF4/APFS thin was run
