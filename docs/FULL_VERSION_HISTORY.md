# Vestigant Spotlight / Spotlight2 Full Version History Baseline

Generated from `Version_Workflow.zip` and the current V1.1.7 source-package documentation on 2026-06-07 16:24:52Z.

This document is intended to become the **append-only baseline** for version history going forward. It preserves older V0.x and V1.0.x history and adds the current V1.1.x continuation status so future releases do not accidentally drop prior context.

# Version History Append-Only Policy

This file is the baseline policy for Vestigant Spotlight / Spotlight2 version history.

## Rules

1. Do not replace the version history with only the latest version.
2. Do not delete V0.x, V1.0.x, or V1.1.x entries when adding a new release.
3. Every future package must keep a durable history file in the source tree.
4. New entries may be prepended for readability, but old entries must remain below.
5. If history is split across files, the top-level `VERSION_HISTORY.md` must point to the complete consolidated file.
6. If a version had a failed build, hotfix, or packaging failure, record that failure and the fix version.
7. The workflow ledger should identify:
   - current validated baseline;
   - current generated package;
   - pending validation;
   - known build/script pitfalls;
   - next safe work.
8. The suggestions/fixes tracker should link each suggestion to a version and validation status.


## Current active version-history status

- **Latest generated source package in this chat:** `V1.1.7`.
- **Most recently Windows/MSVC + macOS AFF4/APFS thin-validated baseline reviewed before the uploaded workflow request:** `V1.1.6.1`.
- **V1.1.7 status:** locally syntax/build/self-test validated in the packaging environment; Windows/MSVC and thin-output validation remain pending unless uploaded later.
- **Version-history policy:** append-only. Future updates must prepend or append new version entries without deleting old V0.x, V1.0.x, or V1.1.x history.

## Recent V1.1.x continuation entries to preserve

### V1.1.7

- Moved `writeAff4CppLiteDynamicLoadProbe(...)` from `src/app/app_runner.cpp` into `Aff4ProbeWorker::executeDynamicLoadProbe(...)`.
- Confirmed both large AFF4/APFS probe bodies now live in `src/parsers/aff4_probe_worker.cpp`.
- Added cancellation callback support to shared APFS OMAP traversal helper calls used by direct-map and dynamic-load probe paths.
- Local validation: C++20 syntax checks, Linux CMake configure/build, CLI version check `Vestigant Spotlight v1.1.7`, and local self-test passed.
- Pending validation: Windows/MSVC build, GUI runtime, macOS AFF4/APFS thin run, and long-run cancellation behavior.

### V1.1.6.1

- Build hotfix for V1.1.6 after the direct-map probe worker split.
- Added missing Windows-only `wideProcessPath(...)` helper to `src/parsers/aff4_probe_worker.cpp`.
- Corrected V1.1.6.1 build-wrapper version check.
- Windows/MSVC build and macOS AFF4/APFS thin output were later reviewed as passing; V1.1.6.1 became the current stable baseline before V1.1.7 work.

### V1.1.6

- Moved the direct-map AFF4/APFS probe body from `src/app/app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp`.
- Added `src/parsers/aff4_probe_worker.h` and wired it into CMake/MSVC build lists.
- Updated app-runner call sites to delegate direct-map probe execution through `Aff4ProbeWorker::executeDirectMapReaderProbe(...)`.
- Left the libaff4 dynamic-load probe in `app_runner.cpp` pending a larger dependency split; this was fixed in V1.1.7.


