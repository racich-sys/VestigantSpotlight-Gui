# V0_7_17 Test Commands

```powershell
$ErrorActionPreference = "Stop"

$Zip = "T:\VestigantSpotlightInv_V0_7_17_progress_upload_profiles_source.zip"
$Root = "T:\VestigantSpotlightInv_V0_7_17"

if (Test-Path -LiteralPath $Root) {
    throw "Folder already exists. Rename/delete first: $Root"
}

Expand-Archive -LiteralPath $Zip -DestinationPath "T:\" -Force

cmd.exe /c "T:\VestigantSpotlightInv_V0_7_17\build_windows_msvc.bat"

& "T:\VestigantSpotlightInv_V0_7_17\build-msvc\Release\VestigantSpotlightCli.exe" --version
```

Expected:

```text
Vestigant Spotlight v0.7.17
```

Open GUI:

```powershell
Start-Process "T:\VestigantSpotlightInv_V0_7_17\build-msvc\Release\VestigantSpotlight.exe"
```

CLI full ingest:

```powershell
& "T:\VestigantSpotlightInv_V0_7_17\build-msvc\Release\VestigantSpotlightCli.exe" `
  --mode run `
  --profile auto `
  --input "T:\Spotlight\Store-V2" `
  --out "Q:\SpotlightCase\V0_7_17_FullStoreV2" `
  --decode-core-native-values `
  --experimental-full-native-values `
  --max-native-records 0 `
  --max-native-blocks 0 `
  --export-profile investigator `
  --verbose
```

Focused upload ZIP, default profile:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_7_17\tools\Create-UploadZip.ps1" `
  -CaseOut "Q:\SpotlightCase\V0_7_17_FullStoreV2"
```

Standard upload ZIP:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_7_17\tools\Create-UploadZip.ps1" `
  -CaseOut "Q:\SpotlightCase\V0_7_17_FullStoreV2" `
  -Profile Standard
```
