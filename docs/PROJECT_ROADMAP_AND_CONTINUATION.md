# Vestigant Spotlight Project Roadmap and Continuation

Current baseline: V0_9_46.

## Current status

V0_9_42 was reviewed from the uploaded build log, source, and reuse-cache thin output. The Windows/MSVC build succeeded, V0_9_42 reuse-cache reached `complete_success`, and the prior performance fixes for CSV export and native 7-Zip raw inventory parsing were already present in source. V0_9_44 therefore moves to bounded iOS investigative-value improvement rather than another speed-only patch.


## V0_9_46 focus

V0_9_46 is a fresh-ZIP inventory correctness release. V0_9_44 confirmed that the native 7-Zip raw inventory path works and parses the large iOS ZIP, but the app-database inventory was too broad. V0_9_46 keeps normal mode Spotlight-first and compact, narrows app database inventory to database-like file names, and preserves extracted database paths where targeted extraction produced staged SQLite databases.

Next validation should confirm that Stage B fresh-ZIP still reports nonzero FFS inventory rows while app database candidates drop from the overbroad V0_9_44 value of 131,610 to a database-like candidate set, and that known WhatsApp/SMS databases retain extracted_path when staged.

## V0_9_44 focus

V0_9_44 adds compact bplist / NSKeyedArchiver marker discovery for iOS CoreSpotlight values. This is a bounded discovery scaffold: it detects likely binary plist / NSKeyedArchiver values and extracts limited printable-token context so investigators can identify records that may warrant deeper decoding. It does not claim full NSKeyedArchiver graph decoding.

## Near-term priorities

1. Validate V0_9_44 on Windows/MSVC and run the standard iOS reuse-cache script.
2. Review the new `iOS - Bplist/NSKeyedArchiver Summary` and `iOS - Bplist/NSKeyedArchiver Detail` GUI views/exports.
3. If reuse-cache validation succeeds, run Stage B fresh-ZIP testing against the large iOS FFS ZIP to validate the native 7-Zip inventory path without cache reuse.
4. Continue improving iOS investigator views, especially direct messages, thread/contact summaries, timeline review, Missing From FFS text context, parser diagnostics, and bplist/NSKeyedArchiver discovery surfaces.
5. Add full NSKeyedArchiver / bplist object-graph decoding only after bounded diagnostics identify useful target classes and Stage B fresh-ZIP stability is confirmed.
6. Resume macOS AFF4/APFS work after iOS investigator views remain stable: focus on APFS extraction, Store-V2 extraction/copy-out validation, LZFSE/LZVN decmpfs, group/source provenance, and external Store-V2 comparison.

## Backburner but useful

- Full Win32 MainWindow/global-state refactor.
- Mass enum replacement for magic strings except active parser areas.
- KnowledgeC.db parsing/correlation, after CoreSpotlight V1 surfaces are stable.
- Timeline anomaly/timestomping review, after date provenance is stable enough for defensible interpretation.
- NSRL/hashset filtering.
- Relativity/eDiscovery load-file export.

## Cleanup policy

Production ZIPs should stay clean. Keep current scripts, consolidated docs, source, tests, and required build files. Avoid shipping old generated patch notes, stale version-specific wrappers, and repeated historical fragments unless they are explicitly part of maintained consolidated documentation.