## Canonical version index detected in uploaded workflow data
This index is generated from filenames and markdown headings/content in `Version_Workflow.zip`, plus the current V1.1.7 source package available in this chat. It is intended to prevent older versions from falling out of the history.
| Version | Source file count | Source files |
|---|---:|---|
| V0_5_0 | 1 | PATCH_SUMMARY_V0_5_0.md |
| V0_5_1 | 2 | PATCH_SUMMARY_V0_5_2.md<br>PATCH_SUMMARY_V0_5_3.md |
| V0_5_2 | 2 | PATCH_SUMMARY_V0_5_2.md<br>PATCH_SUMMARY_V0_5_3.md |
| V0_5_3 | 1 | PATCH_SUMMARY_V0_5_4.md |
| V0_5_4 | 2 | PATCH_SUMMARY_V0_5_3.md<br>PATCH_SUMMARY_V0_5_4.md |
| V0_5_5 | 3 | PATCH_SUMMARY_V0_5_5.md<br>PATCH_SUMMARY_V0_5_6.md<br>PATCH_SUMMARY_V0_5_7.md |
| V0_5_6 | 3 | PATCH_SUMMARY_V0_5_6.md<br>PATCH_SUMMARY_V0_5_7.md<br>PATCH_SUMMARY_V0_5_9.md |
| V0_5_7 | 1 | PATCH_SUMMARY_V0_5_7.md |
| V0_5_8 | 1 | PATCH_SUMMARY_V0_5_9.md |
| V0_5_9 | 1 | PATCH_SUMMARY_V0_5_9.md |
| V0_6 | 1 | PATCH_SUMMARY_V0_6_2.md |
| V0_6_0 | 1 | PATCH_SUMMARY_V0_6_0.md |
| V0_6_1 | 1 | PATCH_SUMMARY_V0_6_2.md |
| V0_6_2 | 1 | PATCH_SUMMARY_V0_6_2.md |
| V0_6_3 | 2 | PATCH_SUMMARY_V0_6_3.md<br>PATCH_SUMMARY_V0_6_4.md |
| V0_6_4 | 1 | PATCH_SUMMARY_V0_6_4.md |
| V0_6_5_1 | 1 | PATCH_SUMMARY_V0_6_6.md |
| V0_6_6 | 2 | PATCH_SUMMARY_V0_6_5_1.md<br>PATCH_SUMMARY_V0_6_7.md |
| V0_6_6_1 | 1 | PATCH_SUMMARY_V0_6_5.md |
| V0_6_7 | 2 | PATCH_SUMMARY_V0_6_6.md<br>PATCH_SUMMARY_V0_6_7.md |
| V0_7_0 | 1 | PATCH_SUMMARY_V0_7_1.md |
| V0_7_1 | 2 | PATCH_SUMMARY_V0_7_1.md<br>PATCH_SUMMARY_V0_7_2.md |
| V0_7_2 | 1 | PATCH_SUMMARY_V0_7_2.md |
| V0_7_3 | 1 | PATCH_SUMMARY_V0_7_3.md |
| V0_7_4 | 2 | PATCH_SUMMARY_V0_7_4.md<br>PATCH_SUMMARY_V0_7_5.md |
| V0_7_5 | 1 | PATCH_SUMMARY_V0_7_5.md |
| V0_7_6 | 1 | PATCH_SUMMARY_V0_7_6.md |
| V0_7_7 | 1 | PATCH_SUMMARY_V0_7_7.md |
| V0_7_8 | 1 | PATCH_SUMMARY_V0_7_8.md |
| V0_7_10 | 1 | PATCH_SUMMARY_V0_7_10.md |
| V0_7_11 | 1 | PATCH_SUMMARY_V0_7_11.md |
| V0_7_12 | 2 | PATCH_SUMMARY_V0_7_12.md<br>PATCH_SUMMARY_V0_7_13.md |
| V0_7_13 | 1 | PATCH_SUMMARY_V0_7_13.md |
| V0_7_15 | 2 | PATCH_SUMMARY_V0_7_15.md<br>PATCH_SUMMARY_V0_7_15_5.md |
| V0_7_15_1 | 1 | PATCH_SUMMARY_V0_7_15_1.md |
| V0_7_15_2 | 2 | PATCH_SUMMARY_V0_7_15_2.md<br>PATCH_SUMMARY_V0_7_15_5.md |
| V0_7_15_3 | 1 | PATCH_SUMMARY_V0_7_15_5.md |
| V0_7_15_4 | 1 | PATCH_SUMMARY_V0_7_15_5.md |
| V0_7_15_5 | 1 | PATCH_SUMMARY_V0_7_15_5.md |
| V0_7_15_7 | 1 | PATCH_SUMMARY_V0_7_16.md |
| V0_7_16 | 1 | PATCH_SUMMARY_V0_7_17.md |
| V0_7_17 | 2 | PATCH_SUMMARY_V0_7_16.md<br>PATCH_SUMMARY_V0_7_17.md |
| V0_7_18 | 1 | PATCH_SUMMARY_V0_7_19.md |
| V0_7_19 | 1 | PATCH_SUMMARY_V0_7_19.md |
| V0_7_20 | 2 | PATCH_SUMMARY_V0_7_20.md<br>PATCH_SUMMARY_V0_7_22.md |
| V0_7_22 | 2 | PATCH_SUMMARY_V0_7_22.md<br>PATCH_SUMMARY_V0_7_23.md |
| V0_7_23 | 2 | PATCH_SUMMARY_V0_7_23.md<br>PATCH_SUMMARY_V0_7_23_5.md |
| V0_7_23_1 | 1 | PATCH_SUMMARY_V0_7_23_2.md |
| V0_7_23_2 | 2 | PATCH_SUMMARY_V0_7_23_2.md<br>PATCH_SUMMARY_V0_7_23_3.md |
| V0_7_23_3 | 1 | PATCH_SUMMARY_V0_7_23_3.md |
| V0_7_23_4 | 1 | PATCH_SUMMARY_V0_7_23_5.md |
| V0_7_23_5 | 1 | PATCH_SUMMARY_V0_7_23_5.md |
| V0_7_23_6 | 1 | PATCH_SUMMARY_V0_7_24.md |
| V0_7_24 | 1 | PATCH_SUMMARY_V0_7_24.md |
| V0_7_24_2 | 1 | PATCH_SUMMARY_V0_7_24_3.md |
| V0_7_24_3 | 1 | PATCH_SUMMARY_V0_7_24_3.md |
| V0_8 | 1 | VERSION_HISTORY_5.md |
| V0_8_59 | 1 | IOS_CORESPOTLIGHT_ROADMAP.md |
| V0_8_69 | 2 | PROJECT_ROADMAP_AND_CONTINUATION.md<br>VERSION_HISTORY_2.md |
| V0_8_74 | 1 | VERSION_HISTORY_5.md |
| V0_8_81 | 1 | VERSION_HISTORY_5.md |
| V0_8_82 | 1 | VERSION_HISTORY_5.md |
| V0_8_83 | 1 | VERSION_HISTORY_5.md |
| V0_8_84 | 1 | VERSION_HISTORY_5.md |
| V0_8_85 | 1 | VERSION_HISTORY_5.md |
| V0_8_85_1 | 1 | VERSION_HISTORY_5.md |
| V0_8_86 | 1 | VERSION_HISTORY_5.md |
| V0_8_87 | 1 | VERSION_HISTORY_5.md |
| V0_8_88_1 | 2 | IOS_CORESPOTLIGHT_ROADMAP.md<br>VERSION_HISTORY_5.md |
| V0_8_89 | 1 | VERSION_HISTORY_5.md |
| V0_8_89_1 | 1 | VERSION_HISTORY_5.md |
| V0_8_90 | 1 | VERSION_HISTORY_5.md |
| V0_8_93 | 1 | VERSION_HISTORY_5.md |
| V0_8_93_3 | 1 | VERSION_HISTORY_5.md |
| V0_8_98 | 2 | Spotlight_V0_8_99_review_and_next_test.txt<br>Spotlight_V0_9_2_review_and_next_test.txt |
| V0_8_98_1 | 2 | Spotlight_V0_8_99_review_and_next_test.txt<br>Spotlight_V0_9_2_review_and_next_test.txt |
| V0_8_99 | 3 | Spotlight_V0_8_97_review_and_next_test.txt<br>Spotlight_V0_8_99_review_and_next_test.txt<br>VERSION_HISTORY_5.md |
| V0_9_0 | 1 | V0_9_1_CHANGE_VALIDATION_NOTE.txt |
| V0_9 | 1 | CONSOLIDATED_USER_MANUAL.md |
| V0_9_1 | 3 | IOS_CORESPOTLIGHT_ROADMAP.md<br>V0_9_1_CHANGE_VALIDATION_NOTE.txt<br>V0_9_2_CHANGE_VALIDATION_NOTE.txt |
| V0_9_2 | 2 | IOS_CORESPOTLIGHT_ROADMAP.md<br>V0_9_3_CHANGE_VALIDATION_NOTE.txt |
| V0_9_3 | 3 | CONSOLIDATED_USER_MANUAL.md<br>IOS_CORESPOTLIGHT_ROADMAP.md<br>V0_9_4_CHANGE_VALIDATION_NOTE.txt |
| V0_9_4 | 6 | Spotlight_V0_9_2_review_and_next_test.txt<br>V0_9_2_CHANGE_VALIDATION_NOTE.txt<br>V0_9_3_CHANGE_VALIDATION_NOTE.txt<br>V0_9_4_CHANGE_VALIDATION_NOTE.txt<br>V0_9_5_CHANGE_VALIDATION_NOTE.txt<br>V0_9_6_CHANGE_VALIDATION_NOTE.txt |
| V0_9_5 | 2 | V0_9_5_CHANGE_VALIDATION_NOTE.txt<br>V0_9_6_CHANGE_VALIDATION_NOTE.txt |
| V0_9_6 | 2 | IOS_CORESPOTLIGHT_ROADMAP.md<br>V0_9_6_CHANGE_VALIDATION_NOTE.txt |
| V0_9_10 | 1 | IOS_CORESPOTLIGHT_ROADMAP.md |
| V0_9_11 | 3 | IOS_CORESPOTLIGHT_ROADMAP.md<br>V0_9_11_1_CHANGE_VALIDATION_NOTE.txt<br>V0_9_14_CHANGE_VALIDATION_NOTE.txt |
| V0_9_11_1 | 2 | IOS_CORESPOTLIGHT_ROADMAP.md<br>V0_9_11_1_CHANGE_VALIDATION_NOTE.txt |
| V0_9_12 | 1 | V0_9_13_CHANGE_VALIDATION_NOTE.txt |
| V0_9_13 | 1 | V0_9_14_CHANGE_VALIDATION_NOTE.txt |
| V0_9_15 | 4 | DETAILED_ROADMAP_AND_TESTING_TIMELINE.md<br>IOS_CORESPOTLIGHT_ROADMAP.md<br>V0_9_13_CHANGE_VALIDATION_NOTE.txt<br>V0_9_14_CHANGE_VALIDATION_NOTE.txt |
| V0_9_16 | 2 | V0_9_17_CHANGE_VALIDATION_NOTE.txt<br>V0_9_18_CHANGE_VALIDATION_NOTE.txt |
| V0_9_17 | 4 | IOS_CORESPOTLIGHT_ROADMAP.md<br>V0_9_15_CHANGE_VALIDATION_NOTE.txt<br>V0_9_17_CHANGE_VALIDATION_NOTE.txt<br>V0_9_18_CHANGE_VALIDATION_NOTE.txt |
| V0_9_18 | 1 | V0_9_19_CHANGE_VALIDATION_NOTE.txt |
| V0_9_19 | 1 | V0_9_20_CHANGE_VALIDATION_NOTE.txt |
| V0_9_20 | 5 | DETAILED_ROADMAP_AND_TESTING_TIMELINE.md<br>V0_9_18_CHANGE_VALIDATION_NOTE.txt<br>V0_9_19_CHANGE_VALIDATION_NOTE.txt<br>V0_9_20_CHANGE_VALIDATION_NOTE.txt<br>V0_9_21_CHANGE_VALIDATION_NOTE.txt |
| V0_9_21 | 2 | V0_9_21_CHANGE_VALIDATION_NOTE.txt<br>V0_9_22_CHANGE_VALIDATION_NOTE.txt |
| V0_9_22 | 1 | V0_9_22_CHANGE_VALIDATION_NOTE.txt |
| V0_9_23 | 1 | V0_9_23_1_CHANGE_VALIDATION_NOTE.txt |
| V0_9_23_1 | 1 | V0_9_24_CHANGE_VALIDATION_NOTE.txt |
| V0_9_24 | 3 | V0_9_23_1_CHANGE_VALIDATION_NOTE.txt<br>V0_9_24_CHANGE_VALIDATION_NOTE.txt<br>V0_9_25_CHANGE_VALIDATION_NOTE.txt |
| V0_9_25 | 1 | V0_9_25_CHANGE_VALIDATION_NOTE.txt |
| V0_9_26 | 1 | V0_9_26_1_CHANGE_VALIDATION_NOTE.txt |
| V0_9_26_1 | 1 | V0_9_26_2_CHANGE_VALIDATION_NOTE.txt |
| V0_9_26_2 | 2 | V0_9_26_1_CHANGE_VALIDATION_NOTE.txt<br>V0_9_26_2_CHANGE_VALIDATION_NOTE.txt |
| V0_9_27 | 1 | V0_9_28_CHANGE_VALIDATION_NOTE.txt |
| V0_9_28 | 2 | V0_9_28_CHANGE_VALIDATION_NOTE.txt<br>V0_9_29_CHANGE_VALIDATION_NOTE.txt |
| V0_9_29 | 3 | RELEASE_NOTES.md<br>V0_9_29_CHANGE_VALIDATION_NOTE.txt<br>V0_9_30_CHANGE_VALIDATION_NOTE.txt |
| V0_9_30 | 1 | RELEASE_NOTES.md |
| V0_9_31 | 2 | V0_9_32_CHANGE_VALIDATION_NOTE.txt<br>VERSION_HISTORY_4.md |
| V0_9_32 | 3 | DETAILED_ROADMAP_AND_TESTING_TIMELINE.md<br>V0_9_30_CHANGE_VALIDATION_NOTE.txt<br>V0_9_32_CHANGE_VALIDATION_NOTE.txt |
| V0_9_33 | 3 | RELEASE_NOTES.md<br>V0_9_34_REVIEW_NOTES.md<br>VERSION_HISTORY_4.md |
| V0_9_34 | 3 | CONSOLIDATED_USER_MANUAL.md<br>V0_9_34_REVIEW_NOTES.md<br>V0_9_36_REVIEW_NOTES.md |
| V0_9_35 | 2 | CONSOLIDATED_USER_MANUAL.md<br>V0_9_36_REVIEW_NOTES.md |
| V0_9_36 | 1 | V0_9_37_REVIEW_NOTES.md |
| V0_9_37 | 5 | CONSOLIDATED_USER_MANUAL.md<br>V0_9_36_REVIEW_NOTES.md<br>V0_9_37_REVIEW_NOTES.md<br>V0_9_38_REVIEW_NOTES.md<br>V0_9_39_REVIEW_NOTES.md |
| V0_9_38 | 2 | DETAILED_ROADMAP_AND_TESTING_TIMELINE.md<br>V0_9_39_REVIEW_NOTES.md |
| V0_9_39 | 3 | V0_9_38_REVIEW_NOTES.md<br>V0_9_39_REVIEW_NOTES.md<br>V0_9_40_REVIEW_NOTES.md |
| V0_9_40 | 1 | V0_9_41_REVIEW_NOTES.md |
| V0_9_41 | 2 | CONSOLIDATED_USER_MANUAL.md<br>V0_9_42_REVIEW_NOTES.md |
| V0_9_42 | 5 | CONSOLIDATED_USER_MANUAL.md<br>DETAILED_ROADMAP_AND_TESTING_TIMELINE.md<br>V0_9_40_REVIEW_NOTES.md<br>V0_9_41_REVIEW_NOTES.md<br>V0_9_42_REVIEW_NOTES.md |
| V0_9_48 | 1 | PROJECT_ROADMAP_AND_CONTINUATION.md |
| V0_9_51 | 1 | V0_9_52_REVIEW_NOTES.md |
| V0_9_54 | 2 | V0_9_52_REVIEW_NOTES.md<br>VERSION_HISTORY_2.md |
| V0_9_55 | 4 | CONSOLIDATED_USER_MANUAL.md<br>DETAILED_ROADMAP_AND_TESTING_TIMELINE.md<br>PROJECT_ROADMAP_AND_CONTINUATION.md<br>VERSION_HISTORY_2.md |
| V0_9_56 | 3 | DETAILED_ROADMAP_AND_TESTING_TIMELINE.md<br>PROJECT_ROADMAP_AND_CONTINUATION.md<br>VERSION_HISTORY_2.md |
| V0_9_57 | 3 | CONSOLIDATED_USER_MANUAL.md<br>PROJECT_ROADMAP_AND_CONTINUATION.md<br>VERSION_HISTORY_2.md |
| V0_9_60 | 5 | CONSOLIDATED_USER_MANUAL.md<br>DETAILED_ROADMAP_AND_TESTING_TIMELINE.md<br>IOS_CORESPOTLIGHT_ROADMAP.md<br>PROJECT_ROADMAP_AND_CONTINUATION.md<br>VERSION_HISTORY_2.md |
| V1_0_0 | 2 | PROJECT_ROADMAP_AND_CONTINUATION.md<br>VERSION_HISTORY_2.md |
| V1_0_1 | 1 | VERSION_HISTORY_2.md |
| V1_0_3 | 1 | VERSION_HISTORY_2.md |
| V1_0_4 | 1 | VERSION_HISTORY_2.md |
| V1_0_5 | 1 | VERSION_HISTORY_2.md |
| V1_0_6 | 1 | VERSION_HISTORY_2.md |
| V1_0_7 | 1 | VERSION_HISTORY_2.md |
| V1_0_8 | 1 | VERSION_HISTORY_2.md |
| V1_0_12 | 1 | VERSION_HISTORY_2.md |
| V1_0_14 | 6 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>VERSION_HISTORY_1.md<br>VERSION_HISTORY_2.md |
| V1_0_15 | 5 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>VERSION_HISTORY_1.md |
| V1_0_18 | 7 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>ROADMAP_CHECKLIST.md<br>VERSION_HISTORY_1.md |
| V1_0_19 | 2 | Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>ROADMAP_CHECKLIST.md |
| V1_0_22 | 4 | Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md |
| V1_0_23 | 4 | Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md |
| V1_0_24 | 9 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>... +1 more |
| V1_0_24_1 | 5 | Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md |
| V1_0_25 | 10 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>PROJECT_ROADMAP_AND_CONTINUATION.md<br>ROADMAP_CHECKLIST.md<br>... +2 more |
| V1_0_26 | 10 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>PROJECT_ROADMAP_AND_CONTINUATION.md<br>ROADMAP_CHECKLIST.md<br>... +2 more |
| V1_0_26_1 | 5 | Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>PROJECT_ROADMAP_AND_CONTINUATION.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md |
| V1_0_27 | 10 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>PROJECT_ROADMAP_AND_CONTINUATION.md<br>ROADMAP_CHECKLIST.md<br>... +2 more |
| V1_0_28 | 8 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>VERSION_HISTORY_1.md |
| V1_0_28_1 | 8 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>VERSION_HISTORY_1.md |
| V1_0_28_2 | 9 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>... +1 more |
| V1_0_29 | 9 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>... +1 more |
| V1_0_30 | 9 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>... +1 more |
| V1_1_0_1 | 8 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>VERSION_HISTORY_1.md |
| V1_1_1 | 8 | CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>VERSION_HISTORY_1.md |
| V1_1_2 | 8 | Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>Current_V1_1_7_source/docs/WORKFLOW_LEDGER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>VERSION_HISTORY_1.md |
| V1_1_3 | 8 | Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>Current_V1_1_7_source/docs/WORKFLOW_LEDGER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>VERSION_HISTORY_1.md |
| V1_1_4 | 8 | Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>Current_V1_1_7_source/docs/WORKFLOW_LEDGER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>VERSION_HISTORY_1.md |
| V1_1_5 | 5 | Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>Current_V1_1_7_source/docs/WORKFLOW_LEDGER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md |
| V1_1_5_1 | 8 | Current_V1_1_7_source/CONSOLIDATED_VERSION_HISTORY.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>Current_V1_1_7_source/docs/WORKFLOW_LEDGER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>VERSION_HISTORY_1.md |
| V1_1_6 | 7 | Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>Current_V1_1_7_source/docs/WORKFLOW_LEDGER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>VERSION_HISTORY_1.md |
| V1_1_6_1 | 8 | Current_V1_1_7_generated/validation/V1_1_7_validation_notes.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>Current_V1_1_7_source/docs/WORKFLOW_LEDGER.md<br>ROADMAP_CHECKLIST.md<br>SUGGESTIONS_AND_FIXES_TRACKER.md<br>VERSION_HISTORY_1.md |
| V1_1_7 | 5 | Current_V1_1_7_generated/validation/V1_1_7_validation_notes.md<br>Current_V1_1_7_source/VERSION_HISTORY.md<br>Current_V1_1_7_source/docs/ROADMAP_CHECKLIST.md<br>Current_V1_1_7_source/docs/SUGGESTIONS_AND_FIXES_TRACKER.md<br>Current_V1_1_7_source/docs/WORKFLOW_LEDGER.md |


