param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_6_119",
  [string]$InputZipOrFolder = "F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip",
  [string]$CaseRootBase = "Q:\SpotlightCase\iOS_TestMatrix_V1_6_119",
  [string]$MatrixWorkRoot = "D:\Downloads\iOS_TestMatrix_V1_6_119_Work",
  [string]$BundlePath = "D:\Downloads\Upload_iOS_TestMatrix_V1_6_119.zip",
  [int]$MaxNativeRecords = 25000,
  [switch]$CleanOut,
  [switch]$NoClipboardOrExplorer,
  [switch]$SkipThin,
  [switch]$SkipAppDbSpotlight,
  [switch]$IncludeValidationSupport,
  [switch]$IncludeBoundedFullNative,
  [switch]$IncludeProduction,
  [string]$ExternalSourceSha256 = "",
  [string]$ExternalSourceHashNote = "",
  [string]$ReuseIosCache = ""
)

$ErrorActionPreference = "Stop"
if (!(Test-Path -LiteralPath $SourceRoot -PathType Container)) { throw "SourceRoot not found: $SourceRoot" }
if (!(Test-Path -LiteralPath $InputZipOrFolder)) { throw "InputZipOrFolder not found: $InputZipOrFolder" }

$scriptDir = Join-Path $SourceRoot "scripts"
$validator = Join-Path $SourceRoot "tools\Verify-iOSSpotlightValidationOutputs.ps1"
if (!(Test-Path -LiteralPath $validator -PathType Leaf)) { throw "iOS validation-output verifier not found: $validator" }

if (Test-Path -LiteralPath $MatrixWorkRoot) { Remove-Item -LiteralPath $MatrixWorkRoot -Recurse -Force }
New-Item -ItemType Directory -Path $MatrixWorkRoot -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $MatrixWorkRoot "zips") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $MatrixWorkRoot "logs") -Force | Out-Null

$statusRows = New-Object System.Collections.Generic.List[object]

function Invoke-IosMatrixStep {
  param(
    [Parameter(Mandatory=$true)][string]$StepName,
    [Parameter(Mandatory=$true)][string]$ScriptName,
    [Parameter(Mandatory=$true)][string]$CaseRoot,
    [Parameter(Mandatory=$true)][string]$ZipPath,
    [switch]$RequireAppDbSpotlightExports,
    [hashtable]$ExtraArgs = @{}
  )

  $scriptPath = Join-Path $script:scriptDir $ScriptName
  if (!(Test-Path -LiteralPath $scriptPath -PathType Leaf)) { throw "Matrix step script not found: $scriptPath" }
  $logPath = Join-Path (Join-Path $script:MatrixWorkRoot "logs") ($StepName + ".log")
  $argList = @(
    "-ExecutionPolicy", "Bypass", "-File", $scriptPath,
    "-SourceRoot", $script:SourceRoot,
    "-InputZipOrFolder", $script:InputZipOrFolder,
    "-CaseRoot", $CaseRoot,
    "-ZipPath", $ZipPath,
    "-NoClipboardOrExplorer"
  )
  if ($script:CleanOut) { $argList += "-CleanOut" }
  if (![string]::IsNullOrWhiteSpace($script:ExternalSourceSha256)) { $argList += @("-ExternalSourceSha256", $script:ExternalSourceSha256) }
  if (![string]::IsNullOrWhiteSpace($script:ExternalSourceHashNote)) { $argList += @("-ExternalSourceHashNote", $script:ExternalSourceHashNote) }
  if (![string]::IsNullOrWhiteSpace($script:ReuseIosCache)) { $argList += @("-ReuseIosCache", $script:ReuseIosCache) }
  foreach ($key in $ExtraArgs.Keys) {
    $value = $ExtraArgs[$key]
    if ($value -is [switch] -or $value -is [bool]) {
      if ($value) { $argList += ("-" + $key) }
    } elseif ($null -ne $value -and ![string]::IsNullOrWhiteSpace([string]$value)) {
      $argList += @(("-" + $key), [string]$value)
    }
  }

  Write-Host "Running iOS matrix step: $StepName"
  Write-Host "Command: powershell $($argList -join ' ')"
  powershell @argList *> $logPath
  $exitCode = $LASTEXITCODE
  $zipExists = Test-Path -LiteralPath $ZipPath -PathType Leaf
  $verifyExit = $null
  if ($zipExists) {
    $verifyArgs = @("-ExecutionPolicy", "Bypass", "-File", $script:validator, "-ZipPath", $ZipPath)
    if ($RequireAppDbSpotlightExports) { $verifyArgs += "-RequireAppDbSpotlightExports" }
    if ($exitCode -ne 0) { $verifyArgs += "-AllowIncompleteRun" }
    powershell @verifyArgs >> $logPath 2>&1
    $verifyExit = $LASTEXITCODE
    Copy-Item -LiteralPath $ZipPath -Destination (Join-Path (Join-Path $script:MatrixWorkRoot "zips") (Split-Path -Leaf $ZipPath)) -Force
  }

  $script:statusRows.Add([pscustomobject]@{
    StepName = $StepName
    ScriptName = $ScriptName
    CaseRoot = $CaseRoot
    ZipPath = $ZipPath
    LogPath = $logPath
    RunnerExitCode = $exitCode
    ZipExists = $zipExists
    VerifyExitCode = $verifyExit
    RequireAppDbSpotlightExports = $RequireAppDbSpotlightExports.IsPresent
  }) | Out-Null
}

