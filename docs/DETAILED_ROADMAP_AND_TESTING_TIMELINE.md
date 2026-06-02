# Vestigant Spotlight / Spotlight2 Detailed Roadmap and Testing Timeline

Current version: 0.9.36
Last reviewed input: V0_9_32 Windows build log and V0_9_32 iOS reuse-cache thin upload.

## Current status from V0_9_32

V0_9_32 built successfully on Windows/MSVC, including GUI, CLI, and self-test binaries. The iOS reuse-cache run reached `complete_success`.

Observed V0_9_32 run metrics:

| Area | V0_9_32 result |
|---|---:|
| iOS CoreSpotlight stores parsed | 6 |
| Raw Spotlight/CoreSpotlight records | 344,445 |
| Compact raw key/value rows | 982,230 |
| Compact date candidates | 336,037 |
| FFS slim lookup rows imported from cache | 1,592,440 |
| App databases found in cache inventory | 11,248 |
| Full FFS inventory materialized in normal mode | No |
| Broad app DB parsed records materialized in normal mode | No |
| Native record limit | Unlimited |
| Run completion | complete_success |

Interpretation: the DB/WAL bloat and export-stall classes that affected V0_9_15 through V0_9_20 are not currently recurring in the normal iOS reuse-cache workflow. The main remaining work is investigator usability, parser coverage, defensibility, and source-mode expansion.

## Development principles going forward

1. Keep Spotlight/CoreSpotlight parsing as the primary product goal.
2. Keep normal iOS investigator runs compact by default.
3. Treat full FFS inventory, app database parsing, and active file comparison as support/correlation workflows unless explicitly enabled.
4. Preserve raw locators, source provenance, parser version, parser limits, and suppression/diagnostic summaries.
5. Make each new investigator view answer a specific review question.
6. Add smoke tests for schema/views and avoid broad, risky refactors unless tied to a failing build/runtime class.

## Near-term iOS Spotlight roadmap

### Phase 1: Stabilize investigator starting views

Status: in progress.

Primary GUI start order:

1. iOS - Investigator Overview
2. iOS - Case Quality Dashboard
3. iOS - Direct User Message Review
4. iOS - Direct User Message Thread Summary
5. iOS - Spotlight Message Body Review
6. iOS - Spotlight Message Media Review
7. iOS - High-Value Missing From FFS
8. iOS - Normalized Spotlight Timeline
9. iOS - Parser Diagnostics Action Summary
10. iOS - Case Provenance Summary

Next work:

- Add clearer view descriptions/help text for these views.
- Add a compact `what to review first` export and GUI view.
- Keep large row-level samples bounded while preserving full local exports where appropriate.
- Add direct filters for `USER_REVIEW_CANDIDATE`, high-value missing paths, communications, attachments, and date anomalies.

### Phase 2: Improve message/body/media interpretation

Status: active.

Next work:

- Continue improving extraction of message-like text from compact Spotlight context.
- Separate direct message records, message-adjacent media, mail, phone/FaceTime, and third-party chat-app references.
- Add more evidence columns explaining why a record was classified as Messages/Mail/WhatsApp/Signal/Telegram/Phone.
- Reduce false positives from app names or ordinary text mentions.
- Add app/container attribution where bundle ID, domain identifier, container path, or AppGroup mapping supports it.

### Phase 3: Add defensibility and parser visibility

Status: started.

Next work:

- Expand parser diagnostics detail and action summaries.
- Show unparsed/unsupported property ranges in GUI.
- Add severity and recommended action for decode gaps.
- Preserve parser version, run settings, source hash status, source path, cache source, and limits/suppression settings in a single case-quality/provenance panel.
- Add schema/view smoke tests for every new view before packaging.

### Phase 4: Add normalized timeline workflow

Status: started.

Next work:

- Expand normalized timeline beyond message/media evidence where date provenance supports it.
- Add anomaly flags for improbable date ordering, snapshot/index dates, future dates, and source/export snapshot artifacts.
- Add Plaso/L2T export mode for timeline events.
- Preserve source field, raw date value, normalized UTC date, interpretation, and confidence.

### Phase 5: Add investigator workflow features

Status: planned.

Next work:

- Add persistent tags/notes for records and artifacts.
- Add GUI filters/toggles for noise categories and high-value-only review.
- Add targeted export from selected/filtered rows.
- Add JSONL export for selected high-value views.
- Add optional keyword-list search across compact Spotlight text context, paths, URLs, accounts, contacts, and validation locators.

## Testing source timeline

### Stage A: Reuse-cache development loop

Use this while actively changing parsing/views/export logic.

Test source:

- Input ZIP: `F:\0446_0001-IT006\00008130-001A75AA1A21001C-2025-12-03-T224939\00008130-001A75AA1A21001C_files_full.zip`
- Reuse cache: `Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_4`

