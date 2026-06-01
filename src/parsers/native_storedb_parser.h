#pragma once
#include "app/models.h"
#include "db/case_db.h"
#include "core/logger.h"
#include "ingest/store_discovery.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vestigant::spotlight {

struct NativeStoreDbParseCounts {
    std::size_t storesSeen = 0;
    std::size_t validStores = 0;
    std::size_t metadataBlocks = 0;
    std::size_t decompressedBlocks = 0;
    std::size_t parsedItems = 0;
    std::size_t rawRecords = 0;
    std::size_t rawKeyValues = 0;
    std::size_t rawDateCandidates = 0;
    std::size_t failures = 0;
    std::size_t fallbackHeaderOnlyItems = 0;
};

enum class NativeDecodeMode {
    HeaderOnly,
    CoreFields,
    FullValues
};

class NativeStoreDbParser {
public:
    explicit NativeStoreDbParser(NativeDecodeMode decodeMode = NativeDecodeMode::HeaderOnly, std::size_t maxRecords = 0, std::size_t maxMetadataBlocks = 0)
        : decodeMode_(decodeMode), maxRecords_(maxRecords), maxMetadataBlocks_(maxMetadataBlocks) {}

    NativeStoreDbParser& setProgressPath(const std::filesystem::path& progressPath) {
        progressPath_ = progressPath;
        return *this;
    }

    NativeStoreDbParser& setPersistAllNativeKeyValues(bool persistAllNativeKeyValues) {
        persistAllNativeKeyValues_ = persistAllNativeKeyValues;
        return *this;
    }

    NativeStoreDbParser& setDbSizeGuardrailBytes(std::uintmax_t dbSizeGuardrailBytes) {
        dbSizeGuardrailBytes_ = dbSizeGuardrailBytes;
        return *this;
    }

    NativeStoreDbParseCounts parseStores(const std::vector<StoreInfo>& stores,
                                         const EvidenceSource& source,
                                         CaseDatabase& db,
                                         Logger& log);
private:
    NativeDecodeMode decodeMode_ = NativeDecodeMode::HeaderOnly;
    std::size_t maxRecords_ = 0;
    std::size_t maxMetadataBlocks_ = 0;
    std::filesystem::path progressPath_;
    bool persistAllNativeKeyValues_ = false;
    std::uintmax_t dbSizeGuardrailBytes_ = 5ull * 1024ull * 1024ull * 1024ull;
};

} // namespace vestigant::spotlight
