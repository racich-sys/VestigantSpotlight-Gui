#include "parsers/apfs_volume_reader.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <set>

namespace vestigant::spotlight {
namespace {

std::string asciiLowerLocal(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool startsWith(const std::string& s, const char* prefix) {
    const std::string p(prefix ? prefix : "");
    return s.rfind(p, 0) == 0;
}

std::uint16_t readLe16Local(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 2U > data.size()) return 0;
    return static_cast<std::uint16_t>(data[off]) | (static_cast<std::uint16_t>(data[off + 1U]) << 8U);
}

std::uint32_t readLe32Local(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 4U > data.size()) return 0;
    return static_cast<std::uint32_t>(data[off]) |
           (static_cast<std::uint32_t>(data[off + 1U]) << 8U) |
           (static_cast<std::uint32_t>(data[off + 2U]) << 16U) |
           (static_cast<std::uint32_t>(data[off + 3U]) << 24U);
}

std::uint64_t readLe64Local(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 8U > data.size()) return 0;
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8U) | static_cast<std::uint64_t>(data[off + static_cast<std::size_t>(i)]);
    return v;
}

std::string bytesToUuidStringLocal(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 16U > data.size()) return "";
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < 16U; ++i) {
        if (i == 4U || i == 6U || i == 8U || i == 10U) os << '-';
        os << std::setw(2) << static_cast<unsigned int>(data[off + i]);
    }
    return os.str();
}

std::string apfsObjectTypeLabelLocal(std::uint32_t rawType) {
    const std::uint32_t base = rawType & 0x0000ffffU;
    switch (base) {
        case 0x0001U: return "OBJECT_TYPE_NX_SUPERBLOCK";
        case 0x0002U: return "OBJECT_TYPE_BTREE";
        case 0x0003U: return "OBJECT_TYPE_BTREE_NODE";
        case 0x000bU: return "OBJECT_TYPE_SPACEMAN";
        case 0x000cU: return "OBJECT_TYPE_SPACEMAN_CAB";
        case 0x000dU: return "OBJECT_TYPE_SPACEMAN_CIB";
        case 0x000eU: return "OBJECT_TYPE_OMAP";
        case 0x000fU: return "OBJECT_TYPE_CHECKPOINT_MAP";
        case 0x0010U: return "OBJECT_TYPE_FS";
        case 0x0011U: return "OBJECT_TYPE_FSTREE";
        case 0x0012U: return "OBJECT_TYPE_BLOCKREFTREE";
        case 0x0013U: return "OBJECT_TYPE_SNAPMETATREE";
        default: return "OBJECT_TYPE_" + std::to_string(base);
    }
}

} // namespace

