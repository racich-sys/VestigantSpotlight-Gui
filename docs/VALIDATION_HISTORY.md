# Validation History

## V0_9_37

- Reviewed uploaded V0_9_3 historical documentation archive (`Docs.zip`).
- Restored the historical V0_9 development process into `docs/CONSOLIDATED_VERSION_HISTORY.md`.
- Kept production package cleanup policy from V0_9_34: historical details are aggregated, not re-added as many root fragments.
- No parser/schema/export behavior changed.

## V0_9_37

- Reviewed V0_9_33 Windows build log and iOS thin upload.
- Confirmed V0_9_33 built successfully and the run reached `complete_success`.
- Performed source-tree cleanup of stale documentation/scripts.
- Corrected stale VERSION/VERSION.txt metadata.
- Added warning-hygiene casts for intentionally unused native parser helper parameters.
- Created current V0_9_37 scripts and removed old version-specific wrappers from the production package.

## V0_9_37

- Input reviewed: V0_9_29 build log and V0_9_29 iOS reuse-cache thin upload.
- V0_9_29 status: Windows/MSVC build completed, GUI linked, and iOS reuse-cache run reached `complete_success`.
- V0_9_29 stable counts: raw records 344,445; compact raw key/value rows 982,230; compact raw date candidates 336,037.
- V0_9_37 validation focus: documentation consolidation, improved compact message/body extraction, parser diagnostics detail sample, schema smoke coverage, and MSVC raw-string risk checks.

See `docs/CONSOLIDATED_VERSION_HISTORY.md` for current user-facing version history.

## V0_9_37 - Missing From FFS text visibility

V0_9_37 addresses the user-reported issue that some Spotlight CSV reports did not show recovered Spotlight text/content.  It adds row-level Missing From FFS text detail and text coverage exports, exposes the same views in the GUI, increases compact same-record text context retention for reference-bearing iOS records, and documents when text is unavailable or suppressed by compact mode.



## V0_9_42 - Missing From FFS text visibility guardrail fix

V0_9_37 improved Missing From FFS text visibility but over-expanded same-record text context and hit the SQLite 5 GiB guardrail during native parse.  V0_9_42 keeps the text-detail views/exports but restores a bounded normal-mode text-context budget and fixes fatal guardrail propagation so runs stop cleanly if a guardrail is ever hit.

## V0_9_42 - Native C++ 7-Zip inventory parser

V0_9_42 reviewed the successful V0_9_41 reuse-cache run and carries forward the V1-readiness performance work. The CSV exporter fast path remains in place. The iOS focused ZIP workflow now lets 7-Zip dump `-slt` output to raw text and then rebuilds FFS/app database inventory CSVs using native C++ parsing rather than the PowerShell raw-listing parser. This is intended to make the Stage B fresh-ZIP test faster and closer to the 60-120 MB/s target where hardware permits.