## Source inventory

A machine-readable source inventory is included at:

- `docs/VERSION_HISTORY_SOURCE_INVENTORY.csv`
- `docs/VERSION_HISTORY_DETECTED_VERSIONS.csv`

A verbose source appendix is included at:

- `docs/VERSION_HISTORY_SOURCE_APPENDIX.md`

## Consolidated source material

The following embedded sections are selected source histories from the uploaded data set and current V1.1.x source documentation. The full source appendix contains every extracted document.


---

### Embedded source: `Version_Workflow/CONSOLIDATED_VERSION_HISTORY.md`

# Version History

## V1.1.1

- Reviewed V1.1.0.1 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Moved iOS inventory import orchestration from `app_runner.cpp` into `EvidenceIntake::importIosInventoryCsvs(...)`.
- Moved cache-SQLite iOS inventory helpers and referenced-path lookup import into `src/ingest/evidence_intake.cpp`.
- Added `EvidenceIntake::importReferencedIosPathLookupFromReuseCache(...)` and preserved run-status reporting through callback injection.
- Replaced detached GUI main ingest worker with a tracked `gIngestThread` that is joined during `WM_DESTROY`.
- Cleared the V1.1.0.1 `apfs_aff4_reader.cpp` C4100 warning without changing behavior.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- Local syntax checks, Linux CMake build, CLI version check, and self-test passed.
- Windows/MSVC build and AFF4/APFS/iOS runtime validation remain required.
- No APFS live traversal replacement, AFF4 read semantic change, Store-V2 parser change, SQLite schema change, or new forensic interpretation output was intentionally added.

## V1.0.30

- Reviewed V1.0.29 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Moved iOS app database record-inventory orchestration into `IosAppDbParser::parseRecordInventories(...)`.
- Reduced `app_runner.cpp` iOS app DB inventory function to a delegating wrapper with status callback preservation.
- Added GUI export thread registry and joined active export workers during `WM_DESTROY` instead of detaching Export Page/Filtered/Checked/Tagged workers.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- No AFF4/APFS traversal, copy-out, Store-V2 parsing, iOS CoreSpotlight schema, or forensic interpretation changes.


V1.0.29 is a narrow hardening release after V1.0.28.2 linked successfully but the PowerShell build wrapper still checked for the stale `1.0.27` version string.

## Changes

- Corrected the versioned PowerShell build wrapper to expect `1.0.29`.
- Closed the parent process copy of redirected subprocess log handles immediately after child process creation.
- Replaced global DLL-directory mutation with `LoadLibraryExW` secure per-module loading for the guarded AFF4 dynamic probe.
- Suspended Win32 ListView redraw during bulk review grid population.
- Added a 50 MB cap for dynamically copied thin-upload export CSVs in both the C++ upload bundler and the standalone PowerShell thin-upload helper.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.

## Validation

- Local C++20 syntax checks passed for `src/app/app_runner.cpp`, `src/parsers/apfs_diagnostic_exporter.cpp`, `src/gui/gui_export_worker.cpp`, and `src/core/app_info.cpp`.
- CMake configure completed with Apple/lzfse detected.
- Windows/MSVC validation remains required.

# V1.0.28.2

V1.0.28.2 is a narrow build/link hotfix after the V1.0.28.1 MSVC build failed with duplicate `isLikelyStoreV2GroupDirectoryName` symbols between `app_runner.obj` and `apfs_diagnostic_exporter.obj`.

## Changes

- Scoped the APFS diagnostic exporter copy of `isLikelyStoreV2GroupDirectoryName()` to the exporter translation unit.
- Updated continuation, roadmap, and suggestions tracker files.
- No extraction, parser, schema, GUI, or forensic interpretation behavior changed.

## Validation

- Local syntax checks were run for `src/parsers/apfs_diagnostic_exporter.cpp`, `src/app/app_runner.cpp`, and `src/core/app_info.cpp`.
- A local object-symbol check confirmed `apfs_diagnostic_exporter.o` no longer exports a public `isLikelyStoreV2GroupDirectoryName` symbol.
- Windows/MSVC validation remains required.

# V1.0.28.1

- Build hotfix for the V1.0.28 Windows/MSVC failure in `src\app\app_runner.cpp` where `asciiLower` was used before declaration after APFS diagnostic writer relocation.
- Added a forward declaration for the existing runner-local helper.
- Kept V1.0.28 APFS diagnostic writer relocation intact.
- No APFS traversal, AFF4 reads, copy-out/staging, Store-V2 parsing, iOS parsing, schema, or GUI behavior was intentionally changed.

# V1.0.28.1

V1.0.28.1 is a thin-upload packaging hotfix after the V1.0.26 AFF4/APFS run and external comparison completed but the thin ZIP failed during PowerShell relative-path inventory generation.

## Changed

- Fixed `tools/Create-SourceProbeUploadZip.ps1` so `Get-RelativePathForThinInventory` no longer uses `[char]'\\'`, which Windows PowerShell treats as a two-character string and rejects.
- Reused the robust relative-path helper for `ExtractedSpotlight` copy paths.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- Added `scripts/Package-V1_0_28_1-macOS-AFF4-ThinFromExistingCase.ps1` for packaging an already-completed V1.0.26 AFF4/APFS case without rerunning the probe.
- Added `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, and `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.

## Not changed

- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.

## Validation

- Reviewed the uploaded V1.0.26 build log; the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.
- Reviewed the user-reported wrapper output showing the AFF4/APFS probe and external comparison completed before packaging failed.
- Local syntax/text checks were performed for modified C++ and PowerShell packaging files. Windows/MSVC V1.0.28.1 validation remains required.

# V1.0.26

- Fixed the remaining thin-upload raw-log leak in the standalone source-probe upload tool by denying raw tool outputs and full raw file inventories.
- Added matching in-app thin-upload deny-list policy for raw AFF4/iOS tool logs and full file-inventory CSVs.
- Updated thin-upload inventory text files to report relative paths instead of full local paths.
- Added bounded hidden Windows subprocess waits to avoid indefinite hangs from prompted/wedged external tools.
- Updated exact AFF4/ZIP byte reads on Windows to use 64-bit `_fseeki64`.
- No APFS traversal, Store-V2 parsing, iOS parsing, GUI schema, or APFS diagnostic writer movement is included in this version.

# V1.0.25

- Fixed the V1.0.24 Windows/MSVC `C2668` ambiguous `buildWhere` compile failure in `src/gui/win32_gui.cpp`.
- Removed the stale local `buildWhere` wrapper left behind after creating `src/gui/gui_view_helpers.h/.cpp`.
- Explicitly routed review-page SQL `WHERE` assembly through the shared `vestigant::spotlight::buildWhere(...)` helper using captured filter state.
- No APFS/AFF4 traversal, Store-V2 parsing, iOS parsing, schema, GUI views, or diagnostic writer behavior was intentionally changed.
- Updated V1.0.25 build/launch/AFF4 wrapper scripts.

# V1.0.18

- Vendored the uploaded Apple/lzfse source tree under `third_party/lzfse`.
- Enabled codec-aware builds when Apple/lzfse is present.
- Added a codec smoke test using a known Apple/lzfse-produced LZVN vector.
- Added AFF4/APFS copy-out summary fields for codec status and decmpfs LZVN/LZFSE row counts.
- Added macOS investigative feature inventory and roadmap documentation.

# V1.0.18

