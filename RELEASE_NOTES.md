# Vestigant Spotlight Release Notes

Current version: 0.9.36

## V0_9_36

V0_9_36 is a documentation-history repair release after V0_9_34 cleanup compressed too much historical detail.

Changes:

- Reviewed the uploaded V0_9_3 documentation archive (`Docs.zip`).
- Restored historical V0_9 development information into `docs/CONSOLIDATED_VERSION_HISTORY.md`.
- Kept the production package clean by aggregating historical notes instead of reintroducing many stale per-version fragments.
- Updated `docs/CONSOLIDATED_USER_MANUAL.md` to explain the current documentation model, standard workflows, iOS review start path, compact-mode interpretation, diagnostics, and AFF4/APFS roadmap location.
- Updated top-level `HELP.md`, `VERSION_HISTORY.md`, validation notes, roadmap, and package-cleanup notes to explain the restored version history.
- Updated version metadata and scripts to V0_9_36.
- No parser, schema, GUI, export, or forensic interpretation behavior was intentionally changed from V0_9_34.

Validation performed in this environment:

- Confirmed version metadata was updated to 0.9.36.
- Confirmed consolidated documentation contains restored entries for V0_9_0 through V0_9_36 based on available historical notes.
- Confirmed production package still avoids reintroducing root-level historical fragments.
- Confirmed ZIP/patch integrity and SHA256 files.

Windows/MSVC build validation remains required because only documentation/version/script metadata changed in this packaging environment.
