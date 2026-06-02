# Vestigant Spotlight Release Notes

Current version: 0.9.30

## V0_9_30

- Reviewed V0_9_29 build/thin output before changing code. The Windows/MSVC build completed, GUI linked, and the iOS reuse-cache run reached `complete_success` with stable compact counts.
- Consolidated stalled/scattered help into `docs/CONSOLIDATED_USER_MANUAL.md`.
- Consolidated current version history into `docs/CONSOLIDATED_VERSION_HISTORY.md`.
- Updated top-level `HELP.md`, `RELEASE_NOTES.md`, and `VERSION_HISTORY.md` to point to the consolidated documentation.
- Improved compact iOS Spotlight message/body review extraction from same-record Spotlight text context, including mail/message title, snippet, description, supporting text, and suggested contact/thread context where present.
- Added parser diagnostics detail view/export so native failures and partial decode errors are visible at record/sample level, not only as summary counts.
- Added GUI view `iOS - Parser Diagnostics Detail Sample`.
- Added normal export `parser_diagnostics_detail_sample.csv`.
- Preserved compact normal iOS mode; full native/dbStr/property persistence and broad FFS/app DB materialization remain support/diagnostic options.

Validation in this environment:

- C++ syntax checks for modified files.
- SQLite schema smoke test expanded to include the new diagnostics detail view.
- Raw-string fragment length check to reduce recurrence of MSVC C2026 oversized literal failures.
- Linux build/self-test attempted where feasible; Windows/MSVC remains required validation.
