# V1.1.9.1 Validation Notes

## Uploaded V1.1.9 artifacts reviewed

- Source ZIP: `VestigantSpotlightInv_V1_1_9.zip`.
- Windows/MSVC build log: compiled and linked CLI, self-test binary, and GUI; binary version output reported `Vestigant Spotlight v1.1.9`.
- Thin macOS AFF4/APFS output: run completed source-probe workflow and staged/enriched Store-V2 artifacts.

## V1.1.9 thin-output counts reviewed

- `raw_record_count=25000` and `artifact_count=25000`.
- `copy_out_rows=9902`, `copied_files=9235`, `skipped_rows=667`, `total_copied_bytes=1386493641`.
- `staged_groups=11`, `staged_store_db_groups=11`, `staged_files=8986`, `staged_bytes=1368577744`.
- External compare: `external_file_count=4123`, `vestigant_file_count=8986`, `file_match_rows=2213`, `external_only_rows=1424`, `vestigant_only_rows=6710`, `hash_different_path_rows=431`, `RELATIVE_PATH_SIZE_MISMATCH=486`.

## Local source changes

- Updated V1.1.9.1 version metadata.
- Updated build wrapper to check `1.1.9.1`.
- Removed the three MSVC C4189 `btnFlags` warnings by using the decoded flag value in APFS OMAP branch-path diagnostics.

## Required external validation

- Windows/MSVC V1.1.9.1 build.
- macOS AFF4/APFS thin run for wrapper/package verification.
- No iOS validation required for this hotfix.
