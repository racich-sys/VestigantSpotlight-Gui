# V1.0.28.1 APFS Diagnostic Exporter Relocation

## Scope

V1.0.28.1 moves the main APFS/AFF4 diagnostic/report writer bodies out of `src/app/app_runner.cpp` and into `src/parsers/apfs_diagnostic_exporter.cpp`.

Moved writer families include:

- container superblock / checkpoint descriptor outputs
- volume superblock outputs
- resolved volume outputs
- volume root-tree lookup outputs
- root-tree node and traversal probe outputs
- filesystem namespace seed outputs
- Spotlight target/inode/xattr/file-extent/file-copy-out outputs
- Store-V2 copy-out versus staged-candidate comparison outputs
- extracted Store-V2 stage outputs
- checkpoint-map outputs

## Non-goals

V1.0.28.1 does not change:

- AFF4 dynamic reads
- APFS traversal
- APFS copy-out/staging decisions
- Store-V2 parser behavior
- iOS parsing
- SQLite schema
- GUI behavior

## Remaining work

The exact-file-signature scan and V1 diagnostic rerun-plan writers still remain in `app_runner.cpp`. They should be moved only after V1.0.28.1 builds and the macOS AFF4/APFS thin output validates.
