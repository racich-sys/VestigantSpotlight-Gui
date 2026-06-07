# Vestigant Spotlight Version History

This file is intentionally short. The append-only project history baseline is maintained in:

- `docs/FULL_VERSION_HISTORY.md`
- `docs/VERSION_HISTORY_APPEND_ONLY_POLICY.md`
- `docs/VERSION_HISTORY_DETECTED_VERSIONS.csv`
- `docs/VERSION_HISTORY_SOURCE_INVENTORY.csv`

Do not truncate old version entries. Append new versions to `docs/FULL_VERSION_HISTORY.md` and update the workflow ledger/checklists for every package.

## Current package

### V1.1.7.1
- Build hotfix for V1.1.7 after the AFF4 dynamic probe relocation: moved/exposed missing worker-local helpers required by `src/parsers/aff4_probe_worker.cpp`.
- Package cleanup: removed obsolete version-specific scripts and root-level old package artifacts/manifests while preserving append-only history in `docs/`.
- Added/updated new-chat continuation guide, source-package layout notes, and workflow ledger instructions.
