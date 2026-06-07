# Vestigant Spotlight V1.3.0

V1.3.0 is a coordinated stability and architecture milestone.

## Changed

- Continued APFS buffer reuse in `aff4_probe_worker.cpp` for hot horizontal-leaf and root-tree traversal paths.
- Added APFS traversal consolidation groundwork and documentation.
- Audited GUI export thread lifecycle; no detached export-thread pattern was found in the current V1.3.0 GUI source.
- Audited GUI database access and deferred pooling pending runtime evidence.
- Updated workflow, roadmap, tracker, handoff, and new-chat continuation documentation.

## Not changed

- No new forensic interpretation labels were added.
- No iOS parser behavior changed.
- No SQLite schema changed.
- No Store-V2 parser behavior changed.

## Test scope

- AFF4/APFS: thin required.
- iOS: not required.
