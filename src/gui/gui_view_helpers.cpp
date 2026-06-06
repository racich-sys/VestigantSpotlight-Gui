#include "gui/gui_view_helpers.h"

#include <cstdlib>
#include <vector>

namespace vestigant::spotlight {

std::string sqlColumns(const ViewSpec& v) {
    std::string out;
    for (std::size_t i = 0; i < v.columns.size(); ++i) {
        if (i) out += ',';
        out += v.columns[i];
    }
    return out;
}

std::string buildWhere(const ViewSpec& v,
                       const std::string& search,
                       int filterColumn,
                       const std::string& filterValue) {
    std::vector<std::string> clauses;
    if (!search.empty() && !v.searchColumns.empty()) {
        std::string q = "(";
        for (std::size_t i = 0; i < v.searchColumns.size(); ++i) {
            if (i) q += " OR ";
            q += "COALESCE(CAST(";
            q += v.searchColumns[i];
            q += " AS TEXT),'') LIKE ?";
        }
        q += ")";
        clauses.push_back(q);
    }
    if (filterColumn >= 0 && filterColumn < static_cast<int>(v.columns.size()) && !filterValue.empty()) {
        std::string q = "COALESCE(CAST(";
        q += v.columns[static_cast<std::size_t>(filterColumn)];
        q += " AS TEXT),'') LIKE ?";
        clauses.push_back(q);
    }
    if (clauses.empty()) return "";
    std::string where = " WHERE ";
    for (std::size_t i = 0; i < clauses.size(); ++i) {
        if (i) where += " AND ";
        where += clauses[i];
    }
    return where;
}

void bindViewSearch(sqlite3_stmt* st,
                    const ViewSpec& v,
                    const std::string& search,
                    int filterColumn,
                    const std::string& filterValue,
                    int& index) {
    if (!search.empty()) {
        const std::string pattern = "%" + search + "%";
        for (std::size_t i = 0; i < v.searchColumns.size(); ++i) {
            sqlite3_bind_text(st, index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
        }
    }
    if (filterColumn >= 0 && filterColumn < static_cast<int>(v.columns.size()) && !filterValue.empty()) {
        const std::string pattern = "%" + filterValue + "%";
        sqlite3_bind_text(st, index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    }
}

int viewColumnIndex(const ViewSpec& v, const char* columnName) {
    for (std::size_t i = 0; i < v.columns.size(); ++i) {
        if (std::string(v.columns[i]) == columnName) return static_cast<int>(i);
    }
    return -1;
}

std::string stmtText(sqlite3_stmt* st, int col) {
    if (col < 0 || col >= sqlite3_column_count(st)) return "";
    const unsigned char* raw = sqlite3_column_text(st, col);
    return raw ? reinterpret_cast<const char*>(raw) : "";
}

std::string resolveArtifactIdForVisibleRow(sqlite3* db, const ViewSpec& v, sqlite3_stmt* st) {
    const int artifactCol = viewColumnIndex(v, "artifact_id");
    if (artifactCol >= 0) {
        std::string id = stmtText(st, artifactCol);
        if (!id.empty()) return id;
    }
    const int storeCol = viewColumnIndex(v, "store_guid");
    int inodeCol = viewColumnIndex(v, "inode_num");
    if (inodeCol < 0) inodeCol = viewColumnIndex(v, "child_inode_num");
    if (inodeCol < 0) return "";
    const std::string storeGuid = storeCol >= 0 ? stmtText(st, storeCol) : "";
    const std::string inode = stmtText(st, inodeCol);
    if (inode.empty()) return "";
    sqlite3_stmt* q = nullptr;
    const char* sqlWithStore = "SELECT artifact_id FROM artifacts WHERE COALESCE(store_guid,'')=? AND CAST(inode_num AS TEXT)=? ORDER BY artifact_id LIMIT 1";
    const char* sqlNoStore = "SELECT artifact_id FROM artifacts WHERE CAST(inode_num AS TEXT)=? ORDER BY artifact_id LIMIT 1";
    const char* sql = storeGuid.empty() ? sqlNoStore : sqlWithStore;
    if (sqlite3_prepare_v2(db, sql, -1, &q, nullptr) != SQLITE_OK) return "";
    int b = 1;
    if (!storeGuid.empty()) sqlite3_bind_text(q, b++, storeGuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(q, b++, inode.c_str(), -1, SQLITE_TRANSIENT);
    std::string id;
    if (sqlite3_step(q) == SQLITE_ROW) id = stmtText(q, 0);
    sqlite3_finalize(q);
    return id;
}

std::string tagsForArtifact(sqlite3* db, const std::string& artifactId) {
    if (artifactId.empty()) return "";
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT GROUP_CONCAT(tag_name, '; ') FROM ("
        "SELECT it.tag_name FROM artifact_tags at "
        "JOIN investigator_tags it ON it.tag_id=at.tag_id "
        "WHERE at.artifact_id=? ORDER BY lower(it.tag_name))";
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return "";
    sqlite3_bind_int64(st, 1, std::strtoll(artifactId.c_str(), nullptr, 10));
    std::string out;
    if (sqlite3_step(st) == SQLITE_ROW) out = stmtText(st, 0);
    sqlite3_finalize(st);
    return out;
}

} // namespace vestigant::spotlight
