param(
  [string]$SourceRoot = (Resolve-Path (Join-Path $PSScriptRoot ".."))
)
$ErrorActionPreference = 'Stop'
$limit = 5000
$failures = @()
Get-ChildItem -LiteralPath (Join-Path $SourceRoot 'src') -Recurse -Include *.cpp,*.h,*.hpp | ForEach-Object {
  $path = $_.FullName
  $text = Get-Content -LiteralPath $path -Raw
  $matches = [regex]::Matches($text, 'R"(?<delim>[A-Za-z0-9_]{0,16})\((?<body>[\s\S]*?)\)\k<delim>"')
  foreach ($m in $matches) {
    $len = $m.Groups['body'].Value.Length
    if ($len -gt $limit) {
      $line = ($text.Substring(0, $m.Index).Split("`n")).Count
      $failures += "${path}:$line raw-string length=$len exceeds limit=$limit"
    }
  }
}
if ($failures.Count -gt 0) {
  $failures | ForEach-Object { Write-Error $_ }
  throw "MSVC raw-string literal risk check failed. Split large SQL/string literals before release."
}
Write-Host "MSVC raw-string literal risk check passed. Limit=$limit"
