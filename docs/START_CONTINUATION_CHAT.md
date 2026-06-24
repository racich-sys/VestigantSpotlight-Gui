# Continue the Vestigant Spotlight / Spotlight2 Project from V1.6.72

Mandatory first step for the next assistant: read `ai_context.md` first from the latest uploaded source package before making any code, script, documentation, or packaging changes.

Latest package prepared: `VestigantSpotlightInv_V1_6_72.zip`.

Latest version: `1.6.72`.

## Current focus

Validate the V1.6.72 AFF4 stalled-parse hotfix. V1.6.70 showed that the AFF4 direct-map/APFS staging path had advanced, but the run appeared stalled during bounded FullValues Store-V2 parsing/finalization.

Grounded V1.6.70 evidence:

- `run_progress.tsv` reached `aff4_direct_map_reader_probe_complete` with `map_entries_scanned=1; chunks_decoded=14; apfs_hits=52`.
- `run_progress.tsv` then reached `aff4_apfs_staged_storev2_parse_start` with `selected_stores=6 decode_mode=FullValues`.
- `aff4_apfs_staged_storev2_parse_progress.tsv` showed store 2/6 with `blocks=2598` and by 20,000 parsed items had `raw_key_values=881055`, `raw_date_candidates=183222`, and DB size `1270317056` bytes.
- `VestigantSpotlight.log` reported `Native parser record diagnostic limit reached. parsed_items=25000 limit=25000`.
- The uploaded snapshot did not show parse-complete, enrichment-complete, upload-bundle-complete, or complete validation stage.

V1.6.72 changes:

- AFF4 staged Store-V2 validation defaults to bounded CoreFields mode unless full native values are explicitly requested.
- One-click and wrapper scripts pass `-DecodeCoreNativeValues` by default and expose `-FullNativeValues`, `-MaxNativeRecords`, and `-MaxNativeBlocks`.
- Native parser progress now writes `native_parse_record_limit_reached` when the cap is hit.
- Wrapper heartbeat tails `aff4_apfs_staged_storev2_parse_progress.tsv` so parser progress is visible during the native parse stage.
- AFF4 path resolution is deferred until after build/self-test so the one-click workflow builds first.
- Current default AFF4 search root is `T:\` because the user reported the drive letter changed from `R:` to `T:`.

## Copy/paste PowerShell command

After downloading the ZIP and `Run-V1_6_72-AfterDownload.ps1` to `D:\Downloads`, run:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_72-AfterDownload.ps1
```

Expected uploads after AFF4 run:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_72.zip
D:\Downloads\V1_6_72_build.log
D:\Downloads\V1_6_72_AFF4_WRAPPER_RUN_SUMMARY.txt
```

Targeted FullValues support run only:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_72-AfterDownload.ps1 -FullNativeValues
```

## Standing rules

- Every release response must put a single copy/paste PowerShell command immediately after download links.
- Every package must include one root-level one-click PowerShell script that builds, runs required self-test, and runs the needed validation workflow.
- Do not claim Windows/MSVC, runtime, iOS, or AFF4 success without uploaded evidence.
- Keep active Markdown consolidated to exactly: `.github/pull_request_template.md`, `ai_context.md`, `docs/PROJECT_REFERENCE_V<version>.md`, `docs/START_CONTINUATION_CHAT.md`, `third_party/lzfse/README.md`.

## Latest continuation update - V1.6.72

Read `ai_context.md` first. Current package is `VestigantSpotlightInv_V1_6_72.zip`.

The latest uploaded V1.6.71 no-record-cap AFF4 run completed with `RunnerExitCode: 0` and wrapper `MaxNativeRecords: 0`, but the result still reported `max_records_used=25000` and `raw_record_count=25000`. V1.6.72 fixes the no-record-cap propagation path so explicit `0` remains uncapped at both wrapper and app levels.

Copy/paste command for the next AFF4 no-record-cap CoreFields validation run:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_72-AfterDownload.ps1
```

Upload after run:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_72.zip
D:\Downloads\V1_6_72_build.log
D:\Downloads\V1_6_72_AFF4_WRAPPER_RUN_SUMMARY.txt
```
