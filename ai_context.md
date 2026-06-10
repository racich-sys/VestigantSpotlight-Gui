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
