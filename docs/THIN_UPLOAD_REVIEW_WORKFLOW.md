## V0_9_28 repeatable review checkpoint

For V0_9_28 and later, always inspect the new communication review outputs during thin-upload review:
- exports/ios_spotlight_communication_summary.csv
- exports/ios_spotlight_communication_record_review_sample.csv
- exports/ios_spotlight_attachment_reference_review_sample.csv
- exports/upload_samples/ios_spotlight_communication_summary_sample.csv
- exports/upload_samples/ios_spotlight_communication_record_review_sample.csv

Compare these against parser_limits_and_suppression_summary.csv, CASE_REVIEW_SUMMARY.txt, and prior high-value text context samples to decide whether the next fix should improve property mapping, date interpretation, app attribution, or GUI usability.



## V0_9_26_2

- Compile-fix release after Windows/MSVC built CLI/tests but failed GUI at `src\gui\win32_gui.cpp(1243): error C2026: string too big, trailing characters truncated`.
- Refactored oversized GUI SQL literal blocks in `win32_gui.cpp` into runtime-joined smaller raw string segments via `execGuiSqlParts(...)`.
- Preserves V0_9_26_1 SQL/view behavior; intended change is MSVC GUI compatibility only.
- Static check: large GUI/database SQL raw string bodies are now split below MSVC string-literal limits.
# Thin upload review workflow

Use this sequence for each Vestigant Spotlight / Spotlight2 build-log and thin-upload cycle.

1. Confirm the uploaded build log compiled the expected version and note warnings/errors.
2. Unpack the thin ZIP to a temporary review folder.
3. Inventory top-level files and `exports/` / `exports/upload_samples/`.
4. Read `case_summary.json`, `CASE_REVIEW_SUMMARY.txt`, `run_status.txt`, `last_stage.txt`, `last_progress.tsv`, `run_progress.tsv`, and `VestigantSpotlight.log`.
5. Read `exports/parser_limits_and_suppression_summary.csv` to determine intentional caps, compact persistence, sampling, FFS/app DB materialization, and guardrails.
6. Read `EXPORT_INDEX.csv` and selected manifests to identify row counts and chunking.
7. Inspect focused iOS Spotlight outputs first: store parse summary, decode/field coverage, text context priority summary, high-value text context sample, missing-from-FFS summary/candidates, object/inode diagnostic summary, and app attribution summary.
8. Compare headline counts to the prior run: raw records, raw key values, raw dates, DB/WAL behavior, row counts, export duration/stalls, and whether `complete_success` was reached.
9. Classify the next change as compile fix, DB/WAL materialization fix, SQL/export expansion fix, review-quality/triage refinement, GUI wiring, packaging, or documentation.
10. Patch only the active failure/improvement class; preserve forensic interpretation constraints and compact normal mode.
11. Build/test locally where possible, package full source ZIP, patch ZIP, SHA256 files, and build/run commands.

Preferred tools: shell/Python/Ruby/sed for unpacking and CSV/JSON review; avoid loading large SQLite databases unless necessary.
