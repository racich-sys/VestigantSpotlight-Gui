#pragma once
#include "app/models.h"
#include "db/case_db.h"

namespace vestigant::spotlight {
class Logger;
struct SqliteEnrichmentCounts {
    std::size_t artifacts = 0;
    std::size_t usage = 0;
    std::size_t timeline = 0;
    std::size_t orphanCandidates = 0;
};

class SqliteEnrichment {
public:
    SqliteEnrichmentCounts run(CaseDatabase& db, const EvidenceSource& source, Logger& log) const;
private:
    void classifyFilesystem(CaseDatabase& db, const EvidenceSource& source, Logger& log, SqliteEnrichmentCounts& counts) const;
};
}