ApfsNxSuperblockSummary parseApfsNxSuperblock(const std::vector<unsigned char>& data,
                                              std::uint64_t virtualOffset,
                                              long long bytesRead) {
    ApfsNxSuperblockSummary nx;
    nx.attempted = true;
    nx.virtualOffset = virtualOffset;
    nx.bytesRead = bytesRead;
    if (data.size() < 4096 || data.size() < 184) {
        nx.validationStatus = "BUFFER_TOO_SMALL";
        nx.notes = "Need at least the first APFS container block to parse nx_superblock_t.";
        return nx;
    }
    if (std::memcmp(data.data() + 32, "NXSB", 4) != 0) {
        nx.validationStatus = "NXSB_NOT_FOUND";
        nx.notes = "Magic NXSB was not present at object offset +32.";
        return nx;
    }
    nx.found = true;
    nx.oid = readLe64Local(data, 8);
    nx.xid = readLe64Local(data, 16);
    nx.objectTypeRaw = readLe32Local(data, 24);
    nx.objectSubtype = readLe32Local(data, 28);
    nx.objectTypeLabel = apfsObjectTypeLabelLocal(nx.objectTypeRaw);
    nx.blockSize = readLe32Local(data, 36);
    nx.blockCount = readLe64Local(data, 40);
    nx.features = readLe64Local(data, 48);
    nx.readonlyCompatibleFeatures = readLe64Local(data, 56);
    nx.incompatibleFeatures = readLe64Local(data, 64);
    nx.containerUuid = bytesToUuidStringLocal(data, 72);
    nx.nextOid = readLe64Local(data, 88);
    nx.nextXid = readLe64Local(data, 96);
    nx.xpDescBlocks = readLe32Local(data, 104);
    nx.xpDataBlocks = readLe32Local(data, 108);
    nx.xpDescBase = readLe64Local(data, 112);
    nx.xpDataBase = readLe64Local(data, 120);
    nx.xpDescNext = readLe32Local(data, 128);
    nx.xpDataNext = readLe32Local(data, 132);
    nx.xpDescIndex = readLe32Local(data, 136);
    nx.xpDescLen = readLe32Local(data, 140);
    nx.xpDataIndex = readLe32Local(data, 144);
    nx.xpDataLen = readLe32Local(data, 148);
    nx.spacemanOid = readLe64Local(data, 152);
    nx.omapOid = readLe64Local(data, 160);
    nx.reaperOid = readLe64Local(data, 168);
    nx.testType = readLe32Local(data, 176);
    nx.maxFileSystems = readLe32Local(data, 180);
    const std::uint32_t fsCountToRead = std::min<std::uint32_t>(nx.maxFileSystems, 100U);
    for (std::uint32_t i = 0; i < fsCountToRead; ++i) {
        const std::size_t off = 184U + static_cast<std::size_t>(i) * 8U;
        if (off + 8 > data.size()) break;
        const std::uint64_t oid = readLe64Local(data, off);
        if (oid != 0) nx.fsOids.push_back(oid);
    }
    if (nx.blockSize < 4096 || nx.blockSize > 65536 || (nx.blockSize & (nx.blockSize - 1U)) != 0U) {
        nx.validationStatus = "NXSB_FOUND_BLOCK_SIZE_SUSPICIOUS";
        nx.notes = "NXSB magic was present, but nx_block_size is outside the APFS expected power-of-two range.";
    } else if (nx.blockCount == 0) {
        nx.validationStatus = "NXSB_FOUND_BLOCK_COUNT_ZERO";
        nx.notes = "NXSB magic was present, but nx_block_count is zero.";
    } else {
        nx.containerSizeBytes = static_cast<std::uint64_t>(nx.blockSize) * nx.blockCount;
        nx.validationStatus = "NXSB_PARSED";
        nx.notes = "APFS container superblock parsed from the libaff4 virtual object at offset 0.";
    }
    return nx;
}


std::uint64_t apfsReadNextLeafOidFromBtreeInfoFooter(const std::vector<unsigned char>& node) {
    // Bounded helper used by comparator code and, beginning in V1.1.9, by the
    // guarded AFF4/APFS live probe to follow horizontal APFS B-tree leaf links.
    // It is deliberately bounds-checked and returns zero for malformed nodes.
    if (node.size() < 96U) return 0;
    const std::uint16_t btnFlags = readLe16Local(node, 32U);
    const bool isLeaf = (btnFlags & 0x0002U) != 0U;
    if (!isLeaf) return 0;
    const std::size_t footerStart = node.size() - 40U;
    if (footerStart + 40U > node.size()) return 0;
    return readLe64Local(node, footerStart + 32U);
}

std::uint64_t makeApfsSearchKey(std::uint64_t objectId, std::uint64_t type) {
    return ((type & 0x0fULL) << kApfsObjectTypeShift) | (objectId & kApfsObjectIdMask);
}

std::uint64_t apfsKeyObjectId(std::uint64_t rawKey) {
    return rawKey & kApfsObjectIdMask;
}

std::uint8_t apfsKeyRecordType(std::uint64_t rawKey) {
    return static_cast<std::uint8_t>((rawKey >> kApfsObjectTypeShift) & 0x0fU);
}

std::string apfsRecordTypeLabel(std::uint8_t type) {
    switch (type) {
        case kApfsTypeSnapshotMetadata: return "SNAP_METADATA";
        case kApfsTypeExtent: return "EXTENT";
        case kApfsTypeInode: return "INODE";
        case kApfsTypeXattr: return "XATTR";
        case kApfsTypeSiblingLink: return "SIBLING_LINK";
        case kApfsTypeDstreamId: return "DSTREAM_ID";
        case kApfsTypeCryptoState: return "CRYPTO_STATE";
        case kApfsTypeFileExtent: return "FILE_EXTENT";
        case kApfsTypeDirRecord: return "DIR_REC";
        case kApfsTypeDirStats: return "DIR_STATS";
        case kApfsTypeSnapName: return "SNAP_NAME";
        case kApfsTypeSiblingMap: return "SIBLING_MAP";
        case kApfsTypeFileInfo: return "FILE_INFO";
        default: return "UNKNOWN_" + std::to_string(static_cast<unsigned int>(type));
    }
}

