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
REM V0_7_23_6 changes:
REM - Date-association hardening compile fix; stops immediately on common object compile failure
REM   - Preserves working MSVC discovery, response-file linking, and GDI GUI linking.
REM   - Adds GUI progress-bar support and focused upload-profile defaults.
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
set "COMMON_RSP=%OBJ%\common_objects.rsp"
set "CLI_RSP=%OBJ%\cli_link.rsp"
set "TESTS_RSP=%OBJ%\tests_link.rsp"
set "GUI_RSP=%OBJ%\gui_link.rsp"

if exist "%OUT%\VestigantSpotlightCli.exe" del /q "%OUT%\VestigantSpotlightCli.exe" >nul 2>nul
if exist "%OUT%\VestigantSpotlightTests.exe" del /q "%OUT%\VestigantSpotlightTests.exe" >nul 2>nul
if exist "%OUT%\VestigantSpotlight.exe" del /q "%OUT%\VestigantSpotlight.exe" >nul 2>nul
if exist "%COMMON_RSP%" del /q "%COMMON_RSP%" >nul 2>nul

call :CompileCommon "src\core\app_info.cpp" "app_info.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\core\csv.cpp" "csv.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\core\hash.cpp" "hash.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\core\logger.cpp" "logger.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\core\path_utils.cpp" "path_utils.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\db\case_db.cpp" "case_db.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\ingest\source_profiles.cpp" "source_profiles.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\ingest\store_discovery.cpp" "store_discovery.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\ingest\evidence_preservation.cpp" "evidence_preservation.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\parsers\native_storedb_parser.cpp" "native_storedb_parser.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommonQuiet "src\parsers\v7_output_importer.cpp" "legacy_importer.obj" "legacy compatibility component"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\enrich_sql\sqlite_enrichment.cpp" "sqlite_enrichment.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\export_sql\sqlite_exporter.cpp" "sqlite_exporter.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\app\case_store.cpp" "case_store.obj"
if errorlevel 1 exit /b !errorlevel!
call :CompileCommon "src\app\app_runner.cpp" "app_runner.obj"
if errorlevel 1 exit /b !errorlevel!

call :RequireObject "case_db.obj"
call :RequireObject "case_store.obj"
call :RequireObject "app_runner.obj"

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
echo Compiling GUI entry point...
cl %CXXFLAGS% /c "src\gui\win32_gui.cpp" /Fo:"%OBJ%\win32_gui.obj"
if errorlevel 1 exit /b !errorlevel!

(
  type "%COMMON_RSP%"
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
echo Build complete:
echo   %OUT%\VestigantSpotlight.exe
echo   %OUT%\VestigantSpotlightCli.exe
echo   %OUT%\VestigantSpotlightTests.exe
echo.
echo Optional self-test:
echo   "%OUT%\VestigantSpotlightTests.exe" "%CD%\build-msvc\selftest_out"
exit /b 0

:CompileCommon
echo.
echo Compiling %~1...
cl %CXXFLAGS% /c "%~1" /Fo:"%OBJ%\%~2"
if errorlevel 1 exit /b !errorlevel!
echo "%OBJ%\%~2" >> "%COMMON_RSP%"
exit /b 0

:CompileCommonQuiet
echo.
echo Compiling %~3...
cl %CXXFLAGS% /c "%~1" /Fo:"%OBJ%\%~2"
if errorlevel 1 exit /b !errorlevel!
echo "%OBJ%\%~2" >> "%COMMON_RSP%"
exit /b 0

:RequireObject
if not exist "%OBJ%\%~1" (
  echo ERROR: Required object is missing: %OBJ%\%~1
  exit /b 1
)
exit /b 0

