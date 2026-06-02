# Vestigant Spotlight Roadmap and Continuation

Current baseline: V0_9_33.

## Immediate priorities

1. Validate V0_9_33 on Windows/MSVC and run the standard iOS reuse-cache test.
2. Continue improving iOS Spotlight investigator views, especially message/mail/call/contact/media timeline review.
3. Keep normal iOS mode compact and Spotlight-first.
4. Expand parser diagnostics/unparsed visibility without broad raw-property materialization.
5. Continue schema/view smoke tests and move any remaining GUI-owned SQL into database/schema ownership when touched.
6. Keep the consolidated manual and version history current every release.

## Useful but staged DFIR features

- Parser/version metadata and source/provenance tracking: high priority.
- Parser diagnostics/unparsed-bin visibility: high priority.
- Normalized timeline and anomaly flags: high priority, continue incrementally.
- Noise-reduction toggles/views: medium-high priority.
- Persistent tags/notes: medium-high priority after current iOS views stabilize.
- JSONL/Plaso-compatible exports: medium priority.
- NSRL/hashset filtering and eDiscovery/Relativity load files: backburner until core iOS Spotlight review is stable.
- Broad Win32 architectural refactors: defer unless tied to active failures.

## Review process for next upload

Use `docs/THIN_UPLOAD_REVIEW_WORKFLOW.md`: inspect build log; unpack thin upload; read run status/progress/log/summary/limits; compare headline counts; inspect representative CSV samples; identify the next failure/improvement class; patch latest source; run checks; package full and patch ZIPs.


V0_9_33 roadmap update:
- Continue prioritizing iOS Spotlight investigator usefulness over broad refactors.
- Keep normal iOS mode compact; use support/diagnostic modes for full native/FFS/app DB materialization.
- Near-term targets: improve parser diagnostics for high-volume native ID range failures, add more timeline/date provenance review, and move additional schema/view SQL into database-owned smoke-tested modules where practical.
- Backburner: Relativity/load-file exports, NSRL filtering, and broad Win32 MainWindow/global-state refactors until iOS views are stable.

## V0_9_33

V0_9_33 reviewed the V0_9_31 build/thin result and found the run completed successfully with stable compact-mode counts. This release focuses on making the iOS Spotlight review workflow more usable for investigators:

- Added iOS - Investigator Overview as a start-here GUI view.
- Added iOS - Direct User Message Review for direct Apple Messages/SMS/RCS/iMessage text recovered from Spotlight compact context.
- Added iOS - Direct User Message Thread Summary to group direct messages by available contact/thread/handle context.
- Added iOS - Timeline Month Summary for compact timeline triage by month/category/anomaly.
- Added normal investigator exports for these views and smoke-test coverage.
- Preserved compact normal iOS mode and did not reintroduce full raw property, FFS, or app DB materialization.

## V0_9_33 roadmap update

V0_9_33 adds `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md` and `docs/V0_9_33_REVIEW_NOTES.md`. The next development priorities are:

1. Keep the iOS reuse-cache workflow stable while improving investigator view usefulness.
2. Add a smaller `what to review first` dashboard and reduce reliance on very large row-level sample CSVs.
3. Move to a fresh full iOS FFS ZIP test after 2-3 stable reuse-cache iterations.
4. Then test selective support/correlation materialization for FFS and app DB records under guardrails.
5. Resume macOS AFF4/APFS work after iOS review surfaces are stable enough, beginning with image inventory and Store-V2 extraction validation.

See `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md` for the full staged plan.
