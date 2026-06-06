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
