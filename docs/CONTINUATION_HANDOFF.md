# Start Continuation Chat - V1.6.28

Upload these files first:

- `VestigantSpotlightInv_V1_6_28.zip`
- `V1_6_28_build.log`, if already run
- Latest iOS upload ZIP, preferably `Upload_Thin_iOS_CoreSpotlight_V1_6_28.zip`
- Prior reference evidence if needed: `Upload_Thin_iOS_CoreSpotlight_V1_6_26.zip` and `V1_6_26_build.log`

Paste this prompt:

```text
Continue the Vestigant Spotlight iOS/CoreSpotlight project from V1.6.28.

Use VestigantSpotlightInv_V1_6_28.zip as the current source baseline. V1.6.28 adds validation surfaces for active filesystem comparison after V1.6.25 materialized 7,766 MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE rows. Validate active_file_comparison_validation_checks_sample.csv, active_file_comparison_candidate_summary_sample.csv, orphaned_deleted_candidates_focus.csv, active_file_comparison_runs_sample.csv, and active_file_comparison_readiness_focus.csv. Missing rows are investigative leads only, not deletion proof.
```

Build command:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_28\scripts\Build-V1_6_28.ps1 -CleanExtract
```

Thin command:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_28\scripts\Run-V1_6_28-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Expected thin upload:

```text
D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_28.zip
```
