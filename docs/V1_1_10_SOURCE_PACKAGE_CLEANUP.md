# V1.1.10 Source Package Cleanup

V1.1.10 uses V1.1.9.1 as the base and performs a package-hygiene pass only.

## Changes

- Regenerated current version metadata and V1.1.10 wrapper scripts.
- Reviewed all `.md`, `.txt`, and `.ps1` files in the source package.
- Removed obsolete root-level package manifests from the prior generated package.
- Removed stale source-review inventory files that were replaced by `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.*`.
- Preserved append-only version history and historical validation material.

## Not changed

- No AFF4/APFS traversal, copy-out, decompression, staging, Store-V2 parsing, external comparison, iOS parsing, GUI review behavior, or SQLite schema behavior was intentionally changed.
