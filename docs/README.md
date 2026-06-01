# Vestigant Spotlight Documentation

Current version: V0_8_99.

Primary documents:

- `USER_MANUAL.md` / `USER_MANUAL_V0_8_99.md`
- `QUICK_START.md` / `QUICK_START_V0_8_99.md`
- `TROUBLESHOOTING.md` / `TROUBLESHOOTING_V0_8_99.md`
- `RELEASE_NOTES.md`
- `VALIDATION_HISTORY.md`
- `PROJECT_ROADMAP_AND_CONTINUATION.md`
- `IOS_CORESPOTLIGHT_ROADMAP.md`

Release notes are now consolidated. Separate `RELEASE_NOTES_V*.md` fragments are intentionally not included in production packages.


## V0_8_99 iOS investigation exports

V0_8_99 adds three iOS-first pivot exports and GUI views for investigator review:

- `ios_protection_class_summary.csv`: summarizes record counts, string-probe counts, selected database counts, and date ranges by iOS protection class.
- `ios_artifact_hint_summary.csv`: groups decoded string probes into investigator-oriented buckets such as Mail AttachmentData paths, iCloud/CloudDocs, Google Drive/Docs, Microsoft Teams/OneDrive, Zoom, map links, calendar invitations, web links, iOS file paths, email/account text, and message/attachment text.
- `ios_record_investigation_hints.csv`: provides per-record protection class, primary investigation hint, hint categories, string-probe count, and index-update timing without embedding raw string samples in that rollup. Raw decoded string values remain in `ios_string_probe_values.csv`.

`Last_Updated` remains metadata/index update timing and must not be treated as usage without supporting decoded fields.


## V0_8_99 note

V0_8_99 fixes the iOS FFS/app database ZIP-entry inventory failure observed in V0_8_88_1 by avoiding Windows path APIs for iOS ZIP entry names. The expected result is nonzero FFS inventory rows and, where relevant databases exist, nonzero app database inventory/table-count rows. Full iOS inventory CSVs may be large; the thin upload script samples oversized CSVs and leaves the complete files in the local case folder.
