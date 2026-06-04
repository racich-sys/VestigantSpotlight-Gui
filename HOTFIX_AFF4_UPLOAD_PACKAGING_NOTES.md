# Vestigant Spotlight v1.0.0 AFF4 Upload Packaging Hotfix

## Reason

The V1.0.0 AFF4/APFS probe runner successfully created the case output and a thin upload ZIP, then failed the post-package validation step because `tools/Create-SourceProbeUploadZip.ps1` did not copy the new V1 diagnostic planning files into the upload bundle.

Observed failure:

`Thin upload ZIP is missing expected entry: AFF4_APFS_V1_DIAGNOSTIC_RERUN_PLAN.md`

This hotfix is script-only. It does not change parser binaries or APFS interpretation logic.

## Changed files

- `tools/Create-SourceProbeUploadZip.ps1`
  - Adds the V1 AFF4/APFS diagnostic plan files to the upload wanted-file list.
  - Searches `CaseRoot\Upload_Thin` in addition to case root, `logs`, and `Upload`.
- `tools/Run-SingleAff4SourceProbeAndZip.ps1`
  - Uses the same case-file resolution logic for ZIP-entry validation that it uses when checking case outputs.

## Fast recovery command

After overlaying this hotfix onto `T:\VestigantSpotlightInv_V1_0_0`, the existing case output can be repackaged without rerunning the 90-minute AFF4 probe:

```powershell
Set-Location D:\Downloads

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_0\tools\Create-SourceProbeUploadZip.ps1 `
  -CaseRoot "Q:\SpotlightCase\TestMacOS_AFF4_V1_0_0" `
  -ReaderToolsRoot "T:\VestigantReaderTools\aff4-cpp-lite" `
  -ZipPath "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_0_REPACK.zip" `
  -UploadWorkRoot "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_0_REPACK_UploadWork"
```

Upload `D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_0_REPACK.zip` for review.
