# Vestigant Spotlight V1.6.102 Project Reference

This file must be reviewed first before every build, package, script, documentation, or code-change cycle. Do not rely on chat memory for project-critical state; if something needs to be remembered, put it in this file and in `docs/START_CONTINUATION_CHAT.md` before packaging.

## Standing project rules

1. Treat the latest uploaded source ZIP as the source of truth.
2. Verify every claimed issue against uploaded logs, thin result bundles, source files, or direct tool output before changing code.
3. Do not blindly paste proposed code snippets; adapt only after verifying current structs, option names, SQL schema, GUI layout, and wrapper flow.
4. Avoid oversized C++ raw string literals. The MSVC audit must pass with zero strings above 5,000 characters.
5. Keep active Markdown consolidated to exactly five package Markdown files:
   - `.github/pull_request_template.md`
   - `ai_context.md`
   - `docs/PROJECT_REFERENCE_V<version>.md`
   - `docs/START_CONTINUATION_CHAT.md`
   - `third_party/lzfse/README.md`
6. Every build must include root-level PowerShell scripts and exact copy/paste commands.
7. `docs/START_CONTINUATION_CHAT.md` must be updated last before packaging.
8. Thin/trial/pressure-test runs must skip original source-container SHA256 unless a separate explicit full-validation hash confirmation is supplied.
9. When thin results are uploaded, proceed to review and next build unless the user explicitly says pause.
10. Investigator-facing timeline/time records must be grouped by file/folder identity. Raw date candidates may remain one row per parsed date for provenance, but the primary GUI/export timeline must remain one summary row per artifact/file/folder/inode (`source_id + artifact_id`, with `source_id + inode/object_id` fallback where needed). Do not reintroduce per-date rows as the default investigator timeline.
11. Points of interest must be presented as unvalidated investigative leads. The user/application will independently validate results; scoring is triage only and must not be reported as proof.
12. If a new requirement or standing instruction needs to persist across chats, write it into this `ai_context.md` file and into `docs/START_CONTINUATION_CHAT.md` before packaging.

## Database architecture roadmap

- Keep SQLite as the authoritative forensic case DB.
- Evaluate DuckDB only for optional analytical/reporting sidecars.
- Evaluate RocksDB or per-store SQLite temp DBs only for parser scratch/cache.
- Prioritize schema/index/query optimization, grouped timeline summaries, three-database sidecars, and controlled parallel parsing before considering a database-engine rewrite.
- The three-database filesystem comparison architecture is in transitional implementation: the primary case DB remains the Spotlight evidence DB, while `filesystem_inventory.sqlite` and `comparison.sqlite` sidecars are materialized after AFF4/APFS enrichment.
- Full physical split/rename to `spotlight.sqlite`, `filesystem_inventory.sqlite`, and `comparison.sqlite` remains deferred until DB bloat and GUI compatibility are managed.

## Performance roadmap

- Add controlled multi-core support after current DB/query and points-of-interest work stabilizes.
- First target should be per-Store-V2 parallel parsing with a bounded worker pool and serialized/per-worker DB merge.
- Do not blindly write to the same SQLite DB from many parser threads.
- Add `--workers N` / `-Workers N`, defaulting conservatively to `min(selected_store_count, logical_cpu_count - 1)`, initially capped around 4 unless validated.
- Use SQL/index optimization for parent-inode chain and active comparison bottlenecks rather than assuming CPU parallelism fixes them.

## Current package

- Current source/package version: `1.6.102`.
- Latest reviewed runtime evidence: V1.6.99 thin upload: `Upload_Thin_MacOS_AFF4_V1_6_99.zip`, `V1_6_99_build.log`, and `V1_6_99_AFF4_WRAPPER_RUN_SUMMARY.txt`.
- V1.6.102 is a runtime hotfix for the V1.6.99 post-enrichment export bottleneck.

## V1.6.99 uploaded evidence reviewed before V1.6.102

The V1.6.99 wrapper summary reported:

- `PressureTestMode=True`
- `ForceContainerHash=False`
- `SkipContainerHash=True`
- `FullNativeValues=True`
- `MaxNativeRecords=0`
- `MaxNativeBlocks=0`
- `RunnerExitCode=0`

Direct inspection of `Upload_Thin_MacOS_AFF4_V1_6_99.zip` found:

- `raw_record_count=102170`
- `raw_key_value_count=4225419`
- `raw_date_candidate_count=815736`
- `artifact_count=101326`
- `usage_evidence_count=1092`
- `timeline_event_count=101326`
- grouped investigator timeline behavior was preserved
- active APFS image comparison completed with `image_inventory_rows=738970`, `present=39325`, `missing_candidates=62001`, and `timeline_updated=101326`
- `three_database_layout_readiness.csv` showed `filesystem_inventory.sqlite` and `comparison.sqlite` materialized
- `aff4_apfs_staged_storev2_points_of_interest_summary.csv` showed 34 `HIGH_VALIDATION_PRIORITY` rows in `MISSING_FROM_ACTIVE_INVENTORY_WITH_USAGE_DATES`
- `aff4_apfs_staged_storev2_high_priority_validation_queue.csv` contained 34 rows
- all inspected POI/validation rows remained labeled `UNVALIDATED_INVESTIGATIVE_LEAD`

## V1.6.99 runtime finding

The run completed, but the new post-enrichment export markers identified the slow step exactly:

- `aff4_apfs_sample_export_start file=aff4_apfs_staged_storev2_high_priority_validation_evidence_packet.csv` at `2026-06-27T17:09:15Z`
- `aff4_apfs_sample_export_complete file=aff4_apfs_staged_storev2_high_priority_validation_evidence_packet.csv rows=1476` at `2026-06-27T19:12:27Z`

