# Version History

## V0_9_34

V0_9_34 reviewed the V0_9_33 build/thin outputs and found the iOS reuse-cache workflow completed successfully.  It starts the requested full-tree cleanup pass by correcting stale version metadata, removing obsolete root notes and old version-specific scripts from the production package, consolidating documentation around the maintained manual/history/roadmap files, and documenting exactly what was removed or deferred.

# Vestigant Spotlight Version History

This file is now the current consolidated version-history entry point.

See `docs/CONSOLIDATED_VERSION_HISTORY.md` for the active, readable history.
Older per-version notes remain in the repository for traceability.

## V0_9_34

V0_9_34 reviewed the V0_9_31 build/thin result and found the run completed successfully with stable compact-mode counts. This release focuses on making the iOS Spotlight review workflow more usable for investigators:

- Added iOS - Investigator Overview as a start-here GUI view.
- Added iOS - Direct User Message Review for direct Apple Messages/SMS/RCS/iMessage text recovered from Spotlight compact context.
- Added iOS - Direct User Message Thread Summary to group direct messages by available contact/thread/handle context.
- Added iOS - Timeline Month Summary for compact timeline triage by month/category/anomaly.
- Added normal investigator exports for these views and smoke-test coverage.
- Preserved compact normal iOS mode and did not reintroduce full raw property, FFS, or app DB materialization.

