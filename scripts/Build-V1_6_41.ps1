param(
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V1_6_41.zip",
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_41",
  [string]$BuildLog = "D:\Downloads\V1_6_41_build.log",
  [switch]$CleanExtract
)

$ErrorActionPreference = "Stop"

if (!(Test-Path -LiteralPath $SourceRoot)) {
  if (!(Test-Path -LiteralPath $ZipPath)) { throw "SourceRoot and source ZIP not found. SourceRoot=$SourceRoot ZIP=$ZipPath" }
  Set-Location D:\Downloads
  Get-FileHash $ZipPath -Algorithm SHA256 | Format-List
  Expand-Archive -LiteralPath $ZipPath -DestinationPath "T:\" -Force
} elseif ($CleanExtract) {
  if (!(Test-Path -LiteralPath $ZipPath)) { throw "Source ZIP not found for CleanExtract: $ZipPath" }
  Set-Location D:\Downloads
  Get-FileHash $ZipPath -Algorithm SHA256 | Format-List
  Remove-Item -LiteralPath $SourceRoot -Recurse -Force -ErrorAction SilentlyContinue
  Expand-Archive -LiteralPath $ZipPath -DestinationPath "T:\" -Force
}

$VersionFile = Join-Path $SourceRoot "VERSION"
if (!(Test-Path -LiteralPath $VersionFile)) { throw "VERSION file not found: $VersionFile" }
$ExpectedVersion = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
if ([string]::IsNullOrWhiteSpace($ExpectedVersion)) { throw "VERSION file is empty: $VersionFile" }
$ExpectedVersionRegex = [regex]::Escape($ExpectedVersion)

function Invoke-CheckedPreflightScript {
  param([string]$ScriptPath, [string]$Name)
  if (Test-Path -LiteralPath $ScriptPath) {
    powershell -ExecutionPolicy Bypass -File $ScriptPath -SourceRoot $SourceRoot
    if ($LASTEXITCODE -ne 0) { throw "$Name failed with exit code ${LASTEXITCODE}: $ScriptPath" }
  }
}

function Invoke-AdvisoryPreflightScript {
  param([string]$ScriptPath, [string]$Name)
  if (Test-Path -LiteralPath $ScriptPath) {
    powershell -ExecutionPolicy Bypass -File $ScriptPath -SourceRoot $SourceRoot
    if ($LASTEXITCODE -ne 0) {
      Write-Warning "$Name returned exit code ${LASTEXITCODE}; continuing to MSVC build because this check is advisory. Script: $ScriptPath"
    }
  }
}

$WrapperCompat = Join-Path $SourceRoot "tools\Verify-PowerShellWrapperCompatibility.ps1"
Invoke-CheckedPreflightScript -ScriptPath $WrapperCompat -Name "PowerShell wrapper compatibility check"

$MsvcStringCheck = Join-Path $SourceRoot "tools\Verify-MsvcStringLiteralRisk.ps1"
Invoke-CheckedPreflightScript -ScriptPath $MsvcStringCheck -Name "MSVC string literal risk check"

# Release-readiness includes documentation and packaging assertions. It is useful, but must not block compilation.
$ReleaseReadiness = Join-Path $SourceRoot "tools\Verify-V1_6_41-ReleaseReadiness.ps1"
Invoke-AdvisoryPreflightScript -ScriptPath $ReleaseReadiness -Name "Release readiness advisory check"

if (!(Test-Path -LiteralPath "$SourceRoot\build_windows_msvc.bat")) { throw "Build script not found: $SourceRoot\build_windows_msvc.bat" }

Remove-Item -LiteralPath "$SourceRoot\build-msvc\obj\win32_gui.obj" -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$SourceRoot\build-msvc\obj\gui_export_worker.obj" -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$SourceRoot\build-msvc\obj\app_info.obj" -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$SourceRoot\build-msvc\Release\VestigantSpotlight.exe" -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$SourceRoot\build-msvc\Release\VestigantSpotlightCli.exe" -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath "$SourceRoot\build-msvc\Release\VestigantSpotlightTests.exe" -Force -ErrorAction SilentlyContinue

& "$SourceRoot\build_windows_msvc.bat" 2>&1 | Tee-Object -FilePath $BuildLog
$buildExitCode = $LASTEXITCODE
$cliExe = "$SourceRoot\build-msvc\Release\VestigantSpotlightCli.exe"
if ($buildExitCode -ne 0) { throw "Build failed with exit code $buildExitCode. Log: $BuildLog" }
if (!(Test-Path -LiteralPath $cliExe)) { throw "Build did not produce CLI executable: $cliExe. Check compile/link errors in log: $BuildLog" }
if (Select-String -LiteralPath $BuildLog -Pattern ': error ', ' error C', 'fatal error' -Quiet) { throw "Build log contains compiler/linker errors. Log: $BuildLog" }

$version = (& $cliExe --version 2>&1 | Out-String).Trim()
if ($version -notmatch $ExpectedVersionRegex) { throw "Unexpected CLI version after build. Expected $ExpectedVersion but got: $version" }
Write-Host $version
Write-Host "Build log: $BuildLog"
