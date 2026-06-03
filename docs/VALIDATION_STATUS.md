# Vestigant Spotlight Validation Status

Current version: 0.9.43

## V0_9_43 packaging validation

Reviewed inputs:

- `V0_9_42_build.log`
- `Upload_Thin_iOS_GUI_V0_9_42_ReusedCache_Check.zip`
- `VestigantSpotlightInv_V0_9_42.zip`
- V0_9_41 and V0_9_40 logs/thin uploads for comparison.

Findings from uploaded outputs:

- V0_9_42 Windows/MSVC build completed and linked CLI, tests, and GUI.
- V0_9_42 iOS reuse-cache run reached `complete_success`.
- V0_9_41 reuse-cache also reached `complete_success`.
- V0_9_40 failed with SQLite/disk-full behavior, confirming V0_9_41/V0_9_42 were the stable comparison baselines.
- V0_9_42 source already contained the CSV export fast path and native C++ 7-Zip raw inventory parser.

Validation performed in this Linux packaging environment:

- Changed-file C++ syntax checks: PASS for `src/parsers/native_storedb_parser.cpp`, `src/db/case_db.cpp`, and `src/export_sql/sqlite_exporter.cpp`.
- New VSQL33 bplist / NSKeyedArchiver SQLite view smoke test: PASS.
- MSVC C2026 raw-string size risk check across major SQL/GUI/export/app files: PASS, no oversized raw-string literals found at the configured 16,000-byte threshold.
- Linux Release build was attempted, but the sandbox timed out while compiling the very large `src/app/app_runner.cpp`; no compile errors were observed before timeout.

Required external Windows validation:

1. Run `scripts\Build-V0_9_43.ps1` or `build_windows_msvc.bat`.
2. Confirm CLI reports `Vestigant Spotlight v0.9.43`.
3. Run `VestigantSpotlightTests.exe` self-test.
4. Run `scripts\Run-V0_9_43-iOS-ReuseCache-CLI-AndZip.ps1` and confirm `complete_success`.
5. Review the new bplist / NSKeyedArchiver summary/detail exports and GUI views.
6. If reuse-cache validation succeeds, run the Stage B fresh-ZIP script to validate actual FFS ZIP enumeration/staging.