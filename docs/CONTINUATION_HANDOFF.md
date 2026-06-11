# Continuation Handoff - V1.6.6.6

Current package: `VestigantSpotlightInv_V1_6_6_6.zip`.

V1.6.6.6 follows the uploaded V1.6.6.5 iOS thin bundle. The thin run reached `complete_success`, reported 6 valid stores, 344,445 raw records, 42,799 raw key/value rows, 344,445 artifacts, 228,699 usage evidence rows, and 277,823 timeline events. No slow or incomplete exports were reported above the thin-performance threshold.

Queued forensic-directive claims were checked against the actual V1.6.6.5 source before changes. APFS guided traversal cycle guards, the bounded bplist/NSKeyedArchiver resolver, and `tel:` / `mailto:` identity fallback were already present. The actionable gap found was that the Spotlight/native-DB mismatch view existed in `case_db.cpp` and `view_registry.cpp` but not in the lightweight GUI bootstrap SQL in `win32_gui.cpp`.

V1.6.6.6 adds `vw_ios_spotlight_comms_missing_from_ffs` to the GUI bootstrap schema and prioritizes `iOS - Spotlight Comms Missing From Native DB` in GUI iOS sorting. The view wording avoids asserting deletion as the only explanation.

Validation performed in this package environment:

- Linux CMake build: run locally during packaging.
- CLI version expected: `Vestigant Spotlight v1.6.6.6`.
- Self-test expected: PASS.
- Static forensic-directive and release-readiness checks expected: PASS.

Next commands:

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_6_6.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_6_6" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_6_6.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_6\scripts\Build-V1_6_6_6.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_6_6\scripts\Run-V1_6_6_6-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Upload next:

- `V1_6_6_6_build.log`
- `Upload_Thin_iOS_CoreSpotlight_V1_6_6_6.zip`

AFF4/APFS thin/full is not required for V1.6.6.6 unless the Windows build, shared schema initialization, or APFS/AFF4 validation checks regress.
