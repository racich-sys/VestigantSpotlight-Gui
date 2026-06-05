#include "parsers/apfs_volume_reader.h"

#include <algorithm>
#include <cctype>
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

} // namespace

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
        if (!nextLeafReader_) break;
        const std::uint64_t nextNode = nextLeafReader_(node, currentNode);
        if (nextNode == 0 || nextNode == currentNode) break;
        benchmarks_.nextLeafTransitions++;
        currentNode = nextNode;
    }
    benchmarks_.directoryEntriesReturned = static_cast<std::uint64_t>(children.size());
    return children;
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
