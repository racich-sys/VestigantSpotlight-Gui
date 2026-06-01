# V0_7_15 Test Commands

```powershell
$ErrorActionPreference = "Stop"

$Zip = "T:\VestigantSpotlightInv_V0_7_15_object_usage_summary_source.zip"
$Root = "T:\VestigantSpotlightInv_V0_7_15"

if (Test-Path -LiteralPath $Root) {
    throw "Folder already exists. Rename/delete first: $Root"
}

Expand-Archive -LiteralPath $Zip -DestinationPath "T:\" -Force

cmd.exe /c "T:\VestigantSpotlightInv_V0_7_15\build_windows_msvc.bat"

& "T:\VestigantSpotlightInv_V0_7_15\build-msvc\Release\VestigantSpotlightCli.exe" --version
```

Expected:

```text
Vestigant Spotlight v0.7.15
```

Open GUI:

```powershell
Start-Process "T:\VestigantSpotlightInv_V0_7_15\build-msvc\Release\VestigantSpotlight.exe"
```

Known database:

```text
Q:\SpotlightCase\V0_7_6_Mac_MVP_Dashboard_Block100\VestigantSpotlight.case.sqlite
```

Expected output after default/minimal export:

```text
Q:\SpotlightCase\<case>\exports\object_usage_summary.csv
Q:\SpotlightCase\<case>\exports\upload_samples\object_usage_summary_focus.csv
```
