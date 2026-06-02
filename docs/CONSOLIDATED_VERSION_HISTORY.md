# Vestigant Spotlight Consolidated Version History

Current version: 0.9.31

## V0_9_31

V0_9_31 continues the iOS investigator workflow after V0_9_30 completed successfully. It reduces oversized contact/thread summary output by grouping handles into review buckets, adds bounded contact/thread detail sampling, adds a message/body focus summary, adds parser diagnostic severity and recommended-action guidance, and introduces a Plaso/L2T-compatible timeline sample export.

## V0_9_31

- Reviewed V0_9_29 build/thin output: Windows/MSVC build completed, GUI linked, self-test passed, and the iOS reuse-cache run reached `complete_success` with stable compact counts.
- Consolidated user help into `docs/CONSOLIDATED_USER_MANUAL.md` and version history into this document.
- Updated top-level `HELP.md`, `RELEASE_NOTES.md`, and `VERSION_HISTORY.md` to point to the consolidated documentation instead of leaving separate stalled fragments.
- Improved `iOS - Spotlight Message Body Review` so message/mail body, subject, snippet, supporting text, and thread/contact context are extracted from compact same-record Spotlight context when available.
- Added `iOS - Parser Diagnostics Detail Sample` and `parser_diagnostics_detail_sample.csv` so unsupported/unparsed native parser details are visible, not only summarized.
- Preserved compact normal iOS mode and did not reintroduce broad native/dbStr/FFS/app DB materialization.

## V0_9_29

- Added schema/view smoke-test coverage for key iOS and diagnostics views.
- Added parser diagnostics summary, case provenance summary, normalized iOS Spotlight timeline, timeline anomaly summary, message body review, user-focus message review, message contact summary, and noise-reduction summary.
- Preserved compact normal iOS mode.

## V0_9_28

- Fixed GUI SQL helper placement/build issue from V0_9_27.
- Added iOS Spotlight Message Text Review and Message Media Review.
- Improved direct extraction of iOS Messages text using `kMDItemAppEntityTitle` and separated message-adjacent media from direct message records.

## V0_9_27

- Incorporated prior GUI SQL literal splitting fix.
- Added record-centric iOS Spotlight communications review, communication summary, and attachment/media reference review.

## V0_9_26_2 / V0_9_26_1

- Fixed MSVC C2026 oversized SQL raw-string literal failures in database and GUI files.
- Confirmed CLI/self-test could build from V0_9_26_1 while GUI needed V0_9_26_2.

## V0_9_26

- Refined chat-app attribution to separate explicit bundle/domain/external-id evidence from generic keyword/link mentions.
- Added repeatable thin-upload review workflow documentation.

## V0_9_25

- Added priority-ranked iOS Spotlight text context views and cleaner thin upload packaging.

## V0_9_24

- Added parser limits/suppression report and iOS Spotlight text context review.
- Made compact/even-looking counts explicit and defensible.

## V0_9_21 to V0_9_23_1

- Resolved DB/WAL growth and export-stall issues by moving broad raw/native/date/object exports to support/diagnostic mode and using compact normal iOS investigator views.
- Added slim FFS lookup for Missing From FFS without full FFS materialization.
- Added high-value Missing From FFS prioritization.

## V0_9_15 to V0_9_20

- Added iOS CoreSpotlight dbStr/property decoding and then corrected the resulting database/materialization over-expansion.
- Added explicit diagnostic flags for full native persistence and guardrails for DB/WAL growth.

## Earlier versions

Earlier versions established macOS Store-V2 parsing, AFF4/APFS staging/external compare work, iOS CoreSpotlight discovery, GUI review tabs, evidence-source staging, ZIP intake, and thin-upload review workflows. Historical fragments remain in the repository for traceability, but current user guidance should start with the consolidated manual.
