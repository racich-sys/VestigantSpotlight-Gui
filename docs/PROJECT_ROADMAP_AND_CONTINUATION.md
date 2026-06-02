# Vestigant Spotlight Project Roadmap and Continuation

Current baseline: V0_9_36.

## Current status

V0_9_36 is a documentation-history repair release.  It restores detailed V0_9 historical development information from the uploaded V0_9_3 docs archive into `docs/CONSOLIDATED_VERSION_HISTORY.md` while keeping the clean production-package approach introduced in V0_9_34.  Parser behavior remains unchanged from V0_9_34.

## Near-term next steps

1. Validate V0_9_36 on Windows/MSVC.
2. Continue iOS Spotlight investigator-view refinement using the saved thin-upload review workflow.
3. Begin Stage B fresh full iOS FFS ZIP testing after the current reuse-cache path remains stable.
4. Keep AFF4/APFS work staged until iOS investigator views and fresh ZIP workflow are stable.
5. Continue safe schema/view cleanup and smoke-test coverage; avoid broad GUI refactors unless tied to observed failures.

# Project Roadmap and Continuation

Current baseline: V0_9_36.

## Current status

V0_9_33 built successfully on Windows/MSVC and the iOS reuse-cache run reached `complete_success` with stable compact-mode counts.  V0_9_36 is a safe cleanup/consolidation release: it corrects stale package metadata, removes old documentation/script clutter from the production ZIP, and preserves the current iOS Spotlight parser/view behavior.

## Near-term priorities

1. Validate V0_9_36 on Windows/MSVC and run the standard iOS reuse-cache test.
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
