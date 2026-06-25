# AI Context - Vestigant Spotlight V1.6.77

This file must be reviewed first before every build, package, script, documentation, or code-change cycle. It is the first-fix reference for recurring project problems.

## Current baseline

- Current package being prepared: `VestigantSpotlightInv_V1_6_77.zip`.
- Current source version: `1.6.77`.
- Latest uploaded source package reviewed first for this cycle: `VestigantSpotlightInv_V1_6_74.zip`.
- Latest uploaded/runtime evidence reviewed before V1.6.77: user-provided V1.6.74 Windows build failure output in `Pasted text.txt`, uploaded V1.6.72 AFF4 result bundle, user-provided V1.6.68 AFF4 console output, user-provided V1.6.66 console output, uploaded AFF4 run bundle `0202_0024-IT003.zip`, prior `V1_6_64_build.log`, `Upload_To_Chat_V1_6_64_Results.zip`, and `Upload_Thin_iOS_CoreSpotlight_V1_6_64.zip`.
- Standing workflow: proceed with next version/build automatically after uploaded build logs or thin/result bundles unless the user explicitly says to pause.
- Standing script workflow rule from the user: every package must provide one automatic PowerShell entry point that builds first, runs the required self-test, and then runs the selected validation workflow. Keep narrower wrappers available as fallback, but the recommended command must use the most automatic wrapper.
- Standing one-click command rule from the user: every release must include a root-level PowerShell script that the user can run after downloading the source ZIP and one-click PS1 with a single copy/paste command. The script must build, run required tests, and run the current needed validation workflow by default.
- Standing command presentation rule from the user: every release response and continuation/reference document must provide all needed PowerShell commands as copy/paste-ready code blocks. Use concrete recommended paths when known. If a placeholder is unavoidable, label exactly what must be replaced.



## V1.6.77 verified input and fix guidance

- Verified uploaded V1.6.76 Windows/MSVC build completed and reported `Vestigant Spotlight v1.6.76`; AFF4 wrapper completed with `RunnerExitCode: 0`, `FullNoGuardrails: True`, `FullNativeValues: False`, and `MaxNativeRecords: 0`.
- Verified V1.6.76 AFF4 output remained uncapped in CoreFields mode: `raw_records=102,170`, `artifacts=101,326`, and `timeline_events=102,170` in `case_summary.json`.
- Verified V1.6.76 unresolved Spotlight object resolution improved path context but still left many unresolved rows: `unresolved_before=71,402`, `artifacts_path_updated=1,281`, `artifacts_name_updated=59`, and `name_scan_paths_reconstructed=1,281` in `aff4_apfs_unresolved_spotlight_object_resolution_probe_summary.json`.
- V1.6.76 resolver limitation: it still used the bounded upload-safe `aff4_apfs_spotlight_name_scan_sample.csv` as resolver input. The AFF4/APFS root-tree traversal has a larger in-memory directory-record set, but the sample CSV is capped and therefore cannot resolve all current APFS child file IDs.
- V1.6.77 first-fix guidance: write a full local APFS directory-record name index (`aff4_apfs_directory_record_name_index.csv`) during direct AFF4/APFS traversal, upload only its sample/summary, and make the unresolved object resolver prefer the full local index while falling back to the old sample when absent.
- Do not upload the full directory-record index by default; it can be large. Include `aff4_apfs_directory_record_name_index_sample.csv`, `aff4_apfs_directory_record_name_index_summary.json`, and `AFF4_APFS_DIRECTORY_RECORD_NAME_INDEX.md` in focused bundles.

## Verified/changed in V1.6.77 - current cycle

### Verified V1.6.75 Windows build and AFF4 runtime

