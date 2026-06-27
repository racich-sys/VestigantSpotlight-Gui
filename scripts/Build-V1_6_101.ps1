param(
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V1_6_101.zip",
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_101",
  [string]$BuildLog = "D:\Downloads\V1_6_101_build.log",
  [switch]$CleanExtract,
  [switch]$SkipSelfTest,
  [switch]$ForcePackageHash
)

$ErrorActionPreference = "Stop"
# Release marker regex: 1\.6\.89


if (!(Test-Path -LiteralPath $SourceRoot)) {
  if (!(Test-Path -LiteralPath $ZipPath)) { throw "SourceRoot and source ZIP not found. SourceRoot=$SourceRoot ZIP=$ZipPath" }
  Set-Location D:\Downloads
  if ($ForcePackageHash) {
    Write-Host "Downloaded source ZIP SHA256 requested by -ForcePackageHash:"
    Get-FileHash $ZipPath -Algorithm SHA256 | Format-List
  } else {
    Write-Host "Thin/test mode: source ZIP hash skipped during build. Use -ForcePackageHash only when specifically needed."
  }
  Expand-Archive -LiteralPath $ZipPath -DestinationPath "T:\" -Force
} elseif ($CleanExtract) {
  if (!(Test-Path -LiteralPath $ZipPath)) { throw "Source ZIP not found for CleanExtract: $ZipPath" }
  Set-Location D:\Downloads
  if ($ForcePackageHash) {
    Write-Host "Downloaded source ZIP SHA256 requested by -ForcePackageHash:"
    Get-FileHash $ZipPath -Algorithm SHA256 | Format-List
  } else {
    Write-Host "Thin/test mode: source ZIP hash skipped during build. Use -ForcePackageHash only when specifically needed."
  }
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

function Invoke-SelfTestProcessToLog {
  param([Parameter(Mandatory=$true)][string]$ExePath, [Parameter(Mandatory=$true)][string]$OutputDirectory, [Parameter(Mandatory=$true)][string]$LogPath)
  # Use System.Diagnostics.Process instead of `& exe 2>&1 | Tee-Object` so Windows PowerShell 5.1 does not
  # convert native stderr warning lines into terminating NativeCommandError records when ErrorActionPreference=Stop.
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $ExePath
  $psi.Arguments = '"' + $OutputDirectory.Replace('"', '\"') + '"'
  $psi.WorkingDirectory = Split-Path -Parent $ExePath
  $psi.UseShellExecute = $false
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true
  $psi.CreateNoWindow = $true
  $p = New-Object System.Diagnostics.Process
  $p.StartInfo = $psi
  [void]$p.Start()
  $stdout = $p.StandardOutput.ReadToEnd()
  $stderr = $p.StandardError.ReadToEnd()
  $p.WaitForExit()
  if (![string]::IsNullOrEmpty($stdout)) {
    Write-Host $stdout.TrimEnd()
    $stdout | Add-Content -LiteralPath $LogPath -Encoding UTF8
  }
  if (![string]::IsNullOrEmpty($stderr)) {
    # Self-test warnings are useful evidence and must be logged, but stderr alone is not a failure.
    Write-Warning $stderr.TrimEnd()
    $stderr | Add-Content -LiteralPath $LogPath -Encoding UTF8
  }
  return [int]$p.ExitCode
}

$WrapperCompat = Join-Path $SourceRoot "tools\Verify-PowerShellWrapperCompatibility.ps1"
Invoke-CheckedPreflightScript -ScriptPath $WrapperCompat -Name "PowerShell wrapper compatibility check"

$MsvcStringCheck = Join-Path $SourceRoot "tools\Verify-MsvcStringLiteralRisk.ps1"
Invoke-CheckedPreflightScript -ScriptPath $MsvcStringCheck -Name "MSVC string literal risk check"

# Release-readiness includes documentation and packaging assertions. It is useful, but must not block compilation.
$ReleaseReadiness = Join-Path $SourceRoot "tools\Verify-V1_6_101-ReleaseReadiness.ps1"
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

$testsExe = "$SourceRoot\build-msvc\Release\VestigantSpotlightTests.exe"
if (!$SkipSelfTest) {
  if (!(Test-Path -LiteralPath $testsExe)) { throw "Build did not produce self-test executable: $testsExe" }
  $selfTestOut = "$SourceRoot\build-msvc\selftest_out"
  if (Test-Path -LiteralPath $selfTestOut) { Remove-Item -LiteralPath $selfTestOut -Recurse -Force -ErrorAction SilentlyContinue }
  New-Item -ItemType Directory -Path $selfTestOut -Force | Out-Null
  Write-Host "Running required self-test: $testsExe $selfTestOut"
  $selfTestExitCode = Invoke-SelfTestProcessToLog -ExePath $testsExe -OutputDirectory $selfTestOut -LogPath $BuildLog
  if ($selfTestExitCode -ne 0) { throw "Self-test failed with exit code $selfTestExitCode. Log: $BuildLog" }
  Write-Host "Self-test completed. Output folder: $selfTestOut"
} else {
  Write-Warning "Skipping self-test by request: -SkipSelfTest"
}

Write-Host "Build log: $BuildLog"
