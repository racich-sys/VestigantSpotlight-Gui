# Vestigant Spotlight Consolidated Version History

Current version: 0.9.46


## V0_9_46

V0_9_46 reviewed the uploaded V0_9_45 Windows build plus reuse-cache and Stage B fresh-ZIP thin outputs. Both runs completed successfully. V0_9_45 fixed the broad app-database inventory issue, reducing the fresh-ZIP app database candidate set to 5,528 while preserving extracted paths for high-value staged databases such as sms.db, WhatsApp ChatStorage.sqlite/ContactsV2.sqlite, Safari databases, Contacts, Calendar, CallHistory, and Keychain targets. The follow-up issue was classification precision: files containing generic words such as `signals` or `history` could still be mislabeled as Signal or Chrome/Web evidence. V0_9_46 tightens Signal and Chrome/Web categorization to stronger app/path indicators and fixes stale top-level version files that caused the build script banner to report source version 0.9.44 even though the binary built as 0.9.45.

## V0_9_45

V0_9_45 reviewed V0_9_44 Windows build plus reuse-cache and Stage B fresh-ZIP thin outputs. Both runs completed successfully. The fresh-ZIP native 7-Zip inventory parser now produced nonzero FFS inventory rows, but it over-classified many non-database files as app databases because several path-based rules did not require a database-like filename. This release narrows iOS app database inventory classification to database-like file names before applying Messages, Signal, Telegram, CallHistory, WebKit, Safari, Chrome/Google, Mail, Calendar, Contacts, Keychain, WhatsApp, and other database-family labels. It also preserves targeted extraction paths in the native C++ inventory CSV when the extracted database exists under EvidenceStaging/ios_app_databases, so future support-mode app DB record parsing has usable paths after a fresh ZIP run.

## V0_9_44

V0_9_44 reviewed V0_9_43 Windows build, reuse-cache thin output, and Stage B fresh-ZIP thin output. The fresh-ZIP path completed but produced an empty FFS/app-database inventory, showing the native C++ parser did not parse the raw 7-Zip listing. This release changes the PowerShell helper to use `cmd.exe` redirection for raw `7z l -slt` output, adds native fallback decoding for UTF-16 raw listing lines, reports a warning if the raw listing parses to zero rows, and preserves the compact normal iOS mode.

Additional parser improvements include zero-copy bounded metadata item parsing from decompressed metadata-block payloads, improved fallback probe deduplication/non-ASCII preservation, and safer removal of CoreSpotlight string trailer markers after trimming padding.

## V0_9_43

Added bounded iOS CoreSpotlight bplist / NSKeyedArchiver marker discovery. The release detected likely binary plist / NSKeyedArchiver values, extracted bounded printable-token context, stored compact synthetic context rows, and exposed summary/detail views and CSV exports. It intentionally did not perform full NSKeyedArchiver graph decoding.

## Earlier V0_9 history

Earlier V0_9 releases focused on compact normal iOS mode, DB/WAL guardrails, iOS investigator views, message/media/text review surfaces, parser limits/suppression reporting, GUI stability, CSV export optimization, reuse-cache workflows, and initial Stage B fresh-ZIP preparation. Preserve prior detailed history from earlier packages if litigation/reporting requires exact version-by-version reconstruction.
