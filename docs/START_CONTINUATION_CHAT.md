# Start Continuation Chat - Vestigant Spotlight V1.6.119

Continue the Vestigant Spotlight project from V1.6.119.

## Current state

V1.6.118 is the latest reviewed macOS AFF4/APFS run. It completed successfully with `RunnerExitCode=0`, `PressureTestMode=True`, `SkipContainerHash=True`, `FullNativeValues=True`, and no native record/block caps. The established AFF4/APFS baseline remained stable: 102170 raw records, 4225419 raw key/value rows, 815736 raw date candidates, 101326 artifacts, 1092 usage-evidence rows, and 101326 grouped timeline rows.

The V1.6.118 build log showed Windows/MSVC compile success, required self-test success, portable staging `PORTABLE_RUNTIME_READY`, production readiness check passed, required runtime files present, portable runtime check passed, and portable ZIP creation.

The Spotlight-only external-volume review outputs were present and empty for the tested image, which is correct for this image after excluding ordinary internal `/System/Volumes/Data/...` paths.

## V1.6.119 changes

V1.6.119 moves toward V1.7 by adding iOS validation scripts around the V1.6.118 app-database Spotlight/search flag work:

- New tool: `tools/Verify-iOSSpotlightValidationOutputs.ps1`.
- New script: `scripts/Run-V1_6_119-iOS-AppDbSpotlight-AndZip.ps1` for bounded app-database Spotlight/search eligibility validation.
- New script: `scripts/Run-V1_6_119-iOS-TestMatrix-AndZip.ps1` for a default two-step iOS test matrix: CoreSpotlight thin sanity plus app-DB Spotlight bounded validation.
- Optional matrix switches allow validation-support, bounded full-native DB, and production iOS runs after the default tests are stable.
- `tools/Run-IosCoreSpotlightFocusedZip.ps1` now runs the iOS validation-output audit after upload ZIP creation.
- `tools/Create-SourceProbeUploadZip.ps1` now explicitly includes iOS app-DB Spotlight eligibility exports/samples.
- `Run-V1_6_119-iOS-Production-AndZip.ps1` now accepts `-ReuseIosCache` consistently.

Inherited V1.6.118 iOS work:

- Table/view/export for `ios_app_db_spotlight_flag_candidates`.
- Updated iOS Spotlight eligibility summary with flag counts and samples.
- GUI view: `iOS - App Spotlight Flag Candidates`.
- App DB flag rows remain app-declared indicators only, not proof of actual CoreSpotlight indexing unless correlated.

Preserved work:

- macOS AFF4/APFS Store-V2 extraction baseline.
- Spotlight-only external-volume precision review.
- Portable runtime packaging and checks.
- Bundled AFF4 reader tools and VC runtime DLLs.
- GUI resizable left investigation pane.
- First/Previous/Next/Last pagination work.

## Standing rules

- Treat the latest uploaded source ZIP as source of truth.
- Verify against logs/source/thin results before changing code.
- Keep exactly five package Markdown files.
- Avoid C++ raw strings over 5,000 bytes.
- Do not treat POIs, external-volume rows, or app DB Spotlight flags as proof.
- Group timeline/time events by artifact/file/folder/inode in investigator-facing outputs.
- Do not build unless the user asks or uploads thin results and asks to continue.

## Next validation commands

Run AFF4/APFS thin validation:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_119-AfterDownload.ps1
```

Build only:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_119-AfterDownload.ps1 -Workflow BuildOnly
```

Portable runtime check after build:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_119\build-msvc\Release\Check-PortableRuntime.ps1
```

Expected macOS AFF4 upload:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_119.zip
D:\Downloads\V1_6_119_build.log
D:\Downloads\V1_6_119_AFF4_WRAPPER_RUN_SUMMARY.txt
```


Run the default iOS test matrix after the build is stable:

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_119-iOS-TestMatrix-AfterDownload.ps1 -CleanOut
```

Or, after the source tree is already built:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_119\scripts\Run-V1_6_119-iOS-TestMatrix-AndZip.ps1 -CleanOut
```

Run only the iOS app-DB Spotlight eligibility validation:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_119\scripts\Run-V1_6_119-iOS-AppDbSpotlight-AndZip.ps1 -CleanOut
```

Expected iOS validation upload when available:

```text
Upload_iOS_TestMatrix_V1_6_119.zip
Upload_iOS_AppDbSpotlight_V1_6_119.zip
Upload_Thin_iOS_CoreSpotlight_V1_6_119.zip
V1_6_119_build.log
iOS wrapper/run summary
Any redacted WhatsApp/schema samples for suspected Spotlight-enabled fields
```

## V1.7 gate

Do not declare V1.7 until macOS AFF4/APFS, portable Release, GUI, Spotlight-only external-volume precision, and fresh iOS validation all pass without production-blocking regressions.