- Added optional Apple/lzfse LZFSE/LZVN codec integration path.
- Added `src/codec/lzfse_codec.h/.cpp` with safe no-output behavior when the codec is not compiled in.
- Added `tools/Prepare-LzfseThirdParty.ps1` to explicitly vendor and manifest Apple/lzfse source under `third_party/lzfse`.
- Updated CMake and no-CMake MSVC build scripts to compile the Apple decoder sources only when the vetted source tree is present.
- Updated APFS decmpfs resource-fork reconstruction so compression types 8/12 call the Apple codec adapter when available and record explicit decode/skipped statuses when unavailable or failed.
- Updated direct AFF4/APFS copy-out to prefer inode data-stream logical size over raw extent-chain end where available.
- Added validation/status documentation for logical-size trim and optional codec integration.

# V1.0.15

- Added AFF4/APFS Store-V2 candidate dual-process comparison.
- New outputs:
  - `aff4_apfs_storev2_candidate_dual_process_compare.csv`
  - `aff4_apfs_storev2_candidate_dual_process_compare_summary.json`
  - `AFF4_APFS_STOREV2_CANDIDATE_DUAL_PROCESS_COMPARE.md`
- The compare output audits raw APFS copy-out candidates against normalized `StagedStoreV2` selections.
- Added packaging and wrapper validation for the new compare outputs.
- Added LZFSE/LZVN source review documentation explaining why APFS structural documentation is authoritative for locating compressed content but not sufficient by itself to enable production codec output.
- Kept normal-mode AFF4/APFS structural diagnostics suppressed while keeping copy-out/staging/parser/enrichment/external-compare outputs enabled.

# V1.0.14

- Moved iOS app DB row parsing into `src/parsers/ios_app_db_parser.cpp`.
- Corrected AFF4/APFS normal-mode logging around suppressed diagnostics.
- Preserved Store-V2 staged parser handoff.

## V1.0.28.1

- Reviewed V1.0.27 Windows/MSVC build and macOS AFF4/APFS thin output.
- Moved the main APFS/AFF4 diagnostic writer families from `src/app/app_runner.cpp` into `src/parsers/apfs_diagnostic_exporter.cpp`.
- Expanded `src/parsers/apfs_diagnostic_exporter.h` with typed writer declarations.
- Kept APFS traversal, Store-V2 parsing, iOS parsing, SQLite schema, GUI behavior, and live extraction behavior unchanged.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.

---

### Embedded source: `Version_Workflow/VERSION_HISTORY_1.md`

## V1.1.6.1

Build hotfix for V1.1.6 after the direct-map probe worker split. Adds the missing Windows-only wide process path helper to `src/parsers/aff4_probe_worker.cpp` and corrects the versioned build wrapper to check for 1.1.6.1. No forensic extraction behavior changes.

## V1.1.6

- Moved the direct-map AFF4/APFS probe body from `src/app/app_runner.cpp` into the new `src/parsers/aff4_probe_worker.cpp` module.
- Added `src/parsers/aff4_probe_worker.h` and wired it into CMake/MSVC build lists.
- Updated app runner call sites to delegate direct-map probe execution through `Aff4ProbeWorker::executeDirectMapReaderProbe(...)`.
- Left the libaff4 dynamic-load probe in `app_runner.cpp` pending a larger dependency split; this is explicitly tracked rather than hidden.

## V1.1.5.1

- Propagated ingest cancellation into guarded AFF4 dynamic/direct probe entry points and selected expensive bounded loops.
- Added case-directory writability preflight before normal logging/database setup.
- Added thin-upload size/policy guard for `exports/upload_samples` in C++ and PowerShell packagers.
- Changed focused iOS 7-Zip extraction log redirection to UTF-8 `Out-File`.
- Wrapped APFS staged Store-V2 diagnostic sample exports in localized error handling.


## V1.1.4

- Repeat-cycle hardening release after V1.1.3 validation.
- Added bplist offset-table/top-object-offset metadata to existing bounded bplist context summaries without claiming full NSKeyedArchiver graph decoding.
- Added safer GUI checked-artifact snapshot helpers for export/page-load requests.
- Strengthened GUI ingest double-click protection with an atomic compare/exchange gate.
- Updated workflow ledger, roadmap, and suggestions tracker for the next AFF4/APFS monolith and comparator work.

## V1.1.3

- Added GUI export-worker cancellation callbacks for shutdown-aware long CSV exports.
- Added secure System32 RichEdit load/cleanup path.
- Wrapped orphan source-row purge deletes in a single transaction.
- Strengthened non-live APFS next-leaf iterator scaffolding for future comparator validation.
- Updated workflow ledger, roadmap checklist, suggestions tracker, and handoff notes.

## V1.1.2

- Added repeat-cycle workflow ledger.
- Added GUI ingest cancellation token/control and runApplication safe cancellation checkpoints.
- Hardened AFF4 dependent DLL search.
- Freed GUI logo bitmap during shutdown.
- Added native parser bulk SQLite PRAGMAs with restoration.
- Added bounded bplist trailer validation metadata to bplist/NSKeyedArchiver context output.

# Version History

## V1.1.1

- Reviewed V1.1.0.1 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Moved iOS inventory import orchestration from `app_runner.cpp` into `EvidenceIntake::importIosInventoryCsvs(...)`.
- Moved cache-SQLite iOS inventory helpers and referenced-path lookup import into `src/ingest/evidence_intake.cpp`.
- Added `EvidenceIntake::importReferencedIosPathLookupFromReuseCache(...)` and preserved run-status reporting through callback injection.
- Replaced detached GUI main ingest worker with a tracked `gIngestThread` that is joined during `WM_DESTROY`.
- Cleared the V1.1.0.1 `apfs_aff4_reader.cpp` C4100 warning without changing behavior.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- Local syntax checks, Linux CMake build, CLI version check, and self-test passed.
- Windows/MSVC build and AFF4/APFS/iOS runtime validation remain required.
- No APFS live traversal replacement, AFF4 read semantic change, Store-V2 parser change, SQLite schema change, or new forensic interpretation output was intentionally added.

## V1.0.30

- Reviewed V1.0.29 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Moved iOS app database record-inventory orchestration into `IosAppDbParser::parseRecordInventories(...)`.
- Reduced `app_runner.cpp` iOS app DB inventory function to a delegating wrapper with status callback preservation.
- Added GUI export thread registry and joined active export workers during `WM_DESTROY` instead of detaching Export Page/Filtered/Checked/Tagged workers.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- No AFF4/APFS traversal, copy-out, Store-V2 parsing, iOS CoreSpotlight schema, or forensic interpretation changes.


V1.0.29 is a narrow hardening release after V1.0.28.2 linked successfully but the PowerShell build wrapper still checked for the stale `1.0.27` version string.

## Changes

- Corrected the versioned PowerShell build wrapper to expect `1.0.29`.
- Closed the parent process copy of redirected subprocess log handles immediately after child process creation.
- Replaced global DLL-directory mutation with `LoadLibraryExW` secure per-module loading for the guarded AFF4 dynamic probe.
- Suspended Win32 ListView redraw during bulk review grid population.
- Added a 50 MB cap for dynamically copied thin-upload export CSVs in both the C++ upload bundler and the standalone PowerShell thin-upload helper.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.

## Validation

- Local C++20 syntax checks passed for `src/app/app_runner.cpp`, `src/parsers/apfs_diagnostic_exporter.cpp`, `src/gui/gui_export_worker.cpp`, and `src/core/app_info.cpp`.
- CMake configure completed with Apple/lzfse detected.
- Windows/MSVC validation remains required.

# V1.0.28.2

V1.0.28.2 is a narrow build/link hotfix after the V1.0.28.1 MSVC build failed with duplicate `isLikelyStoreV2GroupDirectoryName` symbols between `app_runner.obj` and `apfs_diagnostic_exporter.obj`.

## Changes

- Scoped the APFS diagnostic exporter copy of `isLikelyStoreV2GroupDirectoryName()` to the exporter translation unit.
- Updated continuation, roadmap, and suggestions tracker files.
- No extraction, parser, schema, GUI, or forensic interpretation behavior changed.

## Validation

- Local syntax checks were run for `src/parsers/apfs_diagnostic_exporter.cpp`, `src/app/app_runner.cpp`, and `src/core/app_info.cpp`.
- A local object-symbol check confirmed `apfs_diagnostic_exporter.o` no longer exports a public `isLikelyStoreV2GroupDirectoryName` symbol.
- Windows/MSVC validation remains required.

# V1.0.28.1

- Build hotfix for the V1.0.28 Windows/MSVC failure in `src\app\app_runner.cpp` where `asciiLower` was used before declaration after APFS diagnostic writer relocation.
- Added a forward declaration for the existing runner-local helper.
- Kept V1.0.28 APFS diagnostic writer relocation intact.
- No APFS traversal, AFF4 reads, copy-out/staging, Store-V2 parsing, iOS parsing, schema, or GUI behavior was intentionally changed.

# V1.0.28.1

V1.0.28.1 is a thin-upload packaging hotfix after the V1.0.26 AFF4/APFS run and external comparison completed but the thin ZIP failed during PowerShell relative-path inventory generation.

## Changed

- Fixed `tools/Create-SourceProbeUploadZip.ps1` so `Get-RelativePathForThinInventory` no longer uses `[char]'\\'`, which Windows PowerShell treats as a two-character string and rejects.
- Reused the robust relative-path helper for `ExtractedSpotlight` copy paths.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- Added `scripts/Package-V1_0_28_1-macOS-AFF4-ThinFromExistingCase.ps1` for packaging an already-completed V1.0.26 AFF4/APFS case without rerunning the probe.
- Added `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, and `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.

## Not changed

- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.

## Validation

- Reviewed the uploaded V1.0.26 build log; the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.
- Reviewed the user-reported wrapper output showing the AFF4/APFS probe and external comparison completed before packaging failed.
- Local syntax/text checks were performed for modified C++ and PowerShell packaging files. Windows/MSVC V1.0.28.1 validation remains required.

# V1.0.26

- Fixed the remaining thin-upload raw-log leak in the standalone source-probe upload tool by denying raw tool outputs and full raw file inventories.
- Added matching in-app thin-upload deny-list policy for raw AFF4/iOS tool logs and full file-inventory CSVs.
- Updated thin-upload inventory text files to report relative paths instead of full local paths.
- Added bounded hidden Windows subprocess waits to avoid indefinite hangs from prompted/wedged external tools.
- Updated exact AFF4/ZIP byte reads on Windows to use 64-bit `_fseeki64`.
- No APFS traversal, Store-V2 parsing, iOS parsing, GUI schema, or APFS diagnostic writer movement is included in this version.

# V1.0.25

- Fixed the V1.0.24 Windows/MSVC `C2668` ambiguous `buildWhere` compile failure in `src/gui/win32_gui.cpp`.
- Removed the stale local `buildWhere` wrapper left behind after creating `src/gui/gui_view_helpers.h/.cpp`.
- Explicitly routed review-page SQL `WHERE` assembly through the shared `vestigant::spotlight::buildWhere(...)` helper using captured filter state.
- No APFS/AFF4 traversal, Store-V2 parsing, iOS parsing, schema, GUI views, or diagnostic writer behavior was intentionally changed.
- Updated V1.0.25 build/launch/AFF4 wrapper scripts.

