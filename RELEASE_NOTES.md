# Vestigant Spotlight Release Notes

Current version: 0.9.42

## V0_9_39

V0_9_39 reviews the V0_9_37 build/thin results and fixes the new database guardrail regression introduced by expanding Missing From FFS same-record text context too aggressively.

Changes:

- Preserves the V0_9_37 Missing From FFS text-detail views/exports.
- Restores normal-mode Spotlight text context to a bounded investigator-safe size: 1,800 bytes, 8 fields, and 320 bytes per field sample.
- Keeps row-level text visibility/status columns so missing-from-FFS candidates show recovered Spotlight text where compact mode has it.
- Fixes fatal SQLite size guardrail propagation inside native metadata parsing so guardrail hits stop cleanly instead of being swallowed and followed by a secondary `COMMIT` error.
- Adds `docs/V0_9_39_REVIEW_NOTES.md`.
- Updates version metadata and scripts to V0_9_39.

Validation performed in this environment:

- Reviewed the V0_9_37 Windows build log and thin upload.
- Confirmed the failure class was DB-size guardrail caused by larger text context values rather than row-count explosion.
- Confirmed source syntax and static raw-string checks.

Windows/MSVC build and the standard iOS reuse-cache run remain required.


## V0_9_37

V0_9_37 is a documentation-history repair release after V0_9_34 cleanup compressed too much historical detail.

Changes:

- Reviewed the uploaded V0_9_3 documentation archive (`Docs.zip`).
- Restored historical V0_9 development information into `docs/CONSOLIDATED_VERSION_HISTORY.md`.
- Kept the production package clean by aggregating historical notes instead of reintroducing many stale per-version fragments.
- Updated `docs/CONSOLIDATED_USER_MANUAL.md` to explain the current documentation model, standard workflows, iOS review start path, compact-mode interpretation, diagnostics, and AFF4/APFS roadmap location.
- Updated top-level `HELP.md`, `VERSION_HISTORY.md`, validation notes, roadmap, and package-cleanup notes to explain the restored version history.
- Updated version metadata and scripts to V0_9_37.
- No parser, schema, GUI, export, or forensic interpretation behavior was intentionally changed from V0_9_34.

Validation performed in this environment:

- Confirmed version metadata was updated to 0.9.37.
- Confirmed consolidated documentation contains restored entries for V0_9_0 through V0_9_37 based on available historical notes.
- Confirmed production package still avoids reintroducing root-level historical fragments.
- Confirmed ZIP/patch integrity and SHA256 files.

Windows/MSVC build validation remains required because only documentation/version/script metadata changed in this packaging environment.

## V0_9_37 - Missing From FFS text visibility

V0_9_37 addresses the user-reported issue that some Spotlight CSV reports did not show recovered Spotlight text/content.  It adds row-level Missing From FFS text detail and text coverage exports, exposes the same views in the GUI, increases compact same-record text context retention for reference-bearing iOS records, and documents when text is unavailable or suppressed by compact mode.

## V0_9_42

- Reviewed V0_9_39 build/thin results; Windows build and reuse-cache run completed successfully.
- Optimized CSV export writing to reduce string allocation and small-write overhead.
- Increased sequential hash-read buffers.
- Improved generated 7-Zip FFS inventory parsing for future actual-ZIP testing.
- Added a fresh-ZIP Stage B run script.
- Preserved compact iOS Spotlight normal mode and current investigator views.

## V0_9_42 - Native C++ 7-Zip inventory parser

V0_9_42 reviewed the successful V0_9_41 reuse-cache run and carries forward the V1-readiness performance work. The CSV exporter fast path remains in place. The iOS focused ZIP workflow now lets 7-Zip dump `-slt` output to raw text and then rebuilds FFS/app database inventory CSVs using native C++ parsing rather than the PowerShell raw-listing parser. This is intended to make the Stage B fresh-ZIP test faster and closer to the 60-120 MB/s target where hardware permits.
