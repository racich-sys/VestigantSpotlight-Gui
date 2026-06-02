# Vestigant Spotlight Project Roadmap and Continuation

Current baseline: V0_9_42.

## Current status

V0_9_37 built successfully, but the V0_9_37 iOS reuse-cache run failed during native parse when the enlarged same-record Spotlight text-context rows exceeded the 5 GiB SQLite guardrail.  V0_9_42 keeps the Missing From FFS text-detail outputs but restores a compact normal-mode text budget and fixes fatal guardrail propagation so future guardrail hits stop cleanly.

## Near-term priorities

1. Validate V0_9_37 on Windows/MSVC and run the standard iOS reuse-cache test.
2. Continue improving iOS investigator views, especially direct messages, thread/contact summaries, timeline review, missing-from-FFS with text context, and parser diagnostics.
3. Add more schema/view smoke tests as new GUI views are added.
4. Keep normal iOS mode compact by default; broad native/dbStr/property persistence and full FFS/app DB materialization remain diagnostics/support options.
5. After two to three stable reuse-cache versions, run the same source through the fresh full FFS ZIP workflow to validate staging, ZIP enumeration, and slim FFS lookup creation without cache reuse.
6. Resume macOS AFF4/APFS work after iOS investigator views remain stable: focus on APFS container/filesystem enumeration, Store-V2 extraction/copy-out validation, group/source provenance, and external Store-V2 comparison.

## Backburner but useful

- Full Win32 MainWindow/global-state refactor.
- Mass enum replacement for magic strings except active parser areas.
- NSRL/hashset filtering.
- Relativity/eDiscovery load-file export.

## Cleanup policy

Production ZIPs should stay clean.  Keep current scripts, consolidated docs, source, tests, and required build files.  Avoid shipping old generated patch notes, stale version-specific wrappers, and repeated historical fragments unless they are explicitly part of maintained consolidated documentation.

## V0_9_37 - Missing From FFS text visibility

V0_9_37 addresses the user-reported issue that some Spotlight CSV reports did not show recovered Spotlight text/content.  It adds row-level Missing From FFS text detail and text coverage exports, exposes the same views in the GUI, increases compact same-record text context retention for reference-bearing iOS records, and documents when text is unavailable or suppressed by compact mode.



## V0_9_42 - Missing From FFS text visibility guardrail fix

V0_9_37 improved Missing From FFS text visibility but over-expanded same-record text context and hit the SQLite 5 GiB guardrail during native parse.  V0_9_42 keeps the text-detail views/exports but restores a bounded normal-mode text-context budget and fixes fatal guardrail propagation so runs stop cleanly if a guardrail is ever hit.

### V0_9_42 V1-readiness testing note

After V0_9_42 is validated with the standard reuse-cache script, run the new Stage B fresh-ZIP script against the same large iOS FFS ZIP. This tests actual ZIP enumeration/staging and the new non-regex 7-Zip inventory path without changing the parser's compact normal-mode assumptions. If Stage B succeeds, later cycles can selectively test support/correlation materialization under guardrails.

## V0_9_42 - Native C++ 7-Zip inventory parser

V0_9_42 reviewed the successful V0_9_41 reuse-cache run and carries forward the V1-readiness performance work. The CSV exporter fast path remains in place. The iOS focused ZIP workflow now lets 7-Zip dump `-slt` output to raw text and then rebuilds FFS/app database inventory CSVs using native C++ parsing rather than the PowerShell raw-listing parser. This is intended to make the Stage B fresh-ZIP test faster and closer to the 60-120 MB/s target where hardware permits.
