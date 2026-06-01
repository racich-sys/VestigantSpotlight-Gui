@echo off
REM Compatibility wrapper. The primary build script is already no-CMake.
call "%~dp0build_windows_msvc.bat" %*
exit /b %errorlevel%
