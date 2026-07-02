# Project Reference - Vestigant Spotlight V1.6.119

Read this file before every build, package, script, documentation, or code-change cycle. If something must persist across chats, add it here and to `docs/START_CONTINUATION_CHAT.md` before packaging.

## Current verified baseline

V1.6.118 is the latest reviewed macOS AFF4/APFS run. It completed with `RunnerExitCode=0`, `PressureTestMode=True`, `SkipContainerHash=True`, `FullNativeValues=True`, `MaxNativeRecords=0`, and `MaxNativeBlocks=0`.

V1.6.118 preserved the established AFF4/APFS full-native baseline for the 0202_0024-IT003 image:

```text
raw_record_count=102170
raw_key_value_count=4225419
raw_date_candidate_count=815736
artifact_count=101326
usage_evidence_count=1092
timeline_event_count=101326
```

The V1.6.118 build log showed Windows/MSVC compile success, required self-test success, portable staging `PORTABLE_RUNTIME_READY`, production readiness check passed, required runtime files present, portable runtime check passed, and portable ZIP creation.

The V1.6.118 Spotlight-only external-volume outputs were present and empty for the tested image, which is correct for this image after excluding ordinary internal `/System/Volumes/Data/...` paths.

## V1.6.119 package intent

V1.6.119 is a production-roadmap hardening build on the path to V1.7. It keeps the stable macOS AFF4/APFS and portable-runtime path intact, and adds dedicated iOS validation scripts so CoreSpotlight and app-database Spotlight/search eligibility work can be tested cleanly.

### V1.6.118 inherited iOS work

- `ios_app_db_spotlight_flag_candidates` table and `vw_ios_app_db_spotlight_flag_candidates` view.
- `vw_ios_app_db_spotlight_enabled_summary` includes `flag_candidate_count`, `flag_confidence_sample`, and `flag_reason_sample`.
- GUI view: `iOS - App Spotlight Flag Candidates`.
- iOS app DB parser performs bounded row sampling for app-maintained Spotlight/search/index eligibility flags.
- Flag values are app-declared eligibility/search state only; they are not proof of actual CoreSpotlight indexing unless correlated.

### V1.6.119 changes

- Added `tools/Verify-iOSSpotlightValidationOutputs.ps1` to verify that iOS upload bundles contain the required CoreSpotlight and app-DB Spotlight validation CSVs.
- Added `scripts/Run-V1_6_119-iOS-AppDbSpotlight-AndZip.ps1` for bounded iOS app-database Spotlight/search eligibility validation.
- Added `scripts/Run-V1_6_119-iOS-TestMatrix-AndZip.ps1` to run a default two-step iOS validation matrix: CoreSpotlight thin sanity plus app-DB Spotlight bounded validation. Optional switches can include validation-support, bounded full-native DB, and production runs.
- `tools/Run-IosCoreSpotlightFocusedZip.ps1` now runs the new iOS validation-output audit after creating an upload ZIP.
- `tools/Create-SourceProbeUploadZip.ps1` now explicitly includes the iOS app-DB Spotlight eligibility CSVs/samples in focused upload ZIPs.
- `Run-V1_6_119-iOS-Production-AndZip.ps1` now accepts `-ReuseIosCache` consistently.
- Preserved Spotlight-only external-volume precision, portable runtime packaging, bundled AFF4 reader tools, bundled VC runtime DLLs, GUI resizable investigator left pane, and existing AFF4/APFS output audits.

## Standing project rules

1. Treat the latest uploaded source ZIP as the source of truth.
2. Verify every claimed issue against uploaded logs, thin result bundles, source files, or direct tool output before changing code.
3. Avoid oversized C++ raw string literals. The MSVC audit must pass with zero strings above 5,000 characters.
4. Keep active Markdown consolidated to exactly five package Markdown files:
   - `.github/pull_request_template.md`
   - `ai_context.md`
   - `docs/PROJECT_REFERENCE_V<version>.md`
   - `docs/START_CONTINUATION_CHAT.md`
   - `third_party/lzfse/README.md`
5. Every build/release/hotfix package must include downloadable PowerShell scripts for build-only and next validation/thin workflow, plus exact copy/paste PowerShell commands.
6. Thin/trial/pressure-test runs must skip original source-container SHA256 unless a separate explicit full-validation hash confirmation is supplied.
7. When thin results are uploaded, review and proceed to the next build unless the user explicitly says pause.
8. Investigator-facing timeline/time records must be grouped by file/folder identity. Raw date candidates may remain one row per parsed date for provenance, but the primary GUI/export timeline must not revert to one row per date.
9. Points of interest and external-volume rows are investigative leads only, not proof.
10. GUI investigation views should show counts where safe; pagination should include First, Previous, Next, and Last; the left-side investigation view selector pane must remain mouse-resizable.
11. Windows `build-msvc\Release` must be made as portable as possible with `resources\reader_tools`, launch/check scripts, and reader-tool manifests. Missing required runtime files must produce `PORTABLE_RUNTIME_INCOMPLETE`, not a silent non-portable folder.

## Database architecture roadmap

- SQLite remains the authoritative forensic case DB.
- DuckDB is optional only for future reporting/analytics sidecars.
- RocksDB or per-store SQLite may be evaluated only as parser scratch/cache.
- Prioritize schema/index/query optimization, grouped timeline summaries, three-database sidecars, and controlled parallel parsing before any database-engine rewrite.
- The three-database filesystem comparison architecture remains transitional: the primary case DB is the Spotlight evidence DB; `filesystem_inventory.sqlite` and `comparison.sqlite` sidecars support APFS inventory/comparison evidence.

## V1.7 readiness criteria

Do not declare V1.7 until:

1. macOS AFF4/APFS thin run passes with stable baseline counts.
2. Portable runtime Release folder and portable ZIP are verified.
3. Spotlight-only external-volume outputs are present, precise, and free of ordinary `/System/Volumes/Data/...` false positives.
4. GUI investigation views remain responsive, with resizable left pane and First/Previous/Next/Last pagination.
5. A fresh iOS validation run confirms CoreSpotlight and app DB Spotlight eligibility outputs are useful and do not regress prior iOS views.
6. Build scripts, root scripts, packaged commands, and thin-upload contents are consistent.

## Expected V1.6.119 validation workflow

Run the AFF4/APFS thin workflow:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_119-AfterDownload.ps1
```

Build only:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_119-AfterDownload.ps1 -Workflow BuildOnly
```

Check portable runtime after build:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_119\build-msvc\Release\Check-PortableRuntime.ps1
```

Expected upload after run:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_119.zip
D:\Downloads\V1_6_119_build.log
D:\Downloads\V1_6_119_AFF4_WRAPPER_RUN_SUMMARY.txt
```

For the next iOS validation, use either the default iOS test matrix or the app-DB focused script:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_119-iOS-TestMatrix-AfterDownload.ps1 -CleanOut
```

Or, after the source tree is already built:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_119\scripts\Run-V1_6_119-iOS-TestMatrix-AndZip.ps1 -CleanOut
```

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_119\scripts\Run-V1_6_119-iOS-AppDbSpotlight-AndZip.ps1 -CleanOut
```

Upload the resulting iOS matrix/app-DB bundle, build log, run summary, and any WhatsApp/schema samples if available. Main iOS check: `ios_app_db_spotlight_flag_candidates.csv` should identify app-maintained Spotlight/search eligibility flags where present, but all rows must remain labeled as app-declared indicators rather than proof of actual CoreSpotlight indexing.
