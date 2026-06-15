# Build Instructions - V1.6.29.4

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_29_4.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_29_4" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_29_4.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_29_4\scripts\Build-V1_6_29_4.ps1 -CleanExtract
```

Upload after build:

```text
D:\Downloads\V1_6_29_4_build.log
```

After the build passes, run:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_29_4\scripts\Run-V1_6_29_4-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Expected upload:

```text
D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_29_4.zip
```
