param(
  [ValidateSet("AFF4Probe", "IOSCoreSpotlightThin", "BuildOnly")]
  [string]$Workflow = "AFF4Probe",
  [string]$DownloadRoot = "D:\Downloads",
  [string]$ZipPath = "D:\Downloads\VestigantSpotlightInv_V1_6_77.zip",
  [string]$Aff4SearchRoot = "T:\",
  [string]$Aff4NameHint = "0202_0024-IT003",
  [string]$FastLocalRoot = "D:\Downloads\SpotlightCase",
  [int]$MaxNativeRecords = 0,
  [switch]$FullNativeValues,
  [switch]$NoCleanOut,
  [switch]$NoFullNoGuardrails
)

$ErrorActionPreference = "Stop"

$buildAndRun = Join-Path $DownloadRoot "BuildAndRun-V1_6_77-FromDownloadedZip.ps1"
if (!(Test-Path -LiteralPath $buildAndRun -PathType Leaf)) {
  $sibling = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "BuildAndRun-V1_6_77-FromDownloadedZip.ps1"
  if (Test-Path -LiteralPath $sibling -PathType Leaf) { $buildAndRun = $sibling }
}
if (!(Test-Path -LiteralPath $ZipPath -PathType Leaf)) {
  throw "Required source ZIP not found: $ZipPath. Download VestigantSpotlightInv_V1_6_77.zip to D:\Downloads or pass -ZipPath."
}
if (!(Test-Path -LiteralPath $buildAndRun -PathType Leaf)) {
  $bootstrapRoot = Join-Path $DownloadRoot "_VestigantSpotlightInv_V1_6_77_Bootstrap"
  $bootstrapSourceRoot = Join-Path $bootstrapRoot "VestigantSpotlightInv_V1_6_77"
  $bootstrapBuildAndRun = Join-Path $bootstrapSourceRoot "BuildAndRun-V1_6_77-FromDownloadedZip.ps1"
  Write-Host "BuildAndRun wrapper was not found beside the download. Extracting bootstrap copy from ZIP: $ZipPath"
  if (Test-Path -LiteralPath $bootstrapRoot) { Remove-Item -LiteralPath $bootstrapRoot -Recurse -Force }
  New-Item -ItemType Directory -Path $bootstrapRoot -Force | Out-Null
  Expand-Archive -LiteralPath $ZipPath -DestinationPath $bootstrapRoot -Force
  if (!(Test-Path -LiteralPath $bootstrapBuildAndRun -PathType Leaf)) {
    throw "BuildAndRun wrapper was not found in the ZIP after bootstrap extraction: $bootstrapBuildAndRun"
  }
  $buildAndRun = $bootstrapBuildAndRun
}

Write-Host "Vestigant Spotlight V1.6.77 one-click build/run wrapper"
Write-Host "Workflow: $Workflow"
Write-Host "BuildAndRun script: $buildAndRun"
Write-Host "Source ZIP: $ZipPath"

$args = @(
  "-ExecutionPolicy", "Bypass",
  "-File", $buildAndRun,
  "-Workflow", $Workflow,
  "-ZipPath", $ZipPath
)

if (!$NoCleanOut) { $args += "-CleanOut" }

if ($Workflow -eq "AFF4Probe") {
  $args += @("-Aff4SearchRoot", $Aff4SearchRoot, "-Aff4NameHint", $Aff4NameHint, "-MaxNativeRecords", ([string]$MaxNativeRecords))
  if (!$NoFullNoGuardrails) { $args += "-FullNoGuardrails" }
  if ($FullNativeValues) { $args += "-FullNativeValues" }
}
elseif ($Workflow -eq "IOSCoreSpotlightThin") {
  $args += @("-UseFastLocalCaseRoot", "-FastLocalRoot", $FastLocalRoot)
}

Write-Host "Executing: powershell $($args -join ' ')"
powershell @args
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) { throw "V1.6.77 one-click build/run workflow failed with exit code $exitCode" }
Write-Host "V1.6.77 one-click build/run workflow completed."
