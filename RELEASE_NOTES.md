# Vestigant Spotlight Release Notes

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

# Vestigant Spotlight Release Notes

Current version: 0.9.60

## V0_9_60 - Windows MSVC batch-label build hotfix

V0_9_60 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55 - V1 GUI polish, custom view sets, processing telemetry, and tag-management repair

V0_9_55 is a GUI-focused V1 readiness release built on the stable V0_9_54 parser/intake baseline. It does not intentionally change parser, cache, ZIP staging, FFS inventory, or app database classification behavior.

Changes:

- Added Vestigant branding to the Case Information workflow using the supplied green Vestigant logo.
- Cleaned the Case Information / Build Processing layout to reduce visual clutter and make the workflow more investigator-focused.
- Reused the bottom Case Information log area as the live processing log/status pane.
- Added processing telemetry messages for elapsed processing time and estimated throughput when a measurable file source size is available.
- Added live elapsed-time status updates while processing runs.
- Added Custom view set support for both MacOS and iOS investigation tabs.
- Added view-set controls: Move Up, Move Down, Hide, Save Set, and Reset Set.
- Custom view sets are stored in the case database through `review_view_preferences`.
- Added a visible Tags button on the investigation toolbar to switch directly to the Tags / Notes workflow.
- Added existing-case GUI schema repair for tag tables, checked-row persistence, notes, and view preferences. This is intended to fix older cases where Manage Tags / tag actions were unavailable because the GUI interaction tables did not yet exist.
- Retained the V0_9_54 V1 production cleanup: Skip preservation, Self-test, Max records, Blocks/store, and legacy V7 GUI/CLI paths remain removed.
- Retained the two-column selected-row details table and investigation-tab-only visibility behavior.

Validation summary:

- Linux CMake configure/build passed for core library, CLI, and tests.
- `VestigantSpotlightTests` self-test passed.
- Static balance check passed for `src/gui/win32_gui.cpp`.
- Confirmed removed V1-blocker GUI controls and V7 importer references remain absent.
- Windows/MSVC GUI build and runtime GUI validation are still required.

## V0_9_60 - Staged AFF4/raw source restoration and MB telemetry

V0_9_60 restores AFF4/APFS image and Raw IMG/DD image choices in the GUI source-type selector as staged image workflows. Folder and ZIP remain the production intake paths; AFF4/APFS is kept visible for macOS forensic images, and Raw IMG/DD is kept visible for raw/external media such as exFAT devices attached to Macs that may contain Spotlight indexes. Processing telemetry now displays throughput in decimal MB and MB/s rather than MiB/MiB/s for investigator readability. No parser interpretation logic was intentionally changed.
