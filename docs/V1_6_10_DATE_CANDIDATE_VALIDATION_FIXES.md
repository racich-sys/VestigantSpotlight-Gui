## V1.6.10 - Date-candidate validation cleanup after V1.6.9 thin

V1.6.10 follows review of the uploaded `Upload_Thin_MacOS_AFF4_V1_6_9.zip`, whose uploaded SHA256 file reported `53D2BC1B47274618D16E4B30191C27310968FBD85E7B0C894499FAEA360BB006`. The V1.6.9 thin reached `complete_aff4_apfs_staged_storev2_validation_probe` and reported `raw_records=25000`, `raw_key_values=6568`, `raw_date_candidates=29387`, `artifact_count=25000`, and `timeline_event_count=25000`. The validation defect was that semantic raw-probe aliases such as `__native_probe_url_candidate_01`, `__native_probe_basename_candidate_01`, and `__native_probe_plist_xml_candidate_01` were also inserted into `raw_date_candidates` because the substring `date` appears inside the word `candidate`. Those rows had blank `parsed_utc` and were useful raw key/value evidence, but they were not date evidence.

Code fixes in V1.6.10:
- Hardened `looksDateField()` so internal raw-probe aliases beginning with `__native_probe_` or `__native_core_probe_string_` cannot be treated as date fields.
- Added an explicit guard for `_candidate_` alias names unless a future alias deliberately contains `date_candidate` or `time_candidate`.
- Preserved URL/path/basename/plist probe aliases in `raw_key_values`, but prevented them from inflating `raw_date_candidates` and parser coverage date counts.
- Updated V1.6.10 build/thin wrapper version strings and validation documentation.

Validation completed in this packaging environment: Linux incremental build PASS, CLI version `Vestigant Spotlight v1.6.10`, self-test PASS, and a reconstructed staged Store-V2 run from the uploaded V1.6.9 thin PASS. The reconstructed staged run confirmed `raw_date_candidates_sample.csv` contained only `Last_Updated` rows while `raw_key_values_sample.csv` retained the semantic probe aliases. Windows/MSVC build, full live V1.6.10 macOS AFF4 thin, and iOS regression thin were not run in this environment.

Test determination: because V1.6.10 changes shared native Store-V2 date-field classification used by both macOS AFF4 validation and iOS CoreSpotlight parsing, run Windows/MSVC build, macOS AFF4 thin, and an iOS CoreSpotlight thin before treating the change as production-ready.

