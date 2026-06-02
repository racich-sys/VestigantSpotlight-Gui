# Vestigant Spotlight User Manual

This document now redirects to the consolidated manual.

Use `docs/CONSOLIDATED_USER_MANUAL.md` as the current user-facing help/manual for version 0.9.34 and later.

### V0_9_34 iOS investigator start path

For iOS CoreSpotlight cases, start with `iOS - Investigator Overview`. It lists the recommended review surfaces and current row counts. For communications, use `iOS - Direct User Message Thread Summary` first to identify high-volume or notable contacts/threads, then open `iOS - Direct User Message Review` for row-level Spotlight message text and validation locators. Use `iOS - Timeline Month Summary` to identify date ranges before opening row-level timeline samples.

These views are Spotlight-first. The rows are evidence from the Spotlight/CoreSpotlight index and should be corroborated with SMS.db, app databases, or FFS lookup when those support sources are available. Normal iOS mode remains compact: it does not persist every raw native property or materialize full FFS/app DB tables by default.

