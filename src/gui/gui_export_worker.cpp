#include "gui/gui_export_worker.h"

#include "db/sqlite_compat.h"
#include "gui/gui_view_helpers.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>

namespace vestigant::spotlight {
namespace {

std::string narrow(const std::wstring& w) {
    std::string out;
    out.reserve(w.size());
    for (wchar_t ch : w) {
        if (ch >= 0 && ch <= 0x7f) out.push_back(static_cast<char>(ch));
        else out.push_back('?');
    }
    return out;
}

std::wstring widenAscii(const std::string& s) {
    std::wstring out;
    out.reserve(s.size());
    for (unsigned char ch : s) out.push_back(static_cast<wchar_t>(ch));
    return out;
}


int exportSqliteBusyRetryHandler(void*, int count) {
    if (count > 50) return 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 1;
}

void configureExportSqliteConnection(sqlite3* db) {
    if (!db) return;
    sqlite3_busy_handler(db, exportSqliteBusyRetryHandler, nullptr);
    sqlite3_exec(db, "PRAGMA temp_store=MEMORY; PRAGMA cache_size=-65536; PRAGMA case_sensitive_like=OFF;", nullptr, nullptr, nullptr);
}

std::string csvEscape(const std::string& s) {
    const bool quote = s.find_first_of(",\r\n\"") != std::string::npos;
    if (!quote) return s;
    std::string out = "\"";
    for (char c : s) out += (c == '"') ? "\"\"" : std::string(1, c);
    out += "\"";
    return out;
}

class ReadOnlyExportDb {
public:
    explicit ReadOnlyExportDb(const std::wstring& path) {
        const std::string p = narrow(path);
        if (sqlite3_open_v2(p.c_str(), &db_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            const std::string msg = db_ ? sqlite3_errmsg(db_) : "unknown";
            if (db_) sqlite3_close_v2(db_);
            db_ = nullptr;
            throw std::runtime_error("Unable to open case database for export: " + msg);
        }
        configureExportSqliteConnection(db_);
    }
    ~ReadOnlyExportDb() { if (db_) sqlite3_close_v2(db_); }
    sqlite3* get() const { return db_; }
private:
    sqlite3* db_ = nullptr;
};

std::wstring withSuffixBeforeExtension(const std::wstring& path, const std::wstring& suffix) {
    const size_t slash = path.find_last_of(L"\\/");
    const size_t dot = path.find_last_of(L'.');
    if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)) return path.substr(0, dot) + suffix + path.substr(dot);
    return path + suffix;
}

std::string idListSql(const std::vector<long long>& ids) {
    std::vector<long long> sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    std::string out;
    for (long long id : sorted) {
        if (id <= 0) continue;
        if (!out.empty()) out += ',';
        out += std::to_string(id);
    }
    return out;
}

bool exportCancelled(const std::function<bool()>& shouldCancel) {
    return shouldCancel && shouldCancel();
}

std::size_t writeSqlCsv(sqlite3* db, const std::wstring& outPath, const std::string& sql, const std::function<bool()>& shouldCancel = {}) {
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    std::ofstream f(narrow(outPath), std::ios::binary);
    if (!f) { sqlite3_finalize(st); throw std::runtime_error("Unable to open CSV for writing"); }
    f << "\xEF\xBB\xBF";
    const int cols = sqlite3_column_count(st);
    for (int c = 0; c < cols; ++c) {
        if (c) f << ',';
        f << csvEscape(sqlite3_column_name(st, c) ? sqlite3_column_name(st, c) : "");
    }
    f << "\n";
    std::size_t rows = 0;
    while (true) {
        if (exportCancelled(shouldCancel)) { sqlite3_finalize(st); throw std::runtime_error("Export cancelled during SQLite scan"); }
        const int rc = sqlite3_step(st);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) { std::string msg = sqlite3_errmsg(db); sqlite3_finalize(st); throw std::runtime_error(msg); }
        for (int c = 0; c < cols; ++c) {
            if (c) f << ',';
            f << csvEscape(stmtText(st, c));
        }
        f << "\n";
        ++rows;
    }
    sqlite3_finalize(st);
    return rows;
}

void writeSupportManifest(const std::wstring& outPath, const std::vector<std::pair<std::wstring, std::size_t>>& files) {
    std::ofstream f(narrow(outPath), std::ios::binary);
    if (!f) throw std::runtime_error("Unable to open support manifest for writing");
    f << "\xEF\xBB\xBF";
    f << "file,row_count\n";
    for (const auto& item : files) f << csvEscape(narrow(item.first)) << ',' << item.second << "\n";
}

