param(
  [string]$CaseRoot = "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_24_ReusedCache",
  [string]$OutZip = "D:\Downloads\Upload_State_V0_9_24_DB_Bloat_Stopped_Check.zip",
  [switch]$StopVestigant
)

$ErrorActionPreference = "Continue"
$work = Join-Path $env:TEMP ("Vestigant_V0_9_24_State_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force -Path $work | Out-Null

$summary = Join-Path $work "00_summary.txt"
@(
  "Vestigant Spotlight V0_9_24 DB/WAL/no-writes state collection",
  "Timestamp: $(Get-Date -Format o)",
  "CaseRoot: $CaseRoot",
  "OutZip: $OutZip",
  "StopVestigant requested: $StopVestigant"
) | Set-Content -LiteralPath $summary -Encoding UTF8

Get-Process | Where-Object { $_.ProcessName -like "VestigantSpotlight*" -or $_.ProcessName -in @('7z','7za','7zr','MsMpEng') } |
  Select-Object Id,ProcessName,CPU,StartTime,Path |
  Format-Table -AutoSize | Out-String -Width 4096 | Set-Content -LiteralPath (Join-Path $work "01_processes_before_stop.txt") -Encoding UTF8

if ($StopVestigant) {
  Get-Process | Where-Object { $_.ProcessName -like "VestigantSpotlight*" } | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Seconds 5
}

Get-Process | Where-Object { $_.ProcessName -like "VestigantSpotlight*" -or $_.ProcessName -in @('7z','7za','7zr','MsMpEng') } |
  Select-Object Id,ProcessName,CPU,StartTime,Path |
  Format-Table -AutoSize | Out-String -Width 4096 | Set-Content -LiteralPath (Join-Path $work "02_processes_after_stop_sample.txt") -Encoding UTF8

if (Test-Path -LiteralPath $CaseRoot) {
  Get-ChildItem -LiteralPath $CaseRoot -Recurse -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 250 LastWriteTime,Length,FullName |
    Format-Table -AutoSize | Out-String -Width 4096 |
    Set-Content -LiteralPath (Join-Path $work "03_recent_250_files.txt") -Encoding UTF8

  Get-ChildItem -LiteralPath $CaseRoot -Recurse -File -ErrorAction SilentlyContinue |
    Sort-Object Length -Descending |
    Select-Object -First 250 LastWriteTime,Length,FullName |
    Format-Table -AutoSize | Out-String -Width 4096 |
    Set-Content -LiteralPath (Join-Path $work "04_largest_250_files.txt") -Encoding UTF8

  Get-ChildItem -LiteralPath $CaseRoot -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -match '^(\.sqlite|\.db|\.csv)$' -or $_.Name -match 'sqlite-wal|sqlite-shm|\.wal$|\.shm$' } |
    Sort-Object Length -Descending |
    Select-Object LastWriteTime,Length,FullName |
    Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath (Join-Path $work "05_db_csv_size_inventory.csv")

  foreach ($rel in @(
    "run_status.txt",
    "last_stage.txt",
    "last_progress.tsv",
    "run_progress.tsv",
    "source_cache_manifest.json",
    "case_summary.json",
    "logs\VestigantSpotlight.log",
    "logs\ios_reuse_cache.log",
    "logs\run_status.txt",
    "logs\run_progress.tsv",
    "logs\last_stage.txt",
    "logs\last_progress.tsv"
  )) {
    $src = Join-Path $CaseRoot $rel
    if (Test-Path -LiteralPath $src) {
      $dst = Join-Path $work ("case_root\" + $rel)
      New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
      Copy-Item -LiteralPath $src -Destination $dst -Force
    }
  }
}

try {
  $outParent = Split-Path -Parent $OutZip
  if ([string]::IsNullOrWhiteSpace($outParent)) { $outParent = (Get-Location).Path }
  New-Item -ItemType Directory -Force -Path $outParent | Out-Null
  $tmpZip = Join-Path $outParent (([System.IO.Path]::GetFileNameWithoutExtension($OutZip)) + ".tmp.zip")
  Remove-Item -LiteralPath $OutZip -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath ($OutZip + ".sha256") -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $tmpZip -Force -ErrorAction SilentlyContinue
  Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
  [System.IO.Compression.ZipFile]::CreateFromDirectory($work, $tmpZip, [System.IO.Compression.CompressionLevel]::Optimal, $false)
  Move-Item -LiteralPath $tmpZip -Destination $OutZip -Force
  Get-FileHash -LiteralPath $OutZip -Algorithm SHA256 | Format-List | Out-File -LiteralPath ($OutZip + ".sha256") -Encoding UTF8
  Write-Host "Wrote: $OutZip"
  Write-Host "Wrote: $OutZip.sha256"
} catch {
  Write-Error $_
  throw
}
