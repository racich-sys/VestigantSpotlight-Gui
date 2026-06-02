# V0_9_42 Review Notes

## Baseline reviewed

Reviewed the V0_9_40 Windows build log and the V0_9_40 reused-cache thin upload. The build completed successfully, including GUI, CLI, and self-test binaries. The reused-cache run did not complete; it failed during native parse with SQLite reporting `database or disk is full` after importing the full 1,592,440-row slim FFS lookup into the active case database and then beginning compact FullValues parsing.

## Failure class

This was not the older tens-of-millions row materialization problem. The row counts were still compact, but normal reuse-cache mode copied too much FFS lookup data into the active case before parsing. That consumed database space before the Spotlight parser wrote its compact native rows.

## Implemented changes

- Verified that the V0_9_40 CSV exporter already used direct `sqlite3_column_text()`, on-the-fly CSV escaping, and a 1 MiB output buffer. Preserved that optimization.
- Removed the stale generated PowerShell `Get-ZipEntriesViaSevenZip` pipeline/Regex function so the source no longer contains an unused slow `7z l -slt | ForEach-Object { [regex]::Match(...) }` path.
- Kept the active 7-Zip inventory path as raw `7z l -slt` output dumped to disk and parsed line-by-line without an external-process PowerShell pipeline.
- Changed normal reused-cache iOS mode so it no longer imports all FFS path lookup rows before native parse. It now inserts a one-row lookup sentinel and imports only exact referenced-path hits from the reuse-cache SQLite after native parsing has produced `vw_ios_spotlight_referenced_paths`.
- Preserved support/full behavior: `--materialize-ios-ffs-inventory` still materializes full FFS inventory for bounded support/correlation runs.

## Expected effect

The active case database should be substantially smaller during the native parse stage because it no longer begins with 1.59 million copied FFS path rows. Missing From FFS views should still have a lookup source and should still mark referenced paths as present when the referenced path exactly matches a path in the reused-cache FFS inventory.

## Validation

Linux build completed and CLI reported `Vestigant Spotlight v0.9.42`. Linux self-test passed. Windows/MSVC validation remains required.
