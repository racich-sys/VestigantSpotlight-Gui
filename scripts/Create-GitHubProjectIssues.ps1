Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Repo = "racich-sys/VestigantSpotlight"
# Requires: gh auth login
# Optional project scope: gh auth refresh -s project

$Issues = @(
  @{Title="V0_9_6 roadmap - Extend iOS WhatsApp parsing and keychain-aware review"; Labels="ios,app-db,whatsapp"; Body="Goal: Validate and extend schema-aware iOS WhatsApp ChatStorage/Contacts/CallHistory parsing using uploaded iLEAPP/WhatsApp references. Acceptance: WhatsApp database status and parsed records views populate when databases are present; keychain material inventory reports presence without claiming decryption."},
  @{Title="V0_9_6 roadmap - Parse Apple Messages SMS.db record content"; Labels="ios,app-db"; Body="Goal: Add read-only staged SMS.db parsing for Apple Messages.`n`nAcceptance: Windows build passes; iOS GUI ingest completes; parsed records export/view has nonzero rows when SMS.db exists; missing/residency correlation can identify Spotlight-only DB-record candidates."},
  @{Title="V0_9_6 roadmap - Add iOS Spotlight-to-app-DB record correlation"; Labels="ios,missing-from-ffs,app-db"; Body="Goal: Correlate CoreSpotlight strings/paths/URLs/timestamps against parsed app database records.`n`nAcceptance: Adds PRESENT_AS_RECORD_IN_APP_DB and SPOTLIGHT_ONLY_DB_RECORD_MISSING statuses; supports Apple Messages first; confidence language stays conservative."},
  @{Title="V0_9_6 roadmap - Decode CoreSpotlight dbStr property/category maps"; Labels="ios,corespotlight"; Body="Goal: Map generic string probes into real CoreSpotlight property/category names using dbStr map/index files.`n`nAcceptance: native_property_dictionary populates; string probes gain mapped field names where recoverable; iOS views prefer mapped fields over generic probes."},
  @{Title="V0_9_6 roadmap - Add investigator keyword search across iOS Spotlight and app DB records"; Labels="ios,gui"; Body="Goal: Add keyword search over decoded CoreSpotlight values, paths, URLs, domains, message-like strings, and parsed app database rows.`n`nAcceptance: Results show source store/database/table/row/protection class/timestamp/confidence/residency status."},
  @{Title="Future - Wire direct AFF4 reader into V0.8.74 APFS copy-out/staging path"; Labels="aff4-apfs,codex"; Body="Goal: Use direct AFF4 ZIP/map/index/LZ4 reader as virtual block reader and feed it into mature APFS traversal/copy-out/staging logic.`n`nAcceptance: Avoids AFF4_open hang; reaches Spotlight target names; produces staged Store-V2 files when extents/xattrs/decmpfs/resource forks can be resolved."}
)
foreach ($Issue in $Issues) { gh issue create --repo $Repo --title $Issue.Title --label $Issue.Labels --body $Issue.Body }
