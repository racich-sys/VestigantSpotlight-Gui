## V1.1.10 update

- Current generated source package: V1.1.10.
- Base used for changes: V1.1.9.1.
- Scope: source-package documentation/script cleanup and current-version wrapper regeneration only.
- Removed only clearly obsolete active-package clutter; ambiguous historical notes/scripts were retained for user approval before any future removal.
- Source-package `.md`, `.txt`, and `.ps1` review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.md`.
- No AFF4/APFS extraction, iOS parsing, GUI behavior, or SQLite schema behavior was intentionally changed.

# Spotlight2 / Vestigant Spotlight Continuation Handoff

## V1.1.10 update

- Current generated source package: V1.1.10.
- Validated baseline reviewed before this version: V1.1.8 Windows/MSVC build and macOS AFF4/APFS thin output.
- Main change: guarded live APFS OMAP horizontal leaf traversal with bounded next-leaf transitions.
- Source-package `.md`, `.txt`, and `.ps1` file review completed; see `docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.md`.


## Current packaged version

- Current source package: `VestigantSpotlightInv_V1_1_7_1.zip`
- Version string: `1.1.7.1`
- Most recently validated stable baseline: `V1.1.6.1` Windows/MSVC build + macOS AFF4/APFS thin output.
- Latest generated version before this package: `V1.1.7`; Windows/MSVC failed because `aff4_probe_worker.cpp` needed helpers left in `app_runner.cpp`.
- V1.1.7.1 fixes that build boundary and cleans source package layout.

## First file to read in a new chat

Read `docs/NEW_CHAT_CONTINUATION_GUIDE.md`, then this handoff.

## User shorthand

If a future request begins with `repeat`, review all uploaded/copied information, identify the newest source/build/thin evidence, continue to the next version, implement as many safe outstanding roadmap/suggestion items as possible, update the handoff/roadmap/suggestions tracker/workflow ledger, package artifacts, and provide concrete PowerShell commands.

## Current V1.1.7.1 scope

Implemented:

- Worker-local helper boundary for moved AFF4 dynamic probe code:
  - `shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(...)`
  - `findToolCandidate(...)`
  - `lastWindowsErrorString(...)`
- Source package cleanup:
  - removed obsolete versioned scripts from `scripts/`;
  - removed old root-level package manifests;
  - preserved append-only version history under `docs/`.
- Added/updated:
  - `docs/NEW_CHAT_CONTINUATION_GUIDE.md`
  - `docs/SOURCE_PACKAGE_CLEANUP_POLICY.md`
  - `docs/FULL_VERSION_HISTORY.md`
  - `docs/VERSION_HISTORY_APPEND_ONLY_POLICY.md`

## Next validation needed

1. Build V1.1.7.1 on Windows/MSVC.
2. Upload `V1_1_7_1_build.log`.
3. Run macOS AFF4/APFS thin regression.
4. Upload `Upload_Thin_MacOS_AFF4_V1_1_7_1.zip`.

---

# Spotlight2 / Vestigant Spotlight Continuation Handoff

## Current packaged version

- Current source package: `VestigantSpotlightInv_V1_1_2.zip`
- Version string: `1.1.2`
- Base version reviewed before this package: `V1.1.2`
- V1.1.0.1 Windows/MSVC build log was uploaded and reviewed. It compiled and linked CLI, tests, and GUI, reported `Vestigant Spotlight v1.1.0.1`, and only showed the known C4100 warning for `shellRunner` in `apfs_aff4_reader.cpp`.
- V1.1.0.1 macOS AFF4/APFS thin ZIP was uploaded and reviewed. Denied raw filenames were absent and the AFF4/APFS staged Store-V2 baseline remained stable.

## User shorthand

If a future request begins with `repeat`, treat it as: review all uploaded/copied information, identify the newest source/build/thin evidence, continue to the next version in the same project style, implement as many safe outstanding roadmap/suggestion items as possible, update the handoff/roadmap/suggestions tracker, package artifacts, and provide concrete PowerShell commands. The expected cadence for `repeat` is broader than a one-line hotfix when the current version builds: spend more time, make a larger coordinated but reviewable step, and run repeated checks before packaging.

## V1.1.2 scope

V1.1.2 is a broader repeat-cycle release focused on evidence-intake isolation and GUI ingest-thread safety. It preserves extraction semantics while moving more iOS inventory import logic out of `app_runner.cpp`.

Implemented:

- Moved `importIosInventoryCsvs(...)` from `app_runner.cpp` into `EvidenceIntake::importIosInventoryCsvs(...)`.
- Moved cache-SQLite iOS FFS/app-database inventory import helpers and referenced-path lookup import into `src/ingest/evidence_intake.cpp`.
- Added `EvidenceIntake::importReferencedIosPathLookupFromReuseCache(...)` and updated `app_runner.cpp` to call it through the intake module.
- Preserved run-status reporting through callback injection so `app_runner.cpp` keeps orchestration/progress ownership while intake owns import mechanics.
- Changed Win32 GUI main ingest/build worker from `std::thread(worker).detach()` to a tracked `gIngestThread` with guarded start and `WM_DESTROY` join.
- Cleared the V1.1.0.1 AFF4 stream inventory C4100 warning using platform-aware `(void)` annotations while preserving the callback signature.
- Updated roadmap/checklist/suggestions tracker and documented this version.

Not changed:

- APFS live traversal replacement.
- AFF4 read semantics.
- APFS copy-out/staging decisions.
- Store-V2 parser semantics.
- SQLite schema.
- GUI platform separation.
- NSKeyedArchiver emitted interpretation.
- Full GUI global-state encapsulation.
- Full `stageZipEvidenceSource(...)` relocation.


## V1.1.2 workflow-ledger note

V1.1.2 adds `docs/WORKFLOW_LEDGER.md`. Future `repeat` cycles should read that ledger before editing so prior fixes, failed attempts, packaging pitfalls, and validation gates are not rediscovered each time.

V1.1.2 implements safe cancellation plumbing, dependent DLL search hardening, GUI logo bitmap cleanup, native parser bulk PRAGMAs, and bounded bplist trailer validation metadata.

## Standard paths

- Source ZIPs: `D:\Downloads`
- Extract source under: `T:\`
- Current source extraction: `T:\VestigantSpotlightInv_V1_1_2`
- Case outputs: `Q:\SpotlightCase`
- macOS AFF4/APFS source: `O:9_0142-IT001\disk3 2024-10-01 10-43-409_0142-IT001.aff4`
- External Store-V2 reference: `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`

## Commands

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V1_1_2.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V1_1_2" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V1_1_2.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_2\scripts\Build-V1_1_2.ps1
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_1_2\scripts\Run-V1_1_2-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

## Next recommended validation

1. Windows/MSVC build for V1.1.2.
2. macOS AFF4/APFS thin run for V1.1.2.
3. Current iOS run because V1.1.2 moved the iOS inventory import boundary into `EvidenceIntake`.

## Next recommended work after V1.1.2 validates

1. Compare V1.1.2 AFF4/APFS thin output to V1.1.0.1 baseline.
2. Continue reducing `writeAff4CppLiteDynamicLoadProbe` only through smaller callable helpers or a dedicated worker with parity output.
3. Move `stageZipEvidenceSource(...)` only after the V1.1.2 intake-import relocation validates.
4. Add passive APFS path-resolution comparator CSVs before using path reconstruction in live staging.
5. Build a real bounded bplist object model before emitting NSKeyedArchiver unflattened investigator fields.
6. Defer full Win32 GUI global-state encapsulation until current worker paths have been validated.

## V1.1.3 handoff update

- Baseline reviewed: V1.1.2 Windows/MSVC build log and macOS AFF4/APFS thin ZIP.
- Implemented: GUI export cancellation callbacks, orphan-source purge transaction, secure RichEdit load/cleanup, non-live APFS next-leaf helper strengthening, workflow ledger update.
- Not changed: live AFF4/APFS extraction, Store-V2 parser semantics, iOS NSKeyedArchiver interpretation, SQLite schema.
- Next validation required: Windows/MSVC V1.1.3 build and macOS AFF4/APFS thin run.

## V1.1.4 handoff update

Baseline reviewed: V1.1.3 Windows/MSVC build and macOS AFF4/APFS thin ZIP. V1.1.4 is a repeat-cycle hardening package. It does not change live APFS extraction. It adds bplist offset-table/top-object-offset metadata, safer GUI checked-artifact snapshots, and a stricter atomic ingest launch gate. The next large target remains `writeAff4CppLiteDynamicLoadProbe` extraction to an `aff4_probe_worker` boundary, but this should be done as a dedicated version because it is the highest-risk remaining refactor.


## V1.1.5 handoff update

Baseline reviewed: V1.1.4 Windows/MSVC build and macOS AFF4/APFS thin ZIP. V1.1.5 is a repeat-cycle hardening package. It propagates cancellation into AFF4 dynamic/direct probes, adds case-directory writability preflight, enforces thin-upload size policy for upload samples, writes 7-Zip extraction logs as UTF-8, and catches APFS staged diagnostic sample export failures locally. Live APFS extraction, Store-V2 parsing, iOS parser semantics, and schema are unchanged.

Next high-risk target remains extracting the dynamic AFF4/APFS probe monolith into an `aff4_probe_worker` boundary. Do this only as a dedicated version with repeated build validation.

## V1.1.5.1 handoff update

V1.1.5 failed MSVC compilation because a cancellation check returned `false` inside an `ApfsOmapTargetResolution`-returning lambda. V1.1.5.1 fixes only that typed return. Next step: build V1.1.5.1 on Windows/MSVC and run the macOS AFF4/APFS thin wrapper.

## V1.1.6 handoff update

V1.1.6 is a partial but physical Tracker #17 modularization. It moves the direct-map AFF4/APFS probe into `src/parsers/aff4_probe_worker.cpp` and updates build files. The libaff4 dynamic-load probe remains in `app_runner.cpp`; see `docs/WORKFLOW_LEDGER.md` for the dependency finding and next split plan.


## V1.1.6.1 build-hotfix note

V1.1.6 moved the direct-map AFF4/APFS probe into `src/parsers/aff4_probe_worker.cpp`, but the MSVC build exposed a Windows-only missing helper: `wideProcessPath`. V1.1.6.1 adds a local Windows helper in the worker and corrects the versioned build script gate. This is recorded as a repeat-process pitfall: after moving code from `app_runner.cpp`, grep for Windows-only helper dependencies that Linux syntax checks cannot see.


## V1.1.7 update

- Baseline: validated V1.1.6.1.
- Completed Tracker #17 major step: moved `writeAff4CppLiteDynamicLoadProbe(...)` from `app_runner.cpp` into `Aff4ProbeWorker::executeDynamicLoadProbe(...)` in `src/parsers/aff4_probe_worker.cpp`.
- `writeAff4DirectMapReaderProbe(...)` had already been moved in V1.1.6; both large AFF4/APFS probe bodies now live in `aff4_probe_worker.cpp`.
- Added cancellation checks into the shared APFS OMAP traversal helper so the direct-map and dynamic-load paths can stop during B-tree traversal.
- Remaining related work: compare V1.1.7 Windows build and thin output against V1.1.6.1; then clean duplicated/unused helper functions and continue moving staged probe workers only after parity is confirmed.

## V1.1.8 Update

- `BaselineVersionHistory.md` is now the append-only version-history baseline in `docs/FULL_VERSION_HISTORY.md` and `VERSION_HISTORY.md`.
- Windows long-path evidence writes were added for APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction output paths.
- SQLite WAL checkpoint/truncate is requested before upload packaging.
- Logger writes are mutex-protected for concurrent GUI/export/ingest paths.
- APFS decmpfs reconstruction remains bounded; the expected-output safety cap is now 256 MiB.



## V1.1.10 Update

- Reviewed uploaded V1.1.9 source ZIP, Windows/MSVC build log, macOS AFF4/APFS thin output, and user-provided Windows/x64 risk note before changes.
- Confirmed V1.1.9 compiled/linked and the binary reported `Vestigant Spotlight v1.1.9`, but the PowerShell build wrapper still checked for `1.1.8`.
- Confirmed V1.1.9 thin output completed AFF4/APFS Store-V2 staging/enrichment with stable baseline counts: `raw_record_count=25000`, `artifact_count=25000`, `staged_groups=11`, `staged_files=8986`, `copied_files=9235`.
- Confirmed V1.1.9 external compare counts matched the V1.0.27/V1.1.4 documented baseline: `external_file_count=4123`, `vestigant_file_count=8986`, `file_match_rows=2213`, `external_only_rows=1424`, `vestigant_only_rows=6710`, `hash_different_path_rows=431`, `RELATIVE_PATH_SIZE_MISMATCH=486`.
- Applied a narrow hotfix only: version-wrapper guard correction and warning cleanup.
- Test decision for V1.1.10: AFF4/APFS thin test required because wrapper/version packaging changed; AFF4/APFS full test not required until the next APFS completeness change. iOS thin/full test not required because no iOS code path changed.

## V1.1.10.1 command-block documentation hotfix

- Corrected current-version build documentation to include the full extraction/build PowerShell block requested by the user.
- Corrected current-version macOS AFF4/APFS thin test documentation to include `Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut`.
- Updated `docs/NEW_CHAT_CONTINUATION_GUIDE.md` so new chats started from the newest upload include the full commands.
- No source parser/extraction behavior changed.

### TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build, because wrappers/docs changed and APFS behavior did not.
- iOS: not required, because no iOS intake/parser/schema/view code changed.
- Trigger for full AFF4/APFS test: any future change to traversal, copy-out, decompression, staging, external compare, or Store-V2 parse behavior.
