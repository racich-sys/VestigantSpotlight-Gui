# Release Notes

Current version: 0.9.34

## V0_9_34

V0_9_34 is a cleanup/consolidation release after the V0_9_33 build and thin upload completed successfully.  The release keeps parser behavior stable and focuses on production-package hygiene:

- Reviewed the V0_9_33 build/thin output.  The run completed successfully with stable compact-mode iOS counts.
- Corrected VERSION and VERSION.txt so the source package version is no longer stale.
- Removed stale root per-version validation-note fragments and old review snippets from the production ZIP.
- Removed old version-specific PowerShell wrappers from the production ZIP and kept current V0_9_34 wrappers plus generic utility scripts.
- Removed superseded old V0_7/V0_8 documentation fragments from the production ZIP.
- Added `docs/PACKAGE_CLEANUP_SUMMARY.md` and `docs/V0_9_34_REVIEW_NOTES.md`.
- Preserved consolidated help/manual/version/roadmap files as the maintained documentation set.
- Fixed minor MSVC warning hygiene in the native parser by explicitly marking intentionally unused helper parameters.

Normal iOS mode remains compact and Spotlight-first.  No broad raw native/dbStr/property persistence, full FFS inventory materialization, or broad app DB record materialization was reintroduced.
