# Source Package Cleanup Policy

The active source package should be uploadable into a new chat without requiring a pile of old version scripts or root-level artifact manifests.

## Keep

- Current version build/run/GUI/package scripts.
- Generic project/release scripts.
- Source code, SQL, tests, third-party source, resources.
- Append-only history and roadmap files in `docs/`.
- Current validation notes.
- Build instructions, manual, help, and troubleshooting.

## Remove from active source root

- Old version-specific build/run/launch/package scripts when superseded by the current version.
- Old root-level patch manifests and deletion manifests.
- Generated binaries, object files, build directories, temporary extracted cases, and thin outputs.

## Preserve old history

Do not delete old version entries from append-only documentation. Consolidate historical data in:

- `docs/FULL_VERSION_HISTORY.md`
- `docs/VERSION_HISTORY_APPEND_ONLY_POLICY.md`
- `docs/VERSION_HISTORY_DETECTED_VERSIONS.csv`
- `docs/VERSION_HISTORY_SOURCE_INVENTORY.csv`

