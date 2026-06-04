# Vestigant Spotlight Release Notes

Current version: 0.9.53

## V0_9_53 - investigation details-pane usability update

V0_9_53 reviewed the uploaded V0_9_48 reuse-cache thin output and the investigator UI request for a bottom metadata/details window. The V0_9_48 reuse-cache run reached `complete_success`, normal mode parsed targeted high-value app databases, and the app parsed/super-timeline exports were populated. No ingest or parser hotfix was required.

Changes:

- Added a bottom read-only `Selected Row Metadata / All Fields` details pane to the shared investigation grid.
- The pane appears in both macOS and iOS investigation review modes because those tabs share the Win32 review grid.
- Selecting a row now displays the selected view name, row number, artifact ID when available, checked/tag state, and every visible result column vertically.
- Long Spotlight/app metadata fields can be reviewed by vertical scrolling/copying instead of horizontal grid scrolling.
- The pane refreshes after page loads, row selection changes, and checkmark toggles.
- Increased ListView text retrieval for the details pane to reduce clipping of long metadata/snippet values.
- Added an existing-case GUI test note/script so GUI-only feature testing can proceed without rerunning ingest.
- Updated version metadata/scripts/docs to V0_9_53.

Reviewed V0_9_48 reuse-cache metrics:

- Status: `complete_success`
- Stores / valid stores: 6 / 6
- Raw records: 344,445
- Raw key/value rows: 982,668
- Raw date candidates: 336,037
- Artifacts: 344,445
- Timeline events: 336,037
- Targeted app DB parser: 50 extracted/opened DBs; 525,409 parsed app records
- `ios_app_parsed_record_summary.csv`: 16 rows
- `investigator_super_timeline_sample.csv`: 5,000 sampled rows
- `investigator_time_anomalies.csv`: 101 rows

Validation performed here:

- Static/structural validation of the Win32 details-pane symbols and event hook.
- Brace/parenthesis balance check for `src/gui/win32_gui.cpp`.
- `src/core/app_info.cpp` syntax check.
- ZIP integrity checks after packaging.

Windows/MSVC validation still required:

- Build V0_9_53 with `scripts\Build-V0_9_53.ps1`.
- Launch the GUI and open an existing completed case.
- Confirm the bottom details pane updates while moving through result rows.
- Reuse-cache/fresh-ZIP ingest reruns are optional unless later parser/staging changes are made.

## V0_9_53 GUI V1 usability update

- Added a platform-scoped View Set selector for MacOS and iOS investigation tabs.
- Presets: Recommended V1, Timeline / Activity, Text / Content, App DB / KnowledgeC, Diagnostics / Coverage, Show All.
- Added grouped RichEdit selected-row details pane with text/content first, dates second, then paths, people/apps, status, provenance, counts, and other fields.
- Added existing-case schema/view upgrade on open so newly added SQL views can appear without re-ingest when the case DB is writable.
- Added review_view_preferences schema table for future per-case custom view ordering/visibility.

## V0_9_53

GUI usability refinement for the investigation grid:

- Selected-row metadata is now shown in a Field / Value layout, with the field name on the left and value on the right.
- The details pane keeps text/content first, dates second, and grouped forensic/provenance fields after that.
- A draggable splitter was added above the details pane so the investigator can adjust the grid/detail height while reviewing results.
- This is a GUI-only change; no ingest, parser, cache, or export behavior was intentionally changed.

## V0_9_53

GUI-only correction for the selected-row details pane:

- Forces selected-row details to be a real Win32 ListView report control with two columns: Field and Metadata / Value.
- Creates details controls hidden by default and shows them only on MacOS/iOS investigation tabs.
- Adds active-tab visibility enforcement during layout/resize so the Case Information and Tags / Notes tabs cannot show the row-details pane.
- Removes RichEdit loading from this path to eliminate accidental/stale RichEdit rendering and make the table implementation unambiguous.
- Updates the startup log to identify the V0_9_53 details-table build.
- No parser, ingest, cache, export, ZIP, FFS inventory, app DB, or forensic interpretation logic changed.
