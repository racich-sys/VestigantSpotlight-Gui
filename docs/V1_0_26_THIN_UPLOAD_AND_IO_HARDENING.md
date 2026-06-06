# V1.0.26 Thin Upload and I/O Hardening

V1.0.26 is a narrow hardening release after the V1.0.25 Windows/MSVC build and macOS AFF4/APFS thin run were reviewed.

## Evidence reviewed

- `V1_0_25_build.log` showed a successful Windows/MSVC build and binary version check for `Vestigant Spotlight v1.0.25`.
- `Upload_Thin_MacOS_AFF4_V1_0_25.zip` reached `complete_source_probe` and preserved the V1.0.25 AFF4/APFS staged Store-V2 baseline.
- The V1.0.25 thin ZIP still contained `aff4_stream_inventory_raw.txt` through the standalone thin upload PowerShell tool path. That raw output remains useful locally but is not appropriate for the thin-upload package.

## Changes

- Added a deny-list policy for thin-upload helper paths:
  - `aff4_stream_inventory_raw.txt`
  - `ios_focused_zip_extract.log`
  - `ios_focused_zip_extract_7z.log`
  - `ios_focused_zip_extract.ps1`
  - `ios_ffs_file_inventory.csv`
  - `image_file_inventory.csv`
- Applied the same policy to the in-app upload-bundle copier and to `tools/Create-SourceProbeUploadZip.ps1`.
- Updated PowerShell-generated `case_file_inventory.txt` and `additional_output_file_inventory.txt` to use relative paths rather than full absolute paths.
- Added a bounded 12-hour wait for hidden Windows subprocess launches so a prompted or wedged external tool is terminated instead of hanging indefinitely.
- Updated exact AFF4 ZIP byte reads on Windows to use `_wfopen_s` plus `_fseeki64`, avoiding 32-bit-offset truncation risk in large AFF4/ZIP containers.

## Not changed

- APFS traversal and APFS Store-V2 staging are unchanged.
- Store-V2 parsing is unchanged.
- iOS CoreSpotlight parsing and app database parsing are unchanged.
- SQLite schema and GUI view definitions are unchanged.
- APFS diagnostic writer bodies were not moved in this version.