# V1.0.18

- Vendored the uploaded Apple/lzfse source tree under `third_party/lzfse`.
- Enabled codec-aware builds when Apple/lzfse is present.
- Added a codec smoke test using a known Apple/lzfse-produced LZVN vector.
- Added AFF4/APFS copy-out summary fields for codec status and decmpfs LZVN/LZFSE row counts.
- Added macOS investigative feature inventory and roadmap documentation.

# V1.0.18

- Added optional Apple/lzfse LZFSE/LZVN codec integration path.
- Added `src/codec/lzfse_codec.h/.cpp` with safe no-output behavior when the codec is not compiled in.
- Added `tools/Prepare-LzfseThirdParty.ps1` to explicitly vendor and manifest Apple/lzfse source under `third_party/lzfse`.
- Updated CMake and no-CMake MSVC build scripts to compile the Apple decoder sources only when the vetted source tree is present.
- Updated APFS decmpfs resource-fork reconstruction so compression types 8/12 call the Apple codec adapter when available and record explicit decode/skipped statuses when unavailable or failed.
- Updated direct AFF4/APFS copy-out to prefer inode data-stream logical size over raw extent-chain end where available.
- Added validation/status documentation for logical-size trim and optional codec integration.

# V1.0.15

- Added AFF4/APFS Store-V2 candidate dual-process comparison.
- New outputs:
  - `aff4_apfs_storev2_candidate_dual_process_compare.csv`
  - `aff4_apfs_storev2_candidate_dual_process_compare_summary.json`
  - `AFF4_APFS_STOREV2_CANDIDATE_DUAL_PROCESS_COMPARE.md`
- The compare output audits raw APFS copy-out candidates against normalized `StagedStoreV2` selections.
- Added packaging and wrapper validation for the new compare outputs.
- Added LZFSE/LZVN source review documentation explaining why APFS structural documentation is authoritative for locating compressed content but not sufficient by itself to enable production codec output.
- Kept normal-mode AFF4/APFS structural diagnostics suppressed while keeping copy-out/staging/parser/enrichment/external-compare outputs enabled.

# V1.0.14

- Moved iOS app DB row parsing into `src/parsers/ios_app_db_parser.cpp`.
- Corrected AFF4/APFS normal-mode logging around suppressed diagnostics.
- Preserved Store-V2 staged parser handoff.

## V1.0.28.1

- Reviewed V1.0.27 Windows/MSVC build and macOS AFF4/APFS thin output.
- Moved the main APFS/AFF4 diagnostic writer families from `src/app/app_runner.cpp` into `src/parsers/apfs_diagnostic_exporter.cpp`.
- Expanded `src/parsers/apfs_diagnostic_exporter.h` with typed writer declarations.
- Kept APFS traversal, Store-V2 parsing, iOS parsing, SQLite schema, GUI behavior, and live extraction behavior unchanged.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.

---

### Embedded source: `Version_Workflow/VERSION_HISTORY_2.md`

## V1.0.14

- Isolated raw AFF4/APFS copy-out row outputs into unique per-target folders under `ExtractedSpotlight/ApfsCopyOutByTarget`.
- Prevents duplicate same-name Store-V2 candidates from overwriting the file selected by normalized staging.
- Thin upload packaging now excludes raw `ApfsCopyOutByTarget` duplicate sources while keeping normalized `StagedStoreV2` samples and CSV provenance.
- Expected benchmark: stage CSV, actual staged file sizes, and external Vestigant manifest should agree for matching staged relative paths.

## V1.0.12

## V1.0.12

- Added opt-in AFF4/APFS structural diagnostic CSV output mode.
- Normal AFF4/APFS source-probe runs now suppress heavy structural APFS diagnostic CSVs while keeping copy-out, staging, parser, enrichment, and external-comparison outputs.
- Added `--aff4-apfs-diagnostic-outputs` / `--diagnostic-apfs-csvs` for full support runs.
- Added callback-driven `ApfsVolumeReader::enumerateDirectory()` lower-bound iterator implementation for isolated APFS directory walk testing.
- Removed low-risk duplicated iOS parser wrapper functions from `app_runner.cpp`.
- Confirmed GUI view registry ownership remains centralized in `view_registry`.
- Updated wrapper validation so suppressed diagnostics do not block normal external comparison.

Delayed:
- Full iOS row parser migration awaits parser-independent row sink.
- Live APFS traversal replacement awaits iterator parity benchmarks.
- LZFSE/LZVN remains pending vetted codec and test vectors.

## V1.0.12

- Continued APFS/AFF4 modularization without changing the already-working Store-V2 staging pipeline from V1.0.8.
- Moved APFS B-tree table-of-contents key/value decoding into `src/parsers/apfs_aff4_reader.*` and left `app_runner.cpp` with thin compatibility wrappers.
- Updated iOS app database table processing to use `IosAppDbTableParseDecision` from `src/parsers/ios_app_db_parser.*` for parser routing, reducing app-runner-local classification branching.
- Kept full iOS row parsers in `app_runner.cpp` for now because they still depend on local SQLite row-binding and timestamp helper state.
- Corrected stale AFF4 run-status wording: AFF4/APFS is no longer reported as unimplemented; it is described as an active guarded staged pipeline.
- Preserved diagnostic CSVs because V1.0.8 still needs support outputs for external-compare mismatch analysis and before promoting AFF4/APFS to ordinary `discoverStores()` ingest.
- Did not add LZFSE/LZVN; this remains blocked on vetted source, MSVC/Linux integration, and test vectors.

Validation performed in the coding environment:

- CMake configure: PASS.
- New parser modules compiled before timeout: PASS.
- Full Linux build reached `app_runner.cpp` and timed out due to compilation time; no compile error was observed before timeout.
- Windows/MSVC build still requires validation on the Windows system.

## V1.0.8

- Added `src/parsers/ios_app_db_parser.h/.cpp` for iOS app database table classification, special-parser routing, and KnowledgeC snippet assembly.
- Added `src/parsers/apfs_aff4_reader.h/.cpp` with a callback-driven APFS lower-bound directory-iterator scaffold and directory-record decoder.
- Added parser module smoke tests and build integration for CMake and MSVC no-CMake builds.
- Preserved V1.0.7 live AFF4/APFS copy-out behavior; live traversal replacement is delayed until iterator parity can be benchmarked.

V1.0.7: Added dedicated APFS module boundary and fixed direct AFF4/APFS copy-status/staging classification.

## V1.0.6 - MSVC preview-status helper scope hotfix

V1.0.6 fixes a Windows/MSVC compile-scope issue introduced in V1.0.5. The direct AFF4/APFS copy-out code referenced `directPreviewStatusForBytes()` before the helper was visible to MSVC. The helper is now available at file scope before both the guarded/indexed and direct AFF4/APFS copy-out paths. No AFF4/APFS runtime behavior was intentionally changed beyond making the V1.0.5 target-index/copy-out work compile on Windows.

## V1.0.4 - AFF4/APFS direct traversal limit cleanup and Store-V2 namespace seeding

V1.0.4 fixes the stale build-script version check from V1.0.3 and cleans up the direct AFF4/APFS traversal behavior. The direct APFS root-tree scan now terminates by queue exhaustion and visited-node cycle protection instead of the prior node/record/depth hard caps. It also records direct directory entries independent from the bounded upload name-sample CSV and uses those entries to recursively seed Store-V2 child copy-attempt rows with group and APFS path context. Full target-guided INODE/FILE_EXTENT copy-out is deferred to V1.0.5 and should be moved out of `app_runner.cpp` into a dedicated APFS lookup module before implementation.

# Version History

## V1.0.1 - AFF4/APFS direct filesystem-tree target scan

V1.0.1 is a focused macOS AFF4/APFS diagnostic follow-up to V1.0.0. The V1.0.0 run confirmed that the direct AFF4 map reader reached APFS container metadata, checkpoints, volume superblocks, volume object maps, and root-tree lookup rows, but did not stage Store-V2 because the APFS filesystem namespace scan was still too shallow.

Changes:

- Added a bounded direct AFF4/APFS filesystem-tree target scan starting from resolved volume root-tree objects.
- Resolves non-root APFS B-tree child nodes through each volume OMAP where possible.
- Prioritizes likely Data volumes during target scanning.
- Records namespace-name samples and Spotlight target hits for `.Spotlight-V100` / `Store-V2` style paths.
- Always writes explicit target scan, inode probe, xattr probe, file-extent probe, copy-out, and staging output files, even when no target is found, so the upload bundle clearly distinguishes no-hit/incomplete states from missing-output packaging errors.
- Keeps actual file copy-out gated until inode, xattr, dstream, file extent, sparse/gap, zero-block, decmpfs, and resource-fork provenance can be recorded defensibly.

Validation summary:

- Linux CMake configure/build passed.
- `VestigantSpotlightTests` self-test passed.
- CLI version check reports `Vestigant Spotlight v1.0.1`.
- Static raw-string/long-line check passed for MSVC C2026 risk.
- Windows/MSVC build, GUI runtime, and live AFF4/APFS target scan still require validation on the Windows evidence workstation.


# Vestigant Spotlight V1.0.0 Notes

V1.0.0 starts the post-0.9.x V1 line. It keeps the stable V0_9_60 GUI/iOS baseline and moves the next work back toward macOS Spotlight investigation from AFF4/APFS forensic images.

## What changed in V1.0.0

- Version identifiers were advanced to `1.0.0` in `VERSION`, `VERSION.txt`, `CMakeLists.txt`, and `src/core/app_info.cpp`.
- Added a V1 AFF4/APFS diagnostic rerun artifact set written during AFF4 source-probe runs:
  - `AFF4_APFS_V1_DIAGNOSTIC_RERUN_PLAN.md`
  - `aff4_apfs_v1_diagnostic_checklist.csv`
  - `aff4_apfs_v1_diagnostic_plan_summary.json`
- Added those V1 AFF4/APFS diagnostic files to the thin-upload review index and upload bundle copy list.
- Updated the single-AFF4 probe wrapper defaults for the V1.0.0 case/output names.
- Added concrete V1.0.0 PowerShell scripts:
  - `scripts/Build-V1_0_0.ps1`
  - `scripts/Launch-V1_0_0-GUI.ps1`
  - `scripts/Run-V1_0_0-macOS-AFF4-Probe-AndZip.ps1`
  - copy/paste command text files for GUI and AFF4/APFS testing.
- Kept AFF4/APFS and Raw IMG/DD source-type paths visible as staged/experimental workflows rather than hiding them.

## V1.0.0 decision rule

Do not make broad APFS reconstruction changes from the old V0_8_69 external-compare bundle alone. First run a fresh V1.0.0 strict single-AFF4 diagnostic against:

