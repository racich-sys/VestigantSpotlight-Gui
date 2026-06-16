# V1.6.29 Code Review Validation Hardening

This document is retained in V1.6.40.1.1 because release readiness requires the V1.6.29 code-review hardening record.

## Implemented hardening carried forward

1. APFS OMAP vertical cycle detection stops repeated branch/leaf node traversal with `VOLUME_OMAP_VERTICAL_CYCLE_DETECTED`.
2. AFF4 direct LZ4 decompression uses subtraction-based bounds checks and length-overflow guards.
3. NSKeyedArchiver/bplist array and dictionary decoding performs explicit count and key/value bounds validation.
4. Bplist decoding has a global expansion cap and repeated complex UID guard.
5. Bplist fallback ripping preserves high-byte text and includes bounded UTF-16LE extraction for iOS Unicode strings.
6. Generic iOS app database parsing uses paginated `LIMIT ? OFFSET ?` extraction instead of a single 50,000-row page.
7. GUI database schema creation is reduced to once per database path in the shared GUI connection pool.
8. `vw_ios_spotlight_comms_missing_from_ffs` remains present and registered.
9. Legacy folder browsing warns on unresolved or MAX_PATH-boundary paths.

## Guardrails retained

Missing-from-FFS rows remain investigative leads only. CoreDuet `interactionC.db` rows remain contextual evidence only.
