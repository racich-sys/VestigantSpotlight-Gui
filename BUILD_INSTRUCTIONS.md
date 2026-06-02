# Build Instructions

Current version: 0.9.34

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V0_9_34.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_9_34" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V0_9_34.zip -DestinationPath T:\ -Force
& "T:\VestigantSpotlightInv_V0_9_34\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath "D:\Downloads\V0_9_34_build.log"
& "T:\VestigantSpotlightInv_V0_9_34\build-msvc\Release\VestigantSpotlightCli.exe" --version
& "T:\VestigantSpotlightInv_V0_9_34\build-msvc\Release\VestigantSpotlightTests.exe" "T:\VestigantSpotlightInv_V0_9_34\build-msvc\selftest_out"
```