if (!$SkipThin) {
  Invoke-IosMatrixStep -StepName "01_CoreSpotlightThin" -ScriptName "Run-V1_6_119-iOS-CoreSpotlight-AndZip.ps1" -CaseRoot (Join-Path $CaseRootBase "01_CoreSpotlightThin") -ZipPath "D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_119.zip"
}

if (!$SkipAppDbSpotlight) {
  Invoke-IosMatrixStep -StepName "02_AppDbSpotlight" -ScriptName "Run-V1_6_119-iOS-AppDbSpotlight-AndZip.ps1" -CaseRoot (Join-Path $CaseRootBase "02_AppDbSpotlight") -ZipPath "D:\Downloads\Upload_iOS_AppDbSpotlight_V1_6_119.zip" -RequireAppDbSpotlightExports -ExtraArgs @{ MaxNativeRecords = $MaxNativeRecords }
}

if ($IncludeValidationSupport) {
  Invoke-IosMatrixStep -StepName "03_ValidationSupport" -ScriptName "Run-V1_6_119-iOS-ValidationSupport-AndZip.ps1" -CaseRoot (Join-Path $CaseRootBase "03_ValidationSupport") -ZipPath "D:\Downloads\Upload_ValidationSupport_iOS_CoreSpotlight_V1_6_119.zip" -RequireAppDbSpotlightExports
}

if ($IncludeBoundedFullNative) {
  Invoke-IosMatrixStep -StepName "04_BoundedFullNativeDb" -ScriptName "Run-V1_6_119-iOS-BoundedFullNativeDb-AndZip.ps1" -CaseRoot (Join-Path $CaseRootBase "04_BoundedFullNativeDb") -ZipPath "D:\Downloads\Upload_BoundedFullNative_iOS_CoreSpotlight_V1_6_119.zip" -RequireAppDbSpotlightExports -ExtraArgs @{ MaxNativeRecords = $MaxNativeRecords }
}

if ($IncludeProduction) {
  Invoke-IosMatrixStep -StepName "05_Production" -ScriptName "Run-V1_6_119-iOS-Production-AndZip.ps1" -CaseRoot (Join-Path $CaseRootBase "05_Production") -ZipPath "D:\Downloads\Upload_Production_iOS_CoreSpotlight_V1_6_119.zip" -RequireAppDbSpotlightExports -ExtraArgs @{ MaterializeIosSupportDb = $true }
}

$statusCsv = Join-Path $MatrixWorkRoot "ios_test_matrix_status.csv"
$statusRows | Export-Csv -NoTypeInformation -Encoding UTF8 $statusCsv

@(
  "Vestigant Spotlight V1.6.119 iOS test matrix",
  "Created: $((Get-Date).ToString('o'))",
  "SourceRoot: $SourceRoot",
  "InputZipOrFolder: $InputZipOrFolder",
  "CaseRootBase: $CaseRootBase",
  "MaxNativeRecords: $MaxNativeRecords",
  "CleanOut: $($CleanOut.IsPresent)",
  "IncludedSteps: $($statusRows.StepName -join ', ')",
  "",
  "Default purpose:",
  "1. CoreSpotlight thin sanity run.",
  "2. App DB Spotlight/search eligibility bounded validation run.",
  "",
  "Upload this bundle plus any step ZIPs requested during review."
) | Set-Content -LiteralPath (Join-Path $MatrixWorkRoot "IOS_TEST_MATRIX_README.txt") -Encoding UTF8

if (Test-Path -LiteralPath $BundlePath) { Remove-Item -LiteralPath $BundlePath -Force }
Compress-Archive -Path (Join-Path $MatrixWorkRoot "*") -DestinationPath $BundlePath -Force
Get-Item -LiteralPath $BundlePath | Select-Object FullName, Length
Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256

$failed = @($statusRows | Where-Object { $_.RunnerExitCode -ne 0 -or $_.ZipExists -ne $true -or ($_.VerifyExitCode -ne $null -and $_.VerifyExitCode -ne 0) })
if ($failed.Count -gt 0) {
  Write-Warning "One or more iOS matrix steps failed or produced incomplete outputs. Upload bundle for review: $BundlePath"
  $failed | Format-Table -AutoSize | Out-String | Write-Host
  exit 1
}

if (!$NoClipboardOrExplorer) {
  try { Set-Clipboard -Value $BundlePath } catch {}
  try { explorer.exe /select,"$BundlePath" | Out-Null } catch {}
}
Write-Host "iOS test matrix completed: $BundlePath"
