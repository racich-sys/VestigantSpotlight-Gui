param(
  [string]$InputZip = "T:\0202_0024-IT002\00008132-000269523699001C_files_full.zip",
  [string]$ReportRoot = "D:\Downloads\iOS_CoreSpotlight_QuickDiagnostics_V1_0_13",
  [string]$ReportZip = "D:\Downloads\iOS_CoreSpotlight_QuickDiagnostics_V1_0_13.zip",
  [string]$EvidenceRoot = "D:\Downloads\iOS_CoreSpotlight_MinimalEvidence_V1_0_13",
  [string]$EvidenceZip = "D:\Downloads\iOS_CoreSpotlight_MinimalEvidence_V1_0_13.zip",
  [switch]$IncludeStoreFiles,
  [switch]$IncludeCacheTextSamples,
  [int]$MaxCacheTextSamplesPerProtectionClass = 25,
  [switch]$NoClipboardOrExplorer
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (!(Test-Path -LiteralPath $InputZip)) { throw "Input ZIP not found: $InputZip" }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function New-CleanDirectory([string]$Path) { if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -Recurse -Force }; New-Item -ItemType Directory -Force -Path $Path | Out-Null }
function New-ZipFromDirectorySafe([string]$SourceDirectory, [string]$DestinationZip) { if (Test-Path -LiteralPath $DestinationZip) { Remove-Item -LiteralPath $DestinationZip -Force }; [System.IO.Compression.ZipFile]::CreateFromDirectory($SourceDirectory,$DestinationZip,[System.IO.Compression.CompressionLevel]::Optimal,$false) }
function Write-EmptyCsv([string]$Path,[string[]]$Headers) { ($Headers -join ',') | Set-Content -LiteralPath $Path -Encoding UTF8 }
function Find-7Zip { foreach($c in @("C:\Program Files\7-Zip\7z.exe","C:\Program Files (x86)\7-Zip\7z.exe","7z.exe")){ try{ $cmd=Get-Command $c -ErrorAction SilentlyContinue; if($cmd){return $cmd.Source}; if(Test-Path -LiteralPath $c){return $c} }catch{} }; return $null }
function Normalize-IosPath([string]$fullName) { if ([string]::IsNullOrWhiteSpace($fullName)) { return "" }; $p=$fullName.Replace('\','/'); $m=[regex]::Match($p,'(?i)(/private/var/.*)$'); if($m.Success){return $m.Groups[1].Value.ToLowerInvariant()}; $m=[regex]::Match($p,'(?i)(/var/.*)$'); if($m.Success){return ('/private'+$m.Groups[1].Value).ToLowerInvariant()}; return ('/'+$p.TrimStart('/')).ToLowerInvariant() }
function Get-ProtectionClass([string]$Name) { if ($Name -match 'CoreSpotlight[\\/]([^/\\]+)[\\/]index\.spotlightV2') { return $Matches[1] }; return "" }
function Test-IsSignalPath([string]$lowerPath) { $pn=$lowerPath.Replace('\','/'); return ($pn.Contains('org.whispersystems.signal') -or $pn.Contains('signal.messenger') -or $pn.Contains('/signal/')) }
function Test-IsChromeBrowserPath([string]$lowerPath) { $pn=$lowerPath.Replace('\','/'); return ($pn.Contains('com.google.chrome') -or $pn.Contains('/chrome/') -or $pn.Contains('/google/chrome')) }
function Get-ProtectionClassHint([string]$path) { $p=$path.ToLowerInvariant(); if($p.Contains('nsfileprotectioncompleteuntilfirstuserauthentication')){return 'NSFileProtectionCompleteUntilFirstUserAuthentication'}; if($p.Contains('nsfileprotectioncompleteunlessopen')){return 'NSFileProtectionCompleteUnlessOpen'}; if($p.Contains('nsfileprotectioncompletewhenuserinactive')){return 'NSFileProtectionCompleteWhenUserInactive'}; if($p.Contains('nsfileprotectioncomplete')){return 'NSFileProtectionComplete'}; if($p.Contains('/priority/')){return 'Priority'}; return 'Unknown' }
function Get-DomainHint([string]$path) { $p=$path.ToLowerInvariant(); if($p.Contains('/library/sms/') -or $p.EndsWith('/sms.db')){return 'Messages'}; if($p.Contains('callhistory')){return 'CallHistory'}; if($p.Contains('whatsapp')){return 'WhatsApp'}; if(Test-IsSignalPath $p){return 'Signal'}; if($p.Contains('telegram')){return 'Telegram'}; if($p.Contains('safari')){return 'Safari'}; if(Test-IsChromeBrowserPath $p){return 'Chrome'}; if($p.Contains('/mail/')){return 'Mail'}; if($p.Contains('/calendar/')){return 'Calendar'}; if($p.Contains('/addressbook/') -or $p.Contains('contacts')){return 'Contacts'}; if($p.Contains('fileprovider') -or $p.Contains('clouddocs') -or $p.Contains('mobile documents')){return 'FileProviderOrCloudDocs'}; return 'Other' }
function Get-AppContainerHint([string]$path) { $p=$path.Replace('\','/'); $m=[regex]::Match($p,'(?i)/Containers/(Data/)?Application/([^/]+)'); if($m.Success){return 'ApplicationContainer:'+$m.Groups[2].Value}; $m=[regex]::Match($p,'(?i)/Containers/Shared/AppGroup/([^/]+)'); if($m.Success){return 'AppGroup:'+$m.Groups[1].Value}; return '' }
function Get-DatabaseCategory([string]$path,[string]$name) { $p=$path.ToLowerInvariant(); $n=$name.ToLowerInvariant(); $dbLike=($n -like '*.db' -or $n -like '*.sqlite' -or $n -like '*.sqlite3' -or $n -like '*.sqlitedb' -or $n -like '*.storedata' -or $n -eq 'chatstorage.sqlite' -or $n -eq 'contactsv2.sqlite' -or $n -eq 'callhistory.sqlite'); if(-not $dbLike){return @('','')}; if($n -eq 'sms.db'){return @('APPLE_MESSAGES','Messages')}; if($p.Contains('callhistory') -or $n -like 'callhistory*'){return @('CALL_HISTORY','PhoneFaceTime')}; if($p.Contains('group.net.whatsapp') -or $p.Contains('/whatsapp/') -or $p.Contains('whatsapp.shared') -or $n -eq 'chatstorage.sqlite' -or $n -eq 'contactsv2.sqlite'){return @('WHATSAPP','WhatsApp')}; if(Test-IsSignalPath $p){return @('SIGNAL','Signal')}; if($p.Contains('telegram')){return @('TELEGRAM','Telegram')}; if($p.Contains('safari')){return @('SAFARI_WEB','Safari')}; if(Test-IsChromeBrowserPath $p){return @('CHROME_WEB','Chrome')}; if($p.Contains('webkit')){return @('WEBKIT','WebKit')}; if($p.Contains('/mail/')){return @('MAIL','Mail')}; if($p.Contains('/calendar/') -or $n.Contains('calendar')){return @('CALENDAR','Calendar')}; if($p.Contains('addressbook') -or $p.Contains('contacts')){return @('CONTACTS','Contacts')}; return @('OTHER_SQLITE_OR_STORE_DATABASE','Other') }
function Get-EntryKind([string]$Name) { $n=$Name.ToLowerInvariant(); if($n -match '(^|[\\/])\.store\.db$'){return '.store.db'}; if($n -match '(^|[\\/])store\.db$'){return 'store.db'}; if($n -match '[\\/]cache[\\/].*\.txt$'){return 'cache_text'}; if($n.EndsWith('.plist')){return 'plist'}; if($n -match 'index\.spotlightv2'){return 'index_component'}; if($n -match 'bundleinfo'){return 'bundle_info'}; return 'other_corespotlight' }
function Get-LeafNameSafe([string]$path) {
  if ([string]::IsNullOrWhiteSpace($path)) { return '' }
  $p = ([string]$path).Replace('\','/').Trim().TrimEnd('/')
  if ([string]::IsNullOrWhiteSpace($p)) { return '' }
  $parts = $p.Split('/') | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
  if ($parts.Count -eq 0) { return '' }
  return [string]$parts[$parts.Count - 1]
}
function Get-ExtensionSafe([string]$name) {
  if ([string]::IsNullOrWhiteSpace($name)) { return '' }
  $n = [string]$name
  $idx = $n.LastIndexOf('.')
  if ($idx -lt 0 -or $idx -eq ($n.Length - 1)) { return '' }
  return $n.Substring($idx + 1).ToLowerInvariant()
}
function Is-ArchiveMetadataPath([string]$fullName) {
  if ([string]::IsNullOrWhiteSpace($fullName)) { return $true }
  $p = ([string]$fullName).Trim()
  $z = ([string]$InputZip).Trim()
  if ($p -eq $z) { return $true }
  if ($p.Replace('/','\') -eq $z.Replace('/','\')) { return $true }
  if ($p -match '^[A-Za-z]:[\/]') { return $true }
  if ($p -match '^[\\]{2}') { return $true }
  if ($p -match '^[-]+$') { return $true }
  return $false
}
function Get-ZipEntriesVia7z([string]$SevenZip) {
  $entries = New-Object System.Collections.ArrayList
  $script:entries = $entries
  $script:rec = @{}
  function Flush {
    if ($script:rec.ContainsKey('Path')) {
      $p=[string]$script:rec['Path']
      if (-not (Is-ArchiveMetadataPath $p) -and [string]$script:rec['Folder'] -ne '+') {
        $leaf=Get-LeafNameSafe $p
        if($leaf){
          $len=0L; [void][Int64]::TryParse([string]$script:rec['Size'],[ref]$len)
          $mod=''; try{$mod=([DateTimeOffset]::Parse([string]$script:rec['Modified'])).UtcDateTime.ToString('o')}catch{$mod=[string]$script:rec['Modified']}
          [void]$script:entries.Add([pscustomobject]@{FullName=$p;Name=$leaf;Length=$len;CompressedLength=0L;ModifiedUtc=$mod})
        }
      }
    }
    $script:rec=@{}
  }
  & $SevenZip l $InputZip -slt 2>$null | ForEach-Object {
    $line=[string]$_
    if([string]::IsNullOrWhiteSpace($line)){Flush; return}
    $m=[regex]::Match($line,'^([^=]+?)\s=\s(.*)$')
    if($m.Success){$script:rec[$m.Groups[1].Value.Trim()]=$m.Groups[2].Value}
  }
  Flush
  return $entries
}
function Get-ZipEntriesViaDotNet { $entries=New-Object System.Collections.ArrayList; $zip=[System.IO.Compression.ZipFile]::OpenRead($InputZip); try{ foreach($e in $zip.Entries){ if([string]::IsNullOrWhiteSpace($e.FullName) -or [string]::IsNullOrWhiteSpace($e.Name)){continue}; [void]$entries.Add([pscustomobject]@{FullName=$e.FullName;Name=$e.Name;Length=[int64]$e.Length;CompressedLength=[int64]$e.CompressedLength;ModifiedUtc=$e.LastWriteTime.UtcDateTime.ToString('o')}) } } finally { $zip.Dispose() }; return $entries }

New-CleanDirectory $ReportRoot
Remove-Item -LiteralPath $ReportZip,$EvidenceZip -Force -ErrorAction SilentlyContinue
if(Test-Path -LiteralPath $EvidenceRoot){Remove-Item -LiteralPath $EvidenceRoot -Recurse -Force -ErrorAction SilentlyContinue}
$sevenZip = Find-7Zip
$entries = if($sevenZip){ Get-ZipEntriesVia7z $sevenZip } else { Get-ZipEntriesViaDotNet }
$zipItem=Get-Item -LiteralPath $InputZip
@("InputZip=$InputZip","InputZipBytes=$($zipItem.Length)","InputZipSha256=$((Get-FileHash -LiteralPath $InputZip -Algorithm SHA256).Hash)","GeneratedUtc=$((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))","InventoryMethod=$(if($sevenZip){'7z_slt'}else{'dotnet_zipfile'})","TotalFileEntries=$($entries.Count)") | Set-Content -LiteralPath (Join-Path $ReportRoot 'RUN_CONTEXT.txt') -Encoding UTF8

$rows=New-Object System.Collections.ArrayList; $ffsRows=New-Object System.Collections.ArrayList; $dbRows=New-Object System.Collections.ArrayList
foreach($e in $entries){ $name=[string]$e.FullName; $lower=$name.ToLowerInvariant(); $base=Get-LeafNameSafe $name; $norm=Normalize-IosPath $name; [void]$ffsRows.Add([pscustomobject]@{NormalizedPath=$norm;OriginalZipEntry=$name;FileName=$base;Extension=(Get-ExtensionSafe $base);SizeBytes=[int64]$e.Length;ZipModifiedUtc=$e.ModifiedUtc;ProtectionClassHint=Get-ProtectionClassHint $name;AppContainerHint=Get-AppContainerHint $name;DomainHint=Get-DomainHint $name;IsDirectory=0;Sha256Status='not_hashed_zip_entry_inventory';InventoryNotes='central_directory_inventory_7z_or_dotnet'}); $cat=Get-DatabaseCategory $name $base; if($cat[0]){ [void]$dbRows.Add([pscustomobject]@{NormalizedPath=$norm;OriginalZipEntry=$name;DatabaseName=$base;DatabaseCategory=$cat[0];AppHint=$cat[1];ProtectionClassHint=Get-ProtectionClassHint $name;SizeBytes=[int64]$e.Length;ZipModifiedUtc=$e.ModifiedUtc;ParseStatus='identified_not_parsed_quickdiag_v0_8_90';RecordInventoryStatus='database_family_present_record_level_parser_pending';Notes='database-resident records require full ingest or targeted database parsing';ExtractedPath=''}) }; if($lower -match 'corespotlight|spotlightknowledge|bundleinfo'){ [void]$rows.Add([pscustomobject]@{Kind=Get-EntryKind $name;ProtectionClass=Get-ProtectionClass $name;Length=[int64]$e.Length;CompressedLength=[int64]$e.CompressedLength;LastWriteTimeUtc=$e.ModifiedUtc;FullName=$name}) } }
$rowArray=@($rows.ToArray()); $ffsArray=@($ffsRows.ToArray()); $dbArray=@($dbRows.ToArray())
if($rowArray.Count){ $rowArray|Sort-Object FullName|Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $ReportRoot 'ios_corespotlight_entry_inventory.csv'); $rowArray|?{$_.Kind -eq 'store.db' -or $_.Kind -eq '.store.db'}|Sort-Object FullName|Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $ReportRoot 'ios_store_entry_inventory.csv'); $rowArray|?{$_.Kind -eq 'cache_text'}|Sort-Object FullName|Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $ReportRoot 'ios_cache_text_inventory.csv'); $rowArray|Group-Object Kind,ProtectionClass|%{ $parts=$_.Name -split ', ',2; [pscustomobject]@{Kind=$parts[0];ProtectionClass=if($parts.Count -gt 1){$parts[1]}else{''};Count=$_.Count;TotalBytes=($_.Group|Measure-Object Length -Sum).Sum} }|Sort-Object Kind,ProtectionClass|Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $ReportRoot 'ios_corespotlight_entry_summary.csv') } else { Write-EmptyCsv (Join-Path $ReportRoot 'ios_corespotlight_entry_inventory.csv') @('Kind','ProtectionClass','Length','CompressedLength','LastWriteTimeUtc','FullName'); Write-EmptyCsv (Join-Path $ReportRoot 'ios_store_entry_inventory.csv') @('Kind','ProtectionClass','Length','CompressedLength','LastWriteTimeUtc','FullName'); Write-EmptyCsv (Join-Path $ReportRoot 'ios_cache_text_inventory.csv') @('Kind','ProtectionClass','Length','CompressedLength','LastWriteTimeUtc','FullName'); Write-EmptyCsv (Join-Path $ReportRoot 'ios_corespotlight_entry_summary.csv') @('Kind','ProtectionClass','Count','TotalBytes') }
if($ffsArray.Count){$ffsArray|Sort-Object NormalizedPath|Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $ReportRoot 'ios_ffs_file_inventory.csv')} else {Write-EmptyCsv (Join-Path $ReportRoot 'ios_ffs_file_inventory.csv') @('NormalizedPath','OriginalZipEntry','FileName','Extension','SizeBytes','ZipModifiedUtc','ProtectionClassHint','AppContainerHint','DomainHint','IsDirectory','Sha256Status','InventoryNotes')}
if($dbArray.Count){$dbArray|Sort-Object DatabaseCategory,AppHint,NormalizedPath|Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $ReportRoot 'ios_app_database_inventory.csv')} else {Write-EmptyCsv (Join-Path $ReportRoot 'ios_app_database_inventory.csv') @('NormalizedPath','OriginalZipEntry','DatabaseName','DatabaseCategory','AppHint','ProtectionClassHint','SizeBytes','ZipModifiedUtc','ParseStatus','RecordInventoryStatus','Notes','ExtractedPath')}

if($IncludeStoreFiles -or $IncludeCacheTextSamples){ New-CleanDirectory $EvidenceRoot; if($sevenZip){ $patterns=@(); if($IncludeStoreFiles){$patterns += @('-ir!*store.db','-ir!*.store.db')}; if($IncludeCacheTextSamples){$patterns += @('-ir!*cache*.txt')}; & $sevenZip x $InputZip "-o$EvidenceRoot" @patterns -y | Out-File -FilePath (Join-Path $ReportRoot 'minimal_evidence_extract_7z.log') -Encoding UTF8 } else { $zip=[System.IO.Compression.ZipFile]::OpenRead($InputZip); try{ foreach($row in $rowArray){ if(($IncludeStoreFiles -and ($row.Kind -eq 'store.db' -or $row.Kind -eq '.store.db')) -or ($IncludeCacheTextSamples -and $row.Kind -eq 'cache_text')){ $entry=$zip.GetEntry([string]$row.FullName); if($entry){ $rel=$entry.FullName.Replace('/','\').TrimStart('\'); $dest=Join-Path $EvidenceRoot $rel; New-Item -ItemType Directory -Force -Path (Split-Path $dest -Parent)|Out-Null; [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$dest,$true) } } } } finally{$zip.Dispose()} }; New-ZipFromDirectorySafe $EvidenceRoot $EvidenceZip; Get-FileHash -LiteralPath $EvidenceZip -Algorithm SHA256 | Format-List | Out-File -FilePath (Join-Path $ReportRoot 'MINIMAL_EVIDENCE_ZIP_SHA256.txt') -Encoding UTF8 }
New-ZipFromDirectorySafe $ReportRoot $ReportZip
Get-Item -LiteralPath $ReportZip | Select-Object FullName,Length,LastWriteTime
Get-FileHash -LiteralPath $ReportZip -Algorithm SHA256
if(Test-Path -LiteralPath $EvidenceZip){Get-Item -LiteralPath $EvidenceZip | Select-Object FullName,Length,LastWriteTime; Get-FileHash -LiteralPath $EvidenceZip -Algorithm SHA256}
if(!$NoClipboardOrExplorer){ try{Set-Clipboard -Value $ReportZip}catch{}; try{explorer.exe /select,$ReportZip|Out-Null}catch{} }