bool apfsIsLikelyStoreV2GroupDirectoryName(const std::string& name) {
    if (name.size() != 36U) return false;
    const std::size_t hyphenPositions[] = {8U, 13U, 18U, 23U};
    for (std::size_t i = 0; i < name.size(); ++i) {
        bool shouldBeHyphen = false;
        for (const std::size_t hp : hyphenPositions) {
            if (i == hp) { shouldBeHyphen = true; break; }
        }
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (shouldBeHyphen) {
            if (name[i] != '-') return false;
        } else if (!std::isxdigit(c)) {
            return false;
        }
    }
    return true;
}

std::string apfsStoreV2ComponentKind(const std::string& name) {
    const std::string n = asciiLowerLocal(name);
    if (n == "store.db" || n == ".store.db") return "STORE_DB";
    if (startsWith(n, "dbstr-")) return "DBSTR_COMPONENT";
    if (startsWith(n, "dbhdr-")) return "DBHDR_COMPONENT";
    if (startsWith(n, "tmp.spotlight")) return "TMP_SPOTLIGHT_COMPONENT";
    if (n == "store.db-wal" || n == "store.db-shm") return "STORE_SQLITE_SUPPORT";
    if (n == "0.directorystorefile" || n == "0.directorystorefile.shadow") return "STOREV2_TOPLEVEL_COMPONENT";
    if (startsWith(n, "0.index") || startsWith(n, "0.shadowindex")) return "STOREV2_TOPLEVEL_COMPONENT";
    if (startsWith(n, "live.")) return "STOREV2_TOPLEVEL_COMPONENT";
    if (n == "reversestore.updates" || n == "store.updates" || n == "store_generation") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n == "reversedirectorystore" || n == "reversedirectorystore.shadow") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n == "permstore" || n == "journalexclusion" || n == "journals.migration_secondchance") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n == "cab.created" || n == "cab.modified" || n == "lion.created" || n == "lion.modified" || n == "star.created" || n == "star.modified") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n == "tmp.cab" || n == "tmp.lion" || n == "tmp.star") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n.size() > 4 && n.substr(n.size() - 4) == ".txt") return "STOREV2_CACHE_FILE";
    return "";
}

bool apfsIsSpotlightStoreV2ComponentName(const std::string& name) {
    return !apfsStoreV2ComponentKind(name).empty();
}

std::string apfsSanitizePathComponent(std::string name) {
    if (name.empty()) name = "unnamed";
    for (char& ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|' || c < 32) ch = '_';
    }
    if (name == "." || name == "..") name = "unnamed";
    if (name.size() > 160) name = name.substr(0, 160);
    return name;
}

ApfsExtractionStatus classifyApfsExtractionStatus(const std::string& copyStatus) {
    ApfsExtractionStatus status;
    if (copyStatus.empty()) {
        status.statusClass = "EMPTY_STATUS";
        return status;
    }
    if (copyStatus == "COPIED_GAPLESS_EXTENT_CHAIN" ||
        copyStatus == "COPIED_DIRECT_INDEXED_EXTENT_CHAIN" ||
        copyStatus == "COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS" ||
        startsWith(copyStatus, "COPIED_DECOMPFS_RESOURCE_FORK")) {
        status.copied = true;
        status.statusClass = "COMPLETE_COPY";
        status.syntheticZeroRegions = (copyStatus == "COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS");
        status.compressedOrResourceFork = startsWith(copyStatus, "COPIED_DECOMPFS_RESOURCE_FORK");
        return status;
    }
    if (copyStatus == "COPIED_PARTIAL_COMPRESSED_OR_RSRC_FORK_CANDIDATE") {
        status.copied = true;
        status.partial = true;
        status.compressedOrResourceFork = true;
        status.statusClass = "PARTIAL_COMPRESSED_OR_RSRC";
        return status;
    }
    if (startsWith(copyStatus, "SKIPPED") || startsWith(copyStatus, "COPY_FAILED") || startsWith(copyStatus, "FAILED")) {
        status.failed = true;
        status.statusClass = "NOT_COPIED";
        return status;
    }
    status.statusClass = "OTHER";
    return status;
}

bool apfsCopyStatusRepresentsCompleteFile(const std::string& copyStatus) {
    const auto c = classifyApfsExtractionStatus(copyStatus);
    return c.copied && !c.partial;
}

bool apfsCopyStatusRepresentsPartialCandidate(const std::string& copyStatus) {
    return classifyApfsExtractionStatus(copyStatus).partial;
}

