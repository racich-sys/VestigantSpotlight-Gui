# V1.0.12 Structural Cleanup and APFS Diagnostic-I/O Review

## Purpose

V1.0.12 continues the V1 modularization work while preserving the currently successful AFF4/APFS staged Store-V2 extraction path. The goal is to reduce monolithic coupling without rewriting the working copy-out path in the same iteration.

## Review findings

- `src/gui/win32_gui.cpp` no longer defines `ViewSpec` or `views()`. Those definitions are owned by `src/gui/view_registry.h/.cpp`, so the suspected GUI ODR duplicate was not present in the reviewed V1.0.11 tree.
- `src/app/app_runner.cpp` still owns the specialized Apple Messages, WhatsApp, and KnowledgeC row emitters. Full migration is delayed until a parser-independent row sink is added.
- Normal AFF4/APFS source-probe runs were still writing many structural diagnostic CSVs. This was the highest-impact safe performance fix for this iteration.

## Implemented

- Added `RunOptions::aff4ApfsDiagnosticOutputs` and CLI flag `--aff4-apfs-diagnostic-outputs` / `--diagnostic-apfs-csvs`.
- Standard V1.0.12 AFF4/APFS wrapper runs no longer pass `--verbose` by default.
- Heavy structural AFF4/APFS CSV outputs are now gated behind `--aff4-apfs-diagnostic-outputs`, `--verbose`, or `--diagnostic-full-native-db`.
- Copy-out, staging, native Store-V2 parser probe, enrichment samples, and external comparison outputs remain enabled in normal source-probe mode.
- The APFS remaining-mismatch diagnostics tool now treats `aff4_apfs_spotlight_xattr_probe.csv` as optional so normal-mode suppressed diagnostic CSVs do not block external comparison.
- Removed low-risk duplicated iOS parser wrapper functions from `app_runner.cpp` and routed KnowledgeC snippet creation through `ios_app_db_parser`.
- Upgraded `ApfsVolumeReader::enumerateDirectory()` from a placeholder to a callback-driven lower-bound iterator that can be tested independently and later bound to the live AFF4/APFS block reader.

## Delayed

- Full migration of Apple Messages, WhatsApp, KnowledgeC, and generic table row emission is delayed until a parser-independent row sink exists.
- Live AFF4/APFS traversal is not replaced by `ApfsVolumeReader` yet. The benchmark before replacement is iterator output parity with the current staged Store-V2 output.
- LZFSE/LZVN remains delayed pending vetted source, MSVC/Linux integration, and known-good vectors.

## Operator usage

Normal faster source-probe:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_12\scripts\Run-V1_0_12-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

Full diagnostic source-probe:

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_0_12\scripts\Run-V1_0_12-macOS-AFF4-Probe-AndZip.ps1 -CleanOut -DiagnosticOutputs
```
