#pragma once

#include <string>

namespace vestigant::spotlight {

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

} // namespace vestigant::spotlight
