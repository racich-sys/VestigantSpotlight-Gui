# Evidence Source Staging Workflow — V0_7_13

## Intended flow

Input Evidence Source
-> Evidence Source Detector
-> Evidence Stager / Extractor
-> Normalized Working Evidence Folder
-> Mac Store-V2 parser OR iOS CoreSpotlight parser
-> SQLite case database
-> GUI review / tags / notes / exports

## Supported in V0_7_13

### Folder source

A folder source can be:
- a Store-V2 folder;
- a parent folder containing one or more Store-V2 folders;
- an extracted evidence folder containing likely iOS CoreSpotlight artifacts.

Default behavior copies the folder into the case staging area. Use `-NoCopyFolder` only when intentionally referencing the folder in place.

### ZIP source

A ZIP source can be:
- a ZIP containing the actual Mac Spotlight directory;
- a ZIP containing a larger Mac evidence subset with Store-V2 somewhere below it;
- a ZIP containing an iOS full-file-system extraction or CoreSpotlight subset.

The ZIP itself is hashed, then extracted into the case staging area. Detection is run against the extracted tree.

### Unsupported registered-only source

V0_7_13 identifies but does not extract:
- `.img`
- `.dd`
- `.aff4`

These are registered with unsupported reasons to preserve investigator intent and source provenance without pretending extraction occurred.

## Outputs

Outputs are written under:

`<CaseOut>\EvidenceInventory\`

For each source:
- `<evidence_source_id>_source_inventory.json`
- `<evidence_source_id>_source_inventory.csv`
- `<evidence_source_id>_source_inventory.sql`
- `<evidence_source_id>_detection_report.txt`
- optional original folder manifest CSV
- optional staged/extracted manifest CSV

Staged evidence is written under:

`<CaseOut>\EvidenceStaging\<evidence_source_id>\`

## Later IMG/AFF4 filesystem boundary

AFF4/IMG support requires more than opening the outer container. Later V0_8.x work must also enumerate partitions/volumes and read APFS, HFS, or HFS+ filesystems in a read-only way. Mounting on Windows should not be the primary workflow. The intended future boundary is:

```text
SourceContainerReader -> VolumeEnumerator -> FilesystemReader -> ArtifactLocator -> EvidenceStager
```

Candidate future research items include evaluating Velocidex `c-aff4` for AFF4 container reading and separate APFS/HFS/HFS+ readers for filesystem enumeration/extraction.
