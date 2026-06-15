# V1.6.10 Local Validation Notes

Scope: local packaging-environment validation only. This environment is Linux and does not contain the user's full AFF4 evidence source or Windows/MSVC toolchain.

## Build and test evidence

- Linux incremental CMake build: PASS (`/mnt/data/v169_build4.log`).
- CLI version: `Vestigant Spotlight v1.6.10` (`/mnt/data/v169_version2.log`).
- Self-test: PASS (`/mnt/data/v169_tests2.log`).
- Static review confirmed V1.6.10 markers for the release-readiness false-positive fix, native probe semantic aliases, staged Store-V2 field-inventory/parser-coverage sample exports, upload packaging inclusion, and revised source-probe/help wording.

## Local staged-folder note

A direct local run against `/mnt/data/thin168_norm/ExtractedSpotlight/StagedStoreV2` was attempted only as an exploratory sanity check. Normal folder discovery did not recognize that standalone extracted staging root shape and produced zero candidates. That run is not used as validation evidence for parser correctness. The V1.6.10 evidence-supported validation is limited to build/version/self-test and source inspection in this environment.

## Not verified here

- Windows/MSVC build.
- Full live macOS AFF4/APFS thin for V1.6.10.
- iOS CoreSpotlight thin or production run for V1.6.10.
