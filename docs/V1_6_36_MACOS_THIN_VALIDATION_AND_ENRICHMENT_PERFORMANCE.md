# V1_6_40_1 macOS zipped Spotlight thin validation and enrichment performance

## Evidence reviewed

The V1.6.32 macOS zipped Spotlight thin run against `E:/test second/Spotlight/Spotlight-V100.zip` reached `complete_success` and used the macOS Store-V2 native persistence mode.

The run parsed 50,000 bounded native records, wrote 19,570 raw key/value rows, 50,000 raw date candidates, 47,928 artifacts, and 50,000 timeline events.

## Issue addressed

The thin run showed `enrichment_parent_inode_links_complete` reported `new_reconstructed_paths=0`, but the next phase still spent approximately five minutes in parent-inode path apply before reporting `artifacts_updated=0`.

## Change

V1.6.40.1.1 skips the parent-inode path apply UPDATE when `newReconstructedPathRows == 0`. It writes explicit run-status markers:

- `enrichment_parent_inode_path_apply_skipped`
- `enrichment_parent_inode_path_apply_complete ... skipped=1 reason=no_new_reconstructed_paths`

## Not changed

- macOS Store-V2 persistence mode from V1.6.31/V1.6.32 remains active.
- iOS compact filtering remains disabled for `--profile macos`.
- Missing-from-FFS and CoreDuet guardrails remain unchanged.
