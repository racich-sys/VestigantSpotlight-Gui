# V1.6.41 Security and iOS Unicode Fixes

This release addresses requested parser hardening and iOS evidence-preservation fixes.

## Implemented

1. LZ4 raw-block bounds checks were hardened in `src/parsers/native_storedb_parser.cpp` by using subtraction-style validation and output-capacity checks rather than addition-only overflow-prone comparisons.
2. CoreSpotlight high-value string probing now preserves high-bit UTF-8 bytes plus tab, carriage return, and line feed. Exact NUL bytes remain delimiters.
3. iOS bplist fallback string ripping now preserves high-bit UTF-8 bytes and CR/LF/TAB while retaining bounded UTF-16 fallback extraction.
4. The internal bplist decoder now expands UID objects into bounded object-table hints where possible and retains recursion/call-count/size limits.
5. `vw_ios_spotlight_comms_missing_from_ffs` remains available in both core and GUI schemas for Spotlight-vs-native-app communication anti-join review.
6. `deriveIosCommunicationFields` retains direct `tel:` and `mailto:` fallback identity recovery.

## Limits

The bplist/NSKeyedArchiver work is an internal bounded decoder, not a libplist replacement. It is designed to preserve more context safely without making unsupported claims when an object graph cannot be fully resolved.
