# V1.6.29.4 current handoff note

V1.6.29.4 follows successful V1.6.29.4 build/thin validation and implements code-review hardening for APFS/AFF4/bplist/iOS app DB/GUI path handling. See `docs/V1_6_29_4_CODE_REVIEW_VALIDATION_HARDENING.md`. Missing-from-FFS and CoreDuet interpretation guardrails remain in place.

# New Chat Continuation Guide - V1.6.29.4

Continue from V1.6.29.4. The next validation target is the V1.6.29.4 iOS thin upload. Validate active-comparison reconciliation, Missing-from-FFS lead-only wording, and CoreDuet interactionC parser checks. Do not treat Missing-from-FFS rows as deletion proof.

# Start Continuation Chat - V1.6.29.4

Upload these files first:

- `VestigantSpotlightInv_V1_6_29_4.zip`
- `V1_6_29_4_build.log`, if already run
- Latest iOS upload ZIP, preferably `Upload_Thin_iOS_CoreSpotlight_V1_6_29_4.zip`
- Prior reference evidence if needed: `Upload_Thin_iOS_CoreSpotlight_V1_6_26.zip` and `V1_6_26_build.log`

Paste this prompt:

```text
Continue the Vestigant Spotlight iOS/CoreSpotlight project from V1.6.29.4.

Use VestigantSpotlightInv_V1_6_29_4.zip as the current source baseline. V1.6.29.4 adds validation surfaces for active filesystem comparison after V1.6.25 materialized 7,766 MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE rows. Validate active_file_comparison_validation_checks_sample.csv, active_file_comparison_candidate_summary_sample.csv, orphaned_deleted_candidates_focus.csv, active_file_comparison_runs_sample.csv, and active_file_comparison_readiness_focus.csv. Missing rows are investigative leads only, not deletion proof.
```

Build command:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_29_4\scripts\Build-V1_6_29_4.ps1 -CleanExtract
```

Thin command:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_29_4\scripts\Run-V1_6_29_4-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Expected thin upload:

```text
D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_29_4.zip
```
