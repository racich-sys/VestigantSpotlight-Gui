#include "enrich_sql/sqlite_enrichment.h"
#include "core/logger.h"
#include "core/path_utils.h"
#include <filesystem>
#include <fstream>
#include <sstream>

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

void applyV7ImportedMetadata(CaseDatabase& db, const std::string& sourceId, Logger& log) {
    const std::string sid = sqlLiteral(sourceId);
    const std::size_t v7KvCount = scalarCount(db, "SELECT COUNT(*) FROM v7_record_key_values WHERE source_id=?", sourceId);
    const std::size_t v7DateCount = scalarCount(db, "SELECT COUNT(*) FROM v7_date_candidates WHERE source_id=?", sourceId);
    const std::size_t v7FullpathCount = scalarCount(db, "SELECT COUNT(*) FROM v7_decoder_fullpaths WHERE source_id=?", sourceId);
    if (v7KvCount == 0 && v7DateCount == 0 && v7FullpathCount == 0) return;

    log.info("Applying V7 imported metadata hydration: kv=" + std::to_string(v7KvCount) +
             " date_candidates=" + std::to_string(v7DateCount) +
             " fullpaths=" + std::to_string(v7FullpathCount));

    db.exec("DROP TABLE IF EXISTS temp_v7_core;");
    db.exec("DROP TABLE IF EXISTS temp_v7_paths;");
    db.exec("DROP TABLE IF EXISTS temp_v7_date_summary;");

    db.exec(R"SQL(
CREATE TEMP TABLE temp_v7_core AS
SELECT source_id,
       store_guid,
       inode_num,
       COALESCE(MAX(CASE WHEN source_db_role='store' AND field_name='_kMDItemFileName' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN field_name='_kMDItemFileName' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN source_db_role='store' AND field_name='kMDItemFSName' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN field_name='kMDItemFSName' THEN NULLIF(field_value,'') END)) AS file_name,
       COALESCE(MAX(CASE WHEN source_db_role='store' AND field_name='kMDItemDisplayName' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN field_name='kMDItemDisplayName' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN source_db_role='store' AND field_name='_kMDItemDisplayNameWithExtensions' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN field_name='_kMDItemDisplayNameWithExtensions' THEN NULLIF(field_value,'') END)) AS display_name,
       COALESCE(MAX(CASE WHEN source_db_role='store' AND field_name='kMDItemContentType' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN field_name='kMDItemContentType' THEN NULLIF(field_value,'') END)) AS content_type,
       COALESCE(MAX(CASE WHEN source_db_role='store' AND field_name='kMDItemContentTypeTree' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN field_name='kMDItemContentTypeTree' THEN NULLIF(field_value,'') END)) AS content_type_tree,
       COALESCE(MAX(CASE WHEN source_db_role='store' AND field_name='kMDItemWhereFroms' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN field_name='kMDItemWhereFroms' THEN NULLIF(field_value,'') END)) AS where_froms,
       COALESCE(MAX(CASE WHEN source_db_role='store' AND field_name='kMDItemAuthors' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN field_name='kMDItemAuthors' THEN NULLIF(field_value,'') END)) AS authors,
       COALESCE(MAX(CASE WHEN source_db_role='store' AND field_name='kMDItemCreator' THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN field_name='kMDItemCreator' THEN NULLIF(field_value,'') END)) AS creator,
       COALESCE(MAX(CASE WHEN source_db_role='store' AND field_name IN ('kMDItemLogicalSize','_kMDItemLogicalSize','kMDItemFSSize','kMDItemFileSize') THEN NULLIF(field_value,'') END),
                MAX(CASE WHEN field_name IN ('kMDItemLogicalSize','_kMDItemLogicalSize','kMDItemFSSize','kMDItemFileSize') THEN NULLIF(field_value,'') END)) AS logical_size_bytes,
       COALESCE(MAX(CASE WHEN source_db_role='store' AND field_name IN ('kMDItemPhysicalSize','_kMDItemPhysicalSize') THEN NULLIF(field_value,'') END),
)SQL" R"SQL(                MAX(CASE WHEN field_name IN ('kMDItemPhysicalSize','_kMDItemPhysicalSize') THEN NULLIF(field_value,'') END)) AS physical_size_bytes,
       COALESCE(MAX(CASE WHEN source_db_role='store' AND lower(field_name) IN ('kmditemtextcontent','kmditemdescription') THEN substr(NULLIF(field_value,''),1,500) END),
                MAX(CASE WHEN lower(field_name) IN ('kmditemtextcontent','kmditemdescription') THEN substr(NULLIF(field_value,''),1,500) END)) AS index_text_snippet
FROM v7_record_key_values
WHERE source_id=)SQL" + sid + R"SQL(
  AND field_name IN (
    '_kMDItemFileName','kMDItemFSName','kMDItemDisplayName','_kMDItemDisplayNameWithExtensions',
    'kMDItemContentType','kMDItemContentTypeTree','kMDItemWhereFroms','kMDItemAuthors','kMDItemCreator',
    'kMDItemLogicalSize','_kMDItemLogicalSize','kMDItemFSSize','kMDItemFileSize','kMDItemPhysicalSize','_kMDItemPhysicalSize',
    'kMDItemTextContent','kMDItemDescription'
  )
GROUP BY source_id, store_guid, inode_num;
)SQL");

    db.exec(R"SQL(
CREATE TEMP TABLE temp_v7_paths AS
WITH picked AS (
  SELECT source_id, store_guid, inode_num,
         COALESCE(MAX(CASE WHEN source_db_role='store' THEN NULLIF(full_path,'') END),
                  MAX(NULLIF(full_path,''))) AS full_path
  FROM v7_decoder_fullpaths
  WHERE source_id=)SQL" + sid + R"SQL(
GROUP BY source_id, store_guid, inode_num
)
SELECT source_id, store_guid, inode_num, full_path,
       CASE WHEN full_path LIKE '..NOT-FOUND../' || '%' THEN substr(full_path, 15) ELSE full_path END AS display_path,
       CASE WHEN full_path LIKE '..NOT-FOUND../' || '%' THEN '/' || substr(full_path, 15) WHEN substr(full_path,1,1)='/' THEN full_path ELSE full_path END AS normalized_path,
       CASE WHEN full_path LIKE '..NOT-FOUND../' || '%' THEN 'PATH_FROM_V7_UNRESOLVED_PARENT' WHEN COALESCE(full_path,'')='' OR full_path='/' THEN 'NO_USABLE_PATH' ELSE 'PATH_FROM_V7_FULLPATHS' END AS path_status
FROM picked;
)SQL");

    db.exec(R"SQL(
CREATE TEMP TABLE temp_v7_date_summary AS
SELECT source_id,
       store_guid,
       inode_num,
       MIN(CASE WHEN lower(field_name) LIKE '%useddates%' THEN COALESCE(NULLIF(parsed_utc,''), NULLIF(field_value,'')) END) AS first_used_date_utc,
       MAX(CASE WHEN lower(field_name) LIKE '%useddates%' OR lower(field_name) LIKE '%lastuseddate%' THEN COALESCE(NULLIF(parsed_utc,''), NULLIF(field_value,'')) END) AS last_used_date_utc,
       GROUP_CONCAT(CASE WHEN lower(field_name) LIKE '%useddates%' THEN COALESCE(NULLIF(parsed_utc,''), NULLIF(field_value,'')) END, '; ') AS used_dates_utc,
       COUNT(CASE WHEN lower(field_name) LIKE '%useddates%' THEN 1 END) AS used_dates_count,
       MIN(CASE WHEN lower(field_name) LIKE '%downloadeddate%' THEN COALESCE(NULLIF(parsed_utc,''), NULLIF(field_value,'')) END) AS downloaded_date_utc
FROM v7_date_candidates
WHERE source_id=)SQL" + sid + R"SQL(
  AND (lower(field_name) LIKE '%useddates%' OR lower(field_name) LIKE '%lastuseddate%' OR lower(field_name) LIKE '%downloadeddate%')
GROUP BY source_id, store_guid, inode_num;
)SQL");

    db.exec("CREATE INDEX IF NOT EXISTS temp_idx_v7_core ON temp_v7_core(source_id,store_guid,inode_num);");
    db.exec("CREATE INDEX IF NOT EXISTS temp_idx_v7_paths ON temp_v7_paths(source_id,store_guid,inode_num);");
    db.exec("CREATE INDEX IF NOT EXISTS temp_idx_v7_dates ON temp_v7_date_summary(source_id,store_guid,inode_num);");

    db.exec(R"SQL(
UPDATE artifacts
SET file_name = COALESCE((SELECT c.file_name FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), NULLIF(file_name,''), '------NONAME------'),
    display_name = COALESCE((SELECT c.display_name FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), NULLIF(display_name,''), (SELECT c.file_name FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), file_name),
    best_path = COALESCE((SELECT p.display_path FROM temp_v7_paths p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num), NULLIF(NULLIF(best_path,''),'/'), (SELECT c.file_name FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), best_path),
    v7_full_path_raw = COALESCE((SELECT p.full_path FROM temp_v7_paths p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num), v7_full_path_raw),
    spotlight_display_path = COALESCE((SELECT p.display_path FROM temp_v7_paths p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num), spotlight_display_path),
    normalized_mac_path = COALESCE((SELECT p.normalized_path FROM temp_v7_paths p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num), normalized_mac_path),
    filesystem_lookup_path = COALESCE((SELECT p.normalized_path FROM temp_v7_paths p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num), filesystem_lookup_path),
    path_source = CASE WHEN EXISTS (SELECT 1 FROM temp_v7_paths p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num) THEN 'V7_FULLPATHS' ELSE COALESCE(path_source, 'RAW_RECORD') END,
    path_status = COALESCE((SELECT p.path_status FROM temp_v7_paths p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num), path_status),
    content_type = COALESCE((SELECT c.content_type FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), NULLIF(content_type,'')),
    content_type_tree = COALESCE((SELECT c.content_type_tree FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), NULLIF(content_type_tree,'')),
)SQL" R"SQL(    where_froms = COALESCE((SELECT c.where_froms FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), NULLIF(where_froms,'')),
    authors = COALESCE((SELECT c.authors FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), NULLIF(authors,'')),
    creator = COALESCE((SELECT c.creator FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), NULLIF(creator,'')),
    logical_size_bytes = COALESCE((SELECT c.logical_size_bytes FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), NULLIF(logical_size_bytes,'')),
    physical_size_bytes = COALESCE((SELECT c.physical_size_bytes FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), NULLIF(physical_size_bytes,'')),
    index_text_snippet = COALESCE((SELECT c.index_text_snippet FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num), NULLIF(index_text_snippet,'')),
    used_dates_utc = COALESCE((SELECT d.used_dates_utc FROM temp_v7_date_summary d WHERE d.source_id=artifacts.source_id AND d.store_guid=artifacts.store_guid AND d.inode_num=artifacts.inode_num), NULLIF(used_dates_utc,'')),
    first_used_candidate_utc = COALESCE((SELECT d.first_used_date_utc FROM temp_v7_date_summary d WHERE d.source_id=artifacts.source_id AND d.store_guid=artifacts.store_guid AND d.inode_num=artifacts.inode_num), first_used_candidate_utc),
    used_dates_count = COALESCE((SELECT d.used_dates_count FROM temp_v7_date_summary d WHERE d.source_id=artifacts.source_id AND d.store_guid=artifacts.store_guid AND d.inode_num=artifacts.inode_num), used_dates_count),
    last_used_date_utc = COALESCE((SELECT d.last_used_date_utc FROM temp_v7_date_summary d WHERE d.source_id=artifacts.source_id AND d.store_guid=artifacts.store_guid AND d.inode_num=artifacts.inode_num), NULLIF(last_used_date_utc,'')),
    downloaded_date_utc = COALESCE((SELECT d.downloaded_date_utc FROM temp_v7_date_summary d WHERE d.source_id=artifacts.source_id AND d.store_guid=artifacts.store_guid AND d.inode_num=artifacts.inode_num), NULLIF(downloaded_date_utc,'')),
)SQL" R"SQL(    confidence = CASE WHEN EXISTS (SELECT 1 FROM temp_v7_core c WHERE c.source_id=artifacts.source_id AND c.store_guid=artifacts.store_guid AND c.inode_num=artifacts.inode_num) OR EXISTS (SELECT 1 FROM temp_v7_paths p WHERE p.source_id=artifacts.source_id AND p.store_guid=artifacts.store_guid AND p.inode_num=artifacts.inode_num) THEN 'HIGH_WITH_V7_IMPORTED_METADATA' ELSE confidence END
WHERE source_id=)SQL" + sid + ";");

    db.exec(R"SQL(
UPDATE artifact_source_instances
SET file_name = COALESCE((SELECT a.file_name FROM artifacts a WHERE a.artifact_id=artifact_source_instances.artifact_id), file_name),
    best_path = COALESCE((SELECT a.best_path FROM artifacts a WHERE a.artifact_id=artifact_source_instances.artifact_id), best_path)
WHERE source_id=)SQL" + sid + ";");

    db.exec(R"SQL(
INSERT INTO usage_evidence(artifact_id,source_id,store_guid,inode_num,field_name,field_value,parsed_utc)
SELECT DISTINCT a.artifact_id, d.source_id, d.store_guid, d.inode_num, d.field_name, d.field_value,
       COALESCE(NULLIF(d.parsed_utc,''), NULLIF(d.field_value,''))
FROM v7_date_candidates d
JOIN artifacts a ON a.source_id=d.source_id AND a.store_guid=d.store_guid AND a.inode_num=d.inode_num
WHERE d.source_id=)SQL" + sid + R"SQL(
  AND (lower(d.field_name) LIKE '%useddates%' OR lower(d.field_name) LIKE '%lastuseddate%')
  AND COALESCE(NULLIF(d.parsed_utc,''), NULLIF(d.field_value,'')) IS NOT NULL;
)SQL");

    db.exec(R"SQL(
INSERT INTO usage_evidence(artifact_id,source_id,store_guid,inode_num,field_name,field_value,parsed_utc)
SELECT DISTINCT a.artifact_id, kv.source_id, kv.store_guid, kv.inode_num, kv.field_name, kv.field_value, ''
FROM v7_record_key_values kv
JOIN artifacts a ON a.source_id=kv.source_id AND a.store_guid=kv.store_guid AND a.inode_num=kv.inode_num
WHERE kv.source_id=)SQL" + sid + R"SQL(
  AND lower(kv.field_name) IN ('kmditemusecount','kmditemusedcount','usecount')
  AND COALESCE(NULLIF(kv.field_value,''),'')<>'';
)SQL");

    db.exec(R"SQL(
UPDATE artifacts
SET use_count_value = COALESCE((
        SELECT kv.field_value
        FROM v7_record_key_values kv
        WHERE kv.source_id=artifacts.source_id
          AND kv.store_guid=artifacts.store_guid
          AND kv.inode_num=artifacts.inode_num
          AND lower(kv.field_name) IN ('kmditemusecount','kmditemusedcount','usecount')
          AND COALESCE(NULLIF(kv.field_value,''),'')<>''
        LIMIT 1
    ), use_count_value),
    usage_field_summary = COALESCE((
        SELECT group_concat(u.field_name || '=' || COALESCE(NULLIF(u.parsed_utc,''), u.field_value), '; ')
        FROM usage_evidence u
        WHERE u.artifact_id=artifacts.artifact_id
    ), usage_field_summary)
WHERE source_id=)SQL" + sid + ";");

    db.exec(R"SQL(
UPDATE artifacts
SET open_count_estimate = COALESCE(NULLIF(CAST(use_count_value AS INTEGER),0), NULLIF(used_dates_count,0), NULLIF((SELECT COUNT(*) FROM usage_evidence u WHERE u.artifact_id=artifacts.artifact_id),0), open_count_estimate)
WHERE source_id=)SQL" + sid + ";");

    db.exec(R"SQL(
INSERT INTO timeline_events(artifact_id,source_id,store_guid,inode_num,event_timestamp_utc,event_type,event_source_field,file_name,path,existence_status,deleted_or_orphaned_candidate)
SELECT a.artifact_id,
       d.source_id,
       d.store_guid,
       d.inode_num,
       d.parsed_utc,
       CASE
         WHEN lower(d.field_name) LIKE '%used%' THEN 'USAGE'
         WHEN lower(d.field_name) LIKE '%download%' THEN 'DOWNLOADED'
         WHEN lower(d.field_name) LIKE '%creation%' OR lower(d.field_name) LIKE '%created%' THEN 'CREATED'
         WHEN lower(d.field_name) LIKE '%modification%' OR lower(d.field_name) LIKE '%modified%' THEN 'MODIFIED'
         ELSE 'V7_DATE_FIELD'
       END,
       'V7:' || d.field_name,
       a.file_name,
       a.best_path,
       'NOT_CHECKED',
       0
FROM v7_date_candidates d
JOIN artifacts a ON a.source_id=d.source_id AND a.store_guid=d.store_guid AND a.inode_num=d.inode_num
WHERE d.source_id=)SQL" + sid + R"SQL(
  AND COALESCE(NULLIF(d.parsed_utc,''),'') <> ''
  AND (
       lower(d.field_name) LIKE '%used%'
    OR lower(d.field_name) LIKE '%download%'
    OR lower(d.field_name) LIKE '%creation%'
    OR lower(d.field_name) LIKE '%created%'
    OR lower(d.field_name) LIKE '%modification%'
    OR lower(d.field_name) LIKE '%modified%'
  );
)SQL");

    const std::size_t hydratedArtifacts = scalarCount(db, "SELECT COUNT(*) FROM artifacts WHERE source_id=? AND confidence='HIGH_WITH_V7_IMPORTED_METADATA'", sourceId);
    const std::size_t usageRows = scalarCount(db, "SELECT COUNT(*) FROM usage_evidence WHERE source_id=?", sourceId);
    log.info("V7 metadata hydration complete: hydrated_artifacts=" + std::to_string(hydratedArtifacts) + " usage_rows=" + std::to_string(usageRows));
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

        db.exec("ANALYZE;");

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
       r.file_name,
       r.display_name,
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
         ELSE 'RAW_RECORD_NO_PATH'
       END,
       CASE
         WHEN r.usable_full_path='/' THEN 'ROOT_OR_VOLUME_PATH'
         WHEN r.usable_full_path<>'' AND instr(r.usable_full_path,'/')>0 THEN 'RAW_PATH_PRESENT'
         WHEN r.usable_full_path<>'' THEN 'RAW_PATH_NO_DIRECTORY_CONTEXT'
         WHEN r.valid_file_name<>'' AND COALESCE(NULLIF(r.parent_inode_num,''),'0') NOT IN ('0','') THEN 'FILE_NAME_ONLY_PARENT_RECONSTRUCTION_PENDING'
         WHEN r.valid_file_name<>'' THEN 'FILE_NAME_ONLY_NO_PARENT_CONTEXT'
         WHEN COALESCE(NULLIF(trim(r.full_path),''),'')='/' AND COALESCE(NULLIF(trim(r.parent_inode_num),''),'0') NOT IN ('0','') THEN 'HEADER_ONLY_NO_USABLE_PATH'
         ELSE 'NO_USABLE_PATH'
       END,
       r.content_type,
       r.content_type_tree,
       r.where_froms,
)SQL" R"SQL(       r.logical_size_bytes,
       r.physical_size_bytes,
       r.last_updated_utc,
       CASE
         WHEN r.usable_full_path<>'' OR r.valid_file_name<>'' THEN 'MEDIUM'
         ELSE 'LOW_HEADER_ONLY_PATH_CONTEXT'
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
        log.info("Building parent inode relationship links and same-folder group counts.");
        appendEnrichmentRunStatus(db, 80, "enrichment_parent_inode_links_start", "source_id=" + source.sourceId);
        db.exec(R"SQL(
WITH sibling_counts AS (
  SELECT source_id, store_guid, parent_inode_num, COUNT(*) AS sibling_count
  FROM artifacts
  WHERE source_id=)SQL" + sid + R"SQL(
    AND COALESCE(NULLIF(parent_inode_num,''),'0') NOT IN ('0','')
  GROUP BY source_id, store_guid, parent_inode_num
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
           WHEN COALESCE(NULLIF(trim(c.file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN trim(c.file_name)
           ELSE ''
         END AS child_valid_name,
         CASE
           WHEN COALESCE(NULLIF(trim(p.file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN trim(p.file_name)
           ELSE ''
         END AS parent_valid_name,
         CASE
           WHEN COALESCE(NULLIF(trim(p.best_path),''),'') <> '' THEN trim(p.best_path)
           ELSE ''
         END AS parent_valid_path
  FROM artifacts c
  LEFT JOIN artifacts p
    ON p.source_id=c.source_id
   AND p.store_guid=c.store_guid
   AND p.inode_num=c.parent_inode_num
  LEFT JOIN sibling_counts sc
    ON sc.source_id=c.source_id
   AND sc.store_guid=c.store_guid
   AND sc.parent_inode_num=c.parent_inode_num
  WHERE c.source_id=)SQL" + sid + R"SQL(
    AND COALESCE(NULLIF(c.parent_inode_num,''),'0') NOT IN ('0','')
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
         WHEN sibling_count > 1 THEN 'LOW_SAME_FOLDER_GROUP_BY_PARENT_INODE'
         ELSE 'LOW_PARENT_INODE_UNRESOLVED'
       END AS confidence
FROM child_parent;
)SQL");

        const std::size_t parentLinkRows = scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=?", source.sourceId);
        const std::size_t reconstructedPathRows = scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND COALESCE(reconstructed_path_candidate,'')<>''", source.sourceId);
        const std::size_t parentMatchedRows = scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE'", source.sourceId);
        const std::size_t childNameRows = scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.')", source.sourceId);
        appendEnrichmentRunStatus(db, 81, "enrichment_parent_inode_links_complete", "links=" + std::to_string(parentLinkRows) + " matched=" + std::to_string(parentMatchedRows) + " child_names=" + std::to_string(childNameRows) + " reconstructed_paths=" + std::to_string(reconstructedPathRows));

        log.info("Applying parent-inode reconstructed paths to weak artifact path rows.");
        appendEnrichmentRunStatus(db, 82, "enrichment_parent_inode_path_apply_start", "source_id=" + source.sourceId);
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
    path_source = 'PARENT_INODE_RECONSTRUCTION',
    path_status = 'RECONSTRUCTED_FROM_PARENT_INODE',
    confidence = CASE
      WHEN COALESCE(confidence,'') LIKE 'HIGH%' THEN confidence
      ELSE 'MEDIUM_PARENT_INODE_RECONSTRUCTED_PATH'
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
        const std::size_t appliedPathRows = scalarCount(db, "SELECT COUNT(*) FROM artifacts WHERE source_id=? AND path_source='PARENT_INODE_RECONSTRUCTION'", source.sourceId);
        appendEnrichmentRunStatus(db, 83, "enrichment_parent_inode_path_apply_complete", "artifacts_updated=" + std::to_string(appliedPathRows));

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

        log.info("Legacy V7 metadata hydration is disabled for normal native-first workflows; legacy V7 tables are retained only for explicitly requested comparison diagnostics.");

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
        log.info("Building timeline_events with set-based SQLite insert.");
        db.exec(R"SQL(
INSERT INTO timeline_events(artifact_id,source_id,store_guid,inode_num,event_timestamp_utc,event_type,event_source_field,file_name,path,existence_status,deleted_or_orphaned_candidate)
SELECT a.artifact_id,
       d.source_id,
       d.store_guid,
       d.inode_num,
       d.parsed_utc,
       CASE
         WHEN lower(d.field_name) LIKE '%used%' THEN 'USAGE'
         WHEN lower(d.field_name) LIKE '%download%' THEN 'DOWNLOADED'
         WHEN lower(d.field_name) LIKE '%creation%' OR lower(d.field_name) LIKE '%created%' THEN 'CREATED'
         WHEN lower(d.field_name) LIKE '%modification%' OR lower(d.field_name) LIKE '%modified%' THEN 'MODIFIED'
         WHEN lower(d.field_name) LIKE '%updated%' THEN 'UPDATED'
         ELSE 'DATE_FIELD'
       END,
       d.field_name,
       a.file_name,
       a.best_path,
       'NOT_CHECKED',
       0
FROM raw_date_candidates d
LEFT JOIN artifacts a ON a.source_id=d.source_id AND a.store_guid=d.store_guid AND a.inode_num=d.inode_num
WHERE d.source_id=)SQL" + sid + R"SQL( AND COALESCE(NULLIF(d.parsed_utc,''),'') <> '';
)SQL");

        insertMetric(db, source.sourceId, "raw_records", std::to_string(rawRecordCount));
        insertMetric(db, source.sourceId, "raw_key_values", std::to_string(kvCount));
        insertMetric(db, source.sourceId, "raw_date_candidates", std::to_string(dateCount));
        insertMetric(db, source.sourceId, "date_field_name_count", std::to_string(scalarCount(db, "SELECT COUNT(DISTINCT field_name) FROM raw_date_candidates WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "date_candidates_artifact_matched", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM raw_date_candidates WHERE source_id=? AND association_status='ARTIFACT_MATCHED_BY_SOURCE_STORE_INODE'", source.sourceId)));
        insertMetric(db, source.sourceId, "date_candidates_artifact_unmatched", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM raw_date_candidates WHERE source_id=? AND COALESCE(association_status,'')<>'ARTIFACT_MATCHED_BY_SOURCE_STORE_INODE'", source.sourceId)));
        insertMetric(db, source.sourceId, "artifact_date_summary_rows", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM artifact_date_summary WHERE source_id=?", source.sourceId)));
        insertMetric(db, source.sourceId, "date_candidate_identity_key", "source_id|store_guid|source_db|inode_num|store_id");
        insertMetric(db, source.sourceId, "records_with_0_dates", std::to_string(scalarCount(db, R"SQL(
SELECT COUNT(*)
FROM raw_records r
WHERE r.source_id=?
  AND NOT EXISTS (
    SELECT 1 FROM raw_date_candidates d
    WHERE d.source_id=r.source_id AND d.store_guid=r.store_guid AND d.source_db=r.source_db AND d.inode_num=r.inode_num AND d.store_id=r.store_id
  )
)SQL", source.sourceId)));
        insertMetric(db, source.sourceId, "records_with_1_date", std::to_string(scalarCount(db, R"SQL(
SELECT COUNT(*) FROM (
  SELECT r.raw_record_id, COUNT(d.raw_date_id) AS c
  FROM raw_records r
  LEFT JOIN raw_date_candidates d ON d.source_id=r.source_id AND d.store_guid=r.store_guid AND d.source_db=r.source_db AND d.inode_num=r.inode_num AND d.store_id=r.store_id
  WHERE r.source_id=?
  GROUP BY r.raw_record_id
  HAVING c=1
)
)SQL", source.sourceId)));
        insertMetric(db, source.sourceId, "records_with_2plus_dates", std::to_string(scalarCount(db, R"SQL(
SELECT COUNT(*) FROM (
  SELECT r.raw_record_id, COUNT(d.raw_date_id) AS c
  FROM raw_records r
  LEFT JOIN raw_date_candidates d ON d.source_id=r.source_id AND d.store_guid=r.store_guid AND d.source_db=r.source_db AND d.inode_num=r.inode_num AND d.store_id=r.store_id
  WHERE r.source_id=?
  GROUP BY r.raw_record_id
  HAVING c>=2
)
)SQL", source.sourceId)));
        insertMetric(db, source.sourceId, "max_dates_per_record", std::to_string(scalarCount(db, R"SQL(
SELECT COALESCE(MAX(c),0) FROM (
  SELECT r.raw_record_id, COUNT(d.raw_date_id) AS c
  FROM raw_records r
  LEFT JOIN raw_date_candidates d ON d.source_id=r.source_id AND d.store_guid=r.store_guid AND d.source_db=r.source_db AND d.inode_num=r.inode_num AND d.store_id=r.store_id
  WHERE r.source_id=?
  GROUP BY r.raw_record_id
)
)SQL", source.sourceId)));
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
        insertMetric(db, source.sourceId, "parent_inode_links_with_reconstructed_path", std::to_string(scalarCount(db, "SELECT COUNT(*) FROM parent_inode_links WHERE source_id=? AND COALESCE(reconstructed_path_candidate,'')<>''", source.sourceId)));
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
    if (!source.evidenceRoot.empty()) {
        log.info("Active filesystem comparison is tabled in v0.6.4; evidenceRoot was supplied but no live/missing or deleted/orphaned classification will be performed.");
    } else {
        log.info("Active filesystem comparison is tabled in v0.6.4; existence_status will remain NOT_CHECKED-style and deleted/orphaned candidates will not be generated.");
    }

    db.begin();
    try {
        db.exec(R"SQL(
UPDATE artifacts
SET existence_status = CASE
        WHEN trim(COALESCE(best_path,''))='' THEN 'NO_FILESYSTEM_PATH_NOT_CHECKED'
        WHEN instr(best_path,'/')=0 AND instr(best_path,char(92))=0 AND trim(COALESCE(index_text_snippet,''))<>'' THEN 'INDEX_ONLY_NO_FILESYSTEM_PATH_NOT_CHECKED'
        ELSE 'NOT_CHECKED'
    END,
    matched_filesystem_path = '',
    deleted_or_orphaned_candidate = 0,
    orphan_reason = ''
WHERE source_id=)SQL" + sid + ";");
        db.exec("DELETE FROM orphaned_deleted_candidates WHERE source_id=" + sid + ";");
        db.exec("UPDATE timeline_events SET existence_status=(SELECT existence_status FROM artifacts a WHERE a.artifact_id=timeline_events.artifact_id), deleted_or_orphaned_candidate=0 WHERE source_id=" + sid + ";");
        db.commit();
    } catch (...) { db.rollbackNoThrow(); throw; }
}

} // namespace vestigant::spotlight
