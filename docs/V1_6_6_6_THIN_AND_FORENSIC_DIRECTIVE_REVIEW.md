# V1.6.6.6 Thin Upload and Forensic Directive Review

## Inputs reviewed

- Uploaded thin bundle: `Upload_Thin_iOS_CoreSpotlight_V1_6_6_5.zip`.
- Source baseline: `VestigantSpotlightInv_V1_6_6_5.zip`.
- Current source after changes: V1.6.6.6.

## V1.6.6.5 thin result

The uploaded V1.6.6.5 thin bundle reached `complete_success`.

Observed counts from `case_summary.json`:

- `store_count`: 6
- `valid_store_count`: 6
- `raw_record_count`: 344,445
- `raw_key_value_count`: 42,799
- `artifact_count`: 344,445
- `usage_evidence_count`: 228,699
- `timeline_event_count`: 277,823

Observed performance from `THIN_PERFORMANCE_SUMMARY.md` and `thin_performance_summary.csv`:

- Observed run-progress duration: 1,242 seconds.
- No slow or incomplete exports were reported above the thin-performance threshold.
- The previously slow/timeout-prone iOS exports completed in V1.6.6.5 thin.

## Verification of queued forensic directives against V1.6.6.5 source

Each user-supplied finding was treated as unverified until checked against the actual V1.6.6.5 source package.

### APFS guided traversal cycle detection

Verified present in `src/parsers/aff4_probe_worker.cpp`:

- `lookupGuidedTargetInode` declares `visitedGuidedNodes` and emits `GUIDED_INODE_LOOKUP_CYCLE_DETECTED`.
- `lookupGuidedFileExtentCandidate` declares `visitedGuidedNodes` and emits `GUIDED_FILE_EXTENT_LOOKUP_CYCLE_DETECTED`.

No APFS traversal code change was made in V1.6.6.6 because the requested cycle guard was already present in the reviewed baseline.

### iOS bplist / NSKeyedArchiver recovery

Verified present in `src/parsers/ios_app_db_parser.cpp`:

- `ripBplistStrings`
- `resolveUid` with object-reference size support
- trailer parsing in `unflattenNSKeyedArchiverOrBplist`
- use of `unflattenNSKeyedArchiverOrBplist` for generic payload text beginning with `bplist`

No blind replacement was made because the reviewed baseline already had a bounded bplist object-table resolver. The implementation remains intentionally bounded and falls back to printable string ripping when object-table parsing is unsafe or unresolved.

### iOS Spotlight/native DB mismatch view

Verified present in `src/db/case_db.cpp` and `src/gui/view_registry.cpp` as `vw_ios_spotlight_comms_missing_from_ffs`.

Gap found: `src/gui/win32_gui.cpp` has a small GUI bootstrap schema used when opening cases in the GUI. That bootstrap did not contain the same view. V1.6.6.6 adds the view there as well and prioritizes the view in both GUI sort lists.

The wording intentionally avoids asserting deletion as the only explanation. The view status is `COMMUNICATION_PRESENT_IN_SPOTLIGHT_NOT_MATCHED_TO_NATIVE_APP_DB`, and the interpretation lists deletion, app removal, encryption/inaccessibility, parser coverage limits, or unmatched identifiers as possibilities requiring source review.

### `tel:` / `mailto:` identity fallback

Verified present in `deriveIosCommunicationFields` in `src/parsers/ios_app_db_parser.cpp`:

- `tel:` bounded extraction using `printableWindow`
- `mailto:` bounded extraction using `printableWindow`
- provenance marker `IDENTITY_REGEX_RECOVERY=True`

No parser change was made in V1.6.6.6 because the requested fallback was already present in the reviewed baseline.

## V1.6.6.6 code changes

- Added `vw_ios_spotlight_comms_missing_from_ffs` to the GUI bootstrap schema in `src/gui/win32_gui.cpp`.
- Added `iOS - Spotlight Comms Missing From Native DB` to the GUI iOS priority list in `src/gui/win32_gui.cpp`.
- Added `iOS - Spotlight Comms Missing From Native DB` to the registry priority list in `src/gui/view_registry.cpp`.
- Added V1.6.6.6 release-readiness checks requiring the view in `case_db.cpp`, `win32_gui.cpp`, and `view_registry.cpp`.
- Added V1.6.6.6 forensic-directive checks for APFS cycle guards, bplist resolver, identity fallbacks, and native-DB mismatch view registration.
- Bumped version metadata and current wrappers to V1.6.6.6.

## Test determination

Run Windows/MSVC build first because V1.6.6.6 changes `win32_gui.cpp`, current wrappers, and release-readiness tooling.

Run iOS thin after the Windows build passes because V1.6.6.6 changes GUI/bootstrap iOS communication/native-DB comparison surfaces and release-readiness validation around iOS views.

AFF4/APFS thin/full is not required for V1.6.6.6 unless the Windows build, shared schema initialization, or APFS/AFF4 validation checks regress. The APFS source was audited for the requested cycle guards but not modified.
