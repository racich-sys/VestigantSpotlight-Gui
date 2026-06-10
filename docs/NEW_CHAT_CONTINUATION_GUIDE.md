# V1.6.6.5 Continuation Handoff

## Current status

V1.6.6.5 is a narrow hotfix after the V1.6.6.3 Windows wrapper stopped at release-readiness with a generic `KNOWLEDGEC_EVENTS` communication/identity predicate warning.

## What changed

- Current build wrapper is `scripts/Build-V1_6_6_5.ps1` and validates CLI version `1.6.6.5`.
- Build-wrapper preflight checks now run after extraction / clean extraction so stale extracted source is not checked before replacement.
- `src/gui/win32_gui.cpp` bootstrap communication views now use `KNOWLEDGEC_COMMUNICATION_INTENT` and provenance markers rather than generic `KNOWLEDGEC_EVENTS`.
- `tools/Verify-V1_6_6_5-ReleaseReadiness.ps1` now scans both `src/db/case_db.cpp` and `src/gui/win32_gui.cpp` and distinguishes allowed suppression guards from disallowed promotional predicates.
- The main case schema still contains suppression guards that explicitly exclude `KNOWLEDGEC_EVENTS` / `KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY` rows unless communication-intent provenance is present.

## Required next artifacts

- `V1_6_6_5_build.log`
- `Upload_Thin_iOS_CoreSpotlight_V1_6_6_5.zip`

## Build and thin commands

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_6_5.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_6_5" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_6_5.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_5\scripts\Build-V1_6_6_5.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_5\scripts\Run-V1_6_6_5-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

## Test scope decision

Run Windows/MSVC build first because V1.6.6.5 directly fixes the wrapper/readiness stop observed in V1.6.6.3.

Run iOS thin after build passes because V1.6.6.3 still needs validation that the three timeout-prone exports no longer time out, and V1.6.6.5 also updates GUI bootstrap communication predicates.

Do not run AFF4/APFS thin/full for V1.6.6.5 unless the Windows build or shared schema/view initialization regresses. This release does not change AFF4/APFS traversal, copy-out, decompression, source-reader, APFS filesystem reading, or Store-V2 staging behavior.
