# Vestigant Spotlight Source Intake Plan

Version: 0.9.31

## Source summary

- Source ID: `d01045cf686b4431`
- Input path: `F:/0446_0001-IT006/00008130-001A75AA1A21001C-2025-12-03-T224939/00008130-001A75AA1A21001C_files_full.zip`
- Input type: `ZIP_SPOTLIGHT_OR_FILESYSTEM_CONTAINER`
- Source kind: `zip_spotlight_source`
- Probe bytes scanned: `67108864`
- Probe truncated: `1`
- Probe signature count: `2`
- Filesystem hints: ``
- Spotlight hints: ``
- Partition scheme: `NONE_DETECTED`
- Partition entries: `0`
- Working root: `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted`
- Parse status: `DISCOVERY_COMPLETE`
- Next action: Proceed to parser selection and native parsing for supported folder/ZIP sources.

## Implemented in this build

- Loose folder Store-V2/CoreSpotlight `store.db` discovery and parsing.
- ZIP source registration, hashing, extraction to controlled staging, and Store-V2/CoreSpotlight discovery from the staged working copy.
- AFF4-first/APFS-first image inventory readiness reporting without creating a second evidentiary archive.
- Bounded source signature probing for ZIP/AFF4 hints, MBR/GPT, APFS NXSB, HFS/HFS+, and Spotlight/CoreSpotlight path strings.
- Raw image partition-map readiness reporting for MBR/protective MBR and GPT entries, retained as secondary to AFF4/APFS work.
- Image-file-inventory and active-file-comparison readiness artifacts for future Spotlight-indexed versus APFS-present/missing comparison.
- Clear unsupported-container status for AFF4/raw sources rather than ambiguous zero-artifact cases.

## Not yet implemented

- AFF4 container stream enumeration/reading. This is now the prioritized image path because most expected Spotlight-containing images are AFF4/APFS.
- Raw image filesystem extraction from detected partition entries. Partition entries are reported for readiness/provenance only.
- APFS filesystem enumeration from AFF4-backed disk streams; HFS/HFS+ remains lower priority.
- Extraction of `.Spotlight-V100` / CoreSpotlight folders from image files.

## Source probe signatures

| Signature | Category | Offset | Confidence | Notes |
|---|---|---:|---|---|
| `ZIP_LOCAL_FILE_HEADER` | container | 0 | HIGH | File begins with ZIP local-file header. AFF4 containers are commonly ZIP-structured containers. |
| `AFF4_STRING` | container | 31455635 | LOW | AFF4 string found during bounded scan. Use with extension/header evidence; not a complete AFF4 validation. |

## Raw image partition probe

No raw image partition entries were reported. This is expected for folder, ZIP, AFF4, missing, non-raw, or unrecognized sources.

## Planned architecture

1. Container reader layer: ZIP first, then AFF4, then raw flat image.
2. Filesystem reader layer: APFS plus HFS/HFS+ enumeration without relying on Windows mounting.
3. Spotlight artifact locator: find `.Spotlight-V100/Store-V2`, iOS CoreSpotlight stores, and related metadata databases inside staged/extracted filesystems.
4. Normalized staging: copy extracted Spotlight artifacts to case-controlled working folders with source provenance and hashes.
5. Existing parser/enrichment/review pipeline consumes staged Spotlight artifacts unchanged.

## Store discovery summary

- Database candidates: 12
- Valid database candidates: 12
- Store groups: 6
- Valid store groups: 6

## First discovered store candidates

| Valid | Role | Store GUID | Path | Error |
|---:|---|---|---|---|
| 1 | dot_store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_MobileMailIndex_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/MobileMailIndex/index.spotlightV2/.store.db` |  |
| 1 | store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_MobileMailIndex_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/MobileMailIndex/index.spotlightV2/store.db` |  |
| 1 | dot_store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_NSFileProtectionComplete_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/NSFileProtectionComplete/index.spotlightV2/.store.db` |  |
| 1 | store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_NSFileProtectionComplete_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/NSFileProtectionComplete/index.spotlightV2/store.db` |  |
| 1 | dot_store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_NSFileProtectionCompleteUnlessOpen_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/NSFileProtectionCompleteUnlessOpen/index.spotlightV2/.store.db` |  |
| 1 | store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_NSFileProtectionCompleteUnlessOpen_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/NSFileProtectionCompleteUnlessOpen/index.spotlightV2/store.db` |  |
| 1 | dot_store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_NSFileProtectionCompleteUntilFirstUserAuthentication_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/NSFileProtectionCompleteUntilFirstUserAuthentication/index.spotlightV2/.store.db` |  |
| 1 | store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_NSFileProtectionCompleteUntilFirstUserAuthentication_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/NSFileProtectionCompleteUntilFirstUserAuthentication/index.spotlightV2/store.db` |  |
| 1 | dot_store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_NSFileProtectionCompleteWhenUserInactive_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/NSFileProtectionCompleteWhenUserInactive/index.spotlightV2/.store.db` |  |
| 1 | store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_NSFileProtectionCompleteWhenUserInactive_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/NSFileProtectionCompleteWhenUserInactive/index.spotlightV2/store.db` |  |
| 1 | dot_store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_Priority_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/Priority/index.spotlightV2/.store.db` |  |
| 1 | store_db | `ios_private_var_mobile_Library_Spotlight_CoreSpotlight_Priority_index.spotlightV2` | `Q:/SpotlightCase/TestiOS_WhatsApp_V0_9_4/EvidenceStaging/zip_source/extracted/private/var/mobile/Library/Spotlight/CoreSpotlight/Priority/index.spotlightV2/store.db` |  |
