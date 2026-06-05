#pragma once

#include "parsers/apfs_volume_reader.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace vestigant::spotlight {

struct ApfsBtreeKvLocation {
    std::uint32_t entryIndex = 0;
    std::size_t keyOffset = 0;
    std::size_t keyLength = 0;
    std::size_t valueOffset = 0;
    std::size_t valueLength = 0;
    std::uint64_t rawKey = 0;
    std::uint64_t objectId = 0;
    std::uint8_t recordType = 0;
};

struct ApfsDirectoryIteratorResult {
    std::vector<ApfsDirectoryEntry> entries;
    ApfsDirectoryIteratorBenchmarks benchmarks;
    std::vector<std::string> warnings;
    bool lowerBoundReaderAvailable = false;
};

class ApfsAff4Reader {
public:
    using NodeBytes = std::vector<unsigned char>;
    using LeafLocator = std::function<std::uint64_t(std::uint64_t searchKey)>;
    using NodeReader = std::function<NodeBytes(std::uint64_t nodeOid)>;
    using KvDecoder = std::function<bool(const NodeBytes& node,
                                         std::uint32_t entryIndex,
                                         ApfsBtreeKvLocation& out,
                                         std::string& detail)>;
    using NextLeafReader = std::function<std::uint64_t(const NodeBytes& node,
                                                       std::uint64_t currentLeafOid)>;

    ApfsAff4Reader() = default;

    void setLeafLocator(LeafLocator fn) { leafLocator_ = std::move(fn); }
    void setNodeReader(NodeReader fn) { nodeReader_ = std::move(fn); }
    void setKvDecoder(KvDecoder fn) { kvDecoder_ = std::move(fn); }
    void setNextLeafReader(NextLeafReader fn) { nextLeafReader_ = std::move(fn); }

    ApfsDirectoryIteratorResult getDirectoryContents(std::uint64_t parentInodeId,
                                                     std::uint32_t maxMalformedNodes = 16);

    static std::optional<ApfsDirectoryEntry> decodeDirectoryRecord(const NodeBytes& node,
                                                                   const ApfsBtreeKvLocation& kv,
                                                                   std::string& detail);

private:
    LeafLocator leafLocator_;
    NodeReader nodeReader_;
    KvDecoder kvDecoder_;
    NextLeafReader nextLeafReader_;
};

} // namespace vestigant::spotlight
