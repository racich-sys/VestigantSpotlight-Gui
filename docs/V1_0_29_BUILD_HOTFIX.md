# V1.0.28.2 Build Hotfix

V1.0.28.2 is a narrow linker hotfix after the V1.0.28.1 MSVC build failed with `LNK2005` for `vestigant::spotlight::isLikelyStoreV2GroupDirectoryName`.

## Root cause

The APFS diagnostic writer relocation copied a helper named `isLikelyStoreV2GroupDirectoryName()` into `src/parsers/apfs_diagnostic_exporter.cpp` while the original orchestrator helper remained in `src/app/app_runner.cpp`. The copied helper had external linkage, so MSVC linked two definitions of the same symbol.

## Fix

The exporter-side helper is now local to `apfs_diagnostic_exporter.cpp` by placing it in an anonymous namespace around that helper only. The existing `app_runner.cpp` helper remains unchanged for the dynamic AFF4/APFS probe logic.

## Scope boundaries

No APFS traversal, AFF4 reads, copy-out/staging behavior, Store-V2 parsing, iOS parsing, SQLite schema, GUI behavior, or forensic interpretation changed in this hotfix.
