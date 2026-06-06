# V1.0.27 Validation Notes

## Baseline reviewed

The uploaded V1.0.26 build log shows the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.

The user-reported AFF4/APFS wrapper output shows the V1.0.26 probe completed through external comparison and remaining-mismatch diagnostic generation, then failed during thin-upload packaging with:

```text
Get-RelativePathForThinInventory : Cannot convert value "\\" to type "System.Char".
```

## Root cause

`tools/Create-SourceProbeUploadZip.ps1` used `[char]'\\'` in `TrimStart()`. Windows PowerShell treats the escaped backslash text as two characters and cannot cast it to `System.Char`.

## Fix implemented

- Replaced relative-path trimming with a `System.Uri.MakeRelativeUri` implementation compatible with Windows PowerShell 5.1.
- Reused the helper for `ExtractedSpotlight` upload-relative paths.
- Changed reader-tools inventory output to relative paths.
- Added `scripts/Package-V1_0_27-macOS-AFF4-ThinFromExistingCase.ps1` so the existing completed V1.0.26 case can be packaged without rerunning the AFF4 probe.
- Added continuing-chat handoff, roadmap checklist, and suggestions/fixes tracker files under `docs/`.

## Local validation performed

- CMake configure: passed.
- `src/core/app_info.cpp` C++20 syntax check: passed.
- `src/gui/gui_view_helpers.cpp` C++20 syntax check: passed.
- Static PowerShell text checks: passed.

## Pending validation

- Windows/MSVC V1.0.27 build.
- Execute the packaging-only wrapper against the existing V1.0.26 case output.
- Upload and review `Upload_Thin_MacOS_AFF4_V1_0_27.zip`.
- Verify generated thin ZIP excludes denied raw logs and inventories.
