# Vestigant Spotlight Version History

Current version: 0.9.38

The maintained full history is `docs/CONSOLIDATED_VERSION_HISTORY.md`.  V0_9_37 restored historical V0_9 details from the uploaded V0_9_3 documentation archive into that single consolidated history.

## V0_9_37 - Missing From FFS text visibility

V0_9_37 addresses the user-reported issue that some Spotlight CSV reports did not show recovered Spotlight text/content.  It adds row-level Missing From FFS text detail and text coverage exports, exposes the same views in the GUI, increases compact same-record text context retention for reference-bearing iOS records, and documents when text is unavailable or suppressed by compact mode.



## V0_9_38 - Missing From FFS text visibility guardrail fix

V0_9_37 improved Missing From FFS text visibility but over-expanded same-record text context and hit the SQLite 5 GiB guardrail during native parse.  V0_9_38 keeps the text-detail views/exports but restores a bounded normal-mode text-context budget and fixes fatal guardrail propagation so runs stop cleanly if a guardrail is ever hit.
