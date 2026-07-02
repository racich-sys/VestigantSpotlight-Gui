param(
  [Parameter(Mandatory=$true)][string]$ZipPath,
  [switch]$RequireAppDbSpotlightExports,
  [switch]$AllowIncompleteRun
)

$ErrorActionPreference = "Stop"
if (!(Test-Path -LiteralPath $ZipPath -PathType Leaf)) { throw "iOS upload ZIP not found: $ZipPath" }

Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
$zip = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
try {
  $entries = @{}
  foreach ($entry in $zip.Entries) {
    $name = $entry.FullName.Replace('\\','/').TrimStart('/')
    $entries[$name.ToLowerInvariant()] = $entry
  }

  function Test-EntryPresent {
    param([string]$Name)
    return $entries.ContainsKey($Name.ToLowerInvariant())
  }

  function Test-AnyEntryPresent {
    param([string[]]$Names)
    foreach ($name in $Names) {
      if (Test-EntryPresent -Name $name) { return $true }
    }
    return $false
  }

  $missing = New-Object System.Collections.Generic.List[string]
  $baseRequired = @(
    "run_status.txt",
    "case_summary.csv",
    "IOS_CORESPOTLIGHT_PLAN.md",
    "exports/upload_samples/upload_table_counts.csv",
    "exports/upload_samples/upload_samples_manifest.csv"
  )
  foreach ($required in $baseRequired) {
    if (!(Test-EntryPresent -Name $required)) { $missing.Add($required) | Out-Null }
  }
  if (!(Test-AnyEntryPresent -Names @("EXPORT_INDEX.csv", "exports/EXPORT_INDEX.csv"))) {
    $missing.Add("EXPORT_INDEX.csv or exports/EXPORT_INDEX.csv") | Out-Null
  }

  $missingAppDb = New-Object System.Collections.Generic.List[string]
  if ($RequireAppDbSpotlightExports) {
    $groups = @(
      @{ Label="iOS app DB Spotlight eligibility summary"; Names=@("exports/ios_app_db_spotlight_enabled_summary.csv") },
      @{ Label="iOS app DB Spotlight schema hits"; Names=@("exports/ios_app_db_spotlight_schema_hits.csv", "exports/ios_app_db_spotlight_schema_hits_sample.csv") },
      @{ Label="iOS app DB Spotlight flag candidates"; Names=@("exports/ios_app_db_spotlight_flag_candidates.csv", "exports/ios_app_db_spotlight_flag_candidates_sample.csv") },
      @{ Label="iOS app DB Spotlight row candidates"; Names=@("exports/ios_app_db_spotlight_row_candidates.csv", "exports/ios_app_db_spotlight_row_candidates_sample.csv") }
    )
    foreach ($group in $groups) {
      if (!(Test-AnyEntryPresent -Names $group.Names)) { $missingAppDb.Add($group.Label) | Out-Null }
    }
  }

  $statusLines = New-Object System.Collections.Generic.List[string]
  $statusLines.Add("ZipPath=$ZipPath") | Out-Null
  $statusLines.Add("EntryCount=$($zip.Entries.Count)") | Out-Null
  $statusLines.Add("RequireAppDbSpotlightExports=$($RequireAppDbSpotlightExports.IsPresent)") | Out-Null
  $statusLines.Add("MissingBaseCount=$($missing.Count)") | Out-Null
  $statusLines.Add("MissingAppDbCount=$($missingAppDb.Count)") | Out-Null
  if ($missing.Count -gt 0) { $statusLines.Add("MissingBase=$($missing -join '; ')") | Out-Null }
  if ($missingAppDb.Count -gt 0) { $statusLines.Add("MissingAppDb=$($missingAppDb -join '; ')") | Out-Null }

  if ($missing.Count -gt 0 -or $missingAppDb.Count -gt 0) {
    $message = "iOS Spotlight validation ZIP is missing required outputs. $($statusLines -join ' | ')"
    if ($AllowIncompleteRun) {
      Write-Warning $message
      exit 0
    }
    throw $message
  }

  $statusLines.Add("Status=PASS") | Out-Null
  Write-Host "iOS Spotlight validation output check passed."
  foreach ($line in $statusLines) { Write-Host $line }
} finally {
  $zip.Dispose()
}
