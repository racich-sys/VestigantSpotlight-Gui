# V1.6.3.1 iOS Existence and Frequency Milestone

## Scope

V1.6.3.1 advances the iOS investigation layer from extraction/completion toward existence and frequency review. It uses V1.3.7 as the validated base and keeps the V1.3.7 stability fixes.

## Implemented

- Promotes dated parsed iOS app-database records into `timeline_events` with `IOS_COMMUNICATION_RECORD`, `IOS_KNOWLEDGEC_EVENT`, and `IOS_APP_ACTIVITY_RECORD` event types.
- Promotes selected dated parsed iOS records into `usage_evidence` using cautious source-field labels under `ios_app_parsed_records.*`.
- Adds `vw_ios_communication_existence_evidence` for row-level communication-existence support from parsed records and provenance markers.
- Adds `vw_ios_communication_identity_frequency` for identity/thread frequency rollups.
- Adds `vw_ios_communication_temporal_frequency` for daily frequency by thread/identity/app/category.
- Adds `vw_ios_communication_source_coverage` for database/table/category coverage counts.
- Registers the new views in the GUI and export layer.

## Interpretation boundary

The new outputs are evidence-support and frequency views. They do not assert final user intent. Each row preserves source table, database, category, timestamp source, parse status, and provenance so an investigator can review the supporting source row before making conclusions.

## Test decision

- iOS thin: required.
- AFF4/APFS thin: not required for this milestone unless separately validating AFF4 changes.
- Full tests: not required unless iOS thin shows regression, stall, malformed exports, or materially unexpected counts.
