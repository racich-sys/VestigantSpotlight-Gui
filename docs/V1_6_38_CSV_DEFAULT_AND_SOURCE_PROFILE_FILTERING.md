# V1.6.38 CSV Default, Source-Profile Filtering, and Unresolved Label Path Guard

## Scope

V1.6.38 implements the queued post-V1.6.37.1 workflow changes and one thin-run-derived corrective fix:

1. GUI processing now defaults to suppressing generated CSV review exports.
2. macOS source profiles now skip iOS-specific parser/export surfaces where they are not applicable.
3. Unresolved macOS Store-V2 review labels are prevented from seeding parent-inode path reconstruction.

## Evidence basis

The V1.6.37.1 macOS zipped Spotlight thin run proved that macOS profile selection and external `dbStr` dictionary loading were functioning, but it also showed that many zero-row iOS CSV exports were still generated for a macOS source. The same run also exposed that review labels such as `UNRESOLVED_SPOTLIGHT_OBJECT_INODE_<inode>` could be used as path components by parent-inode reconstruction.

## Implemented changes

### GUI CSV default

`src/gui/win32_gui.cpp` now labels the processing checkbox as `Exclude CSV exports` and checks it by default. The existing run option `suppressCsvExports` is retained and still writes `CSV_EXPORTS_DISABLED.txt` plus a small export index when selected.

### Source profile parser filtering

`src/app/app_runner.cpp` now emits `ios_source_specific_parsers_skipped_non_ios_profile` for non-iOS ZIP sources and skips iOS FFS/app database parser stages for those profiles. Existing iOS-only parser execution remains unchanged for iOS profile runs.

### Source profile export filtering

`src/export_sql/sqlite_exporter.cpp` now checks `case_info.profile`. When the profile is `macos`, export calls whose output filename starts with `ios_` are skipped and marked with `export_query_skipped_source_profile` in run-progress output. Shared/native/macOS-relevant exports continue.

### Unresolved label path guard

`src/enrich_sql/sqlite_enrichment.cpp` now treats `UNRESOLVED_SPOTLIGHT_OBJECT_INODE_%` and `Unresolved Spotlight object inode=%` as placeholder labels, not valid path/name components. These labels remain review handles but are no longer used to build `PARENT_NAME_PLUS_CHILD_NAME_ONLY`, recursive relative parent-inode paths, or artifact `best_path` values.

## Validation

Local Linux build and smoke test completed for V1.6.38. Windows/MSVC validation still requires the user-provided build log.