- Evidence: uploaded `V1_6_77_build.log` compiled CLI, tests, GUI, reported `Vestigant Spotlight v1.6.75`, and the schema/iOS/APFS smoke test passed.
- Evidence: uploaded `V1_6_77_AFF4_WRAPPER_RUN_SUMMARY.txt` reported `RunnerExitCode: 0`, `FullNoGuardrails: True`, `FullNativeValues: False`, `MaxNativeRecords: 0`, and the expected AFF4 path `T:\0202_0024-IT003\disk3 2026-06-22 08-12-11\0202_0024-IT003.aff4`.
- Evidence: direct inspection of uploaded `Upload_Thin_MacOS_AFF4_V1_6_77.zip` found `case_summary.json` with `raw_record_count=102170`, `raw_key_value_count=116445`, `artifact_count=101326`, `timeline_event_count=102170`, and `orphaned_or_deleted_candidate_count=0`.
- Evidence: direct inspection of `aff4_apfs_unresolved_spotlight_object_resolution_probe_summary.json` found `unresolved_before=71402`, `direct_candidate_rows=1340`, `parent_candidate_rows=26764`, `artifacts_name_updated=1340`, and `artifacts_path_updated=0`.
- Evidence: direct inspection of `aff4_apfs_staged_storev2_artifacts_sample.csv` found the V1.6.75 resolver reduced unresolved rows in the 5,000-row sample from the earlier V1.6.72 count of 3,940 to 3,526, but still did not provide full APFS paths for the resolved names.

### APFS name-scan parent-chain path reconstruction added

- Issue: V1.6.75 used APFS `child_file_id_candidate` matches to name 1,340 unresolved objects but did not reconstruct full paths because it only accepted full paths from `aff4_apfs_logical_directory_walk.csv`.
- Fix in V1.6.77: unresolved object resolution now reconstructs candidate APFS paths from `aff4_apfs_spotlight_name_scan_sample.csv` by chaining `child_file_id_candidate -> parent_object_id_candidate -> decoded_name`, with cycle guards and Data-volume preference.
- Fix in V1.6.77: direct child-file-ID matches may now update `best_path`, `spotlight_display_path`, and `normalized_mac_path` from either the logical directory walk or a reconstructed APFS name-scan parent chain.
- New summary field: `name_scan_paths_reconstructed`.
- Evidence from dry analysis of the uploaded V1.6.75 result: 1,281 of the 1,340 direct child-file-ID matches had reconstructable APFS name-scan paths in the uploaded diagnostic CSVs, so V1.6.77 should materially improve `artifacts_path_updated` if the same APFS name-scan data is generated at runtime.
- Forensic caution: reconstructed name-scan paths are candidate current paths; they should be reviewed with APFS provenance and not treated as proof of deletion or proof that a file existed at that path for every Spotlight timestamp.
- First-fix guidance: after the next AFF4 run, inspect `aff4_apfs_unresolved_spotlight_object_resolution_probe_summary.json`. If `artifacts_path_updated` remains 0, check whether `name_scan_paths_reconstructed` is 0 and verify that the APFS name scan still includes `child_file_id_candidate`, `parent_object_id_candidate`, and `decoded_name`.

### Explicit single-AFF4 duplicate full-read reduction added

- Issue: earlier AFF4 logs showed explicit AFF4 full/no-guardrails runs streamed the full 74,468,278,910-byte AFF4 for generic source signature scanning, then streamed it again for the evidentiary SHA256 hash.
- Fix in V1.6.77: for explicit single-AFF4 runs, the generic pre-stage source signature scan is bounded even when full/no-guardrails parsing is enabled. The evidentiary SHA256 hash remains enabled, and exact AFF4 ZIP/APFS direct-map scans still run later.
- First-fix guidance: if V1.6.77+ logs show `source_probe_signature_complete bytes_scanned=74468278910` in an explicit single-AFF4 run, inspect `sourceProbeFullScan` and `aff4BoundedGenericSignatureScan` in `src/app/app_runner.cpp`.



## Verified/changed in V1.6.77

### V1.6.74 Windows/MSVC GUI result-handler build failure fixed

- Evidence: user-provided V1.6.74 Windows build output failed in `src\gui\win32_gui.cpp(3997)` with `error C2039: 'ok': is not a member of 'std::unique_ptr'`, `error C2039: 'message': is not a member of 'std::unique_ptr'`, and related `operator ->` errors.
- Root cause: the V1.6.74 `WM_EXPORT_DB_CSV_RESULT` handler expected an undefined/stale `ExportResultPayload` object, while `postExportResult()` actually posts a `std::wstring*` plus success/failure in `WPARAM`, matching the other export-result handlers.
- Fix in V1.6.77: changed `WM_EXPORT_DB_CSV_RESULT` to consume `std::wstring*` exactly like the existing page/filter/visible/checked/tagged export handlers and to use `WPARAM` only for fallback status if no message pointer is supplied.
- Additional hardening in V1.6.77: searched source for `std::numeric_limits<...>::max()` and `min()` calls and wrapped them as `(std::numeric_limits<...>::max)()` / `(std::numeric_limits<...>::min)()` where present to reduce repeat MSVC `min`/`max` macro expansion failures.
- Verification: Linux CMake build completed after one timeout/resume, CLI reported `Vestigant Spotlight v1.6.77`, Linux self-test passed, and static audit confirmed no remaining `ExportResultPayload` token and no raw `std::numeric_limits<...>::max()`/`min()` tokens in `src`. Windows/MSVC build for V1.6.77 remains unverified until the user uploads the V1.6.77 build log.
- First-fix guidance: if a future GUI export handler fails on MSVC with `std::unique_ptr`/member errors, inspect the matching `PostMessageW` payload type and the result handler together. Do not invent a payload struct unless the poster actually allocates that same type.

