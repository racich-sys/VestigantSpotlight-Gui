#include "parsers/apfs_aff4_reader.h"
#include "core/path_utils.h"
#include "core/csv.h"
#include "core/app_info.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>
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

// AFF4 stream inventory helpers moved from app_runner.cpp in V1.1.1.

namespace {

bool hasExtensionInsensitiveLocal(const std::filesystem::path& p, const std::string& ext) {
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return e == ext;
}

bool isZipSourcePathLocal(const std::filesystem::path& p) { return hasExtensionInsensitiveLocal(p, ".zip"); }
bool isAff4SourcePathLocal(const std::filesystem::path& p) { return hasExtensionInsensitiveLocal(p, ".aff4"); }
bool isRawImageSourcePathLocal(const std::filesystem::path& p) {
    return hasExtensionInsensitiveLocal(p, ".img") || hasExtensionInsensitiveLocal(p, ".dd") || hasExtensionInsensitiveLocal(p, ".raw");
}

std::string inputSourceTypeLocal(const std::filesystem::path& p) {
    if (isZipSourcePathLocal(p)) return "ZIP_SPOTLIGHT_OR_FILESYSTEM_CONTAINER";
    if (isAff4SourcePathLocal(p)) return "AFF4_CONTAINER";
    if (isRawImageSourcePathLocal(p)) return "RAW_FLAT_IMAGE";
    std::error_code ec;
    if (std::filesystem::is_directory(p, ec)) return "FOLDER_OR_EXTRACTED_FILESYSTEM_ROOT";
    if (std::filesystem::is_regular_file(p, ec)) return "UNRECOGNIZED_FILE_SOURCE";
    return "UNKNOWN_OR_MISSING_SOURCE";
}

} // namespace


std::string commandQuoteLocal(const std::string& s) {
    std::string out = "\"";
    for (char ch : s) {
        if (ch == '"') out += "\\\"";
        else out.push_back(ch);
    }
    out += "\"";
    return out;
}

std::string commandQuoteLocal(const std::filesystem::path& p) { return commandQuoteLocal(pathString(p)); }

std::string shellRedirectCommand(const std::filesystem::path& exe, const std::vector<std::string>& args, const std::filesystem::path& outputFile) {
    std::string cmd = commandQuoteLocal(exe);
    for (const auto& a : args) cmd += " " + commandQuoteLocal(a);
    cmd += " > " + commandQuoteLocal(outputFile) + " 2>&1";
    return cmd;
}

std::string trimLine(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    std::size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) ++start;
    if (start > 0) s.erase(0, start);
    return s;
}

Aff4StreamInventoryEntry classifyAff4StreamLine(int index, const std::string& line) {
    Aff4StreamInventoryEntry e;
    e.lineIndex = index;
    e.rawLine = line;
    const std::string l = toLower(line);
    e.candidateType = "UNCLASSIFIED_STREAM_LINE";
    e.apfsRelevance = "UNKNOWN_UNTIL_STREAM_EXPORTED_OR_READ";
    e.recommendedAction = "Review line manually; no automatic extraction selected.";
    if (l.find("physicalmemory") != std::string::npos || l.find("/memory") != std::string::npos || l.find("kallsyms") != std::string::npos || l.find("/proc/") != std::string::npos) {
        e.candidateType = "LIKELY_MEMORY_OR_OS_RUNTIME_STREAM";
        e.candidateScore -= 40;
        e.apfsRelevance = "LOW";
        e.recommendedAction = "Do not prioritize for APFS/Spotlight extraction unless no disk/image streams are present.";
    }
    if (l.find("aff4:image") != std::string::npos || l.find("imagestream") != std::string::npos || l.find("image stream") != std::string::npos) {
        e.candidateType = "AFF4_IMAGE_STREAM_CANDIDATE";
        e.candidateScore += 50;
    }
    if (l.find("physicaldrive") != std::string::npos || l.find("physical drive") != std::string::npos || l.find("/dev/disk") != std::string::npos || l.find("/dev/rdisk") != std::string::npos || l.find("disk0") != std::string::npos || l.find("disk1") != std::string::npos) {
        e.candidateType = "PHYSICAL_DISK_STREAM_CANDIDATE";
        e.candidateScore += 70;
        e.apfsRelevance = "HIGH_IF_MAC_OR_IOS_IMAGE";
        e.recommendedAction = "Prioritize this stream for controlled export or stream-backed APFS probing.";
    }
    if (l.find("apfs") != std::string::npos || l.find("nxsb") != std::string::npos) {
        e.candidateScore += 40;
        e.apfsRelevance = "HIGH_APFS_NAME_HINT";
        e.recommendedAction = "Use this stream as an APFS candidate if the stream can be exported or read safely.";
    }
    if (l.find("spotlight") != std::string::npos || l.find("corespotlight") != std::string::npos || l.find("index.db") != std::string::npos || l.find("store-v2") != std::string::npos || l.find("store.db") != std::string::npos) {
        e.candidateScore += 60;
        e.apfsRelevance = "HIGH_SPOTLIGHT_NAME_HINT";
        e.recommendedAction = "Preserve as a Spotlight/CoreSpotlight candidate; later extraction should stage this path with provenance.";
    }
    if (l.find("map") != std::string::npos && e.candidateScore < 40) {
        e.candidateType = "AFF4_MAP_OR_SPARSE_STREAM_CANDIDATE";
        e.candidateScore += 25;
        e.recommendedAction = "Review as a possible sparse disk/image map; confirm with AFF4 metadata before export.";
    }
    if (e.candidateScore >= 80 && e.candidateType == "UNCLASSIFIED_STREAM_LINE") e.candidateType = "HIGH_VALUE_STREAM_CANDIDATE";
    else if (e.candidateScore >= 40 && e.candidateType == "UNCLASSIFIED_STREAM_LINE") e.candidateType = "POSSIBLE_IMAGE_STREAM_CANDIDATE";
    if (e.candidateScore < 0) e.notes = "Negative score indicates a probable non-disk stream for Spotlight/APFS purposes.";
    else if (e.candidateScore == 0) e.notes = "Raw stream-list line retained for provenance; classification requires manual review.";
    else e.notes = "Heuristic classification only; do not treat as validated APFS until exported/read and probed.";
    return e;
}

