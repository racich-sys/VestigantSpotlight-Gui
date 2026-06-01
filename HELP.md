## V0_9_17 - iOS CoreSpotlight output and database bloat control

V0_9_17 fixes the V0_9_16 large-case regression where successful iOS dbStr/property decoding generated too many low-level raw key/value rows and massive support CSVs during normal investigator/reuse-cache runs. Normal iOS runs now keep high-value raw key/value rows by default, preserve date candidates for provenance, and move full native/dbStr/support exports behind explicit diagnostic/support modes. Added `--diagnostic-full-native-db` for bounded support runs that intentionally need full native key/value persistence.

## V0_9_17 quick help

V0_9_17 focuses on iOS CoreSpotlight dictionary decoding. Use the normal iOS/CoreSpotlight reuse-cache workflow to validate whether `dbStr-*` map files are present and whether property/category dictionaries populate. Review the new iOS views: `Spotlight dbStr Map Inventory`, `Spotlight Dictionary Coverage`, and `Spotlight Apple Field Coverage`.

# V0_9_6 update

V0_9_6 adds reusable iOS source-intake/cache support for very large iOS FFS ZIP testing. Use `--reuse-ios-cache <completed-case-folder>` or, in the GUI, select `iOS/CoreSpotlight` and place the prior completed case folder in the `Evidence root / iOS cache case` field. This is intended for parser/enrichment iteration against the same source ZIP after a successful baseline run such as `Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4`. Reuse mode writes `source_cache_manifest.json` and `logs\ios_reuse_cache.log` and avoids re-listing/re-extracting the 250+ GiB ZIP.

# Vestigant Spotlight Help

Version: V0_9_1

## Current iOS focus

V0_9_1 is focused on iOS investigation workflow while carrying forward guarded AFF4/APFS direct-reader diagnostics from Codex work. Use the iOS/CoreSpotlight profile for iOS full-file-system ZIPs.

Default iOS test input:

```text
T:\0202_0024-IT002\00008132-000269523699001C_files_full.zip
```

Default V0_9_1 case output:

```text
Q:\SpotlightCase\TestiOS_V0_9_1
```

## GUI workflow

1. Build the project with `scripts\Build-V0_9_1.ps1` or the root `build_windows_msvc.bat`.
2. Launch `T:\VestigantSpotlightInv_V0_9_1\build-msvc\Release\VestigantSpotlight.exe`.
3. Use:
   - Source type: ZIP
   - Profile: iOS/CoreSpotlight
   - Input: `T:\0202_0024-IT002\00008132-000269523699001C_files_full.zip`
   - Case location: `Q:\SpotlightCase\TestiOS_V0_9_1`
   - Mode: Process Raw Spotlight Evidence
   - Full native values: enabled
   - Export profile: Investigator
4. Open the iOS Investigation tab and use the iOS-specific buttons.

## iOS residency views

V0_9_1 adds:

- iOS - FFS File Inventory
- iOS - App Database Inventory
- iOS - Spotlight Referenced Paths
- iOS - Missing From FFS Candidates
- iOS - Residency Summary
- iOS - Database Residency Candidates

Interpretation rule: missing from FFS inventory does not by itself prove deletion. It means a Spotlight-derived path/reference was not found in the enumerated FFS ZIP path inventory. Database-resident content must be checked against parsed app databases in a later phase.

## Fast no-GUI diagnostic path

Run:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V0_9_1\scripts\Run-V0_9_1-iOS-QuickDiagnostics.ps1
```

This creates quick diagnostic ZIPs in `D:\Downloads` and now includes FFS path inventory plus app database family inventory.


## V0_9_1 note

V0_9_1 fixes the iOS FFS/app database ZIP-entry inventory failure observed in V0_8_88_1 by avoiding Windows path APIs for iOS ZIP entry names. The expected result is nonzero FFS inventory rows and, where relevant databases exist, nonzero app database inventory/table-count rows. Full iOS inventory CSVs may be large; the thin upload script samples oversized CSVs and leaves the complete files in the local case folder.


## GitHub automation

V0_9_1 includes `.github/workflows/windows-msvc-build.yml` plus GitHub issue/roadmap helper scripts. The intended stable repository path remains `T:\VestigantSpotlight`; versioned source packages can be synced into that repo with `scripts\Sync-Version-To-GitRepo.ps1`.

## V0_9_1 iOS app-database interpretation update

V0_9_1 adds `ios_apple_messages_database_status.csv` and `ios_app_live_activity_timeline.csv`. The Apple Messages status export distinguishes an SMS.db that is present but has zero live message/chat/attachment rows from a parser failure. Database-residency candidates are now aggregated by database family and exclude WAL/SHM artifacts to reduce duplicate leads.



## V0_9_1 source-scoped iOS object linking

V0_9_1 adds active-source cleanup before export and three iOS object-linking exports: `ios_spotlight_object_identity.csv`, `ios_spotlight_to_ffs_object_links.csv`, and `ios_spotlight_to_app_db_record_links.csv`. These views are intended to help link CoreSpotlight IDs/path fragments/string probes back to FFS paths and parsed app database families. Correlation remains conservative unless exact app database row matching is available.

## V0_9_1 WhatsApp / keychain update

V0_9_1 adds the first schema-aware iOS WhatsApp review layer. The parser uses the uploaded WhatsApp/iLEAPP reference material as the trusted local schema reference for iOS WhatsApp databases and targets `ChatStorage.sqlite`, `ContactsV2.sqlite`, and WhatsApp `CallHistory.sqlite` where those databases are present in the iOS FFS ZIP. New GUI/export views include `iOS - WhatsApp Database Status`, `iOS - WhatsApp Parsed Records`, and `iOS - WhatsApp Parsed Summary`.

V0_9_1 also adds an inventory-only `iOS - Keychain Material Inventory` view and export. Keychain/keybag presence is reported as acquisition context only; the tool does not yet claim WhatsApp decryption or key extraction.

V0_9_1 tightens WhatsApp database classification so unrelated files whose filenames contain `whatsapp`, such as web assets or SVG logos, are not treated as WhatsApp app databases.

## V0_9_6 reuse-cache acceleration

For large iOS FFS ZIP retesting, use `--reuse-ios-cache <completed-case-folder>`. Version 0.9.6 first tries to import reusable iOS inventory tables directly from the prior case SQLite database. This avoids relisting the ZIP and avoids reparsing the full inventory CSV when the prior case database is present. If the prior database is not available, the tool falls back to streaming CSV import and logs progress in `run_status.txt`.
