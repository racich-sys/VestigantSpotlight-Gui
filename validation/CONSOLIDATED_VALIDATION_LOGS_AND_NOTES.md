## Current package validation: V1_1_11

# V1.1.11 Validation Notes

- Reviewed uploaded `V1_1_10_1_build.log`: Windows/MSVC build completed successfully, CLI/tests/GUI linked, and `Vestigant Spotlight v1.1.10.1` was reported.
- Reviewed uploaded `Upload_Thin_MacOS_AFF4_V1_1_10_1.zip`: AFF4/APFS run completed source-probe workflow; staged Store-V2 parse/enrichment produced 25,000 artifacts.
- External compare summary remained stable against the prior V1.1.9/V1.1.10.1 class: 4,123 external files, 8,986 Vestigant staged files, 2,213 file matches, 1,424 external-only rows, 6,710 Vestigant-only rows, and 486 relative-path size mismatches.
- Remaining mismatch diagnostics stayed at 486 rows: 4 `DATA_FORK_SIZE_DISAGREES_WITH_EXTERNAL` and 482 `NO_EXACT_COPYOUT_CANDIDATE`.

- Scope: documentation/package hygiene release.
- Consolidated standalone development notes into `docs/CONSOLIDATED_DEVELOPMENT_NOTES.md`.
- Consolidated standalone validation logs/notes into `validation/CONSOLIDATED_VALIDATION_LOGS_AND_NOTES.md`.
- Removed the now-consolidated standalone note/log files from the active package.
- Added `docs/SUPPORT_DIAGNOSTIC_TOOLS_REGISTER.md` to track retained support/diagnostic tools and their retention rationale.
- No support/diagnostic tools were deleted in this version because each remains tied to active AFF4/APFS validation, iOS support, general packaging/staging, or on-demand troubleshooting.
- No AFF4/APFS extraction, iOS parsing, GUI behavior, Store-V2 parser behavior, or SQLite schema behavior was intentionally changed.

## TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build.
- iOS: not required.
- Reason: V1.1.11 changes documentation/package hygiene only. The V1.1.10.1 build and AFF4/APFS thin output were reviewed before this version; no extraction/traversal/copy-out/decompression/parser code changed.
- Trigger for escalating AFF4/APFS to full test: any next change to live APFS traversal, copy-out, decompression, extent handling, path reconstruction, external compare logic, or Store-V2 staging behavior.
- Trigger for iOS testing: any next change to iOS ZIP staging, CoreSpotlight parsing, FFS lookup, app DB parsing, bplist/NSKeyedArchiver handling, iOS schema, or iOS GUI views.
- Required next uploaded artifacts: `V1_1_11_build.log` and `Upload_Thin_MacOS_AFF4_V1_1_11.zip`.

## Local validation performed here

- Confirmed version metadata was updated to 1.1.11.
- Confirmed current-version scripts were regenerated for V1_1_11.
- Confirmed standalone note files were consolidated and removed from active package.
- Confirmed validation logs/notes were consolidated into `validation/CONSOLIDATED_VALIDATION_LOGS_AND_NOTES.md`.
- Confirmed support/diagnostic tools were retained and tracked in `docs/SUPPORT_DIAGNOSTIC_TOOLS_REGISTER.md`.

## Not run here

- Windows/MSVC build was not run in this environment.
- AFF4/APFS thin run was not run in this environment.
- iOS run was not run because no iOS behavior changed.

---

# Consolidated Validation Logs and Notes

This file consolidates prior standalone validation notes/logs from the active package. Individual validation files were removed after consolidation; new validation notes should be appended here going forward unless a specific external log must be uploaded separately.

## Consolidation index
- `gui_export_worker_syntax.log`
- `app_runner_syntax.log`
- `app_info_syntax.log`
- `V1_1_9_validation_notes.md`
- `V1_1_9_local_validation.log`
- `V1_1_9_1_validation_notes.md`
- `V1_1_8_validation_notes.md`
- `V1_1_8_local_validation.log`
- `V1_1_7_validation_notes.md`
- `V1_1_7_local_validation.log`
- `V1_1_7_1_validation_notes.md`
- `V1_1_7_1_local_validation.log`
- `V1_1_6_validation_notes.md`
- `V1_1_6_local_validation.log`
- `V1_1_6_1_validation_notes.md`
- `V1_1_6_1_local_validation.log`
- `V1_1_5_validation_notes.md`
- `V1_1_5_local_validation.log`
- `V1_1_5_1_validation_notes.md`
- `V1_1_5_1_local_validation.log`
- `V1_1_4_validation_notes.md`
- `V1_1_4_local_validation.log`
- `V1_1_3_validation_notes.md`
- `V1_1_3_local_validation.log`
- `V1_1_2_validation_notes.md`
- `V1_1_2_local_validation.log`
- `V1_1_1_validation_notes.md`
- `V1_1_1_local_validation.log`
- `V1_1_10_validation_notes.md`
- `V1_1_10_local_validation.log`
- `V1_1_10_1_validation_notes.md`
- `V1_1_0_validation_notes.md`
- `V1_1_0_local_validation.log`
- `V1_1_0_1_validation_notes.md`
- `V1_1_0_1_selftest.log`
- `V1_1_0_1_local_validation.log`
- `V1_1_0_1_cmake_configure.log`
- `V1_1_0_1_cmake_build.log`
- `V1_1_0_1_cli_version.log`
- `V1_0_7_validation_summary.txt`
- `V1_0_6_linux_validation_summary.txt`
- `V1_0_4_syntax_check.log`
- `V1_0_31_validation_notes.md`
- `V1_0_31_local_validation.log`
- `V1_0_30_validation_notes.md`
- `V1_0_30_local_validation.log`
- `V1_0_29_validation_notes.md`
- `V1_0_29_local_validation.log`
- `V1_0_27_validation_notes.md`
- `V1_0_27_local_validation.log`
- `V1_0_27_gui_export_worker_syntax.log`
- `V1_0_27_app_runner_syntax.log`
- `V1_0_27_app_info_syntax.log`
- `V1_0_26_validation_notes.md`
- `V1_0_26_patch_manifest.txt`
- `V1_0_26_local_validation.log`
- `V1_0_26_app_runner_syntax.log`
- `V1_0_26_1_validation_notes.md`
- `V1_0_26_1_local_validation.log`
- `V1_0_25_validation_notes.md`
- `V1_0_25_patch_manifest.txt`
- `V1_0_25_local_validation.log`
- `V1_0_23_validation_notes.md`
- `V1_0_22_validation_summary.txt`
- `V1_0_21_validation_summary.txt`
- `V1_0_20_validation_summary.txt`
- `V1_0_18_validation_summary.txt`
- `V1_0_15_validation_summary.txt`
- `V1_0_12_validation_summary.txt`
- `V1_0_10_validation_summary.txt`

---

## Source: `gui_export_worker_syntax.log`

```text

```

## Source: `app_runner_syntax.log`

```text

```

## Source: `app_info_syntax.log`

```text

```

## Source: `V1_1_9_validation_notes.md`

# V1.1.9.1 Validation Notes

## Baseline reviewed

- V1.1.8 Windows/MSVC build log: build completed and binary reported Vestigant Spotlight v1.1.8.
- V1.1.8 macOS AFF4/APFS thin ZIP: generated successfully; denied raw upload filenames were absent; AFF4/APFS Store-V2 baseline counts remained stable.

## Local validation performed

- C++20 syntax check: `src/parsers/aff4_probe_worker.cpp`.
- C++20 syntax check: `src/app/app_runner.cpp`.
- C++20 syntax check: `src/parsers/apfs_volume_reader.cpp`.
- C++20 syntax check: `src/gui/gui_export_worker.cpp`.
- C++20 syntax check: `src/core/app_info.cpp`.
- Linux CMake configure/build.
- CLI version check returned `Vestigant Spotlight v1.1.9.1.1`.
- Local self-test passed.

## Required external validation

- Windows/MSVC V1.1.9.1 build.
- V1.1.9.1 macOS AFF4/APFS thin run.
- Compare external-reference counts and mismatch diagnostics against V1.1.8.
- Review whether next-leaf traversal changes staged Store-V2 file counts or mismatch classes.

## Source: `V1_1_9_local_validation.log`

```text
V1.1.9 local validation summary
- Syntax checks: PASS before documentation finalization.
- Linux CMake configure/build: PASS before documentation finalization.
- CLI version: Vestigant Spotlight v1.1.9.
- Local self-test: PASS.
```

## Source: `V1_1_9_1_validation_notes.md`

# V1.1.9.1 Validation Notes

## Uploaded V1.1.9 artifacts reviewed

- Source ZIP: `VestigantSpotlightInv_V1_1_9.zip`.
- Windows/MSVC build log: compiled and linked CLI, self-test binary, and GUI; binary version output reported `Vestigant Spotlight v1.1.9`.
- Thin macOS AFF4/APFS output: run completed source-probe workflow and staged/enriched Store-V2 artifacts.

## V1.1.9 thin-output counts reviewed

- `raw_record_count=25000` and `artifact_count=25000`.
- `copy_out_rows=9902`, `copied_files=9235`, `skipped_rows=667`, `total_copied_bytes=1386493641`.
- `staged_groups=11`, `staged_store_db_groups=11`, `staged_files=8986`, `staged_bytes=1368577744`.
- External compare: `external_file_count=4123`, `vestigant_file_count=8986`, `file_match_rows=2213`, `external_only_rows=1424`, `vestigant_only_rows=6710`, `hash_different_path_rows=431`, `RELATIVE_PATH_SIZE_MISMATCH=486`.

## Local source changes

- Updated V1.1.9.1 version metadata.
- Updated build wrapper to check `1.1.9.1`.
- Removed the three MSVC C4189 `btnFlags` warnings by using the decoded flag value in APFS OMAP branch-path diagnostics.

## Required external validation

- Windows/MSVC V1.1.9.1 build.
- macOS AFF4/APFS thin run for wrapper/package verification.
- No iOS validation required for this hotfix.

## Source: `V1_1_8_validation_notes.md`

# V1.1.8 Validation Notes

## Baseline reviewed

- Source baseline: V1.1.7.1.
- Uploaded V1.1.7.1 Windows/MSVC build log: decoded and reviewed; build completed and reported `Vestigant Spotlight v1.1.7.1`.
- Uploaded V1.1.7.1 macOS AFF4/APFS thin ZIP: reviewed; thin ZIP exists and denied raw upload filenames were absent.
- User-provided `BaselineVersionHistory.md` was treated as the append-only version history baseline.

## Changes validated locally

- Version metadata updated to V1.1.8.
- `docs/FULL_VERSION_HISTORY.md`, `docs/BaselineVersionHistory.md`, and `VERSION_HISTORY.md` now use the user-provided baseline with V1.1.8 appended at the top.
- Windows long-path helper API added in `src/core/path_utils.*`.
- APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction file writes now use long-path-capable binary output on Windows.
- Logger writes are mutex-protected.
- SQLite close/checkpoint path now requests WAL truncate.
- Explicit WAL checkpoint/truncate status marker was added before upload packaging.
- APFS decmpfs reconstruction expected-size cap reduced to 256 MiB.

## Local checks performed

- C++20 syntax checks passed for changed/dependent source files:
  - `src/core/path_utils.cpp`
  - `src/core/logger.cpp`
  - `src/parsers/aff4_probe_worker.cpp`
  - `src/app/app_runner.cpp`
  - `src/codec/lzfse_codec.cpp`
  - `src/db/case_db.cpp`
  - `src/core/app_info.cpp`
- Linux CMake configure/build passed.
- CLI version check returned `Vestigant Spotlight v1.1.8`.
- Local self-test passed.

## Not validated here

- Windows/MSVC V1.1.8 build.
- Windows long-path CreateFileW runtime behavior.
- Windows GUI runtime.
- V1.1.8 macOS AFF4/APFS thin run.
- Current iOS runtime parity.

## Source: `V1_1_8_local_validation.log`

```text
V1.1.8 local validation summary

PASS: g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/core/path_utils.cpp
PASS: g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/core/logger.cpp
PASS: g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/aff4_probe_worker.cpp
PASS: g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp
PASS: g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/codec/lzfse_codec.cpp
PASS: g++ -std=c++20 -Isrc -fsyntax-only src/db/case_db.cpp
PASS: g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
PASS: cmake -S . -B build-cmake-validate
PASS: cmake --build build-cmake-validate -j2
PASS: ./build-cmake-validate/VestigantSpotlightCli --version -> Vestigant Spotlight v1.1.8
PASS: ./build-cmake-validate/VestigantSpotlightTests build-cmake-validate/selftest_out
```

## Source: `V1_1_7_validation_notes.md`

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

## Source: `V1_1_7_local_validation.log`

```text
g++ aff4_probe_worker syntax: PASS
g++ app_runner syntax: PASS
cmake configure/build: PASS
CLI version: Vestigant Spotlight v1.1.7
self-test: PASS
```

## Source: `V1_1_7_1_validation_notes.md`

# V1.1.7.1 Validation Notes

## Purpose

V1.1.7.1 is a build hotfix and package cleanup release after V1.1.7 moved both large AFF4/APFS probe bodies into `src/parsers/aff4_probe_worker.cpp`.

## Build failure addressed

Windows/MSVC reported missing identifiers in `aff4_probe_worker.cpp`:

- `shouldSkipLibAff4DynamicProbeForKnownBlockingLayout`
- `findToolCandidate`
- `lastWindowsErrorString`

Root cause: the V1.1.7 dynamic probe relocation moved code that depended on app-runner-local helpers without moving/exposing those helper boundaries.

## Fix implemented

- Added worker-local helper implementations in `src/parsers/aff4_probe_worker.cpp`.
- Preserved internal linkage to avoid exporting new public API or creating ODR/linker risk.
- Left `app_runner.cpp` helpers in place where still used by orchestration/other code.

## Cleanup implemented

- Removed obsolete version-specific build/run/launch/package scripts from `scripts/`.
- Removed old root-level package/deleted-files manifests from the active source root.
- Added `docs/NEW_CHAT_CONTINUATION_GUIDE.md`.
- Added `docs/SOURCE_PACKAGE_CLEANUP_POLICY.md`.
- Added `docs/FULL_VERSION_HISTORY.md` and append-only version history policy files.
- Updated top-level `BUILD_INSTRUCTIONS.md`, `HELP.md`, and troubleshooting/quick-start docs.

## Local validation performed

- C++20 syntax check: `src/parsers/aff4_probe_worker.cpp` — PASS.
- C++20 syntax check: `src/app/app_runner.cpp` — PASS.
- C++20 syntax check: `src/core/app_info.cpp` — PASS.
- Linux CMake configure/build — PASS.
- CLI version check — `Vestigant Spotlight v1.1.7.1`.
- Local self-test — PASS.

## Not validated here

- Windows/MSVC full build.
- Windows GUI runtime.
- V1.1.7.1 macOS AFF4/APFS thin run.

## Notes

Linux build still reports unused-function warnings in `aff4_probe_worker.cpp` because several helpers are Windows-only or preserved as part of the staged AFF4 probe boundary. Do not remove them until Windows/MSVC and AFF4 thin parity are confirmed.

## Source: `V1_1_7_1_local_validation.log`