Aff4StreamInventoryResult runAff4StreamInventory(const RunOptions& opt,
                                                 const EvidenceSource& source,
                                                 const std::filesystem::path& originalInput,
                                                 const std::filesystem::path& caseDir,
                                                 Logger& log,
                                                 Aff4ToolResolver toolResolver,
                                                 Aff4ExecutableRunner executableRunner,
                                                 Aff4ShellCommandRunner shellRunner) {
#if defined(_WIN32)
    (void)shellRunner; // Reserved for non-Windows shell-style AFF4 inventory fallbacks.
#else
    (void)executableRunner; // Windows uses direct process execution; non-Windows uses shellRunner.
#endif
    Aff4StreamInventoryResult result;
    result.status = "NOT_AFF4_SOURCE";
    if (!isAff4SourcePathLocal(originalInput)) return result;
    const std::vector<std::string> names = {
#ifdef _WIN32
        "aff4imager.exe", "aff4imager"
#else
        "aff4imager"
#endif
    };
    result.toolPath = toolResolver ? toolResolver("VESTIGANT_AFF4IMAGER", names) : std::filesystem::path{};
    result.rawOutputPath = caseDir / "aff4_stream_inventory_raw.txt";
    const std::filesystem::path csvPath = caseDir / "aff4_stream_inventory.csv";
    const std::filesystem::path planPath = caseDir / "AFF4_STREAM_SELECTION_PLAN.md";
    if (!opt.enableAff4StreamInventory) {
        result.status = opt.strictSingleAff4 ? "SKIPPED_STRICT_SINGLE_AFF4" : "SKIPPED_BY_DEFAULT";
        std::ofstream raw(result.rawOutputPath, std::ios::binary);
        raw << "External aff4imager stream listing skipped. It is opt-in only because external AFF4 tools may inspect related AFF4 files outside the selected evidence file. Use --enable-aff4-stream-inventory only for deliberate stream-list testing.\n";
    } else if (result.toolPath.empty()) {
        result.status = "AFF4IMAGER_MISSING_NOT_INVOKED";
        std::ofstream raw(result.rawOutputPath, std::ios::binary);
        raw << "aff4imager was not found. Configure --reader-tools, VESTIGANT_READER_TOOLS, VESTIGANT_AFF4IMAGER, or PATH.\n";
    } else {
        result.status = "AFF4IMAGER_INVOKED_LIST_STREAMS";
        const std::string cmd = shellRedirectCommand(result.toolPath, {"-l", pathString(originalInput)}, result.rawOutputPath);
        log.info("Running AFF4 stream inventory command: " + cmd);
#if defined(_WIN32)
        result.commandExitCode = executableRunner ? executableRunner(result.toolPath, {"-l", pathString(originalInput)}, result.rawOutputPath) : -1;
#else
        result.commandExitCode = shellRunner ? shellRunner(cmd) : -1;
#endif
        log.info("AFF4 stream inventory command completed: exit_code=" + std::to_string(result.commandExitCode));
        if (result.commandExitCode != 0) result.status = "AFF4IMAGER_LIST_STREAMS_NONZERO_EXIT";
    }
    try {
        std::ifstream in(result.rawOutputPath, std::ios::binary);
        std::string line;
        int idx = 0;
        while (std::getline(in, line)) {
            line = trimLine(line);
            if (line.empty()) continue;
            ++idx;
            ++result.rawLineCount;
            if (line.size() > 4000) line = line.substr(0, 4000) + "...";
            result.entries.push_back(classifyAff4StreamLine(idx, line));
        }
    } catch (...) {}
    {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,tool_path,status,command_exit_code,line_index,candidate_score,candidate_type,apfs_relevance,recommended_action,notes,raw_line\n";
        if (result.entries.empty()) {
            out << csvEscape(source.sourceId) << ','
                << csvEscape(pathString(originalInput)) << ','
                << csvEscape(inputSourceTypeLocal(originalInput)) << ','
                << csvEscape(pathString(result.toolPath)) << ','
                << csvEscape(result.status) << ','
                << result.commandExitCode << ",0,0,NO_STREAM_LINES_RECORDED,UNKNOWN,Review raw output and tool readiness.,No AFF4 stream lines were parsed," << csvEscape("") << "\n";
        } else {
            for (const auto& e : result.entries) {
                out << csvEscape(source.sourceId) << ','
                    << csvEscape(pathString(originalInput)) << ','
                    << csvEscape(inputSourceTypeLocal(originalInput)) << ','
                    << csvEscape(pathString(result.toolPath)) << ','
                    << csvEscape(result.status) << ','
                    << result.commandExitCode << ','
                    << e.lineIndex << ','
                    << e.candidateScore << ','
                    << csvEscape(e.candidateType) << ','
                    << csvEscape(e.apfsRelevance) << ','
                    << csvEscape(e.recommendedAction) << ','
                    << csvEscape(e.notes) << ','
                    << csvEscape(e.rawLine) << "\n";
            }
        }
    }
    std::vector<Aff4StreamInventoryEntry> candidates = result.entries;
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        if (a.candidateScore != b.candidateScore) return a.candidateScore > b.candidateScore;
        return a.lineIndex < b.lineIndex;
    });
    {
        std::ofstream out(planPath, std::ios::binary);
        out << "# AFF4 Stream Selection Plan\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Source\n\n";
        out << "- Source ID: `" << source.sourceId << "`\n";
        out << "- Input path: `" << pathString(originalInput) << "`\n";
        out << "- Tool path: `" << pathString(result.toolPath) << "`\n";
        out << "- Inventory status: `" << result.status << "`\n";
        out << "- Command exit code: `" << result.commandExitCode << "`\n";
        out << "- Raw stream-list lines parsed: `" << result.rawLineCount << "`\n\n";
        out << "## Top stream candidates\n\n";
        if (candidates.empty()) {
            out << "No stream candidates were parsed. If `aff4imager` is missing, provide it through `--reader-tools`, `VESTIGANT_READER_TOOLS`, `VESTIGANT_AFF4IMAGER`, or PATH. If it ran but returned no lines, review `aff4_stream_inventory_raw.txt`.\n\n";
        } else {
            out << "| Rank | Score | Type | APFS relevance | Raw line |\n";
            out << "|---:|---:|---|---|---|\n";
            int rank = 0;
            for (const auto& e : candidates) {
                if (++rank > 10) break;
                std::string raw = e.rawLine;
                std::replace(raw.begin(), raw.end(), '|', '/');
                if (raw.size() > 220) raw = raw.substr(0, 220) + "...";
                out << "| " << rank << " | " << e.candidateScore << " | " << e.candidateType << " | " << e.apfsRelevance << " | `" << raw << "` |\n";
            }
            out << "\n";
        }
        out << "## Next action\n\n";
        out << "1. Confirm which listed AFF4 stream represents the physical disk/APFS container.\n";
        out << "2. Export or expose only that selected stream to the APFS reader layer.\n";
        out << "3. Probe the exported/stream-backed image for APFS/HFS signatures and partition/container metadata.\n";
        out << "4. Populate image-file inventory from read-only APFS enumeration, then join that inventory to Spotlight artifacts for active-file comparison.\n\n";
        out << "This build records stream candidates only. It does not extract AFF4 streams or parse APFS/HFS filesystems.\n";
    }
    log.info("AFF4 stream inventory artifacts written: " + pathString(csvPath));
    return result;
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
