
## V1.6.40.1.1 macOS unresolved Store-V2 object labels

- Added explicit unresolved object labels for macOS Store-V2 records that still lack structured names after dictionary/path-probe enrichment.
- Labels are forensic review handles, not asserted filenames.
- Added parser metric `unresolved_identifier_label_artifacts`.


## V1.6.40.1.1 Late Review Addendum

- Added late-review fixes for iOS CoreSpotlight bundle attribution, bracketed timestamp-array normalization, per-path GUI read-only DB pooling, and bplist JSON stringification caps.
- Confirmed null-byte-safe CSV export was already present before this addendum.

# V1.6.40.1.1 New Chat Continuation

Provide `VestigantSpotlightInv_V1_6_40_1.zip`, `V1_6_40_1_build.log`, and the V1.6.40.1.1 macOS zipped Spotlight thin upload. Validation targets: `native_path_probe_artifacts_updated`, `native_basename_probe_artifacts_updated`, and reduced GUI `------NONAME------` where candidates exist.

# V1.6.40.1.1 New Chat Continuation

Provide `VestigantSpotlightInv_V1_6_40_1.zip`, `V1_6_40_1_build.log`, and the macOS zipped Spotlight thin upload. The specific validation target is external dbStr map loading for macOS Store-V2 and resulting GUI display-name/path improvement.

# V1.6.40.1.1 New Chat Continuation

Provide `VestigantSpotlightInv_V1_6_40_1.zip`, `V1_6_40_1_build.log`, and the next macOS zipped Spotlight thin upload. The specific issue under test is whether native path probe candidates now replace `------NONAME------` in GUI review rows.

# V1_6_40_1 note

V1_6_40_1 skips the parent-inode path apply UPDATE when `new_reconstructed_paths=0`, based on the V1.6.32 macOS zipped Spotlight thin result. Build success remains unverified until the Windows log is uploaded.

# V1_6_40_1 note

V1_6_40_1 fixes recurring build-blocking release-readiness failures: release-readiness is advisory from the build wrapper, while wrapper compatibility and raw-string risk remain fatal. Expected CLI version is read dynamically from `VERSION`.

# V1.6.40.1.1 current handoff note

V1.6.40.1.1 follows successful V1.6.40.1.1 build/thin validation and implements code-review hardening for APFS/AFF4/bplist/iOS app DB/GUI path handling. See `docs/V1_6_40_1_CODE_REVIEW_VALIDATION_HARDENING.md`. Missing-from-FFS and CoreDuet interpretation guardrails remain in place.

# New Chat Continuation Guide - V1.6.40.1.1

Continue from V1.6.40.1.1. The next validation target is the V1.6.40.1.1 iOS thin upload. Validate active-comparison reconciliation, Missing-from-FFS lead-only wording, and CoreDuet interactionC parser checks. Do not treat Missing-from-FFS rows as deletion proof.

# Start Continuation Chat - V1.6.40.1.1

Upload these files first:

- `VestigantSpotlightInv_V1_6_40_1.zip`
- `V1_6_40_1_build.log`, if already run
- Latest iOS upload ZIP, preferably `Upload_Thin_iOS_CoreSpotlight_V1_6_40_1.zip`
- Prior reference evidence if needed: `Upload_Thin_iOS_CoreSpotlight_V1_6_26.zip` and `V1_6_26_build.log`

Paste this prompt:

```text
Continue the Vestigant Spotlight iOS/CoreSpotlight project from V1.6.40.1.1.

Use VestigantSpotlightInv_V1_6_40_1.zip as the current source baseline. V1.6.40.1.1 adds validation surfaces for active filesystem comparison after V1.6.25 materialized 7,766 MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE rows. Validate active_file_comparison_validation_checks_sample.csv, active_file_comparison_candidate_summary_sample.csv, orphaned_deleted_candidates_focus.csv, active_file_comparison_runs_sample.csv, and active_file_comparison_readiness_focus.csv. Missing rows are investigative leads only, not deletion proof.
```

Build command:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_40_1\scripts\Build-V1_6_40_1.ps1 -CleanExtract
```

Thin command:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_40_1\scripts\Run-V1_6_40_1-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Expected thin upload:

```text
D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_40_1.zip
```

## V1.6.40.1.1 continuation note

Use the V1.6.40.1.1 ZIP as the source of truth. Validate build log and thin upload before further changes. Track whether GUI source changes compile under MSVC and whether active filesystem/CoreDuet validation samples remain PASS.

## V1.6.40.1.1 continuation

Use V1.6.40.1.1 to retest the macOS `.Spotlight-V100` folder. Expected status includes `native_kv_persistence_macos_storev2`, not `native_kv_persistence_filtered` or iOS CoreSpotlight compact wording.
