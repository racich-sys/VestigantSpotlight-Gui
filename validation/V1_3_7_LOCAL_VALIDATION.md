# V1.3.7 Local Validation Notes

Purpose: hotfix after V1.3.6 iOS thin failed during full FFS inventory materialization with `database or disk is full` / `disk I/O error`.

Change: iOS diagnostics mode with minimal export profile no longer automatically requests full FFS/app DB materialization. Full materialization remains available through `--materialize-ios-ffs-inventory`, `--materialize-ios-app-db-records`, `--materialize-ios-support-db`, or export profiles `diagnostics`, `support`, or `full`.

Expected effect: standard iOS thin wrapper uses slim FFS lookup and avoids inserting 2.2M+ full FFS rows into the active case DB.

Test scope: iOS thin required. AFF4/APFS not required for this hotfix.
