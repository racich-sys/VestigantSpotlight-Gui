# Vestigant Spotlight Version History

Current version: 0.9.43


## V0_9_43 - Bplist / NSKeyedArchiver discovery scaffold

V0_9_43 reviewed the successful V0_9_42 Windows build and iOS reuse-cache thin result. Because the performance work from V0_9_42 was already present and the reuse-cache path completed, the release adds a bounded investigative scaffold for likely iOS CoreSpotlight binary plist / NSKeyedArchiver values. The new parser logic detects bplist and NSKeyedArchiver markers, extracts a bounded printable-token context, stores one compact synthetic context row per matching record, and surfaces the results through CaseDatabase views, GUI views, normal CSV exports, upload samples, review index, and case summary text. It is intentionally not a full NSKeyedArchiver graph decoder; that remains a future parser phase after Stage B fresh-ZIP validation.

The maintained full history is `docs/CONSOLIDATED_VERSION_HISTORY.md`.  V0_9_37 restored historical V0_9 details from the uploaded V0_9_3 documentation archive into that single consolidated history.

## V0_9_37 - Missing From FFS text visibility

V0_9_37 addresses the user-reported issue that some Spotlight CSV reports did not show recovered Spotlight text/content.  It adds row-level Missing From FFS text detail and text coverage exports, exposes the same views in the GUI, increases compact same-record text context retention for reference-bearing iOS records, and documents when text is unavailable or suppressed by compact mode.



## V0_9_39 - Missing From FFS text visibility guardrail fix

V0_9_37 improved Missing From FFS text visibility but over-expanded same-record text context and hit the SQLite 5 GiB guardrail during native parse.  V0_9_39 keeps the text-detail views/exports but restores a bounded normal-mode text-context budget and fixes fatal guardrail propagation so runs stop cleanly if a guardrail is ever hit.

## V0_9_42 - V1-readiness performance pass

V0_9_42 reviewed the successful V0_9_39 build/thin result and adds targeted performance work for the next testing phase: faster CSV export buffering/escaping, larger sequential hash-read buffers, faster non-regex 7-Zip inventory parsing for actual FFS ZIP runs, and a dedicated Stage B fresh-ZIP test script. The full chronological history remains in `docs/CONSOLIDATED_VERSION_HISTORY.md`.

## V0_9_42 - Native C++ 7-Zip inventory parser

V0_9_42 reviewed the successful V0_9_41 reuse-cache run and carries forward the V1-readiness performance work. The CSV exporter fast path remains in place. The iOS focused ZIP workflow now lets 7-Zip dump `-slt` output to raw text and then rebuilds FFS/app database inventory CSVs using native C++ parsing rather than the PowerShell raw-listing parser. This is intended to make the Stage B fresh-ZIP test faster and closer to the 60-120 MB/s target where hardware permits.
