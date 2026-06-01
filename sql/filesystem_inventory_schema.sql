-- Vestigant Spotlight optional filesystem inventory schema.
-- This schema is intentionally separate from the main case database so full
-- filesystem comparison can be enabled without bloating normal Spotlight review.

CREATE TABLE IF NOT EXISTS fs_sources (
    fs_source_id INTEGER PRIMARY KEY,
    source_id TEXT,
    container_type TEXT,
    container_path TEXT,
    container_sha256 TEXT,
    registered_utc TEXT,
    notes TEXT
);

CREATE TABLE IF NOT EXISTS fs_volumes (
    fs_volume_id INTEGER PRIMARY KEY,
    fs_source_id INTEGER NOT NULL,
    volume_sequence INTEGER,
    volume_role TEXT,
    volume_name TEXT,
    volume_uuid TEXT,
    filesystem_type TEXT,
    block_size INTEGER,
    apfs_fs_oid INTEGER,
    apfs_omap_oid INTEGER,
    apfs_root_tree_oid INTEGER,
    parse_status TEXT,
    notes TEXT,
    FOREIGN KEY(fs_source_id) REFERENCES fs_sources(fs_source_id)
);

CREATE TABLE IF NOT EXISTS fs_nodes (
    fs_node_id INTEGER PRIMARY KEY,
    fs_volume_id INTEGER NOT NULL,
    object_id INTEGER,
    parent_object_id INTEGER,
    object_type TEXT,
    name TEXT,
    normalized_path TEXT,
    path_confidence TEXT,
    logical_size INTEGER,
    allocated_size INTEGER,
    created_utc TEXT,
    modified_utc TEXT,
    accessed_utc TEXT,
    changed_utc TEXT,
    birth_utc TEXT,
    flags TEXT,
    owner_id INTEGER,
    group_id INTEGER,
    source_record_type TEXT,
    source_physical_ref TEXT,
    deleted_or_unlinked_status TEXT,
    parse_status TEXT,
    FOREIGN KEY(fs_volume_id) REFERENCES fs_volumes(fs_volume_id)
);

CREATE INDEX IF NOT EXISTS idx_fs_nodes_volume_object ON fs_nodes(fs_volume_id, object_id);
CREATE INDEX IF NOT EXISTS idx_fs_nodes_volume_parent ON fs_nodes(fs_volume_id, parent_object_id);
CREATE INDEX IF NOT EXISTS idx_fs_nodes_path ON fs_nodes(normalized_path);
CREATE INDEX IF NOT EXISTS idx_fs_nodes_name ON fs_nodes(name);

CREATE TABLE IF NOT EXISTS fs_extents (
    fs_extent_id INTEGER PRIMARY KEY,
    fs_node_id INTEGER NOT NULL,
    logical_offset INTEGER,
    physical_block INTEGER,
    byte_count INTEGER,
    extent_source TEXT,
    confidence TEXT,
    FOREIGN KEY(fs_node_id) REFERENCES fs_nodes(fs_node_id)
);

CREATE INDEX IF NOT EXISTS idx_fs_extents_node ON fs_extents(fs_node_id);

CREATE TABLE IF NOT EXISTS fs_spotlight_targets (
    fs_spotlight_target_id INTEGER PRIMARY KEY,
    fs_node_id INTEGER,
    normalized_path TEXT,
    target_type TEXT,
    extract_status TEXT,
    extracted_path TEXT,
    extracted_sha256 TEXT,
    notes TEXT,
    FOREIGN KEY(fs_node_id) REFERENCES fs_nodes(fs_node_id)
);

CREATE INDEX IF NOT EXISTS idx_fs_spotlight_targets_path ON fs_spotlight_targets(normalized_path);

CREATE TABLE IF NOT EXISTS fs_compare_spotlight (
    fs_compare_id INTEGER PRIMARY KEY,
    spotlight_artifact_id INTEGER,
    fs_node_id INTEGER,
    match_type TEXT,
    match_confidence TEXT,
    match_reason TEXT,
    compared_utc TEXT
);

CREATE INDEX IF NOT EXISTS idx_fs_compare_spotlight_artifact ON fs_compare_spotlight(spotlight_artifact_id);
CREATE INDEX IF NOT EXISTS idx_fs_compare_spotlight_node ON fs_compare_spotlight(fs_node_id);
