## V1.6.12 - Parent-inode path reconstruction validation after V1.6.10 thin

V1.6.12 follows review of the uploaded `Upload_Thin_MacOS_AFF4_V1_6_10.zip`, whose uploaded SHA256 file reported `0D42F7F285F8DC526A1E0C1E7A93B217BFD90FAA35BF9A1BD4209FF1154BD39D`. The V1.6.10 thin reached `complete_aff4_apfs_staged_storev2_validation_probe` with `store_count=11`, `valid_store_count=7`, `database_candidate_count=22`, `valid_database_candidate_count=14`, `parser_selected_database_count=7`, `native_decode_mode=AFF4_APFS_STAGED_STOREV2_FullValues`, `raw_record_count=25000`, `raw_key_value_count=6568`, `raw_date_candidate_count=25000`, `artifact_count=25000`, and `timeline_event_count=25000`. The V1.6.10 date-candidate fix was validated because `raw_date_candidates_sample.csv` contained only `Last_Updated` rows while raw probe aliases remained in `raw_key_values`.

The remaining validation defect was parent-inode path reconstruction. The V1.6.10 thin reported `parent_inode_links=24998`, `matched=23871`, and `child_names=1425`, but `reconstructed_paths=0` and `artifacts_updated=0`. That showed relationship evidence was being captured, but post-parse path-chain reconstruction was not using the complete artifact set to recover reviewable path candidates.

Code fixes in V1.6.12:
- Added a post-artifact recursive parent-inode chain reconstruction pass in SQLite enrichment. This pass rebuilds candidate paths after all raw records are materialized, avoiding parser-order loss where children are seen before parent records.
- Preserves absolute path candidates as preferred evidence and retains relative parent-inode chains as review-only evidence when no absolute ancestor path is available.
- Updates `parent_inode_links.reconstructed_path_candidate`, `path_reconstruction_method`, and confidence with chain-derived candidates where the immediate parent-link pass left the candidate blank.
- Applies chain-derived candidates to weak artifact path rows using explicit path provenance values: `PARENT_INODE_CHAIN_RECONSTRUCTION` or `PARENT_INODE_RELATIVE_CHAIN_REVIEW`.
- Updates `vw_path_reconstruction` so the applied-path flag recognizes the new path-source values.
- Adds a focused AFF4/APFS thin validation sample: `aff4_apfs_staged_storev2_path_reconstruction_sample.csv`.
- Updates the macOS AFF4 thin wrapper to require the new path-reconstruction sample, in addition to parser and field-coverage samples.

Validation completed in this packaging environment: Linux CMake build PASS, CLI version `Vestigant Spotlight v1.6.12`, self-test PASS, and a local staged Store-V2 run against uploaded V1.6.10 staged data PASS. Windows/MSVC build, full live V1.6.12 macOS AFF4 thin, and iOS CoreSpotlight regression thin were not run here.

Test determination: run Windows/MSVC build and the macOS AFF4 thin for V1.6.12. iOS thin remains recommended after macOS because native parser/enrichment code is shared, but the V1.6.12 change is macOS path-reconstruction-focused.