ApfsVolumeReader::ApfsVolumeReader(std::string aff4StreamPath,
                                   std::uint64_t volumeRootTreeOid,
                                   std::uint64_t omapRootOid,
                                   Logger* log)
    : aff4StreamPath_(std::move(aff4StreamPath)),
      volumeRootTreeOid_(volumeRootTreeOid),
      omapRootOid_(omapRootOid),
      log_(log) {}

std::vector<ApfsDirectoryEntry> ApfsVolumeReader::enumerateDirectory(std::uint64_t directoryInodeId) {
    std::vector<ApfsDirectoryEntry> children;
    const std::uint64_t searchKey = makeApfsSearchKey(directoryInodeId, kApfsTypeDirRecord);
    benchmarks_.lowerBoundLookups++;
    if (!nodeReader_ || !kvDecoder_) {
        benchmarks_.malformedNodeStops++;
        if (log_) log_->warn("APFS directory iterator cannot run: node reader or key/value decoder is not configured.");
        return children;
    }

    std::uint64_t currentNode = 0;
    if (leafLocator_) {
        currentNode = leafLocator_(searchKey);
    } else {
        currentNode = omapResolver_ ? omapResolver_(volumeRootTreeOid_) : volumeRootTreeOid_;
    }
    if (currentNode == 0) return children;

    std::set<std::uint64_t> visited;
    while (currentNode != 0) {
        if (!visited.insert(currentNode).second) {
            benchmarks_.cycleStops++;
            if (log_) log_->warn("APFS B-tree cycle detected during directory enumeration at node " + std::to_string(currentNode));
            break;
        }
        NodeBytes node = nodeReader_(currentNode);
        if (node.size() < 64U) {
            benchmarks_.malformedNodeStops++;
            if (log_) log_->warn("APFS B-tree node too small during directory enumeration at node " + std::to_string(currentNode));
            break;
        }
        const std::uint16_t level = static_cast<std::uint16_t>(node[34]) | (static_cast<std::uint16_t>(node[35]) << 8U);
        const std::uint32_t nkeys = static_cast<std::uint32_t>(node[36]) |
                                    (static_cast<std::uint32_t>(node[37]) << 8U) |
                                    (static_cast<std::uint32_t>(node[38]) << 16U) |
                                    (static_cast<std::uint32_t>(node[39]) << 24U);
        if (level > 0U && !leafLocator_) {
            // Branch descent is intentionally callback-driven.  The current
            // APFS module knows the deterministic search key and record
            // termination rules, while the live reader supplies the OMAP and
            // branch child decoding details.
            benchmarks_.malformedNodeStops++;
            if (log_) log_->warn("APFS branch node encountered without leaf locator callback; lower-bound lookup cannot continue safely.");
            break;
        }

        bool passedTarget = false;
        for (std::uint32_t i = 0; i < nkeys; ++i) {
            ApfsVolumeBtreeKvLocation kv;
            kv.entryIndex = i;
            std::string detail;
            if (!kvDecoder_(node, i, kv, detail)) continue;
            if (kv.rawKey != 0) {
                kv.objectId = apfsKeyObjectId(kv.rawKey);
                kv.recordType = apfsKeyRecordType(kv.rawKey);
            }
            if (kv.objectId > directoryInodeId) {
                passedTarget = true;
                break;
            }
            if (kv.objectId != directoryInodeId || kv.recordType != kApfsTypeDirRecord) continue;
            if (kv.valueOffset + 8U > node.size() || kv.keyOffset + 12U > node.size() || kv.keyLength < 12U) continue;
            std::uint64_t childId = 0;
            for (std::size_t b = 0; b < 8U; ++b) childId |= (static_cast<std::uint64_t>(node[kv.valueOffset + b]) << (b * 8U));
            const std::uint32_t nameLenAndHash = static_cast<std::uint32_t>(node[kv.keyOffset + 8U]) |
                                                 (static_cast<std::uint32_t>(node[kv.keyOffset + 9U]) << 8U) |
                                                 (static_cast<std::uint32_t>(node[kv.keyOffset + 10U]) << 16U) |
                                                 (static_cast<std::uint32_t>(node[kv.keyOffset + 11U]) << 24U);
            const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
            const std::size_t nameOff = kv.keyOffset + 12U;
            const std::size_t available = (kv.keyLength > 12U && nameOff <= node.size()) ? std::min<std::size_t>(kv.keyLength - 12U, node.size() - nameOff) : 0U;
            std::string name;
            const std::size_t toCopy = std::min(nameLen, available);
            name.reserve(toCopy);
            for (std::size_t b = 0; b < toCopy; ++b) {
                const unsigned char c = node[nameOff + b];
                if (c == 0) break;
                name.push_back((c >= 32 && c != 127) ? static_cast<char>(c) : '_');
            }
            ApfsDirectoryEntry entry;
            entry.parentId = directoryInodeId;
            entry.childFileId = childId;
            entry.name = name;
            entry.provenance = "apfs_volume_reader_lower_bound_directory_iterator";
            children.push_back(std::move(entry));
        }
        benchmarks_.leafNodesVisited++;
        if (passedTarget) break;
        std::uint64_t nextNode = 0;
        if (nextLeafReader_) {
            nextNode = nextLeafReader_(node, currentNode);
        } else {
            nextNode = apfsReadNextLeafOidFromBtreeInfoFooter(node);
        }
        if (nextNode == 0 || nextNode == currentNode) break;
        benchmarks_.nextLeafTransitions++;
        currentNode = nextNode;
    }
    benchmarks_.directoryEntriesReturned = static_cast<std::uint64_t>(children.size());
    return children;
}


