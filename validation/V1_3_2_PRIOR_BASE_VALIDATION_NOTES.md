# V1.3.2 Local Validation Notes

## Inputs reviewed

- Uploaded V1.2.0 source package.
- Uploaded V1.2.0 Windows/MSVC build log.

## Build-log review

The V1.2.0 build log shows the common sources, CLI, self-test entry point, GUI view registry/helpers/export worker, and GUI entry point compiled and linked. The compiled CLI version check reported `Vestigant Spotlight v1.2.0`, and the build completed with GUI, CLI, and test executables.

## Source changes validated locally

- Version metadata updated to 1.3.2.
- Versioned scripts updated to V1_3_2 and build wrapper checks `1\.2\.1`.
- Guided APFS target inode/file-extent lookup code now reuses outer node buffers instead of allocating fresh local vectors per lookup.
- Resolved APFS child node buffers are swapped into the reusable buffer to avoid avoidable copy assignment.

## Constraints

- Windows/MSVC build was not run in this packaging environment.
- AFF4/APFS thin run was not run in this packaging environment.
- No iOS parser test was run because iOS parsing was not changed.

## Required next commands

Build:

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_3_2.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_3_2" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_3_2.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_3_2\scripts\Build-V1_3_2.ps1
```

Thin-create:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_3_2\scripts\Run-V1_3_2-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```
