
## V1.6.41.1 - CSV default, source-profile filtering, unresolved-label path guard

- GUI processing now defaults to `Exclude CSV exports` checked. SQLite case output remains the default review artifact unless CSV exports are explicitly enabled.
- Non-iOS ZIP profiles now record that iOS FFS/app-database parser stages were skipped.
- macOS-profile exports now skip `ios_*` CSV export calls rather than writing large groups of zero-row iOS CSVs.
- Unresolved Store-V2 review labels are no longer accepted as valid filename/path components for parent-inode path reconstruction.
- Added `docs/V1_6_41_CSV_DEFAULT_AND_SOURCE_PROFILE_FILTERING.md`.


## V1.6.41.1 macOS unresolved Store-V2 object labels

- Added explicit unresolved object labels for macOS Store-V2 records that still lack structured names after dictionary/path-probe enrichment.
- Labels are forensic review handles, not asserted filenames.
- Added parser metric `unresolved_identifier_label_artifacts`.


## V1.6.41.1 Late Review Addendum

- Added late-review fixes for iOS CoreSpotlight bundle attribution, bracketed timestamp-array normalization, per-path GUI read-only DB pooling, and bplist JSON stringification caps.
- Confirmed null-byte-safe CSV export was already present before this addendum.

# V1.6.41.1 Continuation Note

Latest source package: V1.6.41.1. It follows V1.6.35, which confirmed external dbStr maps loaded, by fixing native path/basename probe promotion so GUI names/paths improve where native candidates exist. Validate with `V1_6_41_build.log` and a rerun of the same macOS zipped Spotlight thin test.

# V1.6.41.1 Continuation Note

Latest source package: V1.6.41.1. It fixes macOS Store-V2 external dbStr map loading after direct inspection of uploaded `store.db`, `.store.db`, and `dbStr-*` sidecar files. Validate with `V1_6_41_build.log`, rerun the macOS zipped Spotlight thin test, and inspect native dbStr inventory/property dictionary counts plus GUI `------NONAME------` rate.

# V1.6.41.1 Continuation Note

Latest source package: V1.6.41.1. It fixes macOS Store-V2 GUI rows showing `------NONAME------` by promoting native path probe candidates from `raw_key_values` into artifact path/display fields before timeline materialization. Next required evidence: `V1_6_41_build.log` and a rerun of the macOS zipped Spotlight thin test.

# V1_6_41 note

V1_6_41 skips the parent-inode path apply UPDATE when `new_reconstructed_paths=0`, based on the V1.6.32 macOS zipped Spotlight thin result. Build success remains unverified until the Windows log is uploaded.

# V1_6_41 note

V1_6_41 fixes recurring build-blocking release-readiness failures: release-readiness is advisory from the build wrapper, while wrapper compatibility and raw-string risk remain fatal. Expected CLI version is read dynamically from `VERSION`.

# Start Continuation Chat - V1.6.41.1

Use `VestigantSpotlightInv_V1_6_41.zip` as the current source baseline.

## Why this hotfix exists

V1.6.29.3 failed MSVC compile in `aff4_probe_worker.cpp` at the new OMAP vertical-cycle note calls. V1.6.41.1 uses the existing `aff4ApfsAppendProbeNote` helper and hardens the build wrapper so a missing CLI executable stops the build before version probing.

## Required next uploads

- `D:\Downloads\V1_6_41_build.log`
- `D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_41.zip`

## Current package: V1.6.41.1

Continue from `VestigantSpotlightInv_V1_6_41.zip`. First validate `V1_6_41_build.log`, then validate `Upload_Thin_iOS_CoreSpotlight_V1_6_41.zip`. V1.6.41.1 includes GUI checked-state locking, stale review-query detach, export-worker detach, length-aware CSV export for embedded NUL/control bytes, and APFS NXSB block-size rejection before use.

## Current package: V1.6.41.1

Continue from `VestigantSpotlightInv_V1_6_41.zip`. First validate `V1_6_41_build.log`. Then rerun the macOS folder Spotlight test against `E:\test second\Spotlight\.Spotlight-V100` and confirm that the run reports `native_kv_persistence_macos_storev2`, writes `native_parse_configuration`, and continues updating root progress during native parse.
