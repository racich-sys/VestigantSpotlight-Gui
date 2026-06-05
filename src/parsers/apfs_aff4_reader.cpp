#include "parsers/apfs_aff4_reader.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace vestigant::spotlight {
namespace {

std::uint16_t readLe16Safe(const std::vector<unsigned char>& data, std::size_t off, bool& ok) {
    if (off + 2U > data.size()) { ok = false; return 0; }
    ok = true;
    return static_cast<std::uint16_t>(data[off]) |
           (static_cast<std::uint16_t>(data[off + 1]) << 8U);
}

std::uint32_t readLe32Safe(const std::vector<unsigned char>& data, std::size_t off, bool& ok) {
    if (off + 4U > data.size()) { ok = false; return 0; }
    ok = true;
    return static_cast<std::uint32_t>(data[off]) |
           (static_cast<std::uint32_t>(data[off + 1]) << 8U) |
           (static_cast<std::uint32_t>(data[off + 2]) << 16U) |
           (static_cast<std::uint32_t>(data[off + 3]) << 24U);
}

std::uint64_t readLe64Safe(const std::vector<unsigned char>& data, std::size_t off, bool& ok) {
    if (off + 8U > data.size()) { ok = false; return 0; }
    ok = true;
    std::uint64_t out = 0;
    for (std::size_t i = 0; i < 8U; ++i) out |= (static_cast<std::uint64_t>(data[off + i]) << (i * 8U));
    return out;
}

std::string safeName(const std::vector<unsigned char>& data, std::size_t off, std::size_t len) {
    if (off >= data.size()) return {};
    len = std::min(len, data.size() - off);
    while (len > 0 && data[off + len - 1] == 0) --len;
    std::string s;
    s.reserve(len);
    for (std::size_t i = 0; i < len; ++i) {
        const unsigned char c = data[off + i];
        s.push_back((c >= 32 && c != 127) ? static_cast<char>(c) : '_');
    }
    return s;
}

} // namespace

bool apfsAff4DecodeFixedKvAbs(const std::vector<unsigned char>& node,
                              std::uint32_t entryIndex,
                              std::size_t valueLenNeeded,
                              std::size_t& keyAbs,
                              std::size_t& valAbs,
                              std::string& detail) {
    if (node.size() < 64U) { detail = "node too small"; return false; }
    bool ok = false;
    const std::uint16_t btnFlags = readLe16Safe(node, 32U, ok);
    if (!ok) { detail = "node flags outside node buffer"; return false; }
    const bool fixedKv = (btnFlags & 0x0004U) != 0U;
    if (!fixedKv) { detail = "node does not advertise fixed-size key/value offsets"; return false; }
    const std::uint16_t tableSpaceOffset = readLe16Safe(node, 40U, ok);
    if (!ok) { detail = "table space offset outside node buffer"; return false; }
    const std::uint16_t tableSpaceLength = readLe16Safe(node, 42U, ok);
    if (!ok) { detail = "table space length outside node buffer"; return false; }
    const std::size_t btnDataStart = 56U;
    std::size_t tocStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset);
    if (tocStart >= node.size()) tocStart = btnDataStart;
    const std::uint32_t nkeys = readLe32Safe(node, 36U, ok);
    if (!ok) { detail = "nkeys outside node buffer"; return false; }
    std::size_t keyAreaStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset) + static_cast<std::size_t>(tableSpaceLength);
    if (keyAreaStart >= node.size()) keyAreaStart = tocStart + 4U * static_cast<std::size_t>(nkeys);
    std::size_t valueAreaEnd = node.size();
    if (valueAreaEnd >= 40U && (btnFlags & 0x0001U) != 0U) valueAreaEnd -= 40U;
    const std::size_t entryOff = tocStart + (static_cast<std::size_t>(entryIndex) * 4U);
    if (entryOff + 4U > node.size()) { detail = "TOC entry beyond node buffer"; return false; }
    const std::uint16_t keyOff = readLe16Safe(node, entryOff + 0U, ok);
    if (!ok) { detail = "key offset outside node buffer"; return false; }
    const std::uint16_t valOff = readLe16Safe(node, entryOff + 2U, ok);
    if (!ok) { detail = "value offset outside node buffer"; return false; }
    keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
    valAbs = (static_cast<std::size_t>(valOff) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(valOff)) : node.size();
    if (keyAbs + 16U > node.size()) { detail = "OMAP key outside node buffer"; return false; }
    if (valAbs + valueLenNeeded > node.size()) { detail = "OMAP value outside node buffer"; return false; }
    detail = "toc_start=" + std::to_string(tocStart) + "; key_area_start=" + std::to_string(keyAreaStart) + "; value_area_end=" + std::to_string(valueAreaEnd);
    return true;
}

