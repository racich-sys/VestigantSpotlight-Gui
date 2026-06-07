## V1.1.10 update

- Current generated source package: V1.1.10.
- Base used for changes: V1.1.9.1.
- Scope: source-package documentation/script cleanup and current-version wrapper regeneration only.
- Removed only clearly obsolete active-package clutter; ambiguous historical notes/scripts were retained for user approval before any future removal.
- Source-package `.md`, `.txt`, and `.ps1` review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.md`.
- No AFF4/APFS extraction, iOS parsing, GUI behavior, or SQLite schema behavior was intentionally changed.

# Vestigant Spotlight User Manual

## V1.1.10 update

- Current generated source package: V1.1.10.
- Validated baseline reviewed before this version: V1.1.8 Windows/MSVC build and macOS AFF4/APFS thin output.
- Main change: guarded live APFS OMAP horizontal leaf traversal with bounded next-leaf transitions.
- Source-package `.md`, `.txt`, and `.ps1` file review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.md`.


Use `docs/CONSOLIDATED_USER_MANUAL.md` as the maintained user-facing manual for version 0.9.37 and later.

### V0_9_42 V1-readiness note

V0_9_42 tightens normal iOS compact-mode text storage to keep Missing From FFS text visibility without exceeding the DB guardrail on the current large iOS source. GUI review-page loads are now tracked and cancellable instead of detached.

## V1.1.8 Update

- `BaselineVersionHistory.md` is now the append-only version-history baseline in `docs/FULL_VERSION_HISTORY.md` and `VERSION_HISTORY.md`.
- Windows long-path evidence writes were added for APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction output paths.
- SQLite WAL checkpoint/truncate is requested before upload packaging.
- Logger writes are mutex-protected for concurrent GUI/export/ingest paths.
- APFS decmpfs reconstruction remains bounded; the expected-output safety cap is now 256 MiB.


## V1.1.10.1 command-block documentation hotfix

- Corrected current-version build documentation to include the full extraction/build PowerShell block requested by the user.
- Corrected current-version macOS AFF4/APFS thin test documentation to include `Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut`.
- Updated `docs/NEW_CHAT_CONTINUATION_GUIDE.md` so new chats started from the newest upload include the full commands.
- No source parser/extraction behavior changed.

### TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build, because wrappers/docs changed and APFS behavior did not.
- iOS: not required, because no iOS intake/parser/schema/view code changed.
- Trigger for full AFF4/APFS test: any future change to traversal, copy-out, decompression, staging, external compare, or Store-V2 parse behavior.
