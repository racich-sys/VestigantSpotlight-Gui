## V1.6.10 - AFF4/APFS Store-V2 validation cleanup after V1.6.8 thin

V1.6.10 follows review of the uploaded `Upload_Thin_MacOS_AFF4_V1_6_8.zip`, whose sandbox-computed SHA256 was `E60F9E7214D799B3AA6EF7587FB4A7862328304A0773F69D82C6C33A1358AC11`. The V1.6.8 thin reached `complete_aff4_apfs_staged_storev2_validation_probe` and corrected the V1.6.7.1 summary-count problem: `case_summary.json` reported `store_count=11`, `valid_store_count=7`, `database_candidate_count=22`, `valid_database_candidate_count=14`, `parser_selected_database_count=7`, `native_decode_mode=AFF4_APFS_STAGED_STOREV2_FullValues`, `raw_record_count=25000`, `raw_key_value_count=2181`, `raw_date_candidate_count=25000`, `artifact_count=25000`, and `timeline_event_count=25000`.

The V1.6.8 thin still showed validation-quality gaps that should be fixed before the next build: the Windows release-readiness script could throw a false-positive "Build wrapper CLI version validation is not pinned" error because it looked for dotted `1.6.8` while the build wrapper also contains escaped regex text; `raw_key_values` still used generic `__native_core_probe_string_##` field names for URLs/paths/plist text; field inventory and parser coverage row counts were reported in the enrichment summary but not included as AFF4/APFS staged validation sample CSVs in the thin package; and operator-facing source-probe/help text still implied that no extraction/parsing occurs even when the guarded AFF4/APFS Store-V2 validation pipeline is active.

Code fixes in V1.6.10:
- Fixed the release-readiness build-wrapper version-pin check to accept both literal `1.6.10` and escaped regex `1\.6\.10` text, removing the false-positive wrapper error while keeping the version pin check.
- Added inferred/private native probe aliases for bounded raw Spotlight probe strings: `__native_probe_url_candidate_##`, `__native_probe_file_path_candidate_##`, `__native_probe_basename_candidate_##`, `__native_probe_email_candidate_##`, and `__native_probe_plist_xml_candidate_##` where the raw value supports that classification.
- Allowed derived native probe aliases to populate `where_froms`, effective metadata path, display name, and file name when no Apple-documented/full structured kMDItem field is decoded.
- Added AFF4/APFS staged Store-V2 validation sample exports for `field_inventory` and `parser_coverage_summary`: `aff4_apfs_staged_storev2_field_inventory_sample.csv` and `aff4_apfs_staged_storev2_parser_coverage_summary_sample.csv`.
- Updated the source-probe upload packaging list to include the new staged validation field-inventory and parser-coverage samples.
- Updated source-probe run-status/help text to stop implying that all AFF4/APFS extraction is absent; the text now separates the active bounded Store-V2 validation pipeline from the still-staged general full-container filesystem extraction work.

Validation completed in this packaging environment: Linux incremental CMake build completed, CLI version reported `Vestigant Spotlight v1.6.10`, and `VestigantSpotlightTests` passed. The prior failed local staged-folder run used a staged root shape not recognized by normal discovery and is not used as validation evidence. Windows/MSVC build, the V1.6.10 macOS AFF4 thin, and iOS regression thin were not run in this environment.

Test determination: because V1.6.10 changes native Store-V2 semantic probe labeling and AFF4/APFS staged validation exports, run Windows/MSVC build and the macOS AFF4 thin. iOS thin is recommended after that because native probe aliasing code is shared with iOS CoreSpotlight raw probe fallback paths.

## Local validation artifacts

- `/mnt/data/v169_build4.log`
- `/mnt/data/v169_version2.log`
- `/mnt/data/v169_tests2.log`

These are packaging-environment validation artifacts only. The Windows/MSVC build and live full AFF4/APFS thin must still be run on the user's Windows evidence environment.