bool apfsAff4DecodeGenericBtreeKvAbs(const std::vector<unsigned char>& node,
                                     std::uint32_t entryIndex,
                                     std::size_t& tocAbs,
                                     std::size_t& keyAbs,
                                     std::size_t& keyLen,
                                     std::size_t& valAbs,
                                     std::size_t& valLen,
                                     std::string& detail) {
    if (node.size() < 64U) { detail = "node too small"; return false; }
    bool ok = false;
    const std::uint16_t btnFlags = readLe16Safe(node, 32U, ok);
    if (!ok) { detail = "node flags outside node buffer"; return false; }
    const bool fixedKv = (btnFlags & 0x0004U) != 0U;
    const std::uint16_t tableSpaceOffset = readLe16Safe(node, 40U, ok);
    if (!ok) { detail = "table space offset outside node buffer"; return false; }
    const std::uint16_t tableSpaceLength = readLe16Safe(node, 42U, ok);
    if (!ok) { detail = "table space length outside node buffer"; return false; }
    const std::size_t btnDataStart = 56U;
    const std::uint32_t nkeys = readLe32Safe(node, 36U, ok);
    if (!ok) { detail = "nkeys outside node buffer"; return false; }
    const std::size_t entrySize = fixedKv ? 4U : 8U;
    std::size_t tocStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset);
    if (tocStart >= node.size()) tocStart = btnDataStart;
    std::size_t keyAreaStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset) + static_cast<std::size_t>(tableSpaceLength);
    if (keyAreaStart >= node.size()) keyAreaStart = tocStart + (entrySize * static_cast<std::size_t>(nkeys));
    std::size_t valueAreaEnd = node.size();
    if (valueAreaEnd >= 40U && (btnFlags & 0x0001U) != 0U) valueAreaEnd -= 40U;
    tocAbs = tocStart + (static_cast<std::size_t>(entryIndex) * entrySize);
    if (tocAbs + entrySize > node.size()) { detail = "TOC entry beyond node buffer"; return false; }
    if (fixedKv) {
        const std::uint16_t keyOff = readLe16Safe(node, tocAbs + 0U, ok);
        if (!ok) { detail = "key offset outside node buffer"; return false; }
        const std::uint16_t valOff = readLe16Safe(node, tocAbs + 2U, ok);
        if (!ok) { detail = "value offset outside node buffer"; return false; }
        keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
        keyLen = 16U;
        valAbs = (static_cast<std::size_t>(valOff) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(valOff)) : node.size();
        valLen = (valAbs + 16U <= node.size()) ? 16U : 0U;
    } else {
        const std::uint16_t keyOff = readLe16Safe(node, tocAbs + 0U, ok);
        if (!ok) { detail = "key offset outside node buffer"; return false; }
        const std::uint16_t keyLength = readLe16Safe(node, tocAbs + 2U, ok);
        if (!ok) { detail = "key length outside node buffer"; return false; }
        const std::uint16_t valOff = readLe16Safe(node, tocAbs + 4U, ok);
        if (!ok) { detail = "value offset outside node buffer"; return false; }
        const std::uint16_t valLength = readLe16Safe(node, tocAbs + 6U, ok);
        if (!ok) { detail = "value length outside node buffer"; return false; }
        keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
        keyLen = keyLength;
        valAbs = (static_cast<std::size_t>(valOff) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(valOff)) : node.size();
        valLen = valLength;
    }
    if (keyAbs > node.size() || keyLen > node.size() || keyAbs + keyLen > node.size()) { detail = "key outside node buffer"; return false; }
    if (valAbs > node.size() || valLen > node.size() || valAbs + valLen > node.size()) { detail = "value outside node buffer"; return false; }
    detail = "toc_start=" + std::to_string(tocStart) + "; key_area_start=" + std::to_string(keyAreaStart) + "; value_area_end=" + std::to_string(valueAreaEnd) + "; fixed_kv=" + (fixedKv ? "1" : "0");
    return true;
}

