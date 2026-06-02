# Vestigant Spotlight Validation Status

Current version: 0.9.36

V0_9_36 is a documentation-history repair release.  No parser, schema, GUI, export, or forensic interpretation behavior was intentionally changed from V0_9_34.

Validation performed during packaging:

- Reviewed uploaded V0_9_3 `Docs.zip` documentation archive.
- Restored historical V0_9 details into `docs/CONSOLIDATED_VERSION_HISTORY.md`.
- Confirmed version metadata updated to 0.9.36.
- Confirmed no root-level historical `V0_*_CHANGE_VALIDATION_NOTE.txt` fragments were reintroduced.
- Confirmed package/patch ZIP integrity and SHA256 files.

Required external validation:

1. Run Windows/MSVC build.
2. Confirm CLI reports `Vestigant Spotlight v0.9.36`.
3. Run self-test.
4. Run the standard iOS reuse-cache script only if you want a fresh runtime confirmation; behavior should match V0_9_34 because no parser/export behavior changed.
