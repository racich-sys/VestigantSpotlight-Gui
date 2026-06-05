#pragma once

#include "core/logger.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vestigant::spotlight {

constexpr std::uint64_t kApfsObjectIdMask = 0x0fffffffffffffffULL;
constexpr std::uint64_t kApfsObjectTypeShift = 60ULL;
constexpr std::uint8_t kApfsTypeSnapshotMetadata = 1U;
constexpr std::uint8_t kApfsTypeExtent = 2U;
constexpr std::uint8_t kApfsTypeInode = 3U;
constexpr std::uint8_t kApfsTypeXattr = 4U;
constexpr std::uint8_t kApfsTypeSiblingLink = 5U;
constexpr std::uint8_t kApfsTypeDstreamId = 6U;
constexpr std::uint8_t kApfsTypeCryptoState = 7U;
constexpr std::uint8_t kApfsTypeFileExtent = 8U;
constexpr std::uint8_t kApfsTypeDirRecord = 9U;
constexpr std::uint8_t kApfsTypeDirStats = 10U;
constexpr std::uint8_t kApfsTypeSnapName = 11U;
constexpr std::uint8_t kApfsTypeSiblingMap = 12U;
constexpr std::uint8_t kApfsTypeFileInfo = 13U;
constexpr std::uint64_t kApfsRootDirectoryInode = 2ULL;

struct ApfsDirectoryEntry {
    std::uint32_t volumeSequence = 0;
    std::uint64_t parentId = 0;
    std::uint64_t childFileId = 0;
    std::string name;
    std::uint8_t itemType = 0;
    std::string absolutePath;
    std::string provenance;
};

struct ApfsExtractionStatus {
    bool copied = false;
    bool partial = false;
    bool syntheticZeroRegions = false;
    bool compressedOrResourceFork = false;
    bool failed = false;
    std::string statusClass;
};

struct ApfsVolumeBtreeKvLocation {
    std::uint32_t entryIndex = 0;
    std::size_t keyOffset = 0;
    std::size_t keyLength = 0;
    std::size_t valueOffset = 0;
    std::size_t valueLength = 0;
    std::uint64_t rawKey = 0;
    std::uint64_t objectId = 0;
    std::uint8_t recordType = 0;
};

struct ApfsDirectoryIteratorBenchmarks {
    std::uint64_t lowerBoundLookups = 0;
    std::uint64_t leafNodesVisited = 0;
    std::uint64_t nextLeafTransitions = 0;
    std::uint64_t cycleStops = 0;
    std::uint64_t malformedNodeStops = 0;
    std::uint64_t directoryEntriesReturned = 0;
};

std::uint64_t makeApfsSearchKey(std::uint64_t objectId, std::uint64_t type);
std::uint64_t apfsKeyObjectId(std::uint64_t rawKey);
std::uint8_t apfsKeyRecordType(std::uint64_t rawKey);
std::string apfsRecordTypeLabel(std::uint8_t type);

bool apfsIsLikelyStoreV2GroupDirectoryName(const std::string& name);
bool apfsIsSpotlightStoreV2ComponentName(const std::string& name);
std::string apfsStoreV2ComponentKind(const std::string& name);
std::string apfsSanitizePathComponent(std::string name);

ApfsExtractionStatus classifyApfsExtractionStatus(const std::string& copyStatus);
bool apfsCopyStatusRepresentsCompleteFile(const std::string& copyStatus);
bool apfsCopyStatusRepresentsPartialCandidate(const std::string& copyStatus);

class ApfsVolumeReader {
public:
    using NodeBytes = std::vector<unsigned char>;
    using OmapResolver = std::function<std::uint64_t(std::uint64_t virtualOid)>;
    using NodeReader = std::function<NodeBytes(std::uint64_t physicalAddressOrOid)>;
    using LeafLocator = std::function<std::uint64_t(std::uint64_t searchKey)>;
    using KvDecoder = std::function<bool(const NodeBytes& node,
                                         std::uint32_t entryIndex,
                                         ApfsVolumeBtreeKvLocation& out,
                                         std::string& detail)>;
    using NextLeafReader = std::function<std::uint64_t(const NodeBytes& node,
                                                       std::uint64_t currentLeafPhysicalAddressOrOid)>;

    ApfsVolumeReader(std::string aff4StreamPath,
                     std::uint64_t volumeRootTreeOid,
                     std::uint64_t omapRootOid,
                     Logger* log = nullptr);

    const std::string& aff4StreamPath() const noexcept { return aff4StreamPath_; }
    std::uint64_t volumeRootTreeOid() const noexcept { return volumeRootTreeOid_; }
    std::uint64_t omapRootOid() const noexcept { return omapRootOid_; }

    void setOmapResolver(OmapResolver fn) { omapResolver_ = std::move(fn); }
    void setNodeReader(NodeReader fn) { nodeReader_ = std::move(fn); }
    void setLeafLocator(LeafLocator fn) { leafLocator_ = std::move(fn); }
    void setKvDecoder(KvDecoder fn) { kvDecoder_ = std::move(fn); }
    void setNextLeafReader(NextLeafReader fn) { nextLeafReader_ = std::move(fn); }

    // Lower-bound directory iterator API.  It can be used immediately in tests
    // with injected callbacks and can later be bound to the live AFF4/APFS
    // block reader without changing app_runner orchestration.
    std::vector<ApfsDirectoryEntry> enumerateDirectory(std::uint64_t directoryInodeId);
    std::optional<std::uint64_t> resolvePathToInode(const std::string& absolutePath);
    bool extractFileToDisk(std::uint64_t fileInodeId, const std::filesystem::path& destinationPath);

    const ApfsDirectoryIteratorBenchmarks& benchmarks() const noexcept { return benchmarks_; }

private:
    std::string aff4StreamPath_;
    std::uint64_t volumeRootTreeOid_ = 0;
    std::uint64_t omapRootOid_ = 0;
    Logger* log_ = nullptr;
    OmapResolver omapResolver_;
    NodeReader nodeReader_;
    LeafLocator leafLocator_;
    KvDecoder kvDecoder_;
    NextLeafReader nextLeafReader_;
    ApfsDirectoryIteratorBenchmarks benchmarks_;
};

} // namespace vestigant::spotlight
