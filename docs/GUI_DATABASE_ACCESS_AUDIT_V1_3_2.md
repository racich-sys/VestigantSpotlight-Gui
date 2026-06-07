# GUI Database Access Audit - V1.3.2

## Finding

The GUI currently uses short-lived `ReadOnlyDb` helper instances at multiple review and detail query sites. The helper configures busy handling and PRAGMAs on each connection. V1.3.2 did not replace this with a global pooled SQLite handle because the risk of introducing lock lifetime and stale-schema problems is higher than the immediate benefit without runtime evidence of VFS exhaustion.

## Decision

Do not implement connection pooling blindly. Keep the existing busy-handler behavior and add connection reuse only after a targeted measurement shows excessive open/close churn or confirmed `SQLITE_BUSY`/VFS failures in normal review use.

## Future implementation requirements

If pooling is added later:

- Use one UI-thread-owned read handle per opened case database.
- Reopen the handle when `gOpenedCaseDb` changes.
- Protect all access through a narrow query function or mutexed scope.
- Avoid holding a read transaction across background ingest/export operations.
- Keep `ensureInvestigatorUiSchemaNoThrow(...)` on writable/open-case paths only, not on every read.
