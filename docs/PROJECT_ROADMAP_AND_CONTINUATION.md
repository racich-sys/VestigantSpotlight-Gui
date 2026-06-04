# Vestigant Spotlight Project Roadmap and Continuation

Current baseline: V0_9_53.

## Current status

V0_9_48 reuse-cache validation completed successfully. Normal Spotlight-first mode now parses targeted already-extracted high-value app databases and produced 525,409 parsed app records in the uploaded thin result. The investigator super timeline sample is populated and time-anomaly export produced rows. The remaining near-term V1 gap addressed by V0_9_53 is GUI usability when reviewing wide result tables.

## V0_9_53 focus

V0_9_53 adds a bottom details pane to the shared investigation grid. The pane displays every column for the selected row vertically, including long text and JSON-like metadata, so investigators do not need to scroll horizontally across wide Spotlight/app database views. The pane is read-only, scrollable, copy-friendly, and works for both macOS and iOS investigation views because those tabs share the review grid implementation.

## Minimal testing loop now that reuse-cache and fresh-ZIP both complete

1. Build the current version.
2. Open an existing completed case in the GUI.
3. Test view selection, row selection, arrow-key navigation, details pane scrolling/copying, search/filter, tags/checkmarks, and exports.
4. Run reuse-cache only when a change creates new parser rows or changes app DB parsing.
5. Run fresh-ZIP only when a change touches ZIP inventory, staging, FFS/app database extraction, cache creation, or source/container handling.

## Near-term V1 priorities

1. Validate V0_9_53 Windows/MSVC GUI build.
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

For the next review, upload `V0_9_53_build.log` and either screenshots/notes from opening an existing case or a thin upload only if an ingest/export was run.
