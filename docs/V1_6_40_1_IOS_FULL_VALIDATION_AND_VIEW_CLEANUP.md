# V1.6.40.1.1 iOS Full Validation and View Cleanup

## Evidence reviewed

Reviewed V1.6.39 Windows build log, refreshed iOS CoreSpotlight small-file package, and V1.6.39 iOS thin upload.

The V1.6.39 build log showed MSVC compiled core/parser/enrichment/export/app code, linked CLI, tests, and GUI, reported `Vestigant Spotlight v1.6.39`, and reached `Build complete`.

The V1.6.39 iOS thin upload showed:

```text
raw_record_count=75493
raw_key_value_count=57875
raw_date_candidate_count=75493
artifact_count=75493
usage_evidence_count=10035
timeline_event_count=93734
raw_failures=0 in upload_table_counts
```

The V1.6.39 thin also showed the `NSFileProtectionCompleteUntilFirstUserAuthentication` store was included and parsed:

```text
NSFileProtectionCompleteUntilFirstUserAuthentication raw_record_count=33121
```

## Defect found

The iOS index-update timeline view still used raw `raw_records.file_name` and `raw_records.full_path`. As a result, the export/view could show `------NONAME------` even where artifact enrichment had already assigned an unresolved review label or better display path.

## V1.6.40.1.1 changes

1. `vw_ios_timeline_index_updates` now joins through `artifact_source_instances` to `artifacts` and uses enriched artifact file name, display name, and best path where available.

2. The GUI now includes an explicit checkbox:

```text
Full no-guardrails validation
```

When checked, GUI ingest disables native record limits, native block limits, and the SQLite DB/WAL size guardrail, enables full iOS FFS/app-DB materialization, enables diagnostic full native key/value persistence, forces full container hashing, and warns in the GUI log that the run may create very large DB/WAL files and take a long time.

The checkbox is intentionally unchecked by default.

## Local validation

Linux build completed and smoke tests passed for V1.6.40.1.1.

A quick iOS small-file validation run with V1.6.40.1.1 produced:

```text
raw_records=10000
raw_date_candidates=10000
raw_failures=0
timeline_events=10000
```

After the view patch, upload sample counts showed no `------NONAME------` values in the iOS index-update timeline sample:

```text
ios_timeline_index_updates_sample.csv rows=5000 noname=0 unresolved_review_labels=4981
artifacts_sample.csv rows=250 noname=0 unresolved_review_labels=249
timeline_events_sample.csv rows=250 noname=0 unresolved_review_labels=248
recent_activity_focus.csv rows=5000 noname=0 unresolved_review_labels=4982
```

Unresolved review labels remain transparent labels, not claimed filenames.
