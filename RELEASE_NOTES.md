# Vestigant Spotlight v1.0.28.2 Release Notes

## Summary

V1.0.28.2 is a narrow build/link hotfix for V1.0.28.1. It fixes the MSVC `LNK2005` duplicate symbol for `isLikelyStoreV2GroupDirectoryName` after APFS diagnostic writer relocation.

## Changes

- Scoped the APFS diagnostic exporter copy of `isLikelyStoreV2GroupDirectoryName()` to the exporter translation unit.
- Preserved the existing runner helper used by dynamic AFF4/APFS probe code.
- Updated continuation, roadmap, and suggestions/fixes tracking docs.

## Not changed

- No APFS traversal changes.
- No AFF4 read changes.
- No copy-out/staging changes.
- No Store-V2 parser changes.
- No iOS parser changes.
- No SQLite schema changes.
- No GUI behavior changes.