```text
V1.1.7.1 local validation log
Sun Jun  7 16:35:34 UTC 2026
--- aff4_probe_worker syntax
--- app_runner syntax
--- app_info syntax
--- script inventory
scripts/Build-V1_1_7_1.ps1
scripts/Create-GitHubLabels.ps1
scripts/Create-GitHubProjectIssues.ps1
scripts/Initialize-GitHubRepo.ps1
scripts/Launch-V1_1_7_1-GUI.ps1
scripts/New-ReleaseBranch.ps1
scripts/Package-GitHubValidationBundle.ps1
scripts/Package-V1_1_7_1-macOS-AFF4-ThinFromExistingCase.ps1
scripts/Run-Local-WindowsBuild.ps1
scripts/Run-V1_1_7_1-macOS-AFF4-Probe-AndZip.ps1
scripts/Run-V1_1_7_1-macOS-AFF4-Probe-AndZip.txt
scripts/Sync-Version-To-GitRepo.ps1
-- The C compiler identification is GNU 14.2.0
-- The CXX compiler identification is GNU 14.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Apple lzfse source detected; enabling VESTIGANT_HAS_LZFSE
-- Configuring done (1.1s)
-- Generating done (0.0s)
-- Build files have been written to: /mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/build-validate
[  3%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/app_info.cpp.o
[  6%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/csv.cpp.o
[ 10%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/hash.cpp.o
[ 13%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/logger.cpp.o
[ 16%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/path_utils.cpp.o
[ 20%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/db/case_db.cpp.o
[ 23%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/source_profiles.cpp.o
[ 26%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/store_discovery.cpp.o
[ 30%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/evidence_preservation.cpp.o
[ 33%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/evidence_intake.cpp.o
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/ingest/evidence_preservation.cpp:57:13: warning: 'std::string vestigant::spotlight::{anonymous}::quoteCmd(const std::filesystem::__cxx11::path&)' defined but not used [-Wunused-function]
   57 | std::string quoteCmd(const fs::path& p) {
      |             ^~~~~~~~
[ 36%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/native_storedb_parser.cpp.o
[ 40%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/ios_app_db_parser.cpp.o
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/native_storedb_parser.cpp:2105:25: warning: 'std::vector<vestigant::spotlight::{anonymous}::ParsedItem> vestigant::spotlight::{anonymous}::parseMetadataItems(const std::vector<unsigned char>&, int, const std::map<int, PropertyDef>&, const std::map<int, std::__cxx11::basic_string<char> >&, const std::map<int, std::vector<int> >&, const std::map<int, std::vector<int> >&)' defined but not used [-Wunused-function]
 2105 | std::vector<ParsedItem> parseMetadataItems(const std::vector<std::uint8_t>& payload,
      |                         ^~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/native_storedb_parser.cpp:1731:6: warning: 'void vestigant::spotlight::{anonymous}::addCoreProbeMetadata(ParsedItem&, const std::vector<unsigned char>&)' defined but not used [-Wunused-function]
 1731 | void addCoreProbeMetadata(ParsedItem& item, const std::vector<std::uint8_t>& data) {
      |      ^~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/native_storedb_parser.cpp:1141:6: warning: 'bool vestigant::spotlight::{anonymous}::isHighValueNativeFieldName(const std::string&)' defined but not used [-Wunused-function]
 1141 | bool isHighValueNativeFieldName(const std::string& fieldLower) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~
[ 43%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_volume_reader.cpp.o
[ 46%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_aff4_reader.cpp.o
[ 50%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/aff4_probe_worker.cpp.o
[ 53%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_diagnostic_exporter.cpp.o
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/apfs_diagnostic_exporter.cpp:1306:6: warning: 'bool vestigant::spotlight::{anonymous}::isLikelyStoreV2GroupDirectoryName(const std::string&)' defined but not used [-Wunused-function]
 1306 | bool isLikelyStoreV2GroupDirectoryName(const std::string& name) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:1268:6: warning: 'bool vestigant::spotlight::{anonymous}::shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, std::string&, std::string&)' defined but not used [-Wunused-function]
 1268 | bool shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(const fs::path& caseDir,
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:1194:6: warning: 'bool vestigant::spotlight::{anonymous}::aff4ZipIsIndexEntry(const std::string&)' defined but not used [-Wunused-function]
 1194 | bool aff4ZipIsIndexEntry(const std::string& name) {
      |      ^~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:1179:5: warning: 'int vestigant::spotlight::{anonymous}::aff4ZipDataChunkIndex(const std::string&)' defined but not used [-Wunused-function]
 1179 | int aff4ZipDataChunkIndex(const std::string& name) {
      |     ^~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:1107:13: warning: 'std::string vestigant::spotlight::{anonymous}::aff4ZipApfsHint(const std::string&)' defined but not used [-Wunused-function]
 1107 | std::string aff4ZipApfsHint(const std::string& name) {
      |             ^~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:1099:13: warning: 'std::string vestigant::spotlight::{anonymous}::aff4ZipSpotlightHint(const std::string&)' defined but not used [-Wunused-function]
 1099 | std::string aff4ZipSpotlightHint(const std::string& name) {
      |             ^~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:1090:13: warning: 'std::string vestigant::spotlight::{anonymous}::aff4ZipEntryClassification(const std::string&)' defined but not used [-Wunused-function]
 1090 | std::string aff4ZipEntryClassification(const std::string& name) {
      |             ^~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:956:11: warning: 'long long int vestigant::spotlight::{anonymous}::cacheNameFileIdClosenessScore(const std::string&, uint64_t)' defined but not used [-Wunused-function]
  956 | long long cacheNameFileIdClosenessScore(const std::string& targetName, std::uint64_t childFileId) {
      |           ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:921:15: warning: 'uint64_t vestigant::spotlight::{anonymous}::decmpfsUncompressedSizeFromPreviewHex(const std::string&)' defined but not used [-Wunused-function]
  921 | std::uint64_t decmpfsUncompressedSizeFromPreviewHex(const std::string& hex) {
      |               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:898:5: warning: 'int vestigant::spotlight::{anonymous}::decmpfsCompressionTypeFromPreviewHex(const std::string&)' defined but not used [-Wunused-function]
  898 | int decmpfsCompressionTypeFromPreviewHex(const std::string& hex) {
      |     ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:889:13: warning: 'std::string vestigant::spotlight::{anonymous}::apfsXattrStorageLabel(uint16_t)' defined but not used [-Wunused-function]
  889 | std::string apfsXattrStorageLabel(std::uint16_t flags) {
      |             ^~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:880:6: warning: 'bool vestigant::spotlight::{anonymous}::isApfsCompressionOrResourceXattrName(const std::string&)' defined but not used [-Wunused-function]
  880 | bool isApfsCompressionOrResourceXattrName(const std::string& name) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:484:13: warning: 'std::string vestigant::spotlight::{anonymous}::joinU64List(const std::vector<long unsigned int>&, std::size_t)' defined but not used [-Wunused-function]
  484 | std::string joinU64List(const std::vector<std::uint64_t>& values, std::size_t maxCount = 32) {
      |             ^~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:449:13: warning: 'std::string vestigant::spotlight::{anonymous}::directPreviewStatusForBytes(const std::vector<unsigned char>&)' defined but not used [-Wunused-function]
  449 | std::string directPreviewStatusForBytes(const std::vector<unsigned char>& bytes) {
      |             ^~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:423:13: warning: 'std::string vestigant::spotlight::{anonymous}::utf16LeNameToAscii(const std::vector<unsigned char>&, std::size_t, std::size_t)' defined but not used [-Wunused-function]
  423 | std::string utf16LeNameToAscii(const std::vector<unsigned char>& data, std::size_t off, std::size_t maxBytes) {
      |             ^~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:417:6: warning: 'bool vestigant::spotlight::{anonymous}::allZeroBytes(const unsigned char*, std::size_t)' defined but not used [-Wunused-function]
  417 | bool allZeroBytes(const unsigned char* b, std::size_t n) {
      |      ^~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:405:13: warning: 'std::string vestigant::spotlight::{anonymous}::guidFromGptBytes(const unsigned char*)' defined but not used [-Wunused-function]
  405 | std::string guidFromGptBytes(const unsigned char* b) {
      |             ^~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:152:13: warning: 'std::string vestigant::spotlight::{anonymous}::lastWindowsErrorString()' defined but not used [-Wunused-function]
  152 | std::string lastWindowsErrorString() {
      |             ^~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/parsers/aff4_probe_worker.cpp:128:10: warning: 'std::filesystem::__cxx11::path vestigant::spotlight::{anonymous}::findToolCandidate(const vestigant::spotlight::RunOptions&, const std::string&, const std::vector<std::__cxx11::basic_string<char> >&)' defined but not used [-Wunused-function]
  128 | fs::path findToolCandidate(const RunOptions& opt, const std::string& envVar, const std::vector<std::string>& names) {
      |          ^~~~~~~~~~~~~~~~~
[ 56%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/codec/lzfse_codec.cpp.o
[ 60%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/enrich_sql/sqlite_enrichment.cpp.o
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/enrich_sql/sqlite_enrichment.cpp:16:13: warning: 'std::string vestigant::spotlight::{anonymous}::stripLeadingSlash(std::string)' defined but not used [-Wunused-function]
   16 | std::string stripLeadingSlash(std::string s) {
      |             ^~~~~~~~~~~~~~~~~
[ 63%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/export_sql/sqlite_exporter.cpp.o
[ 66%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/app/case_store.cpp.o
[ 70%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/app/app_runner.cpp.o
[ 73%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_decode.c.o
[ 76%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_decode_base.c.o
[ 80%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_fse.c.o
[ 83%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzvn_decode_base.c.o
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/app/app_runner.cpp:3159:28: warning: 'vestigant::spotlight::{anonymous}::IosZipInventoryParseResult vestigant::spotlight::{anonymous}::parseIosSevenZipRawInventoryToCsv(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, vestigant::spotlight::Logger&)' defined but not used [-Wunused-function]
 3159 | IosZipInventoryParseResult parseIosSevenZipRawInventoryToCsv(const fs::path& caseDir, const fs::path& zipPath, Logger& log) {
      |                            ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/app/app_runner.cpp:2524:10: warning: 'std::filesystem::__cxx11::path vestigant::spotlight::{anonymous}::writeIosFocusedZipExtractorScript(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, bool)' defined but not used [-Wunused-function]
 2524 | fs::path writeIosFocusedZipExtractorScript(const fs::path& caseDir, const fs::path& zipPath, const fs::path& stageRoot, const fs::path& inventoryPath, bool throwOnNoMatch) {
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/app/app_runner.cpp:1447:30: warning: 'vestigant::spotlight::{anonymous}::ApfsInodeExtendedFieldDecode vestigant::spotlight::{anonymous}::decodeApfsInodeExtendedFieldsForProbe(const std::vector<unsigned char>&, std::size_t, std::size_t)' defined but not used [-Wunused-function]
 1447 | ApfsInodeExtendedFieldDecode decodeApfsInodeExtendedFieldsForProbe(const std::vector<unsigned char>& node,
      |                              ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_cleanup_v1171/VestigantSpotlightInv_V1_1_7_1/src/app/app_runner.cpp:563:6: warning: 'bool vestigant::spotlight::{anonymous}::copyDirectoryIfExists(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, std::ofstream&, const std::string&)' defined but not used [-Wunused-function]
  563 | bool copyDirectoryIfExists(const fs::path& src, const fs::path& dst, std::ofstream& manifest, const std::string& absentStatus = "MISSING_DIR") {
      |      ^~~~~~~~~~~~~~~~~~~~~
[ 86%] Linking CXX static library libvestigant_spotlight_core.a
[ 86%] Built target vestigant_spotlight_core
[ 90%] Building CXX object CMakeFiles/VestigantSpotlightCli.dir/src/cli/main.cpp.o
[ 93%] Building CXX object CMakeFiles/VestigantSpotlightTests.dir/tests/main.cpp.o
[ 96%] Linking CXX executable VestigantSpotlightCli
[100%] Linking CXX executable VestigantSpotlightTests
[100%] Built target VestigantSpotlightTests
[100%] Built target VestigantSpotlightCli
Vestigant Spotlight v1.1.7.1
Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v1.1.7.1: "build-validate/selftest_out"
```

## Source: `V1_1_6_validation_notes.md`

# V1.1.6 Validation Notes

## Scope

V1.1.6 begins the Tracker #17 AFF4/APFS probe-worker extraction by physically moving the direct-map probe body out of `src/app/app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp`.

## Baseline reviewed

- Uploaded `V1_1_5_1_build.log`: Windows/MSVC build completed and reported `Vestigant Spotlight v1.1.5.1`.
- Uploaded `Upload_Thin_MacOS_AFF4_V1_1_5_1.zip`: generated successfully, denied raw upload filenames absent, and AFF4/APFS Store-V2 baseline counts remained present.

## Implemented

- Added `src/parsers/aff4_probe_worker.h`.
- Added `src/parsers/aff4_probe_worker.cpp`.
- Moved `writeAff4DirectMapReaderProbe(...)` body into `Aff4ProbeWorker::executeDirectMapReaderProbe(...)`.
- Updated `src/app/app_runner.cpp` to delegate direct-map probe calls to the new worker.
- Added the worker source to CMake and MSVC build lists.
- Updated workflow ledger, roadmap, handoff, and suggestions tracker.

## Deferred

`writeAff4CppLiteDynamicLoadProbe(...)` remains in `app_runner.cpp`. A full extraction attempt exposed a large dependency surface on app-runner-local APFS helper structs/functions and status helpers. The next safe step is a dedicated dependency-boundary pass rather than copying hidden dependencies blindly.

## Local validation

- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/aff4_probe_worker.cpp`: PASS
- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp`: PASS
- Linux CMake configure/build: PASS
- CLI version check: `Vestigant Spotlight v1.1.6`
- Local self-test: PASS

## Not validated

- Windows/MSVC V1.1.6 build.
- Windows GUI runtime.
- V1.1.6 macOS AFF4/APFS thin run.

## Source: `V1_1_6_local_validation.log`

```text
2026-06-07T12:56:00Z V1.1.6 local validation
Baseline: V1.1.5.1 build/thin reviewed.
g++ aff4_probe_worker syntax: PASS
g++ app_runner syntax: PASS
Linux CMake configure/build: PASS
CLI version: Vestigant Spotlight v1.1.6
Self-test: PASS
```

## Source: `V1_1_6_1_validation_notes.md`

# V1.1.6.1 Validation Notes

## Scope

V1.1.6.1 is a narrow Windows/MSVC build hotfix for V1.1.6.

The V1.1.6 direct-map AFF4/APFS probe worker split moved Windows-only file-read code into `src/parsers/aff4_probe_worker.cpp`. MSVC then reported that `wideProcessPath(...)` was not defined in the new translation unit.

## Change

- Added a translation-unit-local Windows-only path widening helper in `src/parsers/aff4_probe_worker.cpp`.
- Corrected the versioned V1.1.6.1 build/run wrappers.
- Updated version, handoff, workflow ledger, roadmap, and suggestions tracker.

## Not changed

- No AFF4/APFS traversal behavior changed.
- No Store-V2 parser behavior changed.
- No iOS parser behavior changed.
- No SQLite schema changed.
- No GUI behavior changed.
- No copy-out/staging semantics changed.

## Local validation

Passed:

- Static check: no bad `return false` cancellation branch remains from V1.1.5.
- Static check: `wideProcessPath(...)` exists in `aff4_probe_worker.cpp` under `_WIN32`.
- Static check: `Build-V1_1_6_1.ps1` checks for `1.1.6.1`.
- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/aff4_probe_worker.cpp`
- `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp`
- `g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp`

Attempted:

- Linux CMake configure/build. Configure succeeded and build progressed through changed/dependent translation units. The container timed out during later existing build steps with no compile error shown before timeout.

Not validated:

- Windows/MSVC full build.
- Windows AFF4/APFS direct-map runtime.
- V1.1.6.1 macOS AFF4/APFS thin run.

## Source: `V1_1_6_1_local_validation.log`

```text
V1.1.6.1 local validation
Sun Jun  7 13:39:07 UTC 2026

Check version files
1.1.6.1
1.1.6.1

Build script version gate:
35:if ($version -notmatch "1\.1\.6\.1") { throw "Unexpected CLI version after build: $version" }

