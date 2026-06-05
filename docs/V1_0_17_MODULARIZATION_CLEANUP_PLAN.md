# V1.0.17 Modularization and Cleanup Plan

## Current structure after V1.0.17

Current line counts from this package:

```text
src/codec/lzfse_codec.cpp           73
src/parsers/apfs_aff4_reader.cpp   242
src/parsers/apfs_volume_reader.cpp 289
src/gui/view_registry.cpp          323
src/parsers/ios_app_db_parser.cpp  898
src/gui/win32_gui.cpp             3301
src/app/app_runner.cpp           15083
```

The previous modularization work created real module boundaries, but `app_runner.cpp` and `win32_gui.cpp` are still too large. The highest-risk future work should move logic out by capability, not by broad mechanical deletion.

## Production cleanup priority

### 1. APFS/AFF4 module migration

Move from `app_runner.cpp` into `src/parsers/apfs_aff4_reader.cpp` and `src/parsers/apfs_volume_reader.cpp`:

- APFS file-copy row construction.
- Direct indexed FILE_EXTENT chain assembly.
- decmpfs xattr/resource-fork lookup and reconstruction.
- lower-bound directory iterator comparator and promotion logic.
- APFS copy-out status classification.

Acceptance benchmark:

- `app_runner.cpp` no longer constructs APFS copy-out rows directly.
- Existing V1.0.16/V1.0.17 AFF4/APFS thin metrics do not regress.
- External compare output still runs automatically.

### 2. macOS Store-V2 investigation module

Create a small module for macOS-specific investigator summaries:

```text
src/investigate/macos_spotlight_views.cpp
src/investigate/macos_spotlight_views.h
```

Move high-level field interpretation and confidence labeling out of SQL/GUI-only code.

Acceptance benchmark:

- macOS views show field provenance and confidence.
- Date/use/source interpretations are traceable to Store-V2 field/key/value provenance.

### 3. GUI review database helper

Create:

```text
src/gui/review_db_helper.cpp
src/gui/review_db_helper.h
```

Move raw SQL out of `win32_gui.cpp` for:

- view page loading,
- filtered export,
- checked artifact persistence,
- tag read/write,
- table/column introspection.

Acceptance benchmark:

- `win32_gui.cpp` stops constructing most raw SQL strings.
- view registry remains the single source for table/column metadata.
- iOS/macOS tab separation continues to use `ViewPlatform`.

### 4. GUI thread/query lifecycle manager

Create:

```text
src/gui/review_query_manager.cpp
src/gui/review_query_manager.h
```

Encapsulate request sequence IDs, cancellation, join, and worker result handoff.

Acceptance benchmark:

- rapid view switching does not overlap stale query writes into the active list view.
- SQLite busy errors remain recoverable and visible.

### 5. iOS parser row sink

`ios_app_db_parser.cpp` now owns most row parsing, but the parser interface should still be made more independent from app-runner insertion details.

Acceptance benchmark:

- Apple Messages, WhatsApp, and KnowledgeC parser output can be unit-tested without opening a full case database.
- `app_runner.cpp` only enumerates candidate databases and passes a row sink.

## Cleanup policy

- Keep V1 build/run scripts for current supported workflows.
- Do not reintroduce V0.9 one-off scripts into production packages.
- Keep support/diagnostic APFS CSVs opt-in, except copy-out/stage/parser/external-compare outputs required for investigator validation.
- Do not delete diagnostic paths until an equivalent support-mode workflow remains available.
