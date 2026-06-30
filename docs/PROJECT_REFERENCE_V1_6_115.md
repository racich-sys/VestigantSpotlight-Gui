# Project Reference - Vestigant Spotlight V1.6.115

Read this file first before every build, package, script, documentation, or code-change cycle. Do not rely on chat memory for project-critical state. If something must persist across chats, add it here and to `docs/START_CONTINUATION_CHAT.md` before packaging.

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
6. Every build/release/hotfix package and final build response must include downloadable PowerShell scripts for both build-only and the next validation/thin-test workflow, the raw PowerShell code or script file contents when requested, and exact copy/paste PowerShell commands. The ZIP must include root-level copies of the primary run script, the build-only script, and a `POWERSHELL_COMMANDS_V<version>.txt` command file, in addition to any copies under `scripts/`.
7. `docs/START_CONTINUATION_CHAT.md` must be updated last before packaging.
8. Thin/trial/pressure-test runs must skip original source-container SHA256 unless a separate explicit full-validation hash confirmation is supplied.
9. When thin results are uploaded, proceed to review and next build unless the user explicitly says pause.
10. Investigator-facing timeline/time records must be grouped by file/folder identity. Raw date candidates may remain one row per parsed date for provenance, but the primary GUI/export timeline must remain one summary row per artifact/file/folder/inode (`source_id + artifact_id`, with `source_id + inode/object_id` fallback where needed). Do not reintroduce per-date rows as the default investigator timeline.
11. Points of interest must be presented as unvalidated investigative leads. The user/application will independently validate results; scoring is triage only and must not be reported as proof.
12. GUI investigation views should show the number of available entries where safe to compute. Pagination should provide First, Previous, Next, and Last navigation when total row counts are available. The left-side investigation view selector pane should be mouse-resizable by dragging its vertical splitter.
13. Future builds must make the Windows `build-msvc\Release` folder as portable as possible. The build must stage `resources\reader_tools`, `resources\apfs_tools`, launch/check scripts, and a reader-tools manifest under `build-msvc\Release`. The app/CLI/GUI must auto-detect `<exe folder>\resources\reader_tools` before falling back to environment variables/PATH. If required third-party reader DLLs cannot be found during staging, the release must clearly mark `PORTABLE_RUNTIME_INCOMPLETE` instead of silently producing a non-portable folder.

## Database architecture roadmap

- Keep SQLite as the authoritative forensic case DB.
- Evaluate DuckDB only for optional analytical/reporting sidecars.
- Evaluate RocksDB or per-store SQLite temp DBs only for parser scratch/cache.
- Prioritize schema/index/query optimization, grouped timeline summaries, three-database sidecars, and controlled parallel parsing before considering a database-engine rewrite.
- The three-database filesystem comparison architecture is transitional: the primary case DB remains the Spotlight evidence DB, while `filesystem_inventory.sqlite` and `comparison.sqlite` sidecars support APFS inventory/comparison evidence.
- Full physical split/rename to `spotlight.sqlite`, `filesystem_inventory.sqlite`, and `comparison.sqlite` remains deferred until DB bloat and GUI compatibility are managed.

## Current package and latest evidence

- Current source/package version: `1.6.115`.
- Latest reviewed runtime evidence before this package: V1.6.110 AFF4/APFS thin upload, V1.6.110 wrapper summary, and V1.6.110 build log.
- V1.6.110 wrapper summary showed `PressureTestMode=True`, `ForceContainerHash=False`, `SkipContainerHash=True`, `FullNativeValues=True`, `MaxNativeRecords=0`, `MaxNativeBlocks=0`, and `RunnerExitCode=0`.
- V1.6.110 Windows/MSVC build log showed CLI/tests/GUI build success and the Schema/iOS/APFS smoke test passed.

## V1.6.110 verified AFF4/APFS baseline

Direct thin review preserved the expected Store-V2 counts for the current AFF4 case:

```text
raw_record_count=102170
raw_key_value_count=4225419
raw_date_candidate_count=815736
artifact_count=101326
usage_evidence_count=1092
timeline_event_count=101326
```

Store-V2 parse-selection coverage remained clean:

```text
candidate_database_groups=8
groups_with_valid_database=6
groups_selected_for_parsing=6
groups_invalid_only=2
all_valid_groups_selected=true
```

Post-cache POI/validation output was at the expected scale:

```text
high_priority_validation_queue rows = 125
compact_high_priority_validation_evidence_packet rows = 125
comparison.sqlite sidecar high-priority evidence packet rows = 3718
```

The V1.6.110 thin run confirmed the V1.6.106/V1.6.107 full-native AFF4 path remained active: parser coverage showed `raw_key_values=4225419` and `raw_date_candidates=815736`.

## Implemented in V1.6.115

