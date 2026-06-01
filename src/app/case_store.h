#pragma once
#include "app/models.h"
#include "core/csv.h"
#include "core/logger.h"
#include "ingest/store_discovery.h"
#include "ingest/source_profiles.h"

namespace vestigant::spotlight {
class CaseStore {
public:
    explicit CaseStore(fs::path caseDir);
    const fs::path& root() const { return caseDir_; }
    fs::path exportsDir() const;
    fs::path logsDir() const;
    CaseInfo initialize(const RunOptions& opt, Logger& log) const;
    EvidenceSource addSource(const RunOptions& opt, Logger& log) const;
    void writeSources(const std::vector<EvidenceSource>& sources) const;
    void writeStoreInventory(const std::vector<StoreInfo>& stores) const;
    void writeSummary(const RunResult& result) const;
private:
    fs::path caseDir_;
};

std::string jsonEscape(const std::string& s);
}
