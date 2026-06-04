# Vestigant Spotlight V0_9_59 Notes

V0_9_59 is a V1 production-readiness cleanup after V0_9_57 compiled and ran on Windows. It improves the processing workflow and review workflow without changing parser interpretation logic.

Key changes:
- The Case Information bottom log is now the live processing log. It clears at run start, timestamps messages, mirrors run/progress status, and emits periodic heartbeat messages while processing continues.
- View loading now shows an explicit marquee progress indicator above the investigation grid and a loading message in the details pane so long SQLite view loads are not mistaken for hangs.
- The V1 GUI source selector now exposes only fully implemented Folder and ZIP intake paths. AFF4/APFS and raw image support remain roadmap items and are not presented as clickable V1 options.
- Legacy V7-only schema tables/indexes were removed from new case initialization.
- CLI/operator self-test mode is deprecated; the automated test executable uses an internal automated self-test path.
- Duplicate AFF4/APFS child/descendant root-tree probe output writers were consolidated into one traversal-output writer.

Validation summary:
- Linux CMake configure/build passed.
- VestigantSpotlightTests passed.
- C++20 syntax checks passed for modified non-Windows translation units.
- Windows/MSVC GUI compile and runtime validation remain required.

# Vestigant Spotlight Project Roadmap and Continuation

## V0_9_59 - Windows GUI forward-declaration compile hotfix

V0_9_59 is a focused Windows/MSVC GUI build hotfix after V0_9_56 reached the GUI compile stage and failed with `C3861: setReviewSummary identifier not found` in `src\gui\win32_gui.cpp`. The fix adds a forward declaration for `setReviewSummary(const std::wstring&)` before the custom view-set helper functions that call it. No parser, ingest, cache, ZIP, FFS inventory, app DB, export, or forensic interpretation behavior was intentionally changed.

Current baseline: V0_9_59.

## V0_9_59 - Windows MSVC batch-label build hotfix

V0_9_59 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## Current status

V0_9_48 reuse-cache validation completed successfully. Normal Spotlight-first mode now parses targeted already-extracted high-value app databases and produced 525,409 parsed app records in the uploaded thin result. The investigator super timeline sample is populated and time-anomaly export produced rows. The remaining near-term V1 gap addressed by V0_9_55 is GUI usability when reviewing wide result tables.

## V0_9_55 focus

V0_9_55 adds a bottom details pane to the shared investigation grid. The pane displays every column for the selected row vertically, including long text and JSON-like metadata, so investigators do not need to scroll horizontally across wide Spotlight/app database views. The pane is read-only, scrollable, copy-friendly, and works for both macOS and iOS investigation views because those tabs share the review grid implementation.

## Minimal testing loop now that reuse-cache and fresh-ZIP both complete

1. Build the current version.
2. Open an existing completed case in the GUI.
3. Test view selection, row selection, arrow-key navigation, details pane scrolling/copying, search/filter, tags/checkmarks, and exports.
4. Run reuse-cache only when a change creates new parser rows or changes app DB parsing.
5. Run fresh-ZIP only when a change touches ZIP inventory, staging, FFS/app database extraction, cache creation, or source/container handling.

## Near-term V1 priorities

1. Validate V0_9_55 Windows/MSVC GUI build.
2. Open the latest successful completed case and test the details pane against wide iOS views such as text context, parsed app records, super timeline, Missing From FFS text detail, and bplist/NSKeyedArchiver detail.
3. Continue refining V1 review surfaces: direct messages, contact/thread summaries, super timeline, Missing From FFS text context, KnowledgeC correlation, parser diagnostics, and provenance warnings.
4. Keep normal mode compact and Spotlight-first. Do not reintroduce broad FFS materialization or broad app DB materialization by default.
5. Add full NSKeyedArchiver object graph decoding only after bounded diagnostics identify useful target classes.
6. Resume macOS AFF4/APFS work after iOS V1 investigator workflows remain stable.

## Deferred / requires external source validation

- LZFSE/LZVN integration requires vetted codec source and build-system/license validation.
- Broad Win32 MainWindow/global-state refactor remains deferred unless needed to fix active defects.
- Litigation/eDiscovery load-file exports, NSRL/hashset filtering, and broad architectural refactors remain staged after V1 usability/stability.

## Next upload needed

For the next review, upload `V0_9_55_build.log` and either screenshots/notes from opening an existing case or a thin upload only if an ingest/export was run.