- Reviewed the V1.6.111 build failure reported by the user. The Windows build reached portable runtime validation and found the required AFF4 reader/VC runtime DLLs, but the one-click build then failed during portable ZIP export because the ZIP destination did not exist yet and the export path emitted a `Resolve-Path`/`NativeCommandError`.
- Replaced the portable ZIP export implementation with `.NET` `System.IO.Compression.ZipFile.CreateFromDirectory()` instead of `Compress-Archive`, avoiding destination pre-resolution of a non-existent ZIP path.
- Wrapped portable ZIP export in the build script as a catch-and-warn post-build packaging step so the main MSVC build/self-test is not marked failed after a successful compile solely because the portable ZIP export step has a packaging issue.
- Added `tools/Verify-ProductionReadiness.ps1`, integrated it into the Windows build log, and copied `production_readiness_status.txt` into thin upload bundles when available.
- Added a mouse-draggable vertical splitter to the macOS Investigation View so the left-side view selector panel can be widened or narrowed.
- Preserved portable reader/runtime bundling under `resources\reader_tools`, including the uploaded reader DLLs and VC runtime DLLs.
- Root and scripts copies of `Run-V1_6_115-AfterDownload.ps1`, `Build-V1_6_115-FromDownloadedZip.ps1`, `Launch-V1_6_115-GUI.ps1`, `Export-PortableReleaseZip_V1_6_115.ps1`, and `POWERSHELL_COMMANDS_V1_6_115.txt` are included.

## Known remaining items

- Windows/MSVC V1.6.115 build and real AFF4/APFS runtime are unverified until the user runs the provided PowerShell workflow and uploads results.
- The portable runtime can only be marked `PORTABLE_RUNTIME_READY` if the build machine has redistributable reader DLLs available through `-ReaderToolsRoot`, `VESTIGANT_READER_TOOLS`, `VESTIGANT_AFF4_CPP_LITE_ROOT`, known reader-tool locations, or PATH. If those files are not available at build time, staging will create the folder/scripts/manifest but mark it `PORTABLE_RUNTIME_INCOMPLETE`.
- Full GUI open/attach workflow for the three-database sidecar layout remains roadmap work.
- Full physical split/rename to `spotlight.sqlite`, `filesystem_inventory.sqlite`, and `comparison.sqlite` remains deferred.
- Controlled parallel Store-V2 parsing remains roadmap work; do not add multi-core parsing until validation and GUI workflow are stable.
- Current iOS production confidence still requires a fresh iOS thin validation run.
- Missing APFS comparison rows and points-of-interest rows are investigative leads only, not deletion proof.

## Expected V1.6.115 validation workflow

Run the AFF4/APFS thin workflow:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_115-AfterDownload.ps1
```

If reader tools are not already discoverable on the build machine, pass an explicit reader-tools root so the Release folder can be made portable:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_115-AfterDownload.ps1 -ReaderToolsRoot "T:\VestigantReaderTools\aff4-cpp-lite"
```

After build, check the copyable Release folder:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_115\build-msvc\Release\Check-PortableRuntime.ps1
```

Expected upload after run:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_115.zip
D:\Downloads\V1_6_115_build.log
D:\Downloads\V1_6_115_AFF4_WRAPPER_RUN_SUMMARY.txt
```

Specific checks for the next upload:

1. Wrapper summary still shows no-hash pressure mode and `RunnerExitCode=0`.
2. Counts remain around the V1.6.110 baseline: 102170 raw records, 4225419 key/value rows, 815736 raw date candidates, 101326 artifacts, 1092 usage rows, and 101326 grouped timeline rows.
3. Store-V2 parse-selection coverage still reports `all_valid_groups_selected=true` for the current AFF4 case unless new evidence changes candidate validity.
4. High-priority queue and compact evidence packet remain around 125 rows after cache-text refresh.
5. `tools/Verify-ThinIdentifierCsvPrecision.ps1` passes during the AFF4 wrapper.
6. The Release folder contains `resources\reader_tools\reader_tools_manifest.csv`, `Check-PortableRuntime.ps1`, `Launch-VestigantSpotlight-GUI.ps1`, and `Run-AFF4Probe-Portable.ps1`.
7. `resources\portable_runtime_status.txt` reports either `PORTABLE_RUNTIME_READY` or clearly documents missing required runtime files.
8. GUI investigation view list remains responsive and pagination exposes First/Previous/Next/Last controls.
## V1.6.115 VC runtime update

- Bundled the uploaded Microsoft VC++ runtime DLLs under `resources\reader_tools`:
  - `MSVCP140.dll`
  - `VCRUNTIME140.dll`
  - `VCRUNTIME140_1.dll`
- Portable staging now treats those three VC runtime DLLs as required files for a copyable `build-msvc\Release` folder.
- `Check-PortableRuntime.ps1` and `tools/Verify-PortableReleaseLayout.ps1` now fail if those VC runtime DLLs are missing from `build-msvc\Release\resources\reader_tools`.
- Redistribution/licensing of third-party and Microsoft runtime files remains unverified; treat this as internal/test packaging unless licenses are reviewed.



## V1.6.115 final implementation note

This package fixes the V1.6.112 production-readiness PowerShell inline-if failure, adds Spotlight-only external-volume evidence review tables/CSV/GUI views, and adds iOS App DB Spotlight eligibility/schema/row candidate review views for CoreSpotlight viability work. All new external-volume and iOS app DB Spotlight rows are investigative leads only.
