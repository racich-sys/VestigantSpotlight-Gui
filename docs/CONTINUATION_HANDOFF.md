
## V1.6.41.1 macOS unresolved Store-V2 object labels

- Added explicit unresolved object labels for macOS Store-V2 records that still lack structured names after dictionary/path-probe enrichment.
- Labels are forensic review handles, not asserted filenames.
- Added parser metric `unresolved_identifier_label_artifacts`.


## V1.6.41.1 Late Review Addendum

- Added late-review fixes for iOS CoreSpotlight bundle attribution, bracketed timestamp-array normalization, per-path GUI read-only DB pooling, and bplist JSON stringification caps.
- Confirmed null-byte-safe CSV export was already present before this addendum.

# V1.6.41.1 Handoff

V1.6.41.1 removes a premature SQL pre-filter from native path probe promotion and adds safe basename candidate promotion for placeholder artifact names. Review `docs/V1_6_41_MACOS_NATIVE_PROBE_PROMOTION_SQL_FILTER_FIX.md`.

# V1.6.41.1 Handoff

V1.6.41.1 follows V1.6.34. It makes external dbStr map loading component-driven for macOS Store-V2, allowing the native parser to load property/category dictionaries from adjacent `dbStr-*` files when header dictionary pointers are zero.

# V1.6.41.1 Handoff

V1.6.41.1 addresses macOS Store-V2 display-name/path enrichment. Review `docs/V1_6_41_MACOS_NATIVE_PATH_PROBE_PROMOTION.md` before continuing. Validate with the same zipped Spotlight source and inspect `native_path_probe_artifacts_updated`.

# V1_6_41 note

V1_6_41 skips the parent-inode path apply UPDATE when `new_reconstructed_paths=0`, based on the V1.6.32 macOS zipped Spotlight thin result. Build success remains unverified until the Windows log is uploaded.

# V1_6_41 note

V1_6_41 fixes recurring build-blocking release-readiness failures: release-readiness is advisory from the build wrapper, while wrapper compatibility and raw-string risk remain fatal. Expected CLI version is read dynamically from `VERSION`.

# V1.6.41.1 current handoff note

V1.6.41.1 follows successful V1.6.41.1 build/thin validation and implements code-review hardening for APFS/AFF4/bplist/iOS app DB/GUI path handling. See `docs/V1_6_41_CODE_REVIEW_VALIDATION_HARDENING.md`. Missing-from-FFS and CoreDuet interpretation guardrails remain in place.

# Start Continuation Chat - V1.6.41.1

Upload these files first:

- `VestigantSpotlightInv_V1_6_41.zip`
- `V1_6_41_build.log`, if already run
- Latest iOS upload ZIP, preferably `Upload_Thin_iOS_CoreSpotlight_V1_6_41.zip`
- Prior reference evidence if needed: `Upload_Thin_iOS_CoreSpotlight_V1_6_26.zip` and `V1_6_26_build.log`

Paste this prompt:

```text
Continue the Vestigant Spotlight iOS/CoreSpotlight project from V1.6.41.1.

Use VestigantSpotlightInv_V1_6_41.zip as the current source baseline. V1.6.41.1 adds validation surfaces for active filesystem comparison after V1.6.25 materialized 7,766 MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE rows. Validate active_file_comparison_validation_checks_sample.csv, active_file_comparison_candidate_summary_sample.csv, orphaned_deleted_candidates_focus.csv, active_file_comparison_runs_sample.csv, and active_file_comparison_readiness_focus.csv. Missing rows are investigative leads only, not deletion proof.
```

Build command:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_41\scripts\Build-V1_6_41.ps1 -CleanExtract
```

Thin command:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_41\scripts\Run-V1_6_41-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Expected thin upload:

```text
D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_41.zip
```

## V1.6.41.1 handoff

V1.6.41.1 follows the validated V1.6.29.4 thin run. New hardening is in GUI checked-state/thread handling, CSV embedded-NUL preservation, and APFS NXSB block-size rejection. Await Windows/MSVC build log and iOS thin output for confirmation.

## V1.6.41.1 handoff

V1.6.41.1 fixes profile-aware native persistence for macOS Store-V2 folder sources and mirrors native parser progress to root progress files. Validate with the same macOS folder Spotlight source that exposed the V1.6.30 iOS-labelled status.
