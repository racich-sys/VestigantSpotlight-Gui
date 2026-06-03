# Vestigant Spotlight Project Roadmap and Continuation

Current baseline: V0_9_48.

## Current status

V0_9_46 was validated from uploaded Windows/MSVC build log plus reuse-cache and Stage B fresh-ZIP thin outputs. Build/version consistency is fixed, both runs completed successfully, the fresh-ZIP native 7-Zip inventory path remains functional, and the app database candidate set remains narrowed to database-like entries.

## V0_9_48 focus

V0_9_48 moves from inventory correctness to bounded iOS investigative value while preserving compact normal mode. It adds conservative bplist object-string extraction, KnowledgeC/CoreDuet classification and support-mode parser scaffolding, KnowledgeC interaction review views/exports, and an explicit investigator time-anomaly triage view with provenance caution.

## Near-term priorities

1. Validate V0_9_48 on Windows/MSVC and run the standard iOS reuse-cache script.
2. Run Stage B fresh-ZIP validation and confirm FFS/app-database inventory remains stable.
3. Inspect `iOS - Bplist/NSKeyedArchiver Detail` for improved string extraction quality and confirm no DB/WAL bloat regression.
4. Validate KnowledgeC/CoreDuet database classification/extraction in fresh-ZIP output.
5. After normal-mode stability is confirmed, run support/full app DB materialization to validate `iOS - KnowledgeC Interaction Summary` and `iOS - KnowledgeC Interaction Events`.
6. Continue improving iOS investigator views, especially direct messages, thread/contact summaries, timeline review, Missing From FFS text context, parser diagnostics, and bplist/NSKeyedArchiver discovery surfaces.
7. Add full NSKeyedArchiver object-graph decoding only after bounded diagnostics identify useful target classes.
8. Resume macOS AFF4/APFS work after iOS investigator views remain stable, with focus on APFS extraction, Store-V2 extraction/copy-out validation, LZFSE/LZVN decmpfs, group/source provenance, and external Store-V2 comparison.

## Deferred / requires external source validation

- LZFSE/LZVN integration requires adding Apple/reference codec source to the build and validating license/build compatibility; do not add blind stubs that cannot decode actual data.
- Full NSKeyedArchiver graph decoding requires a bounded, clean-room parser with cycle/depth/object-count guards.

## Backburner but useful

- Full Win32 MainWindow/global-state refactor.
- Mass enum replacement for magic strings except active parser areas.
- KnowledgeC.db parsing/correlation, after CoreSpotlight V1 surfaces are stable.
- Timeline anomaly/timestomping review, after date provenance is stable enough for defensible interpretation.
- NSRL/hashset filtering.
- Relativity/eDiscovery load-file export.

## Cleanup policy

Production ZIPs should stay clean. Keep current scripts, consolidated docs, source, tests, and required build files. Avoid shipping old generated patch notes, stale version-specific wrappers, and repeated historical fragments unless they are explicitly part of maintained consolidated documentation.
## V0_9_48 continuation note
V0_9_48 should be tested with the normal reuse-cache and fresh-ZIP scripts. Key expected improvement: app DB parsed summaries and KnowledgeC/super-timeline outputs should no longer be empty when extracted high-value databases are available. Preserve the compact normal-mode guardrail: full FFS inventory and broad app DB parsing remain opt-in support/full behavior. Next likely work after V0_9_48 results: refine KnowledgeC field mapping from actual populated rows, correlate KnowledgeC document titles/bundle IDs with Spotlight records, and consider vetted LZFSE/LZVN integration only after codec source is supplied and license/build implications are reviewed.
