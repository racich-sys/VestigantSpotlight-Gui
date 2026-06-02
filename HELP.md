# Vestigant Spotlight Help

Current version: 0.9.33

Start here:

- `docs/CONSOLIDATED_USER_MANUAL.md` — current user-facing workflow/help for GUI, CLI, iOS CoreSpotlight, macOS/AFF4/APFS status, exports, limits, diagnostics, and troubleshooting.
- `docs/CONSOLIDATED_VERSION_HISTORY.md` — consolidated current version history.
- `docs/THIN_UPLOAD_REVIEW_WORKFLOW.md` — repeatable review workflow for uploaded build logs and thin bundles.

Older per-version notes remain in the repository for traceability, but the consolidated manual and version history should be treated as the current help source.

### V0_9_33 iOS investigator start path

For iOS CoreSpotlight cases, start with `iOS - Investigator Overview`. It lists the recommended review surfaces and current row counts. For communications, use `iOS - Direct User Message Thread Summary` first to identify high-volume or notable contacts/threads, then open `iOS - Direct User Message Review` for row-level Spotlight message text and validation locators. Use `iOS - Timeline Month Summary` to identify date ranges before opening row-level timeline samples.

These views are Spotlight-first. The rows are evidence from the Spotlight/CoreSpotlight index and should be corroborated with SMS.db, app databases, or FFS lookup when those support sources are available. Normal iOS mode remains compact: it does not persist every raw native property or materialize full FFS/app DB tables by default.

## V0_9_33 roadmap / testing timeline

The current detailed roadmap, testing-source transition plan, and AFF4/APFS work plan are in `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md`. Use this before changing source type or switching from reuse-cache testing to fresh full-ZIP or support/correlation runs.
