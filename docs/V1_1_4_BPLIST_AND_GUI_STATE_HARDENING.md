# V1.1.4 Bplist and GUI State Hardening

V1.1.4 is a repeat-cycle hardening release based on the V1.1.3 build/thin baseline.

## Implemented

- Extended bounded CoreSpotlight bplist context metadata with offset-table validation details and the top-object relative offset where valid.
- Added checked-artifact snapshot helpers for review/export request construction.
- Replaced the ingest-start load/store gate with an atomic compare/exchange gate to reject repeated start requests before a second worker can be created.

## Not implemented

- Full NSKeyedArchiver UID graph decoding.
- Live APFS path reconstruction.
- Live APFS B-tree horizontal traversal replacement.
- Dynamic AFF4/APFS probe monolith extraction.

Those remain tracked for dedicated versions.
