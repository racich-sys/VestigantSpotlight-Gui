# V0_7_16 Test Commands

## Build

```powershell
$ErrorActionPreference = "Stop"

$Zip = "T:\VestigantSpotlightInv_V0_7_16_gui_full_ingest_status_source.zip"
$Root = "T:\VestigantSpotlightInv_V0_7_16"

if (Test-Path -LiteralPath $Root) {
    throw "Folder already exists. Rename/delete first: $Root"
}

Expand-Archive -LiteralPath $Zip -DestinationPath "T:\" -Force

cmd.exe /c "T:\VestigantSpotlightInv_V0_7_16\build_windows_msvc.bat"

& "T:\VestigantSpotlightInv_V0_7_16\build-msvc\Release\VestigantSpotlightCli.exe" --version
```

Expected:

```text
Vestigant Spotlight v0.7.16
```

## GUI

```powershell
Start-Process "T:\VestigantSpotlightInv_V0_7_16\build-msvc\Release\VestigantSpotlight.exe"
```

## Full Store-V2 CLI run

```powershell
& "T:\VestigantSpotlightInv_V0_7_16\build-msvc\Release\VestigantSpotlightCli.exe" `
  --mode run `
  --profile auto `
  --input "T:\Spotlight\Store-V2" `
  --out "Q:\SpotlightCase\V0_7_16_FullStoreV2" `
  --decode-core-native-values `
  --experimental-full-native-values `
  --max-native-records 0 `
  --max-native-blocks 0 `
  --export-profile investigator `
  --verbose
```

## Thin upload ZIP

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_7_16\tools\Create-UploadZip.ps1" `
  -CaseOut "Q:\SpotlightCase\V0_7_16_FullStoreV2"
```