void exportSupportForArtifactIdSql(sqlite3* db, const std::string& idSql, const std::wstring& baseCsvPath, std::vector<std::pair<std::wstring, std::size_t>>& manifestRows, const std::function<bool()>& shouldCancel = {}) {
    const std::wstring rawKvPath = withSuffixBeforeExtension(baseCsvPath, L"_raw_key_values");
    const std::wstring rawDatesPath = withSuffixBeforeExtension(baseCsvPath, L"_raw_date_candidates");
    const std::wstring usagePath = withSuffixBeforeExtension(baseCsvPath, L"_usage_evidence");
    const std::wstring timelinePath = withSuffixBeforeExtension(baseCsvPath, L"_timeline_events");

    const std::string rawKvSql =
        "SELECT r.raw_kv_id,r.source_id,r.store_guid,r.source_db,r.inode_num,r.store_id,r.parent_inode_num,r.full_path,r.record_state,r.field_name,r.field_value "
        "FROM raw_key_values r JOIN artifacts a ON a.source_id=r.source_id AND a.store_guid=r.store_guid AND a.inode_num=r.inode_num "
        "WHERE a.artifact_id IN (" + idSql + ") ORDER BY a.artifact_id,r.raw_kv_id";
    manifestRows.push_back({rawKvPath, writeSqlCsv(db, rawKvPath, rawKvSql, shouldCancel)});

    const std::string rawDatesSql =
        "SELECT raw_date_id,artifact_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,file_name,best_path,field_name,field_value,parsed_utc,parse_method,date_type,association_status,association_confidence "
        "FROM raw_date_candidates WHERE artifact_id IN (" + idSql + ") ORDER BY artifact_id,parsed_utc,raw_date_id";
    manifestRows.push_back({rawDatesPath, writeSqlCsv(db, rawDatesPath, rawDatesSql, shouldCancel)});

    const std::string usageSql =
        "SELECT usage_id,artifact_id,source_id,store_guid,inode_num,field_name,field_value,parsed_utc FROM usage_evidence "
        "WHERE artifact_id IN (" + idSql + ") ORDER BY artifact_id,parsed_utc,usage_id";
    manifestRows.push_back({usagePath, writeSqlCsv(db, usagePath, usageSql, shouldCancel)});

    const std::string timelineSql =
        "SELECT timeline_id,artifact_id,source_id,store_guid,inode_num,file_name,path,event_timestamp_utc,event_type,event_source_field,existence_status,deleted_or_orphaned_candidate "
        "FROM timeline_events WHERE artifact_id IN (" + idSql + ") ORDER BY artifact_id,event_timestamp_utc,timeline_id";
    manifestRows.push_back({timelinePath, writeSqlCsv(db, timelinePath, timelineSql, shouldCancel)});
}

} // namespace

GuiExportResult GuiExportWorker::exportCurrentPage(const GuiViewExportRequest& request) {
    GuiExportResult result;
    try {
        ReadOnlyExportDb db(request.dbPath);
        const std::string where = buildWhere(request.view, request.search, request.filterColumn, request.filterValue);
        const std::string order = request.orderBy.empty() ? (request.view.orderBy ? request.view.orderBy : "") : request.orderBy;
        std::string sql = "SELECT " + sqlColumns(request.view) + " FROM " + request.view.tableName + where;
        if (!order.empty()) sql += " ORDER BY " + order;
        sql += " LIMIT ? OFFSET ?";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        int bind = 1;
        bindViewSearch(st, request.view, request.search, request.filterColumn, request.filterValue, bind);
        sqlite3_bind_int(st, bind++, request.pageSize > 0 ? request.pageSize : 1000);
        sqlite3_bind_int64(st, bind++, static_cast<sqlite3_int64>(request.page < 0 ? 0 : request.page) * (request.pageSize > 0 ? request.pageSize : 1000));
        std::ofstream f(narrow(request.outPath), std::ios::binary);
        if (!f) { sqlite3_finalize(st); throw std::runtime_error("Unable to open current-page CSV for writing"); }
        f << "\xEF\xBB\xBF";
        f << csvEscape("checked") << ',' << csvEscape("tags");
        for (std::size_t c = 0; c < request.view.columns.size(); ++c) f << ',' << csvEscape(request.view.columns[c]);
        f << "\n";
        std::size_t rows = 0;
        while (true) {
            if (exportCancelled(request.shouldCancel)) { sqlite3_finalize(st); throw std::runtime_error("Export cancelled during current-page scan"); }
            const int rc = sqlite3_step(st);
            if (rc == SQLITE_DONE) break;
            if (rc != SQLITE_ROW) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
            const std::string artifactId = resolveArtifactIdForVisibleRow(db.get(), request.view, st);
            const long long artifactIdNum = artifactId.empty() ? 0LL : std::strtoll(artifactId.c_str(), nullptr, 10);
            const bool checked = artifactIdNum != 0LL && request.checkedArtifactIds.find(artifactIdNum) != request.checkedArtifactIds.end();
            f << csvEscape(checked ? "1" : "0") << ',' << csvEscape(tagsForArtifact(db.get(), artifactId));
            for (int c = 0; c < sqlite3_column_count(st); ++c) f << ',' << csvEscape(stmtText(st, c));
            f << "\n";
            ++rows;
        }
        sqlite3_finalize(st);
        result.ok = true;
        result.rows = rows;
        result.message = L"Exported current page rows=" + std::to_wstring(static_cast<unsigned long long>(rows)) + L" to: " + request.outPath;
    } catch (const std::exception& ex) {
        result.ok = false;
        result.message = L"ERROR exporting current page: " + widenAscii(ex.what());
    }
    return result;
}

