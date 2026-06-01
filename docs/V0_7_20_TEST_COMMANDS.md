# V0_7_20 Test Commands

```powershell
$ErrorActionPreference = "Stop"

$Zip = "T:\VestigantSpotlightInv_V0_7_20_gui_workflow_perf_source.zip"
$Root = "T:\VestigantSpotlightInv_V0_7_20"

if (Test-Path -LiteralPath $Root) {
    throw "Folder already exists. Rename/delete first: $Root"
}

Expand-Archive -LiteralPath $Zip -DestinationPath "T:\" -Force

cmd.exe /c "T:\VestigantSpotlightInv_V0_7_20\build_windows_msvc.bat"

& "T:\VestigantSpotlightInv_V0_7_20\build-msvc\Release\VestigantSpotlightCli.exe" --version
```

Expected:

```text
Vestigant Spotlight v0.7.20
```
