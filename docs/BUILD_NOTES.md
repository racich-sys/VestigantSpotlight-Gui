# Build Notes - v0.3.5

From Developer PowerShell or ordinary PowerShell with Visual Studio 2022 installed:

```powershell
cd T:\VestigantSpotlightInv_V0_3_2
.\build_windows_msvc.bat
```

The script does not require CMake. It locates Visual Studio 2022 Community/Professional/Enterprise/BuildTools and compiles directly with `cl.exe`.

Required Visual Studio components:

```text
Desktop development with C++
MSVC v143 x64/x86 build tools
Windows 10 or Windows 11 SDK
```

Runtime database dependency uses Windows `winsqlite3.dll` via `winsqlite3.lib`.
