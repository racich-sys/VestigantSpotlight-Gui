# V0_7_13 Test Commands

## Extract and build full source package

```powershell
$ErrorActionPreference = "Stop"

$Zip = "T:\VestigantSpotlightInv_V0_7_13_evidence_staging_zip_intake_source.zip"
$Root = "T:\VestigantSpotlightInv_V0_7_13"

if (Test-Path -LiteralPath $Root) {
    throw "Folder already exists. Rename/delete first: $Root"
}

Expand-Archive -LiteralPath $Zip -DestinationPath "T:\" -Force

cmd.exe /c "T:\VestigantSpotlightInv_V0_7_13\build_windows_msvc.bat"
```

## Verify version

```powershell
& "T:\VestigantSpotlightInv_V0_7_13\build-msvc\Release\VestigantSpotlightCli.exe" --version
```

Expected:

```text
Vestigant Spotlight v0.7.13
```

## Test folder staging

```powershell
& "T:\VestigantSpotlightInv_V0_7_13\tools\Stage-EvidenceSource.ps1" `
  -SourcePath "T:\Spotlight\Store-V2" `
  -CaseOut "Q:\SpotlightCase\V0_7_13_StagingTest_Folder" `
  -EvidenceSourceId "mac_storev2_folder_test"
```

## Test ZIP staging

```powershell
& "T:\VestigantSpotlightInv_V0_7_13\tools\Stage-EvidenceSource.ps1" `
  -SourcePath "T:\Spotlight\Store-V2-Test.zip" `
  -CaseOut "Q:\SpotlightCase\V0_7_13_StagingTest_Zip" `
  -EvidenceSourceId "mac_storev2_zip_test"
```

## Use staged/extracted folder for normal parser run

After staging, review:

```text
Q:\SpotlightCase\V0_7_13_StagingTest_Zip\EvidenceInventory\mac_storev2_zip_test_detection_report.txt
```

If it identifies a Store-V2 path, run the parser against the staged/extracted root:

```powershell
& "T:\VestigantSpotlightInv_V0_7_13\build-msvc\Release\VestigantSpotlightCli.exe" `
  --mode diagnostics `
  --profile auto `
  --input "Q:\SpotlightCase\V0_7_13_StagingTest_Zip\EvidenceStaging\mac_storev2_zip_test\extracted" `
  --out "Q:\SpotlightCase\V0_7_13_FromStagedZip" `
  --experimental-full-native-values `
  --max-native-records 25000 `
  --max-native-blocks 100 `
  --export-profile investigator `
  --verbose
```

## Create thin upload ZIP

```powershell
& "T:\VestigantSpotlightInv_V0_7_13\tools\Create-UploadZip.ps1" `
  -CaseOut "Q:\SpotlightCase\V0_7_13_StagingTest_Zip"
```

## V0_7_13_1 Robocopy compatibility note

If PowerShell blocks script execution, run helper scripts with a process-only bypass:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_7_13_1\tools\Stage-EvidenceSource.ps1" `
  -SourcePath "T:\Spotlight\Store-V2" `
  -CaseOut "Q:\SpotlightCase\V0_7_13_StagingTest_Folder" `
  -EvidenceSourceId "mac_storev2_folder_test" `
  -Force
```
