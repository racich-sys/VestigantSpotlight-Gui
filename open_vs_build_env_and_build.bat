@echo off
REM Compatibility wrapper. build_windows_msvc.bat auto-detects Visual Studio and does not require CMake.
call "%~dp0build_windows_msvc.bat" %*
exit /b %errorlevel%