### V1.6.73/V1.6.74 Windows/MSVC numeric_limits max macro build failure fixed

- Evidence: user-provided V1.6.73 Windows build output failed in `src\gui\gui_export_worker.cpp(542)` with MSVC macro expansion errors around `std::numeric_limits<std::size_t>::max()`: warnings `C4003` and errors `C2589`, `C2059`, and `C2143`.
- Root cause: Windows headers can expose a function-like `max` macro; the V1.6.73 GUI export worker used `std::numeric_limits<std::size_t>::max()` in a way MSVC preprocessor treated as a macro invocation.
- Fix in V1.6.77: changed the expression to `(std::numeric_limits<std::size_t>::max)()` in `src/gui/gui_export_worker.cpp`.
- Verification: Linux CMake build completed, CLI reported `Vestigant Spotlight v1.6.77`, and Linux self-test passed. Windows/MSVC build for V1.6.77 is not verified until the user uploads the V1.6.77 build log.
- First-fix guidance: if MSVC reports `not enough arguments for function-like macro invocation 'max'` or `illegal token on right side of '::'`, search for `::max()`/`::min()` style calls and wrap the function name as `(std::...::max)()` or avoid the macro-sensitive token.

### AFF4 unresolved Spotlight object name/path resolution probe added

- Evidence from uploaded V1.6.72 result review: many artifacts had `path_status=UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL`; the 5,000-row artifacts sample included 3,940 unresolved labels.
- Evidence from V1.6.72 APFS diagnostics: `aff4_apfs_spotlight_name_scan_sample.csv` contains APFS directory-record rows with `decoded_name`, `parent_object_id_candidate`, and `child_file_id_candidate`; sample review showed some unresolved Store-V2 object IDs can match APFS `child_file_id_candidate` rows.
- Fix in V1.6.77: added `runAff4ApfsUnresolvedObjectResolutionProbe` after staged Store-V2 enrichment. It compares unresolved Store-V2 artifact `inode_num` and `parent_inode_num` against APFS name-scan child/parent file IDs, writes `aff4_apfs_unresolved_spotlight_object_resolution_probe.csv`, writes `aff4_apfs_unresolved_spotlight_object_resolution_probe_summary.json`, and writes `AFF4_APFS_UNRESOLVED_SPOTLIGHT_OBJECT_RESOLUTION_PROBE.md`.
- Fix in V1.6.77: direct APFS child-file-ID matches can update unresolved artifact labels to APFS-derived names; full paths are applied only when a matching child ID also has an APFS logical-directory path. Parent-only APFS context is exported for review but is not applied as a current path.
- Fix in V1.6.77: upload packaging includes the new unresolved-object resolution outputs plus `aff4_apfs_staged_storev2_unresolved_after_resolution_sample.csv`.
- Forensic caution: APFS name resolution is a candidate/provenance aid. Do not treat parent-only context or missing APFS matches as deletion/orphan proof.
- First-fix guidance: after V1.6.77 AFF4 runs, inspect `aff4_apfs_unresolved_spotlight_object_resolution_probe_summary.json` first, then compare `direct_candidate_rows`, `artifacts_name_updated`, `artifacts_path_updated`, and remaining unresolved rows in `aff4_apfs_staged_storev2_unresolved_after_resolution_sample.csv`.

## Verified/changed in V1.6.77 from V1.6.72 uploads

### Verified V1.6.72 AFF4 uncapped CoreFields runtime milestone

