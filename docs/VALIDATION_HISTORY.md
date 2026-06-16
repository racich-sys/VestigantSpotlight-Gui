# V1.6.38 - CoreDuet interactionC workflow

- Added CoreDuet `interactionC.db` database-status, summary, and event-review views.
- Added GUI review entries and CSV exports for CoreDuet People / interactionC workflow.
- Added bounded upload samples for interactionC status/summary/events.
- Added `docs/INTERACTIONC_WORKFLOW.md` and running `docs/START_CONTINUATION_CHAT.md`.
- Validation here is static source/package audit only; Windows/MSVC build is still required.

# V1.6.18.1 - Windows GUI subclass hotfix

- Restored `ReviewListSubclassProc` and `ReviewDetailsSplitterSubclassProc` definitions needed by the compacted V1.6.18 Investigation Results controls.
- Added static release-readiness markers for the restored subclass callbacks.
- Windows/MSVC build was not run in this environment; upload `D:\Downloads\V1_6_18_1_build.log` after local build.

## V1.6.18 - iOS thin validation closure and compact GUI header layout

V1.6.18 follows review of the uploaded `Upload_Thin_iOS_CoreSpotlight_V1_6_17.zip`, whose computed SHA256 was `11C80504B37AF0A19FC0DA342FC691C3DF238DFAF3B0EEFCB6C54CC50EE89522`. The V1.6.17 iOS thin reached `complete_success`, generated 203 upload files, and reported `store_count=6`, `valid_store_count=6`, `database_candidate_count=12`, `valid_database_candidate_count=12`, `parser_selected_database_count=6`, `native_decode_mode=CoreFields`, `raw_record_count=344445`, `raw_key_value_count=60665`, `raw_date_candidate_count=0`, `artifact_count=344445`, `usage_evidence_count=228699`, and `timeline_event_count=277823`.

The V1.6.17 email-category precision fix validated on the uploaded thin sample: `EMAIL_ADDRESS_OR_ACCOUNT` decreased from 2913 rows in V1.6.16 to 2702 rows in V1.6.17, and the 5000-row `ios_string_probe_values_sample.csv` review found no email-category rows containing the path/URL/space indicators that drove the V1.6.16 false-positive issue. The remaining zero `raw_date_candidate_count` is documented as normal compact iOS CoreFields behavior in this run; Last_Updated remains on raw records, while broad date expansion requires bounded diagnostic/full support mode.

V1.6.18 changes:

- Compact the Windows GUI Case Information / Build Processing header in `src/gui/win32_gui.cpp` by reducing row heights, combo drop-down extents, explanatory text height, and action-button heights.
- Compact the Investigation Results top action area so the grid starts higher and more rows/details remain visible.
- Update current continuation, quick-start, release, and validation documents that were stale at V1.6.12 or V1.6.7.1.
- Add V1.6.18 build/run wrappers and release-readiness checks.

Validation completed here: V1.6.17 iOS thin upload review PASS for the targeted email-category regression. Linux CMake build and self-test for V1.6.18 should be run before Windows/MSVC packaging. Windows/MSVC build and live V1.6.18 GUI layout verification are not verified here.

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

## V1.6.11 - Parent-inode path reconstruction validation after V1.6.10 thin

V1.6.11 follows review of the uploaded `Upload_Thin_MacOS_AFF4_V1_6_10.zip`, whose uploaded SHA256 file reported `0D42F7F285F8DC526A1E0C1E7A93B217BFD90FAA35BF9A1BD4209FF1154BD39D`. The V1.6.10 thin reached `complete_aff4_apfs_staged_storev2_validation_probe` with `store_count=11`, `valid_store_count=7`, `database_candidate_count=22`, `valid_database_candidate_count=14`, `parser_selected_database_count=7`, `native_decode_mode=AFF4_APFS_STAGED_STOREV2_FullValues`, `raw_record_count=25000`, `raw_key_value_count=6568`, `raw_date_candidate_count=25000`, `artifact_count=25000`, and `timeline_event_count=25000`. The V1.6.10 date-candidate fix was validated because `raw_date_candidates_sample.csv` contained only `Last_Updated` rows while raw probe aliases remained in `raw_key_values`.