`O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`

Then review the new thin upload to classify the next fix as one of:

- AFF4 container/virtual read access,
- APFS checkpoint or object-map resolution,
- APFS filesystem root-tree namespace traversal,
- Spotlight target inode/xattr/extent correlation,
- sparse/gap/zero-physical-block reconstruction policy,
- decmpfs/resource-fork handling,
- staged Store-V2 parsing/enrichment,
- external-reference comparison,
- macOS investigator views.

## Validation performed in this package

- Reviewed uploaded `V0_9_60_build.log`; it shows CLI, tests, and GUI linked successfully and the built binary reported `Vestigant Spotlight v0.9.60`.
- Reviewed the old `Upload_Thin_V0_8_69_ExternalCompare.zip` enough to confirm historical AFF4/APFS extraction reached staged Store-V2 parsing/enrichment and external comparison, but it is old and should not be treated as current V1 truth.
- Performed a Linux CMake build and ran the included self-test for V1.0.0 in this environment.
- Performed static checks for common MSVC C2026 raw-string risk.

Windows/MSVC GUI build and the live AFF4 image probe still require execution on the user's Windows machine with access to the standard O:, Q:, T:, and D: paths.

---

# Vestigant Spotlight V0_9_60 Notes

V0_9_60 is a V1 production-readiness cleanup after V0_9_57 compiled and ran on Windows. It improves the processing workflow and review workflow without changing parser interpretation logic.

Key changes:
- The Case Information bottom log is now the live processing log. It clears at run start, timestamps messages, mirrors run/progress status, and emits periodic heartbeat messages while processing continues.
- View loading now shows an explicit marquee progress indicator above the investigation grid and a loading message in the details pane so long SQLite view loads are not mistaken for hangs.
- The V1 GUI source selector now exposes only fully implemented Folder and ZIP intake paths. AFF4/APFS and raw image support remain roadmap items and are not presented as clickable V1 options.
- Legacy V7-only schema tables/indexes were removed from new case initialization.
- CLI/operator self-test mode is deprecated; the automated test executable uses an internal automated self-test path.
- Duplicate AFF4/APFS child/descendant root-tree probe output writers were consolidated into one traversal-output writer.

Validation summary:
- Linux CMake configure/build passed.
- VestigantSpotlightTests passed.
- C++20 syntax checks passed for modified non-Windows translation units.
- Windows/MSVC GUI compile and runtime validation remain required.

# Version History

## V0_9_60 - Windows GUI forward-declaration compile hotfix

V0_9_60 is a focused Windows/MSVC GUI build hotfix after V0_9_56 reached the GUI compile stage and failed with `C3861: setReviewSummary identifier not found` in `src\gui\win32_gui.cpp`. The fix adds a forward declaration for `setReviewSummary(const std::wstring&)` before the custom view-set helper functions that call it. No parser, ingest, cache, ZIP, FFS inventory, app DB, export, or forensic interpretation behavior was intentionally changed.

## V0_9_60 - Windows MSVC batch-label build hotfix

V0_9_60 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55

GUI-focused V1 readiness update. Added supplied Vestigant logo branding, cleaned the Case Information / Build Processing layout, added live elapsed-time processing telemetry to the bottom Case Information log pane, added case-persisted Custom view sets for macOS/iOS investigation tabs, and repaired tag-management schema availability for older existing cases. No parser, ZIP staging, cache, FFS inventory, app DB classification, or forensic interpretation behavior was intentionally changed.

## V0_9_54

V1 production cleanup. Removed visible developer/testing controls from the investigator GUI, removed legacy V7 importer source/build paths, and moved GUI review-view SQL ownership into the database layer while preserving older case compatibility.

---

### Embedded source: `Version_Workflow/VERSION_HISTORY_4.md`

# Vestigant Spotlight Version History

This file is now the current consolidated version-history entry point.

See `docs/CONSOLIDATED_VERSION_HISTORY.md` for the active, readable history.
Older per-version notes remain in the repository for traceability.

## V0_9_33

V0_9_33 reviewed the V0_9_31 build/thin result and found the run completed successfully with stable compact-mode counts. This release focuses on making the iOS Spotlight review workflow more usable for investigators:

- Added iOS - Investigator Overview as a start-here GUI view.
- Added iOS - Direct User Message Review for direct Apple Messages/SMS/RCS/iMessage text recovered from Spotlight compact context.
- Added iOS - Direct User Message Thread Summary to group direct messages by available contact/thread/handle context.
- Added iOS - Timeline Month Summary for compact timeline triage by month/category/anomaly.
- Added normal investigator exports for these views and smoke-test coverage.
- Preserved compact normal iOS mode and did not reintroduce full raw property, FFS, or app DB materialization.

---

### Embedded source: `Version_Workflow/VERSION_HISTORY_5.md`

# V0_8_99 Build Script Hotfix

- Converted Windows batch and PowerShell scripts to CRLF line endings.
- Fixes Windows batch error: `The system cannot find the batch label specified - TryVsPath`.
- No intended parser, database, GUI, or export behavior change from V0_8_93_3.

# Vestigant Spotlight Release Notes / Consolidated History

## V0_8_99

Focus: GUI tab containment hotfix for iOS investigation views. The MacOS Investigation View hides iOS-only views, and iOS buttons keep the shared review grid in the iOS Investigation View. No parser/export behavior change from V0_8_93.



## V0_8_99

Focus: iOS parsed app database records upload visibility and review packaging after V0_8_90 GUI validation.

Changes:
- Fixed app-generated Upload bundle copy list so `ios_app_parsed_records.csv` and `ios_app_parsed_record_summary.csv` are copied into `Upload/exports` when generated.
- Preserved the V0_8_90 parsed app database row extraction behavior; no intended Store-V2 parser behavior change.
- Updated build/run/package scripts and documentation to V0_8_99 paths.

Validation target:
- Windows/MSVC build completes.
- GUI iOS ingest completes.
- `exports/ios_app_parsed_records.csv` has nonzero rows when parsed app DB rows exist.
- `Upload/exports/ios_app_parsed_records.csv` and `Upload/exports/ios_app_parsed_record_summary.csv` are present in the thin upload bundle.

Current limitation:
- Parsed app DB records remain generic/dynamic-schema extraction. Specific message/call/browser/application row parsers and record-level Spotlight-to-app-row matching remain future phases.

## V0_8_99

Focus: merge useful Codex V0_8_89_1 work while keeping the active iOS investigation path forward.

Changes:
- Added generic read-only parsed-row extraction for staged iOS app SQLite databases. New outputs include `ios_app_parsed_records.csv` and `ios_app_parsed_record_summary.csv`, with matching GUI views for Parsed App Records and Parsed App Summary.
- Added conservative app-database row extraction for high-value table families: messages, message attachments, participants, calls, web/Safari history, mail, calendar, contacts, and chat-style tables. Parsed rows are review leads, not proof of usage/deletion or exact Spotlight-to-row correlation.
- Improved iOS database residency candidates so they can report when parsed app records are available for a matching database family.
- Integrated Codex AFF4/APFS direct-reader progress as a guarded diagnostic path for BlackBag/LZ4 `aff4:DiscontiguousImage` layouts: pre-open AFF4 guard, direct AFF4 ZIP map/index/data reader, APFS container/volume/root-tree resolution, filesystem namespace seed outputs, and bounded Spotlight target/copy-attempt diagnostics where available.
- Preserved the main V0_8_89 iOS FFS ZIP inventory fixes: safe slash-based ZIP-entry parsing, 7-Zip `l -slt` inventory, sampled thin-upload CSV handling, and consolidated release/history docs.

Limitations:
- iOS app parsed rows are generic schema-driven extractions. App-specific deleted/live semantics, thread reconstruction, attachment reconstruction, and exact Spotlight-to-row matching remain future work.
- CoreSpotlight dbStr/property/category map decoding remains incomplete.
- AFF4/APFS direct traversal remains diagnostic/experimental. It can reach APFS metadata/root-tree/target-name levels on the supplied BlackBag/LZ4 AFF4 path, but the mature V0_8_74 copy-out/staging behavior still needs full direct-reader parity.

## V0_8_99

Focus: move the iOS app-database stage from table counts toward investigator-reviewable records while keeping conservative forensic wording.

Changes:
- Added generic read-only row parsing for staged iOS SQLite app databases when known high-value tables are present. Current target families include messages, message attachments, participants, calls, Safari/web history, mail, calendar, contacts, and chat-style tables.
- Added `ios_app_parsed_records.csv` and `ios_app_parsed_record_summary.csv`, plus matching GUI views, so investigators can review extracted app-database rows instead of only table/count inventory.
- Added `ios_app_parsed_records` to the case database with source database, table, category, timestamp, participant/contact, URL, title, file path, identifier, snippet, status, and provenance fields.
- Reduced `ios_database_residency_candidates.csv` expansion by aggregating database/table/parsed-record matches before export. This keeps the pivot useful on large full-file-system ZIPs without multiplying each Spotlight candidate by every matching table-count row.
- Thin upload packaging now includes the parsed app-record summary and samples the potentially large parsed-record CSV.

Expected validation indicators:
- Windows/MSVC build completes and the CLI reports `Vestigant Spotlight v0.8.99`.
- Self-test passes.
- Full iOS GUI ingest logs `parsed_app_records=<nonzero>` when supported staged app databases contain target rows.
- `ios_app_parsed_record_summary.csv` shows which databases/tables produced parsed rows.
- Database residency candidates may report `POTENTIAL_PARSED_APP_RECORDS_AVAILABLE`; this means relevant parsed app records exist for the database family, not that a specific CoreSpotlight string has been row-matched.

Current limitation:
- This is a generic, schema-tolerant parser pass. It does not yet implement app-specific deleted/live semantics, message-thread reconstruction, attachment file carving, or exact Spotlight-to-app-row correlation.

## V0_8_99

Focus: iOS investigation inventory reliability after V0_8_88_1 GUI validation.

Changes:
- Fixed the iOS FFS ZIP inventory failure seen in V0_8_88_1 where the generated extractor logged `ios_ffs_inventory_7z_list_error=Exception calling "GetFileName" ... Illegal characters in path` and therefore imported zero FFS/app database inventory rows.
- Replaced Windows `IO.Path.GetFileName()` parsing for ZIP entry names with safe slash-based ZIP-entry leaf/extension parsing. This avoids Windows path-character restrictions when reading iOS ZIP central-directory entries.
- Skips archive metadata records from `7z l -slt` before treating output as ZIP entries.
- Updated quick diagnostic PowerShell with the same safe ZIP-entry parsing logic.
- Updated thin upload packaging so very large iOS CSVs are sampled in the upload ZIP while full CSVs remain in the local case folder. This is intended to prevent the FFS inventory from making the upload bundle excessively large.
- No intended Store-V2 parser behavior change from V0_8_88_1.