- Evidence: uploaded `V1_6_72_AFF4_WRAPPER_RUN_SUMMARY.txt` reported `RunnerExitCode: 0`, `FullNoGuardrails: True`, `FullNativeValues: False`, and `MaxNativeRecords: 0` for `T:\0202_0024-IT003\disk3 2026-06-22 08-12-11\0202_0024-IT003.aff4`.
- Evidence: direct inspection of uploaded `Upload_Thin_MacOS_AFF4_V1_6_72.zip` found `aff4_apfs_staged_storev2_parser_probe_summary.json` with `parse_probe_status=PARSE_PROBE_COMPLETED`, `selected_databases=6`, `max_records_used=0`, `native_decode_mode=CoreFields`, `stores_seen=6`, `valid_stores=6`, `metadata_blocks=3821`, `decompressed_blocks=3821`, `parsed_items=102170`, `raw_records=102170`, `raw_key_values=116445`, `raw_date_candidates=102170`, `failures=0`, and `fallback_header_only_items=0`.
- Evidence: direct inspection of `case_summary.json` in the same upload found `store_count=8`, `valid_store_count=6`, `database_candidate_count=16`, `valid_database_candidate_count=12`, `parser_selected_database_count=6`, `artifact_count=101326`, `timeline_event_count=102170`, and `orphaned_or_deleted_candidate_count=0`.
- Evidence: direct inspection of APFS stage summaries found `aff4_apfs_logical_directory_walk_summary.json` with `indexed_inode_records=725298`, `indexed_file_extent_records=770801`, `materialized_target_inode_rows=13262`, `materialized_target_file_extent_rows=20281`, and `copy_out_rows=8429`; `aff4_apfs_spotlight_file_copy_out_summary.json` reported `copied_files=8389` and `total_copied_bytes=1072069036`.
- Verified status: V1.6.72 fixed the prior max-records pass-through problem. `MaxNativeRecords=0` now reaches parser summary as `max_records_used=0` and parsed beyond the prior 25,000-record cap.
- First-fix guidance: if future no-cap AFF4 runs stop at exactly 25,000 records, inspect the wrapper summary, `tools/Run-SingleAff4SourceProbeAndZip.ps1`, CLI arguments, and `app_runner.cpp` explicit/implicit max-native-record handling before changing parser logic.

### AFF4 unresolved Spotlight-object path linkage remains incomplete

- Evidence: in the uploaded V1.6.72 AFF4 result, `aff4_apfs_staged_storev2_artifacts_sample.csv` had 5000 sample rows; 3940 had `path_status=UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL` and unresolved labels such as `UNRESOLVED_SPOTLIGHT_OBJECT_INODE_<id>`.
- Evidence: `aff4_apfs_staged_storev2_path_reconstruction_metrics_sample.csv` reported `parent_inode_links=83592`, `parent_inode_links_matched=72948`, `parent_inode_links_with_path_context_candidate=16377`, `parent_inode_links_with_existing_path_context=16377`, and `parent_inode_links_with_new_reconstructed_path=0`.
- Evidence: sample-level direct comparison of Store-V2 artifact `inode_num` to APFS logical-directory `child_file_id` produced 0 matches in the 5000-row artifact sample. Direct comparison of artifact `parent_inode_num` to APFS logical-directory `parent_object_id` produced 4 parent-context matches, all to `parent_object_id=2` Spotlight-root context.
- Interpretation: do not assume Store-V2 object identifiers are always directly equal to current APFS file IDs. The APFS volume is available, but the current mapping bridge is incomplete and should be handled as a diagnostic/probabilistic path-resolution problem until a direct mapping is verified.
- First-fix guidance: next object-linkage work should add an explicit unresolved-object resolution diagnostic/export comparing Store-V2 artifact object IDs, parent object IDs, raw/native path fields, APFS logical-directory walk child/parent IDs, copy-out target rows, and FullValues-derived fields if enabled. Do not mark unresolved objects as deleted/orphaned solely from missing direct APFS ID matches.

### GUI CSV export and Tags / Notes cleanup

