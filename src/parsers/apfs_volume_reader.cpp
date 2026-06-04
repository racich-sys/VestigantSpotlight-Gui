#include "parsers/apfs_volume_reader.h"

#include <algorithm>
#include <cctype>
#include <sstream>

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
    benchmarks_.lowerBoundLookups++;
    if (log_) {
        log_->info("APFS module directory iterator API not yet wired to AFF4 block reader; requested inode=" + std::to_string(directoryInodeId));
    }
    return {};
}

std::optional<std::uint64_t> ApfsVolumeReader::resolvePathToInode(const std::string& absolutePath) {
    if (absolutePath.empty() || absolutePath == "/") return kApfsRootDirectoryInode;
    if (log_) log_->info("APFS module path resolver API not yet wired to AFF4 block reader; requested path=" + absolutePath);
    return std::nullopt;
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
