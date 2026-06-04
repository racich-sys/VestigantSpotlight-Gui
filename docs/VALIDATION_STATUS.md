# Vestigant Spotlight Validation Status

Current version: 0.9.53

## V0_9_53 local validation

Reviewed uploaded V0_9_48 reuse-cache thin output:

- Last stage: `complete_success`
- Stores / valid stores: 6 / 6
- Raw records: 344,445
- Raw key/value rows: 982,668
- Raw date candidates: 336,037
- Artifacts: 344,445
- Timeline events: 336,037
- Targeted app DB parser: 50 extracted/opened databases; 525,409 parsed app records
- `ios_app_parsed_record_summary.csv`: 16 rows
- `investigator_super_timeline_sample.csv`: 5,000 sampled rows
- `investigator_time_anomalies.csv`: 101 rows

Changed source validation performed in this environment:

- Reviewed and patched `src/gui/win32_gui.cpp` for the details pane.
- Performed structural/static checks for expected Win32 symbols and details-pane event hook.
- Verified version metadata updated to 0.9.53.
- Verified source/patch ZIP integrity after packaging.

Validation limits:

- Windows/MSVC GUI compile/link was not run here.
- Native Win32 UI behavior must be validated on the Windows test system.
- No parser or ingest behavior was intentionally changed in V0_9_53, so reuse-cache/fresh-ZIP reruns are optional unless GUI build/test exposes an issue.

## V0_9_53 expected Windows verification

- Build banner reports source version 0.9.53.
- Binary reports `Vestigant Spotlight v0.9.53`.
- GUI opens an existing completed case.
- MacOS and iOS investigation tabs both show the bottom `Selected Row Metadata / All Fields` pane.
- Selecting rows and using arrow keys updates the pane.
- Long values can be reviewed vertically and copied from the pane.
- Search/filter/sort/export/checkmark/tag workflows still operate.