- User request after V1.6.72: restore the prior MacOS and iOS investigation-tab workflow to export all SQLite case database records to CSV with configurable row chunks; default chunk size must be 500,000 rows per CSV.
- Fix in V1.6.77: added `GuiExportWorker::exportCaseDatabaseTablesChunked`, an `Export DB CSV` button, and a `CSV rows/file` edit box on the investigation review controls.
- User request after V1.6.72: clean up the poorly laid out Tags / Notes tab shown in the uploaded screenshot.
- Fix in V1.6.77: adjusted Tags / Notes tab labels, button grouping, spacing, note/status/table placement, and resize layout to reduce clipping and improve usable space.
- First-fix guidance: if Windows GUI build fails, inspect `src/gui/gui_export_worker.cpp`, `src/gui/gui_export_worker.h`, and `src/gui/win32_gui.cpp` first. If runtime export fails, inspect the selected output folder, `CASE_SQLITE_TABLE_EXPORT_MANIFEST.csv`, and whether the case DB is open read-only by another process.

### Duplicate full-AFF4 read optimization partially implemented in V1.6.77

- Evidence from prior AFF4 logs: full source signature scan and original-container SHA256 hashing each streamed the full 74,468,278,910-byte AFF4.
- Fix in V1.6.77: explicit single-AFF4 workflows keep the generic source-signature probe bounded so the source is not fully streamed once for generic string hints and again for SHA256 hashing.
- Not changed in V1.6.77: SHA256 hashing remains a full evidentiary read unless skipped/deferred or externally supplied. This preserves provenance; a future enhancement could combine hash and generic signature scanning in a single stream if full generic scanning is re-enabled.

## Newly changed in V1.6.77

### AFF4 V1.6.70 stalled FullValues parse review and bounded CoreFields default

- Evidence: uploaded V1.6.70 `run_progress.tsv` reached `aff4_direct_map_reader_probe_complete` with `map_entries_scanned=1; chunks_decoded=14; apfs_hits=52`, then reached `aff4_apfs_staged_storev2_parse_start` with `selected_stores=6 decode_mode=FullValues`.
- Evidence: uploaded `VestigantSpotlight.log` reported `Run started. app_version=1.6.70`, input `T:/0202_0024-IT003/disk3 2026-06-22 08-12-11/0202_0024-IT003.aff4`, source scan size `74468278910`, SHA256 prefix `76668ee0bfbc`, and Store-V2 parser selection `valid_candidates=12 selected_primary_databases=6`.
- Evidence: uploaded `aff4_apfs_staged_storev2_parse_progress.tsv` showed store 2/6 had `blocks=2598`, reached `parsed_items=20000`, `raw_key_values=881055`, `raw_date_candidates=183222`, and the case DB was already `1270317056` bytes.
- Evidence: uploaded `VestigantSpotlight.log` later reported `Native parser record diagnostic limit reached. parsed_items=25000 limit=25000`, but the uploaded `last_stage.txt` and `run_status.txt` remained at `aff4_apfs_staged_storev2_parse_start`; no parse-complete/enrichment/upload-complete stage was present in the uploaded stalled snapshot.
- Interpretation: this is not the earlier zero-row AFF4 failure. Direct-map/APFS staging worked; the bottleneck is bounded FullValues Store-V2 native parsing/finalization, which can create hundreds of thousands of key/value rows quickly.
- Fix in V1.6.77: AFF4 staged Store-V2 validation defaults to bounded `CoreFields` mode unless `--experimental-full-native-values` is explicitly supplied. Wrapper scripts pass `-DecodeCoreNativeValues` by default and expose `-FullNativeValues`, `-MaxNativeRecords`, and `-MaxNativeBlocks` controls.
- Fix in V1.6.77: native parser writes `native_parse_record_limit_reached` to progress when the max-record cap is hit.
- Fix in V1.6.77: AFF4 wrapper heartbeat tails `aff4_apfs_staged_storev2_parse_progress.tsv` so parser progress remains visible while the main run status is still at the stage-level parse marker.
- First-fix guidance: if a future AFF4 run reaches Store-V2 parse but appears stalled, inspect `aff4_apfs_staged_storev2_parse_progress.tsv` first, then compare `raw_key_values`, `raw_date_candidates`, `case_db_mb`, and whether `native_parse_record_limit_reached` or `native_parse_complete` appears. Do not change direct-map/APFS copy-out code unless the run fails before staged Store-V2 discovery.

### One-click PowerShell command rule reaffirmed

