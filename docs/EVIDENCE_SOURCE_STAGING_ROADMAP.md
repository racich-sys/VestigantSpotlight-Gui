# Evidence Source Staging Roadmap

## Goal

Separate evidence acquisition/staging from Spotlight parsing.

The parser should receive normalized staged Spotlight/CoreSpotlight artifacts. It should not need to know whether the original source was a folder, ZIP archive, raw image, or AFF4 container.

## Supported source types to add

1. Directory source
   - Direct Mac `Store-V2` directory.
   - Parent folder containing one or more Spotlight stores.
   - Extracted iOS full-file-system folder.

2. ZIP source
   - ZIP containing the actual Mac Spotlight directory.
   - ZIP containing a full-file-system iOS extraction.
   - ZIP containing extracted CoreSpotlight databases/artifacts.

3. Disk image / container source
   - Raw `.img` / `.dd` flat image.
   - AFF4 Mac image/container.
   - Later: E01/L01 if stable libraries are available.

## Workflow

Input evidence source -> source detector -> hash source -> stage/extract -> artifact locator -> parser route -> SQLite case database -> GUI review.

## Key rule

Mounting APFS/HFS/HFS+ on Windows must not be required. Mounting can be optional, but the preferred workflow is extractor-assisted staging.

## Database provenance fields

Future source tables should track:

- source_id
- source_type
- original_path
- original_name
- original_size_bytes
- original_sha256
- container_member_path
- staged_root_path
- staged_artifact_path
- staged_artifact_sha256
- detected_platform
- detected_artifact_type
- parser_route
- staging_status
- extraction_notes

## Phased implementation

### Phase 1: ZIP support

- Hash ZIP.
- Extract ZIP to controlled staging folder.
- Locate Mac Spotlight Store-V2 artifacts.
- Locate iOS CoreSpotlight candidate databases.
- Preserve member path provenance.

### Phase 2: iOS CoreSpotlight intake

- Discover CoreSpotlight SQLite `index.db` files and related metadata.
- Register iOS sources separately from Mac Store-V2 sources.
- Create iOS parser interface and schema adapters.

### Phase 3: Image/container source registration

- Accept `.img`, `.dd`, and AFF4 paths as evidence sources.
- Hash and register source.
- Report extraction support status.
- Use external/helper extraction modules where available.

### Phase 4: Image/container Spotlight extraction

- Enumerate APFS/HFS/HFS+ contents without relying on Windows mounting.
- Extract only Spotlight/CoreSpotlight-relevant files into staging.
- Hash staged artifacts and preserve original path provenance.
