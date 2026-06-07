# Current Continuation Handoff - V1.3.2

Newest generated source:
- `VestigantSpotlightInv_V1_3_2.zip`

Build command:
```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_3_2.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_3_2" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_3_2.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_3_2\scripts\Build-V1_3_2.ps1
```

Thin-create command:
```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_3_2\scripts\Run-V1_3_2-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

V1.3.2 completed/worked sections:
- Group A: APFS buffer reuse and GUI case-tab ingest-safety hardening.
- Group A audit: export threads and GUI database access reviewed.
- Groups B/C/D/E: documented as roadmap, not implemented in this version.

Next upload needed:
- `V1_3_2_build.log`
- `Upload_Thin_MacOS_AFF4_V1_3_2.zip`

Optional helpful GUI/AFF4 exports after a full GUI run:
- `logs/VestigantSpotlight.log`
- `logs/run_status.txt`
- `last_progress.tsv` and `logs/last_progress.tsv` if present
- `case_summary.json`
- `aff4_apfs_external_spotlight_compare_summary.json`
- `aff4_apfs_remaining_mismatch_diagnostics.csv` and summary JSON if present
- screenshot(s) of any Case tab button/autosave issue
