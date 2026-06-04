# Vestigant Spotlight V1.0.4 - AFF4/APFS Extraction and Repository Cleanup

## Immediate review inputs

Reviewed the V1.0.1 Windows build log and the V1.0.1 macOS AFF4/APFS thin output. V1.0.1 built successfully and accessed the selected AFF4/APFS image deeply enough to resolve APFS volume metadata and find `.Spotlight-V100` / `Store-V2` namespace entries. The remaining blocker was not AFF4 access; it was target object correlation and copy-out.

## Implemented in V1.0.4

### AFF4/APFS extraction movement

- Removed the hardcoded Spotlight traversal caps named in the review:
  - `kMaxSpotlightScanNodes`
  - `kMaxSpotlightDecodedNameSamples`
  - `kMaxSpotlightScanRecordsPerNode`
  - `kMaxSpotlightScanDepth`
  - `kMaxCopyOutFiles`
- Kept structural cycle protection through visited-node / visited-directory sets rather than the old fixed depth/node caps.
- Added APFS directory-entry parent mapping and path-context reconstruction for Store-V2 copy attempts.
  - Target rows now carry reconstructed APFS absolute path context in notes, for example expected Data-volume paths beginning `/System/Volumes/Data/...`.
- Added traversal-time INODE caching and later target correlation.
  - Previously scanned INODE rows are now correlated back to Store-V2 targets before guided private-id/dstream extent lookup.
- Added zero-fill assembly handling for sparse logical gaps and zero physical extents.
  - Sparse or zero regions are written as synthetic zero bytes instead of automatically skipping the file.
  - Output rows record synthetic zero counts and validation status.
  - Output hashes are calculated over the reconstructed logical byte stream.
- Forced GUI runs to keep preservation and native metadata decoding enabled.
- Removed GUI controls/handles for deprecated core/full-native toggles.
- Removed the app-runner self-test function and fake `/Users/alice/...` data from the primary binary.
- Replaced the test executable with a schema/view smoke test that does not generate fake forensic case records.
- Purged stale V0.9 run/package/collect scripts, V0.9 validation artifacts, and old Codex change notes from the production package.

## Partially implemented or delayed

### LZFSE/LZVN static integration

Not implemented in V1.0.4. A vetted Apple/reference `lzfse` source tree was not present in the uploaded project source, and adding a decompression stub would be forensically unsafe. Current decmpfs reconstruction support remains limited to already implemented plain/zlib and limited marker-handling paths.

Benchmark for implementation:

- Add a vendored `third_party/lzfse/` source tree with license text.
- Build it on Linux and MSVC from the project CMake and MSVC batch build.
- Add unit vectors for at least: raw uncompressed decmpfs, zlib decmpfs, LZVN, LZFSE, malformed payload, truncated resource fork.
- Require extracted output size and SHA256 match for known-good external extraction samples before enabling production staging.

### Full transition from source-probe to normal ingest

Not completed in V1.0.4. The code now moves closer to copy-out, but the AFF4/APFS path still remains diagnostic-forward until V1.0.4 output proves that target INODE, XATTR, FILE_EXTENT, sparse/zero provenance, and staged Store-V2 files are valid.

Benchmark for promotion into `discoverStores()`:

- `aff4_apfs_spotlight_inode_probe_summary.json` shows nonzero target inode hits.
- `aff4_apfs_spotlight_file_extent_probe_summary.json` shows nonzero target extent rows.
- `aff4_apfs_spotlight_file_copy_out_summary.json` shows copied files and no unexplained read/write failures.
- `aff4_apfs_extracted_storev2_stage_summary.json` shows staged Store-V2 groups/files.
- External comparison shows material improvement against `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`.
- Staged Store-V2 files parse through `NativeStoreDbParser` and produce case DB artifacts without corrupt/false-positive files.

### Diagnostic view cleanup

Only package/script cleanup was done in V1.0.4. SQLite diagnostic views were not removed broadly because several of them are currently used to report parser coverage and evidence limitations. Removing them without a view registry/support-export split could reduce forensic transparency.

Benchmark for production cleanup:

- Classify each view as `investigator`, `case_quality`, `support`, or `internal_diagnostic`.
- GUI should show only `investigator` and selected `case_quality` views by default.
- Support/internal views should be generated only when an explicit support/diagnostic export is requested.
- No user-facing View Set should show views whose names include `diagnostic` unless support mode is active.

### Remaining APFS copy-out safeguards

The per-file copy-out memory/size gate remains at 512 MB. This is retained to avoid creating massive memory buffers or unexpectedly large outputs during the first post-correlation AFF4 run.

Benchmark for relaxing this:

- Convert all copy-out to streaming reads/writes with progress counters.
- Confirm no single-file memory allocation scales with the file's logical size.
- Confirm timeout/progress UI reflects active AFF4/APFS copy-out throughput.
- Add large-file test case with sparse regions and multi-extent layout.

## Next expected validation

Run the V1.0.4 AFF4/APFS probe against the same explicit AFF4 file and upload:

- `D:\Downloads\V1_0_4_build.log`
- `D:\Downloads\Upload_Thin_MacOS_AFF4_V1_0_4.zip`

Primary review metrics:

- target inode hits > 0
- target FILE_EXTENT rows > 0
- copied files > 0, or clear no-match statuses explaining why
- synthetic zero rows recorded only when sparse/zero regions are present
- staged Store-V2 groups/files > 0
- native parser probe artifacts > 0 if Store-V2 staging succeeds
