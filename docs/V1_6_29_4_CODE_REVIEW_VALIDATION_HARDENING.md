# V1.6.29.4 Code Review Validation Hardening

## Triggering evidence reviewed

- `V1_6_29_4_build.log` reported source version `1.6.29.4`, linked CLI/tests/GUI, printed `Vestigant Spotlight v1.6.29.4`, and completed without decoded error/warning/fail lines.
- `Upload_Thin_iOS_CoreSpotlight_V1_6_29_4.zip` completed with `complete_success` and all active-comparison plus CoreDuet validation checks passing.
- `selftest_out.zip` contained generated self-test case databases and parent-inode reconstruction run logs showing parent-inode enrichment and path reconstruction completed.
- User-provided V1.6.x review issues were checked against current source and implemented where verifiable.

## Implemented source fixes

1. APFS OMAP vertical cycle detection now tracks repeated node OIDs in the vertical B-tree descent paths and stops with `VOLUME_OMAP_VERTICAL_CYCLE_DETECTED` instead of relying only on depth limits.
2. AFF4 direct LZ4 decompression now uses subtraction-based bounds checks for literal and match lengths and guards variable-length length accumulation against overflow.
3. NSKeyedArchiver/bplist dictionary and array decoding now validates key/value bounds before iteration and rejects oversized object counts.
4. Bplist decoding now includes a global expansion call cap and repeated-complex-UID guard to avoid repeated expansion of cyclic or highly shared object graphs.
5. Bplist fallback string ripping now preserves high-byte text and includes a bounded UTF-16LE sliding-window fallback for iOS Unicode strings.
6. Generic iOS app database row parsing now paginates with `LIMIT ? OFFSET ?` instead of silently stopping after the first 50,000 rows.
7. GUI case-database view creation is reduced to once per database path in the shared GUI connection pool, reducing repeated schema churn during active ingest.
8. The strict `vw_ios_spotlight_comms_missing_from_ffs` view name already exists and remains registered.
9. Legacy folder browsing now checks `SHGetPathFromIDListW` failure and MAX_PATH-boundary paths and warns the investigator instead of silently returning a truncated/empty folder path.

## Build/preflight fixes

- Split oversized SQL raw strings so the static MSVC string-literal risk check no longer finds raw strings over 5,000 characters.
- Verified the V1.6.29.4 build wrapper and readiness check are pinned to `1.6.29.4`.
- Hardened the build wrapper so PowerShell preflight scripts are checked for nonzero exit codes before MSVC starts.

## Guardrail status

Missing-from-FFS outputs remain investigative leads only. CoreDuet `interactionC.db` rows remain contextual evidence only. No deletion or communication conclusion is made solely from these rows.
