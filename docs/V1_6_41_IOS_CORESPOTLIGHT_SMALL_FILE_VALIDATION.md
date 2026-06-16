# V1.6.41.1 iOS CoreSpotlight Small-File Validation

## Evidence reviewed

Uploaded package: `Upload_iOS_CoreSpotlight_SmallFiles.zip`

Package SHA256 observed locally:

```text
250eea085bb385958e41830db5be2f2492134737a5377af80cc575d7df28e4e5
```

The package contained 588 ZIP entries and included Store-V2/CoreSpotlight files from MobileMailIndex, Priority, NSFileProtectionComplete, NSFileProtectionCompleteUnlessOpen, and NSFileProtectionCompleteWhenUserInactive.

The package full inventory showed the `NSFileProtectionCompleteUntilFirstUserAuthentication/index.spotlightV2/store.db` and `.store.db` files were each 281,710,592 bytes and were excluded by the 100 MB packaging cap. Re-run the small-file package with a higher cap, or upload those two files separately, when validating that protection class.

## Defects found in V1.6.38

1. iOS CoreSpotlight Store-V2 object IDs can exceed signed 64-bit range. V1.6.38 stored native record identifiers in signed 64-bit fields and rejected these records during header parsing. On this package, the V1.6.38 parser produced 18,987 `metadata_item_parse` failures with messages of the form `v2 inode id exceeds int64 range`.

2. iOS compact native persistence suppressed the Store-V2 header `Last_Updated` timestamp from `raw_date_candidates`. This caused iOS CoreSpotlight timeline materialization to be empty even when the native header timestamp was available on the record.

3. A CSV export path could write invalid UTF-8 bytes from raw SQLite text values. Null/control-byte visibility had already been improved, but invalid UTF-8 bytes were still being emitted unchanged.

## V1.6.41.1 changes

1. Store-V2 item identifiers are treated as unsigned 64-bit values in the native parser. This applies to inode/object ID, item ID, and parent object ID. Values are still stored as decimal text in SQLite.

2. The Store-V2 header `Last_Updated` value is always persisted as a native date candidate when present, including iOS compact mode.

3. SQLite CSV exports now preserve valid UTF-8 and replace invalid UTF-8 bytes with visible `[0xNN]` markers. Existing `[NUL]`, `[0xNN]`, `\\r`, `\\n`, and `\\t` handling remains in place.

## Local validation after patch

The V1.6.41.1 Linux build completed and the schema/iOS/APFS smoke test passed:

```text
Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v1.6.41.1
```

The uploaded iOS small-file package was rerun through the V1.6.41.1 Linux CLI using profile `ios`, CoreFields native decode, diagnostics export profile, a 50,000-record cap, and a 100,000-native-block cap.

Observed SQLite case metrics after V1.6.41.1:

```text
raw_records=38340
raw_key_values=2785
raw_date_candidates=38340
raw_failures=0
artifacts=38340
timeline_events=38340
native_property_dictionary=551
native_category_dictionary=13115
records_with_0_dates=0
```

Store coverage from the same run:

```text
ios_MobileMailIndex_index.spotlightV2=34356 records
ios_Priority_index.spotlightV2=3893 records
ios_NSFileProtectionComplete_index.spotlightV2=87 records
ios_NSFileProtectionCompleteWhenUserInactive_index.spotlightV2=2 records
ios_NSFileProtectionCompleteUnlessOpen_index.spotlightV2=2 records
```

The previously problematic `ios_spotlight_communication_summary.csv` was readable as strict UTF-8 after the CSV export sanitizer change.

## Remaining validation gap

The uploaded small-file package did not include the large `NSFileProtectionCompleteUntilFirstUserAuthentication/index.spotlightV2/store.db` and `.store.db` files due to the 100 MB file cap. Those files should be collected with a larger cap for full iOS CoreSpotlight validation.
