#pragma once

#include "db/sqlite_compat.h"
#include "gui/view_registry.h"

#include <string>

namespace vestigant::spotlight {

std::string sqlColumns(const ViewSpec& v);
std::string buildWhere(const ViewSpec& v,
                       const std::string& search,
                       int filterColumn = -1,
                       const std::string& filterValue = "");
void bindViewSearch(sqlite3_stmt* st,
                    const ViewSpec& v,
                    const std::string& search,
                    int filterColumn,
                    const std::string& filterValue,
                    int& index);
int viewColumnIndex(const ViewSpec& v, const char* columnName);
std::string stmtText(sqlite3_stmt* st, int col);
std::string resolveArtifactIdForVisibleRow(sqlite3* db, const ViewSpec& v, sqlite3_stmt* st);
std::string tagsForArtifact(sqlite3* db, const std::string& artifactId);

} // namespace vestigant::spotlight
