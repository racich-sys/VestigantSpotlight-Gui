# Vestigant Spotlight Consolidated Version History

Current version: 0.9.48

## V0_9_48

V0_9_48 reviewed the uploaded V0_9_46 Windows/MSVC build, reuse-cache thin upload, Stage B fresh-ZIP thin upload, and new parser recommendations. V0_9_46 was stable: source and binary versions matched, both runs completed successfully, fresh-ZIP inventory remained at 2,245,783 files, and app database candidates remained narrowed to 5,528. The next improvement class was investigative value rather than a stability hotfix.

This release adds conservative bplist object-string discovery for ASCII and UTF-16BE `bplist00` string objects, KnowledgeC/CoreDuet database identification and support-mode parser scaffolding, KnowledgeC summary/event review views, and an explicit investigator time-anomaly triage view. It intentionally preserves compact normal iOS mode: no broad FFS materialization and no broad app database record materialization by default. Full LZFSE/LZVN integration remains deferred until the Apple reference codec source is explicitly added and build integration can be validated.

## V0_9_45

V0_9_45 reviewed V0_9_44 Windows build plus reuse-cache and Stage B fresh-ZIP thin outputs. Both runs completed successfully. The fresh-ZIP native 7-Zip inventory parser now produced nonzero FFS inventory rows, but it over-classified many non-database files as app databases because several path-based rules did not require a database-like filename. This release narrows iOS app database inventory classification to database-like file names before applying Messages, Signal, Telegram, CallHistory, WebKit, Safari, Chrome/Google, Mail, Calendar, Contacts, Keychain, WhatsApp, and other database-family labels. It also preserves targeted extraction paths in the native C++ inventory CSV when the extracted database exists under EvidenceStaging/ios_app_databases, so future support-mode app DB record parsing has usable paths after a fresh ZIP run.

## V0_9_44

V0_9_44 reviewed V0_9_43 Windows build, reuse-cache thin output, and Stage B fresh-ZIP thin output. The fresh-ZIP path completed but produced an empty FFS/app-database inventory, showing the native C++ parser did not parse the raw 7-Zip listing. This release changes the PowerShell helper to use `cmd.exe` redirection for raw `7z l -slt` output, adds native fallback decoding for UTF-16 raw listing lines, reports a warning if the raw listing parses to zero rows, and preserves the compact normal iOS mode.

Additional parser improvements include zero-copy bounded metadata item parsing from decompressed metadata-block payloads, improved fallback probe deduplication/non-ASCII preservation, and safer removal of CoreSpotlight string trailer markers after trimming padding.

## V0_9_43

Added bounded iOS CoreSpotlight bplist / NSKeyedArchiver marker discovery. The release detected likely binary plist / NSKeyedArchiver values, extracted bounded printable-token context, stored compact synthetic context rows, and exposed summary/detail views and CSV exports. It intentionally did not perform full NSKeyedArchiver graph decoding.

## Earlier V0_9 history

Earlier V0_9 releases focused on compact normal iOS mode, DB/WAL guardrails, iOS investigator views, message/media/text review surfaces, parser limits/suppression reporting, GUI stability, CSV export optimization, reuse-cache workflows, and initial Stage B fresh-ZIP preparation. Preserve prior detailed history from earlier packages if litigation/reporting requires exact version-by-version reconstruction.
