# V1.6.35 current handoff note

V1.6.35 follows successful V1.6.35 build/thin validation and implements code-review hardening for APFS/AFF4/bplist/iOS app DB/GUI path handling. See `docs/V1_6_35_CODE_REVIEW_VALIDATION_HARDENING.md`. Missing-from-FFS and CoreDuet interpretation guardrails remain in place.

# Active Filesystem Comparison Roadmap - V1.6.35

V1.6.27 produced 7,766 materialized Missing-from-FFS reference candidates and passed eight active-comparison validation checks. V1.6.35 targets the remaining lookup-source REVIEW by preserving lookup source through the GUI/review Missing-from-FFS view and candidate materialization.

# Active Filesystem Comparison Roadmap - V1.6.35

V1.6.26 produced a completed iOS FFS exact-path plus Spotlight reference lookup run with 7,766 materialized reference candidates. V1.6.35 adds validation checks to confirm:

- run candidate counts match materialized rows;
- candidate summary totals reconcile;
- unsafe deletion-proof wording is absent;
- high-value message attachment candidates are visible separately from low-value cache/thumbnail candidates;
- lookup-source reporting is populated in the reference view/export surface.

Next guardrail retirement may proceed only after these checks pass in a real thin or support run.

# Active Filesystem Comparison Roadmap - V1.6.35

V1.6.35 adds validation views and upload samples for the current iOS FFS active-comparison workflow.

## Current validation outputs

- `active_file_comparison_validation_checks.csv`
- `active_file_comparison_candidate_summary.csv`
- `exports/upload_samples/active_file_comparison_validation_checks_sample.csv`
- `exports/upload_samples/active_file_comparison_candidate_summary_sample.csv`
- `exports/upload_samples/orphaned_deleted_candidates_focus.csv`

## Current candidate statuses

- `MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE`
- `MISSING_FROM_IOS_FFS_EXACT_PATH_CANDIDATE`
- `PRESENT_IN_IOS_FFS_EXACT_PATH`

## Remaining roadmap

- Validate V1.6.35 count consistency and lookup-source preservation.
- Add AFF4/APFS image-backed active comparison using `image_file_inventory`.
- Add inode/parent-object matching when stable object identifiers exist.
