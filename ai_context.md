# AI Context - Vestigant Spotlight V1.6.72

This file must be reviewed first before every build, package, script, documentation, or code-change cycle. It is the first-fix reference for recurring project problems.

## Current baseline

- Current package being prepared: `VestigantSpotlightInv_V1_6_72.zip`.
- Current source version: `1.6.72`.
- Latest uploaded source package reviewed first for this cycle: `VestigantSpotlightInv_V1_6_69.zip`.
- Latest uploaded/runtime evidence reviewed before V1.6.72: user-provided V1.6.68 AFF4 console output, user-provided V1.6.66 console output, uploaded AFF4 run bundle `0202_0024-IT003.zip`, prior `V1_6_64_build.log`, `Upload_To_Chat_V1_6_64_Results.zip`, and `Upload_Thin_iOS_CoreSpotlight_V1_6_64.zip`.
- Standing workflow: proceed with next version/build automatically after uploaded build logs or thin/result bundles unless the user explicitly says to pause.
- Standing script workflow rule from the user: every package must provide one automatic PowerShell entry point that builds first, runs the required self-test, and then runs the selected validation workflow. Keep narrower wrappers available as fallback, but the recommended command must use the most automatic wrapper.
- Standing one-click command rule from the user: every release must include a root-level PowerShell script that the user can run after downloading the source ZIP and one-click PS1 with a single copy/paste command. The script must build, run required tests, and run the current needed validation workflow by default.
- Standing command presentation rule from the user: every release response and continuation/reference document must provide all needed PowerShell commands as copy/paste-ready code blocks. Use concrete recommended paths when known. If a placeholder is unavoidable, label exactly what must be replaced.

## Newly changed in V1.6.72

### AFF4 V1.6.70 stalled FullValues parse review and bounded CoreFields default

- Evidence: uploaded V1.6.70 `run_progress.tsv` reached `aff4_direct_map_reader_probe_complete` with `map_entries_scanned=1; chunks_decoded=14; apfs_hits=52`, then reached `aff4_apfs_staged_storev2_parse_start` with `selected_stores=6 decode_mode=FullValues`.
- Evidence: uploaded `VestigantSpotlight.log` reported `Run started. app_version=1.6.70`, input `T:/0202_0024-IT003/disk3 2026-06-22 08-12-11/0202_0024-IT003.aff4`, source scan size `74468278910`, SHA256 prefix `76668ee0bfbc`, and Store-V2 parser selection `valid_candidates=12 selected_primary_databases=6`.
- Evidence: uploaded `aff4_apfs_staged_storev2_parse_progress.tsv` showed store 2/6 had `blocks=2598`, reached `parsed_items=20000`, `raw_key_values=881055`, `raw_date_candidates=183222`, and the case DB was already `1270317056` bytes.
- Evidence: uploaded `VestigantSpotlight.log` later reported `Native parser record diagnostic limit reached. parsed_items=25000 limit=25000`, but the uploaded `last_stage.txt` and `run_status.txt` remained at `aff4_apfs_staged_storev2_parse_start`; no parse-complete/enrichment/upload-complete stage was present in the uploaded stalled snapshot.
- Interpretation: this is not the earlier zero-row AFF4 failure. Direct-map/APFS staging worked; the bottleneck is bounded FullValues Store-V2 native parsing/finalization, which can create hundreds of thousands of key/value rows quickly.
- Fix in V1.6.72: AFF4 staged Store-V2 validation defaults to bounded `CoreFields` mode unless `--experimental-full-native-values` is explicitly supplied. Wrapper scripts pass `-DecodeCoreNativeValues` by default and expose `-FullNativeValues`, `-MaxNativeRecords`, and `-MaxNativeBlocks` controls.
- Fix in V1.6.72: native parser writes `native_parse_record_limit_reached` to progress when the max-record cap is hit.
- Fix in V1.6.72: AFF4 wrapper heartbeat tails `aff4_apfs_staged_storev2_parse_progress.tsv` so parser progress remains visible while the main run status is still at the stage-level parse marker.
- First-fix guidance: if a future AFF4 run reaches Store-V2 parse but appears stalled, inspect `aff4_apfs_staged_storev2_parse_progress.tsv` first, then compare `raw_key_values`, `raw_date_candidates`, `case_db_mb`, and whether `native_parse_record_limit_reached` or `native_parse_complete` appears. Do not change direct-map/APFS copy-out code unless the run fails before staged Store-V2 discovery.

### One-click PowerShell command rule reaffirmed

- Every release must include one root-level `Run-V<version>-AfterDownload.ps1` script and a final response section titled `Copy/paste PowerShell command` immediately after the download links.
- The command must build first, run required tests, then run the current validation workflow. V1.6.72 defers AFF4 path resolution until after build/self-test so a missing/moved drive letter does not prevent build validation from starting.
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
- Fallback carried forward: the unified `BuildAndRun-V1_6_72-FromDownloadedZip.ps1` attempts an emergency AFF4 diagnostic upload bundle if the AFF4 runner exits nonzero after a case folder exists.
- Usability fix carried forward: the unified AFF4 workflow can auto-locate a matching `.aff4` when `-Aff4Path` is omitted by using `-Aff4SearchRoot` and `-Aff4NameHint`.
- First-fix guidance: if a future AFF4 run reaches `complete_aff4_apfs_staged_storev2_validation_probe` but fails after `upload_bundle_complete` or inside `Create-SourceProbeUploadZip.ps1`, treat it as a packaging/upload bug first. Review `Upload_Thin_MacOS_AFF4_V<version>_FAILED_WRAPPER_RESCUE.zip`, `run_progress.tsv`, `run_status.txt`, and `wrapper_case_path_manifest.txt` before changing parser code.

