# Vestigant Spotlight V1.3.2

V1.3.2 is a Group A stability release after V1.3.0 validation.

## Completed in this version

- Reused APFS next-leaf scratch buffers outside bounded OMAP/root-tree depth loops in `aff4_probe_worker.cpp`.
- Hardened Case Information tab behavior during ingest so case path/database/open/save mutations are deferred or blocked while processing owns the case database.
- Updated roadmap/tracker documentation for Groups A, B, C, D, and E.

## Not changed

- No new exfiltration/destruction conclusions or labels.
- No iOS parser changes.
- No SQLite schema changes.
- No APFS copy-out/decompression/Store-V2 parser semantic changes.

## Test scope

- AFF4/APFS: thin required.
- iOS: not required.
