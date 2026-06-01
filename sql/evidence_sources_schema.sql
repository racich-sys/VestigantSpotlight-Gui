-- Vestigant Spotlight V0_7_13 evidence-source inventory schema.
-- Import into a case database when sqlite3.exe is available:
-- sqlite3.exe "Q:\SpotlightCase\YourCase\VestigantSpotlight.case.sqlite" ".read evidence_sources_schema.sql"

CREATE TABLE IF NOT EXISTS evidence_sources (
    evidence_source_id TEXT PRIMARY KEY,
    source_type TEXT,
    original_path TEXT,
    original_size INTEGER,
    original_sha256 TEXT,
    directory_manifest_path TEXT,
    directory_manifest_sha256 TEXT,
    directory_manifest_file_count INTEGER,
    container_member_path TEXT,
    extracted_path TEXT,
    extracted_sha256 TEXT,
    extracted_manifest_path TEXT,
    extracted_manifest_file_count INTEGER,
    parser_route TEXT,
    source_notes TEXT,
    unsupported_reason TEXT,
    detected_store_v2_count INTEGER,
    detected_corespotlight_count INTEGER,
    started_utc TEXT,
    completed_utc TEXT
);

CREATE INDEX IF NOT EXISTS idx_evidence_sources_source_type ON evidence_sources(source_type);
CREATE INDEX IF NOT EXISTS idx_evidence_sources_parser_route ON evidence_sources(parser_route);
CREATE INDEX IF NOT EXISTS idx_evidence_sources_original_sha256 ON evidence_sources(original_sha256);