### V1.6.66 one-script self-test failure hotfix carried forward

- Evidence: user-provided V1.6.66 console output showed the Windows/MSVC build and binary version check reached `Vestigant Spotlight v1.6.66`, then `scripts\Build-V1_6_66.ps1` reported a self-test failure immediately after `VestigantSpotlightTests.exe` wrote the warning `raw_key_values is empty...` through the `& exe 2>&1 | Tee-Object` pipeline.
- Interpretation: this was a PowerShell wrapper failure path in the new mandatory self-test stage. Windows PowerShell 5.1 can surface native stderr lines as `NativeCommandError` records when `$ErrorActionPreference = "Stop"`; warnings written to stderr must be captured and logged without being treated as automatic test failure.
- Fix carried forward: `scripts\Build-V1_6_72.ps1` runs `VestigantSpotlightTests.exe` through `System.Diagnostics.Process`, captures stdout/stderr, appends both to the build log, and fails only on the process exit code. Stderr warnings are still logged as evidence.
- First-fix guidance: if a future self-test fails, inspect the actual `Self-test failed with exit code ...` line and appended self-test stdout/stderr in the build log. Do not treat a warning line alone as proof of self-test failure.

## Current unresolved validation items

### V1.6.72 runtime is not yet verified

- V1.6.72 local/package validation may confirm Linux build/static checks, but Windows/MSVC build, Windows self-test, iOS thin runtime, and AFF4 runtime are not verified until the user uploads V1.6.72 logs/bundles.
- Do not claim V1.6.72 runtime success without uploaded evidence.

### AFF4 direct/APFS production extraction remains staged

- V1.6.64 fixed GUI routing into source-probe/readiness mode, and V1.6.68 console evidence showed staged Store-V2 validation could reach selected databases/artifacts/timeline rows.
- Full AFF4/APFS production extraction is not yet verified until a complete V1.6.72+ run produces and uploads the review bundle successfully.
- A successful V1.6.72 AFF4 broad probe should produce a diagnostic/upload ZIP that shows exactly which AFF4/APFS stages ran, skipped, completed, or failed.

### iOS production-readiness remains pending

- Uploaded V1.6.64 `ios_production_readiness_summary.csv` reported source hash status as review/not production-final because the thin run did not record a production source hash.
- Final/full iOS production validation still requires forced container hashing or an externally supplied SHA256 with provenance.
- GUI investigator workflow review and final documentation that distinguishes investigative leads from conclusions remain pending.

## Do not do / release blockers

- Do not release a package unless root `VERSION`, `VERSION.txt`, `src/core/app_info.cpp`, wrapper names, case-root defaults, active docs, and scripts agree.
- Do not release without a root-level one-click `Run-V<version>-AfterDownload.ps1` script and a copy/paste command for it in the final response.
- Do not claim V1.6.72 Windows/MSVC, V1.6.72 iOS thin, or V1.6.72 AFF4 runtime success until uploaded logs prove it.
- Do not call AFF4/APFS a full production extractor unless uploaded validation proves copy-out/staging/parsing/enrichment success on the test evidence.
- Do not copy the full raw inventory logs into chat upload bundles.
- Do not treat Missing-from-FFS leads as proof of deletion.
- Keep source-type selective parsing behavior.
- Keep AFF4/raw options visible as staged/experimental, not removed.
- Keep C++ raw string literals below the MSVC risk threshold.
- Keep active Markdown consolidated to exactly these five files: `.github/pull_request_template.md`, `ai_context.md`, `docs/PROJECT_REFERENCE_V<version>.md`, `docs/START_CONTINUATION_CHAT.md`, and `third_party/lzfse/README.md`.

## Newly changed in V1.6.72

### AFF4 MaxNativeRecords 0 hotfix: explicit zero now means uncapped

- Evidence: uploaded V1.6.71 wrapper summary created 2026-06-24T13:09:04-04:00 recorded `MaxNativeRecords: 0`, `FullNativeValues: False`, and `RunnerExitCode: 0`.
- Evidence: uploaded V1.6.71 result ZIP SHA256 was `9497504E999CB62DD413016007977B37187BE1CB26E93F5006644FEB56FE178C`.
- Evidence: extracted V1.6.71 result `aff4_apfs_staged_storev2_parser_probe_summary.json` still reported `max_records_used=25000`, `parsed_items=25000`, and `raw_records=25000`; `case_summary.json` also reported `raw_record_count=25000` and `timeline_event_count=25000`.
- Root cause verified in V1.6.71 source: wrapper scripts only passed `-MaxNativeRecords` when the value was greater than 0, `tools/Run-SingleAff4SourceProbeAndZip.ps1` only passed `--max-native-records` when the value was greater than 0, and `src/app/app_runner.cpp` converted `opt.maxNativeRecords == 0` to the default `25000U`.
- Fix in V1.6.72: AFF4 wrappers pass `MaxNativeRecords` when it is 0 or greater; `tools/Run-SingleAff4SourceProbeAndZip.ps1` passes `--max-native-records 0`; `src/app/app_runner.cpp` uses `opt.maxNativeRecordsExplicit` so omitted limit defaults to 25000 but explicit 0 remains uncapped.
- Fix in V1.6.72: the one-click `Run-V1_6_72-AfterDownload.ps1` defaults to `MaxNativeRecords = 0` for the current AFF4 CoreFields no-record-cap validation run.
- First-fix guidance: if a future no-cap AFF4 run still stops at 25000, inspect wrapper summary, parser summary `max_records_used`, `case_summary.raw_record_count`, and source handling around `maxNativeRecordsExplicit` before changing Store-V2 parser logic.

