
## V1.6.40.1 - MSVC raw-string safety hotfix

- Split oversized SQL raw-string blocks in `src/db/case_db.cpp` after the V1.6.40 iOS index-update timeline view change.
- No parser, enrichment, or GUI behavior change from V1.6.40.
- Local raw-string audit found no raw-string body over 5,000 characters.


## V1.6.40.1.1 - CSV default, source-profile filtering, unresolved-label path guard

- GUI processing now defaults to `Exclude CSV exports` checked. SQLite case output remains the default review artifact unless CSV exports are explicitly enabled.
- Non-iOS ZIP profiles now record that iOS FFS/app-database parser stages were skipped.
- macOS-profile exports now skip `ios_*` CSV export calls rather than writing large groups of zero-row iOS CSVs.
- Unresolved Store-V2 review labels are no longer accepted as valid filename/path components for parent-inode path reconstruction.
- Added `docs/V1_6_40_1_CSV_DEFAULT_AND_SOURCE_PROFILE_FILTERING.md`.


## V1.6.37.1 macOS unresolved Store-V2 object labels

- Added explicit unresolved object labels for macOS Store-V2 records that still lack structured names after dictionary/path-probe enrichment.
- Labels are forensic review handles, not asserted filenames.
- Added parser metric `unresolved_identifier_label_artifacts`.

# V1.6.37.1 Known Issues Update

- Windows/MSVC build for V1.6.37.1 is not verified until `V1_6_40_1_build.log` is uploaded.
- macOS Store-V2 native parser remains in safe core-probe mode; structured property dictionary decoding remains experimental.
- Active filesystem comparison for ZIP/folder Spotlight sources remains NOT_CHECKED unless a validated filesystem inventory is provided.

# Known Issues - 1.6.40.1

- Windows/MSVC build validation is pending until `V1_6_40_1_build.log` is uploaded.
- macOS profile still creates some legacy iOS-named export files with zero rows; this is a UI/export hygiene issue and should be separated from parser correctness.
- The external thin wrapper may report a diagnostic blank exit code even when the CLI writes `complete_success`; rely on run_status/last_stage until the wrapper is revised.

# Known Issues - 1.6.40.1

- Windows/MSVC build validation is pending until the user uploads `V1_6_40_1_build.log`.
- Release-readiness is now advisory during build; review warnings should still be addressed before final release packaging, but they should not block compilation.

# Known Issues - V1.6.37.1

- V1.6.37.1 Windows/MSVC build is not verified until `V1_6_40_1_build.log` is uploaded and reviewed.
- Missing-from-FFS candidates remain investigative leads only, not deletion proof.
- CoreDuet `interactionC.db` rows remain contextual evidence only.
- AFF4/APFS image-backed active filesystem comparison remains pending.

## V1.6.37.1 Known limitations

- GUI source changes are statically checked in this package but require Windows/MSVC build validation from `V1_6_40_1_build.log`.
- Detached stale review-query workers rely on the existing request-sequence/progress-handler cancellation pattern; a future thread-pool design may be cleaner.
- AFF4/APFS image-backed active filesystem comparison remains pending.

## V1.6.37.1 Known limitations

- V1.6.37.1 Windows/MSVC build is not verified until `V1_6_40_1_build.log` is uploaded.
- The macOS folder Spotlight run that triggered this fix stopped during native parse setup in V1.6.30; V1.6.37.1 should be rerun against the same folder to confirm whether parse completion is restored or whether a deeper native Store-V2 parser issue remains.