- Every release must include one root-level `Run-V<version>-AfterDownload.ps1` script and a final response section titled `Copy/paste PowerShell command` immediately after the download links.
- The command must build first, run required tests, then run the current validation workflow. V1.6.77 defers AFF4 path resolution until after build/self-test so a missing/moved drive letter does not prevent build validation from starting.
- Current AFF4 default search root is `T:\` because the user reported the evidence drive letter changed from `R:` to `T:` while the path was otherwise the same.

## Verified from uploaded V1.6.64 iOS thin evidence

### V1.6.64 Windows/MSVC build verified

- Evidence: uploaded `V1_6_64_build.log` compiled and linked `VestigantSpotlight.exe`, `VestigantSpotlightCli.exe`, and `VestigantSpotlightTests.exe`.
- Evidence: the same build log reported `Vestigant Spotlight v1.6.64` during the built binary version check.
- First-fix guidance: if a future version fails the build-wrapper version check, first inspect root `VERSION`, `VERSION.txt`, `src/core/app_info.cpp`, and versioned wrapper names before changing parser code.

### V1.6.64 iOS thin runtime verified complete

- Evidence: uploaded `Upload_To_Chat_V1_6_64_Results.zip` contained `case_root\last_stage.txt` with `complete_success`.
- Evidence: uploaded `case_root\case_info.json` reported `app_version=1.6.64`.
- Evidence: uploaded `case_root\case_summary.json` reported stable baseline counts against the last known-good V1.6.61 thin run: raw records `686917`, raw key/value rows `120843`, artifacts `686917`, usage evidence rows `228699`, timeline events `1011956`, and orphaned/deleted candidates `34914`.
- Evidence: uploaded nested `active_file_comparison_validation_checks_sample.csv` reported PASS for reconciliation, lead-only language, no unsafe deletion language, and reference lookup source population.
- Evidence: uploaded nested `ios_coreduet_interactionc_validation_checks_sample.csv` reported PASS for contextual guardrail notes, canonical `ZINTERACTIONS` reconciliation, join-table suppression, and phone-label promotion suppression.
- Evidence: uploaded chat bundle did not include the denied full raw inventory log `ios_ffs_7z_inventory_raw_slt.txt`; it included only `case_logs\ios_ffs_7z_inventory_raw_slt_summary.txt`.
- First-fix guidance: if a future iOS thin run changes the orphaned/deleted candidate count, compare `active_file_comparison_validation_checks*`, `active_file_comparison_runs*`, `orphaned_deleted_candidates`, and Missing-from-FFS reference exports before changing parser logic.

### V1.6.64 iOS cache reuse did not activate because the expected prior cache path was absent

- Evidence: uploaded `WRAPPER_RUN_SUMMARY.txt` reported `AutoReusePriorIosCacheEffective: True`, blank `ResolvedReuseIosCache`, and `ReuseCacheValidationReason: cache folder not found: Q:\SpotlightCase\TestIOS_CoreSpotlight_V1_6_61`.
- Evidence: uploaded `WRAPPER_RUN_SUMMARY.txt` reported `UseFastLocalCaseRoot: False`; the wrapper therefore looked under `Q:\SpotlightCase` instead of `D:\Downloads\SpotlightCase`.
- Evidence: uploaded `case_root\run_progress.tsv` did not contain `stage_zip_source_reuse_cache`, so focused ZIP extraction and 7z inventory parsing were not reused in this run.
- First-fix guidance: if cache reuse is the test target, run the unified wrapper with `-Workflow IOSCoreSpotlightThin -UseFastLocalCaseRoot -FastLocalRoot "D:\Downloads\SpotlightCase"` or provide an explicit `-ReuseIosCache` path. Do not change iOS parser code until `ios_reuse_cache_wrapper.log`, `WRAPPER_RUN_SUMMARY.txt`, and `source_cache_manifest.json` show why reuse was rejected.

## Verified/pending AFF4 evidence and fixes carried forward

### Uploaded AFF4 run logs: 0202_0024-IT003, V1.6.64 app version

- Evidence: user-uploaded `0202_0024-IT003.zip` contained AFF4 run logs and upload artifacts from an AFF4 source-probe run. The package includes the uploaded log ZIP at `validation/aff4_run_logs/0202_0024-IT003_V1_6_64_AFF4_run_logs_uploaded.zip` and a concise review at `validation/AFF4_RUN_REVIEW_0202_0024_IT003_V1_6_64.txt`.
- Evidence: `source_probe_summary.json` in that uploaded bundle reported `app_version=1.6.64`, input type `AFF4_CONTAINER`, `bytes_scanned=74468278910`, `signature_count=136`, and filesystem hint `APFS_NXSB_MAGIC`.
- Evidence: `aff4_zip_probe_summary.json` reported `CENTRAL_DIRECTORY_PARSED`, `zip64_used=true`, and `entries_written=6493`.
- Evidence: `aff4_zip_central_directory.csv` contained one `/map` entry, one `/idx` entry, 3244 `/data/########` entries, and 3244 `/data/########.index` entries under stream base `aff4%3A%2F%2Fed87f67b-1f52-4d61-bfa0-59e5318f4570`.
- Evidence: `case_summary.json` reported no Store-V2/database/artifact/timeline ingestion from the V1.6.64 AFF4 run: `store_count=0`, `valid_store_count=0`, `database_candidate_count=0`, `artifact_count=0`, and `timeline_event_count=0`.
- Evidence: `aff4_cpp_lite_dynamic_load_probe.csv` reported `SKIPPED_KNOWN_BLOCKING_LAYOUT` because the metadata identified a BlackBag-style APFS `DiscontiguousImage` with LZ4 `ImageStream` storage and the build guarded against the known Windows `AFF4_open` blocking path.
- Source-audit finding carried forward from V1.6.67/V1.6.69: earlier source hardcoded the direct AFF4 stream base `aff4%3A%2F%2F99930a27-3e61-419e-8b6b-65a3a40bedcb`, which did not match the uploaded AFF4's actual central-directory stream base.
- Fix carried forward: `Aff4ProbeWorker::executeDirectMapReaderProbe` selects the AFF4 stream base dynamically from central-directory entries that have both `/idx` and `/map`, preferring the candidate with the most `/data` entries.
- Logging/package fix carried forward: `tools/Create-SourceProbeUploadZip.ps1` includes `aff4_direct_map_reader_probe.csv`, `aff4_direct_map_reader_probe_summary.json`, `AFF4_DIRECT_MAP_READER_PROBE.md`, `aff4_direct_sqlite_candidate_carve.csv`, and `aff4_direct_sqlite_candidate_carve_summary.json`.
- First-fix guidance: if the next AFF4 run produces zero Store-V2/database/artifact rows, inspect `aff4_direct_map_reader_probe_summary.json` first. If it reports `IDX_READ_FAILED`, `MAP_READ_FAILED`, or `DIRECT_MAP_READER_NO_CHUNKS_DECODED`, fix direct map/index/chunk decoding before changing APFS or Store-V2 parser logic. If chunks decode and APFS hits appear, focus on APFS OMAP/root-tree traversal and Store-V2 copy-out.

