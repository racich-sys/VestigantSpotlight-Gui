
## V1.6.40.1.1 - CSV default, source-profile filtering, unresolved-label path guard

- GUI processing now defaults to `Exclude CSV exports` checked. SQLite case output remains the default review artifact unless CSV exports are explicitly enabled.
- Non-iOS ZIP profiles now record that iOS FFS/app-database parser stages were skipped.
- macOS-profile exports now skip `ios_*` CSV export calls rather than writing large groups of zero-row iOS CSVs.
- Unresolved Store-V2 review labels are no longer accepted as valid filename/path components for parent-inode path reconstruction.
- Added `docs/V1_6_40_1_CSV_DEFAULT_AND_SOURCE_PROFILE_FILTERING.md`.

# V1.6.40.1.1 current handoff note

V1.6.40.1.1 follows successful V1.6.40.1.1 build/thin validation and implements code-review hardening for APFS/AFF4/bplist/iOS app DB/GUI path handling. See `docs/V1_6_40_1_CODE_REVIEW_VALIDATION_HARDENING.md`. Missing-from-FFS and CoreDuet interpretation guardrails remain in place.

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


## V1.6.6.2 workflow ledger

- Completed prerequisite review of every `.md` and `.txt` file in the uploaded V1.6.6.1 package.
- Implemented KnowledgeC/CoreDuet identity suppression hardening and SQLite view predicate correction.
- Added self-test coverage for KnowledgeC device/generic/communication-intent separation.
- Next: Windows/MSVC build plus iOS thin upload review. AFF4/APFS thin is not required unless shared schema/build behavior regresses.

# Roadmap Checklist - V1.3.2

## Active priority order

1. AFF4/APFS progress visibility and status accuracy.
2. iOS communication/identity investigation.
3. Case tab/button stability.

## A. Stability / Architecture

- [x] Virtual ListView rendering.
- [x] Export thread lifecycle audit.
- [x] APFS guided buffer reuse.
- [x] AFF4 direct-map progress heartbeats.
- [x] GUI GB-of-GB progress estimate when source size and percent are available.
- [ ] Safe Live Ingest Preview panel.
- [ ] GUI SQLite access pooling only if measured need is confirmed.
- [ ] Full APFS BlockReader abstraction.

## B. iOS Communication & Identity Analysis

- [x] Communication-frequency SQL view and GUI route.
- [x] Communication-frequency CSV export.
- [x] Provenance markers for identity-bound communication hints.
- [x] Thread-volume provenance marker when kMDItemDomainIdentifier appears in parsed metadata.
- [x] KnowledgeC `/app/activity` and `/item/interactions` stream inclusion.
- [x] Cautious KnowledgeC communication-intent labeling.
- [ ] CoreSpotlight native parser field-level extraction for DomainIdentifier/Author/Recipient beyond existing text/context surfaces.
- [ ] NSKeyedArchiver UID graph reconstruction.
- [ ] iOS thin validation of new communication-frequency view.

## C. Case Tab Stability

- [x] Ingest-time mutation-control disabling/deferred autosave from V1.3.1.
- [x] Compact Case Information / Build Processing and Investigation Results top controls in V1.6.18.
- [ ] Review remaining Case tab buttons from Windows GUI runs after V1.6.18 visual validation.
- [ ] Improve running status display from APFS/AFF4 worker heartbeats.

## D. APFS Metadata Completeness

- [ ] WhereFroms extraction.
- [ ] Quarantine metadata extraction.
- [ ] Volume metadata extraction.
- [ ] Deleted/tombstone artifact surfacing with provenance-only language.

## E. Unified APFS Reader

- [ ] Shared B-tree leaf traversal helpers.
- [ ] Shared BlockReader abstraction.
- [ ] Direct-map and libaff4 APFS paths converged onto shared traversal math.

## V1.6.3.1 Update
- Implemented cautious WhereFroms XATTR surfacing, bounded bplist/NSKeyedArchiver graph-sample output, safe iOS provenance markers, and tombstone/deleted review routing keywords.
- Preserved non-interpretive wording: no automatic exfiltration or destruction conclusions were added.
- Local validation: Linux CMake build PASS; CLI version reports v1.6.3.1; self-test PASS.
- Next required upload: V1.6.3.1 Windows build log and iOS thin output.


## V1.6.3.1 requested-fixes verification

- Verified and retained the requested GUI database pool deadlock fix, APFS guided traversal cycle detection, iOS bplist string extraction, Notes/Location routing, and widened table-column catchers.
- Added `tools/Verify-V1_6_3-RequestedFixes.ps1` for repeatable source-presence validation.
- Standard iOS thin should be run; AFF4/APFS thin is not required unless APFS cycle-guard behavior is specifically tested.

## V1.6.3.1 ai_context.md bootstrap

- Added root `ai_context.md` as the living project context file.
- Future source packages must carry forward and update `ai_context.md` with current state, roadmap, known bugs, and graveyard items.
- No parser, extraction, GUI, SQLite schema, or forensic interpretation behavior intentionally changed by this documentation-only package.

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
