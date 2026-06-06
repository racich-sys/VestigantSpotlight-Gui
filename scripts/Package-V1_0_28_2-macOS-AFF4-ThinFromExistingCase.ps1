param(
  [string]$SourceRoot = "T:\VestigantSpotlightInv_V1_0_28_2",
  [string]$CaseRoot = "Q:\SpotlightCase\TestMacOS_AFF4_V1_0_26",
  [string]$ReaderToolsRoot = "T:\VestigantReaderTools\aff4-cpp-lite",
  [string]$ExternalCompareOutRoot = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_26_ExternalCompare",
  [string]$ZipPath = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_28_2.zip",
  [string]$UploadWorkRoot = "D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_28_2_UploadWork",
  [switch]$IncludeLogsTailOnly,
  [switch]$DiagnosticOutputs
)

$ErrorActionPreference = "Stop"
$UploadTool = Join-Path $SourceRoot "tools\Create-SourceProbeUploadZip.ps1"
if (!(Test-Path -LiteralPath $UploadTool)) { throw "Upload packaging helper not found: $UploadTool" }
if (!(Test-Path -LiteralPath $CaseRoot)) { throw "Existing case root not found: $CaseRoot" }
if (!(Test-Path -LiteralPath $ReaderToolsRoot)) { throw "Reader tools root not found: $ReaderToolsRoot" }

$args = @{
  CaseRoot = $CaseRoot
  ReaderToolsRoot = $ReaderToolsRoot
  ZipPath = $ZipPath
  UploadWorkRoot = $UploadWorkRoot
  IncludeLogsTailOnly = $IncludeLogsTailOnly
}
if (![string]::IsNullOrWhiteSpace($ExternalCompareOutRoot) -and (Test-Path -LiteralPath $ExternalCompareOutRoot)) {
  $args.AdditionalOutputRoot = $ExternalCompareOutRoot
} elseif (![string]::IsNullOrWhiteSpace($ExternalCompareOutRoot)) {
  Write-Warning "External comparison output root not found, packaging case outputs only: $ExternalCompareOutRoot"
}
if ($DiagnosticOutputs) { $args.IncludeStructuralDiagnostics = $true }

& $UploadTool @args
if ($LASTEXITCODE -ne 0) { throw "Thin upload packaging failed with exit code $LASTEXITCODE" }
if (!(Test-Path -LiteralPath $ZipPath)) { throw "Thin upload ZIP was not created: $ZipPath" }
Write-Host "Packaged existing macOS AFF4/APFS case into thin upload ZIP: $ZipPath"
