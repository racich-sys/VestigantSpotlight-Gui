# V1.6.35 Build Instructions Addendum

V1.6.35 retains the dynamic-version/advisory release-readiness build workflow and adds macOS Store-V2 external dbStr map loading.

# V1.6.35 Build Instructions Addendum

V1.6.35 retains dynamic-version build checking and advisory release-readiness behavior. Fatal preflight is limited to wrapper compatibility and MSVC raw-string risk.

# Build Instructions - 1.6.35

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_35.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_35" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_35.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_35\scripts\Build-V1_6_35.ps1 -CleanExtract
```

Upload after build: `D:\Downloads\V1_6_35_build.log`.

# Build Instructions - 1.6.35

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_35.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_35" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_35.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_35\scripts\Build-V1_6_35.ps1 -CleanExtract
```

Upload after build: `D:\Downloads\V1_6_35_build.log`.

# Build Instructions - V1.6.35

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_6_35.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_6_35" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_6_35.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_35\scripts\Build-V1_6_35.ps1 -CleanExtract
```

Upload after build:

```text
D:\Downloads\V1_6_35_build.log
```

After the build passes, run:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_6_35\scripts\Run-V1_6_35-iOS-CoreSpotlight-AndZip.ps1 -CleanOut
```

Expected upload:

```text
D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_35.zip
```

## V1.6.35 build

Use `scripts\Build-V1_6_35.ps1 -CleanExtract` from the expanded V1.6.35 source tree. Upload `D:\Downloads\V1_6_35_build.log` for validation.

## V1.6.35 build

Use `scripts\Build-V1_6_35.ps1 -CleanExtract`. Upload `D:\Downloads\V1_6_35_build.log` for validation.
