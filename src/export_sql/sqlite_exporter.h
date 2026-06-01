#pragma once
#include "db/case_db.h"
#include <filesystem>
#include <string>

namespace vestigant::spotlight {
class Logger;
class SqliteExporter {
public:
    void exportReviewPackage(CaseDatabase& db, const fs::path& exportDir, Logger& log, const std::string& exportProfile = "minimal") const;
private:
    void exportQuery(CaseDatabase& db, const fs::path& file, const std::string& sql, Logger& log) const;
    void exportUploadSamples(CaseDatabase& db, const fs::path& sampleDir, Logger& log) const;
};
}
