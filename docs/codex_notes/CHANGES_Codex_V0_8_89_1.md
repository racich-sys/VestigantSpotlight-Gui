# Codex V0_8_99 Changes

This patch moves the iOS app-database workflow beyond table/count inventory while keeping interpretation conservative.

## Added

- Generic read-only parsing for staged iOS SQLite app databases with high-value table families:
  - messages
  - message attachments
  - participants
  - calls
  - web/Safari history
  - mail
  - calendar
  - contacts
  - chat-style tables
- New case table: `ios_app_parsed_records`.
- New exports and GUI views:
  - `ios_app_parsed_records.csv`
  - `ios_app_parsed_record_summary.csv`
- Database residency view now reports `POTENTIAL_PARSED_APP_RECORDS_AVAILABLE` when parsed app records exist for the relevant database family.
- Thin upload script includes/samples the new parsed app-record exports.

## Improved

- `ios_database_residency_candidates.csv` is aggregated to avoid multiplying every Spotlight candidate by every matching app-database table-count row.
- CSV export writing now handles deep Windows paths more safely and avoids adding temporary part filenames unless an export actually chunks.
- Self-test file checks handle long Windows paths.

## Still Conservative

- Parsed app-database rows are review leads, not proof of deletion, usage, or exact Spotlight-to-row correspondence.
- App-specific deleted/live semantics, message-thread reconstruction, attachment carving, and exact row-level correlation remain future work.

## Validation Performed

- MSVC build completed.
- CLI version reports `Vestigant Spotlight v0.8.99`.
- Built-in self-test passed.
- Synthetic iOS ZIP containing `private/var/mobile/Library/SMS/sms.db` completed with `parsed_app_records=2` and populated `ios_app_parsed_record_summary.csv`.
