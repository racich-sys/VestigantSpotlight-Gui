
## V1.6.40.1.1 - CSV default, source-profile filtering, unresolved-label path guard

- GUI processing now defaults to `Exclude CSV exports` checked. SQLite case output remains the default review artifact unless CSV exports are explicitly enabled.
- Non-iOS ZIP profiles now record that iOS FFS/app-database parser stages were skipped.
- macOS-profile exports now skip `ios_*` CSV export calls rather than writing large groups of zero-row iOS CSVs.
- Unresolved Store-V2 review labels are no longer accepted as valid filename/path components for parent-inode path reconstruction.
- Added `docs/V1_6_40_1_CSV_DEFAULT_AND_SOURCE_PROFILE_FILTERING.md`.


## V1.6.40.1.1 macOS unresolved Store-V2 object labels

- Added explicit unresolved object labels for macOS Store-V2 records that still lack structured names after dictionary/path-probe enrichment.
- Labels are forensic review handles, not asserted filenames.
- Added parser metric `unresolved_identifier_label_artifacts`.


## V1.6.40.1.1 Late Review Addendum

- Added late-review fixes for iOS CoreSpotlight bundle attribution, bracketed timestamp-array normalization, per-path GUI read-only DB pooling, and bplist JSON stringification caps.
- Confirmed null-byte-safe CSV export was already present before this addendum.

# V1.6.40.1.1 Suggestions / Fixes Tracker Update

- [x] Remove premature SQL weak-path filter from native path probe promotion.
- [x] Add bounded basename candidate promotion for placeholder artifact names.
- [ ] Validate V1.6.40.1.1 MSVC build.
- [ ] Rerun macOS zipped Spotlight thin test and compare metrics/placeholders.

# V1.6.40.1.1 Suggestions / Fixes Tracker Update

- [x] Review uploaded macOS Store-V2 sidecar files.
- [x] Enable external dbStr map loading for macOS Store-V2 based on component presence.
- [ ] Validate MSVC build.
- [ ] Rerun macOS zipped Spotlight thin test and compare dictionary counts / GUI placeholder rate.

# V1.6.40.1.1 Suggestions / Fixes Tracker Update

- [x] Promote macOS Store-V2 native path probe values into artifact display/path fields where GUI rows had placeholder names.
- [ ] Validate V1.6.40.1.1 with MSVC build log.
- [ ] Rerun macOS zipped Spotlight thin test and compare GUI `------NONAME------` rate.

# V1_6_40_1 note

V1_6_40_1 skips the parent-inode path apply UPDATE when `new_reconstructed_paths=0`, based on the V1.6.32 macOS zipped Spotlight thin result. Build success remains unverified until the Windows log is uploaded.

# V1_6_40_1 note

V1_6_40_1 fixes recurring build-blocking release-readiness failures: release-readiness is advisory from the build wrapper, while wrapper compatibility and raw-string risk remain fatal. Expected CLI version is read dynamically from `VERSION`.

# V1.6.40.1.1 code-review issue tracker

- [x] APFS OMAP vertical B-tree cycle detection.
- [x] AFF4 LZ4 subtraction-based overflow-safe bounds checks.
- [x] Bplist dictionary/array bounds checks.
- [x] Bplist expansion cap and repeated complex UID guard.
- [x] UTF-16LE bplist fallback string extraction.
- [x] Generic iOS app database pagination instead of single 50,000-row page.
- [x] GUI schema-open churn reduced by one-time-per-path schema ensure.
- [x] `vw_ios_spotlight_comms_missing_from_ffs` confirmed present.
- [x] Folder picker now warns on unresolved or MAX_PATH-boundary paths.
- [ ] AFF4/APFS image-backed active filesystem comparison remains pending.

# V1.6.40.1.1 Release Notes

## Purpose

V1.6.40.1.1 records active filesystem comparison as the next implementation target and replaces stale `v0.6.4` log/CLI wording with current V1.6.40.1.1 limitation language.

## Triggering evidence from V1.6.22.1 thin

- `run_status.txt` ended in `complete_success`.
- `VestigantSpotlight_tail250.log` reported active filesystem comparison was tabled and that `existence_status` would remain `NOT_CHECKED`-style.
- `active_file_comparison_readiness.csv` reported `comparison_ready=0` and `comparison_status=ZIP_PARSED_FOR_SPOTLIGHT_NOT_IMAGE_FILE_INVENTORY`.

## Changed in V1.6.40.1.1

- Added `docs/ACTIVE_FILESYSTEM_COMPARISON_ROADMAP.md`.
- Updated continuation docs to make active filesystem comparison the next queued implementation target.
- Replaced stale `v0.6.4` active-comparison log strings in current source files.
- Kept V1.6.22.1 thin diagnostic-wrapper behavior.
- Kept V1.6.22 interactionC precision behavior.

## Not implemented yet

V1.6.40.1.1 implements Phase 1 active filesystem comparison for iOS FFS exact-path lookup. `MISSING_FROM_IOS_FFS_EXACT_PATH_CANDIDATE` rows are investigative leads only, not deletion proof. AFF4/APFS image-inventory joins remain pending.


## V1.6.18 implemented / queued validation

