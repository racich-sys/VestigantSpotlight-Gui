# iOS Investigation Validation Workflow - V1.6.41.1

1. Build V1.6.41.1 and review `V1_6_41_build.log`.
2. Run `scripts\Run-V1_6_41-iOS-CoreSpotlight-AndZip.ps1 -CleanOut`.
3. Upload `Upload_Thin_iOS_CoreSpotlight_V1_6_41.zip`.
4. Validate CoreSpotlight counts, interactionC samples, string-probe category precision, and active filesystem comparison outputs.
5. For active filesystem comparison, review `active_file_comparison_runs_sample.csv`, `active_file_comparison_readiness_focus.csv`, `spotlight_active_file_comparison_focus.csv`, and `orphaned_deleted_candidates_sample.csv`.
6. Treat missing iOS FFS exact-path rows as investigative leads only. Do not infer deletion without corroboration.
7. If thin passes, run validation-support and bounded full-native workflows before retiring additional parser/export guardrails.