std::string ApfsVolumeReader::resolveAbsolutePath(std::uint64_t childInodeId) {
    if (!leafLocator_ || !nodeReader_ || !kvDecoder_) return {};
    if (childInodeId == 0 || childInodeId == kApfsRootDirectoryInode) return "/";
    std::vector<std::string> pathComponents;
    std::set<std::uint64_t> visited;
    std::uint64_t currentInode = childInodeId;
    for (std::size_t depth = 0; depth < 256U && currentInode != 0 && currentInode != kApfsRootDirectoryInode; ++depth) {
        if (!visited.insert(currentInode).second) break;
        const std::uint64_t searchKey = makeApfsSearchKey(currentInode, kApfsTypeInode);
        const std::uint64_t leafOid = leafLocator_(searchKey);
        if (!leafOid) break;
        const NodeBytes leaf = nodeReader_(leafOid);
        if (leaf.empty()) break;
        std::uint64_t parentId = 0;
        bool foundInode = false;
        for (std::uint32_t i = 0; i < 4096U; ++i) {
            ApfsVolumeBtreeKvLocation kv;
            std::string detail;
            if (!kvDecoder_(leaf, i, kv, detail)) break;
            if (kv.recordType == kApfsTypeInode && kv.objectId == currentInode && kv.valueOffset + 8U <= leaf.size()) {
                parentId = readLe64Local(leaf, kv.valueOffset);
                foundInode = true;
                break;
            }
        }
        if (!foundInode || parentId == 0 || parentId == currentInode) break;
        std::string name;
        const auto siblings = enumerateDirectory(parentId);
        for (const auto& entry : siblings) {
            if (entry.childFileId == currentInode) {
                name = entry.name;
                break;
            }
        }
        if (name.empty()) break;
        pathComponents.push_back(apfsSanitizePathComponent(name));
        currentInode = parentId;
    }
    if (pathComponents.empty()) return {};
    std::string out;
    for (auto it = pathComponents.rbegin(); it != pathComponents.rend(); ++it) {
        out += "/";
        out += *it;
    }
    return out.empty() ? std::string("/") : out;
}

std::optional<std::uint64_t> ApfsVolumeReader::resolvePathToInode(const std::string& absolutePath) {
    if (absolutePath.empty() || absolutePath == "/") return kApfsRootDirectoryInode;
    std::uint64_t current = kApfsRootDirectoryInode;
    std::size_t pos = 0;
    while (pos < absolutePath.size()) {
        while (pos < absolutePath.size() && absolutePath[pos] == '/') ++pos;
        if (pos >= absolutePath.size()) break;
        std::size_t next = absolutePath.find('/', pos);
        std::string part = absolutePath.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        if (part.empty()) break;
        const std::string want = asciiLowerLocal(part);
        bool found = false;
        for (const auto& child : enumerateDirectory(current)) {
            if (asciiLowerLocal(child.name) == want) {
                current = child.childFileId;
                found = true;
                break;
            }
        }
        if (!found) return std::nullopt;
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    return current;
}

bool ApfsVolumeReader::extractFileToDisk(std::uint64_t fileInodeId, const std::filesystem::path& destinationPath) {
    if (log_) {
        std::ostringstream os;
        os << "APFS module extraction API not yet wired to AFF4 block reader; inode=" << fileInodeId
           << " destination=" << destinationPath.string();
        log_->warn(os.str());
    }
    return false;
}

} // namespace vestigant::spotlight
