# V1.6.6.5 Thin Upload Review and Release Notes

## Trigger

The uploaded V1.6.6.4 Windows build log completed without a compiler/linker failure and reported `Vestigant Spotlight v1.6.6.4`.

The uploaded V1.6.6.4 iOS thin bundle completed successfully:

```text
2026-06-10T18:10:13Z complete_success
```

## V1.6.6.4 thin evidence reviewed

From `case_summary.json` in the uploaded thin bundle:

```text
source_count=1
store_count=6
valid_store_count=6
database_candidate_count=12
valid_database_candidate_count=12
parser_selected_database_count=6
native_decode_mode=CoreFields
metadata_values_decoded=true
raw_record_count=344445
raw_key_value_count=22569
artifact_count=344445
usage_evidence_count=228699
timeline_event_count=277823
orphaned_or_deleted_candidate_count=0
```

The three V1.6.6.2 timeout-prone thin exports no longer timed out in the V1.6.6.4 upload.

## Issue found

The thin bundle proved that high-value compact native string probes were present, but they were not flowing into the main iOS text-context and communication review views.

`exports/ios_string_probe_category_summary.csv` showed:

```text
MESSAGE_TEXT_OR_MESSAGE_APP=9591
URL_OR_WEB_LINK=6748
FILE_OR_IOS_PATH=5195
EMAIL_ADDRESS_OR_ACCOUNT=933
CLOUD_STORAGE_OR_SYNC=98
CALENDAR_OR_INVITATION=2
OTHER_STRING_PROBE=2
```

However, the same thin run still exported zero rows for several message/text-context review surfaces. The issue was not absence of strings; it was that compact CoreSpotlight native probe fields named `__native_core_probe_string_*` were retained as probe rows but not admitted into the row-level text-context and communication-review SQL path.

One performance issue also remained: `ios_spotlight_investigator_overview.csv` completed but took 46 seconds and was marked `slow_complete`.

## Implemented in V1.6.6.5

- Updated `src/parsers/native_storedb_parser.cpp` so high-signal `__native_core_probe_string_*` values that look like forensic reference values are eligible for same-record text context.
- Canonicalized native probe labels in compact text context as `__native_core_probe_string=<value>` instead of exposing only numbered probe field names.
- Updated `src/db/case_db.cpp` iOS text-context review views to:
  - include `source_field_name`;
  - include `__native_core_probe_string_%` rows;
  - classify native SMS/iMessage/mail/account/file-reference probes as message or mail/account context.
- Updated iOS communication-review views to:
  - aggregate `native_probe_context_count`;
  - expose `native_probe_context_sample`;
  - add `SPOTLIGHT_MESSAGE_OR_ATTACHMENT_TEXT_PROBE`;
  - add `SPOTLIGHT_MAIL_OR_ACCOUNT_TEXT_PROBE`;
  - include native probe fallback text in investigator-visible review fields.
- Updated iOS message text/body/media views and GUI registry columns so native probe counts and samples are visible in the relevant investigator views.
- Reworked `vw_ios_spotlight_investigator_overview` to use lightweight base-table/probe counts instead of forcing heavier joined review views during thin export.
- Added `runIosCoreProbeTextContextSmokeTest` to `tests/main.cpp`.

## Validation performed locally

```text
Linux CMake build: PASS
CLI version: Vestigant Spotlight v1.6.6.5
Self-test: PASS
New iOS core probe text-context smoke test: PASS
Windows/MSVC build: not run in this environment
PowerShell release-readiness execution: not run in this environment
```

## Test scope decision

Run the Windows/MSVC build wrapper for V1.6.6.5 first.

Run iOS thin again after the Windows build passes. The expected validation target is that the prior native probe/string evidence begins appearing in the iOS text-context and communication review exports and that `ios_spotlight_investigator_overview.csv` no longer reports as a 46-second `slow_complete`.

AFF4/APFS thin or full testing is not required for V1.6.6.5 unless the Windows build or shared schema/view initialization regresses. V1.6.6.5 does not change AFF4/APFS traversal, AFF4 source reading, APFS filesystem reading, decompression, copy-out, or Store-V2 staging logic.