The remaining validation defect was parent-inode path reconstruction. The V1.6.10 thin reported `parent_inode_links=24998`, `matched=23871`, and `child_names=1425`, but `reconstructed_paths=0` and `artifacts_updated=0`. That showed relationship evidence was being captured, but post-parse path-chain reconstruction was not using the complete artifact set to recover reviewable path candidates.

Code fixes in V1.6.11:
- Added a post-artifact recursive parent-inode chain reconstruction pass in SQLite enrichment. This pass rebuilds candidate paths after all raw records are materialized, avoiding parser-order loss where children are seen before parent records.
- Preserves absolute path candidates as preferred evidence and retains relative parent-inode chains as review-only evidence when no absolute ancestor path is available.
- Updates `parent_inode_links.reconstructed_path_candidate`, `path_reconstruction_method`, and confidence with chain-derived candidates where the immediate parent-link pass left the candidate blank.
- Applies chain-derived candidates to weak artifact path rows using explicit path provenance values: `PARENT_INODE_CHAIN_RECONSTRUCTION` or `PARENT_INODE_RELATIVE_CHAIN_REVIEW`.
- Updates `vw_path_reconstruction` so the applied-path flag recognizes the new path-source values.
- Adds a focused AFF4/APFS thin validation sample: `aff4_apfs_staged_storev2_path_reconstruction_sample.csv`.
- Updates the macOS AFF4 thin wrapper to require the new path-reconstruction sample, in addition to parser and field-coverage samples.

Validation completed in this packaging environment: Linux CMake build PASS, CLI version `Vestigant Spotlight v1.6.11`, self-test PASS, and a local staged Store-V2 run against uploaded V1.6.10 staged data PASS. Windows/MSVC build, full live V1.6.11 macOS AFF4 thin, and iOS CoreSpotlight regression thin were not run here.

Test determination: run Windows/MSVC build and the macOS AFF4 thin for V1.6.11. iOS thin remains recommended after macOS because native parser/enrichment code is shared, but the V1.6.11 change is macOS path-reconstruction-focused.

## V1.6.6.5 - iOS native CoreSpotlight probe review bridge

V1.6.6.5 was built after review of the uploaded V1.6.6.4 Windows build log and iOS thin bundle. The Windows build log showed a completed V1.6.6.4 build. The iOS thin bundle reached `complete_success`, with 6 valid stores, 344,445 raw records, 22,569 raw key/value rows, 344,445 artifacts, 228,699 usage evidence rows, and 277,823 timeline events.

The V1.6.6.4 thin run showed that timeout-prone V1.6.6.2 exports were fixed, but compact native probe strings were still not flowing into the main iOS text-context and communication review surfaces. `ios_string_probe_category_summary.csv` contained 9,591 message/app string probes and 933 email/account probes, while message/text-context review exports remained empty.

Implemented:
- high-signal `__native_core_probe_string_*` values can now feed same-record text context;
- iOS text-context review exposes `source_field_name` and classifies native SMS/iMessage/mail/account/file-reference probes;
- iOS communication/message review views expose `native_probe_context_count` and `native_probe_context_sample`;
- new communication buckets identify `SPOTLIGHT_MESSAGE_OR_ATTACHMENT_TEXT_PROBE` and `SPOTLIGHT_MAIL_OR_ACCOUNT_TEXT_PROBE`;
- `vw_ios_spotlight_investigator_overview` now uses lightweight base/probe counts to avoid the V1.6.6.4 46-second slow overview export;
- self-test coverage now includes native CoreSpotlight probe-to-text-context and communication-review checks.

Validation performed here:
- Linux CMake build: PASS.
- CLI version: `Vestigant Spotlight v1.6.6.5`.
- Self-test: PASS.
- Windows/MSVC build: not run here.

## V1.6.6.5 Local Validation