This one thin CSV export consumed about two hours. The later sidecar materialization of `comparison.sqlite.high_priority_validation_evidence_packet` completed in about one second and produced 3718 rows, so the bottleneck was the early CSV query path, not the underlying sidecar concept.

The parent-inode fast path did work in V1.6.99:

- `enrichment_parent_inode_chain_fast_path_evaluation ... actionable_weak_artifact_rows=3`
- `enrichment_parent_inode_chain_candidates_ready ... skipped=1 reason=negligible_weak_artifact_path_targets`
- `enrichment_parent_inode_chain_complete ... skipped=1 reason=negligible_weak_artifact_path_targets`

## Implemented in V1.6.102

- Replaced the early AFF4/APFS thin CSV export of `aff4_apfs_staged_storev2_high_priority_validation_evidence_packet.csv` with a compact validation-packet index.
- The compact CSV keeps the same filename and validation role, but now writes one bounded summary row per high-priority POI lead instead of expanding row-level raw evidence through a large UNION query during the thin bundle.
- The compact row includes raw date count, missing-candidate count, cache-text count, usage-date count, and an explicit locator telling the validator to use `comparison.sqlite.high_priority_validation_evidence_packet` and the primary raw evidence tables for row-level validation.
- Full row-level high-priority validation evidence remains materialized in `comparison.sqlite.high_priority_validation_evidence_packet` after cache text processing.
- Added explicit markers:
  - `aff4_apfs_high_priority_evidence_compact_export_start`
  - `aff4_apfs_high_priority_evidence_compact_export_complete`
- Kept V1.6.99 parent-chain fast-path behavior.
- Kept SQLite as authoritative forensic case DB and retained sidecars as supporting outputs.

## Expected V1.6.102 runtime indicators

Expected count behavior for the current AFF4 case:

- `raw_record_count` around `102170`
- `raw_key_value_count` around `4225419`
- `raw_date_candidate_count` around `815736`
- `artifact_count` around `101326`
- `timeline_event_count` around `101326`
- `usage_evidence_count` around `1092`
- `comparison_candidate_count` around `62001`
- `aff4_apfs_staged_storev2_high_priority_validation_queue.csv` present with about `34` rows
- `aff4_apfs_staged_storev2_high_priority_validation_evidence_packet.csv` present with compact rows, expected about the high-priority queue count
- `comparison.sqlite` has the full `high_priority_validation_evidence_packet` sidecar table

Expected parent-inode markers:

- `enrichment_parent_inode_chain_fast_path_evaluation ... actionable_weak_artifact_rows=<N>`
- for this case, expected skip reason `negligible_weak_artifact_path_targets`

Expected post-enrichment export behavior:

- `aff4_apfs_sample_export_start file=aff4_apfs_staged_storev2_high_priority_validation_evidence_packet.csv`
- `aff4_apfs_sample_export_complete file=aff4_apfs_staged_storev2_high_priority_validation_evidence_packet.csv rows=<about 34>`
- this should complete promptly rather than taking hours

Expected no-hash behavior remains:

- `PressureTestMode=True`
- `ForceContainerHash=False`
- `SkipContainerHash=True`
- No `original_container_hash_start` marker.

## Known remaining items

- Full GUI open/attach workflow for the three-database layout remains roadmap work.
- Full physical split/rename to `spotlight.sqlite`, `filesystem_inventory.sqlite`, and `comparison.sqlite` remains deferred.
- Controlled parallel Store-V2 parsing remains roadmap work.
- Partial Store-V2 map decoding remains for `vol_4_fs_28335_parent_10287386_Data`: `dbStr-2=8770/9238` and `dbStr-5=215/294` in latest diagnostics.
- Missing APFS comparison rows and points-of-interest rows are investigative leads only, not deletion proof.
- Windows/MSVC V1.6.102 build and real AFF4/APFS runtime are unverified until the user runs the provided PowerShell workflow and uploads results.

## Validation required from user for V1.6.102

Run:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_102-AfterDownload.ps1
```

Upload:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_102.zip
D:\Downloads\V1_6_102_build.log
D:\Downloads\V1_6_102_AFF4_WRAPPER_RUN_SUMMARY.txt
```

A thin `.sha256.txt` sidecar is not expected unless a full-validation hash workflow is explicitly requested.

## V1.6.102 update

V1.6.102 refreshes POI/high-priority validation CSV outputs after Spotlight Cache text incorporation. V1.6.100 wrote the fast compact high-priority CSV before cache text rows were available, so the thin high-priority queue showed 34 rows while the later comparison sidecar showed 125 rows. V1.6.102 keeps the compact bounded CSV design but overwrites the relevant POI/high-priority CSVs after cache processing so the upload bundle reflects post-cache validation leads.

## V1.6.102 changes
- Fixed iOS SQLite text extraction to preserve embedded NUL bytes using sqlite3_column_bytes().
- GUI ReadOnlyDb now opens SQLITE_OPEN_READONLY only; explicit GUI writes use WritableGuiDb.
- Folder selection uses IFileOpenDialog instead of SHBrowseForFolderW/MAX_PATH buffer.
- Export workers remain tracked and are joined at application shutdown to avoid truncated CSV exports.
- iOS generic app DB table parsing now uses rowid keyset pagination with LIMIT/OFFSET fallback for WITHOUT ROWID tables.
- Bplist/NSKeyedArchiver JSON expansion now has a shared string budget across recursive branches.
- New-case inode/parent-inode table definitions use INTEGER affinity for the primary Spotlight tables.
