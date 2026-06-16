# V1.6.38 Release Notes

## Purpose

V1.6.38 records active filesystem comparison as the next implementation target and replaces stale `v0.6.4` log/CLI wording with current V1.6.38 limitation language.

## Triggering evidence from V1.6.22.1 thin

- `run_status.txt` ended in `complete_success`.
- `VestigantSpotlight_tail250.log` reported active filesystem comparison was tabled and that `existence_status` would remain `NOT_CHECKED`-style.
- `active_file_comparison_readiness.csv` reported `comparison_ready=0` and `comparison_status=ZIP_PARSED_FOR_SPOTLIGHT_NOT_IMAGE_FILE_INVENTORY`.

## Changed in V1.6.38

- Added `docs/ACTIVE_FILESYSTEM_COMPARISON_ROADMAP.md`.
- Updated continuation docs to make active filesystem comparison the next queued implementation target.
- Replaced stale `v0.6.4` active-comparison log strings in current source files.
- Kept V1.6.22.1 thin diagnostic-wrapper behavior.
- Kept V1.6.22 interactionC precision behavior.

## Not implemented yet

V1.6.38 implements Phase 1 active filesystem comparison for iOS FFS exact-path lookup. `MISSING_FROM_IOS_FFS_EXACT_PATH_CANDIDATE` rows are investigative leads only, not deletion proof. AFF4/APFS image-inventory joins remain pending.


# Continuation Handoff - V1.6.18

Current package: `VestigantSpotlightInv_V1_6_18.zip`.

Immediate next action: build V1.6.18 on Windows/MSVC and visually validate the compact Case Information / Build Processing top section and Investigation Results action bar.

Upload after build: `V1_6_18_build.log`.

If the build passes and GUI layout is acceptable, run the iOS CoreSpotlight thin wrapper and upload `Upload_Thin_iOS_CoreSpotlight_V1_6_18.zip`.

V1.6.18 is a validation-follow-up release. It closes the targeted V1.6.17 iOS email-category regression review and moves forward on the GUI top-section crowding issue.