- Linux CMake configure/build: PASS.
- CLI version: `Vestigant Spotlight v1.6.6.5`.
- Self-test: PASS (`Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v1.6.6.5`).
- Static current-wrapper/text audit: PASS.
- Static KnowledgeC promotional predicate audit: PASS for `src/db/case_db.cpp` and `src/gui/win32_gui.cpp`.
- Windows/MSVC build: not run in this environment; required next artifact is `V1_6_6_5_build.log`.
- iOS thin: required next artifact is `Upload_Thin_iOS_CoreSpotlight_V1_6_6_5.zip`.
- AFF4/APFS thin/full: not required unless build/shared schema behavior regresses.

## V1.6.6.3 Local Validation

- Reviewed uploaded `Upload_Thin_iOS_CoreSpotlight_V1_6_6_2.zip` before source changes.
- Linux CMake configure/build: PASS.
- CLI version: `Vestigant Spotlight v1.6.6.3`.
- Self-test: PASS (`Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v1.6.6.3`).
- Windows/MSVC build: not run in this environment for V1.6.6.3; superseded by the V1.6.6.5 build-wrapper hotfix.
- iOS thin: still required under V1.6.6.5 after the Windows build passes.
- AFF4/APFS thin/full: not required for this version unless build/shared schema behavior regresses.

## V1.6.6.2 Local Validation

- Linux CMake configure/build: PASS.
- CLI version: `Vestigant Spotlight v1.6.6.2`.
- Self-test: PASS, including `runKnowledgeCIdentitySuppressionSmokeTest`.
- Windows/MSVC build: not run in this environment; required next artifact is `V1_6_6_2_build.log`.
- iOS thin: required next artifact is `Upload_Thin_iOS_CoreSpotlight_V1_6_6_2.zip`.
- AFF4/APFS thin/full: not required for this version unless shared schema/build behavior regresses.

## V1.3.2 local validation

- Linux CMake configure: PASS.
- Linux build: PASS for CLI/tests.
- CLI version: `Vestigant Spotlight v1.3.2`.
- Self-test: PASS.
- Windows/MSVC build: required.
- AFF4/APFS thin: required.
- iOS thin: required if validating new communication-frequency/KnowledgeC outputs.

## V1.3.2 update

- Scope: documentation/package hygiene release.
- Consolidated standalone development notes into `docs/CONSOLIDATED_DEVELOPMENT_NOTES.md`.
- Consolidated standalone validation logs/notes into `validation/CONSOLIDATED_VALIDATION_LOGS_AND_NOTES.md`.
- Removed the now-consolidated standalone note/log files from the active package.
- Added `docs/SUPPORT_DIAGNOSTIC_TOOLS_REGISTER.md` to track retained support/diagnostic tools and their retention rationale.
- No support/diagnostic tools were deleted in this version because each remains tied to active AFF4/APFS validation, iOS support, general packaging/staging, or on-demand troubleshooting.
- No AFF4/APFS extraction, iOS parsing, GUI behavior, Store-V2 parser behavior, or SQLite schema behavior was intentionally changed.

- Reviewed uploaded `V1_1_10_1_build.log`: Windows/MSVC build completed successfully, CLI/tests/GUI linked, and `Vestigant Spotlight v1.1.10.1` was reported.
- Reviewed uploaded `Upload_Thin_MacOS_AFF4_V1_1_10_1.zip`: AFF4/APFS run completed source-probe workflow; staged Store-V2 parse/enrichment produced 25,000 artifacts.
- External compare summary remained stable against the prior V1.1.9/V1.1.10.1 class: 4,123 external files, 8,986 Vestigant staged files, 2,213 file matches, 1,424 external-only rows, 6,710 Vestigant-only rows, and 486 relative-path size mismatches.
- Remaining mismatch diagnostics stayed at 486 rows: 4 `DATA_FORK_SIZE_DISAGREES_WITH_EXTERNAL` and 482 `NO_EXACT_COPYOUT_CANDIDATE`.

## TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build.
- iOS: not required.
- Reason: V1.3.2 changes Win32 GUI review-grid rendering behavior and documentation only. The V1.1.11 build and AFF4/APFS thin output were reviewed before this version; no extraction/traversal/copy-out/decompression/parser code changed.
- Trigger for escalating AFF4/APFS to full test: any next change to live APFS traversal, copy-out, decompression, extent handling, path reconstruction, external compare logic, or Store-V2 staging behavior.
- Trigger for iOS testing: any next change to iOS ZIP staging, CoreSpotlight parsing, FFS lookup, app DB parsing, bplist/NSKeyedArchiver handling, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_3_3_build.log` and `Upload_Thin_MacOS_AFF4_V1_3_3.zip`.

## Standard V1.3.2 build command block

```powershell
Set-Location D:\Downloads

Get-FileHash .\VestigantSpotlightInv_V1_3_3.zip -Algorithm SHA256

Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_3_3" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_3_3.zip -DestinationPath T:\ -Force

powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_3_3\scripts\Build-V1_3_3.ps1
```

## Standard V1.3.2 AFF4/APFS thin command

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_3_3\scripts\Run-V1_3_3-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```


# V1.0.27 Process and GUI SQLite Hardening

V1.0.27 is a narrow hardening release after V1.0.26.1 built successfully and the macOS AFF4/APFS thin ZIP was generated and reviewed.

## Evidence reviewed before patching

- `V1_0_26_1_build.log` showed the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26.1`.
- `Upload_Thin_MacOS_AFF4_V1_0_26_1.zip` was present and reviewed.
- The V1.0.26.1 thin ZIP did not contain the denied raw log/inventory names:
  - `aff4_stream_inventory_raw.txt`
  - `ios_focused_zip_extract.log`
  - `ios_focused_zip_extract_7z.log`
  - `ios_focused_zip_extract.ps1`
  - `ios_ffs_file_inventory.csv`
  - `image_file_inventory.csv`
- The V1.0.26.1 case/additional inventories used relative paths rather than full `Q:\`, `D:\`, or `T:\` paths.
- The AFF4/APFS run reached `complete_source_probe`.
- APFS staged Store-V2 parse baseline remained: `raw_records=25000`, `raw_key_values=2181`, `raw_date_candidates=25000`.
- External comparison summary remained: `external_file_count=4123`, `vestigant_file_count=8986`, `file_match_rows=2213`, `external_only_rows=1424`, `vestigant_only_rows=6710`, `hash_different_path_rows=431`, and `RELATIVE_PATH_SIZE_MISMATCH=486`.

## Implemented changes

1. Added Windows Job Object wrapping to hidden external process launches in `app_runner.cpp`.
   - `runShellCommandNoWindow()` now creates a kill-on-close Job Object when available.
   - redirected process execution also uses the same Job Object helper.
   - on timeout, `TerminateJobObject()` is used when a job was assigned; otherwise the parent process is terminated as fallback.
   - this is intended to prevent orphaned child processes from locking evidence files.

2. Added resilient SQLite busy retry handling for GUI review database connections.
   - `win32_gui.cpp` now installs a bounded custom `sqlite3_busy_handler` for `ReadOnlyDb` connections.
   - `gui_export_worker.cpp` now installs a matching portable busy handler for export-worker database connections.
   - temp-store/cache PRAGMAs remain unchanged.

3. Added a thin-upload ZIP deny-list self-check to `tools/Create-SourceProbeUploadZip.ps1`.
   - after `Compress-Archive`, the script opens the generated ZIP and fails if any denied raw filenames are present.

4. Updated continuity files:
   - `docs/CONTINUATION_HANDOFF.md`
   - `docs/ROADMAP_CHECKLIST.md`
   - `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`

## Intentionally deferred

- APFS absolute-path reverse catalog walker: deferred because the proposed implementation is pseudocode and would require validated APFS B-tree lookup/value parsing before being forensically safe.
- Evidence intake/staging extraction module: deferred because moving ZIP staging and iOS inventory import is broad and should not be combined with process/GUI hardening.
- iOS NSKeyedArchiver unflattener: deferred until a real bplist object model/UID parser is present; returning placeholder JSON would create misleading investigative output.
- APFS diagnostic writer relocation: still a good next target, but not combined with V1.0.27.

## Validation performed locally

- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp`
- `g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp`
- `g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp`

Windows/MSVC build and Windows GUI runtime validation are still required for V1.0.27.

---

# V1.0.27

