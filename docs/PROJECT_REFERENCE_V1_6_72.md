# Vestigant Spotlight / Spotlight2 Project Reference V1.6.72

## Current package

- Package: `VestigantSpotlightInv_V1_6_72.zip`
- Version: `1.6.72`
- Current priority: AFF4/APFS staged Store-V2 validation, with upload packaging and review artifacts preserved.

## Required first file

Read `ai_context.md` first before any code, script, documentation, package, or build change.

## Primary command rule

Every release must include a root-level one-click PowerShell script named `Run-V<version>-AfterDownload.ps1`. The final response must provide a single copy/paste command for that script immediately after the download links.

## Primary copy/paste command for this package

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_72-AfterDownload.ps1
```

Default behavior for V1.6.72:

- builds from `D:\Downloads\VestigantSpotlightInv_V1_6_72.zip`
- runs required self-test
- runs AFF4 broad probe
- searches `T:\` for an `.aff4` path containing `0202_0024-IT003`
- runs with `-CleanOut`
- runs with `-FullNoGuardrails`
- runs bounded Store-V2 validation in CoreFields mode by default

## Fallback/direct AFF4 command

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\BuildAndRun-V1_6_72-FromDownloadedZip.ps1 -Workflow AFF4Probe -CleanOut -Aff4SearchRoot "T:\" -Aff4NameHint "0202_0024-IT003" -FullNoGuardrails
```

## Optional AFF4 command with reader tools

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\BuildAndRun-V1_6_72-FromDownloadedZip.ps1 -Workflow AFF4Probe -CleanOut -Aff4SearchRoot "T:\" -Aff4NameHint "0202_0024-IT003" -ReaderToolsRoot "T:\VestigantReaderTools\aff4-cpp-lite" -FullNoGuardrails
```

## iOS thin command

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_72-AfterDownload.ps1 -Workflow IOSCoreSpotlightThin
```

## Build/self-test only command

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_72-AfterDownload.ps1 -Workflow BuildOnly
```

## GUI launch after build/extraction

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_72\scripts\Launch-V1_6_72-GUI.ps1
```

## Expected AFF4 upload artifacts

Upload these after the AFF4 run:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_72.zip
D:\Downloads\V1_6_72_build.log
D:\Downloads\V1_6_72_AFF4_WRAPPER_RUN_SUMMARY.txt
```

Fallback upload if wrapper rescue is created:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_72_FAILED_WRAPPER_RESCUE.zip
```

## V1.6.72 change summary

- Added root and script copies of `Run-V1_6_72-AfterDownload.ps1`.
- The one-click script defaults to the current AFF4 test and calls the unified wrapper with `AFF4Probe`, `CleanOut`, `Aff4SearchRoot T:\`, `Aff4NameHint 0202_0024-IT003`, and `FullNoGuardrails`; it leaves FullValues off unless `-FullNativeValues` is explicitly supplied.
- Updated `ai_context.md` to make this a standing requirement for future packages.
- Preserved the V1.6.69 AFF4 packaging fix that avoids passing a blank `ReaderToolsRoot` into upload packaging.
- Preserved the V1.6.69 emergency rescue bundle behavior for AFF4 runner failures after a case folder exists.
- Preserved the direct AFF4 stream-base dynamic selection fix from the V1.6.67/V1.6.69 line.

## Validation status

Local validation may verify Linux build/static checks. Windows/MSVC build, Windows self-test, iOS thin runtime, and AFF4 runtime are not verified until uploaded logs prove them.

## Active Markdown policy

The package must contain exactly these active Markdown files:

- `.github/pull_request_template.md`
- `ai_context.md`
- `docs/PROJECT_REFERENCE_V1_6_72.md`
- `docs/START_CONTINUATION_CHAT.md`
- `third_party/lzfse/README.md`


## V1.6.72 AFF4 stalled-parse hotfix

Uploaded V1.6.70 AFF4 logs showed direct-map/APFS staging succeeded and the native parser entered FullValues mode against six selected staged Store-V2 databases. The parser reached 20,000 parsed items with 881,055 raw key/value rows and later logged the 25,000-record diagnostic cap, but the uploaded snapshot did not show parse-complete or upload packaging. V1.6.72 therefore changes the AFF4 validation default to bounded CoreFields mode, keeps FullValues available through an explicit FullNativeValues switch, defers AFF4 path resolution until after build/self-test, and improves progress/heartbeat visibility for the native parser.

### Copy/paste PowerShell command

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_72-AfterDownload.ps1
```

Expected outputs after AFF4 run:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_72.zip
D:\Downloads\V1_6_72_build.log
D:\Downloads\V1_6_72_AFF4_WRAPPER_RUN_SUMMARY.txt
```

For targeted full native value testing only:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_72-AfterDownload.ps1 -FullNativeValues
```

## V1.6.72 AFF4 no-record-cap hotfix

V1.6.72 fixes a verified no-record-cap propagation bug in the AFF4 staged Store-V2 validation workflow.

Verified from uploaded V1.6.71 no-cap artifacts:

- The wrapper summary recorded `MaxNativeRecords: 0`, `FullNativeValues: False`, and `RunnerExitCode: 0`.
- The uploaded ZIP SHA256 was `9497504E999CB62DD413016007977B37187BE1CB26E93F5006644FEB56FE178C`.
- The extracted parser summary still reported `max_records_used=25000`, `parsed_items=25000`, and `raw_records=25000`.
- Therefore V1.6.71 completed successfully, but it did not perform the requested uncapped CoreFields parse.

Root cause fixed:

- AFF4 wrappers now pass `MaxNativeRecords` when it is 0 or greater.
- `tools/Run-SingleAff4SourceProbeAndZip.ps1` now passes `--max-native-records 0` to the CLI.
- `src/app/app_runner.cpp` now uses `maxNativeRecordsExplicit` so omitted max-record value defaults to 25000, while explicit zero remains uncapped.
- The root one-click script defaults to MaxNativeRecords 0 for the current AFF4 no-record-cap validation target.

Copy/paste PowerShell command:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_72-AfterDownload.ps1
```

Expected upload after run:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_72.zip
D:\Downloads\V1_6_72_build.log
D:\Downloads\V1_6_72_AFF4_WRAPPER_RUN_SUMMARY.txt
```