### V1.6.68 AFF4 advanced ingestion then failed at upload ZIP packaging

- Evidence: user-provided V1.6.68 AFF4 console output showed the AFF4 workflow reached SQLite enrichment and wrote `AFF4 APFS staged Store-V2 enrichment probe` to `Q:/SpotlightCase/TestMacOS_AFF4_V1_6_68/aff4_apfs_staged_storev2_enrichment_probe_summary.json`.
- Evidence: the same console output reported `summary: sources=1 store_groups=8 valid_store_groups=6 database_candidates=16 valid_database_candidates=12 selected_databases=6 artifacts=24814 usage=924 timeline=228063 orphan_deleted_candidates=0`.
- Evidence: the same console output showed `complete_aff4_apfs_staged_storev2_validation_probe` with `raw_records=25000 selected_databases=6`, so the direct-map/store staging fix advanced this evidence beyond the prior V1.6.64 zero-row source-probe result.
- Evidence: after output verification, the run failed at `tools\Create-SourceProbeUploadZip.ps1` with `Cannot bind argument to parameter 'LiteralPath' because it is an empty string`, called from `tools\Run-SingleAff4SourceProbeAndZip.ps1:594`.
- Fix carried forward from V1.6.69: `Create-SourceProbeUploadZip.ps1` checks `ReaderToolsRoot` only when it is nonblank; `Run-SingleAff4SourceProbeAndZip.ps1` only splats `ReaderToolsRoot` into normal and failed upload packaging when it is nonblank.
- Fallback carried forward: the unified `BuildAndRun-V1_6_77-FromDownloadedZip.ps1` attempts an emergency AFF4 diagnostic upload bundle if the AFF4 runner exits nonzero after a case folder exists.
- Usability fix carried forward: the unified AFF4 workflow can auto-locate a matching `.aff4` when `-Aff4Path` is omitted by using `-Aff4SearchRoot` and `-Aff4NameHint`.
- First-fix guidance: if a future AFF4 run reaches `complete_aff4_apfs_staged_storev2_validation_probe` but fails after `upload_bundle_complete` or inside `Create-SourceProbeUploadZip.ps1`, treat it as a packaging/upload bug first. Review `Upload_Thin_MacOS_AFF4_V<version>_FAILED_WRAPPER_RESCUE.zip`, `run_progress.tsv`, `run_status.txt`, and `wrapper_case_path_manifest.txt` before changing parser code.

