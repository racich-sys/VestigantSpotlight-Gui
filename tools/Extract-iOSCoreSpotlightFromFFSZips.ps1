param(
    [string]$ZipFolder = "T:\iOS_Extraction",
    [string]$ExtractRoot = "D:\Downloads\iOS_CoreSpotlight_Focused_Extracts",
    [string]$ReportRoot = "D:\Downloads\iOS_CoreSpotlight_Focused_Report",
    [string]$ReportZip = "D:\Downloads\iOS_CoreSpotlight_Focused_Report.zip",
    [string]$EvidenceZip = "D:\Downloads\iOS_CoreSpotlight_Focused_Extracts.zip",
    [string]$SevenZip = "C:\Program Files\7-Zip\7z.exe",
    [switch]$SkipEvidenceZip
)

$ErrorActionPreference = "Stop"

if (!(Test-Path -LiteralPath $ZipFolder)) { throw "ZIP folder not found: $ZipFolder" }
if (!(Test-Path -LiteralPath $SevenZip)) { throw "7-Zip executable not found: $SevenZip" }

Remove-Item -LiteralPath $ExtractRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $ReportRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $ReportZip -Force -ErrorAction SilentlyContinue
if (!$SkipEvidenceZip) { Remove-Item -LiteralPath $EvidenceZip -Force -ErrorAction SilentlyContinue }

New-Item -ItemType Directory -Force -Path $ExtractRoot | Out-Null
New-Item -ItemType Directory -Force -Path $ReportRoot | Out-Null

$zipFiles = Get-ChildItem -LiteralPath $ZipFolder -File -Filter "*.zip" -Force | Sort-Object Name

@(
  "ZipFolder=$ZipFolder"
  "ExtractRoot=$ExtractRoot"
  "SevenZip=$SevenZip"
  "GeneratedUtc=$((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))"
  "ZipCount=$($zipFiles.Count)"
) | Set-Content -LiteralPath (Join-Path $ReportRoot "RUN_CONTEXT.txt") -Encoding UTF8

$summaryRows = New-Object System.Collections.Generic.List[object]

foreach ($zip in $zipFiles) {
    $safe = [IO.Path]::GetFileNameWithoutExtension($zip.Name) -replace '[^\w\.\-]+','_'
    $sampleOut = Join-Path $ExtractRoot $safe
    New-Item -ItemType Directory -Force -Path $sampleOut | Out-Null

    $extractLog = Join-Path $ReportRoot "$safe.extract_log.txt"
    $listCsv = Join-Path $ReportRoot "$safe.focused_listing.csv"

    Write-Host "Focused iOS CoreSpotlight extraction from: $($zip.Name)"

    $patterns = @(
      "-ir!*private\var\mobile\Library\Spotlight\CoreSpotlight\*",
      "-ir!*var\mobile\Library\Spotlight\CoreSpotlight\*",
      "-ir!*private\var\mobile\Library\Spotlight\BundleInfo\*",
      "-ir!*var\mobile\Library\Spotlight\BundleInfo\*"
    )

    & $SevenZip x "$($zip.FullName)" "-o$sampleOut" @patterns -y > $extractLog 2>&1

    $files = @(Get-ChildItem -LiteralPath $sampleOut -Recurse -File -Force -ErrorAction SilentlyContinue)
    $files |
      Select-Object FullName,Length,LastWriteTime |
      Export-Csv -NoTypeInformation -Encoding UTF8 $listCsv

    $summaryRows.Add([pscustomobject]@{
        ZipName = $zip.Name
        ZipPath = $zip.FullName
        ExtractedSampleFolder = $sampleOut
        ExtractedFileCount = $files.Count
        ExtractedBytes = ($files | Measure-Object Length -Sum).Sum
        ExtractLog = $extractLog
        Listing = $listCsv
    }) | Out-Null
}

$summaryRows |
  Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $ReportRoot "ios_corespotlight_focused_extract_summary.csv")

Get-ChildItem -LiteralPath $ExtractRoot -Recurse -File -Force -ErrorAction SilentlyContinue |
  Select-Object FullName,Length,LastWriteTime |
  Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $ReportRoot "all_extracted_files.csv")

Compress-Archive -Path (Join-Path $ReportRoot "*") -DestinationPath $ReportZip -Force

if (!$SkipEvidenceZip) {
    & $SevenZip a -tzip "$EvidenceZip" "$ExtractRoot\*" -mx=1 > (Join-Path $ReportRoot "evidence_zip_create.log") 2>&1
}

Get-Item $ReportZip -ErrorAction SilentlyContinue | Select-Object FullName,Length
if (!$SkipEvidenceZip) { Get-Item $EvidenceZip -ErrorAction SilentlyContinue | Select-Object FullName,Length }
Get-FileHash $ReportZip -Algorithm SHA256
if (!$SkipEvidenceZip) { Get-FileHash $EvidenceZip -Algorithm SHA256 }
