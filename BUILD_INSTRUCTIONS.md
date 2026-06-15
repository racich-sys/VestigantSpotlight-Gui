# Build Instructions - V1.6.28

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_28.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_28" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_28.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_28\scripts\Build-V1_6_28.ps1 -CleanExtract
```

Upload after build:

```text
D:\Downloads\V1_6_28_build.log
```

After the build passes, run:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_28\scripts\Run-V1_6_28-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Expected upload:

```text
D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_28.zip
```
