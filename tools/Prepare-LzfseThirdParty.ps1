param(
  [string]$SourceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")),
  [string]$LzfseZip = "D:\Downloads\lzfse-master.zip",
  [string]$ExpectedSha256 = "23855f54ff38ff2f679f79730d20df970dcd3f6cd5ad33505fcdc4220b3ab158"
)

$ErrorActionPreference = "Stop"
$vendorRoot = Join-Path $SourceRoot "third_party\lzfse"
if (!(Test-Path -LiteralPath $LzfseZip)) {
  throw "Apple lzfse ZIP not found: $LzfseZip. Download from https://github.com/lzfse/lzfse and pin/record the SHA256."
}
$hash = (Get-FileHash -LiteralPath $LzfseZip -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "lzfse ZIP SHA256: $hash"
if ($ExpectedSha256 -and ($hash -ne $ExpectedSha256.ToLowerInvariant())) {
  throw "lzfse ZIP SHA256 mismatch. Expected $ExpectedSha256 but got $hash"
}
Remove-Item -LiteralPath $vendorRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $vendorRoot | Out-Null
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("vestigant_lzfse_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tmp | Out-Null
try {
  Expand-Archive -LiteralPath $LzfseZip -DestinationPath $tmp -Force
  $srcHeader = Get-ChildItem -Path $tmp -Recurse -Filter lzfse.h | Where-Object { $_.FullName -match "[\\/]src[\\/]lzfse\.h$" } | Select-Object -First 1
  if (!$srcHeader) { throw "Could not find src\lzfse.h inside $LzfseZip" }
  $repoRoot = Split-Path (Split-Path $srcHeader.FullName -Parent) -Parent
  Copy-Item -Path (Join-Path $repoRoot "*") -Destination $vendorRoot -Recurse -Force
  $manifest = Join-Path $vendorRoot "VESTIGANT_VENDOR_MANIFEST.txt"
  @(
    "vendor=Apple lzfse/lzfse reference implementation",
    "source_zip=$LzfseZip",
    "source_zip_sha256=$hash",
    "prepared_utc=$((Get-Date).ToUniversalTime().ToString('s'))Z",
    "required_header=third_party/lzfse/src/lzfse.h"
  ) | Set-Content -LiteralPath $manifest -Encoding UTF8
  Write-Host "Prepared Apple lzfse source under: $vendorRoot"
} finally {
  Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
