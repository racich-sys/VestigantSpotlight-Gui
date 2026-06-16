# V1.6.41.1 macOS Store-V2 Native Path Probe Promotion

## Triggering evidence

A GUI current-page export from the macOS zipped Spotlight Store-V2 validation case showed that 249 of 250 displayed rows used the placeholder `------NONAME------`; only one row had a populated filename/path. The V1.6.32 macOS ZIP thin log showed the native parser successfully parsed 50,000 records, wrote 19,570 key/value rows, and ran with macOS Store-V2 persistence selected. Diagnostic key/value output showed native probe path candidates existed, but enrichment did not promote those path candidates into artifact display fields before GUI/timeline materialization.

## Fix

V1.6.41.1 adds a SQLite-native enrichment pass immediately after artifact creation and before source-copy comparison, parent-inode linking, and timeline materialization.

The new pass:

- reads `raw_key_values.field_name LIKE '__native_probe_file_path_candidate_%'`;
- accepts absolute macOS-style path candidates only;
- chooses the longest candidate path per artifact;
- applies candidates only where the current artifact path/name is blank, placeholder, or weak;
- updates `artifacts.file_name`, `display_name`, `best_path`, `spotlight_display_path`, and `normalized_mac_path`;
- sets `path_source='NATIVE_PATH_PROBE_FULL_PATH'` and `path_status='RAW_NATIVE_PATH_PROBE_PRESENT'`;
- records parser metric `native_path_probe_artifacts_updated`;
- writes run-status stages `enrichment_native_path_probe_apply_start` and `enrichment_native_path_probe_apply_complete`.

## Intended result

macOS Store-V2 GUI rows that previously displayed `------NONAME------` should now display recovered basenames and paths when native path probe values were available in `raw_key_values`.

## Guardrails

This does not assert that a file currently exists on the filesystem. It promotes native Spotlight path evidence into review/display fields for investigator usability and marks the path provenance explicitly as native probe-derived.

## Validation required

Run the same macOS zipped Spotlight thin test against `E:\test second\Spotlight\Spotlight-V100.zip` after MSVC build validation. Review:

- `run_status.txt` for `enrichment_native_path_probe_apply_complete`;
- `parser_coverage_summary.csv` for `native_path_probe_artifacts_updated`;
- GUI current-page export for a reduced `------NONAME------` rate;
- `artifact/path` fields for `path_source=NATIVE_PATH_PROBE_FULL_PATH`.
