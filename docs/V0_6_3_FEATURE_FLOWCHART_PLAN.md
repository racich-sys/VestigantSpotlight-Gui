# Vestigant Spotlight v0.6.3 Feature Flowchart and Path Forward

```text
[Evidence intake]
       |
       v
[Normal case run]
       |-- archive-first preservation remains default
       |-- parser reads preserved staging copy
       |
       v
[Diagnostics run]
       |-- skips 7z archive by default
       |-- reads input directly
       |-- enables safe native core probes
       |
       v
[Store discovery and selection]
       |-- inventory store.db and .store.db
       |-- select one primary database per logical store group
       |
       v
[Native parser]
       |-- header decode: inode, parent inode, item id, last updated
       |-- property dictionary decode: field names and value-type metadata
       |-- safe probe decode: bounded high-value raw string/path probes
       |-- full structured values: still experimental
       |
       v
[SQLite normalization]
       |-- raw_records
       |-- raw_key_values
       |-- raw_date_candidates
       |-- native_property_dictionary
       |-- native_decode_attempts
       |
       v
[Enrichment]
       |-- artifacts
       |-- timeline
       |-- usage evidence when usage fields decode
       |-- parent-inode relationships
       |-- same-folder grouping
       |-- external volume indicators from Spotlight-native data
       |
       v
[Investigator exports]
       |-- review CSVs
       |-- diagnostic CSVs
       |-- coverage summaries
```

## Path forward

```text
v0.6.3: decoder diagnostics, no archive by default in diagnostics mode
v0.6.4: safe structured metadata decoding for high-value fields
v0.6.5: date and usage enrichment from decoded native fields
v0.6.6: Spotlight-native parent-inode path reconstruction
v0.6.7: Spotlight-native external volume/removable media indicators
v0.7.0: GUI review workflow
```

## Deferred

Active filesystem comparison / evidence-root live-vs-missing analysis is tabled. It should remain optional future work and should not block Spotlight-native parser progress.
