# V1.0.13

- Isolated raw AFF4/APFS copy-out files from normalized Store-V2 staging to prevent same-name duplicate APFS candidates from overwriting selected staging sources.
- Raw APFS copy-out rows now write to `ExtractedSpotlight/ApfsCopyOutByTarget/seq_<target>_fid_<id>_parent_<id>_<group>/...`.
- Normalized investigator-facing Store-V2 files remain under `ExtractedSpotlight/StagedStoreV2/<group>/...`.
- Thin-upload packaging excludes `ExtractedSpotlight/ApfsCopyOutByTarget` so support bundles do not balloon with raw duplicate copy-out files.

## V1.0.13

- Added opt-in AFF4/APFS structural diagnostic CSV output mode.
- Normal AFF4/APFS source-probe runs now suppress heavy structural APFS diagnostic CSVs while keeping copy-out, staging, parser, enrichment, and external-comparison outputs.
- Added `--aff4-apfs-diagnostic-outputs` / `--diagnostic-apfs-csvs` for full support runs.
- Added callback-driven `ApfsVolumeReader::enumerateDirectory()` lower-bound iterator implementation for isolated APFS directory walk testing.
- Removed low-risk duplicated iOS parser wrapper functions from `app_runner.cpp`.
- Confirmed GUI view registry ownership remains centralized in `view_registry`.
- Updated wrapper validation so suppressed diagnostics do not block normal external comparison.

Delayed:
- Full iOS row parser migration awaits parser-independent row sink.
- Live APFS traversal replacement awaits iterator parity benchmarks.
- LZFSE/LZVN remains pending vetted codec and test vectors.
