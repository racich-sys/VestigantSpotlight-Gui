#include "db/case_db.h"
#include "core/app_info.h"
#include "core/path_utils.h"
#include <stdexcept>
#include <sstream>
#include <initializer_list>
#include <cstring>

namespace vestigant::spotlight {
namespace {

std::string joinSql(std::initializer_list<const char*> parts) {
    std::string out;
    for (const char* p : parts) {
        if (p) out += p;
    }
    return out;
}

[[noreturn]] void throwSql(sqlite3* db, const std::string& what) {
    std::string msg = what;
    if (db) {
        msg += ": ";
        msg += sqlite3_errmsg(db);
    }
    throw std::runtime_error(msg);
}
}

std::string sqlNowUtc() { return nowUtc(); }

std::string sqlQuoteIdentifier(const std::string& s) {
    std::string out = "\"";
    for (char c : s) out += (c == '"') ? "\"\"" : std::string(1, c);
    out += "\"";
    return out;
}

SqlStatement::SqlStatement(sqlite3* db, const std::string& sql) : db_(db) {
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) throwSql(db_, "sqlite prepare failed: " + sql);
}
SqlStatement::~SqlStatement() { if (stmt_) sqlite3_finalize(stmt_); }
SqlStatement::SqlStatement(SqlStatement&& other) noexcept : db_(other.db_), stmt_(other.stmt_) { other.db_ = nullptr; other.stmt_ = nullptr; }
SqlStatement& SqlStatement::operator=(SqlStatement&& other) noexcept {
    if (this != &other) {
        if (stmt_) sqlite3_finalize(stmt_);
        db_ = other.db_; stmt_ = other.stmt_; other.db_ = nullptr; other.stmt_ = nullptr;
    }
    return *this;
}
void SqlStatement::bind(int index, const std::string& value) { if (sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) throwSql(db_, "sqlite bind text failed"); }
void SqlStatement::bind(int index, long long value) { if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) throwSql(db_, "sqlite bind int64 failed"); }
void SqlStatement::bindNull(int index) { if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) throwSql(db_, "sqlite bind null failed"); }
bool SqlStatement::stepRow() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throwSql(db_, "sqlite step failed");
}
void SqlStatement::stepDone() {
    int rc = sqlite3_step(stmt_);
    if (rc != SQLITE_DONE) throwSql(db_, "sqlite step done failed");
}
void SqlStatement::reset() { sqlite3_reset(stmt_); sqlite3_clear_bindings(stmt_); }
std::string SqlStatement::colText(int index) const { const unsigned char* t = sqlite3_column_text(stmt_, index); return t ? reinterpret_cast<const char*>(t) : std::string(); }
long long SqlStatement::colInt64(int index) const { return sqlite3_column_int64(stmt_, index); }

CaseDatabase::~CaseDatabase() { close(); }
void CaseDatabase::open(const fs::path& dbPath) {
    close();
    dbPath_ = dbPath;
    fs::create_directories(dbPath.parent_path());
    if (sqlite3_open(pathString(dbPath).c_str(), &db_) != SQLITE_OK) throwSql(db_, "Unable to open SQLite case database");
    sqlite3_busy_timeout(db_, 30000);
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    exec("PRAGMA temp_store=MEMORY;");
    exec("PRAGMA foreign_keys=ON;");
    exec("PRAGMA cache_size=-200000;");
}
void CaseDatabase::close() {
    if (db_) {
        sqlite3_wal_checkpoint_v2(db_, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);
        int rc = sqlite3_close(db_);
        if (rc == SQLITE_BUSY) {
            // V0.9.42: prefer a deferred close over leaving callers with a live handle
            // when a read-only GUI statement is still unwinding.  sqlite3_close_v2()
            // arranges final cleanup after outstanding statements are finalized.
            sqlite3_close_v2(db_);
        }
        db_ = nullptr;
    }
}
void CaseDatabase::exec(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : sqlite3_errmsg(db_);
        sqlite3_free(err);
        throw std::runtime_error("sqlite exec failed: " + msg + "\nSQL: " + sql);
    }
}
SqlStatement CaseDatabase::prepare(const std::string& sql) { return SqlStatement(db_, sql); }
void CaseDatabase::begin() { exec("BEGIN IMMEDIATE;"); }
void CaseDatabase::commit() { exec("COMMIT;"); }
void CaseDatabase::rollbackNoThrow() { try { exec("ROLLBACK;"); } catch (...) {} }

void CaseDatabase::initializeSchema() {
    exec(R"SQL(
CREATE TABLE IF NOT EXISTS case_info (
  key TEXT PRIMARY KEY,
  value TEXT
);
CREATE TABLE IF NOT EXISTS evidence_sources (
  source_id TEXT PRIMARY KEY,
  profile TEXT,
  input_path TEXT,
  evidence_root TEXT,
  source_kind TEXT,
  added_utc TEXT,
  notes TEXT
);
CREATE TABLE IF NOT EXISTS source_files (
  source_id TEXT,
  relative_path TEXT,
  absolute_path TEXT,
  file_role TEXT,
  size_bytes INTEGER,
  sha256 TEXT,
  modified_utc TEXT
);
CREATE TABLE IF NOT EXISTS preserved_evidence_sets (
  preservation_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  archive_path TEXT,
  archive_format TEXT,
  archive_sha256 TEXT,
  archive_size_bytes INTEGER,
  created_utc TEXT,
  tool_used TEXT,
  tool_version TEXT,
  original_root_path TEXT,
  preserved_root_path TEXT,
  file_count INTEGER,
  total_original_bytes INTEGER,
  preservation_status TEXT,
  integrity_status TEXT,
  notes TEXT
);
CREATE TABLE IF NOT EXISTS preserved_evidence_files (
  preserved_file_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  relative_path TEXT,
  original_absolute_path TEXT,
  preserved_path TEXT,
  file_role TEXT,
  size_bytes INTEGER,
  sha256 TEXT,
  included_in_archive INTEGER,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS archive_integrity_checks (
  check_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  archive_path TEXT,
  check_type TEXT,
  check_started_utc TEXT,
  check_finished_utc TEXT,
  result TEXT,
  message TEXT
);
CREATE TABLE IF NOT EXISTS store_groups (
  source_id TEXT,
  store_guid TEXT,
  store_path TEXT,
  store_file_count INTEGER,
  has_store_db INTEGER,
  has_dotstore_db INTEGER,
  PRIMARY KEY(source_id, store_guid, store_path)
);
CREATE TABLE IF NOT EXISTS raw_records (
  raw_record_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  store_path TEXT,
  source_db TEXT,
  inode_num TEXT,
  store_id TEXT,
  parent_inode_num TEXT,
  flags TEXT,
  last_updated_raw TEXT,
  last_updated_utc TEXT,
  file_name TEXT,
  content_type TEXT,
  content_type_tree TEXT,
  where_froms TEXT,
  display_name TEXT,
  full_path TEXT,
  record_state TEXT,
  logical_size_bytes TEXT,
  physical_size_bytes TEXT
);
CREATE TABLE IF NOT EXISTS raw_key_values (
  raw_kv_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  store_path TEXT,
  source_db TEXT,
  inode_num TEXT,
  store_id TEXT,
  parent_inode_num TEXT,
  full_path TEXT,
  record_state TEXT,
  field_name TEXT,
  field_value TEXT
);
CREATE TABLE IF NOT EXISTS raw_date_candidates (
  raw_date_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  store_path TEXT,
  source_db TEXT,
  inode_num TEXT,
  store_id TEXT,
  field_name TEXT,
  field_value TEXT,
  parsed_utc TEXT,
  parse_method TEXT,
  artifact_id INTEGER,
)SQL" R"SQL(  parent_inode_num TEXT,
  file_name TEXT,
  best_path TEXT,
  date_type TEXT,
  association_status TEXT,
  association_confidence TEXT
);
CREATE TABLE IF NOT EXISTS artifact_date_summary (
  artifact_id INTEGER PRIMARY KEY,
  source_id TEXT,
  store_guid TEXT,
  inode_num TEXT,
  parent_inode_num TEXT,
  file_name TEXT,
  display_name TEXT,
  best_path TEXT,
  path_source TEXT,
  path_status TEXT,
  logical_size_bytes INTEGER,
  physical_size_bytes INTEGER,
  content_type TEXT,
  where_froms TEXT,
  created_earliest_utc TEXT,
  created_latest_utc TEXT,
  modified_earliest_utc TEXT,
  modified_latest_utc TEXT,
  downloaded_earliest_utc TEXT,
  downloaded_latest_utc TEXT,
  usage_earliest_utc TEXT,
  usage_latest_utc TEXT,
  interesting_or_index_earliest_utc TEXT,
  interesting_or_index_latest_utc TEXT,
  likely_snapshot_date_count INTEGER DEFAULT 0,
  associated_date_count INTEGER DEFAULT 0,
  unassociated_date_count INTEGER DEFAULT 0,
  available_date_fields TEXT,
  association_confidence_summary TEXT,
  snapshot_warning_reasons TEXT,
  first_date_utc TEXT,
  last_date_utc TEXT,
  total_date_count INTEGER DEFAULT 0,
  created_date_count INTEGER DEFAULT 0,
  modified_date_count INTEGER DEFAULT 0,
  downloaded_date_count INTEGER DEFAULT 0,
  usage_date_count INTEGER DEFAULT 0,
  interesting_or_index_date_count INTEGER DEFAULT 0,
  metadata_seen_or_index_updated_count INTEGER DEFAULT 0,
  other_date_count INTEGER DEFAULT 0,
  likely_snapshot_or_index_date_count INTEGER DEFAULT 0,
  interpreted_date_types TEXT,
  date_association_status TEXT,
  date_association_confidence TEXT,
  refreshed_utc TEXT
);
CREATE TABLE IF NOT EXISTS raw_failures (
  failure_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  phase TEXT,
  store_guid TEXT,
  source_db TEXT,
  message TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS volume_configuration (
  source_id TEXT,
  store_guid TEXT,
  key_path TEXT,
  value TEXT
);
CREATE TABLE IF NOT EXISTS artifacts (
  artifact_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  inode_num TEXT,
  parent_inode_num TEXT,
  file_name TEXT,
  display_name TEXT,
  best_path TEXT,
  v7_full_path_raw TEXT,
  spotlight_display_path TEXT,
  normalized_mac_path TEXT,
  filesystem_lookup_path TEXT,
  path_source TEXT,
  path_status TEXT,
  content_type TEXT,
  content_type_tree TEXT,
  where_froms TEXT,
  authors TEXT,
  creator TEXT,
  logical_size_bytes TEXT,
  physical_size_bytes TEXT,
  last_updated_utc TEXT,
  downloaded_date_utc TEXT,
  used_dates_utc TEXT,
  first_used_candidate_utc TEXT,
  last_used_date_utc TEXT,
  used_dates_count INTEGER DEFAULT 0,
  use_count_value TEXT,
  usage_field_summary TEXT,
  open_count_estimate INTEGER DEFAULT 0,
  index_text_snippet TEXT,
)SQL" R"SQL(  existence_status TEXT DEFAULT 'NOT_CHECKED',
  matched_filesystem_path TEXT,
  deleted_or_orphaned_candidate INTEGER DEFAULT 0,
  orphan_reason TEXT,
  confidence TEXT DEFAULT 'MEDIUM',
  is_mounted_volume_path INTEGER DEFAULT 0,
  mounted_volume_name TEXT,
  external_volume_reason TEXT
);
CREATE TABLE IF NOT EXISTS usage_evidence (
  usage_id INTEGER PRIMARY KEY AUTOINCREMENT,
  artifact_id INTEGER,
  source_id TEXT,
  store_guid TEXT,
  inode_num TEXT,
  field_name TEXT,
  field_value TEXT,
  parsed_utc TEXT
);
CREATE TABLE IF NOT EXISTS timeline_events (
  timeline_id INTEGER PRIMARY KEY AUTOINCREMENT,
  artifact_id INTEGER,
  source_id TEXT,
  store_guid TEXT,
  inode_num TEXT,
  event_timestamp_utc TEXT,
  event_type TEXT,
  event_source_field TEXT,
  file_name TEXT,
  path TEXT,
  existence_status TEXT,
  deleted_or_orphaned_candidate INTEGER DEFAULT 0
);
CREATE TABLE IF NOT EXISTS orphaned_deleted_candidates (
  candidate_id INTEGER PRIMARY KEY AUTOINCREMENT,
  artifact_id INTEGER,
  source_id TEXT,
  store_guid TEXT,
  inode_num TEXT,
  file_name TEXT,
  best_path TEXT,
  content_type TEXT,
  existence_status TEXT,
  orphan_reason TEXT,
  index_text_snippet TEXT
);
)SQL");
    exec(R"SQL(CREATE TABLE IF NOT EXISTS external_volume_candidates (
  candidate_id INTEGER PRIMARY KEY AUTOINCREMENT,
  artifact_id INTEGER,
  source_id TEXT,
  store_guid TEXT,
  inode_num TEXT,
  file_name TEXT,
  best_path TEXT,
  mounted_volume_name TEXT,
  reason TEXT,
  confidence TEXT,
  detection_source_field TEXT,
  detection_source_value TEXT
);
CREATE TABLE IF NOT EXISTS artifact_source_instances (
  instance_id INTEGER PRIMARY KEY AUTOINCREMENT,
  artifact_id INTEGER,
  raw_record_id INTEGER,
  source_id TEXT,
  store_guid TEXT,
  inode_num TEXT,
  source_db TEXT,
  source_db_role TEXT,
  last_updated_utc TEXT,
  file_name TEXT,
  best_path TEXT
);
CREATE TABLE IF NOT EXISTS source_copy_comparison (
  comparison_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  inode_num TEXT,
  source_instance_count INTEGER,
  has_store_db INTEGER,
  has_dotstore_db INTEGER,
  comparison_status TEXT,
  preferred_source_db TEXT
);
CREATE TABLE IF NOT EXISTS parent_inode_links (
  link_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  child_artifact_id INTEGER,
  child_inode_num TEXT,
  child_parent_inode_num TEXT,
  child_file_name TEXT,
  child_best_path TEXT,
  parent_artifact_id INTEGER,
  parent_inode_num TEXT,
  parent_file_name TEXT,
  parent_best_path TEXT,
  sibling_group_key TEXT,
  sibling_count INTEGER,
  relationship_status TEXT,
  path_reconstruction_method TEXT,
  reconstructed_path_candidate TEXT,
  confidence TEXT
);
CREATE TABLE IF NOT EXISTS field_inventory (
  field_inventory_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  field_name TEXT,
  row_count INTEGER,
  populated_count INTEGER,
  sample_value TEXT
);
CREATE TABLE IF NOT EXISTS parser_coverage_summary (
  summary_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  metric_name TEXT,
  metric_value TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS processing_phases (
  phase_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  phase_name TEXT,
  started_utc TEXT,
  finished_utc TEXT,
  input_row_count INTEGER,
  output_row_count INTEGER,
  status TEXT,
  message TEXT
);
CREATE TABLE IF NOT EXISTS tags (
  tag_id INTEGER PRIMARY KEY AUTOINCREMENT,
  artifact_id INTEGER,
  tag TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS notes (
  note_id INTEGER PRIMARY KEY AUTOINCREMENT,
  artifact_id INTEGER,
  note TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS native_property_dictionary (
  native_property_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  source_db TEXT,
  spotlight_version INTEGER,
  property_index INTEGER,
  property_name TEXT,
  prop_type_dec INTEGER,
  value_type_dec INTEGER,
  prop_type_hex TEXT,
  value_type_hex TEXT,
  is_core_native_field INTEGER DEFAULT 0,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS native_category_dictionary (
  native_category_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  source_db TEXT,
  spotlight_version INTEGER,
  category_index INTEGER,
  category_name TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS native_index_dictionary_summary (
  native_index_summary_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  source_db TEXT,
  spotlight_version INTEGER,
  map_name TEXT,
  index_rows INTEGER,
  value_ref_count INTEGER,
  max_refs_per_index INTEGER,
  max_ref_index INTEGER,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS native_dbstr_map_inventory (
  native_dbstr_map_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  source_db TEXT,
  map_id INTEGER,
  data_path TEXT,
  offsets_path TEXT,
  header_path TEXT,
  data_exists INTEGER,
  offsets_exists INTEGER,
  header_exists INTEGER,
  data_bytes INTEGER,
  offsets_bytes INTEGER,
  header_bytes INTEGER,
  offset_entries INTEGER,
  parsed_entries INTEGER,
  skipped_entries INTEGER,
  status TEXT,
  message TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS native_decode_attempts (
  native_decode_attempt_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  store_guid TEXT,
  source_db TEXT,
  decode_mode TEXT,
  spotlight_version INTEGER,
  properties_count INTEGER,
  categories_count INTEGER,
  metadata_blocks INTEGER,
  decompressed_blocks INTEGER,
  raw_records INTEGER,
  raw_key_values INTEGER,
  raw_date_candidates INTEGER,
  fallback_header_only_items INTEGER,
  failures INTEGER,
  started_utc TEXT,
  finished_utc TEXT,
  status TEXT,
  message TEXT
);
CREATE TABLE IF NOT EXISTS source_probe_runs (
  source_probe_run_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  input_path TEXT,
  input_type TEXT,
  source_kind TEXT,
  size_bytes INTEGER,
  working_root TEXT,
)SQL" R"SQL(  container_reader_status TEXT,
  filesystem_reader_status TEXT,
  spotlight_discovery_status TEXT,
  database_candidates INTEGER,
  valid_database_candidates INTEGER,
  store_groups INTEGER,
  valid_store_groups INTEGER,
  probe_bytes_scanned INTEGER,
  probe_truncated INTEGER,
  probe_signature_count INTEGER,
  partition_scheme TEXT,
  partition_entry_count INTEGER,
  filesystem_hints TEXT,
  spotlight_hints TEXT,
  parse_status TEXT,
  next_action TEXT,
  notes TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS source_probe_signatures (
  source_probe_signature_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  signature_name TEXT,
  category TEXT,
  offset_bytes INTEGER,
  confidence TEXT,
  notes TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS source_partition_probe (
  source_partition_probe_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  scheme TEXT,
  partition_index INTEGER,
  start_lba INTEGER,
  sector_count INTEGER,
  offset_bytes INTEGER,
  size_bytes INTEGER,
  type_code TEXT,
  type_guid TEXT,
  name TEXT,
  filesystem_hint TEXT,
  confidence TEXT,
  status TEXT,
  notes TEXT,
  created_utc TEXT
);
)SQL");
    exec(R"SQL(CREATE TABLE IF NOT EXISTS image_inventory_sources (
  image_inventory_source_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  input_path TEXT,
  input_type TEXT,
  container_type TEXT,
  container_reader_status TEXT,
  partition_reader_status TEXT,
  filesystem_reader_status TEXT,
  spotlight_locator_status TEXT,
  active_comparison_status TEXT,
  preferred_reader_order TEXT,
  source_hash_sha256 TEXT,
  size_bytes INTEGER,
  partition_scheme TEXT,
  partition_count INTEGER,
  apfs_hint_count INTEGER,
  spotlight_hint_count INTEGER,
  inventory_file_count INTEGER DEFAULT 0,
  inventory_directory_count INTEGER DEFAULT 0,
  comparison_candidate_count INTEGER DEFAULT 0,
  comparison_ready INTEGER DEFAULT 0,
  next_action TEXT,
  notes TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS image_file_inventory (
  image_file_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  container_type TEXT,
  container_path TEXT,
  aff4_stream_id TEXT,
  aff4_stream_name TEXT,
  partition_index INTEGER,
  partition_scheme TEXT,
  partition_offset_bytes INTEGER,
  filesystem_type TEXT,
  apfs_container_id TEXT,
  apfs_volume_name TEXT,
  filesystem_object_id TEXT,
  parent_filesystem_object_id TEXT,
  inode_num TEXT,
  parent_inode_num TEXT,
  full_path TEXT,
  file_name TEXT,
  is_directory INTEGER,
  logical_size_bytes INTEGER,
  allocated_size_bytes INTEGER,
  created_utc TEXT,
  modified_utc TEXT,
  accessed_utc TEXT,
  changed_utc TEXT,
  file_sha256 TEXT,
  source_confidence TEXT,
  extraction_status TEXT,
  provenance TEXT,
  created_utc_inventory TEXT
);
CREATE TABLE IF NOT EXISTS active_file_comparison_runs (
  comparison_run_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  image_inventory_available INTEGER,
  spotlight_artifact_count INTEGER,
  image_file_count INTEGER,
  inode_match_count INTEGER,
  path_match_count INTEGER,
  missing_candidate_count INTEGER,
  not_checked_count INTEGER,
  run_status TEXT,
  comparison_basis TEXT,
  notes TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS processing_log (
  log_id INTEGER PRIMARY KEY AUTOINCREMENT,
  created_utc TEXT,
  level TEXT,
  message TEXT
);
)SQL");

    // V0_8_93_1: iOS FFS ZIP inventory and database-resident object correlation scaffolding.
    exec(R"SQL(
CREATE TABLE IF NOT EXISTS ios_ffs_file_inventory (
  ios_file_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  original_zip_entry TEXT,
  normalized_path TEXT,
  file_name TEXT,
  extension TEXT,
  size_bytes INTEGER,
  zip_modified_utc TEXT,
  protection_class_hint TEXT,
  app_container_hint TEXT,
  domain_hint TEXT,
  is_directory INTEGER DEFAULT 0,
  sha256_status TEXT,
  inventory_notes TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_ffs_path_lookup (
  ios_path_lookup_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  normalized_path TEXT,
  file_name TEXT,
  size_bytes INTEGER,
  zip_modified_utc TEXT,
  protection_class_hint TEXT,
  app_container_hint TEXT,
  domain_hint TEXT,
  is_directory INTEGER DEFAULT 0,
  lookup_source TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_app_database_inventory (
  ios_db_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  original_zip_entry TEXT,
  normalized_path TEXT,
  database_name TEXT,
  database_category TEXT,
  app_hint TEXT,
  protection_class_hint TEXT,
  size_bytes INTEGER,
  zip_modified_utc TEXT,
  parse_status TEXT,
  record_inventory_status TEXT,
  notes TEXT,
  extracted_path TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_app_database_record_inventory (
  ios_record_inventory_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  ios_db_id INTEGER,
  database_normalized_path TEXT,
  database_name TEXT,
  database_category TEXT,
  app_hint TEXT,
  table_name TEXT,
  row_count INTEGER,
  sample_columns TEXT,
  record_category TEXT,
  parse_status TEXT,
  notes TEXT,
  created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_app_parsed_records (
  ios_app_record_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT,
  ios_db_id INTEGER,
  database_normalized_path TEXT,
  database_name TEXT,
  database_category TEXT,
  app_hint TEXT,
  table_name TEXT,
  record_category TEXT,
  source_primary_key TEXT,
  record_timestamp_utc TEXT,
  timestamp_source TEXT,
  contact_or_participant TEXT,
  url TEXT,
  title TEXT,
  file_path TEXT,
  item_identifier TEXT,
  text_snippet TEXT,
  parse_status TEXT,
  provenance TEXT,
  created_utc TEXT
);
CREATE INDEX IF NOT EXISTS idx_ios_ffs_path ON ios_ffs_file_inventory(source_id, normalized_path);
CREATE INDEX IF NOT EXISTS idx_ios_ffs_name ON ios_ffs_file_inventory(source_id, file_name);
CREATE INDEX IF NOT EXISTS idx_ios_ffs_lookup_path ON ios_ffs_path_lookup(source_id, normalized_path);
CREATE INDEX IF NOT EXISTS idx_ios_ffs_lookup_name ON ios_ffs_path_lookup(source_id, file_name);
CREATE INDEX IF NOT EXISTS idx_ios_db_path ON ios_app_database_inventory(source_id, normalized_path);
CREATE INDEX IF NOT EXISTS idx_ios_db_category ON ios_app_database_inventory(source_id, database_category, app_hint);
CREATE INDEX IF NOT EXISTS idx_ios_db_record_source ON ios_app_database_record_inventory(source_id, database_category, table_name);
CREATE INDEX IF NOT EXISTS idx_ios_db_record_dbid ON ios_app_database_record_inventory(ios_db_id);
CREATE INDEX IF NOT EXISTS idx_ios_app_parsed_source ON ios_app_parsed_records(source_id, database_category, record_category);
CREATE INDEX IF NOT EXISTS idx_ios_app_parsed_db ON ios_app_parsed_records(ios_db_id, table_name);
CREATE INDEX IF NOT EXISTS idx_ios_app_parsed_lookup ON ios_app_parsed_records(source_id, url, contact_or_participant, item_identifier);
)SQL");

    auto ensureColumn = [this](const std::string& table, const std::string& column, const std::string& type) {
        bool exists = false;
        auto pragma = prepare("PRAGMA table_info(" + table + ")");
        while (pragma.stepRow()) {
            if (pragma.colText(1) == column) { exists = true; break; }
        }
        if (!exists) exec("ALTER TABLE " + table + " ADD COLUMN " + column + " " + type + ";");
    };

    // V0_7_23: add date/object association fields before any review indexes reference them.
    ensureColumn("ios_app_database_inventory", "extracted_path", "TEXT");
    ensureColumn("ios_app_database_record_inventory", "ios_db_id", "INTEGER");
    ensureColumn("ios_app_database_record_inventory", "database_normalized_path", "TEXT");
    ensureColumn("ios_app_database_record_inventory", "sample_columns", "TEXT");
    ensureColumn("ios_app_parsed_records", "ios_db_id", "INTEGER");
    ensureColumn("ios_app_parsed_records", "database_normalized_path", "TEXT");
    ensureColumn("ios_app_parsed_records", "record_timestamp_utc", "TEXT");
    ensureColumn("ios_app_parsed_records", "timestamp_source", "TEXT");
    ensureColumn("ios_app_parsed_records", "contact_or_participant", "TEXT");
    ensureColumn("ios_app_parsed_records", "url", "TEXT");
    ensureColumn("ios_app_parsed_records", "title", "TEXT");
    ensureColumn("ios_app_parsed_records", "file_path", "TEXT");
    ensureColumn("ios_app_parsed_records", "item_identifier", "TEXT");
    ensureColumn("ios_app_parsed_records", "text_snippet", "TEXT");

    ensureColumn("raw_date_candidates", "artifact_id", "INTEGER");
    ensureColumn("raw_date_candidates", "parent_inode_num", "TEXT");
    ensureColumn("raw_date_candidates", "file_name", "TEXT");
    ensureColumn("raw_date_candidates", "best_path", "TEXT");
    ensureColumn("raw_date_candidates", "date_type", "TEXT");
    ensureColumn("raw_date_candidates", "association_status", "TEXT");
    ensureColumn("raw_date_candidates", "association_confidence", "TEXT");

    exec(R"SQL(
CREATE INDEX IF NOT EXISTS idx_preserved_files_source ON preserved_evidence_files(source_id);
CREATE INDEX IF NOT EXISTS idx_preserved_files_role ON preserved_evidence_files(file_role);
CREATE INDEX IF NOT EXISTS idx_raw_records_inode ON raw_records(source_id, store_guid, inode_num);
CREATE INDEX IF NOT EXISTS idx_raw_records_parent ON raw_records(source_id, store_guid, parent_inode_num);
CREATE INDEX IF NOT EXISTS idx_raw_kv_field ON raw_key_values(field_name);
CREATE INDEX IF NOT EXISTS idx_raw_kv_inode ON raw_key_values(source_id, store_guid, inode_num);
CREATE INDEX IF NOT EXISTS idx_raw_kv_inode_field ON raw_key_values(source_id, store_guid, inode_num, field_name);
CREATE INDEX IF NOT EXISTS idx_raw_kv_record_join ON raw_key_values(source_id, store_guid, source_db, inode_num, store_id);
CREATE INDEX IF NOT EXISTS idx_dates_parsed ON raw_date_candidates(parsed_utc);
CREATE INDEX IF NOT EXISTS idx_raw_dates_inode_field ON raw_date_candidates(source_id, store_guid, inode_num, field_name);
CREATE INDEX IF NOT EXISTS idx_raw_dates_record_join ON raw_date_candidates(source_id, store_guid, source_db, inode_num, store_id, parsed_utc);
CREATE INDEX IF NOT EXISTS idx_raw_dates_artifact_parsed ON raw_date_candidates(artifact_id, parsed_utc);
CREATE INDEX IF NOT EXISTS idx_raw_dates_artifact_type_parsed ON raw_date_candidates(artifact_id, date_type, parsed_utc);
CREATE INDEX IF NOT EXISTS idx_raw_dates_usage_review ON raw_date_candidates(date_type, artifact_id, parsed_utc);
CREATE INDEX IF NOT EXISTS idx_raw_dates_association_status ON raw_date_candidates(association_status, association_confidence);
CREATE INDEX IF NOT EXISTS idx_artifact_date_summary_source_artifact ON artifact_date_summary(source_id, artifact_id);
CREATE INDEX IF NOT EXISTS idx_artifact_date_summary_last_date ON artifact_date_summary(last_date_utc, artifact_id);
CREATE INDEX IF NOT EXISTS idx_raw_dates_field_parsed ON raw_date_candidates(field_name, parsed_utc);
CREATE INDEX IF NOT EXISTS idx_artifacts_path ON artifacts(best_path);
CREATE INDEX IF NOT EXISTS idx_artifacts_inode ON artifacts(source_id, store_guid, inode_num);
CREATE INDEX IF NOT EXISTS idx_artifacts_parent ON artifacts(source_id, store_guid, parent_inode_num);
CREATE INDEX IF NOT EXISTS idx_artifacts_file_name ON artifacts(file_name);
CREATE INDEX IF NOT EXISTS idx_artifacts_last_updated ON artifacts(last_updated_utc);
CREATE INDEX IF NOT EXISTS idx_artifacts_usage_dates ON artifacts(last_used_date_utc, first_used_candidate_utc);
CREATE INDEX IF NOT EXISTS idx_timeline_time ON timeline_events(event_timestamp_utc);
CREATE INDEX IF NOT EXISTS idx_timeline_artifact_time ON timeline_events(artifact_id, event_timestamp_utc);
CREATE INDEX IF NOT EXISTS idx_timeline_source_field_time ON timeline_events(event_source_field, event_timestamp_utc);
CREATE INDEX IF NOT EXISTS idx_orphan_status ON orphaned_deleted_candidates(existence_status);
CREATE INDEX IF NOT EXISTS idx_external_volume_source ON external_volume_candidates(source_id, mounted_volume_name);
)SQL" R"SQL(CREATE INDEX IF NOT EXISTS idx_artifact_instances_artifact ON artifact_source_instances(artifact_id);
CREATE INDEX IF NOT EXISTS idx_artifact_instances_raw ON artifact_source_instances(raw_record_id);
CREATE INDEX IF NOT EXISTS idx_source_copy_status ON source_copy_comparison(source_id, comparison_status);
CREATE INDEX IF NOT EXISTS idx_parent_inode_links_child ON parent_inode_links(source_id, store_guid, child_inode_num);
CREATE INDEX IF NOT EXISTS idx_parent_inode_links_parent ON parent_inode_links(source_id, store_guid, child_parent_inode_num);
CREATE INDEX IF NOT EXISTS idx_parent_inode_links_status ON parent_inode_links(source_id, relationship_status);
CREATE INDEX IF NOT EXISTS idx_field_inventory_field ON field_inventory(source_id, field_name);
CREATE INDEX IF NOT EXISTS idx_parser_coverage_metric ON parser_coverage_summary(source_id, metric_name);
CREATE INDEX IF NOT EXISTS idx_native_property_dictionary_name ON native_property_dictionary(source_id, property_name);
CREATE INDEX IF NOT EXISTS idx_native_property_dictionary_store ON native_property_dictionary(source_id, store_guid, source_db);
CREATE INDEX IF NOT EXISTS idx_native_category_dictionary_store ON native_category_dictionary(source_id, store_guid, source_db);
CREATE INDEX IF NOT EXISTS idx_native_dbstr_map_inventory_store ON native_dbstr_map_inventory(source_id, store_guid, source_db);
CREATE INDEX IF NOT EXISTS idx_native_index_dictionary_summary_store ON native_index_dictionary_summary(source_id, store_guid, source_db);
CREATE INDEX IF NOT EXISTS idx_native_decode_attempts_store ON native_decode_attempts(source_id, store_guid, source_db);
CREATE INDEX IF NOT EXISTS idx_source_probe_runs_source ON source_probe_runs(source_id, created_utc);
CREATE INDEX IF NOT EXISTS idx_source_probe_runs_type_status ON source_probe_runs(input_type, parse_status);
CREATE INDEX IF NOT EXISTS idx_source_probe_signatures_source ON source_probe_signatures(source_id, category, signature_name);
CREATE INDEX IF NOT EXISTS idx_source_probe_signatures_offset ON source_probe_signatures(source_id, offset_bytes);
CREATE INDEX IF NOT EXISTS idx_source_partition_probe_source ON source_partition_probe(source_id, scheme, partition_index);
CREATE INDEX IF NOT EXISTS idx_source_partition_probe_offset ON source_partition_probe(source_id, offset_bytes);

DROP VIEW IF EXISTS vw_source_probe_inventory;
CREATE VIEW vw_source_probe_inventory AS
SELECT r.source_probe_run_id,
       r.created_utc,
       r.source_id,
       r.input_type,
       r.source_kind,
       r.input_path,
       r.size_bytes,
       r.working_root,
       r.container_reader_status,
)SQL" R"SQL(       r.filesystem_reader_status,
       r.spotlight_discovery_status,
       r.database_candidates,
       r.valid_database_candidates,
       r.store_groups,
       r.valid_store_groups,
       r.probe_bytes_scanned,
       r.probe_truncated,
       r.probe_signature_count,
       r.partition_scheme,
       r.partition_entry_count,
       r.filesystem_hints,
       r.spotlight_hints,
       r.parse_status,
       r.next_action,
       r.notes
FROM source_probe_runs r
ORDER BY r.created_utc DESC, r.source_probe_run_id DESC;

DROP VIEW IF EXISTS vw_source_probe_signatures;
CREATE VIEW vw_source_probe_signatures AS
SELECT s.source_probe_signature_id,
       s.created_utc,
       s.source_id,
       r.input_type,
       r.source_kind,
       s.signature_name,
       s.category,
       s.offset_bytes,
       s.confidence,
       s.notes
FROM source_probe_signatures s
LEFT JOIN source_probe_runs r ON r.source_id=s.source_id
ORDER BY s.source_id, s.offset_bytes, s.signature_name;

DROP VIEW IF EXISTS vw_source_partition_inventory;
)SQL");
    exec(R"SQL(CREATE VIEW vw_source_partition_inventory AS
SELECT p.source_partition_probe_id,
       p.created_utc,
       p.source_id,
       r.input_type,
       r.source_kind,
       p.scheme,
       p.partition_index,
       p.start_lba,
       p.sector_count,
       p.offset_bytes,
       p.size_bytes,
       p.type_code,
       p.type_guid,
       p.name,
       p.filesystem_hint,
       p.confidence,
       p.status,
       p.notes
FROM source_partition_probe p
LEFT JOIN source_probe_runs r ON r.source_id=p.source_id
ORDER BY p.source_id, p.scheme, p.partition_index, p.offset_bytes;
)SQL");

    exec(R"SQL(
CREATE INDEX IF NOT EXISTS idx_image_inventory_sources_source ON image_inventory_sources(source_id, created_utc);
CREATE INDEX IF NOT EXISTS idx_image_inventory_sources_status ON image_inventory_sources(input_type, container_reader_status, filesystem_reader_status, active_comparison_status);
CREATE INDEX IF NOT EXISTS idx_image_file_inventory_source_path ON image_file_inventory(source_id, full_path);
CREATE INDEX IF NOT EXISTS idx_image_file_inventory_source_inode ON image_file_inventory(source_id, inode_num, parent_inode_num);
CREATE INDEX IF NOT EXISTS idx_image_file_inventory_object ON image_file_inventory(source_id, filesystem_object_id, parent_filesystem_object_id);
CREATE INDEX IF NOT EXISTS idx_image_file_inventory_volume ON image_file_inventory(source_id, apfs_volume_name, filesystem_type);
CREATE INDEX IF NOT EXISTS idx_active_file_comparison_runs_source ON active_file_comparison_runs(source_id, created_utc);

DROP VIEW IF EXISTS vw_image_inventory_sources;
CREATE VIEW vw_image_inventory_sources AS
SELECT image_inventory_source_id,
       created_utc,
       source_id,
       input_type,
       container_type,
       input_path,
       size_bytes,
       source_hash_sha256,
       container_reader_status,
       partition_reader_status,
       filesystem_reader_status,
       spotlight_locator_status,
       active_comparison_status,
       preferred_reader_order,
       partition_scheme,
       partition_count,
       apfs_hint_count,
       spotlight_hint_count,
       inventory_file_count,
       inventory_directory_count,
       comparison_candidate_count,
       comparison_ready,
       next_action,
       notes
FROM image_inventory_sources
ORDER BY created_utc DESC, image_inventory_source_id DESC;

DROP VIEW IF EXISTS vw_image_file_inventory;
CREATE VIEW vw_image_file_inventory AS
SELECT image_file_id,
       created_utc_inventory,
       source_id,
       container_type,
       container_path,
       aff4_stream_id,
       aff4_stream_name,
       partition_index,
       partition_scheme,
       partition_offset_bytes,
       filesystem_type,
       apfs_container_id,
       apfs_volume_name,
       filesystem_object_id,
       parent_filesystem_object_id,
       inode_num,
       parent_inode_num,
       file_name,
       full_path,
       is_directory,
       logical_size_bytes,
       allocated_size_bytes,
       created_utc,
       modified_utc,
       accessed_utc,
       changed_utc,
       file_sha256,
       source_confidence,
       extraction_status,
       provenance
FROM image_file_inventory
ORDER BY source_id, apfs_volume_name, full_path, image_file_id;

DROP VIEW IF EXISTS vw_active_file_comparison_readiness;
CREATE VIEW vw_active_file_comparison_readiness AS
SELECT s.image_inventory_source_id,
       s.created_utc,
)SQL" R"SQL(       s.source_id,
       s.input_type,
       s.container_type,
       s.container_reader_status,
       s.partition_reader_status,
       s.filesystem_reader_status,
       s.spotlight_locator_status,
       s.active_comparison_status,
       s.inventory_file_count,
       s.comparison_candidate_count,
       s.comparison_ready,
       COALESCE((SELECT COUNT(*) FROM artifacts a WHERE a.source_id=s.source_id),0) AS spotlight_artifact_count,
       COALESCE((SELECT COUNT(*) FROM image_file_inventory f WHERE f.source_id=s.source_id),0) AS image_inventory_rows,
       s.next_action,
       s.notes
FROM image_inventory_sources s
ORDER BY s.created_utc DESC, s.image_inventory_source_id DESC;

DROP VIEW IF EXISTS vw_spotlight_active_file_comparison;
CREATE VIEW vw_spotlight_active_file_comparison AS
WITH image_counts AS (
  SELECT source_id, COUNT(*) AS image_file_rows
  FROM image_file_inventory
  GROUP BY source_id
), inode_matches AS (
  SELECT a.artifact_id, MIN(f.image_file_id) AS image_file_id
  FROM artifacts a
  JOIN image_file_inventory f
    ON f.source_id=a.source_id
   AND COALESCE(NULLIF(f.inode_num,''),'')<>''
   AND f.inode_num=a.inode_num
   AND (COALESCE(NULLIF(f.parent_inode_num,''),'')='' OR COALESCE(NULLIF(a.parent_inode_num,''),'')='' OR f.parent_inode_num=a.parent_inode_num)
  GROUP BY a.artifact_id
), path_matches AS (
  SELECT a.artifact_id, MIN(f.image_file_id) AS image_file_id
  FROM artifacts a
  JOIN image_file_inventory f
    ON f.source_id=a.source_id
   AND COALESCE(NULLIF(f.full_path,''),'')<>''
   AND COALESCE(NULLIF(a.best_path,''),'')<>''
   AND lower(f.full_path)=lower(a.best_path)
  GROUP BY a.artifact_id
), best_match AS (
  SELECT a.artifact_id,
         COALESCE(im.image_file_id, pm.image_file_id) AS image_file_id,
         CASE WHEN im.image_file_id IS NOT NULL THEN 'INODE_PARENT_OPTIONAL'
              WHEN pm.image_file_id IS NOT NULL THEN 'FULL_PATH_EXACT_CASE_INSENSITIVE'
              ELSE '' END AS match_basis
  FROM artifacts a
  LEFT JOIN inode_matches im ON im.artifact_id=a.artifact_id
  LEFT JOIN path_matches pm ON pm.artifact_id=a.artifact_id
)
SELECT a.artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.best_path,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.usage_field_summary,
       COALESCE(ic.image_file_rows,0) AS image_inventory_rows,
       bm.image_file_id,
       f.full_path AS matched_image_path,
       f.file_name AS matched_image_file_name,
       f.filesystem_type AS matched_filesystem_type,
       f.apfs_volume_name AS matched_apfs_volume_name,
       f.filesystem_object_id AS matched_filesystem_object_id,
       f.logical_size_bytes AS matched_logical_size_bytes,
       bm.match_basis,
)SQL" R"SQL(       CASE
         WHEN COALESCE(ic.image_file_rows,0)=0 THEN 'IMAGE_FILE_INVENTORY_NOT_AVAILABLE'
         WHEN bm.image_file_id IS NOT NULL THEN 'ACTIVE_FILE_PRESENT_IN_IMAGE_INVENTORY'
         WHEN COALESCE(NULLIF(a.best_path,''),'')='' AND COALESCE(NULLIF(a.inode_num,''),'')='' THEN 'SPOTLIGHT_ARTIFACT_HAS_NO_COMPARABLE_PATH_OR_INODE'
         ELSE 'SPOTLIGHT_INDEXED_NOT_FOUND_IN_IMAGE_INVENTORY'
       END AS active_file_comparison_status,
       CASE
         WHEN COALESCE(ic.image_file_rows,0)=0 THEN 'No APFS/HFS image inventory has been loaded for this source yet.'
         WHEN bm.image_file_id IS NOT NULL THEN 'Matched against image_file_inventory using the recorded match basis.'
         ELSE 'Spotlight artifact did not match current image_file_inventory by inode/parent or exact path.'
       END AS comparison_notes
FROM artifacts a
LEFT JOIN image_counts ic ON ic.source_id=a.source_id
LEFT JOIN best_match bm ON bm.artifact_id=a.artifact_id
LEFT JOIN image_file_inventory f ON f.image_file_id=bm.image_file_id;
)SQL");

    exec(R"SQL(
CREATE INDEX IF NOT EXISTS idx_processing_phases_source ON processing_phases(source_id, phase_name);
CREATE INDEX IF NOT EXISTS idx_raw_records_source_db ON raw_records(source_id, source_db);
CREATE INDEX IF NOT EXISTS idx_raw_records_record_sort ON raw_records(source_id, store_guid, source_db, last_updated_utc, raw_record_id);
CREATE INDEX IF NOT EXISTS idx_raw_records_object_key ON raw_records(source_id, store_guid, source_db, inode_num, store_id, raw_record_id);
CREATE INDEX IF NOT EXISTS idx_raw_kv_object_key ON raw_key_values(source_id, store_guid, source_db, inode_num, store_id, raw_kv_id);
CREATE INDEX IF NOT EXISTS idx_raw_dates_object_key ON raw_date_candidates(source_id, store_guid, source_db, inode_num, store_id, raw_date_id);
CREATE TABLE IF NOT EXISTS investigator_tags (
  tag_id INTEGER PRIMARY KEY AUTOINCREMENT,
  tag_name TEXT UNIQUE NOT NULL,
  tag_color TEXT,
  created_utc TEXT,
  notes TEXT
);
CREATE TABLE IF NOT EXISTS artifact_tags (
  artifact_tag_id INTEGER PRIMARY KEY AUTOINCREMENT,
  artifact_id INTEGER NOT NULL,
  tag_id INTEGER NOT NULL,
  created_utc TEXT,
  UNIQUE(artifact_id, tag_id)
);
CREATE TABLE IF NOT EXISTS investigator_notes (
  note_id INTEGER PRIMARY KEY AUTOINCREMENT,
  target_type TEXT NOT NULL,
  target_id TEXT NOT NULL,
  note_text TEXT,
  created_utc TEXT,
  updated_utc TEXT
);
CREATE INDEX IF NOT EXISTS idx_artifact_tags_artifact ON artifact_tags(artifact_id);
CREATE INDEX IF NOT EXISTS idx_artifact_tags_tag ON artifact_tags(tag_id);
CREATE INDEX IF NOT EXISTS idx_investigator_notes_target ON investigator_notes(target_type, target_id);
CREATE TABLE IF NOT EXISTS review_view_preferences (
  platform TEXT NOT NULL,
  view_name TEXT NOT NULL,
  is_visible INTEGER NOT NULL DEFAULT 1,
  display_order INTEGER NOT NULL DEFAULT 0,
  preset_name TEXT DEFAULT 'Recommended V1',
  updated_utc TEXT DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY(platform, view_name)
);
CREATE TABLE IF NOT EXISTS gui_checked_artifacts (
  artifact_id INTEGER PRIMARY KEY,
  checked_utc TEXT,
  notes TEXT
);
CREATE INDEX IF NOT EXISTS idx_gui_checked_artifacts_checked ON gui_checked_artifacts(checked_utc);

DROP VIEW IF EXISTS vw_checked_artifacts;
CREATE VIEW vw_checked_artifacts AS
WITH tag_summary AS (
  SELECT at.artifact_id, GROUP_CONCAT(it.tag_name, '; ') AS tags, COUNT(*) AS tag_count
  FROM artifact_tags at
  JOIN investigator_tags it ON it.tag_id=at.tag_id
  GROUP BY at.artifact_id
), note_summary AS (
  SELECT CAST(target_id AS INTEGER) AS artifact_id,
         COUNT(*) AS note_count,
         MAX(COALESCE(NULLIF(updated_utc,''), created_utc)) AS last_note_utc,
         (SELECT note_text FROM investigator_notes n2 WHERE n2.target_type='artifact' AND CAST(n2.target_id AS INTEGER)=CAST(n.target_id AS INTEGER) ORDER BY COALESCE(NULLIF(n2.updated_utc,''), n2.created_utc) DESC, n2.note_id DESC LIMIT 1) AS last_note_text
  FROM investigator_notes n
  WHERE target_type='artifact'
  GROUP BY CAST(target_id AS INTEGER)
), date_summary AS (
  SELECT artifact_id, usage_latest_utc, modified_latest_utc, created_latest_utc, downloaded_latest_utc, last_date_utc
  FROM artifact_date_summary
)
SELECT c.checked_utc,
       a.artifact_id,
       COALESCE(ts.tags,'') AS tags,
       COALESCE(ts.tag_count,0) AS tag_count,
       COALESCE(ns.note_count,0) AS note_count,
       COALESCE(ns.last_note_utc,'') AS last_note_utc,
       COALESCE(ns.last_note_text,'') AS last_note_text,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
)SQL" R"SQL(       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.use_count_value,
       a.usage_field_summary,
       ds.usage_latest_utc,
       ds.modified_latest_utc,
       ds.created_latest_utc,
       ds.downloaded_latest_utc,
       ds.last_date_utc,
       a.confidence
FROM gui_checked_artifacts c
JOIN artifacts a ON a.artifact_id=c.artifact_id
LEFT JOIN tag_summary ts ON ts.artifact_id=a.artifact_id
LEFT JOIN note_summary ns ON ns.artifact_id=a.artifact_id
LEFT JOIN date_summary ds ON ds.artifact_id=a.artifact_id;
)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_tagged_artifacts;
CREATE VIEW vw_tagged_artifacts AS
WITH note_summary AS (
  SELECT target_id AS artifact_id,
         COUNT(*) AS note_count,
         MAX(COALESCE(NULLIF(updated_utc,''), created_utc)) AS last_note_utc,
         (SELECT note_text FROM investigator_notes n2 WHERE n2.target_type='artifact' AND n2.target_id=n.target_id ORDER BY COALESCE(NULLIF(n2.updated_utc,''), n2.created_utc) DESC, n2.note_id DESC LIMIT 1) AS last_note_text
  FROM investigator_notes n
  WHERE target_type='artifact'
  GROUP BY target_id
), date_summary AS (
  SELECT artifact_id, usage_latest_utc, modified_latest_utc, created_latest_utc, downloaded_latest_utc
  FROM artifact_date_summary
)
SELECT t.tag_id,
       t.tag_name,
       at.created_utc AS tagged_utc,
       a.artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.use_count_value,
       a.usage_field_summary,
       a.open_count_estimate,
       ds.usage_latest_utc,
       ds.modified_latest_utc,
       ds.created_latest_utc,
       ds.downloaded_latest_utc,
       COALESCE(ns.note_count,0) AS note_count,
       COALESCE(ns.last_note_utc,'') AS last_note_utc,
       COALESCE(ns.last_note_text,'') AS last_note_text,
       a.confidence
FROM artifact_tags at
JOIN investigator_tags t ON t.tag_id=at.tag_id
JOIN artifacts a ON a.artifact_id=at.artifact_id
LEFT JOIN note_summary ns ON ns.artifact_id=CAST(a.artifact_id AS TEXT)
LEFT JOIN date_summary ds ON ds.artifact_id=a.artifact_id;
)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_artifact_notes;
CREATE VIEW vw_artifact_notes AS
WITH tag_summary AS (
  SELECT at.artifact_id, GROUP_CONCAT(it.tag_name, '; ') AS tags
  FROM artifact_tags at
  JOIN investigator_tags it ON it.tag_id=at.tag_id
  GROUP BY at.artifact_id
)
SELECT n.note_id,
       n.created_utc,
       n.updated_utc,
       n.note_text,
       CAST(n.target_id AS INTEGER) AS artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       COALESCE(ts.tags,'') AS tags
FROM investigator_notes n
LEFT JOIN artifacts a ON a.artifact_id=CAST(n.target_id AS INTEGER)
LEFT JOIN tag_summary ts ON ts.artifact_id=a.artifact_id
WHERE n.target_type='artifact';
)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_export_ready_artifacts;
CREATE VIEW vw_export_ready_artifacts AS
WITH tag_summary AS (
  SELECT at.artifact_id, GROUP_CONCAT(it.tag_name, '; ') AS tags, COUNT(*) AS tag_count
  FROM artifact_tags at
  JOIN investigator_tags it ON it.tag_id=at.tag_id
  GROUP BY at.artifact_id
), note_summary AS (
  SELECT CAST(target_id AS INTEGER) AS artifact_id, COUNT(*) AS note_count, MAX(COALESCE(NULLIF(updated_utc,''), created_utc)) AS last_note_utc
  FROM investigator_notes
  WHERE target_type='artifact'
  GROUP BY CAST(target_id AS INTEGER)
)
SELECT a.artifact_id,
       COALESCE(ts.tags,'') AS tags,
       COALESCE(ts.tag_count,0) AS tag_count,
       COALESCE(ns.note_count,0) AS note_count,
       COALESCE(ns.last_note_utc,'') AS last_note_utc,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.use_count_value,
       a.usage_field_summary,
       ads.usage_latest_utc,
       ads.modified_latest_utc,
       ads.created_latest_utc,
       ads.downloaded_latest_utc,
       a.confidence
FROM artifacts a
LEFT JOIN tag_summary ts ON ts.artifact_id=a.artifact_id
LEFT JOIN note_summary ns ON ns.artifact_id=a.artifact_id
LEFT JOIN artifact_date_summary ads ON ads.artifact_id=a.artifact_id
WHERE COALESCE(ts.tag_count,0)>0 OR COALESCE(ns.note_count,0)>0;
)SQL");

    ensureColumn("artifacts", "v7_full_path_raw", "TEXT");
    ensureColumn("artifacts", "spotlight_display_path", "TEXT");
    ensureColumn("artifacts", "normalized_mac_path", "TEXT");
    ensureColumn("artifacts", "filesystem_lookup_path", "TEXT");
    ensureColumn("artifacts", "path_source", "TEXT");
    ensureColumn("artifacts", "path_status", "TEXT");
    ensureColumn("artifacts", "first_used_candidate_utc", "TEXT");
    ensureColumn("artifacts", "used_dates_count", "INTEGER DEFAULT 0");
    ensureColumn("artifacts", "use_count_value", "TEXT");
    ensureColumn("artifacts", "usage_field_summary", "TEXT");
    ensureColumn("artifacts", "is_mounted_volume_path", "INTEGER DEFAULT 0");
    ensureColumn("artifacts", "mounted_volume_name", "TEXT");
    ensureColumn("artifacts", "external_volume_reason", "TEXT");
    ensureColumn("external_volume_candidates", "detection_source_field", "TEXT");
    ensureColumn("external_volume_candidates", "detection_source_value", "TEXT");


    // Persistent investigator-facing SQL views used by the GUI review layer.
    // These keep the UI database-backed and pageable instead of loading large CSVs.
    exec(R"SQL(
DROP VIEW IF EXISTS vw_usage_artifacts;
CREATE VIEW vw_usage_artifacts AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,
       first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,usage_field_summary,open_count_estimate,
       existence_status,deleted_or_orphaned_candidate,confidence
FROM artifacts
WHERE COALESCE(usage_field_summary,'')<>''
   OR COALESCE(last_used_date_utc,'')<>''
   OR COALESCE(first_used_candidate_utc,'')<>''
   OR COALESCE(used_dates_count,0)>0
   OR COALESCE(use_count_value,'')<>'';

DROP VIEW IF EXISTS vw_timeline_usage_focus;
CREATE VIEW vw_timeline_usage_focus AS
SELECT t.timeline_id,t.event_timestamp_utc AS event_utc,t.event_type,t.event_source_field,t.artifact_id,t.source_id,t.store_guid,t.inode_num,
       a.parent_inode_num,a.file_name,a.display_name,a.best_path,a.content_type,a.where_froms,'MEDIUM_USAGE_FIELD' AS confidence,'' AS notes
FROM timeline_events t
LEFT JOIN artifacts a ON a.artifact_id=t.artifact_id
WHERE lower(COALESCE(t.event_source_field,'')) LIKE '%used%'
   OR lower(COALESCE(t.event_type,'')) LIKE '%used%'
   OR lower(COALESCE(t.event_source_field,'')) LIKE '%usage%'
   OR lower(COALESCE(t.event_source_field,'')) LIKE '%open%';

DROP VIEW IF EXISTS vw_wherefroms_downloads;
CREATE VIEW vw_wherefroms_downloads AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,
       downloaded_date_utc,last_updated_utc,existence_status,deleted_or_orphaned_candidate,confidence
FROM artifacts
WHERE COALESCE(where_froms,'')<>'' OR COALESCE(downloaded_date_utc,'')<>'';

DROP VIEW IF EXISTS vw_recent_activity;
CREATE VIEW vw_recent_activity AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,
       last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,where_froms,confidence
FROM artifacts
WHERE COALESCE(last_updated_utc,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'';

DROP VIEW IF EXISTS vw_content_type_summary;
CREATE VIEW vw_content_type_summary AS
SELECT COALESCE(NULLIF(content_type,''),'(blank)') AS content_type,
       COUNT(*) AS artifact_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_artifact_count,
       SUM(CASE WHEN COALESCE(best_path,'')<>'' THEN 1 ELSE 0 END) AS path_artifact_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
)SQL" R"SQL(       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc,
       SUM(CAST(COALESCE(NULLIF(logical_size_bytes,''),'0') AS INTEGER)) AS total_logical_size_bytes,
       SUM(CAST(COALESCE(NULLIF(physical_size_bytes,''),'0') AS INTEGER)) AS total_physical_size_bytes
FROM artifacts
GROUP BY COALESCE(NULLIF(content_type,''),'(blank)');

DROP VIEW IF EXISTS vw_store_content_type_summary;
CREATE VIEW vw_store_content_type_summary AS
SELECT store_guid,
       COALESCE(NULLIF(content_type,''),'(blank)') AS content_type,
       COUNT(*) AS artifact_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_artifact_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc
FROM artifacts
GROUP BY store_guid, COALESCE(NULLIF(content_type,''),'(blank)');

DROP VIEW IF EXISTS vw_folder_activity;
CREATE VIEW vw_folder_activity AS
SELECT store_guid,
       parent_inode_num,
       COUNT(*) AS child_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_child_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc,
       MIN(NULLIF(best_path,'')) AS sample_child_path,
       SUM(CASE WHEN COALESCE(content_type,'')='public.folder' THEN 1 ELSE 0 END) AS folder_child_count
FROM artifacts
WHERE COALESCE(parent_inode_num,'')<>''
GROUP BY store_guid, parent_inode_num
HAVING COUNT(*) > 1;

DROP VIEW IF EXISTS vw_path_reconstruction;
CREATE VIEW vw_path_reconstruction AS
SELECT pl.link_id,
       pl.source_id,
       pl.store_guid,
       pl.child_artifact_id AS artifact_id,
       pl.child_inode_num AS inode_num,
       pl.child_parent_inode_num AS parent_inode_num,
       COALESCE(NULLIF(a.file_name,''), pl.child_file_name) AS file_name,
       COALESCE(NULLIF(a.display_name,''), pl.child_file_name) AS display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       pl.parent_artifact_id,
       pl.parent_inode_num AS resolved_parent_inode_num,
       pl.parent_file_name,
       pl.parent_best_path,
       pl.reconstructed_path_candidate,
       CASE WHEN COALESCE(a.path_source,'')='PARENT_INODE_RECONSTRUCTION' THEN 1 ELSE 0 END AS applied_to_artifact_path,
)SQL" R"SQL(       CASE WHEN COALESCE(a.best_path,'')=COALESCE(pl.reconstructed_path_candidate,'') AND COALESCE(pl.reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END AS candidate_matches_artifact_path,
       pl.sibling_group_key,
       pl.sibling_count,
       pl.relationship_status,
       pl.path_reconstruction_method,
       pl.confidence
FROM parent_inode_links pl
LEFT JOIN artifacts a ON a.artifact_id=pl.child_artifact_id
ORDER BY pl.source_id, pl.store_guid, CAST(pl.child_parent_inode_num AS INTEGER), CAST(pl.child_inode_num AS INTEGER), pl.link_id;

DROP VIEW IF EXISTS vw_same_folder_groups;
)SQL");
    exec(R"SQL(CREATE VIEW vw_same_folder_groups AS
SELECT source_id,
       store_guid,
       child_parent_inode_num AS parent_inode_num,
       MAX(parent_artifact_id) AS parent_artifact_id,
       MAX(CASE WHEN COALESCE(NULLIF(trim(parent_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN parent_file_name ELSE '' END) AS parent_file_name,
       MAX(CASE WHEN COALESCE(NULLIF(trim(parent_best_path),''),'') NOT IN ('/','------NONAME------','(null)','NULL','.') THEN parent_best_path ELSE '' END) AS parent_best_path,
       COUNT(*) AS child_count,
       SUM(CASE WHEN relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE' THEN 1 ELSE 0 END) AS resolved_parent_link_count,
       SUM(CASE WHEN COALESCE(reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END) AS reconstructed_child_path_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) AS child_name_count,
       MIN(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END) AS first_child_name,
       MAX(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END) AS last_child_name,
       CASE
         WHEN SUM(CASE WHEN COALESCE(reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END)>0 THEN 'RECONSTRUCTED_CHILD_PATHS_PRESENT'
         WHEN SUM(CASE WHEN relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE' THEN 1 ELSE 0 END)>0 THEN 'PARENT_LINKS_WITHOUT_RECONSTRUCTED_PATH'
         ELSE 'SAME_PARENT_INODE_GROUP_ONLY'
       END AS folder_group_status,
       MAX(confidence) AS max_confidence,
       sibling_group_key
FROM parent_inode_links
GROUP BY source_id, store_guid, child_parent_inode_num, sibling_group_key
HAVING COUNT(*) > 1
ORDER BY child_count DESC, source_id, store_guid, CAST(child_parent_inode_num AS INTEGER);

DROP VIEW IF EXISTS vw_volume_root_focus;
CREATE VIEW vw_volume_root_focus AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,content_type_tree,
       last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,is_mounted_volume_path,
       mounted_volume_name,external_volume_reason,confidence
FROM artifacts
WHERE COALESCE(content_type,'')='public.volume'
   OR COALESCE(is_mounted_volume_path,0)<>0
   OR COALESCE(mounted_volume_name,'')<>''
   OR COALESCE(external_volume_reason,'')<>'';

DROP VIEW IF EXISTS vw_ios_relevant_fields;
CREATE VIEW vw_ios_relevant_fields AS
SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,full_path,record_state,field_name,field_value
FROM raw_key_values
)SQL" R"SQL(WHERE lower(field_name) LIKE '%content%'
   OR lower(field_name) LIKE '%text%'
   OR lower(field_name) LIKE '%message%'
   OR lower(field_name) LIKE '%conversation%'
   OR lower(field_name) LIKE '%sender%'
   OR lower(field_name) LIKE '%recipient%'
   OR lower(field_name) LIKE '%author%'
   OR lower(field_name) LIKE '%creator%'
   OR lower(field_name) LIKE '%account%'
   OR lower(field_name) LIKE '%phone%'
   OR lower(field_name) LIKE '%mail%'
   OR lower(field_name) LIKE '%bundle%'
   OR lower(field_name) LIKE '%domain%'
   OR lower(field_name) LIKE '%used%'
   OR lower(field_name) LIKE '%wherefrom%'
   OR lower(field_name) LIKE '%download%'
   OR lower(field_name) LIKE '%path%'
   OR lower(field_name) LIKE '%filename%'
   OR lower(field_name) LIKE '%displayname%';

DROP VIEW IF EXISTS vw_ios_store_parse_summary;
CREATE VIEW vw_ios_store_parse_summary AS
SELECT store_guid,
       source_db,
       COUNT(*) AS raw_record_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(file_name),''),'') NOT IN ('','------NONAME------','------PLIST------','(null)','NULL','.') THEN 1 ELSE 0 END) AS record_file_name_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(full_path),''),'') NOT IN ('','/','------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) AS record_full_path_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(file_name),''),'') IN ('------NONAME------','------PLIST------','(null)','NULL','.') OR COALESCE(NULLIF(trim(file_name),''),'')='' THEN 1 ELSE 0 END) AS placeholder_file_name_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(full_path),''),'')='/' THEN 1 ELSE 0 END) AS placeholder_root_path_count,
       MIN(NULLIF(last_updated_utc,'')) AS earliest_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS latest_last_updated_utc
FROM raw_records
WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
GROUP BY store_guid, source_db;

DROP VIEW IF EXISTS vw_ios_string_probe_category_summary;
CREATE VIEW vw_ios_string_probe_category_summary AS
WITH probe AS (
  SELECT store_guid, source_db, inode_num, field_name, field_value, LOWER(field_value) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), categorized AS (
  SELECT CASE
    WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'URL_OR_WEB_LINK'
    WHEN v LIKE '%@%' AND v LIKE '%.%' THEN 'EMAIL_ADDRESS_OR_ACCOUNT'
    WHEN v LIKE '%imessage%' OR v LIKE '%sms%' OR v LIKE '%message%' THEN 'MESSAGE_TEXT_OR_MESSAGE_APP'
    WHEN v LIKE '%icloud%' OR v LIKE '%onedrive%' OR v LIKE '%dropbox%' OR v LIKE '%google drive%' OR v LIKE '%drive.google%' THEN 'CLOUD_STORAGE_OR_SYNC'
)SQL" R"SQL(    WHEN v LIKE '%calendar%' OR v LIKE '%invite%' OR v LIKE '%rsvp%' OR v LIKE '%event%' THEN 'CALENDAR_OR_INVITATION'
    WHEN v LIKE 'file:%' OR v LIKE '/private/var/%' OR v LIKE '%/mobile/%' THEN 'FILE_OR_IOS_PATH'
    ELSE 'OTHER_STRING_PROBE' END AS probe_category,
    store_guid, source_db, inode_num, field_value
  FROM probe
)
SELECT probe_category,
       COUNT(*) AS row_count,
       COUNT(DISTINCT store_guid) AS store_count,
       COUNT(DISTINCT inode_num) AS distinct_record_count,
       COUNT(DISTINCT field_value) AS distinct_value_count,
       substr(MIN(field_value),1,500) AS min_sample_value,
       substr(MAX(field_value),1,500) AS max_sample_value
FROM categorized
GROUP BY probe_category;

DROP VIEW IF EXISTS vw_ios_string_probe_values;
)SQL");
    exec(R"SQL(CREATE VIEW vw_ios_string_probe_values AS
SELECT CASE
    WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN 'URL_OR_WEB_LINK'
    WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN 'EMAIL_ADDRESS_OR_ACCOUNT'
    WHEN LOWER(field_value) LIKE '%imessage%' OR LOWER(field_value) LIKE '%sms%' OR LOWER(field_value) LIKE '%message%' THEN 'MESSAGE_TEXT_OR_MESSAGE_APP'
    WHEN LOWER(field_value) LIKE '%icloud%' OR LOWER(field_value) LIKE '%onedrive%' OR LOWER(field_value) LIKE '%dropbox%' OR LOWER(field_value) LIKE '%google drive%' OR LOWER(field_value) LIKE '%drive.google%' THEN 'CLOUD_STORAGE_OR_SYNC'
    WHEN LOWER(field_value) LIKE '%calendar%' OR LOWER(field_value) LIKE '%invite%' OR LOWER(field_value) LIKE '%rsvp%' OR LOWER(field_value) LIKE '%event%' THEN 'CALENDAR_OR_INVITATION'
    WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN 'FILE_OR_IOS_PATH'
    ELSE 'OTHER_STRING_PROBE' END AS probe_category,
       raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,substr(field_value,1,2000) AS field_value_sample
FROM raw_key_values
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(field_value,'')<>'';
)SQL");

    exec(joinSql({
R"VSQLFIX(
DROP VIEW IF EXISTS vw_ios_record_string_probe_summary;
CREATE VIEW vw_ios_record_string_probe_summary AS
WITH kv AS (
  SELECT source_id, store_guid, source_db, inode_num, store_id, parent_inode_num,
         CASE
           WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN 'URL_OR_WEB_LINK'
           WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN 'EMAIL_ADDRESS_OR_ACCOUNT'
           WHEN LOWER(field_value) LIKE '%imessage%' OR LOWER(field_value) LIKE '%sms%' OR LOWER(field_value) LIKE '%message%' THEN 'MESSAGE_TEXT_OR_MESSAGE_APP'
           WHEN LOWER(field_value) LIKE '%icloud%' OR LOWER(field_value) LIKE '%onedrive%' OR LOWER(field_value) LIKE '%dropbox%' OR LOWER(field_value) LIKE '%google drive%' OR LOWER(field_value) LIKE '%drive.google%' THEN 'CLOUD_STORAGE_OR_SYNC'
           WHEN LOWER(field_value) LIKE '%calendar%' OR LOWER(field_value) LIKE '%invite%' OR LOWER(field_value) LIKE '%rsvp%' OR LOWER(field_value) LIKE '%event%' THEN 'CALENDAR_OR_INVITATION'
           WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN 'FILE_OR_IOS_PATH'
           ELSE 'OTHER_STRING_PROBE'
         END AS probe_category,
         field_name,
         field_value
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), agg AS (
  SELECT source_id, store_guid, source_db, inode_num, store_id,
         COUNT(*) AS string_probe_rows,
         COUNT(DISTINCT field_name) AS distinct_probe_field_count,
         GROUP_CONCAT(DISTINCT probe_category) AS probe_categories,
         substr(GROUP_CONCAT(substr(field_name || '=' || REPLACE(REPLACE(field_value, char(13),' '), char(10),' '),1,500), ' || '),1,4000) AS string_probe_sample
  FROM kv
  GROUP BY source_id, store_guid, source_db, inode_num, store_id
)
SELECT r.raw_record_id,
       r.source_id,
       r.store_guid,
       r.source_db,
       r.inode_num,
       r.store_id,
       r.parent_inode_num,
       r.file_name,
       r.content_type,
       r.display_name,
       r.full_path,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       a.string_probe_rows,
       a.distinct_probe_field_count,
       a.probe_categories,
       a.string_probe_sample,
       r.record_state
FROM raw_records r
JOIN agg a
  ON a.source_id = r.source_id
 AND a.store_guid = r.store_guid
 AND a.source_db = r.source_db
 AND a.inode_num = r.inode_num
 AND a.store_id = r.store_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_timeline_index_updates;
CREATE VIEW vw_ios_timeline_index_updates AS
SELECT raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,file_name,content_type,display_name,full_path,last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       record_state
FROM raw_records
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(last_updated_utc,'')<>'';


DROP VIEW IF EXISTS vw_ios_spotlight_date_provenance;
CREATE VIEW vw_ios_spotlight_date_provenance AS
WITH dc AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         COUNT(*) AS date_candidate_count,
         MAX(CASE WHEN field_name='Last_Updated' THEN parsed_utc ELSE parsed_utc END) AS primary_date_utc,
         MAX(CASE WHEN field_name='Last_Updated' THEN field_name ELSE field_name END) AS primary_date_field,
         MAX(CASE WHEN field_name='Last_Updated' THEN field_value ELSE field_value END) AS primary_raw_value,
         MAX(CASE WHEN field_name='Last_Updated' THEN parse_method ELSE parse_method END) AS primary_parse_method,
         GROUP_CONCAT(DISTINCT field_name) AS date_source_fields,
         GROUP_CONCAT(DISTINCT parse_method) AS date_parse_methods,
         GROUP_CONCAT(DISTINCT date_type) AS date_type_summary,
         substr(GROUP_CONCAT(COALESCE(field_name,'') || '=' || COALESCE(field_value,'') || ' -> ' || COALESCE(parsed_utc,'') || ' [' || COALESCE(parse_method,'') || ']',' || '),1,4000) AS date_source_evidence
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.raw_record_id,
       r.source_id,
       r.store_guid,
       r.source_db,
       r.store_path,
       r.inode_num AS spotlight_inode_or_object_id,
       r.store_id AS spotlight_store_id,
       r.parent_inode_num,
       r.file_name,
       r.display_name,
       r.full_path,
       r.content_type,
       r.record_state,
       COALESCE(dc.primary_date_utc, r.last_updated_utc) AS spotlight_date_utc,
       COALESCE(dc.primary_date_field, 'Last_Updated') AS spotlight_date_source_field,
       CASE WHEN dc.primary_date_field IS NOT NULL THEN 'raw_date_candidates' ELSE 'raw_records' END AS spotlight_date_source_table,
       COALESCE(dc.primary_raw_value, r.last_updated_raw) AS spotlight_date_raw_value,
       COALESCE(dc.primary_parse_method, 'native_epoch_microseconds') AS spotlight_date_parse_method,
       COALESCE(NULLIF(dc.date_type_summary,''), 'metadata_seen_or_index_updated') AS spotlight_date_type,
       COALESCE(dc.date_source_fields, 'Last_Updated') AS spotlight_date_source_fields,
       COALESCE(dc.date_parse_methods, 'native_epoch_microseconds') AS spotlight_date_parse_methods,
       COALESCE(dc.date_candidate_count, CASE WHEN COALESCE(r.last_updated_utc,'')<>'' THEN 1 ELSE 0 END) AS spotlight_date_candidate_count,
       COALESCE(dc.date_source_evidence, 'Last_Updated=' || COALESCE(r.last_updated_raw,'') || ' -> ' || COALESCE(r.last_updated_utc,'')) AS spotlight_date_source_evidence,
       'Validate against raw_date_candidates.field_name/field_value/parsed_utc/parse_method or raw_records.last_updated_raw/last_updated_utc for this Store-V2 record.' AS date_validation_hint,
       'CoreSpotlight date provenance. Last_Updated is metadata/index timing unless another decoded field supports created/modified/accessed/user-activity semantics.' AS interpretation_note
FROM raw_records r
)VSQLFIX",
R"VSQLFIX(LEFT JOIN dc ON dc.source_id=r.source_id
            AND dc.store_guid=r.store_guid
            AND dc.source_db=r.source_db
            AND dc.inode_num=r.inode_num
            AND COALESCE(dc.store_id,'')=COALESCE(r.store_id,'')
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_artifacts;
CREATE VIEW vw_ios_artifacts AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,last_updated_utc,confidence
FROM artifacts
WHERE store_guid LIKE 'ios_%' OR source_id IN (SELECT source_id FROM raw_records WHERE source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%');

DROP VIEW IF EXISTS vw_ios_ffs_file_inventory;
CREATE VIEW vw_ios_ffs_file_inventory AS
SELECT ios_file_id,source_id,normalized_path,original_zip_entry,file_name,extension,size_bytes,zip_modified_utc,
       protection_class_hint,app_container_hint,domain_hint,is_directory,sha256_status,inventory_notes,created_utc
FROM ios_ffs_file_inventory
ORDER BY normalized_path;

DROP VIEW IF EXISTS vw_ios_database_artifact_inventory;
CREATE VIEW vw_ios_database_artifact_inventory AS
SELECT ios_db_id,source_id,normalized_path,original_zip_entry,database_name,database_category,app_hint,
       protection_class_hint,size_bytes,zip_modified_utc,parse_status,record_inventory_status,notes,extracted_path,created_utc
FROM ios_app_database_inventory
ORDER BY database_category,app_hint,normalized_path;

DROP VIEW IF EXISTS vw_ios_app_database_record_inventory;
CREATE VIEW vw_ios_app_database_record_inventory AS
SELECT ios_record_inventory_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,row_count,sample_columns,record_category,parse_status,notes,created_utc
FROM ios_app_database_record_inventory
ORDER BY database_category,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_app_database_record_summary;
CREATE VIEW vw_ios_app_database_record_summary AS
SELECT database_category,app_hint,record_category,parse_status,COUNT(*) AS table_count,
       SUM(COALESCE(row_count,0)) AS total_rows,MIN(database_name) AS first_database,MAX(database_name) AS last_database
FROM ios_app_database_record_inventory
GROUP BY database_category,app_hint,record_category,parse_status
ORDER BY database_category,record_category;

DROP VIEW IF EXISTS vw_ios_app_parsed_records;
CREATE VIEW vw_ios_app_parsed_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc
FROM ios_app_parsed_records
ORDER BY database_category,record_category,record_timestamp_utc,database_name,table_name,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_app_parsed_record_summary;
CREATE VIEW vw_ios_app_parsed_record_summary AS
SELECT database_category,app_hint,record_category,parse_status,COUNT(*) AS parsed_record_count,
       COUNT(DISTINCT database_name) AS database_count,
       MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,
       MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
       MIN(database_name) AS first_database,
       MAX(database_name) AS last_database
FROM ios_app_parsed_records
GROUP BY database_category,app_hint,record_category,parse_status
ORDER BY database_category,record_category;

DROP VIEW IF EXISTS vw_ios_apple_messages_parsed_records;
CREATE VIEW vw_ios_apple_messages_parsed_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc
FROM ios_app_parsed_records
WHERE database_category='APPLE_MESSAGES' OR lower(database_name) IN ('sms.db','chat.db') OR lower(database_normalized_path) LIKE '%/sms.db'
ORDER BY record_timestamp_utc,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_apple_messages_parsed_summary;
CREATE VIEW vw_ios_apple_messages_parsed_summary AS
SELECT record_category,parse_status,COUNT(*) AS parsed_record_count,COUNT(DISTINCT ios_db_id) AS database_count,
       MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
       COUNT(NULLIF(contact_or_participant,'')) AS records_with_contact_or_handle,
       COUNT(NULLIF(file_path,'')) AS records_with_file_path,
       COUNT(NULLIF(text_snippet,'')) AS records_with_text_or_metadata,
       MIN(database_normalized_path) AS first_database_path,MAX(database_normalized_path) AS last_database_path
FROM vw_ios_apple_messages_parsed_records
GROUP BY record_category,parse_status;


DROP VIEW IF EXISTS vw_ios_whatsapp_parsed_records;
CREATE VIEW vw_ios_whatsapp_parsed_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc
FROM ios_app_parsed_records
WHERE database_category='WHATSAPP'
   OR lower(database_normalized_path) LIKE '%group.net.whatsapp%'
   OR lower(database_normalized_path) LIKE '%/whatsapp/%'
   OR lower(database_name) IN ('chatstorage.sqlite','contactsv2.sqlite','callhistory.sqlite')
ORDER BY record_timestamp_utc,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_whatsapp_parsed_summary;
CREATE VIEW vw_ios_whatsapp_parsed_summary AS
SELECT record_category,parse_status,COUNT(*) AS parsed_record_count,COUNT(DISTINCT ios_db_id) AS database_count,
       MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
       COUNT(NULLIF(contact_or_participant,'')) AS records_with_contact_or_jid,
       COUNT(NULLIF(file_path,'')) AS records_with_media_or_file_path,
       COUNT(NULLIF(text_snippet,'')) AS records_with_text_or_metadata,
       MIN(database_normalized_path) AS first_database_path,MAX(database_normalized_path) AS last_database_path
FROM vw_ios_whatsapp_parsed_records
GROUP BY record_category,parse_status;

)VSQLFIX",
R"VSQLFIX(DROP VIEW IF EXISTS vw_ios_spotlight_referenced_paths;
CREATE VIEW vw_ios_spotlight_referenced_paths AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,field_value,LOWER(field_value) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
    AND COALESCE(field_name,'') <> '__spotlight_investigator_text_context'
), extracted AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,field_value,
         CASE
           WHEN v LIKE 'file:///private/var/%' THEN substr(field_value,8)
           WHEN v LIKE 'file:///var/%' THEN '/private' || substr(field_value,8)
           WHEN v LIKE '/private/var/%' THEN field_value
           WHEN v LIKE '/var/%' THEN '/private' || field_value
           WHEN instr(v,'/private/var/')>0 THEN substr(field_value,instr(v,'/private/var/'))
           WHEN instr(v,'/var/mobile/')>0 THEN '/private' || substr(field_value,instr(v,'/var/mobile/'))
           ELSE ''
         END AS extracted_path,
         CASE
           WHEN v LIKE 'file:%' OR v LIKE '%/private/var/%' OR v LIKE '%/var/mobile/%' THEN 'IOS_FILE_PATH_OR_FILE_URL'
           WHEN v LIKE '%http://%' OR v LIKE '%https://%' THEN 'WEB_URL_NO_LOCAL_PATH'
           ELSE 'NON_PATH_REFERENCE'
         END AS reference_type
  FROM probes
), normalized AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,field_value,reference_type,
         CASE WHEN extracted_path<>'' THEN
           lower(replace(replace(replace(replace(replace(replace(replace(
             replace(replace(extracted_path,'file://',''),'%20',' '),
             '%2F','/'),'%2f','/'),'\','/'),'//','/'),'//','/'),'//','/'),'//','/'))
         ELSE '' END AS normalized_ios_path
  FROM extracted
)
SELECT raw_kv_id AS reference_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,
       substr(field_value,1,2000) AS raw_reference_value,
       reference_type,
       normalized_ios_path,
       CASE WHEN normalized_ios_path<>'' THEN 'MEDIUM_STRING_PROBE_PATH' ELSE 'LOW_NO_LOCAL_PATH' END AS confidence,
       'Spotlight string probe reference; formal CoreSpotlight property mapping remains parser roadmap work. V0_9_25 normalizes repeated slashes before FFS lookup to reduce false missing classifications.' AS notes
FROM normalized
WHERE reference_type<>'NON_PATH_REFERENCE';

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_candidates;
CREATE VIEW vw_ios_spotlight_missing_from_ffs_candidates AS
WITH refs AS (
  SELECT * FROM vw_ios_spotlight_referenced_paths WHERE COALESCE(normalized_ios_path,'')<>''
), files AS (
  SELECT source_id,normalized_path,file_name,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,'full_inventory' AS lookup_source
  FROM ios_ffs_file_inventory
  UNION ALL
  SELECT l.source_id,l.normalized_path,l.file_name,l.size_bytes,l.zip_modified_utc,l.protection_class_hint,l.app_container_hint,l.domain_hint,COALESCE(NULLIF(l.lookup_source,''),'slim_path_lookup') AS lookup_source
  FROM ios_ffs_path_lookup l
  WHERE NOT EXISTS (SELECT 1 FROM ios_ffs_file_inventory f WHERE f.source_id=l.source_id LIMIT 1)
), lookup_available AS (
  SELECT source_id,MIN(lookup_source) AS lookup_source
  FROM files
  GROUP BY source_id
), ctx AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         substr(MAX(field_value),1,4000) AS spotlight_text_context_sample
  FROM raw_key_values
  WHERE field_name='__spotlight_investigator_text_context'
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
), classified AS (
  SELECT r.reference_id,r.source_id,r.store_guid,r.source_db,r.inode_num,r.store_id,r.parent_inode_num,r.field_name,
         r.raw_reference_value,r.reference_type,r.normalized_ios_path,
         COALESCE(ctx.spotlight_text_context_sample,'') AS spotlight_text_context_sample,
         CASE WHEN COALESCE(ctx.spotlight_text_context_sample,'')<>'' THEN 'TEXT_CONTEXT_RECOVERED_FROM_SAME_SPOTLIGHT_RECORD' ELSE 'NO_TEXT_CONTEXT_RECOVERED_IN_COMPACT_MODE' END AS spotlight_text_context_status,
         COALESCE(f.file_name,'') AS matched_file_name,COALESCE(f.size_bytes,0) AS matched_size_bytes,COALESCE(f.zip_modified_utc,'') AS matched_zip_modified_utc,
         COALESCE(f.protection_class_hint,'') AS matched_protection_class,COALESCE(f.app_container_hint,'') AS matched_app_container,COALESCE(f.domain_hint,'') AS matched_domain,
         COALESCE(f.lookup_source,la.lookup_source,'lookup_available_no_matching_path') AS ffs_lookup_source,
         CASE
           WHEN r.field_name LIKE '%Thumbnail%' OR r.normalized_ios_path LIKE '%/brandthumbs/%' OR r.normalized_ios_path LIKE '%/thumbnail%' OR r.normalized_ios_path LIKE '%/thumbnails/%' THEN 'LOW_APP_THUMBNAIL_OR_CACHE_REFERENCE'
           WHEN r.field_name='kMDItemAttachmentPaths' OR r.field_name LIKE 'com_apple_mobilesms_%' OR r.normalized_ios_path LIKE '%/sms/attachments/%' OR r.normalized_ios_path LIKE '%/messages/%' THEN 'HIGH_MESSAGE_ATTACHMENT_OR_PLUGIN_REFERENCE'
           WHEN r.field_name='kMDItemContentURL' AND (r.normalized_ios_path LIKE '%/documents/%' OR r.normalized_ios_path LIKE '%/nocloud%' OR r.normalized_ios_path LIKE '%/mobile documents/%') THEN 'HIGH_APP_DOCUMENT_REFERENCE'
           WHEN r.normalized_ios_path LIKE '%/dcim/%' OR r.normalized_ios_path LIKE '%/media/dcim/%' OR r.normalized_ios_path LIKE '%/photos/%' THEN 'HIGH_MEDIA_REFERENCE'
           WHEN r.normalized_ios_path LIKE '%/tmp/%' OR r.normalized_ios_path LIKE '%/caches/%' OR r.normalized_ios_path LIKE '%/cache/%' THEN 'MEDIUM_TEMP_OR_CACHE_REFERENCE'
           ELSE 'MEDIUM_APP_FILE_REFERENCE'
         END AS missing_candidate_category,
         CASE
           WHEN r.field_name LIKE '%Thumbnail%' OR r.normalized_ios_path LIKE '%/brandthumbs/%' OR r.normalized_ios_path LIKE '%/thumbnail%' OR r.normalized_ios_path LIKE '%/thumbnails/%' THEN 'LOW_INVESTIGATIVE_VALUE'
           WHEN r.field_name='kMDItemAttachmentPaths' OR r.field_name LIKE 'com_apple_mobilesms_%' OR r.normalized_ios_path LIKE '%/sms/attachments/%' OR r.normalized_ios_path LIKE '%/messages/%' OR r.field_name='kMDItemContentURL' THEN 'HIGH_INVESTIGATIVE_VALUE'
           ELSE 'MEDIUM_INVESTIGATIVE_VALUE'
         END AS investigative_priority,
         CASE
)VSQLFIX",
R"VSQLFIX(           WHEN r.field_name='kMDItemAttachmentPaths' OR r.field_name LIKE 'com_apple_mobilesms_%' OR r.normalized_ios_path LIKE '%/sms/attachments/%' OR r.normalized_ios_path LIKE '%/messages/%' THEN 1
           WHEN r.field_name='kMDItemContentURL' AND (r.normalized_ios_path LIKE '%/documents/%' OR r.normalized_ios_path LIKE '%/nocloud%' OR r.normalized_ios_path LIKE '%/mobile documents/%') THEN 2
           WHEN r.normalized_ios_path LIKE '%/dcim/%' OR r.normalized_ios_path LIKE '%/media/dcim/%' OR r.normalized_ios_path LIKE '%/photos/%' THEN 3
           WHEN r.normalized_ios_path LIKE '%/tmp/%' OR r.normalized_ios_path LIKE '%/caches/%' OR r.normalized_ios_path LIKE '%/cache/%' THEN 7
           WHEN r.field_name LIKE '%Thumbnail%' OR r.normalized_ios_path LIKE '%/brandthumbs/%' OR r.normalized_ios_path LIKE '%/thumbnail%' OR r.normalized_ios_path LIKE '%/thumbnails/%' THEN 9
           ELSE 5
         END AS investigative_priority_sort,
         CASE
           WHEN r.field_name LIKE '%Thumbnail%' OR r.normalized_ios_path LIKE '%/brandthumbs/%' THEN 'Likely app thumbnail/brand/cache reference; useful for app/content context but lower deletion value than user document or attachment paths.'
           WHEN r.field_name='kMDItemAttachmentPaths' OR r.normalized_ios_path LIKE '%/sms/attachments/%' THEN 'Message/attachment path recovered from Spotlight and absent from available FFS lookup; prioritize for deleted/unresolved attachment review.'
           WHEN r.field_name LIKE 'com_apple_mobilesms_%' THEN 'Messages link-preview/plugin path recovered from Spotlight; prioritize as communication-context evidence.'
           WHEN r.field_name='kMDItemContentURL' THEN 'ContentURL recovered from Spotlight; review text context and app/container before treating absence from FFS as deletion.'
           ELSE 'Spotlight path reference absent from available FFS lookup; review text context, source store, and acquisition scope.'
         END AS investigative_reason
  FROM refs r
  JOIN lookup_available la ON la.source_id=r.source_id
  LEFT JOIN files f ON f.source_id=r.source_id AND f.normalized_path=r.normalized_ios_path
  LEFT JOIN ctx ON ctx.source_id=r.source_id AND ctx.store_guid=r.store_guid AND ctx.source_db=r.source_db AND ctx.inode_num=r.inode_num AND COALESCE(ctx.store_id,'')=COALESCE(r.store_id,'')
  WHERE f.normalized_path IS NULL
)
SELECT reference_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,
       raw_reference_value,reference_type,normalized_ios_path,
       missing_candidate_category,investigative_priority,investigative_priority_sort,investigative_reason,
       spotlight_text_context_sample,spotlight_text_context_status,
       matched_file_name,matched_size_bytes,matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain,
       ffs_lookup_source,'FFS_LOOKUP_AVAILABLE' AS ffs_lookup_status,
       'SPOTLIGHT_ONLY_FILE_MISSING_OR_UNRESOLVED' AS residency_status,
       CASE WHEN investigative_priority='HIGH_INVESTIGATIVE_VALUE' THEN 'HIGH_PATH_ABSENT_FROM_FFS_LOOKUP'
            WHEN investigative_priority='LOW_INVESTIGATIVE_VALUE' THEN 'LOW_APP_CACHE_OR_THUMBNAIL_PATH_ABSENT_FROM_FFS_LOOKUP'
            ELSE 'MEDIUM_PATH_ABSENT_FROM_FFS_LOOKUP' END AS confidence,
       'Missing means absent from the full FFS inventory or the slim FFS path lookup available in this case. The text_context_sample is recovered from the same Spotlight record to help assess investigative value; absence from FFS does not by itself prove user deletion or app-level deletion.' AS interpretation_note
FROM classified;

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_summary;
CREATE VIEW vw_ios_spotlight_missing_from_ffs_summary AS
SELECT source_id,store_guid,source_db,field_name,reference_type,missing_candidate_category,investigative_priority,investigative_priority_sort,spotlight_text_context_status,ffs_lookup_status,
       COUNT(*) AS missing_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_object_count,
       COUNT(DISTINCT normalized_ios_path) AS distinct_missing_path_count,
       MIN(normalized_ios_path) AS min_missing_path_sample,
       MAX(normalized_ios_path) AS max_missing_path_sample,
       substr(MAX(spotlight_text_context_sample),1,4000) AS spotlight_text_context_sample,
       MAX(investigative_reason) AS investigative_reason,
       'Compact normal-mode Missing From FFS summary. V0_9_25 adds priority/category fields so likely app thumbnail/cache noise does not dominate investigator review.' AS interpretation_note
FROM vw_ios_spotlight_missing_from_ffs_candidates
GROUP BY source_id,store_guid,source_db,field_name,reference_type,missing_candidate_category,investigative_priority,investigative_priority_sort,spotlight_text_context_status,ffs_lookup_status;

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_high_value_candidates;
CREATE VIEW vw_ios_spotlight_missing_from_ffs_high_value_candidates AS
SELECT *
FROM vw_ios_spotlight_missing_from_ffs_candidates
WHERE investigative_priority IN ('HIGH_INVESTIGATIVE_VALUE','MEDIUM_INVESTIGATIVE_VALUE')
  AND missing_candidate_category <> 'LOW_APP_THUMBNAIL_OR_CACHE_REFERENCE';

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_high_value_summary;
CREATE VIEW vw_ios_spotlight_missing_from_ffs_high_value_summary AS
SELECT source_id,store_guid,source_db,field_name,reference_type,missing_candidate_category,investigative_priority,investigative_priority_sort,spotlight_text_context_status,ffs_lookup_status,
       COUNT(*) AS missing_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_object_count,
       COUNT(DISTINCT normalized_ios_path) AS distinct_missing_path_count,
       MIN(normalized_ios_path) AS min_missing_path_sample,
       MAX(normalized_ios_path) AS max_missing_path_sample,
       substr(MAX(spotlight_text_context_sample),1,4000) AS spotlight_text_context_sample,
       MAX(investigative_reason) AS investigative_reason,
       'High/medium-priority subset of Missing From FFS candidates, excluding likely thumbnail/brand/cache-only references. Use this as the first investigator review surface before the full candidate view.' AS interpretation_note
FROM vw_ios_spotlight_missing_from_ffs_high_value_candidates
)VSQLFIX",
R"VSQLFIX(GROUP BY source_id,store_guid,source_db,field_name,reference_type,missing_candidate_category,investigative_priority,investigative_priority_sort,spotlight_text_context_status,ffs_lookup_status;

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_text_detail;
CREATE VIEW vw_ios_spotlight_missing_from_ffs_text_detail AS
WITH rr AS (
  SELECT raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,file_name,display_name,content_type,content_type_tree,last_updated_utc,record_state,full_path
  FROM raw_records
), ctx AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         substr(MAX(field_value),1,4000) AS spotlight_text_full_or_sample,
         MAX(LENGTH(field_value)) AS spotlight_text_length,
         COUNT(*) AS spotlight_text_context_row_count
  FROM raw_key_values
  WHERE field_name='__spotlight_investigator_text_context'
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT c.reference_id,
       COALESCE(rr.raw_record_id,0) AS raw_record_id,
       c.source_id,c.store_guid,c.source_db,c.inode_num,c.store_id,c.parent_inode_num,
       COALESCE(rr.file_name,'') AS spotlight_file_name,
       COALESCE(rr.display_name,'') AS spotlight_display_name,
       COALESCE(rr.content_type,'') AS spotlight_content_type,
       COALESCE(rr.content_type_tree,'') AS spotlight_content_type_tree,
       COALESCE(rr.last_updated_utc,'') AS spotlight_last_updated_utc,
       c.field_name AS missing_reference_source_field,
       c.reference_type,
       c.raw_reference_value,
       c.normalized_ios_path,
       c.missing_candidate_category,
       c.investigative_priority,
       c.investigative_priority_sort,
       c.investigative_reason,
       COALESCE(ctx.spotlight_text_full_or_sample,c.spotlight_text_context_sample,'') AS spotlight_text_preview,
       COALESCE(ctx.spotlight_text_full_or_sample,c.spotlight_text_context_sample,'') AS spotlight_text_full_or_sample,
       COALESCE(ctx.spotlight_text_length,LENGTH(COALESCE(c.spotlight_text_context_sample,''))) AS spotlight_text_length,
       '__spotlight_investigator_text_context' AS spotlight_text_source_field,
       CASE
         WHEN COALESCE(ctx.spotlight_text_full_or_sample,c.spotlight_text_context_sample,'')<>'' THEN 'TEXT_VISIBLE_IN_THIS_REPORT'
         WHEN c.spotlight_text_context_status LIKE 'TEXT_CONTEXT_RECOVERED%' THEN 'TEXT_CONTEXT_STATUS_INDICATES_TEXT_BUT_VALUE_EMPTY_REVIEW_SQLITE'
         ELSE 'NO_SAME_RECORD_TEXT_RECOVERED_IN_COMPACT_MODE'
       END AS spotlight_text_visibility_status,
       c.spotlight_text_context_status,
       COALESCE(ctx.spotlight_text_context_row_count,0) AS spotlight_text_context_row_count,
       c.ffs_lookup_source,c.ffs_lookup_status,c.residency_status,c.confidence,
       c.matched_file_name,c.matched_size_bytes,c.matched_zip_modified_utc,c.matched_protection_class,c.matched_app_container,c.matched_domain,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(c.reference_id AS TEXT),'') || '; field_name=' || COALESCE(c.field_name,'') || '; normalized_ios_path=' || COALESCE(c.normalized_ios_path,'') AS missing_reference_validation_locator,
       'raw_records.raw_record_id=' || COALESCE(CAST(rr.raw_record_id AS TEXT),'') || '; source_db=' || COALESCE(c.source_db,'') || '; store_guid=' || COALESCE(c.store_guid,'') || '; inode_or_object_id=' || COALESCE(c.inode_num,'') || '; store_id=' || COALESCE(c.store_id,'') AS spotlight_record_locator,
       CASE
         WHEN COALESCE(ctx.spotlight_text_full_or_sample,c.spotlight_text_context_sample,'')<>'' THEN 'Same-record Spotlight text/content context is included here so Missing From FFS can be reviewed without manual SQLite searching. Values remain compact samples; rerun diagnostic full-native mode only if complete raw property values are required.'
         ELSE 'No same-record text context was recovered in compact mode for this missing reference. This may mean the Spotlight record primarily held paths/identifiers, or the text is in fields not yet decoded by the native parser.'
       END AS content_visibility_note,
       c.interpretation_note
FROM vw_ios_spotlight_missing_from_ffs_candidates c
LEFT JOIN rr ON rr.source_id=c.source_id AND rr.store_guid=c.store_guid AND rr.source_db=c.source_db AND rr.inode_num=c.inode_num AND COALESCE(rr.store_id,'')=COALESCE(c.store_id,'')
LEFT JOIN ctx ON ctx.source_id=c.source_id AND ctx.store_guid=c.store_guid AND ctx.source_db=c.source_db AND ctx.inode_num=c.inode_num AND COALESCE(ctx.store_id,'')=COALESCE(c.store_id,'');

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_text_coverage_summary;
CREATE VIEW vw_ios_spotlight_missing_from_ffs_text_coverage_summary AS
SELECT source_id,store_guid,source_db,missing_candidate_category,investigative_priority,spotlight_text_visibility_status,
       COUNT(*) AS missing_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_object_count,
       SUM(CASE WHEN spotlight_text_visibility_status='TEXT_VISIBLE_IN_THIS_REPORT' THEN 1 ELSE 0 END) AS candidates_with_visible_text,
       SUM(CASE WHEN spotlight_text_visibility_status<>'TEXT_VISIBLE_IN_THIS_REPORT' THEN 1 ELSE 0 END) AS candidates_without_visible_text,
       MIN(NULLIF(spotlight_last_updated_utc,'')) AS earliest_spotlight_last_updated_utc,
       MAX(NULLIF(spotlight_last_updated_utc,'')) AS latest_spotlight_last_updated_utc,
       MIN(substr(normalized_ios_path,1,500)) AS min_missing_path_sample,
       MAX(substr(normalized_ios_path,1,500)) AS max_missing_path_sample,
       MAX(substr(spotlight_text_preview,1,1000)) AS spotlight_text_sample,
       'Shows whether Missing From FFS candidates have same-record Spotlight text visible in the normal investigator exports. Use ios_spotlight_missing_from_ffs_text_detail.csv for row-level content.' AS interpretation_note
FROM vw_ios_spotlight_missing_from_ffs_text_detail
GROUP BY source_id,store_guid,source_db,missing_candidate_category,investigative_priority,spotlight_text_visibility_status;


DROP VIEW IF EXISTS vw_ios_spotlight_text_context_review;
CREATE VIEW vw_ios_spotlight_text_context_review AS
WITH base AS (
  SELECT kv.raw_kv_id,
         COALESCE(rr.raw_record_id,0) AS raw_record_id,
         kv.source_id,
         kv.store_guid,
         kv.source_db,
         kv.inode_num AS spotlight_inode_or_object_id,
         kv.store_id AS spotlight_store_id,
         kv.parent_inode_num,
         rr.file_name,
         rr.display_name,
         rr.content_type,
         rr.last_updated_utc,
         kv.field_value AS spotlight_text_context_sample,
         lower(COALESCE(kv.field_value,'')) AS v,
         lower(COALESCE(rr.content_type,'')) AS ct
  FROM raw_key_values kv
  LEFT JOIN raw_records rr ON rr.source_id=kv.source_id AND rr.store_guid=kv.store_guid AND rr.source_db=kv.source_db AND rr.inode_num=kv.inode_num AND COALESCE(rr.store_id,'')=COALESCE(kv.store_id,'')
  WHERE kv.field_name='__spotlight_investigator_text_context')VSQLFIX",
R"VSQLFIX(
), labeled AS (
  SELECT *,
       CASE
         WHEN ct='public.message' OR v LIKE '%kmditemmessageservice%' OR v LIKE '%/sms/attachments/%' OR v LIKE '%com.apple.mobilesms%' THEN 'MESSAGE_OR_ATTACHMENT_CONTEXT'
         WHEN ct='public.email-message' OR v LIKE '%com_apple_mail_%' OR v LIKE '%kmditememail%' OR v LIKE '%from:%' OR v LIKE '%to:%' THEN 'MAIL_OR_EMAIL_CONTEXT'
         WHEN v LIKE '%_kmditembundleid=net.whatsapp.whatsapp%' OR v LIKE '%_kmditemexternalid=net.whatsapp.whatsapp%' OR v LIKE '%_kmditemdomainidentifier=net.whatsapp%' OR v LIKE '%_kmditemalternatenames=whatsapp%' THEN 'WHATSAPP_APP_OR_CHAT_CONTEXT'
         WHEN v LIKE '%_kmditembundleid=org.whispersystems.signal%' OR v LIKE '%_kmditemexternalid=org.whispersystems.signal%' OR v LIKE '%_kmditemdomainidentifier=org.whispersystems.signal%' OR v LIKE '%signal.messenger%' THEN 'SIGNAL_APP_OR_CHAT_CONTEXT'
         WHEN v LIKE '%_kmditembundleid=ph.telegra.telegraph%' OR v LIKE '%_kmditemexternalid=ph.telegra.telegraph%' OR v LIKE '%telegram.messenger%' OR v LIKE '%org.telegram%' THEN 'TELEGRAM_APP_OR_CHAT_CONTEXT'
         WHEN v LIKE '%whatsapp group%' OR v LIKE '%chat.whatsapp.com%' OR v LIKE '%wa.me/%' OR v LIKE '%api.whatsapp.com%' THEN 'WHATSAPP_LINK_OR_TEXT_MENTION'
         WHEN v LIKE '%t.me/%' OR v LIKE '%telegram.me/%' THEN 'TELEGRAM_LINK_OR_TEXT_MENTION'
         WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' OR v LIKE '%kmditemcontenturl%' THEN 'URL_OR_WEB_CONTEXT'
         WHEN ct='kspotlightitemtypecall' OR v LIKE '%tel://%' OR v LIKE '%callbackurl=tel%' THEN 'CALL_LOG_OR_PHONE_CONTEXT'
         WHEN ct='public.contact' OR v LIKE '%kmditemphonenumbers%' OR v LIKE '%contact%' THEN 'CONTACT_CONTEXT'
         WHEN ct='public.calendar-event' OR v LIKE '%calendar%' OR v LIKE '%event%' OR v LIKE '%.ics%' THEN 'CALENDAR_OR_EVENT_CONTEXT'
         WHEN ct LIKE 'public.%image%' OR ct IN ('public.jpeg','public.heic','public.png','com.compuserve.gif') THEN 'PHOTO_OR_MEDIA_CONTEXT'
         WHEN ct LIKE '%movie%' OR ct LIKE '%video%' OR ct IN ('public.mpeg-4','public.3gpp') THEN 'VIDEO_OR_MEDIA_CONTEXT'
         WHEN v LIKE '%/var/mobile/%' OR v LIKE '%file://%' THEN 'LOCAL_FILE_OR_PATH_CONTEXT'
         WHEN v LIKE '%@%' THEN 'ACCOUNT_OR_IDENTIFIER_CONTEXT'
         ELSE 'GENERAL_SPOTLIGHT_TEXT_CONTEXT'
       END AS text_context_category
  FROM base
), scored AS (
  SELECT *,
       CASE
         WHEN text_context_category IN ('MESSAGE_OR_ATTACHMENT_CONTEXT','MAIL_OR_EMAIL_CONTEXT') THEN 'HIGH_SPOTLIGHT_TEXT_VALUE'
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 'HIGH_APP_ATTRIBUTION_VALUE'
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 'MEDIUM_CHAT_LINK_OR_TEXT_VALUE'
         WHEN text_context_category IN ('URL_OR_WEB_CONTEXT','CALL_LOG_OR_PHONE_CONTEXT','CONTACT_CONTEXT','CALENDAR_OR_EVENT_CONTEXT','LOCAL_FILE_OR_PATH_CONTEXT','ACCOUNT_OR_IDENTIFIER_CONTEXT') THEN 'MEDIUM_SPOTLIGHT_TEXT_VALUE'
         WHEN text_context_category IN ('PHOTO_OR_MEDIA_CONTEXT','VIDEO_OR_MEDIA_CONTEXT') THEN 'MEDIUM_MEDIA_CONTEXT_VALUE'
         ELSE 'LOW_GENERAL_TEXT_VALUE'
       END AS review_priority,
       CASE
         WHEN text_context_category='MESSAGE_OR_ATTACHMENT_CONTEXT' THEN 1
         WHEN text_context_category='MAIL_OR_EMAIL_CONTEXT' THEN 2
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 3
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 9
         WHEN text_context_category='URL_OR_WEB_CONTEXT' THEN 4
         WHEN text_context_category='CALL_LOG_OR_PHONE_CONTEXT' THEN 5
         WHEN text_context_category='CONTACT_CONTEXT' THEN 6
         WHEN text_context_category='CALENDAR_OR_EVENT_CONTEXT' THEN 7
         WHEN text_context_category='LOCAL_FILE_OR_PATH_CONTEXT' THEN 8
         WHEN text_context_category='ACCOUNT_OR_IDENTIFIER_CONTEXT' THEN 10
         WHEN text_context_category IN ('PHOTO_OR_MEDIA_CONTEXT','VIDEO_OR_MEDIA_CONTEXT') THEN 10
         ELSE 20
       END AS review_priority_sort,
       CASE
         WHEN text_context_category='MESSAGE_OR_ATTACHMENT_CONTEXT' THEN 'Same-record Spotlight context indicates message, SMS/RCS/iMessage, or attachment evidence; prioritize for communications review and Missing From FFS triage.'
         WHEN text_context_category='MAIL_OR_EMAIL_CONTEXT' THEN 'Same-record Spotlight context indicates mail/email content or identifiers; prioritize for communication and account review.'
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 'Same-record Spotlight context contains explicit bundle/domain/external-id evidence for a chat application; prioritize for app attribution and indexed-only/deleted candidate review.'
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 'Same-record Spotlight context contains a chat-app link or textual mention only; review as possible communication/link evidence, not as installed-app attribution by itself.'
         WHEN text_context_category='URL_OR_WEB_CONTEXT' THEN 'Same-record Spotlight context includes URL/web content; review for browsing, shared links, or app deep-link evidence.'
         WHEN text_context_category='CALL_LOG_OR_PHONE_CONTEXT' THEN 'Same-record Spotlight context indicates call/phone callback evidence; correlate with CallHistory if support parsing is enabled.'
         WHEN text_context_category='CONTACT_CONTEXT' THEN 'Same-record Spotlight context indicates contact/person/account metadata.'
         WHEN text_context_category='CALENDAR_OR_EVENT_CONTEXT' THEN 'Same-record Spotlight context indicates calendar or event-like metadata.'
         WHEN text_context_category IN ('PHOTO_OR_MEDIA_CONTEXT','VIDEO_OR_MEDIA_CONTEXT') THEN 'Same-record Spotlight context indicates media; review with dates/path context before drawing usage conclusions.'
         ELSE 'General same-record Spotlight text context retained in compact normal mode.'
       END AS text_context_reason
  FROM labeled
)
SELECT raw_kv_id,raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       file_name,display_name,content_type,last_updated_utc,text_context_category,review_priority,review_priority_sort,text_context_reason,
       CASE
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 'EXPLICIT_CHAT_APP_BUNDLE_DOMAIN_OR_EXTERNAL_ID'
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 'CHAT_LINK_OR_TEXT_MENTION_ONLY_NOT_APP_ATTRIBUTION'
         WHEN text_context_category='MESSAGE_OR_ATTACHMENT_CONTEXT' THEN 'APPLE_MESSAGE_CONTENT_TYPE_OR_MOBILESMS_FIELD'
         WHEN text_context_category='MAIL_OR_EMAIL_CONTEXT' THEN 'MAIL_CONTENT_TYPE_OR_MAIL_SEARCH_INDEXER_FIELD'
         ELSE 'CATEGORY_BY_CONTENT_TYPE_FIELD_OR_VALUE_PATTERN'
       END AS classification_evidence,
       spotlight_text_context_sample,
       'Compact same-record Spotlight text context retained in normal iOS mode; this is not a full raw property dump. Use raw_kv_id/raw_record_id/source_db to validate against the local SQLite database.' AS interpretation_note
FROM scored;
)VSQLFIX",
R"VSQLFIX(
DROP VIEW IF EXISTS vw_ios_spotlight_high_value_text_context_review;
CREATE VIEW vw_ios_spotlight_high_value_text_context_review AS
SELECT *
FROM vw_ios_spotlight_text_context_review
WHERE review_priority_sort <= 9
ORDER BY review_priority_sort,last_updated_utc DESC,raw_record_id DESC;
)VSQLFIX",
R"VSQLFIX(
DROP VIEW IF EXISTS vw_ios_spotlight_text_context_priority_summary;
CREATE VIEW vw_ios_spotlight_text_context_priority_summary AS
SELECT text_context_category,review_priority,review_priority_sort,
       COUNT(*) AS text_context_record_count,
       COUNT(DISTINCT source_id || ':' || store_guid || ':' || COALESCE(spotlight_inode_or_object_id,'') || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       COUNT(NULLIF(last_updated_utc,'')) AS rows_with_last_updated,
       MIN(last_updated_utc) AS earliest_last_updated_utc,
       MAX(last_updated_utc) AS latest_last_updated_utc,
       substr(MIN(spotlight_text_context_sample),1,1000) AS min_text_context_sample,
       substr(MAX(spotlight_text_context_sample),1,1000) AS max_text_context_sample,
       MIN(classification_evidence) AS classification_evidence,
       MIN(text_context_reason) AS text_context_reason,
       'Priority summary for compact same-record Spotlight text retained in normal iOS mode. Priorities are triage labels, not final Apple schema classifications.' AS interpretation_note
FROM vw_ios_spotlight_text_context_review
GROUP BY text_context_category,review_priority,review_priority_sort;

DROP VIEW IF EXISTS vw_ios_spotlight_chat_app_attribution_summary;
CREATE VIEW vw_ios_spotlight_chat_app_attribution_summary AS
SELECT text_context_category,review_priority,classification_evidence,
       COUNT(*) AS context_record_count,
       COUNT(DISTINCT source_id || ':' || store_guid || ':' || COALESCE(spotlight_inode_or_object_id,'') || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       MIN(last_updated_utc) AS earliest_last_updated_utc,
       MAX(last_updated_utc) AS latest_last_updated_utc,
       substr(MIN(spotlight_text_context_sample),1,1000) AS min_text_context_sample,
       substr(MAX(spotlight_text_context_sample),1,1000) AS max_text_context_sample,
       MIN(text_context_reason) AS text_context_reason,
       'V0_9_26 separates explicit chat-app bundle/domain/external-id attribution from plain keyword/link mentions so words like Signal Hill do not inflate Signal app evidence.' AS interpretation_note
FROM vw_ios_spotlight_text_context_review
WHERE text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT','WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION')
GROUP BY text_context_category,review_priority,classification_evidence;
)VSQLFIX",
R"VSQLFIX(
DROP VIEW IF EXISTS vw_ios_spotlight_human_text_values;
CREATE VIEW vw_ios_spotlight_human_text_values AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,store_path,inode_num,store_id,parent_inode_num,field_name,field_value,
         LOWER(COALESCE(field_value,'')) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), labeled AS (
  SELECT *,
    CASE
      WHEN v LIKE '%file://%' OR v LIKE '%/private/var/mobile/%' OR v LIKE '%/var/mobile/%' THEN 'FILE_PATH_OR_ATTACHMENT'
      WHEN v LIKE '%zoom.%' OR v LIKE '%meet.google.%' OR v LIKE '%teams.microsoft.%' OR v LIKE '%webex.%' THEN 'MEETING_OR_CONFERENCE'
      WHEN v LIKE '%.ics%' OR v LIKE '%text/calendar%' OR v LIKE '%vevent%' OR v LIKE '%calendar.google.com%' OR v LIKE '%/calendar/%' THEN 'CALENDAR_OR_INVITATION'
      WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'WEB_OR_URL'
      WHEN v LIKE 'from:%' OR v LIKE 'to:%' OR v LIKE 'cc:%' OR v LIKE '%mailto:%' OR (v LIKE '%@%' AND v NOT LIKE '%http://%' AND v NOT LIKE '%https://%') THEN 'EMAIL_OR_ACCOUNT_TEXT'
      WHEN v LIKE '%net.whatsapp.whatsapp%' OR v LIKE '%chat.whatsapp.com%' OR v LIKE '%wa.me/%' OR v LIKE '%api.whatsapp.com%' OR v LIKE '%whatsapp group%' THEN 'WHATSAPP_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.whispersystems.signal%' OR v LIKE '%signal.messenger%' OR v LIKE '%signal.org/%' THEN 'SIGNAL_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.telegram%' OR v LIKE '%telegram.messenger%' OR v LIKE '%t.me/%' OR v LIKE '%telegram.me/%' THEN 'TELEGRAM_TEXT_OR_REFERENCE'
      ELSE 'OTHER_HUMAN_READABLE_TEXT'
    END AS human_text_category,
    TRIM(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(COALESCE(field_value,''),
      char(13),' '), char(10),' '), char(9),' '), '<br>',' '), '<br/>',' '), '<br />',' '),
      '&amp;','&'), '&lt;','<'), '&gt;','>'), '&nbsp;',' '), '&quot;','"')) AS readable_text
  FROM probes
)
SELECT l.raw_kv_id,COALESCE(r.raw_record_id,0) AS raw_record_id,l.source_id,l.store_guid,l.source_db,l.inode_num,l.store_id,l.parent_inode_num,l.field_name,
       l.human_text_category,
       LENGTH(COALESCE(l.field_value,'')) AS original_value_length,
       substr(l.readable_text,1,3000) AS readable_text_sample,
       CASE
         WHEN l.human_text_category IN ('FILE_PATH_OR_ATTACHMENT','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION','WEB_OR_URL','EMAIL_OR_ACCOUNT_TEXT') THEN 'HIGH_HUMAN_REVIEW_VALUE'
         WHEN LENGTH(l.readable_text)>=24 THEN 'MEDIUM_HUMAN_REVIEW_VALUE'
         ELSE 'LOW_HUMAN_REVIEW_VALUE'
       END AS review_priority,
       'Generic iOS CoreSpotlight text recovery; formal CoreSpotlight property names/dbStr maps remain a later parser phase.' AS interpretation_note
FROM labeled l
LEFT JOIN (
  SELECT source_id,store_guid,source_db,inode_num,COALESCE(store_id,'') AS store_id_key,MIN(raw_record_id) AS raw_record_id
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,inode_num,COALESCE(store_id,'')
) r ON r.source_id=l.source_id AND r.store_guid=l.store_guid AND r.source_db=l.source_db AND r.inode_num=l.inode_num AND r.store_id_key=COALESCE(l.store_id,'')
WHERE LENGTH(l.readable_text)>=4
  AND lower(COALESCE(l.field_name,'')) NOT LIKE '%ranking%'
  AND lower(COALESCE(l.field_name,'')) NOT LIKE 'kmdstore%'
  AND lower(COALESCE(l.field_name,'')) NOT LIKE '_kstore%'
  AND lower(COALESCE(l.field_name,'')) NOT IN (
    '_kmditemserialnumber','_kmditemstoragebytes','_kmditemstoragesize','kmditemphysicalsize',
    '_kmditemcontentindexversion','_kmditemtextcontentindexexists','_kmditemgroupid',
    'kmdstoreuuid','kmdstoreaccumulatedsizes','_kmdclientcheckedin'
  )
  AND (
    l.human_text_category IN ('FILE_PATH_OR_ATTACHMENT','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION','WEB_OR_URL','EMAIL_OR_ACCOUNT_TEXT','WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE')
    OR lower(COALESCE(l.field_name,'')) IN (
      '_kmditemexternalid','_kmditembundleid','_kmditemdomainidentifier','_kmditemauthoridentifier',
      'kmditemtitle','kmditemdisplayname','kmditemdescription','kmditemheadline','kmditemcomment','kmditemkeywords',
      'kmditemidentifier','kmditemurl','kmditemcontenturl','kmditempath','kmditemfsname','kmditemwherefroms',
      'kmditemauthors','kmditemauthoraddresses','kmditemrecipients','kmditemrecipientaddresses',
      'kmditemcontaineridentifier','kmditemcontainertitle','kmditemcontainerdisplayname',
      'com_apple_mail_subject','com_apple_mail_sender','com_apple_mail_to','com_apple_mail_cc','com_apple_mail_messageid',
      'com_apple_mobilesms_handle','com_apple_mobilesms_guid','com_apple_mobilesms_chatidentifier'
    )
    OR lower(COALESCE(l.field_name,'')) LIKE '%title%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%subject%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%url%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%path%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%identifier%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%sender%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%recipient%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%author%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%account%'
  );

DROP VIEW IF EXISTS vw_ios_spotlight_human_text_rollup;
CREATE VIEW vw_ios_spotlight_human_text_rollup AS
WITH text_values AS (
  SELECT v.*, r.last_updated_utc, r.record_state
  FROM vw_ios_spotlight_human_text_values v
  LEFT JOIN raw_records r ON r.raw_record_id=v.raw_record_id
)
SELECT raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT human_text_category) AS distinct_text_category_count,
       GROUP_CONCAT(DISTINCT human_text_category) AS human_text_categories,
       MAX(CASE WHEN review_priority='HIGH_HUMAN_REVIEW_VALUE' THEN 1 ELSE 0 END) AS has_high_review_value_text,
       MIN(NULLIF(last_updated_utc,'')) AS last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       substr(GROUP_CONCAT(field_name || '=' || readable_text_sample, ' || '),1,6000) AS readable_text_rollup_sample,
       'Record-level human-readable text rollup from iOS CoreSpotlight string probes.' AS interpretation_note
FROM text_values
)VSQLFIX",
R"VSQLFIX(GROUP BY raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num;


DROP VIEW IF EXISTS vw_ios_spotlight_investigative_items_with_dates;
CREATE VIEW vw_ios_spotlight_investigative_items_with_dates AS
SELECT v.raw_kv_id,
       v.raw_record_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.original_value_length,
       v.readable_text_sample,
       v.review_priority,
       dp.spotlight_date_utc,
       dp.spotlight_date_source_field,
       dp.spotlight_date_source_table,
       dp.spotlight_date_raw_value,
       dp.spotlight_date_parse_method,
       dp.spotlight_date_type,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%creation%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%created%' THEN 'created_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modification%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modified%' THEN 'modified_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%access%' THEN 'accessed_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%open%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'metadata_seen_or_index_updated'
         ELSE 'unclassified_spotlight_date_candidate'
       END AS spotlight_date_semantic_class,
       dp.spotlight_date_source_evidence,
       dp.date_validation_hint,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'Do not report as created/modified/accessed/opened. This is CoreSpotlight metadata/index update timing unless another decoded field supports activity semantics.'
         WHEN COALESCE(dp.spotlight_date_source_field,'')<>'' THEN 'Report only with the listed raw Spotlight source field, raw value, parse method, and validation hint.'
         ELSE 'No direct Spotlight date was recovered for this text value.'
       END AS date_reporting_caution,
       'Spotlight/CoreSpotlight extracted text item with attached date provenance where directly linkable by raw_record_id. FFS/app database data is supporting context only.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
LEFT JOIN vw_ios_spotlight_date_provenance dp ON dp.raw_record_id=v.raw_record_id;

DROP VIEW IF EXISTS vw_ios_spotlight_date_field_summary;
CREATE VIEW vw_ios_spotlight_date_field_summary AS
WITH dates AS (
  SELECT source_id, store_guid, source_db, field_name, parse_method, date_type,
         parsed_utc, field_value, inode_num, store_id,
         CASE
           WHEN lower(COALESCE(field_name,'')) LIKE '%creation%' OR lower(COALESCE(field_name,'')) LIKE '%created%' THEN 'created_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%modification%' OR lower(COALESCE(field_name,'')) LIKE '%modified%' THEN 'modified_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%access%' THEN 'accessed_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%open%' OR lower(COALESCE(field_name,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
           WHEN lower(COALESCE(field_name,''))='last_updated' THEN 'metadata_seen_or_index_updated'
           ELSE 'unclassified_spotlight_date_candidate'
         END AS spotlight_date_semantic_class
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
)
SELECT source_id, store_guid, source_db, field_name AS spotlight_date_source_field,
       spotlight_date_semantic_class, COALESCE(date_type,'') AS raw_date_type,
       COALESCE(parse_method,'') AS parse_method,
       COUNT(*) AS date_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_record_count,
       MIN(parsed_utc) AS earliest_parsed_utc,
       MAX(parsed_utc) AS latest_parsed_utc,
       substr(MIN(field_value),1,500) AS min_raw_value_sample,
       substr(MAX(field_value),1,500) AS max_raw_value_sample,
       CASE
         WHEN spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'CoreSpotlight metadata/index update timing; do not report as created/modified/accessed/opened usage without another decoded field.'
         WHEN spotlight_date_semantic_class LIKE '%candidate' THEN 'Candidate semantic class inferred from Spotlight raw date field name; validate field meaning before reporting.'
         ELSE 'Unclassified Spotlight date candidate; validate against raw_date_candidates and source Store-V2 record.'
       END AS reporting_caution
FROM dates
GROUP BY source_id, store_guid, source_db, field_name, spotlight_date_semantic_class, date_type, parse_method;

DROP VIEW IF EXISTS vw_ios_spotlight_investigative_item_date_evidence;
CREATE VIEW vw_ios_spotlight_investigative_item_date_evidence AS
SELECT v.raw_kv_id,
       v.raw_record_id,
       NULL AS raw_date_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.review_priority,
       v.original_value_length,
       v.readable_text_sample,
       dp.spotlight_date_utc,
       dp.spotlight_date_source_field,
       dp.spotlight_date_source_table,
       dp.spotlight_date_raw_value,
       dp.spotlight_date_parse_method,
       dp.spotlight_date_type,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%creation%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%created%' THEN 'created_date_candidate'
)VSQLFIX",
R"VSQLFIX(         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modification%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modified%' THEN 'modified_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%access%' THEN 'accessed_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%open%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'metadata_seen_or_index_updated'
         ELSE 'unclassified_spotlight_date_candidate'
       END AS spotlight_date_semantic_class,
       '' AS date_association_status,
       '' AS date_association_confidence,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(v.raw_kv_id AS TEXT),'') || '; field_name=' || COALESCE(v.field_name,'') AS value_validation_locator,
       'aggregated_date_provenance; source_fields=' || COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(dp.spotlight_date_raw_value,'') || '; parsed_utc=' || COALESCE(dp.spotlight_date_utc,'') || '; parse_method=' || COALESCE(dp.spotlight_date_parse_method,'') AS date_validation_locator,
       'source_db=' || COALESCE(v.source_db,'') || '; store_guid=' || COALESCE(v.store_guid,'') || '; raw_record_id=' || COALESCE(CAST(v.raw_record_id AS TEXT),'') || '; inode_or_object_id=' || COALESCE(v.inode_num,'') || '; store_id=' || COALESCE(v.store_id,'') AS spotlight_record_locator,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'Date is linked to this Spotlight record but represents CoreSpotlight metadata/index update timing unless a separately decoded field supports user activity.'
         WHEN COALESCE(dp.spotlight_date_source_field,'')<>'' THEN 'Date is aggregated at the Spotlight record level. Use date_source_evidence/raw_date_candidates for full per-date validation; full per-value/per-date expansion is diagnostics-only.'
         ELSE 'No direct Spotlight date was recovered for this value.'
       END AS date_reporting_caution,
       'Each row links one recovered investigative Spotlight value to aggregated date provenance for the same Store-V2 record. This avoids the prior many-to-many value/date expansion that produced tens of millions of rows in normal exports.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
LEFT JOIN vw_ios_spotlight_date_provenance dp ON dp.raw_record_id=v.raw_record_id;

)VSQLFIX"
}));

    exec(R"SQL(

DROP VIEW IF EXISTS vw_ios_spotlight_high_value_timeline;
CREATE VIEW vw_ios_spotlight_high_value_timeline AS
WITH base AS (
  SELECT * FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE review_priority IN ('HIGH_HUMAN_REVIEW_VALUE','MEDIUM_HUMAN_REVIEW_VALUE')
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.original_value_length,
       b.readable_text_sample,b.review_priority,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_source_table,
       b.spotlight_date_raw_value,b.spotlight_date_parse_method,b.spotlight_date_type,
       b.spotlight_date_semantic_class,b.date_validation_hint,b.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FILE_PATH_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       CASE
         WHEN b.spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'SPOTLIGHT_INDEX_TIME_WITH_VALUE_CONTEXT'
         WHEN b.spotlight_date_semantic_class LIKE '%candidate' THEN 'SPOTLIGHT_ACTIVITY_DATE_CANDIDATE_WITH_VALUE_CONTEXT'
         ELSE 'SPOTLIGHT_VALUE_WITH_UNCLASSIFIED_DATE_CONTEXT'
       END AS investigative_timeline_basis,
       'Spotlight-first high-value timeline. FFS and app database fields are context/corroboration only; the Spotlight value/date fields remain the primary evidence to validate.' AS interpretation_note
FROM base b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_file_reference_review;
CREATE VIEW vw_ios_spotlight_file_reference_review AS
WITH ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.readable_text_sample AS spotlight_file_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(f.residency_status,'NO_EXACT_FFS_PATH_LINK') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       CASE WHEN COALESCE(f.residency_status,'')='PRESENT_AS_FILE_IN_FFS' THEN 'SPOTLIGHT_PATH_PRESENT_IN_FFS_INVENTORY'
            WHEN COALESCE(f.residency_status,'')<>'' THEN f.residency_status
            ELSE 'SPOTLIGHT_FILE_REFERENCE_NO_EXACT_FFS_MATCH_IN_CURRENT_LINK_VIEW' END AS file_reference_status,
       'Spotlight/CoreSpotlight file/path reference with date provenance. FFS presence supports current-file existence only and is not proof of use or deletion by itself.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
WHERE b.human_text_category='FILE_PATH_OR_ATTACHMENT';

)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_url_reference_review;
CREATE VIEW vw_ios_spotlight_url_reference_review AS
WITH vals AS (
  SELECT *, lower(COALESCE(readable_text_sample,'')) AS v
  FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION')
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT v.raw_kv_id,v.raw_record_id,v.source_id,v.store_guid,v.source_db,
       v.spotlight_inode_or_object_id,v.spotlight_store_id,v.parent_inode_num,
       v.spotlight_value_source_field,v.human_text_category,v.readable_text_sample AS spotlight_url_or_web_reference,
       CASE
         WHEN instr(v.v,'https://')>0 THEN substr(v.v,instr(v.v,'https://'),300)
         WHEN instr(v.v,'http://')>0 THEN substr(v.v,instr(v.v,'http://'),300)
         WHEN instr(v.v,'www.')>0 THEN substr(v.v,instr(v.v,'www.'),300)
         ELSE substr(v.v,1,300)
       END AS normalized_url_reference_sample,
       v.spotlight_date_utc,v.spotlight_date_source_field,v.spotlight_date_raw_value,
       v.spotlight_date_parse_method,v.spotlight_date_semantic_class,v.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       'Spotlight/CoreSpotlight URL/web-like reference with date provenance. Browser/app database fields are supporting context and not an exact value match unless separately validated.' AS interpretation_note
FROM vals v
LEFT JOIN app a ON a.candidate_id=v.raw_kv_id;

)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_account_contact_reference_review;
CREATE VIEW vw_ios_spotlight_account_contact_reference_review AS
WITH app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.readable_text_sample AS spotlight_account_or_contact_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       'Spotlight/CoreSpotlight account/contact-like reference with date provenance. Treat as a Spotlight value first; app database context is family-level unless exact string matching is later added.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id
WHERE b.human_text_category IN ('EMAIL_OR_ACCOUNT_TEXT','WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE');

DROP VIEW IF EXISTS vw_ios_spotlight_decode_gap_summary;
CREATE VIEW vw_ios_spotlight_decode_gap_summary AS
WITH gaps AS (
  SELECT source_id,store_guid,source_db,decode_gap_status,last_updated_utc
  FROM vw_ios_spotlight_decode_gap_records
)
SELECT g.source_id,g.store_guid,g.source_db,g.decode_gap_status,
       COUNT(*) AS gap_record_count,
       MIN(NULLIF(g.last_updated_utc,'')) AS earliest_gap_last_updated_utc,
       MAX(NULLIF(g.last_updated_utc,'')) AS latest_gap_last_updated_utc,
       COALESCE(dc.raw_record_count,0) AS store_raw_record_count,
       COALESCE(dc.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(dc.human_text_value_count,0) AS human_text_value_count,
       COALESCE(dc.pct_records_with_human_text,'') AS pct_records_with_human_text,
       COALESCE(dc.decode_failures,0) AS native_decode_failures,
       COALESCE(dc.decode_status,'') AS native_decode_status,
       'Summary of Spotlight/CoreSpotlight records parsed at header level but lacking recovered key/value or human-readable text values. This is the primary native parser improvement target list.' AS interpretation_note
FROM gaps g
LEFT JOIN vw_ios_spotlight_decode_coverage_summary dc ON dc.source_id=g.source_id AND dc.store_guid=g.store_guid AND dc.source_db=g.source_db
GROUP BY g.source_id,g.store_guid,g.source_db,g.decode_gap_status;

)SQL");

    exec(joinSql({
R"VSQLFIX(

DROP VIEW IF EXISTS vw_ios_spotlight_entity_review;
CREATE VIEW vw_ios_spotlight_entity_review AS
WITH base AS (
  SELECT b.*,
         lower(COALESCE(b.readable_text_sample,'')) AS lower_text
  FROM vw_ios_spotlight_investigative_items_with_dates b
  WHERE COALESCE(b.readable_text_sample,'')<>''
), typed AS (
  SELECT b.*,
         CASE
           WHEN b.human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION') THEN 'URL_OR_WEB_REFERENCE'
           WHEN b.human_text_category='FILE_PATH_OR_ATTACHMENT' THEN 'FILE_OR_ATTACHMENT_REFERENCE'
           WHEN b.human_text_category='EMAIL_OR_ACCOUNT_TEXT' THEN 'ACCOUNT_OR_EMAIL_REFERENCE'
           WHEN b.human_text_category IN ('WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE') THEN 'COMMUNICATION_APP_REFERENCE'
           WHEN b.human_text_category LIKE '%MESSAGE%' THEN 'MESSAGE_OR_COMMUNICATION_TEXT'
           ELSE 'OTHER_SPOTLIGHT_TEXT_REFERENCE'
         END AS entity_type
  FROM base b
), normalized AS (
  SELECT t.*,
         CASE
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'https://')>0 THEN substr(t.lower_text,instr(t.lower_text,'https://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'http://')>0 THEN substr(t.lower_text,instr(t.lower_text,'http://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'www.')>0 THEN substr(t.lower_text,instr(t.lower_text,'www.'),512)
           WHEN t.entity_type='FILE_OR_ATTACHMENT_REFERENCE' THEN replace(replace(replace(replace(t.lower_text,'file://',''),'<',''),'>',''),'\\','/')
           ELSE trim(t.lower_text)
         END AS normalized_entity_value
  FROM typed t
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,matched_record_category,matched_table_name,parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc,sample_parsed_value
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT n.raw_kv_id,
       n.raw_record_id,
       n.source_id,
       n.store_guid,
       n.source_db,
       n.spotlight_inode_or_object_id,
       n.spotlight_store_id,
       n.parent_inode_num,
       n.entity_type,
       n.human_text_category,
       n.review_priority,
       n.spotlight_value_source_field,
       n.normalized_entity_value,
       n.readable_text_sample,
       n.original_value_length,
       n.spotlight_date_utc,
       n.spotlight_date_source_field,
       n.spotlight_date_raw_value,
       n.spotlight_date_parse_method,
       n.spotlight_date_semantic_class,
       n.date_validation_hint,
       n.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FFS_LINK_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_LINK_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.matched_record_category,'') AS matched_record_category,
       COALESCE(a.matched_table_name,'') AS matched_table_name,
       COALESCE(a.sample_parsed_value,'') AS sample_app_db_value,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(n.raw_kv_id AS TEXT),'') || '; raw_records.raw_record_id=' || COALESCE(CAST(n.raw_record_id AS TEXT),'') || '; field_name=' || COALESCE(n.spotlight_value_source_field,'') AS reference_validation_locator,
       'raw_date_candidates.field_name=' || COALESCE(n.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(n.spotlight_date_raw_value,'') || '; parse_method=' || COALESCE(n.spotlight_date_parse_method,'') AS date_validation_locator,
       'Spotlight-first entity view. The entity/value and date columns originate from CoreSpotlight/Spotlight parsed records; app DB and FFS fields are corroborating context only.' AS interpretation_note
FROM normalized n
LEFT JOIN ffs f ON f.reference_id=n.raw_kv_id
LEFT JOIN app a ON a.candidate_id=n.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_entity_summary;
CREATE VIEW vw_ios_spotlight_entity_summary AS
SELECT entity_type,
       human_text_category,
       review_priority,
       store_guid,
       source_db,
       spotlight_value_source_field,
       spotlight_date_semantic_class,
       COUNT(*) AS entity_row_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT normalized_entity_value) AS distinct_normalized_entity_count,
       SUM(CASE WHEN ffs_residency_status='PRESENT_AS_FILE_IN_FFS' THEN 1 ELSE 0 END) AS ffs_present_context_count,
       SUM(CASE WHEN app_db_link_status LIKE 'PRESENT%' OR app_db_link_status LIKE '%PRESENT%' THEN 1 ELSE 0 END) AS app_db_present_context_count,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       MIN(substr(normalized_entity_value,1,240)) AS min_sample_entity,
       MAX(substr(normalized_entity_value,1,240)) AS max_sample_entity,
       'Spotlight entity summary. Counts are derived from recovered CoreSpotlight text/probe values and their Spotlight date provenance; app/FFS context is only supporting context.' AS interpretation_note
FROM vw_ios_spotlight_entity_review
GROUP BY entity_type,human_text_category,review_priority,store_guid,source_db,spotlight_value_source_field,spotlight_date_semantic_class;

DROP VIEW IF EXISTS vw_ios_spotlight_native_parser_targets;
CREATE VIEW vw_ios_spotlight_native_parser_targets AS
SELECT source_id,
       store_guid,
       source_db,
       'RECORDS_WITHOUT_RECOVERED_TEXT' AS parser_target_type,
       decode_gap_status AS target_name,
       gap_record_count AS target_count,
       store_raw_record_count AS store_raw_record_count,
)VSQLFIX",
R"VSQLFIX(       recovered_key_value_count AS recovered_key_value_count,
       human_text_value_count AS human_text_value_count,
       pct_records_with_human_text AS pct_records_with_human_text,
       native_decode_failures AS native_decode_failures,
       CASE WHEN gap_record_count>100000 THEN 'HIGH' WHEN gap_record_count>10000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Improve native CoreSpotlight property/dictionary/value decoding for records that parse at header level but do not yield recovered text/key-value rows.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_decode_gap_summary
UNION ALL
SELECT source_id,
       store_guid,
       source_db,
       'GENERIC_STRING_PROBE_FIELD' AS parser_target_type,
       field_name AS target_name,
       value_row_count AS target_count,
       NULL AS store_raw_record_count,
       value_row_count AS recovered_key_value_count,
       distinct_record_count AS human_text_value_count,
       '' AS pct_records_with_human_text,
       NULL AS native_decode_failures,
       CASE WHEN value_row_count>10000 THEN 'HIGH' WHEN value_row_count>1000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Map generic __native_core_probe_string_* fields back to real CoreSpotlight property names/types where possible.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_field_coverage_summary
WHERE field_decode_status='GENERIC_NATIVE_STRING_PROBE';

DROP VIEW IF EXISTS vw_ios_spotlight_decode_coverage_summary;
CREATE VIEW vw_ios_spotlight_decode_coverage_summary AS
WITH rr AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS raw_record_count,
         SUM(CASE WHEN COALESCE(last_updated_utc,'')<>'' THEN 1 ELSE 0 END) AS records_with_last_updated,
         MIN(NULLIF(last_updated_utc,'')) AS earliest_last_updated_utc,
         MAX(NULLIF(last_updated_utc,'')) AS latest_last_updated_utc
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), kv AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS recovered_key_value_count,
         COUNT(DISTINCT field_name) AS recovered_field_name_count,
         COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS records_with_recovered_values
  FROM raw_key_values
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), ht AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS human_text_value_count,
         COUNT(DISTINCT raw_record_id) AS records_with_human_text,
         COUNT(DISTINCT human_text_category) AS human_text_category_count
  FROM vw_ios_spotlight_human_text_values
  GROUP BY source_id,store_guid,source_db
), nd AS (
  SELECT source_id,store_guid,source_db,
         MAX(decode_mode) AS decode_mode,
         MAX(spotlight_version) AS spotlight_version,
         MAX(properties_count) AS native_property_count,
         MAX(categories_count) AS native_category_count,
         MAX(metadata_blocks) AS metadata_blocks,
         MAX(decompressed_blocks) AS decompressed_blocks,
         MAX(failures) AS decode_failures,
         MAX(status) AS decode_status,
         MAX(message) AS decode_message
  FROM native_decode_attempts
  GROUP BY source_id,store_guid,source_db
)
SELECT rr.source_id,rr.store_guid,rr.source_db,
       COALESCE(nd.decode_mode,'') AS decode_mode,
       COALESCE(nd.spotlight_version,0) AS spotlight_version,
       rr.raw_record_count,
       COALESCE(kv.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(kv.recovered_field_name_count,0) AS recovered_field_name_count,
       COALESCE(kv.records_with_recovered_values,0) AS records_with_recovered_values,
       COALESCE(ht.human_text_value_count,0) AS human_text_value_count,
       COALESCE(ht.records_with_human_text,0) AS records_with_human_text,
       COALESCE(ht.human_text_category_count,0) AS human_text_category_count,
       CASE WHEN rr.raw_record_count>0 THEN printf('%.2f', 100.0 * COALESCE(ht.records_with_human_text,0) / rr.raw_record_count) ELSE '0.00' END AS pct_records_with_human_text,
       COALESCE(nd.native_property_count,0) AS native_property_count,
       COALESCE(nd.native_category_count,0) AS native_category_count,
       COALESCE(nd.metadata_blocks,0) AS metadata_blocks,
       COALESCE(nd.decompressed_blocks,0) AS decompressed_blocks,
       COALESCE(nd.decode_failures,0) AS decode_failures,
       COALESCE(nd.decode_status,'NO_NATIVE_DECODE_ATTEMPT_ROW') AS decode_status,
       rr.earliest_last_updated_utc,rr.latest_last_updated_utc,
       CASE WHEN COALESCE(nd.native_property_count,0)=0 THEN 'PROPERTY_DICTIONARY_NOT_DECODED_GENERIC_PROBES_ONLY'
            WHEN COALESCE(kv.recovered_key_value_count,0)=0 THEN 'NO_KEY_VALUES_RECOVERED'
            ELSE 'GENERIC_TEXT_VALUES_RECOVERED' END AS spotlight_decode_interpretation,
       'Spotlight-first coverage view. App/FFS correlation is supporting context; this row measures native CoreSpotlight record/value recovery.' AS interpretation_note
FROM rr
LEFT JOIN kv ON kv.source_id=rr.source_id AND kv.store_guid=rr.store_guid AND kv.source_db=rr.source_db
LEFT JOIN ht ON ht.source_id=rr.source_id AND ht.store_guid=rr.store_guid AND ht.source_db=rr.source_db
LEFT JOIN nd ON nd.source_id=rr.source_id AND nd.store_guid=rr.store_guid AND nd.source_db=rr.source_db;

DROP VIEW IF EXISTS vw_ios_spotlight_field_coverage_summary;
CREATE VIEW vw_ios_spotlight_field_coverage_summary AS
SELECT source_id,store_guid,source_db,field_name,
       COUNT(*) AS value_row_count,
       COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS distinct_record_count,
       MIN(LENGTH(COALESCE(field_value,''))) AS min_value_length,
       MAX(LENGTH(COALESCE(field_value,''))) AS max_value_length,
       substr(MIN(COALESCE(field_value,'')),1,1000) AS min_sample_value,
       substr(MAX(COALESCE(field_value,'')),1,1000) AS max_sample_value,
       CASE WHEN field_name LIKE '__native_core_probe_string_%' THEN 'GENERIC_NATIVE_STRING_PROBE'
            WHEN field_name LIKE '__native_%' THEN 'GENERIC_NATIVE_FIELD'
            ELSE 'NAMED_SPOTLIGHT_FIELD' END AS field_decode_status,
       'Field coverage summary from recovered Spotlight key/value rows. Generic probe names indicate values recovered before formal property-name mapping.' AS interpretation_note
)VSQLFIX",
R"VSQLFIX(FROM raw_key_values
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(field_value,'')<>''
GROUP BY source_id,store_guid,source_db,field_name;

DROP VIEW IF EXISTS vw_ios_spotlight_text_category_summary;
CREATE VIEW vw_ios_spotlight_text_category_summary AS
SELECT human_text_category,review_priority,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT store_guid) AS store_count,
       MIN(original_value_length) AS min_original_value_length,
       MAX(original_value_length) AS max_original_value_length,
       substr(MIN(readable_text_sample),1,1000) AS min_sample_text,
       substr(MAX(readable_text_sample),1,1000) AS max_sample_text,
       'Spotlight recovered text category summary. Categories are triage labels over Spotlight text values, not final CoreSpotlight property names.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values
GROUP BY human_text_category,review_priority;

DROP VIEW IF EXISTS vw_ios_spotlight_record_review;
CREATE VIEW vw_ios_spotlight_record_review AS
WITH text_roll AS (
  SELECT raw_record_id,text_value_count,distinct_text_category_count,human_text_categories,has_high_review_value_text,readable_text_rollup_sample
  FROM vw_ios_spotlight_human_text_rollup
), date_one AS (
  SELECT raw_record_id,
         MAX(spotlight_date_utc) AS spotlight_date_utc,
         MAX(spotlight_date_source_field) AS spotlight_date_source_field,
         MAX(spotlight_date_source_table) AS spotlight_date_source_table,
         MAX(spotlight_date_raw_value) AS spotlight_date_raw_value,
         MAX(spotlight_date_parse_method) AS spotlight_date_parse_method,
         MAX(spotlight_date_type) AS spotlight_date_type,
         MAX(spotlight_date_source_evidence) AS spotlight_date_source_evidence,
         MAX(date_validation_hint) AS date_validation_hint,
         COUNT(*) AS collapsed_date_candidate_count
  FROM vw_ios_spotlight_date_provenance
  GROUP BY raw_record_id
)
SELECT r.raw_record_id,r.source_id,r.store_guid,r.source_db,r.inode_num AS spotlight_inode_or_object_id,r.store_id AS spotlight_store_id,r.parent_inode_num,
       COALESCE(dp.spotlight_date_utc,r.last_updated_utc) AS spotlight_date_utc,
       COALESCE(dp.spotlight_date_source_field,'Last_Updated') AS spotlight_date_source_field,
       COALESCE(dp.spotlight_date_source_table,'raw_records') AS spotlight_date_source_table,
       COALESCE(dp.spotlight_date_raw_value,r.last_updated_raw) AS spotlight_date_raw_value,
       COALESCE(dp.spotlight_date_parse_method,'native_epoch_microseconds') AS spotlight_date_parse_method,
       COALESCE(dp.spotlight_date_type,'metadata_seen_or_index_updated') AS spotlight_date_type,
       COALESCE(dp.spotlight_date_source_evidence,'Last_Updated=' || COALESCE(r.last_updated_raw,'') || ' -> ' || COALESCE(r.last_updated_utc,'')) AS spotlight_date_source_evidence,
       COALESCE(dp.date_validation_hint,'Validate against raw_records.last_updated_raw/last_updated_utc for this Store-V2 record.') AS spotlight_date_validation_hint,
       COALESCE(dp.collapsed_date_candidate_count,0) AS collapsed_date_candidate_count,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       COALESCE(t.text_value_count,0) AS spotlight_text_value_count,
       COALESCE(t.distinct_text_category_count,0) AS spotlight_text_category_count,
       COALESCE(t.human_text_categories,'') AS spotlight_text_categories,
       COALESCE(t.readable_text_rollup_sample,'') AS spotlight_text_rollup_sample,
       0 AS ffs_reference_count,
       0 AS ffs_present_reference_count,
       0 AS ffs_missing_or_unresolved_reference_count,
       0 AS app_db_candidate_count,
       0 AS app_db_present_candidate_count,
       0 AS app_db_unresolved_candidate_count,
       CASE WHEN COALESCE(t.has_high_review_value_text,0)=1 THEN 'HIGH_SPOTLIGHT_TEXT_VALUE'
            WHEN COALESCE(t.text_value_count,0)>0 THEN 'SPOTLIGHT_TEXT_VALUE'
            ELSE 'SPOTLIGHT_RECORD_NO_RECOVERED_TEXT' END AS spotlight_review_priority,
       CASE WHEN COALESCE(t.text_value_count,0)>0 THEN 'TEXT_VALUES_RECOVERED_FROM_SPOTLIGHT'
            ELSE 'NO_TEXT_VALUES_RECOVERED_FOR_RECORD' END AS spotlight_decode_status,
       'Spotlight-first record review. V0_9_21 keeps GUI rows raw_record anchored and avoids broad FFS/app joins; full per-record exports are support/diagnostic-only to prevent long SQL materialization. Use Missing From FFS and object/object-summary views for residency pivots.' AS interpretation_note
FROM raw_records r
LEFT JOIN text_roll t ON t.raw_record_id=r.raw_record_id
LEFT JOIN date_one dp ON dp.raw_record_id=r.raw_record_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_summary;
CREATE VIEW vw_ios_spotlight_object_inode_summary AS
WITH rec AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count,
         COUNT(DISTINCT COALESCE(parent_inode_num,'')) AS distinct_parent_id_count,
         MIN(last_updated_utc) AS earliest_last_updated_utc,
         MAX(last_updated_utc) AS latest_last_updated_utc,
         MIN(raw_record_id) AS first_raw_record_id,
         MAX(raw_record_id) AS last_raw_record_id
  FROM raw_records
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), kv AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_key_value_rows,
         COUNT(DISTINCT field_name) AS distinct_spotlight_field_count,
         substr(MAX(CASE WHEN field_name='__spotlight_investigator_text_context' THEN field_value ELSE '' END),1,1800) AS spotlight_text_context_sample
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
)
)VSQLFIX",
R"VSQLFIX(SELECT rec.source_id,rec.store_guid,rec.source_db,rec.spotlight_inode_or_object_id,rec.spotlight_store_id,
       rec.raw_record_count,rec.distinct_parent_id_count,
       COALESCE(kv.raw_key_value_rows,0) AS raw_key_value_rows,
       COALESCE(kv.distinct_spotlight_field_count,0) AS distinct_spotlight_field_count,
       0 AS date_candidate_rows,
       rec.earliest_last_updated_utc,rec.latest_last_updated_utc,
       '' AS earliest_spotlight_date_utc,
       '' AS latest_spotlight_date_utc,
       rec.first_raw_record_id,rec.last_raw_record_id,
       COALESCE(kv.spotlight_text_context_sample,'') AS spotlight_text_context_sample,
       CASE WHEN rec.raw_record_count>1 THEN 'MULTIPLE_SPOTLIGHT_RECORDS_SHARE_OBJECT_ID'
            WHEN COALESCE(kv.raw_key_value_rows,0)>20 THEN 'SINGLE_RECORD_MANY_FIELDS'
            ELSE 'SINGLE_OR_LOW_EXPANSION_OBJECT' END AS object_materialization_status,
       'Object/inode-centric rollup. V0_9_21 normal exports summarize this view because the full per-object listing is support/diagnostic-only.' AS interpretation_note
FROM rec
LEFT JOIN kv ON kv.source_id=rec.source_id AND kv.store_guid=rec.store_guid AND kv.source_db=rec.source_db AND kv.spotlight_inode_or_object_id=rec.spotlight_inode_or_object_id AND kv.spotlight_store_id=rec.spotlight_store_id;

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_diagnostic_summary;
CREATE VIEW vw_ios_spotlight_object_inode_diagnostic_summary AS
WITH obj AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), buckets AS (
  SELECT source_id,store_guid,source_db,
         CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
              WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
              WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
              ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END AS object_record_bucket,
         COUNT(*) AS object_count,
         SUM(raw_record_count) AS raw_record_count,
         MIN(raw_record_count) AS min_records_per_object,
         MAX(raw_record_count) AS max_records_per_object
  FROM obj
  GROUP BY source_id,store_guid,source_db,
           CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
                WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
                WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
                ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END
)
SELECT source_id,store_guid,source_db,object_record_bucket,object_count,raw_record_count,min_records_per_object,max_records_per_object,
       'Compact object/inode materialization diagnostic. Use this normal export to decide whether the case should pivot to object-centric aggregation; full per-object rows require support/diagnostic export.' AS interpretation_note
FROM buckets;

DROP VIEW IF EXISTS vw_ios_spotlight_object_identity;
CREATE VIEW vw_ios_spotlight_object_identity AS
WITH kv AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         COUNT(*) AS string_probe_count,
         MAX(CASE WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN substr(field_value,1,1500) ELSE '' END) AS sample_url_or_web,
         MAX(CASE WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN substr(field_value,1,1500) ELSE '' END) AS sample_path_or_file_ref,
         MAX(CASE WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN substr(field_value,1,700) ELSE '' END) AS sample_email_or_account,
         MAX(substr(field_value,1,1500)) AS sample_decoded_string
  FROM raw_key_values
  WHERE source_id IN (SELECT source_id FROM evidence_sources)
    AND (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.raw_record_id,r.source_id,r.store_guid,r.source_db,
       CASE
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' OR r.store_guid LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' THEN 'NSFileProtectionCompleteUntilFirstUserAuthentication'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUnlessOpen%' OR r.store_guid LIKE '%NSFileProtectionCompleteUnlessOpen%' THEN 'NSFileProtectionCompleteUnlessOpen'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteWhenUserInactive%' OR r.store_guid LIKE '%NSFileProtectionCompleteWhenUserInactive%' THEN 'NSFileProtectionCompleteWhenUserInactive'
         WHEN r.source_db LIKE '%NSFileProtectionComplete%' OR r.store_guid LIKE '%NSFileProtectionComplete%' THEN 'NSFileProtectionComplete'
         WHEN r.source_db LIKE '%/Priority/%' OR r.store_guid LIKE '%Priority%' THEN 'Priority'
         ELSE 'UnknownOrUnparsedProtectionClass'
       END AS protection_class,
       r.inode_num AS spotlight_inode_or_object_id,
       r.parent_inode_num AS spotlight_parent_id,
       r.store_id AS spotlight_store_id,
       r.file_name,r.display_name,r.full_path,r.content_type,r.content_type_tree,r.record_state,
       r.last_updated_utc,
       COALESCE(k.string_probe_count,0) AS string_probe_count,
       COALESCE(NULLIF(k.sample_url_or_web,''),'') AS sample_url_or_web,
       COALESCE(NULLIF(k.sample_path_or_file_ref,''),'') AS sample_path_or_file_ref,
       COALESCE(NULLIF(k.sample_email_or_account,''),'') AS sample_email_or_account,
       COALESCE(k.sample_decoded_string,'') AS sample_decoded_string,
       CASE WHEN COALESCE(r.full_path,'')<>'' AND r.full_path<>'/' THEN 'PATH_FIELD_PRESENT'
            WHEN COALESCE(NULLIF(k.sample_path_or_file_ref,''),'')<>'' THEN 'STRING_PATH_REFERENCE_PRESENT'
            WHEN COALESCE(k.string_probe_count,0)>0 THEN 'STRING_PROBES_ONLY'
            ELSE 'IDENTIFIERS_ONLY' END AS identity_basis,
       'Use Spotlight IDs/path fragments with FFS inventory and parsed app DB records; Last_Updated remains index/update timing.' AS interpretation_note
FROM raw_records r
)VSQLFIX",
R"VSQLFIX(LEFT JOIN kv k ON k.source_id=r.source_id AND k.store_guid=r.store_guid AND k.source_db=r.source_db AND k.inode_num=r.inode_num AND COALESCE(k.store_id,'')=COALESCE(r.store_id,'')
WHERE r.source_id IN (SELECT source_id FROM evidence_sources)
  AND (r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%');

DROP VIEW IF EXISTS vw_ios_spotlight_to_ffs_object_links;
CREATE VIEW vw_ios_spotlight_to_ffs_object_links AS
WITH refs AS (
  SELECT * FROM vw_ios_spotlight_referenced_paths WHERE COALESCE(normalized_ios_path,'')<>''
), files AS (
  SELECT source_id,normalized_path,file_name,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,'full_inventory' AS lookup_source
  FROM ios_ffs_file_inventory
  UNION ALL
  SELECT l.source_id,l.normalized_path,l.file_name,l.size_bytes,l.zip_modified_utc,l.protection_class_hint,l.app_container_hint,l.domain_hint,COALESCE(NULLIF(l.lookup_source,''),'slim_path_lookup') AS lookup_source
  FROM ios_ffs_path_lookup l
  WHERE NOT EXISTS (SELECT 1 FROM ios_ffs_file_inventory f WHERE f.source_id=l.source_id LIMIT 1)
)
SELECT r.reference_id,r.source_id,r.store_guid,r.source_db,r.inode_num AS spotlight_inode_or_object_id,
       r.store_id AS spotlight_store_id,r.parent_inode_num AS spotlight_parent_id,
       r.field_name,r.reference_type,r.normalized_ios_path,
       CASE WHEN f.normalized_path IS NOT NULL THEN 'PRESENT_AS_FILE_IN_FFS'
            ELSE 'SPOTLIGHT_ONLY_FILE_MISSING_OR_UNRESOLVED' END AS residency_status,
       CASE WHEN f.normalized_path IS NOT NULL THEN 'HIGH_PATH_MATCH'
            ELSE 'MEDIUM_PATH_ABSENT_FROM_ZIP_INVENTORY' END AS confidence,
       COALESCE(f.file_name,'') AS matched_file_name,
       COALESCE(f.size_bytes,0) AS matched_size_bytes,
       COALESCE(f.zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.protection_class_hint,'') AS matched_protection_class,
       COALESCE(f.app_container_hint,'') AS matched_app_container,
       COALESCE(f.domain_hint,'') AS matched_domain,
       CASE WHEN f.normalized_path IS NOT NULL
            THEN 'Exact normalized Spotlight path matched an enumerated FFS ZIP path. This supports current-file presence, not usage.'
            ELSE 'Path was absent from enumerated FFS ZIP path inventory. This is a lead only and does not by itself prove user deletion.' END AS interpretation_note
FROM refs r
LEFT JOIN files f ON f.source_id=r.source_id AND f.normalized_path=r.normalized_ios_path;

DROP VIEW IF EXISTS vw_ios_spotlight_to_app_db_record_links;
CREATE VIEW vw_ios_spotlight_to_app_db_record_links AS
WITH parsed AS (
  SELECT source_id,
         CASE
           WHEN database_category='APPLE_MESSAGES' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
           WHEN database_category='CALL_HISTORY' THEN 'CALL_OR_FACETIME_RELATED'
           WHEN database_category='WHATSAPP' THEN 'WHATSAPP_RELATED'
           WHEN database_category='SIGNAL' THEN 'SIGNAL_RELATED'
           WHEN database_category='TELEGRAM' THEN 'TELEGRAM_RELATED'
           WHEN database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT') THEN 'WEB_OR_BROWSER_RELATED'
           WHEN database_category='MAIL' THEN 'MAIL_OR_ACCOUNT_RELATED'
           WHEN database_category='CONTACTS' THEN 'CONTACT_OR_ADDRESS_BOOK_RELATED'
           WHEN database_category='CALENDAR' THEN 'CALENDAR_OR_INVITATION_RELATED'
           ELSE '' END AS object_category,
         COUNT(*) AS parsed_record_count,
         MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,
         MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
         MIN(database_normalized_path) AS sample_database_path,
         MIN(table_name) AS sample_table_name,
         MIN(record_category) AS sample_record_category,
         MIN(substr(COALESCE(NULLIF(url,''),NULLIF(title,''),NULLIF(text_snippet,''),NULLIF(file_path,''),''),1,1200)) AS sample_parsed_value
  FROM ios_app_parsed_records
  WHERE source_id IN (SELECT source_id FROM evidence_sources)
  GROUP BY source_id,object_category
)
SELECT c.candidate_id,c.source_id,c.store_guid,c.source_db,c.inode_num AS spotlight_inode_or_object_id,c.store_id AS spotlight_store_id,c.parent_inode_num AS spotlight_parent_id,
       c.field_name,c.object_category,c.database_residency_status,c.database_name,c.database_category,c.app_hint,c.candidate_database_path,
       c.record_inventory_status,c.matched_record_category,c.matched_table_name,c.matched_table_row_count,
       COALESCE(p.parsed_record_count,0) AS parsed_record_count,
       COALESCE(p.earliest_record_timestamp_utc,'') AS earliest_record_timestamp_utc,
       COALESCE(p.latest_record_timestamp_utc,'') AS latest_record_timestamp_utc,
       COALESCE(p.sample_database_path,'') AS sample_database_path,
       COALESCE(p.sample_table_name,'') AS sample_table_name,
       COALESCE(p.sample_record_category,'') AS sample_record_category,
       COALESCE(p.sample_parsed_value,'') AS sample_parsed_value,
       c.string_value_sample,
       CASE WHEN COALESCE(p.parsed_record_count,0)>0 THEN 'POTENTIAL_APP_DB_RECORD_FAMILY_PRESENT'
            WHEN c.database_residency_status LIKE 'POTENTIAL_RECORD_TABLE%' THEN 'APP_DB_TABLE_PRESENT_RECORD_LEVEL_MATCH_NOT_PROVEN'
            ELSE c.database_residency_status END AS app_db_link_status,
       'Family-level correlation only. Exact Spotlight string-to-app database row matching remains a later phase.' AS interpretation_note
FROM vw_ios_database_residency_candidates c
LEFT JOIN parsed p ON p.source_id=c.source_id AND p.object_category=c.object_category;

DROP VIEW IF EXISTS vw_ios_spotlight_residency_summary;
CREATE VIEW vw_ios_spotlight_residency_summary AS
SELECT residency_status,confidence,COUNT(*) AS reference_count,
       COUNT(DISTINCT spotlight_inode_or_object_id) AS distinct_record_count,
       MIN(normalized_ios_path) AS first_path_sample,MAX(normalized_ios_path) AS last_path_sample
FROM vw_ios_spotlight_to_ffs_object_links
GROUP BY residency_status,confidence
UNION ALL
SELECT database_residency_status AS residency_status,'DATABASE_FAMILY_HEURISTIC' AS confidence,COUNT(*) AS reference_count,
       COUNT(DISTINCT inode_num) AS distinct_record_count,MIN(candidate_database_path) AS first_path_sample,MAX(candidate_database_path) AS last_path_sample
FROM vw_ios_database_residency_candidates
GROUP BY database_residency_status;

DROP VIEW IF EXISTS vw_ios_protection_class_summary;
CREATE VIEW vw_ios_protection_class_summary AS
)VSQLFIX",
R"VSQLFIX(WITH ios_records AS (
  SELECT raw_record_id,source_id,store_guid,source_db,store_path,inode_num,store_id,last_updated_utc,
         CASE
           WHEN source_db LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' OR store_guid LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' THEN 'NSFileProtectionCompleteUntilFirstUserAuthentication'
           WHEN source_db LIKE '%NSFileProtectionCompleteUnlessOpen%' OR store_guid LIKE '%NSFileProtectionCompleteUnlessOpen%' THEN 'NSFileProtectionCompleteUnlessOpen'
           WHEN source_db LIKE '%NSFileProtectionCompleteWhenUserInactive%' OR store_guid LIKE '%NSFileProtectionCompleteWhenUserInactive%' THEN 'NSFileProtectionCompleteWhenUserInactive'
           WHEN source_db LIKE '%NSFileProtectionComplete%' OR store_guid LIKE '%NSFileProtectionComplete%' THEN 'NSFileProtectionComplete'
           WHEN source_db LIKE '%/Priority/%' OR store_guid LIKE '%Priority%' THEN 'Priority'
           ELSE 'UnknownOrUnparsedProtectionClass'
         END AS protection_class
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
), kv AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,COUNT(*) AS string_probe_rows
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.protection_class,
       COUNT(*) AS raw_record_count,
       SUM(CASE WHEN COALESCE(k.string_probe_rows,0)>0 THEN 1 ELSE 0 END) AS records_with_string_probes,
       COALESCE(SUM(k.string_probe_rows),0) AS string_probe_rows,
       COUNT(DISTINCT r.source_db) AS selected_database_count,
       MIN(NULLIF(r.last_updated_utc,'')) AS earliest_last_updated_utc,
       MAX(NULLIF(r.last_updated_utc,'')) AS latest_last_updated_utc
FROM ios_records r
LEFT JOIN kv k ON k.source_id=r.source_id AND k.store_guid=r.store_guid AND k.source_db=r.source_db AND k.inode_num=r.inode_num AND k.store_id=r.store_id
GROUP BY r.protection_class;

DROP VIEW IF EXISTS vw_ios_artifact_hint_summary;
)VSQLFIX"
}));
    exec(R"SQL(CREATE VIEW vw_ios_artifact_hint_summary AS
WITH probe AS (
  SELECT store_guid,source_db,inode_num,store_id,LOWER(field_value) AS v,field_value
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), categorized AS (
  SELECT store_guid,source_db,inode_num,store_id,field_value,
         CASE
           WHEN v LIKE '%/mail/attachmentdata/%' THEN 'MAIL_ATTACHMENT_PATH'
           WHEN v LIKE '%com.apple.clouddocs.iclouddrivefileprovider%' OR v LIKE '%icloud drive%' OR v LIKE '%/mobile documents/%' THEN 'ICLOUD_DRIVE_OR_CLOUDDOCS'
           WHEN v LIKE '%drive.google.com%' THEN 'GOOGLE_DRIVE_LINK'
           WHEN v LIKE '%docs.google.com%' THEN 'GOOGLE_DOCS_LINK'
           WHEN v LIKE '%teams.microsoft.com%' THEN 'MICROSOFT_TEAMS_LINK'
           WHEN v LIKE '%onedrive%' THEN 'ONEDRIVE_LINK_OR_TEXT'
           WHEN v LIKE '%zoom.us/%' THEN 'ZOOM_LINK'
           WHEN v LIKE '%maps.app.goo.gl%' OR v LIKE '%maps.google.%' THEN 'MAP_LINK'
           WHEN v LIKE '%invite.ics%' OR v LIKE '%calendar%' OR v LIKE '%rsvp%' THEN 'CALENDAR_INVITATION'
           WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'WEB_URL_OR_HTML_LINK'
           WHEN v LIKE 'file:%' OR v LIKE '/private/var/%' OR v LIKE '%/mobile/%' THEN 'IOS_FILE_PATH'
           WHEN v LIKE '%@%' AND v LIKE '%.%' THEN 'EMAIL_OR_ACCOUNT_TEXT'
           WHEN v LIKE '%imessage%' OR v LIKE '%sms%' OR v LIKE '%message%' THEN 'MESSAGE_OR_MESSAGE_ATTACHMENT_TEXT'
           ELSE 'OTHER_STRING_PROBE'
         END AS artifact_hint
  FROM probe
)
SELECT artifact_hint,
       COUNT(*) AS string_probe_rows,
       COUNT(DISTINCT store_guid) AS store_count,
       COUNT(DISTINCT inode_num) AS distinct_record_count,
       COUNT(DISTINCT field_value) AS distinct_value_count,
       substr(MIN(field_value),1,250) AS min_sample_value,
       substr(MAX(field_value),1,250) AS max_sample_value
FROM categorized
GROUP BY artifact_hint
ORDER BY string_probe_rows DESC, artifact_hint;

DROP VIEW IF EXISTS vw_ios_record_investigation_hints;
)SQL");
    exec(R"SQL(CREATE VIEW vw_ios_record_investigation_hints AS
WITH kv AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         CASE
           WHEN LOWER(field_value) LIKE '%/mail/attachmentdata/%' THEN 'MAIL_ATTACHMENT_PATH'
           WHEN LOWER(field_value) LIKE '%com.apple.clouddocs.iclouddrivefileprovider%' OR LOWER(field_value) LIKE '%icloud drive%' OR LOWER(field_value) LIKE '%/mobile documents/%' THEN 'ICLOUD_DRIVE_OR_CLOUDDOCS'
           WHEN LOWER(field_value) LIKE '%drive.google.com%' THEN 'GOOGLE_DRIVE_LINK'
           WHEN LOWER(field_value) LIKE '%docs.google.com%' THEN 'GOOGLE_DOCS_LINK'
           WHEN LOWER(field_value) LIKE '%teams.microsoft.com%' THEN 'MICROSOFT_TEAMS_LINK'
           WHEN LOWER(field_value) LIKE '%onedrive%' THEN 'ONEDRIVE_LINK_OR_TEXT'
           WHEN LOWER(field_value) LIKE '%zoom.us/%' THEN 'ZOOM_LINK'
           WHEN LOWER(field_value) LIKE '%maps.app.goo.gl%' OR LOWER(field_value) LIKE '%maps.google.%' THEN 'MAP_LINK'
           WHEN LOWER(field_value) LIKE '%invite.ics%' OR LOWER(field_value) LIKE '%calendar%' OR LOWER(field_value) LIKE '%rsvp%' THEN 'CALENDAR_INVITATION'
           WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN 'WEB_URL_OR_HTML_LINK'
           WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN 'IOS_FILE_PATH'
           WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN 'EMAIL_OR_ACCOUNT_TEXT'
           WHEN LOWER(field_value) LIKE '%imessage%' OR LOWER(field_value) LIKE '%sms%' OR LOWER(field_value) LIKE '%message%' THEN 'MESSAGE_OR_MESSAGE_ATTACHMENT_TEXT'
           ELSE 'OTHER_STRING_PROBE'
         END AS artifact_hint
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), agg AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         COUNT(*) AS string_probe_rows,
         GROUP_CONCAT(DISTINCT artifact_hint) AS artifact_hints,
         MAX(CASE WHEN artifact_hint='MAIL_ATTACHMENT_PATH' THEN 1 ELSE 0 END) AS has_mail_attachment,
         MAX(CASE WHEN artifact_hint='ICLOUD_DRIVE_OR_CLOUDDOCS' THEN 1 ELSE 0 END) AS has_icloud_docs,
         MAX(CASE WHEN artifact_hint IN ('GOOGLE_DRIVE_LINK','GOOGLE_DOCS_LINK') THEN 1 ELSE 0 END) AS has_google_workspace,
         MAX(CASE WHEN artifact_hint IN ('MICROSOFT_TEAMS_LINK','ONEDRIVE_LINK_OR_TEXT') THEN 1 ELSE 0 END) AS has_microsoft_cloud,
         MAX(CASE WHEN artifact_hint='CALENDAR_INVITATION' THEN 1 ELSE 0 END) AS has_calendar_invite,
         MAX(CASE WHEN artifact_hint='EMAIL_OR_ACCOUNT_TEXT' THEN 1 ELSE 0 END) AS has_email_text,
)SQL" R"SQL(         MAX(CASE WHEN artifact_hint='WEB_URL_OR_HTML_LINK' THEN 1 ELSE 0 END) AS has_web_url
  FROM kv
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.raw_record_id,
       r.source_id,
       r.store_guid,
       CASE
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' OR r.store_guid LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' THEN 'NSFileProtectionCompleteUntilFirstUserAuthentication'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUnlessOpen%' OR r.store_guid LIKE '%NSFileProtectionCompleteUnlessOpen%' THEN 'NSFileProtectionCompleteUnlessOpen'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteWhenUserInactive%' OR r.store_guid LIKE '%NSFileProtectionCompleteWhenUserInactive%' THEN 'NSFileProtectionCompleteWhenUserInactive'
         WHEN r.source_db LIKE '%NSFileProtectionComplete%' OR r.store_guid LIKE '%NSFileProtectionComplete%' THEN 'NSFileProtectionComplete'
         WHEN r.source_db LIKE '%/Priority/%' OR r.store_guid LIKE '%Priority%' THEN 'Priority'
         ELSE 'UnknownOrUnparsedProtectionClass'
       END AS protection_class,
       CASE
         WHEN a.has_mail_attachment=1 THEN 'Mail attachment / Mail AttachmentData path'
         WHEN a.has_icloud_docs=1 THEN 'iCloud Drive or CloudDocs provider content'
         WHEN a.has_google_workspace=1 THEN 'Google Docs or Google Drive link/content'
         WHEN a.has_microsoft_cloud=1 THEN 'Microsoft Teams or OneDrive link/content'
         WHEN a.has_calendar_invite=1 THEN 'Calendar invitation or ICS-like content'
         WHEN a.has_email_text=1 THEN 'Email address/account-like text'
         WHEN a.has_web_url=1 THEN 'Web URL or HTML link content'
         ELSE 'Other decoded string probe'
       END AS primary_investigation_hint,
       a.artifact_hints,
       a.string_probe_rows,
       r.source_db,
       r.inode_num,
       r.store_id,
       r.parent_inode_num,
       r.file_name,
       r.content_type,
       r.display_name,
       r.full_path,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       r.record_state
FROM raw_records r
JOIN agg a ON a.source_id=r.source_id AND a.store_guid=r.store_guid AND a.source_db=r.source_db AND a.inode_num=r.inode_num AND a.store_id=r.store_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';
)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_date_field_attribution;
CREATE VIEW vw_date_field_attribution AS
WITH common_date_counts AS (
  SELECT substr(parsed_utc,1,10) AS date_only, COUNT(*) AS row_count
  FROM raw_date_candidates
  WHERE COALESCE(parsed_utc,'')<>''
  GROUP BY substr(parsed_utc,1,10)
  HAVING COUNT(*) >= 1000
), top_common_dates AS (
  SELECT date_only FROM common_date_counts ORDER BY row_count DESC, date_only DESC LIMIT 5
), attributed AS (
  SELECT d.raw_date_id,
         d.source_id,
         d.store_guid,
         d.source_db,
         d.inode_num,
         d.store_id,
         COALESCE(d.artifact_id, a.artifact_id) AS artifact_id,
         COALESCE(NULLIF(d.parent_inode_num,''), a.parent_inode_num) AS parent_inode_num,
         COALESCE(NULLIF(d.file_name,''), a.file_name) AS file_name,
         a.display_name,
         COALESCE(NULLIF(d.best_path,''), a.best_path) AS best_path,
         a.path_source,
         a.path_status,
         a.logical_size_bytes,
         a.physical_size_bytes,
         a.content_type,
         a.where_froms,
         d.field_name AS raw_spotlight_field,
         d.field_value AS raw_spotlight_value,
         d.parsed_utc,
         d.parse_method,
         substr(d.parsed_utc,1,10) AS parsed_date_only,
         COALESCE(cdc.row_count,0) AS common_date_row_count,
         CASE
           WHEN lower(d.field_name) LIKE '%lastuseddate%' THEN 'opened_last'
           WHEN lower(d.field_name) LIKE '%useddates%' THEN 'used_date'
           WHEN lower(d.field_name) LIKE '%recent%spotlight%engagement%' THEN 'spotlight_engagement'
           WHEN lower(d.field_name) LIKE '%download%' THEN 'downloaded'
           WHEN lower(d.field_name) LIKE '%contentcreation%' OR lower(d.field_name) LIKE '%creationdate%' THEN 'created'
           WHEN lower(d.field_name) LIKE '%contentmodification%' OR lower(d.field_name) LIKE '%contentchange%' OR lower(d.field_name) LIKE '%modificationdate%' THEN 'modified'
           WHEN lower(d.field_name) LIKE '%interestingdate%' THEN 'interesting_or_index_date'
           WHEN lower(d.field_name) LIKE '%gps%' THEN 'gps_date'
           WHEN lower(d.field_name) LIKE '%startdate%' THEN 'start_date'
           WHEN lower(d.field_name) LIKE '%enddate%' THEN 'end_date'
           WHEN lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 'metadata_seen_or_index_updated'
           ELSE 'other_date'
         END AS event_type_interpretation,
         CASE
           WHEN lower(d.field_name) LIKE '%lastuseddate%' OR lower(d.field_name) LIKE '%useddates%' THEN 'usage/open evidence from Spotlight usage field'
           WHEN lower(d.field_name) LIKE '%download%' THEN 'download/origin date field'
)SQL" R"SQL(           WHEN lower(d.field_name) LIKE '%contentcreation%' OR lower(d.field_name) LIKE '%creationdate%' THEN 'file/content creation date candidate'
           WHEN lower(d.field_name) LIKE '%contentmodification%' OR lower(d.field_name) LIKE '%contentchange%' OR lower(d.field_name) LIKE '%modificationdate%' THEN 'file/content modification date candidate'
           WHEN lower(d.field_name) LIKE '%interestingdate%' THEN 'Spotlight interesting/index date; review carefully'
           WHEN lower(d.field_name) LIKE '%ranking%' THEN 'Spotlight ranking date; not direct user activity by itself'
           WHEN lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 'Spotlight record/index update time; not direct user activity by itself'
           ELSE 'date candidate from decoded Spotlight metadata'
         END AS interpretation_note,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'ARTIFACT_MATCHED_BY_SOURCE_STORE_INODE'
           ELSE 'NO_ARTIFACT_MATCH_BY_SOURCE_STORE_INODE'
         END AS association_status,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND COALESCE(NULLIF(COALESCE(d.best_path,a.best_path),''),'')<>'' AND COALESCE(NULLIF(COALESCE(d.file_name,a.file_name),''),'')<>'' THEN 'HIGH_OBJECT_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND (COALESCE(NULLIF(COALESCE(d.best_path,a.best_path),''),'')<>'' OR COALESCE(NULLIF(COALESCE(d.file_name,a.file_name),''),'')<>'') THEN 'MEDIUM_OBJECT_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'LOW_OBJECT_CONTEXT'
           ELSE 'NONE'
         END AS association_confidence,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'source_id + store_guid + inode_num'
           ELSE ''
         END AS association_method,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND COALESCE(NULLIF(COALESCE(d.best_path,a.best_path),''),'')<>'' THEN 'HAS_ARTIFACT_AND_PATH_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND COALESCE(NULLIF(COALESCE(d.file_name,a.file_name),''),'')<>'' THEN 'HAS_ARTIFACT_AND_NAME_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'HAS_ARTIFACT_ONLY'
           ELSE 'DATE_ONLY_NO_ARTIFACT_CONTEXT'
         END AS object_context_status,
         CASE
           WHEN lower(d.field_name) LIKE '%interestingdate%' OR lower(d.field_name) LIKE '%ranking%' OR lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 1
)SQL" R"SQL(           WHEN substr(d.parsed_utc,1,10) IN (SELECT date_only FROM top_common_dates) AND lower(d.field_name) NOT LIKE '%creation%' AND lower(d.field_name) NOT LIKE '%modification%' AND lower(d.field_name) NOT LIKE '%used%' AND lower(d.field_name) NOT LIKE '%download%' THEN 1
           ELSE 0
         END AS likely_snapshot_or_index_date,
         CASE
           WHEN lower(d.field_name) LIKE '%interestingdate%' THEN 'FIELD_IS_SPOTLIGHT_INTERESTING_DATE'
           WHEN lower(d.field_name) LIKE '%ranking%' THEN 'FIELD_IS_SPOTLIGHT_RANKING_DATE'
           WHEN lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 'FIELD_IS_RECORD_LAST_UPDATED_OR_INDEX_DATE'
           WHEN substr(d.parsed_utc,1,10) IN (SELECT date_only FROM top_common_dates) THEN 'DATE_IS_COMMON_ACROSS_INDEX_SAMPLE'
           ELSE ''
         END AS snapshot_warning_reason
  FROM raw_date_candidates d
  LEFT JOIN artifacts a ON a.source_id=d.source_id AND a.store_guid=d.store_guid AND a.inode_num=d.inode_num
  LEFT JOIN common_date_counts cdc ON cdc.date_only=substr(d.parsed_utc,1,10)
  WHERE COALESCE(d.parsed_utc,'')<>''
)
SELECT *,
       CASE
         WHEN likely_snapshot_or_index_date=1 AND common_date_row_count>0 THEN snapshot_warning_reason || '; common date row count=' || common_date_row_count
         ELSE snapshot_warning_reason
       END AS snapshot_warning_detail
FROM attributed;

)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_usage_event_detail_attributed;
CREATE VIEW vw_usage_event_detail_attributed AS
SELECT ROW_NUMBER() OVER (ORDER BY d.parsed_utc, d.raw_date_id) AS usage_event_id,
       d.parsed_utc AS event_utc,
       d.event_type_interpretation AS date_type,
       d.raw_spotlight_field AS source_field,
       d.raw_spotlight_value AS raw_value,
       d.interpretation_note AS usage_reason,
       d.likely_snapshot_or_index_date,
       d.snapshot_warning_reason,
       d.snapshot_warning_detail,
       d.association_status,
       d.association_confidence,
       d.object_context_status,
       d.artifact_id,
       d.source_id,
       d.store_guid,
       d.inode_num,
       d.parent_inode_num,
       d.file_name,
       d.display_name,
       d.best_path,
       d.path_source,
       d.path_status,
       d.logical_size_bytes,
       d.physical_size_bytes,
       d.content_type,
       d.where_froms
FROM vw_date_field_attribution d
WHERE d.event_type_interpretation IN ('opened_last','used_date','spotlight_engagement')
   OR lower(d.raw_spotlight_field) LIKE '%used%'
   OR lower(d.raw_spotlight_field) LIKE '%open%';

)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_artifact_dates_wide;
CREATE VIEW vw_artifact_dates_wide AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,logical_size_bytes,physical_size_bytes,content_type,where_froms,
       created_earliest_utc,created_latest_utc,modified_earliest_utc,modified_latest_utc,downloaded_earliest_utc,downloaded_latest_utc,usage_earliest_utc,usage_latest_utc,interesting_or_index_earliest_utc,interesting_or_index_latest_utc,
       likely_snapshot_date_count,associated_date_count,unassociated_date_count,available_date_fields,association_confidence_summary,snapshot_warning_reasons
FROM artifact_date_summary;

)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_snapshot_date_warnings;
CREATE VIEW vw_snapshot_date_warnings AS
SELECT raw_date_id,artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,logical_size_bytes,physical_size_bytes,content_type,raw_spotlight_field,parsed_utc,event_type_interpretation,interpretation_note,association_status,association_confidence,object_context_status,common_date_row_count,snapshot_warning_reason,snapshot_warning_detail
FROM vw_date_field_attribution
WHERE likely_snapshot_or_index_date=1;

)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_object_date_summary;
CREATE VIEW vw_object_date_summary AS
SELECT artifact_id,
       source_id,
       store_guid,
       inode_num,
       parent_inode_num,
       file_name,
       display_name,
       best_path,
       path_source,
       path_status,
       logical_size_bytes,
       physical_size_bytes,
       content_type,
       first_date_utc,
       last_date_utc,
       total_date_count,
       created_date_count,
       modified_date_count,
       downloaded_date_count,
       usage_date_count,
       interesting_or_index_date_count,
       metadata_seen_or_index_updated_count,
       other_date_count,
       likely_snapshot_or_index_date_count,
       available_date_fields,
       interpreted_date_types,
       snapshot_warning_reasons,
       date_association_status,
       date_association_confidence
FROM artifact_date_summary;

)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_object_usage_summary;
CREATE VIEW vw_object_usage_summary AS
WITH usage_artifacts AS (
  SELECT artifact_id
  FROM artifacts
  WHERE artifact_id IS NOT NULL
    AND (
         COALESCE(used_dates_count,0)>0
      OR COALESCE(NULLIF(last_used_date_utc,''),'')<>''
      OR COALESCE(NULLIF(first_used_candidate_utc,''),'')<>''
      OR COALESCE(NULLIF(use_count_value,''),'')<>''
      OR COALESCE(open_count_estimate,0)>0
    )
  UNION
  SELECT DISTINCT artifact_id
  FROM usage_evidence
  WHERE artifact_id IS NOT NULL
  UNION
  SELECT DISTINCT artifact_id
  FROM raw_date_candidates
  WHERE artifact_id IS NOT NULL
    AND COALESCE(parsed_utc,'')<>''
    AND date_type IN ('opened_last','used_date','spotlight_engagement')
), usage_dates AS (
  SELECT d.artifact_id,
         MIN(d.parsed_utc) AS usage_earliest_utc,
         MAX(d.parsed_utc) AS usage_latest_utc,
         COUNT(*) AS usage_date_row_count,
         GROUP_CONCAT(DISTINCT d.field_name) AS usage_date_source_fields,
         GROUP_CONCAT(d.parsed_utc, '; ') AS fused_usage_dates_utc,
         SUM(CASE WHEN d.date_type IN ('interesting_or_index_date','metadata_seen_or_index_updated') THEN 1 ELSE 0 END) AS likely_snapshot_or_index_usage_date_count,
         GROUP_CONCAT(DISTINCT CASE WHEN d.date_type IN ('interesting_or_index_date','metadata_seen_or_index_updated') THEN d.field_name ELSE NULL END) AS snapshot_warning_reasons
  FROM raw_date_candidates d
  JOIN usage_artifacts ua ON ua.artifact_id=d.artifact_id
  WHERE d.artifact_id IS NOT NULL
    AND COALESCE(d.parsed_utc,'')<>''
    AND d.date_type IN ('opened_last','used_date','spotlight_engagement')
  GROUP BY d.artifact_id
), usage_rows AS (
  SELECT artifact_id,
         COUNT(*) AS usage_evidence_row_count,
         GROUP_CONCAT(DISTINCT field_name) AS usage_evidence_fields,
         GROUP_CONCAT(field_name || '=' || COALESCE(NULLIF(parsed_utc,''), field_value), '; ') AS usage_evidence_values
  FROM usage_evidence
  WHERE artifact_id IS NOT NULL
  GROUP BY artifact_id
), date_wide AS (
  SELECT d.artifact_id,
         MIN(CASE WHEN d.date_type='created' THEN d.parsed_utc END) AS created_earliest_utc,
         MAX(CASE WHEN d.date_type='created' THEN d.parsed_utc END) AS created_latest_utc,
         MIN(CASE WHEN d.date_type='modified' THEN d.parsed_utc END) AS modified_earliest_utc,
         MAX(CASE WHEN d.date_type='modified' THEN d.parsed_utc END) AS modified_latest_utc,
         MIN(CASE WHEN d.date_type='downloaded' THEN d.parsed_utc END) AS downloaded_earliest_utc,
         MAX(CASE WHEN d.date_type='downloaded' THEN d.parsed_utc END) AS downloaded_latest_utc,
         MIN(CASE WHEN d.date_type='interesting_or_index_date' THEN d.parsed_utc END) AS interesting_or_index_earliest_utc,
)SQL" R"SQL(         MAX(CASE WHEN d.date_type='interesting_or_index_date' THEN d.parsed_utc END) AS interesting_or_index_latest_utc,
         SUM(CASE WHEN d.date_type IN ('interesting_or_index_date','metadata_seen_or_index_updated') THEN 1 ELSE 0 END) AS likely_snapshot_date_count,
         GROUP_CONCAT(DISTINCT d.field_name) AS available_date_fields
  FROM raw_date_candidates d
  JOIN usage_artifacts ua ON ua.artifact_id=d.artifact_id
  WHERE d.artifact_id IS NOT NULL
    AND COALESCE(d.parsed_utc,'')<>''
  GROUP BY d.artifact_id
)
SELECT a.artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.spotlight_display_path,
       a.normalized_mac_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.authors,
       a.creator,
       a.existence_status,
       a.deleted_or_orphaned_candidate,
       a.confidence,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.used_dates_utc,
       a.use_count_value,
       a.open_count_estimate,
       COALESCE(ud.usage_earliest_utc, NULLIF(a.first_used_candidate_utc,'')) AS usage_earliest_utc,
       COALESCE(ud.usage_latest_utc, NULLIF(a.last_used_date_utc,''), NULLIF(a.first_used_candidate_utc,'')) AS usage_latest_utc,
       COALESCE(ud.fused_usage_dates_utc, NULLIF(a.used_dates_utc,''), NULLIF(a.last_used_date_utc,''), NULLIF(a.first_used_candidate_utc,'')) AS fused_usage_dates_utc,
       COALESCE(ud.usage_date_row_count, 0) AS usage_date_row_count,
       COALESCE(ur.usage_evidence_row_count, 0) AS usage_evidence_row_count,
       COALESCE(NULLIF(ud.usage_date_source_fields,''), NULLIF(ur.usage_evidence_fields,''), '') AS usage_source_fields,
       COALESCE(NULLIF(a.usage_field_summary,''), NULLIF(ur.usage_evidence_values,''), '') AS usage_field_summary,
       COALESCE(NULLIF(ur.usage_evidence_values,''), NULLIF(a.usage_field_summary,''), '') AS usage_supporting_values,
       COALESCE(ud.likely_snapshot_or_index_usage_date_count, 0) AS likely_snapshot_or_index_usage_date_count,
       COALESCE(ud.snapshot_warning_reasons, '') AS snapshot_warning_reasons,
       dw.created_earliest_utc,
       dw.created_latest_utc,
       dw.modified_earliest_utc,
       dw.modified_latest_utc,
       dw.downloaded_earliest_utc,
       dw.downloaded_latest_utc,
       dw.interesting_or_index_earliest_utc,
       dw.interesting_or_index_latest_utc,
       COALESCE(dw.likely_snapshot_date_count,0) AS likely_snapshot_date_count,
       COALESCE(dw.available_date_fields,'') AS available_date_fields,
       CASE
)SQL" R"SQL(         WHEN COALESCE(ud.usage_date_row_count,0)>0 AND COALESCE(ur.usage_evidence_row_count,0)>0 THEN 'DATE_ATTRIBUTION_AND_USAGE_EVIDENCE'
         WHEN COALESCE(ud.usage_date_row_count,0)>0 THEN 'DATE_ATTRIBUTION_USAGE_FIELDS'
         WHEN COALESCE(ur.usage_evidence_row_count,0)>0 THEN 'USAGE_EVIDENCE_ROWS'
         WHEN COALESCE(a.used_dates_count,0)>0 OR COALESCE(NULLIF(a.last_used_date_utc,''),'')<>'' OR COALESCE(NULLIF(a.first_used_candidate_utc,''),'')<>'' THEN 'ARTIFACT_USAGE_COLUMNS'
         WHEN COALESCE(NULLIF(a.use_count_value,''),'')<>'' OR COALESCE(a.open_count_estimate,0)>0 THEN 'USAGE_COUNT_ONLY'
         ELSE 'NO_USAGE_SIGNAL'
       END AS object_usage_basis
FROM usage_artifacts ua
JOIN artifacts a ON a.artifact_id=ua.artifact_id
LEFT JOIN usage_dates ud ON ud.artifact_id=a.artifact_id
LEFT JOIN usage_rows ur ON ur.artifact_id=a.artifact_id
LEFT JOIN date_wide dw ON dw.artifact_id=a.artifact_id;
)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_usage_timeline_attributed;
CREATE VIEW vw_usage_timeline_attributed AS
SELECT ROW_NUMBER() OVER (ORDER BY COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,''), artifact_id) DESC, artifact_id DESC) AS timeline_id,
       COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,'')) AS event_utc,
       'usage_summary' AS date_type,
       usage_source_fields AS source_field,
       object_usage_basis AS usage_reason,
       CASE WHEN COALESCE(likely_snapshot_or_index_usage_date_count,0)>0 THEN 1 ELSE 0 END AS likely_snapshot_or_index_date,
       snapshot_warning_reasons AS snapshot_warning_reason,
       artifact_id,
       source_id,
       store_guid,
       inode_num,
       parent_inode_num,
       file_name,
       display_name,
       best_path,
       path_source,
       path_status,
       content_type,
       logical_size_bytes,
       physical_size_bytes,
       use_count_value,
       open_count_estimate,
       used_dates_count,
       usage_earliest_utc,
       usage_latest_utc,
       fused_usage_dates_utc,
       usage_date_row_count,
       usage_evidence_row_count,
       where_froms,
       confidence
FROM vw_object_usage_summary;

)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_ios_apple_messages_database_status;
CREATE VIEW vw_ios_apple_messages_database_status AS
WITH sms_db AS (
  SELECT ios_db_id,source_id,normalized_path,database_name,database_category,app_hint,parse_status,record_inventory_status,extracted_path
  FROM ios_app_database_inventory
  WHERE database_category='APPLE_MESSAGES'
    AND lower(database_name) IN ('sms.db','chat.db')
    AND lower(normalized_path) NOT LIKE '%-wal'
    AND lower(normalized_path) NOT LIKE '%-shm'
), ri AS (
  SELECT ios_db_id,
         SUM(CASE WHEN table_name='message' THEN COALESCE(row_count,0) ELSE 0 END) AS message_rows,
         SUM(CASE WHEN table_name='chat' THEN COALESCE(row_count,0) ELSE 0 END) AS chat_rows,
         SUM(CASE WHEN table_name='attachment' THEN COALESCE(row_count,0) ELSE 0 END) AS attachment_rows,
         SUM(CASE WHEN table_name='handle' THEN COALESCE(row_count,0) ELSE 0 END) AS handle_rows,
         SUM(CASE WHEN table_name IN ('chat_message_join','message_attachment_join','chat_handle_join') THEN COALESCE(row_count,0) ELSE 0 END) AS join_rows,
         GROUP_CONCAT(CASE WHEN record_category IN ('MESSAGE_RECORDS','MESSAGE_ATTACHMENTS','MESSAGE_PARTICIPANTS') THEN table_name || ':' || COALESCE(row_count,0) END) AS relevant_table_counts
  FROM ios_app_database_record_inventory
  GROUP BY ios_db_id
), pr AS (
  SELECT ios_db_id,COUNT(*) AS parsed_message_rows
  FROM ios_app_parsed_records
  WHERE database_category='APPLE_MESSAGES'
  GROUP BY ios_db_id
)
SELECT d.ios_db_id,d.source_id,d.normalized_path,d.database_name,d.app_hint,d.parse_status,d.record_inventory_status,d.extracted_path,
       COALESCE(ri.message_rows,0) AS message_rows,
       COALESCE(ri.chat_rows,0) AS chat_rows,
       COALESCE(ri.attachment_rows,0) AS attachment_rows,
       COALESCE(ri.handle_rows,0) AS handle_rows,
       COALESCE(ri.join_rows,0) AS join_rows,
       COALESCE(pr.parsed_message_rows,0) AS parsed_message_rows,
       COALESCE(ri.relevant_table_counts,'') AS relevant_table_counts,
       CASE
         WHEN COALESCE(pr.parsed_message_rows,0)>0 THEN 'SMS_DB_PRESENT_WITH_PARSED_ROWS'
         WHEN COALESCE(ri.message_rows,0)>0 OR COALESCE(ri.attachment_rows,0)>0 THEN 'SMS_DB_PRESENT_RELEVANT_ROWS_NOT_PARSED'
         WHEN d.ios_db_id IS NOT NULL THEN 'SMS_DB_PRESENT_NO_LIVE_MESSAGE_CHAT_ATTACHMENT_ROWS'
         ELSE 'SMS_DB_NOT_FOUND'
       END AS apple_messages_residency_status,
       'Spotlight may contain message-like text even when the live SMS.db message/chat/attachment tables are empty; treat as Spotlight-only candidate unless a matching app database row is parsed.' AS interpretation_note
FROM sms_db d
LEFT JOIN ri ON ri.ios_db_id=d.ios_db_id
LEFT JOIN pr ON pr.ios_db_id=d.ios_db_id;
)SQL");

    exec(joinSql({
R"VSQLFIX(
DROP VIEW IF EXISTS vw_ios_whatsapp_database_status;
CREATE VIEW vw_ios_whatsapp_database_status AS
WITH src AS (
  SELECT source_id FROM evidence_sources ORDER BY added_utc LIMIT 1
), wa_db AS (
  SELECT ios_db_id,source_id,normalized_path,database_name,database_category,app_hint,parse_status,record_inventory_status,extracted_path
  FROM ios_app_database_inventory
  WHERE database_category='WHATSAPP'
    AND lower(normalized_path) NOT LIKE '%-wal'
    AND lower(normalized_path) NOT LIKE '%-shm'
), ri AS (
  SELECT ios_db_id,
         SUM(CASE WHEN lower(table_name)='zwamessage' THEN COALESCE(row_count,0) ELSE 0 END) AS message_rows,
         SUM(CASE WHEN lower(table_name)='zwamediaitem' THEN COALESCE(row_count,0) ELSE 0 END) AS media_rows,
         SUM(CASE WHEN lower(table_name)='zwachatsession' THEN COALESCE(row_count,0) ELSE 0 END) AS chat_rows,
         SUM(CASE WHEN lower(table_name) IN ('zwaaddressbookcontact','zwaprofilepushname','zwagroupmember','zwavcardmention') THEN COALESCE(row_count,0) ELSE 0 END) AS contact_or_member_rows,
         SUM(CASE WHEN lower(table_name) LIKE '%call%' THEN COALESCE(row_count,0) ELSE 0 END) AS call_rows,
         GROUP_CONCAT(CASE WHEN record_category IN ('MESSAGE_RECORDS','MESSAGE_ATTACHMENTS','MESSAGE_PARTICIPANTS','CHAT_RECORDS','CALL_RECORDS') THEN table_name || ':' || COALESCE(row_count,0) END) AS relevant_table_counts
  FROM ios_app_database_record_inventory
  GROUP BY ios_db_id
), pr AS (
  SELECT ios_db_id,COUNT(*) AS parsed_whatsapp_rows
  FROM ios_app_parsed_records
  WHERE database_category='WHATSAPP'
  GROUP BY ios_db_id
), status_rows AS (
  SELECT d.ios_db_id,d.source_id,d.normalized_path,d.database_name,d.app_hint,d.parse_status,d.record_inventory_status,d.extracted_path,
         COALESCE(ri.message_rows,0) AS message_rows,
         COALESCE(ri.media_rows,0) AS media_rows,
         COALESCE(ri.chat_rows,0) AS chat_rows,
         COALESCE(ri.contact_or_member_rows,0) AS contact_or_member_rows,
         COALESCE(ri.call_rows,0) AS call_rows,
         COALESCE(pr.parsed_whatsapp_rows,0) AS parsed_whatsapp_rows,
         COALESCE(ri.relevant_table_counts,'') AS relevant_table_counts,
         CASE
           WHEN COALESCE(pr.parsed_whatsapp_rows,0)>0 THEN 'WHATSAPP_DB_PRESENT_WITH_PARSED_ROWS'
           WHEN COALESCE(ri.message_rows,0)>0 OR COALESCE(ri.media_rows,0)>0 OR COALESCE(ri.chat_rows,0)>0 OR COALESCE(ri.call_rows,0)>0 THEN 'WHATSAPP_DB_PRESENT_RELEVANT_ROWS_NOT_PARSED'
           ELSE 'WHATSAPP_DB_PRESENT_NO_RELEVANT_LIVE_ROWS'
         END AS whatsapp_residency_status,
         'WhatsApp rows are parsed from staged iOS app SQLite databases using schema patterns from the uploaded local iLEAPP and WhatsApp references; encrypted or absent databases remain inventory-only.' AS interpretation_note
  FROM wa_db d
  LEFT JOIN ri ON ri.ios_db_id=d.ios_db_id
  LEFT JOIN pr ON pr.ios_db_id=d.ios_db_id
)
SELECT * FROM status_rows
UNION ALL
SELECT NULL AS ios_db_id, COALESCE((SELECT source_id FROM src),'') AS source_id, '' AS normalized_path,
       'ChatStorage.sqlite / ContactsV2.sqlite / CallHistory.sqlite' AS database_name,
       'WhatsApp' AS app_hint, '' AS parse_status, '' AS record_inventory_status, '' AS extracted_path,
       0 AS message_rows, 0 AS media_rows, 0 AS chat_rows, 0 AS contact_or_member_rows, 0 AS call_rows, 0 AS parsed_whatsapp_rows,
       '' AS relevant_table_counts, 'WHATSAPP_DB_NOT_FOUND' AS whatsapp_residency_status,
       'No iOS WhatsApp ChatStorage/Contacts/CallHistory database was identified in the current FFS inventory; Spotlight WhatsApp-like strings, if any, remain Spotlight-only candidates unless another app database or file path supports them.' AS interpretation_note
WHERE NOT EXISTS (SELECT 1 FROM wa_db);

DROP VIEW IF EXISTS vw_ios_keychain_material_inventory;
CREATE VIEW vw_ios_keychain_material_inventory AS
SELECT source_id, normalized_path, original_zip_entry, file_name, extension, size_bytes, zip_modified_utc,
       protection_class_hint, app_container_hint, domain_hint, sha256_status, inventory_notes,
       CASE
         WHEN lower(file_name) IN ('keychain-2.db','keychain-2-debug.db') THEN 'KEYCHAIN_DATABASE'
         WHEN lower(normalized_path) LIKE '%/private/var/keychains/%' THEN 'KEYCHAIN_DIRECTORY_ARTIFACT'
         WHEN lower(file_name) LIKE '%_keychain.plist' OR lower(file_name)='keychain_decrypted.plist' THEN 'EXTERNAL_OR_DECRYPTED_KEYCHAIN_PLIST'
         WHEN lower(normalized_path) LIKE '%keybag%' THEN 'KEYBAG_OR_KEYCHAIN_SUPPORT'
         ELSE 'KEYCHAIN_CORE_MATERIAL'
       END AS keychain_material_type,
       'Inventory only. Presence of keychain/keybag material in the FFS root does not mean WhatsApp or other app data can be decrypted unless parser-specific key extraction and validation are implemented.' AS interpretation_note
FROM ios_ffs_file_inventory
WHERE lower(normalized_path) LIKE '%/private/var/keychains/%'
   OR lower(file_name) IN ('keychain-2.db','keychain-2-debug.db')
   OR lower(normalized_path) LIKE '%keybag%'
   OR lower(file_name) LIKE '%_keychain.plist' OR lower(file_name)='keychain_decrypted.plist'
ORDER BY keychain_material_type, normalized_path;

DROP VIEW IF EXISTS vw_ios_keychain_support_reference_inventory;
CREATE VIEW vw_ios_keychain_support_reference_inventory AS
SELECT source_id, normalized_path, original_zip_entry, file_name, extension, size_bytes, zip_modified_utc,
       protection_class_hint, app_container_hint, domain_hint, sha256_status, inventory_notes,
       'KEYCHAIN_LIBRARY_OR_CODE_REFERENCE' AS keychain_reference_type,
       'Lower-priority reference: path contains keychain text outside the core /private/var/Keychains or keybag locations. Usually framework/code support, not keychain material.' AS interpretation_note
FROM ios_ffs_file_inventory
WHERE lower(normalized_path) LIKE '%keychain%'
  AND lower(normalized_path) NOT LIKE '%/private/var/keychains/%'
  AND lower(file_name) NOT IN ('keychain-2.db','keychain-2-debug.db','keychain_decrypted.plist')
  AND lower(file_name) NOT LIKE '%_keychain.plist'
  AND lower(normalized_path) NOT LIKE '%keybag%'
ORDER BY normalized_path;

DROP VIEW IF EXISTS vw_ios_protected_data_decryption_candidates;
CREATE VIEW vw_ios_protected_data_decryption_candidates AS
WITH keychain_presence AS (
  SELECT source_id, COUNT(*) AS keychain_material_count,
         GROUP_CONCAT(DISTINCT keychain_material_type) AS keychain_material_types
  FROM vw_ios_keychain_material_inventory
  GROUP BY source_id
)
SELECT d.source_id,
       d.ios_db_id,
       d.normalized_path,
       d.database_name,
       d.database_category,
       d.app_hint,
       d.protection_class_hint,
       d.parse_status,
       d.record_inventory_status,
       COALESCE(k.keychain_material_count,0) AS keychain_material_count,
       COALESCE(k.keychain_material_types,'') AS keychain_material_types,
       CASE
         WHEN COALESCE(k.keychain_material_count,0)>0 AND (lower(d.protection_class_hint) LIKE '%complete%' OR lower(d.notes) LIKE '%encrypted%' OR lower(d.parse_status) LIKE '%encrypted%' OR lower(d.parse_status) LIKE '%failed%') THEN 'KEYCHAIN_MATERIAL_PRESENT_AND_DATABASE_MAY_BE_PROTECTED'
         WHEN COALESCE(k.keychain_material_count,0)>0 THEN 'KEYCHAIN_MATERIAL_PRESENT_FOR_CASE'
         ELSE 'NO_KEYCHAIN_MATERIAL_IN_INVENTORY'
       END AS candidate_status,
       'Candidate correlation only. This does not decrypt data or prove that a keychain item unlocks this database. Use for prioritizing future validated decryption workflows.' AS interpretation_note
FROM ios_app_database_inventory d
LEFT JOIN keychain_presence k ON k.source_id=d.source_id
WHERE d.database_category IN ('APPLE_MESSAGES','WHATSAPP','SIGNAL','TELEGRAM','SAFARI_WEB','CHROME_WEB','WEBKIT','MAIL','CALENDAR','CONTACTS','KNOWLEDGEC_COREDUET','KEYCHAIN')
   OR lower(d.normalized_path) LIKE '%nsfileprotection%'
   OR lower(d.parse_status) LIKE '%encrypted%'
   OR lower(d.notes) LIKE '%encrypted%'
ORDER BY candidate_status DESC, d.database_category, d.normalized_path;

DROP VIEW IF EXISTS vw_ios_communications_review_records;
CREATE VIEW vw_ios_communications_review_records AS
SELECT ios_app_record_id, source_id, ios_db_id,
       CASE
         WHEN database_category='APPLE_MESSAGES' THEN 'Apple Messages / SMS.db'
         WHEN database_category='WHATSAPP' THEN 'WhatsApp'
         WHEN database_category='CALL_HISTORY' THEN 'Phone / FaceTime Call History'
         WHEN lower(database_category) LIKE '%mail%' OR lower(app_hint) LIKE '%mail%' THEN 'Mail'
         ELSE COALESCE(NULLIF(app_hint,''), database_category)
       END AS communication_source,
)VSQLFIX",
R"VSQLFIX(       database_normalized_path, database_name, database_category, app_hint, table_name, record_category, source_primary_key,
       record_timestamp_utc, timestamp_source, contact_or_participant, url, title, file_path, item_identifier, text_snippet, parse_status, provenance,
       CASE
         WHEN lower(record_category) LIKE '%call%' THEN 'CALL'
         WHEN lower(record_category) LIKE '%attachment%' OR COALESCE(file_path,'')<>'' THEN 'ATTACHMENT_OR_MEDIA'
         WHEN lower(record_category) LIKE '%participant%' OR lower(record_category) LIKE '%contact%' THEN 'PARTICIPANT_OR_CONTACT'
         WHEN lower(record_category) LIKE '%chat%' THEN 'CHAT_OR_THREAD'
         WHEN lower(record_category) LIKE '%message%' THEN 'MESSAGE'
         ELSE 'COMMUNICATION_RELATED_RECORD'
       END AS communication_record_type,
       CASE WHEN COALESCE(record_timestamp_utc,'')<>'' THEN 'APP_DB_TIMESTAMP' ELSE 'APP_DB_RECORD_NO_NORMALIZED_TIMESTAMP' END AS timeline_basis,
       'Parsed local app database record. Treat as live/acquired app data only for the staged database listed; correlate with Spotlight separately before drawing deletion or residency conclusions.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category IN ('APPLE_MESSAGES','WHATSAPP','CALL_HISTORY')
   OR lower(record_category) LIKE '%message%'
   OR lower(record_category) LIKE '%call%'
   OR lower(record_category) LIKE '%chat%'
   OR lower(record_category) LIKE '%participant%'
   OR lower(table_name) LIKE '%message%'
   OR lower(table_name) LIKE '%chat%'
   OR lower(table_name) LIKE '%call%';

DROP VIEW IF EXISTS vw_ios_communications_review_summary;
CREATE VIEW vw_ios_communications_review_summary AS
SELECT communication_source, database_category, app_hint, record_category, communication_record_type, parse_status,
       COUNT(*) AS parsed_record_count, COUNT(DISTINCT ios_db_id) AS database_count,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc, MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc,
       SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS records_with_contact_or_participant,
       SUM(CASE WHEN COALESCE(text_snippet,'')<>'' THEN 1 ELSE 0 END) AS records_with_text,
       SUM(CASE WHEN COALESCE(url,'')<>'' THEN 1 ELSE 0 END) AS records_with_url,
       SUM(CASE WHEN COALESCE(file_path,'')<>'' THEN 1 ELSE 0 END) AS records_with_file_path,
       MIN(database_normalized_path) AS first_database_path, MAX(database_normalized_path) AS last_database_path
FROM vw_ios_communications_review_records
GROUP BY communication_source, database_category, app_hint, record_category, communication_record_type, parse_status
ORDER BY communication_source, record_category, communication_record_type;

DROP VIEW IF EXISTS vw_ios_communication_frequency;
CREATE VIEW vw_ios_communication_frequency AS
SELECT
    COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key) AS communication_thread_id,
    COUNT(*) AS total_records_in_thread,
    MIN(record_timestamp_utc) AS first_communication_utc,
    MAX(record_timestamp_utc) AS last_communication_utc,
    GROUP_CONCAT(DISTINCT contact_or_participant) AS involved_identities,
    GROUP_CONCAT(DISTINCT app_hint) AS apps_utilized,
    GROUP_CONCAT(DISTINCT record_category) AS record_categories,
    GROUP_CONCAT(DISTINCT parse_status) AS parse_statuses,
    'Thread/contact grouped iOS communication frequency view. Thread identifiers use item_identifier when available, then contact/source primary key fallback. Treat as committed parsed-record frequency, not a final communication assertion without source-row review.' AS interpretation_note
FROM ios_app_parsed_records
WHERE (record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_EVENTS')
       OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
       OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
       OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%')
  AND COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key) IS NOT NULL
GROUP BY COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key)
ORDER BY total_records_in_thread DESC, last_communication_utc DESC;

DROP VIEW IF EXISTS vw_ios_communication_existence_evidence;
CREATE VIEW vw_ios_communication_existence_evidence AS
SELECT
  ios_app_record_id,
  source_id,
  database_category,
  app_hint,
  database_name,
  table_name,
  record_category,
  source_primary_key,
  record_timestamp_utc,
  timestamp_source,
  COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key) AS communication_thread_id,
  contact_or_participant AS identity_hint,
  url,
  title,
  file_path,
  substr(text_snippet,1,500) AS text_snippet_sample,
  parse_status,
  provenance,
  CASE
    WHEN record_category='MESSAGE_DELETED_OR_RECOVERABLE' THEN 'deleted_or_recoverable_message_table_or_spotlight_marker'
    WHEN provenance LIKE '%COMMUNICATION_INTENT_STREAM%' OR provenance LIKE '%INTENT_TARGET%' THEN 'knowledgec_or_intent_communication_marker'
    WHEN provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%' THEN 'domain_identifier_or_thread_marker'
    WHEN provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%' THEN 'author_recipient_or_identity_marker'
    WHEN record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS') THEN 'parsed_communication_app_database_record'
    ELSE 'communication_related_parsed_record'
  END AS existence_basis,
  'Presence/frequency support view. Rows show committed parsed records and provenance markers that support existence or activity frequency; review source row before making final conclusions.' AS interpretation_note
FROM ios_app_parsed_records
WHERE record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_EVENTS')
   OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
   OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
   OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%'
   OR provenance LIKE '%INTENT_TARGET%';

DROP VIEW IF EXISTS vw_ios_communication_identity_frequency;
CREATE VIEW vw_ios_communication_identity_frequency AS
SELECT
  COALESCE(NULLIF(contact_or_participant,''), NULLIF(item_identifier,''), '(no explicit identity)') AS identity_or_thread_hint,
  database_category,
  app_hint,
  record_category,
  COUNT(*) AS related_record_count,
  COUNT(DISTINCT COALESCE(NULLIF(item_identifier,''), source_primary_key)) AS distinct_thread_or_record_keys,
  MIN(NULLIF(record_timestamp_utc,'')) AS first_seen_utc,
  MAX(NULLIF(record_timestamp_utc,'')) AS last_seen_utc,
  GROUP_CONCAT(DISTINCT table_name) AS source_tables,
  GROUP_CONCAT(DISTINCT parse_status) AS parse_statuses,
  'Identity/frequency rollup from parsed app records and communication provenance markers.' AS interpretation_note
FROM ios_app_parsed_records
WHERE record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_EVENTS')
   OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
   OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
   OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%'
GROUP BY COALESCE(NULLIF(contact_or_participant,''), NULLIF(item_identifier,''), '(no explicit identity)'), database_category, app_hint, record_category
ORDER BY related_record_count DESC, last_seen_utc DESC;

DROP VIEW IF EXISTS vw_ios_communication_temporal_frequency;
CREATE VIEW vw_ios_communication_temporal_frequency AS
SELECT
  substr(record_timestamp_utc,1,10) AS communication_date_utc,
  COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), '(no explicit thread)') AS communication_thread_or_identity,
  database_category,
  app_hint,
  record_category,
  COUNT(*) AS records_on_date,
  COUNT(DISTINCT contact_or_participant) AS distinct_identity_hints,
  MIN(record_timestamp_utc) AS first_record_utc,
  MAX(record_timestamp_utc) AS last_record_utc,
  GROUP_CONCAT(DISTINCT parse_status) AS parse_statuses
FROM ios_app_parsed_records
WHERE COALESCE(record_timestamp_utc,'')<>''
  AND (record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_EVENTS')
       OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
       OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
       OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%')
GROUP BY substr(record_timestamp_utc,1,10), COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), '(no explicit thread)'), database_category, app_hint, record_category
ORDER BY communication_date_utc DESC, records_on_date DESC;

DROP VIEW IF EXISTS vw_ios_communication_source_coverage;
CREATE VIEW vw_ios_communication_source_coverage AS
SELECT
  database_category,
  app_hint,
  database_name,
  table_name,
  record_category,
  parse_status,
  COUNT(*) AS parsed_record_count,
  SUM(CASE WHEN COALESCE(record_timestamp_utc,'')<>'' THEN 1 ELSE 0 END) AS records_with_timestamp,
  SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS records_with_identity_hint,
  SUM(CASE WHEN COALESCE(item_identifier,'')<>'' THEN 1 ELSE 0 END) AS records_with_thread_or_item_id,
  MIN(NULLIF(record_timestamp_utc,'')) AS first_seen_utc,
  MAX(NULLIF(record_timestamp_utc,'')) AS last_seen_utc,
  'Communication existence/frequency source coverage by database/table/category.' AS interpretation_note
FROM ios_app_parsed_records
WHERE record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_EVENTS')
   OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
   OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
   OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%'
GROUP BY database_category, app_hint, database_name, table_name, record_category, parse_status
ORDER BY parsed_record_count DESC, records_with_timestamp DESC;


DROP VIEW IF EXISTS vw_ios_url_frequency;
CREATE VIEW vw_ios_url_frequency AS
SELECT
  lower(COALESCE(NULLIF(url,''), NULLIF(file_path,''))) AS url_or_reference,
  COUNT(*) AS related_record_count,
  MIN(NULLIF(record_timestamp_utc,'')) AS first_seen_utc,
  MAX(NULLIF(record_timestamp_utc,'')) AS last_seen_utc,
  GROUP_CONCAT(DISTINCT database_category) AS database_categories,
  GROUP_CONCAT(DISTINCT app_hint) AS apps,
  GROUP_CONCAT(DISTINCT record_category) AS record_categories,
  GROUP_CONCAT(DISTINCT table_name) AS source_tables,
  'URL/reference frequency from parsed iOS app DB records; review source rows before final conclusions.' AS interpretation_note
FROM ios_app_parsed_records
WHERE COALESCE(NULLIF(url,''), NULLIF(file_path,'')) IS NOT NULL
  AND (lower(COALESCE(url,'')) LIKE 'http%' OR lower(COALESCE(file_path,'')) LIKE 'http%' OR record_category IN ('WEB_HISTORY','WEB_VISITS','WEB_DOWNLOADS'))
GROUP BY lower(COALESCE(NULLIF(url,''), NULLIF(file_path,'')))
ORDER BY related_record_count DESC, last_seen_utc DESC;

DROP VIEW IF EXISTS vw_ios_attachment_reference_frequency;
CREATE VIEW vw_ios_attachment_reference_frequency AS
SELECT
  COALESCE(NULLIF(file_path,''), NULLIF(title,''), NULLIF(item_identifier,''), source_primary_key) AS attachment_or_file_reference,
  COUNT(*) AS related_record_count,
  MIN(NULLIF(record_timestamp_utc,'')) AS first_seen_utc,
  MAX(NULLIF(record_timestamp_utc,'')) AS last_seen_utc,
  GROUP_CONCAT(DISTINCT database_category) AS database_categories,
  GROUP_CONCAT(DISTINCT app_hint) AS apps,
  GROUP_CONCAT(DISTINCT record_category) AS record_categories,
  GROUP_CONCAT(DISTINCT table_name) AS source_tables,
  'Attachment/file reference frequency from parsed iOS app DB records; review source rows before final conclusions.' AS interpretation_note
FROM ios_app_parsed_records
WHERE COALESCE(NULLIF(file_path,''), NULLIF(title,''), NULLIF(item_identifier,''), source_primary_key) IS NOT NULL
  AND (record_category IN ('MESSAGE_ATTACHMENTS','MAIL_RECORDS','WEB_DOWNLOADS','NOTES_RECORDS')
       OR lower(COALESCE(file_path,'')) LIKE '%attach%'
       OR lower(COALESCE(file_path,'')) LIKE '%.pdf%'
       OR lower(COALESCE(file_path,'')) LIKE '%.doc%'
       OR lower(COALESCE(file_path,'')) LIKE '%.xls%'
       OR lower(COALESCE(file_path,'')) LIKE '%.jpg%'
       OR lower(COALESCE(file_path,'')) LIKE '%.png%')
GROUP BY COALESCE(NULLIF(file_path,''), NULLIF(title,''), NULLIF(item_identifier,''), source_primary_key)
ORDER BY related_record_count DESC, last_seen_utc DESC;

DROP VIEW IF EXISTS vw_ios_spotlight_communication_candidates;
CREATE VIEW vw_ios_spotlight_communication_candidates AS
WITH probe AS (
  SELECT kv.raw_kv_id, kv.source_id, kv.store_guid, kv.source_db, kv.inode_num, kv.store_id, kv.parent_inode_num, kv.field_name, kv.field_value,
         lower(kv.field_value) AS v, rr.last_updated_utc
  FROM raw_key_values kv
  LEFT JOIN raw_records rr ON rr.source_id=kv.source_id AND rr.store_guid=kv.store_guid AND rr.source_db=kv.source_db AND rr.inode_num=kv.inode_num AND COALESCE(rr.store_id,'')=COALESCE(kv.store_id,'')
  WHERE COALESCE(kv.field_value,'')<>''
    AND (kv.store_guid LIKE 'ios_%' OR kv.source_db LIKE '%CoreSpotlight%' OR kv.store_path LIKE '%CoreSpotlight%')
), categorized AS (
  SELECT *,
         CASE
           WHEN v LIKE '%whatsapp%' THEN 'WHATSAPP_TEXT_OR_REFERENCE'
           WHEN v LIKE '%imessage%' OR v LIKE '%sms.db%' OR v LIKE '%/sms/%' OR v LIKE '%com.apple.mobilesms%' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
           WHEN v LIKE '%message%' OR v LIKE '%chat%' OR v LIKE '%conversation%' THEN 'MESSAGE_OR_CHAT_TEXT_CANDIDATE'
           WHEN v LIKE '%facetime%' OR v LIKE '%callhistory%' OR v LIKE '%call history%' THEN 'PHONE_OR_FACETIME_RELATED'
           WHEN v LIKE '%mailto:%' OR (v LIKE '%@%' AND v LIKE '%.%') THEN 'EMAIL_OR_ACCOUNT_RELATED'
           ELSE 'OTHER_COMMUNICATION_RELATED'
         END AS communication_candidate_type
  FROM probe
  WHERE v LIKE '%whatsapp%' OR v LIKE '%imessage%' OR v LIKE '%sms.db%' OR v LIKE '%/sms/%' OR v LIKE '%com.apple.mobilesms%'
     OR v LIKE '%message%' OR v LIKE '%chat%' OR v LIKE '%conversation%' OR v LIKE '%facetime%' OR v LIKE '%callhistory%' OR v LIKE '%call history%'
     OR v LIKE '%mailto:%' OR (v LIKE '%@%' AND v LIKE '%.%')
)
SELECT raw_kv_id, source_id, store_guid, source_db, inode_num AS spotlight_inode_or_object_id, store_id AS spotlight_store_id, parent_inode_num AS spotlight_parent_id,
       field_name, communication_candidate_type, last_updated_utc AS spotlight_last_updated_utc, substr(field_value,1,600) AS string_value_sample,
       CASE
         WHEN communication_candidate_type='WHATSAPP_TEXT_OR_REFERENCE' AND EXISTS (SELECT 1 FROM vw_ios_whatsapp_database_status WHERE whatsapp_residency_status<>'WHATSAPP_DB_NOT_FOUND') THEN 'WHATSAPP_DATABASE_FAMILY_PRESENT'
         WHEN communication_candidate_type='APPLE_MESSAGES_OR_SMS_RELATED' AND EXISTS (SELECT 1 FROM vw_ios_apple_messages_database_status WHERE apple_messages_residency_status<>'SMS_DB_NOT_FOUND') THEN 'SMS_DATABASE_FAMILY_PRESENT'
         WHEN communication_candidate_type='PHONE_OR_FACETIME_RELATED' AND EXISTS (SELECT 1 FROM ios_app_database_inventory WHERE database_category='CALL_HISTORY') THEN 'CALL_HISTORY_DATABASE_FAMILY_PRESENT'
         ELSE 'NO_CONFIRMED_MATCHING_APP_DATABASE_ROW'
       END AS communication_residency_context,
       'Spotlight string candidate only. Review parsed app database views and exact links before treating this as a live message/call/chat record.' AS interpretation_note
FROM categorized
ORDER BY communication_candidate_type, raw_kv_id;

)VSQLFIX"
}));

    exec(R"SQL(
DROP VIEW IF EXISTS vw_ios_app_live_activity_timeline;
CREATE VIEW vw_ios_app_live_activity_timeline AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,
       'app_database_record_time' AS timeline_basis,
       'Parsed local app database row; stronger than Spotlight Last_Updated but still requires schema-specific interpretation.' AS interpretation_note
FROM ios_app_parsed_records
WHERE COALESCE(record_timestamp_utc,'')<>''
ORDER BY record_timestamp_utc DESC, database_category, record_category, ios_app_record_id;
)SQL");

    exec(joinSql({
R"VSQLFIX(
DROP VIEW IF EXISTS vw_ios_spotlight_human_text_values;
CREATE VIEW vw_ios_spotlight_human_text_values AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,store_path,inode_num,store_id,parent_inode_num,field_name,field_value,
         LOWER(COALESCE(field_value,'')) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), labeled AS (
  SELECT *,
    CASE
      WHEN v LIKE '%file://%' OR v LIKE '%/private/var/mobile/%' OR v LIKE '%/var/mobile/%' THEN 'FILE_PATH_OR_ATTACHMENT'
      WHEN v LIKE '%zoom.%' OR v LIKE '%meet.google.%' OR v LIKE '%teams.microsoft.%' OR v LIKE '%webex.%' THEN 'MEETING_OR_CONFERENCE'
      WHEN v LIKE '%.ics%' OR v LIKE '%text/calendar%' OR v LIKE '%vevent%' OR v LIKE '%calendar.google.com%' OR v LIKE '%/calendar/%' THEN 'CALENDAR_OR_INVITATION'
      WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'WEB_OR_URL'
      WHEN v LIKE 'from:%' OR v LIKE 'to:%' OR v LIKE 'cc:%' OR v LIKE '%mailto:%' OR (v LIKE '%@%' AND v NOT LIKE '%http://%' AND v NOT LIKE '%https://%') THEN 'EMAIL_OR_ACCOUNT_TEXT'
      WHEN v LIKE '%net.whatsapp.whatsapp%' OR v LIKE '%chat.whatsapp.com%' OR v LIKE '%wa.me/%' OR v LIKE '%api.whatsapp.com%' OR v LIKE '%whatsapp group%' THEN 'WHATSAPP_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.whispersystems.signal%' OR v LIKE '%signal.messenger%' OR v LIKE '%signal.org/%' THEN 'SIGNAL_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.telegram%' OR v LIKE '%telegram.messenger%' OR v LIKE '%t.me/%' OR v LIKE '%telegram.me/%' THEN 'TELEGRAM_TEXT_OR_REFERENCE'
      ELSE 'OTHER_HUMAN_READABLE_TEXT'
    END AS human_text_category,
    TRIM(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(COALESCE(field_value,''),
      char(13),' '), char(10),' '), char(9),' '), '<br>',' '), '<br/>',' '), '<br />',' '),
      '&amp;','&'), '&lt;','<'), '&gt;','>'), '&nbsp;',' '), '&quot;','"')) AS readable_text
  FROM probes
)
SELECT l.raw_kv_id,COALESCE(r.raw_record_id,0) AS raw_record_id,l.source_id,l.store_guid,l.source_db,l.inode_num,l.store_id,l.parent_inode_num,l.field_name,
       l.human_text_category,
       LENGTH(COALESCE(l.field_value,'')) AS original_value_length,
       substr(l.readable_text,1,3000) AS readable_text_sample,
       CASE
         WHEN l.human_text_category IN ('FILE_PATH_OR_ATTACHMENT','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION','WEB_OR_URL','EMAIL_OR_ACCOUNT_TEXT') THEN 'HIGH_HUMAN_REVIEW_VALUE'
         WHEN LENGTH(l.readable_text)>=24 THEN 'MEDIUM_HUMAN_REVIEW_VALUE'
         ELSE 'LOW_HUMAN_REVIEW_VALUE'
       END AS review_priority,
       'Generic iOS CoreSpotlight text recovery; formal CoreSpotlight property names/dbStr maps remain a later parser phase.' AS interpretation_note
FROM labeled l
LEFT JOIN (
  SELECT source_id,store_guid,source_db,inode_num,COALESCE(store_id,'') AS store_id_key,MIN(raw_record_id) AS raw_record_id
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,inode_num,COALESCE(store_id,'')
) r ON r.source_id=l.source_id AND r.store_guid=l.store_guid AND r.source_db=l.source_db AND r.inode_num=l.inode_num AND r.store_id_key=COALESCE(l.store_id,'')
WHERE LENGTH(l.readable_text)>=4
  AND lower(COALESCE(l.field_name,'')) NOT LIKE '%ranking%'
  AND lower(COALESCE(l.field_name,'')) NOT LIKE 'kmdstore%'
  AND lower(COALESCE(l.field_name,'')) NOT LIKE '_kstore%'
  AND lower(COALESCE(l.field_name,'')) NOT IN (
    '_kmditemserialnumber','_kmditemstoragebytes','_kmditemstoragesize','kmditemphysicalsize',
    '_kmditemcontentindexversion','_kmditemtextcontentindexexists','_kmditemgroupid',
    'kmdstoreuuid','kmdstoreaccumulatedsizes','_kmdclientcheckedin'
  )
  AND (
    l.human_text_category IN ('FILE_PATH_OR_ATTACHMENT','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION','WEB_OR_URL','EMAIL_OR_ACCOUNT_TEXT','WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE')
    OR lower(COALESCE(l.field_name,'')) IN (
      '_kmditemexternalid','_kmditembundleid','_kmditemdomainidentifier','_kmditemauthoridentifier',
      'kmditemtitle','kmditemdisplayname','kmditemdescription','kmditemheadline','kmditemcomment','kmditemkeywords',
      'kmditemidentifier','kmditemurl','kmditemcontenturl','kmditempath','kmditemfsname','kmditemwherefroms',
      'kmditemauthors','kmditemauthoraddresses','kmditemrecipients','kmditemrecipientaddresses',
      'kmditemcontaineridentifier','kmditemcontainertitle','kmditemcontainerdisplayname',
      'com_apple_mail_subject','com_apple_mail_sender','com_apple_mail_to','com_apple_mail_cc','com_apple_mail_messageid',
      'com_apple_mobilesms_handle','com_apple_mobilesms_guid','com_apple_mobilesms_chatidentifier'
    )
    OR lower(COALESCE(l.field_name,'')) LIKE '%title%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%subject%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%url%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%path%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%identifier%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%sender%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%recipient%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%author%'
    OR lower(COALESCE(l.field_name,'')) LIKE '%account%'
  );

DROP VIEW IF EXISTS vw_ios_spotlight_human_text_rollup;
CREATE VIEW vw_ios_spotlight_human_text_rollup AS
WITH text_values AS (
  SELECT v.*, r.last_updated_utc, r.record_state
  FROM vw_ios_spotlight_human_text_values v
  LEFT JOIN raw_records r ON r.raw_record_id=v.raw_record_id
)
SELECT raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT human_text_category) AS distinct_text_category_count,
       GROUP_CONCAT(DISTINCT human_text_category) AS human_text_categories,
       MAX(CASE WHEN review_priority='HIGH_HUMAN_REVIEW_VALUE' THEN 1 ELSE 0 END) AS has_high_review_value_text,
       MIN(NULLIF(last_updated_utc,'')) AS last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       substr(GROUP_CONCAT(field_name || '=' || readable_text_sample, ' || '),1,6000) AS readable_text_rollup_sample,
       'Record-level human-readable text rollup from iOS CoreSpotlight string probes.' AS interpretation_note
FROM text_values
GROUP BY raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num;


DROP VIEW IF EXISTS vw_ios_spotlight_investigative_items_with_dates;
CREATE VIEW vw_ios_spotlight_investigative_items_with_dates AS
SELECT v.raw_kv_id,
)VSQLFIX",
R"VSQLFIX(       v.raw_record_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.original_value_length,
       v.readable_text_sample,
       v.review_priority,
       dp.spotlight_date_utc,
       dp.spotlight_date_source_field,
       dp.spotlight_date_source_table,
       dp.spotlight_date_raw_value,
       dp.spotlight_date_parse_method,
       dp.spotlight_date_type,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%creation%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%created%' THEN 'created_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modification%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modified%' THEN 'modified_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%access%' THEN 'accessed_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%open%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'metadata_seen_or_index_updated'
         ELSE 'unclassified_spotlight_date_candidate'
       END AS spotlight_date_semantic_class,
       dp.spotlight_date_source_evidence,
       dp.date_validation_hint,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'Do not report as created/modified/accessed/opened. This is CoreSpotlight metadata/index update timing unless another decoded field supports activity semantics.'
         WHEN COALESCE(dp.spotlight_date_source_field,'')<>'' THEN 'Report only with the listed raw Spotlight source field, raw value, parse method, and validation hint.'
         ELSE 'No direct Spotlight date was recovered for this text value.'
       END AS date_reporting_caution,
       'Spotlight/CoreSpotlight extracted text item with attached date provenance where directly linkable by raw_record_id. FFS/app database data is supporting context only.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
LEFT JOIN vw_ios_spotlight_date_provenance dp ON dp.raw_record_id=v.raw_record_id;

DROP VIEW IF EXISTS vw_ios_spotlight_date_field_summary;
CREATE VIEW vw_ios_spotlight_date_field_summary AS
WITH dates AS (
  SELECT source_id, store_guid, source_db, field_name, parse_method, date_type,
         parsed_utc, field_value, inode_num, store_id,
         CASE
           WHEN lower(COALESCE(field_name,'')) LIKE '%creation%' OR lower(COALESCE(field_name,'')) LIKE '%created%' THEN 'created_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%modification%' OR lower(COALESCE(field_name,'')) LIKE '%modified%' THEN 'modified_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%access%' THEN 'accessed_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%open%' OR lower(COALESCE(field_name,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
           WHEN lower(COALESCE(field_name,''))='last_updated' THEN 'metadata_seen_or_index_updated'
           ELSE 'unclassified_spotlight_date_candidate'
         END AS spotlight_date_semantic_class
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
)
SELECT source_id, store_guid, source_db, field_name AS spotlight_date_source_field,
       spotlight_date_semantic_class, COALESCE(date_type,'') AS raw_date_type,
       COALESCE(parse_method,'') AS parse_method,
       COUNT(*) AS date_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_record_count,
       MIN(parsed_utc) AS earliest_parsed_utc,
       MAX(parsed_utc) AS latest_parsed_utc,
       substr(MIN(field_value),1,500) AS min_raw_value_sample,
       substr(MAX(field_value),1,500) AS max_raw_value_sample,
       CASE
         WHEN spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'CoreSpotlight metadata/index update timing; do not report as created/modified/accessed/opened usage without another decoded field.'
         WHEN spotlight_date_semantic_class LIKE '%candidate' THEN 'Candidate semantic class inferred from Spotlight raw date field name; validate field meaning before reporting.'
         ELSE 'Unclassified Spotlight date candidate; validate against raw_date_candidates and source Store-V2 record.'
       END AS reporting_caution
FROM dates
GROUP BY source_id, store_guid, source_db, field_name, spotlight_date_semantic_class, date_type, parse_method;

DROP VIEW IF EXISTS vw_ios_spotlight_investigative_item_date_evidence;
CREATE VIEW vw_ios_spotlight_investigative_item_date_evidence AS
SELECT v.raw_kv_id,
       v.raw_record_id,
       NULL AS raw_date_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.review_priority,
       v.original_value_length,
       v.readable_text_sample,
       dp.spotlight_date_utc,
       dp.spotlight_date_source_field,
       dp.spotlight_date_source_table,
       dp.spotlight_date_raw_value,
       dp.spotlight_date_parse_method,
       dp.spotlight_date_type,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%creation%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%created%' THEN 'created_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modification%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modified%' THEN 'modified_date_candidate'
)VSQLFIX",
R"VSQLFIX(         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%access%' THEN 'accessed_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%open%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'metadata_seen_or_index_updated'
         ELSE 'unclassified_spotlight_date_candidate'
       END AS spotlight_date_semantic_class,
       '' AS date_association_status,
       '' AS date_association_confidence,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(v.raw_kv_id AS TEXT),'') || '; field_name=' || COALESCE(v.field_name,'') AS value_validation_locator,
       'aggregated_date_provenance; source_fields=' || COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(dp.spotlight_date_raw_value,'') || '; parsed_utc=' || COALESCE(dp.spotlight_date_utc,'') || '; parse_method=' || COALESCE(dp.spotlight_date_parse_method,'') AS date_validation_locator,
       'source_db=' || COALESCE(v.source_db,'') || '; store_guid=' || COALESCE(v.store_guid,'') || '; raw_record_id=' || COALESCE(CAST(v.raw_record_id AS TEXT),'') || '; inode_or_object_id=' || COALESCE(v.inode_num,'') || '; store_id=' || COALESCE(v.store_id,'') AS spotlight_record_locator,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'Date is linked to this Spotlight record but represents CoreSpotlight metadata/index update timing unless a separately decoded field supports user activity.'
         WHEN COALESCE(dp.spotlight_date_source_field,'')<>'' THEN 'Date is aggregated at the Spotlight record level. Use date_source_evidence/raw_date_candidates for full per-date validation; full per-value/per-date expansion is diagnostics-only.'
         ELSE 'No direct Spotlight date was recovered for this value.'
       END AS date_reporting_caution,
       'Each row links one recovered investigative Spotlight value to aggregated date provenance for the same Store-V2 record. This avoids the prior many-to-many value/date expansion that produced tens of millions of rows in normal exports.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
LEFT JOIN vw_ios_spotlight_date_provenance dp ON dp.raw_record_id=v.raw_record_id;

)VSQLFIX"
}));

    exec(R"SQL(

DROP VIEW IF EXISTS vw_ios_spotlight_high_value_timeline;
CREATE VIEW vw_ios_spotlight_high_value_timeline AS
WITH base AS (
  SELECT * FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE review_priority IN ('HIGH_HUMAN_REVIEW_VALUE','MEDIUM_HUMAN_REVIEW_VALUE')
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.original_value_length,
       b.readable_text_sample,b.review_priority,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_source_table,
       b.spotlight_date_raw_value,b.spotlight_date_parse_method,b.spotlight_date_type,
       b.spotlight_date_semantic_class,b.date_validation_hint,b.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FILE_PATH_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       CASE
         WHEN b.spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'SPOTLIGHT_INDEX_TIME_WITH_VALUE_CONTEXT'
         WHEN b.spotlight_date_semantic_class LIKE '%candidate' THEN 'SPOTLIGHT_ACTIVITY_DATE_CANDIDATE_WITH_VALUE_CONTEXT'
         ELSE 'SPOTLIGHT_VALUE_WITH_UNCLASSIFIED_DATE_CONTEXT'
       END AS investigative_timeline_basis,
       'Spotlight-first high-value timeline. FFS and app database fields are context/corroboration only; the Spotlight value/date fields remain the primary evidence to validate.' AS interpretation_note
FROM base b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_file_reference_review;
CREATE VIEW vw_ios_spotlight_file_reference_review AS
WITH ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.readable_text_sample AS spotlight_file_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(f.residency_status,'NO_EXACT_FFS_PATH_LINK') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       CASE WHEN COALESCE(f.residency_status,'')='PRESENT_AS_FILE_IN_FFS' THEN 'SPOTLIGHT_PATH_PRESENT_IN_FFS_INVENTORY'
            WHEN COALESCE(f.residency_status,'')<>'' THEN f.residency_status
            ELSE 'SPOTLIGHT_FILE_REFERENCE_NO_EXACT_FFS_MATCH_IN_CURRENT_LINK_VIEW' END AS file_reference_status,
       'Spotlight/CoreSpotlight file/path reference with date provenance. FFS presence supports current-file existence only and is not proof of use or deletion by itself.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
WHERE b.human_text_category='FILE_PATH_OR_ATTACHMENT';

)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_url_reference_review;
CREATE VIEW vw_ios_spotlight_url_reference_review AS
WITH vals AS (
  SELECT *, lower(COALESCE(readable_text_sample,'')) AS v
  FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION')
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT v.raw_kv_id,v.raw_record_id,v.source_id,v.store_guid,v.source_db,
       v.spotlight_inode_or_object_id,v.spotlight_store_id,v.parent_inode_num,
       v.spotlight_value_source_field,v.human_text_category,v.readable_text_sample AS spotlight_url_or_web_reference,
       CASE
         WHEN instr(v.v,'https://')>0 THEN substr(v.v,instr(v.v,'https://'),300)
         WHEN instr(v.v,'http://')>0 THEN substr(v.v,instr(v.v,'http://'),300)
         WHEN instr(v.v,'www.')>0 THEN substr(v.v,instr(v.v,'www.'),300)
         ELSE substr(v.v,1,300)
       END AS normalized_url_reference_sample,
       v.spotlight_date_utc,v.spotlight_date_source_field,v.spotlight_date_raw_value,
       v.spotlight_date_parse_method,v.spotlight_date_semantic_class,v.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       'Spotlight/CoreSpotlight URL/web-like reference with date provenance. Browser/app database fields are supporting context and not an exact value match unless separately validated.' AS interpretation_note
FROM vals v
LEFT JOIN app a ON a.candidate_id=v.raw_kv_id;

)SQL");

    exec(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_account_contact_reference_review;
CREATE VIEW vw_ios_spotlight_account_contact_reference_review AS
WITH app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.readable_text_sample AS spotlight_account_or_contact_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       'Spotlight/CoreSpotlight account/contact-like reference with date provenance. Treat as a Spotlight value first; app database context is family-level unless exact string matching is later added.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id
WHERE b.human_text_category IN ('EMAIL_OR_ACCOUNT_TEXT','WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE');

DROP VIEW IF EXISTS vw_ios_spotlight_decode_gap_summary;
CREATE VIEW vw_ios_spotlight_decode_gap_summary AS
WITH gaps AS (
  SELECT source_id,store_guid,source_db,decode_gap_status,last_updated_utc
  FROM vw_ios_spotlight_decode_gap_records
)
SELECT g.source_id,g.store_guid,g.source_db,g.decode_gap_status,
       COUNT(*) AS gap_record_count,
       MIN(NULLIF(g.last_updated_utc,'')) AS earliest_gap_last_updated_utc,
       MAX(NULLIF(g.last_updated_utc,'')) AS latest_gap_last_updated_utc,
       COALESCE(dc.raw_record_count,0) AS store_raw_record_count,
       COALESCE(dc.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(dc.human_text_value_count,0) AS human_text_value_count,
       COALESCE(dc.pct_records_with_human_text,'') AS pct_records_with_human_text,
       COALESCE(dc.decode_failures,0) AS native_decode_failures,
       COALESCE(dc.decode_status,'') AS native_decode_status,
       'Summary of Spotlight/CoreSpotlight records parsed at header level but lacking recovered key/value or human-readable text values. This is the primary native parser improvement target list.' AS interpretation_note
FROM gaps g
LEFT JOIN vw_ios_spotlight_decode_coverage_summary dc ON dc.source_id=g.source_id AND dc.store_guid=g.store_guid AND dc.source_db=g.source_db
GROUP BY g.source_id,g.store_guid,g.source_db,g.decode_gap_status;

)SQL");

    exec(joinSql({
R"VSQLFIX(

DROP VIEW IF EXISTS vw_ios_spotlight_entity_review;
CREATE VIEW vw_ios_spotlight_entity_review AS
WITH base AS (
  SELECT b.*,
         lower(COALESCE(b.readable_text_sample,'')) AS lower_text
  FROM vw_ios_spotlight_investigative_items_with_dates b
  WHERE COALESCE(b.readable_text_sample,'')<>''
), typed AS (
  SELECT b.*,
         CASE
           WHEN b.human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION') THEN 'URL_OR_WEB_REFERENCE'
           WHEN b.human_text_category='FILE_PATH_OR_ATTACHMENT' THEN 'FILE_OR_ATTACHMENT_REFERENCE'
           WHEN b.human_text_category='EMAIL_OR_ACCOUNT_TEXT' THEN 'ACCOUNT_OR_EMAIL_REFERENCE'
           WHEN b.human_text_category IN ('WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE') THEN 'COMMUNICATION_APP_REFERENCE'
           WHEN b.human_text_category LIKE '%MESSAGE%' THEN 'MESSAGE_OR_COMMUNICATION_TEXT'
           ELSE 'OTHER_SPOTLIGHT_TEXT_REFERENCE'
         END AS entity_type
  FROM base b
), normalized AS (
  SELECT t.*,
         CASE
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'https://')>0 THEN substr(t.lower_text,instr(t.lower_text,'https://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'http://')>0 THEN substr(t.lower_text,instr(t.lower_text,'http://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'www.')>0 THEN substr(t.lower_text,instr(t.lower_text,'www.'),512)
           WHEN t.entity_type='FILE_OR_ATTACHMENT_REFERENCE' THEN replace(replace(replace(replace(t.lower_text,'file://',''),'<',''),'>',''),'\\','/')
           ELSE trim(t.lower_text)
         END AS normalized_entity_value
  FROM typed t
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,matched_record_category,matched_table_name,parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc,sample_parsed_value
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT n.raw_kv_id,
       n.raw_record_id,
       n.source_id,
       n.store_guid,
       n.source_db,
       n.spotlight_inode_or_object_id,
       n.spotlight_store_id,
       n.parent_inode_num,
       n.entity_type,
       n.human_text_category,
       n.review_priority,
       n.spotlight_value_source_field,
       n.normalized_entity_value,
       n.readable_text_sample,
       n.original_value_length,
       n.spotlight_date_utc,
       n.spotlight_date_source_field,
       n.spotlight_date_raw_value,
       n.spotlight_date_parse_method,
       n.spotlight_date_semantic_class,
       n.date_validation_hint,
       n.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FFS_LINK_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_LINK_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.matched_record_category,'') AS matched_record_category,
       COALESCE(a.matched_table_name,'') AS matched_table_name,
       COALESCE(a.sample_parsed_value,'') AS sample_app_db_value,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(n.raw_kv_id AS TEXT),'') || '; raw_records.raw_record_id=' || COALESCE(CAST(n.raw_record_id AS TEXT),'') || '; field_name=' || COALESCE(n.spotlight_value_source_field,'') AS reference_validation_locator,
       'raw_date_candidates.field_name=' || COALESCE(n.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(n.spotlight_date_raw_value,'') || '; parse_method=' || COALESCE(n.spotlight_date_parse_method,'') AS date_validation_locator,
       'Spotlight-first entity view. The entity/value and date columns originate from CoreSpotlight/Spotlight parsed records; app DB and FFS fields are corroborating context only.' AS interpretation_note
FROM normalized n
LEFT JOIN ffs f ON f.reference_id=n.raw_kv_id
LEFT JOIN app a ON a.candidate_id=n.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_entity_summary;
CREATE VIEW vw_ios_spotlight_entity_summary AS
SELECT entity_type,
       human_text_category,
       review_priority,
       store_guid,
       source_db,
       spotlight_value_source_field,
       spotlight_date_semantic_class,
       COUNT(*) AS entity_row_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT normalized_entity_value) AS distinct_normalized_entity_count,
       SUM(CASE WHEN ffs_residency_status='PRESENT_AS_FILE_IN_FFS' THEN 1 ELSE 0 END) AS ffs_present_context_count,
       SUM(CASE WHEN app_db_link_status LIKE 'PRESENT%' OR app_db_link_status LIKE '%PRESENT%' THEN 1 ELSE 0 END) AS app_db_present_context_count,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       MIN(substr(normalized_entity_value,1,240)) AS min_sample_entity,
       MAX(substr(normalized_entity_value,1,240)) AS max_sample_entity,
       'Spotlight entity summary. Counts are derived from recovered CoreSpotlight text/probe values and their Spotlight date provenance; app/FFS context is only supporting context.' AS interpretation_note
FROM vw_ios_spotlight_entity_review
GROUP BY entity_type,human_text_category,review_priority,store_guid,source_db,spotlight_value_source_field,spotlight_date_semantic_class;

DROP VIEW IF EXISTS vw_ios_spotlight_native_parser_targets;
CREATE VIEW vw_ios_spotlight_native_parser_targets AS
SELECT source_id,
       store_guid,
       source_db,
       'RECORDS_WITHOUT_RECOVERED_TEXT' AS parser_target_type,
       decode_gap_status AS target_name,
       gap_record_count AS target_count,
       store_raw_record_count AS store_raw_record_count,
)VSQLFIX",
R"VSQLFIX(       recovered_key_value_count AS recovered_key_value_count,
       human_text_value_count AS human_text_value_count,
       pct_records_with_human_text AS pct_records_with_human_text,
       native_decode_failures AS native_decode_failures,
       CASE WHEN gap_record_count>100000 THEN 'HIGH' WHEN gap_record_count>10000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Improve native CoreSpotlight property/dictionary/value decoding for records that parse at header level but do not yield recovered text/key-value rows.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_decode_gap_summary
UNION ALL
SELECT source_id,
       store_guid,
       source_db,
       'GENERIC_STRING_PROBE_FIELD' AS parser_target_type,
       field_name AS target_name,
       value_row_count AS target_count,
       NULL AS store_raw_record_count,
       value_row_count AS recovered_key_value_count,
       distinct_record_count AS human_text_value_count,
       '' AS pct_records_with_human_text,
       NULL AS native_decode_failures,
       CASE WHEN value_row_count>10000 THEN 'HIGH' WHEN value_row_count>1000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Map generic __native_core_probe_string_* fields back to real CoreSpotlight property names/types where possible.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_field_coverage_summary
WHERE field_decode_status='GENERIC_NATIVE_STRING_PROBE';

DROP VIEW IF EXISTS vw_ios_spotlight_decode_coverage_summary;
CREATE VIEW vw_ios_spotlight_decode_coverage_summary AS
WITH rr AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS raw_record_count,
         SUM(CASE WHEN COALESCE(last_updated_utc,'')<>'' THEN 1 ELSE 0 END) AS records_with_last_updated,
         MIN(NULLIF(last_updated_utc,'')) AS earliest_last_updated_utc,
         MAX(NULLIF(last_updated_utc,'')) AS latest_last_updated_utc
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), kv AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS recovered_key_value_count,
         COUNT(DISTINCT field_name) AS recovered_field_name_count,
         COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS records_with_recovered_values
  FROM raw_key_values
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), ht AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS human_text_value_count,
         COUNT(DISTINCT raw_record_id) AS records_with_human_text,
         COUNT(DISTINCT human_text_category) AS human_text_category_count
  FROM vw_ios_spotlight_human_text_values
  GROUP BY source_id,store_guid,source_db
), nd AS (
  SELECT source_id,store_guid,source_db,
         MAX(decode_mode) AS decode_mode,
         MAX(spotlight_version) AS spotlight_version,
         MAX(properties_count) AS native_property_count,
         MAX(categories_count) AS native_category_count,
         MAX(metadata_blocks) AS metadata_blocks,
         MAX(decompressed_blocks) AS decompressed_blocks,
         MAX(failures) AS decode_failures,
         MAX(status) AS decode_status,
         MAX(message) AS decode_message
  FROM native_decode_attempts
  GROUP BY source_id,store_guid,source_db
)
SELECT rr.source_id,rr.store_guid,rr.source_db,
       COALESCE(nd.decode_mode,'') AS decode_mode,
       COALESCE(nd.spotlight_version,0) AS spotlight_version,
       rr.raw_record_count,
       COALESCE(kv.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(kv.recovered_field_name_count,0) AS recovered_field_name_count,
       COALESCE(kv.records_with_recovered_values,0) AS records_with_recovered_values,
       COALESCE(ht.human_text_value_count,0) AS human_text_value_count,
       COALESCE(ht.records_with_human_text,0) AS records_with_human_text,
       COALESCE(ht.human_text_category_count,0) AS human_text_category_count,
       CASE WHEN rr.raw_record_count>0 THEN printf('%.2f', 100.0 * COALESCE(ht.records_with_human_text,0) / rr.raw_record_count) ELSE '0.00' END AS pct_records_with_human_text,
       COALESCE(nd.native_property_count,0) AS native_property_count,
       COALESCE(nd.native_category_count,0) AS native_category_count,
       COALESCE(nd.metadata_blocks,0) AS metadata_blocks,
       COALESCE(nd.decompressed_blocks,0) AS decompressed_blocks,
       COALESCE(nd.decode_failures,0) AS decode_failures,
       COALESCE(nd.decode_status,'NO_NATIVE_DECODE_ATTEMPT_ROW') AS decode_status,
       rr.earliest_last_updated_utc,rr.latest_last_updated_utc,
       CASE WHEN COALESCE(nd.native_property_count,0)=0 THEN 'PROPERTY_DICTIONARY_NOT_DECODED_GENERIC_PROBES_ONLY'
            WHEN COALESCE(kv.recovered_key_value_count,0)=0 THEN 'NO_KEY_VALUES_RECOVERED'
            ELSE 'GENERIC_TEXT_VALUES_RECOVERED' END AS spotlight_decode_interpretation,
       'Spotlight-first coverage view. App/FFS correlation is supporting context; this row measures native CoreSpotlight record/value recovery.' AS interpretation_note
FROM rr
LEFT JOIN kv ON kv.source_id=rr.source_id AND kv.store_guid=rr.store_guid AND kv.source_db=rr.source_db
LEFT JOIN ht ON ht.source_id=rr.source_id AND ht.store_guid=rr.store_guid AND ht.source_db=rr.source_db
LEFT JOIN nd ON nd.source_id=rr.source_id AND nd.store_guid=rr.store_guid AND nd.source_db=rr.source_db;

DROP VIEW IF EXISTS vw_ios_spotlight_field_coverage_summary;
CREATE VIEW vw_ios_spotlight_field_coverage_summary AS
SELECT source_id,store_guid,source_db,field_name,
       COUNT(*) AS value_row_count,
       COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS distinct_record_count,
       MIN(LENGTH(COALESCE(field_value,''))) AS min_value_length,
       MAX(LENGTH(COALESCE(field_value,''))) AS max_value_length,
       substr(MIN(COALESCE(field_value,'')),1,1000) AS min_sample_value,
       substr(MAX(COALESCE(field_value,'')),1,1000) AS max_sample_value,
       CASE WHEN field_name LIKE '__native_core_probe_string_%' THEN 'GENERIC_NATIVE_STRING_PROBE'
            WHEN field_name LIKE '__native_%' THEN 'GENERIC_NATIVE_FIELD'
            ELSE 'NAMED_SPOTLIGHT_FIELD' END AS field_decode_status,
       'Field coverage summary from recovered Spotlight key/value rows. Generic probe names indicate values recovered before formal property-name mapping.' AS interpretation_note
)VSQLFIX",
R"VSQLFIX(FROM raw_key_values
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(field_value,'')<>''
GROUP BY source_id,store_guid,source_db,field_name;

DROP VIEW IF EXISTS vw_ios_spotlight_text_category_summary;
CREATE VIEW vw_ios_spotlight_text_category_summary AS
SELECT human_text_category,review_priority,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT store_guid) AS store_count,
       MIN(original_value_length) AS min_original_value_length,
       MAX(original_value_length) AS max_original_value_length,
       substr(MIN(readable_text_sample),1,1000) AS min_sample_text,
       substr(MAX(readable_text_sample),1,1000) AS max_sample_text,
       'Spotlight recovered text category summary. Categories are triage labels over Spotlight text values, not final CoreSpotlight property names.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values
GROUP BY human_text_category,review_priority;

DROP VIEW IF EXISTS vw_ios_spotlight_record_review;
CREATE VIEW vw_ios_spotlight_record_review AS
WITH text_roll AS (
  SELECT raw_record_id,text_value_count,distinct_text_category_count,human_text_categories,has_high_review_value_text,readable_text_rollup_sample
  FROM vw_ios_spotlight_human_text_rollup
), date_one AS (
  SELECT raw_record_id,
         MAX(spotlight_date_utc) AS spotlight_date_utc,
         MAX(spotlight_date_source_field) AS spotlight_date_source_field,
         MAX(spotlight_date_source_table) AS spotlight_date_source_table,
         MAX(spotlight_date_raw_value) AS spotlight_date_raw_value,
         MAX(spotlight_date_parse_method) AS spotlight_date_parse_method,
         MAX(spotlight_date_type) AS spotlight_date_type,
         MAX(spotlight_date_source_evidence) AS spotlight_date_source_evidence,
         MAX(date_validation_hint) AS date_validation_hint,
         COUNT(*) AS collapsed_date_candidate_count
  FROM vw_ios_spotlight_date_provenance
  GROUP BY raw_record_id
)
SELECT r.raw_record_id,r.source_id,r.store_guid,r.source_db,r.inode_num AS spotlight_inode_or_object_id,r.store_id AS spotlight_store_id,r.parent_inode_num,
       COALESCE(dp.spotlight_date_utc,r.last_updated_utc) AS spotlight_date_utc,
       COALESCE(dp.spotlight_date_source_field,'Last_Updated') AS spotlight_date_source_field,
       COALESCE(dp.spotlight_date_source_table,'raw_records') AS spotlight_date_source_table,
       COALESCE(dp.spotlight_date_raw_value,r.last_updated_raw) AS spotlight_date_raw_value,
       COALESCE(dp.spotlight_date_parse_method,'native_epoch_microseconds') AS spotlight_date_parse_method,
       COALESCE(dp.spotlight_date_type,'metadata_seen_or_index_updated') AS spotlight_date_type,
       COALESCE(dp.spotlight_date_source_evidence,'Last_Updated=' || COALESCE(r.last_updated_raw,'') || ' -> ' || COALESCE(r.last_updated_utc,'')) AS spotlight_date_source_evidence,
       COALESCE(dp.date_validation_hint,'Validate against raw_records.last_updated_raw/last_updated_utc for this Store-V2 record.') AS spotlight_date_validation_hint,
       COALESCE(dp.collapsed_date_candidate_count,0) AS collapsed_date_candidate_count,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       COALESCE(t.text_value_count,0) AS spotlight_text_value_count,
       COALESCE(t.distinct_text_category_count,0) AS spotlight_text_category_count,
       COALESCE(t.human_text_categories,'') AS spotlight_text_categories,
       COALESCE(t.readable_text_rollup_sample,'') AS spotlight_text_rollup_sample,
       0 AS ffs_reference_count,
       0 AS ffs_present_reference_count,
       0 AS ffs_missing_or_unresolved_reference_count,
       0 AS app_db_candidate_count,
       0 AS app_db_present_candidate_count,
       0 AS app_db_unresolved_candidate_count,
       CASE WHEN COALESCE(t.has_high_review_value_text,0)=1 THEN 'HIGH_SPOTLIGHT_TEXT_VALUE'
            WHEN COALESCE(t.text_value_count,0)>0 THEN 'SPOTLIGHT_TEXT_VALUE'
            ELSE 'SPOTLIGHT_RECORD_NO_RECOVERED_TEXT' END AS spotlight_review_priority,
       CASE WHEN COALESCE(t.text_value_count,0)>0 THEN 'TEXT_VALUES_RECOVERED_FROM_SPOTLIGHT'
            ELSE 'NO_TEXT_VALUES_RECOVERED_FOR_RECORD' END AS spotlight_decode_status,
       'Spotlight-first record review. V0_9_21 keeps GUI rows raw_record anchored and avoids broad FFS/app joins; full per-record exports are support/diagnostic-only to prevent long SQL materialization. Use Missing From FFS and object/object-summary views for residency pivots.' AS interpretation_note
FROM raw_records r
LEFT JOIN text_roll t ON t.raw_record_id=r.raw_record_id
LEFT JOIN date_one dp ON dp.raw_record_id=r.raw_record_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_summary;
CREATE VIEW vw_ios_spotlight_object_inode_summary AS
WITH rec AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count,
         COUNT(DISTINCT COALESCE(parent_inode_num,'')) AS distinct_parent_id_count,
         MIN(last_updated_utc) AS earliest_last_updated_utc,
         MAX(last_updated_utc) AS latest_last_updated_utc,
         MIN(raw_record_id) AS first_raw_record_id,
         MAX(raw_record_id) AS last_raw_record_id
  FROM raw_records
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), kv AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_key_value_rows,
         COUNT(DISTINCT field_name) AS distinct_spotlight_field_count,
         substr(MAX(CASE WHEN field_name='__spotlight_investigator_text_context' THEN field_value ELSE '' END),1,1800) AS spotlight_text_context_sample
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
)
)VSQLFIX",
R"VSQLFIX(SELECT rec.source_id,rec.store_guid,rec.source_db,rec.spotlight_inode_or_object_id,rec.spotlight_store_id,
       rec.raw_record_count,rec.distinct_parent_id_count,
       COALESCE(kv.raw_key_value_rows,0) AS raw_key_value_rows,
       COALESCE(kv.distinct_spotlight_field_count,0) AS distinct_spotlight_field_count,
       0 AS date_candidate_rows,
       rec.earliest_last_updated_utc,rec.latest_last_updated_utc,
       '' AS earliest_spotlight_date_utc,
       '' AS latest_spotlight_date_utc,
       rec.first_raw_record_id,rec.last_raw_record_id,
       COALESCE(kv.spotlight_text_context_sample,'') AS spotlight_text_context_sample,
       CASE WHEN rec.raw_record_count>1 THEN 'MULTIPLE_SPOTLIGHT_RECORDS_SHARE_OBJECT_ID'
            WHEN COALESCE(kv.raw_key_value_rows,0)>20 THEN 'SINGLE_RECORD_MANY_FIELDS'
            ELSE 'SINGLE_OR_LOW_EXPANSION_OBJECT' END AS object_materialization_status,
       'Object/inode-centric rollup. V0_9_21 normal exports summarize this view because the full per-object listing is support/diagnostic-only.' AS interpretation_note
FROM rec
LEFT JOIN kv ON kv.source_id=rec.source_id AND kv.store_guid=rec.store_guid AND kv.source_db=rec.source_db AND kv.spotlight_inode_or_object_id=rec.spotlight_inode_or_object_id AND kv.spotlight_store_id=rec.spotlight_store_id;

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_diagnostic_summary;
CREATE VIEW vw_ios_spotlight_object_inode_diagnostic_summary AS
WITH obj AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), buckets AS (
  SELECT source_id,store_guid,source_db,
         CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
              WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
              WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
              ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END AS object_record_bucket,
         COUNT(*) AS object_count,
         SUM(raw_record_count) AS raw_record_count,
         MIN(raw_record_count) AS min_records_per_object,
         MAX(raw_record_count) AS max_records_per_object
  FROM obj
  GROUP BY source_id,store_guid,source_db,
           CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
                WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
                WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
                ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END
)
SELECT source_id,store_guid,source_db,object_record_bucket,object_count,raw_record_count,min_records_per_object,max_records_per_object,
       'Compact object/inode materialization diagnostic. Use this normal export to decide whether the case should pivot to object-centric aggregation; full per-object rows require support/diagnostic export.' AS interpretation_note
FROM buckets;

DROP VIEW IF EXISTS vw_ios_spotlight_decode_gap_records;
CREATE VIEW vw_ios_spotlight_decode_gap_records AS
SELECT raw_record_id,source_id,store_guid,source_db,inode_num AS spotlight_inode_or_object_id,store_id AS spotlight_store_id,parent_inode_num,last_updated_utc,
       file_name,display_name,full_path,content_type,record_state,
       'NO_KEY_VALUES_OR_TEXT_PROBES_RECOVERED_FOR_SPOTLIGHT_RECORD' AS decode_gap_status,
       'This Spotlight/CoreSpotlight record was parsed at the record/header level but no key/value or human-readable text values were recovered. These rows identify the next native parser decoding target.' AS interpretation_note
FROM raw_records r
WHERE (r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%')
  AND NOT EXISTS (
    SELECT 1 FROM raw_key_values kv
    WHERE kv.source_id=r.source_id AND kv.store_guid=r.store_guid AND kv.source_db=r.source_db
      AND kv.inode_num=r.inode_num AND COALESCE(kv.store_id,'')=COALESCE(r.store_id,'')
  );

DROP VIEW IF EXISTS vw_ios_database_residency_candidates;
CREATE VIEW vw_ios_database_residency_candidates AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,field_value,LOWER(field_value) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), cats AS (
  SELECT *, CASE
    WHEN v LIKE '%whatsapp%' THEN 'WHATSAPP_RELATED'
    WHEN v LIKE '%signal%' THEN 'SIGNAL_RELATED'
    WHEN v LIKE '%telegram%' THEN 'TELEGRAM_RELATED'
    WHEN v LIKE '%/sms/%' OR v LIKE '%library/sms%' OR v LIKE '%sms.db%' OR v LIKE '%imessage%' OR v LIKE '%com.apple.mobilesms%' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
    WHEN (v LIKE '%/callhistory%' OR v LIKE '%callhistory.storedata%' OR v LIKE '%facetime%' OR v LIKE 'tel:%' OR v LIKE '% tel:%')
         AND v NOT LIKE '%tel.meet%' AND v NOT LIKE '%meet.google%' THEN 'CALL_OR_FACETIME_RELATED'
    WHEN v LIKE '%.ics%' OR v LIKE '%/calendar/%' OR v LIKE '%calendar.google.com%' OR v LIKE '%text/calendar%' OR v LIKE '%vevent%' OR v LIKE '%vcalendar%' OR v LIKE '%webcal:%' OR v LIKE '%invite.ics%' THEN 'CALENDAR_OR_INVITATION_RELATED'
    WHEN v LIKE '%/library/mail/%' OR v LIKE '%attachmentdata%' OR v LIKE '%message/rfc822%' OR v LIKE '%mail.google.com%' OR v LIKE 'from:%' OR v LIKE 'to:%' OR v LIKE 'cc:%' OR v LIKE '%mailto:%' THEN 'MAIL_OR_ACCOUNT_RELATED'
    WHEN v LIKE '%addressbook%' OR v LIKE '%begin:vcard%' OR v LIKE '%vcard%' THEN 'CONTACT_OR_ADDRESS_BOOK_RELATED'
    WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' OR v LIKE '%safari%' OR v LIKE '%history%' THEN 'WEB_OR_BROWSER_RELATED'
    ELSE 'OTHER_DB_RESIDENCY_REVIEW'
  END AS object_category
  FROM probes
), db_clean AS (
  SELECT *,
         CASE
           WHEN database_category='APPLE_MESSAGES' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
           WHEN database_category='CALL_HISTORY' THEN 'CALL_OR_FACETIME_RELATED'
           WHEN database_category='WHATSAPP' THEN 'WHATSAPP_RELATED'
           WHEN database_category='SIGNAL' THEN 'SIGNAL_RELATED'
           WHEN database_category='TELEGRAM' THEN 'TELEGRAM_RELATED'
)VSQLFIX",
R"VSQLFIX(           WHEN database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT') THEN 'WEB_OR_BROWSER_RELATED'
           WHEN database_category='MAIL' THEN 'MAIL_OR_ACCOUNT_RELATED'
           WHEN database_category='CONTACTS' THEN 'CONTACT_OR_ADDRESS_BOOK_RELATED'
           WHEN database_category='CALENDAR' THEN 'CALENDAR_OR_INVITATION_RELATED'
           ELSE '' END AS app_db_object_category
  FROM ios_app_database_inventory
  WHERE COALESCE(database_category,'')<>''
    AND LOWER(COALESCE(normalized_path,'')) NOT LIKE '%-wal'
    AND LOWER(COALESCE(normalized_path,'')) NOT LIKE '%-shm'
    AND LOWER(COALESCE(database_name,'')) NOT LIKE '%-wal'
    AND LOWER(COALESCE(database_name,'')) NOT LIKE '%-shm'
), db_family AS (
  SELECT source_id,app_db_object_category AS object_category,
         COUNT(DISTINCT ios_db_id) AS matching_database_count,
         MIN(database_name) AS database_name,
         GROUP_CONCAT(DISTINCT database_category) AS database_category,
         MIN(app_hint) AS app_hint,
         MIN(normalized_path) AS candidate_database_path,
         MAX(COALESCE(record_inventory_status,'')) AS record_inventory_status
  FROM db_clean
  WHERE app_db_object_category<>''
  GROUP BY source_id,app_db_object_category
), ri_family AS (
  SELECT d.source_id,d.app_db_object_category AS object_category,
         COUNT(*) AS matching_table_count,
         MIN(ri.record_category) AS matched_record_category,
         MIN(ri.table_name) AS matched_table_name,
         MAX(COALESCE(ri.row_count,0)) AS matched_table_row_count
  FROM ios_app_database_record_inventory ri
  JOIN db_clean d ON d.ios_db_id=ri.ios_db_id
  WHERE d.app_db_object_category<>''
  GROUP BY d.source_id,d.app_db_object_category
), parsed_family AS (
  SELECT d.source_id,d.app_db_object_category AS object_category,
         COUNT(*) AS parsed_record_count,
         MIN(p.record_category) AS parsed_record_category
  FROM ios_app_parsed_records p
  JOIN db_clean d ON d.ios_db_id=p.ios_db_id
  WHERE d.app_db_object_category<>''
  GROUP BY d.source_id,d.app_db_object_category
)
SELECT c.raw_kv_id AS candidate_id,c.source_id,c.store_guid,c.source_db,c.inode_num,c.store_id,c.parent_inode_num,c.field_name,
       c.object_category,substr(c.field_value,1,2000) AS string_value_sample,
       d.database_name,d.database_category,d.app_hint,d.candidate_database_path,d.record_inventory_status,
       COALESCE(pb.parsed_record_category,ri.matched_record_category,'') AS matched_record_category,
       COALESCE(ri.matched_table_name,'') AS matched_table_name,
       COALESCE(ri.matched_table_row_count,0) AS matched_table_row_count,
       CASE WHEN COALESCE(pb.parsed_record_count,0)>0 THEN 'DATABASE_FAMILY_PARSED_RECORDS_AVAILABLE_VALUE_MATCH_NOT_PROVEN'
            WHEN COALESCE(ri.matching_table_count,0)>0 THEN 'DATABASE_FAMILY_TABLE_PRESENT_VALUE_MATCH_NOT_PROVEN'
            WHEN COALESCE(d.matching_database_count,0)>0 THEN 'DATABASE_FAMILY_PRESENT_RECORD_TABLE_NOT_PARSED'
            ELSE 'NO_KNOWN_APP_DATABASE_INVENTORY_MATCH' END AS database_residency_status,
       CASE WHEN COALESCE(pb.parsed_record_count,0)>0
            THEN 'Strict string classification matched a database family with parsed records. This is a lead only; exact Spotlight string-to-row correlation is not yet proven.'
            WHEN COALESCE(d.matching_database_count,0)>0
            THEN 'Strict string classification matched a database family. It does not prove the specific value is present in the database.'
            ELSE 'No matching database family was identified in the current app database inventory.' END AS interpretation_note
FROM cats c
LEFT JOIN db_family d ON d.source_id=c.source_id AND d.object_category=c.object_category
LEFT JOIN ri_family ri ON ri.source_id=c.source_id AND ri.object_category=c.object_category
LEFT JOIN parsed_family pb ON pb.source_id=c.source_id AND pb.object_category=c.object_category
WHERE c.object_category<>'OTHER_DB_RESIDENCY_REVIEW';

)VSQLFIX"
}));

    // V0_9_6: iOS investigator pivot and unified keyword-search surface views.
    exec(R"SQL(
DROP VIEW IF EXISTS vw_ios_contact_identity_records;
CREATE VIEW vw_ios_contact_identity_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,
       CASE
         WHEN lower(table_name)='abperson' THEN 'ADDRESSBOOK_PERSON_ROW'
         WHEN lower(table_name)='contacts' THEN 'CONTACTS_CACHE_ROW'
         WHEN lower(table_name) LIKE '%fulltextsearch_content%' THEN 'CONTACT_FULLTEXT_CONTENT'
         WHEN COALESCE(contact_or_participant,'')<>'' THEN 'CONTACT_TEXT_OR_ADDRESS_VALUE'
         WHEN COALESCE(item_identifier,'')<>'' THEN 'CONTACT_IDENTIFIER_VALUE'
         ELSE 'CONTACT_REVIEW_ROW'
       END AS contact_identity_type,
       substr(trim(COALESCE(contact_or_participant,'') || ' ' || COALESCE(title,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'')),1,1200) AS identity_value_sample,
       'Parsed contact/address-book database row or contact cache row. Contact cache/FTS rows can duplicate or tokenize contact data; use database path/table/provenance before reporting.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category='CONTACTS'
  AND lower(table_name) NOT LIKE '%docsize%'
  AND lower(table_name) NOT LIKE '%segdir%'
  AND lower(table_name) NOT LIKE '%segments%'
  AND lower(table_name) NOT LIKE '%_stat%'
  AND (
       lower(table_name) IN ('abperson','contacts','abpersonfulltextsearch_content','abpersonsmartdialerfulltextsearch_content')
       OR COALESCE(contact_or_participant,'')<>'' OR COALESCE(title,'')<>'' OR COALESCE(text_snippet,'')<>'' OR COALESCE(item_identifier,'')<>''
  );

DROP VIEW IF EXISTS vw_ios_contact_identity_summary;
CREATE VIEW vw_ios_contact_identity_summary AS
SELECT database_name,database_normalized_path,table_name,contact_identity_type,parse_status,
       COUNT(*) AS contact_review_row_count,
       COUNT(DISTINCT item_identifier) AS distinct_item_identifier_count,
       SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS rows_with_contact_text,
       SUM(CASE WHEN COALESCE(title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_title,
       SUM(CASE WHEN COALESCE(text_snippet,'')<>'' THEN 1 ELSE 0 END) AS rows_with_text_snippet,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc,
       MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc
FROM vw_ios_contact_identity_records
GROUP BY database_name,database_normalized_path,table_name,contact_identity_type,parse_status
ORDER BY contact_review_row_count DESC,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_web_history_review_records;
CREATE VIEW vw_ios_web_history_review_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       url,title,item_identifier,text_snippet,parse_status,provenance,
       CASE
         WHEN lower(table_name) LIKE '%bookmark%' THEN 'BOOKMARK_OR_SAVED_WEB_ITEM'
         WHEN lower(table_name) LIKE '%history%' THEN 'WEB_HISTORY_OR_VISIT'
         ELSE 'WEB_DATABASE_RECORD'
       END AS web_record_type,
       substr(trim(COALESCE(title,'') || ' ' || COALESCE(url,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'')),1,1600) AS web_review_value_sample,
       'Parsed local web/browser database row. Timestamp interpretation depends on the source table and parser provenance.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT')
   OR lower(app_hint) IN ('safari','chrome','webkit')
   OR lower(database_name) IN ('history.db','safaritabs.db')
ORDER BY record_timestamp_utc DESC,database_name,table_name,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_web_history_review_summary;
CREATE VIEW vw_ios_web_history_review_summary AS
SELECT database_category,app_hint,database_name,table_name,web_record_type,parse_status,
       COUNT(*) AS web_review_row_count,
       SUM(CASE WHEN COALESCE(url,'')<>'' THEN 1 ELSE 0 END) AS rows_with_url,
       SUM(CASE WHEN COALESCE(title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_title,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc,
       MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc,
       MIN(database_normalized_path) AS first_database_path,MAX(database_normalized_path) AS last_database_path
FROM vw_ios_web_history_review_records
GROUP BY database_category,app_hint,database_name,table_name,web_record_type,parse_status
ORDER BY web_review_row_count DESC,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_calendar_review_records;
CREATE VIEW vw_ios_calendar_review_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,
       CASE
         WHEN lower(table_name) LIKE '%attendee%' THEN 'CALENDAR_ATTENDEE_OR_INVITEE'
         WHEN lower(table_name) LIKE '%location%' THEN 'CALENDAR_LOCATION'
         WHEN lower(table_name) LIKE '%attachment%' THEN 'CALENDAR_ATTACHMENT'
         WHEN COALESCE(contact_or_participant,'')<>'' THEN 'CALENDAR_ACCOUNT_OR_INVITEE'
         ELSE 'CALENDAR_EVENT_OR_SUPPORT_ROW'
       END AS calendar_record_type,
       substr(trim(COALESCE(title,'') || ' ' || COALESCE(contact_or_participant,'') || ' ' || COALESCE(url,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'')),1,1600) AS calendar_review_value_sample,
)SQL" R"SQL(       'Parsed calendar database row. Calendar rows may include calendars, attendees, suggestions, attachments, or event-support rows; use table/provenance before reporting.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category='CALENDAR'
  AND (COALESCE(title,'')<>'' OR COALESCE(contact_or_participant,'')<>'' OR COALESCE(url,'')<>'' OR COALESCE(text_snippet,'')<>'' OR COALESCE(item_identifier,'')<>'' OR COALESCE(record_timestamp_utc,'')<>'')
ORDER BY record_timestamp_utc DESC,database_name,table_name,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_calendar_review_summary;
CREATE VIEW vw_ios_calendar_review_summary AS
SELECT database_name,database_normalized_path,table_name,calendar_record_type,parse_status,
       COUNT(*) AS calendar_review_row_count,
       SUM(CASE WHEN COALESCE(title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_title,
       SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS rows_with_contact_or_account,
)SQL" R"SQL(       SUM(CASE WHEN COALESCE(record_timestamp_utc,'')<>'' THEN 1 ELSE 0 END) AS rows_with_timestamp,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc,
       MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc
FROM vw_ios_calendar_review_records
GROUP BY database_name,database_normalized_path,table_name,calendar_record_type,parse_status
ORDER BY calendar_review_row_count DESC,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_investigation_keyword_surface;
CREATE VIEW vw_ios_investigation_keyword_surface AS
SELECT 'CORESPOTLIGHT_TEXT' AS surface_source, source_id, CAST(raw_kv_id AS TEXT) AS source_record_id,
       human_text_category AS review_category, store_guid AS source_container, source_db AS source_location,
       field_name AS field_or_table, '' AS record_timestamp_utc, readable_text_sample AS searchable_value_sample,
       '' AS path_or_url, '' AS contact_or_identity, review_priority AS review_priority,
       'SPOTLIGHT_INDEX_VALUE' AS residency_context, interpretation_note
FROM vw_ios_spotlight_human_text_values
UNION ALL
SELECT 'APP_DATABASE_RECORD' AS surface_source, source_id, CAST(ios_app_record_id AS TEXT) AS source_record_id,
       database_category || ':' || record_category AS review_category, database_name AS source_container, database_normalized_path AS source_location,
       table_name AS field_or_table, record_timestamp_utc,
       substr(trim(COALESCE(title,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'') || ' ' || COALESCE(contact_or_participant,'') || ' ' || COALESCE(url,'') || ' ' || COALESCE(file_path,'')),1,2000) AS searchable_value_sample,
       COALESCE(NULLIF(url,''),file_path) AS path_or_url, contact_or_participant AS contact_or_identity,
       CASE WHEN database_category IN ('APPLE_MESSAGES','WHATSAPP','CALL_HISTORY') THEN 'HIGH_APP_COMMUNICATION_RECORD'
            WHEN database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT','MAIL','CALENDAR','CONTACTS') THEN 'MEDIUM_HIGH_APP_RECORD'
            ELSE 'APP_DATABASE_RECORD' END AS review_priority,
       'APP_DATABASE_RECORD_PRESENT_IN_ACQUIRED_DATABASE' AS residency_context,
       'Parsed app database value. This indicates the value came from the acquired/staged database listed, not necessarily from Spotlight.' AS interpretation_note
FROM ios_app_parsed_records
WHERE trim(COALESCE(title,'') || COALESCE(text_snippet,'') || COALESCE(item_identifier,'') || COALESCE(contact_or_participant,'') || COALESCE(url,'') || COALESCE(file_path,''))<>''
UNION ALL
SELECT 'FFS_HIGH_VALUE_PATH' AS surface_source, source_id, CAST(ios_file_id AS TEXT) AS source_record_id,
       COALESCE(NULLIF(app_container_hint,''),domain_hint) AS review_category, file_name AS source_container, normalized_path AS source_location,
       extension AS field_or_table, zip_modified_utc AS record_timestamp_utc, substr(normalized_path,1,2000) AS searchable_value_sample,
       normalized_path AS path_or_url, '' AS contact_or_identity,
       'HIGH_VALUE_FFS_PATH' AS review_priority,
       'PATH_PRESENT_IN_FFS_ZIP_INVENTORY' AS residency_context,
       'FFS path inventory row. Presence means the path was enumerated in the ZIP; absence from this filtered view does not mean absence from the ZIP.' AS interpretation_note
FROM ios_ffs_file_inventory
WHERE lower(normalized_path) LIKE '%sms%' OR lower(normalized_path) LIKE '%message%' OR lower(normalized_path) LIKE '%whatsapp%'
   OR lower(normalized_path) LIKE '%callhistory%' OR lower(normalized_path) LIKE '%facetime%' OR lower(normalized_path) LIKE '%addressbook%'
   OR lower(normalized_path) LIKE '%contacts%' OR lower(normalized_path) LIKE '%calendar%' OR lower(normalized_path) LIKE '%safari%'
   OR lower(normalized_path) LIKE '%history.db%' OR lower(normalized_path) LIKE '%mail%' OR lower(normalized_path) LIKE '%keychain%'
   OR lower(normalized_path) LIKE '%attachment%' OR lower(normalized_path) LIKE '%documents%' OR lower(normalized_path) LIKE '%downloads%'
UNION ALL
SELECT 'APP_DATABASE_INVENTORY' AS surface_source, source_id, CAST(ios_db_id AS TEXT) AS source_record_id,
       database_category AS review_category, database_name AS source_container, normalized_path AS source_location,
       app_hint AS field_or_table, zip_modified_utc AS record_timestamp_utc, substr(normalized_path,1,2000) AS searchable_value_sample,
       normalized_path AS path_or_url, '' AS contact_or_identity,
       'APP_DATABASE_DISCOVERY' AS review_priority,
       COALESCE(record_inventory_status,parse_status) AS residency_context,
       'App database inventory row. Use record inventory and parsed-record views to confirm whether rows were parsed.' AS interpretation_note
FROM ios_app_database_inventory;


)SQL");
    exec(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_dbstr_map_inventory;
CREATE VIEW vw_ios_spotlight_dbstr_map_inventory AS
SELECT source_id,store_guid,source_db,map_id,
       CASE map_id WHEN 1 THEN 'properties' WHEN 2 THEN 'categories' WHEN 4 THEN 'index_1' WHEN 5 THEN 'index_2' ELSE 'unknown' END AS map_role,
       data_exists,offsets_exists,header_exists,data_bytes,offsets_bytes,header_bytes,
       offset_entries,parsed_entries,skipped_entries,status,message,
       CASE WHEN status='PARSED' AND parsed_entries>0 THEN 'AVAILABLE'
            WHEN status='MISSING_COMPONENT' THEN 'MISSING_DBSTR_COMPONENT'
            WHEN status='FAILED' THEN 'PARSE_FAILED'
            ELSE 'UNRESOLVED' END AS parser_use_status,
       created_utc
FROM native_dbstr_map_inventory;

DROP VIEW IF EXISTS vw_ios_spotlight_dictionary_coverage;
CREATE VIEW vw_ios_spotlight_dictionary_coverage AS
WITH rr AS (
  SELECT source_id,store_guid,source_db,COUNT(*) AS raw_record_count
  FROM raw_records
  WHERE lower(source_db) LIKE '%corespotlight%' OR lower(store_path) LIKE '%index.spotlightv2%'
  GROUP BY source_id,store_guid,source_db
), props AS (
  SELECT source_id,store_guid,source_db,COUNT(*) AS property_count,
         SUM(CASE WHEN property_name LIKE 'kMDItem%' OR property_name LIKE 'CSSearchable%' THEN 1 ELSE 0 END) AS apple_or_md_named_property_count,
         SUM(CASE WHEN property_name LIKE '__native_core_probe%' THEN 1 ELSE 0 END) AS generic_probe_named_property_count
  FROM native_property_dictionary GROUP BY source_id,store_guid,source_db
), cats AS (
  SELECT source_id,store_guid,source_db,COUNT(*) AS category_count
  FROM native_category_dictionary GROUP BY source_id,store_guid,source_db
), idx AS (
  SELECT source_id,store_guid,source_db,
         SUM(index_rows) AS index_rows,
         SUM(value_ref_count) AS index_value_ref_count
  FROM native_index_dictionary_summary GROUP BY source_id,store_guid,source_db
), dbs AS (
  SELECT source_id,store_guid,source_db,
         SUM(CASE WHEN map_id=1 AND status='PARSED' THEN parsed_entries ELSE 0 END) AS dbstr_property_entries,
         SUM(CASE WHEN map_id=2 AND status='PARSED' THEN parsed_entries ELSE 0 END) AS dbstr_category_entries,
         GROUP_CONCAT('dbStr-' || map_id || ':' || status || ':' || parsed_entries || '/' || offset_entries, '; ') AS dbstr_status_summary
  FROM native_dbstr_map_inventory GROUP BY source_id,store_guid,source_db
)
SELECT rr.source_id,rr.store_guid,rr.source_db,rr.raw_record_count,
       COALESCE(props.property_count,0) AS property_count,
       COALESCE(cats.category_count,0) AS category_count,
       COALESCE(idx.index_rows,0) AS index_rows,
       COALESCE(idx.index_value_ref_count,0) AS index_value_ref_count,
       COALESCE(dbs.dbstr_property_entries,0) AS dbstr_property_entries,
       COALESCE(dbs.dbstr_category_entries,0) AS dbstr_category_entries,
       COALESCE(props.apple_or_md_named_property_count,0) AS apple_or_md_named_property_count,
       COALESCE(props.generic_probe_named_property_count,0) AS generic_probe_named_property_count,
       CASE WHEN COALESCE(props.property_count,0)>0 THEN 'PROPERTY_MAP_AVAILABLE'
            WHEN COALESCE(dbs.dbstr_property_entries,0)=0 THEN 'PROPERTY_MAP_MISSING_OR_FAILED'
            ELSE 'PROPERTY_MAP_UNRESOLVED' END AS property_decode_status,
       COALESCE(dbs.dbstr_status_summary,'') AS dbstr_status_summary
FROM rr
LEFT JOIN props ON props.source_id=rr.source_id AND props.store_guid=rr.store_guid AND props.source_db=rr.source_db
LEFT JOIN cats ON cats.source_id=rr.source_id AND cats.store_guid=rr.store_guid AND cats.source_db=rr.source_db
LEFT JOIN idx ON idx.source_id=rr.source_id AND idx.store_guid=rr.store_guid AND idx.source_db=rr.source_db
LEFT JOIN dbs ON dbs.source_id=rr.source_id AND dbs.store_guid=rr.store_guid AND dbs.source_db=rr.source_db;

DROP VIEW IF EXISTS vw_ios_spotlight_apple_field_coverage;
CREATE VIEW vw_ios_spotlight_apple_field_coverage AS
SELECT source_id,store_guid,source_db,field_name,
       CASE
         WHEN field_name IN ('uniqueIdentifier','domainIdentifier','relatedUniqueIdentifier','weakRelatedUniqueIdentifier','targetContentIdentifier','persistentIdentifier') THEN 'APPLE_OBJECT_IDENTITY'
         WHEN field_name IN ('title','displayName','alternateNames','contentDescription','textContent','htmlContentData') THEN 'APPLE_TEXT_CONTENT'
         WHEN field_name IN ('contentURL','path','url','webpageURL','referrerURL') THEN 'APPLE_PATH_OR_URL'
         WHEN field_name IN ('metadataModificationDate','contentCreationDate','contentModificationDate','downloadedDate','lastUsedDate','addedDate','startDate','endDate','dueDate','completionDate','importantDates','timestamp','gpsDateStamp') THEN 'APPLE_DATE_FIELD'
         WHEN field_name IN ('accountHandles','accountIdentifier','authorAddresses','authorEmailAddresses','authorNames','emailAddresses','instantMessageAddresses','phoneNumbers','recipientAddresses','recipientEmailAddresses','recipientNames','mailboxIdentifiers','authors','primaryRecipients','additionalRecipients','hiddenAdditionalRecipients') THEN 'APPLE_MESSAGE_CONTACT_FIELD'
         WHEN field_name IN ('containerDisplayName','containerIdentifier','containerTitle','containerOrder') THEN 'APPLE_CONTAINER_FIELD'
         WHEN field_name LIKE 'kMDItem%' THEN 'APPLE_MDIMPORTER_KMDITEM_FIELD'
         WHEN field_name LIKE '__native_core_probe_string_%' THEN 'GENERIC_NATIVE_PROBE_FIELD'
         ELSE 'APP_PRIVATE_OR_UNKNOWN_FIELD' END AS apple_semantic_group,
       COUNT(*) AS value_row_count,
       COUNT(DISTINCT inode_num || ':' || store_id) AS distinct_record_count,
       MIN(substr(field_value,1,300)) AS sample_value,
       CASE WHEN field_name LIKE '__native_core_probe_string_%' THEN 'Needs dbStr/property-name mapping or app-private decoding.'
            WHEN field_name LIKE 'kMDItem%' THEN 'Matches public macOS Spotlight metadata naming.'
            ELSE 'Classification is based on Apple public Core Spotlight field taxonomy where names match.' END AS interpretation_note
FROM raw_key_values
WHERE lower(source_db) LIKE '%corespotlight%' OR lower(store_path) LIKE '%index.spotlightv2%'
GROUP BY source_id,store_guid,source_db,field_name;
)SQL");

    exec(joinSql({
        R"VSQL27(
DROP VIEW IF EXISTS vw_ios_spotlight_communication_record_review;
CREATE VIEW vw_ios_spotlight_communication_record_review AS
WITH rec AS (
  SELECT raw_record_id,source_id,store_guid,store_path,source_db,inode_num,COALESCE(store_id,'') AS store_id_key,store_id,parent_inode_num,
         file_name,display_name,content_type,last_updated_utc,last_updated_raw,record_state
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
), kv AS (
  SELECT r.raw_record_id,kv.field_name,kv.field_value,lower(COALESCE(kv.field_value,'')) AS v
  FROM rec r
  JOIN raw_key_values kv ON kv.source_id=r.source_id AND kv.store_guid=r.store_guid AND kv.source_db=r.source_db
       AND kv.inode_num=r.inode_num AND COALESCE(kv.store_id,'')=r.store_id_key
  WHERE COALESCE(kv.field_value,'')<>''
), p AS (
  SELECT raw_record_id,
    MAX(CASE WHEN field_name='_kMDItemBundleID' THEN field_value ELSE '' END) AS bundle_id,
    MAX(CASE WHEN field_name='_kMDItemDomainIdentifier' THEN field_value ELSE '' END) AS domain_identifier,
    MAX(CASE WHEN field_name='_kMDItemPersonaID' THEN field_value ELSE '' END) AS persona_id,
    MAX(CASE WHEN field_name='_kMDItemAppEntityTypeIdentifier' THEN field_value ELSE '' END) AS app_entity_type_identifier,
    MAX(CASE WHEN field_name='_kMDItemAppEntityTypeDisplayRepresentationName' THEN field_value ELSE '' END) AS app_entity_type_display_name,
    MAX(CASE WHEN field_name='_kMDItemAppEntityInstanceIdentifier' THEN field_value ELSE '' END) AS app_entity_instance_identifier,
    MAX(CASE WHEN field_name IN ('kMDItemTitle','_kMDItemAppEntityTitle','kMDItemAppEntityTitle') THEN field_value ELSE '' END) AS title,
    MAX(CASE WHEN field_name IN ('_kMDItemAppEntitySubtitle','kMDItemAppEntitySubtitle','_kMDItemSnippet','kMDItemDescription','kMDItemDescription') THEN field_value ELSE '' END) AS app_entity_subtitle,
    MAX(CASE WHEN field_name IN ('kMDItemDisplayName','_ICItemDisplayName','FPFilename') THEN field_value ELSE '' END) AS item_display_name,
    MAX(CASE WHEN field_name IN ('kMDItemDescription','_kMDItemSnippet','kMDItemComment','kMDItemAppEntitySubtitle','_kMDItemAppEntitySubtitle') THEN field_value ELSE '' END) AS description_or_snippet,
    MAX(CASE WHEN field_name IN ('_kMDItemSnippet','kMDItemTextContent','kMDItemTextContentSummary') THEN field_value ELSE '' END) AS snippet,
    MAX(CASE WHEN field_name='kMDItemAccountIdentifier' THEN field_value ELSE '' END) AS account_identifier,
    MAX(CASE WHEN field_name='kMDItemContainerDisplayName' THEN field_value ELSE '' END) AS container_display_name,
    MAX(CASE WHEN field_name='_kMDItemMessageService' THEN field_value ELSE '' END) AS message_service,
    MAX(CASE WHEN field_name IN ('kMDItemPhoneNumbers','com_apple_mobilephone_callbackURL') THEN field_value ELSE '' END) AS phone_or_callback,
    MAX(CASE WHEN field_name='com_apple_mobilephone_callbackURL' THEN field_value ELSE '' END) AS callback_url,
    MAX(CASE WHEN field_name IN ('kMDItemContentURL','kMDItemURL','_kMDItemUserActivityRequiredString','kMDItemRelatedUniqueIdentifier') THEN field_value ELSE '' END) AS url_or_content_reference,
    MAX(CASE WHEN field_name IN ('kMDItemAttachmentPaths','com_apple_mobilesms_groupPhotoPath','com_apple_mobilesms_lpPluginPaths','com_apple_mobilesms_suggested_contact_photo','com_apple_mobilesms_livePhotoComplementPath') THEN field_value ELSE '' END) AS attachment_or_media_path,
    MAX(CASE WHEN field_name IN ('com_apple_mail_messageID','kMDItemEventMessageIdentifier') THEN field_value ELSE '' END) AS message_identifier,
    MAX(CASE WHEN field_name IN ('com_apple_mail_search_indexer_mailbox','kMDItemMailboxIdentifiers') THEN field_value ELSE '' END) AS mailbox_or_thread,
    MAX(CASE WHEN field_name IN ('kMDItemAuthorEmailAddresses','kMDItemRecipientEmailAddresses','kMDItemEmailAddresses','kMDItemAuthors','kMDItemRecipients') THEN field_value ELSE '' END) AS mail_participants,
    MAX(CASE WHEN field_name IN ('kMDItemPhotosSavedFromAppBundleIdentifier','kMDItemPhotosSavedFromAppName') THEN field_value ELSE '' END) AS saved_from_app,
    MAX(CASE WHEN field_name='__spotlight_investigator_text_context' THEN field_value ELSE '' END) AS spotlight_text_context_sample,
    SUM(CASE WHEN field_name='__spotlight_investigator_text_context' THEN 1 ELSE 0 END) AS has_text_context,
    COUNT(*) AS compact_value_count
  FROM kv
  GROUP BY raw_record_id
), d AS (
  SELECT r.raw_record_id,
         MAX(NULLIF(rd.parsed_utc,'')) AS spotlight_date_utc,
         MAX(rd.field_name) AS spotlight_date_source_field,
         COUNT(rd.raw_date_id) AS compact_date_candidate_count
  FROM rec r
  LEFT JOIN raw_date_candidates rd ON rd.source_id=r.source_id AND rd.store_guid=r.store_guid AND rd.source_db=r.source_db
       AND rd.inode_num=r.inode_num AND COALESC)VSQL27",
        R"VSQL27(E(rd.store_id,'')=r.store_id_key
  GROUP BY r.raw_record_id
), classified AS (
  SELECT r.*,p.*,COALESCE(d.spotlight_date_utc,r.last_updated_utc) AS spotlight_date_utc,
         COALESCE(d.spotlight_date_source_field,'Last_Updated') AS spotlight_date_source_field,
         COALESCE(d.compact_date_candidate_count,0) AS compact_date_candidate_count,
         lower(COALESCE(r.content_type,'')) AS ct,
         lower(COALESCE(p.bundle_id,'')) AS lbundle,
         lower(COALESCE(p.domain_identifier,'')) AS ldomain,
         lower(COALESCE(p.spotlight_text_context_sample,'')) AS lctx,
         lower(COALESCE(p.title,'')) AS ltitle,
         lower(COALESCE(p.description_or_snippet,'')) AS ldesc,
         lower(COALESCE(p.app_entity_type_display_name,'')) AS lentity_type
  FROM rec r
  LEFT JOIN p ON p.raw_record_id=r.raw_record_id
  LEFT JOIN d ON d.raw_record_id=r.raw_record_id
), scored AS (
  SELECT *,
    CASE
      WHEN ct LIKE '%email%' OR lbundle LIKE '%mobilemail%' OR lbundle LIKE '%email.searchindexer%' OR COALESCE(message_identifier,'') LIKE '<%@%' OR COALESCE(message_identifier,'') LIKE '<%>' THEN 'APPLE_MAIL_OR_EMAIL'
      WHEN (ct LIKE '%message%' OR lbundle LIKE '%mobilesms%' OR COALESCE(message_service,'')<>'' OR lower(COALESCE(account_identifier,'')) LIKE 'imessage%' OR lower(COALESCE(account_identifier,'')) LIKE 'sms%' OR lower(COALESCE(account_identifier,'')) LIKE 'rcs%')
           AND (lentity_type NOT LIKE '%asset%' OR lbundle LIKE '%mobilesms%' OR ct LIKE '%message%' OR ldomain LIKE 'sms;%' OR ldomain LIKE 'imessage;%') THEN 'APPLE_MESSAGES_SMS_RCS_IMESSAGE'
      WHEN (lower(COALESCE(saved_from_app,'')) LIKE '%mobilesms%' OR lower(COALESCE(saved_from_app,'')) LIKE '%mobile sms%' OR lower(COALESCE(saved_from_app,'')) LIKE '%messages%')
           AND (ct LIKE '%image%' OR ct LIKE '%movie%' OR ct LIKE '%video%' OR lentity_type LIKE '%asset%' OR lbundle LIKE '%mobileslideshow%') THEN 'MEDIA_SAVED_OR_SHARED_FROM_MESSAGES'
      WHEN ct='kspotlightitemtypecall' OR lbundle LIKE '%mobilephone%' OR COALESCE(callback_url,'')<>'' OR lctx LIKE '%com_apple_mobilephone_%' THEN 'PHONE_OR_FACETIME_CALL'
      WHEN lower(COALESCE(saved_from_app,'')) LIKE '%whatsapp%' OR lctx LIKE '%net.whatsapp.whatsapp%' OR lctx LIKE '%chat.whatsapp.com%' OR lctx LIKE '%wa.me/%' THEN 'WHATSAPP_RELATED_SPOTLIGHT_CONTEXT'
      WHEN lctx LIKE '%org.whispersystems.signal%' OR lctx LIKE '%signal.messenger%' OR lctx LIKE '%signal.org/%' THEN 'SIGNAL_RELATED_SPOTLIGHT_CONTEXT'
      WHEN lctx LIKE '%org.telegram%' OR lctx LIKE '%telegram.messenger%' OR lctx LIKE '%t.me/%' OR lctx LIKE '%telegram.me/%' THEN 'TELEGRAM_RELATED_SPOTLIGHT_CONTEXT'
      WHEN ct LIKE '%calendar%' OR lctx LIKE '%calendar%' OR lctx LIKE '%vevent%' THEN 'CALENDAR_OR_INVITATION_CONTEXT'
      WHEN lower(COALESCE(app_entity_type_identifier,'')) LIKE '%contact%' OR lower(COALESCE(domain_identifier,'')) LIKE '%contact%' OR COALESCE(phone_or_callback,'')<>'' THEN 'CONTACT_OR_ACCOUNT_CONTEXT'
      WHEN COALESCE(url_or_content_reference,'')<>'' OR lctx LIKE '%http://%' OR lctx LIKE '%https://%' OR lctx LIKE '%www.%' THEN 'URL_OR_WEB_CONTEXT'
      ELSE 'OTHER_SPOTLIGHT_CONTEXT'
    END AS communication_context_type
  FROM classified
)
SELECT raw_record_id,source_id,store_guid,source_db,inode_num AS spotlight_inode_or_object_id,store_id AS spotlight_store_id,parent_inode_num,
       spotlight_date_utc,spotlight_date_source_field,compact_date_candidate_count,last_updated_utc,
       communication_context_type,
       CASE
         WHEN communication_context_type IN ('APPLE_MESSAGES_SMS_RCS_IMESSAGE','APPLE_MAIL_OR_EMAIL','PHONE_OR_FACETIME_CALL') THEN 'HIGH_COMMUNICATION_REVIEW_VALUE'
         WHEN communication_context_type='MEDIA_SAVED_OR_SHARED_FROM_MESSAGES' THEN 'HIGH_MESSAGE_MEDIA_REVIEW_VALUE'
         WHEN communication_context_type LIKE '%WHATSAPP%' OR communication_context_type LIKE '%SIGNAL%' OR communication_context_type LIKE '%TELEGRAM%' THEN 'HIGH_APP_COMMUNICATION_CONTEXT_VALUE'
         WHEN communication_context_type IN ('CONTACT_OR_ACCOUNT_CONTEXT','URL_OR_WEB_CONTEXT','CALENDAR_OR_INVITATION_CONTEXT') THEN 'MEDIUM_COMMUNICATION_REVIEW_VALUE'
         ELSE 'LOW_CONTEXT_VALUE'
       END AS review_priority,
       CASE
         WHEN communication_context_type='APPLE_MESSAGES_SMS_RCS_IMESSAGE' THEN 1
         WHEN communication_context_type='APPLE_MAIL_OR_EMAIL' THEN 2
         WHEN communication_context_type='PHONE_OR_FACETIME_CALL' THEN 3
         WHEN communication_context_type='MEDIA_SAVED_OR_SHARED_FROM_MESSAGES' THEN 4
         WHEN communication_context_type LIKE '%WHATSAPP%' OR communication_context_type LIKE '%SIGNAL%' OR communication_context_type LIKE '%TELEGRAM%' THEN 5
         WHEN communication_context_type='CONTACT_OR_ACCOUNT_CONTEXT' THEN 6
         WHEN communication_context_type='URL_OR_WEB_CONTEXT' THEN 7
         WHEN communication_context_type='CALENDAR_OR_INVITATION_CONTEXT' THEN 8
         ELSE 50
       END AS review_priority_sort,
       content_type,bundle_id,domain_identifier,persona_id,app_entity_type_identifier,app_entity_type_display_name,app_entity_instance_identifier,
       CASE
         WHEN domain_identifier LIKE 'SMS;%;%' THEN substr(domain_identifier,5 + instr(substr(domain_identifier,5),';'))
         WHEN domain_identifier LIKE 'iMessage;%;%' THEN substr(domain_identifier,10 + instr(substr(domain_identifier,10),';'))
         WHEN domain_identifier='chatDomain' THEN 'conversation_index_record'
         ELSE ''
       END AS message_domain_handle_or_chat,
       COALESCE(NULLIF(title,''),NULLIF(item_display)VSQL27",
        R"VSQL27(_name,''),NULLIF(display_name,''),NULLIF(file_name,'')) AS best_title_or_name,
       substr(COALESCE(NULLIF(title,''),NULLIF(description_or_snippet,''),NULLIF(snippet,''),NULLIF(spotlight_text_context_sample,'')),1,2500) AS investigator_visible_text,
       substr(description_or_snippet,1,1800) AS description_or_snippet,
       substr(snippet,1,1200) AS snippet,
       account_identifier,container_display_name,message_service,phone_or_callback,callback_url,url_or_content_reference,attachment_or_media_path,message_identifier,mailbox_or_thread,mail_participants,saved_from_app,
       compact_value_count,has_text_context,substr(spotlight_text_context_sample,1,2500) AS spotlight_text_context_sample,
       CASE
         WHEN communication_context_type IN ('APPLE_MESSAGES_SMS_RCS_IMESSAGE','APPLE_MAIL_OR_EMAIL','PHONE_OR_FACETIME_CALL') THEN 'Explicit Apple communication content type/bundle/service/callback evidence recovered from Spotlight.'
         WHEN communication_context_type='MEDIA_SAVED_OR_SHARED_FROM_MESSAGES' THEN 'Media/photo asset indexed by Spotlight with Photos saved-from-app context pointing to Messages/MobileSMS. Treat as message-related media context and corroborate with FFS/app DB where available.'
         WHEN communication_context_type LIKE '%WHATSAPP%' OR communication_context_type LIKE '%SIGNAL%' OR communication_context_type LIKE '%TELEGRAM%' THEN 'Chat-app context found in Spotlight text/app reference; treat as Spotlight evidence and corroborate with app DB if support parsing is enabled.'
         ELSE 'Communication-adjacent Spotlight context retained for triage; verify with raw locators before reporting.'
       END AS interpretation_note,
       'raw_records.raw_record_id=' || raw_record_id || '; source_db=' || COALESCE(source_db,'') || '; inode_or_object=' || COALESCE(inode_num,'') || '; store_id=' || COALESCE(store_id,'') AS validation_locator
FROM scored
WHERE communication_context_type<>'OTHER_SPOTLIGHT_CONTEXT';

DROP VIEW IF EXISTS vw_ios_spotlight_communication_summary;
CREATE VIEW vw_ios_spotlight_communication_summary AS
SELECT communication_context_type,review_priority,review_priority_sort,bundle_id,domain_identifier,message_service,content_type,
       COUNT(*) AS spotlight_record_count,
       COUNT(DISTINCT spotlight_inode_or_object_id || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       COUNT(NULLIF(best_title_or_name,'')) AS rows_with_title_or_name,
       COUNT(NULLIF(investigator_visible_text,'')) AS rows_with_investigator_visible_text,
       COUNT(NULLIF(description_or_snippet,'')) AS rows_with_description_or_snippet,
       COUNT(NULLIF(attachment_or_media_path,'')) AS rows_with_attachment_or_media_path,
       COUNT(NULLIF(url_or_content_reference,'')) AS rows_with_url_or_content_reference,
       COUNT(NULLIF(phone_or_callback,'')) AS rows_with_phone_or_callback,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       substr(MIN(COALESCE(NULLIF(investigator_visible_text,''),NULLIF(best_title_or_name,''),NULLIF(description_or_snippet,''),NULLIF(spotlight_text_context_sample,''))),1,1000) AS min_context_sample,
       substr(MAX(COALESCE(NULLIF(investigator_visible_text,''),NULLIF(best_title_or_name,''),NULLIF(description_or_snippet,''),NULLIF(spotlight_text_context_sample,''))),1,1000) AS max_context_sample,
       'Record-centric summary of communication-relevant Spotlight/CoreSpotlight records. Counts are Spotlight records, not live app database records.' AS interpretation_note
FROM vw_ios_spotlight_communication_record_review
GROUP BY communication_context_type,review_priority,review_priority_sort,bundle_id,domain_identifier,message_service,content_type;

DROP VIEW IF EXISTS vw_ios_spotlight_attachment_reference_review;
CREATE VIEW vw_ios_spotlight_attachment_reference_review AS
SELECT raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       spotlight_date_utc,communication_context_type,review_priority,content_type,bundle_id,domain_identifier,
       best_title_or_name,investigator_visible_text,description_or_snippet,attachment_or_media_path,url_or_content_reference,spotlight_text_context_sample,
       CASE WHEN COALESCE(attachment_or_media_path,'')<>'' THEN 'EXPLICIT_ATTACHMENT_OR_MEDIA_PATH_FIELD'
            WHEN COALESCE(url_or_content_reference,'')<>'' THEN 'URL_OR_CONTENT_REFERENCE_FIELD'
            ELSE 'TEXT_CONTEXT_ONLY' END AS attachment_reference_basis,
       validation_locator,
       'Attachment/media-focused subset of communication Spotlight review. Missing/present status is available in Missing From FFS views where path lookup can be resolved.' AS interpretation_note
FROM vw_ios_spotlight_communication_record_review
WHERE COALESCE(attachment_or_media_path,'')<>'' OR COALESCE(url_or_content_reference,'')<>'';

DROP VIEW IF EXISTS vw_ios_spotlight_message_text_review;
CREATE VIEW vw_ios_spotlight_message_text_review AS
SELECT raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       spotlight_date_utc,spotlight_date_source_field,communication_context_type,review_priority,review_priority_sort,content_type,bundle_id,domain_identifier,
       message_domain_handle_or_chat,best_title_or_name,investigator_visible_text,description_or_snippet,snippet,account_identifier,message_service,
       phone_or_callback,callback_url,url_or_content_reference,attachment_or_media_path,message_identifier,mailbox_or_thread,mail_participants,
       spotlight_text_context_sample,interpretation_note,validation_locator
FROM vw_ios_spotlight_communication_record_review
WHERE communication_context_type IN ('APPLE_MESSAGES_SMS_RCS_IMESSAGE','APPLE_MAIL_OR_EMAIL','PHONE_OR_FACETIME_CALL')
   OR communication_context_type LIKE '%WHATSAPP%'
   OR communication_context_type LIKE '%SIGNAL%'
   OR communication_context_type LIKE '%TELEGRAM%';

DROP VIEW IF EXISTS vw_ios_spotlight_message_media_review;
CREATE VIEW vw_ios_spotlight_message_media_review AS
SELECT raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       spotlight_date_utc,spotlight_date_source_field,communication_context_type,review_priority,review_priority_sort,content_type,bundle_id,domain_identifier,
       saved_from_app,best_title_or_name,investigator_visible_text,description_or_snippet,attachment_or_media_path,url_or_content_reference,
       spotlight_text_context_sample,validation_locator,
       'Media/photo/video Spotlight record with message-related saved-from-app or attachment context; review as message-adjacent media, not as a direct message body unless corroborated.' AS interpretation_note
FROM vw_ios_spotlight_communication_record_review
WHERE communication_context_type='MEDIA_SAVED_OR_SHARED_FROM_MESSAGES'
   OR COALESCE(saved_from_app,'')<>''
   OR COALESCE(attachment_or_media_path,'')<>'';
)VSQL27"
    }));


    // V0_9_32: defensibility and investigator-review views. Keep these in CaseDatabase so
    // the GUI consumes schema owned by the database layer instead of duplicating SQL.
    exec(joinSql({R"VSQL29(
DROP VIEW IF EXISTS vw_case_provenance_summary;
CREATE VIEW vw_case_provenance_summary AS
SELECT 'case_info' AS provenance_scope,
       key AS provenance_key,
       value AS provenance_value,
       'Case metadata recorded at parse/export time. Use app_version, created_utc, skip_container_hash, and guardrail settings when reporting parser provenance.' AS interpretation_note
FROM case_info
UNION ALL
SELECT 'evidence_source' AS provenance_scope,
       source_id || ':input_path' AS provenance_key,
       input_path AS provenance_value,
       'Evidence-source input path recorded in the case database. Hash may be deferred during reuse-cache development runs when skip_container_hash=true.' AS interpretation_note
FROM evidence_sources
UNION ALL
SELECT 'evidence_source' AS provenance_scope,
       source_id || ':source_kind' AS provenance_key,
       source_kind AS provenance_value,
       'Evidence-source kind/profile recorded for forensic provenance.' AS interpretation_note
FROM evidence_sources;

DROP VIEW IF EXISTS vw_parser_diagnostics_summary;
CREATE VIEW vw_parser_diagnostics_summary AS
WITH failure_buckets AS (
  SELECT 'raw_failures' AS diagnostic_source,
         COALESCE(phase,'(blank)') AS diagnostic_category,
         COUNT(*) AS diagnostic_count,
         MIN(created_utc) AS first_seen_utc,
         MAX(created_utc) AS last_seen_utc,
         substr(MIN(message),1,1200) AS sample_min_message,
         substr(MAX(message),1,1200) AS sample_max_message
  FROM raw_failures
  GROUP BY COALESCE(phase,'(blank)')
), partial_decode AS (
  SELECT 'partial_decode_error' AS diagnostic_source,
         'raw_key_values.__decode_error' AS diagnostic_category,
         COUNT(*) AS diagnostic_count,
         '' AS first_seen_utc,
         '' AS last_seen_utc,
         substr(MIN(field_value),1,1200) AS sample_min_message,
         substr(MAX(field_value),1,1200) AS sample_max_message
  FROM raw_key_values
  WHERE field_name='__decode_error'
), attempts AS (
  SELECT 'native_decode_attempts' AS diagnostic_source,
         COALESCE(status,'(blank)') AS diagnostic_category,
         COUNT(*) AS diagnostic_count,
         MIN(started_utc) AS first_seen_utc,
         MAX(finished_utc) AS last_seen_utc,
         substr(MIN(message),1,1200) AS sample_min_message,
         substr(MAX(message),1,1200) AS sample_max_message
  FROM native_decode_attempts
  GROUP BY COALESCE(status,'(blank)')
)
SELECT diagnostic_source,diagnostic_category,diagnostic_count,first_seen_utc,last_seen_utc,sample_min_message,sample_max_message,
       CASE WHEN diagnostic_source='raw_failures' THEN 'Parser failure bucket captured during native Store-V2/CoreSpotlight parsing. Investigators should treat non-zero values as visible unparsed/corrupt/unsupported data indicators.'
            WHEN diagnostic_source='partial_decode_error' THEN 'Partial item decode errors persisted as compact error rows. Non-zero counts identify native property/value decoding targets.'
            ELSE 'Native decode attempt status by store/source database.' END AS interpretation_note
FROM failure_buckets
UNION ALL SELECT diagnostic_source,diagnostic_category,diagnostic_count,first_seen_utc,last_seen_utc,sample_min_message,sample_max_message,
       'Partial item decode errors persisted as compact error rows. Non-zero counts identify native property/value decoding targets.' FROM partial_decode
UNION ALL SELECT diagnostic_source,diagnostic_category,diagnostic_count,first_seen_utc,last_seen_utc,sample_min_message,sample_max_message,
       'Native decode attempt status by store/source database.' FROM attempts;

)VSQL29", R"VSQL29(
DROP VIEW IF EXISTS vw_ios_spotlight_message_body_review;
CREATE VIEW vw_ios_spotlight_message_body_review AS
WITH base AS (
  SELECT *,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'kMDItemAppEntityTitle=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'kMDItemAppEntityTitle=') + length('kMDItemAppEntityTitle='))
              ELSE '' END AS title_tail,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'kMDItemAppEntitySubtitle=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'kMDItemAppEntitySubtitle=') + length('kMDItemAppEntitySubtitle='))
              ELSE '' END AS subtitle_tail,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'com_apple_mobilesms_suggested_contact_name=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'com_apple_mobilesms_suggested_contact_name=') + length('com_apple_mobilesms_suggested_contact_name='))
              ELSE '' END AS contact_tail
  FROM vw_ios_spotlight_message_text_review
), extracted AS (
  SELECT *,
         trim(CASE WHEN title_tail<>'' AND instr(title_tail,' |')>0 THEN substr(title_tail,1,instr(title_tail,' |')-1) ELSE title_tail END) AS extracted_title,
         trim(CASE WHEN subtitle_tail<>'' AND instr(subtitle_tail,' |')>0 THEN substr(subtitle_tail,1,instr(subtitle_tail,' |')-1) ELSE subtitle_tail END) AS extracted_subtitle,
         trim(CASE WHEN contact_tail<>'' AND instr(contact_tail,' |')>0 THEN substr(contact_tail,1,instr(contact_tail,' |')-1) ELSE contact_tail END) AS suggested_contact_name
  FROM base
)
SELECT raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       spotlight_date_utc,spotlight_date_source_field,communication_context_type,review_priority,review_priority_sort,content_type,bundle_id,domain_identifier,
       message_domain_handle_or_chat,suggested_contact_name,extracted_subtitle AS conversation_or_thread_title,extracted_title AS extracted_message_text_or_subject,
       CASE
         WHEN communication_context_type='APPLE_MESSAGES_SMS_RCS_IMESSAGE' AND extracted_title<>'' THEN 'DIRECT_APPLE_MESSAGE_TEXT_OR_REACTION'
         WHEN communication_context_type='APPLE_MESSAGES_SMS_RCS_IMESSAGE' AND domain_identifier='chatDomain' THEN 'CONVERSATION_INDEX_RECORD_NO_BODY'
         WHEN communication_context_type='APPLE_MAIL_OR_EMAIL' AND extracted_title<>'' THEN 'MAIL_SUBJECT_OR_TEXT_CONTEXT'
         WHEN communication_context_type='PHONE_OR_FACETIME_CALL' THEN 'CALL_OR_FACETIME_CONTEXT'
         WHEN communication_context_type LIKE '%WHATSAPP%' OR communication_context_type LIKE '%SIGNAL%' OR communication_context_type LIKE '%TELEGRAM%' THEN 'THIRD_PARTY_CHAT_APP_CONTEXT'
         ELSE 'COMMUNICATION_REVIEW_CONTEXT'
       END AS body_review_bucket,
       CASE
         WHEN message_domain_handle_or_chat GLOB '[0-9][0-9][0-9][0-9][0-9]' OR message_domain_handle_or_chat GLOB '[0-9][0-9][0-9][0-9]' OR LOWER(extracted_title) LIKE '%attn.tv/%' OR LOWER(extracted_title) LIKE '%stop to opt out%' THEN 'LIKELY_MARKETING_OR_SHORT_CODE'
         WHEN domain_identifier='chatDomain' AND extracted_title='' THEN 'CONVERSATION_PLACEHOLDER_OR_INDEX_ROW'
         ELSE 'USER_REVIEW_CANDIDATE'
       END AS noise_hint,
       investigator_visible_text,description_or_snippet,snippet,account_identifier,message_service,phone_or_callback,callback_url,url_or_content_reference,attachment_or_media_path,message_identifier,mailbox_or_thread,mail_participants,
       spotlight_text_context_sample,validation_locator,
       'V0_9_32 extracts message body/subject, conversation title, and suggested contact from compact Spotlight text context. This is Spotlight index evidence; corroborate against Messages/Mail/app DB records when support parsing is enabled.' AS interpretation_note
FROM extracted;

DROP VIEW IF EXISTS vw_ios_spotlight_user_focus_message_review;
CREATE VIEW vw_ios_spotlight_user_focus_message_review AS
SELECT *
FROM vw_ios_spotlight_message_body_review
WHERE body_review_bucket NOT IN ('CONVERSATION_INDEX_RECORD_NO_BODY')
  AND noise_hint='USER_REVIEW_CANDIDATE'
  AND COALESCE(extracted_message_text_or_subject,'')<>'';

DROP VIEW IF EXISTS vw_ios_spotlight_message_contact_summary;
CREATE VIEW vw_ios_spotlight_message_contact_summary AS
SELECT communication_context_type,bundle_id,domain_identifier,message_domain_handle_or_chat,suggested_contact_name,conversation_or_thread_title,noise_hint,
       COUNT(*) AS spotlight_record_count,
       COUNT(DISTINCT spotlight_inode_or_object_id || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       SUM(CASE WHEN COALESCE(extracted_message_text_or_subject,'')<>'' THEN 1 ELSE 0 END) AS rows_with_extracted_message_text,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       substr(MIN(NULLIF(extracted_message_text_or_subject,'')),1,1200) AS min_message_sample,
       substr(MAX(NULLIF(extracted_message_text_or_subject,'')),1,1200) AS max_message_sample,
       'Contact/thread summary built from Spotlight message-domain handles and same-record text context; counts are Spotlight index rows, not live app DB rows.' AS interpretation_note
FROM vw_ios_spotlight_message_body_review
GROUP BY communication_context_type,bundle_id,domain_identifier,message_domain_handle_or_chat,suggested_contact_name,conversation_or_thread_title,noise_hint;
)VSQL29", R"VSQL29(
DROP VIEW IF EXISTS vw_ios_spotlight_noise_reduction_summary;
CREATE VIEW vw_ios_spotlight_noise_reduction_summary AS
SELECT noise_hint,body_review_bucket,communication_context_type,
       COUNT(*) AS spotlight_record_count,
       SUM(CASE WHEN COALESCE(extracted_message_text_or_subject,'')<>'' THEN 1 ELSE 0 END) AS rows_with_extracted_text,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       substr(MIN(COALESCE(NULLIF(extracted_message_text_or_subject,''),NULLIF(spotlight_text_context_sample,''))),1,1200) AS min_sample,
       substr(MAX(COALESCE(NULLIF(extracted_message_text_or_subject,''),NULLIF(spotlight_text_context_sample,''))),1,1200) AS max_sample,
       'Noise reduction summary is non-destructive. It helps triage conversation placeholders and likely marketing/short-code rows while keeping all Spotlight evidence available in the full review views.' AS interpretation_note
FROM vw_ios_spotlight_message_body_review
GROUP BY noise_hint,body_review_bucket,communication_context_type;

DROP VIEW IF EXISTS vw_ios_spotlight_normalized_timeline;
CREATE VIEW vw_ios_spotlight_normalized_timeline AS
SELECT raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       spotlight_date_utc AS event_time_utc,
       'Spotlight Index/Record Date' AS event_type,
       spotlight_date_source_field AS source_field,
       communication_context_type AS review_category,
       COALESCE(NULLIF(extracted_message_text_or_subject,''),NULLIF(conversation_or_thread_title,''),NULLIF(investigator_visible_text,''),NULLIF(spotlight_text_context_sample,'')) AS event_summary,
       message_domain_handle_or_chat AS contact_or_thread,
       bundle_id,content_type,validation_locator,
       CASE WHEN spotlight_date_utc>(SELECT COALESCE(MAX(value),'') FROM case_info WHERE key='created_utc') THEN 'DATE_AFTER_PARSE_RUN_REVIEW'
            WHEN spotlight_date_utc<>'' AND spotlight_date_utc<'2010-01-01T00:00:00Z' THEN 'UNUSUALLY_OLD_DATE_REVIEW'
            ELSE 'NO_BASIC_DATE_ANOMALY' END AS date_anomaly_flag,
       'Normalized iOS Spotlight timeline row. Event type reflects Spotlight/index date provenance, not necessarily user action unless supported by message/content context.' AS interpretation_note
FROM vw_ios_spotlight_message_body_review
WHERE COALESCE(spotlight_date_utc,'')<>''
UNION ALL
SELECT raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       spotlight_date_utc AS event_time_utc,
       'Spotlight Message Media/Attachment Reference' AS event_type,
       spotlight_date_source_field AS source_field,
       communication_context_type AS review_category,
       COALESCE(NULLIF(investigator_visible_text,''),NULLIF(best_title_or_name,''),NULLIF(attachment_or_media_path,''),NULLIF(url_or_content_reference,''),NULLIF(spotlight_text_context_sample,'')) AS event_summary,
       domain_identifier AS contact_or_thread,
       bundle_id,content_type,validation_locator,
       CASE WHEN spotlight_date_utc>(SELECT COALESCE(MAX(value),'') FROM case_info WHERE key='created_utc') THEN 'DATE_AFTER_PARSE_RUN_REVIEW'
            WHEN spotlight_date_utc<>'' AND spotlight_date_utc<'2010-01-01T00:00:00Z' THEN 'UNUSUALLY_OLD_DATE_REVIEW'
            ELSE 'NO_BASIC_DATE_ANOMALY' END AS date_anomaly_flag,
       'Normalized iOS Spotlight media/attachment timeline row. Review attachment/path context before treating as message content.' AS interpretation_note
FROM vw_ios_spotlight_message_media_review
WHERE COALESCE(spotlight_date_utc,'')<>'';

DROP VIEW IF EXISTS vw_ios_spotlight_timeline_anomaly_summary;
CREATE VIEW vw_ios_spotlight_timeline_anomaly_summary AS
SELECT date_anomaly_flag,event_type,review_category,
       COUNT(*) AS timeline_row_count,
       COUNT(DISTINCT spotlight_inode_or_object_id || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       MIN(event_time_utc) AS earliest_event_utc,
       MAX(event_time_utc) AS latest_event_utc,
       substr(MIN(event_summary),1,1200) AS min_event_sample,
       substr(MAX(event_summary),1,1200) AS max_event_sample,
       'Basic anomaly summary for normalized iOS Spotlight timeline. Flags are triage indicators and require source-field validation.' AS interpretation_note

FROM vw_ios_spotlight_normalized_timeline
GROUP BY date_anomaly_flag,event_type,review_category;
)VSQL29"}));

    // V0_9_32: documentation/consolidation support plus more useful compact message/body extraction
    // and visible parser diagnostics detail. These override V0_9_29 views without changing raw persistence.
    exec(joinSql({R"VSQL30(
DROP VIEW IF EXISTS vw_ios_spotlight_message_body_review;
CREATE VIEW vw_ios_spotlight_message_body_review AS
WITH base AS (
  SELECT *,
         COALESCE(spotlight_text_context_sample,'') AS ctx,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'kMDItemAppEntityTitle=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'kMDItemAppEntityTitle=') + length('kMDItemAppEntityTitle=')) ELSE '' END AS app_title_tail,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'_kMDItemAppEntityTitle=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'_kMDItemAppEntityTitle=') + length('_kMDItemAppEntityTitle=')) ELSE '' END AS app_title2_tail,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'kMDItemTitle=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'kMDItemTitle=') + length('kMDItemTitle=')) ELSE '' END AS kmd_title_tail,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'_kMDItemSnippet=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'_kMDItemSnippet=') + length('_kMDItemSnippet=')) ELSE '' END AS snippet_tail,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'kMDItemDescription=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'kMDItemDescription=') + length('kMDItemDescription=')) ELSE '' END AS desc_tail,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'kMDItemAppEntitySubtitle=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'kMDItemAppEntitySubtitle=') + length('kMDItemAppEntitySubtitle=')) ELSE '' END AS subtitle_tail,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'_kMDItemAppEntitySubtitle=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'_kMDItemAppEntitySubtitle=') + length('_kMDItemAppEntitySubtitle=')) ELSE '' END AS subtitle2_tail,
         CASE WHEN instr(COALESCE(spotlight_text_context_sample,''),'com_apple_mobilesms_suggested_contact_name=')>0
              THEN substr(COALESCE(spotlight_text_context_sample,''), instr(COALESCE(spotlight_text_context_sample,''),'com_apple_mobilesms_suggested_contact_name=') + length('com_apple_mobilesms_suggested_contact_name=')) ELSE '' END AS contact_tail
  FROM vw_ios_spotlight_message_text_review
), cut AS (
  SELECT *,
         trim(CASE WHEN app_title_tail<>'' AND instr(app_title_tail,' |')>0 THEN substr(app_title_tail,1,instr(app_title_tail,' |')-1) ELSE app_title_tail END) AS c_app_title,
         trim(CASE WHEN app_title2_tail<>'' AND instr(app_title2_tail,' |')>0 THEN substr(app_title2_tail,1,instr(app_title2_tail,' |')-1) ELSE app_title2_tail END) AS c_app_title2,
         trim(CASE WHEN kmd_title_tail<>'' AND instr(kmd_title_tail,' |')>0 THEN substr(kmd_title_tail,1,instr(kmd_title_tail,' |')-1) ELSE kmd_title_tail END) AS c_kmd_title,
         trim(CASE WHEN snippet_tail<>'' AND instr(snippet_tail,' |')>0 THEN substr(snippet_tail,1,instr(snippet_tail,' |')-1) ELSE snippet_tail END) AS c_snippet,
         trim(CASE WHEN desc_tail<>'' AND instr(desc_tail,' |')>0 THEN substr(desc_tail,1,instr(desc_tail,' |')-1) ELSE desc_tail END) AS c_desc,
         trim(CASE WHEN subtitle_tail<>'' AND instr(subtitle_tail,' |')>0 THEN substr(subtitle_tail,1,instr(subtitle_tail,' |')-1) ELSE subtitle_tail END) AS c_subtitle,
         trim(CASE WHEN subtitle2_tail<>'' AND instr(subtitle2_tail,' |')>0 THEN substr(subtitle2_tail,1,instr(subtitle2_tail,' |')-1) ELSE subtitle2_tail END) AS c_subtitle2,
         trim(CASE WHEN contact_tail<>'' AND instr(contact_tail,' |')>0 THEN substr(contact_tail,1,instr(contact_tail,' |')-1) ELSE contact_tail END) AS c_contact
  FROM base
), extracted AS (
  SELECT *,
         COALESCE(NULLIF(c_app_title,''),NULLIF(c_app_title2,''),NULLIF(c_kmd_title,''),NULLIF(best_title_or_name,''),NULLIF(investigator_visible_text,'')) AS extracted_subject_or_body,
         COALESCE(NULLIF(c_subtitle,''),NULLIF(c_subtitle2,''),NULLIF(c_desc,''),NULLIF(c_snippet,''),NULLIF(description_or_snippet,''),NULLIF(snippet,'')) AS extracted_supporting_text,
         c_contact AS suggested_contact_name_v30
  FROM cut
)
SELECT raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       spotlight_date_utc,spotlight_date_source_field,communication_context_type,review_priority,review_priority_sort,content_type,bundle_id,domain_identifier,
       message_domain_handle_or_chat,suggested_contact_name_v30 AS suggested_contact_name,extracted_supporting_text AS conversation_or_thread_title,extracted_subject_or_body AS extracted_message_text_or_subject,
       CASE
         WHEN communication_context_type='APPLE_MESSAGES_SMS_RCS_IMESSAGE' AND COALESCE(extracted_subject_or_body,'')<>'' THEN 'DIRECT_APPLE_MESSAGE_TEXT_OR_REACTION'
)VSQL30", R"VSQL30(
         WHEN communication_context_type='APPLE_MESSAGES_SMS_RCS_IMESSAGE' AND domain_identifier='chatDomain' THEN 'CONVERSATION_INDEX_RECORD_NO_BODY'
         WHEN communication_context_type='APPLE_MAIL_OR_EMAIL' AND COALESCE(extracted_subject_or_body,extracted_supporting_text,'')<>'' THEN 'MAIL_SUBJECT_SNIPPET_OR_TEXT_CONTEXT'
         WHEN communication_context_type='PHONE_OR_FACETIME_CALL' THEN 'CALL_OR_FACETIME_CONTEXT'
         WHEN communication_context_type LIKE '%WHATSAPP%' OR communication_context_type LIKE '%SIGNAL%' OR communication_context_type LIKE '%TELEGRAM%' THEN 'THIRD_PARTY_CHAT_APP_CONTEXT'
         ELSE 'COMMUNICATION_REVIEW_CONTEXT'
       END AS body_review_bucket,
       CASE
         WHEN message_domain_handle_or_chat GLOB '[0-9][0-9][0-9][0-9][0-9]' OR message_domain_handle_or_chat GLOB '[0-9][0-9][0-9][0-9]' OR LOWER(extracted_subject_or_body) LIKE '%attn.tv/%' OR LOWER(extracted_subject_or_body) LIKE '%stop to opt out%' THEN 'LIKELY_MARKETING_OR_SHORT_CODE'
         WHEN domain_identifier='chatDomain' AND COALESCE(extracted_subject_or_body,'')='' THEN 'CONVERSATION_PLACEHOLDER_OR_INDEX_ROW'
         ELSE 'USER_REVIEW_CANDIDATE'
       END AS noise_hint,
       investigator_visible_text,description_or_snippet,snippet,account_identifier,message_service,phone_or_callback,callback_url,url_or_content_reference,attachment_or_media_path,message_identifier,mailbox_or_thread,mail_participants,
       spotlight_text_context_sample,validation_locator,
       'V0_9_32 extracts message/mail body, subject, snippet, and thread/contact context from compact same-record Spotlight text. This is Spotlight index evidence; corroborate against app DB/FFS where available.' AS interpretation_note
FROM extracted;

DROP VIEW IF EXISTS vw_ios_spotlight_user_focus_message_review;
CREATE VIEW vw_ios_spotlight_user_focus_message_review AS
SELECT *
FROM vw_ios_spotlight_message_body_review
WHERE body_review_bucket NOT IN ('CONVERSATION_INDEX_RECORD_NO_BODY')
  AND noise_hint='USER_REVIEW_CANDIDATE'
  AND (COALESCE(extracted_message_text_or_subject,'')<>'' OR COALESCE(conversation_or_thread_title,'')<>'');

DROP VIEW IF EXISTS vw_ios_spotlight_message_contact_summary;
CREATE VIEW vw_ios_spotlight_message_contact_summary AS
SELECT communication_context_type,bundle_id,domain_identifier,message_domain_handle_or_chat,suggested_contact_name,conversation_or_thread_title,noise_hint,
       COUNT(*) AS spotlight_record_count,
       COUNT(DISTINCT spotlight_inode_or_object_id || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       SUM(CASE WHEN COALESCE(extracted_message_text_or_subject,'')<>'' THEN 1 ELSE 0 END) AS rows_with_extracted_message_text,
       SUM(CASE WHEN COALESCE(conversation_or_thread_title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_thread_or_snippet_text,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       substr(MIN(NULLIF(extracted_message_text_or_subject,'')),1,1200) AS min_message_sample,
       substr(MAX(NULLIF(extracted_message_text_or_subject,'')),1,1200) AS max_message_sample,
       'Contact/thread summary built from Spotlight message-domain handles and same-record text context; counts are Spotlight index rows, not live app DB rows.' AS interpretation_note
FROM vw_ios_spotlight_message_body_review
GROUP BY communication_context_type,bundle_id,domain_identifier,message_domain_handle_or_chat,suggested_contact_name,conversation_or_thread_title,noise_hint;

DROP VIEW IF EXISTS vw_ios_spotlight_noise_reduction_summary;
CREATE VIEW vw_ios_spotlight_noise_reduction_summary AS
SELECT noise_hint,body_review_bucket,communication_context_type,
       COUNT(*) AS spotlight_record_count,
       SUM(CASE WHEN COALESCE(extracted_message_text_or_subject,'')<>'' THEN 1 ELSE 0 END) AS rows_with_extracted_text,
       SUM(CASE WHEN COALESCE(conversation_or_thread_title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_supporting_text,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       substr(MIN(COALESCE(NULLIF(extracted_message_text_or_subject,''),NULLIF(conversation_or_thread_title,''),NULLIF(spotlight_text_context_sample,''))),1,1200) AS min_sample,
       substr(MAX(COALESCE(NULLIF(extracted_message_text_or_subject,''),NULLIF(conversation_or_thread_title,''),NULLIF(spotlight_text_context_sample,''))),1,1200) AS max_sample,
       'Noise reduction summary is non-destructive. It helps triage conversation placeholders and likely marketing/short-code rows while keeping all Spotlight evidence available in the full review views.' AS interpretation_note
FROM vw_ios_spotlight_message_body_review
GROUP BY noise_hint,body_review_bucket,communication_context_type;

DROP VIEW IF EXISTS vw_parser_diagnostics_detail_sample;
CREATE VIEW vw_parser_diagnostics_detail_sample AS
SELECT 'raw_failures' AS diagnostic_source,
       failure_id AS diagnostic_row_id,
       created_utc,
       COALESCE(phase,'') AS diagnostic_category,
       COALESCE(store_guid,'') AS store_guid,
       COALESCE(source_db,'') AS source_db,
       '' AS spotlight_record_locator,
       substr(COALESCE(message,''),1,2000) AS diagnostic_message,
       'Parser/native decode failure row. This is visible unparsed/unsupported/corrupt evidence context; use summary counts before treating absence of parsed values as absence of evidence.' AS interpretation_note
FROM raw_failures
UNION ALL
SELECT 'partial_decode_error' AS diagnostic_source,
       raw_kv_id AS diagnostic_row_id,
       '' AS created_utc,
       field_name AS diagnostic_category,
       COALESCE(store_guid,'') AS store_guid,
       COALESCE(source_db,'') AS source_db,
       'inode_or_object=' || COALESCE(inode_num,'') || '; store_id=' || COALESCE(store_id,'') AS spotlight_record_locator,
       substr(COALESCE(field_value,''),1,2000) AS diagnostic_message,
       'Partial property/value decode error retained from compact raw_key_values. This identifies records that need parser improvement or bounded diagnostic reparse.' AS interpretation_note
FROM raw_key_values
WHERE field_name='__decode_error';
)VSQL30"}));


    // V0_9_32: compact review-quality refinements. Keep normal investigator exports small
    // while adding defensible summaries and interoperability surfaces.
    exec(joinSql({R"VSQL31(
DROP VIEW IF EXISTS vw_ios_spotlight_message_contact_summary;
CREATE VIEW vw_ios_spotlight_message_contact_summary AS
WITH classified AS (
  SELECT *,
         CASE
           WHEN message_domain_handle_or_chat GLOB '[0-9][0-9][0-9][0-9][0-9]' OR message_domain_handle_or_chat GLOB '[0-9][0-9][0-9][0-9]' THEN 'SHORT_CODE_OR_MARKETING_HANDLE'
           WHEN message_domain_handle_or_chat LIKE '+%' OR message_domain_handle_or_chat LIKE 'SMS;+;%' OR message_domain_handle_or_chat LIKE 'iMessage;%;+%' THEN 'PHONE_NUMBER_OR_HANDLE'
           WHEN LOWER(message_domain_handle_or_chat) LIKE '%chat%' OR domain_identifier='chatDomain' THEN 'CHAT_OR_THREAD_IDENTIFIER'
           WHEN message_domain_handle_or_chat LIKE '%@%' THEN 'EMAIL_OR_ACCOUNT_HANDLE'
           WHEN COALESCE(message_domain_handle_or_chat,'')='' THEN 'NO_HANDLE_IN_COMPACT_CONTEXT'
           ELSE 'OTHER_HANDLE_OR_DOMAIN'
         END AS handle_bucket
  FROM vw_ios_spotlight_message_body_review
), summarized AS (
  SELECT communication_context_type,bundle_id,content_type,body_review_bucket,noise_hint,handle_bucket,
         COUNT(*) AS spotlight_record_count,
         COUNT(DISTINCT spotlight_inode_or_object_id || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
         SUM(CASE WHEN COALESCE(extracted_message_text_or_subject,'')<>'' THEN 1 ELSE 0 END) AS rows_with_extracted_message_text,
         SUM(CASE WHEN COALESCE(conversation_or_thread_title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_thread_or_snippet_text,
         COUNT(DISTINCT NULLIF(message_domain_handle_or_chat,'')) AS distinct_handle_or_thread_count,
         MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
         MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
         substr(MIN(COALESCE(NULLIF(extracted_message_text_or_subject,''),NULLIF(conversation_or_thread_title,''),NULLIF(message_domain_handle_or_chat,''))),1,1200) AS min_message_sample,
         substr(MAX(COALESCE(NULLIF(extracted_message_text_or_subject,''),NULLIF(conversation_or_thread_title,''),NULLIF(message_domain_handle_or_chat,''))),1,1200) AS max_message_sample
  FROM classified
  GROUP BY communication_context_type,bundle_id,content_type,body_review_bucket,noise_hint,handle_bucket
)
SELECT *,
       'V0_9_32 compact contact/thread summary. This intentionally groups handles into buckets to avoid very large per-thread exports; use the detail sample or GUI record views for examples.' AS interpretation_note
FROM summarized;

DROP VIEW IF EXISTS vw_ios_spotlight_message_contact_thread_detail_sample;
CREATE VIEW vw_ios_spotlight_message_contact_thread_detail_sample AS
SELECT communication_context_type,bundle_id,domain_identifier,message_domain_handle_or_chat,suggested_contact_name,conversation_or_thread_title,body_review_bucket,noise_hint,
       COUNT(*) AS spotlight_record_count,
       COUNT(DISTINCT spotlight_inode_or_object_id || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       SUM(CASE WHEN COALESCE(extracted_message_text_or_subject,'')<>'' THEN 1 ELSE 0 END) AS rows_with_extracted_message_text,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       substr(MIN(NULLIF(extracted_message_text_or_subject,'')),1,1200) AS min_message_sample,
       substr(MAX(NULLIF(extracted_message_text_or_subject,'')),1,1200) AS max_message_sample,
       'Bounded top contact/thread detail sample from Spotlight message body review. Counts are index rows, not live-app DB rows.' AS interpretation_note
FROM vw_ios_spotlight_message_body_review
GROUP BY communication_context_type,bundle_id,domain_identifier,message_domain_handle_or_chat,suggested_contact_name,conversation_or_thread_title,body_review_bucket,noise_hint
ORDER BY CASE WHEN noise_hint='USER_REVIEW_CANDIDATE' THEN 0 ELSE 1 END, rows_with_extracted_message_text DESC, spotlight_record_count DESC, latest_spotlight_date_utc DESC
LIMIT 5000;

DROP VIEW IF EXISTS vw_ios_spotlight_message_body_focus_summary;
CREATE VIEW vw_ios_spotlight_message_body_focus_summary AS
SELECT noise_hint,body_review_bucket,communication_context_type,bundle_id,content_type,
       COUNT(*) AS spotlight_record_count,
       COUNT(DISTINCT spotlight_inode_or_object_id || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       SUM(CASE WHEN COALESCE(extracted_message_text_or_subject,'')<>'' THEN 1 ELSE 0 END) AS rows_with_extracted_text,
       SUM(CASE WHEN COALESCE(conversation_or_thread_title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_supporting_text,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       substr(MIN(COALESCE(NULLIF(extracted_message_text_or_subject,''),NULLIF(conversation_or_thread_title,''),NULLIF(spotlight_text_context_sample,''))),1,1200) AS min_review_sample,
       substr(MAX(COALESCE(NULLIF(extracted_message_text_or_subject,''),NULLIF(conversation_or_thread_title,''),NULLIF(spotlight_text_context_sample,''))),1,1200) AS max_review_sample,
       'Compact summary of message/body review buckets. Use this first to triage user-facing message evidence before opening row-level samples.' AS interpretation_note
FROM vw_ios_spotlight_message_body_review
GROUP BY noise_hint,body_review_bucket,communication_context_type,bundle_id,content_type;
)VSQL31", R"VSQL31(
DROP VIEW IF EXISTS vw_parser_diagnostics_action_summary;
CREATE VIEW vw_parser_diagnostics_action_summary AS
SELECT diagnostic_source,diagnostic_category,diagnostic_count,first_seen_utc,last_seen_utc,sample_min_message,sample_max_message,
       CASE
         WHEN diagnostic_source='native_decode_attempts' AND diagnostic_category='SUCCESS' THEN 'INFO_SUCCESS'
         WHEN diagnostic_source='partial_decode_error' THEN 'PARSER_GAP_REVIEW'
         WHEN LOWER(sample_min_message || ' ' || sample_max_message) LIKE '%inode id exceeds int64 range%' THEN 'HIGH_VOLUME_NATIVE_ID_RANGE_GAP'
         WHEN diagnostic_count > 10000 THEN 'HIGH_VOLUME_PARSER_DIAGNOSTIC'
         WHEN diagnostic_count > 0 THEN 'PARSER_DIAGNOSTIC_REVIEW'
         ELSE 'INFO'
       END AS diagnostic_severity,
       CASE
         WHEN diagnostic_source='native_decode_attempts' AND diagnostic_category='SUCCESS' THEN 'No action required for successful store decode attempts.'
         WHEN LOWER(sample_min_message || ' ' || sample_max_message) LIKE '%inode id exceeds int64 range%' THEN 'Prioritize native V2 identifier decoding review. These rows likely represent unsupported/overflow identifier forms and can mask useful Spotlight records.'
         WHEN diagnostic_source='partial_decode_error' THEN 'Use bounded diagnostic native DB mode on representative records to improve property/category decoding.'
         ELSE 'Review representative rows in parser_diagnostics_detail_sample before treating absence of parsed values as absence of evidence.'
       END AS recommended_action,
       interpretation_note
FROM vw_parser_diagnostics_summary;

DROP VIEW IF EXISTS vw_ios_spotlight_plaso_l2tcsv_timeline_sample;
CREATE VIEW vw_ios_spotlight_plaso_l2tcsv_timeline_sample AS
SELECT substr(event_time_utc,1,10) AS date,
       substr(event_time_utc,12,8) AS time,
       'UTC' AS timezone,
       CASE WHEN event_type LIKE '%Created%' THEN '..CB' ELSE 'M...' END AS MACB,
       'Spotlight' AS source,
       'iOS CoreSpotlight' AS sourcetype,
       event_type AS type,
       '' AS user,
       '' AS host,
       substr(event_summary,1,80) AS short,
       event_summary || ' | category=' || COALESCE(review_category,'') || ' | field=' || COALESCE(source_field,'') || ' | record=' || COALESCE(raw_record_id,'') AS desc,
       '2' AS version,
       COALESCE(contact_or_thread,'') AS filename,
       COALESCE(spotlight_inode_or_object_id,'') AS inode,
       'date_anomaly=' || COALESCE(date_anomaly_flag,'') || '; locator=' || COALESCE(validation_locator,'') AS notes,
       'vestigant_spotlight_ios_normalized_timeline' AS format,
       'bundle_id=' || COALESCE(bundle_id,'') || '; content_type=' || COALESCE(content_type,'') || '; store_guid=' || COALESCE(store_guid,'') AS extra
FROM vw_ios_spotlight_normalized_timeline
WHERE COALESCE(event_time_utc,'')<>''
ORDER BY event_time_utc DESC, raw_record_id DESC
LIMIT 5000;

DROP VIEW IF EXISTS vw_ios_spotlight_case_quality_dashboard;
CREATE VIEW vw_ios_spotlight_case_quality_dashboard AS
SELECT 'case_summary' AS quality_area, 'raw_records' AS metric, CAST(COUNT(*) AS TEXT) AS value, 'Number of compact Spotlight/CoreSpotlight raw records in the case database.' AS interpretation_note FROM raw_records
UNION ALL SELECT 'case_summary','compact_raw_key_values',CAST(COUNT(*) AS TEXT),'Compact persisted key/value rows; not a full raw-property dump in normal iOS mode.' FROM raw_key_values
UNION ALL SELECT 'case_summary','compact_date_candidates',CAST(COUNT(*) AS TEXT),'Compact persisted date candidates used for timeline/date provenance.' FROM raw_date_candidates
UNION ALL SELECT 'diagnostics','parser_diagnostic_rows',CAST(COALESCE(SUM(diagnostic_count),0) AS TEXT),'Visible parser failures/partial decode diagnostics. Review parser_diagnostics_action_summary for priority.' FROM vw_parser_diagnostics_summary WHERE diagnostic_source<>'native_decode_attempts'
UNION ALL SELECT 'investigator_review','user_focus_message_rows',CAST(COUNT(*) AS TEXT),'Rows in user-focus Spotlight message review with compact extracted message/thread text.' FROM vw_ios_spotlight_user_focus_message_review
UNION ALL SELECT 'investigator_review','missing_from_ffs_candidates',CAST(COUNT(*) AS TEXT),'Spotlight path/reference candidates not matched in available FFS lookup.' FROM vw_ios_spotlight_missing_from_ffs_candidates;
)VSQL31"}));


    exec(joinSql({R"VSQL32(
DROP VIEW IF EXISTS vw_ios_spotlight_direct_user_message_review;
CREATE VIEW vw_ios_spotlight_direct_user_message_review AS
SELECT raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       spotlight_date_utc,spotlight_date_source_field,communication_context_type,content_type,bundle_id,domain_identifier,
       message_domain_handle_or_chat,suggested_contact_name,conversation_or_thread_title,
       extracted_message_text_or_subject AS message_text,
       LENGTH(COALESCE(extracted_message_text_or_subject,'')) AS message_text_length,
       investigator_visible_text,body_review_bucket,noise_hint,account_identifier,message_service,
       phone_or_callback,callback_url,url_or_content_reference,attachment_or_media_path,message_identifier,mailbox_or_thread,mail_participants,
       spotlight_text_context_sample,validation_locator,
       'Direct user-review Apple Messages/SMS/RCS/iMessage text recovered from Spotlight/CoreSpotlight compact context. Treat as indexed Spotlight evidence; corroborate with SMS.db/app records when support parsing is enabled.' AS interpretation_note
FROM vw_ios_spotlight_message_body_review
WHERE communication_context_type='APPLE_MESSAGES_SMS_RCS_IMESSAGE'
  AND body_review_bucket='DIRECT_APPLE_MESSAGE_TEXT_OR_REACTION'
  AND noise_hint='USER_REVIEW_CANDIDATE'
  AND COALESCE(extracted_message_text_or_subject,'')<>''
  AND COALESCE(content_type,'')='public.message';

DROP VIEW IF EXISTS vw_ios_spotlight_direct_user_message_thread_summary;
CREATE VIEW vw_ios_spotlight_direct_user_message_thread_summary AS
SELECT COALESCE(NULLIF(suggested_contact_name,''),NULLIF(message_domain_handle_or_chat,''),NULLIF(conversation_or_thread_title,''),'NO_COMPACT_THREAD_OR_HANDLE') AS thread_or_contact_key,
       suggested_contact_name,message_domain_handle_or_chat,conversation_or_thread_title,bundle_id,content_type,
       COUNT(*) AS spotlight_message_record_count,
       COUNT(DISTINCT spotlight_inode_or_object_id || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       MIN(message_text_length) AS min_message_text_length,
       MAX(message_text_length) AS max_message_text_length,
       ROUND(AVG(message_text_length),1) AS avg_message_text_length,
       substr(MIN(NULLIF(message_text,'')),1,1200) AS min_message_sample,
       substr(MAX(NULLIF(message_text,'')),1,1200) AS max_message_sample,
       'Thread/contact summary of direct Apple Messages/SMS/RCS/iMessage Spotlight records. Use this first to identify high-volume people/threads, then open direct message review for row-level records.' AS interpretation_note
FROM vw_ios_spotlight_direct_user_message_review
GROUP BY suggested_contact_name,message_domain_handle_or_chat,conversation_or_thread_title,bundle_id,content_type;

DROP VIEW IF EXISTS vw_ios_spotlight_timeline_month_summary;
CREATE VIEW vw_ios_spotlight_timeline_month_summary AS
SELECT substr(event_time_utc,1,7) AS event_month_utc,review_category,event_type,bundle_id,content_type,date_anomaly_flag,
       COUNT(*) AS timeline_event_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       MIN(NULLIF(event_time_utc,'')) AS earliest_event_time_utc,
       MAX(NULLIF(event_time_utc,'')) AS latest_event_time_utc,
       substr(MIN(NULLIF(event_summary,'')),1,1200) AS min_event_sample,
)VSQL32", R"VSQL32(
       substr(MAX(NULLIF(event_summary,'')),1,1200) AS max_event_sample,
       'Monthly summary of normalized Spotlight timeline events. This is a triage surface for date ranges and anomaly buckets, not a replacement for row-level date provenance.' AS interpretation_note
FROM vw_ios_spotlight_normalized_timeline
WHERE COALESCE(event_time_utc,'')<>''
GROUP BY substr(event_time_utc,1,7),review_category,event_type,bundle_id,content_type,date_anomaly_flag;

DROP VIEW IF EXISTS vw_ios_spotlight_investigator_overview;
CREATE VIEW vw_ios_spotlight_investigator_overview AS
SELECT '01_start_here' AS review_order,'case_quality_dashboard' AS review_area,'iOS - Case Quality Dashboard' AS gui_view,'vw_ios_spotlight_case_quality_dashboard' AS sqlite_view,CAST(COUNT(*) AS TEXT) AS row_count,
       'Start here for compact counts, parser diagnostics, missing-FFS candidate count, and whether the run is compact/normal mode.' AS why_review_this
FROM vw_ios_spotlight_case_quality_dashboard
UNION ALL SELECT '02_direct_messages','communications','iOS - Direct User Message Review','vw_ios_spotlight_direct_user_message_review',CAST(COUNT(*) AS TEXT),
       'Direct Apple Messages/SMS/RCS/iMessage text recovered from Spotlight compact context. This is the most investigator-facing message-text view.'
FROM vw_ios_spotlight_direct_user_message_review
UNION ALL SELECT '03_threads','communications','iOS - Direct User Message Thread Summary','vw_ios_spotlight_direct_user_message_thread_summary',CAST(COUNT(*) AS TEXT),
       'Use this to identify high-volume or notable handles/threads before opening row-level message records.'
FROM vw_ios_spotlight_direct_user_message_thread_summary
UNION ALL SELECT '04_message_bodies','communications','iOS - Spotlight Message Body Review','vw_ios_spotlight_message_body_review',CAST(COUNT(*) AS TEXT),
       'Broader message/mail/call/body review including mail subjects, calls, media, and message-adjacent records.'
FROM vw_ios_spotlight_message_body_review
UNION ALL SELECT '05_missing_from_ffs','residency','iOS - High-Value Missing From FFS','vw_ios_spotlight_missing_from_ffs_high_value_candidates',CAST(COUNT(*) AS TEXT),
       'Spotlight path/reference candidates that did not match available FFS lookup; review with Spotlight text context and lookup status.'
FROM vw_ios_spotlight_missing_from_ffs_high_value_candidates
UNION ALL SELECT '06_timeline','timeline','iOS - Timeline Month Summary','vw_ios_spotlight_timeline_month_summary',CAST(COUNT(*) AS TEXT),
       'Monthly normalized Spotlight timeline summary for triage before opening row-level timeline samples.'
FROM vw_ios_spotlight_timeline_month_summary
UNION ALL SELECT '07_parser_diagnostics','parser_diagnostics','iOS - Parser Diagnostics Action Summary','vw_parser_diagnostics_action_summary',CAST(COUNT(*) AS TEXT),
       'Visible parser gaps/unparsed diagnostics. Non-zero values should be interpreted as coverage limits, not absence of evidence.'
FROM vw_parser_diagnostics_action_summary
UNION ALL SELECT '08_bplist_nskeyedarchiver','parser_diagnostics','iOS - Bplist/NSKeyedArchiver Summary','vw_ios_spotlight_bplist_nskeyedarchiver_summary',CAST(COUNT(*) AS TEXT),
       'Bounded discovery surface for binary plist / NSKeyedArchiver payloads found in iOS CoreSpotlight values. V0_9_54 extracts bounded bplist object string tokens only; full object graph decoding remains future work.'
FROM vw_ios_spotlight_bplist_nskeyedarchiver_summary
UNION ALL SELECT '09_super_timeline','timeline','iOS - Investigator Super Timeline','vw_investigator_super_timeline',CAST(COUNT(*) AS TEXT),
       'Unified chronological surface combining normalized Spotlight timeline, targeted app database events, KnowledgeC/CoreDuet rows, and usage evidence where available.'
FROM vw_investigator_super_timeline;
)VSQL32"}));

    // V0_9_44: bounded bplist / NSKeyedArchiver discovery views. These expose
    // compact parser-produced token summaries without asserting full semantic decode.
    exec(joinSql({R"VSQL33(
DROP VIEW IF EXISTS vw_ios_spotlight_bplist_nskeyedarchiver_detail;
CREATE VIEW vw_ios_spotlight_bplist_nskeyedarchiver_detail AS
SELECT kv.raw_kv_id,
       kv.source_id,
       kv.store_guid,
       kv.source_db,
       kv.inode_num AS spotlight_inode_or_object_id,
       kv.store_id AS spotlight_store_id,
       kv.parent_inode_num,
       COALESCE(r.raw_record_id,'') AS raw_record_id,
       COALESCE(r.last_updated_utc,'') AS last_updated_utc,
       COALESCE(r.file_name,'') AS file_name,
       COALESCE(r.display_name,'') AS display_name,
       COALESCE(r.content_type,'') AS content_type,
       kv.full_path,
       kv.field_name AS parser_context_field,
       kv.field_value AS bplist_context,
       CASE WHEN instr(lower(kv.field_value),'nskeyedarchiver_field_count=0')>0 THEN 'BPLIST_DETECTED_NO_NSKEYED_MARKER'
            WHEN instr(lower(kv.field_value),'nskeyedarchiver_field_count=')>0 THEN 'NSKEYEDARCHIVER_MARKER_DETECTED'
            ELSE 'BPLIST_OR_NSKEYED_CONTEXT_DETECTED' END AS bplist_detection_status,
       'V0_9_54 bounded bplist object-string discovery for iOS CoreSpotlight binary plist / NSKeyedArchiver payloads. This is not a full NSKeyedArchiver object-graph decode; validate against raw field values before asserting app-specific meaning.' AS interpretation_note
FROM raw_key_values kv
LEFT JOIN raw_records r
  ON r.source_id=kv.source_id
 AND r.store_guid=kv.store_guid
 AND r.source_db=kv.source_db
 AND r.inode_num=kv.inode_num
 AND COALESCE(r.store_id,'')=COALESCE(kv.store_id,'')
WHERE kv.field_name='__spotlight_bplist_nskeyedarchiver_context';

DROP VIEW IF EXISTS vw_ios_spotlight_bplist_nskeyedarchiver_summary;
CREATE VIEW vw_ios_spotlight_bplist_nskeyedarchiver_summary AS
SELECT source_id,
       store_guid,
       source_db,
       bplist_detection_status,
       COUNT(*) AS spotlight_record_count,
       COUNT(DISTINCT spotlight_inode_or_object_id || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       MIN(NULLIF(last_updated_utc,'')) AS earliest_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS latest_last_updated_utc,
       substr(MIN(NULLIF(bplist_context,'')),1,1200) AS min_context_sample,
       substr(MAX(NULLIF(bplist_context,'')),1,1200) AS max_context_sample,
       'Summary of compact bplist / NSKeyedArchiver token-discovery contexts. Counts are parser-discovery counts, not decoded app event counts.' AS interpretation_note
FROM vw_ios_spotlight_bplist_nskeyedarchiver_detail
GROUP BY source_id,store_guid,source_db,bplist_detection_status;
)VSQL33"}));

    // V0_9_54: explicit investigator time anomaly and KnowledgeC/CoreDuet
    // interaction views. These are triage surfaces and preserve provenance; they
    // do not assert misconduct without source-field validation.
    exec(joinSql({R"VSQL47(
DROP VIEW IF EXISTS vw_investigator_time_anomalies;
CREATE VIEW vw_investigator_time_anomalies AS
WITH candidates AS (
  SELECT a.artifact_id,
         a.source_id,
         a.store_guid,
         a.inode_num AS spotlight_inode_or_object_id,
         a.file_name,
         a.display_name,
         a.best_path,
         a.content_type,
         a.last_updated_utc,
         a.downloaded_date_utc,
         a.first_used_candidate_utc,
         a.last_used_date_utc,
         CASE
           WHEN COALESCE(a.last_used_date_utc,'')<>'' AND COALESCE(a.last_updated_utc,'')<>'' AND a.last_used_date_utc < a.last_updated_utc
             THEN 'REVIEW_USED_BEFORE_LAST_UPDATED'
           WHEN COALESCE(a.first_used_candidate_utc,'')<>'' AND COALESCE(a.last_updated_utc,'')<>'' AND a.first_used_candidate_utc < a.last_updated_utc
             THEN 'REVIEW_FIRST_USED_BEFORE_LAST_UPDATED'
           WHEN COALESCE(a.downloaded_date_utc,'')<>'' AND COALESCE(a.last_updated_utc,'')<>'' AND a.downloaded_date_utc < a.last_updated_utc
             THEN 'REVIEW_DOWNLOADED_BEFORE_LAST_UPDATED'
           ELSE 'NO_BASIC_ANOMALY'
         END AS anomaly_type,
         'Triage only: compares available Spotlight-derived usage/download/update fields. Validate raw source field provenance before inferring timestomping or user action.' AS interpretation_note
  FROM artifacts a
  WHERE (COALESCE(a.last_used_date_utc,'')<>'' OR COALESCE(a.first_used_candidate_utc,'')<>'' OR COALESCE(a.downloaded_date_utc,'')<>'')
    AND COALESCE(a.last_updated_utc,'')<>''
)
SELECT * FROM candidates WHERE anomaly_type<>'NO_BASIC_ANOMALY';

DROP VIEW IF EXISTS vw_ios_knowledgec_interaction_events;
CREATE VIEW vw_ios_knowledgec_interaction_events AS
SELECT ios_app_record_id,
       source_id,
       ios_db_id,
       database_normalized_path,
       database_name,
       database_category,
       app_hint,
       table_name,
       record_category,
       source_primary_key,
       record_timestamp_utc,
       timestamp_source,
       contact_or_participant AS app_bundle_id,
       title AS interaction_type,
       item_identifier AS knowledge_stream_name,
       text_snippet,
       parse_status,
       provenance,
       created_utc,
       'KnowledgeC/CoreDuet interaction event parsed from extracted app database in support/full mode. Absence of rows in normal mode usually means broad app DB record materialization was intentionally skipped.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category IN ('KNOWLEDGEC_COREDUET','COREDUET_INTERACTIONS')
   OR record_category LIKE 'KNOWLEDGEC_%'
ORDER BY record_timestamp_utc, ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_knowledgec_interaction_summary;
CREATE VIEW vw_ios_knowledgec_interaction_summary AS
SELECT database_category,
       app_hint,
       knowledge_stream_name,
       app_bundle_id,
       parse_status,
       COUNT(*) AS event_count,
       MIN(record_timestamp_utc) AS earliest_event_utc,
       MAX(record_timestamp_utc) AS latest_event_utc,
       substr(MIN(NULLIF(text_snippet,'')),1,1200) AS min_event_sample,
       substr(MAX(NULLIF(text_snippet,'')),1,1200) AS max_event_sample,
       'KnowledgeC/CoreDuet summary. Rows are available only when targeted database extraction and app DB record materialization are enabled.' AS interpretation_note
FROM vw_ios_knowledgec_interaction_events
GROUP BY database_category,app_hint,knowledge_stream_name,app_bundle_id,parse_status;

DROP VIEW IF EXISTS vw_investigator_super_timeline;
CREATE VIEW vw_investigator_super_timeline AS
SELECT event_time_utc AS event_utc,
       'Spotlight Index' AS source_module,
       review_category AS category,
       event_type AS action,
       bundle_id AS app_context,
       contact_or_thread AS target,
       event_summary AS details,
       validation_locator AS provenance,
       interpretation_note
FROM vw_ios_spotlight_normalized_timeline
WHERE COALESCE(event_time_utc,'')<>''
UNION ALL
SELECT record_timestamp_utc AS event_utc,
       'App Database' AS source_module,
       database_category AS category,
       record_category AS action,
       app_hint AS app_context,
       contact_or_participant AS target,
       COALESCE(title,'') || CASE WHEN COALESCE(text_snippet,'')<>'' THEN ' | ' || text_snippet ELSE '' END AS details,
       provenance,
       'Parsed iOS app database activity row; availability depends on targeted app DB extraction/materialization settings.' AS interpretation_note
FROM ios_app_parsed_records
WHERE COALESCE(record_timestamp_utc,'')<>''
UNION ALL
SELECT parsed_utc AS event_utc,
       'Usage Evidence' AS source_module,
       'FILE_USAGE' AS category,
       field_name AS action,
       '' AS app_context,
       inode_num AS target,
       field_value AS details,
       'artifact_id=' || COALESCE(CAST(artifact_id AS TEXT),'') || '; source_id=' || COALESCE(source_id,'') || '; store_guid=' || COALESCE(store_guid,'') AS provenance,
       'Usage evidence row with original field provenance. Validate raw source field before inferring user activity.' AS interpretation_note
FROM usage_evidence
WHERE COALESCE(parsed_utc,'')<>'';
)VSQL47"}));


    ensureGuiReviewViews();
}

void CaseDatabase::ensureGuiReviewViews() {
    sqlite3* db = db_;
    if (!db) return;

    auto execGuiSql = [db](const char* sql) {
        char* err = nullptr;
        const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::string msg = err ? err : "unknown SQLite error creating review views";
            sqlite3_free(err);
            throw std::runtime_error("Unable to create/update GUI review views: " + msg);
        }
    };

    auto execGuiSqlParts = [&](std::initializer_list<const char*> parts) {
        std::string sql;
        size_t total = 0;
        for (const char* part : parts) {
            if (part) total += std::strlen(part);
        }
        sql.reserve(total);
        for (const char* part : parts) {
            if (part) sql += part;
        }
        execGuiSql(sql.c_str());
    };

    auto guiColumnExists = [db](const char* table, const char* column) -> bool {
        std::string sql = std::string("PRAGMA table_info(") + table + ")";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) return false;
        bool exists = false;
        while (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char* name = sqlite3_column_text(st, 1);
            if (name && std::string(reinterpret_cast<const char*>(name)) == column) { exists = true; break; }
        }
        sqlite3_finalize(st);
        return exists;
    };
    auto ensureGuiColumn = [&](const char* table, const char* column, const char* type) {
        if (!guiColumnExists(table, column)) {
            std::string sql = std::string("ALTER TABLE ") + table + " ADD COLUMN " + column + " " + type;
            execGuiSql(sql.c_str());
        }
    };
    ensureGuiColumn("raw_date_candidates", "artifact_id", "INTEGER");
    ensureGuiColumn("raw_date_candidates", "parent_inode_num", "TEXT");
    ensureGuiColumn("raw_date_candidates", "file_name", "TEXT");
    ensureGuiColumn("raw_date_candidates", "best_path", "TEXT");
    ensureGuiColumn("raw_date_candidates", "date_type", "TEXT");
    ensureGuiColumn("raw_date_candidates", "association_status", "TEXT");
    ensureGuiColumn("raw_date_candidates", "association_confidence", "TEXT");


    execGuiSql(R"SQL(
CREATE TABLE IF NOT EXISTS review_view_preferences (
  platform TEXT NOT NULL,
  view_name TEXT NOT NULL,
  is_visible INTEGER NOT NULL DEFAULT 1,
  display_order INTEGER NOT NULL DEFAULT 0,
  preset_name TEXT DEFAULT 'Recommended V1',
  updated_utc TEXT DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY(platform, view_name)
);
)SQL");

    execGuiSql(R"SQL(
CREATE TABLE IF NOT EXISTS artifact_date_summary (
  artifact_id INTEGER PRIMARY KEY,
  source_id TEXT,
  store_guid TEXT,
  inode_num TEXT,
  parent_inode_num TEXT,
  file_name TEXT,
  display_name TEXT,
  best_path TEXT,
  path_source TEXT,
  path_status TEXT,
  logical_size_bytes INTEGER,
  physical_size_bytes INTEGER,
  content_type TEXT,
  where_froms TEXT,
  created_earliest_utc TEXT,
  created_latest_utc TEXT,
  modified_earliest_utc TEXT,
  modified_latest_utc TEXT,
  downloaded_earliest_utc TEXT,
  downloaded_latest_utc TEXT,
  usage_earliest_utc TEXT,
  usage_latest_utc TEXT,
  interesting_or_index_earliest_utc TEXT,
  interesting_or_index_latest_utc TEXT,
  likely_snapshot_date_count INTEGER DEFAULT 0,
  associated_date_count INTEGER DEFAULT 0,
  unassociated_date_count INTEGER DEFAULT 0,
  available_date_fields TEXT,
  association_confidence_summary TEXT,
  snapshot_warning_reasons TEXT,
  first_date_utc TEXT,
  last_date_utc TEXT,
  total_date_count INTEGER DEFAULT 0,
  created_date_count INTEGER DEFAULT 0,
  modified_date_count INTEGER DEFAULT 0,
  downloaded_date_count INTEGER DEFAULT 0,
  usage_date_count INTEGER DEFAULT 0,
  interesting_or_index_date_count INTEGER DEFAULT 0,
  metadata_seen_or_index_updated_count INTEGER DEFAULT 0,
  other_date_count INTEGER DEFAULT 0,
  likely_snapshot_or_index_date_count INTEGER DEFAULT 0,
  interpreted_date_types TEXT,
  date_association_status TEXT,
  date_association_confidence TEXT,
  refreshed_utc TEXT
);
CREATE INDEX IF NOT EXISTS idx_artifact_date_summary_source_artifact ON artifact_date_summary(source_id, artifact_id);
CREATE INDEX IF NOT EXISTS idx_artifact_date_summary_last_date ON artifact_date_summary(last_date_utc, artifact_id);
)SQL");

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_usage_artifacts;
CREATE VIEW vw_usage_artifacts AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,
       first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,usage_field_summary,open_count_estimate,
       existence_status,deleted_or_orphaned_candidate,confidence
FROM artifacts
WHERE COALESCE(usage_field_summary,'')<>''
   OR COALESCE(last_used_date_utc,'')<>''
   OR COALESCE(first_used_candidate_utc,'')<>''
   OR COALESCE(used_dates_count,0)>0
   OR COALESCE(use_count_value,'')<>'';

DROP VIEW IF EXISTS vw_timeline_usage_focus;
CREATE VIEW vw_timeline_usage_focus AS
SELECT t.timeline_id,t.event_timestamp_utc AS event_utc,t.event_type,t.event_source_field,t.artifact_id,t.source_id,t.store_guid,t.inode_num,
       a.parent_inode_num,a.file_name,a.display_name,a.best_path,a.content_type,a.where_froms,'MEDIUM_USAGE_FIELD' AS confidence,'' AS notes
FROM timeline_events t
LEFT JOIN artifacts a ON a.artifact_id=t.artifact_id
WHERE lower(COALESCE(t.event_source_field,'')) LIKE '%used%'
   OR lower(COALESCE(t.event_type,'')) LIKE '%used%'
   OR lower(COALESCE(t.event_source_field,'')) LIKE '%usage%'
   OR lower(COALESCE(t.event_source_field,'')) LIKE '%open%';

DROP VIEW IF EXISTS vw_wherefroms_downloads;
CREATE VIEW vw_wherefroms_downloads AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,
       downloaded_date_utc,last_updated_utc,existence_status,deleted_or_orphaned_candidate,confidence
FROM artifacts
WHERE COALESCE(where_froms,'')<>'' OR COALESCE(downloaded_date_utc,'')<>'';

DROP VIEW IF EXISTS vw_recent_activity;
CREATE VIEW vw_recent_activity AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,
       last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,where_froms,confidence
FROM artifacts
WHERE COALESCE(last_updated_utc,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'';

DROP VIEW IF EXISTS vw_content_type_summary;
CREATE VIEW vw_content_type_summary AS
SELECT COALESCE(NULLIF(content_type,''),'(blank)') AS content_type,
       COUNT(*) AS artifact_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_artifact_count,
       SUM(CASE WHEN COALESCE(best_path,'')<>'' THEN 1 ELSE 0 END) AS path_artifact_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
)SQL" R"SQL(       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc,
       SUM(CAST(COALESCE(NULLIF(logical_size_bytes,''),'0') AS INTEGER)) AS total_logical_size_bytes,
       SUM(CAST(COALESCE(NULLIF(physical_size_bytes,''),'0') AS INTEGER)) AS total_physical_size_bytes
FROM artifacts
GROUP BY COALESCE(NULLIF(content_type,''),'(blank)');

DROP VIEW IF EXISTS vw_store_content_type_summary;
CREATE VIEW vw_store_content_type_summary AS
SELECT store_guid,
       COALESCE(NULLIF(content_type,''),'(blank)') AS content_type,
       COUNT(*) AS artifact_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_artifact_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc
FROM artifacts
GROUP BY store_guid, COALESCE(NULLIF(content_type,''),'(blank)');

DROP VIEW IF EXISTS vw_folder_activity;
CREATE VIEW vw_folder_activity AS
SELECT store_guid,
       parent_inode_num,
       COUNT(*) AS child_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_child_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc,
       MIN(NULLIF(best_path,'')) AS sample_child_path,
       SUM(CASE WHEN COALESCE(content_type,'')='public.folder' THEN 1 ELSE 0 END) AS folder_child_count
FROM artifacts
WHERE COALESCE(parent_inode_num,'')<>''
GROUP BY store_guid, parent_inode_num
HAVING COUNT(*) > 1;

DROP VIEW IF EXISTS vw_path_reconstruction;
CREATE VIEW vw_path_reconstruction AS
SELECT pl.link_id,
       pl.source_id,
       pl.store_guid,
       pl.child_artifact_id AS artifact_id,
       pl.child_inode_num AS inode_num,
       pl.child_parent_inode_num AS parent_inode_num,
       COALESCE(NULLIF(a.file_name,''), pl.child_file_name) AS file_name,
       COALESCE(NULLIF(a.display_name,''), pl.child_file_name) AS display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       pl.parent_artifact_id,
       pl.parent_inode_num AS resolved_parent_inode_num,
       pl.parent_file_name,
       pl.parent_best_path,
       pl.reconstructed_path_candidate,
       CASE WHEN COALESCE(a.path_source,'')='PARENT_INODE_RECONSTRUCTION' THEN 1 ELSE 0 END AS applied_to_artifact_path,
)SQL" R"SQL(       CASE WHEN COALESCE(a.best_path,'')=COALESCE(pl.reconstructed_path_candidate,'') AND COALESCE(pl.reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END AS candidate_matches_artifact_path,
       pl.sibling_group_key,
       pl.sibling_count,
       pl.relationship_status,
       pl.path_reconstruction_method,
       pl.confidence
FROM parent_inode_links pl
LEFT JOIN artifacts a ON a.artifact_id=pl.child_artifact_id
ORDER BY pl.source_id, pl.store_guid, CAST(pl.child_parent_inode_num AS INTEGER), CAST(pl.child_inode_num AS INTEGER), pl.link_id;

DROP VIEW IF EXISTS vw_same_folder_groups;
)SQL");
    execGuiSql(R"SQL(CREATE VIEW vw_same_folder_groups AS
SELECT source_id,
       store_guid,
       child_parent_inode_num AS parent_inode_num,
       MAX(parent_artifact_id) AS parent_artifact_id,
       MAX(CASE WHEN COALESCE(NULLIF(trim(parent_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN parent_file_name ELSE '' END) AS parent_file_name,
       MAX(CASE WHEN COALESCE(NULLIF(trim(parent_best_path),''),'') NOT IN ('/','------NONAME------','(null)','NULL','.') THEN parent_best_path ELSE '' END) AS parent_best_path,
       COUNT(*) AS child_count,
       SUM(CASE WHEN relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE' THEN 1 ELSE 0 END) AS resolved_parent_link_count,
       SUM(CASE WHEN COALESCE(reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END) AS reconstructed_child_path_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) AS child_name_count,
       MIN(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END) AS first_child_name,
       MAX(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END) AS last_child_name,
       CASE
         WHEN SUM(CASE WHEN COALESCE(reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END)>0 THEN 'RECONSTRUCTED_CHILD_PATHS_PRESENT'
         WHEN SUM(CASE WHEN relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE' THEN 1 ELSE 0 END)>0 THEN 'PARENT_LINKS_WITHOUT_RECONSTRUCTED_PATH'
         ELSE 'SAME_PARENT_INODE_GROUP_ONLY'
       END AS folder_group_status,
       MAX(confidence) AS max_confidence,
       sibling_group_key
FROM parent_inode_links
GROUP BY source_id, store_guid, child_parent_inode_num, sibling_group_key
HAVING COUNT(*) > 1
ORDER BY child_count DESC, source_id, store_guid, CAST(child_parent_inode_num AS INTEGER);

DROP VIEW IF EXISTS vw_volume_root_focus;
CREATE VIEW vw_volume_root_focus AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,content_type_tree,
       last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,is_mounted_volume_path,
       mounted_volume_name,external_volume_reason,confidence
FROM artifacts
WHERE COALESCE(content_type,'')='public.volume'
   OR COALESCE(is_mounted_volume_path,0)<>0
   OR COALESCE(mounted_volume_name,'')<>''
   OR COALESCE(external_volume_reason,'')<>'';
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_investigator_keyword_search_values;
CREATE VIEW vw_investigator_keyword_search_values AS
SELECT CASE WHEN store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%' THEN 'ios' ELSE 'macos_or_unknown' END AS platform,
       'raw_key_values' AS source_table,
       source_id, store_guid, source_db,
       NULL AS artifact_id,
       inode_num, parent_inode_num,
       field_name,
       field_value AS search_value,
       '' AS date_utc,
       '' AS file_name,
       full_path AS best_path,
       '' AS content_type,
       store_path AS provenance
FROM raw_key_values
WHERE COALESCE(field_value,'')<>''
UNION ALL
SELECT CASE WHEN store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%' THEN 'ios' ELSE 'macos_or_unknown' END AS platform,
       'raw_records' AS source_table,
       source_id, store_guid, source_db,
       NULL AS artifact_id,
       inode_num, parent_inode_num,
       'record_summary' AS field_name,
       trim(COALESCE(file_name,'') || ' ' || COALESCE(display_name,'') || ' ' || COALESCE(full_path,'') || ' ' || COALESCE(content_type,'') || ' ' || COALESCE(where_froms,'')) AS search_value,
       last_updated_utc AS date_utc,
       file_name,
       full_path AS best_path,
       content_type,
       store_path AS provenance
FROM raw_records
WHERE trim(COALESCE(file_name,'') || COALESCE(display_name,'') || COALESCE(full_path,'') || COALESCE(content_type,'') || COALESCE(where_froms,''))<>''
UNION ALL
SELECT CASE WHEN store_guid LIKE 'ios_%' THEN 'ios' ELSE 'macos_or_unknown' END AS platform,
       'artifacts' AS source_table,
       source_id,
       store_guid,
       '' AS source_db,
       artifact_id,
       inode_num,
       parent_inode_num,
       'artifact_summary' AS field_name,
       trim(COALESCE(file_name,'') || ' ' || COALESCE(display_name,'') || ' ' || COALESCE(best_path,'') || ' ' || COALESCE(content_type,'') || ' ' || COALESCE(where_froms,'') || ' ' || COALESCE(index_text_snippet,'')) AS search_value,
       COALESCE(NULLIF(last_updated_utc,''), NULLIF(last_used_date_utc,''), NULLIF(first_used_candidate_utc,''), NULLIF(downloaded_date_utc,'')) AS date_utc,
       file_name,
       best_path,
       content_type,
       path_source || ';' || path_status AS provenance
FROM artifacts
WHERE trim(COALESCE(file_name,'') || COALESCE(display_name,'') || COALESCE(best_path,'') || COALESCE(content_type,'') || COALESCE(where_froms,'') || COALESCE(index_text_snippet,''))<>'';

CREATE TABLE IF NOT EXISTS ios_ffs_file_inventory (
  ios_file_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT, original_zip_entry TEXT, normalized_path TEXT, file_name TEXT, extension TEXT, size_bytes INTEGER,
)SQL" R"SQL(  zip_modified_utc TEXT, protection_class_hint TEXT, app_container_hint TEXT, domain_hint TEXT, is_directory INTEGER DEFAULT 0,
  sha256_status TEXT, inventory_notes TEXT, created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_ffs_path_lookup (
  ios_path_lookup_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT, normalized_path TEXT, file_name TEXT, size_bytes INTEGER, zip_modified_utc TEXT,
  protection_class_hint TEXT, app_container_hint TEXT, domain_hint TEXT, is_directory INTEGER DEFAULT 0, lookup_source TEXT, created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_app_database_inventory (
  ios_db_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT, original_zip_entry TEXT, normalized_path TEXT, database_name TEXT, database_category TEXT, app_hint TEXT,
  protection_class_hint TEXT, size_bytes INTEGER, zip_modified_utc TEXT, parse_status TEXT, record_inventory_status TEXT, notes TEXT, extracted_path TEXT, created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_app_database_record_inventory (
  ios_record_inventory_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, ios_db_id INTEGER, database_normalized_path TEXT, database_name TEXT, database_category TEXT, app_hint TEXT,
  table_name TEXT, row_count INTEGER, sample_columns TEXT, record_category TEXT, parse_status TEXT, notes TEXT, created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_app_parsed_records (
  ios_app_record_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, ios_db_id INTEGER, database_normalized_path TEXT, database_name TEXT, database_category TEXT, app_hint TEXT,
  table_name TEXT, record_category TEXT, source_primary_key TEXT, record_timestamp_utc TEXT, timestamp_source TEXT, contact_or_participant TEXT, url TEXT, title TEXT,
  file_path TEXT, item_identifier TEXT, text_snippet TEXT, parse_status TEXT, provenance TEXT, created_utc TEXT
);
CREATE INDEX IF NOT EXISTS idx_ios_ffs_path ON ios_ffs_file_inventory(source_id, normalized_path);
CREATE INDEX IF NOT EXISTS idx_ios_ffs_lookup_path ON ios_ffs_path_lookup(source_id, normalized_path);
CREATE INDEX IF NOT EXISTS idx_ios_db_category ON ios_app_database_inventory(source_id, database_category, app_hint);
CREATE INDEX IF NOT EXISTS idx_ios_db_record_source ON ios_app_database_record_inventory(source_id, database_category, table_name);
CREATE INDEX IF NOT EXISTS idx_ios_app_parsed_source ON ios_app_parsed_records(source_id, database_category, record_category);
CREATE INDEX IF NOT EXISTS idx_ios_app_parsed_db ON ios_app_parsed_records(ios_db_id, table_name);

DROP VIEW IF EXISTS vw_ios_relevant_fields;
CREATE VIEW vw_ios_relevant_fields AS
SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,full_path,record_state,field_name,field_value
FROM raw_key_values
WHERE lower(field_name) LIKE '%content%'
   OR lower(field_name) LIKE '%text%'
   OR lower(field_name) LIKE '%message%'
   OR lower(field_name) LIKE '%conversation%'
   OR lower(field_name) LIKE '%sender%'
   OR lower(field_name) LIKE '%recipient%'
   OR lower(field_name) LIKE '%author%'
   OR lower(field_name) LIKE '%creator%'
   OR lower(field_name) LIKE '%account%'
   OR lower(field_name) LIKE '%phone%'
   OR lower(field_name) LIKE '%mail%'
   OR lower(field_name) LIKE '%bundle%'
   OR lower(field_name) LIKE '%domain%'
   OR lower(field_name) LIKE '%used%'
   OR lower(field_name) LIKE '%wherefrom%'
   OR lower(field_name) LIKE '%download%'
   OR lower(field_name) LIKE '%path%'
   OR lower(field_name) LIKE '%filename%'
   OR lower(field_name) LIKE '%displayname%';

DROP VIEW IF EXISTS vw_ios_store_parse_summary;
CREATE VIEW vw_ios_store_parse_summary AS
SELECT store_guid,
       source_db,
       COUNT(*) AS raw_record_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(file_name),''),'') NOT IN ('','------NONAME------','------PLIST------','(null)','NULL','.') THEN 1 ELSE 0 END) AS record_file_name_count,
)SQL" R"SQL(       SUM(CASE WHEN COALESCE(NULLIF(trim(full_path),''),'') NOT IN ('','/','------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) AS record_full_path_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(file_name),''),'') IN ('------NONAME------','------PLIST------','(null)','NULL','.') OR COALESCE(NULLIF(trim(file_name),''),'')='' THEN 1 ELSE 0 END) AS placeholder_file_name_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(full_path),''),'')='/' THEN 1 ELSE 0 END) AS placeholder_root_path_count,
       MIN(NULLIF(last_updated_utc,'')) AS earliest_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS latest_last_updated_utc
FROM raw_records
WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
GROUP BY store_guid, source_db;

DROP VIEW IF EXISTS vw_ios_string_probe_category_summary;
CREATE VIEW vw_ios_string_probe_category_summary AS
WITH probe AS (
  SELECT store_guid, source_db, inode_num, field_name, field_value, LOWER(field_value) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), categorized AS (
  SELECT CASE
    WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'URL_OR_WEB_LINK'
    WHEN v LIKE '%@%' AND v LIKE '%.%' THEN 'EMAIL_ADDRESS_OR_ACCOUNT'
    WHEN v LIKE '%imessage%' OR v LIKE '%sms%' OR v LIKE '%message%' THEN 'MESSAGE_TEXT_OR_MESSAGE_APP'
    WHEN v LIKE '%icloud%' OR v LIKE '%onedrive%' OR v LIKE '%dropbox%' OR v LIKE '%google drive%' OR v LIKE '%drive.google%' THEN 'CLOUD_STORAGE_OR_SYNC'
    WHEN v LIKE '%calendar%' OR v LIKE '%invite%' OR v LIKE '%rsvp%' OR v LIKE '%event%' THEN 'CALENDAR_OR_INVITATION'
    WHEN v LIKE 'file:%' OR v LIKE '/private/var/%' OR v LIKE '%/mobile/%' THEN 'FILE_OR_IOS_PATH'
    ELSE 'OTHER_STRING_PROBE' END AS probe_category,
    store_guid, source_db, inode_num, field_value
  FROM probe
)
SELECT probe_category,
       COUNT(*) AS row_count,
       COUNT(DISTINCT store_guid) AS store_count,
       COUNT(DISTINCT inode_num) AS distinct_record_count,
       COUNT(DISTINCT field_value) AS distinct_value_count,
       substr(MIN(field_value),1,500) AS min_sample_value,
       substr(MAX(field_value),1,500) AS max_sample_value
FROM categorized
GROUP BY probe_category;

DROP VIEW IF EXISTS vw_ios_string_probe_values;
)SQL");
    execGuiSqlParts({
        R"VSGUI(CREATE VIEW vw_ios_string_probe_values AS
SELECT CASE
    WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN 'URL_OR_WEB_LINK'
    WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN 'EMAIL_ADDRESS_OR_ACCOUNT'
    WHEN LOWER(field_value) LIKE '%imessage%' OR LOWER(field_value) LIKE '%sms%' OR LOWER(field_value) LIKE '%message%' THEN 'MESSAGE_TEXT_OR_MESSAGE_APP'
    WHEN LOWER(field_value) LIKE '%icloud%' OR LOWER(field_value) LIKE '%onedrive%' OR LOWER(field_value) LIKE '%dropbox%' OR LOWER(field_value) LIKE '%google drive%' OR LOWER(field_value) LIKE '%drive.google%' THEN 'CLOUD_STORAGE_OR_SYNC'
    WHEN LOWER(field_value) LIKE '%calendar%' OR LOWER(field_value) LIKE '%invite%' OR LOWER(field_value) LIKE '%rsvp%' OR LOWER(field_value) LIKE '%event%' THEN 'CALENDAR_OR_INVITATION'
    WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN 'FILE_OR_IOS_PATH'
    ELSE 'OTHER_STRING_PROBE' END AS probe_category,
       raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,substr(field_value,1,2000) AS field_value_sample
FROM raw_key_values
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(field_value,'')<>'';

DROP VIEW IF EXISTS vw_ios_record_string_probe_summary;
CREATE VIEW vw_ios_record_string_probe_summary AS
WITH kv AS (
  SELECT source_id, store_guid, source_db, inode_num, store_id, parent_inode_num,
         CASE
           WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN 'URL_OR_WEB_LINK'
           WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN 'EMAIL_ADDRESS_OR_ACCOUNT'
           WHEN LOWER(field_value) LIKE '%imessage%' OR LOWER(field_value) LIKE '%sms%' OR LOWER(field_value) LIKE '%message%' THEN 'MESSAGE_TEXT_OR_MESSAGE_APP'
           WHEN LOWER(field_value) LIKE '%icloud%' OR LOWER(field_value) LIKE '%onedrive%' OR LOWER(field_value) LIKE '%dropbox%' OR LOWER(field_value) LIKE '%google drive%' OR LOWER(field_value) LIKE '%drive.google%' THEN 'CLOUD_STORAGE_OR_SYNC'
           WHEN LOWER(field_value) LIKE '%calendar%' OR LOWER(field_value) LIKE '%invite%' OR LOWER(field_value) LIKE '%rsvp%' OR LOWER(field_value) LIKE '%event%' THEN 'CALENDAR_OR_INVITATION'
           WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN 'FILE_OR_IOS_PATH'
           ELSE 'OTHER_STRING_PROBE'
         END AS probe_category,
         field_name,
         field_value
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), agg AS (
  SELECT source_id, store_guid, source_db, inode_num, store_id,
         COUNT(*) AS string_probe_rows,
         COUNT(DISTINCT field_name) AS distinct_probe_field_count,
         GROUP_CONCAT(DISTINCT probe_category) AS probe_categories,
         substr(GROUP_CONCAT(substr(field_name || '=' || REPLACE(REPLACE(field_value, char(13),' '), char(10),' '),1,500), ' || '),1,4000) AS string_probe_sample
  FROM kv
  GROUP BY source_id, store_guid, source_db, inode_num, store_id
)
SELECT r.raw_record_id,
       r.source_id,
       r.store_guid,
       r.source_db,
       r.inode_num,
       r.store_id,
       r.parent_inode_num,
       r.file_name,
       r.content_type,
       r.display_name,
       r.full_path,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       a.string_probe_rows,
       a.distinct_probe_field_count,
       a.probe_categories,
       a.string_probe_sample,
       r.record_state
FROM raw_records r
JOIN agg a
  ON a.source_id = r.source_id
 AND a.store_guid = r.store_guid
 AND a.source_db = r.source_db
 AND a.inode_num = r.inode_num
 AND a.store_id = r.store_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_timeline_index_updates;
CREATE VIEW vw_ios_timeline_index_updates AS
SELECT raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,file_name,content_type,display_name,full_path,last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       record_state
FROM raw_records
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(last_updated_utc,'')<>'';


DROP VIEW IF EXISTS vw_ios_spotlight_date_provenance;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_date_provenance AS
WITH dc AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         COUNT(*) AS date_candidate_count,
         MAX(CASE WHEN field_name='Last_Updated' THEN parsed_utc ELSE parsed_utc END) AS primary_date_utc,
         MAX(CASE WHEN field_name='Last_Updated' THEN field_name ELSE field_name END) AS primary_date_field,
         MAX(CASE WHEN field_name='Last_Updated' THEN field_value ELSE field_value END) AS primary_raw_value,
         MAX(CASE WHEN field_name='Last_Updated' THEN parse_method ELSE parse_method END) AS primary_parse_method,
         GROUP_CONCAT(DISTINCT field_name) AS date_source_fields,
         GROUP_CONCAT(DISTINCT parse_method) AS date_parse_methods,
         GROUP_CONCAT(DISTINCT date_type) AS date_type_summary,
         substr(GROUP_CONCAT(COALESCE(field_name,'') || '=' || COALESCE(field_value,'') || ' -> ' || COALESCE(parsed_utc,'') || ' [' || COALESCE(parse_method,'') || ']',' || '),1,4000) AS date_source_evidence
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.raw_record_id,
       r.source_id,
       r.store_guid,
       r.source_db,
       r.store_path,
       r.inode_num AS spotlight_inode_or_object_id,
       r.store_id AS spotlight_store_id,
       r.parent_inode_num,
       r.file_name,
       r.display_name,
       r.full_path,
       r.content_type,
       r.record_state,
       COALESCE(dc.primary_date_utc, r.last_updated_utc) AS spotlight_date_utc,
       COALESCE(dc.primary_date_field, 'Last_Updated') AS spotlight_date_source_field,
       CASE WHEN dc.primary_date_field IS NOT NULL THEN 'raw_date_candidates' ELSE 'raw_records' END AS spotlight_date_source_table,
       COALESCE(dc.primary_raw_value, r.last_updated_raw) AS spotlight_date_raw_value,
       COALESCE(dc.primary_parse_method, 'native_epoch_microseconds') AS spotlight_date_parse_method,
       COALESCE(NULLIF(dc.date_type_summary,''), 'metadata_seen_or_index_updated') AS spotlight_date_type,
       COALESCE(dc.date_source_fields, 'Last_Updated') AS spotlight_date_source_fields,
       COALESCE(dc.date_parse_methods, 'native_epoch_microseconds') AS spotlight_date_parse_methods,
       COALESCE(dc.date_candidate_count, CASE WHEN COALESCE(r.last_updated_utc,'')<>'' THEN 1 ELSE 0 END) AS spotlight_date_candidate_count,
       COALESCE(dc.date_source_evidence, 'Last_Updated=' || COALESCE(r.last_updated_raw,'') || ' -> ' || COALESCE(r.last_updated_utc,'')) AS spotlight_date_source_evidence,
       'Validate against raw_date_candidates.field_name/field_value/parsed_utc/parse_method or raw_records.last_updated_raw/last_updated_utc for this Store-V2 record.' AS date_validation_hint,
       'CoreSpotlight date provenance. Last_Updated is metadata/index timing unless another decoded field supports created/modified/accessed/user-activity semantics.' AS interpretation_note
FROM raw_records r
LEFT JOIN dc ON dc.source_id=r.source_id
            AND dc.store_guid=r.store_guid
            AND dc.source_db=r.source_db
            AND dc.inode_num=r.inode_num
            AND COALESCE(dc.store_id,'')=COALESCE(r.store_id,'')
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_artifacts;
CREATE VIEW vw_ios_artifacts AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,last_updated_utc,confidence
FROM artifacts
WHERE store_guid LIKE 'ios_%' OR source_id IN (SELECT source_id FROM raw_records WHERE source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%');


DROP VIEW IF EXISTS vw_ios_ffs_file_inventory;
CREATE VIEW vw_ios_ffs_file_inventory AS
SELECT ios_file_id,source_id,normalized_path,original_zip_entry,file_name,extension,size_bytes,zip_modified_utc,
       protection_class_hint,app_container_hint,domain_hint,is_directory,sha256_status,inventory_notes,created_utc
FROM ios_ffs_file_inventory
ORDER BY normalized_path;

DROP VIEW IF EXISTS vw_ios_database_artifact_inventory;
CREATE VIEW vw_ios_database_artifact_inventory AS
SELECT ios_db_id,source_id,normalized_path,original_zip_entry,database_name,database_category,app_hint,
       protection_class_hint,size_bytes,zip_modified_utc,parse_status,record_inventory_status,notes,extracted_path,created_utc
FROM ios_app_database_inventory
ORDER BY database_category,app_hint,normalized_path;

DROP VIEW IF EXISTS vw_ios_app_database_record_inventory;
CREATE VIEW vw_ios_app_database_record_inventory AS
SELECT ios_record_inventory_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,row_count,sample_columns,record_category,parse_status,notes,created_utc
FROM ios_app_database_record_inventory
ORDER BY database_category,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_app_database_record_summary;
CREATE VIEW vw_ios_app_database_record_summary AS
SELECT database_category,app_hint,record_category,parse_status,COUNT(*) AS table_count,
       SUM(COALESCE(row_count,0)) AS total_rows,MIN(database_name) AS first_database,MAX(database_name) AS last_database
FROM ios_app_database_record_inventory
GROUP BY database_category,app_hint,record_category,parse_status
ORDER BY database_category,record_category;

DROP VIEW IF EXISTS vw_ios_app_parsed_records;
CREATE VIEW vw_ios_app_parsed_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc
FROM ios_app_parsed_records
ORDER BY database_category,record_category,record_timestamp_utc,database_name,table_name,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_app_parsed_record_summary;
CREATE VIEW vw_ios_app_parsed_record_summary AS
SELECT database_category,app_hint,record_category,parse_status,COUNT(*) AS parsed_record_count,
       COUNT(DISTINCT database_name) AS database_count,
       MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,
       MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
       MIN(database_name) AS first_database,
       MAX(database_name) AS last_database
FROM ios_app_parsed_records
GROUP BY database_category,app_hint,record_category,parse_status
ORDER BY database_category,record_category;

DROP VIEW IF EXISTS vw_ios_apple_messages_parsed_records;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_apple_messages_parsed_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc
FROM ios_app_parsed_records
WHERE database_category='APPLE_MESSAGES' OR lower(database_name) IN ('sms.db','chat.db') OR lower(database_normalized_path) LIKE '%/sms.db'
ORDER BY record_timestamp_utc,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_apple_messages_parsed_summary;
CREATE VIEW vw_ios_apple_messages_parsed_summary AS
SELECT record_category,parse_status,COUNT(*) AS parsed_record_count,COUNT(DISTINCT ios_db_id) AS database_count,
       MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
       COUNT(NULLIF(contact_or_participant,'')) AS records_with_contact_or_handle,
       COUNT(NULLIF(file_path,'')) AS records_with_file_path,
       COUNT(NULLIF(text_snippet,'')) AS records_with_text_or_metadata,
       MIN(database_normalized_path) AS first_database_path,MAX(database_normalized_path) AS last_database_path
FROM vw_ios_apple_messages_parsed_records
GROUP BY record_category,parse_status;


DROP VIEW IF EXISTS vw_ios_whatsapp_parsed_records;
CREATE VIEW vw_ios_whatsapp_parsed_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc
FROM ios_app_parsed_records
WHERE database_category='WHATSAPP'
   OR lower(database_normalized_path) LIKE '%group.net.whatsapp%'
   OR lower(database_normalized_path) LIKE '%/whatsapp/%'
   OR lower(database_name) IN ('chatstorage.sqlite','contactsv2.sqlite','callhistory.sqlite')
ORDER BY record_timestamp_utc,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_whatsapp_parsed_summary;
CREATE VIEW vw_ios_whatsapp_parsed_summary AS
SELECT record_category,parse_status,COUNT(*) AS parsed_record_count,COUNT(DISTINCT ios_db_id) AS database_count,
       MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
       COUNT(NULLIF(contact_or_participant,'')) AS records_with_contact_or_jid,
       COUNT(NULLIF(file_path,'')) AS records_with_media_or_file_path,
       COUNT(NULLIF(text_snippet,'')) AS records_with_text_or_metadata,
       MIN(database_normalized_path) AS first_database_path,MAX(database_normalized_path) AS last_database_path
FROM vw_ios_whatsapp_parsed_records
GROUP BY record_category,parse_status;

DROP VIEW IF EXISTS vw_ios_spotlight_referenced_paths;
CREATE VIEW vw_ios_spotlight_referenced_paths AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,field_value,LOWER(field_value) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), extracted AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,field_value,
         CASE
           WHEN v LIKE 'file:///private/var/%' THEN substr(field_value,8)
           WHEN v LIKE 'file:///var/%' THEN '/private' || substr(field_value,8)
           WHEN v LIKE '/private/var/%' THEN field_value
           WHEN v LIKE '/var/%' THEN '/private' || field_value
           WHEN instr(v,'/private/var/')>0 THEN substr(field_value,instr(v,'/private/var/'))
           WHEN instr(v,'/var/mobile/')>0 THEN '/private' || substr(field_value,instr(v,'/var/mobile/'))
           ELSE ''
         END AS extracted_path,
         CASE
           WHEN v LIKE 'file:%' OR v LIKE '%/private/var/%' OR v LIKE '%/var/mobile/%' THEN 'IOS_FILE_PATH_OR_FILE_URL'
           WHEN v LIKE '%http://%' OR v LIKE '%https://%' THEN 'WEB_URL_NO_LOCAL_PATH'
           ELSE 'NON_PATH_REFERENCE'
         END AS reference_type
  FROM probes
)
SELECT raw_kv_id AS reference_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,
       substr(field_value,1,2000) AS raw_reference_value,
       reference_type,
       CASE WHEN extracted_path<>'' THEN lower(replace(replace(replace(extracted_path,'file://',''),'%20',' '),'\','/')) ELSE '' END AS normalized_ios_path,
       CASE WHEN extracted_path<>'' THEN 'MEDIUM_STRING_PROBE_PATH' ELSE 'LOW_NO_LOCAL_PATH' END AS confidence,
       'Spotlight string probe reference; formal CoreSpotlight property mapping remains parser roadmap work' AS notes
FROM extracted
WHERE reference_type<>'NON_PATH_REFERENCE';

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_candidates;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_missing_from_ffs_candidates AS
WITH refs AS (
  SELECT * FROM vw_ios_spotlight_referenced_paths WHERE COALESCE(normalized_ios_path,'')<>''
), files AS (
  SELECT source_id,normalized_path,file_name,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,'full_inventory' AS lookup_source
  FROM ios_ffs_file_inventory
  UNION ALL
  SELECT l.source_id,l.normalized_path,l.file_name,l.size_bytes,l.zip_modified_utc,l.protection_class_hint,l.app_container_hint,l.domain_hint,COALESCE(NULLIF(l.lookup_source,''),'slim_path_lookup') AS lookup_source
  FROM ios_ffs_path_lookup l
  WHERE NOT EXISTS (SELECT 1 FROM ios_ffs_file_inventory f WHERE f.source_id=l.source_id LIMIT 1)
), lookup_sources AS (
  SELECT DISTINCT source_id FROM files
), ctx AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         substr(MAX(field_value),1,4000) AS spotlight_text_context_sample
  FROM raw_key_values
  WHERE field_name='__spotlight_investigator_text_context'
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.reference_id,r.source_id,r.store_guid,r.source_db,r.inode_num,r.store_id,r.parent_inode_num,r.field_name,
       r.raw_reference_value,r.reference_type,r.normalized_ios_path,
       CASE
         WHEN r.field_name LIKE '%Thumbnail%' OR r.normalized_ios_path LIKE '%/brandthumbs/%' OR r.normalized_ios_path LIKE '%/thumbnail%' OR r.normalized_ios_path LIKE '%/thumbnails/%' THEN 'LOW_APP_THUMBNAIL_OR_CACHE_REFERENCE'
         WHEN r.field_name='kMDItemAttachmentPaths' OR r.normalized_ios_path LIKE '%/sms/attachments/%' THEN 'MESSAGE_ATTACHMENT_REFERENCE'
         WHEN r.field_name LIKE 'com_apple_mobilesms_%' THEN 'MESSAGES_PLUGIN_OR_LINK_PREVIEW_REFERENCE'
         WHEN r.field_name='kMDItemContentURL' THEN 'CONTENT_URL_REFERENCE'
         WHEN r.normalized_ios_path LIKE '%/documents/%' THEN 'APP_DOCUMENT_OR_USER_CONTENT_REFERENCE'
         ELSE 'GENERAL_SPOTLIGHT_PATH_REFERENCE'
       END AS missing_candidate_category,
       CASE
         WHEN r.field_name='kMDItemAttachmentPaths' OR r.normalized_ios_path LIKE '%/sms/attachments/%' OR r.field_name LIKE 'com_apple_mobilesms_%' THEN 'HIGH_INVESTIGATIVE_VALUE'
         WHEN r.field_name LIKE '%Thumbnail%' OR r.normalized_ios_path LIKE '%/brandthumbs/%' OR r.normalized_ios_path LIKE '%/thumbnail%' OR r.normalized_ios_path LIKE '%/thumbnails/%' THEN 'LOW_INVESTIGATIVE_VALUE'
         ELSE 'MEDIUM_INVESTIGATIVE_VALUE'
       END AS investigative_priority,
       CASE
         WHEN r.field_name='kMDItemAttachmentPaths' OR r.normalized_ios_path LIKE '%/sms/attachments/%' OR r.field_name LIKE 'com_apple_mobilesms_%' THEN 1
         WHEN r.field_name='kMDItemContentURL' THEN 3
         WHEN r.normalized_ios_path LIKE '%/documents/%' THEN 4
         WHEN r.field_name LIKE '%Thumbnail%' OR r.normalized_ios_path LIKE '%/brandthumbs/%' OR r.normalized_ios_path LIKE '%/thumbnail%' OR r.normalized_ios_path LIKE '%/thumbnails/%' THEN 9
         ELSE 5
       END AS investigative_priority_sort,
       CASE
         WHEN r.field_name LIKE '%Thumbnail%' OR r.normalized_ios_path LIKE '%/brandthumbs/%' THEN 'Likely app thumbnail/brand/cache reference; useful for app/content context but lower deletion value than user document or attachment paths.'
         WHEN r.field_name='kMDItemAttachmentPaths' OR r.normalized_ios_path LIKE '%/sms/attachments/%' THEN 'Message/attachment path recovered from Spotlight and absent from available FFS lookup; prioritize for deleted/unresolved attachment review.'
         WHEN r.field_name LIKE 'com_apple_mobilesms_%' THEN 'Messages link-preview/plugin path recovered from Spotlight; prioritize as communication-context evidence.'
         WHEN r.field_name='kMDItemContentURL' THEN 'ContentURL recovered from Spotlight; review text context and app/container before treating absence from FFS as deletion.'
         ELSE 'Spotlight path reference absent from available FFS lookup; review text context, source store, and acquisition scope.'
       END AS investigative_reason,
       COALESCE(ctx.spotlight_text_context_sample,'') AS spotlight_text_context_sample,
       CASE WHEN COALESCE(ctx.spotlight_text_context_sample,'')<>'' THEN 'TEXT_CONTEXT_RECOVERED_FROM_SAME_SPOTLIGHT_RECORD' ELSE 'NO_TEXT_CONTEXT_RECOVERED_IN_COMPACT_MODE' END AS spotlight_text_context_status,
       COALESCE(f.file_name,'') AS matched_file_name,COALESCE(f.size_bytes,0) AS matched_size_bytes,COALESCE(f.zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.protection_class_hint,'') AS matched_protection_class,COALESCE(f.app_container_hint,'') AS matched_app_container,COALESCE(f.domain_hint,'') AS matched_domain,
       COALESCE(f.lookup_source,'') AS ffs_lookup_source,
       'FFS_LOOKUP_AVAILABLE' AS ffs_lookup_status,
       'SPOTLIGHT_ONLY_FILE_MISSING_OR_UNRESOLVED' AS residency_status,
       CASE
         WHEN r.field_name='kMDItemAttachmentPaths' OR r.normalized_ios_path LIKE '%/sms/attachments/%' OR r.field_name LIKE 'com_apple_mobilesms_%' THEN 'HIGH_PATH_ABSENT_FROM_FFS_LOOKUP'
         WHEN r.field_name LIKE '%Thumbnail%' OR r.normalized_ios_path LIKE '%/brandthumbs/%' OR r.normalized_ios_path LIKE '%/thumbnail%' OR r.normalized_ios_path LIKE '%/thumbnails/%' THEN 'LOW_APP_CACHE_OR_THUMBNAIL_PATH_ABSENT_FROM_FFS_LOOKUP'
         ELSE 'MEDIUM_PATH_ABSENT_FROM_FFS_LOOKUP'
       END AS confidence,
       'Missing means absent from the full FFS inventory or the slim FFS path lookup available in this case. The text_context_sample is recovered from the same Spotlight record to help assess investigative value; absence from FFS does not by itself prove user deletion or app-level deletion.' AS interpretation_note
FROM refs r
JOIN lookup_sources ls ON ls.source_id=r.source_id
LEFT JOIN files f ON f.source_id=r.source_id AND f.normalized_path=r.normalized_ios_path
LEFT JOIN ctx ON ctx.source_id=r.source_id AND ctx.store_guid=r.store_guid AND ctx.source_db=r.source_db AND ctx.inode_num=r.inode_num AND COALESCE(ctx.store_id,'')=COALESCE(r.store_id,'')
WHERE f.normalized_path IS NULL;

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_summary;
CREATE VIEW vw_ios_spotlight_missing_from_ffs_summary AS
SELECT source_id,store_guid,source_db,field_name,reference_type,missing_candidate_category,investigative_priority,investigative_priority_sort,spotlight_text_context_status,ffs_lookup_status,
       COUNT(*) AS missing_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_object_count,
       COUNT(DISTINCT normalized_ios_path) AS distinct_missing_path_count,
       MIN(normalized_ios_path) AS min_missing_path_sample,
       MAX(normalized_ios_path) AS max_missing_path_sample,
       substr(MAX(spotlight_text_context_sample),1,4000) AS spotlight_text_context_sample,
       MAX(investigative_reason) AS investigative_reason,
       'Compact normal-mode Missing From FFS summary. Priority/category fields keep likely app thumbnail/cache noise from dominating investigator review.' AS interpretation_note
FROM vw_ios_spotlight_missing_from_ffs_candidates
GROUP BY source_id,store_guid,source_db,field_name,reference_type,missing_candidate_category,investigative_priority,investigative_priority_sort,spotlight_text_context_status,ffs_lookup_status;

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_high_value_candidates;
CREATE VIEW vw_ios_spotlight_missing_from_ffs_high_value_candidates AS
SELECT *
FROM vw_ios_spotlight_missing_from_ffs_candidates
WHERE investigative_priority IN ('HIGH_INVESTIGATIVE_VALUE','MEDIUM_INVESTIGATIVE_VALUE')
  AND missing_candidate_category <> 'LOW_APP_THUMBNAIL_OR_CACHE_REFERENCE';

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_high_value_summary;
CREATE VIEW vw_ios_spotlight_missing_from_ffs_high_value_summary AS
SELECT source_id,store_guid,source_db,field_name,reference_type,missing_candidate_category,investigative_priority,investigative_priority_sort,spotlight_text_context_status,ffs_lookup_status,
       COUNT(*) AS missing_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_object_count,
       COUNT(DISTINCT normalized_ios_path) AS distinct_missing_path_count,
       MIN(normalized_ios_path) AS min_missing_path_sample,
       MAX(normalized_ios_path) AS max_missing_path_sample,
       substr(MAX(spotlight_text_context_sample),1,4000) AS spotlight_text_context_sample,
       MAX(investigative_reason) AS investigative_reason,
       'High/medium-priority subset of Missing From FFS candidates, excluding likely thumbnail/brand/cache-only references. Use this first before the full candidate view.' AS interpretation_note
FROM vw_ios_spotlight_missing_from_ffs_high_value_candidates
GROUP BY source_id,store_guid,source_db,field_name,reference_type,missing_candidate_category,investigative_priority,investigative_priority_sort,spotlight_text_context_status,ffs_lookup_status;


DROP VIEW IF EXISTS vw_ios_spotlight_text_context_review;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_text_context_review AS
WITH base AS (
  SELECT kv.raw_kv_id,
         COALESCE(rr.raw_record_id,0) AS raw_record_id,
         kv.source_id,
         kv.store_guid,
         kv.source_db,
         kv.inode_num AS spotlight_inode_or_object_id,
         kv.store_id AS spotlight_store_id,
         kv.parent_inode_num,
         rr.file_name,
         rr.display_name,
         rr.content_type,
         rr.last_updated_utc,
         kv.field_value AS spotlight_text_context_sample,
         lower(COALESCE(kv.field_value,'')) AS v,
         lower(COALESCE(rr.content_type,'')) AS ct
  FROM raw_key_values kv
  LEFT JOIN raw_records rr ON rr.source_id=kv.source_id AND rr.store_guid=kv.store_guid AND rr.source_db=kv.source_db AND rr.inode_num=kv.inode_num AND COALESCE(rr.store_id,'')=COALESCE(kv.store_id,'')
  WHERE kv.field_name='__spotlight_investigator_text_context'
), labeled AS (
  SELECT *,
       CASE
         WHEN ct='public.message' OR v LIKE '%kmditemmessageservice%' OR v LIKE '%/sms/attachments/%' OR v LIKE '%com.apple.mobilesms%' THEN 'MESSAGE_OR_ATTACHMENT_CONTEXT'
         WHEN ct='public.email-message' OR v LIKE '%com_apple_mail_%' OR v LIKE '%kmditememail%' OR v LIKE '%from:%' OR v LIKE '%to:%' THEN 'MAIL_OR_EMAIL_CONTEXT'
         WHEN v LIKE '%_kmditembundleid=net.whatsapp.whatsapp%' OR v LIKE '%_kmditemexternalid=net.whatsapp.whatsapp%' OR v LIKE '%_kmditemdomainidentifier=net.whatsapp%' OR v LIKE '%_kmditemalternatenames=whatsapp%' THEN 'WHATSAPP_APP_OR_CHAT_CONTEXT'
         WHEN v LIKE '%_kmditembundleid=org.whispersystems.signal%' OR v LIKE '%_kmditemexternalid=org.whispersystems.signal%' OR v LIKE '%_kmditemdomainidentifier=org.whispersystems.signal%' OR v LIKE '%signal.messenger%' THEN 'SIGNAL_APP_OR_CHAT_CONTEXT'
         WHEN v LIKE '%_kmditembundleid=ph.telegra.telegraph%' OR v LIKE '%_kmditemexternalid=ph.telegra.telegraph%' OR v LIKE '%telegram.messenger%' OR v LIKE '%org.telegram%' THEN 'TELEGRAM_APP_OR_CHAT_CONTEXT'
         WHEN v LIKE '%whatsapp group%' OR v LIKE '%chat.whatsapp.com%' OR v LIKE '%wa.me/%' OR v LIKE '%api.whatsapp.com%' THEN 'WHATSAPP_LINK_OR_TEXT_MENTION'
         WHEN v LIKE '%t.me/%' OR v LIKE '%telegram.me/%' THEN 'TELEGRAM_LINK_OR_TEXT_MENTION'
         WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' OR v LIKE '%kmditemcontenturl%' THEN 'URL_OR_WEB_CONTEXT'
         WHEN ct='kspotlightitemtypecall' OR v LIKE '%tel://%' OR v LIKE '%callbackurl=tel%' THEN 'CALL_LOG_OR_PHONE_CONTEXT'
         WHEN ct='public.contact' OR v LIKE '%kmditemphonenumbers%' OR v LIKE '%contact%' THEN 'CONTACT_CONTEXT'
         WHEN ct='public.calendar-event' OR v LIKE '%calendar%' OR v LIKE '%event%' OR v LIKE '%.ics%' THEN 'CALENDAR_OR_EVENT_CONTEXT'
         WHEN ct LIKE 'public.%image%' OR ct IN ('public.jpeg','public.heic','public.png','com.compuserve.gif') THEN 'PHOTO_OR_MEDIA_CONTEXT'
         WHEN ct LIKE '%movie%' OR ct LIKE '%video%' OR ct IN ('public.mpeg-4','public.3gpp') THEN 'VIDEO_OR_MEDIA_CONTEXT'
         WHEN v LIKE '%/var/mobile/%' OR v LIKE '%file://%' THEN 'LOCAL_FILE_OR_PATH_CONTEXT'
         WHEN v LIKE '%@%' THEN 'ACCOUNT_OR_IDENTIFIER_CONTEXT'
         ELSE 'GENERAL_SPOTLIGHT_TEXT_CONTEXT'
       END AS text_context_category
  FROM base
), scored AS (
  SELECT *,
       CASE
         WHEN text_context_category IN ('MESSAGE_OR_ATTACHMENT_CONTEXT','MAIL_OR_EMAIL_CONTEXT') THEN 'HIGH_SPOTLIGHT_TEXT_VALUE'
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 'HIGH_APP_ATTRIBUTION_VALUE'
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 'MEDIUM_CHAT_LINK_OR_TEXT_VALUE'
         WHEN text_context_category IN ('URL_OR_WEB_CONTEXT','CALL_LOG_OR_PHONE_CONTEXT','CONTACT_CONTEXT','CALENDAR_OR_EVENT_CONTEXT','LOCAL_FILE_OR_PATH_CONTEXT','ACCOUNT_OR_IDENTIFIER_CONTEXT') THEN 'MEDIUM_SPOTLIGHT_TEXT_VALUE'
         WHEN text_context_category IN ('PHOTO_OR_MEDIA_CONTEXT','VIDEO_OR_MEDIA_CONTEXT') THEN 'MEDIUM_MEDIA_CONTEXT_VALUE'
         ELSE 'LOW_GENERAL_TEXT_VALUE'
       END AS review_priority,
       CASE
         WHEN text_context_category='MESSAGE_OR_ATTACHMENT_CONTEXT' THEN 1
         WHEN text_context_category='MAIL_OR_EMAIL_CONTEXT' THEN 2
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 3
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 9
         WHEN text_context_category='URL_OR_WEB_CONTEXT' THEN 4
         WHEN text_context_category='CALL_LOG_OR_PHONE_CONTEXT' THEN 5
         WHEN text_context_category='CONTACT_CONTEXT' THEN 6
         WHEN text_context_category='CALENDAR_OR_EVENT_CONTEXT' THEN 7
         WHEN text_context_category='LOCAL_FILE_OR_PATH_CONTEXT' THEN 8
         WHEN text_context_category='ACCOUNT_OR_IDENTIFIER_CONTEXT' THEN 10
         WHEN text_context_category IN ('PHOTO_OR_MEDIA_CONTEXT','VIDEO_OR_MEDIA_CONTEXT') THEN 10
         ELSE 20
       END AS review_priority_sort,
       CASE
         WHEN text_context_category='MESSAGE_OR_ATTACHMENT_CONTEXT' THEN 'Same-record Spotlight context indicates message, SMS/RCS/iMessage, or attachment evidence; prioritize for communications review and Missing From FFS triage.'
         WHEN text_context_category='MAIL_OR_EMAIL_CONTEXT' THEN 'Same-record Spotlight context indicates mail/email content or identifiers; prioritize for communication and account review.'
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 'Same-record Spotlight context contains explicit bundle/domain/external-id evidence for a chat application; prioritize for app attribution and indexed-only/deleted candidate review.'
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 'Same-record Spotlight context contains a chat-app link or textual mention only; review as possible communication/link evidence, not as installed-app attribution by itself.'
         WHEN text_context_category='URL_OR_WEB_CONTEXT' THEN 'Same-record Spotlight context includes URL/web content; review for browsing, shared links, or app deep-link evidence.'
         WHEN text_context_category='CALL_LOG_OR_PHONE_CONTEXT' THEN 'Same-record Spotlight context indicates call/phone callback evidence; correlate with CallHistory if support parsing is enabled.'
         WHEN text_context_category='CONTACT_CONTEXT' THEN 'Same-record Spotlight context indicates contact/person/account metadata.'
         WHEN text_context_category='CALENDAR_OR_EVENT_CONTEXT' THEN 'Same-record Spotlight context indicates calendar or event-like metadata.'
)VSGUI",
        R"VSGUI(         WHEN text_context_category IN ('PHOTO_OR_MEDIA_CONTEXT','VIDEO_OR_MEDIA_CONTEXT') THEN 'Same-record Spotlight context indicates media; review with dates/path context before drawing usage conclusions.'
         ELSE 'General same-record Spotlight text context retained in compact normal mode.'
       END AS text_context_reason
  FROM labeled
)
SELECT raw_kv_id,raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       file_name,display_name,content_type,last_updated_utc,text_context_category,review_priority,review_priority_sort,text_context_reason,
       CASE
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 'EXPLICIT_CHAT_APP_BUNDLE_DOMAIN_OR_EXTERNAL_ID'
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 'CHAT_LINK_OR_TEXT_MENTION_ONLY_NOT_APP_ATTRIBUTION'
         WHEN text_context_category='MESSAGE_OR_ATTACHMENT_CONTEXT' THEN 'APPLE_MESSAGE_CONTENT_TYPE_OR_MOBILESMS_FIELD'
         WHEN text_context_category='MAIL_OR_EMAIL_CONTEXT' THEN 'MAIL_CONTENT_TYPE_OR_MAIL_SEARCH_INDEXER_FIELD'
         ELSE 'CATEGORY_BY_CONTENT_TYPE_FIELD_OR_VALUE_PATTERN'
       END AS classification_evidence,
       spotlight_text_context_sample,
       'Compact same-record Spotlight text context retained in normal iOS mode; this is not a full raw property dump. Use raw_kv_id/raw_record_id/source_db to validate against the local SQLite database.' AS interpretation_note
FROM scored;

DROP VIEW IF EXISTS vw_ios_spotlight_high_value_text_context_review;
CREATE VIEW vw_ios_spotlight_high_value_text_context_review AS
SELECT *
FROM vw_ios_spotlight_text_context_review
WHERE review_priority_sort <= 9
ORDER BY review_priority_sort,last_updated_utc DESC,raw_record_id DESC;

DROP VIEW IF EXISTS vw_ios_spotlight_text_context_priority_summary;
CREATE VIEW vw_ios_spotlight_text_context_priority_summary AS
SELECT text_context_category,review_priority,review_priority_sort,
       COUNT(*) AS text_context_record_count,
       COUNT(DISTINCT source_id || ':' || store_guid || ':' || COALESCE(spotlight_inode_or_object_id,'') || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       COUNT(NULLIF(last_updated_utc,'')) AS rows_with_last_updated,
       MIN(last_updated_utc) AS earliest_last_updated_utc,
       MAX(last_updated_utc) AS latest_last_updated_utc,
       substr(MIN(spotlight_text_context_sample),1,1000) AS min_text_context_sample,
       substr(MAX(spotlight_text_context_sample),1,1000) AS max_text_context_sample,
       MIN(classification_evidence) AS classification_evidence,
       MIN(text_context_reason) AS text_context_reason,
       'Priority summary for compact same-record Spotlight text retained in normal iOS mode. Priorities are triage labels, not final Apple schema classifications.' AS interpretation_note
FROM vw_ios_spotlight_text_context_review
GROUP BY text_context_category,review_priority,review_priority_sort;

DROP VIEW IF EXISTS vw_ios_spotlight_chat_app_attribution_summary;
CREATE VIEW vw_ios_spotlight_chat_app_attribution_summary AS
SELECT text_context_category,review_priority,classification_evidence,
       COUNT(*) AS context_record_count,
       COUNT(DISTINCT source_id || ':' || store_guid || ':' || COALESCE(spotlight_inode_or_object_id,'') || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       MIN(last_updated_utc) AS earliest_last_updated_utc,
       MAX(last_updated_utc) AS latest_last_updated_utc,
       substr(MIN(spotlight_text_context_sample),1,1000) AS min_text_context_sample,
       substr(MAX(spotlight_text_context_sample),1,1000) AS max_text_context_sample,
       MIN(text_context_reason) AS text_context_reason,
       'V0_9_26 separates explicit chat-app bundle/domain/external-id attribution from plain keyword/link mentions so words like Signal Hill do not inflate Signal app evidence.' AS interpretation_note
FROM vw_ios_spotlight_text_context_review
WHERE text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT','WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION')
GROUP BY text_context_category,review_priority,classification_evidence;

DROP VIEW IF EXISTS vw_ios_spotlight_human_text_values;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_human_text_values AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,store_path,inode_num,store_id,parent_inode_num,field_name,field_value,
         LOWER(COALESCE(field_value,'')) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), labeled AS (
  SELECT *,
    CASE
      WHEN v LIKE '%file://%' OR v LIKE '%/private/var/mobile/%' OR v LIKE '%/var/mobile/%' THEN 'FILE_PATH_OR_ATTACHMENT'
      WHEN v LIKE '%zoom.%' OR v LIKE '%meet.google.%' OR v LIKE '%teams.microsoft.%' OR v LIKE '%webex.%' THEN 'MEETING_OR_CONFERENCE'
      WHEN v LIKE '%.ics%' OR v LIKE '%text/calendar%' OR v LIKE '%vevent%' OR v LIKE '%calendar.google.com%' OR v LIKE '%/calendar/%' THEN 'CALENDAR_OR_INVITATION'
      WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'WEB_OR_URL'
      WHEN v LIKE 'from:%' OR v LIKE 'to:%' OR v LIKE 'cc:%' OR v LIKE '%mailto:%' OR (v LIKE '%@%' AND v NOT LIKE '%http://%' AND v NOT LIKE '%https://%') THEN 'EMAIL_OR_ACCOUNT_TEXT'
      WHEN v LIKE '%net.whatsapp.whatsapp%' OR v LIKE '%chat.whatsapp.com%' OR v LIKE '%wa.me/%' OR v LIKE '%api.whatsapp.com%' OR v LIKE '%whatsapp group%' THEN 'WHATSAPP_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.whispersystems.signal%' OR v LIKE '%signal.messenger%' OR v LIKE '%signal.org/%' THEN 'SIGNAL_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.telegram%' OR v LIKE '%telegram.messenger%' OR v LIKE '%t.me/%' OR v LIKE '%telegram.me/%' THEN 'TELEGRAM_TEXT_OR_REFERENCE'
      ELSE 'OTHER_HUMAN_READABLE_TEXT'
    END AS human_text_category,
    TRIM(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(COALESCE(field_value,''),
      char(13),' '), char(10),' '), char(9),' '), '<br>',' '), '<br/>',' '), '<br />',' '),
      '&amp;','&'), '&lt;','<'), '&gt;','>'), '&nbsp;',' '), '&quot;','"')) AS readable_text
  FROM probes
)
SELECT l.raw_kv_id,COALESCE(r.raw_record_id,0) AS raw_record_id,l.source_id,l.store_guid,l.source_db,l.inode_num,l.store_id,l.parent_inode_num,l.field_name,
       l.human_text_category,
       LENGTH(COALESCE(l.field_value,'')) AS original_value_length,
       substr(l.readable_text,1,3000) AS readable_text_sample,
       CASE
         WHEN l.human_text_category IN ('FILE_PATH_OR_ATTACHMENT','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION','WEB_OR_URL','EMAIL_OR_ACCOUNT_TEXT') THEN 'HIGH_HUMAN_REVIEW_VALUE'
         WHEN LENGTH(l.readable_text)>=24 THEN 'MEDIUM_HUMAN_REVIEW_VALUE'
         ELSE 'LOW_HUMAN_REVIEW_VALUE'
       END AS review_priority,
       'Generic iOS CoreSpotlight text recovery; formal CoreSpotlight property names/dbStr maps remain a later parser phase.' AS interpretation_note
FROM labeled l
LEFT JOIN (
  SELECT source_id,store_guid,source_db,inode_num,COALESCE(store_id,'') AS store_id_key,MIN(raw_record_id) AS raw_record_id
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,inode_num,COALESCE(store_id,'')
) r ON r.source_id=l.source_id AND r.store_guid=l.store_guid AND r.source_db=l.source_db AND r.inode_num=l.inode_num AND r.store_id_key=COALESCE(l.store_id,'')
WHERE LENGTH(l.readable_text)>=4;

DROP VIEW IF EXISTS vw_ios_spotlight_human_text_rollup;
CREATE VIEW vw_ios_spotlight_human_text_rollup AS
WITH text_values AS (
  SELECT v.*, r.last_updated_utc, r.record_state
  FROM vw_ios_spotlight_human_text_values v
  LEFT JOIN raw_records r ON r.raw_record_id=v.raw_record_id
)
SELECT raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT human_text_category) AS distinct_text_category_count,
       GROUP_CONCAT(DISTINCT human_text_category) AS human_text_categories,
       MAX(CASE WHEN review_priority='HIGH_HUMAN_REVIEW_VALUE' THEN 1 ELSE 0 END) AS has_high_review_value_text,
       MIN(NULLIF(last_updated_utc,'')) AS last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       substr(GROUP_CONCAT(field_name || '=' || readable_text_sample, ' || '),1,6000) AS readable_text_rollup_sample,
       'Record-level human-readable text rollup from iOS CoreSpotlight string probes.' AS interpretation_note
FROM text_values
GROUP BY raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num;


DROP VIEW IF EXISTS vw_ios_spotlight_investigative_items_with_dates;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_investigative_items_with_dates AS
SELECT v.raw_kv_id,
       v.raw_record_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.original_value_length,
       v.readable_text_sample,
       v.review_priority,
       dp.spotlight_date_utc,
       dp.spotlight_date_source_field,
       dp.spotlight_date_source_table,
       dp.spotlight_date_raw_value,
       dp.spotlight_date_parse_method,
       dp.spotlight_date_type,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%creation%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%created%' THEN 'created_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modification%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modified%' THEN 'modified_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%access%' THEN 'accessed_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%open%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'metadata_seen_or_index_updated'
         ELSE 'unclassified_spotlight_date_candidate'
       END AS spotlight_date_semantic_class,
       dp.spotlight_date_source_evidence,
       dp.date_validation_hint,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'Do not report as created/modified/accessed/opened. This is CoreSpotlight metadata/index update timing unless another decoded field supports activity semantics.'
         WHEN COALESCE(dp.spotlight_date_source_field,'')<>'' THEN 'Report only with the listed raw Spotlight source field, raw value, parse method, and validation hint.'
         ELSE 'No direct Spotlight date was recovered for this text value.'
       END AS date_reporting_caution,
       'Spotlight/CoreSpotlight extracted text item with attached date provenance where directly linkable by raw_record_id. FFS/app database data is supporting context only.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
LEFT JOIN vw_ios_spotlight_date_provenance dp ON dp.raw_record_id=v.raw_record_id;

DROP VIEW IF EXISTS vw_ios_spotlight_date_field_summary;
CREATE VIEW vw_ios_spotlight_date_field_summary AS
WITH dates AS (
  SELECT source_id, store_guid, source_db, field_name, parse_method, date_type,
         parsed_utc, field_value, inode_num, store_id,
         CASE
           WHEN lower(COALESCE(field_name,'')) LIKE '%creation%' OR lower(COALESCE(field_name,'')) LIKE '%created%' THEN 'created_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%modification%' OR lower(COALESCE(field_name,'')) LIKE '%modified%' THEN 'modified_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%access%' THEN 'accessed_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%open%' OR lower(COALESCE(field_name,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
           WHEN lower(COALESCE(field_name,''))='last_updated' THEN 'metadata_seen_or_index_updated'
           ELSE 'unclassified_spotlight_date_candidate'
         END AS spotlight_date_semantic_class
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
)
SELECT source_id, store_guid, source_db, field_name AS spotlight_date_source_field,
       spotlight_date_semantic_class, COALESCE(date_type,'') AS raw_date_type,
       COALESCE(parse_method,'') AS parse_method,
       COUNT(*) AS date_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_record_count,
       MIN(parsed_utc) AS earliest_parsed_utc,
       MAX(parsed_utc) AS latest_parsed_utc,
       substr(MIN(field_value),1,500) AS min_raw_value_sample,
       substr(MAX(field_value),1,500) AS max_raw_value_sample,
       CASE
         WHEN spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'CoreSpotlight metadata/index update timing; do not report as created/modified/accessed/opened usage without another decoded field.'
         WHEN spotlight_date_semantic_class LIKE '%candidate' THEN 'Candidate semantic class inferred from Spotlight raw date field name; validate field meaning before reporting.'
         ELSE 'Unclassified Spotlight date candidate; validate against raw_date_candidates and source Store-V2 record.'
       END AS reporting_caution
FROM dates
GROUP BY source_id, store_guid, source_db, field_name, spotlight_date_semantic_class, date_type, parse_method;

DROP VIEW IF EXISTS vw_ios_spotlight_investigative_item_date_evidence;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_investigative_item_date_evidence AS
WITH date_evidence AS (
  SELECT raw_date_id, source_id, store_guid, source_db, store_path, inode_num, store_id,
         parent_inode_num, file_name, best_path,
         field_name AS spotlight_date_source_field,
         field_value AS spotlight_date_raw_value,
         parsed_utc AS spotlight_date_utc,
         parse_method AS spotlight_date_parse_method,
         date_type AS spotlight_date_type,
         association_status,
         association_confidence,
         CASE
           WHEN lower(COALESCE(field_name,'')) LIKE '%creation%' OR lower(COALESCE(field_name,'')) LIKE '%created%' THEN 'created_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%modification%' OR lower(COALESCE(field_name,'')) LIKE '%modified%' THEN 'modified_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%access%' THEN 'accessed_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%open%' OR lower(COALESCE(field_name,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
           WHEN lower(COALESCE(field_name,''))='last_updated' THEN 'metadata_seen_or_index_updated'
           ELSE 'unclassified_spotlight_date_candidate'
         END AS spotlight_date_semantic_class
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
)
SELECT v.raw_kv_id,
       v.raw_record_id,
       d.raw_date_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.review_priority,
       v.original_value_length,
       v.readable_text_sample,
       d.spotlight_date_utc,
       d.spotlight_date_source_field,
       'raw_date_candidates' AS spotlight_date_source_table,
       d.spotlight_date_raw_value,
       d.spotlight_date_parse_method,
       d.spotlight_date_type,
       d.spotlight_date_semantic_class,
       d.association_status AS date_association_status,
       d.association_confidence AS date_association_confidence,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(v.raw_kv_id AS TEXT),'') || '; field_name=' || COALESCE(v.field_name,'') AS value_validation_locator,
       'raw_date_candidates.raw_date_id=' || COALESCE(CAST(d.raw_date_id AS TEXT),'') || '; field_name=' || COALESCE(d.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(d.spotlight_date_raw_value,'') || '; parsed_utc=' || COALESCE(d.spotlight_date_utc,'') || '; parse_method=' || COALESCE(d.spotlight_date_parse_method,'') AS date_validation_locator,
       'source_db=' || COALESCE(v.source_db,'') || '; store_guid=' || COALESCE(v.store_guid,'') || '; raw_record_id=' || COALESCE(CAST(v.raw_record_id AS TEXT),'') || '; inode_or_object_id=' || COALESCE(v.inode_num,'') || '; store_id=' || COALESCE(v.store_id,'') AS spotlight_record_locator,
       CASE
         WHEN d.spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'Date is linked to this Spotlight record but represents CoreSpotlight metadata/index update timing unless a separately decoded field supports user activity.'
         WHEN d.spotlight_date_semantic_class IN ('created_date_candidate','modified_date_candidate','accessed_date_candidate','opened_or_used_date_candidate') THEN 'Date is directly linked to this recovered Spotlight value through the same Store-V2 record; validate raw field semantics before reporting as activity.'
         ELSE 'Date is directly linked to this recovered Spotlight value through the same Store-V2 record, but semantic meaning is not yet classified.'
       END AS date_reporting_caution,
       'Each row links one recovered human-readable Spotlight value to one raw Spotlight date candidate from the same Store-V2 record. Use validation locator fields to verify in the parsed raw tables and original store.db.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
JOIN date_evidence d ON d.source_id=v.source_id
                    AND d.store_guid=v.store_guid
                    AND d.source_db=v.source_db
                    AND d.inode_num=v.inode_num
                    AND COALESCE(d.store_id,'')=COALESCE(v.store_id,'');

)VSGUI"
    });

    execGuiSql(R"SQL(

DROP VIEW IF EXISTS vw_ios_spotlight_high_value_timeline;
CREATE VIEW vw_ios_spotlight_high_value_timeline AS
WITH base AS (
  SELECT * FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE review_priority IN ('HIGH_HUMAN_REVIEW_VALUE','MEDIUM_HUMAN_REVIEW_VALUE')
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.original_value_length,
       b.readable_text_sample,b.review_priority,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_source_table,
       b.spotlight_date_raw_value,b.spotlight_date_parse_method,b.spotlight_date_type,
       b.spotlight_date_semantic_class,b.date_validation_hint,b.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FILE_PATH_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       CASE
         WHEN b.spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'SPOTLIGHT_INDEX_TIME_WITH_VALUE_CONTEXT'
         WHEN b.spotlight_date_semantic_class LIKE '%candidate' THEN 'SPOTLIGHT_ACTIVITY_DATE_CANDIDATE_WITH_VALUE_CONTEXT'
         ELSE 'SPOTLIGHT_VALUE_WITH_UNCLASSIFIED_DATE_CONTEXT'
       END AS investigative_timeline_basis,
       'Spotlight-first high-value timeline. FFS and app database fields are context/corroboration only; the Spotlight value/date fields remain the primary evidence to validate.' AS interpretation_note
FROM base b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_file_reference_review;
CREATE VIEW vw_ios_spotlight_file_reference_review AS
WITH ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.readable_text_sample AS spotlight_file_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(f.residency_status,'NO_EXACT_FFS_PATH_LINK') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       CASE WHEN COALESCE(f.residency_status,'')='PRESENT_AS_FILE_IN_FFS' THEN 'SPOTLIGHT_PATH_PRESENT_IN_FFS_INVENTORY'
            WHEN COALESCE(f.residency_status,'')<>'' THEN f.residency_status
            ELSE 'SPOTLIGHT_FILE_REFERENCE_NO_EXACT_FFS_MATCH_IN_CURRENT_LINK_VIEW' END AS file_reference_status,
       'Spotlight/CoreSpotlight file/path reference with date provenance. FFS presence supports current-file existence only and is not proof of use or deletion by itself.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
WHERE b.human_text_category='FILE_PATH_OR_ATTACHMENT';

)SQL");

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_url_reference_review;
CREATE VIEW vw_ios_spotlight_url_reference_review AS
WITH vals AS (
  SELECT *, lower(COALESCE(readable_text_sample,'')) AS v
  FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION')
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT v.raw_kv_id,v.raw_record_id,v.source_id,v.store_guid,v.source_db,
       v.spotlight_inode_or_object_id,v.spotlight_store_id,v.parent_inode_num,
       v.spotlight_value_source_field,v.human_text_category,v.readable_text_sample AS spotlight_url_or_web_reference,
       CASE
         WHEN instr(v.v,'https://')>0 THEN substr(v.v,instr(v.v,'https://'),300)
         WHEN instr(v.v,'http://')>0 THEN substr(v.v,instr(v.v,'http://'),300)
         WHEN instr(v.v,'www.')>0 THEN substr(v.v,instr(v.v,'www.'),300)
         ELSE substr(v.v,1,300)
       END AS normalized_url_reference_sample,
       v.spotlight_date_utc,v.spotlight_date_source_field,v.spotlight_date_raw_value,
       v.spotlight_date_parse_method,v.spotlight_date_semantic_class,v.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       'Spotlight/CoreSpotlight URL/web-like reference with date provenance. Browser/app database fields are supporting context and not an exact value match unless separately validated.' AS interpretation_note
FROM vals v
LEFT JOIN app a ON a.candidate_id=v.raw_kv_id;

)SQL");

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_account_contact_reference_review;
CREATE VIEW vw_ios_spotlight_account_contact_reference_review AS
WITH app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.readable_text_sample AS spotlight_account_or_contact_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       'Spotlight/CoreSpotlight account/contact-like reference with date provenance. Treat as a Spotlight value first; app database context is family-level unless exact string matching is later added.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id
WHERE b.human_text_category IN ('EMAIL_OR_ACCOUNT_TEXT','WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE');

DROP VIEW IF EXISTS vw_ios_spotlight_decode_gap_summary;
CREATE VIEW vw_ios_spotlight_decode_gap_summary AS
WITH gaps AS (
  SELECT source_id,store_guid,source_db,decode_gap_status,last_updated_utc
  FROM vw_ios_spotlight_decode_gap_records
)
SELECT g.source_id,g.store_guid,g.source_db,g.decode_gap_status,
       COUNT(*) AS gap_record_count,
       MIN(NULLIF(g.last_updated_utc,'')) AS earliest_gap_last_updated_utc,
       MAX(NULLIF(g.last_updated_utc,'')) AS latest_gap_last_updated_utc,
       COALESCE(dc.raw_record_count,0) AS store_raw_record_count,
       COALESCE(dc.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(dc.human_text_value_count,0) AS human_text_value_count,
       COALESCE(dc.pct_records_with_human_text,'') AS pct_records_with_human_text,
       COALESCE(dc.decode_failures,0) AS native_decode_failures,
       COALESCE(dc.decode_status,'') AS native_decode_status,
       'Summary of Spotlight/CoreSpotlight records parsed at header level but lacking recovered key/value or human-readable text values. This is the primary native parser improvement target list.' AS interpretation_note
FROM gaps g
LEFT JOIN vw_ios_spotlight_decode_coverage_summary dc ON dc.source_id=g.source_id AND dc.store_guid=g.store_guid AND dc.source_db=g.source_db
GROUP BY g.source_id,g.store_guid,g.source_db,g.decode_gap_status;

)SQL");

    execGuiSqlParts({
        R"VSGUI(

DROP VIEW IF EXISTS vw_ios_spotlight_entity_review;
CREATE VIEW vw_ios_spotlight_entity_review AS
WITH base AS (
  SELECT b.*,
         lower(COALESCE(b.readable_text_sample,'')) AS lower_text
  FROM vw_ios_spotlight_investigative_items_with_dates b
  WHERE COALESCE(b.readable_text_sample,'')<>''
), typed AS (
  SELECT b.*,
         CASE
           WHEN b.human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION') THEN 'URL_OR_WEB_REFERENCE'
           WHEN b.human_text_category='FILE_PATH_OR_ATTACHMENT' THEN 'FILE_OR_ATTACHMENT_REFERENCE'
           WHEN b.human_text_category='EMAIL_OR_ACCOUNT_TEXT' THEN 'ACCOUNT_OR_EMAIL_REFERENCE'
           WHEN b.human_text_category IN ('WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE') THEN 'COMMUNICATION_APP_REFERENCE'
           WHEN b.human_text_category LIKE '%MESSAGE%' THEN 'MESSAGE_OR_COMMUNICATION_TEXT'
           ELSE 'OTHER_SPOTLIGHT_TEXT_REFERENCE'
         END AS entity_type
  FROM base b
), normalized AS (
  SELECT t.*,
         CASE
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'https://')>0 THEN substr(t.lower_text,instr(t.lower_text,'https://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'http://')>0 THEN substr(t.lower_text,instr(t.lower_text,'http://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'www.')>0 THEN substr(t.lower_text,instr(t.lower_text,'www.'),512)
           WHEN t.entity_type='FILE_OR_ATTACHMENT_REFERENCE' THEN replace(replace(replace(replace(t.lower_text,'file://',''),'<',''),'>',''),'\\','/')
           ELSE trim(t.lower_text)
         END AS normalized_entity_value
  FROM typed t
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,matched_record_category,matched_table_name,parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc,sample_parsed_value
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT n.raw_kv_id,
       n.raw_record_id,
       n.source_id,
       n.store_guid,
       n.source_db,
       n.spotlight_inode_or_object_id,
       n.spotlight_store_id,
       n.parent_inode_num,
       n.entity_type,
       n.human_text_category,
       n.review_priority,
       n.spotlight_value_source_field,
       n.normalized_entity_value,
       n.readable_text_sample,
       n.original_value_length,
       n.spotlight_date_utc,
       n.spotlight_date_source_field,
       n.spotlight_date_raw_value,
       n.spotlight_date_parse_method,
       n.spotlight_date_semantic_class,
       n.date_validation_hint,
       n.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FFS_LINK_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_LINK_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.matched_record_category,'') AS matched_record_category,
       COALESCE(a.matched_table_name,'') AS matched_table_name,
       COALESCE(a.sample_parsed_value,'') AS sample_app_db_value,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(n.raw_kv_id AS TEXT),'') || '; raw_records.raw_record_id=' || COALESCE(CAST(n.raw_record_id AS TEXT),'') || '; field_name=' || COALESCE(n.spotlight_value_source_field,'') AS reference_validation_locator,
       'raw_date_candidates.field_name=' || COALESCE(n.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(n.spotlight_date_raw_value,'') || '; parse_method=' || COALESCE(n.spotlight_date_parse_method,'') AS date_validation_locator,
       'Spotlight-first entity view. The entity/value and date columns originate from CoreSpotlight/Spotlight parsed records; app DB and FFS fields are corroborating context only.' AS interpretation_note
FROM normalized n
LEFT JOIN ffs f ON f.reference_id=n.raw_kv_id
LEFT JOIN app a ON a.candidate_id=n.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_entity_summary;
CREATE VIEW vw_ios_spotlight_entity_summary AS
SELECT entity_type,
       human_text_category,
       review_priority,
       store_guid,
       source_db,
       spotlight_value_source_field,
       spotlight_date_semantic_class,
       COUNT(*) AS entity_row_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT normalized_entity_value) AS distinct_normalized_entity_count,
       SUM(CASE WHEN ffs_residency_status='PRESENT_AS_FILE_IN_FFS' THEN 1 ELSE 0 END) AS ffs_present_context_count,
       SUM(CASE WHEN app_db_link_status LIKE 'PRESENT%' OR app_db_link_status LIKE '%PRESENT%' THEN 1 ELSE 0 END) AS app_db_present_context_count,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       MIN(substr(normalized_entity_value,1,240)) AS min_sample_entity,
       MAX(substr(normalized_entity_value,1,240)) AS max_sample_entity,
       'Spotlight entity summary. Counts are derived from recovered CoreSpotlight text/probe values and their Spotlight date provenance; app/FFS context is only supporting context.' AS interpretation_note
FROM vw_ios_spotlight_entity_review
GROUP BY entity_type,human_text_category,review_priority,store_guid,source_db,spotlight_value_source_field,spotlight_date_semantic_class;

DROP VIEW IF EXISTS vw_ios_spotlight_native_parser_targets;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_native_parser_targets AS
SELECT source_id,
       store_guid,
       source_db,
       'RECORDS_WITHOUT_RECOVERED_TEXT' AS parser_target_type,
       decode_gap_status AS target_name,
       gap_record_count AS target_count,
       store_raw_record_count AS store_raw_record_count,
       recovered_key_value_count AS recovered_key_value_count,
       human_text_value_count AS human_text_value_count,
       pct_records_with_human_text AS pct_records_with_human_text,
       native_decode_failures AS native_decode_failures,
       CASE WHEN gap_record_count>100000 THEN 'HIGH' WHEN gap_record_count>10000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Improve native CoreSpotlight property/dictionary/value decoding for records that parse at header level but do not yield recovered text/key-value rows.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_decode_gap_summary
UNION ALL
SELECT source_id,
       store_guid,
       source_db,
       'GENERIC_STRING_PROBE_FIELD' AS parser_target_type,
       field_name AS target_name,
       value_row_count AS target_count,
       NULL AS store_raw_record_count,
       value_row_count AS recovered_key_value_count,
       distinct_record_count AS human_text_value_count,
       '' AS pct_records_with_human_text,
       NULL AS native_decode_failures,
       CASE WHEN value_row_count>10000 THEN 'HIGH' WHEN value_row_count>1000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Map generic __native_core_probe_string_* fields back to real CoreSpotlight property names/types where possible.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_field_coverage_summary
WHERE field_decode_status='GENERIC_NATIVE_STRING_PROBE';

DROP VIEW IF EXISTS vw_ios_spotlight_decode_coverage_summary;
CREATE VIEW vw_ios_spotlight_decode_coverage_summary AS
WITH rr AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS raw_record_count,
         SUM(CASE WHEN COALESCE(last_updated_utc,'')<>'' THEN 1 ELSE 0 END) AS records_with_last_updated,
         MIN(NULLIF(last_updated_utc,'')) AS earliest_last_updated_utc,
         MAX(NULLIF(last_updated_utc,'')) AS latest_last_updated_utc
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), kv AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS recovered_key_value_count,
         COUNT(DISTINCT field_name) AS recovered_field_name_count,
         COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS records_with_recovered_values
  FROM raw_key_values
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), ht AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS human_text_value_count,
         COUNT(DISTINCT raw_record_id) AS records_with_human_text,
         COUNT(DISTINCT human_text_category) AS human_text_category_count
  FROM vw_ios_spotlight_human_text_values
  GROUP BY source_id,store_guid,source_db
), nd AS (
  SELECT source_id,store_guid,source_db,
         MAX(decode_mode) AS decode_mode,
         MAX(spotlight_version) AS spotlight_version,
         MAX(properties_count) AS native_property_count,
         MAX(categories_count) AS native_category_count,
         MAX(metadata_blocks) AS metadata_blocks,
         MAX(decompressed_blocks) AS decompressed_blocks,
         MAX(failures) AS decode_failures,
         MAX(status) AS decode_status,
         MAX(message) AS decode_message
  FROM native_decode_attempts
  GROUP BY source_id,store_guid,source_db
)
SELECT rr.source_id,rr.store_guid,rr.source_db,
       COALESCE(nd.decode_mode,'') AS decode_mode,
       COALESCE(nd.spotlight_version,0) AS spotlight_version,
       rr.raw_record_count,
       COALESCE(kv.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(kv.recovered_field_name_count,0) AS recovered_field_name_count,
       COALESCE(kv.records_with_recovered_values,0) AS records_with_recovered_values,
       COALESCE(ht.human_text_value_count,0) AS human_text_value_count,
       COALESCE(ht.records_with_human_text,0) AS records_with_human_text,
       COALESCE(ht.human_text_category_count,0) AS human_text_category_count,
       CASE WHEN rr.raw_record_count>0 THEN printf('%.2f', 100.0 * COALESCE(ht.records_with_human_text,0) / rr.raw_record_count) ELSE '0.00' END AS pct_records_with_human_text,
       COALESCE(nd.native_property_count,0) AS native_property_count,
       COALESCE(nd.native_category_count,0) AS native_category_count,
       COALESCE(nd.metadata_blocks,0) AS metadata_blocks,
       COALESCE(nd.decompressed_blocks,0) AS decompressed_blocks,
       COALESCE(nd.decode_failures,0) AS decode_failures,
       COALESCE(nd.decode_status,'NO_NATIVE_DECODE_ATTEMPT_ROW') AS decode_status,
       rr.earliest_last_updated_utc,rr.latest_last_updated_utc,
       CASE WHEN COALESCE(nd.native_property_count,0)=0 THEN 'PROPERTY_DICTIONARY_NOT_DECODED_GENERIC_PROBES_ONLY'
            WHEN COALESCE(kv.recovered_key_value_count,0)=0 THEN 'NO_KEY_VALUES_RECOVERED'
            ELSE 'GENERIC_TEXT_VALUES_RECOVERED' END AS spotlight_decode_interpretation,
       'Spotlight-first coverage view. App/FFS correlation is supporting context; this row measures native CoreSpotlight record/value recovery.' AS interpretation_note
FROM rr
LEFT JOIN kv ON kv.source_id=rr.source_id AND kv.store_guid=rr.store_guid AND kv.source_db=rr.source_db
LEFT JOIN ht ON ht.source_id=rr.source_id AND ht.store_guid=rr.store_guid AND ht.source_db=rr.source_db
LEFT JOIN nd ON nd.source_id=rr.source_id AND nd.store_guid=rr.store_guid AND nd.source_db=rr.source_db;

DROP VIEW IF EXISTS vw_ios_spotlight_field_coverage_summary;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_field_coverage_summary AS
SELECT source_id,store_guid,source_db,field_name,
       COUNT(*) AS value_row_count,
       COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS distinct_record_count,
       MIN(LENGTH(COALESCE(field_value,''))) AS min_value_length,
       MAX(LENGTH(COALESCE(field_value,''))) AS max_value_length,
       substr(MIN(COALESCE(field_value,'')),1,1000) AS min_sample_value,
       substr(MAX(COALESCE(field_value,'')),1,1000) AS max_sample_value,
       CASE WHEN field_name LIKE '__native_core_probe_string_%' THEN 'GENERIC_NATIVE_STRING_PROBE'
            WHEN field_name LIKE '__native_%' THEN 'GENERIC_NATIVE_FIELD'
            ELSE 'NAMED_SPOTLIGHT_FIELD' END AS field_decode_status,
       'Field coverage summary from recovered Spotlight key/value rows. Generic probe names indicate values recovered before formal property-name mapping.' AS interpretation_note
FROM raw_key_values
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(field_value,'')<>''
GROUP BY source_id,store_guid,source_db,field_name;

DROP VIEW IF EXISTS vw_ios_spotlight_text_category_summary;
CREATE VIEW vw_ios_spotlight_text_category_summary AS
SELECT human_text_category,review_priority,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT store_guid) AS store_count,
       MIN(original_value_length) AS min_original_value_length,
       MAX(original_value_length) AS max_original_value_length,
       substr(MIN(readable_text_sample),1,1000) AS min_sample_text,
       substr(MAX(readable_text_sample),1,1000) AS max_sample_text,
       'Spotlight recovered text category summary. Categories are triage labels over Spotlight text values, not final CoreSpotlight property names.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values
GROUP BY human_text_category,review_priority;

DROP VIEW IF EXISTS vw_ios_spotlight_record_review;
CREATE VIEW vw_ios_spotlight_record_review AS
WITH text_roll AS (
  SELECT raw_record_id,text_value_count,distinct_text_category_count,human_text_categories,has_high_review_value_text,readable_text_rollup_sample
  FROM vw_ios_spotlight_human_text_rollup
), date_one AS (
  SELECT raw_record_id,
         MAX(spotlight_date_utc) AS spotlight_date_utc,
         MAX(spotlight_date_source_field) AS spotlight_date_source_field,
         MAX(spotlight_date_source_table) AS spotlight_date_source_table,
         MAX(spotlight_date_raw_value) AS spotlight_date_raw_value,
         MAX(spotlight_date_parse_method) AS spotlight_date_parse_method,
         MAX(spotlight_date_type) AS spotlight_date_type,
         MAX(spotlight_date_source_evidence) AS spotlight_date_source_evidence,
         MAX(date_validation_hint) AS date_validation_hint,
         COUNT(*) AS collapsed_date_candidate_count
  FROM vw_ios_spotlight_date_provenance
  GROUP BY raw_record_id
)
SELECT r.raw_record_id,r.source_id,r.store_guid,r.source_db,r.inode_num AS spotlight_inode_or_object_id,r.store_id AS spotlight_store_id,r.parent_inode_num,
       COALESCE(dp.spotlight_date_utc,r.last_updated_utc) AS spotlight_date_utc,
       COALESCE(dp.spotlight_date_source_field,'Last_Updated') AS spotlight_date_source_field,
       COALESCE(dp.spotlight_date_source_table,'raw_records') AS spotlight_date_source_table,
       COALESCE(dp.spotlight_date_raw_value,r.last_updated_raw) AS spotlight_date_raw_value,
       COALESCE(dp.spotlight_date_parse_method,'native_epoch_microseconds') AS spotlight_date_parse_method,
       COALESCE(dp.spotlight_date_type,'metadata_seen_or_index_updated') AS spotlight_date_type,
       COALESCE(dp.spotlight_date_source_evidence,'Last_Updated=' || COALESCE(r.last_updated_raw,'') || ' -> ' || COALESCE(r.last_updated_utc,'')) AS spotlight_date_source_evidence,
       COALESCE(dp.date_validation_hint,'Validate against raw_records.last_updated_raw/last_updated_utc for this Store-V2 record.') AS spotlight_date_validation_hint,
       COALESCE(dp.collapsed_date_candidate_count,0) AS collapsed_date_candidate_count,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       COALESCE(t.text_value_count,0) AS spotlight_text_value_count,
       COALESCE(t.distinct_text_category_count,0) AS spotlight_text_category_count,
       COALESCE(t.human_text_categories,'') AS spotlight_text_categories,
       COALESCE(t.readable_text_rollup_sample,'') AS spotlight_text_rollup_sample,
       0 AS ffs_reference_count,
       0 AS ffs_present_reference_count,
       0 AS ffs_missing_or_unresolved_reference_count,
       0 AS app_db_candidate_count,
       0 AS app_db_present_candidate_count,
       0 AS app_db_unresolved_candidate_count,
       CASE WHEN COALESCE(t.has_high_review_value_text,0)=1 THEN 'HIGH_SPOTLIGHT_TEXT_VALUE'
            WHEN COALESCE(t.text_value_count,0)>0 THEN 'SPOTLIGHT_TEXT_VALUE'
            ELSE 'SPOTLIGHT_RECORD_NO_RECOVERED_TEXT' END AS spotlight_review_priority,
       CASE WHEN COALESCE(t.text_value_count,0)>0 THEN 'TEXT_VALUES_RECOVERED_FROM_SPOTLIGHT'
            ELSE 'NO_TEXT_VALUES_RECOVERED_FOR_RECORD' END AS spotlight_decode_status,
       'Spotlight-first record review. V0_9_21 keeps GUI rows raw_record anchored and avoids broad FFS/app joins; full per-record exports are support/diagnostic-only to prevent long SQL materialization. Use Missing From FFS and object/object-summary views for residency pivots.' AS interpretation_note
FROM raw_records r
LEFT JOIN text_roll t ON t.raw_record_id=r.raw_record_id
LEFT JOIN date_one dp ON dp.raw_record_id=r.raw_record_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_summary;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_object_inode_summary AS
WITH rec AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count,
         COUNT(DISTINCT COALESCE(parent_inode_num,'')) AS distinct_parent_id_count,
         MIN(last_updated_utc) AS earliest_last_updated_utc,
         MAX(last_updated_utc) AS latest_last_updated_utc,
         MIN(raw_record_id) AS first_raw_record_id,
         MAX(raw_record_id) AS last_raw_record_id
  FROM raw_records
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), kv AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_key_value_rows,
         COUNT(DISTINCT field_name) AS distinct_spotlight_field_count,
         substr(MAX(CASE WHEN field_name='__spotlight_investigator_text_context' THEN field_value ELSE '' END),1,1800) AS spotlight_text_context_sample
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
)
SELECT rec.source_id,rec.store_guid,rec.source_db,rec.spotlight_inode_or_object_id,rec.spotlight_store_id,
       rec.raw_record_count,rec.distinct_parent_id_count,
       COALESCE(kv.raw_key_value_rows,0) AS raw_key_value_rows,
       COALESCE(kv.distinct_spotlight_field_count,0) AS distinct_spotlight_field_count,
       0 AS date_candidate_rows,
       rec.earliest_last_updated_utc,rec.latest_last_updated_utc,
       '' AS earliest_spotlight_date_utc,
       '' AS latest_spotlight_date_utc,
       rec.first_raw_record_id,rec.last_raw_record_id,
       COALESCE(kv.spotlight_text_context_sample,'') AS spotlight_text_context_sample,
       CASE WHEN rec.raw_record_count>1 THEN 'MULTIPLE_SPOTLIGHT_RECORDS_SHARE_OBJECT_ID'
            WHEN COALESCE(kv.raw_key_value_rows,0)>20 THEN 'SINGLE_RECORD_MANY_FIELDS'
            ELSE 'SINGLE_OR_LOW_EXPANSION_OBJECT' END AS object_materialization_status,
       'Object/inode-centric rollup. V0_9_21 normal exports summarize this view because the full per-object listing is support/diagnostic-only.' AS interpretation_note
FROM rec
LEFT JOIN kv ON kv.source_id=rec.source_id AND kv.store_guid=rec.store_guid AND kv.source_db=rec.source_db AND kv.spotlight_inode_or_object_id=rec.spotlight_inode_or_object_id AND kv.spotlight_store_id=rec.spotlight_store_id;

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_diagnostic_summary;
CREATE VIEW vw_ios_spotlight_object_inode_diagnostic_summary AS
WITH obj AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), buckets AS (
  SELECT source_id,store_guid,source_db,
         CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
              WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
              WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
              ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END AS object_record_bucket,
         COUNT(*) AS object_count,
         SUM(raw_record_count) AS raw_record_count,
         MIN(raw_record_count) AS min_records_per_object,
         MAX(raw_record_count) AS max_records_per_object
  FROM obj
  GROUP BY source_id,store_guid,source_db,
           CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
                WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
                WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
                ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END
)
SELECT source_id,store_guid,source_db,object_record_bucket,object_count,raw_record_count,min_records_per_object,max_records_per_object,
       'Compact object/inode materialization diagnostic. Use this normal export to decide whether the case should pivot to object-centric aggregation; full per-object rows require support/diagnostic export.' AS interpretation_note
FROM buckets;

DROP VIEW IF EXISTS vw_ios_spotlight_object_identity;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_object_identity AS
WITH kv AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         COUNT(*) AS string_probe_count,
         MAX(CASE WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN substr(field_value,1,1500) ELSE '' END) AS sample_url_or_web,
         MAX(CASE WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN substr(field_value,1,1500) ELSE '' END) AS sample_path_or_file_ref,
         MAX(CASE WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN substr(field_value,1,700) ELSE '' END) AS sample_email_or_account,
         MAX(substr(field_value,1,1500)) AS sample_decoded_string
  FROM raw_key_values
  WHERE source_id IN (SELECT source_id FROM evidence_sources)
    AND (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.raw_record_id,r.source_id,r.store_guid,r.source_db,
       CASE
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' OR r.store_guid LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' THEN 'NSFileProtectionCompleteUntilFirstUserAuthentication'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUnlessOpen%' OR r.store_guid LIKE '%NSFileProtectionCompleteUnlessOpen%' THEN 'NSFileProtectionCompleteUnlessOpen'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteWhenUserInactive%' OR r.store_guid LIKE '%NSFileProtectionCompleteWhenUserInactive%' THEN 'NSFileProtectionCompleteWhenUserInactive'
         WHEN r.source_db LIKE '%NSFileProtectionComplete%' OR r.store_guid LIKE '%NSFileProtectionComplete%' THEN 'NSFileProtectionComplete'
         WHEN r.source_db LIKE '%/Priority/%' OR r.store_guid LIKE '%Priority%' THEN 'Priority'
         ELSE 'UnknownOrUnparsedProtectionClass'
       END AS protection_class,
       r.inode_num AS spotlight_inode_or_object_id,
       r.parent_inode_num AS spotlight_parent_id,
       r.store_id AS spotlight_store_id,
       r.file_name,r.display_name,r.full_path,r.content_type,r.content_type_tree,r.record_state,
       r.last_updated_utc,
       COALESCE(k.string_probe_count,0) AS string_probe_count,
       COALESCE(NULLIF(k.sample_url_or_web,''),'') AS sample_url_or_web,
       COALESCE(NULLIF(k.sample_path_or_file_ref,''),'') AS sample_path_or_file_ref,
       COALESCE(NULLIF(k.sample_email_or_account,''),'') AS sample_email_or_account,
       COALESCE(k.sample_decoded_string,'') AS sample_decoded_string,
       CASE WHEN COALESCE(r.full_path,'')<>'' AND r.full_path<>'/' THEN 'PATH_FIELD_PRESENT'
            WHEN COALESCE(NULLIF(k.sample_path_or_file_ref,''),'')<>'' THEN 'STRING_PATH_REFERENCE_PRESENT'
            WHEN COALESCE(k.string_probe_count,0)>0 THEN 'STRING_PROBES_ONLY'
            ELSE 'IDENTIFIERS_ONLY' END AS identity_basis,
       'Use Spotlight IDs/path fragments with FFS inventory and parsed app DB records; Last_Updated remains index/update timing.' AS interpretation_note
FROM raw_records r
LEFT JOIN kv k ON k.source_id=r.source_id AND k.store_guid=r.store_guid AND k.source_db=r.source_db AND k.inode_num=r.inode_num AND COALESCE(k.store_id,'')=COALESCE(r.store_id,'')
WHERE r.source_id IN (SELECT source_id FROM evidence_sources)
  AND (r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%');

DROP VIEW IF EXISTS vw_ios_spotlight_to_ffs_object_links;
CREATE VIEW vw_ios_spotlight_to_ffs_object_links AS
WITH refs AS (
  SELECT * FROM vw_ios_spotlight_referenced_paths WHERE COALESCE(normalized_ios_path,'')<>''
), files AS (
  SELECT source_id,normalized_path,file_name,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,'full_inventory' AS lookup_source
  FROM ios_ffs_file_inventory
  UNION ALL
  SELECT l.source_id,l.normalized_path,l.file_name,l.size_bytes,l.zip_modified_utc,l.protection_class_hint,l.app_container_hint,l.domain_hint,COALESCE(NULLIF(l.lookup_source,''),'slim_path_lookup') AS lookup_source
  FROM ios_ffs_path_lookup l
  WHERE NOT EXISTS (SELECT 1 FROM ios_ffs_file_inventory f WHERE f.source_id=l.source_id LIMIT 1)
)
SELECT r.reference_id,r.source_id,r.store_guid,r.source_db,r.inode_num AS spotlight_inode_or_object_id,
       r.store_id AS spotlight_store_id,r.parent_inode_num AS spotlight_parent_id,
       r.field_name,r.reference_type,r.normalized_ios_path,
       CASE WHEN f.normalized_path IS NOT NULL THEN 'PRESENT_AS_FILE_IN_FFS'
            ELSE 'SPOTLIGHT_ONLY_FILE_MISSING_OR_UNRESOLVED' END AS residency_status,
       CASE WHEN f.normalized_path IS NOT NULL THEN 'HIGH_PATH_MATCH'
            ELSE 'MEDIUM_PATH_ABSENT_FROM_ZIP_INVENTORY' END AS confidence,
       COALESCE(f.file_name,'') AS matched_file_name,
       COALESCE(f.size_bytes,0) AS matched_size_bytes,
       COALESCE(f.zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.protection_class_hint,'') AS matched_protection_class,
       COALESCE(f.app_container_hint,'') AS matched_app_container,
       COALESCE(f.domain_hint,'') AS matched_domain,
       CASE WHEN f.normalized_path IS NOT NULL
            THEN 'Exact normalized Spotlight path matched an enumerated FFS ZIP path. This supports current-file presence, not usage.'
            ELSE 'Path was absent from enumerated FFS ZIP path inventory. This is a lead only and does not by itself prove user deletion.' END AS interpretation_note
FROM refs r
LEFT JOIN files f ON f.source_id=r.source_id AND f.normalized_path=r.normalized_ios_path;

DROP VIEW IF EXISTS vw_ios_spotlight_to_app_db_record_links;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_to_app_db_record_links AS
WITH parsed AS (
  SELECT source_id,
         CASE
           WHEN database_category='APPLE_MESSAGES' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
           WHEN database_category='CALL_HISTORY' THEN 'CALL_OR_FACETIME_RELATED'
           WHEN database_category='WHATSAPP' THEN 'WHATSAPP_RELATED'
           WHEN database_category='SIGNAL' THEN 'SIGNAL_RELATED'
           WHEN database_category='TELEGRAM' THEN 'TELEGRAM_RELATED'
           WHEN database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT') THEN 'WEB_OR_BROWSER_RELATED'
           WHEN database_category='MAIL' THEN 'MAIL_OR_ACCOUNT_RELATED'
           WHEN database_category='CONTACTS' THEN 'CONTACT_OR_ADDRESS_BOOK_RELATED'
           WHEN database_category='CALENDAR' THEN 'CALENDAR_OR_INVITATION_RELATED'
           ELSE '' END AS object_category,
         COUNT(*) AS parsed_record_count,
         MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,
         MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
         MIN(database_normalized_path) AS sample_database_path,
         MIN(table_name) AS sample_table_name,
         MIN(record_category) AS sample_record_category,
         MIN(substr(COALESCE(NULLIF(url,''),NULLIF(title,''),NULLIF(text_snippet,''),NULLIF(file_path,''),''),1,1200)) AS sample_parsed_value
  FROM ios_app_parsed_records
  WHERE source_id IN (SELECT source_id FROM evidence_sources)
  GROUP BY source_id,object_category
)
SELECT c.candidate_id,c.source_id,c.store_guid,c.source_db,c.inode_num AS spotlight_inode_or_object_id,c.store_id AS spotlight_store_id,c.parent_inode_num AS spotlight_parent_id,
       c.field_name,c.object_category,c.database_residency_status,c.database_name,c.database_category,c.app_hint,c.candidate_database_path,
       c.record_inventory_status,c.matched_record_category,c.matched_table_name,c.matched_table_row_count,
       COALESCE(p.parsed_record_count,0) AS parsed_record_count,
       COALESCE(p.earliest_record_timestamp_utc,'') AS earliest_record_timestamp_utc,
       COALESCE(p.latest_record_timestamp_utc,'') AS latest_record_timestamp_utc,
       COALESCE(p.sample_database_path,'') AS sample_database_path,
       COALESCE(p.sample_table_name,'') AS sample_table_name,
       COALESCE(p.sample_record_category,'') AS sample_record_category,
       COALESCE(p.sample_parsed_value,'') AS sample_parsed_value,
       c.string_value_sample,
       CASE WHEN COALESCE(p.parsed_record_count,0)>0 THEN 'POTENTIAL_APP_DB_RECORD_FAMILY_PRESENT'
            WHEN c.database_residency_status LIKE 'POTENTIAL_RECORD_TABLE%' THEN 'APP_DB_TABLE_PRESENT_RECORD_LEVEL_MATCH_NOT_PROVEN'
            ELSE c.database_residency_status END AS app_db_link_status,
       'Family-level correlation only. Exact Spotlight string-to-app database row matching remains a later phase.' AS interpretation_note
FROM vw_ios_database_residency_candidates c
LEFT JOIN parsed p ON p.source_id=c.source_id AND p.object_category=c.object_category;

DROP VIEW IF EXISTS vw_ios_spotlight_residency_summary;
CREATE VIEW vw_ios_spotlight_residency_summary AS
SELECT residency_status,confidence,COUNT(*) AS reference_count,
       COUNT(DISTINCT spotlight_inode_or_object_id) AS distinct_record_count,
       MIN(normalized_ios_path) AS first_path_sample,MAX(normalized_ios_path) AS last_path_sample
FROM vw_ios_spotlight_to_ffs_object_links
GROUP BY residency_status,confidence
UNION ALL
SELECT database_residency_status AS residency_status,'DATABASE_FAMILY_HEURISTIC' AS confidence,COUNT(*) AS reference_count,
       COUNT(DISTINCT inode_num) AS distinct_record_count,MIN(candidate_database_path) AS first_path_sample,MAX(candidate_database_path) AS last_path_sample
FROM vw_ios_database_residency_candidates
GROUP BY database_residency_status;

DROP VIEW IF EXISTS vw_ios_protection_class_summary;
)VSGUI"
    });
    execGuiSql(R"SQL(CREATE VIEW vw_ios_protection_class_summary AS
WITH ios_records AS (
  SELECT raw_record_id,source_id,store_guid,source_db,store_path,inode_num,store_id,last_updated_utc,
         CASE
           WHEN source_db LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' OR store_guid LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' THEN 'NSFileProtectionCompleteUntilFirstUserAuthentication'
           WHEN source_db LIKE '%NSFileProtectionCompleteUnlessOpen%' OR store_guid LIKE '%NSFileProtectionCompleteUnlessOpen%' THEN 'NSFileProtectionCompleteUnlessOpen'
           WHEN source_db LIKE '%NSFileProtectionCompleteWhenUserInactive%' OR store_guid LIKE '%NSFileProtectionCompleteWhenUserInactive%' THEN 'NSFileProtectionCompleteWhenUserInactive'
           WHEN source_db LIKE '%NSFileProtectionComplete%' OR store_guid LIKE '%NSFileProtectionComplete%' THEN 'NSFileProtectionComplete'
           WHEN source_db LIKE '%/Priority/%' OR store_guid LIKE '%Priority%' THEN 'Priority'
           ELSE 'UnknownOrUnparsedProtectionClass'
         END AS protection_class
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
), kv AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,COUNT(*) AS string_probe_rows
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.protection_class,
       COUNT(*) AS raw_record_count,
       SUM(CASE WHEN COALESCE(k.string_probe_rows,0)>0 THEN 1 ELSE 0 END) AS records_with_string_probes,
       COALESCE(SUM(k.string_probe_rows),0) AS string_probe_rows,
       COUNT(DISTINCT r.source_db) AS selected_database_count,
       MIN(NULLIF(r.last_updated_utc,'')) AS earliest_last_updated_utc,
       MAX(NULLIF(r.last_updated_utc,'')) AS latest_last_updated_utc
FROM ios_records r
LEFT JOIN kv k ON k.source_id=r.source_id AND k.store_guid=r.store_guid AND k.source_db=r.source_db AND k.inode_num=r.inode_num AND k.store_id=r.store_id
GROUP BY r.protection_class;

DROP VIEW IF EXISTS vw_ios_artifact_hint_summary;
CREATE VIEW vw_ios_artifact_hint_summary AS
WITH probe AS (
  SELECT store_guid,source_db,inode_num,store_id,LOWER(field_value) AS v,field_value
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), categorized AS (
  SELECT store_guid,source_db,inode_num,store_id,field_value,
         CASE
           WHEN v LIKE '%/mail/attachmentdata/%' THEN 'MAIL_ATTACHMENT_PATH'
)SQL" R"SQL(           WHEN v LIKE '%com.apple.clouddocs.iclouddrivefileprovider%' OR v LIKE '%icloud drive%' OR v LIKE '%/mobile documents/%' THEN 'ICLOUD_DRIVE_OR_CLOUDDOCS'
           WHEN v LIKE '%drive.google.com%' THEN 'GOOGLE_DRIVE_LINK'
           WHEN v LIKE '%docs.google.com%' THEN 'GOOGLE_DOCS_LINK'
           WHEN v LIKE '%teams.microsoft.com%' THEN 'MICROSOFT_TEAMS_LINK'
           WHEN v LIKE '%onedrive%' THEN 'ONEDRIVE_LINK_OR_TEXT'
           WHEN v LIKE '%zoom.us/%' THEN 'ZOOM_LINK'
           WHEN v LIKE '%maps.app.goo.gl%' OR v LIKE '%maps.google.%' THEN 'MAP_LINK'
           WHEN v LIKE '%invite.ics%' OR v LIKE '%calendar%' OR v LIKE '%rsvp%' THEN 'CALENDAR_INVITATION'
           WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'WEB_URL_OR_HTML_LINK'
           WHEN v LIKE 'file:%' OR v LIKE '/private/var/%' OR v LIKE '%/mobile/%' THEN 'IOS_FILE_PATH'
           WHEN v LIKE '%@%' AND v LIKE '%.%' THEN 'EMAIL_OR_ACCOUNT_TEXT'
           WHEN v LIKE '%imessage%' OR v LIKE '%sms%' OR v LIKE '%message%' THEN 'MESSAGE_OR_MESSAGE_ATTACHMENT_TEXT'
           ELSE 'OTHER_STRING_PROBE'
         END AS artifact_hint
  FROM probe
)
SELECT artifact_hint,
       COUNT(*) AS string_probe_rows,
       COUNT(DISTINCT store_guid) AS store_count,
       COUNT(DISTINCT inode_num) AS distinct_record_count,
       COUNT(DISTINCT field_value) AS distinct_value_count,
       substr(MIN(field_value),1,250) AS min_sample_value,
       substr(MAX(field_value),1,250) AS max_sample_value
FROM categorized
GROUP BY artifact_hint
ORDER BY string_probe_rows DESC, artifact_hint;

DROP VIEW IF EXISTS vw_ios_record_investigation_hints;
)SQL");
    execGuiSql(R"SQL(CREATE VIEW vw_ios_record_investigation_hints AS
WITH kv AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         CASE
           WHEN LOWER(field_value) LIKE '%/mail/attachmentdata/%' THEN 'MAIL_ATTACHMENT_PATH'
           WHEN LOWER(field_value) LIKE '%com.apple.clouddocs.iclouddrivefileprovider%' OR LOWER(field_value) LIKE '%icloud drive%' OR LOWER(field_value) LIKE '%/mobile documents/%' THEN 'ICLOUD_DRIVE_OR_CLOUDDOCS'
           WHEN LOWER(field_value) LIKE '%drive.google.com%' THEN 'GOOGLE_DRIVE_LINK'
           WHEN LOWER(field_value) LIKE '%docs.google.com%' THEN 'GOOGLE_DOCS_LINK'
           WHEN LOWER(field_value) LIKE '%teams.microsoft.com%' THEN 'MICROSOFT_TEAMS_LINK'
           WHEN LOWER(field_value) LIKE '%onedrive%' THEN 'ONEDRIVE_LINK_OR_TEXT'
           WHEN LOWER(field_value) LIKE '%zoom.us/%' THEN 'ZOOM_LINK'
           WHEN LOWER(field_value) LIKE '%maps.app.goo.gl%' OR LOWER(field_value) LIKE '%maps.google.%' THEN 'MAP_LINK'
           WHEN LOWER(field_value) LIKE '%invite.ics%' OR LOWER(field_value) LIKE '%calendar%' OR LOWER(field_value) LIKE '%rsvp%' THEN 'CALENDAR_INVITATION'
           WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN 'WEB_URL_OR_HTML_LINK'
           WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN 'IOS_FILE_PATH'
           WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN 'EMAIL_OR_ACCOUNT_TEXT'
           WHEN LOWER(field_value) LIKE '%imessage%' OR LOWER(field_value) LIKE '%sms%' OR LOWER(field_value) LIKE '%message%' THEN 'MESSAGE_OR_MESSAGE_ATTACHMENT_TEXT'
           ELSE 'OTHER_STRING_PROBE'
         END AS artifact_hint
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), agg AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         COUNT(*) AS string_probe_rows,
         GROUP_CONCAT(DISTINCT artifact_hint) AS artifact_hints,
         MAX(CASE WHEN artifact_hint='MAIL_ATTACHMENT_PATH' THEN 1 ELSE 0 END) AS has_mail_attachment,
         MAX(CASE WHEN artifact_hint='ICLOUD_DRIVE_OR_CLOUDDOCS' THEN 1 ELSE 0 END) AS has_icloud_docs,
         MAX(CASE WHEN artifact_hint IN ('GOOGLE_DRIVE_LINK','GOOGLE_DOCS_LINK') THEN 1 ELSE 0 END) AS has_google_workspace,
         MAX(CASE WHEN artifact_hint IN ('MICROSOFT_TEAMS_LINK','ONEDRIVE_LINK_OR_TEXT') THEN 1 ELSE 0 END) AS has_microsoft_cloud,
         MAX(CASE WHEN artifact_hint='CALENDAR_INVITATION' THEN 1 ELSE 0 END) AS has_calendar_invite,
         MAX(CASE WHEN artifact_hint='EMAIL_OR_ACCOUNT_TEXT' THEN 1 ELSE 0 END) AS has_email_text,
)SQL" R"SQL(         MAX(CASE WHEN artifact_hint='WEB_URL_OR_HTML_LINK' THEN 1 ELSE 0 END) AS has_web_url
  FROM kv
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.raw_record_id,
       r.source_id,
       r.store_guid,
       CASE
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' OR r.store_guid LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' THEN 'NSFileProtectionCompleteUntilFirstUserAuthentication'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUnlessOpen%' OR r.store_guid LIKE '%NSFileProtectionCompleteUnlessOpen%' THEN 'NSFileProtectionCompleteUnlessOpen'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteWhenUserInactive%' OR r.store_guid LIKE '%NSFileProtectionCompleteWhenUserInactive%' THEN 'NSFileProtectionCompleteWhenUserInactive'
         WHEN r.source_db LIKE '%NSFileProtectionComplete%' OR r.store_guid LIKE '%NSFileProtectionComplete%' THEN 'NSFileProtectionComplete'
         WHEN r.source_db LIKE '%/Priority/%' OR r.store_guid LIKE '%Priority%' THEN 'Priority'
         ELSE 'UnknownOrUnparsedProtectionClass'
       END AS protection_class,
       CASE
         WHEN a.has_mail_attachment=1 THEN 'Mail attachment / Mail AttachmentData path'
         WHEN a.has_icloud_docs=1 THEN 'iCloud Drive or CloudDocs provider content'
         WHEN a.has_google_workspace=1 THEN 'Google Docs or Google Drive link/content'
         WHEN a.has_microsoft_cloud=1 THEN 'Microsoft Teams or OneDrive link/content'
         WHEN a.has_calendar_invite=1 THEN 'Calendar invitation or ICS-like content'
         WHEN a.has_email_text=1 THEN 'Email address/account-like text'
         WHEN a.has_web_url=1 THEN 'Web URL or HTML link content'
         ELSE 'Other decoded string probe'
       END AS primary_investigation_hint,
       a.artifact_hints,
       a.string_probe_rows,
       r.source_db,
       r.inode_num,
       r.store_id,
       r.parent_inode_num,
       r.file_name,
       r.content_type,
       r.display_name,
       r.full_path,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       r.record_state
FROM raw_records r
JOIN agg a ON a.source_id=r.source_id AND a.store_guid=r.store_guid AND a.source_db=r.source_db AND a.inode_num=r.inode_num AND a.store_id=r.store_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';
)SQL");
    execGuiSql(R"SQL(
CREATE TABLE IF NOT EXISTS investigator_tags (
  tag_id INTEGER PRIMARY KEY AUTOINCREMENT,
  tag_name TEXT UNIQUE NOT NULL,
  tag_color TEXT,
  created_utc TEXT,
  notes TEXT
);
CREATE TABLE IF NOT EXISTS artifact_tags (
  artifact_tag_id INTEGER PRIMARY KEY AUTOINCREMENT,
  artifact_id INTEGER NOT NULL,
  tag_id INTEGER NOT NULL,
  created_utc TEXT,
  UNIQUE(artifact_id, tag_id)
);
CREATE TABLE IF NOT EXISTS investigator_notes (
  note_id INTEGER PRIMARY KEY AUTOINCREMENT,
  target_type TEXT NOT NULL,
  target_id TEXT NOT NULL,
  note_text TEXT,
  created_utc TEXT,
  updated_utc TEXT
);
CREATE INDEX IF NOT EXISTS idx_artifact_tags_artifact ON artifact_tags(artifact_id);
CREATE INDEX IF NOT EXISTS idx_artifact_tags_tag ON artifact_tags(tag_id);
CREATE INDEX IF NOT EXISTS idx_investigator_notes_target ON investigator_notes(target_type, target_id);
CREATE TABLE IF NOT EXISTS gui_checked_artifacts (
  artifact_id INTEGER PRIMARY KEY,
  checked_utc TEXT,
  notes TEXT
);
CREATE INDEX IF NOT EXISTS idx_gui_checked_artifacts_checked ON gui_checked_artifacts(checked_utc);

DROP VIEW IF EXISTS vw_checked_artifacts;
CREATE VIEW vw_checked_artifacts AS
WITH tag_summary AS (
  SELECT at.artifact_id, GROUP_CONCAT(it.tag_name, '; ') AS tags, COUNT(*) AS tag_count
  FROM artifact_tags at
  JOIN investigator_tags it ON it.tag_id=at.tag_id
  GROUP BY at.artifact_id
), note_summary AS (
  SELECT CAST(target_id AS INTEGER) AS artifact_id,
         COUNT(*) AS note_count,
         MAX(COALESCE(NULLIF(updated_utc,''), created_utc)) AS last_note_utc,
         (SELECT note_text FROM investigator_notes n2 WHERE n2.target_type='artifact' AND CAST(n2.target_id AS INTEGER)=CAST(n.target_id AS INTEGER) ORDER BY COALESCE(NULLIF(n2.updated_utc,''), n2.created_utc) DESC, n2.note_id DESC LIMIT 1) AS last_note_text
  FROM investigator_notes n
  WHERE target_type='artifact'
  GROUP BY CAST(target_id AS INTEGER)
), date_summary AS (
  SELECT artifact_id, usage_latest_utc, modified_latest_utc, created_latest_utc, downloaded_latest_utc, last_date_utc
  FROM artifact_date_summary
)
SELECT c.checked_utc,
       a.artifact_id,
       COALESCE(ts.tags,'') AS tags,
       COALESCE(ts.tag_count,0) AS tag_count,
       COALESCE(ns.note_count,0) AS note_count,
       COALESCE(ns.last_note_utc,'') AS last_note_utc,
       COALESCE(ns.last_note_text,'') AS last_note_text,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
)SQL" R"SQL(       a.use_count_value,
       a.usage_field_summary,
       ds.usage_latest_utc,
       ds.modified_latest_utc,
       ds.created_latest_utc,
       ds.downloaded_latest_utc,
       ds.last_date_utc,
       a.confidence
FROM gui_checked_artifacts c
JOIN artifacts a ON a.artifact_id=c.artifact_id
LEFT JOIN tag_summary ts ON ts.artifact_id=a.artifact_id
LEFT JOIN note_summary ns ON ns.artifact_id=a.artifact_id
LEFT JOIN date_summary ds ON ds.artifact_id=a.artifact_id;

DROP VIEW IF EXISTS vw_tagged_artifacts;
CREATE VIEW vw_tagged_artifacts AS
WITH note_summary AS (
  SELECT target_id AS artifact_id,
         COUNT(*) AS note_count,
         MAX(COALESCE(NULLIF(updated_utc,''), created_utc)) AS last_note_utc,
         (SELECT note_text FROM investigator_notes n2 WHERE n2.target_type='artifact' AND n2.target_id=n.target_id ORDER BY COALESCE(NULLIF(n2.updated_utc,''), n2.created_utc) DESC, n2.note_id DESC LIMIT 1) AS last_note_text
  FROM investigator_notes n
  WHERE target_type='artifact'
  GROUP BY target_id
), date_summary AS (
  SELECT artifact_id, usage_latest_utc, modified_latest_utc, created_latest_utc, downloaded_latest_utc
  FROM artifact_date_summary
)
SELECT t.tag_id,
       t.tag_name,
       at.created_utc AS tagged_utc,
       a.artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.use_count_value,
       a.usage_field_summary,
       a.open_count_estimate,
       ds.usage_latest_utc,
       ds.modified_latest_utc,
       ds.created_latest_utc,
       ds.downloaded_latest_utc,
       COALESCE(ns.note_count,0) AS note_count,
       COALESCE(ns.last_note_utc,'') AS last_note_utc,
       COALESCE(ns.last_note_text,'') AS last_note_text,
       a.confidence
FROM artifact_tags at
JOIN investigator_tags t ON t.tag_id=at.tag_id
JOIN artifacts a ON a.artifact_id=at.artifact_id
LEFT JOIN note_summary ns ON ns.artifact_id=CAST(a.artifact_id AS TEXT)
LEFT JOIN date_summary ds ON ds.artifact_id=a.artifact_id;

DROP VIEW IF EXISTS vw_artifact_notes;
CREATE VIEW vw_artifact_notes AS
WITH tag_summary AS (
  SELECT at.artifact_id, GROUP_CONCAT(it.tag_name, '; ') AS tags
  FROM artifact_tags at
  JOIN investigator_tags it ON it.tag_id=at.tag_id
  GROUP BY at.artifact_id
)
SELECT n.note_id,
       n.created_utc,
       n.updated_utc,
       n.note_text,
       CAST(n.target_id AS INTEGER) AS artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
)SQL" R"SQL(       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       COALESCE(ts.tags,'') AS tags
FROM investigator_notes n
LEFT JOIN artifacts a ON a.artifact_id=CAST(n.target_id AS INTEGER)
LEFT JOIN tag_summary ts ON ts.artifact_id=a.artifact_id
WHERE n.target_type='artifact';

DROP VIEW IF EXISTS vw_export_ready_artifacts;
)SQL");
    execGuiSql(R"SQL(CREATE VIEW vw_export_ready_artifacts AS
WITH tag_summary AS (
  SELECT at.artifact_id, GROUP_CONCAT(it.tag_name, '; ') AS tags, COUNT(*) AS tag_count
  FROM artifact_tags at
  JOIN investigator_tags it ON it.tag_id=at.tag_id
  GROUP BY at.artifact_id
), note_summary AS (
  SELECT CAST(target_id AS INTEGER) AS artifact_id, COUNT(*) AS note_count, MAX(COALESCE(NULLIF(updated_utc,''), created_utc)) AS last_note_utc
  FROM investigator_notes
  WHERE target_type='artifact'
  GROUP BY CAST(target_id AS INTEGER)
)
SELECT a.artifact_id,
       COALESCE(ts.tags,'') AS tags,
       COALESCE(ts.tag_count,0) AS tag_count,
       COALESCE(ns.note_count,0) AS note_count,
       COALESCE(ns.last_note_utc,'') AS last_note_utc,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.use_count_value,
       a.usage_field_summary,
       ads.usage_latest_utc,
       ads.modified_latest_utc,
       ads.created_latest_utc,
       ads.downloaded_latest_utc,
       a.confidence
FROM artifacts a
LEFT JOIN tag_summary ts ON ts.artifact_id=a.artifact_id
LEFT JOIN note_summary ns ON ns.artifact_id=a.artifact_id
LEFT JOIN artifact_date_summary ads ON ads.artifact_id=a.artifact_id
WHERE COALESCE(ts.tag_count,0)>0 OR COALESCE(ns.note_count,0)>0;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_date_field_attribution;
CREATE VIEW vw_date_field_attribution AS
WITH common_date_counts AS (
  SELECT substr(parsed_utc,1,10) AS date_only, COUNT(*) AS row_count
  FROM raw_date_candidates
  WHERE COALESCE(parsed_utc,'')<>''
  GROUP BY substr(parsed_utc,1,10)
  HAVING COUNT(*) >= 1000
), top_common_dates AS (
  SELECT date_only FROM common_date_counts ORDER BY row_count DESC, date_only DESC LIMIT 5
), attributed AS (
  SELECT d.raw_date_id,
         d.source_id,
         d.store_guid,
         d.source_db,
         d.inode_num,
         d.store_id,
         COALESCE(d.artifact_id, a.artifact_id) AS artifact_id,
         COALESCE(NULLIF(d.parent_inode_num,''), a.parent_inode_num) AS parent_inode_num,
         COALESCE(NULLIF(d.file_name,''), a.file_name) AS file_name,
         a.display_name,
         COALESCE(NULLIF(d.best_path,''), a.best_path) AS best_path,
         a.path_source,
         a.path_status,
         a.logical_size_bytes,
         a.physical_size_bytes,
         a.content_type,
         a.where_froms,
         d.field_name AS raw_spotlight_field,
         d.field_value AS raw_spotlight_value,
         d.parsed_utc,
         d.parse_method,
         substr(d.parsed_utc,1,10) AS parsed_date_only,
         COALESCE(cdc.row_count,0) AS common_date_row_count,
         CASE
           WHEN lower(d.field_name) LIKE '%lastuseddate%' THEN 'opened_last'
           WHEN lower(d.field_name) LIKE '%useddates%' THEN 'used_date'
           WHEN lower(d.field_name) LIKE '%recent%spotlight%engagement%' THEN 'spotlight_engagement'
           WHEN lower(d.field_name) LIKE '%download%' THEN 'downloaded'
           WHEN lower(d.field_name) LIKE '%contentcreation%' OR lower(d.field_name) LIKE '%creationdate%' THEN 'created'
           WHEN lower(d.field_name) LIKE '%contentmodification%' OR lower(d.field_name) LIKE '%contentchange%' OR lower(d.field_name) LIKE '%modificationdate%' THEN 'modified'
           WHEN lower(d.field_name) LIKE '%interestingdate%' THEN 'interesting_or_index_date'
           WHEN lower(d.field_name) LIKE '%gps%' THEN 'gps_date'
           WHEN lower(d.field_name) LIKE '%startdate%' THEN 'start_date'
           WHEN lower(d.field_name) LIKE '%enddate%' THEN 'end_date'
           WHEN lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 'metadata_seen_or_index_updated'
           ELSE 'other_date'
         END AS event_type_interpretation,
         CASE
           WHEN lower(d.field_name) LIKE '%lastuseddate%' OR lower(d.field_name) LIKE '%useddates%' THEN 'usage/open evidence from Spotlight usage field'
           WHEN lower(d.field_name) LIKE '%download%' THEN 'download/origin date field'
)SQL" R"SQL(           WHEN lower(d.field_name) LIKE '%contentcreation%' OR lower(d.field_name) LIKE '%creationdate%' THEN 'file/content creation date candidate'
           WHEN lower(d.field_name) LIKE '%contentmodification%' OR lower(d.field_name) LIKE '%contentchange%' OR lower(d.field_name) LIKE '%modificationdate%' THEN 'file/content modification date candidate'
           WHEN lower(d.field_name) LIKE '%interestingdate%' THEN 'Spotlight interesting/index date; review carefully'
           WHEN lower(d.field_name) LIKE '%ranking%' THEN 'Spotlight ranking date; not direct user activity by itself'
           WHEN lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 'Spotlight record/index update time; not direct user activity by itself'
           ELSE 'date candidate from decoded Spotlight metadata'
         END AS interpretation_note,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'ARTIFACT_MATCHED_BY_SOURCE_STORE_INODE'
           ELSE 'NO_ARTIFACT_MATCH_BY_SOURCE_STORE_INODE'
         END AS association_status,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND COALESCE(NULLIF(COALESCE(d.best_path,a.best_path),''),'')<>'' AND COALESCE(NULLIF(COALESCE(d.file_name,a.file_name),''),'')<>'' THEN 'HIGH_OBJECT_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND (COALESCE(NULLIF(COALESCE(d.best_path,a.best_path),''),'')<>'' OR COALESCE(NULLIF(COALESCE(d.file_name,a.file_name),''),'')<>'') THEN 'MEDIUM_OBJECT_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'LOW_OBJECT_CONTEXT'
           ELSE 'NONE'
         END AS association_confidence,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'source_id + store_guid + inode_num'
           ELSE ''
         END AS association_method,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND COALESCE(NULLIF(COALESCE(d.best_path,a.best_path),''),'')<>'' THEN 'HAS_ARTIFACT_AND_PATH_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND COALESCE(NULLIF(COALESCE(d.file_name,a.file_name),''),'')<>'' THEN 'HAS_ARTIFACT_AND_NAME_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'HAS_ARTIFACT_ONLY'
           ELSE 'DATE_ONLY_NO_ARTIFACT_CONTEXT'
         END AS object_context_status,
         CASE
           WHEN lower(d.field_name) LIKE '%interestingdate%' OR lower(d.field_name) LIKE '%ranking%' OR lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 1
)SQL" R"SQL(           WHEN substr(d.parsed_utc,1,10) IN (SELECT date_only FROM top_common_dates) AND lower(d.field_name) NOT LIKE '%creation%' AND lower(d.field_name) NOT LIKE '%modification%' AND lower(d.field_name) NOT LIKE '%used%' AND lower(d.field_name) NOT LIKE '%download%' THEN 1
           ELSE 0
         END AS likely_snapshot_or_index_date,
         CASE
           WHEN lower(d.field_name) LIKE '%interestingdate%' THEN 'FIELD_IS_SPOTLIGHT_INTERESTING_DATE'
           WHEN lower(d.field_name) LIKE '%ranking%' THEN 'FIELD_IS_SPOTLIGHT_RANKING_DATE'
           WHEN lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 'FIELD_IS_RECORD_LAST_UPDATED_OR_INDEX_DATE'
           WHEN substr(d.parsed_utc,1,10) IN (SELECT date_only FROM top_common_dates) THEN 'DATE_IS_COMMON_ACROSS_INDEX_SAMPLE'
           ELSE ''
         END AS snapshot_warning_reason
  FROM raw_date_candidates d
  LEFT JOIN artifacts a ON a.source_id=d.source_id AND a.store_guid=d.store_guid AND a.inode_num=d.inode_num
  LEFT JOIN common_date_counts cdc ON cdc.date_only=substr(d.parsed_utc,1,10)
  WHERE COALESCE(d.parsed_utc,'')<>''
)
SELECT *,
       CASE
         WHEN likely_snapshot_or_index_date=1 AND common_date_row_count>0 THEN snapshot_warning_reason || '; common date row count=' || common_date_row_count
         ELSE snapshot_warning_reason
       END AS snapshot_warning_detail
FROM attributed;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_usage_event_detail_attributed;
CREATE VIEW vw_usage_event_detail_attributed AS
SELECT ROW_NUMBER() OVER (ORDER BY d.parsed_utc, d.raw_date_id) AS usage_event_id,
       d.parsed_utc AS event_utc,
       d.event_type_interpretation AS date_type,
       d.raw_spotlight_field AS source_field,
       d.raw_spotlight_value AS raw_value,
       d.interpretation_note AS usage_reason,
       d.likely_snapshot_or_index_date,
       d.snapshot_warning_reason,
       d.snapshot_warning_detail,
       d.association_status,
       d.association_confidence,
       d.object_context_status,
       d.artifact_id,
       d.source_id,
       d.store_guid,
       d.inode_num,
       d.parent_inode_num,
       d.file_name,
       d.display_name,
       d.best_path,
       d.path_source,
       d.path_status,
       d.logical_size_bytes,
       d.physical_size_bytes,
       d.content_type,
       d.where_froms
FROM vw_date_field_attribution d
WHERE d.event_type_interpretation IN ('opened_last','used_date','spotlight_engagement')
   OR lower(d.raw_spotlight_field) LIKE '%used%'
   OR lower(d.raw_spotlight_field) LIKE '%open%';
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_artifact_dates_wide;
CREATE VIEW vw_artifact_dates_wide AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,logical_size_bytes,physical_size_bytes,content_type,where_froms,
       created_earliest_utc,created_latest_utc,modified_earliest_utc,modified_latest_utc,downloaded_earliest_utc,downloaded_latest_utc,usage_earliest_utc,usage_latest_utc,interesting_or_index_earliest_utc,interesting_or_index_latest_utc,
       likely_snapshot_date_count,associated_date_count,unassociated_date_count,available_date_fields,association_confidence_summary,snapshot_warning_reasons
FROM artifact_date_summary;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_snapshot_date_warnings;
CREATE VIEW vw_snapshot_date_warnings AS
SELECT raw_date_id,artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,logical_size_bytes,physical_size_bytes,content_type,raw_spotlight_field,parsed_utc,event_type_interpretation,interpretation_note,association_status,association_confidence,object_context_status,common_date_row_count,snapshot_warning_reason,snapshot_warning_detail
FROM vw_date_field_attribution
WHERE likely_snapshot_or_index_date=1;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_object_date_summary;
CREATE VIEW vw_object_date_summary AS
SELECT artifact_id,
       source_id,
       store_guid,
       inode_num,
       parent_inode_num,
       file_name,
       display_name,
       best_path,
       path_source,
       path_status,
       logical_size_bytes,
       physical_size_bytes,
       content_type,
       first_date_utc,
       last_date_utc,
       total_date_count,
       created_date_count,
       modified_date_count,
       downloaded_date_count,
       usage_date_count,
       interesting_or_index_date_count,
       metadata_seen_or_index_updated_count,
       other_date_count,
       likely_snapshot_or_index_date_count,
       available_date_fields,
       interpreted_date_types,
       snapshot_warning_reasons,
       date_association_status,
       date_association_confidence
FROM artifact_date_summary;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_object_usage_summary;
CREATE VIEW vw_object_usage_summary AS
WITH usage_artifacts AS (
  SELECT artifact_id
  FROM artifacts
  WHERE artifact_id IS NOT NULL
    AND (
         COALESCE(used_dates_count,0)>0
      OR COALESCE(NULLIF(last_used_date_utc,''),'')<>''
      OR COALESCE(NULLIF(first_used_candidate_utc,''),'')<>''
      OR COALESCE(NULLIF(use_count_value,''),'')<>''
      OR COALESCE(open_count_estimate,0)>0
    )
  UNION
  SELECT DISTINCT artifact_id
  FROM usage_evidence
  WHERE artifact_id IS NOT NULL
  UNION
  SELECT DISTINCT artifact_id
  FROM raw_date_candidates
  WHERE artifact_id IS NOT NULL
    AND COALESCE(parsed_utc,'')<>''
    AND date_type IN ('opened_last','used_date','spotlight_engagement')
), usage_dates AS (
  SELECT d.artifact_id,
         MIN(d.parsed_utc) AS usage_earliest_utc,
         MAX(d.parsed_utc) AS usage_latest_utc,
         COUNT(*) AS usage_date_row_count,
         GROUP_CONCAT(DISTINCT d.field_name) AS usage_date_source_fields,
         GROUP_CONCAT(d.parsed_utc, '; ') AS fused_usage_dates_utc,
         SUM(CASE WHEN d.date_type IN ('interesting_or_index_date','metadata_seen_or_index_updated') THEN 1 ELSE 0 END) AS likely_snapshot_or_index_usage_date_count,
         GROUP_CONCAT(DISTINCT CASE WHEN d.date_type IN ('interesting_or_index_date','metadata_seen_or_index_updated') THEN d.field_name ELSE NULL END) AS snapshot_warning_reasons
  FROM raw_date_candidates d
  JOIN usage_artifacts ua ON ua.artifact_id=d.artifact_id
  WHERE d.artifact_id IS NOT NULL
    AND COALESCE(d.parsed_utc,'')<>''
    AND d.date_type IN ('opened_last','used_date','spotlight_engagement')
  GROUP BY d.artifact_id
), usage_rows AS (
  SELECT artifact_id,
         COUNT(*) AS usage_evidence_row_count,
         GROUP_CONCAT(DISTINCT field_name) AS usage_evidence_fields,
         GROUP_CONCAT(field_name || '=' || COALESCE(NULLIF(parsed_utc,''), field_value), '; ') AS usage_evidence_values
  FROM usage_evidence
  WHERE artifact_id IS NOT NULL
  GROUP BY artifact_id
), date_wide AS (
  SELECT d.artifact_id,
         MIN(CASE WHEN d.date_type='created' THEN d.parsed_utc END) AS created_earliest_utc,
         MAX(CASE WHEN d.date_type='created' THEN d.parsed_utc END) AS created_latest_utc,
         MIN(CASE WHEN d.date_type='modified' THEN d.parsed_utc END) AS modified_earliest_utc,
         MAX(CASE WHEN d.date_type='modified' THEN d.parsed_utc END) AS modified_latest_utc,
         MIN(CASE WHEN d.date_type='downloaded' THEN d.parsed_utc END) AS downloaded_earliest_utc,
         MAX(CASE WHEN d.date_type='downloaded' THEN d.parsed_utc END) AS downloaded_latest_utc,
         MIN(CASE WHEN d.date_type='interesting_or_index_date' THEN d.parsed_utc END) AS interesting_or_index_earliest_utc,
)SQL" R"SQL(         MAX(CASE WHEN d.date_type='interesting_or_index_date' THEN d.parsed_utc END) AS interesting_or_index_latest_utc,
         SUM(CASE WHEN d.date_type IN ('interesting_or_index_date','metadata_seen_or_index_updated') THEN 1 ELSE 0 END) AS likely_snapshot_date_count,
         GROUP_CONCAT(DISTINCT d.field_name) AS available_date_fields
  FROM raw_date_candidates d
  JOIN usage_artifacts ua ON ua.artifact_id=d.artifact_id
  WHERE d.artifact_id IS NOT NULL
    AND COALESCE(d.parsed_utc,'')<>''
  GROUP BY d.artifact_id
)
SELECT a.artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.spotlight_display_path,
       a.normalized_mac_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.authors,
       a.creator,
       a.existence_status,
       a.deleted_or_orphaned_candidate,
       a.confidence,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.used_dates_utc,
       a.use_count_value,
       a.open_count_estimate,
       COALESCE(ud.usage_earliest_utc, NULLIF(a.first_used_candidate_utc,'')) AS usage_earliest_utc,
       COALESCE(ud.usage_latest_utc, NULLIF(a.last_used_date_utc,''), NULLIF(a.first_used_candidate_utc,'')) AS usage_latest_utc,
       COALESCE(ud.fused_usage_dates_utc, NULLIF(a.used_dates_utc,''), NULLIF(a.last_used_date_utc,''), NULLIF(a.first_used_candidate_utc,'')) AS fused_usage_dates_utc,
       COALESCE(ud.usage_date_row_count, 0) AS usage_date_row_count,
       COALESCE(ur.usage_evidence_row_count, 0) AS usage_evidence_row_count,
       COALESCE(NULLIF(ud.usage_date_source_fields,''), NULLIF(ur.usage_evidence_fields,''), '') AS usage_source_fields,
       COALESCE(NULLIF(a.usage_field_summary,''), NULLIF(ur.usage_evidence_values,''), '') AS usage_field_summary,
       COALESCE(NULLIF(ur.usage_evidence_values,''), NULLIF(a.usage_field_summary,''), '') AS usage_supporting_values,
       COALESCE(ud.likely_snapshot_or_index_usage_date_count, 0) AS likely_snapshot_or_index_usage_date_count,
       COALESCE(ud.snapshot_warning_reasons, '') AS snapshot_warning_reasons,
       dw.created_earliest_utc,
       dw.created_latest_utc,
       dw.modified_earliest_utc,
       dw.modified_latest_utc,
       dw.downloaded_earliest_utc,
       dw.downloaded_latest_utc,
       dw.interesting_or_index_earliest_utc,
       dw.interesting_or_index_latest_utc,
       COALESCE(dw.likely_snapshot_date_count,0) AS likely_snapshot_date_count,
       COALESCE(dw.available_date_fields,'') AS available_date_fields,
       CASE
)SQL" R"SQL(         WHEN COALESCE(ud.usage_date_row_count,0)>0 AND COALESCE(ur.usage_evidence_row_count,0)>0 THEN 'DATE_ATTRIBUTION_AND_USAGE_EVIDENCE'
         WHEN COALESCE(ud.usage_date_row_count,0)>0 THEN 'DATE_ATTRIBUTION_USAGE_FIELDS'
         WHEN COALESCE(ur.usage_evidence_row_count,0)>0 THEN 'USAGE_EVIDENCE_ROWS'
         WHEN COALESCE(a.used_dates_count,0)>0 OR COALESCE(NULLIF(a.last_used_date_utc,''),'')<>'' OR COALESCE(NULLIF(a.first_used_candidate_utc,''),'')<>'' THEN 'ARTIFACT_USAGE_COLUMNS'
         WHEN COALESCE(NULLIF(a.use_count_value,''),'')<>'' OR COALESCE(a.open_count_estimate,0)>0 THEN 'USAGE_COUNT_ONLY'
         ELSE 'NO_USAGE_SIGNAL'
       END AS object_usage_basis
FROM usage_artifacts ua
JOIN artifacts a ON a.artifact_id=ua.artifact_id
LEFT JOIN usage_dates ud ON ud.artifact_id=a.artifact_id
LEFT JOIN usage_rows ur ON ur.artifact_id=a.artifact_id
LEFT JOIN date_wide dw ON dw.artifact_id=a.artifact_id;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_usage_timeline_attributed;
CREATE VIEW vw_usage_timeline_attributed AS
SELECT ROW_NUMBER() OVER (ORDER BY COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,''), artifact_id) DESC, artifact_id DESC) AS timeline_id,
       COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,'')) AS event_utc,
       'usage_summary' AS date_type,
       usage_source_fields AS source_field,
       object_usage_basis AS usage_reason,
       CASE WHEN COALESCE(likely_snapshot_or_index_usage_date_count,0)>0 THEN 1 ELSE 0 END AS likely_snapshot_or_index_date,
       snapshot_warning_reasons AS snapshot_warning_reason,
       artifact_id,
       source_id,
       store_guid,
       inode_num,
       parent_inode_num,
       file_name,
       display_name,
       best_path,
       path_source,
       path_status,
       content_type,
       logical_size_bytes,
       physical_size_bytes,
       use_count_value,
       open_count_estimate,
       used_dates_count,
       usage_earliest_utc,
       usage_latest_utc,
       fused_usage_dates_utc,
       usage_date_row_count,
       usage_evidence_row_count,
       where_froms,
       confidence
FROM vw_object_usage_summary;

)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_apple_messages_database_status;
CREATE VIEW vw_ios_apple_messages_database_status AS
WITH sms_db AS (
  SELECT ios_db_id,source_id,normalized_path,database_name,database_category,app_hint,parse_status,record_inventory_status,extracted_path
  FROM ios_app_database_inventory
  WHERE database_category='APPLE_MESSAGES'
    AND lower(database_name) IN ('sms.db','chat.db')
    AND lower(normalized_path) NOT LIKE '%-wal'
    AND lower(normalized_path) NOT LIKE '%-shm'
), ri AS (
  SELECT ios_db_id,
         SUM(CASE WHEN table_name='message' THEN COALESCE(row_count,0) ELSE 0 END) AS message_rows,
         SUM(CASE WHEN table_name='chat' THEN COALESCE(row_count,0) ELSE 0 END) AS chat_rows,
         SUM(CASE WHEN table_name='attachment' THEN COALESCE(row_count,0) ELSE 0 END) AS attachment_rows,
         SUM(CASE WHEN table_name='handle' THEN COALESCE(row_count,0) ELSE 0 END) AS handle_rows,
         SUM(CASE WHEN table_name IN ('chat_message_join','message_attachment_join','chat_handle_join') THEN COALESCE(row_count,0) ELSE 0 END) AS join_rows,
         GROUP_CONCAT(CASE WHEN record_category IN ('MESSAGE_RECORDS','MESSAGE_ATTACHMENTS','MESSAGE_PARTICIPANTS') THEN table_name || ':' || COALESCE(row_count,0) END) AS relevant_table_counts
  FROM ios_app_database_record_inventory
  GROUP BY ios_db_id
), pr AS (
  SELECT ios_db_id,COUNT(*) AS parsed_message_rows
  FROM ios_app_parsed_records
  WHERE database_category='APPLE_MESSAGES'
  GROUP BY ios_db_id
)
SELECT d.ios_db_id,d.source_id,d.normalized_path,d.database_name,d.app_hint,d.parse_status,d.record_inventory_status,d.extracted_path,
       COALESCE(ri.message_rows,0) AS message_rows,
       COALESCE(ri.chat_rows,0) AS chat_rows,
       COALESCE(ri.attachment_rows,0) AS attachment_rows,
       COALESCE(ri.handle_rows,0) AS handle_rows,
       COALESCE(ri.join_rows,0) AS join_rows,
       COALESCE(pr.parsed_message_rows,0) AS parsed_message_rows,
       COALESCE(ri.relevant_table_counts,'') AS relevant_table_counts,
       CASE
         WHEN COALESCE(pr.parsed_message_rows,0)>0 THEN 'SMS_DB_PRESENT_WITH_PARSED_ROWS'
         WHEN COALESCE(ri.message_rows,0)>0 OR COALESCE(ri.attachment_rows,0)>0 THEN 'SMS_DB_PRESENT_RELEVANT_ROWS_NOT_PARSED'
         WHEN d.ios_db_id IS NOT NULL THEN 'SMS_DB_PRESENT_NO_LIVE_MESSAGE_CHAT_ATTACHMENT_ROWS'
         ELSE 'SMS_DB_NOT_FOUND'
       END AS apple_messages_residency_status,
       'Spotlight may contain message-like text even when the live SMS.db message/chat/attachment tables are empty; treat as Spotlight-only candidate unless a matching app database row is parsed.' AS interpretation_note
FROM sms_db d
LEFT JOIN ri ON ri.ios_db_id=d.ios_db_id
LEFT JOIN pr ON pr.ios_db_id=d.ios_db_id;
)SQL");

    execGuiSqlParts({
        R"VSGUI(
DROP VIEW IF EXISTS vw_ios_whatsapp_database_status;
CREATE VIEW vw_ios_whatsapp_database_status AS
WITH src AS (
  SELECT source_id FROM evidence_sources ORDER BY added_utc LIMIT 1
), wa_db AS (
  SELECT ios_db_id,source_id,normalized_path,database_name,database_category,app_hint,parse_status,record_inventory_status,extracted_path
  FROM ios_app_database_inventory
  WHERE database_category='WHATSAPP'
    AND lower(normalized_path) NOT LIKE '%-wal'
    AND lower(normalized_path) NOT LIKE '%-shm'
), ri AS (
  SELECT ios_db_id,
         SUM(CASE WHEN lower(table_name)='zwamessage' THEN COALESCE(row_count,0) ELSE 0 END) AS message_rows,
         SUM(CASE WHEN lower(table_name)='zwamediaitem' THEN COALESCE(row_count,0) ELSE 0 END) AS media_rows,
         SUM(CASE WHEN lower(table_name)='zwachatsession' THEN COALESCE(row_count,0) ELSE 0 END) AS chat_rows,
         SUM(CASE WHEN lower(table_name) IN ('zwaaddressbookcontact','zwaprofilepushname','zwagroupmember','zwavcardmention') THEN COALESCE(row_count,0) ELSE 0 END) AS contact_or_member_rows,
         SUM(CASE WHEN lower(table_name) LIKE '%call%' THEN COALESCE(row_count,0) ELSE 0 END) AS call_rows,
         GROUP_CONCAT(CASE WHEN record_category IN ('MESSAGE_RECORDS','MESSAGE_ATTACHMENTS','MESSAGE_PARTICIPANTS','CHAT_RECORDS','CALL_RECORDS') THEN table_name || ':' || COALESCE(row_count,0) END) AS relevant_table_counts
  FROM ios_app_database_record_inventory
  GROUP BY ios_db_id
), pr AS (
  SELECT ios_db_id,COUNT(*) AS parsed_whatsapp_rows
  FROM ios_app_parsed_records
  WHERE database_category='WHATSAPP'
  GROUP BY ios_db_id
), status_rows AS (
  SELECT d.ios_db_id,d.source_id,d.normalized_path,d.database_name,d.app_hint,d.parse_status,d.record_inventory_status,d.extracted_path,
         COALESCE(ri.message_rows,0) AS message_rows,
         COALESCE(ri.media_rows,0) AS media_rows,
         COALESCE(ri.chat_rows,0) AS chat_rows,
         COALESCE(ri.contact_or_member_rows,0) AS contact_or_member_rows,
         COALESCE(ri.call_rows,0) AS call_rows,
         COALESCE(pr.parsed_whatsapp_rows,0) AS parsed_whatsapp_rows,
         COALESCE(ri.relevant_table_counts,'') AS relevant_table_counts,
         CASE
           WHEN COALESCE(pr.parsed_whatsapp_rows,0)>0 THEN 'WHATSAPP_DB_PRESENT_WITH_PARSED_ROWS'
           WHEN COALESCE(ri.message_rows,0)>0 OR COALESCE(ri.media_rows,0)>0 OR COALESCE(ri.chat_rows,0)>0 OR COALESCE(ri.call_rows,0)>0 THEN 'WHATSAPP_DB_PRESENT_RELEVANT_ROWS_NOT_PARSED'
           ELSE 'WHATSAPP_DB_PRESENT_NO_RELEVANT_LIVE_ROWS'
         END AS whatsapp_residency_status,
         'WhatsApp rows are parsed from staged iOS app SQLite databases using schema patterns from the uploaded local iLEAPP and WhatsApp references; encrypted or absent databases remain inventory-only.' AS interpretation_note
  FROM wa_db d
  LEFT JOIN ri ON ri.ios_db_id=d.ios_db_id
  LEFT JOIN pr ON pr.ios_db_id=d.ios_db_id
)
SELECT * FROM status_rows
UNION ALL
SELECT NULL AS ios_db_id, COALESCE((SELECT source_id FROM src),'') AS source_id, '' AS normalized_path,
       'ChatStorage.sqlite / ContactsV2.sqlite / CallHistory.sqlite' AS database_name,
       'WhatsApp' AS app_hint, '' AS parse_status, '' AS record_inventory_status, '' AS extracted_path,
       0 AS message_rows, 0 AS media_rows, 0 AS chat_rows, 0 AS contact_or_member_rows, 0 AS call_rows, 0 AS parsed_whatsapp_rows,
       '' AS relevant_table_counts, 'WHATSAPP_DB_NOT_FOUND' AS whatsapp_residency_status,
       'No iOS WhatsApp ChatStorage/Contacts/CallHistory database was identified in the current FFS inventory; Spotlight WhatsApp-like strings, if any, remain Spotlight-only candidates unless another app database or file path supports them.' AS interpretation_note
WHERE NOT EXISTS (SELECT 1 FROM wa_db);

DROP VIEW IF EXISTS vw_ios_keychain_material_inventory;
CREATE VIEW vw_ios_keychain_material_inventory AS
SELECT source_id, normalized_path, original_zip_entry, file_name, extension, size_bytes, zip_modified_utc,
       protection_class_hint, app_container_hint, domain_hint, sha256_status, inventory_notes,
       CASE
         WHEN lower(file_name) IN ('keychain-2.db','keychain-2-debug.db') THEN 'KEYCHAIN_DATABASE'
         WHEN lower(normalized_path) LIKE '%/private/var/keychains/%' THEN 'KEYCHAIN_DIRECTORY_ARTIFACT'
         WHEN lower(file_name) LIKE '%_keychain.plist' OR lower(file_name)='keychain_decrypted.plist' THEN 'EXTERNAL_OR_DECRYPTED_KEYCHAIN_PLIST'
         WHEN lower(normalized_path) LIKE '%keybag%' THEN 'KEYBAG_OR_KEYCHAIN_SUPPORT'
         ELSE 'KEYCHAIN_CORE_MATERIAL'
       END AS keychain_material_type,
       'Inventory only. Presence of keychain/keybag material in the FFS root does not mean WhatsApp or other app data can be decrypted unless parser-specific key extraction and validation are implemented.' AS interpretation_note
FROM ios_ffs_file_inventory
WHERE lower(normalized_path) LIKE '%/private/var/keychains/%'
   OR lower(file_name) IN ('keychain-2.db','keychain-2-debug.db')
   OR lower(normalized_path) LIKE '%keybag%'
   OR lower(file_name) LIKE '%_keychain.plist' OR lower(file_name)='keychain_decrypted.plist'
ORDER BY keychain_material_type, normalized_path;

DROP VIEW IF EXISTS vw_ios_keychain_support_reference_inventory;
CREATE VIEW vw_ios_keychain_support_reference_inventory AS
SELECT source_id, normalized_path, original_zip_entry, file_name, extension, size_bytes, zip_modified_utc,
       protection_class_hint, app_container_hint, domain_hint, sha256_status, inventory_notes,
       'KEYCHAIN_LIBRARY_OR_CODE_REFERENCE' AS keychain_reference_type,
       'Lower-priority reference: path contains keychain text outside the core /private/var/Keychains or keybag locations. Usually framework/code support, not keychain material.' AS interpretation_note
FROM ios_ffs_file_inventory
WHERE lower(normalized_path) LIKE '%keychain%'
  AND lower(normalized_path) NOT LIKE '%/private/var/keychains/%'
  AND lower(file_name) NOT IN ('keychain-2.db','keychain-2-debug.db','keychain_decrypted.plist')
  AND lower(file_name) NOT LIKE '%_keychain.plist'
  AND lower(normalized_path) NOT LIKE '%keybag%'
ORDER BY normalized_path;

DROP VIEW IF EXISTS vw_ios_protected_data_decryption_candidates;
CREATE VIEW vw_ios_protected_data_decryption_candidates AS
WITH keychain_presence AS (
  SELECT source_id, COUNT(*) AS keychain_material_count,
         GROUP_CONCAT(DISTINCT keychain_material_type) AS keychain_material_types
  FROM vw_ios_keychain_material_inventory
  GROUP BY source_id
)
SELECT d.source_id,
       d.ios_db_id,
       d.normalized_path,
       d.database_name,
       d.database_category,
       d.app_hint,
       d.protection_class_hint,
       d.parse_status,
       d.record_inventory_status,
       COALESCE(k.keychain_material_count,0) AS keychain_material_count,
       COALESCE(k.keychain_material_types,'') AS keychain_material_types,
       CASE
         WHEN COALESCE(k.keychain_material_count,0)>0 AND (lower(d.protection_class_hint) LIKE '%complete%' OR lower(d.notes) LIKE '%encrypted%' OR lower(d.parse_status) LIKE '%encrypted%' OR lower(d.parse_status) LIKE '%failed%') THEN 'KEYCHAIN_MATERIAL_PRESENT_AND_DATABASE_MAY_BE_PROTECTED'
         WHEN COALESCE(k.keychain_material_count,0)>0 THEN 'KEYCHAIN_MATERIAL_PRESENT_FOR_CASE'
         ELSE 'NO_KEYCHAIN_MATERIAL_IN_INVENTORY'
       END AS candidate_status,
       'Candidate correlation only. This does not decrypt data or prove that a keychain item unlocks this database. Use for prioritizing future validated decryption workflows.' AS interpretation_note
FROM ios_app_database_inventory d
LEFT JOIN keychain_presence k ON k.source_id=d.source_id
WHERE d.database_category IN ('APPLE_MESSAGES','WHATSAPP','SIGNAL','TELEGRAM','SAFARI_WEB','CHROME_WEB','WEBKIT','MAIL','CALENDAR','CONTACTS','KNOWLEDGEC_COREDUET','KEYCHAIN')
   OR lower(d.normalized_path) LIKE '%nsfileprotection%'
   OR lower(d.parse_status) LIKE '%encrypted%'
   OR lower(d.notes) LIKE '%encrypted%'
ORDER BY candidate_status DESC, d.database_category, d.normalized_path;

DROP VIEW IF EXISTS vw_ios_communications_review_records;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_communications_review_records AS
SELECT ios_app_record_id, source_id, ios_db_id,
       CASE
         WHEN database_category='APPLE_MESSAGES' THEN 'Apple Messages / SMS.db'
         WHEN database_category='WHATSAPP' THEN 'WhatsApp'
         WHEN database_category='CALL_HISTORY' THEN 'Phone / FaceTime Call History'
         WHEN lower(database_category) LIKE '%mail%' OR lower(app_hint) LIKE '%mail%' THEN 'Mail'
         ELSE COALESCE(NULLIF(app_hint,''), database_category)
       END AS communication_source,
       database_normalized_path, database_name, database_category, app_hint, table_name, record_category, source_primary_key,
       record_timestamp_utc, timestamp_source, contact_or_participant, url, title, file_path, item_identifier, text_snippet, parse_status, provenance,
       CASE
         WHEN lower(record_category) LIKE '%call%' THEN 'CALL'
         WHEN lower(record_category) LIKE '%attachment%' OR COALESCE(file_path,'')<>'' THEN 'ATTACHMENT_OR_MEDIA'
         WHEN lower(record_category) LIKE '%participant%' OR lower(record_category) LIKE '%contact%' THEN 'PARTICIPANT_OR_CONTACT'
         WHEN lower(record_category) LIKE '%chat%' THEN 'CHAT_OR_THREAD'
         WHEN lower(record_category) LIKE '%message%' THEN 'MESSAGE'
         ELSE 'COMMUNICATION_RELATED_RECORD'
       END AS communication_record_type,
       CASE WHEN COALESCE(record_timestamp_utc,'')<>'' THEN 'APP_DB_TIMESTAMP' ELSE 'APP_DB_RECORD_NO_NORMALIZED_TIMESTAMP' END AS timeline_basis,
       'Parsed local app database record. Treat as live/acquired app data only for the staged database listed; correlate with Spotlight separately before drawing deletion or residency conclusions.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category IN ('APPLE_MESSAGES','WHATSAPP','CALL_HISTORY')
   OR lower(record_category) LIKE '%message%'
   OR lower(record_category) LIKE '%call%'
   OR lower(record_category) LIKE '%chat%'
   OR lower(record_category) LIKE '%participant%'
   OR lower(table_name) LIKE '%message%'
   OR lower(table_name) LIKE '%chat%'
   OR lower(table_name) LIKE '%call%';

DROP VIEW IF EXISTS vw_ios_communications_review_summary;
CREATE VIEW vw_ios_communications_review_summary AS
SELECT communication_source, database_category, app_hint, record_category, communication_record_type, parse_status,
       COUNT(*) AS parsed_record_count, COUNT(DISTINCT ios_db_id) AS database_count,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc, MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc,
       SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS records_with_contact_or_participant,
       SUM(CASE WHEN COALESCE(text_snippet,'')<>'' THEN 1 ELSE 0 END) AS records_with_text,
       SUM(CASE WHEN COALESCE(url,'')<>'' THEN 1 ELSE 0 END) AS records_with_url,
       SUM(CASE WHEN COALESCE(file_path,'')<>'' THEN 1 ELSE 0 END) AS records_with_file_path,
       MIN(database_normalized_path) AS first_database_path, MAX(database_normalized_path) AS last_database_path
FROM vw_ios_communications_review_records
GROUP BY communication_source, database_category, app_hint, record_category, communication_record_type, parse_status
ORDER BY communication_source, record_category, communication_record_type;

DROP VIEW IF EXISTS vw_ios_communication_frequency;
CREATE VIEW vw_ios_communication_frequency AS
SELECT
    COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key) AS communication_thread_id,
    COUNT(*) AS total_records_in_thread,
    MIN(record_timestamp_utc) AS first_communication_utc,
    MAX(record_timestamp_utc) AS last_communication_utc,
    GROUP_CONCAT(DISTINCT contact_or_participant) AS involved_identities,
    GROUP_CONCAT(DISTINCT app_hint) AS apps_utilized,
    GROUP_CONCAT(DISTINCT record_category) AS record_categories,
    GROUP_CONCAT(DISTINCT parse_status) AS parse_statuses,
    'Thread/contact grouped iOS communication frequency view. Thread identifiers use item_identifier when available, then contact/source primary key fallback. Treat as committed parsed-record frequency, not a final communication assertion without source-row review.' AS interpretation_note
FROM ios_app_parsed_records
WHERE (record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_EVENTS')
       OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
       OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
       OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%')
  AND COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key) IS NOT NULL
GROUP BY COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key)
ORDER BY total_records_in_thread DESC, last_communication_utc DESC;

DROP VIEW IF EXISTS vw_ios_communication_existence_evidence;
CREATE VIEW vw_ios_communication_existence_evidence AS
SELECT
  ios_app_record_id,
  source_id,
  database_category,
  app_hint,
  database_name,
  table_name,
  record_category,
  source_primary_key,
  record_timestamp_utc,
  timestamp_source,
  COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key) AS communication_thread_id,
  contact_or_participant AS identity_hint,
  url,
  title,
  file_path,
  substr(text_snippet,1,500) AS text_snippet_sample,
  parse_status,
  provenance,
  CASE
    WHEN record_category='MESSAGE_DELETED_OR_RECOVERABLE' THEN 'deleted_or_recoverable_message_table_or_spotlight_marker'
    WHEN provenance LIKE '%COMMUNICATION_INTENT_STREAM%' OR provenance LIKE '%INTENT_TARGET%' THEN 'knowledgec_or_intent_communication_marker'
    WHEN provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%' THEN 'domain_identifier_or_thread_marker'
    WHEN provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%' THEN 'author_recipient_or_identity_marker'
    WHEN record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS') THEN 'parsed_communication_app_database_record'
    ELSE 'communication_related_parsed_record'
  END AS existence_basis,
  'Presence/frequency support view. Rows show committed parsed records and provenance markers that support existence or activity frequency; review source row before making final conclusions.' AS interpretation_note
FROM ios_app_parsed_records
WHERE record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_EVENTS')
   OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
   OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
   OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%'
   OR provenance LIKE '%INTENT_TARGET%';

DROP VIEW IF EXISTS vw_ios_communication_identity_frequency;
CREATE VIEW vw_ios_communication_identity_frequency AS
SELECT
  COALESCE(NULLIF(contact_or_participant,''), NULLIF(item_identifier,''), '(no explicit identity)') AS identity_or_thread_hint,
  database_category,
  app_hint,
  record_category,
  COUNT(*) AS related_record_count,
  COUNT(DISTINCT COALESCE(NULLIF(item_identifier,''), source_primary_key)) AS distinct_thread_or_record_keys,
  MIN(NULLIF(record_timestamp_utc,'')) AS first_seen_utc,
  MAX(NULLIF(record_timestamp_utc,'')) AS last_seen_utc,
  GROUP_CONCAT(DISTINCT table_name) AS source_tables,
  GROUP_CONCAT(DISTINCT parse_status) AS parse_statuses,
  'Identity/frequency rollup from parsed app records and communication provenance markers.' AS interpretation_note
FROM ios_app_parsed_records
WHERE record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_EVENTS')
   OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
   OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
   OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%'
GROUP BY COALESCE(NULLIF(contact_or_participant,''), NULLIF(item_identifier,''), '(no explicit identity)'), database_category, app_hint, record_category
ORDER BY related_record_count DESC, last_seen_utc DESC;

DROP VIEW IF EXISTS vw_ios_communication_temporal_frequency;
CREATE VIEW vw_ios_communication_temporal_frequency AS
SELECT
  substr(record_timestamp_utc,1,10) AS communication_date_utc,
  COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), '(no explicit thread)') AS communication_thread_or_identity,
  database_category,
  app_hint,
  record_category,
  COUNT(*) AS records_on_date,
  COUNT(DISTINCT contact_or_participant) AS distinct_identity_hints,
  MIN(record_timestamp_utc) AS first_record_utc,
  MAX(record_timestamp_utc) AS last_record_utc,
  GROUP_CONCAT(DISTINCT parse_status) AS parse_statuses
FROM ios_app_parsed_records
WHERE COALESCE(record_timestamp_utc,'')<>''
  AND (record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_EVENTS')
       OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
       OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
       OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%')
GROUP BY substr(record_timestamp_utc,1,10), COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), '(no explicit thread)'), database_category, app_hint, record_category
ORDER BY communication_date_utc DESC, records_on_date DESC;

DROP VIEW IF EXISTS vw_ios_communication_source_coverage;
CREATE VIEW vw_ios_communication_source_coverage AS
SELECT
  database_category,
  app_hint,
  database_name,
  table_name,
  record_category,
  parse_status,
  COUNT(*) AS parsed_record_count,
  SUM(CASE WHEN COALESCE(record_timestamp_utc,'')<>'' THEN 1 ELSE 0 END) AS records_with_timestamp,
  SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS records_with_identity_hint,
  SUM(CASE WHEN COALESCE(item_identifier,'')<>'' THEN 1 ELSE 0 END) AS records_with_thread_or_item_id,
  MIN(NULLIF(record_timestamp_utc,'')) AS first_seen_utc,
  MAX(NULLIF(record_timestamp_utc,'')) AS last_seen_utc,
  'Communication existence/frequency source coverage by database/table/category.' AS interpretation_note
FROM ios_app_parsed_records
WHERE record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_EVENTS')
   OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
   OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
   OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%'
GROUP BY database_category, app_hint, database_name, table_name, record_category, parse_status
ORDER BY parsed_record_count DESC, records_with_timestamp DESC;

DROP VIEW IF EXISTS vw_ios_spotlight_communication_candidates;
CREATE VIEW vw_ios_spotlight_communication_candidates AS
WITH probe AS (
  SELECT kv.raw_kv_id, kv.source_id, kv.store_guid, kv.source_db, kv.inode_num, kv.store_id, kv.parent_inode_num, kv.field_name, kv.field_value,
         lower(kv.field_value) AS v, rr.last_updated_utc
  FROM raw_key_values kv
  LEFT JOIN raw_records rr ON rr.source_id=kv.source_id AND rr.store_guid=kv.store_guid AND rr.source_db=kv.source_db AND rr.inode_num=kv.inode_num AND COALESCE(rr.store_id,'')=COALESCE(kv.store_id,'')
  WHERE COALESCE(kv.field_value,'')<>''
    AND (kv.store_guid LIKE 'ios_%' OR kv.source_db LIKE '%CoreSpotlight%' OR kv.store_path LIKE '%CoreSpotlight%')
), categorized AS (
  SELECT *,
         CASE
           WHEN v LIKE '%whatsapp%' THEN 'WHATSAPP_TEXT_OR_REFERENCE'
           WHEN v LIKE '%imessage%' OR v LIKE '%sms.db%' OR v LIKE '%/sms/%' OR v LIKE '%com.apple.mobilesms%' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
           WHEN v LIKE '%message%' OR v LIKE '%chat%' OR v LIKE '%conversation%' THEN 'MESSAGE_OR_CHAT_TEXT_CANDIDATE'
           WHEN v LIKE '%facetime%' OR v LIKE '%callhistory%' OR v LIKE '%call history%' THEN 'PHONE_OR_FACETIME_RELATED'
           WHEN v LIKE '%mailto:%' OR (v LIKE '%@%' AND v LIKE '%.%') THEN 'EMAIL_OR_ACCOUNT_RELATED'
           ELSE 'OTHER_COMMUNICATION_RELATED'
         END AS communication_candidate_type
  FROM probe
  WHERE v LIKE '%whatsapp%' OR v LIKE '%imessage%' OR v LIKE '%sms.db%' OR v LIKE '%/sms/%' OR v LIKE '%com.apple.mobilesms%'
     OR v LIKE '%message%' OR v LIKE '%chat%' OR v LIKE '%conversation%' OR v LIKE '%facetime%' OR v LIKE '%callhistory%' OR v LIKE '%call history%'
     OR v LIKE '%mailto:%' OR (v LIKE '%@%' AND v LIKE '%.%')
)
SELECT raw_kv_id, source_id, store_guid, source_db, inode_num AS spotlight_inode_or_object_id, store_id AS spotlight_store_id, parent_inode_num AS spotlight_parent_id,
       field_name, communication_candidate_type, last_updated_utc AS spotlight_last_updated_utc, substr(field_value,1,600) AS string_value_sample,
       CASE
         WHEN communication_candidate_type='WHATSAPP_TEXT_OR_REFERENCE' AND EXISTS (SELECT 1 FROM vw_ios_whatsapp_database_status WHERE whatsapp_residency_status<>'WHATSAPP_DB_NOT_FOUND') THEN 'WHATSAPP_DATABASE_FAMILY_PRESENT'
         WHEN communication_candidate_type='APPLE_MESSAGES_OR_SMS_RELATED' AND EXISTS (SELECT 1 FROM vw_ios_apple_messages_database_status WHERE apple_messages_residency_status<>'SMS_DB_NOT_FOUND') THEN 'SMS_DATABASE_FAMILY_PRESENT'
         WHEN communication_candidate_type='PHONE_OR_FACETIME_RELATED' AND EXISTS (SELECT 1 FROM ios_app_database_inventory WHERE database_category='CALL_HISTORY') THEN 'CALL_HISTORY_DATABASE_FAMILY_PRESENT'
         ELSE 'NO_CONFIRMED_MATCHING_APP_DATABASE_ROW'
       END AS communication_residency_context,
       'Spotlight string candidate only. Review parsed app database views and exact links before treating this as a live message/call/chat record.' AS interpretation_note
FROM categorized
ORDER BY communication_candidate_type, raw_kv_id;

)VSGUI"
    });

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_app_live_activity_timeline;
CREATE VIEW vw_ios_app_live_activity_timeline AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,
       'app_database_record_time' AS timeline_basis,
       'Parsed local app database row; stronger than Spotlight Last_Updated but still requires schema-specific interpretation.' AS interpretation_note
FROM ios_app_parsed_records
WHERE COALESCE(record_timestamp_utc,'')<>''
ORDER BY record_timestamp_utc DESC, database_category, record_category, ios_app_record_id;
)SQL");

    execGuiSqlParts({
        R"VSGUI(
DROP VIEW IF EXISTS vw_ios_spotlight_human_text_values;
CREATE VIEW vw_ios_spotlight_human_text_values AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,store_path,inode_num,store_id,parent_inode_num,field_name,field_value,
         LOWER(COALESCE(field_value,'')) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), labeled AS (
  SELECT *,
    CASE
      WHEN v LIKE '%file://%' OR v LIKE '%/private/var/mobile/%' OR v LIKE '%/var/mobile/%' THEN 'FILE_PATH_OR_ATTACHMENT'
      WHEN v LIKE '%zoom.%' OR v LIKE '%meet.google.%' OR v LIKE '%teams.microsoft.%' OR v LIKE '%webex.%' THEN 'MEETING_OR_CONFERENCE'
      WHEN v LIKE '%.ics%' OR v LIKE '%text/calendar%' OR v LIKE '%vevent%' OR v LIKE '%calendar.google.com%' OR v LIKE '%/calendar/%' THEN 'CALENDAR_OR_INVITATION'
      WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'WEB_OR_URL'
      WHEN v LIKE 'from:%' OR v LIKE 'to:%' OR v LIKE 'cc:%' OR v LIKE '%mailto:%' OR (v LIKE '%@%' AND v NOT LIKE '%http://%' AND v NOT LIKE '%https://%') THEN 'EMAIL_OR_ACCOUNT_TEXT'
      WHEN v LIKE '%net.whatsapp.whatsapp%' OR v LIKE '%chat.whatsapp.com%' OR v LIKE '%wa.me/%' OR v LIKE '%api.whatsapp.com%' OR v LIKE '%whatsapp group%' THEN 'WHATSAPP_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.whispersystems.signal%' OR v LIKE '%signal.messenger%' OR v LIKE '%signal.org/%' THEN 'SIGNAL_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.telegram%' OR v LIKE '%telegram.messenger%' OR v LIKE '%t.me/%' OR v LIKE '%telegram.me/%' THEN 'TELEGRAM_TEXT_OR_REFERENCE'
      ELSE 'OTHER_HUMAN_READABLE_TEXT'
    END AS human_text_category,
    TRIM(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(COALESCE(field_value,''),
      char(13),' '), char(10),' '), char(9),' '), '<br>',' '), '<br/>',' '), '<br />',' '),
      '&amp;','&'), '&lt;','<'), '&gt;','>'), '&nbsp;',' '), '&quot;','"')) AS readable_text
  FROM probes
)
SELECT l.raw_kv_id,COALESCE(r.raw_record_id,0) AS raw_record_id,l.source_id,l.store_guid,l.source_db,l.inode_num,l.store_id,l.parent_inode_num,l.field_name,
       l.human_text_category,
       LENGTH(COALESCE(l.field_value,'')) AS original_value_length,
       substr(l.readable_text,1,3000) AS readable_text_sample,
       CASE
         WHEN l.human_text_category IN ('FILE_PATH_OR_ATTACHMENT','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION','WEB_OR_URL','EMAIL_OR_ACCOUNT_TEXT') THEN 'HIGH_HUMAN_REVIEW_VALUE'
         WHEN LENGTH(l.readable_text)>=24 THEN 'MEDIUM_HUMAN_REVIEW_VALUE'
         ELSE 'LOW_HUMAN_REVIEW_VALUE'
       END AS review_priority,
       'Generic iOS CoreSpotlight text recovery; formal CoreSpotlight property names/dbStr maps remain a later parser phase.' AS interpretation_note
FROM labeled l
LEFT JOIN (
  SELECT source_id,store_guid,source_db,inode_num,COALESCE(store_id,'') AS store_id_key,MIN(raw_record_id) AS raw_record_id
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,inode_num,COALESCE(store_id,'')
) r ON r.source_id=l.source_id AND r.store_guid=l.store_guid AND r.source_db=l.source_db AND r.inode_num=l.inode_num AND r.store_id_key=COALESCE(l.store_id,'')
WHERE LENGTH(l.readable_text)>=4;

DROP VIEW IF EXISTS vw_ios_spotlight_human_text_rollup;
CREATE VIEW vw_ios_spotlight_human_text_rollup AS
WITH text_values AS (
  SELECT v.*, r.last_updated_utc, r.record_state
  FROM vw_ios_spotlight_human_text_values v
  LEFT JOIN raw_records r ON r.raw_record_id=v.raw_record_id
)
SELECT raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT human_text_category) AS distinct_text_category_count,
       GROUP_CONCAT(DISTINCT human_text_category) AS human_text_categories,
       MAX(CASE WHEN review_priority='HIGH_HUMAN_REVIEW_VALUE' THEN 1 ELSE 0 END) AS has_high_review_value_text,
       MIN(NULLIF(last_updated_utc,'')) AS last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       substr(GROUP_CONCAT(field_name || '=' || readable_text_sample, ' || '),1,6000) AS readable_text_rollup_sample,
       'Record-level human-readable text rollup from iOS CoreSpotlight string probes.' AS interpretation_note
FROM text_values
GROUP BY raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num;


DROP VIEW IF EXISTS vw_ios_spotlight_investigative_items_with_dates;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_investigative_items_with_dates AS
SELECT v.raw_kv_id,
       v.raw_record_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.original_value_length,
       v.readable_text_sample,
       v.review_priority,
       dp.spotlight_date_utc,
       dp.spotlight_date_source_field,
       dp.spotlight_date_source_table,
       dp.spotlight_date_raw_value,
       dp.spotlight_date_parse_method,
       dp.spotlight_date_type,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%creation%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%created%' THEN 'created_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modification%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modified%' THEN 'modified_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%access%' THEN 'accessed_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%open%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'metadata_seen_or_index_updated'
         ELSE 'unclassified_spotlight_date_candidate'
       END AS spotlight_date_semantic_class,
       dp.spotlight_date_source_evidence,
       dp.date_validation_hint,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'Do not report as created/modified/accessed/opened. This is CoreSpotlight metadata/index update timing unless another decoded field supports activity semantics.'
         WHEN COALESCE(dp.spotlight_date_source_field,'')<>'' THEN 'Report only with the listed raw Spotlight source field, raw value, parse method, and validation hint.'
         ELSE 'No direct Spotlight date was recovered for this text value.'
       END AS date_reporting_caution,
       'Spotlight/CoreSpotlight extracted text item with attached date provenance where directly linkable by raw_record_id. FFS/app database data is supporting context only.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
LEFT JOIN vw_ios_spotlight_date_provenance dp ON dp.raw_record_id=v.raw_record_id;

DROP VIEW IF EXISTS vw_ios_spotlight_date_field_summary;
CREATE VIEW vw_ios_spotlight_date_field_summary AS
WITH dates AS (
  SELECT source_id, store_guid, source_db, field_name, parse_method, date_type,
         parsed_utc, field_value, inode_num, store_id,
         CASE
           WHEN lower(COALESCE(field_name,'')) LIKE '%creation%' OR lower(COALESCE(field_name,'')) LIKE '%created%' THEN 'created_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%modification%' OR lower(COALESCE(field_name,'')) LIKE '%modified%' THEN 'modified_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%access%' THEN 'accessed_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%open%' OR lower(COALESCE(field_name,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
           WHEN lower(COALESCE(field_name,''))='last_updated' THEN 'metadata_seen_or_index_updated'
           ELSE 'unclassified_spotlight_date_candidate'
         END AS spotlight_date_semantic_class
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
)
SELECT source_id, store_guid, source_db, field_name AS spotlight_date_source_field,
       spotlight_date_semantic_class, COALESCE(date_type,'') AS raw_date_type,
       COALESCE(parse_method,'') AS parse_method,
       COUNT(*) AS date_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_record_count,
       MIN(parsed_utc) AS earliest_parsed_utc,
       MAX(parsed_utc) AS latest_parsed_utc,
       substr(MIN(field_value),1,500) AS min_raw_value_sample,
       substr(MAX(field_value),1,500) AS max_raw_value_sample,
       CASE
         WHEN spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'CoreSpotlight metadata/index update timing; do not report as created/modified/accessed/opened usage without another decoded field.'
         WHEN spotlight_date_semantic_class LIKE '%candidate' THEN 'Candidate semantic class inferred from Spotlight raw date field name; validate field meaning before reporting.'
         ELSE 'Unclassified Spotlight date candidate; validate against raw_date_candidates and source Store-V2 record.'
       END AS reporting_caution
FROM dates
GROUP BY source_id, store_guid, source_db, field_name, spotlight_date_semantic_class, date_type, parse_method;

DROP VIEW IF EXISTS vw_ios_spotlight_investigative_item_date_evidence;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_investigative_item_date_evidence AS
WITH date_evidence AS (
  SELECT raw_date_id, source_id, store_guid, source_db, store_path, inode_num, store_id,
         parent_inode_num, file_name, best_path,
         field_name AS spotlight_date_source_field,
         field_value AS spotlight_date_raw_value,
         parsed_utc AS spotlight_date_utc,
         parse_method AS spotlight_date_parse_method,
         date_type AS spotlight_date_type,
         association_status,
         association_confidence,
         CASE
           WHEN lower(COALESCE(field_name,'')) LIKE '%creation%' OR lower(COALESCE(field_name,'')) LIKE '%created%' THEN 'created_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%modification%' OR lower(COALESCE(field_name,'')) LIKE '%modified%' THEN 'modified_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%access%' THEN 'accessed_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%open%' OR lower(COALESCE(field_name,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
           WHEN lower(COALESCE(field_name,''))='last_updated' THEN 'metadata_seen_or_index_updated'
           ELSE 'unclassified_spotlight_date_candidate'
         END AS spotlight_date_semantic_class
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
)
SELECT v.raw_kv_id,
       v.raw_record_id,
       d.raw_date_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.review_priority,
       v.original_value_length,
       v.readable_text_sample,
       d.spotlight_date_utc,
       d.spotlight_date_source_field,
       'raw_date_candidates' AS spotlight_date_source_table,
       d.spotlight_date_raw_value,
       d.spotlight_date_parse_method,
       d.spotlight_date_type,
       d.spotlight_date_semantic_class,
       d.association_status AS date_association_status,
       d.association_confidence AS date_association_confidence,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(v.raw_kv_id AS TEXT),'') || '; field_name=' || COALESCE(v.field_name,'') AS value_validation_locator,
       'raw_date_candidates.raw_date_id=' || COALESCE(CAST(d.raw_date_id AS TEXT),'') || '; field_name=' || COALESCE(d.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(d.spotlight_date_raw_value,'') || '; parsed_utc=' || COALESCE(d.spotlight_date_utc,'') || '; parse_method=' || COALESCE(d.spotlight_date_parse_method,'') AS date_validation_locator,
       'source_db=' || COALESCE(v.source_db,'') || '; store_guid=' || COALESCE(v.store_guid,'') || '; raw_record_id=' || COALESCE(CAST(v.raw_record_id AS TEXT),'') || '; inode_or_object_id=' || COALESCE(v.inode_num,'') || '; store_id=' || COALESCE(v.store_id,'') AS spotlight_record_locator,
       CASE
         WHEN d.spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'Date is linked to this Spotlight record but represents CoreSpotlight metadata/index update timing unless a separately decoded field supports user activity.'
         WHEN d.spotlight_date_semantic_class IN ('created_date_candidate','modified_date_candidate','accessed_date_candidate','opened_or_used_date_candidate') THEN 'Date is directly linked to this recovered Spotlight value through the same Store-V2 record; validate raw field semantics before reporting as activity.'
         ELSE 'Date is directly linked to this recovered Spotlight value through the same Store-V2 record, but semantic meaning is not yet classified.'
       END AS date_reporting_caution,
       'Each row links one recovered human-readable Spotlight value to one raw Spotlight date candidate from the same Store-V2 record. Use validation locator fields to verify in the parsed raw tables and original store.db.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
JOIN date_evidence d ON d.source_id=v.source_id
                    AND d.store_guid=v.store_guid
                    AND d.source_db=v.source_db
                    AND d.inode_num=v.inode_num
                    AND COALESCE(d.store_id,'')=COALESCE(v.store_id,'');

)VSGUI"
    });

    execGuiSql(R"SQL(

DROP VIEW IF EXISTS vw_ios_spotlight_high_value_timeline;
CREATE VIEW vw_ios_spotlight_high_value_timeline AS
WITH base AS (
  SELECT * FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE review_priority IN ('HIGH_HUMAN_REVIEW_VALUE','MEDIUM_HUMAN_REVIEW_VALUE')
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.original_value_length,
       b.readable_text_sample,b.review_priority,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_source_table,
       b.spotlight_date_raw_value,b.spotlight_date_parse_method,b.spotlight_date_type,
       b.spotlight_date_semantic_class,b.date_validation_hint,b.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FILE_PATH_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       CASE
         WHEN b.spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'SPOTLIGHT_INDEX_TIME_WITH_VALUE_CONTEXT'
         WHEN b.spotlight_date_semantic_class LIKE '%candidate' THEN 'SPOTLIGHT_ACTIVITY_DATE_CANDIDATE_WITH_VALUE_CONTEXT'
         ELSE 'SPOTLIGHT_VALUE_WITH_UNCLASSIFIED_DATE_CONTEXT'
       END AS investigative_timeline_basis,
       'Spotlight-first high-value timeline. FFS and app database fields are context/corroboration only; the Spotlight value/date fields remain the primary evidence to validate.' AS interpretation_note
FROM base b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_file_reference_review;
CREATE VIEW vw_ios_spotlight_file_reference_review AS
WITH ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.readable_text_sample AS spotlight_file_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(f.residency_status,'NO_EXACT_FFS_PATH_LINK') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       CASE WHEN COALESCE(f.residency_status,'')='PRESENT_AS_FILE_IN_FFS' THEN 'SPOTLIGHT_PATH_PRESENT_IN_FFS_INVENTORY'
            WHEN COALESCE(f.residency_status,'')<>'' THEN f.residency_status
            ELSE 'SPOTLIGHT_FILE_REFERENCE_NO_EXACT_FFS_MATCH_IN_CURRENT_LINK_VIEW' END AS file_reference_status,
       'Spotlight/CoreSpotlight file/path reference with date provenance. FFS presence supports current-file existence only and is not proof of use or deletion by itself.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
WHERE b.human_text_category='FILE_PATH_OR_ATTACHMENT';

)SQL");

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_url_reference_review;
CREATE VIEW vw_ios_spotlight_url_reference_review AS
WITH vals AS (
  SELECT *, lower(COALESCE(readable_text_sample,'')) AS v
  FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION')
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT v.raw_kv_id,v.raw_record_id,v.source_id,v.store_guid,v.source_db,
       v.spotlight_inode_or_object_id,v.spotlight_store_id,v.parent_inode_num,
       v.spotlight_value_source_field,v.human_text_category,v.readable_text_sample AS spotlight_url_or_web_reference,
       CASE
         WHEN instr(v.v,'https://')>0 THEN substr(v.v,instr(v.v,'https://'),300)
         WHEN instr(v.v,'http://')>0 THEN substr(v.v,instr(v.v,'http://'),300)
         WHEN instr(v.v,'www.')>0 THEN substr(v.v,instr(v.v,'www.'),300)
         ELSE substr(v.v,1,300)
       END AS normalized_url_reference_sample,
       v.spotlight_date_utc,v.spotlight_date_source_field,v.spotlight_date_raw_value,
       v.spotlight_date_parse_method,v.spotlight_date_semantic_class,v.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       'Spotlight/CoreSpotlight URL/web-like reference with date provenance. Browser/app database fields are supporting context and not an exact value match unless separately validated.' AS interpretation_note
FROM vals v
LEFT JOIN app a ON a.candidate_id=v.raw_kv_id;

)SQL");

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_account_contact_reference_review;
CREATE VIEW vw_ios_spotlight_account_contact_reference_review AS
WITH app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.readable_text_sample AS spotlight_account_or_contact_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       'Spotlight/CoreSpotlight account/contact-like reference with date provenance. Treat as a Spotlight value first; app database context is family-level unless exact string matching is later added.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id
WHERE b.human_text_category IN ('EMAIL_OR_ACCOUNT_TEXT','WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE');

DROP VIEW IF EXISTS vw_ios_spotlight_decode_gap_summary;
CREATE VIEW vw_ios_spotlight_decode_gap_summary AS
WITH gaps AS (
  SELECT source_id,store_guid,source_db,decode_gap_status,last_updated_utc
  FROM vw_ios_spotlight_decode_gap_records
)
SELECT g.source_id,g.store_guid,g.source_db,g.decode_gap_status,
       COUNT(*) AS gap_record_count,
       MIN(NULLIF(g.last_updated_utc,'')) AS earliest_gap_last_updated_utc,
       MAX(NULLIF(g.last_updated_utc,'')) AS latest_gap_last_updated_utc,
       COALESCE(dc.raw_record_count,0) AS store_raw_record_count,
       COALESCE(dc.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(dc.human_text_value_count,0) AS human_text_value_count,
       COALESCE(dc.pct_records_with_human_text,'') AS pct_records_with_human_text,
       COALESCE(dc.decode_failures,0) AS native_decode_failures,
       COALESCE(dc.decode_status,'') AS native_decode_status,
       'Summary of Spotlight/CoreSpotlight records parsed at header level but lacking recovered key/value or human-readable text values. This is the primary native parser improvement target list.' AS interpretation_note
FROM gaps g
LEFT JOIN vw_ios_spotlight_decode_coverage_summary dc ON dc.source_id=g.source_id AND dc.store_guid=g.store_guid AND dc.source_db=g.source_db
GROUP BY g.source_id,g.store_guid,g.source_db,g.decode_gap_status;

)SQL");

    execGuiSqlParts({
        R"VSGUI(

DROP VIEW IF EXISTS vw_ios_spotlight_entity_review;
CREATE VIEW vw_ios_spotlight_entity_review AS
WITH base AS (
  SELECT b.*,
         lower(COALESCE(b.readable_text_sample,'')) AS lower_text
  FROM vw_ios_spotlight_investigative_items_with_dates b
  WHERE COALESCE(b.readable_text_sample,'')<>''
), typed AS (
  SELECT b.*,
         CASE
           WHEN b.human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION') THEN 'URL_OR_WEB_REFERENCE'
           WHEN b.human_text_category='FILE_PATH_OR_ATTACHMENT' THEN 'FILE_OR_ATTACHMENT_REFERENCE'
           WHEN b.human_text_category='EMAIL_OR_ACCOUNT_TEXT' THEN 'ACCOUNT_OR_EMAIL_REFERENCE'
           WHEN b.human_text_category IN ('WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE') THEN 'COMMUNICATION_APP_REFERENCE'
           WHEN b.human_text_category LIKE '%MESSAGE%' THEN 'MESSAGE_OR_COMMUNICATION_TEXT'
           ELSE 'OTHER_SPOTLIGHT_TEXT_REFERENCE'
         END AS entity_type
  FROM base b
), normalized AS (
  SELECT t.*,
         CASE
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'https://')>0 THEN substr(t.lower_text,instr(t.lower_text,'https://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'http://')>0 THEN substr(t.lower_text,instr(t.lower_text,'http://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'www.')>0 THEN substr(t.lower_text,instr(t.lower_text,'www.'),512)
           WHEN t.entity_type='FILE_OR_ATTACHMENT_REFERENCE' THEN replace(replace(replace(replace(t.lower_text,'file://',''),'<',''),'>',''),'\\','/')
           ELSE trim(t.lower_text)
         END AS normalized_entity_value
  FROM typed t
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,matched_record_category,matched_table_name,parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc,sample_parsed_value
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT n.raw_kv_id,
       n.raw_record_id,
       n.source_id,
       n.store_guid,
       n.source_db,
       n.spotlight_inode_or_object_id,
       n.spotlight_store_id,
       n.parent_inode_num,
       n.entity_type,
       n.human_text_category,
       n.review_priority,
       n.spotlight_value_source_field,
       n.normalized_entity_value,
       n.readable_text_sample,
       n.original_value_length,
       n.spotlight_date_utc,
       n.spotlight_date_source_field,
       n.spotlight_date_raw_value,
       n.spotlight_date_parse_method,
       n.spotlight_date_semantic_class,
       n.date_validation_hint,
       n.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FFS_LINK_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_LINK_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.matched_record_category,'') AS matched_record_category,
       COALESCE(a.matched_table_name,'') AS matched_table_name,
       COALESCE(a.sample_parsed_value,'') AS sample_app_db_value,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(n.raw_kv_id AS TEXT),'') || '; raw_records.raw_record_id=' || COALESCE(CAST(n.raw_record_id AS TEXT),'') || '; field_name=' || COALESCE(n.spotlight_value_source_field,'') AS reference_validation_locator,
       'raw_date_candidates.field_name=' || COALESCE(n.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(n.spotlight_date_raw_value,'') || '; parse_method=' || COALESCE(n.spotlight_date_parse_method,'') AS date_validation_locator,
       'Spotlight-first entity view. The entity/value and date columns originate from CoreSpotlight/Spotlight parsed records; app DB and FFS fields are corroborating context only.' AS interpretation_note
FROM normalized n
LEFT JOIN ffs f ON f.reference_id=n.raw_kv_id
LEFT JOIN app a ON a.candidate_id=n.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_entity_summary;
CREATE VIEW vw_ios_spotlight_entity_summary AS
SELECT entity_type,
       human_text_category,
       review_priority,
       store_guid,
       source_db,
       spotlight_value_source_field,
       spotlight_date_semantic_class,
       COUNT(*) AS entity_row_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT normalized_entity_value) AS distinct_normalized_entity_count,
       SUM(CASE WHEN ffs_residency_status='PRESENT_AS_FILE_IN_FFS' THEN 1 ELSE 0 END) AS ffs_present_context_count,
       SUM(CASE WHEN app_db_link_status LIKE 'PRESENT%' OR app_db_link_status LIKE '%PRESENT%' THEN 1 ELSE 0 END) AS app_db_present_context_count,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       MIN(substr(normalized_entity_value,1,240)) AS min_sample_entity,
       MAX(substr(normalized_entity_value,1,240)) AS max_sample_entity,
       'Spotlight entity summary. Counts are derived from recovered CoreSpotlight text/probe values and their Spotlight date provenance; app/FFS context is only supporting context.' AS interpretation_note
FROM vw_ios_spotlight_entity_review
GROUP BY entity_type,human_text_category,review_priority,store_guid,source_db,spotlight_value_source_field,spotlight_date_semantic_class;

DROP VIEW IF EXISTS vw_ios_spotlight_native_parser_targets;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_native_parser_targets AS
SELECT source_id,
       store_guid,
       source_db,
       'RECORDS_WITHOUT_RECOVERED_TEXT' AS parser_target_type,
       decode_gap_status AS target_name,
       gap_record_count AS target_count,
       store_raw_record_count AS store_raw_record_count,
       recovered_key_value_count AS recovered_key_value_count,
       human_text_value_count AS human_text_value_count,
       pct_records_with_human_text AS pct_records_with_human_text,
       native_decode_failures AS native_decode_failures,
       CASE WHEN gap_record_count>100000 THEN 'HIGH' WHEN gap_record_count>10000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Improve native CoreSpotlight property/dictionary/value decoding for records that parse at header level but do not yield recovered text/key-value rows.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_decode_gap_summary
UNION ALL
SELECT source_id,
       store_guid,
       source_db,
       'GENERIC_STRING_PROBE_FIELD' AS parser_target_type,
       field_name AS target_name,
       value_row_count AS target_count,
       NULL AS store_raw_record_count,
       value_row_count AS recovered_key_value_count,
       distinct_record_count AS human_text_value_count,
       '' AS pct_records_with_human_text,
       NULL AS native_decode_failures,
       CASE WHEN value_row_count>10000 THEN 'HIGH' WHEN value_row_count>1000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Map generic __native_core_probe_string_* fields back to real CoreSpotlight property names/types where possible.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_field_coverage_summary
WHERE field_decode_status='GENERIC_NATIVE_STRING_PROBE';

DROP VIEW IF EXISTS vw_ios_spotlight_decode_coverage_summary;
CREATE VIEW vw_ios_spotlight_decode_coverage_summary AS
WITH rr AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS raw_record_count,
         SUM(CASE WHEN COALESCE(last_updated_utc,'')<>'' THEN 1 ELSE 0 END) AS records_with_last_updated,
         MIN(NULLIF(last_updated_utc,'')) AS earliest_last_updated_utc,
         MAX(NULLIF(last_updated_utc,'')) AS latest_last_updated_utc
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), kv AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS recovered_key_value_count,
         COUNT(DISTINCT field_name) AS recovered_field_name_count,
         COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS records_with_recovered_values
  FROM raw_key_values
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), ht AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS human_text_value_count,
         COUNT(DISTINCT raw_record_id) AS records_with_human_text,
         COUNT(DISTINCT human_text_category) AS human_text_category_count
  FROM vw_ios_spotlight_human_text_values
  GROUP BY source_id,store_guid,source_db
), nd AS (
  SELECT source_id,store_guid,source_db,
         MAX(decode_mode) AS decode_mode,
         MAX(spotlight_version) AS spotlight_version,
         MAX(properties_count) AS native_property_count,
         MAX(categories_count) AS native_category_count,
         MAX(metadata_blocks) AS metadata_blocks,
         MAX(decompressed_blocks) AS decompressed_blocks,
         MAX(failures) AS decode_failures,
         MAX(status) AS decode_status,
         MAX(message) AS decode_message
  FROM native_decode_attempts
  GROUP BY source_id,store_guid,source_db
)
SELECT rr.source_id,rr.store_guid,rr.source_db,
       COALESCE(nd.decode_mode,'') AS decode_mode,
       COALESCE(nd.spotlight_version,0) AS spotlight_version,
       rr.raw_record_count,
       COALESCE(kv.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(kv.recovered_field_name_count,0) AS recovered_field_name_count,
       COALESCE(kv.records_with_recovered_values,0) AS records_with_recovered_values,
       COALESCE(ht.human_text_value_count,0) AS human_text_value_count,
       COALESCE(ht.records_with_human_text,0) AS records_with_human_text,
       COALESCE(ht.human_text_category_count,0) AS human_text_category_count,
       CASE WHEN rr.raw_record_count>0 THEN printf('%.2f', 100.0 * COALESCE(ht.records_with_human_text,0) / rr.raw_record_count) ELSE '0.00' END AS pct_records_with_human_text,
       COALESCE(nd.native_property_count,0) AS native_property_count,
       COALESCE(nd.native_category_count,0) AS native_category_count,
       COALESCE(nd.metadata_blocks,0) AS metadata_blocks,
       COALESCE(nd.decompressed_blocks,0) AS decompressed_blocks,
       COALESCE(nd.decode_failures,0) AS decode_failures,
       COALESCE(nd.decode_status,'NO_NATIVE_DECODE_ATTEMPT_ROW') AS decode_status,
       rr.earliest_last_updated_utc,rr.latest_last_updated_utc,
       CASE WHEN COALESCE(nd.native_property_count,0)=0 THEN 'PROPERTY_DICTIONARY_NOT_DECODED_GENERIC_PROBES_ONLY'
            WHEN COALESCE(kv.recovered_key_value_count,0)=0 THEN 'NO_KEY_VALUES_RECOVERED'
            ELSE 'GENERIC_TEXT_VALUES_RECOVERED' END AS spotlight_decode_interpretation,
       'Spotlight-first coverage view. App/FFS correlation is supporting context; this row measures native CoreSpotlight record/value recovery.' AS interpretation_note
FROM rr
LEFT JOIN kv ON kv.source_id=rr.source_id AND kv.store_guid=rr.store_guid AND kv.source_db=rr.source_db
LEFT JOIN ht ON ht.source_id=rr.source_id AND ht.store_guid=rr.store_guid AND ht.source_db=rr.source_db
LEFT JOIN nd ON nd.source_id=rr.source_id AND nd.store_guid=rr.store_guid AND nd.source_db=rr.source_db;

DROP VIEW IF EXISTS vw_ios_spotlight_field_coverage_summary;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_field_coverage_summary AS
SELECT source_id,store_guid,source_db,field_name,
       COUNT(*) AS value_row_count,
       COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS distinct_record_count,
       MIN(LENGTH(COALESCE(field_value,''))) AS min_value_length,
       MAX(LENGTH(COALESCE(field_value,''))) AS max_value_length,
       substr(MIN(COALESCE(field_value,'')),1,1000) AS min_sample_value,
       substr(MAX(COALESCE(field_value,'')),1,1000) AS max_sample_value,
       CASE WHEN field_name LIKE '__native_core_probe_string_%' THEN 'GENERIC_NATIVE_STRING_PROBE'
            WHEN field_name LIKE '__native_%' THEN 'GENERIC_NATIVE_FIELD'
            ELSE 'NAMED_SPOTLIGHT_FIELD' END AS field_decode_status,
       'Field coverage summary from recovered Spotlight key/value rows. Generic probe names indicate values recovered before formal property-name mapping.' AS interpretation_note
FROM raw_key_values
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(field_value,'')<>''
GROUP BY source_id,store_guid,source_db,field_name;

DROP VIEW IF EXISTS vw_ios_spotlight_text_category_summary;
CREATE VIEW vw_ios_spotlight_text_category_summary AS
SELECT human_text_category,review_priority,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT store_guid) AS store_count,
       MIN(original_value_length) AS min_original_value_length,
       MAX(original_value_length) AS max_original_value_length,
       substr(MIN(readable_text_sample),1,1000) AS min_sample_text,
       substr(MAX(readable_text_sample),1,1000) AS max_sample_text,
       'Spotlight recovered text category summary. Categories are triage labels over Spotlight text values, not final CoreSpotlight property names.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values
GROUP BY human_text_category,review_priority;

DROP VIEW IF EXISTS vw_ios_spotlight_record_review;
CREATE VIEW vw_ios_spotlight_record_review AS
WITH text_roll AS (
  SELECT raw_record_id,text_value_count,distinct_text_category_count,human_text_categories,has_high_review_value_text,readable_text_rollup_sample
  FROM vw_ios_spotlight_human_text_rollup
), date_one AS (
  SELECT raw_record_id,
         MAX(spotlight_date_utc) AS spotlight_date_utc,
         MAX(spotlight_date_source_field) AS spotlight_date_source_field,
         MAX(spotlight_date_source_table) AS spotlight_date_source_table,
         MAX(spotlight_date_raw_value) AS spotlight_date_raw_value,
         MAX(spotlight_date_parse_method) AS spotlight_date_parse_method,
         MAX(spotlight_date_type) AS spotlight_date_type,
         MAX(spotlight_date_source_evidence) AS spotlight_date_source_evidence,
         MAX(date_validation_hint) AS date_validation_hint,
         COUNT(*) AS collapsed_date_candidate_count
  FROM vw_ios_spotlight_date_provenance
  GROUP BY raw_record_id
)
SELECT r.raw_record_id,r.source_id,r.store_guid,r.source_db,r.inode_num AS spotlight_inode_or_object_id,r.store_id AS spotlight_store_id,r.parent_inode_num,
       COALESCE(dp.spotlight_date_utc,r.last_updated_utc) AS spotlight_date_utc,
       COALESCE(dp.spotlight_date_source_field,'Last_Updated') AS spotlight_date_source_field,
       COALESCE(dp.spotlight_date_source_table,'raw_records') AS spotlight_date_source_table,
       COALESCE(dp.spotlight_date_raw_value,r.last_updated_raw) AS spotlight_date_raw_value,
       COALESCE(dp.spotlight_date_parse_method,'native_epoch_microseconds') AS spotlight_date_parse_method,
       COALESCE(dp.spotlight_date_type,'metadata_seen_or_index_updated') AS spotlight_date_type,
       COALESCE(dp.spotlight_date_source_evidence,'Last_Updated=' || COALESCE(r.last_updated_raw,'') || ' -> ' || COALESCE(r.last_updated_utc,'')) AS spotlight_date_source_evidence,
       COALESCE(dp.date_validation_hint,'Validate against raw_records.last_updated_raw/last_updated_utc for this Store-V2 record.') AS spotlight_date_validation_hint,
       COALESCE(dp.collapsed_date_candidate_count,0) AS collapsed_date_candidate_count,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       COALESCE(t.text_value_count,0) AS spotlight_text_value_count,
       COALESCE(t.distinct_text_category_count,0) AS spotlight_text_category_count,
       COALESCE(t.human_text_categories,'') AS spotlight_text_categories,
       COALESCE(t.readable_text_rollup_sample,'') AS spotlight_text_rollup_sample,
       0 AS ffs_reference_count,
       0 AS ffs_present_reference_count,
       0 AS ffs_missing_or_unresolved_reference_count,
       0 AS app_db_candidate_count,
       0 AS app_db_present_candidate_count,
       0 AS app_db_unresolved_candidate_count,
       CASE WHEN COALESCE(t.has_high_review_value_text,0)=1 THEN 'HIGH_SPOTLIGHT_TEXT_VALUE'
            WHEN COALESCE(t.text_value_count,0)>0 THEN 'SPOTLIGHT_TEXT_VALUE'
            ELSE 'SPOTLIGHT_RECORD_NO_RECOVERED_TEXT' END AS spotlight_review_priority,
       CASE WHEN COALESCE(t.text_value_count,0)>0 THEN 'TEXT_VALUES_RECOVERED_FROM_SPOTLIGHT'
            ELSE 'NO_TEXT_VALUES_RECOVERED_FOR_RECORD' END AS spotlight_decode_status,
       'Spotlight-first record review. V0_9_21 keeps GUI rows raw_record anchored and avoids broad FFS/app joins; full per-record exports are support/diagnostic-only to prevent long SQL materialization. Use Missing From FFS and object/object-summary views for residency pivots.' AS interpretation_note
FROM raw_records r
LEFT JOIN text_roll t ON t.raw_record_id=r.raw_record_id
LEFT JOIN date_one dp ON dp.raw_record_id=r.raw_record_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_summary;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_object_inode_summary AS
WITH rec AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count,
         COUNT(DISTINCT COALESCE(parent_inode_num,'')) AS distinct_parent_id_count,
         MIN(last_updated_utc) AS earliest_last_updated_utc,
         MAX(last_updated_utc) AS latest_last_updated_utc,
         MIN(raw_record_id) AS first_raw_record_id,
         MAX(raw_record_id) AS last_raw_record_id
  FROM raw_records
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), kv AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_key_value_rows,
         COUNT(DISTINCT field_name) AS distinct_spotlight_field_count,
         substr(MAX(CASE WHEN field_name='__spotlight_investigator_text_context' THEN field_value ELSE '' END),1,1800) AS spotlight_text_context_sample
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
)
SELECT rec.source_id,rec.store_guid,rec.source_db,rec.spotlight_inode_or_object_id,rec.spotlight_store_id,
       rec.raw_record_count,rec.distinct_parent_id_count,
       COALESCE(kv.raw_key_value_rows,0) AS raw_key_value_rows,
       COALESCE(kv.distinct_spotlight_field_count,0) AS distinct_spotlight_field_count,
       0 AS date_candidate_rows,
       rec.earliest_last_updated_utc,rec.latest_last_updated_utc,
       '' AS earliest_spotlight_date_utc,
       '' AS latest_spotlight_date_utc,
       rec.first_raw_record_id,rec.last_raw_record_id,
       COALESCE(kv.spotlight_text_context_sample,'') AS spotlight_text_context_sample,
       CASE WHEN rec.raw_record_count>1 THEN 'MULTIPLE_SPOTLIGHT_RECORDS_SHARE_OBJECT_ID'
            WHEN COALESCE(kv.raw_key_value_rows,0)>20 THEN 'SINGLE_RECORD_MANY_FIELDS'
            ELSE 'SINGLE_OR_LOW_EXPANSION_OBJECT' END AS object_materialization_status,
       'Object/inode-centric rollup. V0_9_21 normal exports summarize this view because the full per-object listing is support/diagnostic-only.' AS interpretation_note
FROM rec
LEFT JOIN kv ON kv.source_id=rec.source_id AND kv.store_guid=rec.store_guid AND kv.source_db=rec.source_db AND kv.spotlight_inode_or_object_id=rec.spotlight_inode_or_object_id AND kv.spotlight_store_id=rec.spotlight_store_id;

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_diagnostic_summary;
CREATE VIEW vw_ios_spotlight_object_inode_diagnostic_summary AS
WITH obj AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), buckets AS (
  SELECT source_id,store_guid,source_db,
         CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
              WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
              WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
              ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END AS object_record_bucket,
         COUNT(*) AS object_count,
         SUM(raw_record_count) AS raw_record_count,
         MIN(raw_record_count) AS min_records_per_object,
         MAX(raw_record_count) AS max_records_per_object
  FROM obj
  GROUP BY source_id,store_guid,source_db,
           CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
                WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
                WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
                ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END
)
SELECT source_id,store_guid,source_db,object_record_bucket,object_count,raw_record_count,min_records_per_object,max_records_per_object,
       'Compact object/inode materialization diagnostic. Use this normal export to decide whether the case should pivot to object-centric aggregation; full per-object rows require support/diagnostic export.' AS interpretation_note
FROM buckets;

DROP VIEW IF EXISTS vw_ios_spotlight_decode_gap_records;
CREATE VIEW vw_ios_spotlight_decode_gap_records AS
SELECT raw_record_id,source_id,store_guid,source_db,inode_num AS spotlight_inode_or_object_id,store_id AS spotlight_store_id,parent_inode_num,last_updated_utc,
       file_name,display_name,full_path,content_type,record_state,
       'NO_KEY_VALUES_OR_TEXT_PROBES_RECOVERED_FOR_SPOTLIGHT_RECORD' AS decode_gap_status,
       'This Spotlight/CoreSpotlight record was parsed at the record/header level but no key/value or human-readable text values were recovered. These rows identify the next native parser decoding target.' AS interpretation_note
FROM raw_records r
WHERE (r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%')
  AND NOT EXISTS (
    SELECT 1 FROM raw_key_values kv
    WHERE kv.source_id=r.source_id AND kv.store_guid=r.store_guid AND kv.source_db=r.source_db
      AND kv.inode_num=r.inode_num AND COALESCE(kv.store_id,'')=COALESCE(r.store_id,'')
  );

DROP VIEW IF EXISTS vw_ios_database_residency_candidates;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_database_residency_candidates AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,field_value,LOWER(field_value) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), cats AS (
  SELECT *, CASE
    WHEN v LIKE '%whatsapp%' THEN 'WHATSAPP_RELATED'
    WHEN v LIKE '%signal%' THEN 'SIGNAL_RELATED'
    WHEN v LIKE '%telegram%' THEN 'TELEGRAM_RELATED'
    WHEN v LIKE '%/sms/%' OR v LIKE '%library/sms%' OR v LIKE '%sms.db%' OR v LIKE '%imessage%' OR v LIKE '%com.apple.mobilesms%' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
    WHEN (v LIKE '%/callhistory%' OR v LIKE '%callhistory.storedata%' OR v LIKE '%facetime%' OR v LIKE 'tel:%' OR v LIKE '% tel:%')
         AND v NOT LIKE '%tel.meet%' AND v NOT LIKE '%meet.google%' THEN 'CALL_OR_FACETIME_RELATED'
    WHEN v LIKE '%.ics%' OR v LIKE '%/calendar/%' OR v LIKE '%calendar.google.com%' OR v LIKE '%text/calendar%' OR v LIKE '%vevent%' OR v LIKE '%vcalendar%' OR v LIKE '%webcal:%' OR v LIKE '%invite.ics%' THEN 'CALENDAR_OR_INVITATION_RELATED'
    WHEN v LIKE '%/library/mail/%' OR v LIKE '%attachmentdata%' OR v LIKE '%message/rfc822%' OR v LIKE '%mail.google.com%' OR v LIKE 'from:%' OR v LIKE 'to:%' OR v LIKE 'cc:%' OR v LIKE '%mailto:%' THEN 'MAIL_OR_ACCOUNT_RELATED'
    WHEN v LIKE '%addressbook%' OR v LIKE '%begin:vcard%' OR v LIKE '%vcard%' THEN 'CONTACT_OR_ADDRESS_BOOK_RELATED'
    WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' OR v LIKE '%safari%' OR v LIKE '%history%' THEN 'WEB_OR_BROWSER_RELATED'
    ELSE 'OTHER_DB_RESIDENCY_REVIEW'
  END AS object_category
  FROM probes
), db_clean AS (
  SELECT *,
         CASE
           WHEN database_category='APPLE_MESSAGES' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
           WHEN database_category='CALL_HISTORY' THEN 'CALL_OR_FACETIME_RELATED'
           WHEN database_category='WHATSAPP' THEN 'WHATSAPP_RELATED'
           WHEN database_category='SIGNAL' THEN 'SIGNAL_RELATED'
           WHEN database_category='TELEGRAM' THEN 'TELEGRAM_RELATED'
           WHEN database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT') THEN 'WEB_OR_BROWSER_RELATED'
           WHEN database_category='MAIL' THEN 'MAIL_OR_ACCOUNT_RELATED'
           WHEN database_category='CONTACTS' THEN 'CONTACT_OR_ADDRESS_BOOK_RELATED'
           WHEN database_category='CALENDAR' THEN 'CALENDAR_OR_INVITATION_RELATED'
           ELSE '' END AS app_db_object_category
  FROM ios_app_database_inventory
  WHERE COALESCE(database_category,'')<>''
    AND LOWER(COALESCE(normalized_path,'')) NOT LIKE '%-wal'
    AND LOWER(COALESCE(normalized_path,'')) NOT LIKE '%-shm'
    AND LOWER(COALESCE(database_name,'')) NOT LIKE '%-wal'
    AND LOWER(COALESCE(database_name,'')) NOT LIKE '%-shm'
), db_family AS (
  SELECT source_id,app_db_object_category AS object_category,
         COUNT(DISTINCT ios_db_id) AS matching_database_count,
         MIN(database_name) AS database_name,
         GROUP_CONCAT(DISTINCT database_category) AS database_category,
         MIN(app_hint) AS app_hint,
         MIN(normalized_path) AS candidate_database_path,
         MAX(COALESCE(record_inventory_status,'')) AS record_inventory_status
  FROM db_clean
  WHERE app_db_object_category<>''
  GROUP BY source_id,app_db_object_category
), ri_family AS (
  SELECT d.source_id,d.app_db_object_category AS object_category,
         COUNT(*) AS matching_table_count,
         MIN(ri.record_category) AS matched_record_category,
         MIN(ri.table_name) AS matched_table_name,
         MAX(COALESCE(ri.row_count,0)) AS matched_table_row_count
  FROM ios_app_database_record_inventory ri
  JOIN db_clean d ON d.ios_db_id=ri.ios_db_id
  WHERE d.app_db_object_category<>''
  GROUP BY d.source_id,d.app_db_object_category
), parsed_family AS (
  SELECT d.source_id,d.app_db_object_category AS object_category,
         COUNT(*) AS parsed_record_count,
         MIN(p.record_category) AS parsed_record_category
  FROM ios_app_parsed_records p
  JOIN db_clean d ON d.ios_db_id=p.ios_db_id
  WHERE d.app_db_object_category<>''
  GROUP BY d.source_id,d.app_db_object_category
)
SELECT c.raw_kv_id AS candidate_id,c.source_id,c.store_guid,c.source_db,c.inode_num,c.store_id,c.parent_inode_num,c.field_name,
       c.object_category,substr(c.field_value,1,2000) AS string_value_sample,
       d.database_name,d.database_category,d.app_hint,d.candidate_database_path,d.record_inventory_status,
       COALESCE(pb.parsed_record_category,ri.matched_record_category,'') AS matched_record_category,
       COALESCE(ri.matched_table_name,'') AS matched_table_name,
       COALESCE(ri.matched_table_row_count,0) AS matched_table_row_count,
       CASE WHEN COALESCE(pb.parsed_record_count,0)>0 THEN 'DATABASE_FAMILY_PARSED_RECORDS_AVAILABLE_VALUE_MATCH_NOT_PROVEN'
            WHEN COALESCE(ri.matching_table_count,0)>0 THEN 'DATABASE_FAMILY_TABLE_PRESENT_VALUE_MATCH_NOT_PROVEN'
            WHEN COALESCE(d.matching_database_count,0)>0 THEN 'DATABASE_FAMILY_PRESENT_RECORD_TABLE_NOT_PARSED'
            ELSE 'NO_KNOWN_APP_DATABASE_INVENTORY_MATCH' END AS database_residency_status,
       CASE WHEN COALESCE(pb.parsed_record_count,0)>0
            THEN 'Strict string classification matched a database family with parsed records. This is a lead only; exact Spotlight string-to-row correlation is not yet proven.'
            WHEN COALESCE(d.matching_database_count,0)>0
            THEN 'Strict string classification matched a database family. It does not prove the specific value is present in the database.'
            ELSE 'No matching database family was identified in the current app database inventory.' END AS interpretation_note
FROM cats c
LEFT JOIN db_family d ON d.source_id=c.source_id AND d.object_category=c.object_category
LEFT JOIN ri_family ri ON ri.source_id=c.source_id AND ri.object_category=c.object_category
LEFT JOIN parsed_family pb ON pb.source_id=c.source_id AND pb.object_category=c.object_category
WHERE c.object_category<>'OTHER_DB_RESIDENCY_REVIEW';

)VSGUI"
    });

    // V0_9_6: iOS investigator pivot and unified keyword-search surface views for opened older cases.
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_contact_identity_records;
CREATE VIEW vw_ios_contact_identity_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,
       CASE
         WHEN lower(table_name)='abperson' THEN 'ADDRESSBOOK_PERSON_ROW'
         WHEN lower(table_name)='contacts' THEN 'CONTACTS_CACHE_ROW'
         WHEN lower(table_name) LIKE '%fulltextsearch_content%' THEN 'CONTACT_FULLTEXT_CONTENT'
         WHEN COALESCE(contact_or_participant,'')<>'' THEN 'CONTACT_TEXT_OR_ADDRESS_VALUE'
         WHEN COALESCE(item_identifier,'')<>'' THEN 'CONTACT_IDENTIFIER_VALUE'
         ELSE 'CONTACT_REVIEW_ROW'
       END AS contact_identity_type,
       substr(trim(COALESCE(contact_or_participant,'') || ' ' || COALESCE(title,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'')),1,1200) AS identity_value_sample,
       'Parsed contact/address-book database row or contact cache row. Contact cache/FTS rows can duplicate or tokenize contact data; use database path/table/provenance before reporting.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category='CONTACTS'
  AND lower(table_name) NOT LIKE '%docsize%'
  AND lower(table_name) NOT LIKE '%segdir%'
  AND lower(table_name) NOT LIKE '%segments%'
  AND lower(table_name) NOT LIKE '%_stat%'
  AND (
       lower(table_name) IN ('abperson','contacts','abpersonfulltextsearch_content','abpersonsmartdialerfulltextsearch_content')
       OR COALESCE(contact_or_participant,'')<>'' OR COALESCE(title,'')<>'' OR COALESCE(text_snippet,'')<>'' OR COALESCE(item_identifier,'')<>''
  );

DROP VIEW IF EXISTS vw_ios_contact_identity_summary;
CREATE VIEW vw_ios_contact_identity_summary AS
SELECT database_name,database_normalized_path,table_name,contact_identity_type,parse_status,
       COUNT(*) AS contact_review_row_count,
       COUNT(DISTINCT item_identifier) AS distinct_item_identifier_count,
       SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS rows_with_contact_text,
       SUM(CASE WHEN COALESCE(title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_title,
       SUM(CASE WHEN COALESCE(text_snippet,'')<>'' THEN 1 ELSE 0 END) AS rows_with_text_snippet,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc,
       MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc
FROM vw_ios_contact_identity_records
GROUP BY database_name,database_normalized_path,table_name,contact_identity_type,parse_status
ORDER BY contact_review_row_count DESC,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_web_history_review_records;
CREATE VIEW vw_ios_web_history_review_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       url,title,item_identifier,text_snippet,parse_status,provenance,
       CASE
         WHEN lower(table_name) LIKE '%bookmark%' THEN 'BOOKMARK_OR_SAVED_WEB_ITEM'
         WHEN lower(table_name) LIKE '%history%' THEN 'WEB_HISTORY_OR_VISIT'
         ELSE 'WEB_DATABASE_RECORD'
       END AS web_record_type,
       substr(trim(COALESCE(title,'') || ' ' || COALESCE(url,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'')),1,1600) AS web_review_value_sample,
       'Parsed local web/browser database row. Timestamp interpretation depends on the source table and parser provenance.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT')
   OR lower(app_hint) IN ('safari','chrome','webkit')
   OR lower(database_name) IN ('history.db','safaritabs.db')
ORDER BY record_timestamp_utc DESC,database_name,table_name,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_web_history_review_summary;
CREATE VIEW vw_ios_web_history_review_summary AS
SELECT database_category,app_hint,database_name,table_name,web_record_type,parse_status,
       COUNT(*) AS web_review_row_count,
       SUM(CASE WHEN COALESCE(url,'')<>'' THEN 1 ELSE 0 END) AS rows_with_url,
       SUM(CASE WHEN COALESCE(title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_title,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc,
       MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc,
       MIN(database_normalized_path) AS first_database_path,MAX(database_normalized_path) AS last_database_path
FROM vw_ios_web_history_review_records
GROUP BY database_category,app_hint,database_name,table_name,web_record_type,parse_status
ORDER BY web_review_row_count DESC,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_calendar_review_records;
CREATE VIEW vw_ios_calendar_review_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,
       CASE
         WHEN lower(table_name) LIKE '%attendee%' THEN 'CALENDAR_ATTENDEE_OR_INVITEE'
         WHEN lower(table_name) LIKE '%location%' THEN 'CALENDAR_LOCATION'
         WHEN lower(table_name) LIKE '%attachment%' THEN 'CALENDAR_ATTACHMENT'
         WHEN COALESCE(contact_or_participant,'')<>'' THEN 'CALENDAR_ACCOUNT_OR_INVITEE'
         ELSE 'CALENDAR_EVENT_OR_SUPPORT_ROW'
       END AS calendar_record_type,
       substr(trim(COALESCE(title,'') || ' ' || COALESCE(contact_or_participant,'') || ' ' || COALESCE(url,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'')),1,1600) AS calendar_review_value_sample,
)SQL" R"SQL(       'Parsed calendar database row. Calendar rows may include calendars, attendees, suggestions, attachments, or event-support rows; use table/provenance before reporting.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category='CALENDAR'
  AND (COALESCE(title,'')<>'' OR COALESCE(contact_or_participant,'')<>'' OR COALESCE(url,'')<>'' OR COALESCE(text_snippet,'')<>'' OR COALESCE(item_identifier,'')<>'' OR COALESCE(record_timestamp_utc,'')<>'')
ORDER BY record_timestamp_utc DESC,database_name,table_name,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_calendar_review_summary;
CREATE VIEW vw_ios_calendar_review_summary AS
SELECT database_name,database_normalized_path,table_name,calendar_record_type,parse_status,
       COUNT(*) AS calendar_review_row_count,
       SUM(CASE WHEN COALESCE(title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_title,
       SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS rows_with_contact_or_account,
)SQL" R"SQL(       SUM(CASE WHEN COALESCE(record_timestamp_utc,'')<>'' THEN 1 ELSE 0 END) AS rows_with_timestamp,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc,
       MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc
FROM vw_ios_calendar_review_records
GROUP BY database_name,database_normalized_path,table_name,calendar_record_type,parse_status
ORDER BY calendar_review_row_count DESC,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_investigation_keyword_surface;
CREATE VIEW vw_ios_investigation_keyword_surface AS
SELECT 'CORESPOTLIGHT_TEXT' AS surface_source, source_id, CAST(raw_kv_id AS TEXT) AS source_record_id,
       human_text_category AS review_category, store_guid AS source_container, source_db AS source_location,
       field_name AS field_or_table, '' AS record_timestamp_utc, readable_text_sample AS searchable_value_sample,
       '' AS path_or_url, '' AS contact_or_identity, review_priority AS review_priority,
       'SPOTLIGHT_INDEX_VALUE' AS residency_context, interpretation_note
FROM vw_ios_spotlight_human_text_values
UNION ALL
SELECT 'APP_DATABASE_RECORD' AS surface_source, source_id, CAST(ios_app_record_id AS TEXT) AS source_record_id,
       database_category || ':' || record_category AS review_category, database_name AS source_container, database_normalized_path AS source_location,
       table_name AS field_or_table, record_timestamp_utc,
       substr(trim(COALESCE(title,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'') || ' ' || COALESCE(contact_or_participant,'') || ' ' || COALESCE(url,'') || ' ' || COALESCE(file_path,'')),1,2000) AS searchable_value_sample,
       COALESCE(NULLIF(url,''),file_path) AS path_or_url, contact_or_participant AS contact_or_identity,
       CASE WHEN database_category IN ('APPLE_MESSAGES','WHATSAPP','CALL_HISTORY') THEN 'HIGH_APP_COMMUNICATION_RECORD'
            WHEN database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT','MAIL','CALENDAR','CONTACTS') THEN 'MEDIUM_HIGH_APP_RECORD'
            ELSE 'APP_DATABASE_RECORD' END AS review_priority,
       'APP_DATABASE_RECORD_PRESENT_IN_ACQUIRED_DATABASE' AS residency_context,
       'Parsed app database value. This indicates the value came from the acquired/staged database listed, not necessarily from Spotlight.' AS interpretation_note
FROM ios_app_parsed_records
WHERE trim(COALESCE(title,'') || COALESCE(text_snippet,'') || COALESCE(item_identifier,'') || COALESCE(contact_or_participant,'') || COALESCE(url,'') || COALESCE(file_path,''))<>''
UNION ALL
SELECT 'FFS_HIGH_VALUE_PATH' AS surface_source, source_id, CAST(ios_file_id AS TEXT) AS source_record_id,
       COALESCE(NULLIF(app_container_hint,''),domain_hint) AS review_category, file_name AS source_container, normalized_path AS source_location,
       extension AS field_or_table, zip_modified_utc AS record_timestamp_utc, substr(normalized_path,1,2000) AS searchable_value_sample,
       normalized_path AS path_or_url, '' AS contact_or_identity,
       'HIGH_VALUE_FFS_PATH' AS review_priority,
       'PATH_PRESENT_IN_FFS_ZIP_INVENTORY' AS residency_context,
       'FFS path inventory row. Presence means the path was enumerated in the ZIP; absence from this filtered view does not mean absence from the ZIP.' AS interpretation_note
FROM ios_ffs_file_inventory
WHERE lower(normalized_path) LIKE '%sms%' OR lower(normalized_path) LIKE '%message%' OR lower(normalized_path) LIKE '%whatsapp%'
   OR lower(normalized_path) LIKE '%callhistory%' OR lower(normalized_path) LIKE '%facetime%' OR lower(normalized_path) LIKE '%addressbook%'
   OR lower(normalized_path) LIKE '%contacts%' OR lower(normalized_path) LIKE '%calendar%' OR lower(normalized_path) LIKE '%safari%'
   OR lower(normalized_path) LIKE '%history.db%' OR lower(normalized_path) LIKE '%mail%' OR lower(normalized_path) LIKE '%keychain%'
   OR lower(normalized_path) LIKE '%attachment%' OR lower(normalized_path) LIKE '%documents%' OR lower(normalized_path) LIKE '%downloads%'
UNION ALL
SELECT 'APP_DATABASE_INVENTORY' AS surface_source, source_id, CAST(ios_db_id AS TEXT) AS source_record_id,
       database_category AS review_category, database_name AS source_container, normalized_path AS source_location,
       app_hint AS field_or_table, zip_modified_utc AS record_timestamp_utc, substr(normalized_path,1,2000) AS searchable_value_sample,
       normalized_path AS path_or_url, '' AS contact_or_identity,
       'APP_DATABASE_DISCOVERY' AS review_priority,
       COALESCE(record_inventory_status,parse_status) AS residency_context,
       'App database inventory row. Use record inventory and parsed-record views to confirm whether rows were parsed.' AS interpretation_note
FROM ios_app_database_inventory;


)SQL");

}



void CaseDatabase::insertCaseInfo(const RunOptions& opt) {
    auto stmt = prepare("INSERT OR REPLACE INTO case_info(key,value) VALUES(?,?)");
    auto put = [&](const std::string& k, const std::string& v) { stmt.bind(1,k); stmt.bind(2,v); stmt.stepDone(); stmt.reset(); };
    put("case_name", opt.caseName);
    put("case_number", opt.caseNumber);
    put("subject_name", opt.subjectName);
    put("company", opt.company);
    put("investigator", opt.investigator);
    put("app_version", appVersion());
    put("created_utc", nowUtc());
    put("export_profile", opt.exportProfile);
    put("profile", opt.profile);
    put("skip_container_hash", opt.skipContainerHash ? "true" : "false");
    put("force_container_hash", opt.forceContainerHash ? "true" : "false");
    put("diagnostic_full_native_db", opt.diagnosticFullNativeDb ? "true" : "false");
    put("aff4_apfs_diagnostic_outputs", opt.aff4ApfsDiagnosticOutputs ? "true" : "false");
    put("materialize_ios_ffs_inventory", opt.materializeIosFfsInventory ? "true" : "false");
    put("materialize_ios_app_db_records", opt.materializeIosAppDbRecords ? "true" : "false");
    put("max_native_records", std::to_string(opt.maxNativeRecords));
    put("max_native_records_explicit", opt.maxNativeRecordsExplicit ? "true" : "false");
    put("max_native_blocks", std::to_string(opt.maxNativeBlocks));
    put("db_size_guardrail_bytes", std::to_string(opt.dbSizeGuardrailBytes));
}

void CaseDatabase::insertEvidenceSource(const EvidenceSource& source) {
    auto stmt = prepare("INSERT OR REPLACE INTO evidence_sources(source_id,profile,input_path,evidence_root,source_kind,added_utc,notes) VALUES(?,?,?,?,?,?,?)");
    stmt.bind(1, source.sourceId); stmt.bind(2, source.profile); stmt.bind(3, pathString(source.inputPath)); stmt.bind(4, pathString(source.evidenceRoot)); stmt.bind(5, source.sourceKind); stmt.bind(6, source.addedUtc); stmt.bind(7, source.notes); stmt.stepDone();
}

void CaseDatabase::insertProcessingLog(const std::string& level, const std::string& message) {
    auto stmt = prepare("INSERT INTO processing_log(created_utc,level,message) VALUES(?,?,?)");
    stmt.bind(1, nowUtc()); stmt.bind(2, level); stmt.bind(3, message); stmt.stepDone();
}

} // namespace vestigant::spotlight
