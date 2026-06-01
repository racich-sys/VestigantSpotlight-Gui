#pragma once
#include "app/models.h"
#include "core/logger.h"
#include "core/csv.h"
#include <vector>

namespace vestigant::spotlight {
std::vector<StoreInfo> discoverStores(const EvidenceSource& source, SourceProfileKind profile, bool fullScan, Logger& log);
StoreInfo inspectStoreHeader(const fs::path& storePath, const fs::path& root, const EvidenceSource& source, Logger& log);
Rows storeInventoryRows(const std::vector<StoreInfo>& stores);
}
