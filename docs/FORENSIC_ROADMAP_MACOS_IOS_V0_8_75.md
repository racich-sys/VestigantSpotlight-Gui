# Vestigant Spotlight Forensic Roadmap - macOS and iOS v0.8.75

## Current macOS/AFF4/APFS position

The macOS path remains in APFS extraction-validation hardening. V0_8_74 reached 13,050 exact relative-path/hash matches against the BADA95B6 external Store-V2 reference, with 47 remaining relative-path size mismatches and 1,734 external-only rows. The next APFS work should continue from the thin compare and remaining-mismatch diagnostics rather than broad rewrites.

### Near-term macOS priorities

1. Classify the remaining BADA95B6 Cache `.txt` mismatches into data-fork, ResourceFork, decmpfs, unsupported codec, FILE_EXTENT, and comparison/reference classes.
2. Keep APFS provenance for object ID, parent object ID, relative Store-V2 path, FILE_EXTENT status, xattrs, decmpfs algorithm, ResourceFork stream object/size, and reconstruction status.
3. Add cached external reference manifest support so future external-reference comparisons do not rehash the baseline every run.
4. Start a normalized searchable value table once extraction validation stabilizes.

## Current iOS/CoreSpotlight position

The iOS parser can parse focused CoreSpotlight extracts through the CLI, but the GUI test against a 41 GB FFS ZIP produced zero stores because the GUI used generic ZIP extraction and did not route the iOS FFS ZIP through focused CoreSpotlight extraction. V0_8_75 addresses that GUI gap.

### V0_8_75 iOS GUI milestone

Definition of done for the next validation run:

1. GUI iOS/CoreSpotlight profile accepts a full iOS FFS ZIP as input.
2. GUI ZIP staging extracts only CoreSpotlight/BundleInfo entries into EvidenceStaging rather than expanding the entire FFS ZIP.
3. `ios_input_store_entry_inventory.csv` lists store.db/.store.db entries found during GUI staging.
4. Store discovery finds valid CoreSpotlight databases from the focused staging folder.
5. Parser selection selects all valid iOS store.db/.store.db candidates.
6. iOS Investigation tab opens Store Summary, String Values, String Categories, Index Timeline, iOS Artifacts, and Relevant Fields views.
7. If there are no rows, logs/status identify whether extraction, discovery, parsing, or GUI view wiring failed.

### iOS investigation roadmap

1. Improve native CoreSpotlight field decoding beyond generic probe strings.
2. Preserve source ZIP entry, protection class, store path, store role, record IDs, raw field names, decoded values, and Last_Updated/index dates.
3. Add investigator keyword search across decoded values, raw probe strings, URLs, email/message-like strings, file URLs, calendar/invitation text, and metadata.
4. Build iOS FFS file inventory for residency correlation.
5. Parse high-value app databases for source-record checks, beginning with Messages/SMS/iMessage, WhatsApp, Mail, Calendar/Reminders, Files/iCloud Drive, and cloud-storage apps.
6. Use conservative residency statuses: SOURCE_RECORD_FOUND, SOURCE_FILE_FOUND, SOURCE_DB_FOUND_RECORD_NOT_FOUND, SOURCE_FILE_NOT_FOUND, INDEXED_ONLY_CANDIDATE, UNSUPPORTED_APP_CORRELATION, and UNRESOLVED_INSUFFICIENT_PROVENANCE.

## V0_8_76 parity update

Going forward, CLI parser/intake improvements must be evaluated for GUI parity. V0_8_76 aligns iOS GUI profile behavior with the CLI helper by enabling full-scan/focused CoreSpotlight selection for GUI iOS runs and adding a database-backed keyword-search values view accessible from the GUI.