std::optional<ApfsDirectoryEntry> ApfsAff4Reader::decodeDirectoryRecord(const NodeBytes& node,
                                                                         const ApfsBtreeKvLocation& kv,
                                                                         std::string& detail) {
    if (kv.recordType != kApfsTypeDirRecord) {
        detail = "not_dir_rec";
        return std::nullopt;
    }
    if (kv.valueOffset + 8U > node.size()) {
        detail = "dir_rec_value_too_short";
        return std::nullopt;
    }
    if (kv.keyOffset + 12U > node.size() || kv.keyLength < 12U) {
        detail = "dir_rec_key_too_short";
        return std::nullopt;
    }
    bool ok = false;
    const std::uint64_t childId = readLe64Safe(node, kv.valueOffset, ok);
    if (!ok) {
        detail = "dir_rec_child_id_read_failed";
        return std::nullopt;
    }
    const std::uint32_t nameLenAndHash = readLe32Safe(node, kv.keyOffset + 8U, ok);
    if (!ok) {
        detail = "dir_rec_name_len_read_failed";
        return std::nullopt;
    }
    const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
    const std::size_t available = kv.keyLength > 12U ? kv.keyLength - 12U : 0U;
    ApfsDirectoryEntry entry;
    entry.parentId = kv.objectId;
    entry.childFileId = childId;
    entry.name = safeName(node, kv.keyOffset + 12U, std::min(nameLen, available));
    entry.provenance = "apfs_aff4_reader_dir_rec_lower_bound_iterator";
    detail = "decoded_dir_rec";
    return entry;
}

ApfsDirectoryIteratorResult ApfsAff4Reader::getDirectoryContents(std::uint64_t parentInodeId,
                                                                 std::uint32_t maxMalformedNodes) {
    ApfsDirectoryIteratorResult result;
    if (!leafLocator_ || !nodeReader_ || !kvDecoder_) {
        result.warnings.push_back("lower_bound_iterator_callbacks_not_configured");
        return result;
    }
    result.lowerBoundReaderAvailable = true;
    const std::uint64_t searchKey = makeApfsSearchKey(parentInodeId, kApfsTypeDirRecord);
    result.benchmarks.lowerBoundLookups++;
    std::uint64_t currentLeafOid = leafLocator_(searchKey);
    std::set<std::uint64_t> visited;
    std::uint32_t malformed = 0;
    while (currentLeafOid != 0) {
        if (!visited.insert(currentLeafOid).second) {
            result.benchmarks.cycleStops++;
            result.warnings.push_back("cycle_detected_oid=" + std::to_string(currentLeafOid));
            break;
        }
        result.benchmarks.leafNodesVisited++;
        NodeBytes leaf = nodeReader_(currentLeafOid);
        bool ok = false;
        const std::uint32_t nkeys = readLe32Safe(leaf, 36U, ok);
        if (!ok || leaf.empty()) {
            result.benchmarks.malformedNodeStops++;
            if (++malformed >= maxMalformedNodes) break;
            break;
        }
        bool targetPassed = false;
        for (std::uint32_t i = 0; i < nkeys; ++i) {
            ApfsBtreeKvLocation kv;
            kv.entryIndex = i;
            std::string detail;
            if (!kvDecoder_(leaf, i, kv, detail)) continue;
            if (kv.rawKey != 0) {
                kv.objectId = apfsKeyObjectId(kv.rawKey);
                kv.recordType = apfsKeyRecordType(kv.rawKey);
            }
            if (kv.objectId > parentInodeId) {
                targetPassed = true;
                break;
            }
            if (kv.objectId == parentInodeId && kv.recordType == kApfsTypeDirRecord) {
                std::string decodeDetail;
                auto entry = decodeDirectoryRecord(leaf, kv, decodeDetail);
                if (entry) result.entries.push_back(std::move(*entry));
            }
        }
        if (targetPassed) break;
        if (!nextLeafReader_) break;
        const std::uint64_t nextOid = nextLeafReader_(leaf, currentLeafOid);
        if (nextOid == 0 || nextOid == currentLeafOid) break;
        result.benchmarks.nextLeafTransitions++;
        currentLeafOid = nextOid;
    }
    result.benchmarks.directoryEntriesReturned = static_cast<std::uint64_t>(result.entries.size());
    return result;
}

} // namespace vestigant::spotlight
