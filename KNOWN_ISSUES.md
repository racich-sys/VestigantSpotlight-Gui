## V0_9_17 - iOS CoreSpotlight output and database bloat control

V0_9_17 fixes the V0_9_16 large-case regression where successful iOS dbStr/property decoding generated too many low-level raw key/value rows and massive support CSVs during normal investigator/reuse-cache runs. Normal iOS runs now keep high-value raw key/value rows by default, preserve date candidates for provenance, and move full native/dbStr/support exports behind explicit diagnostic/support modes. Added `--diagnostic-full-native-db` for bounded support runs that intentionally need full native key/value persistence.

## V0_9_17 known issues / interpretation limits

- `dbStr-*` decoding is new and must be validated against the next thin upload before conclusions are drawn from named-property coverage.
- Missing `dbStr-*` files should be treated as an acquisition/staging/parser input issue, not as proof that the original device lacked property dictionaries.
- Apple public Core Spotlight field matching is semantic interpretation only. Raw record IDs, store paths, raw field names, raw values, parse methods, and source locators remain the validation source.
- The GPL `spotlight_parser` reference was not copied into this project; discrepancies against mac_apt/spotlight_parser should be reviewed as parser-behavior deltas.

# Known Issues / Limitations - V0_9_1

- iOS `Last_Updated` is metadata/index update timing, not usage by itself.
- V0_9_1 adds generic parsed-row extraction for selected high-value SQLite table families, but it does not yet implement app-specific deleted/live semantics, message-thread reconstruction, attachment reconstruction, or exact Spotlight-to-row correlation for SMS, calls, WhatsApp, Signal, Telegram, Safari/WebKit/Chrome, Mail, Calendar, Contacts, or FileProvider databases.
- Missing-from-FFS candidates are path-inventory correlations only. They may represent deletion, acquisition scope/protection limits, cache purge, path-normalization mismatch, or parser limitation.
- CoreSpotlight dbStr/property/category map decoding remains incomplete.
- Windows/MSVC GUI validation must still be performed after downloading the source ZIP.


## V0_9_1 note

V0_9_1 fixes the iOS FFS/app database ZIP-entry inventory failure observed in V0_8_88_1 by avoiding Windows path APIs for iOS ZIP entry names. The expected result is nonzero FFS inventory rows and, where relevant databases exist, nonzero app database inventory/table-count rows. Full iOS inventory CSVs may be large; the thin upload script samples oversized CSVs and leaves the complete files in the local case folder.

- AFF4/APFS direct-reader work remains diagnostic/experimental and does not yet provide full V0_8_74 copy-out/staging parity for BlackBag/LZ4 AFF4 images.

## V0_9_1 iOS app-database interpretation update

V0_9_1 adds `ios_apple_messages_database_status.csv` and `ios_app_live_activity_timeline.csv`. The Apple Messages status export distinguishes an SMS.db that is present but has zero live message/chat/attachment rows from a parser failure. Database-residency candidates are now aggregated by database family and exclude WAL/SHM artifacts to reduce duplicate leads.



## V0_9_1 source-scoped iOS object linking

V0_9_1 adds active-source cleanup before export and three iOS object-linking exports: `ios_spotlight_object_identity.csv`, `ios_spotlight_to_ffs_object_links.csv`, and `ios_spotlight_to_app_db_record_links.csv`. These views are intended to help link CoreSpotlight IDs/path fragments/string probes back to FFS paths and parsed app database families. Correlation remains conservative unless exact app database row matching is available.

## V0_9_1 iOS residency classification note

`ios_database_residency_candidates.csv` is intentionally conservative. A database-family match is a lead, not proof that the exact Spotlight value still exists in that database. Stronger conclusions require future row-level matching against parsed app database records and improved CoreSpotlight property decoding.

- V0_9_1 human-readable iOS text views are based on generic CoreSpotlight string probes; exact property names and exact app-database row matching still require later dbStr/property-map decoding and schema-specific app DB parsers.

- V0_9_1 attempts to suppress visible helper console windows during GUI-launched iOS ZIP processing, but Windows/MSVC GUI validation is required to confirm no helper stage still opens a visible console.

## V0_9_1 WhatsApp / keychain update

V0_9_1 adds the first schema-aware iOS WhatsApp review layer. The parser uses the uploaded WhatsApp/iLEAPP reference material as the trusted local schema reference for iOS WhatsApp databases and targets `ChatStorage.sqlite`, `ContactsV2.sqlite`, and WhatsApp `CallHistory.sqlite` where those databases are present in the iOS FFS ZIP. New GUI/export views include `iOS - WhatsApp Database Status`, `iOS - WhatsApp Parsed Records`, and `iOS - WhatsApp Parsed Summary`.

V0_9_1 also adds an inventory-only `iOS - Keychain Material Inventory` view and export. Keychain/keybag presence is reported as acquisition context only; the tool does not yet claim WhatsApp decryption or key extraction.

V0_9_1 tightens WhatsApp database classification so unrelated files whose filenames contain `whatsapp`, such as web assets or SVG logos, are not treated as WhatsApp app databases.

## V0_9_6 large iOS reuse-cache note

The `--reuse-ios-cache` workflow is designed for repeat testing against very large iOS FFS ZIPs. For fastest reuse, keep the prior completed cache case folder intact, including `VestigantSpotlight.case.sqlite` or `spotlight_case.db`, `EvidenceStaging`, `ios_ffs_file_inventory.csv`, and `ios_app_database_inventory.csv`. If the prior SQLite case database is unavailable, v0.9.6 falls back to streaming CSV import, which is safer than the previous in-memory path but still slower than the cache-SQLite fast path.

## V0_9_17 note
V0_9_15 could over-expand iOS exports when dbStr property decoding increased raw key/value counts. V0_9_17 limits normal investigator exports to Spotlight-first review surfaces and makes broad support/native diagnostics opt-in.
