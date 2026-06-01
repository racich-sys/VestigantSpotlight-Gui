# Build and Run Instructions - V0_8_62

## Build

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V0_8_62.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_8_62" -Recurse -Force -ErrorAction SilentlyContinue

Expand-Archive `
  -LiteralPath .\VestigantSpotlightInv_V0_8_62.zip `
  -DestinationPath T:\ `
  -Force

& "T:\VestigantSpotlightInv_V0_8_62\build_windows_msvc.bat" 2>&1 |
  Tee-Object -FilePath "D:\Downloads\V0_8_62_build.log"
```

## macOS AFF4/APFS validation run

```powershell
& "T:\VestigantSpotlightInv_V0_8_62\tools\Run-SingleAff4SourceProbeAndZip.ps1" `
  -Aff4Input "O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4" `
  -Out "T:\SpotlightCase\V0_8_62_ExternalCompare" `
  -ReaderToolsRoot "T:\VestigantReaderTools\aff4-cpp-lite" `
  -ZipPath "D:\Downloads\Upload_Thin_V0_8_62_ExternalCompare.zip" `
  -EnableAff4VirtualApfsProbe `
  -ExternalSpotlightRoot "T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2" `
  -ExternalCompareOutRoot "T:\V0_8_62_ExternalCompare_CompareOnly" `
  -UploadWorkRoot "D:\Downloads\V0_8_62_UploadWork" `
  -NoClipboardOrExplorer `
  -CleanOut
```

## iOS CoreSpotlight focused run

```powershell
& "T:\VestigantSpotlightInv_V0_8_62\tools\Run-IosCoreSpotlightFocusedZip.ps1" `
  -InputZipOrFolder "D:\Downloads\iOS_CoreSpotlight_Focused_Extracts.zip" `
  -Out "T:\SpotlightCase\V0_8_62_iOS_CoreSpotlight" `
  -ZipPath "D:\Downloads\Upload_Thin_V0_8_62_iOS_CoreSpotlight.zip" `
  -CleanOut `
  -NoClipboardOrExplorer
```
