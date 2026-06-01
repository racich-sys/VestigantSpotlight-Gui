# Vestigant Spotlight forensic roadmap - macOS and iOS

## Current validation position

- macOS AFF4/APFS extraction has progressed from Store-V2 discovery to large-scale Store-V2 file staging and comparison against an external reference export.
- Current V0.8.73 APFS reference comparison for BADA95B6 shows 13,050 exact relative path/SHA256 matches, 47 relative-path size mismatches, 1,734 external-only rows, and 813 Vestigant-only rows.
- iOS CoreSpotlight ZIP intake now reaches parse/enrichment/export successfully for at least one selected store, producing CoreSpotlight store inventory, string probes, index-update timeline, and parser coverage exports.

## macOS roadmap

### Phase M1 - APFS/AFF4 extraction validation

Goal: extract `.Spotlight-V100/Store-V2` from AFF4/APFS without mounting and match an external reference export by relative path, size, and SHA256.

Near-term work:
- Continue reducing BADA95B6 Cache `.txt` mismatches.
- Classify each remaining mismatch as data-fork, ResourceFork, decmpfs, unsupported compression codec, path/candidate-selection, FILE_EXTENT, or comparison/reference issue.
- Preserve per-file APFS provenance: APFS object ID, parent object ID, target path, extent status, xattr names, decmpfs algorithm, resource fork stream ID/size, copy status, validation status, and reconstruction status.
- Keep external comparison reproducible and cacheable with reference manifests.

### Phase M2 - Compression and resource-fork hardening

Goal: reconstruct compressed APFS Store-V2 Cache entries when possible.

Near-term work:
- Finish ZLIB_RSRC reconstruction wherever the resource fork stream is available.
- Keep LZVN/LZFSE/LZBITMAP unsupported-codec outputs explicit until native codecs are available.
- Add optional external codec helper evaluation only if it can be packaged reproducibly and legally.
- Add diagnostics for resource-fork table offsets, chunk sizes, decoded bytes, failure reason, and expected uncompressed size.

### Phase M3 - Store-V2 native parsing and enrichment

Goal: decode Store-V2 records into investigator-facing artifacts.

Work:
- Continue safe structured value decoding with per-property exception isolation, bounds checks, and recursion limits.
- Preserve all raw field/property names and decoded values.
- Normalize date candidates with raw source field, interpreted type, UTC value, and snapshot/index-date warning status.
- Improve parent-inode path reconstruction and same-folder grouping.
- Associate usage/fused dates with object-centric artifact rows.

### Phase M4 - Search and review

Goal: enable keyword and keyword-list searches across decoded Spotlight content.

Work:
- Build normalized searchable value tables from paths, names, snippets, URLs, email/message-like strings, decoded metadata, raw fields, and dates.
- Add CLI keyword search and keyword-list search.
- Add GUI Keyword Search, Keyword Timeline, Hit by App/Container, Indexed-Only Candidates, and Present Source Items views.

### Phase M5 - Residency and active filesystem comparison

Goal: compare Spotlight-indexed records against files present in AFF4/APFS image inventories.

Work:
- Build APFS file inventory from AFF4/APFS source.
- Correlate Spotlight hits by path, object ID/inode, parent ID, hash, and size.
- Classify residency conservatively: source file present, source path present different object, indexed-only candidate, unresolved insufficient provenance.

## iOS roadmap

### Phase I1 - FFS ZIP intake and store discovery

Goal: reliably enumerate CoreSpotlight stores inside iOS FFS ZIPs and focused extracts.

Near-term work:
- Inventory `store.db` and `.store.db` ZIP entries before extraction.
- Confirm CLI discovery counts match ZIP entry inventory.
- Parse all valid iOS stores when `--profile ios --full-scan` is used.
- Preserve protection class folder, ZIP entry path, extracted path, store role, size, hash, and validity.

### Phase I2 - CoreSpotlight parse/enrichment

Goal: parse iOS CoreSpotlight records into useful investigative artifacts.

Work:
- Expand native value decoding beyond generic probe strings.
- Identify formal fields/categories where possible.
- Preserve record ID, store ID, inode-like IDs, Last_Updated/index timestamps, protection class, source store path, and raw value context.
- Treat Last_Updated as Spotlight index/update metadata, not user activity without corroboration.

### Phase I3 - Investigative summaries

Goal: quickly identify useful iOS artifacts.

Work:
- Maintain exports for store parse summary, string probe values, category summaries, URL/domain summaries, email/message-like values, cloud/storage values, calendar/invitation values, and index-update timeline.
- Add app/container inference using path/domain/bundle hints.
- Add high-value hit summaries and artifact timelines.

### Phase I4 - Keyword search and residency correlation

Goal: find investigator terms such as names, emails, phone numbers, and project terms, then determine whether the source item still appears resident.

Work:
- Search decoded CoreSpotlight values, raw probe strings, URLs, emails, message-like text, file URLs, calendar text, and metadata.
- Correlate hits against FFS file inventory and supported app databases.
- Example: a CoreSpotlight hit for “Keith” inferred from WhatsApp/message-like text should be compared against available WhatsApp SQLite databases and reported as source record found, DB found record not found, indexed-only candidate, unsupported app correlation, or unresolved.

### Phase I5 - App-specific correlation

Goal: map Spotlight hits to source app content where possible.

Priority targets:
- Messages/SMS/iMessage
- Mail
- WhatsApp
- Signal/Telegram where legally and technically available
- Calendar/Reminders
- Files/iCloud Drive/Dropbox/OneDrive/Google Drive
- Browser/web history where present

## Cross-platform roadmap

- Keep recurring validation workflows reproducible and versioned.
- Preserve tool/method notes for parsing repeated APFS/iOS thin upload artifacts.
- Add cached reference manifests for external comparisons.
- Add forensic-safe search/residency labels and avoid overclaiming deletion.
- Add GUI filtering, tagging, notes, keyword-search views, and exportable checked-item workflows after extraction and parser validation stabilize.
