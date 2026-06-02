#include "export_sql/sqlite_exporter.h"
#include "core/csv.h"
#include "core/logger.h"
#include "core/path_utils.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <chrono>

namespace vestigant::spotlight {
namespace {
constexpr std::size_t DefaultExportChunkRows = 250000;
constexpr std::size_t ExportProgressRowInterval = 250000;

bool tableExists(CaseDatabase& db, const std::string& tableName);

std::string exportStatusClean(std::string s) {
    for (char& ch : s) {
        if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    }
    if (s.size() > 900) s = s.substr(0, 900) + "...";
    return s;
}

fs::path inferCaseDirFromExportPath(fs::path p) {
    if (p.has_filename()) p = p.parent_path();
    while (!p.empty()) {
        if (p.filename().string() == "exports") return p.parent_path();
        p = p.parent_path();
    }
    return {};
}

void appendExportRunStatus(const fs::path& exportRelatedPath, int percent, const std::string& stage, const std::string& message = {}) {
    const fs::path caseDir = inferCaseDirFromExportPath(exportRelatedPath);
    if (caseDir.empty()) return;
    try {
        fs::create_directories(caseDir / "logs");
        const std::string ts = nowUtc();
        const std::string cleanStage = exportStatusClean(stage);
        const std::string cleanMessage = exportStatusClean(message);
        auto writeProgress = [&](const fs::path& path, std::ios::openmode mode) {
            std::ofstream out(path, mode | std::ios::binary);
            out << ts << "\t" << percent << "\t" << cleanStage << "\t" << cleanMessage << "\n" << std::flush;
        };
        auto writeStatus = [&](const fs::path& path) {
            std::ofstream out(path, std::ios::app | std::ios::binary);
            out << ts << " stage=" << cleanStage;
            if (!cleanMessage.empty()) out << " message=" << cleanMessage;
            out << "\n" << std::flush;
        };
        auto writeLast = [&](const fs::path& path) {
            std::ofstream out(path, std::ios::binary);
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

std::string partName(const fs::path& file, std::size_t part) {
    std::ostringstream os;
    os << file.stem().string() << "_part_" << std::setw(4) << std::setfill('0') << part << file.extension().string();
    return os.str();
}

fs::path ioPath(const fs::path& p) {
#if defined(_WIN32)
    std::error_code ec;
    fs::path abs = fs::absolute(p, ec);
    if (ec) abs = p;
    std::wstring w = abs.wstring();
    if (w.rfind(LR"(\\?\)", 0) == 0) return abs;
    if (w.rfind(LR"(\\)", 0) == 0) return fs::path(LR"(\\?\UNC\)" + w.substr(2));
    return fs::path(LR"(\\?\)" + w);
#else
    return p;
#endif
}

void writeHeader(std::ofstream& out, SqlStatement& stmt) {
    const int n = sqlite3_column_count(stmt.raw());
    for (int i=0; i<n; ++i) { if (i) out << ','; out << csvEscape(sqlite3_column_name(stmt.raw(), i) ? sqlite3_column_name(stmt.raw(), i) : ""); }
    out << "\n";
}

void writeRow(std::ofstream& out, SqlStatement& stmt) {
    const int n = sqlite3_column_count(stmt.raw());
    for (int i=0; i<n; ++i) { if (i) out << ','; out << csvEscape(stmt.colText(i)); }
    out << "\n";
}

std::vector<std::string> splitSimpleCsv(const std::string& line) {
    std::vector<std::string> parts;
    std::string cur;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') { cur.push_back('"'); ++i; }
            else inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            parts.push_back(cur); cur.clear();
        } else cur.push_back(c);
    }
    parts.push_back(cur);
    return parts;
}


long long scalarInt64(CaseDatabase& db, const std::string& sql) {
    auto stmt = db.prepare(sql);
    if (stmt.stepRow()) return stmt.colInt64(0);
    return 0;
}

std::string scalarText(CaseDatabase& db, const std::string& sql) {
    auto stmt = db.prepare(sql);
    if (stmt.stepRow()) return stmt.colText(0);
    return std::string();
}

std::string caseInfoValue(CaseDatabase& db, const std::string& key, const std::string& fallback);

std::string htmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 && c != '\t' && c != '\n' && c != '\r') out += ' ';
                else out.push_back(c);
        }
    }
    return out;
}

void writeHtmlQueryTable(CaseDatabase& db, std::ofstream& out, const std::string& title, const std::string& description, const std::string& sql) {
    out << "<section class=\"panel\"><h2>" << htmlEscape(title) << "</h2>";
    if (!description.empty()) out << "<p>" << htmlEscape(description) << "</p>";
    try {
        auto stmt = db.prepare(sql);
        const int n = sqlite3_column_count(stmt.raw());
        out << "<div class=\"tablewrap\"><table><thead><tr>";
        for (int i = 0; i < n; ++i) {
            const char* name = sqlite3_column_name(stmt.raw(), i);
            out << "<th>" << htmlEscape(name ? name : "") << "</th>";
        }
        out << "</tr></thead><tbody>";
        std::size_t rows = 0;
        while (stmt.stepRow()) {
            out << "<tr>";
            for (int i = 0; i < n; ++i) out << "<td>" << htmlEscape(stmt.colText(i)) << "</td>";
            out << "</tr>";
            ++rows;
        }
        if (rows == 0) out << "<tr><td colspan=\"" << n << "\">No rows returned.</td></tr>";
        out << "</tbody></table></div>";
    } catch (const std::exception& ex) {
        out << "<p class=\"warn\">Table generation failed: " << htmlEscape(ex.what()) << "</p>";
    } catch (...) {
        out << "<p class=\"warn\">Table generation failed with an unknown error.</p>";
    }
    out << "</section>\n";
}

