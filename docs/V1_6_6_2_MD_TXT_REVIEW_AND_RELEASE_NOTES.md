# V1.6.6.2 Markdown/Text Review and Release Notes

## Review completion

- Reviewed 63 `.md` / `.txt` files from the uploaded V1.6.6.1 source package before making V1.6.6.2 source changes.
- The review found the active project emphasis to be iOS CoreSpotlight/app-database identity, communication, frequency, timeline, and usage attribution, with AFF4/APFS support retained but not the code area touched in V1.6.6.2.
- The most relevant guardrail carried forward from V1.6.6.1 was that KnowledgeC/CoreDuet device/app state streams must not be promoted as identity evidence.

## Changes made after review

- `src/parsers/ios_app_db_parser.cpp`: generic KnowledgeC fallback now leaves `contact_or_participant` empty for device/app state rows, classifies those rows as `KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY`, and marks provenance with `IDENTITY_PROMOTION_SUPPRESSED=True`.
- `src/db/case_db.cpp`: communication and identity SQLite views now use `KNOWLEDGEC_COMMUNICATION_INTENT` and provenance markers rather than generic `KNOWLEDGEC_EVENTS` for communication/identity promotion.
- `tests/main.cpp`: added `runKnowledgeCIdentitySuppressionSmokeTest`, which inserts synthetic device, generic, and communication-intent KnowledgeC rows and validates the generated views.
- Current wrappers/docs were updated to V1.6.6.2.

## Test determination

- iOS thin validation is required because the changed behavior affects iOS KnowledgeC/CoreDuet classification and SQLite iOS communication/identity views.
- AFF4/APFS thin/full validation is not required for this version unless shared schema initialization or build behavior regresses, because V1.6.6.2 does not change AFF4/APFS traversal, copy-out, decompression, source-reader, or Store-V2 staging code.

## Reviewed files

- `.github/pull_request_template.md` — 24 lines
- `BUILD_INSTRUCTIONS.md` — 39 lines
- `CMakeLists.txt` — 86 lines
- `CONSOLIDATED_VERSION_HISTORY.md` — 299 lines
- `HELP.md` — 39 lines
- `KNOWN_ISSUES.md` — 102 lines
- `RELEASE_NOTES.md` — 6 lines
- `V1_3_2_3_DELETED_FILES_MANIFEST.md` — 123 lines
- `VERSION.txt` — 1 lines
- `VERSION_HISTORY.md` — 2139 lines
- `ai_context.md` — 248 lines
- `build-linux/CMakeCache.txt` — 402 lines
- `build-linux/CMakeFiles/TargetDirectories.txt` — 5 lines
- `build-linux/CMakeFiles/VestigantSpotlightCli.dir/link.txt` — 1 lines
- `build-linux/CMakeFiles/VestigantSpotlightTests.dir/link.txt` — 1 lines
- `build-linux/CMakeFiles/vestigant_spotlight_core.dir/link.txt` — 2 lines
- `docs/APFS_TRAVERSAL_CONSOLIDATION_PLAN.md` — 26 lines
- `docs/BUILD_NOTES.md` — 117 lines
- `docs/BaselineVersionHistory.md` — 2125 lines
- `docs/CONSOLIDATED_DEVELOPMENT_NOTES.md` — 1732 lines
- `docs/CONSOLIDATED_USER_MANUAL.md` — 222 lines
- `docs/CONSOLIDATED_VERSION_HISTORY.md` — 281 lines
- `docs/CONTINUATION_HANDOFF.md` — 66 lines
- `docs/DETAILED_ROADMAP_AND_TESTING_TIMELINE.md` — 259 lines
- `docs/EVIDENCE_SOURCE_STAGING_ROADMAP.md` — 81 lines
- `docs/EVIDENCE_SOURCE_STAGING_WORKFLOW.md` — 68 lines
- `docs/FILESYSTEM_INVENTORY_SQLITE_PLAN.md` — 61 lines
- `docs/FULL_VERSION_HISTORY.md` — 2125 lines
- `docs/GITHUB_PROJECT_SETUP.md` — 56 lines
- `docs/GUI_DATABASE_ACCESS_AUDIT_V1_3_2.md` — 19 lines
- `docs/IOS_CORESPOTLIGHT_ROADMAP.md` — 198 lines
- `docs/MACOS_INVESTIGATIVE_FEATURES_CURRENT_AND_ROADMAP.md` — 42 lines
- `docs/NEW_CHAT_CONTINUATION_GUIDE.md` — 49 lines
- `docs/PACKAGE_CLEANUP_SUMMARY.md` — 20 lines
- `docs/PROJECT_ROADMAP_AND_CONTINUATION.md` — 211 lines
- `docs/QUICK_START.md` — 65 lines
- `docs/README.md` — 130 lines
- `docs/ROADMAP_CHECKLIST.md` — 68 lines
- `docs/SOURCE_PACKAGE_CLEANUP_POLICY.md` — 28 lines
- `docs/SUGGESTIONS_AND_FIXES_TRACKER.md` — 71 lines
- `docs/SUPPORT_DIAGNOSTIC_TOOLS_REGISTER.md` — 21 lines
- `docs/THIN_UPLOAD_REVIEW_WORKFLOW.md` — 50 lines
- `docs/TROUBLESHOOTING.md` — 88 lines
- `docs/USER_MANUAL.md` — 46 lines
- `docs/V1_3_7_REQUESTED_FIXES_VERIFICATION.md` — 41 lines
- `docs/V1_4_1_IOS_EXISTENCE_FREQUENCY_MILESTONE.md` — 25 lines
- `docs/V1_6_1_SCHEMA_MODULARIZATION_PLAN.md` — 15 lines
- `docs/V1_6_4_1_FORENSIC_DIRECTIVES_AUDIT.md` — 38 lines
- `docs/VALIDATION_HISTORY.md` — 195 lines
- `docs/VALIDATION_STATUS.md` — 171 lines
- `docs/VERSION_HISTORY_APPEND_ONLY_POLICY.md` — 19 lines
- `docs/WORKFLOW_LEDGER.md` — 390 lines
- `scripts/Run-V1_6_6_1-iOS-CoreSpotlight-AndZip.txt` — 10 lines
- `scripts/Run-V1_6_6_1-macOS-AFF4-Probe-AndZip.txt` — 9 lines
- `third_party/lzfse/CMakeLists.txt` — 137 lines
- `third_party/lzfse/README.md` — 73 lines
- `third_party/lzfse/VESTIGANT_VENDOR_MANIFEST.txt` — 9 lines
- `validation/CONSOLIDATED_VALIDATION_LOGS_AND_NOTES.md` — 2692 lines
- `validation/V1_3_2_1_LOCAL_VALIDATION_NOTES.md` — 7 lines
- `validation/V1_3_2_LOCAL_VALIDATION_NOTES.md` — 14 lines
- `validation/V1_3_2_PRIOR_BASE_VALIDATION_NOTES.md` — 44 lines
- `validation/V1_3_6_LOCAL_VALIDATION.md` — 9 lines
- `validation/V1_3_7_LOCAL_VALIDATION.md` — 9 lines
