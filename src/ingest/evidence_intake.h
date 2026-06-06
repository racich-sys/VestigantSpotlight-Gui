#pragma once

#include "db/case_db.h"
#include <cstddef>
#include <functional>
#include <filesystem>
#include <string>
#include <utility>

namespace vestigant::spotlight {

class Logger;

// Narrow intake helpers split out of app_runner.cpp in V1.1.1.
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


class EvidenceIntake {
public:
    using RunStatusWriter = std::function<void(const std::filesystem::path&, const std::string&, const std::string&)>;

    static void importIosInventoryCsvs(CaseDatabase& db,
                                       const std::filesystem::path& caseDir,
                                       const std::string& sourceId,
                                       Logger& log,
                                       const std::filesystem::path& reuseCacheDir = {},
                                       bool materializeFullFfsInventory = true,
                                       RunStatusWriter statusWriter = {});

    static std::size_t importReferencedIosPathLookupFromReuseCache(CaseDatabase& db,
                                                                  const std::filesystem::path& caseDir,
                                                                  const std::string& sourceId,
                                                                  Logger& log,
                                                                  const std::filesystem::path& reuseCacheDir = {},
                                                                  RunStatusWriter statusWriter = {});
};

// Apply/restore aggressive SQLite settings only around regenerable iOS CSV
// inventory ingestion.  These functions are no-throw from the caller's point of
// view when used through the NoThrow variant so cleanup never masks parser errors.
void applyIosCsvBulkImportPragmas(CaseDatabase& db);
void restoreIosCsvBulkImportPragmasNoThrow(CaseDatabase& db);

} // namespace vestigant::spotlight
