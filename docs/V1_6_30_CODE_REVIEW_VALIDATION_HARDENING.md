# V1.6.38 Code Review Validation Hardening

Generated from the V1.6.29.4 build/thin evidence and the follow-on issue list covering `win32_gui.cpp`, `aff4_probe_worker.cpp`, `ios_app_db_parser.cpp`, and `sqlite_exporter.cpp`.

## Evidence reviewed

- `V1_6_29_4_build.log` showed source version `1.6.29.4`, CLI/tests/GUI link steps, binary version `Vestigant Spotlight v1.6.29.4`, and `Build complete` with no decoded compiler/linker error lines.
- `Upload_Thin_iOS_CoreSpotlight_V1_6_29_4.zip` completed with `run_status.txt` ending in `complete_success`.
- Active filesystem validation checks all passed in the V1.6.29.4 thin output.
- CoreDuet `interactionC.db` validation checks all passed in the V1.6.29.4 thin output.

## Implemented in V1.6.38

1. **Checked-artifact state synchronization**
   - `gCheckedArtifactIds` reads and writes now route through locked helper functions or explicit `gReviewStateMutex` critical sections.
   - Bulk-load now builds a temporary set and swaps it under lock.

2. **Review-page background query handoff**
   - `loadReviewPage()` no longer blocks the UI thread by joining a still-running prior review query.
   - The prior request sequence is invalidated and the stale worker is detached; stale workers continue to use the existing cancellation/progress guard.

3. **AFF4 LZ4 bounds hardening retained**
   - The V1.6.29 subtraction-based literal/match checks and 64 MB output cap are retained.

4. **Export worker handle accumulation reduced**
   - Export workers are detached on registration instead of being accumulated indefinitely in `gExportThreads` until application shutdown.
   - This keeps the UI behavior asynchronous while avoiding a long-session thread-handle vector buildup.

5. **CSV embedded-NUL preservation**
   - SQL export now passes `sqlite3_column_bytes()` to the CSV field writer.
   - The CSV writer iterates by byte length, not null-terminated C strings.
   - Embedded NULs and other control bytes are preserved as visible forensic placeholders such as `[NUL]` and `[0xNN]`.

6. **APFS NXSB block-size rejection before allocation/use**
   - `parseApfsNxSuperblock()` now rejects non-power-of-two or out-of-range block sizes before any APFS traversal uses the value for reads, reserves, or object-offset math.
   - Rejected block sizes set `found=false` and `blockSize=0` with an explicit validation status and note.

## Not changed

- No fuzzy filesystem matching was added.
- Missing-from-FFS rows remain investigative leads only, not deletion proof.
- CoreDuet `interactionC.db` rows remain contextual evidence only.
- AFF4/APFS image-backed active filesystem comparison remains pending beyond the current iOS FFS exact/reference validation workflow.