GuiExportResult GuiExportWorker::exportFilteredView(const GuiViewExportRequest& request) {
    GuiExportResult result;
    try {
        ReadOnlyExportDb db(request.dbPath);
        const std::string where = buildWhere(request.view, request.search, request.filterColumn, request.filterValue);
        const std::string order = request.orderBy.empty() ? (request.view.orderBy ? request.view.orderBy : "") : request.orderBy;
        std::string sql = "SELECT " + sqlColumns(request.view) + " FROM " + request.view.tableName + where;
        if (!order.empty()) sql += " ORDER BY " + order;
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        int bind = 1;
        bindViewSearch(st, request.view, request.search, request.filterColumn, request.filterValue, bind);
        std::ofstream f(narrow(request.outPath), std::ios::binary);
        if (!f) { sqlite3_finalize(st); throw std::runtime_error("Unable to open filtered CSV for writing"); }
        f << "\xEF\xBB\xBF";
        f << csvEscape("checked") << ',' << csvEscape("tags");
        for (std::size_t c = 0; c < request.view.columns.size(); ++c) f << ',' << csvEscape(request.view.columns[c]);
        f << "\n";
        std::size_t rows = 0;
        while (true) {
            if (exportCancelled(request.shouldCancel)) { sqlite3_finalize(st); throw std::runtime_error("Export cancelled during filtered-view scan"); }
            const int rc = sqlite3_step(st);
            if (rc == SQLITE_DONE) break;
            if (rc != SQLITE_ROW) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
            const std::string artifactId = resolveArtifactIdForVisibleRow(db.get(), request.view, st);
            const long long artifactIdNum = artifactId.empty() ? 0LL : std::strtoll(artifactId.c_str(), nullptr, 10);
            const bool checked = artifactIdNum != 0LL && request.checkedArtifactIds.find(artifactIdNum) != request.checkedArtifactIds.end();
            f << csvEscape(checked ? "1" : "0") << ',' << csvEscape(tagsForArtifact(db.get(), artifactId));
            for (int c = 0; c < sqlite3_column_count(st); ++c) f << ',' << csvEscape(stmtText(st, c));
            f << "\n";
            ++rows;
        }
        sqlite3_finalize(st);
        result.ok = true;
        result.rows = rows;
        result.message = L"Exported filtered view rows=" + std::to_wstring(static_cast<unsigned long long>(rows)) + L" to: " + request.outPath;
    } catch (const std::exception& ex) {
        result.ok = false;
        result.message = L"ERROR exporting filtered view: " + widenAscii(ex.what());
    }
    return result;
}

