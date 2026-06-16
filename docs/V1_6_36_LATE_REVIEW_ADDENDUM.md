# V1.6.40.1.1 Late Review Addendum

This addendum records the late changes added to V1.6.40.1.1 after review of the uploaded issue list.

## Implemented

1. iOS CoreSpotlight app attribution
   - `deriveIosCommunicationFields` now searches for `kMDItemBundleID`, `bundleId`, `bundle_id`, and `bundleIdentifier`.
   - The detected bundle identifier is added to provenance as `APP_ATTRIBUTION=...`.
   - Generic/blank titles are upgraded to `Spotlight Record (<bundle>)`; existing titles receive a bounded bundle suffix when not already present.

2. Spotlight timestamp arrays
   - `normalizeIosAppTimestamp` now accepts bracketed array strings and parses the first comma-delimited timestamp.
   - This is a bounded low-risk compatibility fix for date values emitted as strings like `[ 654321000.0, 654321100.0 ]`.

3. GUI read-only database pool safety
   - `ReadOnlyDb` now pools SQLite connections per database path instead of a single global pointer.
   - Opening a different case path no longer closes the connection pointer used by an active worker for another path.
   - Schema ensure is performed outside the global pool mutex and only for read/write opens.

4. Bplist/NSKeyedArchiver stringification cap
   - `resolveUid` now enforces a `BplistJsonStringLimit` while flattening arrays and dictionaries.
   - Oversized output is truncated with `<truncation_size_limit_reached>`.

## Already implemented before this addendum

- CSV exports already used `sqlite3_column_bytes()` and length-aware `writeCsvFieldFast` with `[NUL]` / `[0xNN]` placeholders. No new exporter change was required for this item.

## Not implemented here

- Full upstream native date-array expansion into multiple `raw_date_candidates` rows was not implemented in this addendum. The lower-risk timestamp-array normalization was implemented instead.
- Shared-pointer based per-query SQLite handle lifetime management was not implemented; the lower-risk per-path pool was implemented.
