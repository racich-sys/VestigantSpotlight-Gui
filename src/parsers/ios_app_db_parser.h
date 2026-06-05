#pragma once

#include "db/case_db.h"
#include "db/sqlite_compat.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace vestigant::spotlight {

struct IosAppDbInventory {
    long long id = 0;
    std::string norm;
    std::string name;
    std::string cat;
    std::string app;
    std::filesystem::path extracted;
};

struct IosAppDbTableParseDecision {
    std::string recordCategory;
    bool useWhatsAppSpecialParser = false;
    bool useAppleMessagesSpecialParser = false;
    bool useKnowledgeCSpecialParser = false;
    std::string parserFamily;
};

std::string iosAppDbRecordCategory(const std::string& databaseCategory,
                                   const std::string& tableName,
                                   const std::string& columnsCsv);

bool iosAppDbIsTargetRecordCategory(const std::string& category);

bool iosAppDbShouldUseWhatsappSpecialParser(const std::string& databaseCategory,
                                            const std::string& tableName,
                                            const std::string& recordCategory);

bool iosAppDbShouldUseAppleMessagesSpecialParser(const std::string& databaseCategory,
                                                 const std::string& tableName,
                                                 const std::string& recordCategory);

bool iosAppDbShouldUseKnowledgeCSpecialParser(const std::string& recordCategory);

std::string iosAppDbBuildKnowledgeCTextSnippet(const std::string& stream,
                                               const std::string& bundleId,
                                               const std::string& startOrCreateDate,
                                               const std::string& endDate);

IosAppDbTableParseDecision iosAppDbBuildTableParseDecision(const std::string& databaseCategory,
                                                           const std::string& tableName,
                                                           const std::string& columnsCsv);

std::size_t iosAppDbParseTable(const std::string& sourceId,
                               const IosAppDbInventory& inv,
                               sqlite3* ext,
                               const std::string& table,
                               const IosAppDbTableParseDecision& parseDecision,
                               SqlStatement& parsedIns);

class IosAppDbParser {
public:
    static std::string recordCategory(const std::string& databaseCategory,
                                      const std::string& tableName,
                                      const std::string& columnsCsv);
    static bool isTargetRecordCategory(const std::string& category);
    static IosAppDbTableParseDecision buildTableParseDecision(const std::string& databaseCategory,
                                                              const std::string& tableName,
                                                              const std::string& columnsCsv);
    static std::size_t parseTable(const std::string& sourceId,
                                  const IosAppDbInventory& inv,
                                  sqlite3* ext,
                                  const std::string& table,
                                  const IosAppDbTableParseDecision& parseDecision,
                                  SqlStatement& parsedIns);
};

} // namespace vestigant::spotlight
