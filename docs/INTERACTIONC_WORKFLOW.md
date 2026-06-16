# CoreDuet interactionC Workflow - V1.6.38

V1.6.38 adds `vw_ios_coreduet_interactionc_validation_checks` and the bounded sample `ios_coreduet_interactionc_validation_checks_sample.csv`. Validate that:

- `ZINTERACTIONS` table inventory exists;
- parsed event rows reconcile to canonical `ZINTERACTIONS`;
- join tables are not promoted to standalone events;
- `Phone:` relabeling remains suppressed for generic CoreDuet numeric identifiers;
- contextual guardrail notes are present on event rows.

# V1.6.38 Release Notes

## Purpose

V1.6.38 is a narrow iOS thin-wrapper diagnostic hotfix after the V1.6.38 thin run exited early during iOS FFS inventory and then failed post-upload validation because bounded samples were not created.

## Verified trigger from user output

- The CLI exited with code 1 during iOS FFS inventory after progress lines up to 300,000 files.
- The upload ZIP was created at `D:\Downloads\Upload_Thin_iOS_CoreSpotlight_V1_6_38.zip`.
- The focused ZIP runner then threw at `tools\Run-IosCoreSpotlightFocusedZip.ps1:240` because required bounded validation samples were absent.

## Fixed

- `tools/Run-IosCoreSpotlightFocusedZip.ps1` now treats missing required bounded samples differently when the CLI has already exited non-zero: it writes a warning and preserves the upload as an incomplete-run diagnostic bundle instead of throwing after upload creation.
- `scripts/Run-V1_6_38-iOS-CoreSpotlight-AndZip.ps1` now retains the diagnostic ZIP at the expected path when the thin CLI exits non-zero but the upload ZIP exists, and tells the user to upload that diagnostic bundle for review.
- Full/production/support wrappers still preserve stricter failure behavior; this hotfix targets the thin validation workflow only.

## Carried forward from V1.6.38

- CoreDuet `interactionC.db` precision cleanup: canonical `ZINTERACTIONS` parsing only, suppression of automatic identity/phone/thread promotion for generic CoreDuet rows, and `IDENTITY_PROMOTION_SUPPRESSED_FOR_COREDUET_INTERACTIONC=True` provenance.

## Validation status

- Static audit: `validation/V1_6_38_STATIC_AUDIT.log`.
- Windows/MSVC build: not verified here.
- V1.6.38 thin rerun: not verified here.


# V1.6.38 Precision Update

V1.6.21 proved that interactionC rows can be parsed, but generic join-table/internal numeric values could look like phone participant hints. V1.6.38 limits generic interactionC parsing to canonical `ZINTERACTIONS` rows and suppresses automatic identity promotion for generic CoreDuet interaction rows.

# V1.6.38 Parser Update

V1.6.38 adds schema-tolerant parsing for `ZINTERACTIONS` and interaction-named CoreDuet tables. Parsed rows are emitted as `COREDUET_INTERACTIONS` with provenance including `no_direct_communication_conclusion=True`. Treat these as contextual interaction evidence unless later schema-specific validation supports a narrower finding.

# CoreDuet `interactionC.db` Workflow - V1.6.38

## Purpose

`interactionC.db` is treated as a CoreDuet People / interaction-context source. It is useful as contextual activity evidence, not as a standalone conclusion engine.

## Existing source handling

- `interactionC.db`, `interactionC.db-wal`, and `interactionC.db-shm` are targeted during iOS ZIP app-database extraction.
- `interactionC.db` and `/CoreDuet/People/` paths are classified as `COREDUET_INTERACTIONS`.
- Parsed rows are exposed through CoreDuet/interactionC views and bounded upload samples.

## Investigator workflow

1. Start with `iOS - CoreDuet interactionC Database Status`.
   - Confirm whether `interactionC.db`, WAL, and SHM were staged.
   - Review table count, inventoried rows, parsed rows, date coverage, and parse status.

2. Move to `iOS - CoreDuet interactionC Summary`.
   - Review grouped event counts by CoreDuet surface, stream, app/participant hint, and parse status.
   - Use earliest/latest timestamps to identify periods of activity.

3. Use `iOS - CoreDuet interactionC Events` only for row-level review.
   - Treat snippets as bounded previews, not full database reconstruction.
   - Preserve timestamp source and provenance in notes/exports.

4. Correlate, do not overstate.
   - Compare with iOS Spotlight communication/text views, app database communication views, usage timeline rows, and Missing-from-FFS candidates.
   - Do not infer communication, deletion, or exfiltration from interactionC rows alone.

## Output files

- `exports/ios_coreduet_interactionc_database_status.csv`
- `exports/ios_coreduet_interactionc_summary.csv`
- `exports/ios_coreduet_interactionc_events.csv` in support/full exports
- `exports/upload_samples/ios_coreduet_interactionc_database_status_sample.csv`
- `exports/upload_samples/ios_coreduet_interactionc_summary_sample.csv`
- `exports/upload_samples/ios_coreduet_interactionc_events_sample.csv`

## Validation limits

Actual row meaning depends on the database schema present in the image. If a future thin shows high-value interactionC tables that are only inventoried but not semantically parsed, add a schema-specific parser after reviewing those table/column names from the uploaded evidence.
