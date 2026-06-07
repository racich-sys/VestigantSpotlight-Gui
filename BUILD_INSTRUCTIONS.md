# Build Instructions — Vestigant Spotlight 1.1.10.1

## Scope

V1.1.10.1 is a documentation/script command hotfix on V1.1.10. It updates the current-version PowerShell command blocks used for build, thin test, and new-chat continuation. No parser, extraction, GUI, or schema behavior was intentionally changed.

## Full Windows/MSVC build command block

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_1_10_1.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_10_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_10_1.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Build-V1_1_10_1.ps1
```

Expected successful version output:

```text
Vestigant Spotlight v1.1.10.1
```

Build log output:

```text
D:\Downloads\V1_1_10_1_build.log
```

## Optional self-test

The build script runs the normal build wrapper and verifies CLI version output. The direct self-test executable path after build is:

```powershell
& "T:\VestigantSpotlightInv_V1_1_10_1\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V1_1_10_1\build-msvc\selftest_out"
```

## GUI launch

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Launch-V1_1_10_1-GUI.ps1
```

## macOS AFF4/APFS thin regression

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_10_1\scripts\Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

Expected thin output:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_1_10_1.zip
```

## Source package cleanup note

This package intentionally keeps only current version-specific scripts in `scripts/`. Older version details are preserved in append-only documentation, not as old executable wrappers in the active source root.