GuiExportResult GuiExportWorker::exportCheckedArtifacts(const std::wstring& dbPath,
                                                        const std::vector<long long>& artifactIds,
                                                        const std::wstring& outPath,
                                                        std::function<bool()> shouldCancel) {
    GuiExportResult result;
    try {
        const std::string ids = idListSql(artifactIds);
        if (ids.empty()) throw std::runtime_error("No checked artifact IDs were supplied for export.");
        ReadOnlyExportDb db(dbPath);
        std::ofstream f(narrow(outPath), std::ios::binary);
        if (!f) throw std::runtime_error("Unable to open checked-artifact CSV for writing");
        f << "\xEF\xBB\xBF";
        const std::vector<const char*> cols = {
            "artifact_id","tags","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path",
            "spotlight_display_path","normalized_mac_path","content_type","content_type_tree","last_updated_utc",
            "first_used_candidate_utc","last_used_date_utc","used_dates_count","use_count_value","usage_field_summary",
            "downloaded_date_utc","where_froms","is_mounted_volume_path","mounted_volume_name","external_volume_reason",
            "existence_status","confidence"
        };
        for (size_t i = 0; i < cols.size(); ++i) { if (i) f << ','; f << csvEscape(cols[i]); }
        f << "\n";
        const std::string sql =
            "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,spotlight_display_path,normalized_mac_path,"
            "content_type,content_type_tree,last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,usage_field_summary,"
            "downloaded_date_utc,where_froms,is_mounted_volume_path,mounted_volume_name,external_volume_reason,existence_status,confidence "
            "FROM artifacts WHERE artifact_id IN (" + ids + ") ORDER BY artifact_id";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        std::size_t rows = 0;
        while (true) {
            if (exportCancelled(shouldCancel)) { sqlite3_finalize(st); throw std::runtime_error("Export cancelled during checked-artifact scan"); }
            int rc = sqlite3_step(st);
            if (rc == SQLITE_DONE) break;
            if (rc != SQLITE_ROW) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
            const std::string artifactId = stmtText(st, 0);
            f << csvEscape(artifactId) << ',' << csvEscape(tagsForArtifact(db.get(), artifactId));
            for (int c = 1; c < sqlite3_column_count(st); ++c) f << ',' << csvEscape(stmtText(st, c));
            f << "\n";
            ++rows;
        }
        sqlite3_finalize(st);
        std::vector<std::pair<std::wstring, std::size_t>> supportFiles;
        exportSupportForArtifactIdSql(db.get(), ids, outPath, supportFiles, shouldCancel);
        const std::wstring manifestPath = withSuffixBeforeExtension(outPath, L"_support_manifest");
        supportFiles.insert(supportFiles.begin(), {outPath, rows});
        writeSupportManifest(manifestPath, supportFiles);
        result.ok = true;
        result.rows = rows;
        result.manifestPath = manifestPath;
        result.message = L"Exported " + std::to_wstring(static_cast<unsigned long long>(rows)) + L" checked artifact(s) plus raw support files. Manifest: " + manifestPath;
    } catch (const std::exception& ex) {
        result.ok = false;
        result.message = L"ERROR exporting checked artifacts: " + widenAscii(ex.what());
    }
    return result;
}

GuiExportResult GuiExportWorker::exportTaggedArtifacts(const std::wstring& dbPath,
                                                       long long tagId,
                                                       const std::wstring& outPath,
                                                       std::function<bool()> shouldCancel) {
    GuiExportResult result;
    try {
        if (tagId < 0) throw std::runtime_error("No tag ID was supplied for export.");
        ReadOnlyExportDb db(dbPath);
        const std::string tagIdSql = std::to_string(tagId);
        const std::string artifactIdSql = "SELECT artifact_id FROM artifact_tags WHERE tag_id=" + tagIdSql;
        const std::string artifactSql =
            "SELECT tag_id,tag_name,tagged_utc,artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,content_type_tree,logical_size_bytes,physical_size_bytes,usage_latest_utc,last_used_date_utc,modified_latest_utc,created_latest_utc,downloaded_latest_utc,where_froms,note_count,last_note_utc,last_note_text,confidence "
            "FROM vw_tagged_artifacts WHERE tag_id=" + tagIdSql + " ORDER BY COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(modified_latest_utc,''), artifact_id) DESC";
        std::vector<std::pair<std::wstring, std::size_t>> supportFiles;
        const std::size_t artifactRows = writeSqlCsv(db.get(), outPath, artifactSql, shouldCancel);
        supportFiles.push_back({outPath, artifactRows});
        exportSupportForArtifactIdSql(db.get(), artifactIdSql, outPath, supportFiles, shouldCancel);
        const std::wstring notesPath = withSuffixBeforeExtension(outPath, L"_notes");
        const std::string notesSql =
            "SELECT n.note_id,n.created_utc,n.updated_utc,n.note_text,n.artifact_id,n.store_guid,n.inode_num,n.parent_inode_num,n.file_name,n.display_name,n.best_path,n.content_type,n.tags "
            "FROM vw_artifact_notes n WHERE n.artifact_id IN (" + artifactIdSql + ") ORDER BY n.updated_utc DESC,n.created_utc DESC,n.note_id DESC";
        supportFiles.push_back({notesPath, writeSqlCsv(db.get(), notesPath, notesSql, shouldCancel)});
        const std::wstring manifestPath = withSuffixBeforeExtension(outPath, L"_support_manifest");
        writeSupportManifest(manifestPath, supportFiles);
        result.ok = true;
        result.rows = artifactRows;
        result.manifestPath = manifestPath;
        result.message = L"Exported tagged artifacts plus raw support files. Manifest: " + manifestPath;
    } catch (const std::exception& ex) {
        result.ok = false;
        result.message = L"ERROR exporting tagged artifacts: " + widenAscii(ex.what());
    }
    return result;
}

} // namespace vestigant::spotlight
