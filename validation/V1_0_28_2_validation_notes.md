# V1.0.28.2 Validation Notes

## Scope

Build/link hotfix only. V1.0.28.1 failed during MSVC link with a duplicate external symbol:

`vestigant::spotlight::isLikelyStoreV2GroupDirectoryName`

The duplicate came from `app_runner.obj` and `apfs_diagnostic_exporter.obj` after APFS diagnostic writer relocation.

## Fix

The exporter-side `isLikelyStoreV2GroupDirectoryName()` helper was scoped to `src/parsers/apfs_diagnostic_exporter.cpp` using an anonymous namespace. The existing runner helper remains available to the dynamic AFF4/APFS probe logic.

## Validated locally

- `src/parsers/apfs_diagnostic_exporter.cpp` C++20 syntax check.
- `src/app/app_runner.cpp` C++20 syntax check.
- `src/core/app_info.cpp` C++20 syntax check.
- Local object-symbol check confirmed the exporter object no longer exports a public `isLikelyStoreV2GroupDirectoryName` definition.

## Not validated locally

- Windows/MSVC full build.
- Windows GUI runtime.
- V1.0.28.2 macOS AFF4/APFS thin run.
