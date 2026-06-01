# Filesystem Inventory SQLite Plan

## Purpose

This plan defines how Vestigant Spotlight should store APFS/HFS/macOS filesystem metadata for later Spotlight-to-filesystem comparison without bloating the main case database.

## Design decision

Use an optional attached database:

- `case.sqlite` remains the primary Spotlight/case review database.
- `filesystem_inventory.sqlite` is created only when filesystem inventory or comparison mode is enabled.
- Thin upload ZIPs should continue to exclude SQLite databases unless specifically requested.

## Default modes

### Default extraction mode

- Parse enough APFS metadata to locate `.Spotlight-V100` / `Store-V2`.
- Extract only targeted Spotlight files.
- Store extraction provenance and hashes.
- Do not inventory the full filesystem.

### Filesystem comparison mode

- Build `filesystem_inventory.sqlite` with lean metadata for APFS/HFS/iOS filesystem objects.
- Do not store file contents.
- Do not store every raw APFS B-tree record in the main case database.
- Write comparison results back to the main case as narrow match/confidence rows.

### Debug mode

- Developer-only raw APFS node, key/value, and extent traces may be written to sidecar CSV/JSON or a separate debug database.
- Debug outputs should not be part of the default production package or focused upload.

## Reuse across macOS and iOS

The filesystem inventory layer should be shared between macOS AFF4/APFS and iOS full-filesystem/APFS work where practical:

- source/container registration
- volume inventory
- filesystem object rows
- path normalization
- extent provenance
- extraction provenance
- hash/provenance records

The parser routes stay separate:

- macOS Spotlight Store-V2 parser
- iOS CoreSpotlight SQLite/plist/NSKeyedArchiver parser

## Future comparison flow

1. Register the exact selected source container.
2. Parse APFS/HFS filesystem metadata into the optional inventory database.
3. Locate Spotlight targets from reconstructed filesystem paths.
4. Extract only selected Spotlight targets and hash them.
5. Parse extracted Spotlight data into the main case database.
6. Compare Spotlight artifacts to filesystem nodes by volume UUID, object ID/inode, parent ID, filename, normalized path, size, and dates.
7. Store match status and confidence in narrow comparison tables.
