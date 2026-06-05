# V1.0.15 Production Modularization Review

## Implemented

- Preserved the V1.0.14 iOS parser modularization: Apple Messages, WhatsApp, KnowledgeC, and generic iOS app DB row parsing are delegated through `iosAppDbParseTable(...)`.
- Preserved the centralized GUI view registry; `ViewSpec` and `views()` remain owned by `src/gui/view_registry.*`.
- Kept AFF4/APFS structural diagnostics suppressed by default while keeping copy-out, normalized staging, parser, enrichment, and external comparison outputs active.
- Added a Store-V2 dual-process candidate compare to audit raw copy-out candidate rows against normalized staged rows.

## Not implemented

- The live AFF4/APFS traversal was not replaced with the lower-bound iterator in this iteration. V1.0.14 already staged files and parsed records; replacing the live path before comparator evidence would risk regression.
- LZFSE/LZVN decompression was not enabled because no vetted codec implementation and known-good vectors were present in the provided repository inputs.

## Next benchmarks

- Run V1.0.15 and review `aff4_apfs_storev2_candidate_dual_process_compare_summary.json`.
- Prioritize any `STAGED_ROW_DIFFERS_FROM_BEST_COPYOUT_CANDIDATE` rows.
- Compare summary counts against `aff4_apfs_external_spotlight_compare_summary.json`.
- Implement the lower-bound APFS iterator as a diagnostic comparator only after candidate selection is stable.