Purpose:

- Fast iteration without repeated full ZIP inventory/extraction.
- Validate CoreSpotlight parsing, compact DB behavior, GUI views, exports, and thin upload packaging.

Continue using reuse-cache testing until these are stable for several consecutive versions:

- Windows build succeeds including GUI, CLI, and self-test.
- Run reaches `complete_success`.
- DB/WAL stay bounded.
- No export query stalls.
- Investigator overview, direct message review, missing-from-FFS, parser diagnostics, and timeline views are useful.
- Thin upload stays reasonably bounded and contains the needed review samples.

### Stage B: Same source, fresh full ZIP workflow

Switch to this after the reuse-cache workflow is stable enough to verify that the program can build a fresh staging/cache without hidden reliance on the old cache.

Purpose:

- Validate direct ZIP entry discovery and extraction.
- Validate FFS slim path lookup creation from the actual ZIP rather than a prior cache.
- Validate whether Defender/ZIP read bottlenecks recur.

Recommended trigger:

- After 2-3 successful reuse-cache iterations with no build failure, no DB/export stall, and useful investigator views.

Expected next commands should use the full-parse script instead of the reuse-cache script. Keep `--skip-container-hash` only for development if the source has already been externally verified; use full source/container hashing for forensic/final validation.

### Stage C: Support/correlation iOS run with selective materialization

Run only after Stage B succeeds.

Purpose:

- Validate selected support features without returning to DB bloat.
- Test `--materialize-ios-ffs-inventory` and `--materialize-ios-app-db-records` on bounded/support runs.
- Correlate Spotlight message/media/path references against Messages, WhatsApp, and other app DB records.

Guardrails:

- Use explicit limits or support profile.
- Monitor DB/WAL and export sizes.
- Keep full app DB records out of normal mode.

### Stage D: Alternate iOS FFS sources

Use after the main WhatsApp-heavy source is stable.

Purpose:

- Check generality against other iOS full filesystem extractions.
- Confirm AppGroup/container mapping, protection-class differences, and CoreSpotlight folder variation.
- Validate that views are not overfit to one acquisition.

Recommended source types:

- Smaller known-good iOS FFS ZIP.
- iOS FFS folder or ZIP set with different app mix.
- Extraction with Apple Messages-heavy data.
- Extraction with few communications artifacts to verify low/no-hit behavior.

## macOS AFF4/APFS roadmap

AFF4/APFS remains important but should not displace current iOS CoreSpotlight investigator usability work.

### AFF4/APFS Phase 1: Build stability and preservation

- Keep using one explicitly selected AFF4 source at a time.
- Hash/register the original AFF4 container.
- Do not create a second evidentiary container when the original input is already a forensic container.
- Preserve source provenance, stream paths, APFS volume identifiers, and extraction status.

Primary test source:

`O:\0109_0142-IT001\disk3 2024-10-01 10-43-40\0109_0142-IT001.aff4`

### AFF4/APFS Phase 2: APFS filesystem inventory

- Improve AFF4 stream enumeration.
- Identify APFS containers/volumes reliably.
- Read APFS object map / volume metadata sufficient for file inventory.
- Export image inventory rows with file paths, inodes/CNIDs/object IDs, sizes, hashes where available, and provenance.

### AFF4/APFS Phase 3: Spotlight extraction from APFS

- Locate `.Spotlight-V100/Store-V2` inside APFS volumes without relying on mounted filesystems.
- Stage Store-V2 groups with source stream/offset/path provenance.
- Compare staged Vestigant Store-V2 copy-out against known external extraction reference:
  `T:\ExternalExtractedSpotlight\.Spotlight-V100\Store-V2`
- Continue using focus group:
  `BADA95B6-4657-4C31-9FBF-C9754AB13701`

### AFF4/APFS Phase 4: Active file comparison

- Compare Spotlight-indexed paths/inodes to image-backed APFS file inventory.
- Classify present, missing from image inventory, unresolved, duplicate/hash-match different path, and path/hash mismatches.
- Keep comparison outputs separate from the Spotlight parser core and clearly label limitations.

### AFF4/APFS Phase 5: Investigator GUI parity

- Expose macOS Store-V2 investigator views in the GUI with the same quality as iOS views.
- Include WhereFroms, LastUsed, URL/referrer evidence, app usage, download evidence, path reconstruction, parent inode context, timeline, and parser diagnostics.

## Backburner items

Useful but not immediate:

- Full Relativity/eDiscovery load-file export.
- NSRL/hashset filtering.
- Full Win32 MainWindow/global-state rewrite.
- Broad magic-string enum refactor.
- Mass APFS/AFF4 refactor not tied to observed failures.

Implement these after iOS Spotlight review and core AFF4/APFS workflows are stable.
