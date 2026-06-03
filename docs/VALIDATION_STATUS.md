# Vestigant Spotlight Validation Status

Current version: 0.9.47

## Uploaded V0_9_46 results reviewed

- Windows/MSVC build log: successful; source version and binary both report 0.9.46.
- Reuse-cache thin output: `complete_success`.
- Fresh-ZIP thin output: `complete_success`.
- Fresh-ZIP inventory: 2,245,783 FFS file rows and 5,528 app database candidates.
- CoreSpotlight metrics remained stable: 6 valid stores, 344,445 raw records, 982,668 raw key/value rows, 336,037 raw date candidates, 344,445 artifacts, and 336,037 timeline events.
- Bplist/NSKeyedArchiver detail rows remained 438 in the V0_9_46 normal run.

## V0_9_47 validation performed here

- `src/parsers/native_storedb_parser.cpp`: Linux `g++ -std=c++20 -fsyntax-only` passed.
- `src/app/app_runner.cpp`: Linux `g++ -std=c++20 -fsyntax-only` passed.
- `src/db/case_db.cpp`: Linux `g++ -std=c++20 -fsyntax-only` passed.
- `src/export_sql/sqlite_exporter.cpp`: Linux `g++ -std=c++20 -fsyntax-only` passed.
- `src/core/app_info.cpp`: Linux `g++ -std=c++20 -fsyntax-only` passed.
- SQLite smoke test for `vw_investigator_time_anomalies`, `vw_ios_knowledgec_interaction_events`, and `vw_ios_knowledgec_interaction_summary` passed.
- Raw-string size scan found 0 oversized raw literals above the configured threshold.
- Classifier simulation against the V0_9_46 fresh-ZIP app database inventory predicts new KnowledgeC/CoreDuet target categories for `knowledgeC.db`, `interactionC.db`, and `globalKnowledge.db` while preserving the 5,528-row app DB candidate set.

## Required Windows validation

1. Run `scripts\Build-V0_9_47.ps1`.
2. Confirm CLI reports `Vestigant Spotlight v0.9.47`.
3. Run `VestigantSpotlightTests.exe`.
4. Run reuse-cache test and confirm `complete_success`.
5. Run Stage B fresh-ZIP test and confirm FFS/app-database inventory remains nonzero and KnowledgeC/CoreDuet targets are classified/extracted.
6. Run a support/full app DB materialization profile when ready to validate KnowledgeC parsed event rows.
