param(
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V1_6_102.zip",
  [string]$DestinationRoot = "T:\",
  [string]$BuildLog = "D:\Downloads\V1_6_102_build.log",
  [Parameter(Mandatory=$true)][string]$Aff4Path,
  [string]$CaseRoot = "Q:\SpotlightCase\TestMacOS_AFF4_V1_6_102",
  [string]$ReaderToolsRoot = "",
  [string]$Aff4ZipPath = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_6_102.zip",
  [switch]$CleanOut,
  [switch]$FullNoGuardrails,
  [switch]$PressureTestMode,
  [switch]$EnableStreamInventory,
  [switch]$FullNativeValues,
  [int]$MaxNativeRecords = 0,
  [int]$MaxNativeBlocks = 0,
  [int]$CliTimeoutMinutes = 180
)

$ErrorActionPreference = "Stop"

function Assert-Leaf {
  param([Parameter(Mandatory=$true)][string]$PathValue, [Parameter(Mandatory=$true)][string]$Description)
  if (!(Test-Path -LiteralPath $PathValue -PathType Leaf)) { throw "$Description not found: $PathValue" }
}

function Assert-DirectoryOrBlank {
  param([string]$PathValue, [string]$Description)
  if (![string]::IsNullOrWhiteSpace($PathValue) -and !(Test-Path -LiteralPath $PathValue -PathType Container)) { throw "$Description not found: $PathValue" }
}

Assert-Leaf -PathValue $ZipPath -Description "Source ZIP"
Assert-Leaf -PathValue $Aff4Path -Description "AFF4 input"
Assert-DirectoryOrBlank -PathValue $ReaderToolsRoot -Description "Reader tools folder"

$SourceRoot = Join-Path $DestinationRoot "VestigantSpotlightInv_V1_6_102"
if (Test-Path -LiteralPath $SourceRoot) {
  Remove-Item -LiteralPath $SourceRoot -Recurse -Force
}
Expand-Archive -LiteralPath $ZipPath -DestinationPath $DestinationRoot -Force

$BuildScript = Join-Path $SourceRoot "scripts\Build-V1_6_102.ps1"
$Runner = Join-Path $SourceRoot "tools\Run-SingleAff4SourceProbeAndZip.ps1"
Assert-Leaf -PathValue $BuildScript -Description "Build script"
Assert-Leaf -PathValue $Runner -Description "AFF4 source-probe runner"

powershell -ExecutionPolicy Bypass -File $BuildScript -SourceRoot $SourceRoot -BuildLog $BuildLog
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE. Review: $BuildLog" }

$runnerArgs = @(
  "-Aff4Input", $Aff4Path,
  "-Out", $CaseRoot,
  "-ZipPath", $Aff4ZipPath,
  "-IncludeLogsTailOnly",
  "-CliTimeoutMinutes", $CliTimeoutMinutes,
  "-NoClipboardOrExplorer"
)
if ($CleanOut) { $runnerArgs += "-CleanOut" }
if (![string]::IsNullOrWhiteSpace($ReaderToolsRoot)) { $runnerArgs += @("-ReaderToolsRoot", $ReaderToolsRoot) }
if ($FullNoGuardrails) {
  $runnerArgs += "-FullScan"
  $runnerArgs += "-EnableAff4DynamicProbe"
  $runnerArgs += "-EnableAff4VirtualApfsProbe"
  $runnerArgs += "-DiagnosticOutputs"
}
# Thin/trial runs do not hash source containers; full validation must use a separate confirmed hash workflow.
$runnerArgs += "-SkipContainerHash"
  if ($PressureTestMode -or $FullNoGuardrails) { $runnerArgs += "-PressureTestMode" }
if ($FullNativeValues -or $PressureTestMode -or $FullNoGuardrails) { $runnerArgs += "-FullNativeValues" } else { $runnerArgs += "-DecodeCoreNativeValues" }
if ($MaxNativeRecords -ge 0) { $runnerArgs += @("-MaxNativeRecords", ([string]$MaxNativeRecords)) }
if ($MaxNativeBlocks -gt 0) { $runnerArgs += @("-MaxNativeBlocks", ([string]$MaxNativeBlocks)) }
if ($EnableStreamInventory) { $runnerArgs += "-EnableAff4StreamInventory" }

powershell -ExecutionPolicy Bypass -File $Runner @runnerArgs
$runnerExit = $LASTEXITCODE

$summaryPath = "D:\Downloads\V1_6_102_AFF4_WRAPPER_RUN_SUMMARY.txt"
@(
  "Vestigant Spotlight V1.6.102 AFF4 wrapper run summary",
  "Created: $((Get-Date).ToString('o'))",
  "ZipPath: $ZipPath",
  "SourceRoot: $SourceRoot",
  "Aff4Path: $Aff4Path",
  "CaseRoot: $CaseRoot",
  "ReaderToolsRoot: $ReaderToolsRoot",
  "Aff4ZipPath: $Aff4ZipPath",
  "BuildLog: $BuildLog",
  "CleanOut: $CleanOut",
  "FullNoGuardrails: $FullNoGuardrails",
  "ForceContainerHash: False",
  "SkipContainerHash: True",
    "PressureTestMode: $($PressureTestMode -or $FullNoGuardrails)",
  "FullNativeValues: $($FullNativeValues -or $PressureTestMode -or $FullNoGuardrails)",
  "MaxNativeRecords: $MaxNativeRecords",
  "MaxNativeBlocks: $MaxNativeBlocks",
  "EnableStreamInventory: $EnableStreamInventory",
  "CliTimeoutMinutes: $CliTimeoutMinutes",
  "RunnerExitCode: $runnerExit"
) | Set-Content -LiteralPath $summaryPath -Encoding UTF8

if ($runnerExit -ne 0) { throw "AFF4 source-probe runner failed with exit code $runnerExit. Review case folder: $CaseRoot and summary: $summaryPath" }
Assert-Leaf -PathValue $Aff4ZipPath -Description "AFF4 upload ZIP"
Remove-Item -LiteralPath ($Aff4ZipPath + ".sha256.txt") -Force -ErrorAction SilentlyContinue
Write-Host "AFF4 upload ZIP: $Aff4ZipPath"
Write-Host "Thin/test mode: upload ZIP SHA256 sidecar intentionally not generated. Full validation can hash later if needed."
Write-Host "AFF4 wrapper summary: $summaryPath"
