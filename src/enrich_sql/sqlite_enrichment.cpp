#include "enrich_sql/sqlite_enrichment.h"
#include "core/logger.h"
#include "core/path_utils.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <cctype>

namespace vestigant::spotlight {
namespace {
std::string sqlLiteral(const std::string& s) {
    std::string out = "'";
    for (char c : s) out += (c == '\'') ? "''" : std::string(1, c);
    out += "'";
    return out;
}
std::string stripLeadingSlash(std::string s) {
    while (!s.empty() && (s.front() == '/' || s.front() == '\\')) s.erase(s.begin());
    for (auto& c : s) if (c == '/') c = fs::path::preferred_separator;
    return s;
}

std::string trimEnrichmentAscii(std::string s) {
    while (!s.empty() && static_cast<unsigned char>(s.front()) <= 0x20) s.erase(s.begin());
    while (!s.empty() && static_cast<unsigned char>(s.back()) <= 0x20) s.pop_back();
    return s;
}
bool isPlaceholderNameForEnrichment(const std::string& s) {
    const auto t = trimEnrichmentAscii(s);
    return t.empty() || t == "------NONAME------" || t == "------PLIST------" || t == "(null)" || t == "NULL" || t == ".";
}
std::string normalizeNativeProbePath(std::string p) {
    p = trimEnrichmentAscii(std::move(p));
    if (p.rfind("file://", 0) == 0) p = p.substr(7);
    while (p.size() > 1 && (p.back() == '/' || p.back() == '\\')) p.pop_back();
    if (p.empty() || p == "/") return {};
    if (p.front() != '/' && p.front() != '\\') return {};
    for (char& ch : p) if (ch == '\\') ch = '/';
    return p;
}

std::uintmax_t safeFileSizeForEnrichment(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(p, ec) || !fs::is_regular_file(p, ec)) return 0;
    const auto n = fs::file_size(p, ec);
    return ec ? 0 : n;
}

std::string sqliteDiskSpaceSummary(CaseDatabase& db) {
    std::ostringstream out;
    const auto dbPath = db.path();
    std::error_code ec;
    const auto parent = dbPath.parent_path().empty() ? fs::current_path(ec) : dbPath.parent_path();
    const auto spaceInfo = fs::space(parent, ec);
    out << "db_path=" << dbPath.string();
    out << " db_bytes=" << safeFileSizeForEnrichment(dbPath);
    out << " wal_bytes=" << safeFileSizeForEnrichment(fs::path(dbPath.string() + "-wal"));
    out << " shm_bytes=" << safeFileSizeForEnrichment(fs::path(dbPath.string() + "-shm"));
    if (ec) {
        out << " free_space_status=UNAVAILABLE error=" << ec.message();
    } else {
        out << " available_bytes=" << static_cast<unsigned long long>(spaceInfo.available);
        out << " capacity_bytes=" << static_cast<unsigned long long>(spaceInfo.capacity);
    }
    return out.str();
}

std::string basenameFromNativeProbePath(std::string p) {
    p = normalizeNativeProbePath(std::move(p));
    if (p.empty()) return {};
    const auto pos = p.find_last_of('/');
    std::string base = (pos == std::string::npos) ? p : p.substr(pos + 1);
    base = trimEnrichmentAscii(std::move(base));
    if (base.empty() || base == "." || base == "..") return {};
    return base;
}
bool isUsefulBasenameForEnrichment(const std::string& candidate) {
    const auto s = trimEnrichmentAscii(candidate);
    if (isPlaceholderNameForEnrichment(s)) return false;
    if (s.size() < 2 || s.size() > 255) return false;
    if (s.find('/') != std::string::npos || s.find('\\') != std::string::npos) return false;
    if (s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0 || s.rfind("file://", 0) == 0) return false;
    int alnum = 0;
    int controls = 0;
    for (unsigned char c : s) {
        if (c < 0x20 || c == 0x7f) ++controls;
        if (std::isalnum(c)) ++alnum;
    }
    if (controls > 0) return false;
    if (alnum < 2) return false;
    return true;
}

bool shouldApplyNativeProbePath(const std::string& currentPath, const std::string& currentName, const std::string& candidatePath) {
    const auto path = normalizeNativeProbePath(candidatePath);
    if (path.empty()) return false;
    const auto current = normalizeNativeProbePath(currentPath);
    if (current.empty()) return true;
    if (isPlaceholderNameForEnrichment(currentName)) return true;
    const auto base = basenameFromNativeProbePath(path);
    if (!base.empty() && current == "/" + base && path.size() > current.size()) return true;
    return false;
}
std::size_t scalarCount(CaseDatabase& db, const std::string& sql, const std::string& sourceId) {
    auto q = db.prepare(sql);
    q.bind(1, sourceId);
    if (q.stepRow()) return static_cast<std::size_t>(q.colInt64(0));
    return 0;
}
std::size_t scalarCount(CaseDatabase& db, const std::string& sql) {
    auto q = db.prepare(sql);
    if (q.stepRow()) return static_cast<std::size_t>(q.colInt64(0));
    return 0;
}
long long lastSqlChangeCount(CaseDatabase& db) {
    return static_cast<long long>(sqlite3_changes(db.raw()));
}
void insertMetric(CaseDatabase& db, const std::string& sourceId, const std::string& name, const std::string& value) {
    auto st = db.prepare("INSERT INTO parser_coverage_summary(source_id,metric_name,metric_value,created_utc) VALUES(?,?,?,?)");
    st.bind(1, sourceId); st.bind(2, name); st.bind(3, value); st.bind(4, nowUtc()); st.stepDone();
}

std::string enrichmentStatusClean(std::string s) {
    for (char& ch : s) {
        if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    }
    if (s.size() > 900) s = s.substr(0, 900) + "...";
    return s;
}

void appendEnrichmentRunStatus(CaseDatabase& db, int percent, const std::string& stage, const std::string& message = {}) {
    fs::path caseDir = db.path();
    if (caseDir.has_filename()) caseDir = caseDir.parent_path();
    if (caseDir.empty()) return;
    try {
        fs::create_directories(caseDir / "logs");
        const std::string ts = nowUtc();
        const std::string cleanStage = enrichmentStatusClean(stage);
        const std::string cleanMessage = enrichmentStatusClean(message);
        auto writeProgress = [&](const fs::path& p, std::ios::openmode mode) {
            std::ofstream out(p, mode | std::ios::binary);
            out << ts << "\t" << percent << "\t" << cleanStage << "\t" << cleanMessage << "\n" << std::flush;
        };
        auto writeStatus = [&](const fs::path& p) {
            std::ofstream out(p, std::ios::app | std::ios::binary);
            out << ts << " stage=" << cleanStage;
            if (!cleanMessage.empty()) out << " message=" << cleanMessage;
            out << "\n" << std::flush;
        };
        auto writeLast = [&](const fs::path& p) {
            std::ofstream out(p, std::ios::binary);
            out << ts << " " << cleanStage << " " << cleanMessage << "\n" << std::flush;
        };
        writeStatus(caseDir / "logs" / "run_status.txt");
        writeStatus(caseDir / "run_status.txt");
        writeLast(caseDir / "logs" / "last_stage.txt");
        writeLast(caseDir / "last_stage.txt");
        writeProgress(caseDir / "logs" / "run_progress.tsv", std::ios::app);
        writeProgress(caseDir / "logs" / "last_progress.tsv", std::ios::trunc);
        writeProgress(caseDir / "run_progress.tsv", std::ios::app);
        writeProgress(caseDir / "last_progress.tsv", std::ios::trunc);
    } catch (...) {}
}

}

