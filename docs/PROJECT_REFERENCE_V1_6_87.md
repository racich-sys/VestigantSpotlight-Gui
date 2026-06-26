# Vestigant Spotlight / Spotlight2 Project Reference V1.6.87

## Purpose

Vestigant Spotlight / Spotlight2 is a Windows C++ forensic tool for macOS Spotlight Store-V2 and iOS CoreSpotlight analysis. Current work focuses on AFF4/APFS macOS Spotlight intake, Store-V2 staging/parsing/enrichment, Cache text review, and investigator GUI/search workflows.

## Current package

- Package: `VestigantSpotlightInv_V1_6_87.zip`
- Version: `1.6.87`
- Primary next test: macOS AFF4 thin/trial run against `0202_0024-IT003.aff4`

## V1.6.87 focus

V1.6.87 is a narrow hotfix after repeated V1.6.86 same-version script patches still allowed the AFF4 thin workflow to hash the 74,468,278,910-byte AFF4 container.

The verified defect was wrapper/control-flow staleness, not APFS parsing: `Run-V1_6_86-AfterDownload.ps1` could prefer a stale `D:\Downloads\BuildAndRun-V1_6_86-FromDownloadedZip.ps1` or sibling script before using the updated wrapper inside the source ZIP. That stale script could pass `-ForceContainerHash`, causing `tools/Run-SingleAff4SourceProbeAndZip.ps1` to print `Source-container SHA256 hashing requested for this run.` and the app to enter `original_container_hash_start`.

## V1.6.87 changes

1. Version advanced to V1.6.87 to avoid stale same-version wrapper collisions.
2. `Run-V1_6_87-AfterDownload.ps1` always extracts and uses the `BuildAndRun` wrapper from `VestigantSpotlightInv_V1_6_87.zip`.
3. The one-click wrapper intentionally ignores stale sibling/download-folder `BuildAndRun` scripts.
4. AFF4 thin/trial wrappers pass `-SkipContainerHash`.
5. `tools/Run-SingleAff4SourceProbeAndZip.ps1` requires both `-ForceContainerHash` and `-ConfirmSourceContainerHash` before it will pass `--force-container-hash` to the CLI.
6. A stale caller that supplies only `-ForceContainerHash` is converted to `--skip-container-hash` and prints an explicit warning.
7. Thin/trial scripts no longer create `Upload_Thin_MacOS_AFF4_V1_6_87.zip.sha256.txt` by default.
8. Build/download wrappers no longer display source ZIP SHA256 during thin/test runs.

## Expected V1.6.87 thin-run console indicators

Expected:

```text
Bootstrap policy: extracting wrapper from ZIP and ignoring stale D:\Downloads sibling scripts.
Thin/test mode: passing --skip-container-hash.
```

Acceptable if a stale caller somehow supplies `-ForceContainerHash`:

```text
Ignoring -ForceContainerHash because -ConfirmSourceContainerHash was not supplied. Thin/test mode is passing --skip-container-hash.
```

Must not appear during thin/trial runs:

```text
Source-container SHA256 hashing requested for this run.
Original container SHA256 hashing started.
original_container_hash_start
```

## Copy/paste command

```powershell
powershell -ExecutionPolicy Bypass -File D:\Downloads\Run-V1_6_87-AfterDownload.ps1
```

Required files in `D:\Downloads`:

```text
D:\Downloads\VestigantSpotlightInv_V1_6_87.zip
D:\Downloads\Run-V1_6_87-AfterDownload.ps1
```

## Expected uploads after next run

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_87.zip
D:\Downloads\V1_6_87_build.log
D:\Downloads\V1_6_87_AFF4_WRAPPER_RUN_SUMMARY.txt
```

A thin upload SHA sidecar is not expected unless a full validation hash is specifically requested later.

## Validation performed before packaging

- Source-level review of all hashing-related C++ and PowerShell paths.
- Static audit for stale wrapper preference and unguarded AFF4 thin hashing calls.
- Linux CMake build.
- CLI version check.
- Required self-test.
- ZIP integrity test.
- Active Markdown count check.

## Validation not performed in this sandbox

- Windows/MSVC V1.6.87 build.
- AFF4/APFS runtime completion.
- GUI runtime validation.
