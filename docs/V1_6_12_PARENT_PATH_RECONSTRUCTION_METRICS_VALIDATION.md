## V1.6.12 - Parent-inode path reconstruction metric clarification after V1.6.11 thin

V1.6.12 follows review of the uploaded `Upload_Thin_MacOS_AFF4_V1_6_11.zip`, whose uploaded SHA256 file reported `1A716392CD6F8F414B9D3EED7C5FB3203E0BB7A277082CCF74830E072D0CEFE4`. The V1.6.11 thin reached `complete_aff4_apfs_staged_storev2_validation_probe` with `store_count=11`, `valid_store_count=7`, `database_candidate_count=22`, `valid_database_candidate_count=14`, `parser_selected_database_count=7`, `native_decode_mode=AFF4_APFS_STAGED_STOREV2_FullValues`, `raw_record_count=25000`, `raw_key_value_count=6568`, `raw_date_candidate_count=25000`, `artifact_count=25000`, and `timeline_event_count=25000`.

The V1.6.11 parent-inode validation sample was present and the run status improved from V1.6.10: `parent_inode_links=24998`, `matched=23871`, `child_names=1425`, and `reconstructed_paths=1425`. However, sample review showed those 1,425 path candidates matched existing raw Spotlight paths, while `artifacts_updated=0`. The validation wording therefore overstated new path reconstruction. The true finding was: parent-link evidence and existing path context were captured, but no newly applied path was created from unnamed child rows.

Code fixes in V1.6.12:
- Separates path-context candidates from newly reconstructed paths in parent-inode metrics and run-status output.
- Adds metrics for `parent_inode_links_with_path_context_candidate`, `parent_inode_links_with_existing_path_context`, `parent_inode_links_with_new_reconstructed_path`, and `parent_inode_artifacts_updated_from_reconstruction`.
- Updates `vw_path_reconstruction` with explicit `path_candidate_status`, `existing_path_context_only`, and `new_reconstructed_path` columns.
- Updates `vw_same_folder_groups` so existing Spotlight path context is not mislabeled as newly reconstructed child paths.
- Adds `aff4_apfs_staged_storev2_path_reconstruction_metrics_sample.csv` to the AFF4/APFS thin output and requires it in the macOS AFF4 wrapper.

Validation completed in this packaging environment: Linux CMake build PASS, CLI version `Vestigant Spotlight v1.6.12`, self-test PASS, and ZIP integrity checks PASS. Windows/MSVC build, full live V1.6.12 macOS AFF4 thin, and iOS CoreSpotlight regression thin were not run here.

Test determination: run Windows/MSVC build and the macOS AFF4 thin for V1.6.12. iOS thin is recommended after macOS because SQLite enrichment code is shared, but the V1.6.12 change is macOS path-validation-output focused.