Expected validation indicators:
- Windows/MSVC build completes.
- GUI iOS ingest completes.
- `ios_ffs_file_inventory.csv` has nonzero rows for the 39 GiB iOS FFS ZIP.
- `ios_app_database_inventory.csv` has nonzero rows if known app databases are present in the ZIP.
- `ios_app_database_record_inventory.csv` and `ios_app_database_record_summary.csv` populate table/count inventory for extracted SQLite databases that can be opened read-only.

Current limitation:
- V0_8_99 still performs table/count inventory for app databases. It does not yet parse message bodies, call rows, WhatsApp rows, Signal rows, Telegram rows, Safari history rows, or other content-level app database records.


## V0_8_99

Focus: make iOS Phase 2/6 inventory usable on large iOS FFS ZIPs and add first record-table inventory for local app databases.

Changes:
- Replaced ZIP64-sensitive .NET-only FFS ZIP inventory with a 7-Zip `l -slt` central-directory inventory path, with .NET fallback when 7-Zip is unavailable.
- Focused iOS ZIP staging now writes a full `ios_ffs_file_inventory.csv` from the FFS ZIP entry listing, not only from staged CoreSpotlight files.
- Known local app database candidates are extracted to `EvidenceStaging/ios_app_databases` for limited record-table inventory.
- Added `extracted_path` to `ios_app_database_inventory` so later parsing can operate on staged database copies rather than the source ZIP.
- Added table/view/export `ios_app_database_record_inventory.csv` for SQLite table names, row counts, sample columns, and table categories.
- Added table/view/export `ios_app_database_record_summary.csv` for summarized database-resident artifact families.
- Database residency candidates now distinguish database-family presence from the presence of relevant record tables.
- Updated quick iOS diagnostics to use the same 7-Zip inventory approach and to collect FFS/app-database inventory without requiring a full GUI ingest.
- Kept iOS views in the iOS Investigation tab and kept release/history documentation consolidated.

Limitations:
- V0_8_99 performs app database table/count inventory, not content-level parsing of SMS.db, CallHistory, WhatsApp, Signal, Telegram, Safari, or other app database rows.
- `POTENTIAL_RECORD_TABLE_PRESENT_IN_APP_DB` means a likely relevant table exists and has rows; it does not yet prove a specific Spotlight string corresponds to a specific live app database row.
- `SPOTLIGHT_ONLY_FILE_MISSING_OR_UNRESOLVED` means the normalized path was absent from the enumerated FFS ZIP inventory. It does not prove user deletion.
- CoreSpotlight dbStr/property-map decoding remains a future parser phase.

## V0_8_87

Added the first iOS investigation phase scaffolding: iOS tab routing cleanup, initial FFS ZIP inventory CSVs, local app-database inventory CSVs, Spotlight referenced-path extraction, missing-from-FFS candidates, and database-residency candidate views/exports. V0_8_87 validation showed CoreSpotlight parsing still worked, but FFS/app database inventory remained empty on the large iOS ZIP; V0_8_99 fixes that by using 7-Zip entry listing and staged database extraction.

## V0_8_86

Added iOS investigator pivots for protection class summary, artifact hint summary, and per-record investigation hints. Added corresponding GUI views, exports, upload packaging, and consolidated release/history documentation.

## V0_8_85_1

Compile hotfix for V0_8_85. Split oversized SQLite SQL raw string literals in `src\db\case_db.cpp` to avoid MSVC `C2026`.

## V0_8_85

Added iOS record string-probe summary export/view, corrected iOS store parse summary placeholder accounting, and removed caps from main iOS timeline and string-probe exports.

## V0_8_84

Changed iOS Store-V2 database selection to parse one primary database per CoreSpotlight group, preferring `store.db` while preserving `.store.db` alternates in inventory/hash outputs.

## V0_8_83

Fixed quick-diagnostic ZIP packaging issues and added redacted iOS investigation summaries/domain URL summaries and cleaner Upload packaging for iOS CSVs.

## V0_8_82

Consolidated release/history documentation and added fast iOS diagnostic collection intended to avoid full GUI ingest when only ZIP/CoreSpotlight inventory is needed.

## V0_8_81

Added iOS string-probe fallback preservation and improved iOS tab status reporting after focused CoreSpotlight parsing.

## Earlier V0_8.x summary

Earlier V0_8.x builds moved the project toward iOS CoreSpotlight ZIP intake, macOS AFF4/APFS readiness, external Store-V2 comparison, upload/thin-bundle workflow, and GUI parity. Legacy V7 import remains deprecated for normal workflows unless a current dependency requires it.

---

### Embedded source: `Version_Workflow/RELEASE_NOTES.md`

# Vestigant Spotlight Release Notes

Current version: 0.9.33

## V0_9_33

- Reviewed V0_9_30 build/thin output; V0_9_30 built successfully and completed the iOS reuse-cache workflow.
- Replaced the overly granular `ios_spotlight_message_contact_summary.csv` with a compact bucketed contact/thread summary so normal investigator exports stay usable.
- Added `ios_spotlight_message_contact_thread_detail_sample.csv` for bounded representative thread/handle examples.
- Added message body focus summary, parser diagnostics action summary, Plaso/L2T timeline sample, and case quality dashboard views/exports.
- Preserved compact normal iOS mode: no full FFS inventory, broad app DB records, or full native property DB materialization by default.

## V0_9_33

- Reviewed V0_9_29 build/thin output before changing code. The Windows/MSVC build completed, GUI linked, and the iOS reuse-cache run reached `complete_success` with stable compact counts.
- Consolidated stalled/scattered help into `docs/CONSOLIDATED_USER_MANUAL.md`.
- Consolidated current version history into `docs/CONSOLIDATED_VERSION_HISTORY.md`.
- Updated top-level `HELP.md`, `RELEASE_NOTES.md`, and `VERSION_HISTORY.md` to point to the consolidated documentation.
- Improved compact iOS Spotlight message/body review extraction from same-record Spotlight text context, including mail/message title, snippet, description, supporting text, and suggested contact/thread context where present.
- Added parser diagnostics detail view/export so native failures and partial decode errors are visible at record/sample level, not only as summary counts.
- Added GUI view `iOS - Parser Diagnostics Detail Sample`.
- Added normal export `parser_diagnostics_detail_sample.csv`.
- Preserved compact normal iOS mode; full native/dbStr/property persistence and broad FFS/app DB materialization remain support/diagnostic options.

Validation in this environment:

- C++ syntax checks for modified files.
- SQLite schema smoke test expanded to include the new diagnostics detail view.
- Raw-string fragment length check to reduce recurrence of MSVC C2026 oversized literal failures.
- Linux build/self-test attempted where feasible; Windows/MSVC remains required validation.

---

### Embedded source: `Current_V1_1_7_source/VERSION_HISTORY.md`

## V1.1.6.1

Build hotfix for V1.1.6 after the direct-map probe worker split. Adds the missing Windows-only wide process path helper to `src/parsers/aff4_probe_worker.cpp` and corrects the versioned build wrapper to check for 1.1.6.1. No forensic extraction behavior changes.

## V1.1.6

- Moved the direct-map AFF4/APFS probe body from `src/app/app_runner.cpp` into the new `src/parsers/aff4_probe_worker.cpp` module.
- Added `src/parsers/aff4_probe_worker.h` and wired it into CMake/MSVC build lists.
- Updated app runner call sites to delegate direct-map probe execution through `Aff4ProbeWorker::executeDirectMapReaderProbe(...)`.
- Left the libaff4 dynamic-load probe in `app_runner.cpp` pending a larger dependency split; this is explicitly tracked rather than hidden.

## V1.1.5.1

- Propagated ingest cancellation into guarded AFF4 dynamic/direct probe entry points and selected expensive bounded loops.
- Added case-directory writability preflight before normal logging/database setup.
- Added thin-upload size/policy guard for `exports/upload_samples` in C++ and PowerShell packagers.
- Changed focused iOS 7-Zip extraction log redirection to UTF-8 `Out-File`.
- Wrapped APFS staged Store-V2 diagnostic sample exports in localized error handling.


## V1.1.4

- Repeat-cycle hardening release after V1.1.3 validation.
- Added bplist offset-table/top-object-offset metadata to existing bounded bplist context summaries without claiming full NSKeyedArchiver graph decoding.
- Added safer GUI checked-artifact snapshot helpers for export/page-load requests.
- Strengthened GUI ingest double-click protection with an atomic compare/exchange gate.
- Updated workflow ledger, roadmap, and suggestions tracker for the next AFF4/APFS monolith and comparator work.

## V1.1.3

- Added GUI export-worker cancellation callbacks for shutdown-aware long CSV exports.
- Added secure System32 RichEdit load/cleanup path.
- Wrapped orphan source-row purge deletes in a single transaction.
- Strengthened non-live APFS next-leaf iterator scaffolding for future comparator validation.
- Updated workflow ledger, roadmap checklist, suggestions tracker, and handoff notes.

## V1.1.2

- Added repeat-cycle workflow ledger.
- Added GUI ingest cancellation token/control and runApplication safe cancellation checkpoints.
- Hardened AFF4 dependent DLL search.
- Freed GUI logo bitmap during shutdown.
- Added native parser bulk SQLite PRAGMAs with restoration.
- Added bounded bplist trailer validation metadata to bplist/NSKeyedArchiver context output.

# Version History

## V1.1.1

- Reviewed V1.1.0.1 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Moved iOS inventory import orchestration from `app_runner.cpp` into `EvidenceIntake::importIosInventoryCsvs(...)`.
- Moved cache-SQLite iOS inventory helpers and referenced-path lookup import into `src/ingest/evidence_intake.cpp`.
- Added `EvidenceIntake::importReferencedIosPathLookupFromReuseCache(...)` and preserved run-status reporting through callback injection.
- Replaced detached GUI main ingest worker with a tracked `gIngestThread` that is joined during `WM_DESTROY`.
- Cleared the V1.1.0.1 `apfs_aff4_reader.cpp` C4100 warning without changing behavior.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- Local syntax checks, Linux CMake build, CLI version check, and self-test passed.
- Windows/MSVC build and AFF4/APFS/iOS runtime validation remain required.
- No APFS live traversal replacement, AFF4 read semantic change, Store-V2 parser change, SQLite schema change, or new forensic interpretation output was intentionally added.

## V1.0.30

- Reviewed V1.0.29 Windows/MSVC build log and macOS AFF4/APFS thin ZIP before changes.
- Moved iOS app database record-inventory orchestration into `IosAppDbParser::parseRecordInventories(...)`.
- Reduced `app_runner.cpp` iOS app DB inventory function to a delegating wrapper with status callback preservation.
- Added GUI export thread registry and joined active export workers during `WM_DESTROY` instead of detaching Export Page/Filtered/Checked/Tagged workers.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.
- No AFF4/APFS traversal, copy-out, Store-V2 parsing, iOS CoreSpotlight schema, or forensic interpretation changes.


