# V1.0.6 Deleted/Stale File Manifest

The clean full V1.0.6 package omits execution files that should not remain in the active V1 production source tree:

- `scripts/Run-SelfTest.ps1` — removed because primary application self-test routing/fake evidence generation is not appropriate for production forensic operation.
- V0.9-specific run/package/collect scripts — none are present in the clean V1.0.6 package.
- V0.9 validation summaries and `docs/codex_notes/CHANGES_Codex_*.md` — none are present in the clean V1.0.6 package.

Patch overlays cannot delete files already present in an existing extracted folder. For a clean tree, extract the full V1.0.6 ZIP into a fresh `T:\VestigantSpotlightInv_V1_0_6` folder.