Windows-only helper location:
74:std::wstring wideProcessPath(const fs::path& p) {
1029:    const std::wstring widePath = wideProcessPath(path);
syntax src/parsers/aff4_probe_worker.cpp
syntax src/app/app_runner.cpp
syntax src/core/app_info.cpp
-- The C compiler identification is GNU 14.2.0
-- The CXX compiler identification is GNU 14.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Apple lzfse source detected; enabling VESTIGANT_HAS_LZFSE
-- Configuring done (1.7s)
-- Generating done (0.0s)
-- Build files have been written to: /mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/build-cmake-v1161
[  3%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/csv.cpp.o
[  6%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/app_info.cpp.o
[ 10%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/hash.cpp.o
[ 13%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/logger.cpp.o
[ 16%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/path_utils.cpp.o
[ 20%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/db/case_db.cpp.o
[ 23%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/source_profiles.cpp.o
[ 26%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/store_discovery.cpp.o
[ 30%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/evidence_preservation.cpp.o
[ 33%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/evidence_intake.cpp.o
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/ingest/evidence_preservation.cpp:57:13: warning: 'std::string vestigant::spotlight::{anonymous}::quoteCmd(const std::filesystem::__cxx11::path&)' defined but not used [-Wunused-function]
   57 | std::string quoteCmd(const fs::path& p) {
      |             ^~~~~~~~
[ 36%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/native_storedb_parser.cpp.o
[ 40%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/ios_app_db_parser.cpp.o
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/native_storedb_parser.cpp:2105:25: warning: 'std::vector<vestigant::spotlight::{anonymous}::ParsedItem> vestigant::spotlight::{anonymous}::parseMetadataItems(const std::vector<unsigned char>&, int, const std::map<int, PropertyDef>&, const std::map<int, std::__cxx11::basic_string<char> >&, const std::map<int, std::vector<int> >&, const std::map<int, std::vector<int> >&)' defined but not used [-Wunused-function]
 2105 | std::vector<ParsedItem> parseMetadataItems(const std::vector<std::uint8_t>& payload,
      |                         ^~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/native_storedb_parser.cpp:1731:6: warning: 'void vestigant::spotlight::{anonymous}::addCoreProbeMetadata(ParsedItem&, const std::vector<unsigned char>&)' defined but not used [-Wunused-function]
 1731 | void addCoreProbeMetadata(ParsedItem& item, const std::vector<std::uint8_t>& data) {
      |      ^~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/native_storedb_parser.cpp:1141:6: warning: 'bool vestigant::spotlight::{anonymous}::isHighValueNativeFieldName(const std::string&)' defined but not used [-Wunused-function]
 1141 | bool isHighValueNativeFieldName(const std::string& fieldLower) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~
[ 43%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_volume_reader.cpp.o
[ 46%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_aff4_reader.cpp.o
[ 50%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/aff4_probe_worker.cpp.o
[ 53%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_diagnostic_exporter.cpp.o
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:1118:6: warning: 'bool vestigant::spotlight::{anonymous}::readAff4StoredZipEntryTextFromProbe(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, const std::string&, std::size_t, std::string&, std::string&)' defined but not used [-Wunused-function]
 1118 | bool readAff4StoredZipEntryTextFromProbe(const fs::path& caseDir,
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:1096:6: warning: 'bool vestigant::spotlight::{anonymous}::aff4ZipIsIndexEntry(const std::string&)' defined but not used [-Wunused-function]
 1096 | bool aff4ZipIsIndexEntry(const std::string& name) {
      |      ^~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:1081:5: warning: 'int vestigant::spotlight::{anonymous}::aff4ZipDataChunkIndex(const std::string&)' defined but not used [-Wunused-function]
 1081 | int aff4ZipDataChunkIndex(const std::string& name) {
      |     ^~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:1009:13: warning: 'std::string vestigant::spotlight::{anonymous}::aff4ZipApfsHint(const std::string&)' defined but not used [-Wunused-function]
 1009 | std::string aff4ZipApfsHint(const std::string& name) {
      |             ^~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:1001:13: warning: 'std::string vestigant::spotlight::{anonymous}::aff4ZipSpotlightHint(const std::string&)' defined but not used [-Wunused-function]
 1001 | std::string aff4ZipSpotlightHint(const std::string& name) {
      |             ^~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:992:13: warning: 'std::string vestigant::spotlight::{anonymous}::aff4ZipEntryClassification(const std::string&)' defined but not used [-Wunused-function]
  992 | std::string aff4ZipEntryClassification(const std::string& name) {
      |             ^~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:858:11: warning: 'long long int vestigant::spotlight::{anonymous}::cacheNameFileIdClosenessScore(const std::string&, uint64_t)' defined but not used [-Wunused-function]
  858 | long long cacheNameFileIdClosenessScore(const std::string& targetName, std::uint64_t childFileId) {
      |           ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:823:15: warning: 'uint64_t vestigant::spotlight::{anonymous}::decmpfsUncompressedSizeFromPreviewHex(const std::string&)' defined but not used [-Wunused-function]
  823 | std::uint64_t decmpfsUncompressedSizeFromPreviewHex(const std::string& hex) {
      |               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:800:5: warning: 'int vestigant::spotlight::{anonymous}::decmpfsCompressionTypeFromPreviewHex(const std::string&)' defined but not used [-Wunused-function]
  800 | int decmpfsCompressionTypeFromPreviewHex(const std::string& hex) {
      |     ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:791:13: warning: 'std::string vestigant::spotlight::{anonymous}::apfsXattrStorageLabel(uint16_t)' defined but not used [-Wunused-function]
  791 | std::string apfsXattrStorageLabel(std::uint16_t flags) {
      |             ^~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:782:6: warning: 'bool vestigant::spotlight::{anonymous}::isApfsCompressionOrResourceXattrName(const std::string&)' defined but not used [-Wunused-function]
  782 | bool isApfsCompressionOrResourceXattrName(const std::string& name) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:393:13: warning: 'std::string vestigant::spotlight::{anonymous}::joinU64List(const std::vector<long unsigned int>&, std::size_t)' defined but not used [-Wunused-function]
  393 | std::string joinU64List(const std::vector<std::uint64_t>& values, std::size_t maxCount = 32) {
      |             ^~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:358:13: warning: 'std::string vestigant::spotlight::{anonymous}::directPreviewStatusForBytes(const std::vector<unsigned char>&)' defined but not used [-Wunused-function]
  358 | std::string directPreviewStatusForBytes(const std::vector<unsigned char>& bytes) {
      |             ^~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:332:13: warning: 'std::string vestigant::spotlight::{anonymous}::utf16LeNameToAscii(const std::vector<unsigned char>&, std::size_t, std::size_t)' defined but not used [-Wunused-function]
  332 | std::string utf16LeNameToAscii(const std::vector<unsigned char>& data, std::size_t off, std::size_t maxBytes) {
      |             ^~~~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:326:6: warning: 'bool vestigant::spotlight::{anonymous}::allZeroBytes(const unsigned char*, std::size_t)' defined but not used [-Wunused-function]
  326 | bool allZeroBytes(const unsigned char* b, std::size_t n) {
      |      ^~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/aff4_probe_worker.cpp:314:13: warning: 'std::string vestigant::spotlight::{anonymous}::guidFromGptBytes(const unsigned char*)' defined but not used [-Wunused-function]
  314 | std::string guidFromGptBytes(const unsigned char* b) {
      |             ^~~~~~~~~~~~~~~~
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/parsers/apfs_diagnostic_exporter.cpp:1306:6: warning: 'bool vestigant::spotlight::{anonymous}::isLikelyStoreV2GroupDirectoryName(const std::string&)' defined but not used [-Wunused-function]
 1306 | bool isLikelyStoreV2GroupDirectoryName(const std::string& name) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[ 56%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/codec/lzfse_codec.cpp.o
[ 60%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/enrich_sql/sqlite_enrichment.cpp.o
[ 63%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/export_sql/sqlite_exporter.cpp.o
/mnt/data/work_v1161/VestigantSpotlightInv_V1_1_6/src/enrich_sql/sqlite_enrichment.cpp:16:13: warning: 'std::string vestigant::spotlight::{anonymous}::stripLeadingSlash(std::string)' defined but not used [-Wunused-function]
   16 | std::string stripLeadingSlash(std::string s) {
      |             ^~~~~~~~~~~~~~~~~
[ 66%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/app/case_store.cpp.o
gmake[2]: *** [CMakeFiles/vestigant_spotlight_core.dir/build.make:331: CMakeFiles/vestigant_spotlight_core.dir/src/export_sql/sqlite_exporter.cpp.o] Terminated
gmake[1]: *** [CMakeFiles/Makefile2:93: CMakeFiles/vestigant_spotlight_core.dir/all] Terminated
gmake: *** [Makefile:91: all] Terminated
```

## Source: `V1_1_5_validation_notes.md`

# V1.1.5 Validation Notes

## Baseline reviewed

- V1.1.4 Windows/MSVC build log: completed and reported `Vestigant Spotlight v1.1.4`.
- V1.1.4 macOS AFF4/APFS thin ZIP: generated successfully; denied raw upload filenames were absent.
- Current review: requested deeper cancellation propagation, upload-sample size guards, UTF-8 extraction logs, local diagnostic sample export error handling, and continued tracking of the dynamic AFF4/APFS probe monolith.

## Changes made

- Propagated the existing ingest cancellation token to `writeAff4CppLiteDynamicLoadProbe(...)` and `writeAff4DirectMapReaderProbe(...)`.
- Added cancellation checks in selected expensive bounded AFF4/APFS loops.
- Added case-directory writability preflight before logger/database setup.
- Replaced recursive `exports/upload_samples` thin-upload copying with explicit policy/size-guarded copy handling.
- Added nested upload-sample size-policy parity to `tools/Create-SourceProbeUploadZip.ps1`.
- Changed targeted app database and focused CoreSpotlight 7-Zip extraction logs to UTF-8 `Out-File`.
- Wrapped APFS staged Store-V2 diagnostic sample CSV exports in a localized try/catch.
- Updated handoff, roadmap, suggestion tracker, workflow ledger, version history, and release notes.

## Local validation

- C++20 syntax checks passed for changed/dependent source files.
- Linux CMake configure passed.
- Linux CMake build passed.
- CLI version check returned `Vestigant Spotlight v1.1.5`.
- Local self-test passed.

## Not validated here

- Windows/MSVC V1.1.5 build.
- Windows GUI runtime.
- GUI Cancel Ingest behavior during long AFF4/APFS probe loops.
- V1.1.5 macOS AFF4/APFS thin run.
- Current iOS runtime parity.

## Deferred

- Full `writeAff4CppLiteDynamicLoadProbe(...)` extraction to `aff4_probe_worker.cpp`.
- Full `stageZipEvidenceSource(...)` relocation to `EvidenceIntake`.
- Live APFS horizontal leaf traversal replacement and absolute path reconstruction.
- Full NSKeyedArchiver UID graph decoding.

## Source: `V1_1_5_local_validation.log`

```text
Command: g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp
Result: PASS

Command: g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp
Result: PASS

Command: g++ -std=c++20 -Isrc -fsyntax-only src/parsers/native_storedb_parser.cpp
Result: PASS

Command: g++ -std=c++20 -Isrc -fsyntax-only src/ingest/evidence_intake.cpp
Result: PASS

Command: g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
Result: PASS

Command: cmake -S . -B build-cmake
Result: PASS

Command: cmake --build build-cmake -j1
Result: PASS (warnings only)

Command: ./build-cmake/VestigantSpotlightCli --version
Output: Vestigant Spotlight v1.1.5

Command: ./build-cmake/VestigantSpotlightTests build-cmake/selftest_out
Output: Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v1.1.5: "build-cmake/selftest_out"
```

## Source: `V1_1_5_1_validation_notes.md`

# V1.1.5.1 Validation Notes

## Scope

V1.1.5.1 is a narrow MSVC build hotfix for V1.1.5.

## Root cause

V1.1.5 added a cancellation check inside a volume OMAP lookup lambda returning `ApfsOmapTargetResolution`, but the cancellation branch returned `false`. MSVC correctly failed with C2440 because `bool` cannot be converted to `ApfsOmapTargetResolution`.

## Fix

The cancellation branch now sets `lookupStatus`, `interpretation`, and `notes` on the local `ApfsOmapTargetResolution out` object and returns `out`.

## Not changed

No APFS traversal semantics, Store-V2 parsing, iOS parsing, GUI behavior, SQLite schema, or copy-out behavior changed.

## Local validation

- Source syntax checks were run for changed/dependent files.
- Linux CMake configure/build and local self-test were attempted/recorded in `V1_1_5_1_local_validation.log`.

## Windows validation pending

- Windows/MSVC build.
- macOS AFF4/APFS thin run.

## Source: `V1_1_5_1_local_validation.log`

```text
V1.1.5.1 local validation started
Sun Jun  7 11:52:58 UTC 2026
--- grep bad return ---
--- syntax app_runner ---
--- syntax deps ---
--- cmake configure/build ---
-- The C compiler identification is GNU 14.2.0
-- The CXX compiler identification is GNU 14.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Apple lzfse source detected; enabling VESTIGANT_HAS_LZFSE
-- Configuring done (3.1s)
-- Generating done (0.0s)
-- Build files have been written to: /mnt/data/fix_v115/VestigantSpotlightInv_V1_1_5/build-cmake-validate
/mnt/data/fix_v115/VestigantSpotlightInv_V1_1_5/src/ingest/evidence_preservation.cpp:57:13: warning: 'std::string vestigant::spotlight::{anonymous}::quoteCmd(const std::filesystem::__cxx11::path&)' defined but not used [-Wunused-function]
   57 | std::string quoteCmd(const fs::path& p) {
      |             ^~~~~~~~
/mnt/data/fix_v115/VestigantSpotlightInv_V1_1_5/src/parsers/native_storedb_parser.cpp:2105:25: warning: 'std::vector<vestigant::spotlight::{anonymous}::ParsedItem> vestigant::spotlight::{anonymous}::parseMetadataItems(const std::vector<unsigned char>&, int, const std::map<int, PropertyDef>&, const std::map<int, std::__cxx11::basic_string<char> >&, const std::map<int, std::vector<int> >&, const std::map<int, std::vector<int> >&)' defined but not used [-Wunused-function]
 2105 | std::vector<ParsedItem> parseMetadataItems(const std::vector<std::uint8_t>& payload,
      |                         ^~~~~~~~~~~~~~~~~~
/mnt/data/fix_v115/VestigantSpotlightInv_V1_1_5/src/parsers/native_storedb_parser.cpp:1731:6: warning: 'void vestigant::spotlight::{anonymous}::addCoreProbeMetadata(ParsedItem&, const std::vector<unsigned char>&)' defined but not used [-Wunused-function]
 1731 | void addCoreProbeMetadata(ParsedItem& item, const std::vector<std::uint8_t>& data) {
      |      ^~~~~~~~~~~~~~~~~~~~
/mnt/data/fix_v115/VestigantSpotlightInv_V1_1_5/src/parsers/native_storedb_parser.cpp:1141:6: warning: 'bool vestigant::spotlight::{anonymous}::isHighValueNativeFieldName(const std::string&)' defined but not used [-Wunused-function]
 1141 | bool isHighValueNativeFieldName(const std::string& fieldLower) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~
--- second syntax pass clang++ app_runner ---
--- second syntax pass g++ app_info ---
second syntax pass complete
```

## Source: `V1_1_4_validation_notes.md`

# V1.1.4 Validation Notes

## Baseline reviewed

- Source baseline: `VestigantSpotlightInv_V1_1_3.zip`.
- Windows/MSVC baseline build log: `V1_1_3_build.log`.
- Baseline thin ZIP: `Upload_Thin_MacOS_AFF4_V1_1_3.zip`.
- Baseline build status: V1.1.3 build completed and reported `Vestigant Spotlight v1.1.3`.
- Thin ZIP status: generated successfully; denied raw upload filenames were absent.
- AFF4/APFS baseline counts reviewed from thin ZIP:
  - `raw_record_count=25000`
  - `raw_key_value_count=2181`
  - `raw_date_candidate_count=25000`
  - `artifact_count=25000`
  - `external_file_count=4123`
  - `vestigant_file_count=8986`
  - `file_match_rows=2213`
  - `external_only_rows=1424`
  - `vestigant_only_rows=6710`

## V1.1.4 implemented changes

- Added bounded bplist offset-table validation metadata to the existing bplist context summary:
  - `offset_table_bytes`
  - `offset_table_status=parsed`
  - `top_object_offset_rel=<offset|invalid>`
- Added checked-artifact snapshot helpers in the Win32 GUI and used them when constructing review/export requests.
- Strengthened the GUI ingest launch gate using `compare_exchange_strong` to reject repeated `Build / Process Case` starts before a second worker is created.
- Updated workflow ledger, handoff, roadmap, suggestions/fixes tracker, release/history notes, and validation notes.

## Not changed

- No live APFS extraction/traversal behavior changed.
- No Store-V2 parser schema changed.
- No SQLite schema changed.
- No full NSKeyedArchiver UID graph decoding was added.
- No live APFS absolute-path substitution was added.
- The dynamic AFF4/APFS probe monolith remains tracked for a dedicated high-risk refactor.

## Local validation performed

Pass 1:

```text
g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/native_storedb_parser.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
```

Pass 2:

```text
g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp
cmake -S . -B build-cmake-validate
cmake --build build-cmake-validate -j2
./build-cmake-validate/VestigantSpotlightCli --version
./build-cmake-validate/VestigantSpotlightTests build-cmake-validate/selftest_out
```

Results:

- Linux CMake configure passed.
- Linux CMake build passed.
- CLI version reported `Vestigant Spotlight v1.1.4`.
- Local self-test passed.

## Not validated here

- Windows/MSVC V1.1.4 build.
- Windows GUI runtime.
- V1.1.4 macOS AFF4/APFS thin run.
- iOS runtime parity after previous EvidenceIntake changes.

## Source: `V1_1_4_local_validation.log`

```text
V1.1.4 local validation
Sun Jun  7 02:57:25 UTC 2026
--- syntax pass 1
--- syntax pass 2
--- cmake configure
-- The C compiler identification is GNU 14.2.0
-- The CXX compiler identification is GNU 14.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Apple lzfse source detected; enabling VESTIGANT_HAS_LZFSE
-- Configuring done (1.7s)
-- Generating done (0.0s)
-- Build files have been written to: /mnt/data/work_v114/VestigantSpotlightInv_V1_1_4/build-cmake-validate
--- cmake build
[  3%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/csv.cpp.o
[  6%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/app_info.cpp.o
[ 10%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/hash.cpp.o
[ 13%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/logger.cpp.o
[ 17%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/path_utils.cpp.o
[ 20%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/db/case_db.cpp.o
[ 24%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/source_profiles.cpp.o
[ 27%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/store_discovery.cpp.o
[ 31%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/evidence_preservation.cpp.o
[ 34%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/evidence_intake.cpp.o
/mnt/data/work_v114/VestigantSpotlightInv_V1_1_4/src/ingest/evidence_preservation.cpp:57:13: warning: 'std::string vestigant::spotlight::{anonymous}::quoteCmd(const std::filesystem::__cxx11::path&)' defined but not used [-Wunused-function]
   57 | std::string quoteCmd(const fs::path& p) {
      |             ^~~~~~~~
[ 37%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/native_storedb_parser.cpp.o
[ 41%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/ios_app_db_parser.cpp.o
/mnt/data/work_v114/VestigantSpotlightInv_V1_1_4/src/parsers/native_storedb_parser.cpp:2105:25: warning: 'std::vector<vestigant::spotlight::{anonymous}::ParsedItem> vestigant::spotlight::{anonymous}::parseMetadataItems(const std::vector<unsigned char>&, int, const std::map<int, PropertyDef>&, const std::map<int, std::__cxx11::basic_string<char> >&, const std::map<int, std::vector<int> >&, const std::map<int, std::vector<int> >&)' defined but not used [-Wunused-function]
 2105 | std::vector<ParsedItem> parseMetadataItems(const std::vector<std::uint8_t>& payload,
      |                         ^~~~~~~~~~~~~~~~~~
/mnt/data/work_v114/VestigantSpotlightInv_V1_1_4/src/parsers/native_storedb_parser.cpp:1731:6: warning: 'void vestigant::spotlight::{anonymous}::addCoreProbeMetadata(ParsedItem&, const std::vector<unsigned char>&)' defined but not used [-Wunused-function]
 1731 | void addCoreProbeMetadata(ParsedItem& item, const std::vector<std::uint8_t>& data) {
      |      ^~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v114/VestigantSpotlightInv_V1_1_4/src/parsers/native_storedb_parser.cpp:1141:6: warning: 'bool vestigant::spotlight::{anonymous}::isHighValueNativeFieldName(const std::string&)' defined but not used [-Wunused-function]
 1141 | bool isHighValueNativeFieldName(const std::string& fieldLower) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~
[ 44%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_volume_reader.cpp.o
[ 48%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_aff4_reader.cpp.o
[ 51%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_diagnostic_exporter.cpp.o
[ 55%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/codec/lzfse_codec.cpp.o
/mnt/data/work_v114/VestigantSpotlightInv_V1_1_4/src/parsers/apfs_diagnostic_exporter.cpp:1306:6: warning: 'bool vestigant::spotlight::{anonymous}::isLikelyStoreV2GroupDirectoryName(const std::string&)' defined but not used [-Wunused-function]
 1306 | bool isLikelyStoreV2GroupDirectoryName(const std::string& name) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[ 58%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/enrich_sql/sqlite_enrichment.cpp.o
[ 62%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/export_sql/sqlite_exporter.cpp.o
/mnt/data/work_v114/VestigantSpotlightInv_V1_1_4/src/enrich_sql/sqlite_enrichment.cpp:16:13: warning: 'std::string vestigant::spotlight::{anonymous}::stripLeadingSlash(std::string)' defined but not used [-Wunused-function]
   16 | std::string stripLeadingSlash(std::string s) {
      |             ^~~~~~~~~~~~~~~~~
[ 65%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/app/case_store.cpp.o
[ 68%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/app/app_runner.cpp.o
[ 72%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_decode.c.o
[ 75%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_decode_base.c.o
[ 79%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_fse.c.o
[ 82%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzvn_decode_base.c.o
--- continuing cmake build
[  3%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/app/app_runner.cpp.o
/mnt/data/work_v114/VestigantSpotlightInv_V1_1_4/src/app/app_runner.cpp:3144:28: warning: 'vestigant::spotlight::{anonymous}::IosZipInventoryParseResult vestigant::spotlight::{anonymous}::parseIosSevenZipRawInventoryToCsv(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, vestigant::spotlight::Logger&)' defined but not used [-Wunused-function]
 3144 | IosZipInventoryParseResult parseIosSevenZipRawInventoryToCsv(const fs::path& caseDir, const fs::path& zipPath, Logger& log) {
      |                            ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v114/VestigantSpotlightInv_V1_1_4/src/app/app_runner.cpp:2509:10: warning: 'std::filesystem::__cxx11::path vestigant::spotlight::{anonymous}::writeIosFocusedZipExtractorScript(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, bool)' defined but not used [-Wunused-function]
 2509 | fs::path writeIosFocusedZipExtractorScript(const fs::path& caseDir, const fs::path& zipPath, const fs::path& stageRoot, const fs::path& inventoryPath, bool throwOnNoMatch) {
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[  6%] Linking CXX static library libvestigant_spotlight_core.a
[ 86%] Built target vestigant_spotlight_core
[ 89%] Building CXX object CMakeFiles/VestigantSpotlightCli.dir/src/cli/main.cpp.o
[ 93%] Building CXX object CMakeFiles/VestigantSpotlightTests.dir/tests/main.cpp.o
[ 96%] Linking CXX executable VestigantSpotlightCli
[100%] Linking CXX executable VestigantSpotlightTests
[100%] Built target VestigantSpotlightTests
[100%] Built target VestigantSpotlightCli
--- version
Vestigant Spotlight v1.1.4
--- selftest
Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v1.1.4: "build-cmake-validate/selftest_out"
--- static review pass
1322:       << ";offset_table_status=parsed"
1323:       << ";top_object_offset_rel=" << (topObjectOffsetOk ? std::to_string(topObjectOffsetRel) : std::string("invalid"));
805:std::set<long long> checkedArtifactIdsSnapshotNoThrow() {
824:    const auto snapshot = checkedArtifactIdsSnapshotNoThrow();
1573:        if (!gIngestActive.compare_exchange_strong(expected, true)) return false;
1837:    if (!gExportPageActive.compare_exchange_strong(expected, true)) {
1855:    request.checkedArtifactIds = checkedArtifactIdsSnapshotNoThrow();
1872:    if (!gExportFilteredActive.compare_exchange_strong(expected, true)) {
1896:    request.checkedArtifactIds = checkedArtifactIdsSnapshotNoThrow();
1914:    if (!gExportCheckedActive.compare_exchange_strong(expected, true)) {
1922:    const auto checkedSnapshot = checkedArtifactIdsSnapshotNoThrow();
1938:    if (!gExportTaggedActive.compare_exchange_strong(expected, true)) {
```

## Source: `V1_1_3_validation_notes.md`

# V1.1.3 Validation Notes

## Scope

V1.1.3 is a repeat-cycle hardening release based on V1.1.2. It implements shutdown-aware GUI export cancellation, orphan-source purge transaction wrapping, secure RichEdit loading, and non-live APFS next-leaf iterator scaffolding improvements.

## Baseline reviewed

- `V1_1_2_build.log`: Windows/MSVC build completed and reported `Vestigant Spotlight v1.1.2`.
- `Upload_Thin_MacOS_AFF4_V1_1_2.zip`: generated successfully; denied raw upload filenames were absent.
- V1.1.2 AFF4/APFS staged Store-V2 baseline remained stable: `raw_records=25000`, `raw_key_values=2181`, `raw_date_candidates=25000`, `artifact_count=25000`.

## Implemented

- Added `GuiViewExportRequest::shouldCancel`.
- Added cancellation checks to export current-page, export filtered, checked export, tagged export, and support CSV writer loops.
- Win32 GUI now passes `gShuttingDown` cancellation callbacks into export workers.
- `purgeOrphanSourceRows(...)` now attempts one SQLite transaction around the table purge loop.
- RichEdit is loaded via `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_SYSTEM32)` and freed on `WM_DESTROY`.
- `ApfsVolumeReader::enumerateDirectory(...)` can use the non-live APFS footer helper when no injected next-leaf reader is supplied.
- Workflow ledger, handoff, roadmap checklist, and suggestions/fixes tracker updated.

## Not changed

- No live AFF4/APFS extraction behavior change.
- No live APFS staged path substitution.
- No full NSKeyedArchiver UID graph decoding.
- No SQLite schema changes.
- No dynamic probe worker extraction yet.

## Local validation

- C++20 syntax checks passed for changed/dependent files.
- Linux CMake configure/build passed.
- CLI version check returned `Vestigant Spotlight v1.1.3`.
- Local self-test passed.

## Source: `V1_1_3_local_validation.log`

```text
# V1.1.3 Local Validation Log

Baseline reviewed:
- V1_1_2_build.log: Windows/MSVC build completed and reported Vestigant Spotlight v1.1.2.
- Upload_Thin_MacOS_AFF4_V1_1_2.zip: thin ZIP generated; denied raw upload filenames absent; case_summary raw_records=25000 raw_key_values=2181 raw_date_candidates=25000 artifact_count=25000.

Validation pass 1:
- g++ C++20 syntax check: src/app/app_runner.cpp PASS.
- g++ C++20 syntax check: src/gui/gui_export_worker.cpp PASS.
- g++ C++20 syntax check: src/parsers/apfs_volume_reader.cpp PASS.
- g++ C++20 syntax check: src/core/app_info.cpp PASS.

Validation pass 2:
- Linux CMake configure PASS.
- Linux CMake build PASS.
- CLI version check PASS: Vestigant Spotlight v1.1.3.
- Local self-test PASS.

Not validated:
- Windows/MSVC V1.1.3 build.
- Windows GUI runtime.
- GUI export cancellation on live large exports.
- V1.1.3 macOS AFF4/APFS thin run.
- Current iOS runtime parity.
```

## Source: `V1_1_2_validation_notes.md`

# V1.1.2 Validation Notes

## Baseline reviewed

- Source baseline: `VestigantSpotlightInv_V1_1_1.zip`.
- Build baseline: `V1_1_1_build.log`, which reported `Vestigant Spotlight v1.1.1`.
- Thin baseline: `Upload_Thin_MacOS_AFF4_V1_1_1.zip`; denied raw upload filenames were absent.
- Review prompt focus: ingest cancellation, dependent DLL loading hardening, GDI cleanup, dynamic-load probe modularization, APFS next-leaf work, bplist trailer parsing, native parser bulk PRAGMAs, and EvidenceIntake completion.

## Changes implemented

- Added `docs/WORKFLOW_LEDGER.md` as the first repeat-cycle state file.
- Added optional `std::atomic_bool` cancellation token to `runApplication`.
- Added GUI `Cancel Ingest` button and `gCancelIngestRequested` token.
- Added safe cancellation checkpoints around startup/source probing/staging/discovery/native parse/enrichment boundaries.
- Hardened AFF4 DLL loading path with `SetDefaultDllDirectories`, `AddDllDirectory`, and `LoadLibraryExW` using DLL/user/default search flags.
- Freed `gLogoBitmap` in `WM_DESTROY`.
- Applied temporary native Store-V2 bulk SQLite PRAGMAs around `NativeStoreDbParser::parseStores` and restored WAL/NORMAL settings after success/error.
- Added bounded bplist trailer validation metadata to existing bplist/NSKeyedArchiver context output.
- Updated continuation, roadmap, and suggestions/fixes tracking docs.

## Deferred

- Full dynamic-load probe extraction to `aff4_probe_worker.cpp`.
- Full `stageZipEvidenceSource(...)` relocation.
- Live APFS B-tree next-leaf traversal replacement.
- Live APFS absolute path reconstruction.
- Full NSKeyedArchiver UID graph decoding.

## Local validation performed

- C++20 syntax pass 1: `app_runner.cpp`, `native_storedb_parser.cpp`, `evidence_intake.cpp`, `apfs_aff4_reader.cpp`, `app_info.cpp`.
- C++20 syntax pass 2 with clang++: `app_runner.cpp`, `native_storedb_parser.cpp`, `evidence_intake.cpp`, `app_info.cpp`.
- Linux CMake configure/build completed.
- CLI version check returned `Vestigant Spotlight v1.1.2`.
- Local self-test passed.

## Not validated locally

- Windows/MSVC build.
- Windows GUI runtime.
- GUI Cancel Ingest behavior during long AFF4/iOS runs.
- V1.1.2 macOS AFF4/APFS thin run.
- Current iOS runtime parity.

## Source: `V1_1_2_local_validation.log`

```text
V1.1.2 local validation

Baseline files reviewed:
- /mnt/data/V1_1_1_build.log
- /mnt/data/Upload_Thin_MacOS_AFF4_V1_1_1.zip
- /mnt/data/Pasted markdown.md

Commands/results:
- g++ app_runner.cpp syntax: PASS
- g++ native_storedb_parser.cpp syntax: PASS
- g++ evidence_intake.cpp syntax: PASS
- g++ apfs_aff4_reader.cpp syntax: PASS
- g++ app_info.cpp syntax: PASS
- clang++ app_runner.cpp syntax: PASS
- clang++ native_storedb_parser.cpp syntax: PASS
- clang++ evidence_intake.cpp syntax: PASS
- clang++ app_info.cpp syntax: PASS
- cmake configure/build: PASS
- CLI version: Vestigant Spotlight v1.1.2
- self-test: PASS

Notes:
- Linux build emitted pre-existing unused-function/unused-parameter warnings in several files. No local compile failure was observed.
- Windows-only GUI and DLL-search changes require MSVC/runtime validation.
```

## Source: `V1_1_1_validation_notes.md`

# V1.1.1 Validation Notes

## Inputs reviewed first

- `/mnt/data/V1_1_0_1_build.log`
  - Windows/MSVC build completed and reported `Vestigant Spotlight v1.1.0.1`.
  - The only noted warning was the known `apfs_aff4_reader.cpp` unused callback parameter warning.
- `/mnt/data/Upload_Thin_MacOS_AFF4_V1_1_0_1.zip`
  - Thin ZIP was present.
  - Denied raw filenames were absent.
  - `case_summary.json` retained the macOS AFF4/APFS staged Store-V2 baseline: `raw_record_count=25000`, `raw_key_value_count=2181`, `raw_date_candidate_count=25000`, `artifact_count=25000`.

## Code changes validated locally

- Moved iOS inventory import mechanics into `EvidenceIntake::importIosInventoryCsvs(...)`.
- Moved referenced-path lookup import into `EvidenceIntake::importReferencedIosPathLookupFromReuseCache(...)`.
- Preserved app-runner run-status writing through callback injection.
- Replaced detached GUI main ingest worker with tracked `gIngestThread` and `WM_DESTROY` join.
- Suppressed the platform-specific AFF4 stream inventory callback warning without changing callback API.

## Checks performed

- C++20 syntax checks for changed/dependent files.
- Repeated syntax checks for selected changed/dependent files.
- Linux CMake configure.
- Linux CMake build.
- CLI version check: `Vestigant Spotlight v1.1.1`.
- Local self-test: passed.
- Static check: no `std::thread(worker).detach()` remains in `win32_gui.cpp`.

## Not validated here

- Windows/MSVC V1.1.1 build.
- Windows GUI runtime behavior.
- GUI close behavior during a long active ingest.
- V1.1.1 macOS AFF4/APFS thin run.
- Current iOS runtime after moving iOS inventory import into `EvidenceIntake`.

## Intentional deferrals

- Full `stageZipEvidenceSource(...)` relocation.
- Dynamic AFF4/APFS probe worker extraction.
- APFS live B-tree next-leaf traversal replacement.
- APFS live absolute path reconstruction.
- NSKeyedArchiver unflattened investigator output.
- Full Win32 GUI global-state object migration.

## Source: `V1_1_1_local_validation.log`

```text
V1.1.1 local validation
2026-06-06T17:56:44Z
Reviewed inputs:
- /mnt/data/V1_1_0_1_build.log
- /mnt/data/Upload_Thin_MacOS_AFF4_V1_1_0_1.zip
Pass 1 syntax checks:
CHECK src/ingest/evidence_intake.cpp
CHECK src/app/app_runner.cpp
CHECK src/parsers/apfs_aff4_reader.cpp
CHECK src/parsers/apfs_volume_reader.cpp
CHECK src/codec/lzfse_codec.cpp
CHECK src/parsers/ios_app_db_parser.cpp
CHECK src/parsers/apfs_diagnostic_exporter.cpp
CHECK src/gui/gui_export_worker.cpp
CHECK src/core/app_info.cpp
Pass 2 syntax checks:
RECHECK src/ingest/evidence_intake.cpp
RECHECK src/app/app_runner.cpp
RECHECK src/parsers/apfs_aff4_reader.cpp
Continuing validation after tool timeout
RECHECK src/gui/gui_export_worker.cpp
RECHECK src/core/app_info.cpp
CMake configure/build/self-test:
CMake configure PASS
CMake build PASS
Vestigant Spotlight v1.1.1
Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v1.1.1: "/tmp/v111_validation_selftest"
Static review:
Detached main ingest worker: none
src/app/app_runner.cpp:11577:            const std::size_t referencedHits = EvidenceIntake::importReferencedIosPathLookupFromReuseCache(db, caseDir, source.sourceId, log, opt.reuseIosCache,
src/ingest/evidence_intake.cpp:609:std::size_t EvidenceIntake::importReferencedIosPathLookupFromReuseCache(CaseDatabase& db,
Validation complete
```

## Source: `V1_1_10_validation_notes.md`

# V1.1.10 Validation Notes

## Base reviewed

- Source base: V1.1.9.1.
- User instruction: review all documentation and scripts; remove only unnecessary items; list ambiguous items before removal.
- Continuation instructions and baseline history files were reviewed before changes.

## Local changes

- Version metadata updated to 1.1.10.
- Current version PowerShell wrappers regenerated as V1_1_10.
- Obsolete active-package clutter removed: stale root-level V1.1.9 manifest/patch files and stale V1.1.9 source-review inventory files replaced by V1.1.10 review files.
- Ambiguous historical docs, validation notes, and support scripts were retained.

## Local validation performed

- Verified current version metadata occurrences for `1.1.10` / `V1_1_10`.
- Verified no remaining `scripts/*V1_1_9_1*` active wrapper filenames.
- Verified append-only version history files begin with V1_1_10 and retain V1_1_9_1 below it.
- Generated package manifest and SHA256 files.

## Required external validation

- Windows/MSVC V1.1.10 build.
- AFF4/APFS thin wrapper run is sufficient; full AFF4/APFS test is not required for this cleanup-only package.
- iOS validation is not required unless subsequent iOS code paths are changed.

## Source: `V1_1_10_local_validation.log`

```text
V1.1.10 local package cleanup validation
PASS: VERSION == 1.1.10
PASS: VERSION.txt == 1.1.10
PASS: src/core/app_info.cpp reports 1.1.10
PASS: CMakeLists.txt project version is 1.1.10
PASS: scripts/Build-V1_1_10.ps1 checks CLI version regex 1\.1\.10
PASS: no active scripts/*V1_1_9_1* wrapper filenames remain
PASS: VERSION_HISTORY.md begins with V1_1_10 and retains prior entries below
PASS: docs/BaselineVersionHistory.md begins with V1_1_10 and retains prior entries below
PASS: docs/FULL_VERSION_HISTORY.md begins with V1_1_10 and retains prior entries below
PASS: docs/SOURCE_DOCS_SCRIPTS_REVIEW_V1_1_10.md/csv exist
PASS: g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
NOTE: Full Windows/MSVC build not run in this Linux packaging environment.
```

## Source: `V1_1_10_1_validation_notes.md`

# V1.1.10.1 Validation Notes

## Scope

Documentation/script-command hotfix on V1.1.10.

## Local validation performed

- Confirmed current-version scripts exist for V1.1.10.1.
- Confirmed `VERSION`, `VERSION.txt`, `CMakeLists.txt`, and `src/core/app_info.cpp` report 1.1.10.1.
- Confirmed `docs/NEW_CHAT_CONTINUATION_GUIDE.md`, `BUILD_INSTRUCTIONS.md`, `docs/QUICK_START.md`, and `HELP.md` include the full requested extract/build command block.
- Confirmed macOS AFF4/APFS thin command is documented as `Run-V1_1_10_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut`.

## Not run here

- Windows/MSVC build was not run in this environment.
- AFF4/APFS thin run was not run in this environment.
- iOS run was not run because no iOS code changed.

## TEST SCOPE DECISION

- AFF4/APFS: thin only after Windows build.
- iOS: not required.
- Reason: documentation/script wrapper correction only; no extraction/parser/schema behavior changed.

## Source: `V1_1_0_validation_notes.md`

# V1.1.1 Validation Notes

## Inputs reviewed

- Uploaded `V1_0_31_build.log`: Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.31`.
- Uploaded `Upload_Thin_MacOS_AFF4_V1_0_31.zip`: generated successfully; denied raw thin-upload filenames were absent.
- Uploaded review markdown: requested database-lifetime cleanup, codec relocation, intake isolation, rerun-plan relocation, AFF4 stream inventory relocation, APFS path/leaf helpers, GUI state work, NSKeyedArchiver work, and NXSB parser relocation.

## Changes validated locally

- Single `CaseDatabase db` declaration remains in `src/app/app_runner.cpp`.
- Decmpfs/resource-fork reconstruction helpers are declared/implemented in `src/codec/lzfse_codec.*`.
- `parseApfsNxSuperblock()` is declared/implemented in `src/parsers/apfs_volume_reader.*`.
- `runAff4StreamInventory()` is declared/implemented in `src/parsers/apfs_aff4_reader.*`.
- `writeAff4ApfsV1DiagnosticRerunPlan()` is declared/implemented in `src/parsers/apfs_diagnostic_exporter.*`.

## Local checks

- C++20 syntax checks passed for changed and dependent files.
- Linux CMake configure/build completed.
- CLI version check reported `Vestigant Spotlight v1.1.1`.
- Local self-test passed.

## Not validated here

- Windows/MSVC build.
- Windows GUI runtime.
- Windows guarded AFF4 dynamic-load runtime.
- V1.1.1 macOS AFF4/APFS thin run.
- Current iOS run.

## Risk notes

- APFS path/leaf helper APIs are not wired into live evidence output.
- NSKeyedArchiver output was not added because a placeholder resolver would risk misleading interpretation.
- The AFF4/APFS dynamic load probe remains large; full worker extraction is still deferred until comparator evidence is available.

## Source: `V1_1_0_local_validation.log`

```text
V1.1.0 local validation log

Input review:
- Reviewed uploaded V1_0_31_build.log.
- Reviewed uploaded Upload_Thin_MacOS_AFF4_V1_0_31.zip.
- Reviewed uploaded roadmap/suggestion markdown.

Syntax pass 1:
- g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/codec/lzfse_codec.cpp : PASS
- g++ -std=c++20 -Isrc -fsyntax-only src/parsers/apfs_volume_reader.cpp : PASS
- g++ -std=c++20 -Isrc -fsyntax-only src/parsers/apfs_aff4_reader.cpp : PASS
- g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/apfs_diagnostic_exporter.cpp : PASS
- g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp : PASS

Syntax pass 2:
- Re-ran syntax checks for changed source files and dependent GUI/core/parser files: PASS

Build/self-test:
- cmake -S . -B build-cmake-validate : PASS
- cmake --build build-cmake-validate -j2 : PASS
- ./build-cmake-validate/VestigantSpotlightCli --version : Vestigant Spotlight v1.1.0
- ./build-cmake-validate/VestigantSpotlightTests build-cmake-validate/selftest_out : PASS

Packaging validation:
- Pending artifact creation after final documentation update.

Final packaging note:
- After documentation-only updates, a repeated full Linux CMake build was started and reached archive/link steps with warnings only before the execution environment timeout terminated it.
- No source compile error was emitted before timeout.
- The earlier full Linux CMake build/self-test on the same source state had passed before documentation edits.
- Final post-documentation C++20 syntax checks passed for changed/dependent source files.
```

## Source: `V1_1_0_1_validation_notes.md`

# V1.1.1 validation notes

## Reason

The V1.1.0 ZIP extracted successfully and SHA256 matched, but `scripts/Build-V1_1_0.ps1` failed because `build_windows_msvc.bat` was absent from the package root.

## Changes

- Restored root `build_windows_msvc.bat`.
- Restored root `build_windows_msvc_nocmake.bat`.
- Restored root `build_linux_test.sh`.
- Updated version metadata and wrappers to V1.1.1.

## Not changed

No parsing, APFS traversal, AFF4 reads, iOS processing, GUI behavior, Store-V2 parsing, or SQLite schema logic changed.

## Local checks

- Verified restored root build scripts exist in source tree.
- Verified version metadata points to 1.1.1.
- Linux CMake configure passed.
- Linux CMake build passed.
- CLI version check returned `Vestigant Spotlight v1.1.1`.
- Local self-test passed.

## Pending

- Windows/MSVC build.
- macOS AFF4/APFS thin run.

## Source: `V1_1_0_1_selftest.log`

```text
Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v1.1.1: "build-test/selftest_out"
```

## Source: `V1_1_0_1_local_validation.log`

```text
V1.1.1 local validation log

ROOT_SCRIPT_PRESENT build_windows_msvc.bat: TRUE
ROOT_SCRIPT_PRESENT build_windows_msvc_nocmake.bat: TRUE
ROOT_SCRIPT_PRESENT build_linux_test.sh: TRUE
VERSION: 1.1.1
APP_INFO: True
V1_1_1_cmake_configure.log: exists=True size=719
V1_1_1_cmake_build.log: exists=True size=8484
V1_1_1_cli_version.log: exists=True size=29
V1_1_1_selftest.log: exists=True size=101
CLI_VERSION_OUTPUT: Vestigant Spotlight v1.1.1
SELFTEST_TAIL: Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v1.1.1: "build-test/selftest_out"
```

## Source: `V1_1_0_1_cmake_configure.log`

```text
-- The C compiler identification is GNU 14.2.0
-- The CXX compiler identification is GNU 14.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Apple lzfse source detected; enabling VESTIGANT_HAS_LZFSE
-- Configuring done (1.7s)
-- Generating done (0.0s)
-- Build files have been written to: /mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/build-test
```

## Source: `V1_1_0_1_cmake_build.log`

```text
[  3%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/csv.cpp.o
[  6%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/app_info.cpp.o
[ 10%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/hash.cpp.o
[ 13%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/logger.cpp.o
[ 17%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/path_utils.cpp.o
[ 20%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/db/case_db.cpp.o
[ 24%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/source_profiles.cpp.o
[ 27%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/store_discovery.cpp.o
[ 31%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/evidence_preservation.cpp.o
[ 34%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/evidence_intake.cpp.o
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/ingest/evidence_preservation.cpp:57:13: warning: 'std::string vestigant::spotlight::{anonymous}::quoteCmd(const std::filesystem::__cxx11::path&)' defined but not used [-Wunused-function]
   57 | std::string quoteCmd(const fs::path& p) {
      |             ^~~~~~~~
[ 37%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/native_storedb_parser.cpp.o
[ 41%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/ios_app_db_parser.cpp.o
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/parsers/native_storedb_parser.cpp:2050:25: warning: 'std::vector<vestigant::spotlight::{anonymous}::ParsedItem> vestigant::spotlight::{anonymous}::parseMetadataItems(const std::vector<unsigned char>&, int, const std::map<int, PropertyDef>&, const std::map<int, std::__cxx11::basic_string<char> >&, const std::map<int, std::vector<int> >&, const std::map<int, std::vector<int> >&)' defined but not used [-Wunused-function]
 2050 | std::vector<ParsedItem> parseMetadataItems(const std::vector<std::uint8_t>& payload,
      |                         ^~~~~~~~~~~~~~~~~~
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/parsers/native_storedb_parser.cpp:1676:6: warning: 'void vestigant::spotlight::{anonymous}::addCoreProbeMetadata(ParsedItem&, const std::vector<unsigned char>&)' defined but not used [-Wunused-function]
 1676 | void addCoreProbeMetadata(ParsedItem& item, const std::vector<std::uint8_t>& data) {
      |      ^~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/parsers/native_storedb_parser.cpp:1141:6: warning: 'bool vestigant::spotlight::{anonymous}::isHighValueNativeFieldName(const std::string&)' defined but not used [-Wunused-function]
 1141 | bool isHighValueNativeFieldName(const std::string& fieldLower) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~
[ 44%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_volume_reader.cpp.o
[ 48%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_aff4_reader.cpp.o
[ 51%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_diagnostic_exporter.cpp.o
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/parsers/apfs_aff4_reader.cpp: In function 'vestigant::spotlight::Aff4StreamInventoryResult vestigant::spotlight::runAff4StreamInventory(const RunOptions&, const EvidenceSource&, const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, Logger&, Aff4ToolResolver, Aff4ExecutableRunner, Aff4ShellCommandRunner)':
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/parsers/apfs_aff4_reader.cpp:297:71: warning: unused parameter 'executableRunner' [-Wunused-parameter]
  297 |                                                  Aff4ExecutableRunner executableRunner,
      |                                                  ~~~~~~~~~~~~~~~~~~~~~^~~~~~~~~~~~~~~~
[ 55%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/codec/lzfse_codec.cpp.o
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/parsers/apfs_diagnostic_exporter.cpp:1306:6: warning: 'bool vestigant::spotlight::{anonymous}::isLikelyStoreV2GroupDirectoryName(const std::string&)' defined but not used [-Wunused-function]
 1306 | bool isLikelyStoreV2GroupDirectoryName(const std::string& name) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[ 58%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/enrich_sql/sqlite_enrichment.cpp.o
[ 62%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/export_sql/sqlite_exporter.cpp.o
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/enrich_sql/sqlite_enrichment.cpp:16:13: warning: 'std::string vestigant::spotlight::{anonymous}::stripLeadingSlash(std::string)' defined but not used [-Wunused-function]
   16 | std::string stripLeadingSlash(std::string s) {
      |             ^~~~~~~~~~~~~~~~~
[ 65%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/app/case_store.cpp.o
[ 68%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/app/app_runner.cpp.o
[ 72%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_decode.c.o
[ 75%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_decode_base.c.o
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/app/app_runner.cpp: In function 'std::filesystem::__cxx11::path vestigant::spotlight::{anonymous}::stageZipEvidenceSource(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, vestigant::spotlight::SourceProfileKind, vestigant::spotlight::Logger&, bool*)':
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/app/app_runner.cpp:3228:101: warning: unused parameter 'profile' [-Wunused-parameter]
 3228 | fs::path stageZipEvidenceSource(const fs::path& zipPath, const fs::path& caseDir, SourceProfileKind profile, Logger& log, bool* iosFocusedUsed = nullptr) {
      |                                                                                   ~~~~~~~~~~~~~~~~~~^~~~~~~
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/app/app_runner.cpp:3228:129: warning: unused parameter 'iosFocusedUsed' [-Wunused-parameter]
 3228 | fs::path stageZipEvidenceSource(const fs::path& zipPath, const fs::path& caseDir, SourceProfileKind profile, Logger& log, bool* iosFocusedUsed = nullptr) {
      |                                                                                                                           ~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~
[ 79%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_fse.c.o
[ 82%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzvn_decode_base.c.o
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/app/app_runner.cpp: At global scope:
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/app/app_runner.cpp:3143:28: warning: 'vestigant::spotlight::{anonymous}::IosZipInventoryParseResult vestigant::spotlight::{anonymous}::parseIosSevenZipRawInventoryToCsv(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, vestigant::spotlight::Logger&)' defined but not used [-Wunused-function]
 3143 | IosZipInventoryParseResult parseIosSevenZipRawInventoryToCsv(const fs::path& caseDir, const fs::path& zipPath, Logger& log) {
      |                            ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/work_v1101/VestigantSpotlightInv_V1_1_1/src/app/app_runner.cpp:2508:10: warning: 'std::filesystem::__cxx11::path vestigant::spotlight::{anonymous}::writeIosFocusedZipExtractorScript(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, bool)' defined but not used [-Wunused-function]
 2508 | fs::path writeIosFocusedZipExtractorScript(const fs::path& caseDir, const fs::path& zipPath, const fs::path& stageRoot, const fs::path& inventoryPath, bool throwOnNoMatch) {
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[ 86%] Linking CXX static library libvestigant_spotlight_core.a
[ 86%] Built target vestigant_spotlight_core
[ 93%] Building CXX object CMakeFiles/VestigantSpotlightTests.dir/tests/main.cpp.o
[ 93%] Building CXX object CMakeFiles/VestigantSpotlightCli.dir/src/cli/main.cpp.o
[ 96%] Linking CXX executable VestigantSpotlightCli
[100%] Linking CXX executable VestigantSpotlightTests
[100%] Built target VestigantSpotlightTests
[100%] Built target VestigantSpotlightCli
```

## Source: `V1_1_0_1_cli_version.log`

```text
Vestigant Spotlight v1.1.1
```

## Source: `V1_0_7_validation_summary.txt`

```text
V1.0.7 validation summary

Implemented a dedicated APFS module boundary and fixed direct AFF4/APFS copied-file status classification.

Validation completed in packaging environment:
- CMake configure: PASS.
- New APFS module syntax check: PASS.
- Test entry-point syntax check with APFS module smoke tests: PASS.
- Linux build progressed through all non-app_runner common objects and timed out while compiling the existing very large app_runner.cpp; no app_runner compile error was observed before timeout.
- ZIP/patch/SHA256 packaging: PASS.

Windows/MSVC validation required after package download:
- Build-V1_0_7.ps1 must compile app_runner.cpp, the new apfs_volume_reader.cpp, CLI, tests, and GUI.
- Run-V1_0_7-macOS-AFF4-Probe-AndZip.ps1 should preserve the V1.0.6 extraction behavior while correcting copied/staged file counts in AFF4/APFS summaries.
```

## Source: `V1_0_6_linux_validation_summary.txt`

```text
Vestigant Spotlight V1.0.6 validation summary

Environment: Linux container build/test validation only.

Checks performed:
- cmake --build build-linux -j2: PASS
- ./build-linux/VestigantSpotlightCli --version: Vestigant Spotlight v1.0.6
- ./build-linux/VestigantSpotlightTests build-linux/selftest_out: PASS, schema smoke test passed
- Build-V1_0_6.ps1 version gate: confirmed expects 1.0.6
- Removed app-runner self-test routing/fake evidence path: grep found no /Users/alice, selfTest, or self-test route in src/app/app_runner.cpp/scripts/tests
- V0.9 run/package/collect scripts: none present in clean package
- V0.9 validation summaries and docs/codex_notes/CHANGES_Codex_*.md: none present in clean package
- AFF4/APFS logical directory walk outputs are listed in app output manifest, source-probe upload copy list, and single-AFF4 wrapper expected ZIP outputs

Not verified:
- Windows/MSVC V1.0.6 build
- Win32 GUI runtime
- Live AFF4/APFS run against O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4
- External Store-V2 comparison after V1.0.6 copy-out changes
```

## Source: `V1_0_4_syntax_check.log`

```text
PASS: c++ -std=c++17 -I src -fsyntax-only src/app/app_runner.cpp
PASS: c++ -std=c++17 -I src -fsyntax-only src/core/app_info.cpp
NOTE: Full Linux CMake build was not completed in this environment because app_runner.cpp compilation exceeded the execution timeout after code-size growth. Windows/MSVC build must be validated by user.
```

## Source: `V1_0_31_validation_notes.md`

# V1.0.31 Validation Notes

## Reviewed inputs

- `V1_0_30_build.log`: Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.30`.
- `Upload_Thin_MacOS_AFF4_V1_0_30.zip`: thin ZIP generated; denied raw filenames were absent; AFF4/APFS Store-V2 staged baseline remained stable.

## Local validation performed

Two C++20 syntax-check passes were run for:

- `src/ingest/evidence_intake.cpp`
- `src/app/app_runner.cpp`
- `src/parsers/ios_app_db_parser.cpp`
- `src/parsers/apfs_diagnostic_exporter.cpp`
- `src/gui/gui_export_worker.cpp`
- `src/core/app_info.cpp`

Linux CMake configure and full build completed.

Local CLI version check returned `Vestigant Spotlight v1.0.31`.

Local self-test passed.

## Not validated here

- Windows/MSVC V1.0.31 build.
- Windows GUI runtime.
- Windows GUI search behavior.
- V1.0.31 macOS AFF4/APFS thin run.
- Current iOS run to validate CSV fallback import PRAGMAs and intake-helper module output parity.

## Source: `V1_0_31_local_validation.log`

```text
PASS1 syntax src/ingest/evidence_intake.cpp
PASS2 syntax src/ingest/evidence_intake.cpp
PASS1 syntax src/app/app_runner.cpp
PASS2 syntax src/app/app_runner.cpp
PASS1 syntax src/parsers/ios_app_db_parser.cpp
PASS2 syntax src/parsers/ios_app_db_parser.cpp
PASS1 syntax src/parsers/apfs_diagnostic_exporter.cpp
PASS2 syntax src/parsers/apfs_diagnostic_exporter.cpp
PASS1 syntax src/gui/gui_export_worker.cpp
PASS2 syntax src/gui/gui_export_worker.cpp
PASS1 syntax src/core/app_info.cpp
PASS2 syntax src/core/app_info.cpp
-- The C compiler identification is GNU 14.2.0
-- The CXX compiler identification is GNU 14.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Apple lzfse source detected; enabling VESTIGANT_HAS_LZFSE
-- Configuring done (1.1s)
-- Generating done (0.0s)
-- Build files have been written to: /mnt/data/VestigantSpotlightInv_V1_0_31/build-cmake-validate
[  6%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/csv.cpp.o
[  6%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/app_info.cpp.o
[ 10%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/hash.cpp.o
[ 13%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/logger.cpp.o
[ 17%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/core/path_utils.cpp.o
[ 20%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/db/case_db.cpp.o
[ 24%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/source_profiles.cpp.o
[ 27%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/store_discovery.cpp.o
[ 31%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/evidence_preservation.cpp.o
[ 34%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/ingest/evidence_intake.cpp.o
/mnt/data/VestigantSpotlightInv_V1_0_31/src/ingest/evidence_preservation.cpp:57:13: warning: 'std::string vestigant::spotlight::{anonymous}::quoteCmd(const std::filesystem::__cxx11::path&)' defined but not used [-Wunused-function]
   57 | std::string quoteCmd(const fs::path& p) {
      |             ^~~~~~~~
[ 37%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/native_storedb_parser.cpp.o
[ 41%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/ios_app_db_parser.cpp.o
/mnt/data/VestigantSpotlightInv_V1_0_31/src/parsers/native_storedb_parser.cpp:2050:25: warning: 'std::vector<vestigant::spotlight::{anonymous}::ParsedItem> vestigant::spotlight::{anonymous}::parseMetadataItems(const std::vector<unsigned char>&, int, const std::map<int, PropertyDef>&, const std::map<int, std::__cxx11::basic_string<char> >&, const std::map<int, std::vector<int> >&, const std::map<int, std::vector<int> >&)' defined but not used [-Wunused-function]
 2050 | std::vector<ParsedItem> parseMetadataItems(const std::vector<std::uint8_t>& payload,
      |                         ^~~~~~~~~~~~~~~~~~
/mnt/data/VestigantSpotlightInv_V1_0_31/src/parsers/native_storedb_parser.cpp:1676:6: warning: 'void vestigant::spotlight::{anonymous}::addCoreProbeMetadata(ParsedItem&, const std::vector<unsigned char>&)' defined but not used [-Wunused-function]
 1676 | void addCoreProbeMetadata(ParsedItem& item, const std::vector<std::uint8_t>& data) {
      |      ^~~~~~~~~~~~~~~~~~~~
/mnt/data/VestigantSpotlightInv_V1_0_31/src/parsers/native_storedb_parser.cpp:1141:6: warning: 'bool vestigant::spotlight::{anonymous}::isHighValueNativeFieldName(const std::string&)' defined but not used [-Wunused-function]
 1141 | bool isHighValueNativeFieldName(const std::string& fieldLower) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~
[ 44%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_volume_reader.cpp.o
[ 48%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_aff4_reader.cpp.o
[ 51%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/parsers/apfs_diagnostic_exporter.cpp.o
[ 55%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/codec/lzfse_codec.cpp.o
/mnt/data/VestigantSpotlightInv_V1_0_31/src/parsers/apfs_diagnostic_exporter.cpp:1306:6: warning: 'bool vestigant::spotlight::{anonymous}::isLikelyStoreV2GroupDirectoryName(const std::string&)' defined but not used [-Wunused-function]
 1306 | bool isLikelyStoreV2GroupDirectoryName(const std::string& name) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[ 58%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/enrich_sql/sqlite_enrichment.cpp.o
/mnt/data/VestigantSpotlightInv_V1_0_31/src/enrich_sql/sqlite_enrichment.cpp:16:13: warning: 'std::string vestigant::spotlight::{anonymous}::stripLeadingSlash(std::string)' defined but not used [-Wunused-function]
   16 | std::string stripLeadingSlash(std::string s) {
      |             ^~~~~~~~~~~~~~~~~
[ 62%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/export_sql/sqlite_exporter.cpp.o
[ 65%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/app/case_store.cpp.o
[ 68%] Building CXX object CMakeFiles/vestigant_spotlight_core.dir/src/app/app_runner.cpp.o
/mnt/data/VestigantSpotlightInv_V1_0_31/src/app/app_runner.cpp: In function 'std::filesystem::__cxx11::path vestigant::spotlight::{anonymous}::stageZipEvidenceSource(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, vestigant::spotlight::SourceProfileKind, vestigant::spotlight::Logger&, bool*)':
/mnt/data/VestigantSpotlightInv_V1_0_31/src/app/app_runner.cpp:3430:101: warning: unused parameter 'profile' [-Wunused-parameter]
 3430 | fs::path stageZipEvidenceSource(const fs::path& zipPath, const fs::path& caseDir, SourceProfileKind profile, Logger& log, bool* iosFocusedUsed = nullptr) {
      |                                                                                   ~~~~~~~~~~~~~~~~~~^~~~~~~
/mnt/data/VestigantSpotlightInv_V1_0_31/src/app/app_runner.cpp:3430:129: warning: unused parameter 'iosFocusedUsed' [-Wunused-parameter]
 3430 | fs::path stageZipEvidenceSource(const fs::path& zipPath, const fs::path& caseDir, SourceProfileKind profile, Logger& log, bool* iosFocusedUsed = nullptr) {
      |                                                                                                                           ~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~
[ 72%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_decode.c.o
[ 75%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_decode_base.c.o
[ 79%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzfse_fse.c.o
[ 82%] Building C object CMakeFiles/vestigant_spotlight_core.dir/third_party/lzfse/src/lzvn_decode_base.c.o
/mnt/data/VestigantSpotlightInv_V1_0_31/src/app/app_runner.cpp: At global scope:
/mnt/data/VestigantSpotlightInv_V1_0_31/src/app/app_runner.cpp:3345:28: warning: 'vestigant::spotlight::{anonymous}::IosZipInventoryParseResult vestigant::spotlight::{anonymous}::parseIosSevenZipRawInventoryToCsv(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, vestigant::spotlight::Logger&)' defined but not used [-Wunused-function]
 3345 | IosZipInventoryParseResult parseIosSevenZipRawInventoryToCsv(const fs::path& caseDir, const fs::path& zipPath, Logger& log) {
      |                            ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/mnt/data/VestigantSpotlightInv_V1_0_31/src/app/app_runner.cpp:2710:10: warning: 'std::filesystem::__cxx11::path vestigant::spotlight::{anonymous}::writeIosFocusedZipExtractorScript(const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, const std::filesystem::__cxx11::path&, bool)' defined but not used [-Wunused-function]
 2710 | fs::path writeIosFocusedZipExtractorScript(const fs::path& caseDir, const fs::path& zipPath, const fs::path& stageRoot, const fs::path& inventoryPath, bool throwOnNoMatch) {
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[ 86%] Linking CXX static library libvestigant_spotlight_core.a
[ 86%] Built target vestigant_spotlight_core
[ 89%] Building CXX object CMakeFiles/VestigantSpotlightTests.dir/tests/main.cpp.o
[ 93%] Building CXX object CMakeFiles/VestigantSpotlightCli.dir/src/cli/main.cpp.o
[ 96%] Linking CXX executable VestigantSpotlightCli
[100%] Linking CXX executable VestigantSpotlightTests
[100%] Built target VestigantSpotlightTests
[100%] Built target VestigantSpotlightCli
Vestigant Spotlight v1.0.31
Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v1.0.31: "/mnt/data/VestigantSpotlightInv_V1_0_31/build-cmake-validate/selftest_out"
```

## Source: `V1_0_30_validation_notes.md`

# V1.0.31 Validation Notes

## Base reviewed

- Base source: `VestigantSpotlightInv_V1_0_29.zip`
- Uploaded build log: `V1_0_29_build.log`
- Uploaded macOS AFF4/APFS thin ZIP: `Upload_Thin_MacOS_AFF4_V1_0_29.zip`

## Uploaded V1.0.29 findings

- Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.29`.
- macOS AFF4/APFS thin ZIP was created and reviewed.
- Denied raw thin-upload filenames were absent:
  - `aff4_stream_inventory_raw.txt`
  - `ios_focused_zip_extract.log`
  - `ios_focused_zip_extract_7z.log`
  - `ios_focused_zip_extract.ps1`
  - `ios_ffs_file_inventory.csv`
  - `image_file_inventory.csv`
- AFF4/APFS staged Store-V2 baseline remained stable:
  - `raw_record_count=25000`
  - `raw_key_value_count=2181`
  - `raw_date_candidate_count=25000`
  - `artifact_count=25000`
  - external file count `4123`
  - Vestigant staged file count `8986`
  - external-only rows `1424`
  - Vestigant-only rows `6710`
  - hash-different-path rows `431`

## V1.0.31 change scope

Implemented:

1. Moved iOS app database record-inventory orchestration into `src/parsers/ios_app_db_parser.cpp`.
2. Added `IosAppDbParser::parseRecordInventories(...)` with a status-writer callback to preserve run-status behavior.
3. Reduced `app_runner.cpp::parseIosAppDatabaseRecordInventories(...)` to a delegating wrapper.
4. Added GUI export thread registration for Export Page, Export Filtered, Export Checked, and Export Tagged.
5. Removed `.detach()` from those four GUI export workers and joined registered export threads during `WM_DESTROY`.
6. Updated continuation, roadmap, suggestions/fixes tracker, versioned scripts, and release/history notes.

Not changed:

- AFF4/APFS traversal.
- APFS copy-out or staging.
- Store-V2 parser behavior.
- SQLite schema.
- iOS CoreSpotlight parser schema or interpretation.
- APFS reverse path reconstruction.
- NSKeyedArchiver/bplist interpretation.
- `runApplication()` database lifetime.

## Local validation pass 1

- `src/parsers/ios_app_db_parser.cpp`: C++20 syntax pass after resolving local `toLower` / path-utils name collision.
- `src/app/app_runner.cpp`: C++20 syntax pass.
- `src/gui/gui_export_worker.cpp`: C++20 syntax pass.
- `src/core/app_info.cpp`: C++20 syntax pass.

`src/gui/win32_gui.cpp` cannot be syntax-checked in this Linux container because it is explicitly Windows-only and includes Win32 headers.

## Local validation pass 2

- `src/parsers/ios_app_db_parser.cpp`: C++20 syntax pass.
- `src/app/app_runner.cpp`: C++20 syntax pass.
- `src/parsers/apfs_diagnostic_exporter.cpp`: C++20 syntax pass.
- `src/gui/gui_export_worker.cpp`: C++20 syntax pass.
- `src/core/app_info.cpp`: C++20 syntax pass.
- CMake configure pass.
- Static review pass confirmed:
  - `Build-V1_0_31.ps1` expects `1.0.31`.
  - app runner delegates iOS DB record inventory to parser module.
  - old app-runner `sqliteScalarCount`/`sqliteColumnList` helpers were removed.
  - parser module owns `IosAppDbParser::parseRecordInventories(...)`.
  - GUI export thread registry is present.
  - four GUI export workers no longer call `.detach()` directly.

## Validation still required

- Windows/MSVC V1.0.31 build.
- Windows GUI runtime test.
- GUI export close/shutdown behavior during and after large exports.
- V1.0.31 macOS AFF4/APFS thin run.
- Current iOS parser run to confirm iOS module delegation preserves counts.

## Source: `V1_0_30_local_validation.log`

```text
PASS 1 syntax checks
src/parsers/ios_app_db_parser.cpp: In function 'std::size_t vestigant::spotlight::iosAppDbParseTable(const std::string&, const IosAppDbInventory&, sqlite3*, const std::string&, const IosAppDbTableParseDecision&, SqlStatement&)':
src/parsers/ios_app_db_parser.cpp:907:47: error: call of overloaded 'toLower(const std::string&)' is ambiguous
  907 |         const std::string lowerTable = toLower(table);
      |                                        ~~~~~~~^~~~~~~
In file included from src/parsers/ios_app_db_parser.cpp:2:
src/core/path_utils.h:9:13: note: candidate: 'std::string vestigant::spotlight::toLower(std::string)'
    9 | std::string toLower(std::string s);
      |             ^~~~~~~
src/parsers/ios_app_db_parser.cpp:17:13: note: candidate: 'std::string vestigant::spotlight::{anonymous}::toLower(std::string)'
   17 | std::string toLower(std::string s) {
      |             ^~~~~~~
ios_parser:1
app_runner:0
gui_export_worker:0
app_info:0
PASS 1 retry
ios_parser:0
app_runner:0
src/gui/win32_gui.cpp:2:2: error: #error This file is Windows-only.
    2 | #error This file is Windows-only.
      |  ^~~~~
src/gui/win32_gui.cpp:15:10: fatal error: windows.h: No such file or directory
   15 | #include <windows.h>
      |          ^~~~~~~~~~~
compilation terminated.
win32_gui:1
gui_export_worker:0
PASS 2 validation
ios_app_db_parser syntax PASS
app_runner syntax PASS
apfs_exporter syntax PASS
gui_export_worker syntax PASS
app_info syntax PASS
cmake configure PASS
PASS 2 static review
PASS build script expects 1.0.31
PASS app runner delegates iOS inventory to parser
PASS old app runner sqliteScalarCount helper removed
PASS parser owns record inventory method
PASS gui export thread registry present
PASS export workers no longer use direct detach in four export functions
```

## Source: `V1_0_29_validation_notes.md`

# V1.0.29 Validation Notes

## Base reviewed

- Base source: `VestigantSpotlightInv_V1_0_28_2.zip`
- Uploaded build log: `V1_0_28_2_build.log`
- Uploaded thin ZIP: `Upload_Thin_MacOS_AFF4_V1_0_28_2.zip`

## Observations

- V1.0.28.2 binaries linked and reported `Vestigant Spotlight v1.0.28.2`.
- The V1.0.28.2 PowerShell build wrapper failed after build because it still checked for `1.0.27`.
- The uploaded V1.0.28.2 thin ZIP existed and did not contain the denied raw filenames checked during review.

## Implemented in V1.0.29

- Corrected versioned scripts for V1.0.29.
- Closed parent-side redirected process log handle immediately after successful `CreateProcessW`.
- Replaced process-wide `SetDllDirectoryW`/`LoadLibraryW` with `LoadLibraryExW` secure search flags.
- Added `WM_SETREDRAW` suspension around Win32 review ListView bulk population.
- Added 50 MB size cap for dynamically globbed thin-upload export CSVs in C++.
- Added corresponding export CSV size cap in standalone PowerShell thin-upload helper.
- Updated continuity and tracker files.

## Local validation

Passed:

```text
g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp
g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/apfs_diagnostic_exporter.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
cmake -S . -B build-cmake-validate
```

Static checks passed for:

- V1.0.29 build-wrapper version regex.
- Absence of `SetDllDirectoryW`.
- Presence of `LoadLibraryExW`.
- Parent log-handle closure after child creation.
- `WM_SETREDRAW` review-list guard.
- C++ and PowerShell thin-upload export CSV size caps.
- Existing `sqliteColumnText` null guard.
- Existing decmpfs 512 MB expected-size cap.

## Not validated here

- Windows/MSVC full build.
- Windows GUI runtime.
- Windows AFF4 dynamic-load runtime behavior.
- V1.0.29 macOS AFF4/APFS thin run.
- V1.0.29 iOS run.

## Source: `V1_0_29_local_validation.log`

```text
$ g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp
returncode=0
STDOUT:

STDERR:



$ g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/apfs_diagnostic_exporter.cpp
returncode=0
STDOUT:

STDERR:



$ g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp
returncode=0
STDOUT:

STDERR:



$ g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
returncode=0
STDOUT:

STDERR:



$ cmake -S . -B build-cmake-validate
returncode=0
STDOUT:
-- The C compiler identification is GNU 14.2.0
-- The CXX compiler identification is GNU 14.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Apple lzfse source detected; enabling VESTIGANT_HAS_LZFSE
-- Configuring done (1.3s)
-- Generating done (0.0s)
-- Build files have been written to: /mnt/data/VestigantSpotlightInv_V1_0_29/build-cmake-validate

STDERR:



Static checks:
PASS: Build script expects 1.0.29 regex
PASS: No SetDllDirectoryW remains in app_runner.cpp
PASS: LoadLibraryExW used in app_runner.cpp
PASS: runProcessNoWindowRedirected closes parent log handle before wait
PASS: WM_SETREDRAW used in win32_gui.cpp
PASS: C++ thin upload 50 MB cap present
PASS: PowerShell thin upload 50 MB cap present
PASS: sqliteColumnText nullptr guard present
PASS: decmpfs 512 MB cap present
```

## Source: `V1_0_27_validation_notes.md`

# V1.0.27 Validation Notes

## Inputs reviewed

- `V1_0_26_1_build.log`: Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26.1`.
- `Upload_Thin_MacOS_AFF4_V1_0_26_1.zip`: generated successfully and reviewed.
- `Pasted markdown (4).md`: reviewed suggestions for Job Objects, APFS path reconstruction, evidence intake modularization, NSKeyedArchiver parsing, and SQLite busy retry handling.

## Thin ZIP review

- Denied raw filenames were not found in the ZIP.
- `case_file_inventory.txt` and `additional_output_file_inventory.txt` did not contain full `Q:\`, `D:\`, or `T:\` absolute path prefixes.
- Run reached `complete_source_probe`.
- `case_summary.json`: `raw_record_count=25000`, `raw_key_value_count=2181`, `raw_date_candidate_count=25000`, `artifact_count=25000`.
- External compare summary: `external_file_count=4123`, `vestigant_file_count=8986`, `file_match_rows=2213`, `external_only_rows=1424`, `vestigant_only_rows=6710`, `hash_different_path_rows=431`.

## Local checks

Passed:

```text
g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
```

Not validated locally:

- Windows/MSVC full build.
- Windows Job Object runtime behavior.
- Windows PowerShell thin ZIP self-check execution.
- Windows GUI runtime and transient lock behavior.

## Source: `V1_0_27_local_validation.log`

```text
V1.0.27 local validation

Passed local syntax checks:
- app_runner.cpp
- gui_export_worker.cpp
- app_info.cpp

See /mnt/data/v1027_validation/*.log for raw outputs.
```

## Source: `V1_0_27_gui_export_worker_syntax.log`

```text

```

## Source: `V1_0_27_app_runner_syntax.log`

```text

```

## Source: `V1_0_27_app_info_syntax.log`

```text

```

## Source: `V1_0_26_validation_notes.md`

# V1.0.26 Validation Notes

## Inputs reviewed

- `VestigantSpotlightInv_V1_0_25.zip`
- `V1_0_25_build.log`
- `Upload_Thin_MacOS_AFF4_V1_0_25.zip`
- User review file `Pasted markdown.md`

## Build-log review

The uploaded V1.0.25 MSVC build log completed successfully and reported `Vestigant Spotlight v1.0.25`. No `error` or `fatal` lines were found in the decoded build log.

## Thin-output review

The uploaded V1.0.25 macOS AFF4/APFS thin output reached `complete_source_probe`. The case summary retained the staged Store-V2 parser baseline:

- `raw_record_count=25000`
- `raw_key_value_count=2181`
- `raw_date_candidate_count=25000`
- `artifact_count=25000`

The thin ZIP still included `aff4_stream_inventory_raw.txt`, which means V1.0.25 did not fully close the raw-log thin-upload leak in the standalone upload-tool path. V1.0.26 addresses that path.

## Local validation performed

- Updated version fields to `1.0.26`.
- Ran C++20 syntax check for `src/app/app_runner.cpp` with `VESTIGANT_HAS_LZFSE=1`.
- Ran C++20 syntax checks for:
  - `src/core/app_info.cpp`
  - `src/gui/gui_view_helpers.cpp`
  - `src/gui/gui_export_worker.cpp`
- Checked V1.0.26 build/launch/AFF4 wrapper scripts for current version references.

## Not validated locally

- Windows/MSVC full build.
- Windows GUI runtime.
- V1.0.26 macOS AFF4/APFS thin run.
- V1.0.26 iOS CoreSpotlight thin run.

## Source: `V1_0_26_patch_manifest.txt`

```text
V1.0.26 patch file manifest

Modified:
- CMakeLists.txt
- CONSOLIDATED_VERSION_HISTORY.md
- RELEASE_NOTES.md
- VERSION
- VERSION.txt
- VERSION_HISTORY.md
- docs/BUILD_NOTES.md
- docs/CONSOLIDATED_VERSION_HISTORY.md
- docs/PROJECT_ROADMAP_AND_CONTINUATION.md
- docs/VALIDATION_HISTORY.md
- docs/VALIDATION_STATUS.md
- scripts/Initialize-GitHubRepo.ps1
- scripts/New-ReleaseBranch.ps1
- scripts/Sync-Version-To-GitRepo.ps1
- src/app/app_runner.cpp
- src/core/app_info.cpp
- tools/Create-SourceProbeUploadZip.ps1

Added:
- V1_0_26_DELETED_FILES_MANIFEST.md
- docs/V1_0_26_THIN_UPLOAD_AND_IO_HARDENING.md
- scripts/Build-V1_0_26.ps1
- scripts/Launch-V1_0_26-GUI.ps1
- scripts/Run-V1_0_26-macOS-AFF4-Probe-AndZip.ps1
- scripts/Run-V1_0_26-macOS-AFF4-Probe-AndZip.txt
- validation/V1_0_26_app_runner_syntax.log
- validation/V1_0_26_local_validation.log
- validation/V1_0_26_patch_manifest.txt
- validation/V1_0_26_validation_notes.md

Deleted/renamed:
- scripts/Build-V1_0_25.ps1
- scripts/Launch-V1_0_25-GUI.ps1
- scripts/Run-V1_0_25-macOS-AFF4-Probe-AndZip.ps1
- scripts/Run-V1_0_25-macOS-AFF4-Probe-AndZip.txt

Renames are documented in V1_0_26_DELETED_FILES_MANIFEST.md.
```

## Source: `V1_0_26_local_validation.log`

```text
V1.0.26 local validation

Validated source changes:
- VERSION and VERSION.txt set to 1.0.26.
- CMakeLists.txt project version set to 1.0.26.
- src/core/app_info.cpp returns 1.0.26.
- Build/launch/AFF4 wrapper scripts updated to V1_0_26 paths and version checks.

Commands run:
- g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp
- g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
- g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_view_helpers.cpp
- g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp

Result:
- All listed syntax checks completed successfully in the Linux validation environment.
- Windows/MSVC build remains required.
```

## Source: `V1_0_26_app_runner_syntax.log`

```text

```

## Source: `V1_0_26_1_validation_notes.md`

# V1.0.27 Validation Notes

## Baseline reviewed

The uploaded V1.0.26 build log shows the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.

The user-reported AFF4/APFS wrapper output shows the V1.0.26 probe completed through external comparison and remaining-mismatch diagnostic generation, then failed during thin-upload packaging with:

```text
Get-RelativePathForThinInventory : Cannot convert value "\\" to type "System.Char".
```

## Root cause

`tools/Create-SourceProbeUploadZip.ps1` used `[char]'\\'` in `TrimStart()`. Windows PowerShell treats the escaped backslash text as two characters and cannot cast it to `System.Char`.

## Fix implemented

- Replaced relative-path trimming with a `System.Uri.MakeRelativeUri` implementation compatible with Windows PowerShell 5.1.
- Reused the helper for `ExtractedSpotlight` upload-relative paths.
- Changed reader-tools inventory output to relative paths.
- Added `scripts/Package-V1_0_27-macOS-AFF4-ThinFromExistingCase.ps1` so the existing completed V1.0.26 case can be packaged without rerunning the AFF4 probe.
- Added continuing-chat handoff, roadmap checklist, and suggestions/fixes tracker files under `docs/`.

## Local validation performed

- CMake configure: passed.
- `src/core/app_info.cpp` C++20 syntax check: passed.
- `src/gui/gui_view_helpers.cpp` C++20 syntax check: passed.
- Static PowerShell text checks: passed.

## Pending validation

- Windows/MSVC V1.0.27 build.
- Execute the packaging-only wrapper against the existing V1.0.26 case output.
- Upload and review `Upload_Thin_MacOS_AFF4_V1_0_27.zip`.
- Verify generated thin ZIP excludes denied raw logs and inventories.

## Source: `V1_0_26_1_local_validation.log`

```text
V1.0.27 local validation

Inputs reviewed:
- /mnt/data/V1_0_26_build.log
- User-reported V1.0.26 macOS AFF4/APFS wrapper output showing probe and external comparison completed, then thin upload packaging failed.

Observed V1.0.26 build status:
- Windows/MSVC build completed.
- Build log reported Vestigant Spotlight v1.0.26.

Observed V1.0.26 thin packaging failure:
- tools/Create-SourceProbeUploadZip.ps1 failed in Get-RelativePathForThinInventory.
- Error: Cannot convert value "\\" to type "System.Char".
- Failure occurred while creating additional_output_file_inventory.txt after external comparison and mismatch diagnostics completed.

V1.0.27 local checks:
- cmake -S . -B /mnt/data/v1026_1_cmake: PASS
- g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp: PASS
- g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_view_helpers.cpp: PASS
- Static check: tools/Create-SourceProbeUploadZip.ps1 no longer contains the old escaped backslash [char] form: PASS
- Static check: Get-RelativePathForThinInventory uses System.Uri.MakeRelativeUri: PASS
- Static check: packaging-only wrapper is referenced in the command text file: PASS

Not validated here:
- Windows/MSVC V1.0.27 full build.
- Windows PowerShell execution of the packaging-only wrapper.
- Generated V1.0.27 thin ZIP contents.
- macOS AFF4/APFS rerun.
- iOS CoreSpotlight rerun.
```

## Source: `V1_0_25_validation_notes.md`

# V1.0.25 Validation Notes

## Baseline reviewed

- Uploaded `V1_0_24_1_build.log` showed a successful Windows/MSVC build.
- Uploaded `Upload_Thin_MacOS_AFF4_V1_0_24_1.zip` reached `complete_source_probe`.
- V1.0.24.1 AFF4/APFS staged Store-V2 parser metrics remained consistent with the prior baseline: `raw_records=25000`, `raw_key_values=2181`, `raw_date_candidates=25000`, `staged_groups=11`, `staged_files=8986`, `staged_bytes=1368577744`.

## Changes validated locally

- Confirmed version fields now report `1.0.25`.
- Confirmed `src/app/app_runner.cpp` no longer copies the following raw files into Thin Upload lists:
  - `aff4_stream_inventory_raw.txt`
  - `ios_focused_zip_extract.log`
  - `ios_focused_zip_extract_7z.log`
  - `ios_focused_zip_extract.ps1`
- Confirmed Thin Upload export copying now iterates regular `.csv` files directly under `caseDir/exports` instead of using a hardcoded required export manifest.
- Confirmed `countCsvDataRows()` now uses binary chunk newline counting.
- Confirmed `extractedIosAppDbPathForZipEntryCpp()` now uses `lexically_normal()` and per-component sanitization.
- Confirmed Windows helper process calls for AFF4 stream inventory and ZIP PowerShell staging now use direct `CreateProcessW` process execution with stdout/stderr redirected to log handles.
- Ran `g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp`; result: passed.

## Not validated here

- Windows/MSVC full build.
- Windows GUI runtime.
- V1.0.25 macOS AFF4/APFS thin run.
- V1.0.25 iOS CoreSpotlight thin run.

## Deferred review items

The following review items were intentionally deferred because they are larger architectural changes and should not be mixed with this security/performance hardening build:

- Moving APFS diagnostic writer bodies into `apfs_diagnostic_exporter.cpp`.
- Moving decmpfs resource-fork reconstruction into the codec module.
- Moving iOS app database record inventory orchestration fully into `IosAppDbParser`.
- Moving dynamic AFF4/APFS probe lambdas into `ApfsVolumeReader`.
- Refactoring the Win32 GUI global state into a window-associated class.
- Refactoring `runApplication()` database lifetime to a single long-lived `CaseDatabase` object.

## Source: `V1_0_25_patch_manifest.txt`

```text
V1.0.25 patch file manifest

Scope:
- Thin Upload security/performance hardening.
- iOS staging row-count and path-normalization hardening.
- Selected Windows hidden process execution hardening.

Files included in patch ZIP:
- CMakeLists.txt
- CONSOLIDATED_VERSION_HISTORY.md
- RELEASE_NOTES.md
- V1_0_24_1_DELETED_FILES_MANIFEST.md
- V1_0_25_DELETED_FILES_MANIFEST.md
- VERSION
- VERSION.txt
- VERSION_HISTORY.md
- docs/BUILD_NOTES.md
- docs/CONSOLIDATED_VERSION_HISTORY.md
- docs/PROJECT_ROADMAP_AND_CONTINUATION.md
- docs/V1_0_24_1_GUI_VIEW_HELPERS_BUILD_HOTFIX.md
- docs/V1_0_25_GUI_VIEW_HELPERS_BUILD_HOTFIX.md
- docs/V1_0_25_THIN_UPLOAD_SECURITY_AND_IOS_PERFORMANCE.md
- docs/VALIDATION_HISTORY.md
- docs/VALIDATION_STATUS.md
- scripts/Build-V1_0_24_1.ps1
- scripts/Build-V1_0_25.ps1
- scripts/Create-GitHubProjectIssues.ps1
- scripts/Initialize-GitHubRepo.ps1
- scripts/Launch-V1_0_24_1-GUI.ps1
- scripts/Launch-V1_0_25-GUI.ps1
- scripts/New-ReleaseBranch.ps1
- scripts/Run-V1_0_24_1-macOS-AFF4-Probe-AndZip.ps1
- scripts/Run-V1_0_24_1-macOS-AFF4-Probe-AndZip.txt
- scripts/Run-V1_0_25-macOS-AFF4-Probe-AndZip.ps1
- scripts/Run-V1_0_25-macOS-AFF4-Probe-AndZip.txt
- scripts/Sync-Version-To-GitRepo.ps1
- src/app/app_runner.cpp
- src/core/app_info.cpp
- validation/V1_0_24_1_local_validation.log
- validation/V1_0_24_1_patch_manifest.txt
- validation/V1_0_24_1_validation_notes.md
- validation/V1_0_25_local_validation.log
- validation/V1_0_25_patch_manifest.txt
- validation/V1_0_25_validation_notes.md

Deleted/renamed historical versioned wrapper files are documented in V1_0_25_DELETED_FILES_MANIFEST.md.
```

## Source: `V1_0_25_local_validation.log`

```text
V1.0.25 local validation

Input baseline:
- V1.0.24.1 source from uploaded/generated package.
- Uploaded V1_0_24_1_build.log: Windows/MSVC build completed and reported Vestigant Spotlight v1.0.24.1.
- Uploaded Upload_Thin_MacOS_AFF4_V1_0_24_1.zip: complete_source_probe; staged Store-V2 parser rows match expected V1 baseline.

Local checks:
- Version bump check: VERSION, VERSION.txt, CMakeLists.txt, src/core/app_info.cpp updated to 1.0.25.
- Static Thin Upload leak check: removed raw AFF4 stream inventory text, iOS focused extraction logs, and iOS focused extraction script from createUploadBundle and refreshUploadRunDiagnostics copy lists.
- Static export-manifest check: removed requiredExportFiles/optionalExportFiles hardcoded export list; added directory iterator for top-level exports/*.csv plus existing upload_samples copy.
- Static performance check: countCsvDataRows changed from std::getline per-row allocation to binary chunk newline counting.
- Static path-normalization check: extractedIosAppDbPathForZipEntryCpp changed to lexically_normal plus sanitized safe path components.
- Static process-launch check: Windows AFF4 stream inventory and PowerShell ZIP staging calls now use CreateProcessW helpers with redirected stdout/stderr log handles and no cmd.exe /C wrapper.
- Syntax check: g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp passed.

Limitations:
- Full Linux CMake build was not completed due local execution timeout.
- Windows/MSVC build is required.
```

## Source: `V1_0_23_validation_notes.md`

# V1.0.23 Validation Notes

## Baseline reviewed

- Source baseline: `VestigantSpotlightInv_V1_0_22.zip`
- Build log reviewed: `V1_0_22_build.log`
- V1.0.22 Windows/MSVC log reported `Vestigant Spotlight v1.0.22` and contained no build errors.

## V1.0.23 scope

- Added `src/parsers/apfs_diagnostic_models.h`.
- Moved APFS/AFF4 diagnostic row/summary structs out of `src/app/app_runner.cpp` into the new shared model header.
- Updated version files, CMake project version, app version, release notes, and versioned PowerShell wrappers to V1.0.23.

## Explicitly not changed

- macOS AFF4/APFS traversal, extraction, copy-out, staging, external compare, or Store-V2 parsing.
- iOS CoreSpotlight extraction/parsing or iOS GUI view behavior.
- Apple/lzfse codec behavior.
- SQLite schema.
- GUI export worker behavior from V1.0.22.

## Local validation performed

- Verified `VERSION`, `VERSION.txt`, `CMakeLists.txt`, and `src/core/app_info.cpp` report V1.0.23.
- Verified `src/app/app_runner.cpp` includes `parsers/apfs_diagnostic_models.h`.
- Verified APFS diagnostic row/summary structs exist in `src/parsers/apfs_diagnostic_models.h` and were removed from `app_runner.cpp`.
- Ran standalone syntax check for `src/parsers/apfs_diagnostic_models.h` using GCC/C++20.
- Ran GCC/C++20 `-fsyntax-only` check for `src/app/app_runner.cpp` with the same source include paths and `VESTIGANT_HAS_LZFSE=1`.
  - Result: syntax check completed successfully.
  - Warnings observed were pre-existing unused-parameter warnings in `stageZipEvidenceSource`.
- Attempted Linux CMake build/self-test.
  - Debug/Release builds progressed through the large `app_runner.cpp` translation unit and showed only warnings before the execution environment timed out.
  - No compile error was observed before timeout.

## Required external validation

- Windows/MSVC build using `scripts\Build-V1_0_23.ps1`.
- GUI launch smoke test using `scripts\Launch-V1_0_23-GUI.ps1`.
- Optional macOS AFF4/APFS thin run using `scripts\Run-V1_0_23-macOS-AFF4-Probe-AndZip.ps1`.

## Source: `V1_0_22_validation_summary.txt`

```text
V1.0.22 validation summary

Changes:
- Review current-page export backend moved to GuiExportWorker.
- Review filtered-view export backend moved to GuiExportWorker.
- UI thread now launches current-page export asynchronously and receives WM_EXPORT_PAGE_RESULT.
- Existing checked/tagged asynchronous exports retained.

Local validation performed by assistant:
- C++ compilation on Linux through CMake should validate non-Win32 modules and CLI/tests.
- Windows/MSVC GUI build still must be verified by user.

Extraction behavior intentionally unchanged.
```

## Source: `V1_0_21_validation_summary.txt`

```text
V1.0.22 validation summary

Purpose:
- Fix V1.0.20 Windows GUI compile failure caused by missing Selected Row Details ListView helper functions.
- Preserve V1.0.20 modularization: export/database worker logic remains in gui_export_worker.

Changed files of interest:
- src/gui/win32_gui.cpp
- src/core/app_info.cpp
- VERSION / VERSION.txt
- CMakeLists.txt
- scripts/Build-V1_0_22.ps1
- scripts/Launch-V1_0_22-GUI.ps1
- scripts/Run-V1_0_22-macOS-AFF4-Probe-AndZip.ps1

Validation performed in packaging environment:
- Confirmed missing helper names are now defined in src/gui/win32_gui.cpp.
- Confirmed GUI export worker files remain present.
- Confirmed version strings updated to 1.0.22.
- Package and ZIP integrity checks performed.

Not verified here:
- Windows/MSVC build.
- Win32 GUI runtime.
- Live macOS/iOS extraction, because extraction code was not changed.
```

## Source: `V1_0_20_validation_summary.txt`

```text
V1.0.20 validation summary

Completed in packaging environment:
- CMake configure: PASS
- Linux build: PASS
- CLI version check: PASS (Vestigant Spotlight v1.0.20)
- VestigantSpotlightTests: PASS
- gui_export_worker.cpp syntax/build: PASS via Linux build
- apfs_diagnostic_exporter.cpp syntax/build: PASS via Linux build
- app_runner.cpp syntax/build with Apple/lzfse vendored: PASS via Linux build

Not completed in packaging environment:
- Windows/MSVC build
- Win32 GUI runtime test
- macOS AFF4/APFS live run
- iOS live run

Scope note:
- V1.0.20 is a modularization/threading release. It does not intentionally change extraction semantics.
```

## Source: `V1_0_18_validation_summary.txt`

```text
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
```

## Source: `V1_0_15_validation_summary.txt`

```text
V1.0.15 validation summary

Changed areas:
- src/app/app_runner.cpp
- tools/Create-SourceProbeUploadZip.ps1
- tools/Run-SingleAff4SourceProbeAndZip.ps1
- scripts/Build-V1_0_15.ps1
- scripts/Run-V1_0_15-macOS-AFF4-Probe-AndZip.ps1
- docs/V1_0_15_STOREV2_DUAL_PROCESS_COMPARE.md
- docs/V1_0_15_LZFSE_LZVN_SOURCE_REVIEW.md

Implemented:
- Added Store-V2 dual-process candidate comparison between AFF4/APFS copy-out candidates and normalized staging selections.
- Added CSV/JSON/Markdown outputs for the compare.
- Added compare outputs to thin-upload packaging and wrapper expected-output checks.
- Updated source/scripts/version metadata to 1.0.15.
- Documented LZFSE/LZVN source status and the production benchmark required before enabling codec output.

Checks performed in Linux sandbox:
- g++ C++20 syntax compile of src/app/app_runner.cpp: PASS.
- g++ C++20 syntax compile of src/parsers/apfs_volume_reader.cpp, src/parsers/apfs_aff4_reader.cpp, src/parsers/ios_app_db_parser.cpp, src/gui/view_registry.cpp, and tests/main.cpp: PASS.
- CMake configure: PASS.
- CMake build progressed through all common modules into app_runner.cpp and timed out during the very large optimized app_runner.cpp compile; no compile error was observed before timeout.

Not verified here:
- Windows/MSVC V1.0.15 build.
- Win32 GUI runtime.
- Live AFF4/APFS V1.0.15 run.
```

## Source: `V1_0_12_validation_summary.txt`

```text
V1.0.12 validation summary

Implemented:
- Added opt-in AFF4/APFS structural diagnostic CSV flag.
- Suppressed heavy structural APFS CSV writes during normal source-probe while preserving copy-out/stage/parser/enrichment outputs.
- Added callback-driven ApfsVolumeReader lower-bound directory iterator implementation.
- Added ApfsVolumeReader synthetic iterator smoke coverage.
- Removed low-risk duplicated iOS parser wrappers from app_runner.cpp.
- Confirmed GUI view registry ODR concern was already resolved in V1.0.11: ViewSpec/views() definitions exist only in view_registry.

Validation performed in this environment:
- g++ C++20 syntax checks passed for:
  - src/parsers/apfs_volume_reader.cpp
  - src/parsers/apfs_aff4_reader.cpp
  - src/parsers/ios_app_db_parser.cpp
  - src/db/case_db.cpp
  - src/cli/main.cpp
  - src/app/app_runner.cpp
  - tests/main.cpp
- CMake configure passed.
- Linux build progressed through changed modules and app_runner.cpp compilation but timed out before full link; no compile error was observed before timeout.

Not verified here:
- Windows/MSVC V1.0.12 build.
- Win32 GUI runtime.
- Live AFF4/APFS V1.0.12 run.
- External comparison after normal-mode diagnostic CSV suppression.
```

## Source: `V1_0_10_validation_summary.txt`

```text
V1.0.11 validation summary

Performed in this environment:
- Created GUI view registry module and moved ViewSpec/views/viewHelpText out of win32_gui.cpp.
- Added ViewPlatform enum and updated tab routing to use platform metadata.
- Added view_registry.cpp to Windows GUI build targets.
- Added source-probe failure packaging attempt for nonzero AFF4/APFS exits.
- CMake configure started successfully.
- Linux build progressed through all common modules and timed out while compiling the existing large app_runner.cpp; no compile error was observed before timeout.
- Standalone syntax compile of src/gui/view_registry.cpp passed with g++.
- Version files updated to 1.0.11.

Not verified here:
- Windows/MSVC build.
- Win32 GUI runtime.
- Live AFF4/APFS V1.0.11 run.
```