V1.0.29 is a narrow hardening release after V1.0.28.2 linked successfully but the PowerShell build wrapper still checked for the stale `1.0.27` version string.

## Changes

- Corrected the versioned PowerShell build wrapper to expect `1.0.29`.
- Closed the parent process copy of redirected subprocess log handles immediately after child process creation.
- Replaced global DLL-directory mutation with `LoadLibraryExW` secure per-module loading for the guarded AFF4 dynamic probe.
- Suspended Win32 ListView redraw during bulk review grid population.
- Added a 50 MB cap for dynamically copied thin-upload export CSVs in both the C++ upload bundler and the standalone PowerShell thin-upload helper.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.

## Validation

- Local C++20 syntax checks passed for `src/app/app_runner.cpp`, `src/parsers/apfs_diagnostic_exporter.cpp`, `src/gui/gui_export_worker.cpp`, and `src/core/app_info.cpp`.
- CMake configure completed with Apple/lzfse detected.
- Windows/MSVC validation remains required.

# V1.0.28.2

V1.0.28.2 is a narrow build/link hotfix after the V1.0.28.1 MSVC build failed with duplicate `isLikelyStoreV2GroupDirectoryName` symbols between `app_runner.obj` and `apfs_diagnostic_exporter.obj`.

## Changes

- Scoped the APFS diagnostic exporter copy of `isLikelyStoreV2GroupDirectoryName()` to the exporter translation unit.
- Updated continuation, roadmap, and suggestions tracker files.
- No extraction, parser, schema, GUI, or forensic interpretation behavior changed.

## Validation

- Local syntax checks were run for `src/parsers/apfs_diagnostic_exporter.cpp`, `src/app/app_runner.cpp`, and `src/core/app_info.cpp`.
- A local object-symbol check confirmed `apfs_diagnostic_exporter.o` no longer exports a public `isLikelyStoreV2GroupDirectoryName` symbol.
- Windows/MSVC validation remains required.

# V1.0.28.1

- Build hotfix for the V1.0.28 Windows/MSVC failure in `src\app\app_runner.cpp` where `asciiLower` was used before declaration after APFS diagnostic writer relocation.
- Added a forward declaration for the existing runner-local helper.
- Kept V1.0.28 APFS diagnostic writer relocation intact.
- No APFS traversal, AFF4 reads, copy-out/staging, Store-V2 parsing, iOS parsing, schema, or GUI behavior was intentionally changed.

# V1.0.28.1

V1.0.28.1 is a thin-upload packaging hotfix after the V1.0.26 AFF4/APFS run and external comparison completed but the thin ZIP failed during PowerShell relative-path inventory generation.

## Changed

- Fixed `tools/Create-SourceProbeUploadZip.ps1` so `Get-RelativePathForThinInventory` no longer uses `[char]'\\'`, which Windows PowerShell treats as a two-character string and rejects.
- Reused the robust relative-path helper for `ExtractedSpotlight` copy paths.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- Added `scripts/Package-V1_0_28_1-macOS-AFF4-ThinFromExistingCase.ps1` for packaging an already-completed V1.0.26 AFF4/APFS case without rerunning the probe.
- Added `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, and `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.

## Not changed

- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.

## Validation

- Reviewed the uploaded V1.0.26 build log; the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.
- Reviewed the user-reported wrapper output showing the AFF4/APFS probe and external comparison completed before packaging failed.
- Local syntax/text checks were performed for modified C++ and PowerShell packaging files. Windows/MSVC V1.0.28.1 validation remains required.

# V1.0.26

- Fixed the remaining thin-upload raw-log leak in the standalone source-probe upload tool by denying raw tool outputs and full raw file inventories.
- Added matching in-app thin-upload deny-list policy for raw AFF4/iOS tool logs and full file-inventory CSVs.
- Updated thin-upload inventory text files to report relative paths instead of full local paths.
- Added bounded hidden Windows subprocess waits to avoid indefinite hangs from prompted/wedged external tools.
- Updated exact AFF4/ZIP byte reads on Windows to use 64-bit `_fseeki64`.
- No APFS traversal, Store-V2 parsing, iOS parsing, GUI schema, or APFS diagnostic writer movement is included in this version.

# V1.0.25

- Fixed the V1.0.24 Windows/MSVC `C2668` ambiguous `buildWhere` compile failure in `src/gui/win32_gui.cpp`.
- Removed the stale local `buildWhere` wrapper left behind after creating `src/gui/gui_view_helpers.h/.cpp`.
- Explicitly routed review-page SQL `WHERE` assembly through the shared `vestigant::spotlight::buildWhere(...)` helper using captured filter state.
- No APFS/AFF4 traversal, Store-V2 parsing, iOS parsing, schema, GUI views, or diagnostic writer behavior was intentionally changed.
- Updated V1.0.25 build/launch/AFF4 wrapper scripts.

# V1.0.18

- Vendored the uploaded Apple/lzfse source tree under `third_party/lzfse`.
- Enabled codec-aware builds when Apple/lzfse is present.
- Added a codec smoke test using a known Apple/lzfse-produced LZVN vector.
- Added AFF4/APFS copy-out summary fields for codec status and decmpfs LZVN/LZFSE row counts.
- Added macOS investigative feature inventory and roadmap documentation.

# V1.0.18

- Added optional Apple/lzfse LZFSE/LZVN codec integration path.
- Added `src/codec/lzfse_codec.h/.cpp` with safe no-output behavior when the codec is not compiled in.
- Added `tools/Prepare-LzfseThirdParty.ps1` to explicitly vendor and manifest Apple/lzfse source under `third_party/lzfse`.
- Updated CMake and no-CMake MSVC build scripts to compile the Apple decoder sources only when the vetted source tree is present.
- Updated APFS decmpfs resource-fork reconstruction so compression types 8/12 call the Apple codec adapter when available and record explicit decode/skipped statuses when unavailable or failed.
- Updated direct AFF4/APFS copy-out to prefer inode data-stream logical size over raw extent-chain end where available.
- Added validation/status documentation for logical-size trim and optional codec integration.

# V1.0.15

- Added AFF4/APFS Store-V2 candidate dual-process comparison.
- New outputs:
  - `aff4_apfs_storev2_candidate_dual_process_compare.csv`
  - `aff4_apfs_storev2_candidate_dual_process_compare_summary.json`
  - `AFF4_APFS_STOREV2_CANDIDATE_DUAL_PROCESS_COMPARE.md`
- The compare output audits raw APFS copy-out candidates against normalized `StagedStoreV2` selections.
- Added packaging and wrapper validation for the new compare outputs.
- Added LZFSE/LZVN source review documentation explaining why APFS structural documentation is authoritative for locating compressed content but not sufficient by itself to enable production codec output.
- Kept normal-mode AFF4/APFS structural diagnostics suppressed while keeping copy-out/staging/parser/enrichment/external-compare outputs enabled.

# V1.0.14

- Moved iOS app DB row parsing into `src/parsers/ios_app_db_parser.cpp`.
- Corrected AFF4/APFS normal-mode logging around suppressed diagnostics.
- Preserved Store-V2 staged parser handoff.

## V1.0.28.1

- Reviewed V1.0.27 Windows/MSVC build and macOS AFF4/APFS thin output.
- Moved the main APFS/AFF4 diagnostic writer families from `src/app/app_runner.cpp` into `src/parsers/apfs_diagnostic_exporter.cpp`.
- Expanded `src/parsers/apfs_diagnostic_exporter.h` with typed writer declarations.
- Kept APFS traversal, Store-V2 parsing, iOS parsing, SQLite schema, GUI behavior, and live extraction behavior unchanged.
- Updated continuation handoff, roadmap checklist, and suggestions/fixes tracker.


## V1.1.7

- Moved the AFF4/libaff4 dynamic-load APFS probe body out of `src/app/app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp` as `Aff4ProbeWorker::executeDynamicLoadProbe(...)`.
- `app_runner.cpp` now delegates both AFF4/APFS probe paths to `Aff4ProbeWorker`, removing the remaining `writeAff4CppLiteDynamicLoadProbe(...)` implementation from the orchestrator.
- Added cancellation propagation into shared APFS OMAP traversal helper calls so direct-map and dynamic-load probe paths can observe investigator cancellation during APFS B-tree walks.
- No live APFS interpretation, copy-out/staging rules, Store-V2 parsing, iOS parsing, or SQLite schema changes.

---

### Embedded source: `Current_V1_1_7_generated/validation/V1_1_7_validation_notes.md`

# V1.1.7 Validation Notes

## Baseline

Started from validated V1.1.6.1.

## Scope

- Moved `writeAff4CppLiteDynamicLoadProbe(...)` from `app_runner.cpp` into `Aff4ProbeWorker::executeDynamicLoadProbe(...)`.
- Both large AFF4/APFS probe bodies now live in `src/parsers/aff4_probe_worker.cpp`.
- Added cancellation callback support to shared APFS OMAP traversal helper calls used by direct-map and dynamic-load probe paths.

## Local validation

- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/aff4_probe_worker.cpp`: PASS
- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp`: PASS
- Linux CMake configure/build: PASS
- CLI version check: `Vestigant Spotlight v1.1.7`
- Local self-test: PASS

## Not validated here

- Windows/MSVC full build.
- Windows GUI runtime.
- V1.1.7 macOS AFF4/APFS thin run.

---

## V1.1.7.1 — Build Hotfix, Package Cleanup, and Continuation Baseline Consolidation

Status: generated from V1.1.7 after the Windows/MSVC build reported unresolved helper dependencies in `src/parsers/aff4_probe_worker.cpp`.

Changes:
- Fixed missing worker-local dependencies introduced by moving `writeAff4CppLiteDynamicLoadProbe(...)` into `aff4_probe_worker.cpp`:
  - `shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(...)`
  - `findToolCandidate(...)`
  - `lastWindowsErrorString(...)`
- Preserved both large AFF4/APFS probe bodies outside `app_runner.cpp`:
  - Direct-map reader probe in `Aff4ProbeWorker::executeDirectMapReaderProbe(...)`.
  - Dynamic libaff4 probe in `Aff4ProbeWorker::executeDynamicLoadProbe(...)`.
- Removed obsolete version-specific build/run/launch/package scripts for prior versions from the active `scripts/` directory.
- Removed old root-level package manifests and deletion manifests from the active source root.
- Preserved append-only version history by adding `docs/FULL_VERSION_HISTORY.md` and `docs/VERSION_HISTORY_APPEND_ONLY_POLICY.md`.
- Added/updated continuation and cleanup documentation so the newest source ZIP can be uploaded into a new chat and used as the project baseline.

Validation performed locally before packaging:
- C++20 syntax check for `src/parsers/aff4_probe_worker.cpp`.
- C++20 syntax check for `src/app/app_runner.cpp`.
- Additional build validation pending Windows/MSVC upload.

