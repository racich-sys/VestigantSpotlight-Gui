# V0_7_19 Test Commands

```powershell
$ErrorActionPreference = "Stop"

$Zip = "T:\VestigantSpotlightInv_V0_7_19_container_preservation_column_order_source.zip"
$Root = "T:\VestigantSpotlightInv_V0_7_19"

if (Test-Path -LiteralPath $Root) {
    throw "Folder already exists. Rename/delete first: $Root"
}

Expand-Archive -LiteralPath $Zip -DestinationPath "T:\" -Force

cmd.exe /c "T:\VestigantSpotlightInv_V0_7_19\build_windows_msvc.bat"

& "T:\VestigantSpotlightInv_V0_7_19\build-msvc\Release\VestigantSpotlightCli.exe" --version
```

Expected:

```text
Vestigant Spotlight v0.7.19
```

## Folder full ingest

```powershell
& "T:\VestigantSpotlightInv_V0_7_19\build-msvc\Release\VestigantSpotlightCli.exe" `
  --mode run `
  --profile auto `
  --input "T:\Spotlight\Store-V2" `
  --out "Q:\SpotlightCase\V0_7_19_FullStoreV2" `
  --decode-core-native-values `
  --experimental-full-native-values `
  --max-native-records 0 `
  --max-native-blocks 0 `
  --export-profile investigator `
  --verbose
```

Folder input should create a 7z preservation archive record.

## ZIP smoke test

```powershell
& "T:\VestigantSpotlightInv_V0_7_19\build-msvc\Release\VestigantSpotlightCli.exe" `
  --mode run `
  --profile auto `
  --input "T:\Spotlight\Store-V2-Test.zip" `
  --out "Q:\SpotlightCase\V0_7_19_ZipSmoke" `
  --decode-core-native-values `
  --experimental-full-native-values `
  --max-native-records 0 `
  --max-native-blocks 0 `
  --export-profile investigator `
  --verbose
```

ZIP input should show `ORIGINAL_CONTAINER_REGISTERED_NO_REARCHIVE` in preserved evidence sets and should not create a second evidentiary 7z archive.
```
