#pragma once
#include "app/models.h"
#include "ingest/store_discovery.h"
#include <filesystem>
#include <string>
#include <vector>

namespace vestigant::spotlight {
namespace fs = std::filesystem;

class CaseDatabase;
class Logger;

struct PreservedEvidenceFile {
    fs::path originalPath;
    fs::path preservedPath;
    std::string relativePath;
    std::string fileRole;
    std::uintmax_t sizeBytes = 0;
    std::string sha256;
};

struct PreservationResult {
    bool attempted = false;
    bool preserved = false;
    bool archiveCreated = false;
    bool archiveVerified = false;
    fs::path preservationDir;
    fs::path stagingRoot;
    fs::path archivePath;
    fs::path parseInputRoot;
    std::string archiveSha256;
    std::uintmax_t archiveSizeBytes = 0;
    std::uintmax_t totalOriginalBytes = 0;
    std::size_t fileCount = 0;
    std::string status;
    std::string integrityStatus;
    std::string message;
};

class EvidencePreserver {
public:
    PreservationResult preserve(const RunOptions& opt,
                                const EvidenceSource& source,
                                const std::vector<StoreInfo>& stores,
                                CaseDatabase& db,
                                Logger& log) const;
private:
    std::vector<PreservedEvidenceFile> identifyFiles(const RunOptions& opt,
                                                      const EvidenceSource& source,
                                                      const std::vector<StoreInfo>& stores,
                                                      Logger& log) const;
};

} // namespace vestigant::spotlight
