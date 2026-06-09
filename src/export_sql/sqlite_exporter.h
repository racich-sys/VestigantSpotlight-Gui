#pragma once
#include "db/case_db.h"
#include <filesystem>
#include <string>
#include <atomic>

namespace vestigant::spotlight {
class Logger;
class SqliteExporter {
public:
    void exportReviewPackage(CaseDatabase& db, const fs::path& exportDir, Logger& log, const std::string& exportProfile = "minimal", const std::atomic_bool* cancelToken = nullptr) const;
private:
    void exportQuery(CaseDatabase& db, const fs::path& file, const std::string& sql, Logger& log) const;
    mutable const std::atomic_bool* cancelToken_ = nullptr;
    mutable bool supportDataExportMode_ = false;
    mutable int exportTimeoutSeconds_ = 120;
    void exportUploadSamples(CaseDatabase& db, const fs::path& sampleDir, Logger& log) const;
};
}
