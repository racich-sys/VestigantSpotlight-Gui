# V1.1.6 AFF4 Probe Worker Direct-Map Split

V1.1.6 begins the Tracker #17 God-closure remediation by physically moving the `writeAff4DirectMapReaderProbe` body out of `src/app/app_runner.cpp` into `src/parsers/aff4_probe_worker.cpp`.

## Implemented

- New module: `src/parsers/aff4_probe_worker.h/.cpp`.
- New entry point: `Aff4ProbeWorker::executeDirectMapReaderProbe(...)`.
- App runner delegates direct-map probe calls to the worker.
- Build systems include `src/parsers/aff4_probe_worker.cpp`.

## Deferred

`writeAff4CppLiteDynamicLoadProbe(...)` remains in `app_runner.cpp`. A full physical extraction was started, but the function depends on many app-runner-local structs/helpers and needs a separate dependency boundary pass. V1.1.6 records this in `docs/WORKFLOW_LEDGER.md` so the same dependency discovery is not repeated.

## Forensic behavior

No live APFS interpretation, AFF4 byte-read semantics, copy-out decisions, or Store-V2 parsing logic were intentionally changed.