SqliteEnrichmentCounts SqliteEnrichment::run(CaseDatabase& db, const EvidenceSource& source, Logger& log) const {
    SqliteEnrichmentCounts counts;
    const std::string sid = sqlLiteral(source.sourceId);
    log.info("Running SQLite-native enrichment.");

    const std::size_t rawRecordCount = scalarCount(db, "SELECT COUNT(*) FROM raw_records WHERE source_id=?", source.sourceId);
    const std::size_t kvCount = scalarCount(db, "SELECT COUNT(*) FROM raw_key_values WHERE source_id=?", source.sourceId);
    const std::size_t dateCount = scalarCount(db, "SELECT COUNT(*) FROM raw_date_candidates WHERE source_id=?", source.sourceId);
    const std::size_t specialDateCount = scalarCount(db,
        "SELECT COUNT(*) FROM raw_date_candidates WHERE source_id=? AND (lower(field_name) LIKE '%downloadeddate%' OR lower(field_name) LIKE '%lastuseddate%' OR lower(field_name) LIKE '%useddates%')",
        source.sourceId);
    log.info("SQLite enrichment input counts: raw_records=" + std::to_string(rawRecordCount) + " raw_key_values=" + std::to_string(kvCount) + " raw_date_candidates=" + std::to_string(dateCount) + " special_date_candidates=" + std::to_string(specialDateCount));
    appendEnrichmentRunStatus(db, 76, "enrichment_input_counts", "raw_records=" + std::to_string(rawRecordCount) + " raw_key_values=" + std::to_string(kvCount) + " raw_date_candidates=" + std::to_string(dateCount));
    if (kvCount == 0) {
        appendEnrichmentRunStatus(db, 76, "enrichment_warning_no_metadata_key_values", "header-only or limited decode: filename/path/usage/wherefroms enrichment will be limited");
        log.warn("raw_key_values is empty. This is expected in header-only native mode, but it prevents full filename/path/usage/WhereFroms enrichment validation.");
    }

    db.begin();
    try {
        appendEnrichmentRunStatus(db, 77, "enrichment_clear_prior_rows", "source_id=" + source.sourceId);
        db.exec("DELETE FROM artifacts WHERE source_id=" + sid + ";");
        db.exec("DELETE FROM artifact_source_instances WHERE source_id=" + sid + ";");
        db.exec("DELETE FROM source_copy_comparison WHERE source_id=" + sid + ";");
        db.exec("DELETE FROM parent_inode_links WHERE source_id=" + sid + ";");
        db.exec("DELETE FROM field_inventory WHERE source_id=" + sid + ";");
        db.exec("DELETE FROM parser_coverage_summary WHERE source_id=" + sid + ";");
        db.exec("DELETE FROM usage_evidence WHERE source_id=" + sid + ";");
        db.exec("DELETE FROM timeline_events WHERE source_id=" + sid + ";");
        db.exec("DELETE FROM orphaned_deleted_candidates WHERE source_id=" + sid + ";");
        db.exec("DELETE FROM external_volume_candidates WHERE source_id=" + sid + ";");

        // Support deep recursive path reconstruction after artifacts are materialized.
        // These indexes are cheap when the tables are small and prevent recursive
        // parent-inode lookups from degenerating on large AFF4/APFS Spotlight cases.
        db.exec("CREATE INDEX IF NOT EXISTS idx_artifacts_recursive_tree ON artifacts(source_id, store_guid, inode_num, parent_inode_num);");
        db.exec("CREATE INDEX IF NOT EXISTS idx_artifacts_recursive_parent ON artifacts(source_id, store_guid, parent_inode_num);");

        log.info("Building deduplicated artifact review rows from raw source records.");
        appendEnrichmentRunStatus(db, 78, "enrichment_build_artifacts_start", "raw_records=" + std::to_string(rawRecordCount));
        db.exec(R"SQL(
WITH preferred AS (
  SELECT source_id, store_guid, inode_num,
         COALESCE(
           MIN(CASE WHEN lower(source_db) NOT LIKE '%.store.db' THEN raw_record_id END),
           MIN(raw_record_id)
         ) AS preferred_raw_record_id
  FROM raw_records
  WHERE source_id=)SQL" + sid + R"SQL(
  GROUP BY source_id, store_guid, inode_num
), normalized AS (
  SELECT r.*,
         CASE
           WHEN COALESCE(NULLIF(trim(r.file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN trim(r.file_name)
           ELSE ''
         END AS valid_file_name,
         CASE
           -- Header-only decoding commonly emits '/' as a placeholder for non-root records.
           -- Treat '/' as an actual root only where the record has no parent inode context.
           WHEN COALESCE(NULLIF(trim(r.full_path),''),'')='/'
                AND COALESCE(NULLIF(trim(r.parent_inode_num),''),'0') NOT IN ('0','') THEN ''
           ELSE COALESCE(NULLIF(trim(r.full_path),''),'')
         END AS usable_full_path
  FROM raw_records r
  JOIN preferred p ON p.preferred_raw_record_id=r.raw_record_id
)
INSERT INTO artifacts(source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,spotlight_display_path,normalized_mac_path,path_source,path_status,content_type,content_type_tree,where_froms,logical_size_bytes,physical_size_bytes,last_updated_utc,confidence)
SELECT r.source_id,
       r.store_guid,
       r.inode_num,
       r.parent_inode_num,
       CASE
         WHEN r.valid_file_name<>'' THEN r.file_name
         WHEN COALESCE(NULLIF(trim(r.file_name),''),'') IN ('','------NONAME------','------PLIST------','(null)','NULL','.') THEN 'UNRESOLVED_SPOTLIGHT_OBJECT_INODE_' || r.inode_num
         ELSE r.file_name
       END,
       CASE
         WHEN COALESCE(NULLIF(trim(r.display_name),''),'') NOT IN ('','------NONAME------','------PLIST------','(null)','NULL','.') THEN r.display_name
         WHEN r.valid_file_name<>'' THEN r.valid_file_name
         WHEN COALESCE(NULLIF(trim(r.file_name),''),'') IN ('','------NONAME------','------PLIST------','(null)','NULL','.') THEN 'Unresolved Spotlight object inode=' || r.inode_num
         ELSE r.file_name
       END,
       CASE
         WHEN r.usable_full_path<>'' THEN r.usable_full_path
         WHEN r.valid_file_name<>'' THEN r.valid_file_name
         ELSE ''
       END,
       CASE WHEN r.usable_full_path<>'' THEN r.usable_full_path ELSE '' END,
       CASE WHEN r.usable_full_path<>'' AND instr(r.usable_full_path,'/')>0 THEN r.usable_full_path ELSE '' END,
       CASE
         WHEN r.usable_full_path<>'' AND instr(r.usable_full_path,'/')>0 THEN 'RAW_RECORD_FULL_PATH'
         WHEN r.usable_full_path<>'' THEN 'RAW_RECORD_PATH_VALUE'
         WHEN r.valid_file_name<>'' THEN 'RAW_RECORD_FILE_NAME_ONLY'
         WHEN COALESCE(NULLIF(trim(r.full_path),''),'')='/' AND COALESCE(NULLIF(trim(r.parent_inode_num),''),'0') NOT IN ('0','') THEN 'HEADER_ONLY_ROOT_PLACEHOLDER_SUPPRESSED'
         ELSE 'RAW_RECORD_UNRESOLVED_IDENTIFIER_LABEL'
       END,
       CASE
         WHEN r.usable_full_path='/' THEN 'ROOT_OR_VOLUME_PATH'
         WHEN r.usable_full_path<>'' AND instr(r.usable_full_path,'/')>0 THEN 'RAW_PATH_PRESENT'
         WHEN r.usable_full_path<>'' THEN 'RAW_PATH_NO_DIRECTORY_CONTEXT'
         WHEN r.valid_file_name<>'' AND COALESCE(NULLIF(r.parent_inode_num,''),'0') NOT IN ('0','') THEN 'FILE_NAME_ONLY_PARENT_RECONSTRUCTION_PENDING'
         WHEN r.valid_file_name<>'' THEN 'FILE_NAME_ONLY_NO_PARENT_CONTEXT'
         WHEN COALESCE(NULLIF(trim(r.full_path),''),'')='/' AND COALESCE(NULLIF(trim(r.parent_inode_num),''),'0') NOT IN ('0','') THEN 'HEADER_ONLY_NO_USABLE_PATH'
         ELSE 'UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL'
       END,
       r.content_type,
       r.content_type_tree,
       r.where_froms,
)SQL" R"SQL(       r.logical_size_bytes,
       r.physical_size_bytes,
       r.last_updated_utc,
       CASE
         WHEN r.usable_full_path<>'' OR r.valid_file_name<>'' THEN 'MEDIUM'
         ELSE 'LOW_HEADER_ONLY_IDENTIFIER_LABEL'
       END
FROM normalized r;
)SQL");

        db.exec(R"SQL(
INSERT INTO artifact_source_instances(artifact_id,raw_record_id,source_id,store_guid,inode_num,source_db,source_db_role,last_updated_utc,file_name,best_path)
SELECT a.artifact_id,
       r.raw_record_id,
       r.source_id,
       r.store_guid,
       r.inode_num,
       r.source_db,
       CASE WHEN lower(r.source_db) LIKE '%.store.db' THEN 'dot_store_db' ELSE 'store_db' END,
       r.last_updated_utc,
       r.file_name,
       COALESCE(NULLIF(r.full_path,''), r.file_name)
FROM raw_records r
JOIN artifacts a ON a.source_id=r.source_id AND a.store_guid=r.store_guid AND a.inode_num=r.inode_num
WHERE r.source_id=)SQL" + sid + R"SQL(;
)SQL");

        log.info("Applying native path probe candidates to artifacts with placeholder names or weak paths.");
        appendEnrichmentRunStatus(db, 79, "enrichment_native_path_probe_apply_start", "source_id=" + source.sourceId);
        std::size_t nativePathProbeUpdated = 0;
        {
            auto q = db.prepare(R"SQL(
SELECT a.artifact_id,
       COALESCE(a.file_name,''),
       COALESCE(a.display_name,''),
       COALESCE(a.best_path,''),
       COALESCE(kv.field_value,'')
FROM artifacts a
JOIN raw_key_values kv
  ON kv.source_id=a.source_id
 AND kv.store_guid=a.store_guid
 AND kv.inode_num=a.inode_num
WHERE a.source_id=?
  AND kv.field_name LIKE '__native_probe_file_path_candidate_%'
  AND trim(COALESCE(kv.field_value,'')) LIKE '/%'
  AND trim(COALESCE(kv.field_value,''))<>'/'
ORDER BY a.artifact_id, length(kv.field_value) DESC, kv.raw_kv_id
)SQL");
            q.bind(1, source.sourceId);
            auto up = db.prepare(R"SQL(
UPDATE artifacts
SET file_name=?,
    display_name=?,
    best_path=?,
    spotlight_display_path=?,
    normalized_mac_path=?,
    path_source='NATIVE_PATH_PROBE_FULL_PATH',
    path_status='RAW_NATIVE_PATH_PROBE_PRESENT',
    confidence=CASE WHEN COALESCE(confidence,'') LIKE 'HIGH%' THEN confidence ELSE 'MEDIUM_NATIVE_PATH_PROBE_PATH' END
WHERE artifact_id=? AND source_id=?
)SQL");
            std::set<long long> appliedArtifactIds;
            while (q.stepRow()) {
                const long long artifactId = q.colInt64(0);
                if (!appliedArtifactIds.insert(artifactId).second) continue;
                const std::string currentName = q.colText(1);
                const std::string currentDisplay = q.colText(2);
                const std::string currentPath = q.colText(3);
                const std::string candidatePath = normalizeNativeProbePath(q.colText(4));
                if (!shouldApplyNativeProbePath(currentPath, currentName, candidatePath)) continue;
                const std::string candidateName = basenameFromNativeProbePath(candidatePath);
                const std::string newName = isPlaceholderNameForEnrichment(currentName) && !candidateName.empty() ? candidateName : currentName;
                const std::string newDisplay = isPlaceholderNameForEnrichment(currentDisplay) ? newName : currentDisplay;
                up.bind(1, newName);
                up.bind(2, newDisplay);
                up.bind(3, candidatePath);
                up.bind(4, candidatePath);
                up.bind(5, candidatePath);
                up.bind(6, artifactId);
                up.bind(7, source.sourceId);
                up.stepDone();
                up.reset();
                ++nativePathProbeUpdated;
            }
        }
        insertMetric(db, source.sourceId, "native_path_probe_artifacts_updated", std::to_string(nativePathProbeUpdated));
        appendEnrichmentRunStatus(db, 79, "enrichment_native_path_probe_apply_complete", "artifacts_updated=" + std::to_string(nativePathProbeUpdated));

        std::size_t nativeBasenameProbeUpdated = 0;
        {
            auto q = db.prepare(R"SQL(
SELECT a.artifact_id,
       COALESCE(a.file_name,''),
       COALESCE(a.display_name,''),
       COALESCE(kv.field_value,'')
FROM artifacts a
JOIN raw_key_values kv
  ON kv.source_id=a.source_id
 AND kv.store_guid=a.store_guid
 AND kv.inode_num=a.inode_num
WHERE a.source_id=?
  AND kv.field_name LIKE '__native_probe_basename_candidate_%'
  AND (
    COALESCE(NULLIF(trim(a.file_name),''),'') IN ('','------NONAME------','------PLIST------','(null)','NULL','.')
    OR COALESCE(NULLIF(trim(a.display_name),''),'') IN ('','------NONAME------','------PLIST------','(null)','NULL','.')
  )
ORDER BY a.artifact_id, length(kv.field_value) DESC, kv.raw_kv_id
)SQL");
            q.bind(1, source.sourceId);
            auto up = db.prepare(R"SQL(
UPDATE artifacts
SET file_name=?,
    display_name=?,
    path_status=CASE WHEN COALESCE(path_status,'')='' OR path_status='NO_USABLE_PATH' THEN 'RAW_NATIVE_BASENAME_PROBE_PRESENT' ELSE path_status END,
    confidence=CASE WHEN COALESCE(confidence,'') LIKE 'HIGH%' THEN confidence ELSE 'MEDIUM_NATIVE_BASENAME_PROBE_NAME' END
WHERE artifact_id=? AND source_id=?
)SQL");
            std::set<long long> appliedArtifactIds;
            while (q.stepRow()) {
                const long long artifactId = q.colInt64(0);
                if (!appliedArtifactIds.insert(artifactId).second) continue;
                const std::string currentName = q.colText(1);
                const std::string currentDisplay = q.colText(2);
                std::string candidate = trimEnrichmentAscii(q.colText(3));
                if (!isUsefulBasenameForEnrichment(candidate)) continue;
                const std::string newName = isPlaceholderNameForEnrichment(currentName) ? candidate : currentName;
                const std::string newDisplay = isPlaceholderNameForEnrichment(currentDisplay) ? newName : currentDisplay;
                up.bind(1, newName);
                up.bind(2, newDisplay);
                up.bind(3, artifactId);
                up.bind(4, source.sourceId);
                up.stepDone();
                up.reset();
                ++nativeBasenameProbeUpdated;
            }
        }
        insertMetric(db, source.sourceId, "native_basename_probe_artifacts_updated", std::to_string(nativeBasenameProbeUpdated));
        appendEnrichmentRunStatus(db, 79, "enrichment_native_basename_probe_apply_complete", "artifacts_updated=" + std::to_string(nativeBasenameProbeUpdated));

        db.exec(R"SQL(
INSERT INTO source_copy_comparison(source_id,store_guid,inode_num,source_instance_count,has_store_db,has_dotstore_db,comparison_status,preferred_source_db)
SELECT source_id,
       store_guid,
       inode_num,
       COUNT(*) AS source_instance_count,
       MAX(CASE WHEN lower(source_db) NOT LIKE '%.store.db' THEN 1 ELSE 0 END) AS has_store_db,
       MAX(CASE WHEN lower(source_db) LIKE '%.store.db' THEN 1 ELSE 0 END) AS has_dotstore_db,
       CASE
         WHEN MAX(CASE WHEN lower(source_db) NOT LIKE '%.store.db' THEN 1 ELSE 0 END)=1 AND MAX(CASE WHEN lower(source_db) LIKE '%.store.db' THEN 1 ELSE 0 END)=1 THEN 'MATCHED_IN_STORE_AND_DOTSTORE'
         WHEN MAX(CASE WHEN lower(source_db) NOT LIKE '%.store.db' THEN 1 ELSE 0 END)=1 THEN 'ONLY_IN_STORE_DB'
         WHEN MAX(CASE WHEN lower(source_db) LIKE '%.store.db' THEN 1 ELSE 0 END)=1 THEN 'ONLY_IN_DOTSTORE_DB'
         ELSE 'UNKNOWN_SOURCE_COPY_STATUS'
       END,
       COALESCE(MAX(CASE WHEN lower(source_db) NOT LIKE '%.store.db' THEN source_db END), MIN(source_db))
FROM raw_records
WHERE source_id=)SQL" + sid + R"SQL(
GROUP BY source_id, store_guid, inode_num;
)SQL");

        appendEnrichmentRunStatus(db, 79, "enrichment_build_artifacts_complete", "artifacts=" + std::to_string(scalarCount(db, "SELECT COUNT(*) FROM artifacts WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "unresolved_identifier_label_artifacts", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM artifacts WHERE source_id=? AND path_status='UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL'", source.sourceId)));

        // Recompute planner statistics after this source's artifacts/source-instance rows are
        // materialized and before the recursive parent-inode CTE.  Running ANALYZE before
        // the bulk insert leaves SQLite without cardinality information for the indexes that
        // dominate the deep path reconstruction step on large AFF4/APFS cases.
        appendEnrichmentRunStatus(db, 79, "enrichment_analyze_after_artifact_insert_start", "source_id=" + source.sourceId);
        db.exec("ANALYZE artifacts;");
        db.exec("ANALYZE artifact_source_instances;");
        db.exec("ANALYZE raw_records;");
        appendEnrichmentRunStatus(db, 79, "enrichment_analyze_after_artifact_insert_complete", "source_id=" + source.sourceId);

        log.info("Building parent inode relationship links and same-folder group counts.");
        appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_links_start", "source_id=" + source.sourceId);
        db.exec(R"SQL(
DROP TABLE IF EXISTS temp_artifact_inode_join_nodes;
CREATE TEMP TABLE temp_artifact_inode_join_nodes AS
SELECT artifact_id,
       source_id,
       store_guid,
       inode_num,
       parent_inode_num,
       CAST(inode_num AS INTEGER) AS inode_key,
       CASE
         WHEN COALESCE(NULLIF(parent_inode_num,''),'0') IN ('0','') THEN NULL
         ELSE CAST(parent_inode_num AS INTEGER)
       END AS parent_inode_key,
       file_name,
       display_name,
       best_path,
       path_status
FROM artifacts
WHERE source_id=)SQL" + sid + R"SQL(;

CREATE INDEX IF NOT EXISTS temp_idx_artifact_inode_join_key ON temp_artifact_inode_join_nodes(source_id, store_guid, inode_key);
CREATE INDEX IF NOT EXISTS temp_idx_artifact_inode_join_parent ON temp_artifact_inode_join_nodes(source_id, store_guid, parent_inode_key);
)SQL");

        appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_join_temp_ready", "source_id=" + source.sourceId);

        db.exec(R"SQL(
WITH sibling_counts AS (
  SELECT source_id, store_guid, parent_inode_key, COUNT(*) AS sibling_count
  FROM temp_artifact_inode_join_nodes
  WHERE parent_inode_key IS NOT NULL AND parent_inode_key<>0
  GROUP BY source_id, store_guid, parent_inode_key
), child_parent AS (
  SELECT c.artifact_id AS child_artifact_id,
         c.source_id,
         c.store_guid,
         c.inode_num AS child_inode_num,
         c.parent_inode_num AS child_parent_inode_num,
         c.file_name AS child_file_name,
         c.best_path AS child_best_path,
         p.artifact_id AS parent_artifact_id,
         p.inode_num AS parent_inode_num,
         p.file_name AS parent_file_name,
         p.best_path AS parent_best_path,
         COALESCE(sc.sibling_count, 0) AS sibling_count,
         CASE
           WHEN COALESCE(NULLIF(trim(c.file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.')
                AND trim(c.file_name) NOT LIKE 'UNRESOLVED_SPOTLIGHT_OBJECT_INODE_%'
                AND trim(c.file_name) NOT LIKE 'Unresolved Spotlight object inode=%' THEN trim(c.file_name)
           ELSE ''
         END AS child_valid_name,
         CASE
           WHEN COALESCE(NULLIF(trim(p.file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.')
                AND trim(p.file_name) NOT LIKE 'UNRESOLVED_SPOTLIGHT_OBJECT_INODE_%'
                AND trim(p.file_name) NOT LIKE 'Unresolved Spotlight object inode=%' THEN trim(p.file_name)
           ELSE ''
         END AS parent_valid_name,
         CASE
           WHEN COALESCE(NULLIF(trim(p.best_path),''),'') <> ''
                AND instr(trim(p.best_path),'UNRESOLVED_SPOTLIGHT_OBJECT_INODE_')=0
                AND instr(trim(p.best_path),'Unresolved Spotlight object inode=')=0 THEN trim(p.best_path)
           ELSE ''
         END AS parent_valid_path
  FROM temp_artifact_inode_join_nodes c
  LEFT JOIN temp_artifact_inode_join_nodes p
    ON p.source_id=c.source_id
   AND p.store_guid=c.store_guid
   AND p.inode_key=c.parent_inode_key
  LEFT JOIN sibling_counts sc
    ON sc.source_id=c.source_id
   AND sc.store_guid=c.store_guid
   AND sc.parent_inode_key=c.parent_inode_key
  WHERE c.parent_inode_key IS NOT NULL
    AND c.parent_inode_key<>0
)
INSERT INTO parent_inode_links(source_id,store_guid,child_artifact_id,child_inode_num,child_parent_inode_num,child_file_name,child_best_path,parent_artifact_id,parent_inode_num,parent_file_name,parent_best_path,sibling_group_key,sibling_count,relationship_status,path_reconstruction_method,reconstructed_path_candidate,confidence)
SELECT source_id,
       store_guid,
       child_artifact_id,
       child_inode_num,
       child_parent_inode_num,
       child_file_name,
       child_best_path,
       parent_artifact_id,
       parent_inode_num,
       parent_file_name,
       parent_best_path,
       source_id || '|' || store_guid || '|' || child_parent_inode_num AS sibling_group_key,
       sibling_count,
       CASE WHEN parent_artifact_id IS NOT NULL THEN 'PARENT_INODE_MATCHED_IN_SAME_STORE' ELSE 'PARENT_INODE_NOT_RESOLVED_IN_SAME_STORE' END AS relationship_status,
       CASE
         WHEN parent_artifact_id IS NOT NULL
              AND parent_valid_path <> ''
              AND instr(parent_valid_path,'/') > 0
              AND child_valid_name <> '' THEN 'PARENT_PATH_PLUS_CHILD_NAME'
         WHEN parent_artifact_id IS NOT NULL
              AND parent_valid_name <> ''
              AND child_valid_name <> '' THEN 'PARENT_NAME_PLUS_CHILD_NAME_ONLY'
         WHEN parent_artifact_id IS NOT NULL
              AND child_valid_name = '' THEN 'PARENT_INODE_MATCH_NO_CHILD_NAME'
         WHEN sibling_count > 1 THEN 'SAME_PARENT_INODE_GROUP_ONLY'
         ELSE 'PARENT_INODE_ONLY'
       END AS path_reconstruction_method,
       CASE
         WHEN parent_artifact_id IS NOT NULL
              AND parent_valid_path <> ''
              AND instr(parent_valid_path,'/') > 0
              AND child_valid_name <> '' THEN rtrim(parent_valid_path,'/') || '/' || child_valid_name
         WHEN parent_artifact_id IS NOT NULL
              AND parent_valid_name <> ''
              AND child_valid_name <> '' THEN parent_valid_name || '/' || child_valid_name
         ELSE ''
       END AS reconstructed_path_candidate,
       CASE
         WHEN parent_artifact_id IS NOT NULL AND parent_valid_path <> '' AND instr(parent_valid_path,'/') > 0 AND child_valid_name <> '' THEN 'MEDIUM_PARENT_PATH_DERIVED_FROM_SPOTLIGHT'
         WHEN parent_artifact_id IS NOT NULL AND child_valid_name = '' THEN 'LOW_PARENT_INODE_MATCH_NO_CHILD_NAME'
         WHEN parent_artifact_id IS NOT NULL THEN 'LOW_PARENT_INODE_MATCH_NO_FULL_PATH'
         WHEN sibling_count > 1 THEN 'LOW_SAME_PARENT_INODE_GROUP'
         ELSE 'LOW_PARENT_INODE_UNRESOLVED'
       END AS confidence
FROM child_parent;
)SQL");

        appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_base_links_complete", "links=" + std::to_string(scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=?", source.sourceId)));
        appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_chain_start", "source_id=" + source.sourceId);

        // V1.6.12: the native Store-V2 metadata stream can enumerate children before parents,
        // which means parser-time path reconstruction may miss otherwise valid parent chains.
        // Rebuild candidate paths after all artifacts are materialized, using all observed
        // parent_inode_num edges.  Relative chains are retained as review-only evidence when
        // no absolute ancestor path is available; absolute/raw paths remain preferred.
        db.exec(R"SQL(
DROP TABLE IF EXISTS temp_parent_inode_nodes;
DROP TABLE IF EXISTS temp_parent_inode_path_candidates;
DROP TABLE IF EXISTS temp_best_parent_inode_path;
CREATE TEMP TABLE temp_parent_inode_nodes AS
SELECT artifact_id,
       source_id,
       store_guid,
       inode_num,
       parent_inode_num,
       inode_key,
       parent_inode_key,
       CASE
         WHEN COALESCE(NULLIF(trim(file_name),''),'') NOT IN ('------NONAME------','------PLIST------','(null)','NULL','.')
              AND trim(file_name) NOT LIKE 'UNRESOLVED_SPOTLIGHT_OBJECT_INODE_%'
              AND trim(file_name) NOT LIKE 'Unresolved Spotlight object inode=%' THEN trim(file_name)
         WHEN COALESCE(NULLIF(trim(display_name),''),'') NOT IN ('------NONAME------','------PLIST------','(null)','NULL','.')
              AND trim(display_name) NOT LIKE 'UNRESOLVED_SPOTLIGHT_OBJECT_INODE_%'
              AND trim(display_name) NOT LIKE 'Unresolved Spotlight object inode=%' THEN trim(display_name)
         ELSE ''
       END AS valid_name,
       CASE
         WHEN trim(COALESCE(best_path,''))='/' AND (parent_inode_key IS NULL OR parent_inode_key=0) THEN '/'
         WHEN COALESCE(NULLIF(trim(best_path),''),'') NOT IN ('/','------NONAME------','------PLIST------','(null)','NULL','.')
              AND instr(trim(best_path),'UNRESOLVED_SPOTLIGHT_OBJECT_INODE_')=0
              AND instr(trim(best_path),'Unresolved Spotlight object inode=')=0 THEN trim(best_path)
         ELSE ''
       END AS existing_path,
       path_status
FROM temp_artifact_inode_join_nodes;

CREATE INDEX IF NOT EXISTS temp_idx_parent_inode_nodes_key ON temp_parent_inode_nodes(source_id, store_guid, inode_key);
CREATE INDEX IF NOT EXISTS temp_idx_parent_inode_nodes_parent ON temp_parent_inode_nodes(source_id, store_guid, parent_inode_key);
)SQL");

        const std::size_t parentChainSeedRows = scalarCount(db, R"SQL(
SELECT COUNT(*)
FROM temp_parent_inode_nodes
WHERE (existing_path<>'' AND instr(existing_path,'/')>0)
   OR (valid_name<>'' AND (parent_inode_key IS NULL OR parent_inode_key=0 OR parent_inode_key=2))
)SQL");
        appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_chain_seed_ready", "source_id=" + source.sourceId + " seed_rows=" + std::to_string(parentChainSeedRows) + " child_absolute_existing_paths_are_not_recursed=1");

        const std::size_t directParentPathContextRows = scalarCount(db, R"SQL(
SELECT COUNT(*)
FROM parent_inode_links
WHERE source_id=?
  AND COALESCE(reconstructed_path_candidate,'')<>''
)SQL", source.sourceId);
        const std::size_t directParentUnresolvedRows = scalarCount(db, R"SQL(
SELECT COUNT(*)
FROM parent_inode_links
WHERE source_id=?
  AND (COALESCE(reconstructed_path_candidate,'')='' OR COALESCE(reconstructed_path_candidate,'') NOT LIKE '/%')
)SQL", source.sourceId);
        const std::size_t parentChainActionableWeakArtifactRows = scalarCount(db, R"SQL(
SELECT COUNT(DISTINCT a.artifact_id)
FROM artifacts a
JOIN parent_inode_links pl
  ON pl.source_id=a.source_id
 AND pl.child_artifact_id=a.artifact_id
WHERE a.source_id=?
  AND COALESCE(NULLIF(pl.child_file_name,''),'')<>''
  AND (
    COALESCE(NULLIF(a.best_path,''),'')=''
    OR COALESCE(a.best_path,'')=COALESCE(a.file_name,'')
    OR instr(COALESCE(a.best_path,''),'/')=0
    OR COALESCE(a.path_status,'') IN ('','RAW_PATH_NO_DIRECTORY_CONTEXT','FILE_NAME_ONLY_PARENT_RECONSTRUCTION_PENDING','FILE_NAME_ONLY_NO_PARENT_CONTEXT','NO_USABLE_PATH')
  )
)SQL", source.sourceId);
        appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_chain_fast_path_evaluation",
            "source_id=" + source.sourceId +
            " direct_path_context_rows=" + std::to_string(directParentPathContextRows) +
            " unresolved_or_relative_links=" + std::to_string(directParentUnresolvedRows) +
            " actionable_weak_artifact_rows=" + std::to_string(parentChainActionableWeakArtifactRows));

        // V1.6.101: skip the expensive recursive parent-inode chain when it cannot
        // improve investigator-facing artifact paths.  V1.6.97 proved that this
        // AFF4/APFS case spent ~14.5 minutes in the recursive review query, then
        // parent-inode path apply updated zero artifacts.  Direct parent-inode links
        // remain preserved for validation; raw Store-V2/APFS evidence remains the
        // authority.  The recursive query now runs only when weak/missing artifact
        // paths are numerous enough to justify recursion or when direct parent context is too sparse to support the
        // fast path. A small number of weak targets is deferred to validation exports instead of spending minutes in recursion.
        const bool noWeakArtifactPathTargets = (parentChainActionableWeakArtifactRows == 0);
        const bool negligibleWeakArtifactPathTargets = (parentChainActionableWeakArtifactRows <= 25 && directParentPathContextRows >= 50000);
        const bool directContextCoversMostLinks = (directParentPathContextRows >= 50000 && directParentUnresolvedRows <= 25000);
        const bool useFastParentChain = noWeakArtifactPathTargets || negligibleWeakArtifactPathTargets || directContextCoversMostLinks;
        const std::string fastPathReason = noWeakArtifactPathTargets ? "no_weak_artifact_path_targets" :
            (negligibleWeakArtifactPathTargets ? "negligible_weak_artifact_path_targets" : "direct_parent_context_fast_path");
        if (useFastParentChain) {
            db.exec(R"SQL(
DROP TABLE IF EXISTS temp_parent_inode_path_candidates;
DROP TABLE IF EXISTS temp_best_parent_inode_path;
CREATE TEMP TABLE temp_parent_inode_path_candidates(
  artifact_id INTEGER,source_id TEXT,store_guid TEXT,inode_num INTEGER,parent_inode_num INTEGER,
  candidate_path TEXT,method TEXT,confidence TEXT,quality INTEGER,depth INTEGER,candidate_len INTEGER
);
CREATE TEMP TABLE temp_best_parent_inode_path AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,candidate_path,method,confidence,quality,depth
FROM temp_parent_inode_path_candidates
WHERE 0;
)SQL");
            appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_chain_candidates_ready",
                "source_id=" + source.sourceId + " candidate_rows=0 skipped=1 reason=" + fastPathReason +
                " direct_path_context_rows=" + std::to_string(directParentPathContextRows) +
                " actionable_weak_artifact_rows=" + std::to_string(parentChainActionableWeakArtifactRows));
            appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_chain_complete",
                "source_id=" + source.sourceId + " skipped=1 reason=" + fastPathReason);
        } else {
            appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_chain_recursive_query_start",
                "source_id=" + source.sourceId + " seed_rows=" + std::to_string(parentChainSeedRows) +
                " direct_path_context_rows=" + std::to_string(directParentPathContextRows) +
                " unresolved_or_relative_links=" + std::to_string(directParentUnresolvedRows) +
                " actionable_weak_artifact_rows=" + std::to_string(parentChainActionableWeakArtifactRows));

        db.exec(R"SQL(
CREATE TEMP TABLE temp_parent_inode_path_candidates AS
WITH RECURSIVE path_chain(artifact_id,source_id,store_guid,inode_num,parent_inode_num,inode_key,parent_inode_key,candidate_path,method,confidence,quality,depth,visited) AS (
  SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,inode_key,parent_inode_key,
         CASE WHEN existing_path='/' THEN '/' ELSE rtrim(existing_path,'/') END AS candidate_path,
         'EXISTING_SPOTLIGHT_PATH_CHAIN_SEED' AS method,
         'MEDIUM_EXISTING_SPOTLIGHT_PATH_CONTEXT' AS confidence,
         0 AS quality,
         0 AS depth,
         '|' || inode_key || '|' AS visited
  FROM temp_parent_inode_nodes
  WHERE existing_path<>'' AND instr(existing_path,'/')>0

  UNION ALL

  SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,inode_key,parent_inode_key,
         '/' || valid_name AS candidate_path,
         'ROOT_NAME_CHAIN_SEED' AS method,
         'LOW_ROOT_NAME_CHAIN_CONTEXT' AS confidence,
         1 AS quality,
         0 AS depth,
         '|' || inode_key || '|' AS visited
  FROM temp_parent_inode_nodes
  WHERE valid_name<>''
    AND (parent_inode_key IS NULL OR parent_inode_key=0 OR parent_inode_key=2)

  UNION ALL

  SELECT child.artifact_id,
         child.source_id,
         child.store_guid,
         child.inode_num,
         child.parent_inode_num,
         child.inode_key,
         child.parent_inode_key,
         CASE
           WHEN pc.candidate_path='/' THEN '/' || child.valid_name
           ELSE pc.candidate_path || '/' || child.valid_name
         END AS candidate_path,
         CASE
           WHEN pc.candidate_path LIKE '/%' THEN 'ANCESTOR_PATH_PLUS_CHILD_CHAIN'
           ELSE 'RELATIVE_PARENT_INODE_NAME_CHAIN'
         END AS method,
         CASE
           WHEN pc.candidate_path LIKE '/%' THEN 'MEDIUM_PARENT_INODE_CHAIN_WITH_ABSOLUTE_ANCESTOR'
           ELSE 'LOW_RELATIVE_PARENT_INODE_CHAIN_REVIEW'
         END AS confidence,
         CASE WHEN pc.candidate_path LIKE '/%' THEN 1 ELSE 3 END AS quality,
         pc.depth + 1 AS depth,
         pc.visited || child.inode_key || '|'
  FROM temp_parent_inode_nodes child
  JOIN path_chain pc
    ON pc.source_id=child.source_id
   AND pc.store_guid=child.store_guid
   AND child.parent_inode_key=pc.inode_key
  WHERE child.valid_name<>''
    AND (child.existing_path='' OR child.existing_path NOT LIKE '/%')
    AND pc.depth < 20
    AND instr(pc.visited, '|' || child.inode_key || '|')=0
)
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,candidate_path,method,confidence,quality,depth,
       length(candidate_path) AS candidate_len
FROM path_chain
WHERE COALESCE(candidate_path,'')<>''
  AND instr(candidate_path,'/')>0
  AND instr(candidate_path,'UNRESOLVED_SPOTLIGHT_OBJECT_INODE_')=0
  AND instr(candidate_path,'Unresolved Spotlight object inode=')=0;

CREATE TEMP TABLE temp_best_parent_inode_path AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,candidate_path,method,confidence,quality,depth
FROM (
  SELECT p.*,
         ROW_NUMBER() OVER (
           PARTITION BY p.artifact_id
           ORDER BY p.quality ASC,
                    CASE WHEN p.candidate_path LIKE '/%' THEN 0 ELSE 1 END ASC,
                    p.depth DESC,
                    length(p.candidate_path) DESC,
                    p.candidate_path ASC
         ) AS rn
  FROM temp_parent_inode_path_candidates p
) ranked
WHERE rn=1;
)SQL");

        const std::size_t parentChainCandidateRows = scalarCount(db, "SELECT COUNT(*) FROM temp_parent_inode_path_candidates");
        appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_chain_candidates_ready", "source_id=" + source.sourceId + " candidate_rows=" + std::to_string(parentChainCandidateRows));

        db.exec(R"SQL(
UPDATE parent_inode_links
SET reconstructed_path_candidate = CASE
      WHEN COALESCE(NULLIF(reconstructed_path_candidate,''),'')=''
           OR (COALESCE(reconstructed_path_candidate,'') NOT LIKE '/%' AND EXISTS (
             SELECT 1 FROM temp_best_parent_inode_path b
             WHERE b.artifact_id=parent_inode_links.child_artifact_id
               AND b.source_id=parent_inode_links.source_id
               AND b.store_guid=parent_inode_links.store_guid
               AND b.candidate_path LIKE '/%'
           )) THEN COALESCE((
             SELECT b.candidate_path
             FROM temp_best_parent_inode_path b
             WHERE b.artifact_id=parent_inode_links.child_artifact_id
               AND b.source_id=parent_inode_links.source_id
               AND b.store_guid=parent_inode_links.store_guid
             LIMIT 1
           ), reconstructed_path_candidate)
      ELSE reconstructed_path_candidate
    END,
    path_reconstruction_method = CASE
      WHEN COALESCE(NULLIF(reconstructed_path_candidate,''),'')=''
           OR (COALESCE(reconstructed_path_candidate,'') NOT LIKE '/%' AND EXISTS (
             SELECT 1 FROM temp_best_parent_inode_path b
             WHERE b.artifact_id=parent_inode_links.child_artifact_id
               AND b.source_id=parent_inode_links.source_id
               AND b.store_guid=parent_inode_links.store_guid
               AND b.candidate_path LIKE '/%'
           )) THEN COALESCE((
             SELECT b.method
             FROM temp_best_parent_inode_path b
             WHERE b.artifact_id=parent_inode_links.child_artifact_id
               AND b.source_id=parent_inode_links.source_id
               AND b.store_guid=parent_inode_links.store_guid
             LIMIT 1
           ), path_reconstruction_method)
      ELSE path_reconstruction_method
    END,
    confidence = CASE
      WHEN COALESCE(NULLIF(reconstructed_path_candidate,''),'')=''
           OR (COALESCE(reconstructed_path_candidate,'') NOT LIKE '/%' AND EXISTS (
             SELECT 1 FROM temp_best_parent_inode_path b
             WHERE b.artifact_id=parent_inode_links.child_artifact_id
               AND b.source_id=parent_inode_links.source_id
               AND b.store_guid=parent_inode_links.store_guid
               AND b.candidate_path LIKE '/%'
           )) THEN COALESCE((
             SELECT b.confidence
             FROM temp_best_parent_inode_path b
             WHERE b.artifact_id=parent_inode_links.child_artifact_id
               AND b.source_id=parent_inode_links.source_id
               AND b.store_guid=parent_inode_links.store_guid
             LIMIT 1
           ), confidence)
      ELSE confidence
    END
WHERE source_id=)SQL" + sid + R"SQL(
  AND EXISTS (
    SELECT 1
    FROM temp_best_parent_inode_path b
    WHERE b.artifact_id=parent_inode_links.child_artifact_id
      AND b.source_id=parent_inode_links.source_id
      AND b.store_guid=parent_inode_links.store_guid
  )
  AND (
    COALESCE(NULLIF(reconstructed_path_candidate,''),'')=''
    OR (COALESCE(reconstructed_path_candidate,'') NOT LIKE '/%' AND EXISTS (
      SELECT 1 FROM temp_best_parent_inode_path b
      WHERE b.artifact_id=parent_inode_links.child_artifact_id
        AND b.source_id=parent_inode_links.source_id
        AND b.store_guid=parent_inode_links.store_guid
        AND b.candidate_path LIKE '/%'
    ))
  );
)SQL");

        appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_chain_complete", "source_id=" + source.sourceId);
        }

        const std::size_t parentLinkRows = scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=?", source.sourceId);
        const std::size_t pathContextRows = scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND COALESCE(reconstructed_path_candidate,'')<>''", source.sourceId);
        const std::size_t existingPathContextRows = scalarCount(db, R"SQL(
SELECT COUNT(*)
FROM parent_inode_links pl
LEFT JOIN artifacts a ON a.artifact_id=pl.child_artifact_id
WHERE pl.source_id=?
  AND COALESCE(pl.reconstructed_path_candidate,'')<>''
  AND COALESCE(a.best_path,'')=COALESCE(pl.reconstructed_path_candidate,'')
  AND COALESCE(a.path_source,'') NOT IN ('PARENT_INODE_RECONSTRUCTION','PARENT_INODE_CHAIN_RECONSTRUCTION','PARENT_INODE_RELATIVE_CHAIN_REVIEW')
)SQL", source.sourceId);
        const std::size_t newReconstructedPathRows = scalarCount(db, R"SQL(
SELECT COUNT(*)
FROM parent_inode_links pl
LEFT JOIN artifacts a ON a.artifact_id=pl.child_artifact_id
WHERE pl.source_id=?
  AND COALESCE(pl.reconstructed_path_candidate,'')<>''
  AND (COALESCE(a.best_path,'')<>COALESCE(pl.reconstructed_path_candidate,'') OR COALESCE(a.path_source,'') IN ('PARENT_INODE_RECONSTRUCTION','PARENT_INODE_CHAIN_RECONSTRUCTION','PARENT_INODE_RELATIVE_CHAIN_REVIEW'))
)SQL", source.sourceId);
        const std::size_t parentMatchedRows = scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE'", source.sourceId);
        const std::size_t childNameRows = scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.')", source.sourceId);
        const std::size_t missingChildNameRows = scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND path_reconstruction_method='PARENT_INODE_MATCH_NO_CHILD_NAME'", source.sourceId);
        appendEnrichmentRunStatus(db, 81, "enrichment_parent_inode_links_complete", "links=" + std::to_string(parentLinkRows) + " matched=" + std::to_string(parentMatchedRows) + " child_names=" + std::to_string(childNameRows) + " missing_child_names=" + std::to_string(missingChildNameRows) + " path_context_candidates=" + std::to_string(pathContextRows) + " existing_path_context=" + std::to_string(existingPathContextRows) + " new_reconstructed_paths=" + std::to_string(newReconstructedPathRows));

        std::size_t appliedPathRows = 0;
        if (newReconstructedPathRows == 0) {
            log.info("Skipping parent-inode reconstructed path apply because no new reconstructed paths require artifact updates.");
            appendEnrichmentRunStatus(db, 82, "enrichment_parent_inode_path_apply_skipped", "source_id=" + source.sourceId + " reason=no_new_reconstructed_paths");
            appendEnrichmentRunStatus(db, 83, "enrichment_parent_inode_path_apply_complete", "artifacts_updated=0 skipped=1 reason=no_new_reconstructed_paths");
        } else {
            log.info("Applying parent-inode reconstructed paths to weak artifact path rows.");
            appendEnrichmentRunStatus(db, 82, "enrichment_parent_inode_path_apply_start", "source_id=" + source.sourceId + " candidates=" + std::to_string(newReconstructedPathRows));
            db.exec(R"SQL(
UPDATE artifacts
SET best_path = COALESCE((
        SELECT NULLIF(pl.reconstructed_path_candidate,'')
        FROM parent_inode_links pl
        WHERE pl.source_id=artifacts.source_id
          AND pl.child_artifact_id=artifacts.artifact_id
          AND COALESCE(pl.reconstructed_path_candidate,'')<>''
        ORDER BY CASE WHEN pl.path_reconstruction_method='PARENT_PATH_PLUS_CHILD_NAME' THEN 0 ELSE 1 END, pl.link_id
        LIMIT 1
    ), best_path),
    normalized_mac_path = COALESCE((
        SELECT NULLIF(pl.reconstructed_path_candidate,'')
        FROM parent_inode_links pl
        WHERE pl.source_id=artifacts.source_id
          AND pl.child_artifact_id=artifacts.artifact_id
          AND COALESCE(pl.reconstructed_path_candidate,'')<>''
        ORDER BY CASE WHEN pl.path_reconstruction_method='PARENT_PATH_PLUS_CHILD_NAME' THEN 0 ELSE 1 END, pl.link_id
        LIMIT 1
    ), normalized_mac_path),
    path_source = CASE
      WHEN EXISTS (SELECT 1 FROM temp_best_parent_inode_path b WHERE b.artifact_id=artifacts.artifact_id AND b.candidate_path LIKE '/%') THEN 'PARENT_INODE_CHAIN_RECONSTRUCTION'
      ELSE 'PARENT_INODE_RELATIVE_CHAIN_REVIEW'
    END,
    path_status = CASE
      WHEN EXISTS (SELECT 1 FROM temp_best_parent_inode_path b WHERE b.artifact_id=artifacts.artifact_id AND b.candidate_path LIKE '/%') THEN 'RECONSTRUCTED_FROM_PARENT_INODE_CHAIN'
      ELSE 'RECONSTRUCTED_RELATIVE_PARENT_INODE_CHAIN_REVIEW'
    END,
    confidence = CASE
      WHEN COALESCE(confidence,'') LIKE 'HIGH%' THEN confidence
      ELSE CASE
        WHEN EXISTS (SELECT 1 FROM temp_best_parent_inode_path b WHERE b.artifact_id=artifacts.artifact_id AND b.candidate_path LIKE '/%') THEN 'MEDIUM_PARENT_INODE_RECONSTRUCTED_PATH'
        ELSE 'LOW_RELATIVE_PARENT_INODE_CHAIN_REVIEW'
      END
    END
WHERE source_id=)SQL" + sid + R"SQL(
  AND EXISTS (
    SELECT 1
    FROM parent_inode_links pl
    WHERE pl.source_id=artifacts.source_id
      AND pl.child_artifact_id=artifacts.artifact_id
      AND COALESCE(pl.reconstructed_path_candidate,'')<>''
  )
  AND (
    COALESCE(NULLIF(best_path,''),'')=''
    OR COALESCE(best_path,'')=COALESCE(file_name,'')
    OR instr(COALESCE(best_path,''),'/')=0
    OR COALESCE(path_status,'') IN ('','RAW_PATH_NO_DIRECTORY_CONTEXT','FILE_NAME_ONLY_PARENT_RECONSTRUCTION_PENDING','FILE_NAME_ONLY_NO_PARENT_CONTEXT','NO_USABLE_PATH')
  );
)SQL");
            appliedPathRows = scalarCount(db, "SELECT COUNT(*) FROM artifacts WHERE source_id=? AND path_source IN ('PARENT_INODE_RECONSTRUCTION','PARENT_INODE_CHAIN_RECONSTRUCTION','PARENT_INODE_RELATIVE_CHAIN_REVIEW')", source.sourceId);
            appendEnrichmentRunStatus(db, 83, "enrichment_parent_inode_path_apply_complete", "artifacts_updated=" + std::to_string(appliedPathRows));
        }
        db.exec("DROP TABLE IF EXISTS temp_best_parent_inode_path;");
        db.exec("DROP TABLE IF EXISTS temp_parent_inode_path_candidates;");
        db.exec("DROP TABLE IF EXISTS temp_parent_inode_nodes;");
        db.exec("DROP TABLE IF EXISTS temp_artifact_inode_join_nodes;");

        if (kvCount > 0) {
            log.info("Applying key/value enrichment updates.");
            db.exec(R"SQL(
INSERT INTO field_inventory(source_id,field_name,row_count,populated_count,sample_value)
SELECT source_id, field_name, COUNT(*), SUM(CASE WHEN COALESCE(field_value,'')<>'' THEN 1 ELSE 0 END), substr(MAX(field_value),1,500)
FROM raw_key_values
WHERE source_id=)SQL" + sid + R"SQL(
GROUP BY source_id, field_name;
)SQL");

            db.exec(R"SQL(
UPDATE artifacts
SET authors = (SELECT group_concat(field_value, '; ') FROM raw_key_values kv WHERE kv.source_id=artifacts.source_id AND kv.store_guid=artifacts.store_guid AND kv.inode_num=artifacts.inode_num AND kv.field_name IN ('kMDItemAuthors','authors')),
    creator = (SELECT group_concat(field_value, '; ') FROM raw_key_values kv WHERE kv.source_id=artifacts.source_id AND kv.store_guid=artifacts.store_guid AND kv.inode_num=artifacts.inode_num AND kv.field_name IN ('kMDItemCreator','creator')),
    used_dates_utc = (SELECT group_concat(COALESCE(NULLIF(d.parsed_utc,''), d.field_value), '; ') FROM raw_date_candidates d WHERE d.source_id=artifacts.source_id AND d.store_guid=artifacts.store_guid AND d.inode_num=artifacts.inode_num AND lower(d.field_name) LIKE '%useddates%' AND COALESCE(NULLIF(d.parsed_utc,''), d.field_value)<>''),
    where_froms = COALESCE(NULLIF(where_froms,''), (SELECT group_concat(field_value, '; ') FROM raw_key_values kv WHERE kv.source_id=artifacts.source_id AND kv.store_guid=artifacts.store_guid AND kv.inode_num=artifacts.inode_num AND kv.field_name='kMDItemWhereFroms')),
    index_text_snippet = (SELECT substr(field_value,1,500) FROM raw_key_values kv WHERE kv.source_id=artifacts.source_id AND kv.store_guid=artifacts.store_guid AND kv.inode_num=artifacts.inode_num AND lower(kv.field_name) IN ('kmditemtextcontent','kmditemdescription','kmditemdisplayname') AND length(kv.field_value)>0 LIMIT 1)
WHERE source_id=)SQL" + sid + ";");

            db.exec(R"SQL(
INSERT INTO usage_evidence(artifact_id,source_id,store_guid,inode_num,field_name,field_value,parsed_utc)
SELECT a.artifact_id, kv.source_id, kv.store_guid, kv.inode_num, kv.field_name, kv.field_value,
       (SELECT parsed_utc FROM raw_date_candidates d WHERE d.source_id=kv.source_id AND d.store_guid=kv.store_guid AND d.inode_num=kv.inode_num AND d.field_name=kv.field_name LIMIT 1)
FROM raw_key_values kv
JOIN artifacts a ON a.source_id=kv.source_id AND a.store_guid=kv.store_guid AND a.inode_num=kv.inode_num
WHERE kv.source_id=)SQL" + sid + R"SQL( AND (lower(kv.field_name) LIKE '%used%' OR lower(kv.field_name) LIKE '%usecount%' OR lower(kv.field_name) LIKE '%lastused%');
)SQL");

            db.exec(R"SQL(
UPDATE artifacts
SET use_count_value = COALESCE((
        SELECT kv.field_value
        FROM raw_key_values kv
        WHERE kv.source_id=artifacts.source_id
          AND kv.store_guid=artifacts.store_guid
          AND kv.inode_num=artifacts.inode_num
          AND lower(kv.field_name) IN ('kmditemusecount','kmditemusedcount','usecount')
          AND COALESCE(NULLIF(kv.field_value,''),'')<>''
        LIMIT 1
    ), use_count_value),
    first_used_candidate_utc = COALESCE((
        SELECT MIN(d.parsed_utc)
        FROM raw_date_candidates d
        WHERE d.source_id=artifacts.source_id
          AND d.store_guid=artifacts.store_guid
          AND d.inode_num=artifacts.inode_num
          AND lower(d.field_name) LIKE '%useddates%'
          AND COALESCE(NULLIF(d.parsed_utc,''),'')<>''
    ), first_used_candidate_utc),
    used_dates_count = COALESCE((
        SELECT COUNT(*)
        FROM raw_date_candidates d
        WHERE d.source_id=artifacts.source_id
          AND d.store_guid=artifacts.store_guid
          AND d.inode_num=artifacts.inode_num
          AND lower(d.field_name) LIKE '%useddates%'
          AND COALESCE(NULLIF(d.parsed_utc,''),'')<>''
    ), used_dates_count),
    usage_field_summary = COALESCE((
        SELECT group_concat(u.field_name || '=' || COALESCE(NULLIF(u.parsed_utc,''), u.field_value), '; ')
        FROM usage_evidence u
        WHERE u.artifact_id=artifacts.artifact_id
    ), usage_field_summary)
WHERE source_id=)SQL" + sid + ";");
        } else {
            log.info("Skipping key/value enrichment updates because raw_key_values is empty in conservative native mode.");
        }

        if (specialDateCount > 0) {
            log.info("Applying special date enrichment updates.");
            db.exec(R"SQL(
UPDATE artifacts
SET downloaded_date_utc = (SELECT parsed_utc FROM raw_date_candidates d WHERE d.source_id=artifacts.source_id AND d.store_guid=artifacts.store_guid AND d.inode_num=artifacts.inode_num AND d.field_name LIKE '%DownloadedDate%' LIMIT 1),
    last_used_date_utc = (SELECT COALESCE(NULLIF(d.parsed_utc,''), d.field_value) FROM raw_date_candidates d WHERE d.source_id=artifacts.source_id AND d.store_guid=artifacts.store_guid AND d.inode_num=artifacts.inode_num AND lower(d.field_name) LIKE '%lastuseddate%' LIMIT 1)
WHERE source_id=)SQL" + sid + ";");
        }

        if (kvCount > 0 || specialDateCount > 0) {
            db.exec(R"SQL(
UPDATE artifacts
SET open_count_estimate = COALESCE((SELECT CAST(field_value AS INTEGER) FROM raw_key_values kv WHERE kv.source_id=artifacts.source_id AND kv.store_guid=artifacts.store_guid AND kv.inode_num=artifacts.inode_num AND lower(kv.field_name) IN ('kmditemusecount','kmditemusedcount','usecount') LIMIT 1),
                                 CASE WHEN used_dates_utc IS NOT NULL AND used_dates_utc <> '' THEN 1 ELSE 0 END,
                                 CASE WHEN last_used_date_utc IS NOT NULL AND last_used_date_utc <> '' THEN 1 ELSE 0 END)
WHERE source_id=)SQL" + sid + ";");
        }

        log.info("Attributing raw date candidates to artifact/object context for review views.");
        appendEnrichmentRunStatus(db, 84, "enrichment_date_attribution_start", "raw_date_candidates=" + std::to_string(dateCount));
        db.exec(R"SQL(
UPDATE raw_date_candidates
SET artifact_id = (SELECT a.artifact_id FROM artifacts a WHERE a.source_id=raw_date_candidates.source_id AND a.store_guid=raw_date_candidates.store_guid AND a.inode_num=raw_date_candidates.inode_num LIMIT 1),
    parent_inode_num = COALESCE((SELECT a.parent_inode_num FROM artifacts a WHERE a.source_id=raw_date_candidates.source_id AND a.store_guid=raw_date_candidates.store_guid AND a.inode_num=raw_date_candidates.inode_num LIMIT 1), parent_inode_num),
    file_name = COALESCE((SELECT a.file_name FROM artifacts a WHERE a.source_id=raw_date_candidates.source_id AND a.store_guid=raw_date_candidates.store_guid AND a.inode_num=raw_date_candidates.inode_num LIMIT 1), file_name),
    best_path = COALESCE((SELECT a.best_path FROM artifacts a WHERE a.source_id=raw_date_candidates.source_id AND a.store_guid=raw_date_candidates.store_guid AND a.inode_num=raw_date_candidates.inode_num LIMIT 1), best_path),
    date_type = CASE
      WHEN lower(field_name) LIKE '%lastuseddate%' THEN 'opened_last'
      WHEN lower(field_name) LIKE '%useddates%' THEN 'used_date'
      WHEN lower(field_name) LIKE '%recent%spotlight%engagement%' THEN 'spotlight_engagement'
      WHEN lower(field_name) LIKE '%download%' THEN 'downloaded'
      WHEN lower(field_name) LIKE '%contentcreation%' OR lower(field_name) LIKE '%creationdate%' THEN 'created'
      WHEN lower(field_name) LIKE '%contentmodification%' OR lower(field_name) LIKE '%contentchange%' OR lower(field_name) LIKE '%modificationdate%' THEN 'modified'
      WHEN lower(field_name) LIKE '%interestingdate%' THEN 'interesting_or_index_date'
      WHEN lower(field_name) LIKE '%last_updated%' OR lower(field_name) LIKE '%lastupdated%' THEN 'metadata_seen_or_index_updated'
      ELSE 'other_date'
    END,
    association_status = CASE
      WHEN EXISTS (SELECT 1 FROM artifacts a WHERE a.source_id=raw_date_candidates.source_id AND a.store_guid=raw_date_candidates.store_guid AND a.inode_num=raw_date_candidates.inode_num) THEN 'ARTIFACT_MATCHED_BY_SOURCE_STORE_INODE'
      ELSE 'NO_ARTIFACT_MATCH_BY_SOURCE_STORE_INODE'
    END,
    association_confidence = CASE
      WHEN EXISTS (SELECT 1 FROM artifacts a WHERE a.source_id=raw_date_candidates.source_id AND a.store_guid=raw_date_candidates.store_guid AND a.inode_num=raw_date_candidates.inode_num AND COALESCE(NULLIF(a.best_path,''),'')<>'' AND COALESCE(NULLIF(a.file_name,''),'')<>'') THEN 'HIGH_OBJECT_CONTEXT'
      WHEN EXISTS (SELECT 1 FROM artifacts a WHERE a.source_id=raw_date_candidates.source_id AND a.store_guid=raw_date_candidates.store_guid AND a.inode_num=raw_date_candidates.inode_num AND (COALESCE(NULLIF(a.best_path,''),'')<>'' OR COALESCE(NULLIF(a.file_name,''),'')<>'')) THEN 'MEDIUM_OBJECT_CONTEXT'
)SQL" R"SQL(      WHEN EXISTS (SELECT 1 FROM artifacts a WHERE a.source_id=raw_date_candidates.source_id AND a.store_guid=raw_date_candidates.store_guid AND a.inode_num=raw_date_candidates.inode_num) THEN 'LOW_OBJECT_CONTEXT'
      ELSE 'NONE'
    END
WHERE source_id=)SQL" + sid + R"SQL(;
)SQL");
        appendEnrichmentRunStatus(db, 85, "enrichment_date_attribution_complete", "associated_dates=" + std::to_string(scalarCount(db, "SELECT COUNT(*) FROM raw_date_candidates WHERE source_id=? AND artifact_id IS NOT NULL", source.sourceId)));

        log.info("Materializing artifact date summaries for responsive artifact_dates_wide export and GUI review.");
        appendEnrichmentRunStatus(db, 86, "enrichment_artifact_date_summary_start", "artifacts=" + std::to_string(scalarCount(db, "SELECT COUNT(*) FROM artifacts WHERE source_id=?", source.sourceId)) + " raw_date_candidates=" + std::to_string(dateCount));
        db.exec("DELETE FROM artifact_date_summary WHERE source_id=" + sid + ";");
        db.exec("DROP TABLE IF EXISTS temp_artifact_date_rollup;");
        appendEnrichmentRunStatus(db, 87, "enrichment_artifact_date_rollup_start", "building temp_artifact_date_rollup");
        db.exec(R"SQL(
CREATE TEMP TABLE temp_artifact_date_rollup AS
SELECT d.artifact_id,
       MIN(d.parsed_utc) AS first_date_utc,
       MAX(d.parsed_utc) AS last_date_utc,
       COUNT(*) AS total_date_count,
       MIN(CASE WHEN d.date_type='created' THEN d.parsed_utc END) AS created_earliest_utc,
       MAX(CASE WHEN d.date_type='created' THEN d.parsed_utc END) AS created_latest_utc,
       MIN(CASE WHEN d.date_type='modified' THEN d.parsed_utc END) AS modified_earliest_utc,
       MAX(CASE WHEN d.date_type='modified' THEN d.parsed_utc END) AS modified_latest_utc,
       MIN(CASE WHEN d.date_type='downloaded' THEN d.parsed_utc END) AS downloaded_earliest_utc,
       MAX(CASE WHEN d.date_type='downloaded' THEN d.parsed_utc END) AS downloaded_latest_utc,
       MIN(CASE WHEN d.date_type IN ('opened_last','used_date','spotlight_engagement') THEN d.parsed_utc END) AS usage_earliest_utc,
       MAX(CASE WHEN d.date_type IN ('opened_last','used_date','spotlight_engagement') THEN d.parsed_utc END) AS usage_latest_utc,
       MIN(CASE WHEN d.date_type='interesting_or_index_date' THEN d.parsed_utc END) AS interesting_or_index_earliest_utc,
       MAX(CASE WHEN d.date_type='interesting_or_index_date' THEN d.parsed_utc END) AS interesting_or_index_latest_utc,
       SUM(CASE WHEN d.date_type='created' THEN 1 ELSE 0 END) AS created_date_count,
       SUM(CASE WHEN d.date_type='modified' THEN 1 ELSE 0 END) AS modified_date_count,
       SUM(CASE WHEN d.date_type='downloaded' THEN 1 ELSE 0 END) AS downloaded_date_count,
       SUM(CASE WHEN d.date_type IN ('opened_last','used_date','spotlight_engagement') THEN 1 ELSE 0 END) AS usage_date_count,
       SUM(CASE WHEN d.date_type='interesting_or_index_date' THEN 1 ELSE 0 END) AS interesting_or_index_date_count,
       SUM(CASE WHEN d.date_type='metadata_seen_or_index_updated' THEN 1 ELSE 0 END) AS metadata_seen_or_index_updated_count,
       SUM(CASE WHEN d.date_type='other_date' THEN 1 ELSE 0 END) AS other_date_count,
       SUM(CASE WHEN d.date_type IN ('interesting_or_index_date','metadata_seen_or_index_updated') OR lower(d.field_name) LIKE '%interestingdate%' OR lower(d.field_name) LIKE '%ranking%' OR lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 1 ELSE 0 END) AS likely_snapshot_or_index_date_count,
       SUM(CASE WHEN d.association_status='ARTIFACT_MATCHED_BY_SOURCE_STORE_INODE' THEN 1 ELSE 0 END) AS associated_date_count,
       SUM(CASE WHEN COALESCE(d.association_status,'')<>'ARTIFACT_MATCHED_BY_SOURCE_STORE_INODE' THEN 1 ELSE 0 END) AS unassociated_date_count,
       GROUP_CONCAT(DISTINCT d.field_name) AS available_date_fields,
       GROUP_CONCAT(DISTINCT d.date_type) AS interpreted_date_types,
)SQL" R"SQL(       GROUP_CONCAT(DISTINCT d.association_confidence) AS association_confidence_summary,
       GROUP_CONCAT(DISTINCT CASE
         WHEN lower(d.field_name) LIKE '%interestingdate%' THEN 'FIELD_IS_SPOTLIGHT_INTERESTING_DATE'
         WHEN lower(d.field_name) LIKE '%ranking%' THEN 'FIELD_IS_SPOTLIGHT_RANKING_DATE'
         WHEN lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 'FIELD_IS_RECORD_LAST_UPDATED_OR_INDEX_DATE'
         WHEN d.date_type='interesting_or_index_date' THEN 'DATE_TYPE_INTERESTING_OR_INDEX'
         WHEN d.date_type='metadata_seen_or_index_updated' THEN 'DATE_TYPE_METADATA_SEEN_OR_INDEX_UPDATED'
         ELSE NULL END) AS snapshot_warning_reasons
FROM raw_date_candidates d
WHERE d.source_id=)SQL" + sid + R"SQL(
  AND d.artifact_id IS NOT NULL
  AND COALESCE(d.parsed_utc,'')<>''
GROUP BY d.artifact_id;
)SQL");
        db.exec("CREATE INDEX IF NOT EXISTS temp_idx_artifact_date_rollup_artifact ON temp_artifact_date_rollup(artifact_id);");
        appendEnrichmentRunStatus(db, 88, "enrichment_artifact_date_rollup_complete", "rollup_rows=" + std::to_string(scalarCount(db, "SELECT COUNT(*) FROM temp_artifact_date_rollup")));
        appendEnrichmentRunStatus(db, 89, "enrichment_artifact_date_summary_insert_start", "materializing rows from artifacts plus date rollup");
        db.exec(R"SQL(
INSERT OR REPLACE INTO artifact_date_summary(
  artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,logical_size_bytes,physical_size_bytes,content_type,where_froms,
  created_earliest_utc,created_latest_utc,modified_earliest_utc,modified_latest_utc,downloaded_earliest_utc,downloaded_latest_utc,usage_earliest_utc,usage_latest_utc,interesting_or_index_earliest_utc,interesting_or_index_latest_utc,
  likely_snapshot_date_count,associated_date_count,unassociated_date_count,available_date_fields,association_confidence_summary,snapshot_warning_reasons,
  first_date_utc,last_date_utc,total_date_count,created_date_count,modified_date_count,downloaded_date_count,usage_date_count,interesting_or_index_date_count,metadata_seen_or_index_updated_count,other_date_count,likely_snapshot_or_index_date_count,
  interpreted_date_types,date_association_status,date_association_confidence,refreshed_utc)
SELECT a.artifact_id,a.source_id,a.store_guid,a.inode_num,a.parent_inode_num,a.file_name,a.display_name,a.best_path,a.path_source,a.path_status,a.logical_size_bytes,a.physical_size_bytes,a.content_type,a.where_froms,
       r.created_earliest_utc,r.created_latest_utc,r.modified_earliest_utc,r.modified_latest_utc,r.downloaded_earliest_utc,r.downloaded_latest_utc,r.usage_earliest_utc,r.usage_latest_utc,r.interesting_or_index_earliest_utc,r.interesting_or_index_latest_utc,
       COALESCE(r.likely_snapshot_or_index_date_count,0),COALESCE(r.associated_date_count,0),COALESCE(r.unassociated_date_count,0),COALESCE(r.available_date_fields,''),COALESCE(r.association_confidence_summary,''),COALESCE(r.snapshot_warning_reasons,''),
       r.first_date_utc,r.last_date_utc,COALESCE(r.total_date_count,0),COALESCE(r.created_date_count,0),COALESCE(r.modified_date_count,0),COALESCE(r.downloaded_date_count,0),COALESCE(r.usage_date_count,0),COALESCE(r.interesting_or_index_date_count,0),COALESCE(r.metadata_seen_or_index_updated_count,0),COALESCE(r.other_date_count,0),COALESCE(r.likely_snapshot_or_index_date_count,0),
       COALESCE(r.interpreted_date_types,''),
       CASE
         WHEN COALESCE(r.total_date_count,0)=0 THEN 'NO_PARSED_DATE_CANDIDATES'
         WHEN COALESCE(r.associated_date_count,0)=COALESCE(r.total_date_count,0) THEN 'ALL_DATES_ASSOCIATED_TO_THIS_OBJECT'
         WHEN COALESCE(r.associated_date_count,0)>0 THEN 'PARTIAL_DATE_ASSOCIATION_REVIEW_NEEDED'
         ELSE 'NO_DATE_ARTIFACT_ASSOCIATION'
       END,
       COALESCE(r.association_confidence_summary,''),
       strftime('%Y-%m-%dT%H:%M:%SZ','now')
FROM artifacts a
LEFT JOIN temp_artifact_date_rollup r ON r.artifact_id=a.artifact_id
WHERE a.source_id=)SQL" + sid + R"SQL(;
)SQL");
        db.exec("DROP TABLE IF EXISTS temp_artifact_date_rollup;");
        const std::size_t artifactDateSummaryRows = scalarCount(db, "SELECT COUNT(*) FROM artifact_date_summary WHERE source_id=?", source.sourceId);
        log.info("Materialized artifact date summary rows=" + std::to_string(artifactDateSummaryRows));
        appendEnrichmentRunStatus(db, 90, "enrichment_artifact_date_summary_complete", "artifact_date_summary_rows=" + std::to_string(artifactDateSummaryRows));

        log.info("Flagging native Spotlight records and native probe values that indicate mounted volume paths.");
        db.exec(R"SQL(
DROP TABLE IF EXISTS temp_native_mounted_volume_probe_raw;
DROP TABLE IF EXISTS temp_native_mounted_volume_probes;
CREATE TEMP TABLE temp_native_mounted_volume_probe_raw AS
WITH cleaned AS (
  SELECT kv.raw_kv_id, kv.source_id, kv.store_guid, kv.inode_num, kv.field_name,
         replace(replace(kv.field_value, '<string>', ''), '</string>', '') AS candidate_path
  FROM raw_key_values kv
  WHERE kv.source_id=)SQL" + sid + R"SQL(
    AND lower(kv.field_value) LIKE '%/volumes/%'
)
SELECT c.*,
       CASE
         WHEN c.candidate_path LIKE '/System/Volumes/Data/Volumes/%' THEN
           CASE WHEN instr(substr(c.candidate_path, length('/System/Volumes/Data/Volumes/') + 1), '/') > 0
                THEN substr(substr(c.candidate_path, length('/System/Volumes/Data/Volumes/') + 1), 1, instr(substr(c.candidate_path, length('/System/Volumes/Data/Volumes/') + 1), '/') - 1)
                ELSE substr(c.candidate_path, length('/System/Volumes/Data/Volumes/') + 1) END
         WHEN c.candidate_path LIKE '/Volumes/%' THEN
           CASE WHEN instr(substr(c.candidate_path, length('/Volumes/') + 1), '/') > 0
                THEN substr(substr(c.candidate_path, length('/Volumes/') + 1), 1, instr(substr(c.candidate_path, length('/Volumes/') + 1), '/') - 1)
                ELSE substr(c.candidate_path, length('/Volumes/') + 1) END
         WHEN c.candidate_path LIKE 'Volumes/%' THEN
           CASE WHEN instr(substr(c.candidate_path, length('Volumes/') + 1), '/') > 0
                THEN substr(substr(c.candidate_path, length('Volumes/') + 1), 1, instr(substr(c.candidate_path, length('Volumes/') + 1), '/') - 1)
                ELSE substr(c.candidate_path, length('Volumes/') + 1) END
         ELSE NULL
       END AS mounted_volume_name
FROM cleaned c
WHERE c.candidate_path LIKE '/System/Volumes/Data/Volumes/%'
   OR c.candidate_path LIKE '/Volumes/%'
   OR c.candidate_path LIKE 'Volumes/%';

CREATE TEMP TABLE temp_native_mounted_volume_probes AS
SELECT source_id,
       store_guid,
       inode_num,
       CASE
         WHEN SUM(CASE WHEN field_name='__native_probe_mounted_volume_path' THEN 1 ELSE 0 END) > 0 THEN '__native_probe_mounted_volume_path'
         ELSE MIN(field_name)
       END AS field_name,
       candidate_path,
       MAX(mounted_volume_name) AS mounted_volume_name
FROM temp_native_mounted_volume_probe_raw
GROUP BY source_id, store_guid, inode_num, candidate_path;
)SQL");

        db.exec(R"SQL(
UPDATE artifacts
SET is_mounted_volume_path = CASE
      WHEN best_path LIKE '/System/Volumes/Data/Volumes/%' OR best_path LIKE '/Volumes/%' OR best_path LIKE 'Volumes/%' THEN 1
      WHEN EXISTS (SELECT 1 FROM temp_native_mounted_volume_probes p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num) THEN 1
      ELSE 0 END,
    mounted_volume_name = COALESCE(
      CASE
        WHEN best_path LIKE '/System/Volumes/Data/Volumes/%' THEN
          CASE WHEN instr(substr(best_path, length('/System/Volumes/Data/Volumes/') + 1), '/') > 0 THEN substr(substr(best_path, length('/System/Volumes/Data/Volumes/') + 1), 1, instr(substr(best_path, length('/System/Volumes/Data/Volumes/') + 1), '/') - 1) ELSE substr(best_path, length('/System/Volumes/Data/Volumes/') + 1) END
        WHEN best_path LIKE '/Volumes/%' THEN
          CASE WHEN instr(substr(best_path, length('/Volumes/') + 1), '/') > 0 THEN substr(substr(best_path, length('/Volumes/') + 1), 1, instr(substr(best_path, length('/Volumes/') + 1), '/') - 1) ELSE substr(best_path, length('/Volumes/') + 1) END
        WHEN best_path LIKE 'Volumes/%' THEN
          CASE WHEN instr(substr(best_path, length('Volumes/') + 1), '/') > 0 THEN substr(substr(best_path, length('Volumes/') + 1), 1, instr(substr(best_path, length('Volumes/') + 1), '/') - 1) ELSE substr(best_path, length('Volumes/') + 1) END
        ELSE NULL
      END,
      (SELECT p.mounted_volume_name FROM temp_native_mounted_volume_probes p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num LIMIT 1),
      mounted_volume_name
    ),
    external_volume_reason = CASE
      WHEN best_path LIKE '/System/Volumes/Data/Volumes/%' THEN 'PATH_PREFIX_SYSTEM_VOLUMES_DATA_VOLUMES_NATIVE_SPOTLIGHT'
      WHEN best_path LIKE '/Volumes/%' OR best_path LIKE 'Volumes/%' THEN 'PATH_PREFIX_VOLUMES_NATIVE_SPOTLIGHT'
      WHEN EXISTS (SELECT 1 FROM temp_native_mounted_volume_probes p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num) THEN 'RAW_NATIVE_PROBE_CONTAINS_MOUNTED_VOLUME_PATH'
      ELSE external_volume_reason END
WHERE source_id=)SQL" + sid + ";");

        db.exec(R"SQL(
INSERT INTO external_volume_candidates(artifact_id,source_id,store_guid,inode_num,file_name,best_path,mounted_volume_name,reason,confidence,detection_source_field,detection_source_value)
SELECT artifact_id,source_id,store_guid,inode_num,file_name,best_path,mounted_volume_name,external_volume_reason,'MOUNTED_VOLUME_PATH_ONLY_NOT_USB_SPECIFIC','best_path',best_path
FROM artifacts
WHERE source_id=)SQL" + sid + R"SQL(
  AND (best_path LIKE '/System/Volumes/Data/Volumes/%' OR best_path LIKE '/Volumes/%' OR best_path LIKE 'Volumes/%');
)SQL");

        db.exec(R"SQL(
INSERT INTO external_volume_candidates(artifact_id,source_id,store_guid,inode_num,file_name,best_path,mounted_volume_name,reason,confidence,detection_source_field,detection_source_value)
SELECT a.artifact_id,a.source_id,a.store_guid,a.inode_num,a.file_name,a.best_path,p.mounted_volume_name,'RAW_NATIVE_PROBE_CONTAINS_MOUNTED_VOLUME_PATH','MOUNTED_VOLUME_PATH_ONLY_NOT_USB_SPECIFIC',p.field_name,p.candidate_path
FROM temp_native_mounted_volume_probes p
JOIN artifacts a ON a.source_id=p.source_id AND a.store_guid=p.store_guid AND a.inode_num=p.inode_num
WHERE a.source_id=)SQL" + sid + R"SQL(
  AND NOT EXISTS (
    SELECT 1 FROM external_volume_candidates e
    WHERE e.artifact_id=a.artifact_id AND e.detection_source_field=p.field_name AND e.detection_source_value=p.candidate_path
  );
)SQL");
        log.info("Building grouped timeline_events from artifact_date_summary; raw_date_candidates remains the date-by-date provenance table.");
        appendEnrichmentRunStatus(db, 90, "enrichment_timeline_summary_insert_start", "source_id=" + source.sourceId + " artifact_date_summary_rows=" + std::to_string(scalarCount(db, "SELECT COUNT(*) FROM artifact_date_summary WHERE source_id=?", source.sourceId)) + " raw_date_candidates=" + std::to_string(dateCount));
        appendEnrichmentRunStatus(db, 90, "enrichment_timeline_disk_space_preflight", sqliteDiskSpaceSummary(db));
        db.exec(R"SQL(
INSERT INTO timeline_events(artifact_id,source_id,store_guid,inode_num,event_timestamp_utc,event_type,event_source_field,file_name,path,existence_status,deleted_or_orphaned_candidate)
SELECT a.artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
       COALESCE(NULLIF(s.last_date_utc,''), NULLIF(s.usage_latest_utc,''), NULLIF(s.modified_latest_utc,''), NULLIF(s.created_latest_utc,''), NULLIF(a.last_updated_utc,''), '') AS event_timestamp_utc,
       'TIME_SUMMARY' AS event_type,
       'GROUPED_BY_ARTIFACT_INODE; total_dates=' || COALESCE(CAST(s.total_date_count AS TEXT),'0') ||
       '; usage_dates=' || COALESCE(CAST(s.usage_date_count AS TEXT),'0') ||
       '; fields=' || substr(COALESCE(s.available_date_fields,''),1,650) AS event_source_field,
       a.file_name,
       a.best_path,
       'NOT_CHECKED',
       0
FROM artifact_date_summary s
JOIN artifacts a ON a.artifact_id=s.artifact_id
WHERE s.source_id=)SQL" + sid + R"SQL(
  AND COALESCE(s.total_date_count,0) > 0;
)SQL");
        const std::size_t timelineRowsAfterInsert = scalarCount(db, "SELECT COUNT(*) FROM timeline_events WHERE source_id=?", source.sourceId);
        appendEnrichmentRunStatus(db, 90, "enrichment_timeline_summary_insert_complete", "timeline_summary_rows=" + std::to_string(timelineRowsAfterInsert) + " grouped_by=artifact_id_inode raw_date_candidates_preserved=" + std::to_string(dateCount));

        insertMetric(db, source.sourceId, "raw_records", std::to_string(rawRecordCount));
        insertMetric(db, source.sourceId, "raw_key_values", std::to_string(kvCount));
        insertMetric(db, source.sourceId, "raw_date_candidates", std::to_string(dateCount));
        insertMetric(db, source.sourceId, "date_field_name_count", std::to_string(scalarCount(db, "SELECT COUNT(DISTINCT field_name) FROM raw_date_candidates WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "date_candidates_artifact_matched", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM raw_date_candidates WHERE source_id=? AND association_status='ARTIFACT_MATCHED_BY_SOURCE_STORE_INODE'", source.sourceId)));
        insertMetric(db, source.sourceId, "date_candidates_artifact_unmatched", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM raw_date_candidates WHERE source_id=? AND COALESCE(association_status,'')<>'ARTIFACT_MATCHED_BY_SOURCE_STORE_INODE'", source.sourceId)));
        insertMetric(db, source.sourceId, "artifact_date_summary_rows", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM artifact_date_summary WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "date_candidate_identity_key", "source_id|store_guid|source_db|inode_num|store_id");
        appendEnrichmentRunStatus(db, 90, "enrichment_metrics_date_count_cache_start", "source_id=" + source.sourceId);
        db.exec(R"SQL(
DROP TABLE IF EXISTS temp_record_date_counts;
CREATE TEMP TABLE temp_record_date_counts AS
SELECT r.raw_record_id AS raw_record_id, COUNT(d.raw_date_id) AS c
FROM raw_records r
LEFT JOIN raw_date_candidates d
  ON d.source_id=r.source_id
 AND d.store_guid=r.store_guid
 AND d.source_db=r.source_db
 AND d.inode_num=r.inode_num
 AND d.store_id=r.store_id
WHERE r.source_id=)SQL" + sid + R"SQL(
GROUP BY r.raw_record_id;
CREATE INDEX IF NOT EXISTS idx_temp_record_date_counts_c ON temp_record_date_counts(c);
)SQL");
        const std::size_t recordsWith0Dates = scalarCount(db, "SELECT COUNT(*) FROM temp_record_date_counts WHERE c=0");
        const std::size_t recordsWith1Date = scalarCount(db, "SELECT COUNT(*) FROM temp_record_date_counts WHERE c=1");
        const std::size_t recordsWith2PlusDates = scalarCount(db, "SELECT COUNT(*) FROM temp_record_date_counts WHERE c>=2");
        const std::size_t maxDatesPerRecord = scalarCount(db, "SELECT COALESCE(MAX(c),0) FROM temp_record_date_counts");
        appendEnrichmentRunStatus(db, 90, "enrichment_metrics_date_count_cache_complete", "records=" + std::to_string(rawRecordCount) + " zero=" + std::to_string(recordsWith0Dates) + " one=" + std::to_string(recordsWith1Date) + " two_plus=" + std::to_string(recordsWith2PlusDates) + " max=" + std::to_string(maxDatesPerRecord));
        insertMetric(db, source.sourceId, "records_with_0_dates", std::to_string(recordsWith0Dates));
        insertMetric(db, source.sourceId, "records_with_1_date", std::to_string(recordsWith1Date));
        insertMetric(db, source.sourceId, "records_with_2plus_dates", std::to_string(recordsWith2PlusDates));
        insertMetric(db, source.sourceId, "max_dates_per_record", std::to_string(maxDatesPerRecord));
        db.exec("DROP TABLE IF EXISTS temp_record_date_counts;");
        insertMetric(db, source.sourceId, "deduplicated_artifacts", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM artifacts WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "source_copy_instances", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM artifact_source_instances WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "field_inventory_fields", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM field_inventory WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "native_property_dictionary_rows", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM native_property_dictionary WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "native_core_property_dictionary_rows", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM native_property_dictionary WHERE source_id=? AND is_core_native_field=1", source.sourceId)));
        insertMetric(db, source.sourceId, "native_decode_attempts", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM native_decode_attempts WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "native_probe_values", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM raw_key_values WHERE source_id=? AND field_name LIKE '__native_%'", source.sourceId)));
        insertMetric(db, source.sourceId, "mounted_volume_candidates", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM external_volume_candidates WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "parent_inode_links", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "parent_inode_links_matched", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE'", source.sourceId)));
        insertMetric(db, source.sourceId, "parent_inode_links_with_path_context_candidate", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND COALESCE(reconstructed_path_candidate,'')<>''", source.sourceId)));
        insertMetric(db, source.sourceId, "parent_inode_links_with_existing_path_context", std::to_string(scalarCount(db, R"SQL(SELECT COUNT(*) FROM parent_inode_links pl LEFT JOIN artifacts a ON a.artifact_id=pl.child_artifact_id WHERE pl.source_id=? AND COALESCE(pl.reconstructed_path_candidate,'')<>'' AND COALESCE(a.best_path,'')=COALESCE(pl.reconstructed_path_candidate,'') AND COALESCE(a.path_source,'') NOT IN ('PARENT_INODE_RECONSTRUCTION','PARENT_INODE_CHAIN_RECONSTRUCTION','PARENT_INODE_RELATIVE_CHAIN_REVIEW'))SQL", source.sourceId)));
        insertMetric(db, source.sourceId, "parent_inode_links_with_new_reconstructed_path", std::to_string(scalarCount(db, R"SQL(SELECT COUNT(*) FROM parent_inode_links pl LEFT JOIN artifacts a ON a.artifact_id=pl.child_artifact_id WHERE pl.source_id=? AND COALESCE(pl.reconstructed_path_candidate,'')<>'' AND (COALESCE(a.best_path,'')<>COALESCE(pl.reconstructed_path_candidate,'') OR COALESCE(a.path_source,'') IN ('PARENT_INODE_RECONSTRUCTION','PARENT_INODE_CHAIN_RECONSTRUCTION','PARENT_INODE_RELATIVE_CHAIN_REVIEW')))SQL", source.sourceId)));
        insertMetric(db, source.sourceId, "unresolved_identifier_labels_suppressed_from_parent_path_reconstruction", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM artifacts WHERE source_id=? AND (file_name LIKE 'UNRESOLVED_SPOTLIGHT_OBJECT_INODE_%' OR display_name LIKE 'Unresolved Spotlight object inode=%')", source.sourceId)));
        insertMetric(db, source.sourceId, "parent_inode_artifacts_updated_from_reconstruction", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM artifacts WHERE source_id=? AND path_source IN ('PARENT_INODE_RECONSTRUCTION','PARENT_INODE_CHAIN_RECONSTRUCTION','PARENT_INODE_RELATIVE_CHAIN_REVIEW')", source.sourceId)));
        insertMetric(db, source.sourceId, "parent_inode_links_without_child_name", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND path_reconstruction_method='PARENT_INODE_MATCH_NO_CHILD_NAME'", source.sourceId)));
        insertMetric(db, source.sourceId, "parent_inode_links_with_child_name", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.')", source.sourceId)));
        insertMetric(db, source.sourceId, "same_folder_groups_with_valid_child_name", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM (SELECT source_id, store_guid, child_parent_inode_num FROM parent_inode_links WHERE source_id=? GROUP BY source_id, store_guid, child_parent_inode_num HAVING COUNT(*) > 1 AND SUM(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) > 0)", source.sourceId)));
        insertMetric(db, source.sourceId, "same_folder_groups_without_valid_child_name", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM (SELECT source_id, store_guid, child_parent_inode_num FROM parent_inode_links WHERE source_id=? GROUP BY source_id, store_guid, child_parent_inode_num HAVING COUNT(*) > 1 AND SUM(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) = 0)", source.sourceId)));
        insertMetric(db, source.sourceId, "same_folder_groups", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM (SELECT source_id, store_guid, child_parent_inode_num FROM parent_inode_links WHERE source_id=? GROUP BY source_id, store_guid, child_parent_inode_num HAVING COUNT(*) > 1)", source.sourceId)));

        db.commit();
        db.exec("PRAGMA wal_checkpoint(PASSIVE);");
    } catch (...) { db.rollbackNoThrow(); throw; }

    classifyFilesystem(db, source, log, counts);

    auto c = db.prepare("SELECT COUNT(*) FROM artifacts WHERE source_id=?"); c.bind(1, source.sourceId); if (c.stepRow()) counts.artifacts = static_cast<std::size_t>(c.colInt64(0));
    auto u = db.prepare("SELECT COUNT(*) FROM usage_evidence WHERE source_id=?"); u.bind(1, source.sourceId); if (u.stepRow()) counts.usage = static_cast<std::size_t>(u.colInt64(0));
    auto t = db.prepare("SELECT COUNT(*) FROM timeline_events WHERE source_id=?"); t.bind(1, source.sourceId); if (t.stepRow()) counts.timeline = static_cast<std::size_t>(t.colInt64(0));
    auto o = db.prepare("SELECT COUNT(*) FROM orphaned_deleted_candidates WHERE source_id=?"); o.bind(1, source.sourceId); if (o.stepRow()) counts.orphanCandidates = static_cast<std::size_t>(o.colInt64(0));
    log.info("SQLite enrichment complete: artifacts=" + std::to_string(counts.artifacts) + " usage=" + std::to_string(counts.usage) + " timeline=" + std::to_string(counts.timeline) + " orphan/deleted=" + std::to_string(counts.orphanCandidates));
    return counts;
}

void SqliteEnrichment::classifyFilesystem(CaseDatabase& db, const EvidenceSource& source, Logger& log, SqliteEnrichmentCounts&) const {
    const std::string sid = sqlLiteral(source.sourceId);
    const std::size_t iosFullInventoryRows = scalarCount(db, "SELECT COUNT(*) FROM ios_ffs_file_inventory WHERE source_id=?", source.sourceId);
    const std::size_t iosPathLookupRows = scalarCount(db, "SELECT COUNT(*) FROM ios_ffs_path_lookup WHERE source_id=?", source.sourceId);
    const std::size_t imageInventoryRows = scalarCount(db, "SELECT COUNT(*) FROM image_file_inventory WHERE source_id=?", source.sourceId);
    const bool hasIosFfsLookup = (iosFullInventoryRows + iosPathLookupRows) > 0;
    const bool hasImageInventory = imageInventoryRows > 0;

    appendEnrichmentRunStatus(db, 91, "active_comparison_prepare_start", "ios_ffs_file_inventory_rows=" + std::to_string(iosFullInventoryRows) + " ios_ffs_path_lookup_rows=" + std::to_string(iosPathLookupRows) + " image_file_inventory_rows=" + std::to_string(imageInventoryRows));
    db.begin();
    try {
        db.exec("DELETE FROM orphaned_deleted_candidates WHERE source_id=" + sid + ";");
        db.exec("DELETE FROM active_file_comparison_runs WHERE source_id=" + sid + ";");
        appendEnrichmentRunStatus(db, 91, "active_comparison_prior_rows_cleared", "source_id=" + source.sourceId);

        if (!hasIosFfsLookup && hasImageInventory) {
            log.info("Active filesystem comparison enabled using APFS/image_file_inventory rows: image_file_inventory_rows=" + std::to_string(imageInventoryRows) + ". Missing/not-found rows are investigative leads only, not deletion proof.");
            appendEnrichmentRunStatus(db, 92, "active_comparison_image_inventory_start", "image_file_inventory_rows=" + std::to_string(imageInventoryRows));

            db.exec("DROP TABLE IF EXISTS temp_apfs_active_inventory;");
            db.exec(R"SQL(
CREATE TEMP TABLE temp_apfs_active_inventory AS
SELECT image_file_id,
       source_id,
       lower(replace(replace(replace(replace(replace(replace(trim(COALESCE(full_path,'')),'\','/'),'//','/'),'//','/'),'//','/'),'//','/'),'//','/')) AS normalized_path,
       lower(trim(COALESCE(file_name,''))) AS lower_file_name,
       COALESCE(NULLIF(inode_num,''), NULLIF(filesystem_object_id,''), '') AS inode_num,
       COALESCE(NULLIF(parent_inode_num,''), NULLIF(parent_filesystem_object_id,''), '') AS parent_inode_num,
       COALESCE(full_path,'') AS full_path,
       COALESCE(file_name,'') AS file_name,
       COALESCE(provenance,'') AS provenance
FROM image_file_inventory
WHERE source_id=)SQL" + sid + R"SQL(
  AND (COALESCE(NULLIF(full_path,''),'')<>'' OR COALESCE(NULLIF(inode_num,''), NULLIF(filesystem_object_id,''), '')<>'');
)SQL");
            db.exec("CREATE INDEX IF NOT EXISTS idx_temp_apfs_active_inventory_image_id ON temp_apfs_active_inventory(image_file_id);");
            db.exec("CREATE INDEX IF NOT EXISTS idx_temp_apfs_active_inventory_inode ON temp_apfs_active_inventory(source_id, inode_num, parent_inode_num);");
            db.exec("CREATE INDEX IF NOT EXISTS idx_temp_apfs_active_inventory_path ON temp_apfs_active_inventory(source_id, normalized_path);");
            db.exec("CREATE INDEX IF NOT EXISTS idx_temp_apfs_active_inventory_name ON temp_apfs_active_inventory(source_id, lower_file_name);");
            const std::size_t normalizedImageRows = scalarCount(db, "SELECT COUNT(*) FROM temp_apfs_active_inventory");
            appendEnrichmentRunStatus(db, 93, "active_comparison_image_inventory_temp_complete", "normalized_inventory_rows=" + std::to_string(normalizedImageRows));

            db.exec("DROP TABLE IF EXISTS temp_apfs_active_artifacts;");
            db.exec(R"SQL(
CREATE TEMP TABLE temp_apfs_active_artifacts AS
SELECT artifact_id,
       source_id,
       store_guid,
       COALESCE(inode_num,'') AS inode_num,
       COALESCE(parent_inode_num,'') AS parent_inode_num,
       lower(replace(replace(replace(replace(replace(replace(trim(COALESCE(NULLIF(filesystem_lookup_path,''), NULLIF(best_path,''), NULLIF(normalized_mac_path,''), NULLIF(spotlight_display_path,''), '')),'\','/'),'//','/'),'//','/'),'//','/'),'//','/'),'//','/')) AS normalized_path,
       lower(trim(COALESCE(file_name,''))) AS lower_file_name
FROM artifacts
WHERE source_id=)SQL" + sid + ";");
            db.exec("CREATE INDEX IF NOT EXISTS idx_temp_apfs_active_artifacts_id ON temp_apfs_active_artifacts(artifact_id);");
            db.exec("CREATE INDEX IF NOT EXISTS idx_temp_apfs_active_artifacts_inode ON temp_apfs_active_artifacts(source_id, inode_num, parent_inode_num);");
            db.exec("CREATE INDEX IF NOT EXISTS idx_temp_apfs_active_artifacts_path ON temp_apfs_active_artifacts(source_id, normalized_path);");
            const std::size_t artifactRows = scalarCount(db, "SELECT COUNT(*) FROM temp_apfs_active_artifacts");
            const std::size_t comparableRows = scalarCount(db, "SELECT COUNT(*) FROM temp_apfs_active_artifacts WHERE normalized_path<>'' OR inode_num<>'' OR lower_file_name<>''");
            appendEnrichmentRunStatus(db, 94, "active_comparison_image_artifacts_temp_complete", "artifact_rows=" + std::to_string(artifactRows) + " comparable_rows=" + std::to_string(comparableRows));

            db.exec("DROP TABLE IF EXISTS temp_apfs_active_matches;");
            db.exec(R"SQL(
CREATE TEMP TABLE temp_apfs_active_matches(
  artifact_id INTEGER PRIMARY KEY,
  image_file_id INTEGER,
  match_basis TEXT
);
)SQL");

            // Earlier APFS comparison code used correlated scalar subqueries with an ORDER BY expression that
            // referenced the outer artifact alias. SQLite accepted the correlation in
            // WHERE but failed in the ORDER BY on the Windows test run with
            // "no such column: a.parent_inode_num".  Keep the same priority model, but
            // materialize the tiers as discrete INSERT OR IGNORE passes so the matching
            // query is portable and failure-isolated:
            //   1. source/object-id + parent match
            //   2. source/object-id with parent optional/blank
            //   3. exact normalized path
            //   4. lower-confidence filename + parent match
            //   5. lower-confidence filename fallback
            db.exec(R"SQL(
INSERT OR IGNORE INTO temp_apfs_active_matches(artifact_id,image_file_id,match_basis)
SELECT a.artifact_id, MIN(f.image_file_id), 'SOURCE_INODE_PARENT_EXACT'
FROM temp_apfs_active_artifacts a
JOIN temp_apfs_active_inventory f
  ON f.source_id=a.source_id
 AND a.inode_num<>''
 AND f.inode_num=a.inode_num
 AND a.parent_inode_num<>''
 AND f.parent_inode_num=a.parent_inode_num
GROUP BY a.artifact_id;
)SQL");
            appendEnrichmentRunStatus(db, 95, "active_comparison_image_match_tier_inode_parent_exact", "matches=" + std::to_string(lastSqlChangeCount(db)));

            db.exec(R"SQL(
INSERT OR IGNORE INTO temp_apfs_active_matches(artifact_id,image_file_id,match_basis)
SELECT a.artifact_id, MIN(f.image_file_id), 'SOURCE_INODE_PARENT_OPTIONAL'
FROM temp_apfs_active_artifacts a
JOIN temp_apfs_active_inventory f
  ON f.source_id=a.source_id
 AND a.inode_num<>''
 AND f.inode_num=a.inode_num
 AND (a.parent_inode_num='' OR f.parent_inode_num='' OR f.parent_inode_num=a.parent_inode_num)
GROUP BY a.artifact_id;
)SQL");
            appendEnrichmentRunStatus(db, 95, "active_comparison_image_match_tier_inode_parent_optional", "matches=" + std::to_string(lastSqlChangeCount(db)));

            db.exec(R"SQL(
INSERT OR IGNORE INTO temp_apfs_active_matches(artifact_id,image_file_id,match_basis)
SELECT a.artifact_id, MIN(f.image_file_id), 'EXACT_NORMALIZED_PATH'
FROM temp_apfs_active_artifacts a
JOIN temp_apfs_active_inventory f
  ON f.source_id=a.source_id
 AND a.normalized_path<>''
 AND f.normalized_path=a.normalized_path
GROUP BY a.artifact_id;
)SQL");
            appendEnrichmentRunStatus(db, 95, "active_comparison_image_match_tier_exact_path", "matches=" + std::to_string(lastSqlChangeCount(db)));

            db.exec(R"SQL(
INSERT OR IGNORE INTO temp_apfs_active_matches(artifact_id,image_file_id,match_basis)
SELECT a.artifact_id, MIN(f.image_file_id), 'LOWER_CONFIDENCE_NAME_PARENT_EXACT_FALLBACK'
FROM temp_apfs_active_artifacts a
JOIN temp_apfs_active_inventory f
  ON f.source_id=a.source_id
 AND a.lower_file_name<>''
 AND f.lower_file_name=a.lower_file_name
 AND a.parent_inode_num<>''
 AND f.parent_inode_num=a.parent_inode_num
GROUP BY a.artifact_id;
)SQL");
            appendEnrichmentRunStatus(db, 95, "active_comparison_image_match_tier_name_parent_exact", "matches=" + std::to_string(lastSqlChangeCount(db)));

            db.exec(R"SQL(
INSERT OR IGNORE INTO temp_apfs_active_matches(artifact_id,image_file_id,match_basis)
SELECT a.artifact_id, MIN(f.image_file_id), 'LOWER_CONFIDENCE_NAME_PARENT_FALLBACK'
FROM temp_apfs_active_artifacts a
JOIN temp_apfs_active_inventory f
  ON f.source_id=a.source_id
 AND a.lower_file_name<>''
 AND f.lower_file_name=a.lower_file_name
 AND (a.parent_inode_num='' OR f.parent_inode_num='' OR f.parent_inode_num=a.parent_inode_num)
GROUP BY a.artifact_id;
)SQL");
            appendEnrichmentRunStatus(db, 95, "active_comparison_image_match_tier_name_parent_optional", "matches=" + std::to_string(lastSqlChangeCount(db)));
            db.exec("CREATE INDEX IF NOT EXISTS idx_temp_apfs_active_matches_id ON temp_apfs_active_matches(artifact_id);");
            db.exec("CREATE INDEX IF NOT EXISTS idx_temp_apfs_active_matches_image ON temp_apfs_active_matches(image_file_id);");
            const std::size_t imageMatchedRows = scalarCount(db, "SELECT COUNT(*) FROM temp_apfs_active_matches WHERE image_file_id IS NOT NULL");
            appendEnrichmentRunStatus(db, 95, "active_comparison_image_matches_complete", "matched_artifacts=" + std::to_string(imageMatchedRows));

            appendEnrichmentRunStatus(db, 95, "active_comparison_image_present_materialize_start", "matched_artifacts=" + std::to_string(imageMatchedRows));
            db.exec("DROP TABLE IF EXISTS temp_apfs_active_present;");
            db.exec(R"SQL(
CREATE TEMP TABLE temp_apfs_active_present AS
SELECT m.artifact_id,
       m.image_file_id,
       COALESCE(f.full_path,'') AS matched_filesystem_path,
       COALESCE(m.match_basis,'') AS match_basis
FROM temp_apfs_active_matches m
LEFT JOIN temp_apfs_active_inventory f ON f.image_file_id=m.image_file_id
WHERE m.image_file_id IS NOT NULL;
)SQL");
            db.exec("CREATE INDEX IF NOT EXISTS idx_temp_apfs_active_present_artifact ON temp_apfs_active_present(artifact_id);");
            const std::size_t presentMaterializedRows = scalarCount(db, "SELECT COUNT(*) FROM temp_apfs_active_present");
            appendEnrichmentRunStatus(db, 95, "active_comparison_image_present_materialize_complete", "rows=" + std::to_string(presentMaterializedRows));

            appendEnrichmentRunStatus(db, 96, "active_comparison_image_baseline_update_start", "artifacts=" + std::to_string(artifactRows));
            db.exec(R"SQL(
UPDATE artifacts
SET filesystem_lookup_path = COALESCE((SELECT normalized_path FROM temp_apfs_active_artifacts a WHERE a.artifact_id=artifacts.artifact_id),''),
    matched_filesystem_path = '',
    deleted_or_orphaned_candidate = 0,
    orphan_reason = '',
    existence_status = CASE
        WHEN trim(COALESCE(best_path,''))='' AND trim(COALESCE(inode_num,''))='' THEN 'NO_COMPARABLE_PATH_OR_INODE_NOT_CHECKED'
        ELSE 'NOT_CHECKED_APFS_IMAGE_INVENTORY_AVAILABLE'
    END
WHERE source_id=)SQL" + sid + ";");
            appendEnrichmentRunStatus(db, 96, "active_comparison_image_baseline_update_complete", "artifacts_updated=" + std::to_string(lastSqlChangeCount(db)));

            appendEnrichmentRunStatus(db, 96, "active_comparison_image_present_update_start", "present_candidates=" + std::to_string(presentMaterializedRows));
            db.exec(R"SQL(
UPDATE artifacts
SET existence_status='PRESENT_IN_APFS_IMAGE_INVENTORY',
    matched_filesystem_path=COALESCE((SELECT p.matched_filesystem_path FROM temp_apfs_active_present p WHERE p.artifact_id=artifacts.artifact_id),''),
    deleted_or_orphaned_candidate=0,
    orphan_reason='',
    confidence='HIGH_APFS_IMAGE_INVENTORY_MATCH:' || COALESCE((SELECT p.match_basis FROM temp_apfs_active_present p WHERE p.artifact_id=artifacts.artifact_id),'')
WHERE source_id=)SQL" + sid + R"SQL(
  AND EXISTS (SELECT 1 FROM temp_apfs_active_present p WHERE p.artifact_id=artifacts.artifact_id);
)SQL");
            const long long presentRows = lastSqlChangeCount(db);
            appendEnrichmentRunStatus(db, 96, "active_comparison_image_present_update_complete", "artifacts_updated=" + std::to_string(presentRows));

            appendEnrichmentRunStatus(db, 97, "active_comparison_image_missing_update_start", "comparable_rows=" + std::to_string(comparableRows));
            db.exec(R"SQL(
UPDATE artifacts
SET existence_status='MISSING_FROM_APFS_IMAGE_INVENTORY_CANDIDATE',
    matched_filesystem_path='',
    deleted_or_orphaned_candidate=1,
    orphan_reason='Spotlight item did not match APFS-derived image_file_inventory by source/inode/object id, exact normalized path, or lower-confidence filename fallback; investigative lead only, not deletion proof.',
    confidence='MEDIUM_APFS_IMAGE_INVENTORY_ABSENCE_LEAD'
WHERE source_id=)SQL" + sid + R"SQL(
  AND EXISTS (SELECT 1 FROM temp_apfs_active_artifacts a WHERE a.artifact_id=artifacts.artifact_id AND (a.normalized_path<>'' OR a.inode_num<>'' OR a.lower_file_name<>''))
  AND NOT EXISTS (SELECT 1 FROM temp_apfs_active_matches m WHERE m.artifact_id=artifacts.artifact_id AND m.image_file_id IS NOT NULL);
)SQL");
            const long long missingRows = lastSqlChangeCount(db);
            appendEnrichmentRunStatus(db, 97, "active_comparison_image_missing_update_complete", "artifacts_updated=" + std::to_string(missingRows));

            appendEnrichmentRunStatus(db, 98, "active_comparison_image_candidate_insert_start", "missing_candidates=" + std::to_string(missingRows));
            db.exec(R"SQL(
INSERT INTO orphaned_deleted_candidates(source_id,artifact_id,store_guid,inode_num,file_name,best_path,content_type,existence_status,orphan_reason,index_text_snippet)
SELECT source_id, artifact_id, store_guid, inode_num, file_name, best_path,
       COALESCE(NULLIF(content_type,''),'APFS_IMAGE_INVENTORY_ABSENCE_LEAD'),
       existence_status, orphan_reason, index_text_snippet
FROM artifacts
WHERE source_id=)SQL" + sid + R"SQL(
  AND existence_status='MISSING_FROM_APFS_IMAGE_INVENTORY_CANDIDATE'
  AND deleted_or_orphaned_candidate=1;
)SQL");
            const long long candidateRows = lastSqlChangeCount(db);
            appendEnrichmentRunStatus(db, 98, "active_comparison_image_candidate_insert_complete", "rows_inserted=" + std::to_string(candidateRows));

            appendEnrichmentRunStatus(db, 98, "active_comparison_image_run_summary_insert_start", "recording APFS image comparison summary");
            db.exec(R"SQL(
INSERT INTO active_file_comparison_runs(source_id,image_inventory_available,spotlight_artifact_count,image_file_count,inode_match_count,path_match_count,missing_candidate_count,not_checked_count,run_status,comparison_basis,notes,created_utc)
SELECT )SQL" + sid + R"SQL(,
       1,
       COUNT(*),
       )SQL" + std::to_string(imageInventoryRows) + R"SQL(,
       (SELECT COUNT(*) FROM temp_apfs_active_matches WHERE image_file_id IS NOT NULL AND match_basis LIKE 'SOURCE_INODE%'),
       (SELECT COUNT(*) FROM temp_apfs_active_matches WHERE image_file_id IS NOT NULL AND match_basis='EXACT_NORMALIZED_PATH'),
       (SELECT COUNT(*) FROM artifacts WHERE source_id=)SQL" + sid + R"SQL( AND existence_status='MISSING_FROM_APFS_IMAGE_INVENTORY_CANDIDATE'),
       SUM(CASE WHEN existence_status LIKE '%NOT_CHECKED%' THEN 1 ELSE 0 END),
       'COMPLETED_APFS_IMAGE_INVENTORY_COMPARISON',
       'APFS image_file_inventory source/inode/object-id, exact normalized path, then lower-confidence name/parent fallback',
       'Compared Spotlight artifacts to APFS-derived image_file_inventory. Missing rows are investigative leads only and are not deletion proof.',
       datetime('now')
FROM artifacts
WHERE source_id=)SQL" + sid + ";");
            appendEnrichmentRunStatus(db, 98, "active_comparison_image_run_summary_insert_complete", "rows_inserted=" + std::to_string(lastSqlChangeCount(db)));

            appendEnrichmentRunStatus(db, 99, "active_comparison_image_timeline_update_start", "syncing timeline existence flags");
            db.exec("UPDATE timeline_events SET existence_status=(SELECT existence_status FROM artifacts a WHERE a.artifact_id=timeline_events.artifact_id), deleted_or_orphaned_candidate=(SELECT deleted_or_orphaned_candidate FROM artifacts a WHERE a.artifact_id=timeline_events.artifact_id) WHERE source_id=" + sid + ";");
            const long long timelineRows = lastSqlChangeCount(db);
            appendEnrichmentRunStatus(db, 99, "active_comparison_image_timeline_update_complete", "timeline_updated=" + std::to_string(timelineRows));
            db.exec("DROP TABLE IF EXISTS temp_apfs_active_present;");
            db.exec("DROP TABLE IF EXISTS temp_apfs_active_matches;");
            db.exec("DROP TABLE IF EXISTS temp_apfs_active_artifacts;");
            db.exec("DROP TABLE IF EXISTS temp_apfs_active_inventory;");
            db.commit();
            appendEnrichmentRunStatus(db, 99, "active_comparison_complete", "status=COMPLETED_APFS_IMAGE_INVENTORY_COMPARISON image_inventory_rows=" + std::to_string(imageInventoryRows) + " present=" + std::to_string(presentRows) + " missing_candidates=" + std::to_string(candidateRows) + " timeline_updated=" + std::to_string(timelineRows));
            return;
        }

        if (!hasIosFfsLookup) {
            if (!source.evidenceRoot.empty()) {
                log.info("Active filesystem comparison has no validated inventory rows; evidenceRoot was supplied but no live/missing or deleted/orphaned classification will be performed in this run.");
            } else {
                log.info("Active filesystem comparison has no validated inventory rows; existence_status will remain NOT_CHECKED-style and deleted/orphaned candidates will not be generated.");
            }
            appendEnrichmentRunStatus(db, 92, "active_comparison_no_inventory_update_start", "setting NOT_CHECKED statuses");
            db.exec(R"SQL(
UPDATE artifacts
SET existence_status = CASE
        WHEN trim(COALESCE(best_path,''))='' THEN 'NO_FILESYSTEM_PATH_NOT_CHECKED'
        WHEN instr(best_path,'/')=0 AND instr(best_path,char(92))=0 AND trim(COALESCE(index_text_snippet,''))<>'' THEN 'INDEX_ONLY_NO_FILESYSTEM_PATH_NOT_CHECKED'
        ELSE 'NOT_CHECKED_NO_ACTIVE_INVENTORY'
    END,
    matched_filesystem_path = '',
    deleted_or_orphaned_candidate = 0,
    orphan_reason = ''
WHERE source_id=)SQL" + sid + ";");
            const long long artifactUpdates = lastSqlChangeCount(db);
            appendEnrichmentRunStatus(db, 98, "active_comparison_image_run_summary_insert_start", "recording APFS image comparison summary");
            db.exec(R"SQL(
INSERT INTO active_file_comparison_runs(source_id,image_inventory_available,spotlight_artifact_count,image_file_count,inode_match_count,path_match_count,missing_candidate_count,not_checked_count,run_status,comparison_basis,notes,created_utc)
SELECT )SQL" + sid + R"SQL(,
       0,
       COUNT(*),
       0,
       0,
       0,
       0,
       COUNT(*),
       'SKIPPED_NO_IOS_FFS_OR_IMAGE_INVENTORY',
       'NO_ACTIVE_INVENTORY',
       'No ios_ffs_file_inventory, ios_ffs_path_lookup, or image_file_inventory rows were available for exact active filesystem comparison.',
       datetime('now')
FROM artifacts
WHERE source_id=)SQL" + sid + ";");
            db.exec("UPDATE timeline_events SET existence_status=(SELECT existence_status FROM artifacts a WHERE a.artifact_id=timeline_events.artifact_id), deleted_or_orphaned_candidate=0 WHERE source_id=" + sid + ";");
            const long long timelineUpdates = lastSqlChangeCount(db);
            db.commit();
            appendEnrichmentRunStatus(db, 99, "active_comparison_complete", "status=SKIPPED_NO_IOS_FFS_OR_IMAGE_INVENTORY artifacts_updated=" + std::to_string(artifactUpdates) + " timeline_updated=" + std::to_string(timelineUpdates));
            return;
        }

        log.info("Active filesystem comparison enabled using exact iOS FFS path lookup: ios_ffs_file_inventory_rows=" + std::to_string(iosFullInventoryRows) + " ios_ffs_path_lookup_rows=" + std::to_string(iosPathLookupRows) + ". Missing rows are investigative leads only, not deletion proof.");
        appendEnrichmentRunStatus(db, 92, "active_comparison_ios_exact_lookup_start", "ios_ffs_file_inventory_rows=" + std::to_string(iosFullInventoryRows) + " ios_ffs_path_lookup_rows=" + std::to_string(iosPathLookupRows));

        appendEnrichmentRunStatus(db, 92, "active_comparison_ffs_temp_build_start", "building normalized FFS lookup temp table");
        db.exec("DROP TABLE IF EXISTS temp_ios_active_ffs_paths;");
        db.exec(R"SQL(
CREATE TEMP TABLE temp_ios_active_ffs_paths(
  normalized_path TEXT PRIMARY KEY,
  file_name TEXT,
  size_bytes INTEGER,
  lookup_source TEXT
);
)SQL");
        if (iosFullInventoryRows > 0) {
            db.exec(R"SQL(
INSERT OR IGNORE INTO temp_ios_active_ffs_paths(normalized_path,file_name,size_bytes,lookup_source)
SELECT lower(replace(replace(replace(replace(replace(replace(trim(normalized_path),'\','/'),'//','/'),'//','/'),'//','/'),'//','/'),'//','/')), file_name, size_bytes, 'ios_ffs_file_inventory'
FROM ios_ffs_file_inventory
WHERE source_id=)SQL" + sid + R"SQL(
  AND trim(COALESCE(normalized_path,''))<>'';
)SQL");
        } else {
            db.exec(R"SQL(
INSERT OR IGNORE INTO temp_ios_active_ffs_paths(normalized_path,file_name,size_bytes,lookup_source)
SELECT lower(replace(replace(replace(replace(replace(replace(trim(normalized_path),'\','/'),'//','/'),'//','/'),'//','/'),'//','/'),'//','/')), file_name, size_bytes, 'ios_ffs_path_lookup'
FROM ios_ffs_path_lookup
WHERE source_id=)SQL" + sid + R"SQL(
  AND trim(COALESCE(normalized_path,''))<>'';
)SQL");
        }
        db.exec("CREATE INDEX IF NOT EXISTS idx_temp_ios_active_ffs_paths_path ON temp_ios_active_ffs_paths(normalized_path);");
        const std::size_t ffsTempRows = scalarCount(db, "SELECT COUNT(*) FROM temp_ios_active_ffs_paths");
        appendEnrichmentRunStatus(db, 93, "active_comparison_ffs_temp_build_complete", "normalized_paths=" + std::to_string(ffsTempRows));

        appendEnrichmentRunStatus(db, 93, "active_comparison_artifact_path_temp_start", "building normalized artifact path temp table");
        db.exec("DROP TABLE IF EXISTS temp_ios_active_artifact_paths;");
        db.exec(R"SQL(
CREATE TEMP TABLE temp_ios_active_artifact_paths AS
SELECT artifact_id,
       lower(replace(replace(replace(replace(replace(replace(
       CASE
         WHEN trim(COALESCE(filesystem_lookup_path,'')) LIKE '/%' AND length(trim(COALESCE(filesystem_lookup_path,'')))>1 THEN trim(filesystem_lookup_path)
         WHEN trim(COALESCE(best_path,'')) LIKE '/%' AND length(trim(COALESCE(best_path,'')))>1 THEN trim(best_path)
         WHEN trim(COALESCE(normalized_mac_path,'')) LIKE '/%' AND length(trim(COALESCE(normalized_mac_path,'')))>1 THEN trim(normalized_mac_path)
         WHEN trim(COALESCE(spotlight_display_path,'')) LIKE '/%' AND length(trim(COALESCE(spotlight_display_path,'')))>1 THEN trim(spotlight_display_path)
         ELSE ''
       END,'\','/'),'//','/'),'//','/'),'//','/'),'//','/'),'//','/')) AS candidate_path
FROM artifacts
WHERE source_id=)SQL" + sid + ";");
        db.exec("CREATE INDEX IF NOT EXISTS idx_temp_ios_active_artifact_paths_id ON temp_ios_active_artifact_paths(artifact_id);");
        db.exec("CREATE INDEX IF NOT EXISTS idx_temp_ios_active_artifact_paths_path ON temp_ios_active_artifact_paths(candidate_path);");
        const std::size_t artifactPathRows = scalarCount(db, "SELECT COUNT(*) FROM temp_ios_active_artifact_paths");
        const std::size_t comparableArtifactPathRows = scalarCount(db, "SELECT COUNT(*) FROM temp_ios_active_artifact_paths WHERE candidate_path<>''");
        appendEnrichmentRunStatus(db, 94, "active_comparison_artifact_path_temp_complete", "artifact_paths=" + std::to_string(artifactPathRows) + " comparable_paths=" + std::to_string(comparableArtifactPathRows));

        appendEnrichmentRunStatus(db, 94, "active_comparison_baseline_update_start", "setting baseline existence statuses");
        db.exec(R"SQL(
UPDATE artifacts
SET filesystem_lookup_path = COALESCE((SELECT candidate_path FROM temp_ios_active_artifact_paths c WHERE c.artifact_id=artifacts.artifact_id),''),
    matched_filesystem_path = '',
    deleted_or_orphaned_candidate = 0,
    orphan_reason = '',
    existence_status = CASE
        WHEN trim(COALESCE(best_path,''))='' THEN 'NO_FILESYSTEM_PATH_NOT_CHECKED'
        WHEN instr(best_path,'/')=0 AND instr(best_path,char(92))=0 AND trim(COALESCE(index_text_snippet,''))<>'' THEN 'INDEX_ONLY_NO_FILESYSTEM_PATH_NOT_CHECKED'
        ELSE 'NOT_CHECKED_NO_COMPARABLE_IOS_PATH'
    END
WHERE source_id=)SQL" + sid + ";");
        appendEnrichmentRunStatus(db, 95, "active_comparison_baseline_update_complete", "artifacts_updated=" + std::to_string(lastSqlChangeCount(db)));

        appendEnrichmentRunStatus(db, 95, "active_comparison_exact_present_update_start", "matching comparable artifact paths to FFS lookup");
        db.exec(R"SQL(
UPDATE artifacts
SET existence_status='PRESENT_IN_IOS_FFS_EXACT_PATH',
    matched_filesystem_path=(
        SELECT p.normalized_path
        FROM temp_ios_active_artifact_paths c
        JOIN temp_ios_active_ffs_paths p ON p.normalized_path=c.candidate_path
        WHERE c.artifact_id=artifacts.artifact_id
        LIMIT 1
    ),
    deleted_or_orphaned_candidate=0,
    orphan_reason='',
    confidence='HIGH_IOS_FFS_EXACT_PATH_MATCH'
WHERE source_id=)SQL" + sid + R"SQL(
  AND EXISTS (
        SELECT 1
        FROM temp_ios_active_artifact_paths c
        JOIN temp_ios_active_ffs_paths p ON p.normalized_path=c.candidate_path
        WHERE c.artifact_id=artifacts.artifact_id
  );
)SQL");
        const long long exactPresentRows = lastSqlChangeCount(db);
        appendEnrichmentRunStatus(db, 96, "active_comparison_exact_present_update_complete", "artifacts_updated=" + std::to_string(exactPresentRows));

        appendEnrichmentRunStatus(db, 96, "active_comparison_exact_missing_update_start", "flagging exact-path absence leads");
        db.exec(R"SQL(
UPDATE artifacts
SET existence_status='MISSING_FROM_IOS_FFS_EXACT_PATH_CANDIDATE',
    matched_filesystem_path='',
    deleted_or_orphaned_candidate=1,
    orphan_reason='EXACT_SPOTLIGHT_PATH_NOT_FOUND_IN_IOS_FFS_LOOKUP; investigative lead only, not deletion proof',
    confidence='MEDIUM_IOS_FFS_PATH_ABSENT_LEAD'
WHERE source_id=)SQL" + sid + R"SQL(
  AND EXISTS (
        SELECT 1 FROM temp_ios_active_artifact_paths c
        WHERE c.artifact_id=artifacts.artifact_id
          AND c.candidate_path<>''
          AND (
               lower(c.candidate_path) LIKE '/private/%'
            OR lower(c.candidate_path) LIKE '/var/%'
            OR lower(c.candidate_path) LIKE '/system/%'
            OR lower(c.candidate_path) LIKE '/applications/%'
            OR lower(c.candidate_path) LIKE '/library/%'
          )
  )
  AND NOT EXISTS (
        SELECT 1
        FROM temp_ios_active_artifact_paths c
        JOIN temp_ios_active_ffs_paths p ON p.normalized_path=c.candidate_path
        WHERE c.artifact_id=artifacts.artifact_id
  );
)SQL");
        const long long exactMissingRows = lastSqlChangeCount(db);
        appendEnrichmentRunStatus(db, 97, "active_comparison_exact_missing_update_complete", "artifacts_updated=" + std::to_string(exactMissingRows));

        appendEnrichmentRunStatus(db, 97, "active_comparison_exact_candidate_insert_start", "materializing exact-path absence lead rows");
        db.exec(R"SQL(
INSERT INTO orphaned_deleted_candidates(source_id,artifact_id,store_guid,inode_num,file_name,best_path,content_type,existence_status,orphan_reason,index_text_snippet)
SELECT source_id, artifact_id, store_guid, inode_num, file_name, best_path,
       COALESCE(NULLIF(content_type,''),'EXACT_SPOTLIGHT_PATH_ABSENT_FROM_FFS_LOOKUP') AS content_type,
       existence_status, orphan_reason, index_text_snippet
FROM artifacts
WHERE source_id=)SQL" + sid + R"SQL(
  AND existence_status='MISSING_FROM_IOS_FFS_EXACT_PATH_CANDIDATE'
  AND deleted_or_orphaned_candidate=1;
)SQL");
        const long long exactCandidateRows = lastSqlChangeCount(db);
        appendEnrichmentRunStatus(db, 97, "active_comparison_exact_candidate_insert_complete", "rows_inserted=" + std::to_string(exactCandidateRows));

        appendEnrichmentRunStatus(db, 97, "active_comparison_reference_materialize_start", "materializing Spotlight recovered-path reference candidates");
        db.exec("DROP TABLE IF EXISTS temp_ios_active_reference_candidates;");
        db.exec(R"SQL(
CREATE TEMP TABLE temp_ios_active_reference_candidates AS
SELECT source_id,
       store_guid,
       inode_num,
       MIN(normalized_ios_path) AS normalized_ios_path,
       COALESCE(NULLIF(MIN(missing_candidate_category),''),'SPOTLIGHT_REFERENCE_ABSENT_FROM_FFS_LOOKUP') AS missing_candidate_category,
       MIN(investigative_priority) AS investigative_priority,
       MIN(investigative_reason) AS investigative_reason,
       MIN(spotlight_text_context_sample) AS spotlight_text_context_sample,
       MIN(confidence) AS reference_confidence,
       COALESCE(NULLIF(MIN(ffs_lookup_source),''),'lookup_available_no_matching_path') AS ffs_lookup_source,
       COUNT(*) AS supporting_reference_count
FROM vw_ios_spotlight_missing_from_ffs_candidates
WHERE source_id=)SQL" + sid + R"SQL(
GROUP BY source_id, store_guid, inode_num, normalized_ios_path, missing_candidate_category;
)SQL");
        db.exec("CREATE INDEX IF NOT EXISTS idx_temp_ios_active_reference_candidates_key ON temp_ios_active_reference_candidates(source_id,store_guid,inode_num,normalized_ios_path,missing_candidate_category);");
        const std::size_t referenceCandidateRows = scalarCount(db, "SELECT COUNT(*) FROM temp_ios_active_reference_candidates");
        appendEnrichmentRunStatus(db, 98, "active_comparison_reference_materialize_complete", "reference_candidates=" + std::to_string(referenceCandidateRows));

        appendEnrichmentRunStatus(db, 98, "active_comparison_reference_artifact_map_start", "building artifact id map for reference candidates");
        db.exec("DROP TABLE IF EXISTS temp_ios_active_reference_artifact_ids;");
        db.exec(R"SQL(
CREATE TEMP TABLE temp_ios_active_reference_artifact_ids AS
SELECT source_id, store_guid, COALESCE(inode_num,'') AS inode_num, MIN(artifact_id) AS artifact_id
FROM artifacts
WHERE source_id=)SQL" + sid + R"SQL(
GROUP BY source_id, store_guid, COALESCE(inode_num,'');
)SQL");
        db.exec("CREATE INDEX IF NOT EXISTS idx_temp_ios_active_reference_artifact_ids_key ON temp_ios_active_reference_artifact_ids(source_id,store_guid,inode_num);");
        appendEnrichmentRunStatus(db, 98, "active_comparison_reference_artifact_map_complete", "artifact_keys=" + std::to_string(scalarCount(db, "SELECT COUNT(*) FROM temp_ios_active_reference_artifact_ids")));

        appendEnrichmentRunStatus(db, 98, "active_comparison_reference_candidate_insert_start", "inserting reference-path absence lead rows");
        db.exec(R"SQL(
INSERT INTO orphaned_deleted_candidates(source_id,artifact_id,store_guid,inode_num,file_name,best_path,content_type,existence_status,orphan_reason,index_text_snippet)
SELECT c.source_id,
       m.artifact_id,
       c.store_guid,
       c.inode_num,
       '',
       c.normalized_ios_path,
       c.missing_candidate_category,
       'MISSING_FROM_IOS_FFS_REFERENCE_CANDIDATE',
       'Spotlight recovered an iOS file/path reference absent from the available FFS lookup; investigative lead only, not deletion proof; priority=' || COALESCE(c.investigative_priority,'') || '; confidence=' || COALESCE(c.reference_confidence,'') || '; lookup=' || COALESCE(c.ffs_lookup_source,'') || '; supporting_references=' || CAST(c.supporting_reference_count AS TEXT) || '; reason=' || COALESCE(c.investigative_reason,''),
       substr(COALESCE(c.spotlight_text_context_sample,''),1,900)
FROM temp_ios_active_reference_candidates c
LEFT JOIN temp_ios_active_reference_artifact_ids m
  ON m.source_id=c.source_id
 AND m.store_guid=c.store_guid
 AND m.inode_num=COALESCE(c.inode_num,'');
)SQL");
        const long long referenceInsertedRows = lastSqlChangeCount(db);
        appendEnrichmentRunStatus(db, 98, "active_comparison_reference_candidate_insert_complete", "rows_inserted=" + std::to_string(referenceInsertedRows));

        appendEnrichmentRunStatus(db, 98, "active_comparison_run_summary_insert_start", "recording active comparison run summary");
        db.exec(R"SQL(
INSERT INTO active_file_comparison_runs(source_id,image_inventory_available,spotlight_artifact_count,image_file_count,inode_match_count,path_match_count,missing_candidate_count,not_checked_count,run_status,comparison_basis,notes,created_utc)
SELECT )SQL" + sid + R"SQL(,
       1,
       COUNT(*),
       )SQL" + std::to_string((iosFullInventoryRows > 0) ? iosFullInventoryRows : iosPathLookupRows) + R"SQL(,
       0,
       SUM(CASE WHEN existence_status='PRESENT_IN_IOS_FFS_EXACT_PATH' THEN 1 ELSE 0 END),
       (SELECT COUNT(*) FROM orphaned_deleted_candidates od WHERE od.source_id=)SQL" + sid + R"SQL(),
       SUM(CASE WHEN existence_status NOT IN ('PRESENT_IN_IOS_FFS_EXACT_PATH','MISSING_FROM_IOS_FFS_EXACT_PATH_CANDIDATE') THEN 1 ELSE 0 END),
       'COMPLETED_IOS_FFS_EXACT_PATH_AND_REFERENCE_LOOKUP',
       'IOS_FFS_EXACT_PATH_AND_SPOTLIGHT_REFERENCE_LOOKUP',
       'Exact artifact-path comparison plus Spotlight recovered-path references checked against ios_ffs_file_inventory or ios_ffs_path_lookup. Missing rows are investigative leads only and are not deletion proof.',
       datetime('now')
FROM artifacts
WHERE source_id=)SQL" + sid + ";");
        appendEnrichmentRunStatus(db, 99, "active_comparison_run_summary_insert_complete", "rows_inserted=" + std::to_string(lastSqlChangeCount(db)));

        appendEnrichmentRunStatus(db, 99, "active_comparison_timeline_update_start", "syncing timeline existence/deleted flags");
        db.exec("UPDATE timeline_events SET existence_status=(SELECT existence_status FROM artifacts a WHERE a.artifact_id=timeline_events.artifact_id), deleted_or_orphaned_candidate=(SELECT deleted_or_orphaned_candidate FROM artifacts a WHERE a.artifact_id=timeline_events.artifact_id) WHERE source_id=" + sid + ";");
        const long long timelineRows = lastSqlChangeCount(db);
        db.exec("DROP TABLE IF EXISTS temp_ios_active_reference_artifact_ids;");
        db.exec("DROP TABLE IF EXISTS temp_ios_active_reference_candidates;");
        db.exec("DROP TABLE IF EXISTS temp_ios_active_artifact_paths;");
        db.exec("DROP TABLE IF EXISTS temp_ios_active_ffs_paths;");
        db.commit();
        appendEnrichmentRunStatus(db, 99, "active_comparison_complete", "status=COMPLETED_IOS_FFS_EXACT_PATH_AND_REFERENCE_LOOKUP present=" + std::to_string(exactPresentRows) + " exact_missing=" + std::to_string(exactMissingRows) + " exact_candidates=" + std::to_string(exactCandidateRows) + " reference_candidates=" + std::to_string(referenceCandidateRows) + " reference_inserted=" + std::to_string(referenceInsertedRows) + " timeline_updated=" + std::to_string(timelineRows));
    } catch (...) {
        db.rollbackNoThrow();
        appendEnrichmentRunStatus(db, 99, "active_comparison_failed", "rolled_back after exception; review VestigantSpotlight.log and case database state");
        throw;
    }
}


} // namespace vestigant::spotlight
