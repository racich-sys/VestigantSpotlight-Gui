@echo off
setlocal EnableExtensions EnableDelayedExpansion
set "ROOT=%~dp0"
cd /d "%ROOT%"

set /p SOURCE_VERSION=<VERSION.txt
echo Building VestigantSpotlight source version %SOURCE_VERSION%

if not exist "src\cli\main.cpp" (
  echo ERROR: This script must be run from the VestigantSpotlight project root.
  echo Current directory: %CD%
  exit /b 1
)

REM ---------------------------------------------------------------------------
REM VestigantSpotlight no-CMake MSVC build - response-file object build.
REM Current no-CMake MSVC build script.  Keep this script version-neutral:
REM it reads VERSION.txt and builds common objects once, then links CLI, tests, and GUI.
REM ---------------------------------------------------------------------------

where cl.exe >nul 2>nul
if errorlevel 1 (
  echo MSVC cl.exe is not currently in PATH. Trying Visual Studio discovery...

  set "VSINSTALL="
  set "VSWHERE=!ProgramFiles(x86)!\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do if not defined VSINSTALL set "VSINSTALL=%%i"
    if not defined VSINSTALL (
      for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -property installationPath`) do if not defined VSINSTALL set "VSINSTALL=%%i"
    )
  )

  if not defined VSINSTALL if exist "!ProgramFiles!\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VSINSTALL=!ProgramFiles!\Microsoft Visual Studio\2022\Community"
  if not defined VSINSTALL if exist "!ProgramFiles!\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VSINSTALL=!ProgramFiles!\Microsoft Visual Studio\2022\Professional"
  if not defined VSINSTALL if exist "!ProgramFiles!\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VSINSTALL=!ProgramFiles!\Microsoft Visual Studio\2022\Enterprise"
  if not defined VSINSTALL if exist "!ProgramFiles!\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VSINSTALL=!ProgramFiles!\Microsoft Visual Studio\2022\BuildTools"
  if not defined VSINSTALL if exist "!ProgramFiles!\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat" set "VSINSTALL=!ProgramFiles!\Microsoft Visual Studio\2022\Preview"
  if not defined VSINSTALL if exist "!ProgramFiles(x86)!\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VSINSTALL=!ProgramFiles(x86)!\Microsoft Visual Studio\2022\Community"
  if not defined VSINSTALL if exist "!ProgramFiles(x86)!\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VSINSTALL=!ProgramFiles(x86)!\Microsoft Visual Studio\2022\Professional"
  if not defined VSINSTALL if exist "!ProgramFiles(x86)!\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VSINSTALL=!ProgramFiles(x86)!\Microsoft Visual Studio\2022\Enterprise"
  if not defined VSINSTALL if exist "!ProgramFiles(x86)!\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VSINSTALL=!ProgramFiles(x86)!\Microsoft Visual Studio\2022\BuildTools"

  if not defined VSINSTALL (
    echo ERROR: Visual Studio was not found in the expected locations.
    echo Install Visual Studio 2022 with "Desktop development with C++".
    exit /b 1
  )

  echo Found Visual Studio: !VSINSTALL!

  if exist "!VSINSTALL!\Common7\Tools\VsDevCmd.bat" (
    call "!VSINSTALL!\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
    if errorlevel 1 exit /b !errorlevel!
  ) else if exist "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" (
    call "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat"
    if errorlevel 1 exit /b !errorlevel!
  ) else (
    echo ERROR: Visual Studio was found at: !VSINSTALL!
    echo but neither VsDevCmd.bat nor vcvars64.bat was found.
    echo Add the Visual Studio workload: Desktop development with C++.
    exit /b 1
  )
)

where cl.exe >nul 2>nul
if errorlevel 1 (
  echo ERROR: cl.exe is still not available after loading Visual Studio environment.
  echo This usually means Visual Studio C++ tools are not installed.
  exit /b 1
)

set "OUT=build-msvc\Release"
set "OBJ=build-msvc\obj"
if not exist "build-msvc" mkdir "build-msvc"
if not exist "%OUT%" mkdir "%OUT%"
if exist "%OBJ%" rmdir /s /q "%OBJ%"
mkdir "%OBJ%"

set "CXXFLAGS=/nologo /std:c++20 /EHa /W4 /permissive- /DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE /utf-8 /I src"
set "LZFSE_ENABLED=0"
if exist "third_party\lzfse\src\lzfse.h" (
  set "LZFSE_ENABLED=1"
  set "CXXFLAGS=!CXXFLAGS! /DVESTIGANT_HAS_LZFSE=1 /I third_party\lzfse\src"
  echo Apple lzfse source detected: enabling VESTIGANT_HAS_LZFSE.
) else (
  echo Apple lzfse source not vendored: LZFSE/LZVN decode remains disabled.
)
set "COMMON_RSP=%OBJ%\common_objects.rsp"
set "CLI_RSP=%OBJ%\cli_link.rsp"
set "TESTS_RSP=%OBJ%\tests_link.rsp"
set "GUI_RSP=%OBJ%\gui_link.rsp"

if exist "%OUT%\VestigantSpotlightCli.exe" del /q "%OUT%\VestigantSpotlightCli.exe" >nul 2>nul
if exist "%OUT%\VestigantSpotlightTests.exe" del /q "%OUT%\VestigantSpotlightTests.exe" >nul 2>nul
if exist "%OUT%\VestigantSpotlight.exe" del /q "%OUT%\VestigantSpotlight.exe" >nul 2>nul
if exist "%COMMON_RSP%" del /q "%COMMON_RSP%" >nul 2>nul

REM ---------------------------------------------------------------------------
REM Compile common objects without CALL labels.  V0_9_57 avoids batch subroutine
REM labels because Windows CMD can intermittently fail with "cannot find the
REM batch label specified" when line endings or label-cache state are damaged.
REM ---------------------------------------------------------------------------
(
  echo src\core\app_info.cpp^|app_info.obj
  echo src\core\csv.cpp^|csv.obj
  echo src\core\hash.cpp^|hash.obj
  echo src\core\logger.cpp^|logger.obj
  echo src\core\path_utils.cpp^|path_utils.obj
  echo src\db\case_db.cpp^|case_db.obj
  echo src\ingest\source_profiles.cpp^|source_profiles.obj
  echo src\ingest\store_discovery.cpp^|store_discovery.obj
  echo src\ingest\evidence_preservation.cpp^|evidence_preservation.obj
  echo src\parsers\native_storedb_parser.cpp^|native_storedb_parser.obj
  echo src\parsers\ios_app_db_parser.cpp^|ios_app_db_parser.obj
  echo src\parsers\apfs_volume_reader.cpp^|apfs_volume_reader.obj
  echo src\parsers\apfs_aff4_reader.cpp^|apfs_aff4_reader.obj
  echo src\codec\lzfse_codec.cpp^|lzfse_codec.obj
  echo src\enrich_sql\sqlite_enrichment.cpp^|sqlite_enrichment.obj
  echo src\export_sql\sqlite_exporter.cpp^|sqlite_exporter.obj
  echo src\app\case_store.cpp^|case_store.obj
  echo src\app\app_runner.cpp^|app_runner.obj
) > "%OBJ%\common_compile_manifest.txt"

type nul > "%COMMON_RSP%"
for /f "usebackq tokens=1,2 delims=|" %%A in ("%OBJ%\common_compile_manifest.txt") do (
  echo.
  echo Compiling %%A...
  cl %CXXFLAGS% /c "%%A" /Fo:"%OBJ%\%%B"
  if errorlevel 1 exit /b 1
  if not exist "%OBJ%\%%B" (
    echo ERROR: Expected object was not created: %OBJ%\%%B
    exit /b 1
  )
  echo "%OBJ%\%%B" >> "%COMMON_RSP%"
)

if "%LZFSE_ENABLED%"=="1" (
  for %%F in (lzfse_decode.c lzfse_decode_base.c lzfse_fse.c lzvn_decode_base.c) do (
    echo.
    echo Compiling third_party\lzfse\src\%%F...
    cl /nologo /TC /W0 /DWIN32 /D_WINDOWS /I third_party\lzfse\src /c "third_party\lzfse\src\%%F" /Fo:"%OBJ%\%%~nF.obj"
    if errorlevel 1 exit /b 1
    echo "%OBJ%\%%~nF.obj" >> "%COMMON_RSP%"
  )
)

if not exist "%OBJ%\case_db.obj" (
  echo ERROR: Required object is missing: %OBJ%\case_db.obj
  exit /b 1
)
if not exist "%OBJ%\case_store.obj" (
  echo ERROR: Required object is missing: %OBJ%\case_store.obj
  exit /b 1
)
if not exist "%OBJ%\app_runner.obj" (
  echo ERROR: Required object is missing: %OBJ%\app_runner.obj
  exit /b 1
)


if not exist "%COMMON_RSP%" (
  echo ERROR: common object response file was not created: %COMMON_RSP%
  exit /b 1
)

echo.
echo Common linker response file: %COMMON_RSP%
for /f %%C in ('find /c /v "" ^< "%COMMON_RSP%"') do echo Common object count: %%C

echo.
echo Compiling CLI entry point...
cl %CXXFLAGS% /c "src\cli\main.cpp" /Fo:"%OBJ%\cli_main.obj"
if errorlevel 1 exit /b !errorlevel!

(
  type "%COMMON_RSP%"
  echo "%OBJ%\cli_main.obj"
  echo winsqlite3.lib
) > "%CLI_RSP%"

echo Linking VestigantSpotlightCli.exe...
link /nologo /OUT:"%OUT%\VestigantSpotlightCli.exe" /SUBSYSTEM:CONSOLE @"%CLI_RSP%"
if errorlevel 1 exit /b !errorlevel!

echo.
echo Compiling self-test entry point...
cl %CXXFLAGS% /c "tests\main.cpp" /Fo:"%OBJ%\tests_main.obj"
if errorlevel 1 exit /b !errorlevel!

(
  type "%COMMON_RSP%"
  echo "%OBJ%\tests_main.obj"
  echo winsqlite3.lib
) > "%TESTS_RSP%"

echo Linking VestigantSpotlightTests.exe...
link /nologo /OUT:"%OUT%\VestigantSpotlightTests.exe" /SUBSYSTEM:CONSOLE @"%TESTS_RSP%"
if errorlevel 1 exit /b !errorlevel!

echo.
echo Compiling GUI view registry...
cl %CXXFLAGS% /c "src\gui\view_registry.cpp" /Fo:"%OBJ%\view_registry.obj"
if errorlevel 1 exit /b !errorlevel!

echo Compiling GUI entry point...
cl %CXXFLAGS% /c "src\gui\win32_gui.cpp" /Fo:"%OBJ%\win32_gui.obj"
if errorlevel 1 exit /b !errorlevel!

(
  type "%COMMON_RSP%"
  echo "%OBJ%\view_registry.obj"
  echo "%OBJ%\win32_gui.obj"
  echo comctl32.lib
  echo shell32.lib
  echo ole32.lib
  echo user32.lib
  echo comdlg32.lib
  echo gdi32.lib
  echo winsqlite3.lib
) > "%GUI_RSP%"

echo Linking VestigantSpotlight.exe...
link /nologo /OUT:"%OUT%\VestigantSpotlight.exe" /SUBSYSTEM:WINDOWS @"%GUI_RSP%"
if errorlevel 1 exit /b !errorlevel!

echo.
echo Built binary version check:
"%OUT%\VestigantSpotlightCli.exe" --version
if errorlevel 1 exit /b !errorlevel!

echo.
if exist "resources" (
  if not exist "%OUT%\resources" mkdir "%OUT%\resources"
  copy /Y "resources\vestigant_logo.bmp" "%OUT%\resources\vestigant_logo.bmp" >nul 2>nul
  copy /Y "resources\vestigant_logo.jpg" "%OUT%\resources\vestigant_logo.jpg" >nul 2>nul
)

echo Build complete:
echo   %OUT%\VestigantSpotlight.exe
echo   %OUT%\VestigantSpotlightCli.exe
echo   %OUT%\VestigantSpotlightTests.exe
echo.
echo Optional self-test:
echo   "%OUT%\VestigantSpotlightTests.exe" "%CD%\build-msvc\selftest_out"
exit /b 0

