#pragma once
#include "app/models.h"
#include "core/logger.h"
#include "db/case_db.h"
#include <cstddef>

namespace vestigant::spotlight {
struct V7ImportCounts {
    std::size_t kvRows = 0;
    std::size_t dateCandidateRows = 0;
    std::size_t parsedDateRows = 0;
    std::size_t fullpathRows = 0;
    std::size_t filesImported = 0;
};
class V7OutputImporter {
public:
    V7ImportCounts importDirectory(const fs::path& v7OutputPath, const std::string& sourceId, CaseDatabase& db, Logger& log) const;
};
}
