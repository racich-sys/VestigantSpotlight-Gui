# Vestigant Spotlight Consolidated Version History

## V0_9_57 - Windows GUI forward-declaration compile hotfix

V0_9_57 is a focused Windows/MSVC GUI build hotfix after V0_9_56 reached the GUI compile stage and failed with `C3861: setReviewSummary identifier not found` in `src\gui\win32_gui.cpp`. The fix adds a forward declaration for `setReviewSummary(const std::wstring&)` before the custom view-set helper functions that call it. No parser, ingest, cache, ZIP, FFS inventory, app DB, export, or forensic interpretation behavior was intentionally changed.

Current version: 0.9.57

## V0_9_57 - Windows MSVC batch-label build hotfix

V0_9_57 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55

V0_9_55 reviewed the uploaded V0_9_48 reuse-cache thin output and the investigator-details-pane design request. The V0_9_48 reuse-cache run completed successfully with 6 valid CoreSpotlight stores, 344,445 raw records, 982,668 raw key/value rows, 336,037 date candidates, 525,409 parsed app database records, a populated investigator super timeline sample, and 101 time-anomaly rows.

This release focuses on V1 investigator usability rather than ingest/parsing changes. It adds a bottom read-only **Selected Row Metadata / All Fields** details pane to the shared investigation grid used by both the macOS and iOS investigation tabs. Selecting a row now shows the view name, row number, artifact ID where available, check/tag state, and every visible result column vertically so investigators can review long Spotlight/app metadata values without horizontal scrolling. The pane supports vertical scrolling and copy/select behavior through a native Win32 multiline edit control.

No parser, staging, FFS inventory, or app database classification behavior was intentionally changed in V0_9_55. Existing completed cases can be used to test this GUI feature. Reuse-cache/fresh-ZIP ingest reruns are only needed if later changes affect parser output, cache behavior, or staging.

## V0_9_45

V0_9_45 reviewed V0_9_44 Windows build plus reuse-cache and Stage B fresh-ZIP thin outputs. Both runs completed successfully. The fresh-ZIP native 7-Zip inventory parser now produced nonzero FFS inventory rows, but it over-classified many non-database files as app databases because several path-based rules did not require a database-like filename. This release narrows iOS app database inventory classification to database-like file names before applying Messages, Signal, Telegram, CallHistory, WebKit, Safari, Chrome/Google, Mail, Calendar, Contacts, Keychain, WhatsApp, and other database-family labels. It also preserves targeted extraction paths in the native C++ inventory CSV when the extracted database exists under EvidenceStaging/ios_app_databases, so future support-mode app DB record parsing has usable paths after a fresh ZIP run.

## V0_9_44

V0_9_44 reviewed V0_9_43 Windows build, reuse-cache thin output, and Stage B fresh-ZIP thin output. The fresh-ZIP path completed but produced an empty FFS/app-database inventory, showing the native C++ parser did not parse the raw 7-Zip listing. This release changes the PowerShell helper to use `cmd.exe` redirection for raw `7z l -slt` output, adds native fallback decoding for UTF-16 raw listing lines, reports a warning if the raw listing parses to zero rows, and preserves the compact normal iOS mode.

Additional parser improvements include zero-copy bounded metadata item parsing from decompressed metadata-block payloads, improved fallback probe deduplication/non-ASCII preservation, and safer removal of CoreSpotlight string trailer markers after trimming padding.

## V0_9_43

Added bounded iOS CoreSpotlight bplist / NSKeyedArchiver marker discovery. The release detected likely binary plist / NSKeyedArchiver values, extracted bounded printable-token context, stored compact synthetic context rows, and exposed summary/detail views and CSV exports. It intentionally did not perform full NSKeyedArchiver graph decoding.

## Earlier V0_9 history

Earlier V0_9 releases focused on compact normal iOS mode, DB/WAL guardrails, iOS investigator views, message/media/text review surfaces, parser limits/suppression reporting, GUI stability, CSV export optimization, reuse-cache workflows, and initial Stage B fresh-ZIP preparation. Preserve prior detailed history from earlier packages if litigation/reporting requires exact version-by-version reconstruction.

## V0_9_55

- Refined the selected-row metadata pane into a two-column Field / Value layout so field identifiers remain on the left and metadata values appear on the right.
- Added a draggable splitter above the details pane so investigators can resize the grid/detail split during review.
- Preserved the V0_9_50 grouped review order: text first, dates second, then paths, people/apps, status, provenance, counts, and other fields.
- GUI-only update; no parser, cache, ZIP, FFS, or app database behavior changed.

## V0_9_55
- GUI-only selected-row metadata pane revision.
- Converted the bottom details pane from formatted text to a true two-column Field / Metadata ListView table.
- Added section-row background shading for field groups.
- Kept drag-resizable details area.
- Explicitly hides the row-details controls on non-investigation tabs.