void writeInvestigatorDashboard(CaseDatabase& db, const fs::path& file, Logger& log) {
    fs::create_directories(ioPath(file.parent_path()));
    std::ofstream out(ioPath(file), std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write investigator dashboard: " + pathString(file));
    const auto appVersion = scalarText(db, "SELECT COALESCE((SELECT value FROM case_info WHERE key='app_version'),'')");
    const auto inputPath = scalarText(db, "SELECT COALESCE((SELECT input_path FROM evidence_sources ORDER BY added_utc LIMIT 1),'')");
    out << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Vestigant Spotlight Investigator Dashboard</title>";
    out << "<style>body{font-family:Segoe UI,Arial,sans-serif;margin:24px;line-height:1.35;background:#fafafa;color:#222} h1{margin-bottom:4px}.sub{color:#555;margin-top:0}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin:16px 0}.metric{background:#fff;border:1px solid #ddd;border-radius:8px;padding:12px}.metric b{display:block;font-size:22px;margin-top:4px}.panel{background:#fff;border:1px solid #ddd;border-radius:8px;padding:14px;margin:16px 0}.tablewrap{overflow:auto;max-height:520px;border:1px solid #e3e3e3}.warn{background:#fff1f1;border:1px solid #d88;padding:8px}table{border-collapse:collapse;width:100%;font-size:12px}td,th{border:1px solid #ddd;padding:5px 7px;vertical-align:top}th{background:#f1f1f1;position:sticky;top:0}code{background:#eee;padding:2px 4px}.note{background:#eef7ff;border:1px solid #8dbbe8;padding:10px;margin:12px 0}</style>";
    out << "</head><body><h1>Vestigant Spotlight Investigator Dashboard</h1>";
    out << "<p class=\"sub\">Generated UTC: " << htmlEscape(nowUtc()) << " | Application version: " << htmlEscape(appVersion) << "</p>";
    out << "<div class=\"note\"><b>Purpose:</b> bounded, investigator-facing HTML review of the local SQLite case. This dashboard intentionally shows limited pivots and samples; the full case database remains local.</div>";
    out << "<p><b>Input evidence path:</b> <code>" << htmlEscape(inputPath) << "</code></p><div class=\"grid\">";
    auto metric = [&](const char* label, long long value) { out << "<div class=\"metric\">" << htmlEscape(label) << "<b>" << value << "</b></div>"; };
    metric("Artifacts", scalarInt64(db, "SELECT COUNT(*) FROM artifacts"));
    metric("Native key/value rows", scalarInt64(db, "SELECT COUNT(*) FROM raw_key_values"));
    metric("Date candidates", scalarInt64(db, "SELECT COUNT(*) FROM raw_date_candidates"));
    metric("Timeline events", scalarInt64(db, "SELECT COUNT(*) FROM timeline_events"));
    metric("Usage evidence rows", scalarInt64(db, "SELECT COUNT(*) FROM usage_evidence"));
    metric("Usage-linked artifacts", scalarInt64(db, "SELECT COUNT(DISTINCT artifact_id) FROM usage_evidence"));
    metric("Object usage rows", scalarInt64(db, "SELECT COUNT(*) FROM vw_object_usage_summary"));
    metric("Object date summary rows", scalarInt64(db, "SELECT COUNT(*) FROM vw_object_date_summary WHERE total_date_count>0"));
    metric("Path candidates", scalarInt64(db, "SELECT COUNT(*) FROM parent_inode_links WHERE COALESCE(reconstructed_path_candidate,'')<>''"));
    metric("Native decode errors", scalarInt64(db, "SELECT COUNT(*) FROM raw_failures"));
    out << "</div><p><a href=\"review_index.html\">Local review index</a> | <a href=\"CASE_REVIEW_SUMMARY.txt\">Case summary</a> | <a href=\"TARGETED_EXPORT_README.txt\">Targeted export instructions</a></p>";
    writeHtmlQueryTable(db, out, "Object-centric usage summary", "One row per usage-bearing artifact/object with filename/path, size, use count, last used date, fused usage dates, source fields, and supporting metadata. Limited to 150 rows.", R"SQL(SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,logical_size_bytes,physical_size_bytes,use_count_value,open_count_estimate,used_dates_count,usage_earliest_utc,usage_latest_utc,fused_usage_dates_utc,usage_source_fields,object_usage_basis,where_froms,confidence FROM vw_object_usage_summary ORDER BY COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,''), artifact_id) DESC LIMIT 150)SQL");
    writeHtmlQueryTable(db, out, "Object date summary", "One row per artifact/object with parsed date counts, interpreted date types, association status, and snapshot/index-date warning rollups. Limited to 150 rows.", R"SQL(SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,total_date_count,usage_date_count,downloaded_date_count,modified_date_count,created_date_count,interesting_or_index_date_count,likely_snapshot_or_index_date_count,date_association_status,date_association_confidence,available_date_fields,snapshot_warning_reasons FROM vw_object_date_summary WHERE total_date_count>0 ORDER BY COALESCE(NULLIF(last_date_utc,''), artifact_id) DESC LIMIT 150)SQL");
    writeHtmlQueryTable(db, out, "Recent activity focus", "Recent records by last-used, first-used, or last-updated signal. Limited to 200 rows.", R"SQL(SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,where_froms,confidence FROM artifacts WHERE COALESCE(last_updated_utc,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' ORDER BY COALESCE(NULLIF(last_used_date_utc,''), NULLIF(first_used_candidate_utc,''), NULLIF(last_updated_utc,'')) DESC, artifact_id LIMIT 200)SQL");
    writeHtmlQueryTable(db, out, "WhereFroms / downloaded-origin artifacts", "Artifacts with WhereFroms or downloaded date fields. Limited to 150 rows.", R"SQL(SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,downloaded_date_utc,last_updated_utc,confidence FROM artifacts WHERE COALESCE(where_froms,'')<>'' OR COALESCE(downloaded_date_utc,'')<>'' ORDER BY COALESCE(NULLIF(downloaded_date_utc,''), NULLIF(last_updated_utc,'')) DESC, artifact_id LIMIT 150)SQL");
    writeHtmlQueryTable(db, out, "Content type summary", "Artifact counts by Spotlight content type, including usage/path rollups. Limited to 200 rows.", R"SQL(SELECT COALESCE(NULLIF(content_type,''),'(blank)') AS content_type, COUNT(*) AS artifact_count, SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_artifact_count, SUM(CASE WHEN COALESCE(best_path,'')<>'' THEN 1 ELSE 0 END) AS path_artifact_count, MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc, MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc FROM artifacts GROUP BY COALESCE(NULLIF(content_type,''),'(blank)') ORDER BY artifact_count DESC, content_type LIMIT 200)SQL");
    writeHtmlQueryTable(db, out, "Same-folder / parent inode activity", "Parent-inode groups with multiple children; useful for reconstructing folder context without active filesystem comparison. Limited to 200 rows.", R"SQL(SELECT store_guid,parent_inode_num,COUNT(*) AS child_count, SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_child_count, MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc, MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc, MIN(NULLIF(best_path,'')) AS sample_child_path FROM artifacts WHERE COALESCE(parent_inode_num,'')<>'' GROUP BY store_guid,parent_inode_num HAVING COUNT(*)>1 ORDER BY usage_child_count DESC, child_count DESC, store_guid, CAST(parent_inode_num AS INTEGER) LIMIT 200)SQL");
    writeHtmlQueryTable(db, out, "Volume root / mounted volume indicators", "Spotlight-native volume root and mounted-volume candidates. Limited to 150 rows.", R"SQL(SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,content_type_tree,last_updated_utc,first_used_candidate_utc,last_used_date_utc,is_mounted_volume_path,mounted_volume_name,external_volume_reason,confidence FROM artifacts WHERE COALESCE(content_type,'')='public.volume' OR COALESCE(is_mounted_volume_path,0)<>0 OR COALESCE(mounted_volume_name,'')<>'' OR COALESCE(external_volume_reason,'')<>'' ORDER BY store_guid, CAST(inode_num AS INTEGER), artifact_id LIMIT 150)SQL");
    writeHtmlQueryTable(db, out, "Top date fields", "Date-field coverage and observed ranges. Limited to 100 rows.", R"SQL(SELECT field_name,COUNT(*) AS row_count, COUNT(DISTINCT source_id || '|' || store_guid || '|' || source_db || '|' || inode_num || '|' || COALESCE(store_id,'')) AS record_count, MIN(COALESCE(NULLIF(parsed_utc,''),field_value)) AS min_observed, MAX(COALESCE(NULLIF(parsed_utc,''),field_value)) AS max_observed FROM raw_date_candidates GROUP BY field_name ORDER BY row_count DESC, field_name LIMIT 100)SQL");
    out << "<div class=\"note\"><b>Limitations:</b> active filesystem comparison and deleted/orphaned classification remain disabled. Path reconstruction is Spotlight-native and confidence-rated.</div></body></html>\n";
    log.info("Investigator dashboard written: " + pathString(file));
}

void writeCaseReviewSummary(CaseDatabase& db, const fs::path& file, Logger& log) {
    fs::create_directories(file.parent_path());
    std::ofstream out(ioPath(file), std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write case review summary: " + pathString(file));
    out << "Vestigant Spotlight Investigator Summary\n";
    out << "Generated UTC: " << nowUtc() << "\n";
    out << "Application version: " << scalarText(db, "SELECT COALESCE((SELECT value FROM case_info WHERE key='app_version'),'')") << "\n";
    out << "Input evidence path: " << scalarText(db, "SELECT COALESCE((SELECT input_path FROM evidence_sources ORDER BY added_utc LIMIT 1),'')") << "\n";
    out << "Evidence preservation/container status: " << scalarText(db, "SELECT CASE WHEN COUNT(*)=0 THEN 'not created in this run or diagnostics mode skipped preservation' ELSE group_concat(preservation_status || ':' || COALESCE(integrity_status,''), '; ') END FROM preserved_evidence_sets") << "\n";
    out << "\nCounts\n";
    out << "Stores discovered: " << scalarInt64(db, "SELECT COUNT(*) FROM store_groups") << "\n";
    out << "Native decode attempts: " << scalarInt64(db, "SELECT COUNT(*) FROM native_decode_attempts") << "\n";
    out << "Artifacts: " << scalarInt64(db, "SELECT COUNT(*) FROM artifacts") << "\n";
    out << "Raw native key/value rows: " << scalarInt64(db, "SELECT COUNT(*) FROM raw_key_values") << "\n";
    out << "Raw date candidates: " << scalarInt64(db, "SELECT COUNT(*) FROM raw_date_candidates") << "\n";
    out << "Timeline events: " << scalarInt64(db, "SELECT COUNT(*) FROM timeline_events") << "\n";
    out << "Usage evidence rows: " << scalarInt64(db, "SELECT COUNT(*) FROM usage_evidence") << "\n";
    out << "Usage-linked artifacts: " << scalarInt64(db, "SELECT COUNT(DISTINCT artifact_id) FROM usage_evidence") << "\n";
    out << "Object-centric usage rows: " << scalarInt64(db, "SELECT COUNT(*) FROM vw_object_usage_summary") << "\n";
    out << "Object date summary rows: " << scalarInt64(db, "SELECT COUNT(*) FROM vw_object_date_summary WHERE total_date_count>0") << "\n";
    out << "Path reconstruction candidates: " << scalarInt64(db, "SELECT COUNT(*) FROM parent_inode_links WHERE COALESCE(reconstructed_path_candidate,'')<>''") << "\n";
    out << "Same-folder groups: " << scalarInt64(db, "SELECT COUNT(*) FROM (SELECT source_id, store_guid, child_parent_inode_num FROM parent_inode_links GROUP BY source_id, store_guid, child_parent_inode_num HAVING COUNT(*)>1)") << "\n";
    out << "Native decode errors: " << scalarInt64(db, "SELECT COUNT(*) FROM raw_failures") << "\n";
    out << "Native partial decode errors: " << scalarInt64(db, "SELECT COUNT(*) FROM raw_key_values WHERE field_name='__decode_error'") << "\n";
    out << "\nRun limits / suppression status\n";
    const long long summaryRawRecords = scalarInt64(db, "SELECT COUNT(*) FROM raw_records");
    const long long summaryRawKv = scalarInt64(db, "SELECT COUNT(*) FROM raw_key_values");
    const long long summaryRawDates = scalarInt64(db, "SELECT COUNT(*) FROM raw_date_candidates");
    auto summaryRatio = [](long long a, long long b) {
        if (b <= 0) return std::string("n/a");
        std::ostringstream os;
        os << std::fixed << std::setprecision(3) << (static_cast<double>(a) / static_cast<double>(b));
        return os.str();
    };
    out << "- Native record limit: " << (caseInfoValue(db, "max_native_records", "not_recorded") == "0" ? "unlimited" : caseInfoValue(db, "max_native_records", "not_recorded")) << "\n";
    out << "- Native metadata block limit: " << (caseInfoValue(db, "max_native_blocks", "not_recorded") == "0" ? "unlimited" : caseInfoValue(db, "max_native_blocks", "not_recorded")) << "\n";
    out << "- Diagnostic full native DB: " << caseInfoValue(db, "diagnostic_full_native_db", "not_recorded") << "\n";
    out << "- raw_key_values per raw_record: " << summaryRatio(summaryRawKv, summaryRawRecords) << " (normal iOS mode is compact and not a full property dump)\n";
    out << "- raw_date_candidates per raw_record: " << summaryRatio(summaryRawDates, summaryRawRecords) << " (normal iOS mode keeps compact date provenance; Last_Updated stays on raw_records)\n";
    out << "- Full FFS inventory materialized: " << caseInfoValue(db, "materialize_ios_ffs_inventory", "not_recorded") << "\n";
    out << "- App DB parsed records materialized: " << caseInfoValue(db, "materialize_ios_app_db_records", "not_recorded") << "\n";
    out << "- Detailed machine-readable limits report: exports/parser_limits_and_suppression_summary.csv\n";
    out << "\nTop populated fields\n";
    {
        auto stmt = db.prepare("SELECT field_name,row_count,populated_count,substr(sample_value,1,160) FROM field_inventory ORDER BY row_count DESC, field_name LIMIT 25");
        while (stmt.stepRow()) {
            const std::string fieldName = stmt.colText(0);
            std::string sample = stmt.colText(3);
            if (fieldName.rfind("__native_core_probe_string_", 0) == 0) {
                sample = "[redacted iOS string probe sample; see exports/ios_string_probe_values.csv for case review]";
            }
            out << "- " << fieldName << " rows=" << stmt.colText(1) << " populated=" << stmt.colText(2) << " sample=" << sample << "\n";
        }
    }
    out << "\niOS CoreSpotlight string probe categories\n";
    {
        auto stmt = db.prepare("SELECT probe_category,row_count FROM vw_ios_string_probe_category_summary ORDER BY row_count DESC, probe_category");
        bool any = false;
        while (stmt.stepRow()) { any = true; out << "- " << stmt.colText(0) << " rows=" << stmt.colText(1) << "\n"; }
        if (!any) out << "- none decoded in this run\n";
    }
    out << "\niOS Spotlight communication review categories\n";
    if (tableExists(db, "vw_ios_spotlight_communication_summary")) {
        auto stmt = db.prepare("SELECT communication_context_type,review_priority,SUM(spotlight_record_count),MIN(earliest_spotlight_date_utc),MAX(latest_spotlight_date_utc),substr(MAX(max_context_sample),1,160) FROM vw_ios_spotlight_communication_summary GROUP BY communication_context_type,review_priority ORDER BY MIN(review_priority_sort),SUM(spotlight_record_count) DESC LIMIT 20");
        bool any = false;
        while (stmt.stepRow()) {
            any = true;
            out << "- " << stmt.colText(0) << " priority=" << stmt.colText(1) << " records=" << stmt.colText(2) << " range=" << stmt.colText(3) << " to " << stmt.colText(4) << " sample=" << stmt.colText(5) << "\n";
        }
        if (!any) out << "- none recovered in this run\n";
    } else {
        out << "- communication summary view not available\n";
    }
    out << "\nTop date fields\n";
    {
        auto stmt = db.prepare("SELECT field_name,COUNT(*) AS row_count,COUNT(DISTINCT source_id || '|' || store_guid || '|' || source_db || '|' || inode_num || '|' || COALESCE(store_id,'')) AS record_count,MIN(COALESCE(NULLIF(parsed_utc,''),field_value)),MAX(COALESCE(NULLIF(parsed_utc,''),field_value)) FROM raw_date_candidates GROUP BY field_name ORDER BY row_count DESC, field_name LIMIT 25");
        while (stmt.stepRow()) out << "- " << stmt.colText(0) << " rows=" << stmt.colText(1) << " records=" << stmt.colText(2) << " range=" << stmt.colText(3) << " to " << stmt.colText(4) << "\n";
    }
    out << "\nTop content types\n";
    {
        auto stmt = db.prepare("SELECT COALESCE(NULLIF(content_type,''),'(blank)') AS content_type, COUNT(*) AS c FROM artifacts GROUP BY COALESCE(NULLIF(content_type,''),'(blank)') ORDER BY c DESC, content_type LIMIT 25");
        while (stmt.stepRow()) out << "- " << stmt.colText(0) << " count=" << stmt.colText(1) << "\n";
    }
    out << "\nImportant limitations\n";
    out << "- Active filesystem comparison is disabled in this version; existence_status remains NOT_CHECKED unless populated by another workflow.\n";
    out << "- Deleted/orphaned classification is disabled until a reliable Mac filesystem evidence root is available.\n";
    out << "- Path reconstruction is Spotlight-native and confidence-rated; unresolved parent inodes can still support same-folder grouping.\n";
    out << "- Native value decoding is still under active validation; raw native key/value exports are preserved for review.\n";
    out << "- iOS CoreSpotlight formal property names, content_type values, and full path/display-name mapping remain incomplete unless explicitly decoded; generic string probe rows are currently the primary iOS content-bearing output.\n";
    out << "- iOS Last_Updated values are index/update timing indicators and must not be reported as user file opening or usage without supporting decoded fields.\n";
    log.info("Case review summary written: " + pathString(file));
}

void writeReviewIndex(const fs::path& file, Logger& log) {
    fs::create_directories(file.parent_path());
    std::ofstream out(ioPath(file), std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write review index: " + pathString(file));
    out << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Vestigant Spotlight Review Index</title>";
    out << "<style>body{font-family:Segoe UI,Arial,sans-serif;margin:24px;line-height:1.4} table{border-collapse:collapse;margin:12px 0} td,th{border:1px solid #ccc;padding:6px 10px;vertical-align:top} code{background:#eee;padding:2px 4px} .note{background:#fff7d6;border:1px solid #e0c45c;padding:10px;margin:12px 0}</style>";
    out << "</head><body>";
    out << "<h1>Vestigant Spotlight Review Index</h1>";
    out << "<p>This is a static review landing page generated with the case outputs.</p>";
    out << "<div class=\"note\"><b>Large database handling:</b> the full SQLite database remains in the local case folder for investigation. It is intentionally not copied into the Upload bundle by default. Use <code>exports/upload_samples/</code> and the focused CSVs for bounded review/upload.</div>";
    out << "<h2>Primary investigator review files</h2><table><tr><th>File</th><th>Purpose</th></tr>";
    auto row = [&](const char* href, const char* desc) { out << "<tr><td><a href=\"" << href << "\">" << href << "</a></td><td>" << desc << "</td></tr>"; };
    row("CASE_REVIEW_SUMMARY.txt", "Plain-language run summary, counts, top fields, and limitations.");
    row("exports/parser_limits_and_suppression_summary.csv", "Explicit report of parser limits, normal-mode suppression, sampling, guardrails, and how to override for bounded support runs.");
    row("exports/ios_spotlight_high_value_text_context_review_sample.csv", "Priority-sorted high/medium-value same-record Spotlight text context for investigator review.");
    row("exports/ios_spotlight_text_context_priority_summary.csv", "Compact summary of same-record Spotlight text context categories and review priorities.");
    row("exports/ios_spotlight_chat_app_attribution_summary.csv", "Separates explicit chat-app bundle/domain/external-id attribution from plain chat-app keyword/link mentions.");
    row("exports/ios_spotlight_communication_summary.csv", "Record-centric iOS Spotlight communication summary by Messages/Mail/Call/chat-app/contact/web categories.");
    row("exports/ios_spotlight_communication_record_review_sample.csv", "Bounded, record-centric communication review sample with title/snippet/account/path/context columns.");
    row("exports/ios_spotlight_message_text_review_sample.csv", "Bounded message/mail/call/chat Spotlight review with extracted investigator-visible text.");
    row("exports/ios_spotlight_message_media_review_sample.csv", "Bounded message-adjacent media/photo/attachment Spotlight review.");
    row("exports/ios_spotlight_attachment_reference_review_sample.csv", "Bounded Spotlight attachment/media/content reference sample for communications review.");
    row("investigator_dashboard.html", "Bounded investigator-facing HTML dashboard with usage, recent activity, WhereFroms, folder, volume, and content-type pivots.");
    row("exports/artifact_summary.csv", "One row per artifact with core metadata, date, usage, and path indicators.");
    row("exports/investigator_timeline.csv", "Investigator-oriented timeline derived from parsed date candidates.");
    row("exports/usage_evidence_expanded.csv", "One row per usage-related date/value event.");
    row("exports/folder_summary.csv", "Parent-inode folder grouping and reconstruction summary.");
    row("exports/path_reconstruction_candidates.csv", "Path candidates derived from parent inode links.");
    row("exports/field_inventory.csv", "Decoded field coverage and sample values.");
    row("exports/date_field_inventory.csv", "Date field coverage and parse method summary.");
    row("exports/parser_coverage_summary.csv", "Parser and enrichment coverage metrics.");
    out << "</table>";
    out << "<h2>Thin upload / bounded database samples</h2><table><tr><th>File</th><th>Purpose</th></tr>";
    row("UPLOAD_README.txt", "Explains the thin-upload model and what was intentionally omitted.");
    row("TARGETED_EXPORT_README.txt", "How to create targeted exports from the local SQLite database without uploading the full database.");
    row("Export-SpotlightTargetedData.ps1", "PowerShell helper for local SQLite targeted exports by usage, timeline, path, field, artifact, or WhereFroms.");
    row("Create-UploadZip.ps1", "Verified ZIP helper for the Upload folder; avoids wildcard/LiteralPath Compress-Archive issues and confirms ZIP entries.");
    row("exports/upload_samples/upload_table_counts.csv", "SQLite table row counts.");
    row("exports/upload_samples/upload_samples_manifest.csv", "Sample files, row caps, and table totals.");
    row("exports/upload_samples/artifacts_sample.csv", "Bounded sample from artifacts table.");
    row("exports/upload_samples/raw_key_values_sample.csv", "Bounded sample from raw_key_values table.");
    row("exports/upload_samples/raw_date_candidates_sample.csv", "Bounded sample from raw_date_candidates table.");
    row("exports/upload_samples/timeline_events_sample.csv", "Bounded sample from timeline_events table.");
    row("exports/upload_samples/artifacts_usage_focus.csv", "Usage-bearing artifacts for investigator triage.");
    row("exports/upload_samples/artifacts_wherefrom_focus.csv", "Artifacts with WhereFroms/download-origin style fields.");
    row("exports/upload_samples/artifacts_path_focus.csv", "Artifacts with stronger path/name reconstruction data.");
    row("exports/upload_samples/path_reconstruction_focus.csv", "Parent-inode reconstructed path candidates with separate applied-path and candidate-match status.");
    row("exports/upload_samples/same_folder_groups_focus.csv", "Same-folder parent-inode groups with reconstruction counts.");
    row("exports/upload_samples/checked_artifacts_focus.csv", "Persisted GUI checked artifacts with tag, note, date, and path context.");
    row("exports/upload_samples/timeline_usage_focus.csv", "Timeline rows associated with usage/opened fields.");
    row("exports/upload_samples/native_key_values_high_value_sample.csv", "High-value native key/value metadata sample.");
    row("exports/upload_samples/content_type_summary.csv", "Content type counts with usage/path/size rollups.");
    row("exports/upload_samples/store_content_type_summary.csv", "Content type counts by Spotlight store.");
    row("exports/upload_samples/folder_activity_summary.csv", "Same-parent folder activity rollup from Spotlight parent inode data.");
    row("exports/upload_samples/recent_activity_focus.csv", "Recent artifacts by usage/update signals.");
    row("exports/upload_samples/volume_root_focus.csv", "Volume root and mounted-volume indicators.");
    row("exports/ios_app_parsed_records.csv", "Generic parsed rows from staged iOS app databases such as Messages, calls, web, calendar, contacts, and attachments.");
    row("exports/ios_app_parsed_record_summary.csv", "Counts and date ranges for generic parsed iOS app database rows.");
    row("exports/ios_apple_messages_parsed_records.csv", "Apple Messages SMS.db message and attachment rows with handle/chat context where schema permits.");
    row("exports/ios_apple_messages_parsed_summary.csv", "Summary counts for Apple Messages SMS.db parsed records and attachments.");
    row("exports/ios_apple_messages_database_status.csv", "SMS.db database/table status showing whether live message/chat/attachment rows exist even when Spotlight contains message-like hits.");
    row("exports/ios_app_live_activity_timeline.csv", "Timeline of parsed local iOS app-database records with schema/timestamp provenance.");
    row("exports/ios_communications_review_records.csv", "Unified iOS communications review rows from Messages, WhatsApp, call history, and other message/chat/call-like app database records.");
    row("exports/ios_communications_review_summary.csv", "Grouped counts and date coverage for the unified iOS communications review rows.");
    row("exports/ios_spotlight_communication_candidates.csv", "CoreSpotlight string probes that appear communication-related, with conservative app-database context.");
    row("exports/ios_spotlight_decode_coverage_summary.csv", "Spotlight-first decode coverage by iOS CoreSpotlight store: raw records, recovered values, human text values, and native decode status.");
    row("exports/ios_spotlight_field_coverage_summary.csv", "Recovered Spotlight field/probe coverage by store and field name; highlights generic native probes versus named fields.");
    row("exports/ios_spotlight_text_category_summary.csv", "Counts of recovered iOS CoreSpotlight human-readable text categories and review priorities.");
    row("exports/ios_spotlight_record_review.csv", "Support/diagnostic only full Spotlight-first record review surface. Normal investigator export uses compact summaries to avoid long SQL materialization.");
    row("exports/ios_spotlight_object_inode_diagnostic_summary.csv", "Compact object/inode record-splitting diagnostic used to evaluate whether repeated Spotlight records map to the same underlying object.");
    row("exports/ios_spotlight_date_provenance.csv", "Per-record iOS CoreSpotlight date/time provenance: raw source field, raw value, parse method, date type, and validation hint.");
    row("exports/ios_spotlight_investigative_items_with_dates.csv", "Recovered CoreSpotlight investigative text items with directly linked Spotlight date/time provenance and validation fields.");
    row("exports/ios_spotlight_investigative_item_date_evidence.csv", "Recovered CoreSpotlight investigative text items joined to every raw Spotlight date candidate from the same Store-V2 record, with validation locators.");
    row("exports/ios_spotlight_date_field_summary.csv", "Summary of iOS CoreSpotlight raw date fields, parsed ranges, semantic class candidates, and reporting cautions.");
    row("exports/ios_spotlight_high_value_timeline.csv", "Support/diagnostic export: Spotlight high-value timeline with secondary FFS/app context. Normal investigator mode uses ios_spotlight_investigative_items_with_dates to avoid heavy correlation joins.");
    row("exports/ios_spotlight_file_reference_review.csv", "Support/diagnostic export: recovered CoreSpotlight file/path references with FFS presence context. Normal investigator mode avoids full path-link materialization by default.");
    row("exports/ios_spotlight_url_reference_review.csv", "Recovered CoreSpotlight URL/web/meeting/calendar references with date provenance and app-family context.");
    row("exports/ios_spotlight_account_contact_reference_review.csv", "Recovered CoreSpotlight account/contact/communications references with date provenance and app-family context.");
    row("exports/ios_spotlight_decode_gap_summary.csv", "Grouped summary of CoreSpotlight records that parsed at record level but still lack recovered values/text. Primary native parser target list.");
    row("exports/ios_spotlight_entity_review.csv", "Support/diagnostic export: normalized entity review with secondary context. Normal investigator mode uses compact Spotlight-first summaries and investigative item exports.");
    row("exports/ios_spotlight_entity_summary.csv", "Grouped counts for normalized Spotlight entities by entity type, source store, source field, date semantic class, and supporting FFS/app context.");
    row("exports/ios_spotlight_native_parser_targets.csv", "Prioritized native CoreSpotlight parser improvement targets: records without recovered text and generic probe fields needing property-name mapping.");
    row("exports/ios_spotlight_dbstr_map_inventory.csv", "iOS CoreSpotlight dbStr map component inventory and parse status for properties, categories, and index maps.");
    row("exports/ios_spotlight_dictionary_coverage.csv", "Per-store dictionary coverage showing whether dbStr/property/category maps were found and parsed.");
    row("exports/ios_spotlight_apple_field_coverage.csv", "Recovered CoreSpotlight field names grouped by Apple public Core Spotlight metadata semantics where names match.");
    row("exports/ios_spotlight_decode_gap_records.csv", "Spotlight records parsed at record/header level but missing recovered key/value or human-readable text values; native parser improvement target list.");
    row("exports/ios_contact_identity_records.csv", "Contact/address-book identity review rows from AddressBook and contact cache databases.");
    row("exports/ios_contact_identity_summary.csv", "Grouped counts for contact identity review rows by database, table, and value coverage.");
    row("exports/ios_web_history_review_records.csv", "Parsed Safari/WebKit/Chrome-style web history, bookmark, URL, and title rows.");
    row("exports/ios_web_history_review_summary.csv", "Grouped counts for parsed web/browser rows.");
    row("exports/ios_calendar_review_records.csv", "Parsed Calendar rows with event/support, attendee/account, attachment, and location context.");
    row("exports/ios_calendar_review_summary.csv", "Grouped counts for parsed Calendar rows.");
    row("exports/ios_investigation_keyword_surface.csv", "Unified iOS keyword-review surface across Spotlight text, parsed app rows, high-value FFS paths, and app database inventory.");
    row("exports/ios_whatsapp_database_status.csv", "WhatsApp iOS database/table status and parser coverage for ChatStorage/Contacts/CallHistory-style databases.");
    row("exports/ios_whatsapp_parsed_records.csv", "Parsed iOS WhatsApp messages, media, chats, contacts, and calls where supported tables are present.");
    row("exports/ios_whatsapp_parsed_summary.csv", "Summary counts for parsed iOS WhatsApp records.");
    row("exports/ios_keychain_material_inventory.csv", "Inventory-only listing of core keychain/keybag files in the iOS FFS ZIP root.");
    row("exports/ios_keychain_support_reference_inventory.csv", "Lower-priority keychain-named framework/code references outside core keychain/keybag locations.");
    out << "</table>";
    out << "<h2>Full local database</h2><table><tr><th>File</th><th>Purpose</th></tr>";
    row("VestigantSpotlight.case.sqlite", "Primary SQLite case database. Local case folder only; usually too large for upload.");
    row("spotlight_case.db", "Convenience copy for external SQLite tools. Local case folder only; usually too large for upload.");
    out << "</table>";
    out << "<h2>Notes</h2><p>Filesystem existence/deleted status is not evaluated in this version. Parent-inode paths are Spotlight-native candidates and should be reviewed with their confidence fields.</p>";
    out << "</body></html>\n";
    log.info("Review index written: " + pathString(file));
}

bool tableExists(CaseDatabase& db, const std::string& tableName) {
    auto stmt = db.prepare("SELECT COUNT(*) FROM sqlite_master WHERE type IN ('table','view') AND name=?");
    stmt.bind(1, tableName);
    return stmt.stepRow() && stmt.colInt64(0) > 0;
}

long long tableRowCount(CaseDatabase& db, const std::string& tableName) {
    if (!tableExists(db, tableName)) return 0;
    return scalarInt64(db, "SELECT COUNT(*) FROM " + sqlQuoteIdentifier(tableName));
}


std::string caseInfoValue(CaseDatabase& db, const std::string& key, const std::string& fallback = {}) {
    try {
        auto stmt = db.prepare("SELECT value FROM case_info WHERE key=? LIMIT 1");
        stmt.bind(1, key);
        if (stmt.stepRow()) return stmt.colText(0);
    } catch (...) {}
    return fallback;
}

void writeParserLimitsAndSuppressionSummary(CaseDatabase& db, const fs::path& file, Logger& log) {
    fs::create_directories(ioPath(file.parent_path()));
    std::ofstream out(ioPath(file), std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write parser limits/suppression summary: " + pathString(file));

    const long long rawRecords = tableRowCount(db, "raw_records");
    const long long rawKv = tableRowCount(db, "raw_key_values");
    const long long rawDates = tableRowCount(db, "raw_date_candidates");
    const long long probeValues = tableExists(db, "vw_ios_string_probe_values") ? tableRowCount(db, "vw_ios_string_probe_values") : 0;
    const long long textContext = scalarInt64(db, "SELECT COUNT(*) FROM raw_key_values WHERE field_name='__spotlight_investigator_text_context'");
    const long long decodeAttempts = tableRowCount(db, "native_decode_attempts");
    const std::string decodeMode = scalarText(db, "SELECT COALESCE(group_concat(DISTINCT decode_mode),'') FROM native_decode_attempts");
    const std::string maxNativeRecords = caseInfoValue(db, "max_native_records", "not_recorded");
    const std::string maxNativeBlocks = caseInfoValue(db, "max_native_blocks", "not_recorded");
    const std::string diagFullNativeDb = caseInfoValue(db, "diagnostic_full_native_db", "not_recorded");
    const std::string materializeFfs = caseInfoValue(db, "materialize_ios_ffs_inventory", "not_recorded");
    const std::string materializeAppDb = caseInfoValue(db, "materialize_ios_app_db_records", "not_recorded");
    const std::string exportProfile = caseInfoValue(db, "export_profile", "not_recorded");
    const std::string skipHash = caseInfoValue(db, "skip_container_hash", "not_recorded");
    const std::string guardrailBytes = caseInfoValue(db, "db_size_guardrail_bytes", "not_recorded");

    auto ratio = [](long long a, long long b) {
        if (b <= 0) return std::string("n/a");
        std::ostringstream os;
        os << std::fixed << std::setprecision(3) << (static_cast<double>(a) / static_cast<double>(b));
        return os.str();
    };
    auto row = [&](const std::string& setting, const std::string& current, const std::string& effect,
                   const std::string& completeness, const std::string& overrideText, const std::string& evidence) {
        out << csvEscape(setting) << ',' << csvEscape(current) << ',' << csvEscape(effect) << ','
            << csvEscape(completeness) << ',' << csvEscape(overrideText) << ',' << csvEscape(evidence) << "\n";
    };

    out << "setting_or_limit,current_value,effect_on_extraction,completeness_or_review_risk,override_or_confirmation,evidence_from_current_run\n";
    row("application_version", caseInfoValue(db, "app_version", "unknown"), "Identifies parser build used for the run.", "None.", "Build/run current version.", "case_info.app_version");
    row("source_container_hash", skipHash == "true" ? "HASH_DEFERRED_BY_SKIP_CONTAINER_HASH" : "hash attempted or not recorded", "Reuse-cache development runs can avoid re-reading very large ZIP sources solely for hashing.", "For forensic/final runs, deferred hash should be replaced with a source/container hash.", "omit --skip-container-hash or use --force-container-hash where supported", "case_info.skip_container_hash=" + skipHash);
    row("native_record_parse_limit", (maxNativeRecords == "0" ? "UNLIMITED" : maxNativeRecords), "Controls whether Store-V2/CoreSpotlight records are intentionally capped.", maxNativeRecords == "0" ? "No deliberate record-count cap recorded." : "Records may be intentionally sampled/capped.", "run without --max-native-records for full record enumeration", "raw_records=" + std::to_string(rawRecords) + "; native_decode_attempts=" + std::to_string(decodeAttempts));
    row("native_metadata_block_limit", (maxNativeBlocks == "0" ? "UNLIMITED" : maxNativeBlocks), "Controls whether native metadata block scanning is capped.", maxNativeBlocks == "0" ? "No deliberate block-count cap recorded." : "Metadata block scan may be intentionally capped.", "run without --max-native-blocks for full block enumeration", "case_info.max_native_blocks=" + maxNativeBlocks);
    row("native_decode_mode", decodeMode.empty() ? "not_recorded" : decodeMode, "Indicates native parser mode reported by native_decode_attempts.", "FullValues still may use compact persistence in normal iOS mode.", "use diagnostic flags only for bounded support runs", "native_decode_attempts.decode_mode=" + decodeMode);
    row("raw_key_value_persistence", diagFullNativeDb == "true" ? "FULL_NATIVE_DB_DIAGNOSTIC_MODE" : "COMPACT_IOS_NORMAL_MODE", "Normal iOS mode keeps high-value reference rows and one synthetic same-record text-context row instead of every decoded property.", "raw_key_values is not a complete property table in normal mode; use field coverage and text context views for investigation.", "--diagnostic-full-native-db with explicit --max-native-records for bounded support runs", "raw_key_values=" + std::to_string(rawKv) + "; raw_key_values_per_raw_record=" + ratio(rawKv, rawRecords));
    row("spotlight_text_context", "SAME_RECORD_CONTEXT_SYNTHETIC_ROW", "Adds __spotlight_investigator_text_context when a record has persisted reference/path/account/contact-style evidence.", "Context is a compact sample, not a full raw property dump.", "diagnostic native DB mode for full raw values", "__spotlight_investigator_text_context rows=" + std::to_string(textContext));
    row("date_candidate_persistence", "COMPACT_ONE_HIGH_VALUE_DATE_PER_RECORD_MAX_IN_NORMAL_IOS_MODE", "Normal iOS mode avoids raw_date_candidates cross-product expansion and keeps Last_Updated on raw_records instead of duplicating it as a date candidate.", "raw_date_candidates is not every date-like property in normal iOS mode.", "diagnostic/full support mode for wider date expansion after bounded validation", "raw_date_candidates=" + std::to_string(rawDates) + "; raw_dates_per_raw_record=" + ratio(rawDates, rawRecords));
    row("string_probe_values", "DERIVED_VIEW_FROM_COMPACT_RAW_VALUES", "String probe views expose reviewable decoded text categories without requiring every raw property row.", "Counts are review surfaces, not a guarantee of all original binary strings.", "diagnostic full-native DB plus targeted export for full support review", "vw_ios_string_probe_values rows=" + std::to_string(probeValues));
    row("ffs_inventory_materialization", materializeFfs == "true" ? "FULL_FFS_INVENTORY_MATERIALIZED" : "SLIM_FFS_PATH_LOOKUP_ONLY", "Normal mode avoids inserting millions of FFS rows and uses slim path lookup for Missing From FFS.", "Missing/present determinations depend on lookup availability and normalized path matching.", "--materialize-ios-ffs-inventory for support/correlation runs", "case_info.materialize_ios_ffs_inventory=" + materializeFfs);
    row("app_db_record_materialization", materializeAppDb == "true" ? "APP_DB_RECORDS_MATERIALIZED" : "APP_DB_SUMMARY_OR_SKIPPED_NORMAL_MODE", "Normal mode avoids broad app DB parsed-record materialization.", "Spotlight remains primary; app DB corroboration may require support mode.", "--materialize-ios-app-db-records or --materialize-ios-support-db", "case_info.materialize_ios_app_db_records=" + materializeAppDb);
    row("normal_export_profile", exportProfile, "Controls normal/support/diagnostic export selection.", "Diagnostic exports may be intentionally absent from normal runs.", "--export-profile diagnostics or full for support exports", "case_info.export_profile=" + exportProfile);
    row("thin_upload_sampling", "SAMPLED_AND_LABELED", "Upload package includes bounded sample/focus CSVs to avoid transferring the full local DB.", "*_sample*.csv and upload_samples files are not full exports.", "Use local SQLite and Export-SpotlightTargetedData.ps1 for targeted full slices.", "upload_samples_manifest.csv documents sample caps and total source rows");
    row("sqlite_guardrail", guardrailBytes, "Stops run if DB or WAL exceed configured safety threshold.", "A guardrail stop is protective, not a parse-complete success.", "--db-size-guardrail-gb N or --disable-db-size-guardrail", "case_info.db_size_guardrail_bytes=" + guardrailBytes);
    log.info("Parser limits/suppression summary written: " + pathString(file));
}

void writeUploadReadme(const fs::path& caseDir, Logger& log) {
    std::ofstream out(ioPath(caseDir / "UPLOAD_README.txt"), std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write UPLOAD_README.txt");
    out << "Vestigant Spotlight Thin Upload Package\n";
    out << "Generated UTC: " << nowUtc() << "\n\n";
    out << "The full SQLite case database can be hundreds of MB or larger. It remains in the local case output folder and is intentionally not copied into Upload by default.\n\n";
    out << "Use these bounded files for review and troubleshooting without uploading the full database:\n";
    out << "- CASE_REVIEW_SUMMARY.txt\n";
    out << "- exports/parser_limits_and_suppression_summary.csv\n";
    out << "- review_index.html\n";
    out << "- exports/upload_samples/upload_table_counts.csv\n";
    out << "- exports/upload_samples/upload_samples_manifest.csv\n";
    out << "- exports/upload_samples/*_sample.csv\n";
    out << "- exports/upload_samples/*_focus.csv\n";
    out << "- TARGETED_EXPORT_README.txt\n";
    out << "- Export-SpotlightTargetedData.ps1\n";
    out << "- Create-UploadZip.ps1\n\n";
    out << "Local-only full database files, when present:\n";
    out << "- VestigantSpotlight.case.sqlite\n";
    out << "- spotlight_case.db\n\n";
    out << "To create the upload zip, run Create-UploadZip.ps1 from the case output folder. It verifies the archive exists and contains entries before reporting success.\n";
    out << "To provide targeted follow-up data, export only the needed table/filter from the local SQLite database instead of uploading the full database.\n";
    log.info("Upload README written: " + pathString(caseDir / "UPLOAD_README.txt"));
}


void writeTargetedExportReadme(const fs::path& caseDir, Logger& log) {
    std::ofstream out(ioPath(caseDir / "TARGETED_EXPORT_README.txt"), std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write TARGETED_EXPORT_README.txt");
    out << "Vestigant Spotlight Targeted Export Helper\n";
    out << "Generated UTC: " << nowUtc() << "\n\n";
    out << "The full SQLite case database remains local. Use Export-SpotlightTargetedData.ps1 to create smaller CSV slices from the local database for review or upload.\n\n";
    out << "Examples:\n";
    out << "  powershell -ExecutionPolicy Bypass -File ./Export-SpotlightTargetedData.ps1 -Mode usage\n";
    out << "  powershell -ExecutionPolicy Bypass -File ./Export-SpotlightTargetedData.ps1 -Mode timeline -StartUtc 2025-12-18T00:00:00Z -EndUtc 2025-12-20T00:00:00Z\n";
    out << "  powershell -ExecutionPolicy Bypass -File ./Export-SpotlightTargetedData.ps1 -Mode path -PathContains LaCie\n";
    out << "  powershell -ExecutionPolicy Bypass -File ./Export-SpotlightTargetedData.ps1 -Mode field -FieldContains WhereFrom\n";
    out << "  powershell -ExecutionPolicy Bypass -File ./Export-SpotlightTargetedData.ps1 -Mode artifact -ArtifactId 23307\n";
    out << "  powershell -ExecutionPolicy Bypass -File ./Export-SpotlightTargetedData.ps1 -Mode artifact -ArtifactId 23307,43922\n";
    out << "  powershell -ExecutionPolicy Bypass -File ./Export-SpotlightTargetedData.ps1 -Mode checked\n";
    out << "  powershell -ExecutionPolicy Bypass -File ./Export-SpotlightTargetedData.ps1 -Mode tag -TagName NeedsReview\n\n";
    out << "Default output folder: TargetedExports under the case folder. The helper can be launched from the case root or from the copied Upload folder; when launched from Upload, it uses the parent case folder to locate VestigantSpotlight.case.sqlite.\n";
    out << "V0.8.5+ targeted CSVs include headers, are written directly by sqlite3 .once output to avoid blank-line expansion, and include targeted_export_manifest.csv plus TARGETED_EXPORTS_README.txt.\n";
    out << "Requires sqlite3.exe in PATH, or pass -SqliteExe with a full path.\n";
    log.info("Targeted export README written: " + pathString(caseDir / "TARGETED_EXPORT_README.txt"));
}

void writeTargetedExportScript(const fs::path& caseDir, Logger& log) {
    std::ofstream out(ioPath(caseDir / "Export-SpotlightTargetedData.ps1"), std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write Export-SpotlightTargetedData.ps1");
    out << R"PS1(# Vestigant Spotlight targeted SQLite export helper
# Generated by Vestigant Spotlight. Keeps the full database local and exports small CSV slices.
# V0_8_5 hardening: direct sqlite .once output, headers on every CSV, no PowerShell pipe blank-line expansion, and manifest generation.
param(
    [string]$DbPath = "",
    [string]$OutDir = "",
    [ValidateSet("usage","timeline","dates","path","wherefrom","field","artifact","checked","tag","counts")]
    [string]$Mode = "usage",
    [string]$StartUtc = "",
    [string]$EndUtc = "",
    [string]$PathContains = "",
    [string]$FieldContains = "",
    [string]$ArtifactId = "",
    [string]$TagName = "",
    [int]$Limit = 5000,
    [string]$SqliteExe = "sqlite3"
)

$ErrorActionPreference = "Stop"

$ScriptRoot = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ScriptRoot)) { $ScriptRoot = (Get-Location).Path }
if ((Split-Path -Leaf $ScriptRoot) -ieq "Upload") {
    $CaseDir = Split-Path -Parent $ScriptRoot
} else {
    $CaseDir = $ScriptRoot
}
if ([string]::IsNullOrWhiteSpace($DbPath)) {
    $DbPath = Join-Path $CaseDir "VestigantSpotlight.case.sqlite"
}
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $CaseDir "TargetedExports"
}

if (!(Test-Path -LiteralPath $DbPath -PathType Leaf)) {
    throw "SQLite database not found: $DbPath"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$script:TargetedManifest = New-Object System.Collections.Generic.List[object]
$script:ExportStartedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")

function Escape-SqlLiteral([string]$s) { return ($s -replace "'", "''") }
function Like-Clause([string]$column, [string]$value) {
    if ([string]::IsNullOrWhiteSpace($value)) { return "1=1" }
    $v = Escape-SqlLiteral $value
    return "lower(coalesce($column,'')) LIKE lower('%$v%')"
}
function Date-Clause([string]$column) {
    $clauses = @()
    if (![string]::IsNullOrWhiteSpace($StartUtc)) { $v = Escape-SqlLiteral $StartUtc; $clauses += "$column >= '$v'" }
    if (![string]::IsNullOrWhiteSpace($EndUtc)) { $v = Escape-SqlLiteral $EndUtc; $clauses += "$column <= '$v'" }
    if ($clauses.Count -eq 0) { return "1=1" }
    return ($clauses -join " AND ")
}
function Convert-ToSqliteDotPath([string]$Path) {
    $full = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    return (($full -replace '\\','/') -replace "'", "''")
}
function Count-CsvDataRows([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return 0 }
    $nonEmpty = 0
    Get-Content -LiteralPath $Path -Encoding UTF8 | ForEach-Object {
        if ($_.Trim().Length -gt 0) { $nonEmpty++ }
    }
    if ($nonEmpty -le 0) { return 0 }
    return [Math]::Max(0, $nonEmpty - 1)
}
)PS1" R"PS1(function Add-ManifestRow([string]$Name, [string]$Path, [int]$Rows, [string]$Description) {
    $script:TargetedManifest.Add([pscustomobject]@{
        generated_utc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
        mode = $Mode
        file = $Name
        path = $Path
        data_rows = $Rows
        limit = $Limit
        description = $Description
    }) | Out-Null
}
function Invoke-SqlCsv([string]$Name, [string]$Sql, [string]$Description = "targeted export") {
    $safeName = ($Name -replace '[^A-Za-z0-9_.-]', '_')
    $outFile = Join-Path $OutDir $safeName
    $sqliteOut = Convert-ToSqliteDotPath $outFile
    Write-Host "Exporting $safeName"

    $sqliteScript = @"
.headers on
.mode csv
.once '$sqliteOut'
$Sql
.output stdout
"@

    $sqliteResult = $sqliteScript | & $SqliteExe -readonly $DbPath 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        $msg = ($sqliteResult | Out-String).Trim()
        throw "sqlite3 failed for $safeName with exit code $exitCode. $msg"
    }
    if (!(Test-Path -LiteralPath $outFile -PathType Leaf)) {
        throw "sqlite3 reported success but did not create output file: $outFile"
    }

    $rows = Count-CsvDataRows $outFile
    Add-ManifestRow $safeName $outFile $rows $Description
    Write-Host "Wrote $outFile rows=$rows"
}
function Artifact-Id-In-Clause([string]$ArtifactIds) {
    if ([string]::IsNullOrWhiteSpace($ArtifactIds)) { throw "-ArtifactId is required for -Mode artifact" }
    $ids = $ArtifactIds -split '[,;\s]+' | Where-Object { $_ -match '^\d+$' } | Select-Object -Unique
    if (-not $ids -or $ids.Count -eq 0) { throw "No valid numeric artifact IDs found in -ArtifactId: $ArtifactIds" }
    return ($ids -join ',')
}
function Write-TargetedReadme() {
    $readme = Join-Path $OutDir "TARGETED_EXPORTS_README.txt"
    @(
        "Vestigant Spotlight targeted export bundle",
        "Generated UTC: $script:ExportStartedUtc",
        "Case folder: $CaseDir",
        "Database: $DbPath",
        "Mode: $Mode",
        "Limit: $Limit",
        "",
        "CSV format notes:",
        "- Every CSV generated by V0_8_5+ includes a header row.",
        "- CSVs are written directly by sqlite3 .once output to avoid PowerShell pipe blank-line expansion.",
        "- targeted_export_manifest.csv lists generated files and data-row counts.",
        "",
        "Parameters:",
        "StartUtc=$StartUtc",
        "EndUtc=$EndUtc",
        "PathContains=$PathContains",
        "FieldContains=$FieldContains",
        "ArtifactId=$ArtifactId",
        "TagName=$TagName"
    ) | Set-Content -LiteralPath $readme -Encoding UTF8
}
function Write-TargetedManifest() {
    $manifest = Join-Path $OutDir "targeted_export_manifest.csv"
)PS1" R"PS1(    $script:TargetedManifest | Export-Csv -LiteralPath $manifest -NoTypeInformation -Encoding UTF8
    Write-Host "Wrote $manifest"
    Write-TargetedReadme
}

$limitSql = [Math]::Max(1, $Limit)

try {
    switch ($Mode) {
        "usage" {
            $sql = @"
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,usage_field_summary,open_count_estimate,existence_status,deleted_or_orphaned_candidate,confidence
FROM artifacts
WHERE coalesce(usage_field_summary,'')<>'' OR coalesce(last_used_date_utc,'')<>'' OR coalesce(first_used_candidate_utc,'')<>'' OR coalesce(used_dates_count,0)>0 OR coalesce(use_count_value,'')<>''
ORDER BY coalesce(last_used_date_utc, first_used_candidate_utc, last_updated_utc) DESC, artifact_id
LIMIT $limitSql;
"@
            Invoke-SqlCsv "target_usage_artifacts.csv" $sql "usage-bearing artifacts"
        }
        "timeline" {
            $dateClause = Date-Clause "t.event_timestamp_utc"
            $pathClause = Like-Clause "a.best_path || ' ' || a.file_name || ' ' || a.display_name" $PathContains
            $sql = @"
SELECT t.timeline_id,t.event_timestamp_utc AS event_utc,t.event_type,t.event_source_field,t.artifact_id,t.source_id,t.store_guid,t.inode_num,a.parent_inode_num,a.file_name,a.display_name,a.best_path,a.content_type,a.where_froms,a.existence_status,a.deleted_or_orphaned_candidate
FROM timeline_events t
LEFT JOIN artifacts a ON a.artifact_id=t.artifact_id
WHERE $dateClause AND $pathClause
ORDER BY t.event_timestamp_utc, t.timeline_id
LIMIT $limitSql;
"@
            Invoke-SqlCsv "target_timeline.csv" $sql "timeline rows with optional date/path filtering"
        }
        "dates" {
            $dateClause = Date-Clause "last_date_utc"
            $pathClause = Like-Clause "best_path || ' ' || file_name || ' ' || display_name" $PathContains
            $sql = @"
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,logical_size_bytes,physical_size_bytes,content_type,first_date_utc,last_date_utc,total_date_count,usage_date_count,downloaded_date_count,modified_date_count,created_date_count,interesting_or_index_date_count,likely_snapshot_or_index_date_count,date_association_status,date_association_confidence,available_date_fields,interpreted_date_types,snapshot_warning_reasons
FROM vw_object_date_summary
WHERE total_date_count>0 AND $dateClause AND $pathClause
ORDER BY coalesce(last_date_utc, first_date_utc) DESC, artifact_id
LIMIT $limitSql;
"@
            Invoke-SqlCsv "target_object_date_summary.csv" $sql "object-centric date summary"
        }
        "path" {
)PS1" R"PS1(            $pathClause = Like-Clause "best_path || ' ' || file_name || ' ' || display_name" $PathContains
            $sql = @"
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,content_type_tree,last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,where_froms,confidence
FROM artifacts
WHERE $pathClause
ORDER BY store_guid, best_path, artifact_id
LIMIT $limitSql;
"@
            Invoke-SqlCsv "target_path_artifacts.csv" $sql "path/name filtered artifacts"
        }
        "wherefrom" {
            $sql = @"
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,downloaded_date_utc,last_updated_utc,confidence
FROM artifacts
WHERE coalesce(where_froms,'')<>'' OR coalesce(downloaded_date_utc,'')<>''
ORDER BY coalesce(downloaded_date_utc,last_updated_utc) DESC, artifact_id
LIMIT $limitSql;
"@
            Invoke-SqlCsv "target_wherefrom_artifacts.csv" $sql "WhereFroms/download-origin artifacts"
        }
        "field" {
            $fieldClause = Like-Clause "field_name || ' ' || field_value" $FieldContains
            $sql = @"
SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,full_path,record_state,field_name,field_value
FROM raw_key_values
WHERE $fieldClause
ORDER BY store_guid, CAST(inode_num AS INTEGER), raw_kv_id
LIMIT $limitSql;
"@
            Invoke-SqlCsv "target_field_values.csv" $sql "raw key/value rows filtered by field/value text"
        }
        "artifact" {
            $ids = Artifact-Id-In-Clause $ArtifactId
            Invoke-SqlCsv "target_artifact_records.csv" "SELECT * FROM artifacts WHERE artifact_id IN ($ids) ORDER BY artifact_id;" "selected artifact records"
            Invoke-SqlCsv "target_artifact_timeline.csv" "SELECT * FROM timeline_events WHERE artifact_id IN ($ids) ORDER BY artifact_id,event_timestamp_utc,timeline_id;" "selected artifact timeline rows"
            Invoke-SqlCsv "target_artifact_raw_key_values.csv" "SELECT * FROM raw_key_values WHERE source_id || '|' || store_guid || '|' || inode_num IN (SELECT source_id || '|' || store_guid || '|' || inode_num FROM artifacts WHERE artifact_id IN ($ids)) ORDER BY source_id,store_guid,CAST(inode_num AS INTEGER),raw_kv_id LIMIT $limitSql;" "selected artifact raw key/value support rows"
            Invoke-SqlCsv "target_artifact_date_candidates.csv" "SELECT * FROM raw_date_candidates WHERE source_id || '|' || store_guid || '|' || inode_num IN (SELECT source_id || '|' || store_guid || '|' || inode_num FROM artifacts WHERE artifact_id IN ($ids)) ORDER BY source_id,store_guid,CAST(inode_num AS INTEGER),raw_date_id LIMIT $limitSql;" "selected artifact raw date support rows"
)PS1" R"PS1(        }
        "checked" {
            Invoke-SqlCsv "target_checked_artifacts.csv" "SELECT * FROM vw_checked_artifacts ORDER BY checked_utc DESC, artifact_id LIMIT $limitSql;" "persisted checked artifacts"
            Invoke-SqlCsv "target_checked_raw_key_values.csv" "SELECT kv.* FROM raw_key_values kv JOIN vw_checked_artifacts c ON c.source_id=kv.source_id AND c.store_guid=kv.store_guid AND c.inode_num=kv.inode_num ORDER BY c.artifact_id, kv.raw_kv_id LIMIT $limitSql;" "raw key/value support for checked artifacts"
            Invoke-SqlCsv "target_checked_dates.csv" "SELECT d.* FROM raw_date_candidates d JOIN vw_checked_artifacts c ON c.source_id=d.source_id AND c.store_guid=d.store_guid AND c.inode_num=d.inode_num ORDER BY c.artifact_id, d.raw_date_id LIMIT $limitSql;" "raw date support for checked artifacts"
        }
        "tag" {
            if ([string]::IsNullOrWhiteSpace($TagName)) { throw "-TagName is required for -Mode tag" }
            $tag = Escape-SqlLiteral $TagName
            Invoke-SqlCsv "target_tagged_artifacts.csv" "SELECT * FROM vw_tagged_artifacts WHERE tag_name='$tag' ORDER BY artifact_id LIMIT $limitSql;" "artifacts tagged with selected tag"
            Invoke-SqlCsv "target_tagged_raw_key_values.csv" "SELECT kv.* FROM raw_key_values kv JOIN vw_tagged_artifacts ta ON ta.source_id=kv.source_id AND ta.store_guid=kv.store_guid AND ta.inode_num=kv.inode_num WHERE ta.tag_name='$tag' ORDER BY ta.artifact_id, kv.raw_kv_id LIMIT $limitSql;" "raw key/value support for selected tag"
            Invoke-SqlCsv "target_tagged_dates.csv" "SELECT d.* FROM raw_date_candidates d JOIN vw_tagged_artifacts ta ON ta.source_id=d.source_id AND ta.store_guid=d.store_guid AND ta.inode_num=d.inode_num WHERE ta.tag_name='$tag' ORDER BY ta.artifact_id, d.raw_date_id LIMIT $limitSql;" "raw date support for selected tag"
        }
        "counts" {
            Invoke-SqlCsv "target_content_type_summary.csv" "SELECT coalesce(nullif(content_type,''),'(blank)') AS content_type, COUNT(*) AS artifact_count FROM artifacts GROUP BY coalesce(nullif(content_type,''),'(blank)') ORDER BY artifact_count DESC, content_type LIMIT $limitSql;" "content type counts"
            Invoke-SqlCsv "target_store_content_summary.csv" "SELECT store_guid, coalesce(nullif(content_type,''),'(blank)') AS content_type, COUNT(*) AS artifact_count FROM artifacts GROUP BY store_guid, coalesce(nullif(content_type,''),'(blank)') ORDER BY store_guid, artifact_count DESC LIMIT $limitSql;" "store-by-content type counts"
        }
    }
}
finally {
    Write-TargetedManifest
}
)PS1";
    log.info("Targeted export script written: " + pathString(caseDir / "Export-SpotlightTargetedData.ps1"));
}

void writeUploadZipScript(const fs::path& caseDir, Logger& log) {
    std::ofstream out(ioPath(caseDir / "Create-UploadZip.ps1"), std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write Create-UploadZip.ps1");
    out << R"PS1(# Vestigant Spotlight verified Upload ZIP helper
# Generated by Vestigant Spotlight.
# Creates a deterministic ZIP from the case Upload folder and verifies the ZIP before reporting success.
param(
    [string]$CaseDir = "",
    [string]$UploadDir = "",
    [string]$ZipPath = "",
    [switch]$IncludeTargetedExports,
    [string]$TargetedExportsDir = ""
)

$ErrorActionPreference = "Stop"

$ScriptRoot = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ScriptRoot)) { $ScriptRoot = (Get-Location).Path }

if ([string]::IsNullOrWhiteSpace($CaseDir)) {
    if ((Split-Path -Leaf $ScriptRoot) -ieq "Upload") {
        $CaseDir = Split-Path -Parent $ScriptRoot
    } else {
        $CaseDir = $ScriptRoot
    }
}

if ([string]::IsNullOrWhiteSpace($UploadDir)) {
    if ((Split-Path -Leaf $ScriptRoot) -ieq "Upload") {
        $UploadDir = $ScriptRoot
    } else {
        $UploadDir = Join-Path $CaseDir "Upload"
    }
}

if ([string]::IsNullOrWhiteSpace($TargetedExportsDir)) {
    $TargetedExportsDir = Join-Path $CaseDir "TargetedExports"
}

if (!(Test-Path -LiteralPath $CaseDir -PathType Container)) {
    throw "Case folder not found: $CaseDir"
}
if (!(Test-Path -LiteralPath $UploadDir -PathType Container)) {
    throw "Upload folder not found: $UploadDir"
}

if ([string]::IsNullOrWhiteSpace($ZipPath)) {
    $caseName = Split-Path -Leaf (Resolve-Path -LiteralPath $CaseDir).Path
    if ([string]::IsNullOrWhiteSpace($caseName)) { $caseName = "VestigantSpotlight" }
    $ZipPath = Join-Path $CaseDir ($caseName + "_Upload.zip")
}

$ZipParent = Split-Path -Parent $ZipPath
if (![string]::IsNullOrWhiteSpace($ZipParent)) {
    New-Item -ItemType Directory -Force -Path $ZipParent | Out-Null
}

$Stage = Join-Path ([System.IO.Path]::GetTempPath()) ("VestigantSpotlightUploadZip_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

try {
    Copy-Item -LiteralPath $UploadDir -Destination (Join-Path $Stage "Upload") -Recurse -Force

    if ($IncludeTargetedExports) {
        if (Test-Path -LiteralPath $TargetedExportsDir -PathType Container) {
            Copy-Item -LiteralPath $TargetedExportsDir -Destination (Join-Path $Stage "TargetedExports") -Recurse -Force
        } else {
            Write-Warning "TargetedExports folder not found; continuing without it: $TargetedExportsDir"
        }
    }

    $blocked = Get-ChildItem -LiteralPath $Stage -Recurse -Force -File |
        Where-Object { $_.Extension -match '^(?i)\.(sqlite|sqlite3|db|db3)$' }
    if ($blocked.Count -gt 0) {
        $blockedList = ($blocked | Select-Object -ExpandProperty FullName) -join "`n"
        throw "Refusing to zip database files in Upload package:`n$blockedList"
    }

    if (Test-Path -LiteralPath $ZipPath) {
)PS1" R"PS1(        Remove-Item -LiteralPath $ZipPath -Force
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $Stage,
        $ZipPath,
        [System.IO.Compression.CompressionLevel]::Optimal,
        $false
    )

    if (!(Test-Path -LiteralPath $ZipPath -PathType Leaf)) {
        throw "ZIP was not created: $ZipPath"
    }

    $zipInfo = Get-Item -LiteralPath $ZipPath
    if ($zipInfo.Length -le 0) {
        throw "ZIP was created but is empty: $ZipPath"
    }

    $zip = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        $entryCount = $zip.Entries.Count
    } finally {
        $zip.Dispose()
    }
    if ($entryCount -le 0) {
        throw "ZIP was created but contains no entries: $ZipPath"
    }

    Write-Host "ZIP_CREATED=$ZipPath"
    Write-Host "ZIP_BYTES=$($zipInfo.Length)"
    Write-Host "ZIP_ENTRIES=$entryCount"
    Write-Host "Upload ZIP verified."
}
finally {
    if (Test-Path -LiteralPath $Stage) {
        Remove-Item -LiteralPath $Stage -Recurse -Force -ErrorAction SilentlyContinue
    }
}
)PS1";
    log.info("Upload ZIP script written: " + pathString(caseDir / "Create-UploadZip.ps1"));
}

void writeExportIndexFile
(const fs::path& indexPath, const fs::path& exportDir, Logger& log) {
    fs::create_directories(indexPath.parent_path());
    std::ofstream out(ioPath(indexPath), std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write export index: " + pathString(indexPath));
    out << "export_name,export_path,manifest_path,total_row_count,chunked,generated_utc\n";
    const auto generated = nowUtc();
    std::size_t exportsIndexed = 0;
    std::error_code ec;
    for (const auto& de : fs::directory_iterator(exportDir, ec)) {
        if (ec) break;
        if (!de.is_regular_file()) continue;
        const auto p = de.path();
        const auto name = p.filename().string();
        const std::string suffix = "_manifest.csv";
        if (name.size() <= suffix.size() || name.substr(name.size() - suffix.size()) != suffix) continue;
        std::ifstream mf(p, std::ios::binary);
        if (!mf) continue;
        std::string line;
        std::getline(mf, line); // header
        long long totalRows = 0;
        long long parts = 0;
        std::string firstFile;
        while (std::getline(mf, line)) {
            if (line.empty()) continue;
            auto cols = splitSimpleCsv(line);
            if (cols.size() >= 3) {
                if (firstFile.empty()) firstFile = cols[0];
                try { totalRows += std::stoll(cols[2]); } catch (...) {}
                ++parts;
            }
        }
        std::string exportName = name.substr(0, name.size() - suffix.size()) + ".csv";
        fs::path exportPath = exportDir / (firstFile.empty() ? exportName : firstFile);
        out << csvEscape(exportName) << ','
            << csvEscape(pathString(exportPath)) << ','
            << csvEscape(pathString(p)) << ','
            << totalRows << ','
            << (parts > 1 ? "1" : "0") << ','
            << csvEscape(generated) << "\n";
        ++exportsIndexed;
    }
    log.info("Export index written: " + pathString(indexPath) + " exports=" + std::to_string(exportsIndexed));
}

} // namespace

void SqliteExporter::exportReviewPackage(CaseDatabase& db, const fs::path& exportDir, Logger& log, const std::string& exportProfile) const {
    fs::create_directories(exportDir);
    const std::string profile = exportProfile.empty() ? std::string("minimal") : exportProfile;
    const bool fullExport = (profile == "full");
    const bool diagnosticsExport = (profile == "diagnostics");
    const bool supportDataExport = fullExport || diagnosticsExport || profile == "support";
    log.info("Export profile=" + profile + (fullExport ? " (full CSV exports enabled)" : " (thin/minimal full-case CSV exports)"));
    appendExportRunStatus(exportDir / "EXPORT_INDEX.csv", 90, "export_profile_start", "profile=" + profile);
    writeParserLimitsAndSuppressionSummary(db, exportDir / "parser_limits_and_suppression_summary.csv", log);

    if (!fullExport) {
        exportQuery(db, exportDir / "source_probe_inventory.csv", "SELECT * FROM vw_source_probe_inventory ORDER BY created_utc DESC, source_probe_run_id DESC", log);
        exportQuery(db, exportDir / "source_probe_signatures.csv", "SELECT * FROM vw_source_probe_signatures ORDER BY source_id, offset_bytes, signature_name", log);
        exportQuery(db, exportDir / "source_partition_probe.csv", "SELECT * FROM vw_source_partition_inventory ORDER BY source_id, scheme, partition_index, offset_bytes", log);
        exportQuery(db, exportDir / "image_inventory_sources.csv", "SELECT * FROM vw_image_inventory_sources ORDER BY created_utc DESC, image_inventory_source_id DESC", log);
        exportQuery(db, exportDir / "active_file_comparison_readiness.csv", "SELECT * FROM vw_active_file_comparison_readiness ORDER BY created_utc DESC, image_inventory_source_id DESC", log);
        if (supportDataExport) exportQuery(db, exportDir / "spotlight_active_file_comparison.csv", "SELECT * FROM vw_spotlight_active_file_comparison ORDER BY artifact_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "image_file_inventory.csv", "SELECT * FROM vw_image_file_inventory ORDER BY source_id, apfs_volume_name, full_path", log);
        exportQuery(db, exportDir / "content_type_summary.csv", "SELECT * FROM vw_content_type_summary ORDER BY artifact_count DESC, content_type", log);
        exportQuery(db, exportDir / "store_content_type_summary.csv", "SELECT * FROM vw_store_content_type_summary ORDER BY store_guid, artifact_count DESC, content_type", log);
        exportQuery(db, exportDir / "usage_evidence.csv", "SELECT usage_id,artifact_id,source_id,store_guid,inode_num,field_name,field_value,parsed_utc FROM usage_evidence ORDER BY store_guid, CAST(inode_num AS INTEGER), usage_id", log);
        exportQuery(db, exportDir / "object_usage_summary.csv", "SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,logical_size_bytes,physical_size_bytes,use_count_value,open_count_estimate,used_dates_count,usage_earliest_utc,usage_latest_utc,fused_usage_dates_utc,usage_date_row_count,usage_evidence_row_count,usage_source_fields,object_usage_basis,where_froms,confidence FROM vw_object_usage_summary ORDER BY COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,''), artifact_id) DESC", log);
        exportQuery(db, exportDir / "date_field_inventory.csv", R"SQL(
SELECT field_name,
       COUNT(*) AS row_count,
       COUNT(DISTINCT source_id || '|' || store_guid || '|' || source_db || '|' || inode_num || '|' || COALESCE(store_id,'')) AS record_count,
       SUM(CASE WHEN COALESCE(parsed_utc,'')<>'' THEN 1 ELSE 0 END) AS parsed_utc_count,
       MIN(NULLIF(parsed_utc,'')) AS earliest_parsed_utc,
       MAX(NULLIF(parsed_utc,'')) AS latest_parsed_utc
FROM raw_date_candidates
GROUP BY field_name
ORDER BY row_count DESC, field_name
)SQL", log);
        exportQuery(db, exportDir / "timeline_date_source_summary.csv", R"SQL(
SELECT raw_spotlight_field,event_type_interpretation,COUNT(*) AS row_count,COUNT(DISTINCT artifact_id) AS artifact_count,MIN(parsed_utc) AS earliest_utc,MAX(parsed_utc) AS latest_utc,MAX(likely_snapshot_or_index_date) AS has_likely_snapshot_or_index_dates
FROM vw_date_field_attribution
WHERE COALESCE(parsed_utc,'')<>''
GROUP BY raw_spotlight_field,event_type_interpretation
ORDER BY row_count DESC, raw_spotlight_field
)SQL", log);
        if (supportDataExport) exportQuery(db, exportDir / "snapshot_date_warnings.csv", "SELECT * FROM vw_snapshot_date_warnings ORDER BY parsed_utc DESC, artifact_id", log);
        exportQuery(db, exportDir / "usage_timeline_attributed.csv", "SELECT * FROM vw_usage_timeline_attributed ORDER BY event_utc DESC, timeline_id DESC", log);
        if (supportDataExport) exportQuery(db, exportDir / "usage_event_detail_attributed_raw.csv", "SELECT * FROM vw_usage_event_detail_attributed ORDER BY event_utc DESC, usage_event_id DESC", log);
        if (supportDataExport) exportQuery(db, exportDir / "artifact_dates_wide.csv", "SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,logical_size_bytes,physical_size_bytes,content_type,where_froms,created_earliest_utc,created_latest_utc,modified_earliest_utc,modified_latest_utc,downloaded_earliest_utc,downloaded_latest_utc,usage_earliest_utc,usage_latest_utc,interesting_or_index_earliest_utc,interesting_or_index_latest_utc,likely_snapshot_date_count,associated_date_count,unassociated_date_count,available_date_fields,association_confidence_summary,snapshot_warning_reasons FROM artifact_date_summary ORDER BY artifact_id", log);
        if (diagnosticsExport) {
            exportQuery(db, exportDir / "parser_coverage_summary.csv", "SELECT summary_id,source_id,metric_name,metric_value,created_utc FROM parser_coverage_summary ORDER BY summary_id", log);
            exportQuery(db, exportDir / "native_decode_attempts.csv", "SELECT native_decode_attempt_id,source_id,store_guid,source_db,decode_mode,spotlight_version,properties_count,categories_count,metadata_blocks,decompressed_blocks,raw_records,raw_key_values,raw_date_candidates,fallback_header_only_items,failures,started_utc,finished_utc,status,message FROM native_decode_attempts ORDER BY native_decode_attempt_id", log);
            exportQuery(db, exportDir / "native_decode_errors.csv", "SELECT failure_id AS error_id,source_id,store_guid,source_db,phase AS stage,message AS error_message,'' AS context,created_utc FROM raw_failures ORDER BY failure_id", log);
            exportQuery(db, exportDir / "native_partial_decode_errors.csv", "SELECT raw_kv_id AS partial_error_id,source_id,store_guid,source_db,inode_num,field_name AS property_name,field_value AS error_message,'' AS context FROM raw_key_values WHERE field_name='__decode_error' ORDER BY raw_kv_id", log);
        }
        exportQuery(db, exportDir / "ios_store_parse_summary.csv", "SELECT * FROM vw_ios_store_parse_summary ORDER BY raw_record_count DESC, store_guid, source_db", log);
        exportQuery(db, exportDir / "ios_protection_class_summary.csv", "SELECT * FROM vw_ios_protection_class_summary ORDER BY raw_record_count DESC, protection_class", log);
        exportQuery(db, exportDir / "ios_artifact_hint_summary.csv", "SELECT * FROM vw_ios_artifact_hint_summary ORDER BY string_probe_rows DESC, artifact_hint", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_record_investigation_hints.csv", "SELECT * FROM vw_ios_record_investigation_hints ORDER BY last_updated_utc DESC, protection_class, primary_investigation_hint, store_guid, CAST(inode_num AS INTEGER), raw_record_id", log);
        exportQuery(db, exportDir / "ios_string_probe_category_summary.csv", R"SQL(
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
GROUP BY probe_category
ORDER BY row_count DESC, probe_category
)SQL", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_string_probe_values.csv", "SELECT * FROM vw_ios_string_probe_values ORDER BY probe_category, store_guid, CAST(inode_num AS INTEGER), raw_kv_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_record_string_probe_summary.csv", "SELECT * FROM vw_ios_record_string_probe_summary ORDER BY last_updated_utc DESC, store_guid, CAST(inode_num AS INTEGER), raw_record_id", log);
        exportQuery(db, exportDir / "ios_domain_url_summary.csv", R"SQL(
WITH values_src AS (
  SELECT store_guid, source_db, inode_num, field_value
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
    AND (LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' OR LOWER(field_value) LIKE '%file:%')
), normalized AS (
  SELECT store_guid,
         source_db,
         inode_num,
         CASE
           WHEN LOWER(field_value) LIKE '%file:%' THEN 'FILE_URL_OR_PATH'
           WHEN LOWER(field_value) LIKE '%icloud%' THEN 'ICLOUD'
           WHEN LOWER(field_value) LIKE '%dropbox%' THEN 'DROPBOX'
           WHEN LOWER(field_value) LIKE '%drive.google%' OR LOWER(field_value) LIKE '%google drive%' THEN 'GOOGLE_DRIVE'
           WHEN LOWER(field_value) LIKE '%google%' THEN 'GOOGLE'
           WHEN LOWER(field_value) LIKE '%microsoft%' OR LOWER(field_value) LIKE '%onedrive%' THEN 'MICROSOFT_OR_ONEDRIVE'
           ELSE 'OTHER_WEB_OR_URL'
         END AS domain_bucket,
         substr(field_value,1,250) AS sample_value
  FROM values_src
)
SELECT domain_bucket,
       COUNT(*) AS row_count,
       COUNT(DISTINCT store_guid) AS store_count,
       COUNT(DISTINCT inode_num) AS distinct_record_count,
       substr(MIN(sample_value),1,250) AS min_sample_value,
       substr(MAX(sample_value),1,250) AS max_sample_value
FROM normalized
GROUP BY domain_bucket
ORDER BY row_count DESC, domain_bucket
)SQL", log);
        exportQuery(db, exportDir / "ios_redacted_investigation_summary.csv", R"SQL(
WITH probe AS (
  SELECT store_guid, source_db, inode_num, LOWER(field_value) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), categorized AS (
  SELECT store_guid, source_db, inode_num,
         CASE
           WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'URL_OR_WEB_LINK'
           WHEN v LIKE '%@%' AND v LIKE '%.%' THEN 'EMAIL_ADDRESS_OR_ACCOUNT'
           WHEN v LIKE '%imessage%' OR v LIKE '%sms%' OR v LIKE '%message%' THEN 'MESSAGE_TEXT_OR_MESSAGE_APP'
           WHEN v LIKE '%icloud%' OR v LIKE '%onedrive%' OR v LIKE '%dropbox%' OR v LIKE '%google drive%' OR v LIKE '%drive.google%' THEN 'CLOUD_STORAGE_OR_SYNC'
           WHEN v LIKE '%calendar%' OR v LIKE '%invite%' OR v LIKE '%rsvp%' OR v LIKE '%event%' THEN 'CALENDAR_OR_INVITATION'
           WHEN v LIKE 'file:%' OR v LIKE '/private/var/%' OR v LIKE '%/mobile/%' THEN 'FILE_OR_IOS_PATH'
           ELSE 'OTHER_STRING_PROBE'
         END AS probe_category
  FROM probe
)
SELECT probe_category,
       store_guid,
       source_db,
       COUNT(*) AS string_probe_rows,
       COUNT(DISTINCT inode_num) AS distinct_records,
       'values_redacted_see_ios_string_probe_values_for_case_review' AS redaction_note
FROM categorized
GROUP BY probe_category, store_guid, source_db
ORDER BY probe_category, string_probe_rows DESC, store_guid, source_db
)SQL", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_timeline_index_updates.csv", "SELECT * FROM vw_ios_timeline_index_updates ORDER BY last_updated_utc DESC, store_guid, CAST(inode_num AS INTEGER), raw_record_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_ffs_file_inventory.csv", "SELECT * FROM vw_ios_ffs_file_inventory ORDER BY normalized_path", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_app_database_inventory.csv", "SELECT * FROM vw_ios_database_artifact_inventory ORDER BY database_category, app_hint, normalized_path", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_app_database_record_inventory.csv", "SELECT * FROM vw_ios_app_database_record_inventory ORDER BY database_category, database_name, table_name", log);
        exportQuery(db, exportDir / "ios_app_database_record_summary.csv", "SELECT * FROM vw_ios_app_database_record_summary ORDER BY database_category, record_category", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_app_parsed_records.csv", "SELECT * FROM vw_ios_app_parsed_records ORDER BY database_category, record_category, record_timestamp_utc, database_name, table_name, ios_app_record_id", log);
        exportQuery(db, exportDir / "ios_app_parsed_record_summary.csv", "SELECT * FROM vw_ios_app_parsed_record_summary ORDER BY database_category, record_category", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_apple_messages_parsed_records.csv", "SELECT * FROM vw_ios_apple_messages_parsed_records ORDER BY record_timestamp_utc, ios_app_record_id", log);
        exportQuery(db, exportDir / "ios_apple_messages_parsed_summary.csv", "SELECT * FROM vw_ios_apple_messages_parsed_summary ORDER BY record_category, parse_status", log);
        exportQuery(db, exportDir / "ios_apple_messages_database_status.csv", "SELECT * FROM vw_ios_apple_messages_database_status ORDER BY apple_messages_residency_status, normalized_path", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_app_live_activity_timeline.csv", "SELECT * FROM vw_ios_app_live_activity_timeline ORDER BY record_timestamp_utc DESC, database_category, record_category, ios_app_record_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_communications_review_records.csv", "SELECT * FROM vw_ios_communications_review_records ORDER BY communication_source, record_timestamp_utc DESC, communication_record_type, ios_app_record_id", log);
        exportQuery(db, exportDir / "ios_communications_review_summary.csv", "SELECT * FROM vw_ios_communications_review_summary ORDER BY communication_source, record_category, communication_record_type", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_communication_candidates.csv", "SELECT * FROM vw_ios_spotlight_communication_candidates ORDER BY communication_candidate_type, raw_kv_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_contact_identity_records.csv", "SELECT * FROM vw_ios_contact_identity_records ORDER BY contact_identity_type, database_name, table_name, ios_app_record_id", log);
        exportQuery(db, exportDir / "ios_contact_identity_summary.csv", "SELECT * FROM vw_ios_contact_identity_summary ORDER BY contact_review_row_count DESC, database_name, table_name", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_web_history_review_records.csv", "SELECT * FROM vw_ios_web_history_review_records ORDER BY record_timestamp_utc DESC, database_name, table_name, ios_app_record_id", log);
        exportQuery(db, exportDir / "ios_web_history_review_summary.csv", "SELECT * FROM vw_ios_web_history_review_summary ORDER BY web_review_row_count DESC, database_name, table_name", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_calendar_review_records.csv", "SELECT * FROM vw_ios_calendar_review_records ORDER BY record_timestamp_utc DESC, database_name, table_name, ios_app_record_id", log);
        exportQuery(db, exportDir / "ios_calendar_review_summary.csv", "SELECT * FROM vw_ios_calendar_review_summary ORDER BY calendar_review_row_count DESC, database_name, table_name", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_investigation_keyword_surface.csv", "SELECT * FROM vw_ios_investigation_keyword_surface ORDER BY review_priority, surface_source, record_timestamp_utc DESC, source_record_id", log);
        exportQuery(db, exportDir / "ios_whatsapp_database_status.csv", "SELECT * FROM vw_ios_whatsapp_database_status ORDER BY whatsapp_residency_status, normalized_path", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_whatsapp_parsed_records.csv", "SELECT * FROM vw_ios_whatsapp_parsed_records ORDER BY record_timestamp_utc, ios_app_record_id", log);
        exportQuery(db, exportDir / "ios_whatsapp_parsed_summary.csv", "SELECT * FROM vw_ios_whatsapp_parsed_summary ORDER BY record_category, parse_status", log);
        exportQuery(db, exportDir / "ios_keychain_material_inventory.csv", "SELECT * FROM vw_ios_keychain_material_inventory ORDER BY keychain_material_type, normalized_path", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_keychain_support_reference_inventory.csv", "SELECT * FROM vw_ios_keychain_support_reference_inventory ORDER BY normalized_path", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_referenced_paths.csv", "SELECT * FROM vw_ios_spotlight_referenced_paths ORDER BY reference_type, normalized_ios_path, reference_id", log);
        exportQuery(db, exportDir / "ios_spotlight_decode_coverage_summary.csv", "SELECT * FROM vw_ios_spotlight_decode_coverage_summary ORDER BY raw_record_count DESC, store_guid", log);
        exportQuery(db, exportDir / "ios_spotlight_field_coverage_summary.csv", "SELECT * FROM vw_ios_spotlight_field_coverage_summary ORDER BY value_row_count DESC, store_guid, field_name", log);
        exportQuery(db, exportDir / "ios_spotlight_text_category_summary.csv", "SELECT * FROM vw_ios_spotlight_text_category_summary ORDER BY text_value_count DESC, human_text_category", log);
        exportQuery(db, exportDir / "ios_spotlight_text_context_priority_summary.csv", "SELECT * FROM vw_ios_spotlight_text_context_priority_summary ORDER BY review_priority_sort, text_context_record_count DESC", log);
        exportQuery(db, exportDir / "ios_spotlight_chat_app_attribution_summary.csv", "SELECT * FROM vw_ios_spotlight_chat_app_attribution_summary ORDER BY text_context_category, context_record_count DESC", log);
        exportQuery(db, exportDir / "ios_spotlight_communication_summary.csv", "SELECT * FROM vw_ios_spotlight_communication_summary ORDER BY review_priority_sort, spotlight_record_count DESC, communication_context_type", log);
        exportQuery(db, exportDir / "ios_spotlight_communication_record_review_sample.csv", "SELECT * FROM vw_ios_spotlight_communication_record_review ORDER BY review_priority_sort, spotlight_date_utc DESC, raw_record_id DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_message_text_review_sample.csv", "SELECT * FROM vw_ios_spotlight_message_text_review ORDER BY review_priority_sort, spotlight_date_utc DESC, raw_record_id DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_message_body_review_sample.csv", "SELECT * FROM vw_ios_spotlight_message_body_review ORDER BY noise_hint, spotlight_date_utc DESC, raw_record_id DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_user_focus_message_review_sample.csv", "SELECT * FROM vw_ios_spotlight_user_focus_message_review ORDER BY spotlight_date_utc DESC, raw_record_id DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_message_contact_summary.csv", "SELECT * FROM vw_ios_spotlight_message_contact_summary ORDER BY noise_hint, spotlight_record_count DESC, latest_spotlight_date_utc DESC", log);
        exportQuery(db, exportDir / "ios_spotlight_message_contact_thread_detail_sample.csv", "SELECT * FROM vw_ios_spotlight_message_contact_thread_detail_sample ORDER BY CASE WHEN noise_hint='USER_REVIEW_CANDIDATE' THEN 0 ELSE 1 END, rows_with_extracted_message_text DESC, spotlight_record_count DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_message_body_focus_summary.csv", "SELECT * FROM vw_ios_spotlight_message_body_focus_summary ORDER BY spotlight_record_count DESC, noise_hint, body_review_bucket", log);
        exportQuery(db, exportDir / "ios_spotlight_noise_reduction_summary.csv", "SELECT * FROM vw_ios_spotlight_noise_reduction_summary ORDER BY spotlight_record_count DESC, noise_hint, body_review_bucket", log);
        exportQuery(db, exportDir / "ios_spotlight_normalized_timeline_sample.csv", "SELECT * FROM vw_ios_spotlight_normalized_timeline ORDER BY event_time_utc DESC, raw_record_id DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_timeline_anomaly_summary.csv", "SELECT * FROM vw_ios_spotlight_timeline_anomaly_summary ORDER BY date_anomaly_flag, timeline_row_count DESC", log);
        exportQuery(db, exportDir / "parser_diagnostics_summary.csv", "SELECT * FROM vw_parser_diagnostics_summary ORDER BY diagnostic_count DESC, diagnostic_source, diagnostic_category", log);
        exportQuery(db, exportDir / "parser_diagnostics_action_summary.csv", "SELECT * FROM vw_parser_diagnostics_action_summary ORDER BY CASE diagnostic_severity WHEN 'HIGH_VOLUME_NATIVE_ID_RANGE_GAP' THEN 0 WHEN 'HIGH_VOLUME_PARSER_DIAGNOSTIC' THEN 1 WHEN 'PARSER_GAP_REVIEW' THEN 2 ELSE 9 END, diagnostic_count DESC", log);
        exportQuery(db, exportDir / "ios_spotlight_plaso_l2tcsv_timeline_sample.csv", "SELECT * FROM vw_ios_spotlight_plaso_l2tcsv_timeline_sample ORDER BY date DESC, time DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_case_quality_dashboard.csv", "SELECT * FROM vw_ios_spotlight_case_quality_dashboard ORDER BY quality_area, metric", log);
        exportQuery(db, exportDir / "parser_diagnostics_detail_sample.csv", "SELECT * FROM vw_parser_diagnostics_detail_sample ORDER BY diagnostic_source, diagnostic_row_id LIMIT 5000", log);
        exportQuery(db, exportDir / "case_provenance_summary.csv", "SELECT * FROM vw_case_provenance_summary ORDER BY provenance_scope, provenance_key", log);
        exportQuery(db, exportDir / "ios_spotlight_message_media_review_sample.csv", "SELECT * FROM vw_ios_spotlight_message_media_review ORDER BY spotlight_date_utc DESC, raw_record_id DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_attachment_reference_review_sample.csv", "SELECT * FROM vw_ios_spotlight_attachment_reference_review ORDER BY spotlight_date_utc DESC, communication_context_type, raw_record_id DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_high_value_text_context_review_sample.csv", "SELECT * FROM vw_ios_spotlight_high_value_text_context_review ORDER BY review_priority_sort, last_updated_utc DESC, raw_record_id DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_text_context_review_sample.csv", "SELECT * FROM vw_ios_spotlight_text_context_review ORDER BY review_priority_sort, last_updated_utc DESC, raw_record_id DESC LIMIT 5000", log);
        exportQuery(db, exportDir / "ios_spotlight_object_inode_diagnostic_summary.csv", "SELECT * FROM vw_ios_spotlight_object_inode_diagnostic_summary ORDER BY raw_record_count DESC, object_count DESC, source_id, store_guid, object_record_bucket", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_object_inode_summary.csv", "SELECT * FROM vw_ios_spotlight_object_inode_summary ORDER BY raw_record_count DESC, raw_key_value_rows DESC, latest_last_updated_utc DESC, source_id, store_guid, spotlight_inode_or_object_id, spotlight_store_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_record_review.csv", "SELECT * FROM vw_ios_spotlight_record_review ORDER BY spotlight_review_priority, spotlight_date_utc DESC, raw_record_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_date_provenance.csv", "SELECT * FROM vw_ios_spotlight_date_provenance ORDER BY spotlight_date_utc DESC, store_guid, CAST(spotlight_inode_or_object_id AS INTEGER), raw_record_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_investigative_items_with_dates.csv", "SELECT * FROM vw_ios_spotlight_investigative_items_with_dates ORDER BY review_priority, spotlight_date_utc DESC, raw_kv_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_investigative_item_date_evidence.csv", "SELECT * FROM vw_ios_spotlight_investigative_item_date_evidence ORDER BY review_priority, spotlight_date_utc DESC, raw_kv_id, raw_date_id", log);
        exportQuery(db, exportDir / "ios_spotlight_date_field_summary.csv", "SELECT * FROM vw_ios_spotlight_date_field_summary ORDER BY date_candidate_count DESC, store_guid, spotlight_date_source_field", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_high_value_timeline.csv", "SELECT * FROM vw_ios_spotlight_high_value_timeline ORDER BY review_priority, spotlight_date_utc DESC, raw_record_id, raw_kv_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_file_reference_review.csv", "SELECT * FROM vw_ios_spotlight_file_reference_review ORDER BY file_reference_status, spotlight_date_utc DESC, raw_record_id, raw_kv_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_url_reference_review.csv", "SELECT * FROM vw_ios_spotlight_url_reference_review ORDER BY human_text_category, spotlight_date_utc DESC, raw_record_id, raw_kv_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_account_contact_reference_review.csv", "SELECT * FROM vw_ios_spotlight_account_contact_reference_review ORDER BY human_text_category, spotlight_date_utc DESC, raw_record_id, raw_kv_id", log);
        exportQuery(db, exportDir / "ios_spotlight_decode_gap_summary.csv", "SELECT * FROM vw_ios_spotlight_decode_gap_summary ORDER BY gap_record_count DESC, store_guid", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_entity_review.csv", "SELECT * FROM vw_ios_spotlight_entity_review ORDER BY review_priority, entity_type, spotlight_date_utc DESC, raw_record_id, raw_kv_id", log);
        exportQuery(db, exportDir / "ios_spotlight_entity_summary.csv", "SELECT * FROM vw_ios_spotlight_entity_summary ORDER BY entity_row_count DESC, entity_type, store_guid, spotlight_value_source_field", log);
        exportQuery(db, exportDir / "ios_spotlight_native_parser_targets.csv", "SELECT * FROM vw_ios_spotlight_native_parser_targets ORDER BY parser_priority, target_count DESC, parser_target_type, store_guid", log);
        exportQuery(db, exportDir / "ios_spotlight_dbstr_map_inventory.csv", "SELECT * FROM vw_ios_spotlight_dbstr_map_inventory ORDER BY store_guid, source_db, map_id", log);
        exportQuery(db, exportDir / "ios_spotlight_dictionary_coverage.csv", "SELECT * FROM vw_ios_spotlight_dictionary_coverage ORDER BY raw_record_count DESC, store_guid, source_db", log);
        exportQuery(db, exportDir / "ios_spotlight_apple_field_coverage.csv", "SELECT * FROM vw_ios_spotlight_apple_field_coverage ORDER BY value_row_count DESC, apple_semantic_group, field_name", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_decode_gap_records.csv", "SELECT * FROM vw_ios_spotlight_decode_gap_records ORDER BY last_updated_utc DESC, store_guid, raw_record_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_human_text_values.csv", "SELECT * FROM vw_ios_spotlight_human_text_values ORDER BY review_priority, human_text_category, raw_kv_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_human_text_rollup.csv", "SELECT * FROM vw_ios_spotlight_human_text_rollup ORDER BY has_high_review_value_text DESC, last_updated_utc DESC, raw_record_id", log);
        exportQuery(db, exportDir / "ios_spotlight_missing_from_ffs_summary.csv", "SELECT * FROM vw_ios_spotlight_missing_from_ffs_summary ORDER BY investigative_priority_sort, missing_candidate_count DESC, store_guid, field_name, reference_type", log);
        exportQuery(db, exportDir / "ios_spotlight_missing_from_ffs_high_value_summary.csv", "SELECT * FROM vw_ios_spotlight_missing_from_ffs_high_value_summary ORDER BY investigative_priority_sort, missing_candidate_count DESC, store_guid, field_name, reference_type", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_missing_from_ffs_candidates.csv", "SELECT * FROM vw_ios_spotlight_missing_from_ffs_candidates ORDER BY investigative_priority_sort, residency_status, confidence, normalized_ios_path, reference_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_missing_from_ffs_high_value_candidates.csv", "SELECT * FROM vw_ios_spotlight_missing_from_ffs_high_value_candidates ORDER BY investigative_priority_sort, residency_status, confidence, normalized_ios_path, reference_id", log);
        exportQuery(db, exportDir / "ios_spotlight_residency_summary.csv", "SELECT * FROM vw_ios_spotlight_residency_summary ORDER BY residency_status, confidence", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_database_residency_candidates.csv", "SELECT * FROM vw_ios_database_residency_candidates ORDER BY object_category, database_residency_status, candidate_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_object_identity.csv", "SELECT * FROM vw_ios_spotlight_object_identity ORDER BY protection_class, store_guid, CAST(spotlight_inode_or_object_id AS INTEGER), raw_record_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_to_ffs_object_links.csv", "SELECT * FROM vw_ios_spotlight_to_ffs_object_links ORDER BY residency_status, confidence, normalized_ios_path, reference_id", log);
        if (supportDataExport) exportQuery(db, exportDir / "ios_spotlight_to_app_db_record_links.csv", "SELECT * FROM vw_ios_spotlight_to_app_db_record_links ORDER BY app_db_link_status, database_category, candidate_id", log);
        appendExportRunStatus(exportDir / "upload_samples" / "upload_samples_manifest.csv", 95, "export_upload_samples_start", "bounded upload samples");
        exportUploadSamples(db, exportDir / "upload_samples", log);
        appendExportRunStatus(exportDir / "upload_samples" / "upload_samples_manifest.csv", 96, "export_upload_samples_complete", "bounded upload samples complete");
        writeCaseReviewSummary(db, exportDir.parent_path() / "CASE_REVIEW_SUMMARY.txt", log);
        writeReviewIndex(exportDir.parent_path() / "review_index.html", log);
        writeInvestigatorDashboard(db, exportDir.parent_path() / "investigator_dashboard.html", log);
        writeUploadReadme(exportDir.parent_path(), log);
        writeTargetedExportReadme(exportDir.parent_path(), log);
        writeTargetedExportScript(exportDir.parent_path(), log);
        writeUploadZipScript(exportDir.parent_path(), log);
        writeExportIndexFile(exportDir / "EXPORT_INDEX.csv", exportDir, log); if (exportDir.has_parent_path()) writeExportIndexFile(exportDir.parent_path() / "EXPORT_INDEX.csv", exportDir, log);
        appendExportRunStatus(exportDir / "EXPORT_INDEX.csv", 97, "export_complete", "profile=" + profile);
        return;
    }

    exportQuery(db, exportDir / "artifacts_review.csv", R"SQL(
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,v7_full_path_raw,spotlight_display_path,normalized_mac_path,filesystem_lookup_path,path_source,path_status,is_mounted_volume_path,mounted_volume_name,external_volume_reason,content_type,content_type_tree,where_froms,authors,creator,logical_size_bytes,physical_size_bytes,last_updated_utc,downloaded_date_utc,used_dates_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,usage_field_summary,open_count_estimate,index_text_snippet,existence_status,matched_filesystem_path,deleted_or_orphaned_candidate,orphan_reason,confidence
FROM artifacts ORDER BY store_guid, CAST(inode_num AS INTEGER)
)SQL", log);
    exportQuery(db, exportDir / "artifact_summary.csv", R"SQL(
WITH kv_counts AS (
  SELECT source_id, store_guid, inode_num, COUNT(*) AS raw_metadata_value_count,
         SUM(CASE WHEN field_name='__decode_error' THEN 1 ELSE 0 END) AS partial_decode_error_count
  FROM raw_key_values
  GROUP BY source_id, store_guid, inode_num
),
date_counts AS (
  SELECT source_id, store_guid, inode_num, COUNT(*) AS date_candidate_count,
         MIN(CASE WHEN field_name IN ('kMDItemContentCreationDate','_kMDItemCreationDate') THEN parsed_utc ELSE NULL END) AS content_created_candidate_utc,
         MAX(CASE WHEN field_name IN ('kMDItemContentModificationDate','_kMDItemContentChangeDate') THEN parsed_utc ELSE NULL END) AS content_modified_candidate_utc
  FROM raw_date_candidates
  GROUP BY source_id, store_guid, inode_num
)
SELECT a.artifact_id,
       a.source_id,
       a.store_guid AS store_id,
       a.inode_num AS inode,
       a.parent_inode_num AS parent_inode,
       a.file_name AS filename_candidate,
       a.display_name,
       a.best_path AS path_candidate,
       a.confidence AS path_confidence,
       a.content_type,
       a.content_type_tree,
       a.where_froms,
       a.authors,
       a.creator,
       a.last_updated_utc AS metadata_last_updated_utc,
       a.downloaded_date_utc,
       dc.content_created_candidate_utc,
       dc.content_modified_candidate_utc,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       CASE WHEN COALESCE(a.usage_field_summary,'')<>'' OR COALESCE(a.used_dates_count,0)>0 OR COALESCE(a.last_used_date_utc,'')<>'' THEN 1 ELSE 0 END AS has_usage_evidence,
       CASE WHEN COALESCE(a.where_froms,'')<>'' THEN 1 ELSE 0 END AS has_where_froms,
       COALESCE(kc.raw_metadata_value_count,0) AS raw_metadata_value_count,
       COALESCE(dc.date_candidate_count,0) AS date_candidate_count,
       0 AS decode_error_count,
       COALESCE(kc.partial_decode_error_count,0) AS partial_decode_error_count,
       a.use_count_value,
       a.open_count_estimate,
       a.usage_field_summary,
       a.existence_status,
       a.deleted_or_orphaned_candidate
FROM artifacts a
LEFT JOIN kv_counts kc ON kc.source_id=a.source_id AND kc.store_guid=a.store_guid AND kc.inode_num=a.inode_num
LEFT JOIN date_counts dc ON dc.source_id=a.source_id AND dc.store_guid=a.store_guid AND dc.inode_num=a.inode_num
ORDER BY a.store_guid, CAST(a.inode_num AS INTEGER), a.artifact_id
)SQL", log);

    exportQuery(db, exportDir / "artifact_source_instances.csv", R"SQL(
SELECT instance_id,artifact_id,raw_record_id,source_id,store_guid,inode_num,source_db,source_db_role,last_updated_utc,file_name,best_path
FROM artifact_source_instances ORDER BY store_guid, CAST(inode_num AS INTEGER), source_db_role
)SQL", log);
    exportQuery(db, exportDir / "source_copy_comparison.csv", R"SQL(
SELECT comparison_id,source_id,store_guid,inode_num,source_instance_count,has_store_db,has_dotstore_db,comparison_status,preferred_source_db
FROM source_copy_comparison ORDER BY store_guid, CAST(inode_num AS INTEGER)
)SQL", log);
    exportQuery(db, exportDir / "image_inventory_sources.csv", "SELECT * FROM vw_image_inventory_sources ORDER BY created_utc DESC, image_inventory_source_id DESC", log);
    exportQuery(db, exportDir / "image_file_inventory.csv", "SELECT * FROM vw_image_file_inventory ORDER BY source_id, apfs_volume_name, full_path", log);
    exportQuery(db, exportDir / "active_file_comparison_readiness.csv", "SELECT * FROM vw_active_file_comparison_readiness ORDER BY created_utc DESC, image_inventory_source_id DESC", log);
    exportQuery(db, exportDir / "spotlight_active_file_comparison.csv", "SELECT * FROM vw_spotlight_active_file_comparison ORDER BY artifact_id", log);
    exportQuery(db, exportDir / "parent_inode_links.csv", R"SQL(
SELECT link_id,source_id,store_guid,child_artifact_id,child_inode_num,child_parent_inode_num,child_file_name,child_best_path,parent_artifact_id,parent_inode_num,parent_file_name,parent_best_path,sibling_group_key,sibling_count,relationship_status,path_reconstruction_method,reconstructed_path_candidate,confidence
FROM parent_inode_links
ORDER BY source_id, store_guid, CAST(child_parent_inode_num AS INTEGER), CAST(child_inode_num AS INTEGER), link_id
)SQL", log);
    exportQuery(db, exportDir / "path_reconstruction_applied.csv", R"SQL(
SELECT link_id,source_id,store_guid,artifact_id,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,parent_artifact_id,resolved_parent_inode_num,parent_file_name,parent_best_path,reconstructed_path_candidate,applied_to_artifact_path,candidate_matches_artifact_path,sibling_group_key,sibling_count,relationship_status,path_reconstruction_method,confidence
FROM vw_path_reconstruction
ORDER BY store_guid, CAST(parent_inode_num AS INTEGER), CAST(inode_num AS INTEGER), link_id
)SQL", log);
    exportQuery(db, exportDir / "same_folder_groups.csv", R"SQL(
SELECT source_id,
       store_guid,
       child_parent_inode_num AS parent_inode_num,
       MAX(parent_artifact_id) AS parent_artifact_id,
       CASE WHEN SUM(CASE WHEN COALESCE(NULLIF(trim(parent_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) > 0 THEN MAX(CASE WHEN COALESCE(NULLIF(trim(parent_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN parent_file_name ELSE NULL END) ELSE '' END AS parent_file_name,
       CASE WHEN SUM(CASE WHEN COALESCE(NULLIF(trim(parent_best_path),''),'') NOT IN ('/','------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) > 0 THEN MAX(CASE WHEN COALESCE(NULLIF(trim(parent_best_path),''),'') NOT IN ('/','------NONAME------','(null)','NULL','.') THEN parent_best_path ELSE NULL END) ELSE '' END AS parent_best_path,
       COUNT(*) AS child_count,
       SUM(CASE WHEN relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE' THEN 1 ELSE 0 END) AS matched_parent_child_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) AS valid_child_name_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') IN ('','------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) AS missing_child_name_count,
       MIN(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END) AS first_valid_child_name,
       MAX(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END) AS last_valid_child_name,
       CASE
         WHEN SUM(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) > 0 THEN 'SAME_FOLDER_GROUP_WITH_AT_LEAST_ONE_CHILD_NAME'
         ELSE 'SAME_FOLDER_GROUP_NO_CHILD_NAMES'
       END AS same_folder_group_status,
       MAX(confidence) AS max_confidence,
       sibling_group_key
FROM parent_inode_links
GROUP BY source_id, store_guid, child_parent_inode_num, sibling_group_key
HAVING COUNT(*) > 1
ORDER BY child_count DESC, source_id, store_guid, CAST(child_parent_inode_num AS INTEGER)
)SQL", log);
    exportQuery(db, exportDir / "folder_summary.csv", R"SQL(
SELECT source_id,
       store_guid,
       child_parent_inode_num AS parent_inode,
       MAX(parent_artifact_id) AS parent_artifact_id,
       CASE WHEN SUM(CASE WHEN COALESCE(NULLIF(trim(parent_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) > 0 THEN MAX(CASE WHEN COALESCE(NULLIF(trim(parent_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN parent_file_name ELSE NULL END) ELSE '' END AS strongest_folder_name_candidate,
       CASE WHEN SUM(CASE WHEN COALESCE(NULLIF(trim(parent_best_path),''),'') NOT IN ('/','------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) > 0 THEN MAX(CASE WHEN COALESCE(NULLIF(trim(parent_best_path),''),'') NOT IN ('/','------NONAME------','(null)','NULL','.') THEN parent_best_path ELSE NULL END) ELSE '' END AS path_candidate,
       COUNT(*) AS child_count,
       SUM(CASE WHEN relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE' THEN 1 ELSE 0 END) AS resolved_parent_link_count,
       SUM(CASE WHEN COALESCE(reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END) AS reconstructed_child_path_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) AS child_name_count,
       MIN(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END) AS first_child_name,
       MAX(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END) AS last_child_name,
       GROUP_CONCAT(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END, '; ') AS child_filename_candidates,
       CASE
         WHEN SUM(CASE WHEN COALESCE(reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END)>0 THEN 'MEDIUM_RECONSTRUCTED_CHILD_PATHS_PRESENT'
         WHEN SUM(CASE WHEN relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE' THEN 1 ELSE 0 END)>0 THEN 'LOW_PARENT_INODE_MATCH_WITHOUT_FULL_PATH'
         ELSE 'LOW_SAME_PARENT_INODE_GROUP_ONLY'
       END AS confidence,
       sibling_group_key
FROM parent_inode_links
GROUP BY source_id, store_guid, child_parent_inode_num, sibling_group_key
HAVING COUNT(*) > 1
ORDER BY child_count DESC, source_id, store_guid, CAST(child_parent_inode_num AS INTEGER)
)SQL", log);

    exportQuery(db, exportDir / "parent_inode_quality_summary.csv", R"SQL(
SELECT source_id,
       store_guid,
       relationship_status,
       path_reconstruction_method,
       confidence,
       COUNT(*) AS row_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) AS rows_with_child_name,
       SUM(CASE WHEN COALESCE(reconstructed_path_candidate,'') <> '' THEN 1 ELSE 0 END) AS rows_with_reconstructed_path
FROM parent_inode_links
GROUP BY source_id, store_guid, relationship_status, path_reconstruction_method, confidence
ORDER BY row_count DESC, source_id, store_guid, relationship_status, path_reconstruction_method
)SQL", log);
    exportQuery(db, exportDir / "path_reconstruction_candidates.csv", R"SQL(
SELECT link_id,source_id,store_guid,child_inode_num,child_parent_inode_num,child_file_name,parent_inode_num,parent_file_name,parent_best_path,reconstructed_path_candidate,path_reconstruction_method,relationship_status,confidence,sibling_count
FROM parent_inode_links
WHERE COALESCE(reconstructed_path_candidate,'') <> ''
ORDER BY source_id, store_guid, CAST(child_inode_num AS INTEGER), link_id
)SQL", log);
    exportQuery(db, exportDir / "object_usage_summary.csv", R"SQL(
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,spotlight_display_path,normalized_mac_path,path_source,path_status,content_type,content_type_tree,logical_size_bytes,physical_size_bytes,where_froms,authors,creator,existence_status,deleted_or_orphaned_candidate,confidence,first_used_candidate_utc,last_used_date_utc,used_dates_count,used_dates_utc,use_count_value,open_count_estimate,usage_earliest_utc,usage_latest_utc,fused_usage_dates_utc,usage_date_row_count,usage_evidence_row_count,usage_source_fields,usage_field_summary,usage_supporting_values,likely_snapshot_or_index_usage_date_count,snapshot_warning_reasons,created_earliest_utc,created_latest_utc,modified_earliest_utc,modified_latest_utc,downloaded_earliest_utc,downloaded_latest_utc,interesting_or_index_earliest_utc,interesting_or_index_latest_utc,likely_snapshot_date_count,available_date_fields,object_usage_basis
FROM vw_object_usage_summary
ORDER BY COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,''), artifact_id) DESC
)SQL", log);
    exportQuery(db, exportDir / "usage_evidence.csv", R"SQL(
SELECT u.usage_id,u.artifact_id,u.source_id,u.store_guid,u.inode_num,a.file_name,a.best_path,u.field_name,u.field_value,u.parsed_utc,a.existence_status,a.deleted_or_orphaned_candidate
FROM usage_evidence u LEFT JOIN artifacts a ON a.artifact_id=u.artifact_id ORDER BY u.store_guid, CAST(u.inode_num AS INTEGER), u.field_name
)SQL", log);
    exportQuery(db, exportDir / "usage_evidence_expanded.csv", R"SQL(
SELECT d.raw_date_id AS usage_event_id,
       a.artifact_id,
       d.source_id,
       d.store_guid AS store_id,
       d.inode_num AS inode,
       a.parent_inode_num AS parent_inode,
       a.file_name AS filename_candidate,
       a.best_path AS path_candidate,
       COALESCE(NULLIF(d.parsed_utc,''), d.field_value) AS usage_event_utc,
       d.field_name AS usage_source_field,
       d.raw_date_id AS raw_date_candidate_id,
       a.content_type,
       a.where_froms,
       a.used_dates_utc AS all_related_used_dates,
       a.last_used_date_utc,
       a.use_count_value,
       a.open_count_estimate,
       a.existence_status
FROM raw_date_candidates d
LEFT JOIN artifacts a ON a.source_id=d.source_id AND a.store_guid=d.store_guid AND a.inode_num=d.inode_num
WHERE lower(d.field_name) LIKE '%used%'
  AND COALESCE(NULLIF(d.parsed_utc,''), d.field_value) <> ''
UNION ALL
SELECT (-1 * u.usage_id) AS usage_event_id,
       u.artifact_id,
       u.source_id,
       u.store_guid AS store_id,
       u.inode_num AS inode,
       a.parent_inode_num AS parent_inode,
       a.file_name AS filename_candidate,
       a.best_path AS path_candidate,
       COALESCE(NULLIF(u.parsed_utc,''), u.field_value) AS usage_event_utc,
       u.field_name AS usage_source_field,
       u.usage_id AS raw_date_candidate_id,
       a.content_type,
       a.where_froms,
       a.used_dates_utc AS all_related_used_dates,
       a.last_used_date_utc,
       a.use_count_value,
       a.open_count_estimate,
       a.existence_status
FROM usage_evidence u
LEFT JOIN artifacts a ON a.artifact_id=u.artifact_id
WHERE COALESCE(NULLIF(u.parsed_utc,''),'')=''
ORDER BY store_id, inode, usage_event_utc, usage_source_field
)SQL", log);

    exportQuery(db, exportDir / "timeline.csv", R"SQL(
SELECT timeline_id,artifact_id,source_id,store_guid,inode_num,event_timestamp_utc,event_type,event_source_field,file_name,path,existence_status,deleted_or_orphaned_candidate
FROM timeline_events ORDER BY event_timestamp_utc, store_guid, CAST(inode_num AS INTEGER)
)SQL", log);
    exportQuery(db, exportDir / "investigator_timeline.csv", R"SQL(
SELECT t.event_timestamp_utc AS event_utc,
       CASE
         WHEN lower(t.event_source_field) LIKE '%lastuseddate%' THEN 'last_used'
         WHEN lower(t.event_source_field) LIKE '%useddates%' THEN 'used_date'
         WHEN lower(t.event_source_field) LIKE '%download%' THEN 'downloaded'
         WHEN lower(t.event_source_field) LIKE '%creation%' OR lower(t.event_source_field) LIKE '%created%' THEN 'created'
         WHEN lower(t.event_source_field) LIKE '%contentchange%' OR lower(t.event_source_field) LIKE '%modification%' OR lower(t.event_source_field) LIKE '%modified%' THEN 'modified'
         WHEN lower(t.event_source_field) LIKE '%updated%' THEN 'metadata_seen'
         ELSE lower(t.event_type)
       END AS event_type,
       t.event_source_field,
       t.artifact_id,
       t.source_id,
       t.store_guid AS store_id,
       t.inode_num AS inode,
       a.parent_inode_num AS parent_inode,
       COALESCE(a.file_name,t.file_name) AS filename_candidate,
       COALESCE(NULLIF(a.best_path,''),t.path) AS path_candidate,
       a.content_type,
       a.where_froms,
       CASE
         WHEN lower(t.event_source_field) LIKE '%lastuseddate%' OR lower(t.event_source_field) LIKE '%useddates%' THEN 'HIGH_USAGE_FIELD'
         WHEN lower(t.event_source_field) LIKE '%download%' THEN 'HIGH_DOWNLOAD_FIELD'
         WHEN lower(t.event_source_field) LIKE '%creation%' OR lower(t.event_source_field) LIKE '%modification%' OR lower(t.event_source_field) LIKE '%contentchange%' THEN 'MEDIUM_METADATA_DATE_FIELD'
         ELSE 'LOW_GENERIC_DATE_FIELD'
       END AS confidence,
       t.existence_status,
       t.deleted_or_orphaned_candidate,
       '' AS notes
FROM timeline_events t
LEFT JOIN artifacts a ON a.artifact_id=t.artifact_id
WHERE COALESCE(NULLIF(t.event_timestamp_utc,''),'')<>''
ORDER BY t.event_timestamp_utc, t.store_guid, CAST(t.inode_num AS INTEGER), t.timeline_id
)SQL", log);

    exportQuery(db, exportDir / "external_volume_candidates.csv", R"SQL(
SELECT candidate_id,artifact_id,source_id,store_guid,inode_num,file_name,best_path,mounted_volume_name,reason,confidence,detection_source_field,detection_source_value
FROM external_volume_candidates ORDER BY source_id, mounted_volume_name, store_guid, CAST(inode_num AS INTEGER), candidate_id
)SQL", log);
    exportQuery(db, exportDir / "orphaned_or_deleted_candidates.csv", R"SQL(
SELECT candidate_id,artifact_id,source_id,store_guid,inode_num,file_name,best_path,content_type,existence_status,orphan_reason,index_text_snippet
FROM orphaned_deleted_candidates ORDER BY store_guid, CAST(inode_num AS INTEGER)
)SQL", log);
    exportQuery(db, exportDir / "field_inventory.csv", R"SQL(
SELECT field_inventory_id,source_id,field_name,row_count,populated_count,sample_value
FROM field_inventory ORDER BY row_count DESC, field_name
)SQL", log);
    exportQuery(db, exportDir / "parser_coverage_summary.csv", R"SQL(
SELECT summary_id,source_id,metric_name,metric_value,created_utc
FROM parser_coverage_summary ORDER BY summary_id
)SQL", log);
    exportQuery(db, exportDir / "raw_date_candidates.csv", R"SQL(
SELECT raw_date_id,source_id,store_guid,store_path,source_db,inode_num,store_id,field_name,field_value,parsed_utc,parse_method
FROM raw_date_candidates
ORDER BY store_guid, CAST(inode_num AS INTEGER), raw_date_id
)SQL", log);
    exportQuery(db, exportDir / "timeline_rejected_date_candidates.csv", R"SQL(
SELECT raw_date_id,source_id,store_guid,store_path,source_db,inode_num,store_id,field_name,field_value,parsed_utc,parse_method,
       'NOT_INSERTED_IN_TIMELINE_BECAUSE_PARSED_UTC_EMPTY' AS rejection_reason
FROM raw_date_candidates
WHERE COALESCE(NULLIF(parsed_utc,''),'')=''
ORDER BY store_guid, CAST(inode_num AS INTEGER), raw_date_id
)SQL", log);
    exportQuery(db, exportDir / "date_field_inventory.csv", R"SQL(
SELECT field_name,
       COUNT(*) AS row_count,
       COUNT(DISTINCT source_id || '|' || store_guid || '|' || source_db || '|' || inode_num || '|' || COALESCE(store_id,'')) AS record_count,
       SUM(CASE WHEN COALESCE(parsed_utc,'')<>'' THEN 1 ELSE 0 END) AS parsed_utc_count,
       COUNT(DISTINCT parse_method) AS parse_method_count,
       GROUP_CONCAT(DISTINCT parse_method) AS parse_methods,
       substr(MIN(field_value),1,500) AS min_sample_value,
       substr(MAX(field_value),1,500) AS max_sample_value
FROM raw_date_candidates
GROUP BY field_name
ORDER BY row_count DESC, field_name
)SQL", log);
    exportQuery(db, exportDir / "date_candidate_summary_by_record.csv", R"SQL(
SELECT r.raw_record_id,
       r.source_id,
       r.store_guid,
       r.source_db,
       r.inode_num,
       r.store_id,
       r.parent_inode_num,
       r.file_name,
       r.full_path,
       COUNT(d.raw_date_id) AS date_candidate_count,
       COUNT(DISTINCT d.field_name) AS date_field_count,
       GROUP_CONCAT(DISTINCT d.field_name) AS date_fields,
       GROUP_CONCAT(d.raw_date_id) AS raw_date_ids,
       MIN(COALESCE(NULLIF(d.parsed_utc,''), d.field_value)) AS earliest_date_candidate,
       MAX(COALESCE(NULLIF(d.parsed_utc,''), d.field_value)) AS latest_date_candidate
FROM raw_records r
LEFT JOIN raw_date_candidates d ON d.source_id=r.source_id AND d.store_guid=r.store_guid AND d.source_db=r.source_db AND d.inode_num=r.inode_num AND d.store_id=r.store_id
GROUP BY r.raw_record_id
ORDER BY date_candidate_count DESC, r.store_guid, CAST(r.inode_num AS INTEGER)
)SQL", log);
    exportQuery(db, exportDir / "date_candidate_multiplicity_summary.csv", R"SQL(
WITH per_record AS (
  SELECT r.raw_record_id, COUNT(d.raw_date_id) AS date_candidate_count
  FROM raw_records r
  LEFT JOIN raw_date_candidates d ON d.source_id=r.source_id AND d.store_guid=r.store_guid AND d.source_db=r.source_db AND d.inode_num=r.inode_num AND d.store_id=r.store_id
  GROUP BY r.raw_record_id
),
bucket_counts AS (
  SELECT
    SUM(CASE WHEN date_candidate_count=0 THEN 1 ELSE 0 END) AS records_with_0_dates,
    SUM(CASE WHEN date_candidate_count=1 THEN 1 ELSE 0 END) AS records_with_1_date,
    SUM(CASE WHEN date_candidate_count>=2 THEN 1 ELSE 0 END) AS records_with_2plus_dates,
    COALESCE(MAX(date_candidate_count),0) AS max_dates_per_record
  FROM per_record
)
SELECT bucket, record_count FROM (
  SELECT 1 AS sort_order, 'records_with_0_dates' AS bucket, COALESCE(records_with_0_dates,0) AS record_count FROM bucket_counts
  UNION ALL
  SELECT 2 AS sort_order, 'records_with_1_date' AS bucket, COALESCE(records_with_1_date,0) AS record_count FROM bucket_counts
  UNION ALL
  SELECT 3 AS sort_order, 'records_with_2plus_dates' AS bucket, COALESCE(records_with_2plus_dates,0) AS record_count FROM bucket_counts
  UNION ALL
  SELECT 4 AS sort_order, 'max_dates_per_record' AS bucket, COALESCE(max_dates_per_record,0) AS record_count FROM bucket_counts
)
ORDER BY sort_order
)SQL", log);
    exportQuery(db, exportDir / "timeline_date_source_summary.csv", R"SQL(
SELECT field_name AS event_source_field,
       CASE
         WHEN lower(field_name) LIKE '%used%' THEN 'USAGE'
         WHEN lower(field_name) LIKE '%download%' THEN 'DOWNLOADED'
         WHEN lower(field_name) LIKE '%creation%' OR lower(field_name) LIKE '%created%' THEN 'CREATED'
         WHEN lower(field_name) LIKE '%modification%' OR lower(field_name) LIKE '%modified%' THEN 'MODIFIED'
         WHEN lower(field_name) LIKE '%updated%' THEN 'UPDATED'
         ELSE 'DATE_FIELD'
       END AS event_type,
       COUNT(*) AS timeline_event_count,
       COUNT(DISTINCT source_id || '|' || store_guid || '|' || source_db || '|' || inode_num || '|' || COALESCE(store_id,'')) AS record_count,
       MIN(COALESCE(NULLIF(parsed_utc,''), field_value)) AS earliest_event_utc,
       MAX(COALESCE(NULLIF(parsed_utc,''), field_value)) AS latest_event_utc
FROM raw_date_candidates
WHERE COALESCE(NULLIF(parsed_utc,''), field_value) <> ''
GROUP BY field_name, event_type
ORDER BY timeline_event_count DESC, field_name, event_type
)SQL", log);
    exportQuery(db, exportDir / "preserved_evidence_files.csv", R"SQL(
SELECT preserved_file_id,source_id,relative_path,original_absolute_path,preserved_path,file_role,size_bytes,sha256,included_in_archive,created_utc
FROM preserved_evidence_files ORDER BY source_id, relative_path
)SQL", log);
    exportQuery(db, exportDir / "preserved_evidence_sets.csv", R"SQL(
SELECT preservation_id,source_id,archive_path,archive_format,archive_sha256,archive_size_bytes,created_utc,tool_used,tool_version,original_root_path,preserved_root_path,file_count,total_original_bytes,preservation_status,integrity_status,notes
FROM preserved_evidence_sets ORDER BY preservation_id
)SQL", log);
    exportQuery(db, exportDir / "raw_records.csv", R"SQL(
SELECT raw_record_id,source_id,store_guid,store_path,source_db,inode_num,store_id,parent_inode_num,flags,last_updated_raw,last_updated_utc,file_name,content_type,content_type_tree,where_froms,display_name,full_path,record_state,logical_size_bytes,physical_size_bytes
FROM raw_records ORDER BY store_guid, source_db, CAST(inode_num AS INTEGER)
)SQL", log);
    exportQuery(db, exportDir / "native_property_dictionary.csv", R"SQL(
SELECT native_property_id,source_id,store_guid,source_db,spotlight_version,property_index,property_name,prop_type_dec,value_type_dec,prop_type_hex,value_type_hex,is_core_native_field,created_utc
FROM native_property_dictionary
ORDER BY store_guid, source_db, property_index, property_name
)SQL", log);
    exportQuery(db, exportDir / "native_category_dictionary.csv", R"SQL(
SELECT native_category_id,source_id,store_guid,source_db,spotlight_version,category_index,category_name,created_utc
FROM native_category_dictionary
ORDER BY store_guid, source_db, category_index, category_name
)SQL", log);
    exportQuery(db, exportDir / "native_dbstr_map_inventory.csv", R"SQL(
SELECT native_dbstr_map_id,source_id,store_guid,source_db,map_id,data_path,offsets_path,header_path,data_exists,offsets_exists,header_exists,data_bytes,offsets_bytes,header_bytes,offset_entries,parsed_entries,skipped_entries,status,message,created_utc
FROM native_dbstr_map_inventory
ORDER BY store_guid, source_db, map_id
)SQL", log);
    exportQuery(db, exportDir / "native_index_dictionary_summary.csv", R"SQL(
SELECT native_index_summary_id,source_id,store_guid,source_db,spotlight_version,map_name,index_rows,value_ref_count,max_refs_per_index,max_ref_index,created_utc
FROM native_index_dictionary_summary
ORDER BY store_guid, source_db, map_name
)SQL", log);
    exportQuery(db, exportDir / "native_value_type_coverage.csv", R"SQL(
SELECT source_id,
       store_guid,
       spotlight_version,
       value_type_dec,
       value_type_hex,
       prop_type_dec,
       prop_type_hex,
       COUNT(*) AS property_count,
       SUM(CASE WHEN is_core_native_field=1 THEN 1 ELSE 0 END) AS core_field_count,
       GROUP_CONCAT(CASE WHEN is_core_native_field=1 THEN property_name ELSE NULL END) AS core_field_names,
       MIN(property_name) AS first_property_name,
       MAX(property_name) AS last_property_name
FROM native_property_dictionary
GROUP BY source_id, store_guid, spotlight_version, value_type_dec, value_type_hex, prop_type_dec, prop_type_hex
ORDER BY store_guid, value_type_dec, prop_type_dec
)SQL", log);
    exportQuery(db, exportDir / "high_value_field_candidates.csv", R"SQL(
SELECT d.native_property_id,
       d.source_id,
       d.store_guid,
       d.source_db,
       d.spotlight_version,
       d.property_index,
       d.property_name,
       d.prop_type_hex,
       d.value_type_hex,
       d.is_core_native_field,
       CASE
         WHEN d.is_core_native_field=1 THEN 'CORE_NATIVE_FIELD_LIST'
         WHEN lower(d.property_name) LIKE '%date%' OR lower(d.property_name) LIKE '%time%' THEN 'DATE_OR_TIME_NAME'
         WHEN lower(d.property_name) LIKE '%used%' OR lower(d.property_name) LIKE '%usecount%' THEN 'USAGE_NAME'
         WHEN lower(d.property_name) LIKE '%wherefrom%' OR lower(d.property_name) LIKE '%url%' THEN 'SOURCE_OR_URL_NAME'
         WHEN lower(d.property_name) LIKE '%path%' OR lower(d.property_name) LIKE '%filename%' OR lower(d.property_name) LIKE '%displayname%' THEN 'PATH_OR_NAME_NAME'
         WHEN lower(d.property_name) LIKE '%contenttype%' OR lower(d.property_name) LIKE '%kind%' THEN 'TYPE_OR_KIND_NAME'
         WHEN lower(d.property_name) LIKE '%author%' OR lower(d.property_name) LIKE '%creator%' THEN 'AUTHOR_OR_CREATOR_NAME'
         WHEN lower(d.property_name) LIKE '%volume%' OR lower(d.property_name) LIKE '%mount%' THEN 'VOLUME_OR_MOUNT_NAME'
         ELSE 'OTHER'
       END AS candidate_reason
FROM native_property_dictionary d
WHERE d.is_core_native_field=1
   OR lower(d.property_name) LIKE '%date%'
   OR lower(d.property_name) LIKE '%time%'
   OR lower(d.property_name) LIKE '%used%'
   OR lower(d.property_name) LIKE '%usecount%'
   OR lower(d.property_name) LIKE '%wherefrom%'
   OR lower(d.property_name) LIKE '%url%'
   OR lower(d.property_name) LIKE '%path%'
   OR lower(d.property_name) LIKE '%filename%'
   OR lower(d.property_name) LIKE '%displayname%'
   OR lower(d.property_name) LIKE '%contenttype%'
   OR lower(d.property_name) LIKE '%kind%'
   OR lower(d.property_name) LIKE '%author%'
   OR lower(d.property_name) LIKE '%creator%'
   OR lower(d.property_name) LIKE '%volume%'
   OR lower(d.property_name) LIKE '%mount%'
ORDER BY d.is_core_native_field DESC, candidate_reason, d.store_guid, d.property_index
)SQL", log);
    exportQuery(db, exportDir / "native_field_hit_summary.csv", R"SQL(
WITH hit_counts AS (
  SELECT source_id, store_guid, source_db, field_name, COUNT(*) AS hit_count, COUNT(DISTINCT inode_num) AS distinct_inode_count, substr(MAX(field_value),1,500) AS sample_value
  FROM raw_key_values
  GROUP BY source_id, store_guid, source_db, field_name
)
SELECT d.source_id,
       d.store_guid,
       d.source_db,
       d.property_index,
       d.property_name,
       d.prop_type_hex,
       d.value_type_hex,
       d.is_core_native_field,
       COALESCE(h.hit_count,0) AS decoded_hit_count,
       COALESCE(h.distinct_inode_count,0) AS decoded_distinct_inode_count,
       COALESCE(h.sample_value,'') AS decoded_sample_value
FROM native_property_dictionary d
LEFT JOIN hit_counts h
  ON h.source_id=d.source_id
 AND h.store_guid=d.store_guid
 AND h.source_db=d.source_db
 AND h.field_name=d.property_name
ORDER BY decoded_hit_count DESC, d.is_core_native_field DESC, d.store_guid, d.property_index
)SQL", log);
    exportQuery(db, exportDir / "native_decode_attempts.csv", R"SQL(
SELECT native_decode_attempt_id,source_id,store_guid,source_db,decode_mode,spotlight_version,properties_count,categories_count,metadata_blocks,decompressed_blocks,raw_records,raw_key_values,raw_date_candidates,fallback_header_only_items,failures,started_utc,finished_utc,status,message
FROM native_decode_attempts
ORDER BY native_decode_attempt_id
)SQL", log);
    exportQuery(db, exportDir / "native_key_values.csv", R"SQL(
SELECT raw_kv_id,source_id,store_guid,store_path,source_db,inode_num,store_id,parent_inode_num,full_path,record_state,field_name,field_value
FROM raw_key_values ORDER BY store_guid, source_db, CAST(inode_num AS INTEGER), raw_kv_id
)SQL", log);
    exportQuery(db, exportDir / "native_probe_values.csv", R"SQL(
SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,full_path,record_state,field_name,field_value
FROM raw_key_values
WHERE field_name LIKE '__native_%'
ORDER BY field_name, store_guid, CAST(inode_num AS INTEGER), raw_kv_id
)SQL", log);
    exportQuery(db, exportDir / "native_probe_samples.csv", R"SQL(
SELECT field_name, COUNT(*) AS row_count, COUNT(DISTINCT field_value) AS distinct_value_count, substr(MIN(field_value),1,500) AS min_sample_value, substr(MAX(field_value),1,500) AS max_sample_value
FROM raw_key_values
WHERE field_name LIKE '__native_%'
GROUP BY field_name
ORDER BY row_count DESC, field_name
)SQL", log);
    exportQuery(db, exportDir / "native_decode_errors.csv", R"SQL(
SELECT failure_id,source_id,phase,store_guid,source_db,message,created_utc
FROM raw_failures
ORDER BY failure_id
)SQL", log);
    exportQuery(db, exportDir / "native_partial_decode_errors.csv", R"SQL(
SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,full_path,record_state,field_value AS decode_error
FROM raw_key_values
WHERE field_name='__decode_error'
ORDER BY store_guid, source_db, CAST(inode_num AS INTEGER), raw_kv_id
)SQL", log);
    // V0_9_20: legacy V7 comparison exports are intentionally suppressed from normal
    // investigation output. The compatibility importer remains compiled for now, but the
    // active workflow is native Store-V2 / CoreSpotlight-first.
    exportQuery(db, exportDir / "path_diagnostics.csv", R"SQL(
SELECT artifact_id,source_id,store_guid,inode_num,file_name,display_name,best_path,v7_full_path_raw,spotlight_display_path,normalized_mac_path,filesystem_lookup_path,path_source,path_status,existence_status,deleted_or_orphaned_candidate,orphan_reason
FROM artifacts
WHERE COALESCE(v7_full_path_raw,'')<>'' OR COALESCE(best_path,'')='' OR best_path LIKE '%NOT-FOUND%' OR COALESCE(path_status,'')<>''
ORDER BY store_guid, CAST(inode_num AS INTEGER)
)SQL", log);
    exportQuery(db, exportDir / "usage_linked_artifacts.csv", R"SQL(
SELECT MIN(u.usage_id) AS first_usage_id,
       u.artifact_id,
       u.source_id,
       u.store_guid,
       u.inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.spotlight_display_path,
       a.normalized_mac_path,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.use_count_value,
       a.usage_field_summary,
       a.open_count_estimate,
       COUNT(u.usage_id) AS usage_evidence_row_count,
       GROUP_CONCAT(u.field_name || '=' || COALESCE(u.field_value,''), '; ') AS usage_evidence_rows,
       a.existence_status
FROM usage_evidence u LEFT JOIN artifacts a ON a.artifact_id=u.artifact_id
GROUP BY u.artifact_id,u.source_id,u.store_guid,u.inode_num,a.file_name,a.display_name,a.best_path,a.spotlight_display_path,a.normalized_mac_path,a.first_used_candidate_utc,a.last_used_date_utc,a.used_dates_count,a.use_count_value,a.usage_field_summary,a.open_count_estimate,a.existence_status
ORDER BY u.store_guid, CAST(u.inode_num AS INTEGER), u.artifact_id
)SQL", log);
    exportQuery(db, exportDir / "metadata_hydration_summary.csv", R"SQL(
SELECT 'artifacts' AS metric, COUNT(*) AS value FROM artifacts
UNION ALL SELECT 'artifacts_with_file_name', COUNT(*) FROM artifacts WHERE COALESCE(file_name,'')<>'' AND file_name<>'------NONAME------'
UNION ALL SELECT 'artifacts_with_display_path', COUNT(*) FROM artifacts WHERE COALESCE(spotlight_display_path,'')<>''
UNION ALL SELECT 'artifacts_with_content_type', COUNT(*) FROM artifacts WHERE COALESCE(content_type,'')<>''
UNION ALL SELECT 'artifacts_with_usage_summary', COUNT(*) FROM artifacts WHERE COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(used_dates_count,0)>0
UNION ALL SELECT 'native_property_dictionary_rows', COUNT(*) FROM native_property_dictionary
UNION ALL SELECT 'native_core_property_dictionary_rows', COUNT(*) FROM native_property_dictionary WHERE is_core_native_field=1
UNION ALL SELECT 'native_decode_attempts', COUNT(*) FROM native_decode_attempts
UNION ALL SELECT 'raw_key_values', COUNT(*) FROM raw_key_values
UNION ALL SELECT 'native_probe_values', COUNT(*) FROM raw_key_values WHERE field_name LIKE '__native_%'
UNION ALL SELECT 'mounted_volume_candidates', COUNT(*) FROM external_volume_candidates
UNION ALL SELECT 'orphan_deleted_candidates', COUNT(*) FROM orphaned_deleted_candidates
)SQL", log);
    try {
        appendExportRunStatus(exportDir / "upload_samples" / "upload_samples_manifest.csv", 95, "export_upload_samples_start", "bounded upload samples");
        exportUploadSamples(db, exportDir / "upload_samples", log);
        appendExportRunStatus(exportDir / "upload_samples" / "upload_samples_manifest.csv", 96, "export_upload_samples_complete", "bounded upload samples complete");
    } catch (const std::exception& ex) {
        log.warn(std::string("Non-fatal upload sample export failure: ") + ex.what());
    } catch (...) {
        log.warn("Non-fatal upload sample export failure: unknown exception");
    }
    writeCaseReviewSummary(db, exportDir.parent_path() / "CASE_REVIEW_SUMMARY.txt", log);
    writeReviewIndex(exportDir.parent_path() / "review_index.html", log);
    writeInvestigatorDashboard(db, exportDir.parent_path() / "investigator_dashboard.html", log);
    writeUploadReadme(exportDir.parent_path(), log);
    writeTargetedExportReadme(exportDir.parent_path(), log);
    writeTargetedExportScript(exportDir.parent_path(), log);
    writeUploadZipScript(exportDir.parent_path(), log);

    writeExportIndexFile(exportDir / "EXPORT_INDEX.csv", exportDir, log);
    if (exportDir.has_parent_path()) writeExportIndexFile(exportDir.parent_path() / "EXPORT_INDEX.csv", exportDir, log);
    appendExportRunStatus(exportDir / "EXPORT_INDEX.csv", 97, "export_complete", "profile=" + profile);
    log.info("SQLite review exports written to: " + pathString(exportDir));
}

void SqliteExporter::exportUploadSamples(CaseDatabase& db, const fs::path& sampleDir, Logger& log) const {
    fs::create_directories(sampleDir);
    constexpr int TableSampleLimit = 250;
    constexpr int FocusSampleLimit = 5000;

    const std::vector<std::string> tables = {
        "case_info",
        "evidence_sources",
        "source_files",
        "source_probe_runs",
        "source_probe_signatures",
        "source_partition_probe",
        "image_inventory_sources",
        "image_file_inventory",
        "active_file_comparison_runs",
        "preserved_evidence_sets",
        "preserved_evidence_files",
        "archive_integrity_checks",
        "store_groups",
        "raw_records",
        "raw_key_values",
        "raw_date_candidates",
        "raw_failures",
        "volume_configuration",
        "artifacts",
        "usage_evidence",
        "timeline_events",
        "orphaned_deleted_candidates",
        "external_volume_candidates",
        "artifact_source_instances",
        "source_copy_comparison",
        "parent_inode_links",
        "field_inventory",
        "parser_coverage_summary",
        "processing_phases",
        "investigator_tags",
        "artifact_tags",
        "gui_checked_artifacts",
        "investigator_notes",
        "tags",
        "notes",
        "native_property_dictionary",
        "native_decode_attempts",
        "processing_log"
    };

    std::ofstream counts(ioPath(sampleDir / "upload_table_counts.csv"), std::ios::binary);
    if (!counts) throw std::runtime_error("Unable to write upload_table_counts.csv");
    counts << "table_name,row_count,sample_file,sample_limit\n";

    std::ofstream manifest(ioPath(sampleDir / "upload_samples_manifest.csv"), std::ios::binary);
    if (!manifest) throw std::runtime_error("Unable to write upload_samples_manifest.csv");
    manifest << "sample_name,source_table,total_table_rows,exported_row_limit,notes\n";

    for (const auto& table : tables) {
        if (!tableExists(db, table)) continue;
        const long long total = tableRowCount(db, table);
        const std::string sampleName = table + "_sample.csv";
        counts << csvEscape(table) << ',' << total << ',' << csvEscape(sampleName) << ',' << TableSampleLimit << "\n";
        manifest << csvEscape(sampleName) << ',' << csvEscape(table) << ',' << total << ',' << TableSampleLimit << ",first rows ordered by rowid where available\n";
        exportQuery(db, sampleDir / sampleName,
                    "SELECT * FROM " + sqlQuoteIdentifier(table) + " ORDER BY rowid LIMIT " + std::to_string(TableSampleLimit),
                    log);
    }

    exportQuery(db, sampleDir / "image_inventory_sources_focus.csv", "SELECT * FROM vw_image_inventory_sources ORDER BY created_utc DESC, image_inventory_source_id DESC LIMIT 250", log);
    manifest << "image_inventory_sources_focus.csv,vw_image_inventory_sources," << tableRowCount(db, "image_inventory_sources") << ",250,image-backed inventory source readiness for AFF4/APFS comparison\n";

    exportQuery(db, sampleDir / "active_file_comparison_readiness_focus.csv", "SELECT * FROM vw_active_file_comparison_readiness ORDER BY created_utc DESC, image_inventory_source_id DESC LIMIT 250", log);
    manifest << "active_file_comparison_readiness_focus.csv,vw_active_file_comparison_readiness," << tableRowCount(db, "image_inventory_sources") << ",250,readiness status for Spotlight-to-image active file comparison\n";

    exportQuery(db, sampleDir / "spotlight_active_file_comparison_focus.csv", "SELECT * FROM vw_spotlight_active_file_comparison ORDER BY artifact_id LIMIT 1000", log);
    manifest << "spotlight_active_file_comparison_focus.csv,vw_spotlight_active_file_comparison," << tableRowCount(db, "artifacts") << ",1000,Spotlight artifacts joined to image_file_inventory when image inventory rows exist\n";

    exportQuery(db, sampleDir / "artifacts_usage_focus.csv", R"SQL(
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,usage_field_summary,open_count_estimate,existence_status,deleted_or_orphaned_candidate,confidence
FROM artifacts
WHERE COALESCE(usage_field_summary,'')<>''
   OR COALESCE(last_used_date_utc,'')<>''
   OR COALESCE(first_used_candidate_utc,'')<>''
   OR COALESCE(used_dates_count,0)>0
   OR COALESCE(use_count_value,'')<>''
ORDER BY store_guid, CAST(inode_num AS INTEGER), artifact_id
LIMIT 5000
)SQL", log);
    manifest << "artifacts_usage_focus.csv,artifacts," << tableRowCount(db, "artifacts") << "," << FocusSampleLimit << ",usage-bearing artifacts only\n";

    exportQuery(db, sampleDir / "object_usage_summary_focus.csv", R"SQL(
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,logical_size_bytes,physical_size_bytes,use_count_value,open_count_estimate,used_dates_count,usage_earliest_utc,usage_latest_utc,fused_usage_dates_utc,usage_date_row_count,usage_evidence_row_count,usage_source_fields,object_usage_basis,where_froms,confidence
FROM vw_object_usage_summary
ORDER BY COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,''), artifact_id) DESC
LIMIT 5000
)SQL", log);
    manifest << "object_usage_summary_focus.csv,vw_object_usage_summary," << tableRowCount(db, "artifacts") << "," << FocusSampleLimit << ",object-centric usage rows with filename/path and fused usage metadata\n";

    exportQuery(db, sampleDir / "artifacts_wherefrom_focus.csv", R"SQL(
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,downloaded_date_utc,last_updated_utc,existence_status,deleted_or_orphaned_candidate,confidence
FROM artifacts
WHERE COALESCE(where_froms,'')<>'' OR COALESCE(downloaded_date_utc,'')<>''
ORDER BY store_guid, CAST(inode_num AS INTEGER), artifact_id
LIMIT 5000
)SQL", log);
    manifest << "artifacts_wherefrom_focus.csv,artifacts," << tableRowCount(db, "artifacts") << "," << FocusSampleLimit << ",where-from/download-origin artifacts only\n";

    exportQuery(db, sampleDir / "artifacts_path_focus.csv", R"SQL(
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,v7_full_path_raw,spotlight_display_path,normalized_mac_path,path_source,path_status,content_type,confidence
FROM artifacts
WHERE COALESCE(best_path,'')<>''
   OR COALESCE(v7_full_path_raw,'')<>''
   OR COALESCE(spotlight_display_path,'')<>''
   OR COALESCE(normalized_mac_path,'')<>''
ORDER BY store_guid, CAST(inode_num AS INTEGER), artifact_id
LIMIT 5000
)SQL", log);
    manifest << "artifacts_path_focus.csv,artifacts," << tableRowCount(db, "artifacts") << "," << FocusSampleLimit << ",artifacts with path/name reconstruction signals\n";

    exportQuery(db, sampleDir / "path_reconstruction_focus.csv", R"SQL(
SELECT link_id,source_id,store_guid,artifact_id,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,parent_artifact_id,resolved_parent_inode_num,parent_file_name,parent_best_path,reconstructed_path_candidate,applied_to_artifact_path,candidate_matches_artifact_path,sibling_count,relationship_status,path_reconstruction_method,confidence
FROM vw_path_reconstruction
ORDER BY applied_to_artifact_path DESC, candidate_matches_artifact_path DESC, confidence DESC, store_guid, CAST(parent_inode_num AS INTEGER), CAST(inode_num AS INTEGER), link_id
LIMIT 5000
)SQL", log);
    manifest << "path_reconstruction_focus.csv,vw_path_reconstruction," << tableRowCount(db, "parent_inode_links") << "," << FocusSampleLimit << ",parent-inode reconstructed path candidates with separate applied-path and candidate-match status\n";

    exportQuery(db, sampleDir / "same_folder_groups_focus.csv", R"SQL(
SELECT source_id,store_guid,parent_inode_num,parent_artifact_id,parent_file_name,parent_best_path,child_count,resolved_parent_link_count,reconstructed_child_path_count,child_name_count,first_child_name,last_child_name,folder_group_status,max_confidence,sibling_group_key
FROM vw_same_folder_groups
ORDER BY child_count DESC, reconstructed_child_path_count DESC, store_guid, CAST(parent_inode_num AS INTEGER)
LIMIT 5000
)SQL", log);
    manifest << "same_folder_groups_focus.csv,vw_same_folder_groups," << tableRowCount(db, "parent_inode_links") << "," << FocusSampleLimit << ",same-folder parent-inode groups with reconstruction counts\n";



    if (tableExists(db, "vw_checked_artifacts")) {
        exportQuery(db, sampleDir / "checked_artifacts_focus.csv", R"SQL(
SELECT checked_utc,artifact_id,tags,tag_count,note_count,last_note_utc,last_note_text,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,usage_latest_utc,last_used_date_utc,modified_latest_utc,created_latest_utc,downloaded_latest_utc,where_froms,confidence
FROM vw_checked_artifacts
ORDER BY checked_utc DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(modified_latest_utc,''), artifact_id) DESC
LIMIT 5000
)SQL", log);
        manifest << "checked_artifacts_focus.csv,vw_checked_artifacts," << tableRowCount(db, "gui_checked_artifacts") << "," << FocusSampleLimit << ",persisted checked artifacts with tags, notes, dates, and path context\n";
    }

    if (tableExists(db, "vw_tagged_artifacts")) {
        exportQuery(db, sampleDir / "tagged_artifacts_focus.csv", R"SQL(
SELECT tag_id,tag_name,tagged_utc,artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,usage_latest_utc,last_used_date_utc,modified_latest_utc,created_latest_utc,where_froms,note_count,last_note_utc,last_note_text,confidence
FROM vw_tagged_artifacts
ORDER BY lower(tag_name), COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(modified_latest_utc,''), artifact_id) DESC
LIMIT 5000
)SQL", log);
        manifest << "tagged_artifacts_focus.csv,vw_tagged_artifacts," << tableRowCount(db, "artifact_tags") << "," << FocusSampleLimit << ",tagged artifact rows with note and date context\n";
    }

    if (tableExists(db, "vw_artifact_notes")) {
        exportQuery(db, sampleDir / "artifact_notes_focus.csv", R"SQL(
SELECT note_id,created_utc,updated_utc,note_text,artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,tags
FROM vw_artifact_notes
ORDER BY updated_utc DESC, created_utc DESC, note_id DESC
LIMIT 5000
)SQL", log);
        manifest << "artifact_notes_focus.csv,vw_artifact_notes," << tableRowCount(db, "investigator_notes") << "," << FocusSampleLimit << ",artifact notes with linked artifact and tag context\n";
    }

    if (tableExists(db, "vw_export_ready_artifacts")) {
        exportQuery(db, sampleDir / "export_ready_artifacts_focus.csv", R"SQL(
SELECT artifact_id,tags,tag_count,note_count,last_note_utc,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,usage_latest_utc,last_used_date_utc,modified_latest_utc,created_latest_utc,downloaded_latest_utc,where_froms,confidence
FROM vw_export_ready_artifacts
ORDER BY tag_count DESC, note_count DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(modified_latest_utc,''), artifact_id) DESC
LIMIT 5000
)SQL", log);
        manifest << "export_ready_artifacts_focus.csv,vw_export_ready_artifacts," << tableRowCount(db, "artifacts") << "," << FocusSampleLimit << ",artifacts selected for investigator export by tags or notes\n";
    }

    exportQuery(db, sampleDir / "timeline_usage_focus.csv", R"SQL(
SELECT t.timeline_id,t.event_timestamp_utc AS event_utc,t.event_type,t.event_source_field,t.artifact_id,t.source_id,t.store_guid,t.inode_num,a.parent_inode_num,a.file_name,a.display_name,a.best_path,a.content_type,a.where_froms,'MEDIUM_USAGE_FIELD' AS confidence,'' AS notes
FROM timeline_events t
LEFT JOIN artifacts a ON a.artifact_id=t.artifact_id
WHERE lower(COALESCE(t.event_source_field,'')) LIKE '%used%'
   OR lower(COALESCE(t.event_type,'')) LIKE '%used%'
   OR lower(COALESCE(t.event_source_field,'')) LIKE '%usage%'
   OR lower(COALESCE(t.event_source_field,'')) LIKE '%open%'
ORDER BY t.event_timestamp_utc, t.timeline_id
LIMIT 5000
)SQL", log);
    manifest << "timeline_usage_focus.csv,timeline_events," << tableRowCount(db, "timeline_events") << "," << FocusSampleLimit << ",timeline rows associated with usage/open evidence\n";

    exportQuery(db, sampleDir / "native_key_values_high_value_sample.csv", R"SQL(
SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,full_path,record_state,field_name,field_value
FROM raw_key_values
WHERE lower(field_name) LIKE '%used%'
   OR lower(field_name) LIKE '%wherefrom%'
   OR lower(field_name) LIKE '%download%'
   OR lower(field_name) LIKE '%path%'
   OR lower(field_name) LIKE '%filename%'
   OR lower(field_name) LIKE '%displayname%'
   OR lower(field_name) LIKE '%contenttype%'
   OR lower(field_name) LIKE '%creator%'
   OR lower(field_name) LIKE '%author%'
   OR lower(field_name) LIKE '%owner%'
ORDER BY store_guid, CAST(inode_num AS INTEGER), raw_kv_id
LIMIT 5000
)SQL", log);
    manifest << "native_key_values_high_value_sample.csv,raw_key_values," << tableRowCount(db, "raw_key_values") << "," << FocusSampleLimit << ",high-value native key/value metadata rows\n";

    if (tableExists(db, "vw_ios_store_parse_summary")) {
        exportQuery(db, sampleDir / "ios_store_parse_summary_sample.csv", "SELECT * FROM vw_ios_store_parse_summary ORDER BY raw_record_count DESC, store_guid, source_db LIMIT 5000", log);
        manifest << "ios_store_parse_summary_sample.csv,vw_ios_store_parse_summary," << tableRowCount(db, "vw_ios_store_parse_summary") << "," << FocusSampleLimit << ",iOS CoreSpotlight per-store parse counts and date ranges\n";
    }
    if (tableExists(db, "vw_ios_protection_class_summary")) {
        exportQuery(db, sampleDir / "ios_protection_class_summary_sample.csv", "SELECT * FROM vw_ios_protection_class_summary ORDER BY raw_record_count DESC, protection_class LIMIT 5000", log);
        manifest << "ios_protection_class_summary_sample.csv,vw_ios_protection_class_summary," << tableRowCount(db, "vw_ios_protection_class_summary") << "," << FocusSampleLimit << ",iOS protection class counts, string-probe counts, and index-update date ranges\n";
    }
    if (tableExists(db, "vw_ios_artifact_hint_summary")) {
        exportQuery(db, sampleDir / "ios_artifact_hint_summary_sample.csv", "SELECT * FROM vw_ios_artifact_hint_summary ORDER BY string_probe_rows DESC, artifact_hint LIMIT 5000", log);
        manifest << "ios_artifact_hint_summary_sample.csv,vw_ios_artifact_hint_summary," << tableRowCount(db, "vw_ios_artifact_hint_summary") << "," << FocusSampleLimit << ",iOS decoded string artifact hint categories\n";
    }
    if (tableExists(db, "vw_ios_record_investigation_hints")) {
        exportQuery(db, sampleDir / "ios_record_investigation_hints_sample.csv", "SELECT * FROM vw_ios_record_investigation_hints ORDER BY last_updated_utc DESC, protection_class, primary_investigation_hint, store_guid, CAST(inode_num AS INTEGER), raw_record_id LIMIT 5000", log);
        manifest << "ios_record_investigation_hints_sample.csv,vw_ios_record_investigation_hints," << tableRowCount(db, "vw_ios_record_investigation_hints") << "," << FocusSampleLimit << ",iOS per-record investigator hint rollup without raw string samples\n";
    }
    if (tableExists(db, "vw_ios_string_probe_category_summary")) {
        exportQuery(db, sampleDir / "ios_string_probe_category_summary_sample.csv", "SELECT * FROM vw_ios_string_probe_category_summary ORDER BY row_count DESC, probe_category LIMIT 5000", log);
        manifest << "ios_string_probe_category_summary_sample.csv,vw_ios_string_probe_category_summary," << tableRowCount(db, "vw_ios_string_probe_category_summary") << "," << FocusSampleLimit << ",iOS string-probe category counts\n";
    }
    if (tableExists(db, "vw_ios_spotlight_text_context_priority_summary")) {
        exportQuery(db, sampleDir / "ios_spotlight_text_context_priority_summary_sample.csv", "SELECT * FROM vw_ios_spotlight_text_context_priority_summary ORDER BY review_priority_sort, text_context_record_count DESC", log);
        manifest << "ios_spotlight_text_context_priority_summary_sample.csv,vw_ios_spotlight_text_context_priority_summary," << tableRowCount(db, "vw_ios_spotlight_text_context_priority_summary") << ",ALL,priority/category summary for same-record compact Spotlight text context\n";
    }
    if (tableExists(db, "vw_ios_spotlight_chat_app_attribution_summary")) {
        exportQuery(db, sampleDir / "ios_spotlight_chat_app_attribution_summary_sample.csv", "SELECT * FROM vw_ios_spotlight_chat_app_attribution_summary ORDER BY text_context_category, context_record_count DESC", log);
        manifest << "ios_spotlight_chat_app_attribution_summary_sample.csv,vw_ios_spotlight_chat_app_attribution_summary," << tableRowCount(db, "vw_ios_spotlight_chat_app_attribution_summary") << ",ALL,chat app attribution vs text/link mention summary\n";
    }
    if (tableExists(db, "vw_ios_spotlight_communication_summary")) {
        exportQuery(db, sampleDir / "ios_spotlight_communication_summary_sample.csv", "SELECT * FROM vw_ios_spotlight_communication_summary ORDER BY review_priority_sort, spotlight_record_count DESC, communication_context_type", log);
        manifest << "ios_spotlight_communication_summary_sample.csv,vw_ios_spotlight_communication_summary," << tableRowCount(db, "vw_ios_spotlight_communication_summary") << ",ALL,record-centric iOS Spotlight communication summary\n";
    }
    if (tableExists(db, "vw_ios_spotlight_communication_record_review")) {
        exportQuery(db, sampleDir / "ios_spotlight_communication_record_review_sample.csv", "SELECT * FROM vw_ios_spotlight_communication_record_review ORDER BY review_priority_sort, spotlight_date_utc DESC, raw_record_id DESC LIMIT 5000", log);
        manifest << "ios_spotlight_communication_record_review_sample.csv,vw_ios_spotlight_communication_record_review," << tableRowCount(db, "vw_ios_spotlight_communication_record_review") << "," << FocusSampleLimit << ",record-centric communications review with direct Spotlight text/context columns\n";
    }
    if (tableExists(db, "vw_ios_spotlight_message_text_review")) {
        exportQuery(db, sampleDir / "ios_spotlight_message_text_review_sample.csv", "SELECT * FROM vw_ios_spotlight_message_text_review ORDER BY review_priority_sort, spotlight_date_utc DESC, raw_record_id DESC LIMIT 5000", log);
        manifest << "ios_spotlight_message_text_review_sample.csv,vw_ios_spotlight_message_text_review," << tableRowCount(db, "vw_ios_spotlight_message_text_review") << "," << FocusSampleLimit << ",message/mail/call/chat Spotlight review with extracted investigator-visible text\n";
    }
    if (tableExists(db, "vw_ios_spotlight_message_media_review")) {
        exportQuery(db, sampleDir / "ios_spotlight_message_media_review_sample.csv", "SELECT * FROM vw_ios_spotlight_message_media_review ORDER BY spotlight_date_utc DESC, raw_record_id DESC LIMIT 5000", log);
        manifest << "ios_spotlight_message_media_review_sample.csv,vw_ios_spotlight_message_media_review," << tableRowCount(db, "vw_ios_spotlight_message_media_review") << "," << FocusSampleLimit << ",message-adjacent media/photo/attachment Spotlight review\n";
    }
    if (tableExists(db, "vw_ios_spotlight_attachment_reference_review")) {
        exportQuery(db, sampleDir / "ios_spotlight_attachment_reference_review_sample.csv", "SELECT * FROM vw_ios_spotlight_attachment_reference_review ORDER BY spotlight_date_utc DESC, communication_context_type, raw_record_id DESC LIMIT 5000", log);
        manifest << "ios_spotlight_attachment_reference_review_sample.csv,vw_ios_spotlight_attachment_reference_review," << tableRowCount(db, "vw_ios_spotlight_attachment_reference_review") << "," << FocusSampleLimit << ",Spotlight communication attachment/media/content references\n";
    }
    if (tableExists(db, "vw_ios_spotlight_high_value_text_context_review")) {
        exportQuery(db, sampleDir / "ios_spotlight_high_value_text_context_review_sample.csv", "SELECT * FROM vw_ios_spotlight_high_value_text_context_review ORDER BY review_priority_sort, last_updated_utc DESC, raw_record_id DESC LIMIT 5000", log);
        manifest << "ios_spotlight_high_value_text_context_review_sample.csv,vw_ios_spotlight_high_value_text_context_review," << tableRowCount(db, "vw_ios_spotlight_high_value_text_context_review") << "," << FocusSampleLimit << ",bounded high/medium-priority same-record Spotlight text context\n";
    }
    if (tableExists(db, "vw_ios_spotlight_text_context_review")) {
        exportQuery(db, sampleDir / "ios_spotlight_text_context_review_sample.csv", "SELECT * FROM vw_ios_spotlight_text_context_review ORDER BY review_priority_sort, last_updated_utc DESC, raw_record_id DESC LIMIT 5000", log);
        manifest << "ios_spotlight_text_context_review_sample.csv,vw_ios_spotlight_text_context_review," << tableRowCount(db, "vw_ios_spotlight_text_context_review") << "," << FocusSampleLimit << ",same-record compact Spotlight text context retained in normal mode for investigator review, priority-sorted in V0_9_25\n";
    }
    if (tableExists(db, "vw_ios_string_probe_values")) {
        exportQuery(db, sampleDir / "ios_string_probe_values_sample.csv", "SELECT * FROM vw_ios_string_probe_values ORDER BY probe_category, store_guid, CAST(inode_num AS INTEGER), raw_kv_id LIMIT 5000", log);
        manifest << "ios_string_probe_values_sample.csv,vw_ios_string_probe_values," << tableRowCount(db, "vw_ios_string_probe_values") << "," << FocusSampleLimit << ",bounded iOS string-probe payload rows for investigation\n";
    }
    if (tableExists(db, "vw_ios_spotlight_missing_from_ffs_high_value_candidates")) {
        exportQuery(db, sampleDir / "ios_spotlight_missing_from_ffs_high_value_candidates_sample.csv", "SELECT * FROM vw_ios_spotlight_missing_from_ffs_high_value_candidates ORDER BY investigative_priority_sort, spotlight_text_context_status DESC, normalized_ios_path, reference_id LIMIT 5000", log);
        manifest << "ios_spotlight_missing_from_ffs_high_value_candidates_sample.csv,vw_ios_spotlight_missing_from_ffs_high_value_candidates," << tableRowCount(db, "vw_ios_spotlight_missing_from_ffs_high_value_candidates") << "," << FocusSampleLimit << ",bounded high/medium-priority missing-from-FFS candidates with same-record Spotlight text context\n";
    }
    if (tableExists(db, "vw_ios_spotlight_missing_from_ffs_candidates")) {
        exportQuery(db, sampleDir / "ios_spotlight_missing_from_ffs_candidates_sample.csv", "SELECT * FROM vw_ios_spotlight_missing_from_ffs_candidates ORDER BY investigative_priority_sort, spotlight_text_context_status DESC, normalized_ios_path, reference_id LIMIT 5000", log);
        manifest << "ios_spotlight_missing_from_ffs_candidates_sample.csv,vw_ios_spotlight_missing_from_ffs_candidates," << tableRowCount(db, "vw_ios_spotlight_missing_from_ffs_candidates") << "," << FocusSampleLimit << ",bounded missing-from-FFS candidates with priority/category and same-record Spotlight text context\n";
    }
    if (tableExists(db, "vw_ios_record_string_probe_summary")) {
        exportQuery(db, sampleDir / "ios_record_string_probe_summary_sample.csv", "SELECT * FROM vw_ios_record_string_probe_summary ORDER BY last_updated_utc DESC, store_guid, CAST(inode_num AS INTEGER), raw_record_id LIMIT 5000", log);
        manifest << "ios_record_string_probe_summary_sample.csv,vw_ios_record_string_probe_summary," << tableRowCount(db, "vw_ios_record_string_probe_summary") << "," << FocusSampleLimit << ",iOS per-record string-probe rollup with bounded raw samples\n";
    }
    if (tableExists(db, "vw_ios_timeline_index_updates")) {
        exportQuery(db, sampleDir / "ios_timeline_index_updates_sample.csv", "SELECT * FROM vw_ios_timeline_index_updates ORDER BY last_updated_utc DESC, store_guid, CAST(inode_num AS INTEGER), raw_record_id LIMIT 5000", log);
        manifest << "ios_timeline_index_updates_sample.csv,vw_ios_timeline_index_updates," << tableRowCount(db, "vw_ios_timeline_index_updates") << "," << FocusSampleLimit << ",iOS Last_Updated index-update timeline, not usage\n";
    }
    if (tableExists(db, "vw_ios_spotlight_dbstr_map_inventory")) {
        exportQuery(db, sampleDir / "ios_spotlight_dbstr_map_inventory_sample.csv", "SELECT * FROM vw_ios_spotlight_dbstr_map_inventory ORDER BY store_guid, source_db, map_id LIMIT 5000", log);
        manifest << "ios_spotlight_dbstr_map_inventory_sample.csv,vw_ios_spotlight_dbstr_map_inventory," << tableRowCount(db, "vw_ios_spotlight_dbstr_map_inventory") << "," << FocusSampleLimit << ",iOS dbStr map component parse status\n";
    }
    if (tableExists(db, "vw_ios_spotlight_dictionary_coverage")) {
        exportQuery(db, sampleDir / "ios_spotlight_dictionary_coverage_sample.csv", "SELECT * FROM vw_ios_spotlight_dictionary_coverage ORDER BY raw_record_count DESC, store_guid, source_db LIMIT 5000", log);
        manifest << "ios_spotlight_dictionary_coverage_sample.csv,vw_ios_spotlight_dictionary_coverage," << tableRowCount(db, "vw_ios_spotlight_dictionary_coverage") << "," << FocusSampleLimit << ",iOS CoreSpotlight dictionary/dbStr coverage by store\n";
    }
    if (tableExists(db, "vw_ios_spotlight_apple_field_coverage")) {
        exportQuery(db, sampleDir / "ios_spotlight_apple_field_coverage_sample.csv", "SELECT * FROM vw_ios_spotlight_apple_field_coverage ORDER BY value_row_count DESC, apple_semantic_group, field_name LIMIT 5000", log);
        manifest << "ios_spotlight_apple_field_coverage_sample.csv,vw_ios_spotlight_apple_field_coverage," << tableRowCount(db, "vw_ios_spotlight_apple_field_coverage") << "," << FocusSampleLimit << ",CoreSpotlight field names grouped by Apple public metadata semantics\n";
    }

    exportQuery(db, sampleDir / "content_type_summary.csv", R"SQL(
SELECT COALESCE(NULLIF(content_type,''),'(blank)') AS content_type,
       COUNT(*) AS artifact_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_artifact_count,
       SUM(CASE WHEN COALESCE(best_path,'')<>'' THEN 1 ELSE 0 END) AS path_artifact_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc,
       SUM(CAST(COALESCE(NULLIF(logical_size_bytes,''),'0') AS INTEGER)) AS total_logical_size_bytes,
       SUM(CAST(COALESCE(NULLIF(physical_size_bytes,''),'0') AS INTEGER)) AS total_physical_size_bytes
FROM artifacts
GROUP BY COALESCE(NULLIF(content_type,''),'(blank)')
ORDER BY artifact_count DESC, content_type
LIMIT 5000
)SQL", log);
    manifest << "content_type_summary.csv,artifacts," << tableRowCount(db, "artifacts") << "," << FocusSampleLimit << ",content type counts with usage/path/size rollups\n";

    exportQuery(db, sampleDir / "store_content_type_summary.csv", R"SQL(
SELECT store_guid,
       COALESCE(NULLIF(content_type,''),'(blank)') AS content_type,
       COUNT(*) AS artifact_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_artifact_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc
FROM artifacts
GROUP BY store_guid, COALESCE(NULLIF(content_type,''),'(blank)')
ORDER BY store_guid, artifact_count DESC, content_type
LIMIT 5000
)SQL", log);
    manifest << "store_content_type_summary.csv,artifacts," << tableRowCount(db, "artifacts") << "," << FocusSampleLimit << ",content type counts by Spotlight store\n";

    exportQuery(db, sampleDir / "folder_activity_summary.csv", R"SQL(
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
HAVING COUNT(*) > 1
ORDER BY usage_child_count DESC, child_count DESC, store_guid, CAST(parent_inode_num AS INTEGER)
LIMIT 5000
)SQL", log);
    manifest << "folder_activity_summary.csv,artifacts," << tableRowCount(db, "artifacts") << "," << FocusSampleLimit << ",same-parent folder activity rollup\n";

    exportQuery(db, sampleDir / "recent_activity_focus.csv", R"SQL(
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,where_froms,confidence
FROM artifacts
WHERE COALESCE(last_updated_utc,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>''
ORDER BY COALESCE(NULLIF(last_used_date_utc,''), NULLIF(first_used_candidate_utc,''), NULLIF(last_updated_utc,'')) DESC, artifact_id
LIMIT 5000
)SQL", log);
    manifest << "recent_activity_focus.csv,artifacts," << tableRowCount(db, "artifacts") << "," << FocusSampleLimit << ",recent artifacts by usage/update signals\n";

    exportQuery(db, sampleDir / "volume_root_focus.csv", R"SQL(
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,content_type_tree,last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,is_mounted_volume_path,mounted_volume_name,external_volume_reason,confidence
FROM artifacts
WHERE COALESCE(content_type,'')='public.volume'
   OR COALESCE(is_mounted_volume_path,0)<>0
   OR COALESCE(mounted_volume_name,'')<>''
   OR COALESCE(external_volume_reason,'')<>''
ORDER BY store_guid, CAST(inode_num AS INTEGER), artifact_id
LIMIT 5000
)SQL", log);
    manifest << "volume_root_focus.csv,artifacts," << tableRowCount(db, "artifacts") << "," << FocusSampleLimit << ",volume root and mounted-volume indicators\n";

    log.info("Upload sample exports written to: " + pathString(sampleDir));
}

void SqliteExporter::exportQuery(CaseDatabase& db, const fs::path& file, const std::string& sql, Logger& log) const {
    appendExportRunStatus(file, 91, "export_query_prepare", file.filename().string());
    struct ExportProgressContext {
        fs::path file;
        std::chrono::steady_clock::time_point last;
        long long ticks = 0;
    } progressCtx{file, std::chrono::steady_clock::now(), 0};
    sqlite3_progress_handler(db.raw(), 500000, [](void* ptr) -> int {
        auto* ctx = static_cast<ExportProgressContext*>(ptr);
        ++ctx->ticks;
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - ctx->last).count() >= 15) {
            ctx->last = now;
            appendExportRunStatus(ctx->file, 92, "export_query_sql_progress", ctx->file.filename().string() + " sqlite_progress_ticks=" + std::to_string(ctx->ticks));
        }
        return 0;
    }, &progressCtx);
    auto stmt = db.prepare(sql);
    appendExportRunStatus(file, 91, "export_query_execute", file.filename().string());
    fs::create_directories(file.parent_path());
    std::vector<std::string> manifestRows;
    std::size_t part = 1;
    std::size_t rowsInPart = 0;
    std::size_t totalRows = 0;
    bool multipleParts = false;
    fs::path currentPath = file;
    std::ofstream out(ioPath(currentPath), std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write export: " + pathString(currentPath));
    writeHeader(out, stmt);
    out.flush();

    auto closePart = [&]() {
        out.flush(); out.close();
        manifestRows.push_back(currentPath.filename().string() + "," + std::to_string(part) + "," + std::to_string(rowsInPart));
    };

    while (stmt.stepRow()) {
        if (rowsInPart >= DefaultExportChunkRows) {
            if (!multipleParts) {
                out.flush(); out.close();
                fs::path firstPartPath = file.parent_path() / partName(file, 1);
                std::error_code ec;
                fs::remove(ioPath(firstPartPath), ec);
                ec.clear();
                fs::rename(ioPath(file), ioPath(firstPartPath), ec);
                if (ec) {
                    fs::copy_file(ioPath(file), ioPath(firstPartPath), fs::copy_options::overwrite_existing, ec);
                    if (ec) throw std::runtime_error("Unable to create chunked export part: " + pathString(firstPartPath) + ": " + ec.message());
                    fs::remove(ioPath(file), ec);
                }
                currentPath = firstPartPath;
                manifestRows.push_back(currentPath.filename().string() + "," + std::to_string(part) + "," + std::to_string(rowsInPart));
                multipleParts = true;
            } else {
                closePart();
            }
            appendExportRunStatus(file, 92, "export_query_rows", file.filename().string() + " rows=" + std::to_string(totalRows) + " parts=" + std::to_string(part));
            ++part; rowsInPart = 0;
            currentPath = file.parent_path() / partName(file, part);
            out.open(ioPath(currentPath), std::ios::binary);
            if (!out) throw std::runtime_error("Unable to write export: " + pathString(currentPath));
            writeHeader(out, stmt);
            out.flush();
        }
        writeRow(out, stmt);
        ++rowsInPart; ++totalRows;
        if ((totalRows % ExportProgressRowInterval) == 0) {
            appendExportRunStatus(file, 92, "export_query_rows", file.filename().string() + " rows=" + std::to_string(totalRows));
        }
    }
    if (!multipleParts) {
        out.flush();
        out.close();
        manifestRows.push_back(file.filename().string() + ",1," + std::to_string(totalRows));
    } else {
        closePart();
    }

    const fs::path manifest = file.parent_path() / (file.stem().string() + "_manifest.csv");
    std::ofstream mf(ioPath(manifest), std::ios::binary);
    mf << "file_name,part_index,row_count\n";
    for (const auto& r : manifestRows) mf << r << "\n";
    log.info("Export written: " + pathString(file) + " rows=" + std::to_string(totalRows) + (multipleParts ? " chunked=1" : " chunked=0"));
    appendExportRunStatus(file, 94, "export_query_complete", file.filename().string() + " rows=" + std::to_string(totalRows) + (multipleParts ? " chunked=1" : " chunked=0"));
    sqlite3_progress_handler(db.raw(), 0, nullptr, nullptr);
}

} // namespace vestigant::spotlight
