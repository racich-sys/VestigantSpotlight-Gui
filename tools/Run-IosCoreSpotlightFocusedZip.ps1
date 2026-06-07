param(
    [Parameter(Mandatory=$true)][string]$InputZipOrFolder,
    [string]$Out = "Q:\SpotlightCase\V0_8_75_iOS_CoreSpotlight",
    [string]$ZipPath = "D:\Downloads\Upload_Thin_V0_8_75_iOS_CoreSpotlight.zip",
    [switch]$CleanOut,
    [switch]$NoClipboardOrExplorer,
    [switch]$FullDiagnostics
)

$ErrorActionPreference = "Stop"
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptRoot
$Cli = Join-Path $ProjectRoot "build-msvc\Release\VestigantSpotlightCli.exe"
$UploadScript = Join-Path $ScriptRoot "Create-SourceProbeUploadZip.ps1"

if (!(Test-Path -LiteralPath $Cli)) { throw "CLI binary not found. Build this source tree first: $Cli" }
if (!(Test-Path -LiteralPath $InputZipOrFolder)) { throw "Input not found: $InputZipOrFolder" }
if ($CleanOut) { Remove-Item -LiteralPath $Out -Recurse -Force -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path $Out | Out-Null

# V0_8_75: record what CoreSpotlight stores exist inside the submitted focused ZIP/folder
# before the CLI stages it. This makes discovery gaps visible in thin uploads.
$InventoryPath = Join-Path $Out "ios_input_store_entry_inventory.csv"
"entry_type,length,last_write_time,full_name" | Set-Content -LiteralPath $InventoryPath -Encoding UTF8
if (Test-Path -LiteralPath $InputZipOrFolder -PathType Leaf) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($InputZipOrFolder)
    try {
        $zip.Entries |
          Where-Object { $_.FullName -match '(^|[\\/])(\.store\.db|store\.db)$' } |
          Sort-Object FullName |
          ForEach-Object {
              $leaf = [System.IO.Path]::GetFileName($_.FullName.Replace('/','\'))
              $line = ('{0},{1},{2},{3}' -f $leaf,$_.Length,$_.LastWriteTime.UtcDateTime.ToString('o'),('"' + ($_.FullName -replace '"','""') + '"'))
              Add-Content -LiteralPath $InventoryPath -Value $line -Encoding UTF8
          }
    } finally { $zip.Dispose() }
} else {
    Get-ChildItem -LiteralPath $InputZipOrFolder -Recurse -File -Force |
      Where-Object { $_.Name -in @('store.db','.store.db') } |
      Sort-Object FullName |
      ForEach-Object {
          $line = ('{0},{1},{2},{3}' -f $_.Name,$_.Length,$_.LastWriteTimeUtc.ToString('o'),('"' + ($_.FullName -replace '"','""') + '"'))
          Add-Content -LiteralPath $InventoryPath -Value $line -Encoding UTF8
      }
}


$ExportProfile = if ($FullDiagnostics) { "diagnostics" } else { "minimal" }
Write-Host "iOS export profile: $ExportProfile (FullDiagnostics=$($FullDiagnostics.IsPresent))"

& $Cli `
  --mode diagnostics `
  --profile ios `
  --input $InputZipOrFolder `
  --out $Out `
  --full-scan `
  --decode-core-native-values `
  --export-profile $ExportProfile `
  --verbose

& $UploadScript `
  -CaseRoot $Out `
  -ZipPath $ZipPath `
  -UploadWorkRoot "D:\Downloads\V0_8_75_iOS_UploadWork" `
  -IncludeLogsTailOnly

Get-Item $ZipPath | Select-Object FullName,Length
Get-FileHash $ZipPath -Algorithm SHA256

if (!$NoClipboardOrExplorer) {
    try { Set-Clipboard -Value $ZipPath } catch {}
    try { explorer.exe /select,"$ZipPath" } catch {}
}
