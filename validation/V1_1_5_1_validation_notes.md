# V1.1.5.1 Validation Notes

## Scope

V1.1.5.1 is a narrow MSVC build hotfix for V1.1.5.

## Root cause

V1.1.5 added a cancellation check inside a volume OMAP lookup lambda returning `ApfsOmapTargetResolution`, but the cancellation branch returned `false`. MSVC correctly failed with C2440 because `bool` cannot be converted to `ApfsOmapTargetResolution`.

## Fix

The cancellation branch now sets `lookupStatus`, `interpretation`, and `notes` on the local `ApfsOmapTargetResolution out` object and returns `out`.

## Not changed

No APFS traversal semantics, Store-V2 parsing, iOS parsing, GUI behavior, SQLite schema, or copy-out behavior changed.

## Local validation

- Source syntax checks were run for changed/dependent files.
- Linux CMake configure/build and local self-test were attempted/recorded in `V1_1_5_1_local_validation.log`.

## Windows validation pending

- Windows/MSVC build.
- macOS AFF4/APFS thin run.
