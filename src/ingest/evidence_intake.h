#pragma once

#include "db/case_db.h"
#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>

namespace vestigant::spotlight {

// Narrow intake helpers split out of app_runner.cpp in V1.0.31.
// These helpers are intentionally behavior-preserving: orchestration remains in
// app_runner.cpp until the full staging/import boundary can be moved and tested.
bool endsWithCpp(const std::string& value, const std::string& suffix);
std::size_t countCsvDataRows(const std::filesystem::path& csvPath);
std::string normalizeIosPathFromZipEntryCpp(const std::string& fullName);
std::string basenameFromZipEntryCpp(std::string p);
std::string extensionFromNameCpp(const std::string& name);
std::string protectionClassHintCpp(const std::string& path);
std::string domainHintCpp(const std::string& path);
std::string appContainerHintCpp(const std::string& path);
std::filesystem::path extractedIosAppDbPathForZipEntryCpp(const std::filesystem::path& caseDir, const std::string& fullName);
std::pair<std::string, std::string> databaseCategoryAndAppHintCpp(const std::string& path, const std::string& name);

// Apply/restore aggressive SQLite settings only around regenerable iOS CSV
// inventory ingestion.  These functions are no-throw from the caller's point of
// view when used through the NoThrow variant so cleanup never masks parser errors.
void applyIosCsvBulkImportPragmas(CaseDatabase& db);
void restoreIosCsvBulkImportPragmasNoThrow(CaseDatabase& db);

} // namespace vestigant::spotlight
