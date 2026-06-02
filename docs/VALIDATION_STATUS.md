# Vestigant Spotlight Validation Status

Current version: 0.9.42

V0_9_42 is a parser stability and Missing From FFS text-visibility guardrail fix.  It is based on the V0_9_37 source and keeps the consolidated documentation/history model.

Validation performed during packaging:

- Reviewed `V0_9_37_build.log` and `Upload_Thin_iOS_GUI_V0_9_37_ReusedCache_Check.zip`.
- Confirmed V0_9_37 Windows/MSVC build succeeded.
- Confirmed the V0_9_37 run failed at the SQLite DB-size guardrail during native parsing after text-context expansion.
- Reduced normal-mode text context budget while preserving Missing From FFS text-detail views.
- Fixed fatal native guardrail exception propagation.
- Confirmed syntax/static validation in this environment.

Required external validation:

1. Run Windows/MSVC build.
2. Confirm CLI reports `Vestigant Spotlight v0.9.42`.
3. Run self-test.
4. Run the standard iOS reuse-cache script and confirm it reaches `complete_success` without DB/WAL guardrail failure.

## V0_9_37 - Missing From FFS text visibility

V0_9_37 addresses the user-reported issue that some Spotlight CSV reports did not show recovered Spotlight text/content.  It adds row-level Missing From FFS text detail and text coverage exports, exposes the same views in the GUI, increases compact same-record text context retention for reference-bearing iOS records, and documents when text is unavailable or suppressed by compact mode.



## V0_9_42 - Missing From FFS text visibility guardrail fix

V0_9_37 improved Missing From FFS text visibility but over-expanded same-record text context and hit the SQLite 5 GiB guardrail during native parse.  V0_9_42 keeps the text-detail views/exports but restores a bounded normal-mode text-context budget and fixes fatal guardrail propagation so runs stop cleanly if a guardrail is ever hit.

### V0_9_42 V1-readiness note

V0_9_42 tightens normal iOS compact-mode text storage to keep Missing From FFS text visibility without exceeding the DB guardrail on the current large iOS source. GUI review-page loads are now tracked and cancellable instead of detached.

## V0_9_42 - Native C++ 7-Zip inventory parser

V0_9_42 reviewed the successful V0_9_41 reuse-cache run and carries forward the V1-readiness performance work. The CSV exporter fast path remains in place. The iOS focused ZIP workflow now lets 7-Zip dump `-slt` output to raw text and then rebuilds FFS/app database inventory CSVs using native C++ parsing rather than the PowerShell raw-listing parser. This is intended to make the Stage B fresh-ZIP test faster and closer to the 60-120 MB/s target where hardware permits.
