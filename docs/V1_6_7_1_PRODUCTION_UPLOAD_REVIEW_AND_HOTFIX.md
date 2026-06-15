# V1.6.7.1 Production Upload Review and Hotfix

Input reviewed: `Upload_Production_iOS_CoreSpotlight_V1_6_7_0.zip`.

## Uploaded V1.6.7.0 production result

The uploaded production bundle reached `complete_success` in `last_stage.txt` and `last_progress.tsv`.

`case_summary.json` reported:

- `source_count`: 1
- `store_count`: 6
- `valid_store_count`: 6
- `database_candidate_count`: 12
- `valid_database_candidate_count`: 12
- `parser_selected_database_count`: 6
- `native_decode_mode`: `CoreFields`
- `metadata_values_decoded`: true
- `raw_record_count`: 344,445
- `raw_key_value_count`: 42,799
- `artifact_count`: 344,445
- `usage_evidence_count`: 228,699
- `timeline_event_count`: 277,823

`run_status.txt` showed `validation_warning_metadata_limited_export` at run start. That means the V1.6.7.0 production wrapper requested investigator/full exports without full native metadata values.

`run_progress.tsv` also showed a long forced-hash interval without granular hash progress: `source_probe_write` was recorded at 2026-06-11T17:25:46Z and the next stage, `ios_ffs_inventory_materialization_skipped`, was recorded at 2026-06-11T19:55:22Z. This is consistent with V1.6.7.0 forcing a SHA256 over a 275,720,574,292-byte input ZIP, but the run-status file did not expose hash progress while that was happening.

The upload bundle did not include `exports/ios_production_readiness_summary.csv` even though `run_status.txt` showed that the export completed with 7 rows.

## V1.6.7.1 changes

- Fixed a Windows-only SHA256 compile hazard in `src/core/hash.cpp`: the Windows hashing branch had a duplicate `std::vector<unsigned char> buffer(4 * 1024 * 1024);` declaration.
- Added `sha256FileWithProgress` to `src/core/hash.h` / `src/core/hash.cpp`.
- Added source-container hash run-status markers in `src/app/app_runner.cpp`: `original_container_hash_start`, `original_container_hash_progress`, and `original_container_hash_complete`.
- Updated `tools/Run-IosCoreSpotlightFocusedZip.ps1` with `FullNativeValues` support and `--experimental-full-native-values` forwarding.
- Updated `scripts/Run-V1_6_7_1-iOS-Production-AndZip.ps1` to set `FullNativeValues = $true` with `ForceContainerHash = $true` and investigator exports.
- Updated production performance summary naming: production runs now write `production_performance_summary.csv` and `PRODUCTION_PERFORMANCE_SUMMARY.md`; thin runs retain `thin_performance_summary.csv` and `THIN_PERFORMANCE_SUMMARY.md`.
- Updated `tools/Create-SourceProbeUploadZip.ps1` so production upload bundles include production-readiness and export-index artifacts when present.

## Validation in this environment

- Linux CMake build: PASS.
- CLI version: `Vestigant Spotlight v1.6.7.1`.
- Self-test: PASS.
- Static source/text/wrapper audit: PASS.
- ZIP integrity test: PASS.
- Windows/MSVC build: not run here.
- V1.6.7.1 iOS production run: not run here.

## Test determination

Run the Windows/MSVC build first because V1.6.7.1 fixes a Windows-only hashing compile hazard. Then run iOS thin. If thin passes, run iOS production to verify that the production wrapper now logs full-native metadata validation rather than the V1.6.7.0 limited-export warning and that forced source hashing now emits progress markers.

AFF4/APFS thin/full is not required for this version unless the Windows build, shared schema initialization, or APFS/AFF4 validation checks regress. The code changes touch shared hashing and source registration, so the build/self-test is required before treating AFF4/APFS as unaffected.
