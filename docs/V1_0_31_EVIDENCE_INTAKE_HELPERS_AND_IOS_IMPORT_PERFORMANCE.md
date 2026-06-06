# V1.0.31 Evidence Intake Helpers and iOS CSV Import Performance

V1.0.31 advances the evidence-intake modularization roadmap without changing APFS/AFF4 extraction physics or SQLite schema.

## Implemented

- Added `src/ingest/evidence_intake.h/.cpp`.
- Moved reusable intake helper logic out of `app_runner.cpp`:
  - `countCsvDataRows`.
  - iOS ZIP path normalization and basename/extension helpers.
  - iOS database category/app/domain/protection/container hint helpers.
  - iOS app database staging path sanitization.
- Added temporary bulk-import SQLite PRAGMAs for regenerable iOS CSV fallback ingestion.
- Restored WAL/NORMAL settings after commit or rollback.
- Added `case_sensitive_like=OFF` to GUI review/export read-only connections.

## Deliberately not implemented

- Full `stageZipEvidenceSource` movement.
- Full `importIosInventoryCsvs` movement.
- APFS reverse path reconstruction.
- NSKeyedArchiver UID/object graph unflattening.
- GUI global state encapsulation.

These remain tracked but should be separate, focused versions.
