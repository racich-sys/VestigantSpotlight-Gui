# V1.1.5 AFF4 Cancellation and Upload Guard Hardening

V1.1.5 is a repeat-cycle hardening release based on the V1.1.4 Windows/MSVC build and macOS AFF4/APFS thin-output baseline.

## Implemented

- Propagated the existing ingest cancellation token into the guarded AFF4/libaff4 dynamic-load probe and the direct AFF4 map-reader probe.
- Added bounded cancellation checks inside selected expensive APFS/AFF4 probe loops so Cancel Ingest can return sooner during long scans.
- Added an early case-directory writability probe before logger/database initialization.
- Replaced PowerShell redirection for targeted app database extraction and focused CoreSpotlight extraction logs with explicit UTF-8 `Out-File` writes.
- Replaced recursive thin-upload copying of `exports/upload_samples` with explicit per-file policy handling and the existing 50 MB size guard.
- Applied the same nested upload-samples size policy to `tools/Create-SourceProbeUploadZip.ps1`.
- Wrapped APFS staged Store-V2 diagnostic sample CSV exports in a localized try/catch so diagnostic-sample failure does not suppress final probe summaries.

## Not implemented

- Full `writeAff4CppLiteDynamicLoadProbe(...)` extraction into `aff4_probe_worker.cpp`.
- Full `stageZipEvidenceSource(...)` relocation into `EvidenceIntake`.
- Live APFS horizontal leaf traversal replacement.
- Live APFS absolute path reconstruction.
- Full NSKeyedArchiver UID graph decoding.

Those remain tracked as separate high-risk/focused targets. V1.1.5 does not alter live Store-V2 parsing semantics or APFS copy-out interpretation.
