# V1.1.8 Windows Path and History Baseline

## Scope

V1.1.8 starts from the validated V1.1.7.1 source/build/thin baseline and the user-provided `BaselineVersionHistory.md`.

## Changes

- Replaced the in-package full history with the user-cleaned baseline and appended V1.1.8.
- Added Windows long-path helpers in `src/core/path_utils.*`.
- Routed APFS/AFF4 Store-V2 evidence copy-out and decmpfs reconstruction writes through long-path-capable file output on Windows.
- Changed `CaseDatabase::close()` to request `SQLITE_CHECKPOINT_TRUNCATE`.
- Added an explicit WAL checkpoint/truncate status marker before upload packaging.
- Added mutex protection around `Logger` writes.
- Lowered the APFS decmpfs expected-output cap to 256 MiB.

## Deferred

- Live APFS horizontal leaf promotion.
- Full NSKeyedArchiver UID graph decoding.
- Win32 virtual list-view conversion.
- Complete EvidenceIntake ZIP-staging relocation.
