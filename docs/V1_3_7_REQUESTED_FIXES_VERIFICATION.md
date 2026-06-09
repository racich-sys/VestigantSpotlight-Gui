# V1.6.3.1 Requested Fixes Verification

This release explicitly verifies the five requested V1.3.x stability and iOS extraction changes against the active source tree.

## Verified implemented

1. GUI database connection deadlock fix
   - `src/gui/win32_gui.cpp` defines `ReadOnlyDb` with only scoped `std::lock_guard<std::mutex>` use inside constructor/static close.
   - There is no `std::unique_lock<std::mutex> lock_;` member retained in `ReadOnlyDb`.

2. APFS guided B-tree cycle detection
   - `src/parsers/aff4_probe_worker.cpp` uses `std::set<std::uint64_t> visitedGuidedNodes` in both guided inode and guided file-extent traversal paths.
   - Cycle states are recorded as `GUIDED_INODE_LOOKUP_CYCLE_DETECTED` and `GUIDED_FILE_EXTENT_LOOKUP_CYCLE_DETECTED`.

3. Embedded bplist string extraction for iOS app DB rows
   - `src/parsers/ios_app_db_parser.cpp` implements `ripBplistStrings(...)`.
   - Generic text extraction calls it when a SQLite text/blob value starts with `bplist` and bounds output size.

4. Notes and Location routing
   - `iosAppDbRecordCategory(...)` routes Notes and Location/Maps-style tables to `NOTES_RECORDS` and `LOCATION_RECORDS`.
   - `iosAppDbIsTargetRecordCategory(...)` includes both categories in the parsed-target set.

5. Wider table-column catchers
   - `parseIosAppDbTableRows(...)` includes `zcontent`, `zsummary`, `data`, and `payload` in text-column detection.
   - Path detection includes `zfilename`, `zpath`, and `zlocalpath`.

## Thin-profile safeguard retained

V1.6.3.1 also retains the V1.3.6.1 iOS thin safeguard: standard iOS thin mode should not materialize the full 2.2M-row FFS inventory unless full/support diagnostics are explicitly requested.

## Validation commands

Run these from the package root to verify source presence:

```powershell
Select-String -Path .\src\gui\win32_gui.cpp -Pattern 'unique_lock<std::mutex> lock_'
Select-String -Path .\src\parsers\aff4_probe_worker.cpp -Pattern 'visitedGuidedNodes|GUIDED_INODE_LOOKUP_CYCLE_DETECTED|GUIDED_FILE_EXTENT_LOOKUP_CYCLE_DETECTED'
Select-String -Path .\src\parsers\ios_app_db_parser.cpp -Pattern 'ripBplistStrings|NOTES_RECORDS|LOCATION_RECORDS|zcontent|payload|zlocalpath'
```

Expected result: first command returns no lines; second and third commands return matching implementation lines.
