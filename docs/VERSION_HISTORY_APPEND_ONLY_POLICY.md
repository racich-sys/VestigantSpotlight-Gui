# Version History Append-Only Policy

This file is the baseline policy for Vestigant Spotlight / Spotlight2 version history.

## Rules

1. Do not replace the version history with only the latest version.
2. Do not delete V0.x, V1.0.x, or V1.1.x entries when adding a new release.
3. Every future package must keep a durable history file in the source tree.
4. New entries may be prepended for readability, but old entries must remain below.
5. If history is split across files, the top-level `VERSION_HISTORY.md` must point to the complete consolidated file.
6. If a version had a failed build, hotfix, or packaging failure, record that failure and the fix version.
7. The workflow ledger should identify:
   - current validated baseline;
   - current generated package;
   - pending validation;
   - known build/script pitfalls;
   - next safe work.
8. The suggestions/fixes tracker should link each suggestion to a version and validation status.
