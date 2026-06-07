# V1.1.6.1 AFF4 Worker Windows Build Hotfix

V1.1.6 physically moved `writeAff4DirectMapReaderProbe(...)` into `src/parsers/aff4_probe_worker.cpp`. The Linux validation passed because the affected code path was under `_WIN32`, but MSVC correctly reported that `wideProcessPath(...)` was not available in the new translation unit.

V1.1.6.1 adds a local Windows-only path widening helper to the AFF4 probe worker. It mirrors the UTF-8/ACP fallback behavior used by the app runner and keeps the helper translation-unit local to avoid new exported symbols.

No APFS traversal, AFF4 read, Store-V2 parser, iOS parser, SQLite schema, copy-out, or GUI behavior was changed.