### V1.6.66 one-script self-test failure hotfix carried forward

- Evidence: user-provided V1.6.66 console output showed the Windows/MSVC build and binary version check reached `Vestigant Spotlight v1.6.66`, then `scripts\Build-V1_6_66.ps1` reported a self-test failure immediately after `VestigantSpotlightTests.exe` wrote the warning `raw_key_values is empty...` through the `& exe 2>&1 | Tee-Object` pipeline.
- Interpretation: this was a PowerShell wrapper failure path in the new mandatory self-test stage. Windows PowerShell 5.1 can surface native stderr lines as `NativeCommandError` records when `$ErrorActionPreference = "Stop"`; warnings written to stderr must be captured and logged without being treated as automatic test failure.
- Fix carried forward: `scripts\Build-V1_6_77.ps1` runs `VestigantSpotlightTests.exe` through `System.Diagnostics.Process`, captures stdout/stderr, appends both to the build log, and fails only on the process exit code. Stderr warnings are still logged as evidence.
- First-fix guidance: if a future self-test fails, inspect the actual `Self-test failed with exit code ...` line and appended self-test stdout/stderr in the build log. Do not treat a warning line alone as proof of self-test failure.

## Current unresolved validation items

### V1.6.77 runtime is not yet verified

- V1.6.77 local/package validation may confirm Linux build/static checks, but Windows/MSVC build, Windows self-test, iOS thin runtime, and AFF4 runtime are not verified until the user uploads V1.6.77 logs/bundles.
- Do not claim V1.6.77 runtime success without uploaded evidence.

### AFF4 direct/APFS production extraction remains staged

- V1.6.64 fixed GUI routing into source-probe/readiness mode, and V1.6.68 console evidence showed staged Store-V2 validation could reach selected databases/artifacts/timeline rows.
- Full AFF4/APFS production extraction is not yet verified until a complete V1.6.77+ run produces and uploads the review bundle successfully.
- A successful V1.6.77 AFF4 broad probe should produce a diagnostic/upload ZIP that shows exactly which AFF4/APFS stages ran, skipped, completed, or failed.

### iOS production-readiness remains pending

- Uploaded V1.6.64 `ios_production_readiness_summary.csv` reported source hash status as review/not production-final because the thin run did not record a production source hash.
- Final/full iOS production validation still requires forced container hashing or an externally supplied SHA256 with provenance.
- GUI investigator workflow review and final documentation that distinguishes investigative leads from conclusions remain pending.

## Do not do / release blockers

- Do not release a package unless root `VERSION`, `VERSION.txt`, `src/core/app_info.cpp`, wrapper names, case-root defaults, active docs, and scripts agree.
- Do not release without a root-level one-click `Run-V<version>-AfterDownload.ps1` script and a copy/paste command for it in the final response.
- Do not claim V1.6.77 Windows/MSVC, V1.6.77 iOS thin, or V1.6.77 AFF4 runtime success until uploaded logs prove it.
- Do not call AFF4/APFS a full production extractor unless uploaded validation proves copy-out/staging/parsing/enrichment success on the test evidence.
- Do not copy the full raw inventory logs into chat upload bundles.
- Do not treat Missing-from-FFS leads as proof of deletion.
- Keep source-type selective parsing behavior.
- Keep AFF4/raw options visible as staged/experimental, not removed.
- Keep C++ raw string literals below the MSVC risk threshold.
- Keep active Markdown consolidated to exactly these five files: `.github/pull_request_template.md`, `ai_context.md`, `docs/PROJECT_REFERENCE_V<version>.md`, `docs/START_CONTINUATION_CHAT.md`, and `third_party/lzfse/README.md`.
