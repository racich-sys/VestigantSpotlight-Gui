# Validation History

## V0_9_36

- Reviewed uploaded V0_9_3 historical documentation archive (`Docs.zip`).
- Restored the historical V0_9 development process into `docs/CONSOLIDATED_VERSION_HISTORY.md`.
- Kept production package cleanup policy from V0_9_34: historical details are aggregated, not re-added as many root fragments.
- No parser/schema/export behavior changed.

## V0_9_36

- Reviewed V0_9_33 Windows build log and iOS thin upload.
- Confirmed V0_9_33 built successfully and the run reached `complete_success`.
- Performed source-tree cleanup of stale documentation/scripts.
- Corrected stale VERSION/VERSION.txt metadata.
- Added warning-hygiene casts for intentionally unused native parser helper parameters.
- Created current V0_9_36 scripts and removed old version-specific wrappers from the production package.

## V0_9_36

- Input reviewed: V0_9_29 build log and V0_9_29 iOS reuse-cache thin upload.
- V0_9_29 status: Windows/MSVC build completed, GUI linked, and iOS reuse-cache run reached `complete_success`.
- V0_9_29 stable counts: raw records 344,445; compact raw key/value rows 982,230; compact raw date candidates 336,037.
- V0_9_36 validation focus: documentation consolidation, improved compact message/body extraction, parser diagnostics detail sample, schema smoke coverage, and MSVC raw-string risk checks.

See `docs/CONSOLIDATED_VERSION_HISTORY.md` for current user-facing version history.
