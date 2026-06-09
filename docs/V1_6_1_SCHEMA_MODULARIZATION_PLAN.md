# V1.6.3.1 Schema Modularization Plan

Purpose: prevent recurring MSVC C2026, SQL/view mismatches, and fragile monolithic schema edits.

Immediate V1.6.3.1 action: release checks keep raw string literals below the configured threshold and V1.6 identity graph SQL is split into small statements.

Next structural target:

- `src/db/schema/schema_core.cpp`
- `src/db/schema/schema_ios_views.cpp`
- `src/db/schema/schema_ios_identity.cpp`
- `src/db/schema/schema_ios_frequency.cpp`
- `src/db/schema/schema_macos_views.cpp`

`case_db.cpp` should become orchestration only. New views should not be added as large monolithic raw string blocks.
