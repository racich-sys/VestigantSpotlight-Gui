# V1.6.6.3 Thin Upload Review and Release Notes

## Uploaded artifact reviewed

- `Upload_Thin_iOS_CoreSpotlight_V1_6_6_2.zip` was unpacked and reviewed before V1.6.6.3 source changes.
- `last_stage.txt` reports `2026-06-10T15:35:40Z complete_success`.
- `run_status.txt` contains 687 status lines and 179 completed export-query entries.
- `case_summary.json` reports 1 source, 6 store groups, 6 valid store groups, 12 database candidates, 12 valid database candidates, 344,445 raw records, 22,569 raw key/value rows, 344,445 artifacts, 228,699 usage-evidence rows, and 277,823 timeline events.
- `UPLOAD_MANIFEST.txt`, `case_file_inventory.txt`, `THIN_PERFORMANCE_SUMMARY.md`, `thin_performance_summary.csv`, `run_status.txt`, `VestigantSpotlight_tail250.log`, `exports/upload_samples/upload_samples_manifest.csv`, and `exports/upload_samples/upload_table_counts.csv` were reviewed for run status, export behavior, row counts, and errors/warnings.

## Thin review findings

The run completed successfully, but three thin/minimal CSV exports timed out at the 120-second per-export guardrail:

- `ios_identity_pivot_frequency.csv`
- `ios_communication_candidate_promotion_sample.csv`
- `ios_spotlight_communication_not_observed_native_sample.csv`

`THIN_PERFORMANCE_SUMMARY.md` also marked `ios_spotlight_investigator_overview.csv` as `slow_complete` with 47 elapsed seconds and 9 rows. `ios_spotlight_entity_summary.csv` completed in 18 seconds and 27 rows. Those two were not changed in V1.6.6.3 because they completed and did not block the run.

## V1.6.6.3 changes

- Minimal/thin iOS exports no longer execute the three timeout-prone joined identity/communication graph views.
- In minimal/thin mode, `ios_identity_pivot_frequency.csv` is now generated from direct base-table app-database and CoreSpotlight key/value summaries and carries the marker `THIN_PROFILE_DIRECT_IDENTITY_PIVOT` in the interpretation note.
- In minimal/thin mode, `ios_communication_candidate_promotion_sample.csv` is now generated from direct app-database rows and carries the marker `THIN_PROFILE_DIRECT_CANDIDATE_SAMPLE` in the interpretation note.
- In minimal/thin mode, `ios_spotlight_communication_not_observed_native_sample.csv` now writes an explicit `THIN_PROFILE_NOT_EVALUATED` notice instead of spending up to 120 seconds on the joined comparison.
- Support/full export profiles retain the richer joined views, still protected by the existing timeout guard.
- `scripts/Build-V1_6_6_3.ps1` now validates the CLI version against `1.6.6.3`. The inherited V1.6.6.2 wrapper contained a stale `1.6.6.1` CLI-version check; this was corrected for V1.6.6.3.
- Release-readiness checks now verify the V1.6.6.3 build-wrapper version pin and the new thin export timeout-guard markers.

## Test determination

Run another iOS thin test for V1.6.6.3 because this version directly changes iOS minimal/thin export SQL behavior and should confirm that the three V1.6.6.2 export timeouts are gone.

AFF4/APFS thin or full testing is not required for V1.6.6.3 unless the Windows build or shared schema/view initialization regresses, because this version does not change AFF4/APFS traversal, copy-out, decompression, AFF4 source reading, APFS filesystem reading, or Store-V2 staging code.
