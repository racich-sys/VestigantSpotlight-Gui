# Build Instructions — Vestigant Spotlight V1.1.7.1

## Windows/MSVC build

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_1_7_1.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_7_1" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_7_1.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Build-V1_1_7_1.ps1
```

Expected successful version output:

```text
Vestigant Spotlight v1.1.7.1
```

## Optional self-test

The build script prints the test command. The direct form is:

```powershell
& "T:\VestigantSpotlightInv_V1_1_7_1\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V1_1_7_1\build-msvc\selftest_out"
```

## GUI launch

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Launch-V1_1_7_1-GUI.ps1
```

## macOS AFF4/APFS thin regression

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_7_1\scripts\Run-V1_1_7_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

Expected thin output:

```text
D:\Downloads\Upload_Thin_MacOS_AFF4_V1_1_7_1.zip
```

## Source package cleanup note

This package intentionally keeps only current version-specific scripts in `scripts/`. Older version details are preserved in append-only documentation, not as old executable wrappers in the active source root.
