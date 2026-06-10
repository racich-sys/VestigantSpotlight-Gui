# V1.6.6.5 Build Wrapper and Release Readiness Notes

V1.6.6.5 retains the V1.6.6.4 build-wrapper/release-readiness protections and adds native CoreSpotlight probe-string review coverage.

Current wrapper/readiness checks verify:

- version metadata is `1.6.6.5`;
- the Windows build wrapper validates the built CLI against `1.6.6.5`;
- GUI export worker helper functions remain visible in `gui_export_worker.cpp`;
- generic `KNOWLEDGEC_EVENTS` is not used as a positive communication/identity promotional predicate;
- native CoreSpotlight probe-string review coverage is present in parser, SQL views, GUI registry, and self-test code;
- MSVC raw-string literal size remains below the project risk guard.

Detailed V1.6.6.5 release notes are in `docs/V1_6_6_5_THIN_UPLOAD_REVIEW_AND_RELEASE_NOTES.md`.
