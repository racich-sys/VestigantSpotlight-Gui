# Current Context - V1.6.6.6

- Always verify claimed missing code against the actual source before changing or claiming implementation.
- V1.6.6.5 thin completed successfully: 6 valid stores; 344,445 raw records; 42,799 raw key/value rows; 344,445 artifacts; 228,699 usage evidence rows; 277,823 timeline events; no slow/incomplete exports above threshold.
- V1.6.6.5 source already contained APFS guided traversal cycle detection, bounded bplist/NSKeyedArchiver recovery helpers, and `tel:` / `mailto:` communication identity fallback.
- V1.6.6.6 adds the missing GUI bootstrap copy of `vw_ios_spotlight_comms_missing_from_ffs` and prioritizes that view in iOS GUI sorting.
- Next validation: Windows/MSVC build and iOS thin. AFF4/APFS thin/full only if build/schema/APFS checks regress.

# ai_context.md - V1.6.6.5 current state

## Current version

V1.6.6.5.

## Current evidence

The uploaded V1.6.6.4 Windows build log completed and reported `Vestigant Spotlight v1.6.6.4`. The uploaded V1.6.6.4 iOS thin bundle reached `complete_success`.

The thin bundle showed compact native string evidence in `ios_string_probe_category_summary.csv`:
- `MESSAGE_TEXT_OR_MESSAGE_APP`: 9,591 rows.
- `EMAIL_ADDRESS_OR_ACCOUNT`: 933 rows.
- `URL_OR_WEB_LINK`: 6,748 rows.
- `FILE_OR_IOS_PATH`: 5,195 rows.

The problem addressed in V1.6.6.5 is that native probe strings were retained but not exposed in the main iOS text-context/communication-review views.

## V1.6.6.5 implementation rule

High-signal `__native_core_probe_string_*` values may feed same-record text context only after `looksLikeForensicReferenceValue` accepts them. Device/app-state KnowledgeC rows remain excluded from identity promotion. Generic `KNOWLEDGEC_EVENTS` must not be used as a positive communication/identity promotional predicate.

## Current required validation

Run Windows build, then iOS thin. AFF4/APFS thin/full is not required for this version unless Windows build/shared schema initialization regresses.
