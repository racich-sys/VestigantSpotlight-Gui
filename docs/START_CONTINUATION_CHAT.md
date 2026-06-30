# Start Continuation Chat - Vestigant Spotlight V1.6.115

Use this to continue the project seamlessly in a new chat.

## Current state

Current package: `VestigantSpotlightInv_V1_6_115.zip`.

V1.6.115 was prepared after reviewing the V1.6.110 Windows/MSVC build log, V1.6.110 AFF4 wrapper summary, and V1.6.110 thin AFF4/APFS upload. V1.6.110 completed the AFF4 workflow with `RunnerExitCode=0`, `PressureTestMode=True`, `SkipContainerHash=True`, `FullNativeValues=True`, and no native record/block caps.

## Standing instructions

1. Read `ai_context.md` first.
2. Treat latest uploaded source ZIP as source of truth.
3. Verify every claimed issue against uploaded logs/thin results/source/tool output before changing code.
4. Do not build again until the user uploads the next thin result or explicitly tells you to build.
5. When a thin result is uploaded, review and proceed unless the user explicitly says pause.
6. Keep exactly five active Markdown files in the package.
7. Update `ai_context.md` whenever something needs to be remembered; do not rely on chat memory.
8. Keep SQLite as the authoritative forensic case DB. DuckDB is only a possible reporting sidecar. RocksDB or per-store SQLite temp DBs are only possible parser scratch/cache options.
9. Investigator timeline rows must remain grouped by file/folder/inode. Raw date candidates remain the drilldown/provenance table.
10. Points of interest are unvalidated investigative leads. The user/application independently validates results.
11. Every build/release/hotfix package and response must provide downloadable PowerShell scripts for build-only and the next validation/thin-test workflow, the script code or contents when requested, and exact copy/paste PowerShell commands. Include root-level copies of the primary run script, build-only script, launch script, and `POWERSHELL_COMMANDS_V<version>.txt` in the ZIP.
12. GUI investigation views should show available row counts where safe. Pagination should include First, Previous, Next, and Last when total rows are known. The left-side investigation view selector pane should be mouse-resizable by dragging its vertical splitter.
13. The Windows `build-msvc\Release` folder should be staged as a portable runtime folder whenever possible. Required reader DLLs must be copied into `resources\reader_tools` or the release must explicitly state `PORTABLE_RUNTIME_INCOMPLETE`.

## V1.6.110 thin review baseline

The V1.6.110 thin run completed successfully and preserved the expected AFF4/APFS Store-V2 baseline:

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

Post-cache validation outputs matched the expected scale:

```text
high_priority_validation_queue rows = 125
compact_high_priority_validation_evidence_packet rows = 125
comparison.sqlite sidecar high-priority evidence packet rows = 3718
```

The V1.6.110 thin review confirmed bundled reader-tool discovery in the Windows Release path and preserved the full-native AFF4 path: parser coverage showed `raw_key_values=4225419` and `raw_date_candidates=815736`.

## V1.6.115 changes

- Fixed the V1.6.113 Windows build-gate failure reported by the user: `src\db\case_db.cpp` contained a new raw SQL string above the 5,000-character MSVC audit limit. The Spotlight external-volume/iOS Spotlight SQL additions were split into smaller raw strings without changing their intended schema/view behavior.
- Fixed the V1.6.112 Windows build/run failure reported by the user. The build and portable runtime checks had completed, but `tools\Verify-ProductionReadiness.ps1` used an inline `$((if (...)))` expression that Windows PowerShell attempted to execute as command text. V1.6.115 computes that status in a separate `$status = if (...) { ... } else { ... }` assignment.
- Hardened `scripts\Build-V1_6_115.ps1` so portable-layout and production-readiness audit output is captured as text under a temporary `Continue` error-action scope.
- Added Spotlight-only external-volume evidence review, limited to Spotlight-derived tables.
- Added new thin/AFF4 upload CSVs: `spotlight_external_volume_candidate_summary.csv`, `spotlight_external_volume_evidence_review.csv`, `spotlight_external_volume_raw_value_hits.csv`, `spotlight_external_volume_cache_text_hits.csv`, and `spotlight_external_volume_dictionary_hits.csv`.
- Added GUI views: `Spotlight External Volume Evidence Review` and `Spotlight External Volume Summary`.
- Added iOS App DB Spotlight viability scaffolding and GUI views: `iOS - App Spotlight Eligibility Summary`, `iOS - App Spotlight Schema Hits`, and `iOS - App Spotlight Row Candidates`.
- Preserved bundled AFF4 reader tools, VC runtime DLLs, portable release staging/check scripts, GUI launch script, build-only script, and next-run PowerShell commands.

## Validation completed before packaging

- Linux CMake build passed.
- CLI version returned `Vestigant Spotlight v1.6.115`.
- Self-test passed.
- Static package audit passed: exactly five active Markdown files and zero C++ raw string literals above 5,000 characters.
- ZIP integrity test passed.

Windows/MSVC V1.6.115 build, portable Release staging on Windows, portable Release ZIP export, and real AFF4/APFS V1.6.115 runtime are not verified until the user runs the provided workflow and uploads results.

## Expected next run

Run:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_115-AfterDownload.ps1
```

Build only:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_115-AfterDownload.ps1 -Workflow BuildOnly
```

After build, check the copyable Release folder:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_115\build-msvc\Release\Check-PortableRuntime.ps1
```

Expected portable Release ZIP:

```text
D:\Downloads\VestigantSpotlightPortable_V1_6_115.zip
D:\Downloads\VestigantSpotlightPortable_V1_6_115.zip.sha256.txt
```

Expected upload after AFF4 run:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_115.zip
D:\Downloads\V1_6_115_build.log
D:\Downloads\V1_6_115_AFF4_WRAPPER_RUN_SUMMARY.txt
```

Also upload these if testing portability:

```text
D:\Downloads\VestigantSpotlightPortable_V1_6_115.zip.sha256.txt
T:\VestigantSpotlightInv_V1_6_115\build-msvc\Release\resources\portable_runtime_status.txt
T:\VestigantSpotlightInv_V1_6_115\build-msvc\Release\resources\reader_tools\reader_tools_manifest.csv
```

## Things to check in the V1.6.115 upload

1. Wrapper summary still shows no-hash pressure mode and `RunnerExitCode=0`.
2. Counts remain around 102170 raw records, 4225419 key/value rows, 815736 raw date candidates, 101326 artifacts, 1092 usage rows, and 101326 grouped timeline rows.
3. `aff4_apfs_staged_storev2_parse_selection_coverage.csv` is present and all valid groups are selected for the current AFF4 case.
4. The high-priority queue and compact evidence packet remain around 125 rows after cache-text refresh.
5. No identifier scientific-notation regression appears in known forensic identifier columns.
6. `build-msvc\Release\resources\portable_runtime_status.txt` reports `PORTABLE_RUNTIME_READY` or clearly documents missing runtime files.
7. `D:\Downloads\VestigantSpotlightPortable_V1_6_115.zip` exists and can be copied to a new machine for launch/runtime testing.
8. The GUI can open from the copied `build-msvc\Release` folder and auto-detect bundled reader tools.