- [x] Reviewed uploaded V1.6.17 iOS thin and validated the targeted email-category false-positive reduction.
- [x] Compact Windows GUI Case Information / Build Processing top section in source.
- [x] Compact Windows GUI Investigation Results action bar in source.
- [x] Update stale current continuation and quick-start docs from V1.6.12/V1.6.7.1 to V1.6.18.
- [ ] Windows/MSVC build validation for V1.6.18.
- [ ] Live Windows GUI visual validation at common DPI settings.
- [ ] Live V1.6.18 iOS thin validation after build passes.

## V1.6.6.2 workflow ledger

- Completed prerequisite review of every `.md` and `.txt` file in the uploaded V1.6.6.1 package.
- Implemented KnowledgeC/CoreDuet identity suppression hardening and SQLite view predicate correction.
- Added self-test coverage for KnowledgeC device/generic/communication-intent separation.
- Next: Windows/MSVC build plus iOS thin upload review. AFF4/APFS thin is not required unless shared schema/build behavior regresses.

## V1.3.3 - iOS thin lightweight sample exports and cautious provenance

- Implemented lightweight base-table sample SQL for previously heavy iOS thin sample exports.
- Full investigator views remain available for explicit full/support diagnostics.
- Added cautious provenance markers for device-owner contact, Trash path components, and LSQuarantine string references.
- Deferred NSKeyedArchiver graph decoding, APFS tombstone GUI surfacing, and WhereFroms decoding to later milestone releases.

# Suggestions and Fixes Tracker - V1.3.2

## Completed in V1.3.2

- AFF4 direct-map reader emits progress heartbeats during long map/chunk scanning stages.
- GUI status heartbeat includes percent-derived `GB of GB processed` estimate when source size and progress percent are available.
- iOS communication frequency view added.
- KnowledgeC communication-intent stream coverage expanded cautiously.
- Communication/identity provenance markers added to parsed iOS app/KnowledgeC records when source text supports them.
- Browser download table categorization added as `WEB_DOWNLOADS` without exfiltration conclusions.

## Highest-priority iOS Spotlight investigation items

- CoreSpotlight communication keys: DomainIdentifier, author, recipient, phone/email hints.
- Frequency/volume grouping by thread/contact identifiers.
- KnowledgeC interaction/intent parsing for share/message activity.
- Deleted/expired Spotlight communication surfacing.
- NSKeyedArchiver graph reconstruction.

## Deferred / Not Yet Implemented

- Full NSKeyedArchiver unflattener.
- WhereFroms/quarantine APFS metadata extraction.
- Full APFS parser unification.
- Safe Live Ingest Preview panel.
- GUI SQLite connection pooling, pending measured need.

## V1.3.2.3 - iOS thin profile and export responsiveness

Completed:
- Default iOS thin runs no longer request diagnostic/support full-case exports.
- Bounded samples are generated for selected heavy iOS CSV surfaces.
- `-FullDiagnostics` is available for support runs that intentionally need full heavy exports.

Remaining:
- Add early/partial JSON summary generation during long runs.
- Add more granular cancellation handling inside the most expensive SQLite view exports.
- Add iOS keychain plist intake after V1.3.2.x thin stability is validated.

## V1.6.3.1 Update
- Implemented cautious WhereFroms XATTR surfacing, bounded bplist/NSKeyedArchiver graph-sample output, safe iOS provenance markers, and tombstone/deleted review routing keywords.
- Preserved non-interpretive wording: no automatic exfiltration or destruction conclusions were added.
- Local validation: Linux CMake build PASS; CLI version reports v1.6.3.1; self-test PASS.
- Next required upload: V1.6.3.1 Windows build log and iOS thin output.

## V1.6.3.1

- Fixed iOS thin materialization regression. Diagnostics mode plus minimal export should not force full FFS inventory insertion.
- Remaining suggestions from prior tracker remain active unless already marked complete.


## V1.6.3.1 requested-fixes verification

- Verified and retained the requested GUI database pool deadlock fix, APFS guided traversal cycle detection, iOS bplist string extraction, Notes/Location routing, and widened table-column catchers.
- Added `tools/Verify-V1_6_3-RequestedFixes.ps1` for repeatable source-presence validation.
- Standard iOS thin should be run; AFF4/APFS thin is not required unless APFS cycle-guard behavior is specifically tested.

## V1.6.3.1 ai_context.md bootstrap

- Added root `ai_context.md` as the living project context file.
- Future source packages must carry forward and update `ai_context.md` with current state, roadmap, known bugs, and graveyard items.
- No parser, extraction, GUI, SQLite schema, or forensic interpretation behavior intentionally changed by this documentation-only package.

- V1.6.3.1 corrected: release-blocking version consistency verification added after Build-V1_6_3.ps1 stale 1.4.1 check was found.

## V1.6.18.1

- [x] Restore GUI review-grid subclass callbacks needed for Windows/MSVC compile.
- [ ] Confirm Windows/MSVC build with `V1_6_18_1_build.log`.
- [ ] Visually confirm compact GUI top/review layout at scaled DPI.

## V1.6.40.1.1

- [x] Add CoreDuet interactionC database-status workflow.
- [x] Add CoreDuet interactionC summary/event GUI views and CSV exports.
- [x] Add bounded upload samples for interactionC review.
- [x] Add running start-continuation document.
- [ ] Validate Windows/MSVC build and V1.6.40.1.1 iOS thin output.
