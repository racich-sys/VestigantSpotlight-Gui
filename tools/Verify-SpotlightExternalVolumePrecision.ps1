param(
  [Parameter(Mandatory=$true)] [string]$ZipPath
)
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath $ZipPath)) { throw "Upload ZIP not found: $ZipPath" }
$temp = Join-Path ([System.IO.Path]::GetTempPath()) ("VestigantSpotlightExternalVolumeAudit_" + [System.Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $temp | Out-Null
try {
  Expand-Archive -LiteralPath $ZipPath -DestinationPath $temp -Force
  $failures = New-Object System.Collections.Generic.List[string]

  function Find-OneRequiredCsv([string]$Name) {
    $file = Get-ChildItem -LiteralPath $temp -Recurse -File -Filter $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if (!$file) { $failures.Add("missing required Spotlight external-volume output: $Name") | Out-Null }
    return $file
  }

  function Test-ExplicitSpotlightVolumeValue([string]$value) {
    if ([string]::IsNullOrWhiteSpace($value)) { return $false }
    $v = $value.ToLowerInvariant()
    if ($v.Contains('/system/volumes/data/') -and -not $v.Contains('/system/volumes/data/volumes/')) { return $false }
    return ($v.Contains('/volumes/') -or $v.Contains('file:///volumes/') -or $v.Contains('/.vol/') -or $v.Contains('file:///.vol/'))
  }

  $expected = @(
    'spotlight_external_volume_candidate_summary.csv',
    'spotlight_external_volume_evidence_review.csv',
    'spotlight_external_volume_raw_value_hits.csv',
    'spotlight_external_volume_cache_text_hits.csv',
    'spotlight_external_volume_dictionary_hits.csv',
    'spotlight_external_volume_volfs_hits.csv'
  )
  $files = @{}
  foreach ($name in $expected) { $files[$name] = Find-OneRequiredCsv $name }

  $raw = $files['spotlight_external_volume_raw_value_hits.csv']
  if ($raw) {
    $rows = Import-Csv -LiteralPath $raw.FullName
    $i = 1
    foreach ($r in $rows) {
      $val = [string]$r.path_or_value
      if (-not (Test-ExplicitSpotlightVolumeValue $val)) { $failures.Add("raw_value_hits row $i is not explicit /Volumes/.vol evidence: field=$($r.source_field) value=$($val.Substring(0,[Math]::Min(160,$val.Length)))") | Out-Null }
      if (([string]$r.evidence_type) -match 'VOLUME_RELATED_TOKEN|REVIEW_TOKEN') { $failures.Add("raw_value_hits row $i has broad token evidence_type=$($r.evidence_type)") | Out-Null }
      $i++
    }
  }

  $cache = $files['spotlight_external_volume_cache_text_hits.csv']
  if ($cache) {
    $rows = Import-Csv -LiteralPath $cache.FullName
    $i = 1
    foreach ($r in $rows) {
      $val = [string]$r.path_or_value
      if (-not (Test-ExplicitSpotlightVolumeValue $val)) { $failures.Add("cache_text_hits row $i is not explicit /Volumes/.vol evidence: value=$($val.Substring(0,[Math]::Min(160,$val.Length)))") | Out-Null }
      if (([string]$r.evidence_type) -match 'VOLUME_RELATED_TOKEN|CACHE_TEXT_VOLUME_TOKEN') { $failures.Add("cache_text_hits row $i has broad token evidence_type=$($r.evidence_type)") | Out-Null }
      $i++
    }
  }

  $volfs = $files['spotlight_external_volume_volfs_hits.csv']
  if ($volfs) {
    $rows = Import-Csv -LiteralPath $volfs.FullName
    $i = 1
    foreach ($r in $rows) {
      $val = [string]$r.path_or_value
      if (-not (Test-ExplicitSpotlightVolumeValue $val)) { $failures.Add("volfs_hits row $i is not explicit .vol evidence: value=$($val.Substring(0,[Math]::Min(160,$val.Length)))") | Out-Null }
      $i++
    }
  }

  $summary = $files['spotlight_external_volume_candidate_summary.csv']
  if ($summary) {
    $rows = Import-Csv -LiteralPath $summary.FullName
    $i = 1
    foreach ($r in $rows) {
      if (([string]$r.volume_name_or_token) -match 'CACHE_TEXT_VOLUME_TOKEN|UNKNOWN_VOLUME_TOKEN|kMDItemWhereFroms|_kMDItemHelpPath') { $failures.Add("candidate_summary row $i has broad token volume_name_or_token=$($r.volume_name_or_token)") | Out-Null }
      if (![string]::IsNullOrWhiteSpace([string]$r.sample_path_or_value) -and -not (Test-ExplicitSpotlightVolumeValue ([string]$r.sample_path_or_value))) { $failures.Add("candidate_summary row $i has non-explicit sample_path_or_value=$($r.sample_path_or_value)") | Out-Null }
      $i++
    }
  }

  if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
  }
  Write-Host "Spotlight external-volume precision audit passed: $ZipPath"
  exit 0
}
finally {
  Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}
