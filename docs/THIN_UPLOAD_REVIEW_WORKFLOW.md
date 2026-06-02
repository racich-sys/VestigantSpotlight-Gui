# Repeatable Thin-Upload Review Workflow

Use this process each time a build log and thin upload ZIP are provided.

1. Inspect the uploaded build log first.
   - Confirm source version.
   - Confirm CLI, self-test, and GUI build status.
   - Note MSVC-specific failures such as C2026 oversized string literals or missing symbols.
2. Unpack the thin upload to a temporary review folder.
3. Inventory key files.
   - `run_status.txt`
   - `last_stage.txt`
   - `last_progress.tsv`
   - `run_progress.tsv`
   - `VestigantSpotlight.log`
   - `case_summary.json`
   - `CASE_REVIEW_SUMMARY.txt`
   - `exports/parser_limits_and_suppression_summary.csv`
   - `exports/EXPORT_INDEX.csv`
4. Compare headline metrics to the prior version.
   - raw records
   - compact raw key/value rows
   - compact date candidates
   - DB/WAL or export-stall indicators
   - completed/stalled/failed stage
5. Inspect representative high-value CSVs.
   - communication summary
   - message body/user-focus review samples
   - missing-from-FFS summaries/samples
   - normalized timeline and anomaly summaries
   - parser diagnostics and provenance summaries
6. Classify the next work item.
   - build failure
   - runtime/stall/DB issue
   - export/view issue
   - review-quality improvement
   - documentation/roadmap update
7. Patch the latest source only after the failure/improvement class is identified.
8. Run available validation.
   - C++ syntax checks
   - raw-string size checks
   - schema/view smoke test
   - Linux build/self-test where feasible
   - Windows/MSVC validation by user
9. Package artifacts.
   - full source ZIP
   - patch ZIP from prior version
   - SHA256 files
   - build/run commands
10. Record what was reviewed and changed in validation notes and consolidated version history.
