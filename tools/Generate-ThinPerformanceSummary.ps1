param(
  [Parameter(Mandatory=$true)][string]$CaseRoot,
  [int]$SlowExportSeconds = 30,
  [string]$ReportTitle = 'Thin Performance Summary',
  [string]$OutputPrefix = 'thin_performance_summary',
  [string]$OutputMarkdownName = 'THIN_PERFORMANCE_SUMMARY.md'
)

$ErrorActionPreference = 'Stop'

function Find-CaseFile {
  param([string]$RelativeName)
  $candidates = @(
    (Join-Path $CaseRoot $RelativeName),
    (Join-Path (Join-Path $CaseRoot 'logs') $RelativeName)
  )
  foreach ($c in $candidates) { if (Test-Path -LiteralPath $c) { return $c } }
  return $null
}

function Parse-IsoUtcSafe {
  param([string]$Value)
  try { return [datetime]::Parse($Value, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime() } catch { return $null }
}

$runProgress = Find-CaseFile -RelativeName 'run_progress.tsv'
$outCsv = Join-Path $CaseRoot ($OutputPrefix + '.csv')
$outMd = Join-Path $CaseRoot $OutputMarkdownName

$rows = New-Object System.Collections.Generic.List[object]
$exports = @{}
$firstTs = $null
$lastTs = $null

if ($runProgress) {
  foreach ($line in Get-Content -LiteralPath $runProgress -ErrorAction SilentlyContinue) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $parts = $line -split "`t", 4
    if ($parts.Count -lt 3) { continue }
    $ts = Parse-IsoUtcSafe $parts[0]
    if ($ts) {
      if (!$firstTs -or $ts -lt $firstTs) { $firstTs = $ts }
      if (!$lastTs -or $ts -gt $lastTs) { $lastTs = $ts }
    }
    $stage = $parts[2]
    $msg = if ($parts.Count -ge 4) { $parts[3] } else { '' }
    if ($stage -like 'export_query_*') {
      $name = $msg
      if ($msg -match '([^\s]+\.csv)') { $name = $Matches[1] }
      if ([string]::IsNullOrWhiteSpace($name)) { $name = '(unknown)' }
      if (!$exports.ContainsKey($name)) {
        $exports[$name] = [ordered]@{
          export_name = $name
          prepare_utc = ''
          execute_utc = ''
          complete_utc = ''
          elapsed_seconds = ''
          rows = ''
          chunked = ''
          max_sqlite_progress_ticks = ''
          status = 'observed'
        }
      }
      $rec = $exports[$name]
      if ($stage -eq 'export_query_prepare' -and $ts) { $rec.prepare_utc = $ts.ToString('o') }
      if ($stage -eq 'export_query_execute' -and $ts) { $rec.execute_utc = $ts.ToString('o') }
      if ($stage -eq 'export_query_complete' -and $ts) {
        $rec.complete_utc = $ts.ToString('o')
        $rec.status = 'complete'
        if ($msg -match 'rows=([0-9]+)') { $rec.rows = $Matches[1] }
        if ($msg -match 'chunked=([0-9]+)') { $rec.chunked = $Matches[1] }
      }
      if ($stage -eq 'export_query_sql_progress') {
        if ($msg -match 'sqlite_progress_ticks=([0-9]+)') {
          $ticks = [int64]$Matches[1]
          if ([string]::IsNullOrWhiteSpace($rec.max_sqlite_progress_ticks) -or $ticks -gt [int64]$rec.max_sqlite_progress_ticks) { $rec.max_sqlite_progress_ticks = [string]$ticks }
        }
      }
    }
  }
}

foreach ($key in $exports.Keys) {
  $rec = $exports[$key]
  $start = $null; $end = $null
  if (![string]::IsNullOrWhiteSpace($rec.execute_utc)) { $start = Parse-IsoUtcSafe $rec.execute_utc }
  elseif (![string]::IsNullOrWhiteSpace($rec.prepare_utc)) { $start = Parse-IsoUtcSafe $rec.prepare_utc }
  if (![string]::IsNullOrWhiteSpace($rec.complete_utc)) { $end = Parse-IsoUtcSafe $rec.complete_utc }
  if ($start -and $end) { $rec.elapsed_seconds = [string][int][Math]::Round(($end - $start).TotalSeconds) }
  if ($start -and !$end) { $rec.status = 'started_not_completed' }
  if (![string]::IsNullOrWhiteSpace($rec.elapsed_seconds) -and [int]$rec.elapsed_seconds -ge $SlowExportSeconds) { $rec.status = 'slow_complete' }
  $rows.Add([pscustomobject]$rec) | Out-Null
}

$sorted = @($rows | Sort-Object @{Expression={ if ($_.elapsed_seconds -match '^\d+$') { [int]$_.elapsed_seconds } else { -1 } }; Descending=$true}, export_name)
if ($sorted.Count -gt 0) {
  $sorted | Export-Csv -LiteralPath $outCsv -NoTypeInformation -Encoding UTF8
} else {
  'export_name,prepare_utc,execute_utc,complete_utc,elapsed_seconds,rows,chunked,max_sqlite_progress_ticks,status' | Set-Content -LiteralPath $outCsv -Encoding UTF8
}

$total = ''
if ($firstTs -and $lastTs) { $total = [int][Math]::Round(($lastTs - $firstTs).TotalSeconds) }
$slow = @($sorted | Where-Object { $_.status -eq 'slow_complete' -or $_.status -eq 'started_not_completed' } | Select-Object -First 20)
$md = New-Object System.Collections.Generic.List[string]
$md.Add('# ' + $ReportTitle) | Out-Null
$md.Add('') | Out-Null
$md.Add("Generated UTC: $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))") | Out-Null
$md.Add("CaseRoot: $CaseRoot") | Out-Null
$md.Add("Run progress file: $runProgress") | Out-Null
$md.Add("Observed run_progress duration seconds: $total") | Out-Null
$md.Add('') | Out-Null
$md.Add('## Slow or incomplete exports') | Out-Null
if ($slow.Count -eq 0) {
  $md.Add('- None observed above threshold.') | Out-Null
} else {
  foreach ($s in $slow) {
    $md.Add(('- {0}: status={1}; elapsed_seconds={2}; rows={3}; sqlite_progress_ticks={4}' -f $s.export_name,$s.status,$s.elapsed_seconds,$s.rows,$s.max_sqlite_progress_ticks)) | Out-Null
  }
}
$md.Add('') | Out-Null
$md.Add('CSV detail: ' + ($OutputPrefix + '.csv')) | Out-Null
$md | Set-Content -LiteralPath $outMd -Encoding UTF8

Write-Host "Performance summary written: $outCsv"
Write-Host "Performance summary written: $outMd"
