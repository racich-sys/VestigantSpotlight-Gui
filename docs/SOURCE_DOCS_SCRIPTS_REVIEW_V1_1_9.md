# V1.1.9 Source Documentation / Text / PowerShell Review

This file records the required review of every `.md`, `.txt`, and `.ps1` file in the source package before the V1.1.9 package was created. The detailed inventory is in `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_9.csv`.

## Summary

- Reviewed files: 141
- Current version-specific scripts were regenerated as V1_1_9 scripts.
- Prior root-level package artifacts/manifests were removed from the active package when obsolete.
- Historical validation notes and append-only version history were retained, not truncated.
- `docs/BaselineVersionHistory.md`, `docs/FULL_VERSION_HISTORY.md`, and root `VERSION_HISTORY.md` remain the version-history baseline going forward.

## Roadmap-based decisions

- Keep all docs required for new-chat continuation: continuation guide, workflow ledger, roadmap checklist, suggestions tracker, help/manual/quick start/troubleshooting.
- Keep PowerShell scripts required for build, GUI launch, AFF4 thin runs, packaging existing cases, GitHub/project setup, source staging, upload ZIP creation, external comparison, AFF4 direct-map probing, iOS focused runs, and LZFSE vendor preparation.
- Keep historical validation notes as an audit trail; do not let old versions fall off the consolidated history.
- Remove only root-level stale package artifacts/manifests that duplicate append-only history and clutter the active source package.

## Removed in this cleanup

- `V1_1_7_1_DELETED_FILES_MANIFEST.md`
- `V1_1_8_DELETED_FILES_MANIFEST.md`
- `V1_1_7_1_patch_manifest.txt`

## V1.1.9 source changes covered by this review

- Promoted guarded live AFF4/APFS OMAP leaf traversal by following bounded APFS B-tree next-leaf links during OMAP target and root-tree lookups.
- Left full APFS absolute path reconstruction and full NSKeyedArchiver UID graph decode deferred until comparator/runtime validation is available.
