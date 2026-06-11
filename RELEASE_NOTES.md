## V1.6.6.6 - Thin result review and GUI bootstrap native-DB mismatch view audit

V1.6.6.6 was built after review of the uploaded V1.6.6.5 iOS thin bundle. The thin run reached `complete_success` and reported 6 valid stores, 344,445 raw records, 42,799 raw key/value rows, 344,445 artifacts, 228,699 usage evidence rows, and 277,823 timeline events. No slow or incomplete exports were reported above the thin-performance threshold.

Source inspection of V1.6.6.5 found that the requested APFS guided traversal cycle guards, bounded bplist/NSKeyedArchiver resolver, and `tel:` / `mailto:` identity fallback were already present. The actionable gap was that `vw_ios_spotlight_comms_missing_from_ffs` existed in the case schema and registry, but not in the lightweight GUI bootstrap SQL.

Changes:

- Added `vw_ios_spotlight_comms_missing_from_ffs` to `src/gui/win32_gui.cpp` GUI bootstrap SQL.
- Prioritized `iOS - Spotlight Comms Missing From Native DB` in both GUI sort lists.
- Added V1.6.6.6 forensic-directive and release-readiness checks requiring the view in `case_db.cpp`, `win32_gui.cpp`, and `view_registry.cpp`.
- Bumped current wrappers, version metadata, and current documentation to V1.6.6.6.

Test determination: run Windows/MSVC build and iOS thin. AFF4/APFS thin/full is not required unless Windows build, shared schema initialization, or APFS/AFF4 validation checks regress.

## V1.6.6.5 - iOS native CoreSpotlight probe review bridge

V1.6.6.5 was built after review of the uploaded V1.6.6.4 Windows build log and iOS thin bundle. The Windows build log showed a completed V1.6.6.4 build. The iOS thin bundle reached `complete_success`, with 6 valid stores, 344,445 raw records, 22,569 raw key/value rows, 344,445 artifacts, 228,699 usage evidence rows, and 277,823 timeline events.

The V1.6.6.4 thin run showed that timeout-prone V1.6.6.2 exports were fixed, but compact native probe strings were still not flowing into the main iOS text-context and communication review surfaces. `ios_string_probe_category_summary.csv` contained 9,591 message/app string probes and 933 email/account probes, while message/text-context review exports remained empty.

Implemented:
- high-signal `__native_core_probe_string_*` values can now feed same-record text context;
- iOS text-context review exposes `source_field_name` and classifies native SMS/iMessage/mail/account/file-reference probes;
- iOS communication/message review views expose `native_probe_context_count` and `native_probe_context_sample`;
- new communication buckets identify `SPOTLIGHT_MESSAGE_OR_ATTACHMENT_TEXT_PROBE` and `SPOTLIGHT_MAIL_OR_ACCOUNT_TEXT_PROBE`;
- `vw_ios_spotlight_investigator_overview` now uses lightweight base/probe counts to avoid the V1.6.6.4 46-second slow overview export;
- self-test coverage now includes native CoreSpotlight probe-to-text-context and communication-review checks.

Validation performed here:
- Linux CMake build: PASS.
- CLI version: `Vestigant Spotlight v1.6.6.5`.
- Self-test: PASS.
- Windows/MSVC build: not run here.

# V1_6_6_5

## Summary

V1.6.6.5 is a build-wrapper and release-readiness hotfix after V1.6.6.3 stopped at the pre-build readiness check for generic `KNOWLEDGEC_EVENTS` in communication/identity predicates.

## Implemented

- Updated the current Windows build wrapper to V1.6.6.5 and corrected the final CLI version assertion to `1.6.6.5`.
- Moved build-wrapper preflight checks after source extraction / clean extraction so the checks validate the package that will actually be built.
- Updated Win32 GUI bootstrap communication views to use `KNOWLEDGEC_COMMUNICATION_INTENT` and provenance markers rather than generic `KNOWLEDGEC_EVENTS`.
- Reworked release-readiness checks to distinguish disallowed promotional predicates from allowed suppression guards that exclude generic KnowledgeC/device-app activity rows.
- Added release-readiness coverage for `src/gui/win32_gui.cpp` in addition to `src/db/case_db.cpp`.
- Added `docs/V1_6_6_5_BUILD_WRAPPER_AND_READINESS_HOTFIX.md`.

## Validation

- Local Linux CMake build: PASS.
- CLI version: `Vestigant Spotlight v1.6.6.5`.
- Self-test: PASS.
- Static current-wrapper/text audit: PASS.
- Static KnowledgeC promotional predicate audit: PASS.
- Windows/MSVC build: not run in this environment.
- Required next validation: Windows/MSVC build, then iOS thin.