V1.0.27 is a thin-upload packaging hotfix after the V1.0.26 AFF4/APFS run and external comparison completed but the thin ZIP failed during PowerShell relative-path inventory generation.

## Changed

- Fixed `tools/Create-SourceProbeUploadZip.ps1` so `Get-RelativePathForThinInventory` no longer uses `[char]'\\'`, which Windows PowerShell treats as a two-character string and rejects.
- Reused the robust relative-path helper for `ExtractedSpotlight` copy paths.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- Added `scripts/Package-V1_0_27-macOS-AFF4-ThinFromExistingCase.ps1` for packaging an already-completed V1.0.26 AFF4/APFS case without rerunning the probe.
- Added `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, and `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.

## Not changed

- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.

## Validation

- Reviewed the uploaded V1.0.26 build log; the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.
- Reviewed the user-reported wrapper output showing the AFF4/APFS probe and external comparison completed before packaging failed.
- Local syntax/text checks were performed for modified C++ and PowerShell packaging files. Windows/MSVC V1.0.27 validation remains required.

## V1.0.26

- Thin-upload redaction and hidden-process/large-offset I/O hardening added. Windows/MSVC validation pending.

V1.0.18 validation summary

Inputs reviewed:
- V1_0_17_build.log
- Upload_Thin_MacOS_AFF4_V1_0_17.zip
- V1.0.17 source package

V1.0.17 observed build status:
- MSVC build completed successfully.
- Apple/lzfse source was detected and VESTIGANT_HAS_LZFSE was enabled.
- Third-party Apple/lzfse decoder sources compiled and linked.

V1.0.17 observed AFF4/APFS metrics:
- Copy-out rows: 9,902
- Copied files: 9,235
- Normalized staged files: 8,986
- Staged bytes: 1,368,577,744
- Valid parsed Store-V2 databases: 2
- Parsed raw records: 25,000
- Apple/lzfse codec status: APPLE_LZFSE_REFERENCE_CODEC_ENABLED
- decmpfs/resource-fork rows in the current test run: 0

V1.0.18 changes validated in this environment:
- app_runner.cpp C++20 syntax check with VESTIGANT_HAS_LZFSE=1: PASS
- ios_app_db_parser.cpp C++20 syntax check: PASS
- lzfse_codec.cpp C++20 syntax check: PASS
- APFS parser module syntax checks: PASS
- GUI view registry syntax check: PASS
- tests/main.cpp syntax check: PASS
- CMake configure: PASS
- Linux build progressed through the vendored Apple/lzfse sources and existing modules, then timed out during the very large app_runner.cpp compile; no compile error was observed before timeout.

Not verified here:
- Windows/MSVC V1.0.18 build
- Win32 GUI runtime
- Live AFF4/APFS V1.0.18 run


## V1.0.25

- Added shared GUI view/export helper module (`src/gui/gui_view_helpers.h/.cpp`) to remove duplicated SQL/view helper logic between the Win32 GUI and `GuiExportWorker`.
- No APFS traversal, Store-V2 parsing, iOS parsing, schema, or GUI view behavior was intentionally changed.
- Windows/MSVC validation is pending.

## V1.0.28.1 local validation

- `src/parsers/apfs_diagnostic_exporter.cpp` C++20 syntax check passed.
- `src/app/app_runner.cpp` C++20 syntax check passed.
- `src/gui/gui_export_worker.cpp` C++20 syntax check passed.
- `src/core/app_info.cpp` C++20 syntax check passed.
- Linux CMake configure passed; Linux build timed out after compiling the moved APFS diagnostic exporter and reaching `app_runner.cpp`, with no compile error observed before timeout.
## V1.0.28.2 local validation

- Syntax checked `src/parsers/apfs_diagnostic_exporter.cpp`, `src/app/app_runner.cpp`, and `src/core/app_info.cpp`.
- Compiled `src/parsers/apfs_diagnostic_exporter.cpp` to a local object and confirmed `isLikelyStoreV2GroupDirectoryName` is a local anonymous-namespace symbol, not a public duplicate.
- Windows/MSVC build and AFF4/APFS thin output remain pending.

