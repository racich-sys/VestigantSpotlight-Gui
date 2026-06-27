#include "parsers/aff4_probe_worker.h"

#include "codec/lzfse_codec.h"
#include "core/app_info.h"
#include "core/csv.h"
#include "core/hash.h"
#include "core/path_utils.h"
#include "db/sqlite_compat.h"
#include "parsers/apfs_aff4_reader.h"
#include "parsers/apfs_diagnostic_exporter.h"
#include "parsers/apfs_diagnostic_models.h"
#include "parsers/apfs_volume_reader.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <chrono>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

namespace fs = std::filesystem;

namespace vestigant::spotlight {
namespace {

#if defined(_WIN32)
std::wstring utf8ToWideProcessPathString(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) {
        n = MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (n <= 0) {
            std::wstring fallback;
            fallback.reserve(s.size());
            for (unsigned char c : s) fallback.push_back(c < 128 ? static_cast<wchar_t>(c) : L'?');
            return fallback;
        }
        std::wstring ws(static_cast<std::size_t>(n), L'\0');
        MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), ws.data(), n);
        return ws;
    }
    std::wstring ws(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), ws.data(), n);
    return ws;
}

std::wstring wideProcessPath(const fs::path& p) {
    std::string s = pathString(p);
    std::replace(s.begin(), s.end(), '/', '\\');
    return utf8ToWideProcessPathString(s);
}
#endif

std::string getenvString(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || !value) return {};
    std::string out(value);
    std::free(value);
    return out;
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
#endif
}

void appendPathCandidates(std::vector<fs::path>& out, const fs::path& dir, const std::vector<std::string>& names) {
    if (dir.empty()) return;
    for (const auto& n : names) out.push_back(dir / n);
}

void appendReaderToolRootCandidates(std::vector<fs::path>& out, const fs::path& root, const std::vector<std::string>& names) {
    if (root.empty()) return;
    appendPathCandidates(out, root, names);
    appendPathCandidates(out, root / "x64" / "Release", names);
    appendPathCandidates(out, root / "x64", names);
    appendPathCandidates(out, root / "Release", names);
    appendPathCandidates(out, root / "bin", names);
    appendPathCandidates(out, root / "lib", names);
    appendPathCandidates(out, root / "include", names);
    appendPathCandidates(out, root / "src", names);
}

std::vector<fs::path> splitSearchPath(const std::string& searchPath) {
    std::vector<fs::path> dirs;
    if (searchPath.empty()) return dirs;
#ifdef _WIN32
    const char sep = ';';
#else
    const char sep = ':';
#endif
    std::stringstream ss(searchPath);
    std::string item;
    while (std::getline(ss, item, sep)) {
        if (!item.empty()) dirs.emplace_back(item);
    }
    return dirs;
}

fs::path findToolCandidate(const RunOptions& opt, const std::string& envVar, const std::vector<std::string>& names) {
    std::vector<fs::path> candidates;
    const std::string explicitTool = getenvString(envVar.c_str());
    if (!explicitTool.empty()) candidates.emplace_back(explicitTool);
    appendReaderToolRootCandidates(candidates, opt.readerToolsDir, names);
    const std::string readerRoot = getenvString("VESTIGANT_READER_TOOLS");
    if (!readerRoot.empty()) appendReaderToolRootCandidates(candidates, fs::path(readerRoot), names);
    const std::string aff4CppLiteRoot = getenvString("VESTIGANT_AFF4_CPP_LITE_ROOT");
    if (!aff4CppLiteRoot.empty()) appendReaderToolRootCandidates(candidates, fs::path(aff4CppLiteRoot), names);
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec) {
        appendPathCandidates(candidates, cwd / "tools" / "readers" / "win64", names);
        appendPathCandidates(candidates, cwd / "tools" / "readers", names);
        appendPathCandidates(candidates, cwd / "tools", names);
    }
    for (const auto& dir : splitSearchPath(getenvString("PATH"))) appendPathCandidates(candidates, dir, names);
    for (const auto& c : candidates) {
        std::error_code existsEc;
        if (fs::exists(c, existsEc) && fs::is_regular_file(c, existsEc)) return c;
    }
    return {};
}

std::string lastWindowsErrorString() {
#ifdef _WIN32
    const DWORD code = GetLastError();
    if (code == 0) return {};
    LPSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD len = FormatMessageA(flags, nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
    std::string msg = "Win32 error " + std::to_string(static_cast<unsigned long>(code));
    if (len && buffer) {
        msg += ": ";
        msg += buffer;
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' ' || msg.back() == '\t')) msg.pop_back();
    }
    if (buffer) LocalFree(buffer);
    return msg;
#else
    return {};
#endif
}

class EvidenceBinaryWriter {
public:
    EvidenceBinaryWriter() = default;
    explicit EvidenceBinaryWriter(const fs::path& path) { open(path); }
    ~EvidenceBinaryWriter() { close(); }

    bool open(const fs::path& path) {
        close();
        path_ = path;
#if defined(_WIN32)
        const std::wstring wide = makeWin32LongPath(path);
        handle_ = CreateFileW(wide.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            error_ = "CreateFileW failed for " + pathString(path) + "; win32_error=" + std::to_string(GetLastError());
            ok_ = false;
            return false;
        }
        ok_ = true;
        return true;
#else
        stream_.open(path, std::ios::binary | std::ios::trunc);
        if (!stream_) {
            error_ = "open failed for " + pathString(path);
            ok_ = false;
            return false;
        }
        ok_ = true;
        return true;
#endif
    }

    bool write(const unsigned char* data, std::size_t size) {
        if (!ok_) return false;
#if defined(_WIN32)
        const unsigned char* cur = data;
        std::size_t left = size;
        while (left != 0) {
            const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(left, 1U << 20));
            DWORD written = 0;
            if (!WriteFile(handle_, cur, chunk, &written, nullptr) || written != chunk) {
                error_ = "WriteFile failed for " + pathString(path_) + "; win32_error=" + std::to_string(GetLastError());
                ok_ = false;
                return false;
            }
            cur += chunk;
            left -= chunk;
        }
        return true;
#else
        if (size != 0) stream_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!stream_) {
            error_ = "write failed for " + pathString(path_);
            ok_ = false;
            return false;
        }
        return true;
#endif
    }

    bool write(const std::vector<unsigned char>& data) { return write(data.data(), data.size()); }
    explicit operator bool() const { return ok_; }
    const std::string& error() const { return error_; }

    bool close() {
#if defined(_WIN32)
        if (handle_ != INVALID_HANDLE_VALUE) {
            const BOOL closed = CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            if (!closed) {
                error_ = "CloseHandle failed for " + pathString(path_) + "; win32_error=" + std::to_string(GetLastError());
                ok_ = false;
                return false;
            }
        }
#else
        if (stream_.is_open()) {
            stream_.close();
            if (!stream_) {
                error_ = "close failed for " + pathString(path_);
                ok_ = false;
                return false;
            }
        }
#endif
        return ok_;
    }

private:
    fs::path path_;
    bool ok_ = false;
    std::string error_;
#if defined(_WIN32)
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    std::ofstream stream_;
#endif
};

bool hasExtensionInsensitive(const fs::path& p, const std::string& ext) {
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return e == ext;
}
bool isZipSourcePath(const fs::path& p) { return hasExtensionInsensitive(p, ".zip"); }
bool isAff4SourcePath(const fs::path& p) { return hasExtensionInsensitive(p, ".aff4"); }
bool isRawImageSourcePath(const fs::path& p) {
    return hasExtensionInsensitive(p, ".img") || hasExtensionInsensitive(p, ".dd") || hasExtensionInsensitive(p, ".raw");
}
std::string inputSourceType(const fs::path& p) {
    if (isZipSourcePath(p)) return "ZIP_SPOTLIGHT_OR_FILESYSTEM_CONTAINER";
    if (isAff4SourcePath(p)) return "AFF4_CONTAINER";
    if (isRawImageSourcePath(p)) return "RAW_FLAT_IMAGE";
    std::error_code ec;
    if (fs::is_directory(p, ec)) return "FOLDER_OR_EXTRACTED_FILESYSTEM_ROOT";
    if (fs::is_regular_file(p, ec)) return "UNRECOGNIZED_FILE_SOURCE";
    return "UNKNOWN_OR_MISSING_SOURCE";
}

bool bytesAt(const std::vector<unsigned char>& data, std::size_t off, const char* needle, std::size_t n) {
    return needle && off <= data.size() && n <= data.size() - off && std::memcmp(data.data() + off, needle, n) == 0;
}
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) out += ' ';
            else out += c;
        }
    }
    return out;
}

std::string cleanStatusField(std::string s) {
    for (char& c : s) {
        if (c == '\t' || c == '\r' || c == '\n') c = ' ';
    }
    return s;
}

void appendRunProgress(const fs::path& caseDir, int percent, const std::string& stage, const std::string& detail = {}) {
    try {
        std::error_code ec;
        fs::create_directories(caseDir / "logs", ec);
        const std::string ts = nowUtc();
        const std::string cleanStage = cleanStatusField(stage);
        const std::string cleanDetail = cleanStatusField(detail);
        auto writeOne = [&](const fs::path& path, std::ios::openmode mode) {
            std::ofstream out(path, mode | std::ios::binary);
            out << ts << "\t" << percent << "\t" << cleanStage << "\t" << cleanDetail << "\n" << std::flush;
        };
        writeOne(caseDir / "logs" / "run_progress.tsv", std::ios::app);
        writeOne(caseDir / "logs" / "last_progress.tsv", std::ios::trunc);
        writeOne(caseDir / "run_progress.tsv", std::ios::app);
        writeOne(caseDir / "last_progress.tsv", std::ios::trunc);
    } catch (...) {}
}

void appendRunStatus(const fs::path& caseDir, const std::string& stage, const std::string& detail = {}) {
    try {
        std::error_code ec;
        fs::create_directories(caseDir / "logs", ec);
        std::ofstream out(caseDir / "run_status.txt", std::ios::app | std::ios::binary);
        out << nowUtc() << " " << stage;
        if (!detail.empty()) out << " " << detail;
        out << "\n" << std::flush;
        std::ofstream last(caseDir / "last_stage.txt", std::ios::binary);
        last << stage << "\n" << std::flush;
    } catch (...) {}
}


std::uint16_t readLe16(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 2 > data.size()) return 0;
    return static_cast<std::uint16_t>(data[off]) | (static_cast<std::uint16_t>(data[off + 1]) << 8);
}

std::uint32_t readLe32(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 4 > data.size()) return 0;
    return static_cast<std::uint32_t>(data[off]) |
           (static_cast<std::uint32_t>(data[off + 1]) << 8) |
           (static_cast<std::uint32_t>(data[off + 2]) << 16) |
           (static_cast<std::uint32_t>(data[off + 3]) << 24);
}

std::uint64_t readLe64(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 8 > data.size()) return 0;
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | static_cast<std::uint64_t>(data[off + static_cast<std::size_t>(i)]);
    }
    return v;
}

struct ApfsInodeExtendedFieldDecode {
    bool sawXfields = false;
    bool sawDstream = false;
    std::uint64_t dstreamSize = 0;
    std::uint64_t dstreamAllocedSize = 0;
    std::uint64_t dstreamDefaultCryptoId = 0;
    std::string status;
    std::string notes;
};

std::size_t alignUp8ForApfsXfield(std::size_t value) {
    return (value + 7U) & ~static_cast<std::size_t>(7U);
}

ApfsInodeExtendedFieldDecode decodeApfsInodeExtendedFieldsForProbe(const std::vector<unsigned char>& node,
                                                                    std::size_t valueAbs,
                                                                    std::size_t valueLen) {
    ApfsInodeExtendedFieldDecode d;
    constexpr std::size_t kJInodeValFixedSize = 92U;
    constexpr std::uint8_t kInoExtTypeDstream = 8U;
    if (valueLen <= kJInodeValFixedSize) {
        d.status = "NO_INODE_XFIELDS";
        return d;
    }
    if (valueAbs > node.size() || node.size() - valueAbs < valueLen) {
        d.status = "INODE_VALUE_BOUNDS_INVALID";
        return d;
    }
    const std::size_t xbase = valueAbs + kJInodeValFixedSize;
    const std::size_t xlen = valueLen - kJInodeValFixedSize;
    if (xlen < 4U || xbase > node.size() || node.size() - xbase < xlen) {
        d.status = "INODE_XFIELDS_TOO_SHORT";
        return d;
    }
    d.sawXfields = true;
    const std::uint16_t numExts = readLe16(node, xbase + 0U);
    const std::uint16_t usedData = readLe16(node, xbase + 2U);
    const std::size_t tableBytes = static_cast<std::size_t>(numExts) * 4U;
    if (numExts == 0) {
        d.status = "INODE_XFIELDS_EMPTY";
        return d;
    }
    if (4U + tableBytes > xlen) {
        d.status = "INODE_XFIELD_TABLE_OUT_OF_BOUNDS";
        d.notes = "num_exts=" + std::to_string(numExts) + "; used_data=" + std::to_string(usedData);
        return d;
    }
    if (usedData != 0 && static_cast<std::size_t>(usedData) > xlen) {
        d.status = "INODE_XFIELD_USED_DATA_OUT_OF_BOUNDS";
        d.notes = "num_exts=" + std::to_string(numExts) + "; used_data=" + std::to_string(usedData);
        return d;
    }

    struct XfEntryForProbe {
        std::uint8_t type = 0;
        std::uint8_t flags = 0;
        std::uint16_t size = 0;
    };
    std::vector<XfEntryForProbe> fields;
    fields.reserve(numExts);
    for (std::uint16_t i = 0; i < numExts; ++i) {
        const std::size_t fieldAbs = xbase + 4U + static_cast<std::size_t>(i) * 4U;
        if (fieldAbs + 4U > node.size()) {
            d.status = "INODE_XFIELD_ENTRY_OUT_OF_BOUNDS";
            return d;
        }
        XfEntryForProbe xf;
        xf.type = node[fieldAbs + 0U];
        xf.flags = node[fieldAbs + 1U];
        xf.size = readLe16(node, fieldAbs + 2U);
        fields.push_back(xf);
    }

    // V0_8_62: APFS extended-field payload alignment in the wild can be easy to
    // mis-handle because xf_blob_t has a 4-byte header followed by xf_data[],
    // and the x_field_t metadata and payload arrays are aligned to eight-byte
    // boundaries. Try several bounded layout interpretations and accept the
    // first plausible INO_EXT_TYPE_DSTREAM. This is a read-only forensic probe:
    // failed candidates are recorded as diagnostics, not fatal parser errors.
    struct LayoutCandidate {
        const char* name;
        std::size_t startRel;
        bool alignRelativeToXfData;
        bool alignEachField;
    };
    const std::vector<LayoutCandidate> layouts = {
        {"xfdata_table_aligned", 4U + alignUp8ForApfsXfield(tableBytes), true, true},
        {"blob_table_aligned", alignUp8ForApfsXfield(4U + tableBytes), false, true},
        {"xfdata_table_raw", 4U + tableBytes, true, true},
        {"blob_table_raw", 4U + tableBytes, false, true},
        {"xfdata_table_aligned_no_each", 4U + alignUp8ForApfsXfield(tableBytes), true, false},
        {"blob_table_aligned_no_each", alignUp8ForApfsXfield(4U + tableBytes), false, false}
    };

    auto alignRel = [](std::size_t rel, bool relativeToXfData) -> std::size_t {
        if (!relativeToXfData) return alignUp8ForApfsXfield(rel);
        if (rel < 4U) return 4U;
        return 4U + alignUp8ForApfsXfield(rel - 4U);
    };

    std::vector<std::string> failedLayouts;
    for (const auto& layout : layouts) {
        std::size_t dataRel = layout.startRel;
        bool layoutBoundsOk = true;
        std::string layoutFailure;
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (layout.alignEachField) dataRel = alignRel(dataRel, layout.alignRelativeToXfData);
            const auto& field = fields[i];
            if (dataRel > xlen || static_cast<std::size_t>(field.size) > xlen - dataRel) {
                layoutBoundsOk = false;
                layoutFailure = std::string(layout.name) + ":field_index=" + std::to_string(i) + ";type=" + std::to_string(field.type) + ";size=" + std::to_string(field.size) + ";data_rel=" + std::to_string(dataRel);
                break;
            }
            const std::size_t dataAbs = xbase + dataRel;
            if (field.type == kInoExtTypeDstream && field.size >= 40U && dataAbs + 40U <= node.size()) {
                const std::uint64_t candidateSize = readLe64(node, dataAbs + 0U);
                const std::uint64_t candidateAlloced = readLe64(node, dataAbs + 8U);
                const std::uint64_t candidateCrypto = readLe64(node, dataAbs + 16U);
                // A zero-size dstream is valid for empty files, but for trimming
                // copy-out we only persist nonzero logical sizes that fit in the
                // decoded allocated-size. Keep the row status regardless.
                if (candidateAlloced == 0 || candidateSize <= candidateAlloced || candidateSize < (1ULL << 62)) {
                    d.sawDstream = true;
                    d.dstreamSize = candidateSize;
                    d.dstreamAllocedSize = candidateAlloced;
                    d.dstreamDefaultCryptoId = candidateCrypto;
                    d.status = "INODE_XFIELDS_DSTREAM_DECODED";
                    d.notes = "num_exts=" + std::to_string(numExts) + "; used_data=" + std::to_string(usedData) + "; layout=" + layout.name;
                    return d;
                }
            }
            dataRel += static_cast<std::size_t>(field.size);
        }
        if (!layoutBoundsOk) failedLayouts.push_back(layoutFailure);
    }

    d.status = "INODE_XFIELDS_NO_DSTREAM";
    d.notes = "num_exts=" + std::to_string(numExts) + "; used_data=" + std::to_string(usedData);
    if (!failedLayouts.empty()) {
        d.notes += "; layout_failures=";
        for (std::size_t i = 0; i < failedLayouts.size() && i < 3U; ++i) {
            if (i) d.notes += " | ";
            d.notes += failedLayouts[i];
        }
    }
    return d;
}

std::string hexByte(unsigned int v) {
    const char* d = "0123456789ABCDEF";
    std::string s;
    s.push_back(d[(v >> 4) & 0xF]);
    s.push_back(d[v & 0xF]);
    return s;
}

std::string guidFromGptBytes(const unsigned char* b) {
    if (!b) return {};
    auto hx = [](unsigned char c) { return hexByte(static_cast<unsigned int>(c)); };
    std::string out;
    out += hx(b[3]); out += hx(b[2]); out += hx(b[1]); out += hx(b[0]); out += '-';
    out += hx(b[5]); out += hx(b[4]); out += '-';
    out += hx(b[7]); out += hx(b[6]); out += '-';
    out += hx(b[8]); out += hx(b[9]); out += '-';
    for (int i = 10; i < 16; ++i) out += hx(b[i]);
    return out;
}

bool allZeroBytes(const unsigned char* b, std::size_t n) {
    if (!b) return true;
    for (std::size_t i = 0; i < n; ++i) if (b[i] != 0) return false;
    return true;
}

std::string utf16LeNameToAscii(const std::vector<unsigned char>& data, std::size_t off, std::size_t maxBytes) {
    std::string out;
    const std::size_t end = std::min<std::size_t>(data.size(), off + maxBytes);
    for (std::size_t p = off; p + 1 < end; p += 2) {
        const std::uint16_t ch = readLe16(data, p);
        if (ch == 0) break;
        if (ch >= 32 && ch <= 126) out.push_back(static_cast<char>(ch));
        else if (ch == '\t') out.push_back(' ');
        else out.push_back('?');
        if (out.size() >= 128) break;
    }
    return out;
}


std::string hexSampleBytes(const unsigned char* data, std::size_t n) {
    if (!data || n == 0) return {};
    std::string out;
    const std::size_t limit = std::min<std::size_t>(n, 32);
    for (std::size_t i = 0; i < limit; ++i) {
        if (i) out += ' ';
        out += hexByte(static_cast<unsigned int>(data[i]));
    }
    return out;
}

std::string directPreviewStatusForBytes(const std::vector<unsigned char>& bytes) {
    if (bytes.size() >= 16U && std::memcmp(bytes.data(), "SQLite format 3", 15U) == 0) return "SQLITE_HEADER";
    if (bytes.size() >= 6U && std::memcmp(bytes.data(), "bplist", 6U) == 0) return "BPLIST_HEADER";
    bool anyNonZero = false;
    for (unsigned char b : bytes) {
        if (b != 0U) { anyNonZero = true; break; }
    }
    return anyNonZero ? "NONZERO_BYTES" : "ALL_ZERO_BYTES";
}

struct Aff4DynamicProbeRow {
    std::string step;
    std::string status;
    fs::path path;
    std::uint64_t objectSize = 0;
    std::uint64_t offset = 0;
    long long bytesRead = -1;
    std::string sampleHex;
    std::string notes;
};

std::string bytesToUuidString(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 16 > data.size()) return {};
    auto hx = [](unsigned char b) -> std::string {
        return std::string(1, "0123456789abcdef"[(b >> 4) & 0xF]) + std::string(1, "0123456789abcdef"[b & 0xF]);
    };
    std::string out;
    out += hx(data[off + 0]); out += hx(data[off + 1]); out += hx(data[off + 2]); out += hx(data[off + 3]); out += '-';
    out += hx(data[off + 4]); out += hx(data[off + 5]); out += '-';
    out += hx(data[off + 6]); out += hx(data[off + 7]); out += '-';
    out += hx(data[off + 8]); out += hx(data[off + 9]); out += '-';
    for (std::size_t i = 10; i < 16; ++i) out += hx(data[off + i]);
    return out;
}

std::string joinU64List(const std::vector<std::uint64_t>& values, std::size_t maxCount = 32) {
    std::string out;
    const std::size_t n = std::min<std::size_t>(values.size(), maxCount);
    for (std::size_t i = 0; i < n; ++i) {
        if (i) out += ";";
        out += std::to_string(values[i]);
    }
    if (values.size() > n) out += ";...";
    return out;
}

std::string apfsObjectTypeLabel(std::uint32_t rawType) {
    const std::uint32_t base = rawType & 0x0000ffffU;
    switch (base) {
        case 0x0001: return "NX_SUPERBLOCK";
        case 0x0002: return "BTREE_NODE";
        case 0x0003: return "BTREE";
        case 0x0005: return "SPACEMAN";
        case 0x0006: return "SPACEMAN_CAB";
        case 0x0007: return "SPACEMAN_CIB";
        case 0x000b: return "OBJECT_MAP";
        case 0x000c: return "CHECKPOINT_MAP";
        case 0x000d: return "VOLUME_SUPERBLOCK";
        case 0x0011: return "REAPER";
        default: return std::string("OBJECT_TYPE_0x") + hexByte((rawType >> 8) & 0xffU) + hexByte(rawType & 0xffU);
    }
}








std::string readFixedUtf8Z(const std::vector<unsigned char>& data, std::size_t off, std::size_t maxLen) {
    if (off >= data.size() || maxLen == 0) return {};
    const std::size_t end = std::min<std::size_t>(data.size(), off + maxLen);
    std::string out;
    out.reserve(end - off);
    for (std::size_t i = off; i < end; ++i) {
        const unsigned char c = data[i];
        if (c == 0) break;
        if (c == '\r' || c == '\n' || c == '\t') out.push_back(' ');
        else out.push_back(static_cast<char>(c));
    }
    while (!out.empty() && static_cast<unsigned char>(out.back()) <= 0x20U) out.pop_back();
    return out;
}

















struct ApfsDirectoryRecordEntry {
    std::uint32_t volumeSequence = 0;
    std::string targetRole;
    std::uint64_t fsOid = 0;
    std::string volumeName;
    std::uint64_t parentObjectId = 0;
    std::uint64_t childFileId = 0;
    std::string name;
};


void writeAff4ApfsDirectoryRecordNameIndexOutputs(const fs::path& caseDir,
                                                  const EvidenceSource& source,
                                                  const fs::path& originalInput,
                                                  const std::vector<ApfsDirectoryRecordEntry>& rows,
                                                  Logger& log) {
    const fs::path fullCsv = caseDir / "aff4_apfs_directory_record_name_index.csv";
    const fs::path sampleCsv = caseDir / "aff4_apfs_directory_record_name_index_sample.csv";
    const fs::path summaryJson = caseDir / "aff4_apfs_directory_record_name_index_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_DIRECTORY_RECORD_NAME_INDEX.md";
    constexpr std::size_t kSampleLimit = 5000U;
    try {
        std::set<std::uint64_t> uniqueChildren;
        std::set<std::uint64_t> uniqueParents;
        const std::string header = "source_id,input_path,input_type,sequence,volume_sequence,target_role,fs_oid,volume_name,parent_object_id_candidate,child_file_id_candidate,decoded_name,status,interpretation,notes\n";
        std::ofstream full(fullCsv, std::ios::binary);
        std::ofstream sample(sampleCsv, std::ios::binary);
        full << header;
        sample << header;
        std::size_t seq = 0;
        std::size_t sampleRows = 0;
        for (const auto& r : rows) {
            if (r.parentObjectId != 0) uniqueParents.insert(r.parentObjectId);
            if (r.childFileId != 0) uniqueChildren.insert(r.childFileId);
            const std::string line =
                csvEscape(source.sourceId) + "," +
                csvEscape(pathString(originalInput)) + "," +
                csvEscape(inputSourceType(originalInput)) + "," +
                std::to_string(seq) + "," +
                std::to_string(r.volumeSequence) + "," +
                csvEscape(r.targetRole) + "," +
                std::to_string(r.fsOid) + "," +
                csvEscape(r.volumeName) + "," +
                std::to_string(r.parentObjectId) + "," +
                std::to_string(r.childFileId) + "," +
                csvEscape(r.name) + "," +
                csvEscape("DIRECT_AFF4_APFS_DIRECTORY_RECORD_INDEX_ROW") + "," +
                csvEscape("Directory record decoded during exhausted direct AFF4/APFS root-tree traversal.") + "," +
                csvEscape("local_full_index_for_unresolved_spotlight_object_resolution") + "\n";
            full << line;
            if (sampleRows < kSampleLimit) {
                sample << line;
                ++sampleRows;
            }
            ++seq;
        }
        {
            std::ofstream out(summaryJson, std::ios::binary);
            out << "{\n";
            out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
            out << "  \"app_version\": \"" << appVersion() << "\",\n";
            out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
            out << "  \"directory_record_rows\": " << rows.size() << ",\n";
            out << "  \"unique_child_file_ids\": " << uniqueChildren.size() << ",\n";
            out << "  \"unique_parent_object_ids\": " << uniqueParents.size() << ",\n";
            out << "  \"sample_rows\": " << sampleRows << ",\n";
            out << "  \"full_index_csv\": \"aff4_apfs_directory_record_name_index.csv\",\n";
            out << "  \"sample_csv\": \"aff4_apfs_directory_record_name_index_sample.csv\",\n";
            out << "  \"notes\": \"Full local APFS directory-record name index written for unresolved Spotlight object resolution. The full CSV is intentionally not part of the standard chat upload; the sample and summary are upload-safe.\"\n";
            out << "}\n";
        }
        {
            std::ofstream out(mdPath, std::ios::binary);
            out << "# AFF4 APFS Directory Record Name Index\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "This local index contains APFS directory-record parent/child/name rows decoded during the direct AFF4/APFS root-tree traversal. V1.6.84 uses it to resolve `UNRESOLVED_SPOTLIGHT_OBJECT_INODE_*` labels without relying on the bounded `aff4_apfs_spotlight_name_scan_sample.csv` diagnostic sample.\n\n";
            out << "## Summary\n\n";
            out << "- Directory-record rows: `" << rows.size() << "`\n";
            out << "- Unique child file IDs: `" << uniqueChildren.size() << "`\n";
            out << "- Unique parent object IDs: `" << uniqueParents.size() << "`\n";
            out << "- Upload sample rows: `" << sampleRows << "`\n\n";
            out << "## Files\n\n";
            out << "- `aff4_apfs_directory_record_name_index.csv` is the full local resolver input and can be large.\n";
            out << "- `aff4_apfs_directory_record_name_index_sample.csv` is the upload-safe sample.\n";
            out << "- `aff4_apfs_directory_record_name_index_summary.json` records row counts and provenance.\n";
        }
        log.info("AFF4 APFS directory-record name index written: " + pathString(fullCsv));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4 APFS directory-record name index outputs: ") + ex.what());
    }
}










struct ApfsOmapTargetResolution {
    std::uint64_t targetOid = 0;
    std::uint64_t targetXid = 0;
    std::uint32_t branchDepth = 0;
    std::string branchPath;
    std::uint64_t leafOid = 0;
    std::uint64_t leafVirtualOffset = 0;
    long long leafBytesRead = -1;
    std::uint16_t leafBtnFlags = 0;
    std::uint16_t leafBtnLevel = 0;
    std::uint32_t leafBtnNkeys = 0;
    std::uint32_t matchedEntryIndex = 0;
    std::uint64_t matchedKeyOid = 0;
    std::uint64_t matchedKeyXid = 0;
    std::uint32_t valueFlags = 0;
    std::uint32_t valueSize = 0;
    std::uint64_t valuePaddr = 0;
    std::uint64_t resolvedVirtualOffset = 0;
    long long resolvedBytesRead = -1;
    std::uint64_t resolvedObjectOid = 0;
    std::uint64_t resolvedObjectXid = 0;
    std::uint32_t resolvedObjectTypeRaw = 0;
    std::string resolvedObjectTypeLabel;
    std::uint32_t resolvedObjectSubtype = 0;
    std::string resolvedMagic;
    std::uint16_t resolvedBtnFlags = 0;
    std::uint16_t resolvedBtnLevel = 0;
    std::uint32_t resolvedBtnNkeys = 0;
    std::string lookupStatus;
    std::string objectStatus;
    std::string interpretation;
    std::string sampleHex;
    std::string resolvedSampleHex;
    std::string notes;
    std::vector<unsigned char> resolvedBuffer;
};

bool aff4ApfsFixedKvAbsForProbe(const std::vector<unsigned char>& node,
                                std::uint32_t entryIndex,
                                std::size_t valueLenNeeded,
                                std::size_t& keyAbs,
                                std::size_t& valAbs,
                                std::string& detail) {
    return apfsAff4DecodeFixedKvAbs(node, entryIndex, valueLenNeeded, keyAbs, valAbs, detail);
}


int aff4ApfsCompareOmapKeyForProbe(std::uint64_t oid, std::uint64_t xid, std::uint64_t targetOid, std::uint64_t targetXid) {
    if (oid < targetOid) return -1;
    if (oid > targetOid) return 1;
    if (xid < targetXid) return -1;
    if (xid > targetXid) return 1;
    return 0;
}

void aff4ApfsAppendBranchPathForProbe(std::string& path, const std::string& segment) {
    if (!path.empty()) path += " -> ";
    path += segment;
}

void aff4ApfsAppendProbeNote(std::string& notes, const std::string& note) {
    if (note.empty()) return;
    if (!notes.empty()) notes += "; ";
    notes += note;
}

bool aff4ApfsFindBestOmapLeafEntryForProbe(const std::vector<unsigned char>& node,
                                           std::uint32_t nkeys,
                                           std::uint64_t targetOid,
                                           std::uint64_t targetXid,
                                           std::uint32_t& matchedEntryIndex,
                                           std::uint64_t& matchedKeyOid,
                                           std::uint64_t& matchedKeyXid,
                                           std::uint32_t& valueFlags,
                                           std::uint32_t& valueSize,
                                           std::uint64_t& valuePaddr,
                                           std::string& bestDetail,
                                           std::string& firstDecodeNote,
                                           const std::function<bool()>& cancelCheck,
                                           bool& cancelled) {
    cancelled = false;
    bool found = false;
    std::uint64_t bestXid = 0;
    const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
    for (std::uint32_t i = 0; i < limit; ++i) {
        if (cancelCheck && cancelCheck()) {
            cancelled = true;
            return false;
        }
        std::size_t keyAbs = 0;
        std::size_t valAbs = 0;
        std::string detail;
        if (!aff4ApfsFixedKvAbsForProbe(node, i, 16U, keyAbs, valAbs, detail)) {
            if (i == 0 && firstDecodeNote.empty()) firstDecodeNote = detail;
            continue;
        }
        const std::uint64_t keyOid = readLe64(node, keyAbs + 0U);
        const std::uint64_t keyXid = readLe64(node, keyAbs + 8U);
        if (keyOid != targetOid || keyXid > targetXid) continue;
        if (!found || keyXid >= bestXid) {
            found = true;
            bestXid = keyXid;
            bestDetail = detail;
            matchedEntryIndex = i;
            matchedKeyOid = keyOid;
            matchedKeyXid = keyXid;
            valueFlags = readLe32(node, valAbs + 0U);
            valueSize = readLe32(node, valAbs + 4U);
            valuePaddr = readLe64(node, valAbs + 8U);
        }
    }
    return found;
}

// Centralized APFS horizontal leaf loader used by direct-map and dynamic/libaff4 probe paths.
// Keep next-leaf footer parsing here rather than duplicating btn_flags/footer math at call sites.
bool aff4ApfsLoadNextLeafForProbe(const std::vector<unsigned char>& currentNode,
                                  std::uint64_t currentNodeOid,
                                  std::uint64_t blockSize,
                                  const std::function<long long(std::uint64_t, std::uint64_t, std::vector<unsigned char>&, std::string&)>& readVirtual,
                                  const std::function<bool(std::uint64_t, std::uint64_t&)>& safeNodeOffset,
                                  std::vector<unsigned char>& nextNode,
                                  std::uint64_t& nextNodeOid,
                                  std::uint64_t& nextNodeOffset,
                                  long long& nextNodeRead,
                                  std::string& notes) {
    const std::uint64_t candidateNextOid = apfsReadNextLeafOidFromBtreeInfoFooter(currentNode);
    if (candidateNextOid == 0 || candidateNextOid == currentNodeOid) return false;
    std::uint64_t candidateNextOffset = 0;
    if (!safeNodeOffset(candidateNextOid, candidateNextOffset)) {
        aff4ApfsAppendProbeNote(notes, "next_leaf_oid_offset_unsafe=" + std::to_string(candidateNextOid));
        return false;
    }
    nextNode.clear();
    std::string nextErr;
    const long long candidateRead = readVirtual(candidateNextOffset, blockSize, nextNode, nextErr);
    if (candidateRead <= 0 || nextNode.size() < 64U) {
        aff4ApfsAppendProbeNote(notes, nextErr.empty() ? ("next_leaf_read_failed_oid=" + std::to_string(candidateNextOid))
                                                       : ("next_leaf_read_failed_oid=" + std::to_string(candidateNextOid) + ": " + nextErr));
        return false;
    }
    const std::string label = apfsObjectTypeLabel(readLe32(nextNode, 24U));
    const std::uint16_t nextLevel = readLe16(nextNode, 34U);
    if ((label != "BTREE" && label != "BTREE_NODE") || nextLevel != 0U) {
        aff4ApfsAppendProbeNote(notes, "next_leaf_unexpected_node_type_or_level=" + label + ":" + std::to_string(nextLevel));
        nextNode.clear();
        return false;
    }
    nextNodeOid = candidateNextOid;
    nextNodeOffset = candidateNextOffset;
    nextNodeRead = candidateRead;
    return true;
}

bool aff4GenericBtreeKvAbsForProbe(const std::vector<unsigned char>& node,
                                   std::uint32_t entryIndex,
                                   std::size_t& tocAbs,
                                   std::size_t& keyAbs,
                                   std::size_t& keyLen,
                                   std::size_t& valAbs,
                                   std::size_t& valLen,
                                   std::string& detail) {
    return apfsAff4DecodeGenericBtreeKvAbs(node, entryIndex, tocAbs, keyAbs, keyLen, valAbs, valLen, detail);
}


ApfsOmapTargetResolution aff4ResolveVolumeOmapTargetObjectForProbe(
    const ApfsVolumeOmapProbeRow& omRow,
    const std::vector<unsigned char>& rootNode,
    std::uint64_t targetOid,
    std::uint64_t targetXid,
    std::uint32_t blockSize,
    const std::function<long long(std::uint64_t, std::uint64_t, std::vector<unsigned char>&, std::string&)>& readVirtual,
    const std::function<bool(std::uint64_t, std::uint64_t&)>& safeNodeOffset,
    const std::string& purpose,
    const std::function<bool()>& cancelCheck = {}) {
    ApfsOmapTargetResolution out;
    out.targetOid = targetOid;
    out.targetXid = targetXid;
    if (targetOid == 0) {
        out.lookupStatus = "OMAP_TARGET_OID_ZERO";
        out.interpretation = purpose + ": target OID was zero.";
        return out;
    }
    if (rootNode.empty() || omRow.treeStatus != "VOLUME_OMAP_BTREE_ROOT_READ") {
        out.lookupStatus = "VOLUME_OMAP_TREE_UNAVAILABLE";
        out.interpretation = purpose + ": volume OMAP B-tree root was not available.";
        out.notes = "volume_omap_tree_status=" + omRow.treeStatus;
        return out;
    }

    std::vector<unsigned char> node = rootNode;
    std::uint64_t nodeOid = omRow.omTreeOid;
    std::uint64_t nodeOffset = omRow.treeVirtualOffset;
    long long nodeRead = omRow.treeBytesRead;
    std::vector<unsigned char> nextLeafNodeBuffer;
    nextLeafNodeBuffer.reserve(static_cast<std::size_t>(blockSize));
    std::vector<unsigned char> reusableChildNodeBuffer;
    reusableChildNodeBuffer.reserve(static_cast<std::size_t>(blockSize));
    std::set<std::uint64_t> visitedNodes;
    constexpr std::uint32_t kMaxDepth = 8;
    for (std::uint32_t depth = 0; depth < kMaxDepth; ++depth) {
        if (!visitedNodes.insert(nodeOid).second) {
            out.lookupStatus = "VOLUME_OMAP_VERTICAL_CYCLE_DETECTED";
            out.interpretation = purpose + ": volume OMAP B-tree traversal detected a repeated branch/leaf node and stopped.";
            aff4ApfsAppendProbeNote(out.notes, "vertical_cycle_detected_oid=" + std::to_string(nodeOid));
            break;
        }
        if (cancelCheck && cancelCheck()) {
            out.lookupStatus = "CANCELLED_BY_USER";
            out.interpretation = purpose + ": OMAP traversal cancelled by investigator request.";
            out.notes = "Cancelled while traversing APFS OMAP B-tree.";
            return out;
        }
        out.branchDepth = depth;
        if (node.size() < 64) {
            out.lookupStatus = "VOLUME_OMAP_NODE_TOO_SMALL";
            out.interpretation = purpose + ": OMAP node read returned too few bytes for B-tree header parsing.";
            break;
        }
        const std::uint32_t rawType = readLe32(node, 24);
        const std::string label = apfsObjectTypeLabel(rawType);
        const std::uint16_t btnFlags = readLe16(node, 32);
        const std::uint16_t btnLevel = readLe16(node, 34);
        const std::uint32_t nkeys = readLe32(node, 36);
        aff4ApfsAppendBranchPathForProbe(out.branchPath, "oid=" + std::to_string(nodeOid) + ";flags=" + std::to_string(btnFlags) + ";level=" + std::to_string(btnLevel) + ";nkeys=" + std::to_string(nkeys));
        if (label != "BTREE" && label != "BTREE_NODE") {
            out.lookupStatus = "VOLUME_OMAP_NODE_UNEXPECTED_OBJECT_TYPE";
            out.interpretation = purpose + ": node in the volume OMAP path was not an APFS B-tree node.";
            out.notes = "object_type=" + label;
            break;
        }
        if (nkeys > 100000U) {
            out.lookupStatus = "VOLUME_OMAP_NODE_SUSPICIOUS_KEY_COUNT";
            out.interpretation = purpose + ": OMAP node reported an implausibly large key count; traversal stopped.";
            out.notes = "nkeys=" + std::to_string(nkeys);
            break;
        }
        if (btnLevel == 0) {
            bool found = false;
            std::string bestDetail;
            std::set<std::uint64_t> visitedLeaves;
            std::uint32_t nextLeafTransitions = 0;
            constexpr std::uint32_t kMaxNextLeafTransitions = 256U;
            while (true) {
                if (!visitedLeaves.insert(nodeOid).second) {
                    aff4ApfsAppendProbeNote(out.notes, "next_leaf_cycle_detected_oid=" + std::to_string(nodeOid));
                    break;
                }
                out.leafOid = nodeOid;
                out.leafVirtualOffset = nodeOffset;
                out.leafBytesRead = nodeRead;
                out.leafBtnFlags = readLe16(node, 32U);
                out.leafBtnLevel = readLe16(node, 34U);
                out.leafBtnNkeys = readLe32(node, 36U);
                out.sampleHex = node.empty() ? std::string{} : hexSampleBytes(node.data(), node.size() < 96 ? node.size() : 96);
                std::string firstDecodeNote;
                bool cancelled = false;
                found = aff4ApfsFindBestOmapLeafEntryForProbe(node,
                                                              out.leafBtnNkeys,
                                                              targetOid,
                                                              targetXid,
                                                              out.matchedEntryIndex,
                                                              out.matchedKeyOid,
                                                              out.matchedKeyXid,
                                                              out.valueFlags,
                                                              out.valueSize,
                                                              out.valuePaddr,
                                                              bestDetail,
                                                              firstDecodeNote,
                                                              cancelCheck,
                                                              cancelled);
                if (cancelled) {
                    out.lookupStatus = "CANCELLED_BY_USER";
                    out.interpretation = purpose + ": volume OMAP lookup cancelled by investigator request.";
                    out.notes = "Cancelled while scanning volume OMAP leaf entries.";
                    return out;
                }
                if (!firstDecodeNote.empty() && out.notes.empty()) out.notes = firstDecodeNote;
                if (found) break;
                if (nextLeafTransitions >= kMaxNextLeafTransitions) {
                    aff4ApfsAppendProbeNote(out.notes, "next_leaf_transition_limit_reached=" + std::to_string(kMaxNextLeafTransitions));
                    break;
                }
                nextLeafNodeBuffer.clear();
                std::uint64_t nextNodeOid = 0;
                std::uint64_t nextNodeOffset = 0;
                long long nextNodeRead = -1;
                if (!aff4ApfsLoadNextLeafForProbe(node, nodeOid, blockSize, readVirtual, safeNodeOffset, nextLeafNodeBuffer, nextNodeOid, nextNodeOffset, nextNodeRead, out.notes)) break;
                ++nextLeafTransitions;
                aff4ApfsAppendBranchPathForProbe(out.branchPath, "next_leaf_oid=" + std::to_string(nextNodeOid) + ";transition=" + std::to_string(nextLeafTransitions));
                node.swap(nextLeafNodeBuffer);
                nodeOid = nextNodeOid;
                nodeOffset = nextNodeOffset;
                nodeRead = nextNodeRead;
            }
            if (!found) {
                out.lookupStatus = nextLeafTransitions > 0 ? "OMAP_TARGET_LOOKUP_NO_MATCH_AFTER_NEXT_LEAF_SCAN" : "OMAP_TARGET_LOOKUP_NO_MATCH_IN_LEAF";
                out.interpretation = purpose + ": volume OMAP leaf scan reached the end of the bounded horizontal leaf chain without finding a key with xid <= target transaction.";
                break;
            }
            out.lookupStatus = nextLeafTransitions > 0 ? "OMAP_TARGET_LOOKUP_RESOLVED_AFTER_NEXT_LEAF_SCAN" : "OMAP_TARGET_LOOKUP_RESOLVED";
            aff4ApfsAppendProbeNote(out.notes, bestDetail);
            if (nextLeafTransitions > 0) aff4ApfsAppendProbeNote(out.notes, "next_leaf_transitions=" + std::to_string(nextLeafTransitions));
            if ((out.valueFlags & 0x00000001U) != 0U) {
                out.objectStatus = "OMAP_TARGET_VALUE_DELETED";
                out.interpretation = purpose + ": matching OMAP value was marked deleted; object not read as active.";
                break;
            }
            if (!safeNodeOffset(out.valuePaddr, out.resolvedVirtualOffset)) {
                out.objectStatus = "OMAP_TARGET_RESOLVED_OFFSET_UNSAFE";
                out.interpretation = purpose + ": matching OMAP value was found, but value_paddr could not be converted to a safe read offset.";
                break;
            }
            std::vector<unsigned char> resolved;
            std::string resolvedErr;
            out.resolvedBytesRead = readVirtual(out.resolvedVirtualOffset, blockSize, resolved, resolvedErr);
            if (out.resolvedBytesRead > 0 && resolved.size() >= 56) {
                out.resolvedObjectOid = readLe64(resolved, 8);
                out.resolvedObjectXid = readLe64(resolved, 16);
                out.resolvedObjectTypeRaw = readLe32(resolved, 24);
                out.resolvedObjectTypeLabel = apfsObjectTypeLabel(out.resolvedObjectTypeRaw);
                out.resolvedObjectSubtype = readLe32(resolved, 28);
                if (resolved.size() >= 36) out.resolvedMagic.assign(reinterpret_cast<const char*>(resolved.data() + 32), reinterpret_cast<const char*>(resolved.data() + 36));
                if (out.resolvedObjectTypeLabel == "BTREE" || out.resolvedObjectTypeLabel == "BTREE_NODE") {
                    out.resolvedBtnFlags = readLe16(resolved, 32);
                    out.resolvedBtnLevel = readLe16(resolved, 34);
                    out.resolvedBtnNkeys = readLe32(resolved, 36);
                    out.objectStatus = "OMAP_TARGET_BTREE_READ";
                    out.interpretation = purpose + ": target object resolved through the volume OMAP and read as a B-tree node.";
                } else {
                    out.objectStatus = "OMAP_TARGET_UNEXPECTED_OBJECT_TYPE";
                    out.interpretation = purpose + ": target object resolved through the volume OMAP, but was not a B-tree node.";
                }
                out.resolvedSampleHex = hexSampleBytes(resolved.data(), resolved.size() < 96 ? resolved.size() : 96);
                out.resolvedBuffer.swap(resolved);
            } else {
                out.objectStatus = "OMAP_TARGET_READ_FAILED";
                out.interpretation = purpose + ": matching OMAP value was found, but the resolved object read failed.";
                out.notes += out.notes.empty() ? resolvedErr : ("; " + resolvedErr);
            }
            break;
        }

        std::uint64_t childOid = 0;
        std::uint32_t childEntry = 0;
        bool childFound = false;
        bool usedFallbackFirst = false;
        const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
        std::uint64_t fallbackChild = 0;
        std::uint32_t fallbackEntry = 0;
        for (std::uint32_t i = 0; i < limit; ++i) {
            std::size_t keyAbs = 0;
            std::size_t valAbs = 0;
            std::string detail;
            if (!aff4ApfsFixedKvAbsForProbe(node, i, 8U, keyAbs, valAbs, detail)) {
                if (i == 0 && out.notes.empty()) out.notes = detail;
                continue;
            }
            const std::uint64_t keyOid = readLe64(node, keyAbs + 0U);
            const std::uint64_t keyXid = readLe64(node, keyAbs + 8U);
            const std::uint64_t candidateChild = readLe64(node, valAbs + 0U);
            if (i == 0) { fallbackChild = candidateChild; fallbackEntry = i; }
            if (aff4ApfsCompareOmapKeyForProbe(keyOid, keyXid, targetOid, targetXid) <= 0) {
                childOid = candidateChild;
                childEntry = i;
                childFound = true;
            }
        }
        if (!childFound && fallbackChild != 0) {
            childOid = fallbackChild;
            childEntry = fallbackEntry;
            childFound = true;
            usedFallbackFirst = true;
        }
        if (!childFound || childOid == 0) {
            out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_NOT_SELECTED";
            out.interpretation = purpose + ": branch node parsed, but no usable child pointer could be selected for the target key.";
            break;
        }
        aff4ApfsAppendBranchPathForProbe(out.branchPath, "child_entry=" + std::to_string(childEntry) + ";child_oid=" + std::to_string(childOid) + (usedFallbackFirst ? ";fallback_first" : ""));
        std::uint64_t childOffset = 0;
        if (!safeNodeOffset(childOid, childOffset)) {
            out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_OFFSET_UNSAFE";
            out.interpretation = purpose + ": selected branch child OID could not be converted to a safe read offset.";
            break;
        }
        reusableChildNodeBuffer.clear();
        std::string childErr;
        const long long childRead = readVirtual(childOffset, blockSize, reusableChildNodeBuffer, childErr);
        if (childRead <= 0 || reusableChildNodeBuffer.size() < 64) {
            out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_READ_FAILED";
            out.interpretation = purpose + ": selected branch child could not be read as a B-tree node.";
            out.notes += out.notes.empty() ? childErr : ("; " + childErr);
            break;
        }
        node.swap(reusableChildNodeBuffer);
        nodeOid = childOid;
        nodeOffset = childOffset;
        nodeRead = childRead;
    }
    if (out.lookupStatus.empty()) {
        out.lookupStatus = "VOLUME_OMAP_BRANCH_TRAVERSAL_LIMIT_REACHED";
        out.interpretation = purpose + ": bounded branch traversal reached the safety depth limit before a leaf node was resolved.";
    }
    return out;
}



constexpr std::uint64_t kApfsFsObjectIdMask = 0x0fffffffffffffffULL;

std::uint64_t apfsFsKeyObjectId(std::uint64_t keyRaw) {
    return keyRaw & kApfsFsObjectIdMask;
}

std::uint8_t apfsFsKeyRecordType(std::uint64_t keyRaw) {
    return static_cast<std::uint8_t>((keyRaw >> 60U) & 0x0fU);
}

std::string apfsFsRecordTypeLabel(std::uint8_t t) {
    switch (t) {
        case 0: return "ANY";
        case 1: return "SNAP_METADATA";
        case 2: return "EXTENT";
        case 3: return "INODE";
        case 4: return "XATTR";
        case 5: return "SIBLING_LINK";
        case 6: return "DSTREAM_ID";
        case 7: return "CRYPTO_STATE";
        case 8: return "FILE_EXTENT";
        case 9: return "DIR_REC";
        case 10: return "DIR_STATS";
        case 11: return "SNAP_NAME";
        case 12: return "SIBLING_MAP";
        case 13: return "FILE_INFO";
        default: return std::string("FS_RECORD_TYPE_") + std::to_string(static_cast<unsigned int>(t));
    }
}

bool isApfsCompressionOrResourceXattrName(const std::string& name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return n == "com.apple.decmpfs" ||
           n == "com.apple.resourcefork" ||
           n.find("decmpfs") != std::string::npos ||
           n.find("resourcefork") != std::string::npos;
}

std::string apfsXattrStorageLabel(std::uint16_t flags) {
    const bool dataStream = (flags & 0x0001U) != 0;
    const bool embedded = (flags & 0x0002U) != 0;
    if (dataStream && embedded) return "INVALID_BOTH_STREAM_AND_EMBEDDED";
    if (dataStream) return "XATTR_DATA_STREAM";
    if (embedded) return "XATTR_DATA_EMBEDDED";
    return "XATTR_STORAGE_UNKNOWN";
}

int decmpfsCompressionTypeFromPreviewHex(const std::string& hex) {
    // Decmpfs embedded preview begins with little-endian CMPF bytes: 66 70 6D 63,
    // followed by a little-endian compression type.  Type 4 and 8 indicate data
    // stored in the resource fork, so the associated com.apple.ResourceFork stream
    // must be followed before the logical file can be reconstructed.
    std::vector<unsigned int> bytes;
    std::istringstream iss(hex);
    std::string tok;
    while (iss >> tok) {
        if (tok.size() > 2) tok = tok.substr(0, 2);
        try {
            bytes.push_back(static_cast<unsigned int>(std::stoul(tok, nullptr, 16)) & 0xffU);
        } catch (...) {
            break;
        }
        if (bytes.size() >= 8U) break;
    }
    if (bytes.size() < 8U) return 0;
    if (!(bytes[0] == 0x66U && bytes[1] == 0x70U && bytes[2] == 0x6dU && bytes[3] == 0x63U)) return 0;
    return static_cast<int>(bytes[4] | (bytes[5] << 8U) | (bytes[6] << 16U) | (bytes[7] << 24U));
}


std::uint64_t decmpfsUncompressedSizeFromPreviewHex(const std::string& hex) {
    std::vector<unsigned int> bytes;
    std::istringstream iss(hex);
    std::string tok;
    while (iss >> tok) {
        if (tok.size() > 2) tok = tok.substr(0, 2);
        try { bytes.push_back(static_cast<unsigned int>(std::stoul(tok, nullptr, 16)) & 0xffU); }
        catch (...) { break; }
        if (bytes.size() >= 16U) break;
    }
    if (bytes.size() < 16U) return 0;
    if (!(bytes[0] == 0x66U && bytes[1] == 0x70U && bytes[2] == 0x6dU && bytes[3] == 0x63U)) return 0;
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < 8U; ++i) v |= (static_cast<std::uint64_t>(bytes[8U + i]) << (8U * i));
    return v;
}



bool tryParseNumericTxtStem(const std::string& name, std::uint64_t& outValue) {
    std::string base = name;
    const std::size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    const std::string suffix = ".txt";
    if (base.size() <= suffix.size()) return false;
    std::string lower = base;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.substr(lower.size() - suffix.size()) != suffix) return false;
    const std::string stem = base.substr(0, base.size() - suffix.size());
    if (stem.empty()) return false;
    for (char ch : stem) if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    try { outValue = static_cast<std::uint64_t>(std::stoull(stem)); return true; }
    catch (...) { return false; }
}

long long cacheNameFileIdClosenessScore(const std::string& targetName, std::uint64_t childFileId) {
    std::uint64_t stem = 0;
    if (!tryParseNumericTxtStem(targetName, stem)) return 0;
    const std::uint64_t diff = (childFileId > stem) ? (childFileId - stem) : (stem - childFileId);
    if (diff <= 16U) return 60000;
    if (diff <= 128U) return 45000;
    if (diff <= 4096U) return 15000;
    if (diff > 100000U) return -30000;
    return -5000;
}

bool isLikelyStoreV2GroupDirectoryName(const std::string& name) {
    if (name.size() != 36U) return false;
    const std::size_t hyphenPositions[] = {8U, 13U, 18U, 23U};
    for (std::size_t i = 0; i < name.size(); ++i) {
        bool shouldBeHyphen = false;
        for (std::size_t hp : hyphenPositions) { if (i == hp) { shouldBeHyphen = true; break; } }
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (shouldBeHyphen) { if (name[i] != '-') return false; }
        else if (!std::isxdigit(c)) return false;
    }
    return true;
}



std::string safePrintableUtf8Fragment(const std::vector<unsigned char>& data, std::size_t off, std::size_t len) {
    if (off >= data.size()) return {};
    const std::size_t end = std::min<std::size_t>(data.size(), off + len);
    std::string out;
    out.reserve(end - off);
    for (std::size_t i = off; i < end; ++i) {
        unsigned char c = data[i];
        if (c == 0) break;
        if (c >= 0x20 && c != 0x7f) out.push_back(static_cast<char>(c));
        else out.push_back('?');
    }
    return out;
}

void parseResolvedApfsVolumeSuperblock(ApfsResolvedVolumeSuperblockRow& row,
                                       const std::vector<unsigned char>& vol,
                                       const std::string& readNotes) {
    if (row.resolvedBytesRead <= 0 || vol.empty()) {
        row.status = "RESOLVED_APSB_READ_FAILED";
        row.interpretation = "The container OMAP selected a physical address for this filesystem OID, but the bounded read returned no bytes.";
        row.notes = readNotes;
        return;
    }
    if (vol.size() < 36) {
        row.status = "RESOLVED_APSB_BUFFER_TOO_SMALL";
        row.interpretation = "The resolved block was too small to contain an APFS object header and magic.";
        row.notes = readNotes;
        row.sampleHex = hexSampleBytes(vol.data(), vol.size());
        return;
    }
    row.objectOid = readLe64(vol, 8);
    row.objectXid = readLe64(vol, 16);
    row.objectTypeRaw = readLe32(vol, 24);
    row.objectTypeLabel = apfsObjectTypeLabel(row.objectTypeRaw);
    row.objectSubtype = readLe32(vol, 28);
    row.magic.assign(reinterpret_cast<const char*>(vol.data() + 32), reinterpret_cast<const char*>(vol.data() + 36));
    row.sampleHex = hexSampleBytes(vol.data(), vol.size() < 96 ? vol.size() : 96);
    if (row.magic != "APSB") {
        row.status = "RESOLVED_OBJECT_NOT_APSB";
        row.interpretation = "The selected OMAP entry did not resolve to APFS volume-superblock magic at object offset +32.";
        row.notes = readNotes;
        return;
    }

    row.status = "APSB_PARSED_FROM_CONTAINER_OMAP";
    row.fsIndex = readLe32(vol, 36);
    row.features = readLe64(vol, 40);
    row.readonlyCompatibleFeatures = readLe64(vol, 48);
    row.incompatibleFeatures = readLe64(vol, 56);
    row.unmountTime = readLe64(vol, 64);
    row.reserveBlockCount = readLe64(vol, 72);
    row.quotaBlockCount = readLe64(vol, 80);
    row.allocBlockCount = readLe64(vol, 88);
    row.rootTreeType = readLe32(vol, 116);
    row.extentrefTreeType = readLe32(vol, 120);
    row.snapMetaTreeType = readLe32(vol, 124);
    row.apfsOmapOid = readLe64(vol, 128);
    row.rootTreeOid = readLe64(vol, 136);
    row.extentrefTreeOid = readLe64(vol, 144);
    row.snapMetaTreeOid = readLe64(vol, 152);
    row.revertToXid = readLe64(vol, 160);
    row.revertToSblockOid = readLe64(vol, 168);
    row.nextObjId = readLe64(vol, 176);
    row.numFiles = readLe64(vol, 184);
    row.numDirectories = readLe64(vol, 192);
    row.numSymlinks = readLe64(vol, 200);
    row.numOtherFsobjects = readLe64(vol, 208);
    row.numSnapshots = readLe64(vol, 216);
    row.totalBlocksAlloced = readLe64(vol, 224);
    row.totalBlocksFreed = readLe64(vol, 232);
    row.volumeUuid = bytesToUuidString(vol, 240);
    row.lastModTime = readLe64(vol, 256);
    row.fsFlags = readLe64(vol, 264);
    row.volumeName = readFixedUtf8Z(vol, 704, 256);
    row.nextDocId = readLe32(vol, 960);
    row.role = readLe16(vol, 964);
    row.interpretation = "APFS volume superblock parsed from the container OMAP-selected physical block. apfs_omap_oid and apfs_root_tree_oid are now available for the volume object-map/catalog phase.";
    row.notes = readNotes;
}





bool containsU64(const std::vector<std::uint64_t>& values, std::uint64_t v) {
    return std::find(values.begin(), values.end(), v) != values.end();
}




struct Aff4ZipCentralDirectoryRow {
    std::size_t index = 0;
    std::string entryName;
    std::uint16_t method = 0;
    std::uint64_t compressedSize = 0;
    std::uint64_t uncompressedSize = 0;
    std::uint64_t localHeaderOffset = 0;
    std::string classification;
    std::string spotlightHint;
    std::string apfsHint;
    std::string notes;
};
std::string asciiLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string aff4ZipEntryClassification(const std::string& name) {
    const std::string n = asciiLower(name);
    if (n.find("corespotlight") != std::string::npos || n.find("store-v2") != std::string::npos || n.find(".store.db") != std::string::npos || n.find("index.db") != std::string::npos) return "SPOTLIGHT_PATH_HINT";
    if (n.find("container.description") != std::string::npos || n.find("information.turtle") != std::string::npos || n.find("version.txt") != std::string::npos || n.find("aff4://") != std::string::npos) return "AFF4_METADATA";
    if (n.find("apfs") != std::string::npos) return "APFS_NAME_HINT";
    if (n.find("image") != std::string::npos || n.find("stream") != std::string::npos || n.find("data") != std::string::npos) return "POSSIBLE_IMAGE_STREAM";
    return "ZIP_ENTRY";
}

std::string aff4ZipSpotlightHint(const std::string& name) {
    const std::string n = asciiLower(name);
    if (n.find("corespotlight") != std::string::npos) return "IOS_CORESPOTLIGHT_NAME_HINT";
    if (n.find("store-v2") != std::string::npos || n.find(".store.db") != std::string::npos) return "MACOS_SPOTLIGHT_NAME_HINT";
    if (n.find("index.db") != std::string::npos) return "SPOTLIGHT_SQLITE_NAME_HINT";
    return "";
}

std::string aff4ZipApfsHint(const std::string& name) {
    const std::string n = asciiLower(name);
    if (n.find("apfs") != std::string::npos) return "APFS_NAME_HINT";
    return "";
}

std::string decimalZeroPad(std::uint32_t value, int width) {
    std::ostringstream ss;
    ss << std::setw(width) << std::setfill('0') << value;
    return ss.str();
}

bool readExactFileBytes(const fs::path& path, std::uint64_t offset, std::size_t length, std::vector<unsigned char>& out, std::string& error) {
    out.clear();
#if defined(_WIN32)
    if (offset > static_cast<std::uint64_t>((std::numeric_limits<long long>::max)())) {
        error = "Offset too large for Windows 64-bit file seek.";
        return false;
    }
    FILE* f = nullptr;
    const std::wstring widePath = wideProcessPath(path);
    if (_wfopen_s(&f, widePath.c_str(), L"rb") != 0 || !f) {
        error = "Unable to open file for exact single-file AFF4 ZIP probe: " + pathString(path);
        return false;
    }
    if (_fseeki64(f, static_cast<long long>(offset), SEEK_SET) != 0) {
        error = "64-bit seek failed at offset " + std::to_string(offset);
        fclose(f);
        return false;
    }
    out.assign(length, 0);
    const std::size_t got = std::fread(out.data(), 1, length, f);
    if (got != length) {
        error = "Short read at offset " + std::to_string(offset) + ": requested=" + std::to_string(length) + " got=" + std::to_string(got);
        out.resize(got);
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
#else
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Unable to open file for exact single-file AFF4 ZIP probe: " + pathString(path);
        return false;
    }
    if (offset > static_cast<std::uint64_t>((std::numeric_limits<std::streamoff>::max)())) {
        error = "Offset too large for platform streamoff.";
        return false;
    }
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) {
        error = "Seek failed at offset " + std::to_string(offset);
        return false;
    }
    out.assign(length, 0);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(length));
    const std::streamsize got = in.gcount();
    if (got < 0) {
        error = "Read failed.";
        return false;
    }
    out.resize(static_cast<std::size_t>(got));
    if (out.size() != length) {
        error = "Short read at offset " + std::to_string(offset) + ": requested=" + std::to_string(length) + " got=" + std::to_string(out.size());
        return false;
    }
    return true;
#endif
}


int aff4ZipDataChunkIndex(const std::string& name) {
    const std::string n = asciiLower(name);
    const std::size_t p = n.rfind("/data/");
    if (p == std::string::npos) return -1;
    std::string tail = n.substr(p + 6);
    if (tail.size() >= 6 && tail.substr(tail.size() - 6) == ".index") return -1;
    if (tail.size() < 8) return -1;
    int v = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(tail[i]))) return -1;
        v = v * 10 + (tail[i] - '0');
    }
    return v;
}

bool aff4ZipIsIndexEntry(const std::string& name) {
    const std::string n = asciiLower(name);
    return n.size() >= 6 && n.substr(n.size() - 6) == ".index";
}

bool readAff4ZipLocalPayloadOffset(const fs::path& path,
                                   const Aff4ZipCentralDirectoryRow& row,
                                   std::uint64_t& payloadOffset,
                                   std::string& error) {
    payloadOffset = 0;
    std::vector<unsigned char> fixed;
    if (!readExactFileBytes(path, row.localHeaderOffset, 30, fixed, error)) return false;
    if (readLe32(fixed, 0) != 0x04034b50U) {
        error = "ZIP local-file-header signature mismatch at offset " + std::to_string(row.localHeaderOffset);
        return false;
    }
    const std::uint16_t nameLen = readLe16(fixed, 26);
    const std::uint16_t extraLen = readLe16(fixed, 28);
    payloadOffset = row.localHeaderOffset + 30ULL + static_cast<std::uint64_t>(nameLen) + static_cast<std::uint64_t>(extraLen);
    return true;
}

bool readAff4StoredZipEntryTextFromProbe(const fs::path& caseDir,
                                         const fs::path& originalInput,
                                         const std::string& wantedEntryName,
                                         std::size_t maxBytes,
                                         std::string& text,
                                         std::string& error) {
    text.clear();
    error.clear();
    const fs::path csvPath = caseDir / "aff4_zip_central_directory.csv";
    if (!fs::exists(csvPath)) {
        error = "AFF4 central-directory probe CSV is not available yet.";
        return false;
    }
    Rows rows;
    try {
        rows = readCsv(csvPath);
    } catch (const std::exception& ex) {
        error = std::string("Unable to read AFF4 central-directory probe CSV: ") + ex.what();
        return false;
    }
    for (const auto& csvRow : rows) {
        if (get(csvRow, "entry_name") != wantedEntryName) continue;
        Aff4ZipCentralDirectoryRow row;
        row.entryName = wantedEntryName;
        try {
            row.method = static_cast<std::uint16_t>(std::stoul(get(csvRow, "compression_method")));
            row.compressedSize = static_cast<std::uint64_t>(std::stoull(get(csvRow, "compressed_size")));
            row.uncompressedSize = static_cast<std::uint64_t>(std::stoull(get(csvRow, "uncompressed_size")));
            row.localHeaderOffset = static_cast<std::uint64_t>(std::stoull(get(csvRow, "local_header_offset")));
        } catch (...) {
            error = "AFF4 central-directory row has invalid numeric values for " + wantedEntryName;
            return false;
        }
        if (row.method != 0) {
            error = wantedEntryName + " is not stored/uncompressed in the AFF4 ZIP container.";
            return false;
        }
        if (row.compressedSize > maxBytes) {
            error = wantedEntryName + " exceeds bounded metadata read cap.";
            return false;
        }
        std::uint64_t payloadOffset = 0;
        if (!readAff4ZipLocalPayloadOffset(originalInput, row, payloadOffset, error)) return false;
        std::vector<unsigned char> bytes;
        if (!readExactFileBytes(originalInput, payloadOffset, static_cast<std::size_t>(row.compressedSize), bytes, error)) return false;
        text.assign(bytes.begin(), bytes.end());
        return true;
    }
    error = "Entry not found in AFF4 central-directory probe CSV: " + wantedEntryName;
    return false;
}

bool shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(const fs::path& caseDir,
                                                        const fs::path& originalInput,
                                                        std::string& reason,
                                                        std::string& metadataSample) {
    reason.clear();
    metadataSample.clear();
    std::string turtle;
    std::string error;
    if (!readAff4StoredZipEntryTextFromProbe(caseDir, originalInput, "information.turtle", 256U * 1024U, turtle, error)) {
        reason = error;
        return false;
    }
    metadataSample = turtle.substr(0, 4000);
    const std::string t = asciiLower(turtle);
    const bool blackbagApfs = t.find("bbt:apfscontainerimage") != std::string::npos ||
                              t.find("bbt:apfst2containertype") != std::string::npos;
    const bool discontiguous = t.find("aff4:discontiguousimage") != std::string::npos ||
                               t.find("discontiguousimage") != std::string::npos;
    const bool lz4ImageStream = t.find("compressionmethod") != std::string::npos &&
                                t.find("lz4") != std::string::npos &&
                                t.find("imagestream") != std::string::npos;
    if (blackbagApfs && discontiguous && lz4ImageStream) {
        reason = "Detected BlackBag-style AFF4 APFS DiscontiguousImage with an LZ4 ImageStream. The current libaff4 dynamic C API can block while opening this layout on Windows; skip the dynamic probe and use the direct AFF4 ZIP map/index parser roadmap instead.";
        return true;
    }
    reason = "AFF4 metadata did not match the known blocking BlackBag/LZ4 discontiguous APFS layout.";
    return false;
}

std::vector<unsigned char> aff4DirectLz4DecompressBlock(const unsigned char* src, std::size_t srcSize, std::size_t expectedOutputSize) {
    constexpr std::size_t MaxOutput = 64U * 1024U * 1024U;
    if (expectedOutputSize > MaxOutput) throw std::runtime_error("AFF4 LZ4 output exceeds safety cap");
    std::vector<unsigned char> out(expectedOutputSize);
    std::size_t pos = 0;
    std::size_t outPos = 0;
    auto readLen = [&](std::size_t base) -> std::size_t {
        std::size_t len = base;
        if (base == 15U) {
            for (;;) {
                if (pos >= srcSize) throw std::runtime_error("AFF4 LZ4 length overrun");
                const unsigned char b = src[pos++];
                if (len > MaxOutput - static_cast<std::size_t>(b)) throw std::runtime_error("AFF4 LZ4 length overflow");
                len += static_cast<std::size_t>(b);
                if (b != 255U) break;
            }
        }
        return len;
    };
    while (pos < srcSize) {
        const unsigned char token = src[pos++];
        const std::size_t literalLen = readLen(static_cast<std::size_t>(token >> 4));
        if (literalLen > srcSize - pos || literalLen > expectedOutputSize - outPos) throw std::runtime_error("AFF4 LZ4 literal overrun");
        std::memcpy(out.data() + outPos, src + pos, literalLen);
        pos += literalLen;
        outPos += literalLen;
        if (outPos == expectedOutputSize || pos >= srcSize) break;
        if (srcSize - pos < 2U) throw std::runtime_error("AFF4 LZ4 match offset missing");
        const std::size_t matchOffset = static_cast<std::size_t>(src[pos]) | (static_cast<std::size_t>(src[pos + 1U]) << 8);
        pos += 2U;
        if (matchOffset == 0U || matchOffset > outPos) throw std::runtime_error("AFF4 LZ4 invalid match offset");
        std::size_t matchLen = readLen(static_cast<std::size_t>(token & 0x0fU));
        if (matchLen > MaxOutput - 4U) throw std::runtime_error("AFF4 LZ4 match length overflow");
        matchLen += 4U;
        if (matchLen > expectedOutputSize - outPos) throw std::runtime_error("AFF4 LZ4 match output overrun");
        while (matchLen-- > 0U) {
            out[outPos] = out[outPos - matchOffset];
            ++outPos;
        }
        if (outPos == expectedOutputSize) break;
    }
    if (outPos != expectedOutputSize) throw std::runtime_error("AFF4 LZ4 output size mismatch");
    return out;
}

struct Aff4DirectMapEntry {
    std::uint64_t virtualOffset = 0;
    std::uint64_t length = 0;
    std::uint64_t streamOffset = 0;
    std::uint32_t streamId = 0;
    std::uint64_t mapEntryIndex = 0;
};

struct Aff4DirectSqliteCandidateRow {
    std::size_t sequence = 0;
    std::string status;
    std::uint64_t virtualOffset = 0;
    std::uint64_t mapEntryIndex = 0;
    std::uint64_t chunkIndex = 0;
    std::uint32_t pageSize = 0;
    std::uint32_t dbPages = 0;
    std::uint64_t requestedBytes = 0;
    std::uint64_t carvedBytes = 0;
    std::string outputRelativePath;
    std::string sqliteOpenStatus;
    std::string sqliteMasterStatus;
    int tableCount = 0;
    std::string tableNames;
    std::string sampleHex;
    std::string notes;
};


} // namespace

void Aff4ProbeWorker::executeDirectMapReaderProbe(const fs::path& caseDir,
                                   const EvidenceSource& source,
                                   const RunOptions& opt,
                                   const fs::path& originalInput,
                                   const std::atomic_bool* cancelToken,
                                   Logger& log) {
    struct ProbeRow {
        std::string step;
        std::string status;
        std::uint64_t virtualOffset = 0;
        std::uint64_t streamOffset = 0;
        std::uint64_t mapEntryIndex = 0;
        std::uint64_t chunkIndex = 0;
        long long bytesRead = -1;
        std::string magic;
        std::string sampleHex;
        std::string notes;
    };
    std::vector<ProbeRow> probeRows;
    std::vector<Aff4DirectSqliteCandidateRow> sqliteCandidateRows;
    auto add = [&](const std::string& step, const std::string& status, std::uint64_t virtualOffset,
                   std::uint64_t streamOffset, std::uint64_t mapEntryIndex, std::uint64_t chunkIndex,
                   long long bytesRead, const std::string& magic, const std::string& sampleHex,
                   const std::string& notes) {
        ProbeRow r;
        r.step = step;
        r.status = status;
        r.virtualOffset = virtualOffset;
        r.streamOffset = streamOffset;
        r.mapEntryIndex = mapEntryIndex;
        r.chunkIndex = chunkIndex;
        r.bytesRead = bytesRead;
        r.magic = magic;
        r.sampleHex = sampleHex;
        r.notes = notes;
        probeRows.push_back(r);
    };

    const fs::path csvPath = caseDir / "aff4_direct_map_reader_probe.csv";
    const fs::path jsonPath = caseDir / "aff4_direct_map_reader_probe_summary.json";
    const fs::path mdPath = caseDir / "AFF4_DIRECT_MAP_READER_PROBE.md";
    const fs::path sqliteCsvPath = caseDir / "aff4_direct_sqlite_candidate_carve.csv";
    const fs::path sqliteJsonPath = caseDir / "aff4_direct_sqlite_candidate_carve_summary.json";
    const fs::path sqliteMdPath = caseDir / "AFF4_DIRECT_SQLITE_CANDIDATE_CARVE.md";
    const fs::path sqliteCandidateDir = caseDir / "Aff4DirectSqliteCandidates";
    auto directProbeCancelled = [&]() -> bool {
        if (cancelToken && cancelToken->load()) {
            appendRunStatus(caseDir, "aff4_direct_map_reader_probe_cancelled", "cancel requested by investigator");
            log.warn("AFF4 direct map reader probe cancelled by investigator.");
            return true;
        }
        return false;
    };
    if (directProbeCancelled()) return;
    appendRunProgress(caseDir, 30, "aff4_direct_map_reader_probe_start", "direct AFF4 map/index/data reader active");
    auto lastDirectHeartbeat = std::chrono::steady_clock::now();
    auto directHeartbeat = [&](const std::string& stage, const std::string& detail, int percent = 30) {
        const auto now = std::chrono::steady_clock::now();
        if (now - lastDirectHeartbeat < std::chrono::seconds(10)) return;
        lastDirectHeartbeat = now;
        appendRunProgress(caseDir, percent, stage, detail);
    };
    std::size_t mapEntryCount = 0;
    std::size_t mapEntriesScanned = 0;
    std::size_t chunksDecoded = 0;
    std::size_t lz4ChunksDecoded = 0;
    std::size_t alignedApfsHits = 0;
    std::size_t directSignatureHits = 0;
    std::size_t sqliteCandidatesFound = 0;
    std::size_t sqliteCandidatesCarved = 0;
    std::size_t sqliteCandidatesOpened = 0;
    ApfsNxSuperblockSummary directBestNx;
    std::vector<ApfsCheckpointDescriptorRow> directDescriptorRows;
    std::vector<ApfsVolumeSuperblockRow> directVolumeRows;
    std::vector<ApfsResolvedVolumeSuperblockRow> directResolvedVolumeRows;
    std::vector<ApfsVolumeOmapProbeRow> directVolumeOmapRows;
    std::vector<ApfsVolumeRootTreeLookupRow> directVolumeRootTreeLookupRows;
    std::vector<ApfsRootTreeNodeProbeRow> directRootTreeNodeRows;
    std::vector<ApfsRootTreeRecordSampleRow> directRootTreeRecordRows;
    std::vector<ApfsRootTreeRecordSampleRow> directSpotlightTargetRows;
    std::vector<ApfsRootTreeRecordSampleRow> directSpotlightNameSampleRows;
    std::vector<ApfsSpotlightCopyAttemptRow> directSpotlightCopyAttemptRows;
    std::vector<ApfsSpotlightInodeProbeRow> directSpotlightInodeRows;
    std::vector<ApfsSpotlightXattrProbeRow> directSpotlightXattrRows;
    std::vector<ApfsSpotlightFileExtentProbeRow> directSpotlightFileExtentRows;
    std::vector<ApfsSpotlightFileCopyOutRow> directSpotlightFileCopyOutRows;
    std::vector<ApfsDirectoryRecordEntry> directDirectoryRecordEntries;
    std::map<std::pair<std::uint32_t, std::uint64_t>, ApfsSpotlightInodeProbeRow> directIndexedInodeByObject;
    std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<ApfsSpotlightFileExtentProbeRow>> directIndexedFileExtentsByObject;
    ApfsSpotlightTargetScanMetrics directSpotlightScanMetrics;
    std::map<std::uint32_t, ApfsVolumeOmapProbeRow> directVolumeOmapBySequence;
    std::map<std::uint32_t, std::vector<unsigned char>> directVolumeOmapRootBySequence;
    std::vector<ApfsCheckpointMapEntryRow> directCheckpointMapRows;
    std::vector<ApfsCheckpointMappedObjectProbeRow> directCheckpointObjectRows;
    std::string finalStatus = "NOT_RUN";
    std::string finalNotes;

    try {
        const fs::path centralCsv = caseDir / "aff4_zip_central_directory.csv";
        Rows rows = readCsv(centralCsv);
        std::map<std::string, Aff4ZipCentralDirectoryRow> byName;
        for (const auto& csvRow : rows) {
            Aff4ZipCentralDirectoryRow r;
            r.entryName = get(csvRow, "entry_name");
            try {
                r.method = static_cast<std::uint16_t>(std::stoul(get(csvRow, "compression_method")));
                r.compressedSize = static_cast<std::uint64_t>(std::stoull(get(csvRow, "compressed_size")));
                r.uncompressedSize = static_cast<std::uint64_t>(std::stoull(get(csvRow, "uncompressed_size")));
                r.localHeaderOffset = static_cast<std::uint64_t>(std::stoull(get(csvRow, "local_header_offset")));
            } catch (...) {
                continue;
            }
            byName[r.entryName] = r;
        }

        auto readStoredEntry = [&](const std::string& name, std::vector<unsigned char>& out, std::string& err) -> bool {
            out.clear();
            const auto it = byName.find(name);
            if (it == byName.end()) {
                err = "AFF4 ZIP entry not found: " + name;
                return false;
            }
            if (it->second.method != 0) {
                err = "AFF4 ZIP entry is not stored/uncompressed: " + name;
                return false;
            }
            if (it->second.compressedSize > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
                err = "AFF4 ZIP entry too large for memory read: " + name;
                return false;
            }
            std::uint64_t payload = 0;
            if (!readAff4ZipLocalPayloadOffset(originalInput, it->second, payload, err)) return false;
            return readExactFileBytes(originalInput, payload, static_cast<std::size_t>(it->second.compressedSize), out, err);
        };

        auto hasSuffix = [](const std::string& value, const std::string& suffix) -> bool {
            return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        auto hasPrefix = [](const std::string& value, const std::string& prefix) -> bool {
            return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
        };
        auto countDataEntriesForBase = [&](const std::string& base) -> std::size_t {
            const std::string dataPrefix = base + "/data/";
            std::size_t count = 0;
            for (const auto& kv : byName) {
                if (hasPrefix(kv.first, dataPrefix)) ++count;
            }
            return count;
        };

        std::string mapUrn;
        std::size_t selectedDataEntryCount = 0;
        std::uint64_t selectedMapBytes = 0;
        for (const auto& kv : byName) {
            if (!hasSuffix(kv.first, "/map")) continue;
            const std::string candidateBase = kv.first.substr(0, kv.first.size() - 4U);
            if (byName.find(candidateBase + "/idx") == byName.end()) continue;
            const std::size_t dataEntryCount = countDataEntriesForBase(candidateBase);
            const std::uint64_t mapBytesForCandidate = kv.second.uncompressedSize;
            if (mapUrn.empty() || dataEntryCount > selectedDataEntryCount ||
                (dataEntryCount == selectedDataEntryCount && mapBytesForCandidate > selectedMapBytes)) {
                mapUrn = candidateBase;
                selectedDataEntryCount = dataEntryCount;
                selectedMapBytes = mapBytesForCandidate;
            }
        }

        std::vector<unsigned char> idxBytes;
        std::vector<unsigned char> mapBytes;
        std::string err;
        if (mapUrn.empty()) {
            finalStatus = "MAP_STREAM_NOT_FOUND";
            finalNotes = "No AFF4 stream base with both /idx and /map entries was found in the central directory.";
            add("direct_map_stream_select", finalStatus, 0, 0, 0, 0, -1, {}, {}, finalNotes);
        } else if (!readStoredEntry(mapUrn + "/idx", idxBytes, err)) {
            finalStatus = "IDX_READ_FAILED";
            finalNotes = err;
            add("direct_idx_read", finalStatus, 0, 0, 0, 0, -1, {}, {}, err);
        } else if (!readStoredEntry(mapUrn + "/map", mapBytes, err)) {
            finalStatus = "MAP_READ_FAILED";
            finalNotes = err;
            add("direct_map_read", finalStatus, 0, 0, 0, 0, -1, {}, {}, err);
        } else {
            add("direct_map_stream_select", "SELECTED", 0, 0, 0, 0, static_cast<long long>(selectedDataEntryCount), {}, {},
                "Selected AFF4 stream base " + mapUrn + " from central-directory entries with /idx, /map, and " + std::to_string(selectedDataEntryCount) + " /data entries.");
            mapEntryCount = mapBytes.size() / 28U;
            add("direct_idx_read", "READ_OK", 0, 0, 0, 0, static_cast<long long>(idxBytes.size()), {}, hexSampleBytes(idxBytes.data(), std::min<std::size_t>(idxBytes.size(), 64U)), "AFF4 map stream index was read directly from the ZIP payload.");
            add("direct_map_read", "READ_OK", 0, 0, 0, 0, static_cast<long long>(mapBytes.size()), {}, hexSampleBytes(mapBytes.data(), std::min<std::size_t>(mapBytes.size(), 64U)), "AFF4 map entries were read directly from the ZIP payload.");

            std::vector<Aff4DirectMapEntry> mapEntries;
            mapEntries.reserve(mapEntryCount);
            for (std::size_t mi = 0; mi < mapEntryCount; ++mi) {
                if (directProbeCancelled()) return;
                if ((mi % 50000U) == 0U) {
                    directHeartbeat("aff4_direct_map_entries_indexing", "map_entry=" + std::to_string(mi) + " of " + std::to_string(mapEntryCount), 30);
                }
                const std::size_t moff = mi * 28U;
                Aff4DirectMapEntry me;
                me.virtualOffset = readLe64(mapBytes, moff);
                me.length = readLe64(mapBytes, moff + 8U);
                me.streamOffset = readLe64(mapBytes, moff + 16U);
                me.streamId = readLe32(mapBytes, moff + 24U);
                me.mapEntryIndex = static_cast<std::uint64_t>(mi);
                if (me.streamId == 0U && me.length > 0U) mapEntries.push_back(me);
            }
            std::sort(mapEntries.begin(), mapEntries.end(), [](const auto& a, const auto& b) {
                if (a.virtualOffset != b.virtualOffset) return a.virtualOffset < b.virtualOffset;
                return a.mapEntryIndex < b.mapEntryIndex;
            });
            auto findMapEntryForVirtual = [&](std::uint64_t v) -> const Aff4DirectMapEntry* {
                auto it = std::upper_bound(mapEntries.begin(), mapEntries.end(), v, [](std::uint64_t value, const Aff4DirectMapEntry& e) {
                    return value < e.virtualOffset;
                });
                if (it == mapEntries.begin()) return nullptr;
                --it;
                if (v < it->virtualOffset) return nullptr;
                const std::uint64_t rel = v - it->virtualOffset;
                if (rel >= it->length) return nullptr;
                return &(*it);
            };

            std::map<std::uint32_t, std::vector<unsigned char>> indexCache;
            std::map<std::uint32_t, std::uint64_t> dataPayloadCache;
            auto decodeImageChunk = [&](std::uint64_t chunk, std::vector<unsigned char>& dec, std::string& decodeErr, bool& usedLz4) -> bool {
                dec.clear();
                decodeErr.clear();
                usedLz4 = false;
                const std::uint32_t bevvy = static_cast<std::uint32_t>(chunk / 1024ULL);
                const std::uint32_t chunkInBevvy = static_cast<std::uint32_t>(chunk % 1024ULL);
                const std::string seg = decimalZeroPad(bevvy, 8);
                std::vector<unsigned char>& indexBytes = indexCache[bevvy];
                if (indexBytes.empty()) {
                    if (!readStoredEntry(mapUrn + "/data/" + seg + ".index", indexBytes, decodeErr)) return false;
                }
                const std::size_t pointOff = static_cast<std::size_t>(chunkInBevvy) * 12U;
                if (pointOff + 12U > indexBytes.size()) {
                    decodeErr = "AFF4 data index point is beyond index payload.";
                    return false;
                }
                const std::uint64_t dataRel = readLe64(indexBytes, pointOff);
                const std::uint32_t compLen = readLe32(indexBytes, pointOff + 8U);
                if (compLen == 0U || compLen > 1024U * 1024U) {
                    decodeErr = "AFF4 compressed chunk length is zero or above safety cap.";
                    return false;
                }
                std::uint64_t dataPayload = 0;
                const auto dpIt = dataPayloadCache.find(bevvy);
                if (dpIt == dataPayloadCache.end()) {
                    const auto entryIt = byName.find(mapUrn + "/data/" + seg);
                    if (entryIt == byName.end()) {
                        decodeErr = "AFF4 data bevvy entry not found: " + seg;
                        return false;
                    }
                    if (!readAff4ZipLocalPayloadOffset(originalInput, entryIt->second, dataPayload, decodeErr)) return false;
                    dataPayloadCache[bevvy] = dataPayload;
                } else {
                    dataPayload = dpIt->second;
                }
                std::vector<unsigned char> comp;
                if (!readExactFileBytes(originalInput, dataPayload + dataRel, static_cast<std::size_t>(compLen), comp, decodeErr)) return false;
                try {
                    dec = (compLen == 32768U) ? comp : aff4DirectLz4DecompressBlock(comp.data(), comp.size(), 32768U);
                    usedLz4 = compLen != 32768U;
                } catch (const std::exception& ex) {
                    decodeErr = ex.what();
                    return false;
                }
                return true;
            };

            auto readVirtualBytes = [&](std::uint64_t virtualStart, std::uint64_t requested, std::vector<unsigned char>& out, std::string& readErr) -> bool {
                out.clear();
                readErr.clear();
                if (requested > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
                    readErr = "Requested virtual read exceeds size_t.";
                    return false;
                }
                out.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(requested, 8ULL * 1024ULL * 1024ULL)));
                std::uint64_t cursor = virtualStart;
                while (out.size() < requested) {
                    if (directProbeCancelled()) return {};
                    const Aff4DirectMapEntry* me = findMapEntryForVirtual(cursor);
                    if (!me) {
                        readErr = "No AFF4 map entry covers virtual offset " + std::to_string(cursor);
                        return false;
                    }
                    const std::uint64_t withinMap = cursor - me->virtualOffset;
                    const std::uint64_t imageStreamOffset = me->streamOffset + withinMap;
                    const std::uint64_t chunk = imageStreamOffset / 32768ULL;
                    const std::size_t chunkOff = static_cast<std::size_t>(imageStreamOffset % 32768ULL);
                    std::vector<unsigned char> dec;
                    bool usedLz4 = false;
                    if (!decodeImageChunk(chunk, dec, readErr, usedLz4)) return false;
                    if (chunkOff >= dec.size()) {
                        readErr = "Decoded AFF4 chunk is shorter than expected.";
                        return false;
                    }
                    const std::uint64_t remainingRequested = requested - static_cast<std::uint64_t>(out.size());
                    const std::uint64_t remainingMap = me->length - withinMap;
                    const std::size_t canCopy = static_cast<std::size_t>(std::min<std::uint64_t>({remainingRequested, remainingMap, static_cast<std::uint64_t>(dec.size() - chunkOff)}));
                    out.insert(out.end(), dec.begin() + static_cast<std::ptrdiff_t>(chunkOff), dec.begin() + static_cast<std::ptrdiff_t>(chunkOff + canCopy));
                    cursor += static_cast<std::uint64_t>(canCopy);
                    if (canCopy == 0U) {
                        readErr = "AFF4 virtual read made no progress.";
                        return false;
                    }
                }
                return true;
            };

            auto readBe16Local = [](const std::vector<unsigned char>& data, std::size_t off) -> std::uint16_t {
                if (off + 2U > data.size()) return 0;
                return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[off]) << 8U) | static_cast<std::uint16_t>(data[off + 1U]));
            };
            auto readBe32Local = [](const std::vector<unsigned char>& data, std::size_t off) -> std::uint32_t {
                if (off + 4U > data.size()) return 0;
                return (static_cast<std::uint32_t>(data[off]) << 24U) |
                       (static_cast<std::uint32_t>(data[off + 1U]) << 16U) |
                       (static_cast<std::uint32_t>(data[off + 2U]) << 8U) |
                       static_cast<std::uint32_t>(data[off + 3U]);
            };
            auto isPowerOfTwo = [](std::uint32_t v) -> bool { return v != 0U && (v & (v - 1U)) == 0U; };
            auto validateSqliteCandidate = [&](const fs::path& candidatePath, Aff4DirectSqliteCandidateRow& row) {
                sqlite3* ext = nullptr;
                int rc = sqlite3_open_v2(pathString(candidatePath).c_str(), &ext, SQLITE_OPEN_READONLY, nullptr);
                if (rc != SQLITE_OK) {
                    row.sqliteOpenStatus = ext ? sqlite3_errmsg(ext) : "sqlite handle not created";
                    if (ext) sqlite3_close(ext);
                    return;
                }
                row.sqliteOpenStatus = "OPEN_OK";
                sqlite3_stmt* st = nullptr;
                rc = sqlite3_prepare_v2(ext, "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name LIMIT 20", -1, &st, nullptr);
                if (rc != SQLITE_OK) {
                    row.sqliteMasterStatus = sqlite3_errmsg(ext);
                    sqlite3_close(ext);
                    return;
                }
                std::vector<std::string> names;
                while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                    const unsigned char* txt = sqlite3_column_text(st, 0);
                    names.push_back(txt ? reinterpret_cast<const char*>(txt) : "");
                }
                sqlite3_finalize(st);
                if (rc == SQLITE_DONE) {
                    row.sqliteMasterStatus = "SQLITE_MASTER_OK";
                    row.tableCount = static_cast<int>(names.size());
                    std::ostringstream joined;
                    for (std::size_t ni = 0; ni < names.size(); ++ni) {
                        if (ni) joined << ';';
                        joined << names[ni];
                    }
                    row.tableNames = joined.str();
                    if (!names.empty()) ++sqliteCandidatesOpened;
                } else {
                    row.sqliteMasterStatus = sqlite3_errmsg(ext);
                }
                sqlite3_close(ext);
            };
            auto carveSqliteCandidate = [&](std::uint64_t hitVirtual, std::uint64_t mapEntryIndex, std::uint64_t chunkIndex) {
                if (sqliteCandidateRows.size() >= 12U) return;
                ++sqliteCandidatesFound;
                Aff4DirectSqliteCandidateRow row;
                row.sequence = sqliteCandidateRows.size() + 1U;
                row.virtualOffset = hitVirtual;
                row.mapEntryIndex = mapEntryIndex;
                row.chunkIndex = chunkIndex;
                std::vector<unsigned char> header;
                std::string readErr;
                if (!readVirtualBytes(hitVirtual, 4096U, header, readErr) || header.size() < 100U) {
                    row.status = "HEADER_READ_FAILED";
                    row.notes = readErr;
                    sqliteCandidateRows.push_back(row);
                    return;
                }
                row.sampleHex = hexSampleBytes(header.data(), std::min<std::size_t>(header.size(), 128U));
                if (!bytesAt(header, 0, "SQLite format 3", 15U)) {
                    row.status = "HEADER_SIGNATURE_MISMATCH";
                    row.notes = "Virtual read at the signature offset did not begin with a SQLite header.";
                    sqliteCandidateRows.push_back(row);
                    return;
                }
                std::uint32_t pageSize = readBe16Local(header, 16U);
                if (pageSize == 1U) pageSize = 65536U;
                row.pageSize = pageSize;
                row.dbPages = readBe32Local(header, 28U);
                constexpr std::uint64_t DefaultCarveBytes = 2ULL * 1024ULL * 1024ULL;
                constexpr std::uint64_t MaxCarveBytes = 8ULL * 1024ULL * 1024ULL;
                std::uint64_t requested = DefaultCarveBytes;
                if (pageSize >= 512U && pageSize <= 65536U && isPowerOfTwo(pageSize) && row.dbPages > 0U) {
                    const std::uint64_t dbBytes = static_cast<std::uint64_t>(pageSize) * static_cast<std::uint64_t>(row.dbPages);
                    if (dbBytes >= pageSize) requested = std::min<std::uint64_t>(dbBytes, MaxCarveBytes);
                }
                row.requestedBytes = requested;
                std::vector<unsigned char> candidate;
                readErr.clear();
                const bool fullRead = readVirtualBytes(hitVirtual, requested, candidate, readErr);
                if (candidate.size() < 100U) {
                    row.status = "CARVE_READ_FAILED";
                    row.notes = readErr;
                    sqliteCandidateRows.push_back(row);
                    return;
                }
                row.carvedBytes = static_cast<std::uint64_t>(candidate.size());
                std::ostringstream name;
                name << "sqlite_candidate_" << std::setw(3) << std::setfill('0') << row.sequence
                     << "_v" << hitVirtual << ".db";
                const fs::path outPath = sqliteCandidateDir / name.str();
                std::error_code mkEc;
                fs::create_directories(sqliteCandidateDir, mkEc);
                std::ofstream out(outPath, std::ios::binary);
                out.write(reinterpret_cast<const char*>(candidate.data()), static_cast<std::streamsize>(candidate.size()));
                out.close();
                row.outputRelativePath = pathString(fs::path("Aff4DirectSqliteCandidates") / name.str());
                row.status = fullRead ? "CARVED_BOUNDED_CANDIDATE" : "CARVED_PARTIAL_CANDIDATE";
                row.notes = fullRead ? "Candidate carved by direct AFF4 virtual-offset reads." : ("Candidate partially carved before read failure: " + readErr);
                ++sqliteCandidatesCarved;
                validateSqliteCandidate(outPath, row);
                sqliteCandidateRows.push_back(row);
            };

            auto rememberNxCandidateFromDecodedChunk = [&](const std::vector<unsigned char>& dec,
                                                           std::size_t magicOffset,
                                                           std::uint64_t blockVirtualOffset,
                                                           std::uint64_t originalMapEntryIndex,
                                                           std::uint64_t chunkIndex) {
                if (magicOffset < 32U) return;
                const std::size_t blockStart = magicOffset - 32U;
                if (blockStart + 4096U > dec.size()) return;
                std::vector<unsigned char> block(dec.begin() + static_cast<std::ptrdiff_t>(blockStart),
                                                 dec.begin() + static_cast<std::ptrdiff_t>(blockStart + 4096U));
                ApfsNxSuperblockSummary candidate = parseApfsNxSuperblock(block, blockVirtualOffset, 4096);
                if (!candidate.found) return;
                candidate.notes = "APFS NXSB parsed from directly decoded AFF4 sorted virtual map. source_map_entry=" +
                                  std::to_string(originalMapEntryIndex) + "; chunk_index=" + std::to_string(chunkIndex);
                if (!directBestNx.found || candidate.xid > directBestNx.xid) {
                    directBestNx = candidate;
                }
            };

            auto probeDirectApfsFromBestNx = [&]() {
                if (!directBestNx.found || directBestNx.blockSize == 0) return;
                std::set<std::string> seenResolvedVolumes;

                auto directReadVirtual = [&](std::uint64_t offset,
                                             std::uint64_t bytes,
                                             std::vector<unsigned char>& out,
                                             std::string& readErr) -> long long {
                    const bool ok = readVirtualBytes(offset, bytes, out, readErr);
                    return ok ? static_cast<long long>(out.size()) : -1;
                };

                auto safeDirectNodeOffset = [&](std::uint64_t oid, std::uint64_t& offsetOut) -> bool {
                    if (directBestNx.blockSize == 0) return false;
                    if (directBestNx.blockCount != 0 && oid >= directBestNx.blockCount) return false;
                    if (oid > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(directBestNx.blockSize))) return false;
                    offsetOut = oid * static_cast<std::uint64_t>(directBestNx.blockSize);
                    return true;
                };

                auto appendDirectRootTreeNodeSample = [&](const ApfsVolumeRootTreeLookupRow& lookup,
                                                          const std::vector<unsigned char>& node) {
                    ApfsRootTreeNodeProbeRow nr;
                    nr.sequence = static_cast<std::uint32_t>(directRootTreeNodeRows.size());
                    nr.volumeSequence = lookup.volumeSequence;
                    nr.targetRole = lookup.targetRole;
                    nr.fsOid = lookup.fsOid;
                    nr.volumeName = lookup.volumeName;
                    nr.apfsRootTreeOid = lookup.apfsRootTreeOid;
                    nr.targetXid = lookup.targetXid;
                    nr.nodeOid = lookup.resolvedObjectOid ? lookup.resolvedObjectOid : lookup.apfsRootTreeOid;
                    nr.virtualOffset = lookup.resolvedVirtualOffset;
                    nr.bytesRead = lookup.resolvedBytesRead;
                    if (node.size() < 64U) {
                        nr.status = "ROOT_TREE_NODE_READ_FAILED";
                        nr.interpretation = "Resolved root-tree object was not large enough for B-tree header sampling.";
                        directRootTreeNodeRows.push_back(nr);
                        return;
                    }
                    nr.objectOid = readLe64(node, 8);
                    nr.objectXid = readLe64(node, 16);
                    nr.objectTypeRaw = readLe32(node, 24);
                    nr.objectTypeLabel = apfsObjectTypeLabel(nr.objectTypeRaw);
                    nr.objectSubtype = readLe32(node, 28);
                    if (node.size() >= 36U) nr.magic.assign(reinterpret_cast<const char*>(node.data() + 32), reinterpret_cast<const char*>(node.data() + 36));
                    nr.btnFlags = readLe16(node, 32);
                    nr.btnLevel = readLe16(node, 34);
                    nr.btnNkeys = readLe32(node, 36);
                    nr.tableSpaceOffset = readLe16(node, 40);
                    nr.tableSpaceLength = readLe16(node, 42);
                    nr.freeSpaceOffset = readLe16(node, 44);
                    nr.freeSpaceLength = readLe16(node, 46);
                    nr.sampleHex = hexSampleBytes(node.data(), std::min<std::size_t>(node.size(), 96U));
                    if (nr.objectTypeLabel == "BTREE" || nr.objectTypeLabel == "BTREE_NODE") {
                        nr.status = "ROOT_TREE_NODE_HEADER_PARSED";
                        nr.interpretation = "APFS filesystem root-tree B-tree node header parsed through the direct AFF4 reader.";
                    } else {
                        nr.status = "ROOT_TREE_NODE_UNEXPECTED_OBJECT_TYPE";
                        nr.interpretation = "Resolved root-tree object was readable but did not parse as an APFS B-tree node.";
                        directRootTreeNodeRows.push_back(nr);
                        return;
                    }
                    directRootTreeNodeRows.push_back(nr);

                    const std::uint32_t limit = std::min<std::uint32_t>(nr.btnNkeys, 32U);
                    for (std::uint32_t i = 0; i < limit; ++i) {
                        std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                        std::string detail;
                        ApfsRootTreeRecordSampleRow rr;
                        rr.sequence = static_cast<std::uint32_t>(directRootTreeRecordRows.size());
                        rr.volumeSequence = lookup.volumeSequence;
                        rr.targetRole = lookup.targetRole;
                        rr.fsOid = lookup.fsOid;
                        rr.volumeName = lookup.volumeName;
                        rr.apfsRootTreeOid = lookup.apfsRootTreeOid;
                        rr.nodeOid = nr.nodeOid;
                        rr.nodeVirtualOffset = lookup.resolvedVirtualOffset;
                        rr.nodeLevel = nr.btnLevel;
                        rr.nodeNkeys = nr.btnNkeys;
                        rr.entryIndex = i;
                        if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) {
                            rr.status = "ROOT_TREE_RECORD_TOC_DECODE_FAILED";
                            rr.interpretation = "The root-tree node TOC entry could not be decoded safely.";
                            rr.notes = detail;
                            directRootTreeRecordRows.push_back(rr);
                            continue;
                        }
                        rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                        rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                        rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                        rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                        rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                        rr.keySampleHex = hexSampleBytes(node.data() + keyAbs, std::min<std::size_t>(keyLen, 64U));
                        if (valLen > 0 && valAbs < node.size()) rr.valueSampleHex = hexSampleBytes(node.data() + valAbs, std::min<std::size_t>(valLen, 64U));
                        if (valLen >= 8U && valAbs + 8U <= node.size()) rr.valueU64_0 = readLe64(node, valAbs);
                        if (valLen >= 16U && valAbs + 16U <= node.size()) rr.valueU64_1 = readLe64(node, valAbs + 8U);
                        if (valLen >= 24U && valAbs + 24U <= node.size()) rr.valueU64_2 = readLe64(node, valAbs + 16U);
                        if (keyLen >= 8U) {
                            rr.keyRaw = readLe64(node, keyAbs);
                            rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                            rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                            rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                        }
                        if (nr.btnLevel > 0 && valLen >= 8U) rr.branchChildOid = readLe64(node, valAbs);
                        if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                            const std::uint32_t nameLenAndHash = readLe32(node, keyAbs + 8U);
                            const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                            rr.decodedName = safePrintableUtf8Fragment(node, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                        }
                        rr.status = "ROOT_TREE_RECORD_SAMPLE_DECODED";
                        rr.interpretation = nr.btnLevel > 0 ? "Root-tree branch record sampled through the direct AFF4 reader." : "Root-tree leaf record sampled through the direct AFF4 reader.";
                        rr.notes = detail;
                        directRootTreeRecordRows.push_back(rr);
                    }
                };

                auto appendDirectVolumeOmapAndRootLookup = [&](const ApfsResolvedVolumeSuperblockRow& row) {
                    ApfsVolumeOmapProbeRow omRow;
                    omRow.sequence = static_cast<std::uint32_t>(directVolumeOmapRows.size());
                    omRow.volumeSequence = row.sequence;
                    omRow.targetRole = row.targetRole;
                    omRow.fsOid = row.fsOid;
                    omRow.volumeObjectOid = row.objectOid;
                    omRow.volumeObjectXid = row.objectXid;
                    omRow.apfsOmapOid = row.apfsOmapOid;
                    omRow.apfsRootTreeOid = row.rootTreeOid;
                    std::vector<unsigned char> omapTreeRootBuf;
                    if (row.apfsOmapOid == 0) {
                        omRow.omapStatus = "VOLUME_OMAP_OID_ZERO";
                        omRow.interpretation = "Parsed APSB did not contain an apfs_omap_oid value.";
                    } else if (!safeDirectNodeOffset(row.apfsOmapOid, omRow.omapVirtualOffset)) {
                        omRow.omapStatus = "VOLUME_OMAP_OFFSET_UNSAFE";
                        omRow.interpretation = "apfs_omap_oid could not be converted to a bounded physical-block offset.";
                    } else {
                        std::vector<unsigned char> omapBuf;
                        std::string omapErr;
                        omRow.omapBytesRead = directReadVirtual(omRow.omapVirtualOffset, directBestNx.blockSize, omapBuf, omapErr);
                        if (omRow.omapBytesRead > 0 && omapBuf.size() >= 88U) {
                            omRow.omapObjectOid = readLe64(omapBuf, 8);
                            omRow.omapObjectXid = readLe64(omapBuf, 16);
                            omRow.omapObjectTypeRaw = readLe32(omapBuf, 24);
                            omRow.omapObjectTypeLabel = apfsObjectTypeLabel(omRow.omapObjectTypeRaw);
                            omRow.omapObjectSubtype = readLe32(omapBuf, 28);
                            omRow.omFlags = readLe32(omapBuf, 32);
                            omRow.omSnapshotCount = readLe32(omapBuf, 36);
                            omRow.omTreeType = readLe32(omapBuf, 40);
                            omRow.omSnapshotTreeType = readLe32(omapBuf, 44);
                            omRow.omTreeOid = readLe64(omapBuf, 48);
                            omRow.omSnapshotTreeOid = readLe64(omapBuf, 56);
                            omRow.omMostRecentSnap = readLe64(omapBuf, 64);
                            omRow.omPendingRevertMin = readLe64(omapBuf, 72);
                            omRow.omPendingRevertMax = readLe64(omapBuf, 80);
                            omRow.sampleHex = hexSampleBytes(omapBuf.data(), std::min<std::size_t>(omapBuf.size(), 96U));
                            if (omRow.omapObjectTypeLabel == "OBJECT_MAP") {
                                omRow.omapStatus = "VOLUME_OMAP_PARSED";
                                omRow.interpretation = "Volume object-map physical object parsed through the direct AFF4 reader.";
                            } else {
                                omRow.omapStatus = "VOLUME_OMAP_UNEXPECTED_OBJECT_TYPE";
                                omRow.interpretation = "apfs_omap_oid was read, but the object header type was not OBJECT_MAP.";
                            }
                            if (omRow.omTreeOid != 0 && safeDirectNodeOffset(omRow.omTreeOid, omRow.treeVirtualOffset)) {
                                std::vector<unsigned char> treeBuf;
                                std::string treeErr;
                                omRow.treeBytesRead = directReadVirtual(omRow.treeVirtualOffset, directBestNx.blockSize, treeBuf, treeErr);
                                if (omRow.treeBytesRead > 0 && treeBuf.size() >= 56U) {
                                    omRow.treeObjectOid = readLe64(treeBuf, 8);
                                    omRow.treeObjectXid = readLe64(treeBuf, 16);
                                    omRow.treeObjectTypeRaw = readLe32(treeBuf, 24);
                                    omRow.treeObjectTypeLabel = apfsObjectTypeLabel(omRow.treeObjectTypeRaw);
                                    omRow.treeObjectSubtype = readLe32(treeBuf, 28);
                                    omRow.treeBtnFlags = readLe16(treeBuf, 32);
                                    omRow.treeBtnLevel = readLe16(treeBuf, 34);
                                    omRow.treeBtnNkeys = readLe32(treeBuf, 36);
                                    omRow.treeTableSpaceOffset = readLe16(treeBuf, 40);
                                    omRow.treeTableSpaceLength = readLe16(treeBuf, 42);
                                    omRow.treeSampleHex = hexSampleBytes(treeBuf.data(), std::min<std::size_t>(treeBuf.size(), 96U));
                                    omRow.treeStatus = (omRow.treeObjectTypeLabel == "BTREE" || omRow.treeObjectTypeLabel == "BTREE_NODE") ? "VOLUME_OMAP_BTREE_ROOT_READ" : "VOLUME_OMAP_TREE_UNEXPECTED_OBJECT_TYPE";
                                    if (omRow.treeStatus == "VOLUME_OMAP_BTREE_ROOT_READ") omapTreeRootBuf = treeBuf;
                                } else {
                                    omRow.treeStatus = "VOLUME_OMAP_BTREE_ROOT_READ_FAILED";
                                    omRow.notes = treeErr;
                                }
                            } else if (omRow.omTreeOid == 0) {
                                omRow.treeStatus = "VOLUME_OMAP_TREE_OID_ZERO";
                            } else {
                                omRow.treeStatus = "VOLUME_OMAP_TREE_OFFSET_UNSAFE";
                            }
                        } else {
                            omRow.omapStatus = "VOLUME_OMAP_READ_FAILED";
                            omRow.interpretation = "Unable to read volume object-map physical object through the direct AFF4 reader.";
                            omRow.notes = omapErr;
                        }
                    }
                    directVolumeOmapRows.push_back(omRow);
                    if (omRow.treeStatus == "VOLUME_OMAP_BTREE_ROOT_READ" && !omapTreeRootBuf.empty()) {
                        directVolumeOmapBySequence[omRow.volumeSequence] = omRow;
                        directVolumeOmapRootBySequence[omRow.volumeSequence] = omapTreeRootBuf;
                    }

                    ApfsVolumeRootTreeLookupRow lookup;
                    lookup.sequence = static_cast<std::uint32_t>(directVolumeRootTreeLookupRows.size());
                    lookup.volumeSequence = row.sequence;
                    lookup.targetRole = row.targetRole;
                    lookup.fsOid = row.fsOid;
                    lookup.volumeName = row.volumeName;
                    lookup.apfsOmapOid = omRow.apfsOmapOid;
                    lookup.omTreeOid = omRow.omTreeOid;
                    lookup.apfsRootTreeOid = row.rootTreeOid;
                    lookup.targetXid = row.objectXid;
                    if (row.rootTreeOid == 0) {
                        lookup.lookupStatus = "VOLUME_ROOT_TREE_OID_ZERO";
                        lookup.interpretation = "Parsed APSB did not contain an apfs_root_tree_oid target.";
                    } else {
                        const ApfsOmapTargetResolution resolved = aff4ResolveVolumeOmapTargetObjectForProbe(
                            omRow,
                            omapTreeRootBuf,
                            row.rootTreeOid,
                            row.objectXid,
                            directBestNx.blockSize,
                            directReadVirtual,
                            safeDirectNodeOffset,
                            "Direct AFF4 APFS volume root-tree lookup",
                            directProbeCancelled);
                        lookup.branchDepth = resolved.branchDepth;
                        lookup.branchPath = resolved.branchPath;
                        lookup.leafOid = resolved.leafOid;
                        lookup.leafVirtualOffset = resolved.leafVirtualOffset;
                        lookup.leafBytesRead = resolved.leafBytesRead;
                        lookup.leafBtnFlags = resolved.leafBtnFlags;
                        lookup.leafBtnLevel = resolved.leafBtnLevel;
                        lookup.leafBtnNkeys = resolved.leafBtnNkeys;
                        lookup.matchedEntryIndex = resolved.matchedEntryIndex;
                        lookup.matchedKeyOid = resolved.matchedKeyOid;
                        lookup.matchedKeyXid = resolved.matchedKeyXid;
                        lookup.valueFlags = resolved.valueFlags;
                        lookup.valueSize = resolved.valueSize;
                        lookup.valuePaddr = resolved.valuePaddr;
                        lookup.resolvedVirtualOffset = resolved.resolvedVirtualOffset;
                        lookup.resolvedBytesRead = resolved.resolvedBytesRead;
                        lookup.resolvedObjectOid = resolved.resolvedObjectOid;
                        lookup.resolvedObjectXid = resolved.resolvedObjectXid;
                        lookup.resolvedObjectTypeRaw = resolved.resolvedObjectTypeRaw;
                        lookup.resolvedObjectTypeLabel = resolved.resolvedObjectTypeLabel;
                        lookup.resolvedObjectSubtype = resolved.resolvedObjectSubtype;
                        lookup.resolvedMagic = resolved.resolvedMagic;
                        lookup.resolvedBtnFlags = resolved.resolvedBtnFlags;
                        lookup.resolvedBtnLevel = resolved.resolvedBtnLevel;
                        lookup.resolvedBtnNkeys = resolved.resolvedBtnNkeys;
                        lookup.lookupStatus = resolved.lookupStatus == "OMAP_TARGET_LOOKUP_RESOLVED" ? "VOLUME_ROOT_TREE_LOOKUP_RESOLVED" : resolved.lookupStatus;
                        lookup.rootTreeStatus = resolved.objectStatus == "OMAP_TARGET_BTREE_READ" ? "ROOT_TREE_BTREE_READ" : resolved.objectStatus;
                        lookup.interpretation = resolved.interpretation;
                        lookup.sampleHex = resolved.sampleHex;
                        lookup.resolvedSampleHex = resolved.resolvedSampleHex;
                        lookup.notes = resolved.notes;
                        if (lookup.rootTreeStatus == "ROOT_TREE_BTREE_READ") appendDirectRootTreeNodeSample(lookup, resolved.resolvedBuffer);
                    }
                    directVolumeRootTreeLookupRows.push_back(lookup);
                };

                auto appendMappedObjectProbe = [&](std::uint32_t entryIndex,
                                                    std::uint64_t oid,
                                                    std::uint64_t fsOid,
                                                    std::uint64_t paddr,
                                                    const std::string& targetRole,
                                                    const std::string& sourceNotes,
                                                    std::uint64_t keyXid,
                                                    std::uint32_t valueFlags,
                                                    std::uint32_t valueSize) {
                    if (directCheckpointObjectRows.size() >= 512U || paddr == 0) return;
                    if (paddr > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(directBestNx.blockSize))) return;
                    const std::uint64_t objVo = paddr * static_cast<std::uint64_t>(directBestNx.blockSize);
                    std::vector<unsigned char> obj;
                    std::string objErr;
                    const bool objOk = readVirtualBytes(objVo, directBestNx.blockSize, obj, objErr);
                    ApfsCheckpointMappedObjectProbeRow pr;
                    pr.sequence = static_cast<std::uint32_t>(directCheckpointObjectRows.size() + 1U);
                    pr.entryIndex = entryIndex;
                    pr.cpmOid = oid;
                    pr.cpmFsOid = fsOid;
                    pr.cpmPaddr = paddr;
                    pr.virtualOffset = objVo;
                    pr.bytesRead = objOk ? static_cast<long long>(obj.size()) : -1;
                    pr.targetRole = targetRole;
                    pr.status = objOk ? "READ_OK" : "READ_FAILED";
                    pr.notes = objErr.empty() ? sourceNotes : (sourceNotes + "; " + objErr);
                    if (objOk && obj.size() >= 36U) {
                        pr.mappedOid = readLe64(obj, 8);
                        pr.mappedXid = readLe64(obj, 16);
                        pr.mappedTypeRaw = readLe32(obj, 24);
                        pr.mappedSubtype = readLe32(obj, 28);
                        pr.mappedTypeLabel = apfsObjectTypeLabel(pr.mappedTypeRaw);
                        if (pr.mappedTypeLabel == "FS") pr.magic.assign(reinterpret_cast<const char*>(obj.data() + 32), reinterpret_cast<const char*>(obj.data() + 36));
                        else if (pr.mappedTypeLabel == "OBJECT_MAP") pr.magic = "OMAP";
                        else if (obj.size() >= 36U) pr.magic.assign(reinterpret_cast<const char*>(obj.data() + 32), reinterpret_cast<const char*>(obj.data() + 36));
                        pr.sampleHex = hexSampleBytes(obj.data(), std::min<std::size_t>(obj.size(), 96U));
                        if (pr.magic == "APSB") pr.interpretation = "OMAP resolved an APFS volume superblock candidate.";
                        else if (pr.mappedTypeLabel == "OBJECT_MAP") pr.interpretation = "Direct object ID resolved an APFS object map.";
                        else pr.interpretation = "Direct APFS object resolution returned an object candidate.";

                        if (pr.magic == "APSB") {
                            ApfsVolumeSuperblockRow vr;
                            vr.sequence = static_cast<std::uint32_t>(directVolumeRows.size() + 1U);
                            vr.fsOid = oid;
                            vr.virtualOffset = objVo;
                            vr.bytesRead = pr.bytesRead;
                            vr.oid = pr.mappedOid;
                            vr.xid = pr.mappedXid;
                            vr.objectTypeRaw = pr.mappedTypeRaw;
                            vr.objectSubtype = pr.mappedSubtype;
                            vr.objectTypeLabel = pr.mappedTypeLabel;
                            vr.magic = pr.magic;
                            vr.status = "APSB_FOUND_VIA_DIRECT_OMAP";
                            vr.fsIndexCandidate = readLe32(obj, 36);
                            vr.featuresCandidate = readLe64(obj, 40);
                            vr.readonlyCompatibleFeaturesCandidate = readLe64(obj, 48);
                            vr.incompatibleFeaturesCandidate = readLe64(obj, 56);
                            vr.unmountTimeCandidate = readLe64(obj, 64);
                            vr.volumeUuidCandidate = bytesToUuidString(obj, 240);
                            vr.interpretation = "APFS volume superblock resolved from the container OMAP leaf using the direct AFF4 reader.";
                            vr.sampleHex = pr.sampleHex;
                            vr.notes = sourceNotes;
                            directVolumeRows.push_back(vr);

                            const std::string resolvedKey = std::to_string(oid) + ":" + std::to_string(paddr);
                            if (seenResolvedVolumes.insert(resolvedKey).second) {
                                ApfsResolvedVolumeSuperblockRow rr;
                                rr.sequence = static_cast<std::uint32_t>(directResolvedVolumeRows.size());
                                rr.targetRole = targetRole;
                                rr.fsOid = oid;
                                rr.containerTargetXid = directBestNx.nextXid;
                                rr.omapKeyOid = oid;
                                rr.omapKeyXid = keyXid;
                                rr.omapValueFlags = valueFlags;
                                rr.omapValueSize = valueSize;
                                rr.omapValuePaddr = paddr;
                                rr.resolvedVirtualOffset = objVo;
                                rr.resolvedBytesRead = pr.bytesRead;
                                parseResolvedApfsVolumeSuperblock(rr, obj, sourceNotes);
                                directResolvedVolumeRows.push_back(rr);
                                if (rr.status == "APSB_PARSED_FROM_CONTAINER_OMAP") appendDirectVolumeOmapAndRootLookup(rr);
                            }
                        }
                    } else {
                        pr.interpretation = "Direct APFS object resolution failed or returned too few bytes.";
                    }
                    directCheckpointObjectRows.push_back(pr);
                };

                auto probeDirectOmapLeaf = [&]() {
                    if (directBestNx.omapOid == 0) return;
                    appendMappedObjectProbe(0, directBestNx.omapOid, 0, directBestNx.omapOid, "DIRECT_NX_OBJECT_MAP_OID", "Direct read of nx_omap_oid as an APFS physical object ID.", directBestNx.nextXid, 0, 0);

                    if (directBestNx.omapOid > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(directBestNx.blockSize))) return;
                    std::vector<unsigned char> omap;
                    std::string omapErr;
                    if (!readVirtualBytes(directBestNx.omapOid * static_cast<std::uint64_t>(directBestNx.blockSize), directBestNx.blockSize, omap, omapErr)) return;
                    if (omap.size() < 88U || apfsObjectTypeLabel(readLe32(omap, 24)) != "OBJECT_MAP") return;
                    const std::uint64_t omTreeOid = readLe64(omap, 48);
                    if (omTreeOid == 0 || omTreeOid > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(directBestNx.blockSize))) return;
                    std::vector<unsigned char> tree;
                    std::string treeErr;
                    if (!readVirtualBytes(omTreeOid * static_cast<std::uint64_t>(directBestNx.blockSize), directBestNx.blockSize, tree, treeErr)) return;
                    if (tree.size() < 96U) return;
                    const std::uint16_t btnFlags = readLe16(tree, 32);
                    const std::uint16_t btnLevel = readLe16(tree, 34);
                    const std::uint32_t btnNkeys = std::min<std::uint32_t>(readLe32(tree, 36), 512U);
                    const std::uint16_t tableSpaceLength = readLe16(tree, 42);
                    const bool fixedKv = (btnFlags & 0x0004U) != 0;
                    const bool isRoot = (btnFlags & 0x0001U) != 0;
                    const bool isLeaf = (btnFlags & 0x0002U) != 0;
                    if (!fixedKv || !isLeaf || btnLevel != 0) return;
                    const std::size_t tocStart = isRoot ? 56U : 40U;
                    const std::size_t keyAreaStart = tocStart + static_cast<std::size_t>(tableSpaceLength);
                    const std::size_t valueAreaEnd = tree.size() - (isRoot ? 40U : 0U);
                    for (std::uint32_t entryIndex = 0; entryIndex < btnNkeys; ++entryIndex) {
                        if (directProbeCancelled()) return;
                        const std::size_t toc = tocStart + static_cast<std::size_t>(entryIndex) * 4U;
                        if (toc + 4U > tree.size()) break;
                        const std::uint16_t keyOff = readLe16(tree, toc);
                        const std::uint16_t valOff = readLe16(tree, toc + 2U);
                        const std::size_t keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
                        const std::size_t valAbs = valueAreaEnd >= static_cast<std::size_t>(valOff) ? valueAreaEnd - static_cast<std::size_t>(valOff) : tree.size();
                        if (keyAbs + 16U > tree.size() || valAbs + 16U > tree.size()) continue;
                        const std::uint64_t keyOid = readLe64(tree, keyAbs);
                        const std::uint64_t keyXid = readLe64(tree, keyAbs + 8U);
                        const std::uint32_t valFlags = readLe32(tree, valAbs);
                        const std::uint32_t valSize = readLe32(tree, valAbs + 4U);
                        const std::uint64_t valPaddr = readLe64(tree, valAbs + 8U);
                        (void)keyXid;
                        (void)valFlags;
                        (void)valSize;
                        if (keyOid == directBestNx.omapOid) {
                            appendMappedObjectProbe(entryIndex, keyOid, 0, valPaddr, "OMAP_LEAF_NX_OBJECT_MAP_SELF", "Container OMAP leaf entry for nx_omap_oid.", keyXid, valFlags, valSize);
                        } else if (containsU64(directBestNx.fsOids, keyOid)) {
                            appendMappedObjectProbe(entryIndex, keyOid, keyOid, valPaddr, "OMAP_LEAF_NX_FILESYSTEM_OID", "Container OMAP leaf entry for APFS filesystem OID.", keyXid, valFlags, valSize);
                        }
                    }
                };

                probeDirectOmapLeaf();

                auto directIsSpotlightStoreV2TopLevelComponentName = [&](const std::string& name) -> bool {
                    const std::string lname = asciiLower(name);
                    if (lname == "0.directorystorefile" || lname == "0.directorystorefile.shadow") return true;
                    if (lname.rfind("0.index", 0) == 0 || lname.rfind("0.shadowindex", 0) == 0) return true;
                    if (lname.rfind("live.", 0) == 0) return true;
                    if (lname == "reversestore.updates" || lname == "store.updates" || lname == "store_generation") return true;
                    if (lname == "reversedirectorystore" || lname == "reversedirectorystore.shadow") return true;
                    if (lname == "permstore" || lname == "journalexclusion" || lname == "journals.migration_secondchance") return true;
                    if (lname == "cab.created" || lname == "cab.modified" || lname == "lion.created" || lname == "lion.modified" || lname == "star.created" || lname == "star.modified") return true;
                    if (lname == "tmp.cab" || lname == "tmp.lion" || lname == "tmp.star") return true;
                    if (lname.rfind("dbstr-", 0) == 0 || lname.rfind("dbhdr-", 0) == 0) return true;
                    if (lname.rfind("tmp.spotlight", 0) == 0) return true;
                    return false;
                };
                auto directIsSpotlightTargetName = [&](const std::string& name) -> bool {
                    const std::string lname = asciiLower(name);
                    return lname == ".spotlight-v100" || lname == "store-v2" || lname == "index.db" ||
                           directIsSpotlightStoreV2TopLevelComponentName(lname) ||
                           lname.find("spotlight") != std::string::npos || lname.find("corespotlight") != std::string::npos;
                };
                auto directSpotlightTargetKind = [&](const std::string& name) -> std::string {
                    const std::string lname = asciiLower(name);
                    if (lname == ".spotlight-v100") return "SPOTLIGHT_ROOT_DIRECTORY";
                    if (lname == "store-v2") return "SPOTLIGHT_STORE_V2_DIRECTORY";
                    if (lname == "store.db" || lname == ".store.db") return "SPOTLIGHT_STORE_DB_FILE";
                    if (lname == "store.db-wal" || lname == "store.db-shm") return "SPOTLIGHT_STORE_SQLITE_SUPPORT_FILE";
                    if (lname.rfind("dbstr-", 0) == 0) return "SPOTLIGHT_DBSTR_FILE";
                    if (lname.rfind("dbhdr-", 0) == 0) return "SPOTLIGHT_DBHDR_FILE";
                    if (directIsSpotlightStoreV2TopLevelComponentName(lname)) return "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
                    if (lname.find("corespotlight") != std::string::npos) return "IOS_CORESPOTLIGHT_NAME";
                    if (lname == "index.db") return "IOS_CORESPOTLIGHT_INDEX_DB_FILE";
                    return "SPOTLIGHT_RELATED_NAME";
                };
                auto appendDirectSpotlightCopyAttempt = [&](const ApfsRootTreeRecordSampleRow& rr) {
                    ApfsSpotlightCopyAttemptRow cr;
                    cr.sequence = static_cast<std::uint32_t>(directSpotlightCopyAttemptRows.size());
                    cr.volumeSequence = rr.volumeSequence;
                    cr.targetRole = rr.targetRole;
                    cr.fsOid = rr.fsOid;
                    cr.volumeName = rr.volumeName;
                    cr.parentObjectId = rr.keyObjectId;
                    cr.childFileId = rr.valueU64_0;
                    cr.targetName = rr.decodedName;
                    cr.targetKind = directSpotlightTargetKind(rr.decodedName);
                    if (rr.keyTypeRaw != 9U) {
                        cr.extractionStatus = "COPY_NOT_ATTEMPTED_NOT_DIRECTORY_RECORD";
                        cr.interpretation = "A Spotlight-related key/name was observed during bounded APFS scan, but it was not a directory-entry record with a child file ID.";
                    } else if (cr.childFileId == 0) {
                        cr.extractionStatus = "COPY_NOT_ATTEMPTED_NO_CHILD_FILE_ID";
                        cr.interpretation = "A Spotlight-related directory-entry name was decoded, but the value did not provide a usable child file ID candidate.";
                    } else if (cr.targetKind == "SPOTLIGHT_ROOT_DIRECTORY" || cr.targetKind == "SPOTLIGHT_STORE_V2_DIRECTORY" || cr.targetKind == "IOS_CORESPOTLIGHT_NAME") {
                        cr.extractionStatus = "COPY_NOT_ATTEMPTED_DIRECTORY_RECURSION_PENDING";
                        cr.interpretation = "A Spotlight-related directory was found through the direct AFF4/APFS map reader. The next step is path-scoped child enumeration and then file extent copy-out.";
                    } else {
                        cr.extractionStatus = "COPY_NOT_ATTEMPTED_FILE_EXTENTS_PENDING";
                        cr.interpretation = "A Spotlight-related file name was found through the direct AFF4/APFS map reader. File-byte extraction remains gated until inode, xattr, and file extent provenance is resolved.";
                    }
                    cr.notes = "direct_aff4_map_reader=1; bounded_btree_scan=1; copy_out_intentionally_gated_for_forensic_provenance";
                    directSpotlightCopyAttemptRows.push_back(cr);
                };

                struct DirectFsPendingNode {
                    std::uint32_t volumeSequence = 0;
                    std::string targetRole;
                    std::uint64_t fsOid = 0;
                    std::string volumeName;
                    std::uint64_t apfsRootTreeOid = 0;
                    std::uint64_t nodeOid = 0;
                    std::uint64_t nodeVirtualOffset = 0;
                    std::uint64_t targetXid = 0;
                    std::uint32_t depth = 0;
                    bool nodeAlreadyResolved = false;
                };
                std::vector<DirectFsPendingNode> directPending;
                std::set<std::string> directSeenNodes;
                auto enqueueDirectNode = [&](const DirectFsPendingNode& n) {
                    if (n.nodeOid == 0) return;
                    const std::string key = std::to_string(n.volumeSequence) + ":" + std::to_string(n.nodeOid);
                    if (!directSeenNodes.insert(key).second) return;
                    directPending.push_back(n);
                };
                for (const auto& lookup : directVolumeRootTreeLookupRows) {
                    if (lookup.rootTreeStatus != "ROOT_TREE_BTREE_READ" || lookup.resolvedVirtualOffset == 0) continue;
                    DirectFsPendingNode pn;
                    pn.volumeSequence = lookup.volumeSequence;
                    pn.targetRole = lookup.targetRole;
                    pn.fsOid = lookup.fsOid;
                    pn.volumeName = lookup.volumeName;
                    pn.apfsRootTreeOid = lookup.apfsRootTreeOid;
                    pn.nodeOid = lookup.resolvedObjectOid ? lookup.resolvedObjectOid : lookup.apfsRootTreeOid;
                    pn.nodeVirtualOffset = lookup.resolvedVirtualOffset;
                    pn.targetXid = lookup.targetXid;
                    pn.depth = 0;
                    pn.nodeAlreadyResolved = true;
                    // Data volume first; other volumes are queued after it.
                    if (lookup.volumeName == "Data") directPending.insert(directPending.begin(), pn);
                    else enqueueDirectNode(pn);
                }

                constexpr std::size_t kDirectSpotlightDiagnosticNameSampleLimit = 200000U;
                std::vector<unsigned char> directFsNodeBuffer;
                directFsNodeBuffer.reserve(static_cast<std::size_t>(directBestNx.blockSize));
                for (std::size_t qi = 0; qi < directPending.size(); ++qi) {
                    const DirectFsPendingNode pending = directPending[qi];
                    std::vector<unsigned char>& node = directFsNodeBuffer;
                    node.clear();
                    std::string nodeErr;
                    std::uint64_t nodeOffset = pending.nodeVirtualOffset;
                    std::uint64_t nodeOid = pending.nodeOid;
                    long long nodeRead = -1;
                    if (pending.nodeAlreadyResolved) {
                        nodeRead = directReadVirtual(nodeOffset, directBestNx.blockSize, node, nodeErr);
                    } else {
                        const auto omIt = directVolumeOmapBySequence.find(pending.volumeSequence);
                        const auto rootIt = directVolumeOmapRootBySequence.find(pending.volumeSequence);
                        if (omIt == directVolumeOmapBySequence.end() || rootIt == directVolumeOmapRootBySequence.end()) continue;
                        const ApfsOmapTargetResolution resolved = aff4ResolveVolumeOmapTargetObjectForProbe(
                            omIt->second,
                            rootIt->second,
                            pending.nodeOid,
                            pending.targetXid,
                            directBestNx.blockSize,
                            directReadVirtual,
                            safeDirectNodeOffset,
                            "Direct AFF4/APFS bounded filesystem-tree scan",
                            directProbeCancelled);
                        if (resolved.objectStatus != "OMAP_TARGET_BTREE_READ" || resolved.resolvedBuffer.size() < 64U) continue;
                        node = resolved.resolvedBuffer;
                        nodeRead = resolved.resolvedBytesRead;
                        nodeOffset = resolved.resolvedVirtualOffset;
                        nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : pending.nodeOid;
                    }
                    if (nodeRead <= 0 || node.size() < 64U) continue;
                    ++directSpotlightScanMetrics.nodesVisited;
                    ++directSpotlightScanMetrics.nodesResolved;
                    const std::uint32_t rawType = readLe32(node, 24);
                    const std::string label = apfsObjectTypeLabel(rawType);
                    if (label != "BTREE" && label != "BTREE_NODE") continue;
                    const std::uint16_t btnLevel = readLe16(node, 34);
                    const std::uint32_t nkeys = std::min<std::uint32_t>(readLe32(node, 36), 65536U);
                    if (btnLevel == 0) ++directSpotlightScanMetrics.leafNodes;
                    else ++directSpotlightScanMetrics.branchNodes;
                    const std::uint32_t recordLimit = nkeys;
                    for (std::uint32_t i = 0; i < recordLimit; ++i) {
                        std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                        std::string detail;
                        if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                        ++directSpotlightScanMetrics.recordsScanned;
                        ApfsRootTreeRecordSampleRow rr;
                        rr.sequence = static_cast<std::uint32_t>(directSpotlightNameSampleRows.size() + directSpotlightTargetRows.size());
                        rr.volumeSequence = pending.volumeSequence;
                        rr.targetRole = pending.targetRole;
                        rr.fsOid = pending.fsOid;
                        rr.volumeName = pending.volumeName;
                        rr.apfsRootTreeOid = pending.apfsRootTreeOid;
                        rr.nodeOid = nodeOid;
                        rr.nodeVirtualOffset = nodeOffset;
                        rr.nodeLevel = btnLevel;
                        rr.nodeNkeys = nkeys;
                        rr.entryIndex = i;
                        rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                        rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                        rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                        rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                        rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                        if (keyLen >= 8U) {
                            rr.keyRaw = readLe64(node, keyAbs);
                            rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                            rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                            rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                        }
                        if (valLen >= 8U && valAbs + 8U <= node.size()) rr.valueU64_0 = readLe64(node, valAbs);
                        if (valLen >= 16U && valAbs + 16U <= node.size()) rr.valueU64_1 = readLe64(node, valAbs + 8U);
                        if (valLen >= 24U && valAbs + 24U <= node.size()) rr.valueU64_2 = readLe64(node, valAbs + 16U);
                        if (btnLevel > 0 && valLen >= 8U && valAbs + 8U <= node.size()) {
                            rr.branchChildOid = readLe64(node, valAbs);
                            if (rr.branchChildOid != 0) {
                                DirectFsPendingNode child;
                                child.volumeSequence = pending.volumeSequence;
                                child.targetRole = pending.targetRole;
                                child.fsOid = pending.fsOid;
                                child.volumeName = pending.volumeName;
                                child.apfsRootTreeOid = pending.apfsRootTreeOid;
                                child.nodeOid = rr.branchChildOid;
                                child.targetXid = pending.targetXid;
                                child.depth = pending.depth + 1U;
                                child.nodeAlreadyResolved = false;
                                enqueueDirectNode(child);
                                ++directSpotlightScanMetrics.branchCandidatesQueued;
                            }
                        }
                        if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                            const std::uint32_t nameLenAndHash = readLe32(node, keyAbs + 8U);
                            const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                            rr.decodedName = safePrintableUtf8Fragment(node, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                            if (!rr.decodedName.empty()) {
                                ++directSpotlightScanMetrics.dirRecordsDecoded;
                                if (rr.valueU64_0 != 0) {
                                    ApfsDirectoryRecordEntry de;
                                    de.volumeSequence = rr.volumeSequence;
                                    de.targetRole = rr.targetRole;
                                    de.fsOid = rr.fsOid;
                                    de.volumeName = rr.volumeName;
                                    de.parentObjectId = rr.keyObjectId;
                                    de.childFileId = rr.valueU64_0;
                                    de.name = rr.decodedName;
                                    directDirectoryRecordEntries.push_back(std::move(de));
                                }
                            }
                        }
                        if (btnLevel == 0 && rr.keyTypeRaw == 3U && valLen >= 16U && valAbs + 16U <= node.size()) {
                            ApfsSpotlightInodeProbeRow ir;
                            ir.sequence = static_cast<std::uint32_t>(directIndexedInodeByObject.size());
                            ir.volumeSequence = rr.volumeSequence;
                            ir.targetRole = rr.targetRole;
                            ir.fsOid = rr.fsOid;
                            ir.volumeName = rr.volumeName;
                            ir.inodeObjectId = rr.keyObjectId;
                            if (valLen >= 8U && valAbs + 8U <= node.size()) ir.inodeParentId = readLe64(node, valAbs + 0U);
                            if (valLen >= 16U && valAbs + 16U <= node.size()) ir.inodePrivateId = readLe64(node, valAbs + 8U);
                            if (valLen >= 24U && valAbs + 24U <= node.size()) ir.inodeCreateTimeRaw = readLe64(node, valAbs + 16U);
                            if (valLen >= 32U && valAbs + 32U <= node.size()) ir.inodeModTimeRaw = readLe64(node, valAbs + 24U);
                            if (valLen >= 40U && valAbs + 40U <= node.size()) ir.inodeChangeTimeRaw = readLe64(node, valAbs + 32U);
                            if (valLen >= 48U && valAbs + 48U <= node.size()) ir.inodeAccessTimeRaw = readLe64(node, valAbs + 40U);
                            if (valLen >= 56U && valAbs + 56U <= node.size()) ir.inodeInternalFlags = readLe64(node, valAbs + 48U);
                            if (valLen >= 60U && valAbs + 60U <= node.size()) ir.inodeNchildrenOrNlink = readLe32(node, valAbs + 56U);
                            if (valLen >= 84U && valAbs + 84U <= node.size()) ir.inodeModeCandidate = readLe16(node, valAbs + 82U);
                            if (valLen >= 92U && valAbs + 92U <= node.size()) ir.inodeUncompressedSize = readLe64(node, valAbs + 84U);
                            const ApfsInodeExtendedFieldDecode xf = decodeApfsInodeExtendedFieldsForProbe(node, valAbs, valLen);
                            ir.inodeXfieldStatus = xf.status;
                            if (xf.sawDstream) {
                                ir.inodeDstreamSize = xf.dstreamSize;
                                ir.inodeDstreamAllocedSize = xf.dstreamAllocedSize;
                                ir.inodeDstreamDefaultCryptoId = xf.dstreamDefaultCryptoId;
                            }
                            ir.nodeOid = nodeOid;
                            ir.nodeVirtualOffset = nodeOffset;
                            ir.nodeLevel = btnLevel;
                            ir.nodeNkeys = nkeys;
                            ir.entryIndex = i;
                            ir.inodeStatus = "DIRECT_INDEXED_INODE_RECORD";
                            ir.interpretation = "INODE record decoded during exhausted direct AFF4/APFS filesystem B-tree traversal and cached for target-guided Store-V2 copy-out.";
                            if (valLen > 0 && valAbs < node.size()) ir.valueSampleHex = hexSampleBytes(node.data() + valAbs, std::min<std::size_t>(std::min<std::size_t>(valLen, node.size() - valAbs), 96U));
                            ir.notes = detail;
                            directIndexedInodeByObject[std::make_pair(rr.volumeSequence, rr.keyObjectId)] = std::move(ir);
                        }
                        if (btnLevel == 0 && rr.keyTypeRaw == 8U && keyLen >= 16U && valLen >= 16U && valAbs + 16U <= node.size()) {
                            ApfsSpotlightFileExtentProbeRow er;
                            er.sequence = static_cast<std::uint32_t>(directIndexedFileExtentsByObject.size());
                            er.volumeSequence = rr.volumeSequence;
                            er.targetRole = rr.targetRole;
                            er.fsOid = rr.fsOid;
                            er.volumeName = rr.volumeName;
                            er.extentFileId = rr.keyObjectId;
                            er.extentLogicalOffset = readLe64(node, keyAbs + 8U);
                            er.lenAndFlags = readLe64(node, valAbs + 0U);
                            er.extentLengthBytes = er.lenAndFlags & 0x00ffffffffffffffULL;
                            er.extentFlags = static_cast<std::uint32_t>((er.lenAndFlags >> 56U) & 0xffU);
                            er.physicalBlock = readLe64(node, valAbs + 8U);
                            if (valLen >= 24U && valAbs + 24U <= node.size()) er.cryptoId = readLe64(node, valAbs + 16U);
                            if (directBestNx.blockSize != 0 && er.physicalBlock <= ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(directBestNx.blockSize))) {
                                er.physicalOffset = er.physicalBlock * static_cast<std::uint64_t>(directBestNx.blockSize);
                            }
                            er.nodeOid = nodeOid;
                            er.nodeVirtualOffset = nodeOffset;
                            er.nodeLevel = btnLevel;
                            er.nodeNkeys = nkeys;
                            er.entryIndex = i;
                            er.extentStatus = "DIRECT_INDEXED_FILE_EXTENT_RECORD";
                            er.interpretation = "FILE_EXTENT record decoded during exhausted direct AFF4/APFS filesystem B-tree traversal and cached for target-guided Store-V2 copy-out.";
                            er.notes = detail;
                            directIndexedFileExtentsByObject[std::make_pair(rr.volumeSequence, rr.keyObjectId)].push_back(std::move(er));
                        }
                        rr.keySampleHex = (keyLen > 0 && keyAbs < node.size()) ? hexSampleBytes(node.data() + keyAbs, std::min<std::size_t>(keyLen, 64U)) : std::string{};
                        rr.valueSampleHex = (valLen > 0 && valAbs < node.size()) ? hexSampleBytes(node.data() + valAbs, std::min<std::size_t>(valLen, 64U)) : std::string{};
                        rr.status = "DIRECT_AFF4_APFS_BTREE_SCAN_RECORD_DECODED";
                        rr.interpretation = btnLevel == 0 ? "Leaf filesystem-tree record decoded during direct AFF4/APFS bounded Spotlight target scan." : "Branch filesystem-tree separator record decoded during direct AFF4/APFS bounded Spotlight target scan.";
                        rr.notes = detail + "; direct_scan_depth=" + std::to_string(pending.depth);
                        if (!rr.decodedName.empty() && directSpotlightNameSampleRows.size() < kDirectSpotlightDiagnosticNameSampleLimit) directSpotlightNameSampleRows.push_back(rr);
                        if (btnLevel == 0 && directIsSpotlightTargetName(rr.decodedName)) {
                            directSpotlightTargetRows.push_back(rr);
                            appendDirectSpotlightCopyAttempt(rr);
                            ++directSpotlightScanMetrics.targetNameHits;
                        }
                    }
                }
                directSpotlightScanMetrics.nodesSkippedByLimit = 0;

                // V1.0.6: The direct AFF4/APFS path now walks the APFS root-tree queue to exhaustion
                // using the visited-node set as the cycle guard.  It also keeps directory-record rows
                // separately from the bounded diagnostic name-sample CSV, so recursive Store-V2 child
                // discovery is no longer blocked by the upload sample size.
                {
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<std::size_t>> childrenByParent;
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::string> nameByObject;
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::uint64_t> parentByObject;
                    for (std::size_t i = 0; i < directDirectoryRecordEntries.size(); ++i) {
                        const auto& e = directDirectoryRecordEntries[i];
                        childrenByParent[std::make_pair(e.volumeSequence, e.parentObjectId)].push_back(i);
                        if (e.childFileId != 0 && e.parentObjectId != 0) parentByObject[std::make_pair(e.volumeSequence, e.childFileId)] = e.parentObjectId;
                        if (e.childFileId != 0 && !e.name.empty()) nameByObject[std::make_pair(e.volumeSequence, e.childFileId)] = e.name;
                    }
                    auto directPathForObject = [&](std::uint32_t volumeSequence, std::uint64_t objectId) -> std::string {
                        std::vector<std::string> parts;
                        std::set<std::uint64_t> seen;
                        std::uint64_t cur = objectId;
                        while (cur != 0 && seen.insert(cur).second) {
                            const auto nameIt = nameByObject.find(std::make_pair(volumeSequence, cur));
                            if (nameIt != nameByObject.end() && !nameIt->second.empty()) parts.push_back(nameIt->second);
                            if (cur == 2) break;
                            const auto parentIt = parentByObject.find(std::make_pair(volumeSequence, cur));
                            if (parentIt == parentByObject.end() || parentIt->second == cur) break;
                            cur = parentIt->second;
                        }
                        std::string path = (volumeSequence == 4) ? "/System/Volumes/Data" : ("/vol_" + std::to_string(volumeSequence));
                        for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
                            if (it->empty() || *it == "/") continue;
                            if (path.empty() || path.back() != '/') path += "/";
                            path += *it;
                        }
                        return path;
                    };
                    std::set<std::tuple<std::uint32_t, std::uint64_t, std::uint64_t, std::string>> seenAttempts;
                    for (const auto& cr : directSpotlightCopyAttemptRows) {
                        seenAttempts.insert(std::make_tuple(cr.volumeSequence, cr.parentObjectId, cr.childFileId, asciiLower(cr.targetName)));
                    }
                    struct DirectStoreWalkItem {
                        std::uint32_t volumeSequence = 0;
                        std::uint64_t dirObjectId = 0;
                        std::uint64_t groupRootObjectId = 0;
                        std::string groupName;
                        std::string relPrefix;
                    };
                    std::vector<DirectStoreWalkItem> storeWalk;
                    std::set<std::pair<std::uint32_t, std::uint64_t>> queuedStoreDirs;
                    auto enqueueStoreDir = [&](std::uint32_t vol, std::uint64_t dir, std::uint64_t root, const std::string& group, const std::string& rel) {
                        if (dir == 0) return;
                        auto key = std::make_pair(vol, dir);
                        if (!childrenByParent.count(key)) return;
                        if (!queuedStoreDirs.insert(key).second) return;
                        DirectStoreWalkItem wi;
                        wi.volumeSequence = vol;
                        wi.dirObjectId = dir;
                        wi.groupRootObjectId = root ? root : dir;
                        wi.groupName = group;
                        wi.relPrefix = rel;
                        storeWalk.push_back(std::move(wi));
                    };
                    auto directGroupNameForDir = [&](std::uint32_t vol, std::uint64_t dir) -> std::string {
                        const auto it = nameByObject.find(std::make_pair(vol, dir));
                        if (it == nameByObject.end()) return {};
                        return isLikelyStoreV2GroupDirectoryName(it->second) ? it->second : std::string{};
                    };
                    for (const auto& e : directDirectoryRecordEntries) {
                        const std::string lname = asciiLower(e.name);
                        if (lname == "store-v2") enqueueStoreDir(e.volumeSequence, e.childFileId, e.childFileId, "", "");
                        if (directIsSpotlightStoreV2TopLevelComponentName(lname)) enqueueStoreDir(e.volumeSequence, e.parentObjectId, e.parentObjectId, directGroupNameForDir(e.volumeSequence, e.parentObjectId), "");
                    }
                    for (std::size_t qi = 0; qi < storeWalk.size(); ++qi) {
                        const auto item = storeWalk[qi];
                        const auto childIt = childrenByParent.find(std::make_pair(item.volumeSequence, item.dirObjectId));
                        if (childIt == childrenByParent.end()) continue;
                        for (const std::size_t idx : childIt->second) {
                            const auto& e = directDirectoryRecordEntries[idx];
                            const std::string lname = asciiLower(e.name);
                            if (lname == ".spotlight-v100" || lname == "store-v2") continue;
                            const bool childIsDir = childrenByParent.count(std::make_pair(e.volumeSequence, e.childFileId)) != 0;
                            std::uint64_t groupRoot = item.groupRootObjectId;
                            std::string groupName = item.groupName;
                            std::string relPrefix = item.relPrefix;
                            if (groupName.empty() && childIsDir && isLikelyStoreV2GroupDirectoryName(e.name)) {
                                groupRoot = e.childFileId;
                                groupName = e.name;
                                relPrefix.clear();
                            }
                            const std::string relPath = relPrefix.empty() ? e.name : (relPrefix + "/" + e.name);
                            if (childIsDir) { enqueueStoreDir(e.volumeSequence, e.childFileId, groupRoot, groupName, relPath); continue; }
                            const auto dedupe = std::make_tuple(e.volumeSequence, e.parentObjectId, e.childFileId, lname);
                            if (!seenAttempts.insert(dedupe).second) continue;
                            ApfsSpotlightCopyAttemptRow cr;
                            cr.sequence = static_cast<std::uint32_t>(directSpotlightCopyAttemptRows.size());
                            cr.volumeSequence = e.volumeSequence;
                            cr.targetRole = e.targetRole;
                            cr.fsOid = e.fsOid;
                            cr.volumeName = e.volumeName;
                            cr.parentObjectId = e.parentObjectId;
                            cr.childFileId = e.childFileId;
                            cr.targetName = e.name;
                            cr.targetKind = directSpotlightTargetKind(e.name);
                            cr.storeV2RootObjectId = groupRoot;
                            cr.storeV2GroupName = groupName;
                            cr.storeV2RelativePath = relPath;
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_FILE_EXTENTS_PENDING";
                            cr.interpretation = "Direct AFF4/APFS Store-V2 recursive namespace row. This ordinary-named file is under a Store-V2 directory/group and is eligible for target-guided inode/extent lookup.";
                            cr.notes = "direct_aff4_v1_0_4_recursive_storev2_seed=1; group_root_object_id=" + std::to_string(groupRoot) + "; group_name=" + groupName + "; rel_path=" + relPath + "; apfs_absolute_path=" + directPathForObject(e.volumeSequence, e.childFileId);
                            directSpotlightCopyAttemptRows.push_back(std::move(cr));
                        }
                    }
                }

                // V1.0.6: Direct target-guided record correlation and guarded copy-out.
                // The previous direct path found Store-V2 names but did not attach those child IDs to
                // INODE/FILE_EXTENT rows.  Because the exhausted root-tree traversal above now indexes
                // leaf INODE and FILE_EXTENT records, copy attempts can be materialized without another
                // broad APFS scan.  This remains conservative: only ordered, readable extents are staged,
                // and sparse/zero regions are explicitly recorded.
                {
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<std::size_t>> childrenByParent;
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::string> nameByObject;
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::uint64_t> parentByObject;
                    for (std::size_t i = 0; i < directDirectoryRecordEntries.size(); ++i) {
                        const auto& e = directDirectoryRecordEntries[i];
                        childrenByParent[std::make_pair(e.volumeSequence, e.parentObjectId)].push_back(i);
                        if (e.childFileId != 0 && e.parentObjectId != 0) parentByObject[std::make_pair(e.volumeSequence, e.childFileId)] = e.parentObjectId;
                        if (e.childFileId != 0 && !e.name.empty()) nameByObject[std::make_pair(e.volumeSequence, e.childFileId)] = e.name;
                    }
                    auto directPathForObject = [&](std::uint32_t volumeSequence, std::uint64_t objectId) -> std::string {
                        std::vector<std::string> parts;
                        std::set<std::uint64_t> seen;
                        std::uint64_t cur = objectId;
                        while (cur != 0 && seen.insert(cur).second) {
                            const auto nameIt = nameByObject.find(std::make_pair(volumeSequence, cur));
                            if (nameIt != nameByObject.end() && !nameIt->second.empty()) parts.push_back(nameIt->second);
                            if (cur == 2) break;
                            const auto parentIt = parentByObject.find(std::make_pair(volumeSequence, cur));
                            if (parentIt == parentByObject.end() || parentIt->second == cur) break;
                            cur = parentIt->second;
                        }
                        std::string path = (volumeSequence == 4) ? "/System/Volumes/Data" : ("/vol_" + std::to_string(volumeSequence));
                        for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
                            if (it->empty() || *it == "/") continue;
                            if (path.empty() || path.back() != '/') path += "/";
                            path += *it;
                        }
                        return path;
                    };
                    auto directPreviewStatusForBytes = [](const std::vector<unsigned char>& bytes) -> std::string {
                        if (bytes.size() >= 16U && std::memcmp(bytes.data(), "SQLite format 3", 15U) == 0) return "SQLITE_HEADER";
                        if (bytes.size() >= 6U && std::memcmp(bytes.data(), "bplist", 6U) == 0) return "BPLIST_HEADER";
                        bool anyNonZero = false;
                        for (unsigned char b : bytes) { if (b != 0U) { anyNonZero = true; break; } }
                        return anyNonZero ? "NONZERO_BYTES" : "ALL_ZERO_BYTES";
                    };
                    std::set<std::uint32_t> inodeMaterializedForTarget;
                    std::map<std::uint32_t, std::uint64_t> directPrivateIdByTarget;
                    for (const auto& cr : directSpotlightCopyAttemptRows) {
                        if (cr.childFileId == 0) continue;
                        const auto inodeIt = directIndexedInodeByObject.find(std::make_pair(cr.volumeSequence, cr.childFileId));
                        if (inodeIt == directIndexedInodeByObject.end()) continue;
                        ApfsSpotlightInodeProbeRow ir = inodeIt->second;
                        ir.sequence = static_cast<std::uint32_t>(directSpotlightInodeRows.size());
                        ir.targetSequence = cr.sequence;
                        ir.targetParentObjectId = cr.parentObjectId;
                        ir.targetChildFileId = cr.childFileId;
                        ir.targetName = cr.targetName;
                        ir.targetKind = cr.targetKind;
                        ir.inodeStatus = (ir.inodeParentId == cr.parentObjectId || cr.parentObjectId == 0) ? "TARGET_INODE_DIRECT_INDEX_HIT" : "TARGET_INODE_DIRECT_INDEX_PARENT_MISMATCH";
                        ir.interpretation = "Direct AFF4/APFS exhausted filesystem-tree index matched this Store-V2 directory-entry child ID to an INODE record.";
                        ir.notes += (ir.notes.empty() ? std::string{} : "; ") + std::string("direct_aff4_v1_0_5_indexed_inode=1; apfs_absolute_path=") + directPathForObject(cr.volumeSequence, cr.childFileId);
                        directPrivateIdByTarget[cr.sequence] = ir.inodePrivateId;
                        if (inodeMaterializedForTarget.insert(cr.sequence).second) directSpotlightInodeRows.push_back(std::move(ir));
                    }

                    auto directObjectCandidatesForExtent = [&](const ApfsSpotlightCopyAttemptRow& cr) {
                        std::vector<std::pair<std::uint64_t, std::string>> out;
                        std::set<std::uint64_t> seen;
                        auto add = [&](std::uint64_t v, const std::string& label) {
                            if (v == 0) return;
                            if (seen.insert(v).second) out.push_back(std::make_pair(v, label));
                        };
                        add(cr.childFileId, "child_file_id");
                        const auto pIt = directPrivateIdByTarget.find(cr.sequence);
                        const std::uint64_t privateId = (pIt == directPrivateIdByTarget.end()) ? 0ULL : pIt->second;
                        add(privateId, "inode_private_id_dstream_candidate");
                        if (privateId != 0) add(privateId >> 4U, "inode_private_id_shifted_right_4");
                        const std::uint64_t shiftedChild = cr.childFileId >> 4U;
                        add(shiftedChild, "child_file_id_shifted_right_4");
                        for (std::uint64_t prefix = 1; prefix <= 15; ++prefix) add((prefix << 56U) | shiftedChild, "high_prefix_plus_child_shifted_right_4");
                        return out;
                    };

                    std::map<std::uint32_t, std::vector<ApfsSpotlightFileExtentProbeRow>> extentsByTargetSequence;
                    std::set<std::tuple<std::uint32_t, std::uint64_t, std::uint64_t, std::uint64_t>> seenTargetExtents;
                    for (const auto& cr : directSpotlightCopyAttemptRows) {
                        if (cr.childFileId == 0) continue;
                        if (cr.targetKind == "SPOTLIGHT_ROOT_DIRECTORY" || cr.targetKind == "SPOTLIGHT_STORE_V2_DIRECTORY" || cr.targetKind == "IOS_CORESPOTLIGHT_NAME") continue;
                        for (const auto& cand : directObjectCandidatesForExtent(cr)) {
                            const auto erIt = directIndexedFileExtentsByObject.find(std::make_pair(cr.volumeSequence, cand.first));
                            if (erIt == directIndexedFileExtentsByObject.end()) continue;
                            for (const auto& er0 : erIt->second) {
                                const auto key = std::make_tuple(cr.sequence, er0.extentFileId, er0.extentLogicalOffset, er0.physicalBlock);
                                if (!seenTargetExtents.insert(key).second) continue;
                                ApfsSpotlightFileExtentProbeRow er = er0;
                                er.sequence = static_cast<std::uint32_t>(directSpotlightFileExtentRows.size());
                                er.targetSequence = cr.sequence;
                                er.targetParentObjectId = cr.parentObjectId;
                                er.targetChildFileId = cr.childFileId;
                                er.targetName = cr.targetName;
                                er.targetKind = cr.targetKind;
                                er.extentStatus = "TARGET_FILE_EXTENT_DIRECT_INDEX_HIT";
                                er.interpretation = "Direct AFF4/APFS exhausted filesystem-tree index matched this Store-V2 target to a FILE_EXTENT record.";
                                er.notes += (er.notes.empty() ? std::string{} : "; ") + std::string("candidate_source=") + cand.second + "; candidate_object_id=" + std::to_string(cand.first) + "; original_extent_object_id=" + std::to_string(er0.extentFileId);
                                if (er.extentLogicalOffset == 0 && er.physicalOffset != 0 && er.extentLengthBytes > 0) {
                                    const std::uint64_t previewLen = std::min<std::uint64_t>(4096ULL, er.extentLengthBytes);
                                    std::vector<unsigned char> preview;
                                    std::string previewErr;
                                    er.previewBytesRead = directReadVirtual(er.physicalOffset, previewLen, preview, previewErr);
                                    if (er.previewBytesRead > 0) {
                                        er.previewStatus = directPreviewStatusForBytes(preview);
                                        er.previewSampleHex = hexSampleBytes(preview.data(), preview.size() < 96 ? preview.size() : 96);
                                    } else {
                                        er.previewStatus = "PREVIEW_READ_FAILED";
                                        er.notes += previewErr.empty() ? "; preview_read_failed" : ("; " + previewErr);
                                    }
                                } else {
                                    er.previewStatus = "PREVIEW_NOT_LOGICAL_ZERO_EXTENT";
                                }
                                extentsByTargetSequence[cr.sequence].push_back(er);
                                directSpotlightFileExtentRows.push_back(er);
                            }
                        }
                    }

                    auto safeStageComponent = [](const std::string& in) -> std::string {
                        std::string out;
                        for (char ch : in) {
                            const unsigned char c = static_cast<unsigned char>(ch);
                            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || ch == '.' || ch == '_' || ch == '-') out.push_back(ch);
                            else out.push_back('_');
                            if (out.size() >= 180U) break;
                        }
                        if (out.empty() || out == "." || out == "..") out = "unnamed";
                        return out;
                    };
                    auto safeRelativeStorePath = [&](const ApfsSpotlightCopyAttemptRow& cr) -> fs::path {
                        fs::path rel;
                        std::string raw = cr.storeV2RelativePath.empty() ? cr.targetName : cr.storeV2RelativePath;
                        std::replace(raw.begin(), raw.end(), '\\', '/');
                        std::stringstream ss(raw);
                        std::string part;
                        while (std::getline(ss, part, '/')) {
                            if (part.empty() || part == "." || part == "..") continue;
                            rel /= safeStageComponent(part);
                        }
                        if (rel.empty()) rel = safeStageComponent(std::to_string(cr.sequence) + "_fid_" + std::to_string(cr.childFileId) + "_" + cr.targetName);
                        return rel;
                    };
                    const std::uint64_t kDirectMaxSingleCopyOutBytes = opt.pressureTestMode ? (std::numeric_limits<std::uint64_t>::max)() : (512ULL * 1024ULL * 1024ULL);
                    const fs::path directStageRoot = caseDir / "ExtractedSpotlight" / "StagedStoreV2";
                    for (const auto& cr : directSpotlightCopyAttemptRows) {
                        const auto extIt = extentsByTargetSequence.find(cr.sequence);
                        if (extIt == extentsByTargetSequence.end() || extIt->second.empty()) continue;
                        std::vector<ApfsSpotlightFileExtentProbeRow> extents = extIt->second;
                        std::sort(extents.begin(), extents.end(), [](const auto& a, const auto& b) {
                            if (a.extentLogicalOffset != b.extentLogicalOffset) return a.extentLogicalOffset < b.extentLogicalOffset;
                            return a.physicalOffset < b.physicalOffset;
                        });
                        std::uint64_t expectedEnd = 0;
                        bool overlap = false;
                        bool invalidLength = false;
                        bool hasSparseGap = false;
                        bool hasZeroPhysical = false;
                        for (const auto& er : extents) {
                            if (er.extentLengthBytes == 0 || er.extentLengthBytes > kDirectMaxSingleCopyOutBytes) invalidLength = true;
                            if (er.extentLogicalOffset < expectedEnd) overlap = true;
                            if (er.extentLogicalOffset > expectedEnd) hasSparseGap = true;
                            if (er.physicalBlock == 0 || er.physicalOffset == 0) hasZeroPhysical = true;
                            const std::uint64_t eEnd = er.extentLogicalOffset + er.extentLengthBytes;
                            if (eEnd < er.extentLogicalOffset) invalidLength = true;
                            expectedEnd = std::max<std::uint64_t>(expectedEnd, eEnd);
                        }
                        std::uint64_t directLogicalSize = expectedEnd;
                        std::string directLogicalSizeSource = "direct_indexed_file_extent_end";
                        const auto inodeForLogicalIt = directIndexedInodeByObject.find(std::make_pair(cr.volumeSequence, cr.childFileId));
                        if (inodeForLogicalIt != directIndexedInodeByObject.end()) {
                            const auto& indexedInode = inodeForLogicalIt->second;
                            if (indexedInode.inodeDstreamSize != 0 && indexedInode.inodeDstreamSize <= expectedEnd) {
                                directLogicalSize = indexedInode.inodeDstreamSize;
                                directLogicalSizeSource = "INO_EXT_TYPE_DSTREAM.size.direct_index";
                            } else if (indexedInode.inodeUncompressedSize != 0 && indexedInode.inodeUncompressedSize <= expectedEnd) {
                                directLogicalSize = indexedInode.inodeUncompressedSize;
                                directLogicalSizeSource = "j_inode_val.uncompressed_size.direct_index";
                            }
                        }
                        ApfsSpotlightFileCopyOutRow row;
                        row.sequence = static_cast<std::uint32_t>(directSpotlightFileCopyOutRows.size());
                        row.targetSequence = cr.sequence;
                        row.volumeSequence = cr.volumeSequence;
                        row.targetRole = cr.targetRole;
                        row.fsOid = cr.fsOid;
                        row.volumeName = cr.volumeName;
                        row.targetParentObjectId = cr.parentObjectId;
                        row.targetChildFileId = cr.childFileId;
                        row.targetName = cr.targetName;
                        row.targetKind = cr.targetKind;
                        row.storeV2RootObjectId = cr.storeV2RootObjectId;
                        row.storeV2GroupName = cr.storeV2GroupName;
                        row.storeV2RelativePath = cr.storeV2RelativePath.empty() ? pathString(safeRelativeStorePath(cr)) : cr.storeV2RelativePath;
                        row.extentCount = static_cast<std::uint32_t>(extents.size());
                        row.assembledBytes = expectedEnd;
                        row.logicalSizeBytes = directLogicalSize;
                        row.logicalSizeSource = directLogicalSizeSource;
                        row.firstPhysicalOffset = extents.empty() ? 0ULL : extents.front().physicalOffset;
                        if (overlap) { row.copyStatus = "SKIPPED_OVERLAPPING_OR_OUT_OF_ORDER_EXTENTS"; row.validationStatus = "OVERLAP_ORDER_GATE_FAILED"; row.notes = "Direct indexed extents overlapped; skipped to avoid shifted output."; directSpotlightFileCopyOutRows.push_back(row); continue; }
                        if (invalidLength || expectedEnd == 0 || directLogicalSize == 0 || expectedEnd > kDirectMaxSingleCopyOutBytes || directLogicalSize > kDirectMaxSingleCopyOutBytes) { row.copyStatus = "SKIPPED_SIZE_LIMIT_OR_INVALID_LENGTH"; row.validationStatus = "SIZE_GATE_FAILED"; row.notes = "Direct indexed extent chain was empty, invalid, or above per-file safety cap."; directSpotlightFileCopyOutRows.push_back(row); continue; }
                        // V1.0.18: write raw APFS copy-out rows to a unique per-target folder.
                        // Earlier builds wrote many duplicate Store-V2 component names into
                        // ExtractedSpotlight/StagedStoreV2/Ungrouped/<name>.  Later rows could
                        // overwrite the file that an earlier, higher-scored staging row referenced,
                        // causing the stage CSV to report the selected row size/hash while the actual
                        // staged file came from the last duplicate writer.  Keep copy-out provenance
                        // separate from the normalized StagedStoreV2 tree so stage selection copies
                        // immutable per-row sources.
                        const fs::path directCopyOutRoot = caseDir / "ExtractedSpotlight" / "ApfsCopyOutByTarget";
                        const std::string rawGroupLabel = safeStageComponent(cr.storeV2GroupName.empty() ? "Ungrouped" : cr.storeV2GroupName);
                        const fs::path groupDir = directCopyOutRoot / ("seq_" + std::to_string(cr.sequence) + "_fid_" + std::to_string(cr.childFileId) + "_parent_" + std::to_string(cr.parentObjectId) + "_" + rawGroupLabel);
                        const fs::path outPath = groupDir / safeRelativeStorePath(cr);
                        std::error_code mkEc;
                        fs::create_directories(outPath.parent_path(), mkEc);
                        if (mkEc) { row.copyStatus = "COPY_FAILED_CREATE_DIRECTORY"; row.validationStatus = "OUTPUT_DIRECTORY_FAILED"; row.notes = mkEc.message(); directSpotlightFileCopyOutRows.push_back(row); continue; }
                        EvidenceBinaryWriter outFile(outPath);
                        if (!outFile) { row.copyStatus = "COPY_FAILED_OPEN_OUTPUT"; row.validationStatus = "OUTPUT_OPEN_FAILED"; row.notes = outFile.error(); directSpotlightFileCopyOutRows.push_back(row); continue; }
                        std::vector<unsigned char> firstBytes;
                        std::uint64_t logicalCursor = 0;
                        std::uint64_t syntheticZeroBytes = 0;
                        bool copyOk = true;
                        std::string copyNotes;
                        auto writeZeros = [&](std::uint64_t count, const std::string& reason) -> bool {
                            std::vector<unsigned char> z(std::min<std::uint64_t>(count, 1024ULL * 1024ULL), 0U);
                            std::uint64_t left = count;
                            while (left != 0) {
                                const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(left, z.size()));
                                if (firstBytes.empty()) firstBytes.assign(z.begin(), z.begin() + std::min<std::size_t>(chunk, 96U));
                                if (!outFile.write(z.data(), chunk)) { copyNotes = "zero-fill write failed: " + reason + "; " + outFile.error(); return false; }
                                left -= chunk;
                            }
                            syntheticZeroBytes += count;
                            return true;
                        };
                        for (const auto& er : extents) {
                            if (logicalCursor >= directLogicalSize) break;
                            if (er.extentLogicalOffset > logicalCursor) {
                                const std::uint64_t gapRaw = er.extentLogicalOffset - logicalCursor;
                                const std::uint64_t gap = std::min<std::uint64_t>(gapRaw, directLogicalSize - logicalCursor);
                                if (!writeZeros(gap, "logical_sparse_gap")) { copyOk = false; break; }
                                logicalCursor += gap;
                                if (logicalCursor >= directLogicalSize) break;
                            }
                            const std::uint64_t writableExtentBytes = std::min<std::uint64_t>(er.extentLengthBytes, directLogicalSize - logicalCursor);
                            if (writableExtentBytes == 0) break;
                            if (er.physicalBlock == 0 || er.physicalOffset == 0) {
                                if (!writeZeros(writableExtentBytes, "zero_physical_extent")) { copyOk = false; break; }
                                logicalCursor += writableExtentBytes;
                                continue;
                            }
                            std::vector<unsigned char> extentBytes;
                            std::string readErr;
                            const long long got = directReadVirtual(er.physicalOffset, writableExtentBytes, extentBytes, readErr);
                            if (got < 0 || static_cast<std::uint64_t>(got) != writableExtentBytes || extentBytes.size() != writableExtentBytes) { copyOk = false; copyNotes = readErr.empty() ? "extent read failed or returned short data" : readErr; break; }
                            if (firstBytes.empty()) firstBytes.assign(extentBytes.begin(), extentBytes.begin() + std::min<std::size_t>(extentBytes.size(), 96U));
                            if (!outFile.write(extentBytes)) { copyOk = false; copyNotes = "extent write failed; " + outFile.error(); break; }
                            logicalCursor += writableExtentBytes;
                        }
                        if (copyOk && logicalCursor != directLogicalSize) {
                            copyOk = false;
                            copyNotes = "logical output short after bounded direct copy; wrote=" + std::to_string(logicalCursor) + "; expected=" + std::to_string(directLogicalSize);
                        }
                        if (!outFile.close()) { copyOk = false; copyNotes = copyNotes.empty() ? outFile.error() : (copyNotes + "; " + outFile.error()); }
                        row.outputPath = pathString(outPath);
                        row.outputRelativePath = pathString(fs::relative(outPath, caseDir));
                        if (!firstBytes.empty()) { row.firstBytesStatus = directPreviewStatusForBytes(firstBytes); row.firstBytesHex = hexSampleBytes(firstBytes.data(), firstBytes.size()); }
                        else row.firstBytesStatus = "NO_PREVIEW";
                        if (!copyOk) {
                            row.copyStatus = "COPY_FAILED_READ_OR_WRITE";
                            row.validationStatus = "FAILED_DURING_COPY";
                            row.notes = copyNotes;
                            std::error_code rmEc; fs::remove(outPath, rmEc);
                        } else {
                            std::error_code sizeEc;
                            const auto sz = fs::file_size(outPath, sizeEc);
                            row.outputSizeBytes = sizeEc ? 0ULL : static_cast<std::uint64_t>(sz);
                            if (opt.pressureTestMode) { row.outputSha256 = "SKIPPED_BY_PRESSURE_TEST_MODE"; } else { try { row.outputSha256 = sha256File(outPath); } catch (const std::exception& ex) { row.notes = std::string("sha256_failed: ") + ex.what(); } }
                            if (syntheticZeroBytes != 0 || hasSparseGap || hasZeroPhysical) {
                                row.copyStatus = "COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS";
                                row.validationStatus = (row.outputSizeBytes == directLogicalSize) ? "SIZE_MATCH_WITH_ZERO_FILL_PROVENANCE" : "SIZE_MISMATCH_AFTER_ZERO_FILL_COPY";
                                row.notes += (row.notes.empty() ? std::string{} : "; ") + (std::string("direct_aff4_v1_0_16_copy_out=1; logical_size_source=") + directLogicalSizeSource + "; synthetic_zero_bytes=") + std::to_string(syntheticZeroBytes) + "; sparse_gap=" + (hasSparseGap ? "true" : "false") + "; zero_physical_extent=" + (hasZeroPhysical ? "true" : "false") + "; apfs_absolute_path=" + directPathForObject(cr.volumeSequence, cr.childFileId);
                                row.interpretation = "Direct AFF4/APFS indexed FILE_EXTENT rows were staged with explicit zero-fill provenance for sparse or zero-physical regions.";
                            } else {
                                row.copyStatus = "COPIED_DIRECT_INDEXED_EXTENT_CHAIN";
                                row.validationStatus = (row.outputSizeBytes == directLogicalSize && directLogicalSize < expectedEnd) ? "TRIMMED_TO_INODE_LOGICAL_SIZE" : ((row.outputSizeBytes == expectedEnd) ? "SIZE_MATCHES_EXTENT_CHAIN" : "SIZE_MISMATCH_AFTER_COPY");
                                row.notes += (row.notes.empty() ? std::string{} : "; ") + (std::string("direct_aff4_v1_0_16_copy_out=1; logical_size_source=") + directLogicalSizeSource + "; apfs_absolute_path=") + directPathForObject(cr.volumeSequence, cr.childFileId);
                                row.interpretation = "Direct AFF4/APFS indexed FILE_EXTENT rows were assembled into a staged Store-V2 copy-out file.";
                            }
                        }
                        directSpotlightFileCopyOutRows.push_back(row);
                    }

                    const fs::path walkCsv = caseDir / "aff4_apfs_logical_directory_walk.csv";
                    const fs::path walkJson = caseDir / "aff4_apfs_logical_directory_walk_summary.json";
                    try {
                        std::ofstream out(walkCsv, std::ios::binary);
                        out << "source_id,input_path,input_type,sequence,volume_sequence,volume_name,parent_object_id,child_file_id,name,walk_role,apfs_absolute_path,notes\n";
                        std::size_t seq = 0;
                        for (const auto& e : directDirectoryRecordEntries) {
                            const std::string lower = asciiLower(e.name);
                            std::string role;
                            if (lower == ".spotlight-v100") role = "SPOTLIGHT_ROOT_DIRECTORY";
                            else if (lower == "store-v2") role = "STORE_V2_DIRECTORY";
                            else if (childrenByParent.count(std::make_pair(e.volumeSequence, e.parentObjectId)) && !directPathForObject(e.volumeSequence, e.childFileId).empty() && directPathForObject(e.volumeSequence, e.childFileId).find("Store-V2") != std::string::npos) role = "STORE_V2_DESCENDANT";
                            if (role.empty()) continue;
                            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                                << seq++ << ',' << e.volumeSequence << ',' << csvEscape(e.volumeName) << ',' << e.parentObjectId << ',' << e.childFileId << ','
                                << csvEscape(e.name) << ',' << csvEscape(role) << ',' << csvEscape(directPathForObject(e.volumeSequence, e.childFileId)) << ','
                                << csvEscape("logical_namespace_walk_from_exhausted_apfs_btree_records") << "\n";
                        }
                    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_logical_directory_walk.csv: ") + ex.what()); }
                    try {
                        std::ofstream out(walkJson, std::ios::binary);
                        out << "{\n";
                        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
                        out << "  \"app_version\": \"" << appVersion() << "\",\n";
                        out << "  \"indexed_inode_records\": " << directIndexedInodeByObject.size() << ",\n";
                        std::size_t extentRecordCount = 0; for (const auto& kv : directIndexedFileExtentsByObject) extentRecordCount += kv.second.size();
                        out << "  \"indexed_file_extent_records\": " << extentRecordCount << ",\n";
                        out << "  \"materialized_target_inode_rows\": " << directSpotlightInodeRows.size() << ",\n";
                        out << "  \"materialized_target_file_extent_rows\": " << directSpotlightFileExtentRows.size() << ",\n";
                        out << "  \"copy_out_rows\": " << directSpotlightFileCopyOutRows.size() << "\n";
                        out << "}\n";
                    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_logical_directory_walk_summary.json: ") + ex.what()); }
                }

                const std::uint32_t descBlockCount = directBestNx.xpDescBlocks & ~(1U << 31);
                const std::uint32_t descToRead = std::min<std::uint32_t>(descBlockCount, 256U);
                for (std::uint32_t i = 0; i < descToRead; ++i) {
                    const std::uint64_t physicalBlock = directBestNx.xpDescBase + static_cast<std::uint64_t>(i);
                    if (physicalBlock > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(directBestNx.blockSize))) break;
                    const std::uint64_t vo = physicalBlock * static_cast<std::uint64_t>(directBestNx.blockSize);
                    std::vector<unsigned char> buf;
                    std::string readErr;
                    const bool ok = readVirtualBytes(vo, directBestNx.blockSize, buf, readErr);

                    ApfsCheckpointDescriptorRow dr;
                    dr.sequence = static_cast<std::uint32_t>(directDescriptorRows.size() + 1U);
                    dr.physicalBlock = physicalBlock;
                    dr.virtualOffset = vo;
                    dr.bytesRead = ok ? static_cast<long long>(buf.size()) : -1;
                    dr.status = ok ? "READ_OK" : "READ_FAILED";
                    dr.notes = readErr.empty() ? "Direct sorted AFF4 virtual read of NX checkpoint descriptor block." : readErr;
                    if (ok && buf.size() >= 36U) {
                        dr.oid = readLe64(buf, 8);
                        dr.xid = readLe64(buf, 16);
                        dr.objectTypeRaw = readLe32(buf, 24);
                        dr.objectSubtype = readLe32(buf, 28);
                        dr.objectTypeLabel = apfsObjectTypeLabel(dr.objectTypeRaw);
                        dr.magic.assign(reinterpret_cast<const char*>(buf.data() + 32), reinterpret_cast<const char*>(buf.data() + 36));
                        dr.sampleHex = hexSampleBytes(buf.data(), std::min<std::size_t>(buf.size(), 96U));
                        if (dr.magic == "NXSB") dr.interpretation = "Checkpoint descriptor block contains an APFS container superblock copy.";
                        else if (dr.magic == "APSB") dr.interpretation = "Checkpoint descriptor block contains an APFS volume superblock.";
                        else if (dr.objectTypeLabel == "CHECKPOINT_MAP") dr.interpretation = "Checkpoint descriptor block contains an APFS checkpoint map.";
                        else dr.interpretation = "Checkpoint descriptor block read; object type/magic retained for APFS resolution.";
                    } else {
                        dr.interpretation = "Direct read failed for checkpoint descriptor block.";
                    }
                    directDescriptorRows.push_back(dr);

                    if (!(ok && buf.size() >= 80U && dr.objectTypeLabel == "CHECKPOINT_MAP")) continue;
                    const std::uint32_t checkpointFlags = readLe32(buf, 32);
                    const std::uint32_t checkpointCount = std::min<std::uint32_t>(readLe32(buf, 36), 2048U);
                    for (std::uint32_t entryIndex = 0; entryIndex < checkpointCount; ++entryIndex) {
                        const std::size_t entryOff = 40U + static_cast<std::size_t>(entryIndex) * 40U;
                        if (entryOff + 40U > buf.size()) break;
                        ApfsCheckpointMapEntryRow mr;
                        mr.sequence = static_cast<std::uint32_t>(directCheckpointMapRows.size() + 1U);
                        mr.entryIndex = entryIndex;
                        mr.checkpointBlock = physicalBlock;
                        mr.checkpointVirtualOffset = vo;
                        mr.checkpointBytesRead = dr.bytesRead;
                        mr.checkpointFlags = checkpointFlags;
                        mr.checkpointCount = checkpointCount;
                        mr.cpmTypeRaw = readLe32(buf, entryOff + 0U);
                        mr.cpmSubtype = readLe32(buf, entryOff + 4U);
                        mr.cpmSize = readLe32(buf, entryOff + 8U);
                        mr.cpmFsOid = readLe64(buf, entryOff + 16U);
                        mr.cpmOid = readLe64(buf, entryOff + 24U);
                        mr.cpmPaddr = readLe64(buf, entryOff + 32U);
                        mr.cpmTypeLabel = apfsObjectTypeLabel(mr.cpmTypeRaw);
                        if (mr.cpmOid == directBestNx.omapOid) mr.targetRole = "NX_OBJECT_MAP";
                        else if (containsU64(directBestNx.fsOids, mr.cpmOid)) mr.targetRole = "NX_FILESYSTEM_OID";
                        else if (mr.cpmFsOid != 0) mr.targetRole = "VOLUME_SCOPED_OBJECT";
                        else mr.targetRole = "CHECKPOINT_OBJECT";
                        mr.interpretation = "Checkpoint mapping parsed from direct AFF4 APFS descriptor ring.";
                        mr.notes = "Direct sorted AFF4 APFS checkpoint map entry.";
                        directCheckpointMapRows.push_back(mr);

                        if (directCheckpointObjectRows.size() >= 512U || mr.cpmPaddr == 0) continue;
                        if (mr.targetRole != "NX_OBJECT_MAP" && mr.targetRole != "NX_FILESYSTEM_OID" && mr.cpmTypeLabel != "OBJECT_MAP" && mr.cpmTypeLabel != "FS") continue;
                        appendMappedObjectProbe(entryIndex, mr.cpmOid, mr.cpmFsOid, mr.cpmPaddr, mr.targetRole, "Checkpoint-mapped object read through direct sorted AFF4 reader.", directBestNx.nextXid, 0, mr.cpmSize);
                    }
                }
            };

            std::set<std::uint64_t> seenChunks;
            const std::size_t maxMapEntriesToScan = std::min<std::size_t>(mapEntries.size(), 50000U);
            for (std::size_t scanIndex = 0; scanIndex < maxMapEntriesToScan && alignedApfsHits < 50U; ++scanIndex) {
                if (directProbeCancelled()) return;
                if ((scanIndex % 1000U) == 0U) {
                    directHeartbeat("aff4_direct_map_chunk_scan", "scan_entry=" + std::to_string(scanIndex) + " of " + std::to_string(maxMapEntriesToScan) + "; chunks_decoded=" + std::to_string(chunksDecoded) + "; apfs_hits=" + std::to_string(alignedApfsHits), 31);
                }
                const auto& scanEntry = mapEntries[scanIndex];
                const std::uint64_t virtualOffset = scanEntry.virtualOffset;
                const std::uint64_t length = scanEntry.length;
                const std::uint64_t streamOffset = scanEntry.streamOffset;
                const std::uint64_t originalMapEntryIndex = scanEntry.mapEntryIndex;
                ++mapEntriesScanned;
                const std::uint64_t startChunk = streamOffset / 32768ULL;
                const std::uint64_t endChunk = (streamOffset + length - 1ULL) / 32768ULL;
                for (std::uint64_t chunk = startChunk; chunk <= endChunk && alignedApfsHits < 50U; ++chunk) {
                    if (directProbeCancelled()) return;
                    if (!seenChunks.insert(chunk).second) continue;
                    std::vector<unsigned char> dec;
                    bool usedLz4 = false;
                    if (!decodeImageChunk(chunk, dec, err, usedLz4)) {
                        add("direct_lz4_decode", "DECODE_FAILED", virtualOffset, streamOffset, originalMapEntryIndex, chunk, -1, {}, {}, err);
                        continue;
                    }
                    if (usedLz4) ++lz4ChunksDecoded;
                    ++chunksDecoded;
                    if ((chunksDecoded % 250U) == 0U) {
                        directHeartbeat("aff4_direct_map_chunks_decoded", "chunks_decoded=" + std::to_string(chunksDecoded) + "; lz4_chunks=" + std::to_string(lz4ChunksDecoded) + "; apfs_hits=" + std::to_string(alignedApfsHits), 32);
                    }
                    if (chunksDecoded == 1U) {
                        add("direct_first_chunk_decode", "DECODE_OK", virtualOffset, streamOffset, originalMapEntryIndex, chunk, static_cast<long long>(dec.size()), {}, hexSampleBytes(dec.data(), std::min<std::size_t>(dec.size(), 64U)), "First AFF4 image-stream chunk decoded directly from map/index/data ZIP members after sorting the AFF4 map by APFS virtual offset.");
                    }
                    if (directSignatureHits < 100U) {
                        const std::vector<std::pair<std::string, std::string>> signatures = {
                            {"NXSB", "APFS_CONTAINER_SUPERBLOCK_MAGIC"},
                            {"APSB", "APFS_VOLUME_SUPERBLOCK_MAGIC"},
                            {".Spotlight-V100", "MACOS_SPOTLIGHT_ROOT_STRING"},
                            {"Store-V2", "MACOS_SPOTLIGHT_STOREV2_STRING"},
                            {"SQLite format 3", "SQLITE_HEADER_STRING"},
                            {"CoreSpotlight", "IOS_CORESPOTLIGHT_STRING"}
                        };
                        for (const auto& sig : signatures) {
                            if (directSignatureHits >= 100U) break;
                            const auto it = std::search(dec.begin(), dec.end(), sig.first.begin(), sig.first.end());
                            if (it == dec.end()) continue;
                            const std::size_t pSig = static_cast<std::size_t>(std::distance(dec.begin(), it));
                            const std::int64_t chunkVirtualStartSigned = static_cast<std::int64_t>(virtualOffset) + static_cast<std::int64_t>(chunk * 32768ULL) - static_cast<std::int64_t>(streamOffset);
                            if (chunkVirtualStartSigned < 0) continue;
                            const std::uint64_t hitVirtual = static_cast<std::uint64_t>(chunkVirtualStartSigned) + static_cast<std::uint64_t>(pSig);
                            const std::size_t sampleStart = pSig >= 32U ? pSig - 32U : pSig;
                            const std::size_t sampleLen = std::min<std::size_t>(128U, dec.size() - sampleStart);
                            ++directSignatureHits;
                            add("direct_decoded_signature_scan", "SIGNATURE_FOUND", hitVirtual, streamOffset, originalMapEntryIndex, chunk, static_cast<long long>(dec.size()), sig.second, hexSampleBytes(dec.data() + sampleStart, sampleLen), "Signature found by scanning directly decoded AFF4 image-stream chunk bytes after sorting AFF4 map entries by APFS virtual offset.");
                            if (sig.second == "SQLITE_HEADER_STRING") {
                                carveSqliteCandidate(hitVirtual, originalMapEntryIndex, chunk);
                            }
                        }
                    }
                    const std::int64_t chunkVirtualStartSigned = static_cast<std::int64_t>(virtualOffset) + static_cast<std::int64_t>(chunk * 32768ULL) - static_cast<std::int64_t>(streamOffset);
                    if (chunkVirtualStartSigned < 0) continue;
                    const std::uint64_t chunkVirtualStart = static_cast<std::uint64_t>(chunkVirtualStartSigned);
                    std::size_t p = 0;
                    const std::uint64_t mod = chunkVirtualStart % 4096ULL;
                    if (mod <= 32ULL) p = static_cast<std::size_t>(32ULL - mod);
                    else p = static_cast<std::size_t>(4096ULL - (mod - 32ULL));
                    for (; p + 4U <= dec.size(); p += 4096U) {
                        std::string magic;
                        if (std::memcmp(dec.data() + p, "NXSB", 4U) == 0) magic = "NXSB";
                        else if (std::memcmp(dec.data() + p, "APSB", 4U) == 0) magic = "APSB";
                        if (magic.empty()) continue;
                        const std::uint64_t hitVirtual = chunkVirtualStart + static_cast<std::uint64_t>(p);
                        const std::size_t sampleStart = p >= 32U ? p - 32U : p;
                        const std::size_t sampleLen = std::min<std::size_t>(96U, dec.size() - sampleStart);
                        ++alignedApfsHits;
                        add("direct_aligned_apfs_magic_scan", "APFS_MAGIC_FOUND", hitVirtual, streamOffset, originalMapEntryIndex, chunk, static_cast<long long>(dec.size()), magic, hexSampleBytes(dec.data() + sampleStart, sampleLen), "APFS object magic found at +32 within a directly decoded AFF4 image-stream chunk after sorting AFF4 map entries by APFS virtual offset.");
                        if (magic == "NXSB") {
                            rememberNxCandidateFromDecodedChunk(dec, p, hitVirtual - 32ULL, originalMapEntryIndex, chunk);
                        }
                    }
                }
            }
            probeDirectApfsFromBestNx();
            appendRunProgress(caseDir, 34, "aff4_direct_map_reader_probe_complete", "map_entries_scanned=" + std::to_string(mapEntriesScanned) + "; chunks_decoded=" + std::to_string(chunksDecoded) + "; apfs_hits=" + std::to_string(alignedApfsHits));
            finalStatus = chunksDecoded > 0U ? "DIRECT_MAP_READER_SMOKE_OK" : "DIRECT_MAP_READER_NO_CHUNKS_DECODED";
            finalNotes = "scan_entries=" + std::to_string(maxMapEntriesToScan) + "; map_entries_total=" + std::to_string(mapEntryCount);
        }
    } catch (const std::exception& ex) {
        finalStatus = "DIRECT_MAP_READER_EXCEPTION";
        finalNotes = ex.what();
        add("direct_map_reader_probe", finalStatus, 0, 0, 0, 0, -1, {}, {}, finalNotes);
    }

    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,step,status,virtual_offset,stream_offset,map_entry_index,chunk_index,bytes_read,magic,sample_hex,notes\n";
        for (const auto& r : probeRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << csvEscape(r.step) << ',' << csvEscape(r.status) << ',' << r.virtualOffset << ',' << r.streamOffset << ','
                << r.mapEntryIndex << ',' << r.chunkIndex << ',' << r.bytesRead << ',' << csvEscape(r.magic) << ','
                << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_direct_map_reader_probe.csv: ") + ex.what());
    }
    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"status\": \"" << jsonEscape(finalStatus) << "\",\n";
        out << "  \"map_entries_total\": " << mapEntryCount << ",\n";
        out << "  \"map_entries_scanned\": " << mapEntriesScanned << ",\n";
        out << "  \"chunks_decoded\": " << chunksDecoded << ",\n";
        out << "  \"lz4_chunks_decoded\": " << lz4ChunksDecoded << ",\n";
        out << "  \"aligned_apfs_magic_hits\": " << alignedApfsHits << ",\n";
        out << "  \"direct_signature_hits\": " << directSignatureHits << ",\n";
        out << "  \"sqlite_candidates_found\": " << sqliteCandidatesFound << ",\n";
        out << "  \"sqlite_candidates_carved\": " << sqliteCandidatesCarved << ",\n";
        out << "  \"sqlite_candidates_opened_with_tables\": " << sqliteCandidatesOpened << ",\n";
        out << "  \"direct_apfs_nxsb_found\": " << (directBestNx.found ? "true" : "false") << ",\n";
        out << "  \"direct_apfs_best_nx_xid\": " << directBestNx.xid << ",\n";
        out << "  \"direct_apfs_checkpoint_descriptor_rows\": " << directDescriptorRows.size() << ",\n";
        out << "  \"direct_apfs_checkpoint_map_entries\": " << directCheckpointMapRows.size() << ",\n";
        out << "  \"direct_apfs_checkpoint_mapped_object_rows\": " << directCheckpointObjectRows.size() << ",\n";
        out << "  \"direct_apfs_resolved_volume_rows\": " << directResolvedVolumeRows.size() << ",\n";
        out << "  \"direct_apfs_volume_omap_rows\": " << directVolumeOmapRows.size() << ",\n";
        out << "  \"direct_apfs_volume_root_tree_lookup_rows\": " << directVolumeRootTreeLookupRows.size() << ",\n";
        out << "  \"direct_apfs_root_tree_node_rows\": " << directRootTreeNodeRows.size() << ",\n";
        out << "  \"direct_apfs_root_tree_record_sample_rows\": " << directRootTreeRecordRows.size() << ",\n";
        out << "  \"direct_apfs_spotlight_nodes_visited\": " << directSpotlightScanMetrics.nodesVisited << ",\n";
        out << "  \"direct_apfs_spotlight_records_scanned\": " << directSpotlightScanMetrics.recordsScanned << ",\n";
        out << "  \"direct_apfs_spotlight_target_hits\": " << directSpotlightTargetRows.size() << ",\n";
        out << "  \"direct_apfs_spotlight_name_samples\": " << directSpotlightNameSampleRows.size() << ",\n";
        out << "  \"direct_apfs_spotlight_copy_attempt_rows\": " << directSpotlightCopyAttemptRows.size() << ",\n";
        out << "  \"notes\": \"" << jsonEscape(finalNotes) << "\"\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_direct_map_reader_probe_summary.json: ") + ex.what());
    }
    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 Direct Map Reader Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "This probe reads BlackBag/LZ4 AFF4 map/index/data ZIP members directly and avoids `AFF4_open`.\n\n";
        out << "## Summary\n\n";
        out << "- Status: `" << finalStatus << "`\n";
        out << "- Map entries total: `" << mapEntryCount << "`\n";
        out << "- Map entries scanned: `" << mapEntriesScanned << "`\n";
        out << "- Chunks decoded: `" << chunksDecoded << "`\n";
        out << "- LZ4 chunks decoded: `" << lz4ChunksDecoded << "`\n";
        out << "- Aligned APFS magic hits: `" << alignedApfsHits << "`\n\n";
        out << "- Direct decoded signature hits: `" << directSignatureHits << "`\n\n";
        out << "- SQLite candidates found: `" << sqliteCandidatesFound << "`\n";
        out << "- SQLite candidates carved: `" << sqliteCandidatesCarved << "`\n";
        out << "- SQLite candidates opened with tables: `" << sqliteCandidatesOpened << "`\n\n";
        out << "- Direct APFS NXSB found: `" << (directBestNx.found ? "true" : "false") << "`\n";
        out << "- Direct APFS checkpoint descriptor rows: `" << directDescriptorRows.size() << "`\n";
        out << "- Direct APFS checkpoint map entries: `" << directCheckpointMapRows.size() << "`\n";
        out << "- Direct APFS mapped object probes: `" << directCheckpointObjectRows.size() << "`\n\n";
        out << "- Direct APFS resolved volume rows: `" << directResolvedVolumeRows.size() << "`\n";
        out << "- Direct APFS volume OMAP rows: `" << directVolumeOmapRows.size() << "`\n";
        out << "- Direct APFS volume root-tree lookups: `" << directVolumeRootTreeLookupRows.size() << "`\n";
        out << "- Direct APFS root-tree node rows: `" << directRootTreeNodeRows.size() << "`\n";
        out << "- Direct APFS Spotlight scan nodes visited: `" << directSpotlightScanMetrics.nodesVisited << "`\n";
        out << "- Direct APFS Spotlight scan records scanned: `" << directSpotlightScanMetrics.recordsScanned << "`\n";
        out << "- Direct APFS Spotlight target hits: `" << directSpotlightTargetRows.size() << "`\n\n";
        out << "A successful chunk decode proves the native direct AFF4 reader path can reconstruct virtual image bytes without the blocking libaff4 open call. The sorted-map APFS path now parses container metadata, resolves APFS volume object maps, probes filesystem root-tree records, and emits bounded Spotlight target-scan outputs. Copy-out remains gated until targeted path, inode, xattr, and file-extent provenance is complete.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_DIRECT_MAP_READER_PROBE.md: ") + ex.what());
    }
    try {
        std::ofstream out(sqliteCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,status,virtual_offset,map_entry_index,chunk_index,page_size,db_pages,requested_bytes,carved_bytes,output_relative_path,sqlite_open_status,sqlite_master_status,table_count,table_names,sample_hex,notes\n";
        for (const auto& r : sqliteCandidateRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << csvEscape(r.status) << ',' << r.virtualOffset << ',' << r.mapEntryIndex << ','
                << r.chunkIndex << ',' << r.pageSize << ',' << r.dbPages << ',' << r.requestedBytes << ',' << r.carvedBytes << ','
                << csvEscape(r.outputRelativePath) << ',' << csvEscape(r.sqliteOpenStatus) << ',' << csvEscape(r.sqliteMasterStatus) << ','
                << r.tableCount << ',' << csvEscape(r.tableNames) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_direct_sqlite_candidate_carve.csv: ") + ex.what());
    }
    try {
        std::ofstream out(sqliteJsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"candidate_directory\": \"" << jsonEscape(pathString(sqliteCandidateDir)) << "\",\n";
        out << "  \"sqlite_candidates_found\": " << sqliteCandidatesFound << ",\n";
        out << "  \"sqlite_candidates_carved\": " << sqliteCandidatesCarved << ",\n";
        out << "  \"sqlite_candidates_opened_with_tables\": " << sqliteCandidatesOpened << ",\n";
        out << "  \"candidate_rows\": " << sqliteCandidateRows.size() << ",\n";
        out << "  \"max_candidates\": 12,\n";
        out << "  \"max_bytes_per_candidate\": " << (8ULL * 1024ULL * 1024ULL) << "\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_direct_sqlite_candidate_carve_summary.json: ") + ex.what());
    }
    try {
        std::ofstream out(sqliteMdPath, std::ios::binary);
        out << "# AFF4 Direct SQLite Candidate Carve\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "This diagnostic carves bounded SQLite candidates from decoded AFF4 virtual offsets where `SQLite format 3` headers were found. It is a controlled bridge around the current APFS traversal blocker; candidates are provenance-linked and capped.\n\n";
        out << "## Summary\n\n";
        out << "- Candidates found: `" << sqliteCandidatesFound << "`\n";
        out << "- Candidates carved: `" << sqliteCandidatesCarved << "`\n";
        out << "- Candidates opened with table names: `" << sqliteCandidatesOpened << "`\n";
        out << "- Candidate directory: `Aff4DirectSqliteCandidates`\n\n";
        out << "Rows with `SQLITE_MASTER_OK` and table names should be used as the next parse targets. Rows that open but report malformed/truncated data indicate the SQLite header is real but the file is fragmented or the bounded carve needs APFS file extents.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_DIRECT_SQLITE_CANDIDATE_CARVE.md: ") + ex.what());
    }
    if (directBestNx.attempted || directBestNx.found) {
        writeAff4ApfsDirectoryRecordNameIndexOutputs(caseDir, source, originalInput, directDirectoryRecordEntries, log);

        const bool writeHeavyApfsDiagnostics = shouldWriteAff4ApfsStructuralDiagnostics(opt.verbose, opt.diagnosticFullNativeDb, opt.aff4ApfsDiagnosticOutputs);
        if (writeHeavyApfsDiagnostics) {
            log.info("AFF4/APFS diagnostic output mode enabled: writing structural probe CSV outputs.");
            writeAff4ApfsContainerViewOutputs(caseDir, source, originalInput, directBestNx, directDescriptorRows, log);
            writeAff4ApfsVolumeSuperblockOutputs(caseDir, source, originalInput, directBestNx, directVolumeRows, log);
            writeAff4ApfsCheckpointMapOutputs(caseDir, source, originalInput, directBestNx, directCheckpointMapRows, directCheckpointObjectRows, log);
            writeAff4ApfsResolvedVolumeOutputs(caseDir, source, originalInput, directBestNx, directResolvedVolumeRows, directVolumeOmapRows, directVolumeRootTreeLookupRows, true, log);
            writeAff4ApfsVolumeRootTreeLookupOutputs(caseDir, source, originalInput, directVolumeRootTreeLookupRows, true, log);
            writeAff4ApfsRootTreeNodeProbeOutputs(caseDir, source, originalInput, directRootTreeNodeRows, directRootTreeRecordRows, true, log);
            writeAff4ApfsFilesystemNamespaceSeedOutputs(caseDir, source, originalInput, directRootTreeRecordRows, std::vector<ApfsRootTreeRecordSampleRow>{}, std::vector<ApfsRootTreeRecordSampleRow>{}, true, log);
            writeAff4ApfsSpotlightTargetScanOutputs(caseDir, source, originalInput, directSpotlightTargetRows, directSpotlightNameSampleRows, directSpotlightCopyAttemptRows, directSpotlightScanMetrics, true, log);
            writeAff4ApfsSpotlightInodeProbeOutputs(caseDir, source, originalInput, directSpotlightInodeRows, true, log);
            writeAff4ApfsSpotlightXattrProbeOutputs(caseDir, source, originalInput, directSpotlightXattrRows, directSpotlightCopyAttemptRows, true, log);
            writeAff4ApfsSpotlightFileExtentProbeOutputs(caseDir, source, originalInput, directSpotlightFileExtentRows, true, log);
        } else {
            log.info("Normal AFF4/APFS source-probe mode: structural diagnostic CSV outputs suppressed; writing copy-out/stage outputs only.");
            appendRunStatus(caseDir, aff4ApfsStructuralDiagnosticsSuppressedStatus(), aff4ApfsStructuralDiagnosticsSuppressedGuidance());
        }
        // Copy-out and staging outputs remain enabled in normal mode because they
        // describe actual extracted evidence and feed the external comparison.
        writeAff4ApfsSpotlightFileCopyOutOutputs(caseDir, source, originalInput, directSpotlightFileCopyOutRows, true, log);
        writeAff4ApfsExtractedStoreV2StageOutputs(caseDir, source, originalInput, directSpotlightFileCopyOutRows, true, log);
    }
    log.info("AFF4 direct map reader probe written: " + pathString(csvPath));
}




void Aff4ProbeWorker::executeDynamicLoadProbe(const fs::path& caseDir,
                                      const EvidenceSource& source,
                                      const RunOptions& opt,
                                      const fs::path& originalInput,
                                      const std::atomic_bool* cancelToken,
                                      Logger& log) {
    if (!isAff4SourcePath(originalInput)) return;
    appendRunProgress(caseDir, 29, "aff4_dynamic_load_probe_entry", "AFF4 dynamic probe entered; direct-map guard may route to direct reader");
    auto dynamicProbeCancelled = [&]() -> bool {
        if (cancelToken && cancelToken->load()) {
            appendRunStatus(caseDir, "aff4_dynamic_load_probe_cancelled", "cancel requested by investigator");
            log.warn("AFF4 dynamic-load probe cancelled by investigator.");
            return true;
        }
        return false;
    };
    if (dynamicProbeCancelled()) return;
    std::vector<Aff4DynamicProbeRow> rows;
    struct Aff4VirtualApfsProbeRow {
        std::string step;
        std::string status;
        std::uint64_t offset = 0;
        long long bytesRead = -1;
        std::string signature;
        std::string confidence;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };
    std::vector<Aff4VirtualApfsProbeRow> apfsRows;
    ApfsNxSuperblockSummary nxSummary;
    std::vector<ApfsCheckpointDescriptorRow> apfsDescriptorRows;
    std::vector<ApfsVolumeSuperblockRow> apfsVolumeRows;
    std::vector<ApfsCheckpointMapEntryRow> apfsCheckpointMapRows;
    std::vector<ApfsCheckpointMappedObjectProbeRow> apfsCheckpointMappedObjectRows;

    struct ApfsObjectIdProbeRow {
        std::string role;
        std::uint64_t oid = 0;
        std::uint64_t virtualOffset = 0;
        long long bytesRead = -1;
        std::uint64_t objectOid = 0;
        std::uint64_t objectXid = 0;
        std::uint32_t objectTypeRaw = 0;
        std::uint32_t objectSubtype = 0;
        std::string objectTypeLabel;
        std::string magic;
        std::string status;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };
    struct ApfsBtreeNodeProbeRow {
        std::string sourceRole;
        std::uint64_t sourceOid = 0;
        std::uint64_t sourcePaddr = 0;
        std::uint64_t virtualOffset = 0;
        long long bytesRead = -1;
        std::uint64_t objectOid = 0;
        std::uint64_t objectXid = 0;
        std::uint32_t objectTypeRaw = 0;
        std::uint32_t objectSubtype = 0;
        std::string objectTypeLabel;
        std::uint16_t btnFlags = 0;
        std::uint16_t btnLevel = 0;
        std::uint32_t btnNkeys = 0;
        std::uint16_t tableSpaceOffset = 0;
        std::uint16_t tableSpaceLength = 0;
        std::uint16_t freeSpaceOffset = 0;
        std::uint16_t freeSpaceLength = 0;
        std::string status;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };

    struct ApfsOmapPhysProbeRow {
        std::string sourceRole;
        std::uint64_t sourceOid = 0;
        std::uint64_t virtualOffset = 0;
        long long bytesRead = -1;
        std::uint64_t objectOid = 0;
        std::uint64_t objectXid = 0;
        std::uint32_t objectTypeRaw = 0;
        std::string objectTypeLabel;
        std::uint32_t objectSubtype = 0;
        std::uint32_t omFlags = 0;
        std::uint32_t omSnapshotCount = 0;
        std::uint32_t omTreeType = 0;
        std::uint32_t omSnapshotTreeType = 0;
        std::uint64_t omTreeOid = 0;
        std::uint64_t omSnapshotTreeOid = 0;
        std::uint64_t omMostRecentSnap = 0;
        std::uint64_t omPendingRevertMin = 0;
        std::uint64_t omPendingRevertMax = 0;
        std::string status;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };

    struct ApfsOmapBtreeRootProbeRow {
        std::string sourceRole;
        std::uint64_t omapOid = 0;
        std::uint64_t omTreeOid = 0;
        std::uint64_t virtualOffset = 0;
        long long bytesRead = -1;
        std::uint64_t objectOid = 0;
        std::uint64_t objectXid = 0;
        std::uint32_t objectTypeRaw = 0;
        std::string objectTypeLabel;
        std::uint32_t objectSubtype = 0;
        std::uint16_t btnFlags = 0;
        std::uint16_t btnLevel = 0;
        std::uint32_t btnNkeys = 0;
        std::uint16_t tableSpaceOffset = 0;
        std::uint16_t tableSpaceLength = 0;
        std::uint16_t freeSpaceOffset = 0;
        std::uint16_t freeSpaceLength = 0;
        std::uint32_t btFlagsCandidate = 0;
        std::uint32_t btNodeSizeCandidate = 0;
        std::uint32_t btKeySizeCandidate = 0;
        std::uint32_t btValSizeCandidate = 0;
        std::string status;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };

    struct ApfsOmapLookupProbeRow {
        std::string targetRole;
        std::uint64_t targetOid = 0;
        std::uint64_t targetXid = 0;
        std::uint64_t omapOid = 0;
        std::uint64_t omTreeOid = 0;
        std::uint16_t rootLevel = 0;
        std::uint32_t rootNkeys = 0;
        std::string status;
        std::string interpretation;
        std::string notes;
    };

    struct ApfsOmapBtreeTocProbeRow {
        std::string sourceRole;
        std::uint64_t omapOid = 0;
        std::uint64_t omTreeOid = 0;
        std::uint64_t virtualOffset = 0;
        std::uint16_t rootLevel = 0;
        std::uint32_t rootNkeys = 0;
        std::uint32_t entryIndex = 0;
        std::uint32_t tocOffset = 0;
        std::uint16_t keyOffsetCandidate = 0;
        std::uint16_t keyLengthCandidate = 0;
        std::uint16_t valueOffsetCandidate = 0;
        std::uint16_t valueLengthCandidate = 0;
        std::string keySampleHex;
        std::string valueSampleHex;
        std::string status;
        std::string interpretation;
        std::string notes;
    };

    struct ApfsOmapLeafKvDecodeRow {
        std::string sourceRole;
        std::uint64_t omapOid = 0;
        std::uint64_t omTreeOid = 0;
        std::uint64_t virtualOffset = 0;
        std::uint16_t rootLevel = 0;
        std::uint32_t rootNkeys = 0;
        std::uint32_t entryIndex = 0;
        std::uint32_t tocOffset = 0;
        std::uint16_t keyOffset = 0;
        std::uint16_t valueOffset = 0;
        std::uint64_t keyOid = 0;
        std::uint64_t keyXid = 0;
        std::uint32_t valueFlags = 0;
        std::uint32_t valueSize = 0;
        std::uint64_t valuePaddr = 0;
        std::uint64_t resolvedVirtualOffset = 0;
        long long resolvedBytesRead = -1;
        std::uint64_t resolvedObjectOid = 0;
        std::uint64_t resolvedObjectXid = 0;
        std::uint32_t resolvedObjectTypeRaw = 0;
        std::string resolvedObjectTypeLabel;
        std::uint32_t resolvedObjectSubtype = 0;
        std::string resolvedMagic;
        std::string targetRole;
        std::string lookupStatus;
        std::string status;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };
    std::vector<ApfsObjectIdProbeRow> apfsObjectIdProbeRows;
    std::vector<ApfsBtreeNodeProbeRow> apfsBtreeNodeProbeRows;
    std::vector<ApfsOmapPhysProbeRow> apfsOmapPhysProbeRows;
    std::vector<ApfsOmapBtreeRootProbeRow> apfsOmapBtreeRootProbeRows;
    std::vector<ApfsOmapLookupProbeRow> apfsOmapLookupProbeRows;
    std::vector<ApfsOmapBtreeTocProbeRow> apfsOmapBtreeTocProbeRows;
    std::vector<ApfsOmapLeafKvDecodeRow> apfsOmapLeafKvDecodeRows;
    std::vector<ApfsResolvedVolumeSuperblockRow> apfsResolvedVolumeSuperblockRows;
    std::vector<ApfsVolumeOmapProbeRow> apfsVolumeOmapProbeRows;
    std::vector<ApfsVolumeRootTreeLookupRow> apfsVolumeRootTreeLookupRows;
    std::vector<ApfsRootTreeNodeProbeRow> apfsRootTreeNodeProbeRows;
    std::vector<ApfsRootTreeRecordSampleRow> apfsRootTreeRecordSampleRows;
    std::vector<ApfsRootTreeChildNodeProbeRow> apfsRootTreeChildNodeProbeRows;
    std::vector<ApfsRootTreeRecordSampleRow> apfsRootTreeChildRecordSampleRows;
    std::vector<ApfsRootTreeChildNodeProbeRow> apfsRootTreeDescendantNodeProbeRows;
    std::vector<ApfsRootTreeRecordSampleRow> apfsRootTreeDescendantRecordSampleRows;
    std::vector<ApfsRootTreeRecordSampleRow> apfsSpotlightTargetScanRows;
    std::vector<ApfsRootTreeRecordSampleRow> apfsSpotlightNameScanSampleRows;
    std::vector<ApfsDirectoryRecordEntry> apfsDirectoryRecordEntries;
    std::vector<ApfsSpotlightCopyAttemptRow> apfsSpotlightCopyAttemptRows;
    std::vector<ApfsSpotlightInodeProbeRow> apfsSpotlightInodeProbeRows;
    std::vector<ApfsSpotlightFileExtentProbeRow> apfsSpotlightFileExtentProbeRows;
    std::vector<ApfsSpotlightXattrProbeRow> apfsSpotlightXattrProbeRows;
    std::vector<ApfsSpotlightFileCopyOutRow> apfsSpotlightFileCopyOutRows;
    ApfsSpotlightTargetScanMetrics apfsSpotlightTargetScanMetrics;
    std::map<std::uint32_t, ApfsVolumeOmapProbeRow> apfsVolumeOmapRowsByVolumeSequence;
    std::map<std::uint32_t, std::vector<unsigned char>> apfsVolumeOmapRootNodesByVolumeSequence;

    std::uint64_t finalObjectSize = 0;
    long long finalBlockSize = -1;
    bool gptFound = false;
    std::size_t partitionCount = 0;
    std::size_t apfsPartitionCount = 0;
    std::size_t nxsbHitCount = 0;
    std::size_t virtualReadCount = 0;

    auto addRow = [&](const std::string& step,
                      const std::string& status,
                      const fs::path& p,
                      std::uint64_t objectSize,
                      std::uint64_t offset,
                      long long bytesRead,
                      const std::string& sampleHex,
                      const std::string& notes) {
        Aff4DynamicProbeRow r;
        r.step = step;
        r.status = status;
        r.path = p;
        r.objectSize = objectSize;
        r.offset = offset;
        r.bytesRead = bytesRead;
        r.sampleHex = sampleHex;
        r.notes = notes;
        rows.push_back(r);
    };
    auto addApfsRow = [&](const std::string& step,
                          const std::string& status,
                          std::uint64_t offset,
                          long long bytesRead,
                          const std::string& signature,
                          const std::string& confidence,
                          const std::string& interpretation,
                          const std::string& sampleHex,
                          const std::string& notes) {
        Aff4VirtualApfsProbeRow r;
        r.step = step;
        r.status = status;
        r.offset = offset;
        r.bytesRead = bytesRead;
        r.signature = signature;
        r.confidence = confidence;
        r.interpretation = interpretation;
        r.sampleHex = sampleHex;
        r.notes = notes;
        apfsRows.push_back(r);
    };

    const fs::path csvPath = caseDir / "aff4_cpp_lite_dynamic_load_probe.csv";
    const fs::path planPath = caseDir / "AFF4_CPP_LITE_DYNAMIC_LOAD_PROBE.md";
    const fs::path apfsCsvPath = caseDir / "aff4_virtual_apfs_probe.csv";
    const fs::path apfsJsonPath = caseDir / "aff4_virtual_apfs_probe_summary.json";
    const fs::path apfsPlanPath = caseDir / "AFF4_VIRTUAL_APFS_PROBE.md";
    const fs::path apfsObjectIdCsvPath = caseDir / "aff4_apfs_object_id_probe.csv";
    const fs::path apfsBtreeCsvPath = caseDir / "aff4_apfs_btree_node_probe.csv";
    const fs::path apfsObjectResolutionJsonPath = caseDir / "aff4_apfs_object_resolution_probe_summary.json";
    const fs::path apfsObjectResolutionMdPath = caseDir / "AFF4_APFS_OBJECT_RESOLUTION_PROBE.md";
    const fs::path apfsOmapPhysCsvPath = caseDir / "aff4_apfs_omap_phys_probe.csv";
    const fs::path apfsOmapBtreeRootCsvPath = caseDir / "aff4_apfs_omap_btree_root_probe.csv";
    const fs::path apfsOmapLookupCsvPath = caseDir / "aff4_apfs_omap_lookup_probe.csv";
    const fs::path apfsOmapBtreeTocCsvPath = caseDir / "aff4_apfs_omap_btree_toc_probe.csv";
    const fs::path apfsOmapSummaryJsonPath = caseDir / "aff4_apfs_omap_probe_summary.json";
    const fs::path apfsOmapLeafKvCsvPath = caseDir / "aff4_apfs_omap_leaf_kv_decode.csv";
    const fs::path apfsOmapLeafLookupCsvPath = caseDir / "aff4_apfs_omap_leaf_lookup_results.csv";
    const fs::path apfsOmapLeafKvMdPath = caseDir / "AFF4_APFS_OMAP_LEAF_KV_DECODE.md";
    const fs::path apfsOmapTocMdPath = caseDir / "AFF4_APFS_OMAP_TOC_PROBE.md";
    const fs::path apfsOmapMdPath = caseDir / "AFF4_APFS_OMAP_PROBE.md";

    auto writeObjectResolutionOutputs = [&]() {
        std::size_t readOk = 0, apsbHits = 0, omapHits = 0, btreeRows = 0, btreeNodeRows = 0;
        std::size_t omapPhysRows = apfsOmapPhysProbeRows.size();
        std::size_t omapBtreeRows = apfsOmapBtreeRootProbeRows.size();
        std::size_t omapLookupRows = apfsOmapLookupProbeRows.size();
        for (const auto& r : apfsObjectIdProbeRows) {
            if (r.bytesRead > 0) ++readOk;
            if (r.magic == "APSB") ++apsbHits;
            if (r.magic == "OMAP") ++omapHits;
        }
        for (const auto& r : apfsBtreeNodeProbeRows) {
            if (r.objectTypeLabel == "BTREE") ++btreeRows;
            if (r.objectTypeLabel == "BTREE_NODE") ++btreeNodeRows;
        }
        try {
            std::ofstream out(apfsObjectIdCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,role,oid,virtual_offset,bytes_read,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,magic,status,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsObjectIdProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.role) << ',' << r.oid << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                    << r.objectOid << ',' << r.objectXid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ','
                    << csvEscape(r.magic) << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_object_id_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsBtreeCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,source_role,source_oid,source_paddr,virtual_offset,bytes_read,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,btn_flags,btn_level,btn_nkeys,table_space_offset,table_space_length,free_space_offset,free_space_length,status,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsBtreeNodeProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.sourceRole) << ',' << r.sourceOid << ',' << r.sourcePaddr << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                    << r.objectOid << ',' << r.objectXid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ','
                    << r.btnFlags << ',' << r.btnLevel << ',' << r.btnNkeys << ',' << r.tableSpaceOffset << ',' << r.tableSpaceLength << ','
                    << r.freeSpaceOffset << ',' << r.freeSpaceLength << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_btree_node_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapPhysCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,source_role,source_oid,virtual_offset,bytes_read,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,om_flags,om_snapshot_count,om_tree_type,om_snapshot_tree_type,om_tree_oid,om_snapshot_tree_oid,om_most_recent_snap,om_pending_revert_min,om_pending_revert_max,status,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsOmapPhysProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.sourceRole) << ',' << r.sourceOid << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                    << r.objectOid << ',' << r.objectXid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ','
                    << r.omFlags << ',' << r.omSnapshotCount << ',' << r.omTreeType << ',' << r.omSnapshotTreeType << ','
                    << r.omTreeOid << ',' << r.omSnapshotTreeOid << ',' << r.omMostRecentSnap << ',' << r.omPendingRevertMin << ',' << r.omPendingRevertMax << ','
                    << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_phys_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapBtreeRootCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,source_role,omap_oid,om_tree_oid,virtual_offset,bytes_read,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,btn_flags,btn_level,btn_nkeys,table_space_offset,table_space_length,free_space_offset,free_space_length,btree_info_flags_candidate,btree_info_node_size_candidate,btree_info_key_size_candidate,btree_info_value_size_candidate,status,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsOmapBtreeRootProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.sourceRole) << ',' << r.omapOid << ',' << r.omTreeOid << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                    << r.objectOid << ',' << r.objectXid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ','
                    << r.btnFlags << ',' << r.btnLevel << ',' << r.btnNkeys << ',' << r.tableSpaceOffset << ',' << r.tableSpaceLength << ','
                    << r.freeSpaceOffset << ',' << r.freeSpaceLength << ',' << r.btFlagsCandidate << ',' << r.btNodeSizeCandidate << ',' << r.btKeySizeCandidate << ',' << r.btValSizeCandidate << ','
                    << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_btree_root_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapLookupCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,target_role,target_oid,target_xid,omap_oid,om_tree_oid,root_level,root_nkeys,status,interpretation,notes\n";
            for (const auto& r : apfsOmapLookupProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.targetRole) << ',' << r.targetOid << ',' << r.targetXid << ',' << r.omapOid << ',' << r.omTreeOid << ','
                    << r.rootLevel << ',' << r.rootNkeys << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_lookup_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapBtreeTocCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,source_role,omap_oid,om_tree_oid,virtual_offset,root_level,root_nkeys,entry_index,toc_offset,key_offset_candidate,key_length_candidate,value_offset_candidate,value_length_candidate,key_sample_hex,value_sample_hex,status,interpretation,notes\n";
            for (const auto& r : apfsOmapBtreeTocProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.sourceRole) << ',' << r.omapOid << ',' << r.omTreeOid << ',' << r.virtualOffset << ',' << r.rootLevel << ',' << r.rootNkeys << ','
                    << r.entryIndex << ',' << r.tocOffset << ',' << r.keyOffsetCandidate << ',' << r.keyLengthCandidate << ','
                    << r.valueOffsetCandidate << ',' << r.valueLengthCandidate << ',' << csvEscape(r.keySampleHex) << ',' << csvEscape(r.valueSampleHex) << ','
                    << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_btree_toc_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapLeafKvCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,source_role,omap_oid,om_tree_oid,virtual_offset,root_level,root_nkeys,entry_index,toc_offset,key_offset,value_offset,key_oid,key_xid,value_flags,value_size,value_paddr,resolved_virtual_offset,resolved_bytes_read,resolved_object_oid,resolved_object_xid,resolved_object_type_raw,resolved_object_type_label,resolved_object_subtype,resolved_magic,target_role,lookup_status,status,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsOmapLeafKvDecodeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.sourceRole) << ',' << r.omapOid << ',' << r.omTreeOid << ',' << r.virtualOffset << ',' << r.rootLevel << ',' << r.rootNkeys << ','
                    << r.entryIndex << ',' << r.tocOffset << ',' << r.keyOffset << ',' << r.valueOffset << ',' << r.keyOid << ',' << r.keyXid << ','
                    << r.valueFlags << ',' << r.valueSize << ',' << r.valuePaddr << ',' << r.resolvedVirtualOffset << ',' << r.resolvedBytesRead << ','
                    << r.resolvedObjectOid << ',' << r.resolvedObjectXid << ',' << r.resolvedObjectTypeRaw << ',' << csvEscape(r.resolvedObjectTypeLabel) << ',' << r.resolvedObjectSubtype << ','
                    << csvEscape(r.resolvedMagic) << ',' << csvEscape(r.targetRole) << ',' << csvEscape(r.lookupStatus) << ',' << csvEscape(r.status) << ','
                    << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_leaf_kv_decode.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapLeafLookupCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,target_role,target_oid,target_xid,selected_key_oid,selected_key_xid,value_flags,value_size,value_paddr,resolved_virtual_offset,resolved_bytes_read,resolved_object_oid,resolved_object_xid,resolved_object_type_raw,resolved_object_type_label,resolved_object_subtype,resolved_magic,status,interpretation,notes\n";
            std::vector<std::pair<std::string, std::uint64_t>> targets;
            targets.push_back({"NX_OMAP_SELF", nxSummary.omapOid});
            for (std::size_t idx = 0; idx < nxSummary.fsOids.size() && idx < 32U; ++idx) {
                targets.push_back({std::string("NX_FS_OID_") + std::to_string(idx), nxSummary.fsOids[idx]});
            }
            for (const auto& target : targets) {
                const ApfsOmapLeafKvDecodeRow* best = nullptr;
                for (const auto& r : apfsOmapLeafKvDecodeRows) {
                    if (r.keyOid != target.second) continue;
                    if (nxSummary.nextXid != 0 && r.keyXid > nxSummary.nextXid) continue;
                    if (!best || r.keyXid > best->keyXid) best = &r;
                }
                if (best) {
                    out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                        << csvEscape(target.first) << ',' << target.second << ',' << nxSummary.nextXid << ',' << best->keyOid << ',' << best->keyXid << ','
                        << best->valueFlags << ',' << best->valueSize << ',' << best->valuePaddr << ',' << best->resolvedVirtualOffset << ',' << best->resolvedBytesRead << ','
                        << best->resolvedObjectOid << ',' << best->resolvedObjectXid << ',' << best->resolvedObjectTypeRaw << ',' << csvEscape(best->resolvedObjectTypeLabel) << ','
                        << best->resolvedObjectSubtype << ',' << csvEscape(best->resolvedMagic) << ",OMAP_LOOKUP_SELECTED," 
                        << csvEscape("Best leaf entry by target OID and highest key XID not exceeding nx_next_xid.") << ',' << csvEscape(best->notes) << "\n";
                } else {
                    out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                        << csvEscape(target.first) << ',' << target.second << ',' << nxSummary.nextXid << ",0,0,0,0,0,0,-1,0,0,0,,0,,OMAP_LOOKUP_NOT_FOUND,"
                        << csvEscape("No decoded OMAP leaf key matched this target OID with an acceptable transaction ID.") << "," << csvEscape("target_oid=" + std::to_string(target.second)) << "\n";
                }
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_leaf_lookup_results.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsObjectResolutionJsonPath, std::ios::binary);
            out << "{\n";
            out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
            out << "  \"app_version\": \"" << appVersion() << "\",\n";
            out << "  \"source_id\": \"" << source.sourceId << "\",\n";
            out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
            out << "  \"probe_scope\": \"EXACT_INPUT_FILE_VIA_LIBAFF4_APFS_OBJECT_RESOLUTION\",\n";
            out << "  \"object_id_probe_rows\": " << apfsObjectIdProbeRows.size() << ",\n";
            out << "  \"object_id_read_ok\": " << readOk << ",\n";
            out << "  \"object_id_omap_hits\": " << omapHits << ",\n";
            out << "  \"object_id_apsb_hits\": " << apsbHits << ",\n";
            out << "  \"btree_probe_rows\": " << apfsBtreeNodeProbeRows.size() << ",\n";
            out << "  \"btree_object_rows\": " << btreeRows << ",\n";
            out << "  \"btree_node_rows\": " << btreeNodeRows << ",\n";
            out << "  \"omap_phys_rows\": " << omapPhysRows << ",\n";
            out << "  \"omap_btree_root_rows\": " << omapBtreeRows << ",\n";
            out << "  \"omap_lookup_rows\": " << omapLookupRows << ",\n";
            out << "  \"omap_btree_toc_rows\": " << apfsOmapBtreeTocProbeRows.size() << ",\n";
            out << "  \"omap_leaf_kv_decode_rows\": " << apfsOmapLeafKvDecodeRows.size() << ",\n";
            out << "  \"resolved_volume_superblock_rows\": " << apfsResolvedVolumeSuperblockRows.size() << ",\n";
            out << "  \"volume_omap_probe_rows\": " << apfsVolumeOmapProbeRows.size() << "\n";
            out << "}\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_object_resolution_probe_summary.json: ") + ex.what());
        }
        try {
            std::ofstream out(apfsObjectResolutionMdPath, std::ios::binary);
            out << "# AFF4 APFS Object Resolution Probe\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Scope\n\n";
            out << "This probe uses bounded reads through libaff4 against only the one explicit AFF4 input file. It adds direct object-ID reads for NX object-map/spaceman/reaper/filesystem OIDs and a generic B-tree/BTREE_NODE header inventory. It does not search `O:\\`, inspect parent folders, call aff4imager, mount, or export RAW/DD.\n\n";
            out << "## Summary\n\n";
            out << "- Object-ID probe rows: `" << apfsObjectIdProbeRows.size() << "`\n";
            out << "- Object-ID reads OK: `" << readOk << "`\n";
            out << "- Object-ID OMAP hits: `" << omapHits << "`\n";
            out << "- Object-ID APSB hits: `" << apsbHits << "`\n";
            out << "- B-tree inventory rows: `" << apfsBtreeNodeProbeRows.size() << "`\n\n";
            out << "## Interpretation\n\n";
            out << "If direct object-ID reads show BTREE or BTREE_NODE objects instead of APSB/OMAP magic, the next step is APFS B-tree/OMAP entry decoding rather than treating object IDs as block addresses. The B-tree inventory is intended to identify node levels, key counts, and candidate table-space layout before implementing OMAP lookup.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_APFS_OBJECT_RESOLUTION_PROBE.md: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapSummaryJsonPath, std::ios::binary);
            std::size_t leafRoots = 0, branchRoots = 0, tocRows = apfsOmapBtreeTocProbeRows.size();
            for (const auto& r : apfsOmapBtreeRootProbeRows) {
                if (r.status == "OMAP_BTREE_ROOT_READ" && r.btnLevel == 0) ++leafRoots;
                if (r.status == "OMAP_BTREE_ROOT_READ" && r.btnLevel > 0) ++branchRoots;
            }
            out << "{\n";
            out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
            out << "  \"app_version\": \"" << appVersion() << "\",\n";
            out << "  \"source_id\": \"" << source.sourceId << "\",\n";
            out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
            out << "  \"probe_scope\": \"EXACT_INPUT_FILE_VIA_LIBAFF4_APFS_OMAP_RECON\",\n";
            out << "  \"omap_phys_rows\": " << apfsOmapPhysProbeRows.size() << ",\n";
            out << "  \"omap_btree_root_rows\": " << apfsOmapBtreeRootProbeRows.size() << ",\n";
            out << "  \"omap_lookup_rows\": " << apfsOmapLookupProbeRows.size() << ",\n";
            out << "  \"omap_leaf_root_rows\": " << leafRoots << ",\n";
            out << "  \"omap_branch_root_rows\": " << branchRoots << ",\n";
            out << "  \"omap_btree_toc_rows\": " << tocRows << ",\n";
            out << "  \"omap_leaf_kv_decode_rows\": " << apfsOmapLeafKvDecodeRows.size() << ",\n";
            out << "  \"resolved_volume_superblock_rows\": " << apfsResolvedVolumeSuperblockRows.size() << ",\n";
            out << "  \"volume_omap_probe_rows\": " << apfsVolumeOmapProbeRows.size() << "\n";
            out << "}\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_probe_summary.json: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapLeafKvMdPath, std::ios::binary);
            std::size_t apsbResolved = 0;
            std::size_t decodedRows = 0;
            for (const auto& r : apfsOmapLeafKvDecodeRows) {
                if (r.status == "OMAP_LEAF_KV_DECODED") ++decodedRows;
                if (r.resolvedMagic == "APSB") ++apsbResolved;
            }
            out << "# AFF4 APFS OMAP Leaf Key/Value Decode\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Scope\n\n";
            out << "This probe decodes fixed-size OMAP leaf B-tree table-of-contents entries from the object-map root using the exact selected AFF4 file through libaff4. It does not scan folders, mount, call aff4imager, or export RAW/DD.\n\n";
            out << "## Summary\n\n";
            out << "- Leaf key/value rows decoded: `" << decodedRows << "`\n";
            out << "- APSB resolved rows: `" << apsbResolved << "`\n";
            out << "- Total leaf decode rows: `" << apfsOmapLeafKvDecodeRows.size() << "`\n";
            out << "- Resolved APSB superblock rows: `" << apfsResolvedVolumeSuperblockRows.size() << "`\n";
            out << "- Volume OMAP probe rows: `" << apfsVolumeOmapProbeRows.size() << "`\n\n";
            out << "## Interpretation\n\n";
            out << "If APSB rows are present, the next step is to parse the APFS volume superblock(s), then volume object maps and catalog/root directory structures to locate Spotlight artifacts. If no APSB rows are present, verify OMAP TOC offset interpretation and continue branch/leaf decoder refinement.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_APFS_OMAP_LEAF_KV_DECODE.md: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapTocMdPath, std::ios::binary);
            out << "# AFF4 APFS OMAP B-tree TOC Probe\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Scope\n\n";
            out << "This file summarizes raw candidate table-of-contents rows sampled from the OMAP B-tree root. It is reconnaissance only: it records candidate key/value locations and short samples so the next implementation can distinguish branch entries from leaf OMAP key/value entries without mounting or exporting a full RAW/DD image.\n\n";
            out << "## Summary\n\n";
            out << "- OMAP B-tree TOC candidate rows: `" << apfsOmapBtreeTocProbeRows.size() << "`\n";
            out << "- OMAP B-tree root rows: `" << apfsOmapBtreeRootProbeRows.size() << "`\n\n";
            out << "## Next step\n\n";
            out << "If the root level is greater than zero, decode branch-node keys and child OIDs/physical addresses. If the root level is zero, decode OMAP leaf keys and values and select the highest transaction ID at or below the NX transaction ID for each target object ID.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_APFS_OMAP_TOC_PROBE.md: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapMdPath, std::ios::binary);
            out << "# AFF4 APFS OMAP Probe\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Scope\n\n";
            out << "This probe parses APFS object-map physical objects (`omap_phys_t`) and reads the B-tree root referenced by `om_tree_oid` using only the explicitly selected AFF4 file through libaff4. It does not scan drives, call aff4imager, mount, or export RAW/DD.\n\n";
            out << "## Summary\n\n";
            out << "- OMAP physical rows: `" << apfsOmapPhysProbeRows.size() << "`\n";
            out << "- OMAP B-tree root rows: `" << apfsOmapBtreeRootProbeRows.size() << "`\n";
            out << "- OMAP lookup readiness rows: `" << apfsOmapLookupProbeRows.size() << "`\n";
            out << "- OMAP B-tree TOC candidate rows: `" << apfsOmapBtreeTocProbeRows.size() << "`\n";
            out << "- Resolved APSB superblock rows: `" << apfsResolvedVolumeSuperblockRows.size() << "`\n";
            out << "- Volume OMAP probe rows: `" << apfsVolumeOmapProbeRows.size() << "`\n\n";
            out << "## Interpretation\n\n";
            out << "If `aff4_apfs_omap_phys_probe.csv` contains an `om_tree_oid`, the AFF4 virtual reader is now reaching the APFS object-map layer. If the B-tree root has `btn_level > 0`, the next implementation step is branch-node traversal before filesystem OIDs can resolve to APFS volume superblocks. If a leaf root is observed, the next step is decoding the OMAP key/value table-of-contents and selecting the highest transaction ID less than or equal to the container transaction ID.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_APFS_OMAP_PROBE.md: ") + ex.what());
        }
    };

    auto writeOutputs = [&]() {
        try {
            std::ofstream out(csvPath, std::ios::binary);
            out << "source_id,input_path,input_type,step,status,path,object_size,offset,bytes_read,sample_hex,notes\n";
            for (const auto& r : rows) {
                out << csvEscape(source.sourceId) << ','
                    << csvEscape(pathString(originalInput)) << ','
                    << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.step) << ','
                    << csvEscape(r.status) << ','
                    << csvEscape(pathString(r.path)) << ','
                    << r.objectSize << ','
                    << r.offset << ','
                    << r.bytesRead << ','
                    << csvEscape(r.sampleHex) << ','
                    << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_cpp_lite_dynamic_load_probe.csv: ") + ex.what());
        }

        try {
            std::ofstream out(apfsCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,step,status,virtual_offset,bytes_read,signature,confidence,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsRows) {
                out << csvEscape(source.sourceId) << ','
                    << csvEscape(pathString(originalInput)) << ','
                    << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.step) << ','
                    << csvEscape(r.status) << ','
                    << r.offset << ','
                    << r.bytesRead << ','
                    << csvEscape(r.signature) << ','
                    << csvEscape(r.confidence) << ','
                    << csvEscape(r.interpretation) << ','
                    << csvEscape(r.sampleHex) << ','
                    << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_virtual_apfs_probe.csv: ") + ex.what());
        }

        try {
            std::ofstream out(apfsJsonPath, std::ios::binary);
            out << "{\n";
            out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
            out << "  \"app_version\": \"" << appVersion() << "\",\n";
            out << "  \"source_id\": \"" << source.sourceId << "\",\n";
            out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
            out << "  \"probe_scope\": \"EXACT_INPUT_FILE_VIA_LIBAFF4_ONLY\",\n";
            out << "  \"dynamic_probe_enabled\": " << (opt.enableAff4DynamicProbe ? "true" : "false") << ",\n";
            out << "  \"strict_single_aff4\": " << (opt.strictSingleAff4 ? "true" : "false") << ",\n";
            out << "  \"object_size\": " << finalObjectSize << ",\n";
            out << "  \"object_block_size\": " << finalBlockSize << ",\n";
            out << "  \"virtual_reads_attempted\": " << virtualReadCount << ",\n";
            out << "  \"gpt_found\": " << (gptFound ? "true" : "false") << ",\n";
            out << "  \"partition_count\": " << partitionCount << ",\n";
            out << "  \"apfs_partition_count\": " << apfsPartitionCount << ",\n";
            out << "  \"nxsb_hit_count\": " << nxsbHitCount << ",\n";
            out << "  \"row_count\": " << apfsRows.size() << "\n";
            out << "}\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_virtual_apfs_probe_summary.json: ") + ex.what());
        }

        try {
            std::ofstream out(planPath, std::ios::binary);
            out << "# AFF4 CPP Lite Dynamic-Load Probe\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Source\n\n";
            out << "- Source ID: `" << source.sourceId << "`\n";
            out << "- Input path: `" << pathString(originalInput) << "`\n";
            out << "- Input type: `" << inputSourceType(originalInput) << "`\n\n";
            out << "## Probe result rows\n\n";
            out << "| Step | Status | Object size | Bytes read | Sample | Notes |\n";
            out << "|---|---|---:|---:|---|---|\n";
            for (const auto& r : rows) {
                std::string note = r.notes;
                std::replace(note.begin(), note.end(), '|', '/');
                std::string sample = r.sampleHex;
                std::replace(sample.begin(), sample.end(), '|', '/');
                out << "| " << r.step << " | " << r.status << " | " << r.objectSize << " | " << r.bytesRead << " | `" << sample << "` | " << note << " |\n";
            }
            out << "\n## Interpretation\n\n";
            out << "`READ_OK` at offset 0 means Vestigant can load libaff4, open the explicit AFF4 image, and perform a bounded random-access read without exporting a full RAW/DD image. V0_8_23 adds a virtual APFS/GPT probe that reuses that read path.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_CPP_LITE_DYNAMIC_LOAD_PROBE.md: ") + ex.what());
        }

        try {
            std::ofstream out(apfsPlanPath, std::ios::binary);
            out << "# AFF4 Virtual APFS Probe\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Purpose\n\n";
            out << "This probe uses the aff4-cpp-lite C API against the exact `--input` AFF4 file and attempts bounded virtual reads for disk/APFS discovery. It does not search parent folders, does not scan O:\\\\, and does not export RAW/DD.\n\n";
            out << "## Result summary\n\n";
            out << "- Dynamic probe enabled: `" << (opt.enableAff4DynamicProbe ? "true" : "false") << "`\n";
            out << "- Strict single AFF4: `" << (opt.strictSingleAff4 ? "true" : "false") << "`\n";
            out << "- Object size: `" << finalObjectSize << "`\n";
            out << "- Object block size: `" << finalBlockSize << "`\n";
            out << "- GPT found: `" << (gptFound ? "true" : "false") << "`\n";
            out << "- Partitions found: `" << partitionCount << "`\n";
            out << "- APFS partitions found: `" << apfsPartitionCount << "`\n";
            out << "- NXSB hits: `" << nxsbHitCount << "`\n\n";
            out << "## Next step\n\n";
            out << "If GPT/APFS rows are present, the next version should wrap this AFF4 read path in an image block-reader interface and feed it to APFS enumeration rather than scanning ZIP member payload strings.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_VIRTUAL_APFS_PROBE.md: ") + ex.what());
        }
    };

    if (!opt.enableAff4DynamicProbe) {
        addRow("dynamic_load_probe", opt.strictSingleAff4 ? "SKIPPED_STRICT_SINGLE_AFF4" : "SKIPPED_BY_DEFAULT", originalInput, 0, 0, -1, {},
               "Dynamic libaff4 open/read is opt-in only because aff4-cpp-lite may discover/open other AFF4-related files from the evidence drive. Use --enable-aff4-dynamic-probe only when deliberately testing libaff4 random access.");
        addApfsRow("virtual_apfs_probe", "SKIPPED_DYNAMIC_PROBE_NOT_ENABLED", 0, -1, {}, "NOT_RUN", "APFS virtual read probe was skipped because --enable-aff4-dynamic-probe was not supplied.", {}, "Run the single-AFF4 helper with -EnableAff4DynamicProbe to perform bounded virtual GPT/APFS reads through libaff4.");
        writeOutputs();
        writeAff4ApfsContainerViewOutputs(caseDir, source, originalInput, nxSummary, apfsDescriptorRows, log);
        writeAff4ApfsVolumeSuperblockOutputs(caseDir, source, originalInput, nxSummary, apfsVolumeRows, log);
        writeAff4ApfsCheckpointMapOutputs(caseDir, source, originalInput, nxSummary, apfsCheckpointMapRows, apfsCheckpointMappedObjectRows, log);
        writeObjectResolutionOutputs();
        log.info("AFF4 CPP Lite dynamic-load probe written: " + pathString(csvPath));
        log.info("AFF4 virtual APFS probe written: " + pathString(apfsCsvPath));
        log.info("AFF4 APFS container view written: " + pathString(caseDir / "aff4_apfs_container_superblock.csv"));
        log.info("AFF4 APFS volume superblock probe written: " + pathString(caseDir / "aff4_apfs_volume_superblocks.csv"));
    log.info("AFF4 APFS checkpoint map probe written: " + pathString(caseDir / "aff4_apfs_checkpoint_map.csv"));
        return;
    }

#ifdef _WIN32
    {
        std::string skipReason;
        std::string metadataSample;
        if (shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(caseDir, originalInput, skipReason, metadataSample)) {
            appendRunStatus(caseDir, "aff4_dynamic_load_probe_guard", "skipped known blocking BlackBag/LZ4 discontiguous AFF4 layout");
            appendRunStatus(caseDir, "aff4_direct_map_reader_probe", "read AFF4 map/index/data ZIP members directly and decode bounded LZ4 chunks");
            log.warn("Skipping libaff4 dynamic AFF4_open probe: " + skipReason);
            Aff4ProbeWorker::executeDirectMapReaderProbe(caseDir, source, opt, originalInput, cancelToken, log);
            addRow("AFF4_open_guard", "SKIPPED_KNOWN_BLOCKING_LAYOUT", originalInput, 0, 0, -1, {}, skipReason);
            addApfsRow("AFF4_open_guard", "SKIPPED_KNOWN_BLOCKING_LAYOUT", 0, -1, {}, "BLOCKED_BY_KNOWN_READER_HANG",
                       "The AFF4 metadata identifies a BlackBag-style APFS DiscontiguousImage with LZ4 ImageStream storage. This build skips libaff4 AFF4_open to avoid the observed Windows hang and records the direct ZIP map/index parser as the next required reader path.",
                       {}, metadataSample);
            try {
                std::ofstream out(caseDir / "AFF4_DIRECT_MAP_READER_REQUIRED.md", std::ios::binary);
                out << "# AFF4 Direct Map Reader Required\n\n";
                out << "Version: " << appVersion() << "\n\n";
                out << "The AFF4 metadata matches a BlackBag-style APFS `DiscontiguousImage` backed by an LZ4 `ImageStream`. The libaff4 dynamic C API was skipped because this layout was observed to block during `AFF4_open` on Windows.\n\n";
                out << "## Next Implementation Step\n\n";
                out << "Decode the stored AFF4 ZIP members directly:\n\n";
                out << "- `information.turtle`\n";
                out << "- `*/map`\n";
                out << "- `*/idx`\n";
                out << "- `*/data/NNNNNNNN.index`\n";
                out << "- `*/data/NNNNNNNN`\n\n";
                out << "The direct reader should map AFF4 virtual offsets to image-stream chunks, decompress LZ4 chunks, and feed bounded reads into the APFS parser without calling `AFF4_open` or exporting a RAW/DD image.\n\n";
                out << "## Guard Reason\n\n";
                out << skipReason << "\n";
            } catch (const std::exception& ex) {
                log.warn(std::string("Unable to write AFF4_DIRECT_MAP_READER_REQUIRED.md: ") + ex.what());
            }
            writeOutputs();
            if (!fs::exists(caseDir / "aff4_apfs_container_superblock.csv")) {
                writeAff4ApfsContainerViewOutputs(caseDir, source, originalInput, nxSummary, apfsDescriptorRows, log);
            }
            if (!fs::exists(caseDir / "aff4_apfs_volume_superblocks.csv")) {
                writeAff4ApfsVolumeSuperblockOutputs(caseDir, source, originalInput, nxSummary, apfsVolumeRows, log);
            }
            if (!fs::exists(caseDir / "aff4_apfs_checkpoint_map.csv")) {
                writeAff4ApfsCheckpointMapOutputs(caseDir, source, originalInput, nxSummary, apfsCheckpointMapRows, apfsCheckpointMappedObjectRows, log);
            }
            writeObjectResolutionOutputs();
            log.info("AFF4 CPP Lite dynamic-load probe skipped by known-layout guard: " + pathString(csvPath));
            return;
        }
    }
    const fs::path libaff4Dll = findToolCandidate(opt, "VESTIGANT_LIBAFF4_DLL", std::vector<std::string>{"libaff4.dll"});
    if (libaff4Dll.empty()) {
        addRow("resolve_libaff4_dll", "MISSING_LIBAFF4_DLL", {}, 0, 0, -1, {}, "Provide --reader-tools or VESTIGANT_LIBAFF4_DLL pointing to libaff4.dll.");
        addApfsRow("resolve_libaff4_dll", "MISSING_LIBAFF4_DLL", 0, -1, {}, "BLOCKED", "Cannot perform virtual GPT/APFS reads without libaff4.dll.", {}, "Reader tools were not found.");
    } else {
        addRow("resolve_libaff4_dll", "FOUND", libaff4Dll, 0, 0, -1, {}, "Resolved libaff4.dll for dynamic-load probe.");
        const fs::path dllDir = libaff4Dll.parent_path();
        DLL_DIRECTORY_COOKIE aff4DllDirCookie = nullptr;
        if (!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS)) {
            addRow("SetDefaultDllDirectories", "WARNING", dllDir, 0, 0, -1, {}, lastWindowsErrorString());
        }
        if (!dllDir.empty()) {
            aff4DllDirCookie = AddDllDirectory(dllDir.wstring().c_str());
            if (!aff4DllDirCookie) addRow("AddDllDirectory", "WARNING", dllDir, 0, 0, -1, {}, lastWindowsErrorString());
        }
        HMODULE h = LoadLibraryExW(libaff4Dll.wstring().c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
        if (!h) {
            addRow("LoadLibraryExW", "LOAD_FAILED", libaff4Dll, 0, 0, -1, {}, lastWindowsErrorString());
            addApfsRow("LoadLibraryExW", "LOAD_FAILED", 0, -1, {}, "BLOCKED", "Cannot perform virtual GPT/APFS reads because libaff4.dll did not load.", {}, lastWindowsErrorString());
        } else {
            addRow("LoadLibraryExW", "LOADED", libaff4Dll, 0, 0, -1, {}, "libaff4.dll loaded with per-module secure DLL search flags.");
            using Aff4InitFn = void (__cdecl *)();
            using Aff4OpenFn = int (__cdecl *)(const char*);
            using Aff4ObjectSizeFn = std::int64_t (__cdecl *)(int);
            using Aff4ObjectBlockSizeFn = std::int64_t (__cdecl *)(int);
            using Aff4ReadFn = int (__cdecl *)(int, std::uint64_t, void*, int);
            using Aff4CloseFn = int (__cdecl *)(int);
            auto pInit = reinterpret_cast<Aff4InitFn>(GetProcAddress(h, "AFF4_init"));
            auto pOpen = reinterpret_cast<Aff4OpenFn>(GetProcAddress(h, "AFF4_open"));
            auto pSize = reinterpret_cast<Aff4ObjectSizeFn>(GetProcAddress(h, "AFF4_object_size"));
            auto pBlockSize = reinterpret_cast<Aff4ObjectBlockSizeFn>(GetProcAddress(h, "AFF4_object_blocksize"));
            auto pRead = reinterpret_cast<Aff4ReadFn>(GetProcAddress(h, "AFF4_read"));
            auto pClose = reinterpret_cast<Aff4CloseFn>(GetProcAddress(h, "AFF4_close"));
            const bool symbolsOk = pOpen && pSize && pRead && pClose;
            addRow("resolve_symbols", symbolsOk ? "SYMBOLS_READY" : "MISSING_REQUIRED_SYMBOL", libaff4Dll, 0, 0, -1, {},
                   std::string("AFF4_init=") + (pInit ? "yes" : "no") + "; AFF4_open=" + (pOpen ? "yes" : "no") + "; AFF4_object_size=" + (pSize ? "yes" : "no") + "; AFF4_object_blocksize=" + (pBlockSize ? "yes" : "no") + "; AFF4_read=" + (pRead ? "yes" : "no") + "; AFF4_close=" + (pClose ? "yes" : "no"));
            if (!symbolsOk) {
                addApfsRow("resolve_symbols", "MISSING_REQUIRED_SYMBOL", 0, -1, {}, "BLOCKED", "Cannot perform virtual GPT/APFS reads because required AFF4 C API symbols are missing.", {}, "Required symbols: AFF4_open, AFF4_object_size, AFF4_read, AFF4_close.");
            } else {
                if (pInit) {
                    try { pInit(); addRow("AFF4_init", "CALLED", libaff4Dll, 0, 0, -1, {}, "AFF4_init completed."); }
                    catch (...) { addRow("AFF4_init", "EXCEPTION", libaff4Dll, 0, 0, -1, {}, "Exception while calling AFF4_init."); }
                }
                const std::string inputUtf8 = pathString(originalInput);
                int handle = -1;
                try { handle = pOpen(inputUtf8.c_str()); }
                catch (...) { addRow("AFF4_open", "EXCEPTION", originalInput, 0, 0, -1, {}, "Exception while calling AFF4_open."); }
                if (handle < 0) {
                    addRow("AFF4_open", "OPEN_FAILED", originalInput, 0, 0, -1, {}, "AFF4_open returned a negative handle. Check whether the container has a first aff4:Image object and whether dependent DLLs are loadable.");
                    addApfsRow("AFF4_open", "OPEN_FAILED", 0, -1, {}, "BLOCKED", "Cannot perform virtual GPT/APFS reads because AFF4_open failed.", {}, "AFF4_open returned a negative handle.");
                } else {
                    addRow("AFF4_open", "OPENED", originalInput, 0, 0, -1, {}, "AFF4_open returned a non-negative handle.");
                    std::int64_t objectSizeSigned = -1;
                    try { objectSizeSigned = pSize(handle); }
                    catch (...) { addRow("AFF4_object_size", "EXCEPTION", originalInput, 0, 0, -1, {}, "Exception while calling AFF4_object_size."); }
                    finalObjectSize = objectSizeSigned > 0 ? static_cast<std::uint64_t>(objectSizeSigned) : 0;
                    addRow("AFF4_object_size", objectSizeSigned > 0 ? "SIZE_REPORTED" : "SIZE_ZERO_OR_UNKNOWN", originalInput, finalObjectSize, 0, objectSizeSigned, {}, "Virtual image size reported by libaff4.");

                    if (pBlockSize) {
                        std::int64_t blockSize = -1;
                        try { blockSize = pBlockSize(handle); }
                        catch (...) { addRow("AFF4_object_blocksize", "EXCEPTION", originalInput, finalObjectSize, 0, -1, {}, "Exception while calling AFF4_object_blocksize."); }
                        finalBlockSize = blockSize;
                        addRow("AFF4_object_blocksize", blockSize > 0 ? "BLOCKSIZE_REPORTED" : "BLOCKSIZE_ZERO_OR_UNKNOWN", originalInput, finalObjectSize, 0, blockSize, {}, "AFF4 object block size reported by libaff4 when available.");
                    }

                    auto readVirtual = [&](std::uint64_t offset, std::size_t length, std::vector<unsigned char>& out, std::string& err) -> long long {
                        out.assign(length, 0);
                        int rc = -1;
                        try { rc = pRead(handle, offset, out.data(), static_cast<int>(length)); }
                        catch (...) { err = "Exception while calling AFF4_read."; return -1; }
                        if (rc < 0) { err = "AFF4_read returned a negative value."; out.clear(); return rc; }
                        if (static_cast<std::size_t>(rc) < out.size()) out.resize(static_cast<std::size_t>(rc));
                        ++virtualReadCount;
                        return static_cast<long long>(rc);
                    };

                    auto addReadRow = [&](const std::string& step, std::uint64_t offset, long long rc, const std::vector<unsigned char>& buf, const std::string& interpretation, const std::string& notes) {
                        const std::size_t sampleLen = buf.size() < 64 ? buf.size() : 64;
                        addApfsRow(step, rc > 0 ? "READ_OK" : (rc == 0 ? "READ_ZERO" : "READ_FAILED"), offset, rc, {}, rc > 0 ? "READ_PERFORMED" : "NO_DATA", interpretation, sampleLen ? hexSampleBytes(buf.data(), sampleLen) : std::string{}, notes);
                    };

                    auto addBtreeProbeFromBuffer = [&](const std::string& sourceRole,
                                                       std::uint64_t sourceOid,
                                                       std::uint64_t sourcePaddr,
                                                       std::uint64_t virtualOffset,
                                                       long long rc,
                                                       const std::vector<unsigned char>& buf,
                                                       const std::string& notes) {
                        if (rc <= 0 || buf.size() < 64) return;
                        const std::uint32_t rawType = readLe32(buf, 24);
                        const std::string label = apfsObjectTypeLabel(rawType);
                        if (label != "BTREE" && label != "BTREE_NODE") return;
                        ApfsBtreeNodeProbeRow br;
                        br.sourceRole = sourceRole;
                        br.sourceOid = sourceOid;
                        br.sourcePaddr = sourcePaddr;
                        br.virtualOffset = virtualOffset;
                        br.bytesRead = rc;
                        br.objectOid = readLe64(buf, 8);
                        br.objectXid = readLe64(buf, 16);
                        br.objectTypeRaw = rawType;
                        br.objectSubtype = readLe32(buf, 28);
                        br.objectTypeLabel = label;
                        br.btnFlags = readLe16(buf, 32);
                        br.btnLevel = readLe16(buf, 34);
                        br.btnNkeys = readLe32(buf, 36);
                        br.tableSpaceOffset = readLe16(buf, 40);
                        br.tableSpaceLength = readLe16(buf, 42);
                        br.freeSpaceOffset = readLe16(buf, 44);
                        br.freeSpaceLength = readLe16(buf, 46);
                        br.status = br.btnNkeys < 100000U ? "BTREE_HEADER_PARSED" : "BTREE_HEADER_SUSPICIOUS";
                        br.interpretation = label + " object header parsed. Values are generic APFS B-tree header candidates pending object-map/catalog-specific decoding.";
                        br.sampleHex = hexSampleBytes(buf.data(), buf.size() < 96 ? buf.size() : 96);
                        br.notes = notes;
                        apfsBtreeNodeProbeRows.push_back(br);
                    };

                    auto addOmapLookupReadinessRow = [&](const std::string& targetRole,
                                                         std::uint64_t targetOid,
                                                         std::uint64_t targetXid,
                                                         std::uint64_t omapOid,
                                                         std::uint64_t omTreeOid,
                                                         std::uint16_t rootLevel,
                                                         std::uint32_t rootNkeys,
                                                         const std::string& status,
                                                         const std::string& interpretation,
                                                         const std::string& notes) {
                        ApfsOmapLookupProbeRow lr;
                        lr.targetRole = targetRole;
                        lr.targetOid = targetOid;
                        lr.targetXid = targetXid;
                        lr.omapOid = omapOid;
                        lr.omTreeOid = omTreeOid;
                        lr.rootLevel = rootLevel;
                        lr.rootNkeys = rootNkeys;
                        lr.status = status;
                        lr.interpretation = interpretation;
                        lr.notes = notes;
                        apfsOmapLookupProbeRows.push_back(lr);
                    };

                    auto addOmapBtreeTocProbeRowsFromBuffer = [&](const std::string& sourceRole,
                                                                  std::uint64_t omapOid,
                                                                  std::uint64_t omTreeOid,
                                                                  std::uint64_t virtualOffset,
                                                                  std::uint16_t rootLevel,
                                                                  std::uint32_t rootNkeys,
                                                                  const std::vector<unsigned char>& buf,
                                                                  const std::string& notes) {
                        if (buf.size() < 64) return;
                        const std::uint16_t btnFlags = readLe16(buf, 32);
                        const bool fixedKv = (btnFlags & 0x0004U) != 0;
                        const std::uint16_t tableSpaceOffset = readLe16(buf, 40);
                        const std::uint16_t tableSpaceLength = readLe16(buf, 42);
                        const std::uint16_t freeSpaceOffset = readLe16(buf, 44);
                        const std::uint16_t freeSpaceLength = readLe16(buf, 46);
                        const std::size_t btnDataStart = 56U;
                        std::size_t tocStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset);
                        if (tocStart >= buf.size()) tocStart = btnDataStart;
                        std::size_t keyAreaStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset) + static_cast<std::size_t>(tableSpaceLength);
                        if (keyAreaStart >= buf.size()) keyAreaStart = tocStart + (fixedKv ? 4U : 8U) * static_cast<std::size_t>(rootNkeys);
                        std::size_t valueAreaEnd = buf.size();
                        if (valueAreaEnd >= 40U && (btnFlags & 0x0001U) != 0) valueAreaEnd -= 40U; // root nodes end before btree_info_t
                        std::size_t valueAreaStart = keyAreaStart + static_cast<std::size_t>(freeSpaceOffset) + static_cast<std::size_t>(freeSpaceLength);
                        if (valueAreaStart > valueAreaEnd) valueAreaStart = valueAreaEnd;
                        std::size_t maxEntries = rootNkeys;
                        if (maxEntries > 128U) maxEntries = 128U;
                        if (maxEntries == 0U) {
                            ApfsOmapBtreeTocProbeRow row;
                            row.sourceRole = sourceRole;
                            row.omapOid = omapOid;
                            row.omTreeOid = omTreeOid;
                            row.virtualOffset = virtualOffset;
                            row.rootLevel = rootLevel;
                            row.rootNkeys = rootNkeys;
                            row.status = "NO_KEYS_REPORTED";
                            row.interpretation = "OMAP B-tree root reported zero keys; no candidate TOC entries sampled.";
                            row.notes = notes;
                            apfsOmapBtreeTocProbeRows.push_back(row);
                            return;
                        }
                        auto sampleAt = [&](std::size_t abs, std::size_t len) -> std::string {
                            if (len == 0 || abs >= buf.size()) return {};
                            std::size_t n = len;
                            if (abs + n > buf.size()) n = buf.size() - abs;
                            if (n > 32U) n = 32U;
                            return n ? hexSampleBytes(buf.data() + abs, n) : std::string{};
                        };
                        for (std::size_t i = 0; i < maxEntries; ++i) {
                            const std::size_t entrySize = fixedKv ? 4U : 8U;
                            const std::size_t entryOff = tocStart + (i * entrySize);
                            ApfsOmapBtreeTocProbeRow row;
                            row.sourceRole = sourceRole;
                            row.omapOid = omapOid;
                            row.omTreeOid = omTreeOid;
                            row.virtualOffset = virtualOffset;
                            row.rootLevel = rootLevel;
                            row.rootNkeys = rootNkeys;
                            row.entryIndex = static_cast<std::uint32_t>(i);
                            row.tocOffset = static_cast<std::uint32_t>(entryOff);
                            if (fixedKv && entryOff + 4U <= buf.size()) {
                                row.keyOffsetCandidate = readLe16(buf, entryOff + 0U);
                                row.keyLengthCandidate = 16;
                                row.valueOffsetCandidate = readLe16(buf, entryOff + 2U);
                                row.valueLengthCandidate = 16;
                                const std::size_t keyAbs = keyAreaStart + static_cast<std::size_t>(row.keyOffsetCandidate);
                                const std::size_t valAbs = (static_cast<std::size_t>(row.valueOffsetCandidate) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(row.valueOffsetCandidate)) : buf.size();
                                row.keySampleHex = sampleAt(keyAbs, 16U);
                                row.valueSampleHex = sampleAt(valAbs, 16U);
                                row.status = (row.keySampleHex.empty() || row.valueSampleHex.empty()) ? "FIXED_KVOFF_OUT_OF_RANGE" : "FIXED_KVOFF_ENTRY_SAMPLED";
                                row.interpretation = "Fixed-size OMAP B-tree TOC entry decoded as kvoff_t using APFS key/value area boundaries.";
                                row.notes = notes + "; btn_flags=" + std::to_string(btnFlags) + "; toc_start=" + std::to_string(tocStart) + "; key_area_start=" + std::to_string(keyAreaStart) + "; value_area_start=" + std::to_string(valueAreaStart) + "; value_area_end=" + std::to_string(valueAreaEnd);
                                if (rootLevel == 0 && keyAbs + 16U <= buf.size() && valAbs + 16U <= buf.size()) {
                                    ApfsOmapLeafKvDecodeRow kv;
                                    kv.sourceRole = sourceRole;
                                    kv.omapOid = omapOid;
                                    kv.omTreeOid = omTreeOid;
                                    kv.virtualOffset = virtualOffset;
                                    kv.rootLevel = rootLevel;
                                    kv.rootNkeys = rootNkeys;
                                    kv.entryIndex = static_cast<std::uint32_t>(i);
                                    kv.tocOffset = static_cast<std::uint32_t>(entryOff);
                                    kv.keyOffset = row.keyOffsetCandidate;
                                    kv.valueOffset = row.valueOffsetCandidate;
                                    kv.keyOid = readLe64(buf, keyAbs + 0U);
                                    kv.keyXid = readLe64(buf, keyAbs + 8U);
                                    kv.valueFlags = readLe32(buf, valAbs + 0U);
                                    kv.valueSize = readLe32(buf, valAbs + 4U);
                                    kv.valuePaddr = readLe64(buf, valAbs + 8U);
                                    kv.status = "OMAP_LEAF_KV_DECODED";
                                    kv.interpretation = "Decoded OMAP leaf key/value entry. value_paddr is a container-relative physical block address.";
                                    kv.sampleHex = row.keySampleHex + " | " + row.valueSampleHex;
                                    kv.notes = row.notes;
                                    if (nxSummary.blockSize != 0 && kv.valuePaddr <= ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                        kv.resolvedVirtualOffset = kv.valuePaddr * static_cast<std::uint64_t>(nxSummary.blockSize);
                                        if (finalObjectSize == 0 || kv.resolvedVirtualOffset < finalObjectSize) {
                                            std::vector<unsigned char> resolved;
                                            std::string resolvedErr;
                                            kv.resolvedBytesRead = readVirtual(kv.resolvedVirtualOffset, nxSummary.blockSize, resolved, resolvedErr);
                                            if (kv.resolvedBytesRead > 0 && resolved.size() >= 36) {
                                                kv.resolvedObjectOid = readLe64(resolved, 8);
                                                kv.resolvedObjectXid = readLe64(resolved, 16);
                                                kv.resolvedObjectTypeRaw = readLe32(resolved, 24);
                                                kv.resolvedObjectTypeLabel = apfsObjectTypeLabel(kv.resolvedObjectTypeRaw);
                                                kv.resolvedObjectSubtype = readLe32(resolved, 28);
                                                if (std::memcmp(resolved.data() + 32, "APSB", 4) == 0) kv.resolvedMagic = "APSB";
                                                else if (std::memcmp(resolved.data() + 32, "OMAP", 4) == 0) kv.resolvedMagic = "OMAP";
                                                else if (std::memcmp(resolved.data() + 32, "NXSB", 4) == 0) kv.resolvedMagic = "NXSB";
                                                else if (std::memcmp(resolved.data() + 32, "BTMR", 4) == 0) kv.resolvedMagic = "BTMR";
                                                if (kv.resolvedMagic == "APSB") kv.interpretation = "Decoded OMAP leaf entry resolved to an APFS volume superblock candidate.";
                                                else kv.interpretation = "Decoded OMAP leaf entry resolved to an APFS object; continue volume/catalog object-map interpretation.";
                                            } else {
                                                kv.notes += "; resolved_read=" + (resolvedErr.empty() ? std::string("no bytes") : resolvedErr);
                                            }
                                        } else {
                                            kv.notes += "; resolved offset beyond AFF4 virtual object size";
                                        }
                                    }
                                    if (kv.keyOid == nxSummary.omapOid) kv.targetRole = "NX_OMAP_SELF";
                                    for (std::size_t idx = 0; idx < nxSummary.fsOids.size() && idx < 32U; ++idx) {
                                        if (kv.keyOid == nxSummary.fsOids[idx]) kv.targetRole = std::string("NX_FS_OID_") + std::to_string(idx);
                                    }
                                    if (!kv.targetRole.empty() && (nxSummary.nextXid == 0 || kv.keyXid <= nxSummary.nextXid)) kv.lookupStatus = "MATCHES_TARGET_OID_XID_BOUNDS";
                                    else if (!kv.targetRole.empty()) kv.lookupStatus = "MATCHES_TARGET_OID_XID_TOO_NEW";
                                    else kv.lookupStatus = "NON_TARGET_OMAP_ENTRY";
                                    apfsOmapLeafKvDecodeRows.push_back(kv);
                                }
                            } else if (!fixedKv && entryOff + 8U <= buf.size()) {
                                row.keyOffsetCandidate = readLe16(buf, entryOff + 0U);
                                row.keyLengthCandidate = readLe16(buf, entryOff + 2U);
                                row.valueOffsetCandidate = readLe16(buf, entryOff + 4U);
                                row.valueLengthCandidate = readLe16(buf, entryOff + 6U);
                                const std::size_t keyAbs = keyAreaStart + static_cast<std::size_t>(row.keyOffsetCandidate);
                                const std::size_t valAbs = (static_cast<std::size_t>(row.valueOffsetCandidate) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(row.valueOffsetCandidate)) : buf.size();
                                row.keySampleHex = sampleAt(keyAbs, row.keyLengthCandidate);
                                row.valueSampleHex = sampleAt(valAbs, row.valueLengthCandidate);
                                row.status = (row.keySampleHex.empty() && row.valueSampleHex.empty()) ? "KVLOC_ENTRY_OUT_OF_RANGE_OR_EMPTY" : "KVLOC_ENTRY_SAMPLED";
                                row.interpretation = "Variable-size OMAP B-tree TOC entry sampled as kvloc_t.";
                                row.notes = notes + "; btn_flags=" + std::to_string(btnFlags) + "; toc_start=" + std::to_string(tocStart) + "; key_area_start=" + std::to_string(keyAreaStart) + "; value_area_end=" + std::to_string(valueAreaEnd);
                            } else {
                                row.status = "TOC_ENTRY_BEYOND_BLOCK";
                                row.interpretation = "Candidate TOC entry offset exceeded the sampled B-tree root block.";
                                row.notes = notes + "; toc_start=" + std::to_string(tocStart) + "; table_space_offset=" + std::to_string(tableSpaceOffset);
                            }
                            apfsOmapBtreeTocProbeRows.push_back(row);
                        }
                    };

                    auto addOmapPhysFromBuffer = [&](const std::string& sourceRole,
                                                     std::uint64_t sourceOid,
                                                     std::uint64_t virtualOffset,
                                                     long long rc,
                                                     const std::vector<unsigned char>& buf,
                                                     const std::string& notes) {
                        if (rc <= 0 || buf.size() < 88) return;
                        const std::uint32_t rawType = readLe32(buf, 24);
                        const std::string label = apfsObjectTypeLabel(rawType);
                        if (label != "OBJECT_MAP") return;

                        ApfsOmapPhysProbeRow om;
                        om.sourceRole = sourceRole;
                        om.sourceOid = sourceOid;
                        om.virtualOffset = virtualOffset;
                        om.bytesRead = rc;
                        om.objectOid = readLe64(buf, 8);
                        om.objectXid = readLe64(buf, 16);
                        om.objectTypeRaw = rawType;
                        om.objectTypeLabel = label;
                        om.objectSubtype = readLe32(buf, 28);
                        om.omFlags = readLe32(buf, 32);
                        om.omSnapshotCount = readLe32(buf, 36);
                        om.omTreeType = readLe32(buf, 40);
                        om.omSnapshotTreeType = readLe32(buf, 44);
                        om.omTreeOid = readLe64(buf, 48);
                        om.omSnapshotTreeOid = readLe64(buf, 56);
                        om.omMostRecentSnap = readLe64(buf, 64);
                        om.omPendingRevertMin = readLe64(buf, 72);
                        om.omPendingRevertMax = readLe64(buf, 80);
                        om.status = om.omTreeOid ? "OMAP_PHYS_PARSED" : "OMAP_PHYS_PARSED_TREE_OID_ZERO";
                        om.interpretation = "APFS object-map physical object parsed. om_tree_oid is the B-tree root needed for object-id to physical-address resolution.";
                        om.sampleHex = hexSampleBytes(buf.data(), buf.size() < 96 ? buf.size() : 96);
                        om.notes = notes;
                        apfsOmapPhysProbeRows.push_back(om);

                        if (om.omTreeOid == 0 || nxSummary.blockSize == 0 || om.omTreeOid > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                            addOmapLookupReadinessRow("OMAP_TREE_ROOT", om.omTreeOid, om.objectXid, om.objectOid, om.omTreeOid, 0, 0, "OMAP_TREE_NOT_READ", "om_tree_oid was zero or overflowed the bounded offset calculation.", "om_tree_oid=" + std::to_string(om.omTreeOid));
                            return;
                        }

                        const std::uint64_t treeOffset = om.omTreeOid * static_cast<std::uint64_t>(nxSummary.blockSize);
                        if (finalObjectSize != 0 && treeOffset >= finalObjectSize) {
                            addOmapLookupReadinessRow("OMAP_TREE_ROOT", om.omTreeOid, om.objectXid, om.objectOid, om.omTreeOid, 0, 0, "OMAP_TREE_OFFSET_BEYOND_OBJECT", "om_tree_oid offset exceeds AFF4 virtual object size.", "offset=" + std::to_string(treeOffset));
                            return;
                        }

                        std::vector<unsigned char> tree;
                        std::string treeErr;
                        const long long treeRead = readVirtual(treeOffset, nxSummary.blockSize, tree, treeErr);
                        ApfsOmapBtreeRootProbeRow tr;
                        tr.sourceRole = sourceRole;
                        tr.omapOid = om.objectOid;
                        tr.omTreeOid = om.omTreeOid;
                        tr.virtualOffset = treeOffset;
                        tr.bytesRead = treeRead;
                        if (treeRead > 0 && tree.size() >= 56) {
                            tr.objectOid = readLe64(tree, 8);
                            tr.objectXid = readLe64(tree, 16);
                            tr.objectTypeRaw = readLe32(tree, 24);
                            tr.objectTypeLabel = apfsObjectTypeLabel(tr.objectTypeRaw);
                            tr.objectSubtype = readLe32(tree, 28);
                            tr.btnFlags = readLe16(tree, 32);
                            tr.btnLevel = readLe16(tree, 34);
                            tr.btnNkeys = readLe32(tree, 36);
                            tr.tableSpaceOffset = readLe16(tree, 40);
                            tr.tableSpaceLength = readLe16(tree, 42);
                            tr.freeSpaceOffset = readLe16(tree, 44);
                            tr.freeSpaceLength = readLe16(tree, 46);
                            if (tree.size() >= 4096) {
                                const std::size_t footer = tree.size() - 40U;
                                tr.btFlagsCandidate = readLe32(tree, footer + 0);
                                tr.btNodeSizeCandidate = readLe32(tree, footer + 4);
                                tr.btKeySizeCandidate = readLe32(tree, footer + 8);
                                tr.btValSizeCandidate = readLe32(tree, footer + 12);
                            }
                            tr.status = (tr.objectTypeLabel == "BTREE" || tr.objectTypeLabel == "BTREE_NODE") ? "OMAP_BTREE_ROOT_READ" : "OMAP_TREE_OBJECT_UNEXPECTED_TYPE";
                            tr.interpretation = "Object-map B-tree root/header read from om_tree_oid. Root level determines whether branch traversal is required before OMAP key/value lookup.";
                            tr.sampleHex = hexSampleBytes(tree.data(), tree.size() < 96 ? tree.size() : 96);
                            tr.notes = treeErr.empty() ? "Read through AFF4 virtual object using om_tree_oid * nx_block_size." : treeErr;
                            addBtreeProbeFromBuffer("OMAP_TREE_ROOT", om.omTreeOid, 0, treeOffset, treeRead, tree, "Read from om_tree_oid parsed from APFS object-map physical object.");
                            addOmapBtreeTocProbeRowsFromBuffer("OMAP_TREE_ROOT", om.objectOid, om.omTreeOid, treeOffset, tr.btnLevel, tr.btnNkeys, tree, "Read from om_tree_oid parsed from APFS object-map physical object.");

                            addOmapLookupReadinessRow("NX_OMAP_SELF", om.objectOid, nxSummary.nextXid, om.objectOid, om.omTreeOid, tr.btnLevel, tr.btnNkeys,
                                                      tr.btnLevel == 0 ? "OMAP_LEAF_ROOT_READY_FOR_KV_DECODE" : "OMAP_BRANCH_TRAVERSAL_REQUIRED",
                                                      tr.btnLevel == 0 ? "The object-map B-tree root is a leaf; next implementation can decode OMAP key/value TOC directly." : "The object-map B-tree root is not a leaf; next implementation must traverse branch nodes toward target OIDs.",
                                                      "btn_flags=" + std::to_string(tr.btnFlags));
                            for (std::size_t idx = 0; idx < nxSummary.fsOids.size() && idx < 32U; ++idx) {
                                addOmapLookupReadinessRow("NX_FS_OID_" + std::to_string(idx), nxSummary.fsOids[idx], nxSummary.nextXid, om.objectOid, om.omTreeOid, tr.btnLevel, tr.btnNkeys,
                                                          tr.btnLevel == 0 ? "TARGET_READY_FOR_LEAF_LOOKUP" : "TARGET_REQUIRES_BRANCH_TRAVERSAL",
                                                          "Filesystem object ID is now queued for real OMAP lookup using the parsed object-map B-tree.",
                                                          "target_oid=" + std::to_string(nxSummary.fsOids[idx]) + "; nx_next_xid=" + std::to_string(nxSummary.nextXid));
                            }
                        } else {
                            tr.status = "OMAP_BTREE_ROOT_READ_FAILED";
                            tr.interpretation = "Unable to read object-map B-tree root through libaff4 virtual read.";
                            tr.notes = treeErr.empty() ? "AFF4_read returned no bytes for om_tree_oid." : treeErr;
                            addOmapLookupReadinessRow("OMAP_TREE_ROOT", om.omTreeOid, om.objectXid, om.objectOid, om.omTreeOid, 0, 0, tr.status, tr.interpretation, tr.notes);
                        }
                        apfsOmapBtreeRootProbeRows.push_back(tr);
                    };

                    auto probeApfsObjectId = [&](const std::string& role, std::uint64_t oid) {
                        ApfsObjectIdProbeRow orow;
                        orow.role = role;
                        orow.oid = oid;
                        if (oid == 0) {
                            orow.status = "OID_ZERO_SKIPPED";
                            orow.interpretation = "Object ID was zero and was not probed.";
                            apfsObjectIdProbeRows.push_back(orow);
                            return;
                        }
                        if (nxSummary.blockSize == 0 || oid > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                            orow.status = "OFFSET_OVERFLOW";
                            orow.interpretation = "Object ID could not be safely converted to a bounded direct object-id virtual offset.";
                            orow.notes = "oid=" + std::to_string(oid) + "; block_size=" + std::to_string(nxSummary.blockSize);
                            apfsObjectIdProbeRows.push_back(orow);
                            return;
                        }
                        orow.virtualOffset = oid * static_cast<std::uint64_t>(nxSummary.blockSize);
                        if (finalObjectSize != 0 && orow.virtualOffset >= finalObjectSize) {
                            orow.status = "OFFSET_BEYOND_OBJECT_SIZE";
                            orow.interpretation = "Direct object-ID virtual offset exceeds AFF4 object size; OMAP lookup is required.";
                            orow.notes = "offset=" + std::to_string(orow.virtualOffset) + "; object_size=" + std::to_string(finalObjectSize);
                            apfsObjectIdProbeRows.push_back(orow);
                            return;
                        }
                        std::vector<unsigned char> obj;
                        std::string objErr;
                        orow.bytesRead = readVirtual(orow.virtualOffset, nxSummary.blockSize, obj, objErr);
                        orow.status = orow.bytesRead > 0 ? "READ_OK" : "READ_FAILED";
                        if (orow.bytesRead > 0 && obj.size() >= 36) {
                            orow.objectOid = readLe64(obj, 8);
                            orow.objectXid = readLe64(obj, 16);
                            orow.objectTypeRaw = readLe32(obj, 24);
                            orow.objectSubtype = readLe32(obj, 28);
                            orow.objectTypeLabel = apfsObjectTypeLabel(orow.objectTypeRaw);
                            if (std::memcmp(obj.data() + 32, "OMAP", 4) == 0) orow.magic = "OMAP";
                            else if (std::memcmp(obj.data() + 32, "APSB", 4) == 0) orow.magic = "APSB";
                            else if (std::memcmp(obj.data() + 32, "NXSB", 4) == 0) orow.magic = "NXSB";
                            else orow.magic.assign(reinterpret_cast<const char*>(obj.data() + 32), reinterpret_cast<const char*>(obj.data() + 36));
                            if (orow.magic == "OMAP") orow.interpretation = "Direct object-ID read appears to expose an APFS object map.";
                            else if (orow.magic == "APSB") orow.interpretation = "Direct object-ID read appears to expose an APFS volume superblock.";
                            else if (orow.objectTypeLabel == "BTREE" || orow.objectTypeLabel == "BTREE_NODE") orow.interpretation = "Direct object-ID read exposed a B-tree object; OMAP/B-tree entry decoding is needed.";
                            else orow.interpretation = "Direct object-ID block read completed but did not expose OMAP/APSB/NXSB magic.";
                            orow.sampleHex = hexSampleBytes(obj.data(), obj.size() < 96 ? obj.size() : 96);
                            addBtreeProbeFromBuffer(std::string("DIRECT_OBJECT_ID_") + role, oid, 0, orow.virtualOffset, orow.bytesRead, obj, "Direct object-ID read from NX metadata or filesystem OID list.");
                            addOmapPhysFromBuffer(std::string("DIRECT_OBJECT_ID_") + role, oid, orow.virtualOffset, orow.bytesRead, obj, "Direct object-ID read from NX metadata or filesystem OID list.");
                        } else {
                            orow.interpretation = "Unable to read direct object-ID candidate through libaff4 virtual read.";
                            orow.notes = objErr.empty() ? "AFF4_read returned no bytes for direct object-ID candidate." : objErr;
                        }
                        apfsObjectIdProbeRows.push_back(orow);
                    };


                    auto buildResolvedVolumeSuperblocksAndVolumeOmapProbes = [&]() {
                        auto appendBranchPath = [](std::string& path, const std::string& item) {
                            if (!path.empty()) path += " > ";
                            path += item;
                        };

                        auto safeNodeOffset = [&](std::uint64_t oid, std::uint64_t& offsetOut) -> bool {
                            if (nxSummary.blockSize == 0) return false;
                            if (oid > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) return false;
                            offsetOut = oid * static_cast<std::uint64_t>(nxSummary.blockSize);
                            if (finalObjectSize != 0 && offsetOut >= finalObjectSize) return false;
                            return true;
                        };

                        auto fixedKvAbs = [&](const std::vector<unsigned char>& node,
                                              std::uint32_t entryIndex,
                                              std::size_t valueLenNeeded,
                                              std::size_t& keyAbs,
                                              std::size_t& valAbs,
                                              std::string& detail) -> bool {
                            if (node.size() < 64) { detail = "node too small"; return false; }
                            const std::uint16_t btnFlags = readLe16(node, 32);
                            const bool fixedKv = (btnFlags & 0x0004U) != 0;
                            if (!fixedKv) { detail = "node does not advertise fixed-size key/value offsets"; return false; }
                            const std::uint16_t tableSpaceOffset = readLe16(node, 40);
                            const std::uint16_t tableSpaceLength = readLe16(node, 42);
                            const std::size_t btnDataStart = 56U;
                            std::size_t tocStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset);
                            if (tocStart >= node.size()) tocStart = btnDataStart;
                            std::size_t keyAreaStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset) + static_cast<std::size_t>(tableSpaceLength);
                            const std::uint32_t nkeys = readLe32(node, 36);
                            if (keyAreaStart >= node.size()) keyAreaStart = tocStart + 4U * static_cast<std::size_t>(nkeys);
                            std::size_t valueAreaEnd = node.size();
                            if (valueAreaEnd >= 40U && (btnFlags & 0x0001U) != 0U) valueAreaEnd -= 40U;
                            const std::size_t entryOff = tocStart + (static_cast<std::size_t>(entryIndex) * 4U);
                            if (entryOff + 4U > node.size()) { detail = "TOC entry beyond node buffer"; return false; }
                            const std::uint16_t keyOff = readLe16(node, entryOff + 0U);
                            const std::uint16_t valOff = readLe16(node, entryOff + 2U);
                            keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
                            valAbs = (static_cast<std::size_t>(valOff) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(valOff)) : node.size();
                            if (keyAbs + 16U > node.size()) { detail = "OMAP key outside node buffer"; return false; }
                            if (valAbs + valueLenNeeded > node.size()) { detail = "OMAP value outside node buffer"; return false; }
                            detail = "toc_start=" + std::to_string(tocStart) + "; key_area_start=" + std::to_string(keyAreaStart) + "; value_area_end=" + std::to_string(valueAreaEnd);
                            return true;
                        };

                        auto compareOmapKey = [](std::uint64_t oid, std::uint64_t xid, std::uint64_t targetOid, std::uint64_t targetXid) -> int {
                            if (oid < targetOid) return -1;
                            if (oid > targetOid) return 1;
                            if (xid < targetXid) return -1;
                            if (xid > targetXid) return 1;
                            return 0;
                        };


                        auto resolveVolumeOmapTargetObject = [&](const ApfsVolumeOmapProbeRow& omRow,
                                                                 const std::vector<unsigned char>& rootNode,
                                                                 std::uint64_t targetOid,
                                                                 std::uint64_t targetXid,
                                                                 const std::string& purpose) -> ApfsOmapTargetResolution {
                            ApfsOmapTargetResolution out;
                            out.targetOid = targetOid;
                            out.targetXid = targetXid;
                            if (targetOid == 0) {
                                out.lookupStatus = "OMAP_TARGET_OID_ZERO";
                                out.interpretation = purpose + ": target OID was zero.";
                                return out;
                            }
                            if (rootNode.empty() || omRow.treeStatus != "VOLUME_OMAP_BTREE_ROOT_READ") {
                                out.lookupStatus = "VOLUME_OMAP_TREE_UNAVAILABLE";
                                out.interpretation = purpose + ": volume OMAP B-tree root was not available.";
                                out.notes = "volume_omap_tree_status=" + omRow.treeStatus;
                                return out;
                            }

                            std::vector<unsigned char> node = rootNode;
                            std::uint64_t nodeOid = omRow.omTreeOid;
                            std::uint64_t nodeOffset = omRow.treeVirtualOffset;
                            long long nodeRead = omRow.treeBytesRead;
                            std::vector<unsigned char> nextLeafNodeBuffer;
                            nextLeafNodeBuffer.reserve(static_cast<std::size_t>(nxSummary.blockSize));
                            std::vector<unsigned char> reusableChildNodeBuffer;
                            reusableChildNodeBuffer.reserve(static_cast<std::size_t>(nxSummary.blockSize));
                            std::set<std::uint64_t> visitedNodes;
                            constexpr std::uint32_t kMaxDepth = 8;
                            for (std::uint32_t depth = 0; depth < kMaxDepth; ++depth) {
                                if (!visitedNodes.insert(nodeOid).second) {
                                    out.lookupStatus = "VOLUME_OMAP_VERTICAL_CYCLE_DETECTED";
                                    out.interpretation = purpose + ": volume OMAP B-tree traversal detected a repeated branch/leaf node and stopped.";
                                    aff4ApfsAppendProbeNote(out.notes, "vertical_cycle_detected_oid=" + std::to_string(nodeOid));
                                    break;
                                }
                                out.branchDepth = depth;
                                if (node.size() < 64) {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_TOO_SMALL";
                                    out.interpretation = purpose + ": OMAP node read returned too few bytes for B-tree header parsing.";
                                    break;
                                }
                                const std::uint32_t rawType = readLe32(node, 24);
                                const std::string label = apfsObjectTypeLabel(rawType);
                                const std::uint16_t btnFlags = readLe16(node, 32);
                                const std::uint16_t btnLevel = readLe16(node, 34);
                                const std::uint32_t nkeys = readLe32(node, 36);
                                appendBranchPath(out.branchPath, "oid=" + std::to_string(nodeOid) + ";flags=" + std::to_string(btnFlags) + ";level=" + std::to_string(btnLevel) + ";nkeys=" + std::to_string(nkeys));
                                if (label != "BTREE" && label != "BTREE_NODE") {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_UNEXPECTED_OBJECT_TYPE";
                                    out.interpretation = purpose + ": node in the volume OMAP path was not an APFS B-tree node.";
                                    out.notes = "object_type=" + label;
                                    break;
                                }
                                if (nkeys > 100000U) {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_SUSPICIOUS_KEY_COUNT";
                                    out.interpretation = purpose + ": OMAP node reported an implausibly large key count; traversal stopped.";
                                    out.notes = "nkeys=" + std::to_string(nkeys);
                                    break;
                                }
                                if (btnLevel == 0) {
                                    bool found = false;
                                    std::string bestDetail;
                                    std::set<std::uint64_t> visitedLeaves;
                                    std::uint32_t nextLeafTransitions = 0;
                                    constexpr std::uint32_t kMaxNextLeafTransitions = 256U;
                                    while (true) {
                                        if (!visitedLeaves.insert(nodeOid).second) {
                                            aff4ApfsAppendProbeNote(out.notes, "next_leaf_cycle_detected_oid=" + std::to_string(nodeOid));
                                            break;
                                        }
                                        out.leafOid = nodeOid;
                                        out.leafVirtualOffset = nodeOffset;
                                        out.leafBytesRead = nodeRead;
                                        out.leafBtnFlags = readLe16(node, 32U);
                                        out.leafBtnLevel = readLe16(node, 34U);
                                        out.leafBtnNkeys = readLe32(node, 36U);
                                        out.sampleHex = node.empty() ? std::string{} : hexSampleBytes(node.data(), node.size() < 96 ? node.size() : 96);
                                        std::string firstDecodeNote;
                                        bool cancelled = false;
                                        found = aff4ApfsFindBestOmapLeafEntryForProbe(node,
                                                                                      out.leafBtnNkeys,
                                                                                      targetOid,
                                                                                      targetXid,
                                                                                      out.matchedEntryIndex,
                                                                                      out.matchedKeyOid,
                                                                                      out.matchedKeyXid,
                                                                                      out.valueFlags,
                                                                                      out.valueSize,
                                                                                      out.valuePaddr,
                                                                                      bestDetail,
                                                                                      firstDecodeNote,
                                                                                      dynamicProbeCancelled,
                                                                                      cancelled);
                                        if (cancelled) {
                                            out.lookupStatus = "CANCELLED_BY_USER";
                                            out.interpretation = purpose + ": volume OMAP lookup cancelled by investigator request.";
                                            out.notes = "Cancelled while scanning volume OMAP leaf entries.";
                                            return out;
                                        }
                                        if (!firstDecodeNote.empty() && out.notes.empty()) out.notes = firstDecodeNote;
                                        if (found) break;
                                        if (nextLeafTransitions >= kMaxNextLeafTransitions) {
                                            aff4ApfsAppendProbeNote(out.notes, "next_leaf_transition_limit_reached=" + std::to_string(kMaxNextLeafTransitions));
                                            break;
                                        }
                                        nextLeafNodeBuffer.clear();
                                        std::uint64_t nextNodeOid = 0;
                                        std::uint64_t nextNodeOffset = 0;
                                        long long nextNodeRead = -1;
                                        if (!aff4ApfsLoadNextLeafForProbe(node, nodeOid, nxSummary.blockSize, readVirtual, safeNodeOffset, nextLeafNodeBuffer, nextNodeOid, nextNodeOffset, nextNodeRead, out.notes)) break;
                                        ++nextLeafTransitions;
                                        appendBranchPath(out.branchPath, "next_leaf_oid=" + std::to_string(nextNodeOid) + ";transition=" + std::to_string(nextLeafTransitions));
                                        node.swap(nextLeafNodeBuffer);
                                        nodeOid = nextNodeOid;
                                        nodeOffset = nextNodeOffset;
                                        nodeRead = nextNodeRead;
                                    }
                                    if (!found) {
                                        out.lookupStatus = nextLeafTransitions > 0 ? "OMAP_TARGET_LOOKUP_NO_MATCH_AFTER_NEXT_LEAF_SCAN" : "OMAP_TARGET_LOOKUP_NO_MATCH_IN_LEAF";
                                        out.interpretation = purpose + ": volume OMAP leaf scan reached the end of the bounded horizontal leaf chain without finding a key with xid <= target transaction.";
                                        break;
                                    }
                                    out.lookupStatus = nextLeafTransitions > 0 ? "OMAP_TARGET_LOOKUP_RESOLVED_AFTER_NEXT_LEAF_SCAN" : "OMAP_TARGET_LOOKUP_RESOLVED";
                                    aff4ApfsAppendProbeNote(out.notes, bestDetail);
                                    if (nextLeafTransitions > 0) aff4ApfsAppendProbeNote(out.notes, "next_leaf_transitions=" + std::to_string(nextLeafTransitions));
                                    if ((out.valueFlags & 0x00000001U) != 0U) {
                                        out.objectStatus = "OMAP_TARGET_VALUE_DELETED";
                                        out.interpretation = purpose + ": matching OMAP value was marked deleted; object not read as active.";
                                        break;
                                    }
                                    if (!safeNodeOffset(out.valuePaddr, out.resolvedVirtualOffset)) {
                                        out.objectStatus = "OMAP_TARGET_RESOLVED_OFFSET_UNSAFE";
                                        out.interpretation = purpose + ": matching OMAP value was found, but value_paddr could not be converted to a safe read offset.";
                                        break;
                                    }
                                    std::vector<unsigned char> resolved;
                                    std::string resolvedErr;
                                    out.resolvedBytesRead = readVirtual(out.resolvedVirtualOffset, nxSummary.blockSize, resolved, resolvedErr);
                                    if (out.resolvedBytesRead > 0 && resolved.size() >= 56) {
                                        out.resolvedObjectOid = readLe64(resolved, 8);
                                        out.resolvedObjectXid = readLe64(resolved, 16);
                                        out.resolvedObjectTypeRaw = readLe32(resolved, 24);
                                        out.resolvedObjectTypeLabel = apfsObjectTypeLabel(out.resolvedObjectTypeRaw);
                                        out.resolvedObjectSubtype = readLe32(resolved, 28);
                                        if (resolved.size() >= 36) out.resolvedMagic.assign(reinterpret_cast<const char*>(resolved.data() + 32), reinterpret_cast<const char*>(resolved.data() + 36));
                                        if (out.resolvedObjectTypeLabel == "BTREE" || out.resolvedObjectTypeLabel == "BTREE_NODE") {
                                            out.resolvedBtnFlags = readLe16(resolved, 32);
                                            out.resolvedBtnLevel = readLe16(resolved, 34);
                                            out.resolvedBtnNkeys = readLe32(resolved, 36);
                                            out.objectStatus = "OMAP_TARGET_BTREE_READ";
                                            out.interpretation = purpose + ": target object resolved through the volume OMAP and read as a B-tree node.";
                                        } else {
                                            out.objectStatus = "OMAP_TARGET_UNEXPECTED_OBJECT_TYPE";
                                            out.interpretation = purpose + ": target object resolved through the volume OMAP, but was not a B-tree node.";
                                        }
                                        out.resolvedSampleHex = hexSampleBytes(resolved.data(), resolved.size() < 96 ? resolved.size() : 96);
                                        out.resolvedBuffer.swap(resolved);
                                    } else {
                                        out.objectStatus = "OMAP_TARGET_READ_FAILED";
                                        out.interpretation = purpose + ": matching OMAP value was found, but the resolved object read failed.";
                                        out.notes += out.notes.empty() ? resolvedErr : ("; " + resolvedErr);
                                    }
                                    break;
                                }

                                std::uint64_t childOid = 0;
                                std::uint32_t childEntry = 0;
                                bool childFound = false;
                                bool usedFallbackFirst = false;
                                const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
                                std::uint64_t fallbackChild = 0;
                                std::uint32_t fallbackEntry = 0;
                                for (std::uint32_t i = 0; i < limit; ++i) {
                                    std::size_t keyAbs = 0;
                                    std::size_t valAbs = 0;
                                    std::string detail;
                                    if (!fixedKvAbs(node, i, 8U, keyAbs, valAbs, detail)) {
                                        if (i == 0 && out.notes.empty()) out.notes = detail;
                                        continue;
                                    }
                                    const std::uint64_t keyOid = readLe64(node, keyAbs + 0U);
                                    const std::uint64_t keyXid = readLe64(node, keyAbs + 8U);
                                    const std::uint64_t candidateChild = readLe64(node, valAbs + 0U);
                                    if (i == 0) { fallbackChild = candidateChild; fallbackEntry = i; }
                                    if (compareOmapKey(keyOid, keyXid, targetOid, targetXid) <= 0) {
                                        childOid = candidateChild;
                                        childEntry = i;
                                        childFound = true;
                                    }
                                }
                                if (!childFound && fallbackChild != 0) {
                                    childOid = fallbackChild;
                                    childEntry = fallbackEntry;
                                    childFound = true;
                                    usedFallbackFirst = true;
                                }
                                if (!childFound || childOid == 0) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_NOT_SELECTED";
                                    out.interpretation = purpose + ": OMAP branch node parsed, but no usable child pointer could be selected for the target key.";
                                    break;
                                }
                                appendBranchPath(out.branchPath, "child_entry=" + std::to_string(childEntry) + ";child_oid=" + std::to_string(childOid) + (usedFallbackFirst ? ";fallback_first" : ""));
                                std::uint64_t childOffset = 0;
                                if (!safeNodeOffset(childOid, childOffset)) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_OFFSET_UNSAFE";
                                    out.interpretation = purpose + ": selected OMAP branch child OID could not be converted to a safe read offset.";
                                    break;
                                }
                                reusableChildNodeBuffer.clear();
                                std::string childErr;
                                const long long childRead = readVirtual(childOffset, nxSummary.blockSize, reusableChildNodeBuffer, childErr);
                                if (childRead <= 0 || reusableChildNodeBuffer.size() < 64) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_READ_FAILED";
                                    out.interpretation = purpose + ": selected OMAP branch child could not be read as a B-tree node.";
                                    out.notes += out.notes.empty() ? childErr : ("; " + childErr);
                                    break;
                                }
                                node.swap(reusableChildNodeBuffer);
                                nodeOid = childOid;
                                nodeOffset = childOffset;
                                nodeRead = childRead;
                            }
                            if (out.lookupStatus.empty()) {
                                out.lookupStatus = "VOLUME_OMAP_BRANCH_TRAVERSAL_LIMIT_REACHED";
                                out.interpretation = purpose + ": bounded OMAP branch traversal reached the safety depth limit before a leaf node was resolved.";
                            }
                            return out;
                        };

                        auto decodeVolumeOmapRootTreeLookup = [&](const ApfsResolvedVolumeSuperblockRow& volRow,
                                                                  const ApfsVolumeOmapProbeRow& omRow,
                                                                  const std::vector<unsigned char>& rootNode) {
                            ApfsVolumeRootTreeLookupRow out;
                            out.sequence = static_cast<std::uint32_t>(apfsVolumeRootTreeLookupRows.size());
                            out.volumeSequence = omRow.volumeSequence;
                            out.targetRole = omRow.targetRole;
                            out.fsOid = omRow.fsOid;
                            out.volumeName = volRow.volumeName;
                            out.apfsOmapOid = omRow.apfsOmapOid;
                            out.omTreeOid = omRow.omTreeOid;
                            out.apfsRootTreeOid = omRow.apfsRootTreeOid;
                            out.targetXid = volRow.objectXid;

                            if (out.apfsRootTreeOid == 0) {
                                out.lookupStatus = "VOLUME_ROOT_TREE_OID_ZERO";
                                out.interpretation = "Parsed APSB did not contain an apfs_root_tree_oid target.";
                                apfsVolumeRootTreeLookupRows.push_back(out);
                                return;
                            }
                            if (rootNode.empty() || omRow.treeStatus != "VOLUME_OMAP_BTREE_ROOT_READ") {
                                out.lookupStatus = "VOLUME_OMAP_TREE_UNAVAILABLE";
                                out.interpretation = "Volume OMAP B-tree root was not available, so apfs_root_tree_oid could not be resolved.";
                                out.notes = "volume_omap_tree_status=" + omRow.treeStatus;
                                apfsVolumeRootTreeLookupRows.push_back(out);
                                return;
                            }

                            std::vector<unsigned char> node = rootNode;
                            std::uint64_t nodeOid = omRow.omTreeOid;
                            std::uint64_t nodeOffset = omRow.treeVirtualOffset;
                            long long nodeRead = omRow.treeBytesRead;
                            std::vector<unsigned char> nextLeafNodeBuffer;
                            nextLeafNodeBuffer.reserve(static_cast<std::size_t>(nxSummary.blockSize));
                            std::vector<unsigned char> reusableChildNodeBuffer;
                            reusableChildNodeBuffer.reserve(static_cast<std::size_t>(nxSummary.blockSize));
                            std::set<std::uint64_t> visitedNodes;
                            constexpr std::uint32_t kMaxDepth = 8;
                            for (std::uint32_t depth = 0; depth < kMaxDepth; ++depth) {
                                if (!visitedNodes.insert(nodeOid).second) {
                                    out.lookupStatus = "VOLUME_OMAP_VERTICAL_CYCLE_DETECTED";
                                    out.interpretation = "Volume OMAP B-tree traversal detected a repeated branch/leaf node and stopped.";
                                    aff4ApfsAppendProbeNote(out.notes, "vertical_cycle_detected_oid=" + std::to_string(nodeOid));
                                    break;
                                }
                                out.branchDepth = depth;
                                if (node.size() < 64) {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_TOO_SMALL";
                                    out.interpretation = "A volume OMAP node read returned too few bytes for B-tree header parsing.";
                                    break;
                                }
                                const std::uint32_t rawType = readLe32(node, 24);
                                const std::string label = apfsObjectTypeLabel(rawType);
                                const std::uint16_t btnFlags = readLe16(node, 32);
                                const std::uint16_t btnLevel = readLe16(node, 34);
                                const std::uint32_t nkeys = readLe32(node, 36);
                                appendBranchPath(out.branchPath, "oid=" + std::to_string(nodeOid) + ";flags=" + std::to_string(btnFlags) + ";level=" + std::to_string(btnLevel) + ";nkeys=" + std::to_string(nkeys));
                                if (label != "BTREE" && label != "BTREE_NODE") {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_UNEXPECTED_OBJECT_TYPE";
                                    out.interpretation = "A node in the volume OMAP path was not an APFS B-tree node.";
                                    out.notes = "object_type=" + label;
                                    break;
                                }
                                if (nkeys > 100000U) {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_SUSPICIOUS_KEY_COUNT";
                                    out.interpretation = "A volume OMAP node reported an implausibly large key count; traversal stopped.";
                                    out.notes = "nkeys=" + std::to_string(nkeys);
                                    break;
                                }
                                if (btnLevel == 0) {
                                    bool found = false;
                                    std::string bestDetail;
                                    std::set<std::uint64_t> visitedLeaves;
                                    std::uint32_t nextLeafTransitions = 0;
                                    constexpr std::uint32_t kMaxNextLeafTransitions = 256U;
                                    while (true) {
                                        if (!visitedLeaves.insert(nodeOid).second) {
                                            aff4ApfsAppendProbeNote(out.notes, "next_leaf_cycle_detected_oid=" + std::to_string(nodeOid));
                                            break;
                                        }
                                        out.leafOid = nodeOid;
                                        out.leafVirtualOffset = nodeOffset;
                                        out.leafBytesRead = nodeRead;
                                        out.leafBtnFlags = readLe16(node, 32U);
                                        out.leafBtnLevel = readLe16(node, 34U);
                                        out.leafBtnNkeys = readLe32(node, 36U);
                                        out.sampleHex = node.empty() ? std::string{} : hexSampleBytes(node.data(), node.size() < 96 ? node.size() : 96);
                                        std::string firstDecodeNote;
                                        bool cancelled = false;
                                        found = aff4ApfsFindBestOmapLeafEntryForProbe(node,
                                                                                      out.leafBtnNkeys,
                                                                                      out.apfsRootTreeOid,
                                                                                      out.targetXid,
                                                                                      out.matchedEntryIndex,
                                                                                      out.matchedKeyOid,
                                                                                      out.matchedKeyXid,
                                                                                      out.valueFlags,
                                                                                      out.valueSize,
                                                                                      out.valuePaddr,
                                                                                      bestDetail,
                                                                                      firstDecodeNote,
                                                                                      dynamicProbeCancelled,
                                                                                      cancelled);
                                        if (cancelled) {
                                            out.lookupStatus = "CANCELLED_BY_USER";
                                            out.interpretation = "Volume root-tree lookup cancelled by investigator request.";
                                            out.notes = "Cancelled while scanning volume OMAP leaf entries.";
                                            break;
                                        }
                                        if (!firstDecodeNote.empty() && out.notes.empty()) out.notes = firstDecodeNote;
                                        if (found) break;
                                        if (nextLeafTransitions >= kMaxNextLeafTransitions) {
                                            aff4ApfsAppendProbeNote(out.notes, "next_leaf_transition_limit_reached=" + std::to_string(kMaxNextLeafTransitions));
                                            break;
                                        }
                                        nextLeafNodeBuffer.clear();
                                        std::uint64_t nextNodeOid = 0;
                                        std::uint64_t nextNodeOffset = 0;
                                        long long nextNodeRead = -1;
                                        if (!aff4ApfsLoadNextLeafForProbe(node, nodeOid, nxSummary.blockSize, readVirtual, safeNodeOffset, nextLeafNodeBuffer, nextNodeOid, nextNodeOffset, nextNodeRead, out.notes)) break;
                                        ++nextLeafTransitions;
                                        appendBranchPath(out.branchPath, "next_leaf_oid=" + std::to_string(nextNodeOid) + ";transition=" + std::to_string(nextLeafTransitions));
                                        node.swap(nextLeafNodeBuffer);
                                        nodeOid = nextNodeOid;
                                        nodeOffset = nextNodeOffset;
                                        nodeRead = nextNodeRead;
                                    }
                                    if (!found) {
                                        if (out.lookupStatus.empty()) out.lookupStatus = nextLeafTransitions > 0 ? "VOLUME_ROOT_TREE_LOOKUP_NO_MATCH_AFTER_NEXT_LEAF_SCAN" : "VOLUME_ROOT_TREE_LOOKUP_NO_MATCH_IN_LEAF";
                                        out.interpretation = "Volume OMAP leaf scan reached the end of the bounded horizontal leaf chain without finding apfs_root_tree_oid with xid <= APSB xid.";
                                        break;
                                    }
                                    out.lookupStatus = nextLeafTransitions > 0 ? "VOLUME_ROOT_TREE_LOOKUP_RESOLVED_AFTER_NEXT_LEAF_SCAN" : "VOLUME_ROOT_TREE_LOOKUP_RESOLVED";
                                    aff4ApfsAppendProbeNote(out.notes, bestDetail);
                                    if (nextLeafTransitions > 0) aff4ApfsAppendProbeNote(out.notes, "next_leaf_transitions=" + std::to_string(nextLeafTransitions));
                                    if ((out.valueFlags & 0x00000001U) != 0U) {
                                        out.rootTreeStatus = "ROOT_TREE_OMAP_VALUE_DELETED";
                                        out.interpretation = "Matching OMAP value was marked deleted; root-tree object not read as active.";
                                        break;
                                    }
                                    if (!safeNodeOffset(out.valuePaddr, out.resolvedVirtualOffset)) {
                                        out.rootTreeStatus = "ROOT_TREE_RESOLVED_OFFSET_UNSAFE";
                                        out.interpretation = "Matching OMAP value was found, but value_paddr could not be converted to a safe read offset.";
                                        break;
                                    }
                                    std::vector<unsigned char> resolved;
                                    std::string resolvedErr;
                                    out.resolvedBytesRead = readVirtual(out.resolvedVirtualOffset, nxSummary.blockSize, resolved, resolvedErr);
                                    if (out.resolvedBytesRead > 0 && resolved.size() >= 56) {
                                        out.resolvedObjectOid = readLe64(resolved, 8);
                                        out.resolvedObjectXid = readLe64(resolved, 16);
                                        out.resolvedObjectTypeRaw = readLe32(resolved, 24);
                                        out.resolvedObjectTypeLabel = apfsObjectTypeLabel(out.resolvedObjectTypeRaw);
                                        out.resolvedObjectSubtype = readLe32(resolved, 28);
                                        if (resolved.size() >= 36) out.resolvedMagic.assign(reinterpret_cast<const char*>(resolved.data() + 32), reinterpret_cast<const char*>(resolved.data() + 36));
                                        if (out.resolvedObjectTypeLabel == "BTREE" || out.resolvedObjectTypeLabel == "BTREE_NODE") {
                                            out.resolvedBtnFlags = readLe16(resolved, 32);
                                            out.resolvedBtnLevel = readLe16(resolved, 34);
                                            out.resolvedBtnNkeys = readLe32(resolved, 36);
                                            out.rootTreeStatus = "ROOT_TREE_BTREE_READ";
                                            out.interpretation = "apfs_root_tree_oid resolved through the volume OMAP and the root-tree B-tree header was read.";
                                        } else {
                                            out.rootTreeStatus = "ROOT_TREE_UNEXPECTED_OBJECT_TYPE";
                                            out.interpretation = "apfs_root_tree_oid resolved through the volume OMAP, but the resolved object was not a B-tree header.";
                                        }
                                        out.resolvedSampleHex = hexSampleBytes(resolved.data(), resolved.size() < 96 ? resolved.size() : 96);
                                    } else {
                                        out.rootTreeStatus = "ROOT_TREE_READ_FAILED";
                                        out.interpretation = "Matching OMAP value was found, but the resolved root-tree object read failed.";
                                        out.notes += out.notes.empty() ? resolvedErr : ("; " + resolvedErr);
                                    }
                                    break;
                                }

                                std::uint64_t childOid = 0;
                                std::uint32_t childEntry = 0;
                                bool childFound = false;
                                bool usedFallbackFirst = false;
                                const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
                                std::uint64_t fallbackChild = 0;
                                std::uint32_t fallbackEntry = 0;
                                for (std::uint32_t i = 0; i < limit; ++i) {
                                    std::size_t keyAbs = 0;
                                    std::size_t valAbs = 0;
                                    std::string detail;
                                    if (!fixedKvAbs(node, i, 8U, keyAbs, valAbs, detail)) {
                                        if (i == 0 && out.notes.empty()) out.notes = detail;
                                        continue;
                                    }
                                    const std::uint64_t keyOid = readLe64(node, keyAbs + 0U);
                                    const std::uint64_t keyXid = readLe64(node, keyAbs + 8U);
                                    const std::uint64_t candidateChild = readLe64(node, valAbs + 0U);
                                    if (i == 0) { fallbackChild = candidateChild; fallbackEntry = i; }
                                    if (compareOmapKey(keyOid, keyXid, out.apfsRootTreeOid, out.targetXid) <= 0) {
                                        childOid = candidateChild;
                                        childEntry = i;
                                        childFound = true;
                                    }
                                }
                                if (!childFound && fallbackChild != 0) {
                                    childOid = fallbackChild;
                                    childEntry = fallbackEntry;
                                    childFound = true;
                                    usedFallbackFirst = true;
                                }
                                if (!childFound || childOid == 0) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_NOT_SELECTED";
                                    out.interpretation = "Branch node parsed, but no usable child pointer could be selected for the target key.";
                                    break;
                                }
                                appendBranchPath(out.branchPath, "child_entry=" + std::to_string(childEntry) + ";child_oid=" + std::to_string(childOid) + (usedFallbackFirst ? ";fallback_first" : ""));
                                std::uint64_t childOffset = 0;
                                if (!safeNodeOffset(childOid, childOffset)) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_OFFSET_UNSAFE";
                                    out.interpretation = "Selected branch child OID could not be converted to a safe read offset.";
                                    break;
                                }
                                reusableChildNodeBuffer.clear();
                                std::string childErr;
                                const long long childRead = readVirtual(childOffset, nxSummary.blockSize, reusableChildNodeBuffer, childErr);
                                if (childRead <= 0 || reusableChildNodeBuffer.size() < 64) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_READ_FAILED";
                                    out.interpretation = "Selected branch child could not be read as a B-tree node.";
                                    out.notes += out.notes.empty() ? childErr : ("; " + childErr);
                                    break;
                                }
                                node.swap(reusableChildNodeBuffer);
                                nodeOid = childOid;
                                nodeOffset = childOffset;
                                nodeRead = childRead;
                            }
                            if (out.lookupStatus.empty()) {
                                out.lookupStatus = "VOLUME_OMAP_BRANCH_TRAVERSAL_LIMIT_REACHED";
                                out.interpretation = "Bounded branch traversal reached the safety depth limit before a leaf node was resolved.";
                            }
                            apfsVolumeRootTreeLookupRows.push_back(out);
                        };

                        auto appendVolumeOmapAndRootLookup = [&](const ApfsResolvedVolumeSuperblockRow& volRow,
                                                                 const ApfsVolumeOmapProbeRow& omRow,
                                                                 const std::vector<unsigned char>& rootNode) {
                            apfsVolumeOmapRowsByVolumeSequence[omRow.volumeSequence] = omRow;
                            if (!rootNode.empty()) apfsVolumeOmapRootNodesByVolumeSequence[omRow.volumeSequence] = rootNode;
                            apfsVolumeOmapProbeRows.push_back(omRow);
                            decodeVolumeOmapRootTreeLookup(volRow, omRow, rootNode);
                        };

                        auto genericBtreeKvAbs = [&](const std::vector<unsigned char>& node,
                                                    std::uint32_t entryIndex,
                                                    std::size_t& tocAbs,
                                                    std::size_t& keyAbs,
                                                    std::size_t& keyLen,
                                                    std::size_t& valAbs,
                                                    std::size_t& valLen,
                                                    std::string& detail) -> bool {
                            if (node.size() < 64) { detail = "node too small"; return false; }
                            const std::uint16_t btnFlags = readLe16(node, 32);
                            const bool fixedKv = (btnFlags & 0x0004U) != 0;
                            const std::uint16_t tableSpaceOffset = readLe16(node, 40);
                            const std::uint16_t tableSpaceLength = readLe16(node, 42);
                            const std::size_t btnDataStart = 56U;
                            const std::uint32_t nkeys = readLe32(node, 36);
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
                                const std::uint16_t keyOff = readLe16(node, tocAbs + 0U);
                                const std::uint16_t valOff = readLe16(node, tocAbs + 2U);
                                keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
                                keyLen = 16U;
                                valAbs = (static_cast<std::size_t>(valOff) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(valOff)) : node.size();
                                valLen = (valAbs + 16U <= node.size()) ? 16U : 0U;
                            } else {
                                const std::uint16_t keyOff = readLe16(node, tocAbs + 0U);
                                const std::uint16_t keyLength = readLe16(node, tocAbs + 2U);
                                const std::uint16_t valOff = readLe16(node, tocAbs + 4U);
                                const std::uint16_t valLength = readLe16(node, tocAbs + 6U);
                                keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
                                keyLen = keyLength;
                                valAbs = (static_cast<std::size_t>(valOff) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(valOff)) : node.size();
                                valLen = valLength;
                            }
                            if (keyAbs >= node.size()) { detail = "key outside node buffer"; return false; }
                            if (keyLen == 0 || keyAbs + keyLen > node.size()) { detail = "key length outside node buffer"; return false; }
                            if (valLen > 0 && (valAbs >= node.size() || valAbs + valLen > node.size())) { detail = "value length outside node buffer"; return false; }
                            detail = "toc_start=" + std::to_string(tocStart) + "; key_area_start=" + std::to_string(keyAreaStart) + "; value_area_end=" + std::to_string(valueAreaEnd) + (fixedKv ? "; fixed_kv=true" : "; fixed_kv=false");
                            return true;
                        };

                        std::set<std::string> seenResolved;
                        for (const auto& kv : apfsOmapLeafKvDecodeRows) {
                            if (kv.targetRole.rfind("NX_FS_OID_", 0) != 0) continue;
                            if (kv.lookupStatus != "MATCHES_TARGET_OID_XID_BOUNDS") continue;
                            const std::string seenKey = kv.targetRole + ":" + std::to_string(kv.keyOid) + ":" + std::to_string(kv.keyXid) + ":" + std::to_string(kv.valuePaddr);
                            if (!seenResolved.insert(seenKey).second) continue;

                            ApfsResolvedVolumeSuperblockRow row;
                            row.sequence = static_cast<std::uint32_t>(apfsResolvedVolumeSuperblockRows.size());
                            row.targetRole = kv.targetRole;
                            row.fsOid = kv.keyOid;
                            row.containerTargetXid = nxSummary.nextXid;
                            row.omapKeyOid = kv.keyOid;
                            row.omapKeyXid = kv.keyXid;
                            row.omapValueFlags = kv.valueFlags;
                            row.omapValueSize = kv.valueSize;
                            row.omapValuePaddr = kv.valuePaddr;
                            row.resolvedVirtualOffset = kv.resolvedVirtualOffset;
                            if ((kv.valueFlags & 0x00000001U) != 0U) {
                                row.notes = "OMAP value has OMAP_VAL_DELETED set; parsed only for forensic visibility.";
                            }
                            if (nxSummary.blockSize == 0 || kv.valuePaddr > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                row.status = "RESOLVED_APSB_OFFSET_OVERFLOW";
                                row.interpretation = "The selected OMAP value physical address could not be converted to a safe virtual offset.";
                                row.notes += row.notes.empty() ? "" : "; ";
                                row.notes += "value_paddr=" + std::to_string(kv.valuePaddr) + "; block_size=" + std::to_string(nxSummary.blockSize);
                                apfsResolvedVolumeSuperblockRows.push_back(row);
                                continue;
                            }
                            if (row.resolvedVirtualOffset == 0) row.resolvedVirtualOffset = kv.valuePaddr * static_cast<std::uint64_t>(nxSummary.blockSize);
                            if (finalObjectSize != 0 && row.resolvedVirtualOffset >= finalObjectSize) {
                                row.status = "RESOLVED_APSB_OFFSET_BEYOND_OBJECT";
                                row.interpretation = "The selected OMAP value physical address points beyond the reported AFF4 virtual object size.";
                                row.notes += row.notes.empty() ? "" : "; ";
                                row.notes += "resolved_offset=" + std::to_string(row.resolvedVirtualOffset) + "; object_size=" + std::to_string(finalObjectSize);
                                apfsResolvedVolumeSuperblockRows.push_back(row);
                                continue;
                            }
                            std::vector<unsigned char> vol;
                            std::string volErr;
                            row.resolvedBytesRead = readVirtual(row.resolvedVirtualOffset, nxSummary.blockSize, vol, volErr);
                            parseResolvedApfsVolumeSuperblock(row, vol, volErr.empty() ? std::string("Read through container OMAP-selected value_paddr.") : volErr);
                            const std::uint32_t volumeSequence = row.sequence;
                            apfsResolvedVolumeSuperblockRows.push_back(row);

                            if (row.status != "APSB_PARSED_FROM_CONTAINER_OMAP") continue;

                            ApfsVolumeOmapProbeRow omRow;
                            omRow.sequence = static_cast<std::uint32_t>(apfsVolumeOmapProbeRows.size());
                            omRow.volumeSequence = volumeSequence;
                            omRow.targetRole = row.targetRole;
                            omRow.fsOid = row.fsOid;
                            omRow.volumeObjectOid = row.objectOid;
                            omRow.volumeObjectXid = row.objectXid;
                            omRow.apfsOmapOid = row.apfsOmapOid;
                            omRow.apfsRootTreeOid = row.rootTreeOid;
                            if (row.apfsOmapOid == 0) {
                                omRow.omapStatus = "VOLUME_OMAP_OID_ZERO";
                                omRow.interpretation = "Parsed APSB did not contain an apfs_omap_oid value.";
                                appendVolumeOmapAndRootLookup(row, omRow, std::vector<unsigned char>{});
                                continue;
                            }
                            if (nxSummary.blockSize == 0 || row.apfsOmapOid > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                omRow.omapStatus = "VOLUME_OMAP_OFFSET_OVERFLOW";
                                omRow.interpretation = "apfs_omap_oid could not be converted to a bounded physical-block offset.";
                                omRow.notes = "apfs_omap_oid=" + std::to_string(row.apfsOmapOid) + "; block_size=" + std::to_string(nxSummary.blockSize);
                                appendVolumeOmapAndRootLookup(row, omRow, std::vector<unsigned char>{});
                                continue;
                            }
                            omRow.omapVirtualOffset = row.apfsOmapOid * static_cast<std::uint64_t>(nxSummary.blockSize);
                            if (finalObjectSize != 0 && omRow.omapVirtualOffset >= finalObjectSize) {
                                omRow.omapStatus = "VOLUME_OMAP_OFFSET_BEYOND_OBJECT";
                                omRow.interpretation = "apfs_omap_oid physical-block offset exceeds the AFF4 virtual object size.";
                                omRow.notes = "offset=" + std::to_string(omRow.omapVirtualOffset) + "; object_size=" + std::to_string(finalObjectSize);
                                appendVolumeOmapAndRootLookup(row, omRow, std::vector<unsigned char>{});
                                continue;
                            }
                            std::vector<unsigned char> omapBuf;
                            std::string omapErr;
                            omRow.omapBytesRead = readVirtual(omRow.omapVirtualOffset, nxSummary.blockSize, omapBuf, omapErr);
                            std::vector<unsigned char> omapTreeRootBuf;
                            if (omRow.omapBytesRead > 0 && omapBuf.size() >= 88) {
                                omRow.omapObjectOid = readLe64(omapBuf, 8);
                                omRow.omapObjectXid = readLe64(omapBuf, 16);
                                omRow.omapObjectTypeRaw = readLe32(omapBuf, 24);
                                omRow.omapObjectTypeLabel = apfsObjectTypeLabel(omRow.omapObjectTypeRaw);
                                omRow.omapObjectSubtype = readLe32(omapBuf, 28);
                                omRow.omFlags = readLe32(omapBuf, 32);
                                omRow.omSnapshotCount = readLe32(omapBuf, 36);
                                omRow.omTreeType = readLe32(omapBuf, 40);
                                omRow.omSnapshotTreeType = readLe32(omapBuf, 44);
                                omRow.omTreeOid = readLe64(omapBuf, 48);
                                omRow.omSnapshotTreeOid = readLe64(omapBuf, 56);
                                omRow.omMostRecentSnap = readLe64(omapBuf, 64);
                                omRow.omPendingRevertMin = readLe64(omapBuf, 72);
                                omRow.omPendingRevertMax = readLe64(omapBuf, 80);
                                omRow.sampleHex = hexSampleBytes(omapBuf.data(), omapBuf.size() < 96 ? omapBuf.size() : 96);
                                if (omRow.omapObjectTypeLabel == "OBJECT_MAP") {
                                    omRow.omapStatus = "VOLUME_OMAP_PARSED";
                                    omRow.interpretation = "Volume object-map physical object parsed from apfs_omap_oid. om_tree_oid is ready for volume-level object lookup.";
                                } else {
                                    omRow.omapStatus = "VOLUME_OMAP_UNEXPECTED_OBJECT_TYPE";
                                    omRow.interpretation = "apfs_omap_oid was read, but the object header type was not OBJECT_MAP.";
                                }
                                if (omRow.omTreeOid != 0 && nxSummary.blockSize != 0 && omRow.omTreeOid <= ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                    omRow.treeVirtualOffset = omRow.omTreeOid * static_cast<std::uint64_t>(nxSummary.blockSize);
                                    if (finalObjectSize == 0 || omRow.treeVirtualOffset < finalObjectSize) {
                                        std::vector<unsigned char> treeBuf;
                                        std::string treeErr;
                                        omRow.treeBytesRead = readVirtual(omRow.treeVirtualOffset, nxSummary.blockSize, treeBuf, treeErr);
                                        if (omRow.treeBytesRead > 0 && treeBuf.size() >= 56) {
                                            omRow.treeObjectOid = readLe64(treeBuf, 8);
                                            omRow.treeObjectXid = readLe64(treeBuf, 16);
                                            omRow.treeObjectTypeRaw = readLe32(treeBuf, 24);
                                            omRow.treeObjectTypeLabel = apfsObjectTypeLabel(omRow.treeObjectTypeRaw);
                                            omRow.treeObjectSubtype = readLe32(treeBuf, 28);
                                            omRow.treeBtnFlags = readLe16(treeBuf, 32);
                                            omRow.treeBtnLevel = readLe16(treeBuf, 34);
                                            omRow.treeBtnNkeys = readLe32(treeBuf, 36);
                                            omRow.treeTableSpaceOffset = readLe16(treeBuf, 40);
                                            omRow.treeTableSpaceLength = readLe16(treeBuf, 42);
                                            omRow.treeSampleHex = hexSampleBytes(treeBuf.data(), treeBuf.size() < 96 ? treeBuf.size() : 96);
                                            omRow.treeStatus = (omRow.treeObjectTypeLabel == "BTREE" || omRow.treeObjectTypeLabel == "BTREE_NODE") ? "VOLUME_OMAP_BTREE_ROOT_READ" : "VOLUME_OMAP_TREE_UNEXPECTED_OBJECT_TYPE";
                                            if (omRow.treeStatus == "VOLUME_OMAP_BTREE_ROOT_READ") {
                                                omapTreeRootBuf = treeBuf;
                                            }
                                        } else {
                                            omRow.treeStatus = "VOLUME_OMAP_BTREE_ROOT_READ_FAILED";
                                            omRow.notes += omRow.notes.empty() ? "" : "; ";
                                            omRow.notes += treeErr.empty() ? std::string("AFF4_read returned no bytes for volume OMAP om_tree_oid.") : treeErr;
                                        }
                                    } else {
                                        omRow.treeStatus = "VOLUME_OMAP_TREE_OFFSET_BEYOND_OBJECT";
                                    }
                                } else if (omRow.omTreeOid == 0) {
                                    omRow.treeStatus = "VOLUME_OMAP_TREE_OID_ZERO";
                                } else {
                                    omRow.treeStatus = "VOLUME_OMAP_TREE_OFFSET_OVERFLOW";
                                }
                            } else {
                                omRow.omapStatus = "VOLUME_OMAP_READ_FAILED";
                                omRow.interpretation = "Unable to read volume object-map physical object through libaff4 virtual read.";
                                omRow.notes = omapErr.empty() ? "AFF4_read returned no bytes for apfs_omap_oid." : omapErr;
                            }
                            appendVolumeOmapAndRootLookup(row, omRow, omapTreeRootBuf);
                        }

                        constexpr std::uint32_t kMaxRootTreeRecordSamplesPerVolume = 32;
                        std::vector<unsigned char> rootTreeProbeNodeBuffer;
                        rootTreeProbeNodeBuffer.reserve(static_cast<std::size_t>(nxSummary.blockSize));
                        for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                            ApfsRootTreeNodeProbeRow nr;
                            nr.sequence = static_cast<std::uint32_t>(apfsRootTreeNodeProbeRows.size());
                            nr.volumeSequence = lookup.volumeSequence;
                            nr.targetRole = lookup.targetRole;
                            nr.fsOid = lookup.fsOid;
                            nr.volumeName = lookup.volumeName;
                            nr.apfsRootTreeOid = lookup.apfsRootTreeOid;
                            nr.targetXid = lookup.targetXid;
                            nr.nodeOid = lookup.resolvedObjectOid ? lookup.resolvedObjectOid : lookup.apfsRootTreeOid;
                            nr.virtualOffset = lookup.resolvedVirtualOffset;
                            nr.bytesRead = lookup.resolvedBytesRead;
                            if (lookup.rootTreeStatus != "ROOT_TREE_BTREE_READ" || lookup.resolvedVirtualOffset == 0) {
                                nr.status = "ROOT_TREE_NODE_NOT_AVAILABLE";
                                nr.interpretation = "No readable APFS filesystem root-tree B-tree header was available for bounded key sampling.";
                                nr.notes = "root_tree_status=" + lookup.rootTreeStatus + "; lookup_status=" + lookup.lookupStatus;
                                apfsRootTreeNodeProbeRows.push_back(nr);
                                continue;
                            }
                            std::vector<unsigned char>& node = rootTreeProbeNodeBuffer;
                            node.clear();
                            std::string readErr;
                            const long long rc = readVirtual(lookup.resolvedVirtualOffset, nxSummary.blockSize, node, readErr);
                            nr.bytesRead = rc;
                            if (rc <= 0 || node.size() < 64) {
                                nr.status = "ROOT_TREE_NODE_READ_FAILED";
                                nr.interpretation = "Resolved root-tree offset could not be read again for node/key sampling.";
                                nr.notes = readErr;
                                apfsRootTreeNodeProbeRows.push_back(nr);
                                continue;
                            }
                            nr.objectOid = readLe64(node, 8);
                            nr.objectXid = readLe64(node, 16);
                            nr.objectTypeRaw = readLe32(node, 24);
                            nr.objectTypeLabel = apfsObjectTypeLabel(nr.objectTypeRaw);
                            nr.objectSubtype = readLe32(node, 28);
                            if (node.size() >= 36) nr.magic.assign(reinterpret_cast<const char*>(node.data() + 32), reinterpret_cast<const char*>(node.data() + 36));
                            nr.btnFlags = readLe16(node, 32);
                            nr.btnLevel = readLe16(node, 34);
                            nr.btnNkeys = readLe32(node, 36);
                            nr.tableSpaceOffset = readLe16(node, 40);
                            nr.tableSpaceLength = readLe16(node, 42);
                            nr.freeSpaceOffset = readLe16(node, 44);
                            nr.freeSpaceLength = readLe16(node, 46);
                            nr.sampleHex = hexSampleBytes(node.data(), node.size() < 96 ? node.size() : 96);
                            if (nr.objectTypeLabel == "BTREE" || nr.objectTypeLabel == "BTREE_NODE") {
                                nr.status = "ROOT_TREE_NODE_HEADER_PARSED";
                                nr.interpretation = "APFS filesystem root-tree B-tree node header parsed from the resolved volume root-tree object.";
                            } else {
                                nr.status = "ROOT_TREE_NODE_UNEXPECTED_OBJECT_TYPE";
                                nr.interpretation = "Resolved root-tree object was readable but did not parse as an APFS B-tree node.";
                                apfsRootTreeNodeProbeRows.push_back(nr);
                                continue;
                            }
                            apfsRootTreeNodeProbeRows.push_back(nr);

                            const std::uint32_t limit = std::min<std::uint32_t>(nr.btnNkeys, kMaxRootTreeRecordSamplesPerVolume);
                            for (std::uint32_t i = 0; i < limit; ++i) {
                                std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                std::string detail;
                                ApfsRootTreeRecordSampleRow rr;
                                rr.sequence = static_cast<std::uint32_t>(apfsRootTreeRecordSampleRows.size());
                                rr.volumeSequence = lookup.volumeSequence;
                                rr.targetRole = lookup.targetRole;
                                rr.fsOid = lookup.fsOid;
                                rr.volumeName = lookup.volumeName;
                                rr.apfsRootTreeOid = lookup.apfsRootTreeOid;
                                rr.nodeOid = nr.nodeOid;
                                rr.nodeVirtualOffset = lookup.resolvedVirtualOffset;
                                rr.nodeLevel = nr.btnLevel;
                                rr.nodeNkeys = nr.btnNkeys;
                                rr.entryIndex = i;
                                if (!genericBtreeKvAbs(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) {
                                    rr.status = "ROOT_TREE_RECORD_TOC_DECODE_FAILED";
                                    rr.interpretation = "The root-tree node TOC entry could not be decoded safely.";
                                    rr.notes = detail;
                                    apfsRootTreeRecordSampleRows.push_back(rr);
                                    continue;
                                }
                                rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                                rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                                rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                                rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                                rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                                rr.keySampleHex = hexSampleBytes(node.data() + keyAbs, std::min<std::size_t>(keyLen, 64U));
                                if (valLen > 0 && valAbs < node.size()) rr.valueSampleHex = hexSampleBytes(node.data() + valAbs, std::min<std::size_t>(valLen, 64U));
                                if (valLen >= 8U && valAbs + 8U <= node.size()) rr.valueU64_0 = readLe64(node, valAbs);
                                if (valLen >= 16U && valAbs + 16U <= node.size()) rr.valueU64_1 = readLe64(node, valAbs + 8U);
                                if (valLen >= 24U && valAbs + 24U <= node.size()) rr.valueU64_2 = readLe64(node, valAbs + 16U);
                                if (keyLen >= 8U) {
                                    rr.keyRaw = readLe64(node, keyAbs);
                                    rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                                    rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                                    rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                                }
                                if (nr.btnLevel > 0 && valLen >= 8U) rr.branchChildOid = readLe64(node, valAbs);
                            if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                                    const std::uint32_t nameLenAndHash = readLe32(node, keyAbs + 8U);
                                    const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                                    rr.decodedName = safePrintableUtf8Fragment(node, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                                }
                                rr.status = "ROOT_TREE_RECORD_SAMPLE_DECODED";
                                rr.interpretation = nr.btnLevel > 0 ? "Root-tree branch/root record sampled; branch_child_oid should be resolved through the same volume OMAP in the next traversal step." : "Root-tree leaf record sampled; generic APFS filesystem key object/type fields were decoded where possible.";
                                rr.notes = detail;

                                apfsRootTreeRecordSampleRows.push_back(rr);
                            }
                        }

                        constexpr std::uint32_t kMaxRootTreeChildNodes = 96;
                        constexpr std::uint32_t kMaxChildRecordSamplesPerNode = 16;
                        std::set<std::string> seenChildTargets;
                        for (const auto& parentRecord : apfsRootTreeRecordSampleRows) {
                            if (parentRecord.branchChildOid == 0) continue;
                            if (apfsRootTreeChildNodeProbeRows.size() >= kMaxRootTreeChildNodes) break;
                            const std::string childKey = std::to_string(parentRecord.volumeSequence) + ":" + std::to_string(parentRecord.branchChildOid);
                            if (!seenChildTargets.insert(childKey).second) continue;

                            ApfsRootTreeChildNodeProbeRow cr;
                            cr.sequence = static_cast<std::uint32_t>(apfsRootTreeChildNodeProbeRows.size());
                            cr.sourceRecordSequence = parentRecord.sequence;
                            cr.volumeSequence = parentRecord.volumeSequence;
                            cr.targetRole = parentRecord.targetRole;
                            cr.fsOid = parentRecord.fsOid;
                            cr.volumeName = parentRecord.volumeName;
                            cr.apfsRootTreeOid = parentRecord.apfsRootTreeOid;
                            cr.parentNodeOid = parentRecord.nodeOid;
                            cr.parentNodeVirtualOffset = parentRecord.nodeVirtualOffset;
                            cr.parentNodeLevel = parentRecord.nodeLevel;
                            cr.parentEntryIndex = parentRecord.entryIndex;
                            cr.branchChildOid = parentRecord.branchChildOid;

                            std::uint64_t targetXid = 0;
                            for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                                if (lookup.volumeSequence == parentRecord.volumeSequence && lookup.apfsRootTreeOid == parentRecord.apfsRootTreeOid) {
                                    targetXid = lookup.targetXid;
                                    break;
                                }
                            }
                            cr.targetXid = targetXid;
                            auto omIt = apfsVolumeOmapRowsByVolumeSequence.find(parentRecord.volumeSequence);
                            auto rootIt = apfsVolumeOmapRootNodesByVolumeSequence.find(parentRecord.volumeSequence);
                            if (targetXid == 0) {
                                cr.lookupStatus = "CHILD_TARGET_XID_UNAVAILABLE";
                                cr.childNodeStatus = "CHILD_NODE_NOT_READ";
                                cr.interpretation = "Parent branch-child OID was sampled, but the volume target transaction ID was unavailable.";
                                apfsRootTreeChildNodeProbeRows.push_back(cr);
                                continue;
                            }
                            if (omIt == apfsVolumeOmapRowsByVolumeSequence.end() || rootIt == apfsVolumeOmapRootNodesByVolumeSequence.end()) {
                                cr.lookupStatus = "CHILD_VOLUME_OMAP_UNAVAILABLE";
                                cr.childNodeStatus = "CHILD_NODE_NOT_READ";
                                cr.interpretation = "Parent branch-child OID was sampled, but the matching volume OMAP root buffer was unavailable.";
                                apfsRootTreeChildNodeProbeRows.push_back(cr);
                                continue;
                            }

                            ApfsOmapTargetResolution resolved = resolveVolumeOmapTargetObject(omIt->second, rootIt->second, parentRecord.branchChildOid, targetXid, "APFS filesystem root-tree child-node lookup");
                            cr.omapBranchDepth = resolved.branchDepth;
                            cr.omapBranchPath = resolved.branchPath;
                            cr.omapLeafOid = resolved.leafOid;
                            cr.omapLeafVirtualOffset = resolved.leafVirtualOffset;
                            cr.omapLeafBytesRead = resolved.leafBytesRead;
                            cr.matchedEntryIndex = resolved.matchedEntryIndex;
                            cr.matchedKeyOid = resolved.matchedKeyOid;
                            cr.matchedKeyXid = resolved.matchedKeyXid;
                            cr.valueFlags = resolved.valueFlags;
                            cr.valueSize = resolved.valueSize;
                            cr.valuePaddr = resolved.valuePaddr;
                            cr.resolvedVirtualOffset = resolved.resolvedVirtualOffset;
                            cr.resolvedBytesRead = resolved.resolvedBytesRead;
                            cr.resolvedObjectOid = resolved.resolvedObjectOid;
                            cr.resolvedObjectXid = resolved.resolvedObjectXid;
                            cr.resolvedObjectTypeRaw = resolved.resolvedObjectTypeRaw;
                            cr.resolvedObjectTypeLabel = resolved.resolvedObjectTypeLabel;
                            cr.resolvedObjectSubtype = resolved.resolvedObjectSubtype;
                            cr.resolvedMagic = resolved.resolvedMagic;
                            cr.resolvedBtnFlags = resolved.resolvedBtnFlags;
                            cr.resolvedBtnLevel = resolved.resolvedBtnLevel;
                            cr.resolvedBtnNkeys = resolved.resolvedBtnNkeys;
                            cr.lookupStatus = resolved.lookupStatus;
                            cr.sampleHex = resolved.sampleHex;
                            cr.resolvedSampleHex = resolved.resolvedSampleHex;
                            cr.notes = resolved.notes;
                            if (resolved.objectStatus == "OMAP_TARGET_BTREE_READ") {
                                cr.childNodeStatus = "CHILD_NODE_BTREE_READ";
                                cr.interpretation = "Branch-child OID resolved through the volume OMAP and was read as an APFS filesystem B-tree child node.";
                            } else {
                                cr.childNodeStatus = resolved.objectStatus.empty() ? "CHILD_NODE_NOT_READ" : resolved.objectStatus;
                                cr.interpretation = resolved.interpretation;
                                apfsRootTreeChildNodeProbeRows.push_back(cr);
                                continue;
                            }
                            apfsRootTreeChildNodeProbeRows.push_back(cr);

                            const std::vector<unsigned char>& childNode = resolved.resolvedBuffer;
                            const std::uint32_t childLimit = std::min<std::uint32_t>(resolved.resolvedBtnNkeys, kMaxChildRecordSamplesPerNode);
                            for (std::uint32_t i = 0; i < childLimit; ++i) {
                                std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                std::string detail;
                                ApfsRootTreeRecordSampleRow rr;
                                rr.sequence = static_cast<std::uint32_t>(apfsRootTreeChildRecordSampleRows.size());
                                rr.volumeSequence = parentRecord.volumeSequence;
                                rr.targetRole = parentRecord.targetRole;
                                rr.fsOid = parentRecord.fsOid;
                                rr.volumeName = parentRecord.volumeName;
                                rr.apfsRootTreeOid = parentRecord.apfsRootTreeOid;
                                rr.nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : parentRecord.branchChildOid;
                                rr.nodeVirtualOffset = resolved.resolvedVirtualOffset;
                                rr.nodeLevel = resolved.resolvedBtnLevel;
                                rr.nodeNkeys = resolved.resolvedBtnNkeys;
                                rr.entryIndex = i;
                                if (!genericBtreeKvAbs(childNode, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) {
                                    rr.status = "CHILD_RECORD_TOC_DECODE_FAILED";
                                    rr.interpretation = "The child-node TOC entry could not be decoded safely.";
                                    rr.notes = detail;
                                    apfsRootTreeChildRecordSampleRows.push_back(rr);
                                    continue;
                                }
                                rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                                rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                                rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                                rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                                rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                                rr.keySampleHex = hexSampleBytes(childNode.data() + keyAbs, std::min<std::size_t>(keyLen, 64U));
                                if (valLen > 0 && valAbs < childNode.size()) rr.valueSampleHex = hexSampleBytes(childNode.data() + valAbs, std::min<std::size_t>(valLen, 64U));
                                if (valLen >= 8U && valAbs + 8U <= childNode.size()) rr.valueU64_0 = readLe64(childNode, valAbs);
                                if (valLen >= 16U && valAbs + 16U <= childNode.size()) rr.valueU64_1 = readLe64(childNode, valAbs + 8U);
                                if (valLen >= 24U && valAbs + 24U <= childNode.size()) rr.valueU64_2 = readLe64(childNode, valAbs + 16U);
                                if (keyLen >= 8U) {
                                    rr.keyRaw = readLe64(childNode, keyAbs);
                                    rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                                    rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                                    rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                                }
                                if (resolved.resolvedBtnLevel > 0 && valLen >= 8U) rr.branchChildOid = readLe64(childNode, valAbs);
                                if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                                    const std::uint32_t nameLenAndHash = readLe32(childNode, keyAbs + 8U);
                                    const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                                    rr.decodedName = safePrintableUtf8Fragment(childNode, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                                }
                                rr.status = "CHILD_RECORD_SAMPLE_DECODED";
                                rr.interpretation = resolved.resolvedBtnLevel > 0 ? "Child branch record sampled; branch_child_oid may be used for another bounded traversal step." : "Child leaf record sampled; generic APFS filesystem key object/type fields were decoded where possible.";
                                rr.notes = detail + "; parent_record_sequence=" + std::to_string(parentRecord.sequence);
                                apfsRootTreeChildRecordSampleRows.push_back(rr);
                            }
                        }

                        constexpr std::uint32_t kMaxRootTreeDescendantNodes = 160;
                        constexpr std::uint32_t kMaxDescendantRecordSamplesPerNode = 16;
                        std::set<std::string> seenDescendantTargets;
                        for (const auto& parentRecord : apfsRootTreeChildRecordSampleRows) {
                            if (parentRecord.branchChildOid == 0) continue;
                            if (apfsRootTreeDescendantNodeProbeRows.size() >= kMaxRootTreeDescendantNodes) break;
                            const std::string descendantKey = std::to_string(parentRecord.volumeSequence) + ":" + std::to_string(parentRecord.branchChildOid);
                            if (!seenDescendantTargets.insert(descendantKey).second) continue;

                            ApfsRootTreeChildNodeProbeRow dr;
                            dr.sequence = static_cast<std::uint32_t>(apfsRootTreeDescendantNodeProbeRows.size());
                            dr.sourceRecordSequence = parentRecord.sequence;
                            dr.volumeSequence = parentRecord.volumeSequence;
                            dr.targetRole = parentRecord.targetRole;
                            dr.fsOid = parentRecord.fsOid;
                            dr.volumeName = parentRecord.volumeName;
                            dr.apfsRootTreeOid = parentRecord.apfsRootTreeOid;
                            dr.parentNodeOid = parentRecord.nodeOid;
                            dr.parentNodeVirtualOffset = parentRecord.nodeVirtualOffset;
                            dr.parentNodeLevel = parentRecord.nodeLevel;
                            dr.parentEntryIndex = parentRecord.entryIndex;
                            dr.branchChildOid = parentRecord.branchChildOid;

                            std::uint64_t targetXid = 0;
                            for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                                if (lookup.volumeSequence == parentRecord.volumeSequence && lookup.apfsRootTreeOid == parentRecord.apfsRootTreeOid) {
                                    targetXid = lookup.targetXid;
                                    break;
                                }
                            }
                            dr.targetXid = targetXid;
                            auto omIt = apfsVolumeOmapRowsByVolumeSequence.find(parentRecord.volumeSequence);
                            auto rootIt = apfsVolumeOmapRootNodesByVolumeSequence.find(parentRecord.volumeSequence);
                            if (targetXid == 0) {
                                dr.lookupStatus = "DESCENDANT_TARGET_XID_UNAVAILABLE";
                                dr.childNodeStatus = "DESCENDANT_NODE_NOT_READ";
                                dr.interpretation = "Descendant branch-child OID was sampled, but the volume target transaction ID was unavailable.";
                                apfsRootTreeDescendantNodeProbeRows.push_back(dr);
                                continue;
                            }
                            if (omIt == apfsVolumeOmapRowsByVolumeSequence.end() || rootIt == apfsVolumeOmapRootNodesByVolumeSequence.end()) {
                                dr.lookupStatus = "DESCENDANT_VOLUME_OMAP_UNAVAILABLE";
                                dr.childNodeStatus = "DESCENDANT_NODE_NOT_READ";
                                dr.interpretation = "Descendant branch-child OID was sampled, but the matching volume OMAP root buffer was unavailable.";
                                apfsRootTreeDescendantNodeProbeRows.push_back(dr);
                                continue;
                            }

                            ApfsOmapTargetResolution resolved = resolveVolumeOmapTargetObject(omIt->second, rootIt->second, parentRecord.branchChildOid, targetXid, "APFS filesystem root-tree descendant-node lookup");
                            dr.omapBranchDepth = resolved.branchDepth;
                            dr.omapBranchPath = resolved.branchPath;
                            dr.omapLeafOid = resolved.leafOid;
                            dr.omapLeafVirtualOffset = resolved.leafVirtualOffset;
                            dr.omapLeafBytesRead = resolved.leafBytesRead;
                            dr.matchedEntryIndex = resolved.matchedEntryIndex;
                            dr.matchedKeyOid = resolved.matchedKeyOid;
                            dr.matchedKeyXid = resolved.matchedKeyXid;
                            dr.valueFlags = resolved.valueFlags;
                            dr.valueSize = resolved.valueSize;
                            dr.valuePaddr = resolved.valuePaddr;
                            dr.resolvedVirtualOffset = resolved.resolvedVirtualOffset;
                            dr.resolvedBytesRead = resolved.resolvedBytesRead;
                            dr.resolvedObjectOid = resolved.resolvedObjectOid;
                            dr.resolvedObjectXid = resolved.resolvedObjectXid;
                            dr.resolvedObjectTypeRaw = resolved.resolvedObjectTypeRaw;
                            dr.resolvedObjectTypeLabel = resolved.resolvedObjectTypeLabel;
                            dr.resolvedObjectSubtype = resolved.resolvedObjectSubtype;
                            dr.resolvedMagic = resolved.resolvedMagic;
                            dr.resolvedBtnFlags = resolved.resolvedBtnFlags;
                            dr.resolvedBtnLevel = resolved.resolvedBtnLevel;
                            dr.resolvedBtnNkeys = resolved.resolvedBtnNkeys;
                            dr.lookupStatus = resolved.lookupStatus;
                            dr.sampleHex = resolved.sampleHex;
                            dr.resolvedSampleHex = resolved.resolvedSampleHex;
                            dr.notes = resolved.notes;
                            if (resolved.objectStatus == "OMAP_TARGET_BTREE_READ") {
                                dr.childNodeStatus = "DESCENDANT_NODE_BTREE_READ";
                                dr.interpretation = "Second-level branch-child OID resolved through the volume OMAP and was read as an APFS filesystem B-tree descendant node.";
                            } else {
                                dr.childNodeStatus = resolved.objectStatus.empty() ? "DESCENDANT_NODE_NOT_READ" : resolved.objectStatus;
                                dr.interpretation = resolved.interpretation;
                                apfsRootTreeDescendantNodeProbeRows.push_back(dr);
                                continue;
                            }
                            apfsRootTreeDescendantNodeProbeRows.push_back(dr);

                            const std::vector<unsigned char>& descendantNode = resolved.resolvedBuffer;
                            const std::uint32_t descendantLimit = std::min<std::uint32_t>(resolved.resolvedBtnNkeys, kMaxDescendantRecordSamplesPerNode);
                            for (std::uint32_t i = 0; i < descendantLimit; ++i) {
                                std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                std::string detail;
                                ApfsRootTreeRecordSampleRow rr;
                                rr.sequence = static_cast<std::uint32_t>(apfsRootTreeDescendantRecordSampleRows.size());
                                rr.volumeSequence = parentRecord.volumeSequence;
                                rr.targetRole = parentRecord.targetRole;
                                rr.fsOid = parentRecord.fsOid;
                                rr.volumeName = parentRecord.volumeName;
                                rr.apfsRootTreeOid = parentRecord.apfsRootTreeOid;
                                rr.nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : parentRecord.branchChildOid;
                                rr.nodeVirtualOffset = resolved.resolvedVirtualOffset;
                                rr.nodeLevel = resolved.resolvedBtnLevel;
                                rr.nodeNkeys = resolved.resolvedBtnNkeys;
                                rr.entryIndex = i;
                                if (!genericBtreeKvAbs(descendantNode, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) {
                                    rr.status = "DESCENDANT_RECORD_TOC_DECODE_FAILED";
                                    rr.interpretation = "The descendant-node TOC entry could not be decoded safely.";
                                    rr.notes = detail;
                                    apfsRootTreeDescendantRecordSampleRows.push_back(rr);
                                    continue;
                                }
                                rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                                rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                                rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                                rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                                rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                                rr.keySampleHex = hexSampleBytes(descendantNode.data() + keyAbs, std::min<std::size_t>(keyLen, 64U));
                                if (valLen > 0 && valAbs < descendantNode.size()) rr.valueSampleHex = hexSampleBytes(descendantNode.data() + valAbs, std::min<std::size_t>(valLen, 64U));
                                if (valLen >= 8U && valAbs + 8U <= descendantNode.size()) rr.valueU64_0 = readLe64(descendantNode, valAbs);
                                if (valLen >= 16U && valAbs + 16U <= descendantNode.size()) rr.valueU64_1 = readLe64(descendantNode, valAbs + 8U);
                                if (valLen >= 24U && valAbs + 24U <= descendantNode.size()) rr.valueU64_2 = readLe64(descendantNode, valAbs + 16U);
                                if (keyLen >= 8U) {
                                    rr.keyRaw = readLe64(descendantNode, keyAbs);
                                    rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                                    rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                                    rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                                }
                                if (resolved.resolvedBtnLevel > 0 && valLen >= 8U) rr.branchChildOid = readLe64(descendantNode, valAbs);
                                if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                                    const std::uint32_t nameLenAndHash = readLe32(descendantNode, keyAbs + 8U);
                                    const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                                    rr.decodedName = safePrintableUtf8Fragment(descendantNode, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                                }
                                rr.status = "DESCENDANT_RECORD_SAMPLE_DECODED";
                                rr.interpretation = resolved.resolvedBtnLevel > 0 ? "Descendant branch record sampled; branch_child_oid may be used for another bounded traversal step." : "Descendant leaf record sampled; generic APFS filesystem key object/type/value fields were decoded where possible.";
                                rr.notes = detail + "; parent_child_record_sequence=" + std::to_string(parentRecord.sequence);
                                apfsRootTreeDescendantRecordSampleRows.push_back(rr);
                            }
                        }
                    };
                    std::vector<unsigned char> buffer;
                    std::string err;
                    long long bytesRead = readVirtual(0, 4096, buffer, err);
                    if (bytesRead >= 0) {
                        const std::size_t sampleLen = buffer.size() < 64 ? buffer.size() : 64;
                        addRow("AFF4_read_offset_0", bytesRead > 0 ? "READ_OK" : "READ_ZERO", originalInput, finalObjectSize, 0, bytesRead, sampleLen ? hexSampleBytes(buffer.data(), sampleLen) : std::string{}, "First bounded read from AFF4 object; this is the no-full-export block-reader smoke test.");
                    } else {
                        addRow("AFF4_read_offset_0", "READ_FAILED", originalInput, finalObjectSize, 0, bytesRead, {}, err.empty() ? "AFF4_read returned a negative value." : err);
                    }
                    addReadRow("virtual_read_offset_0", 0, bytesRead, buffer, "Read first 4096 virtual bytes from AFF4 image object.", err.empty() ? "Offset 0 read through libaff4 C API." : err);

                    nxSummary = parseApfsNxSuperblock(buffer, 0, bytesRead);
                    if (nxSummary.found) {
                        addApfsRow("apfs_nx_superblock_parse", nxSummary.validationStatus, 0, bytesRead, "NXSB", nxSummary.validationStatus == "NXSB_PARSED" ? "HIGH_APFS_CONTAINER" : "APFS_CONTAINER_WITH_WARNINGS", "APFS container superblock fields parsed from virtual offset 0.", buffer.empty() ? std::string{} : hexSampleBytes(buffer.data(), buffer.size() < 64 ? buffer.size() : 64), nxSummary.notes);

                        const bool blockSizeOk = nxSummary.blockSize >= 4096 && nxSummary.blockSize <= 65536 && ((nxSummary.blockSize & (nxSummary.blockSize - 1U)) == 0U);
                        const std::uint32_t rawDescCount = nxSummary.xpDescLen ? nxSummary.xpDescLen : nxSummary.xpDescBlocks;
                        const std::uint32_t descBlocksToScan = std::min<std::uint32_t>(rawDescCount, 128U);
                        if (!blockSizeOk || nxSummary.xpDescBase == 0 || nxSummary.xpDescBlocks == 0 || descBlocksToScan == 0) {
                            ApfsCheckpointDescriptorRow dr;
                            dr.status = "CHECKPOINT_DESCRIPTOR_SCAN_SKIPPED";
                            dr.interpretation = "NXSB parsed, but checkpoint descriptor base/count/length was not suitable for bounded scanning.";
                            dr.notes = "block_size=" + std::to_string(nxSummary.blockSize) + "; xp_desc_base=" + std::to_string(nxSummary.xpDescBase) + "; xp_desc_blocks=" + std::to_string(nxSummary.xpDescBlocks) + "; xp_desc_len=" + std::to_string(nxSummary.xpDescLen);
                            apfsDescriptorRows.push_back(dr);
                        } else {
                            for (std::uint32_t seq = 0; seq < descBlocksToScan; ++seq) {
                                const std::uint64_t ringIndex = (static_cast<std::uint64_t>(nxSummary.xpDescIndex) + static_cast<std::uint64_t>(seq)) % static_cast<std::uint64_t>(nxSummary.xpDescBlocks);
                                const std::uint64_t physicalBlock = nxSummary.xpDescBase + ringIndex;
                                ApfsCheckpointDescriptorRow dr;
                                dr.sequence = seq;
                                dr.physicalBlock = physicalBlock;
                                if (physicalBlock > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                    dr.status = "OFFSET_OVERFLOW";
                                    dr.interpretation = "Checkpoint descriptor block offset overflowed uint64 safety bounds.";
                                    dr.notes = "physical_block=" + std::to_string(physicalBlock) + "; block_size=" + std::to_string(nxSummary.blockSize);
                                    apfsDescriptorRows.push_back(dr);
                                    continue;
                                }
                                dr.virtualOffset = physicalBlock * static_cast<std::uint64_t>(nxSummary.blockSize);
                                std::vector<unsigned char> desc;
                                std::string descErr;
                                dr.bytesRead = readVirtual(dr.virtualOffset, nxSummary.blockSize, desc, descErr);
                                dr.status = dr.bytesRead > 0 ? "READ_OK" : "READ_FAILED";
                                if (dr.bytesRead > 0 && desc.size() >= 32) {
                                    dr.oid = readLe64(desc, 8);
                                    dr.xid = readLe64(desc, 16);
                                    dr.objectTypeRaw = readLe32(desc, 24);
                                    dr.objectSubtype = readLe32(desc, 28);
                                    dr.objectTypeLabel = apfsObjectTypeLabel(dr.objectTypeRaw);
                                    if (desc.size() >= 36) {
                                        if (std::memcmp(desc.data() + 32, "NXSB", 4) == 0) dr.magic = "NXSB";
                                        else if (std::memcmp(desc.data() + 32, "APSB", 4) == 0) dr.magic = "APSB";
                                        else if (std::memcmp(desc.data() + 32, "OMAP", 4) == 0) dr.magic = "OMAP";
                                    }
                                    dr.sampleHex = hexSampleBytes(desc.data(), desc.size() < 64 ? desc.size() : 64);
                                    if (dr.magic == "NXSB") dr.interpretation = "Checkpoint descriptor contains an APFS container superblock.";
                                    else if (dr.magic == "APSB") dr.interpretation = "Checkpoint descriptor appears to contain an APFS volume superblock candidate.";
                                    else if (!dr.magic.empty()) dr.interpretation = "Checkpoint descriptor contains a recognized APFS magic value.";
                                    else dr.interpretation = "Checkpoint descriptor object header read; no NXSB/APSB magic at +32.";
                                    if (dr.objectTypeLabel == "CHECKPOINT_MAP" && desc.size() >= 40 && nxSummary.blockSize >= 4096) {
                                        const std::uint32_t cpmFlags = readLe32(desc, 32);
                                        const std::uint32_t cpmCount = readLe32(desc, 36);
                                        const std::uint32_t entriesToParse = std::min<std::uint32_t>(cpmCount, 128U);
                                        for (std::uint32_t entryIndex = 0; entryIndex < entriesToParse; ++entryIndex) {
                                            const std::size_t entryOff = 40U + static_cast<std::size_t>(entryIndex) * 40U;
                                            if (entryOff + 40U > desc.size()) break;
                                            ApfsCheckpointMapEntryRow mr;
                                            mr.sequence = seq;
                                            mr.entryIndex = entryIndex;
                                            mr.checkpointBlock = physicalBlock;
                                            mr.checkpointVirtualOffset = dr.virtualOffset;
                                            mr.checkpointBytesRead = dr.bytesRead;
                                            mr.checkpointFlags = cpmFlags;
                                            mr.checkpointCount = cpmCount;
                                            mr.cpmTypeRaw = readLe32(desc, entryOff + 0);
                                            mr.cpmSubtype = readLe32(desc, entryOff + 4);
                                            mr.cpmSize = readLe32(desc, entryOff + 8);
                                            mr.cpmFsOid = readLe64(desc, entryOff + 16);
                                            mr.cpmOid = readLe64(desc, entryOff + 24);
                                            mr.cpmPaddr = readLe64(desc, entryOff + 32);
                                            mr.cpmTypeLabel = apfsObjectTypeLabel(mr.cpmTypeRaw);
                                            if (mr.cpmOid == nxSummary.omapOid) mr.targetRole = "NX_OBJECT_MAP";
                                            else if (mr.cpmOid == nxSummary.spacemanOid) mr.targetRole = "NX_SPACEMAN";
                                            else if (mr.cpmOid == nxSummary.reaperOid) mr.targetRole = "NX_REAPER";
                                            else if (containsU64(nxSummary.fsOids, mr.cpmOid)) mr.targetRole = "NX_FILESYSTEM_OID";
                                            else if (containsU64(nxSummary.fsOids, mr.cpmFsOid)) mr.targetRole = "VOLUME_SCOPED_OBJECT";
                                            else mr.targetRole = "CHECKPOINT_MAPPED_OBJECT";
                                            mr.interpretation = "APFS checkpoint-map entry parsed. cpm_paddr is treated as the candidate physical APFS block for bounded probing.";
                                            if (cpmCount > entriesToParse) mr.notes = "checkpoint_map_count_truncated_to_128";
                                            apfsCheckpointMapRows.push_back(mr);

                                            if (mr.cpmPaddr != 0 && mr.cpmPaddr <= nxSummary.blockCount && mr.cpmPaddr <= ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                                ApfsCheckpointMappedObjectProbeRow pr;
                                                pr.sequence = seq;
                                                pr.entryIndex = entryIndex;
                                                pr.cpmOid = mr.cpmOid;
                                                pr.cpmFsOid = mr.cpmFsOid;
                                                pr.cpmPaddr = mr.cpmPaddr;
                                                pr.targetRole = mr.targetRole;
                                                pr.virtualOffset = mr.cpmPaddr * static_cast<std::uint64_t>(nxSummary.blockSize);
                                                std::vector<unsigned char> mapped;
                                                std::string mappedErr;
                                                pr.bytesRead = readVirtual(pr.virtualOffset, nxSummary.blockSize, mapped, mappedErr);
                                                pr.status = pr.bytesRead > 0 ? "READ_OK" : "READ_FAILED";
                                                if (pr.bytesRead > 0 && mapped.size() >= 36) {
                                                    pr.mappedOid = readLe64(mapped, 8);
                                                    pr.mappedXid = readLe64(mapped, 16);
                                                    pr.mappedTypeRaw = readLe32(mapped, 24);
                                                    pr.mappedSubtype = readLe32(mapped, 28);
                                                    pr.mappedTypeLabel = apfsObjectTypeLabel(pr.mappedTypeRaw);
                                                    if (std::memcmp(mapped.data() + 32, "OMAP", 4) == 0) pr.magic = "OMAP";
                                                    else if (std::memcmp(mapped.data() + 32, "APSB", 4) == 0) pr.magic = "APSB";
                                                    else if (std::memcmp(mapped.data() + 32, "NXSB", 4) == 0) pr.magic = "NXSB";
                                                    else pr.magic.assign(reinterpret_cast<const char*>(mapped.data() + 32), reinterpret_cast<const char*>(mapped.data() + 36));
                                                    pr.sampleHex = hexSampleBytes(mapped.data(), mapped.size() < 64 ? mapped.size() : 64);
                                                    if (pr.magic == "OMAP") pr.interpretation = "Mapped checkpoint object appears to be an APFS object map. Next step is OMAP B-tree parsing.";
                                                    else if (pr.magic == "APSB") pr.interpretation = "Mapped checkpoint object appears to be an APFS volume superblock.";
                                                    else if (pr.magic == "NXSB") pr.interpretation = "Mapped checkpoint object appears to be an APFS container superblock.";
                                                    else pr.interpretation = "Mapped checkpoint object was read; no OMAP/APSB/NXSB magic at +32.";
                                                    addBtreeProbeFromBuffer(std::string("CHECKPOINT_MAPPED_") + pr.targetRole, pr.cpmOid, pr.cpmPaddr, pr.virtualOffset, pr.bytesRead, mapped, "Mapped through APFS checkpoint map cpm_paddr.");
                                                } else {
                                                    pr.interpretation = "Unable to read mapped checkpoint object through libaff4 virtual read.";
                                                    pr.notes = mappedErr.empty() ? "AFF4_read returned no bytes for mapped checkpoint object." : mappedErr;
                                                }
                                                apfsCheckpointMappedObjectRows.push_back(pr);
                                            }
                                        }
                                    }
                                } else {
                                    dr.interpretation = "Unable to read checkpoint descriptor block through libaff4 virtual read.";
                                    dr.notes = descErr.empty() ? "AFF4_read returned no bytes for checkpoint descriptor block." : descErr;
                                }
                                apfsDescriptorRows.push_back(dr);
                            }
                        }

                        const std::size_t volumeProbeLimit = std::min<std::size_t>(nxSummary.fsOids.size(), 32U);
                        for (std::size_t i = 0; i < volumeProbeLimit; ++i) {
                            const std::uint64_t fsOid = nxSummary.fsOids[i];
                            ApfsVolumeSuperblockRow vr;
                            vr.sequence = static_cast<std::uint32_t>(i);
                            vr.fsOid = fsOid;
                            if (nxSummary.blockSize == 0 || fsOid > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                vr.status = "OFFSET_OVERFLOW";
                                vr.interpretation = "Candidate APFS filesystem object ID could not be converted to a bounded virtual block offset.";
                                vr.notes = "fs_oid=" + std::to_string(fsOid) + "; block_size=" + std::to_string(nxSummary.blockSize);
                                apfsVolumeRows.push_back(vr);
                                continue;
                            }
                            vr.virtualOffset = fsOid * static_cast<std::uint64_t>(nxSummary.blockSize);
                            std::vector<unsigned char> vol;
                            std::string volErr;
                            vr.bytesRead = readVirtual(vr.virtualOffset, nxSummary.blockSize, vol, volErr);
                            vr.status = vr.bytesRead > 0 ? "READ_OK" : "READ_FAILED";
                            if (vr.bytesRead > 0 && vol.size() >= 64) {
                                vr.oid = readLe64(vol, 8);
                                vr.xid = readLe64(vol, 16);
                                vr.objectTypeRaw = readLe32(vol, 24);
                                vr.objectSubtype = readLe32(vol, 28);
                                vr.objectTypeLabel = apfsObjectTypeLabel(vr.objectTypeRaw);
                                if (vol.size() >= 36 && std::memcmp(vol.data() + 32, "APSB", 4) == 0) {
                                    vr.magic = "APSB";
                                    vr.status = "APSB_FOUND";
                                    vr.interpretation = "APFS volume superblock candidate found at fs_oid * nx_block_size.";
                                    vr.fsIndexCandidate = readLe32(vol, 36);
                                    vr.featuresCandidate = readLe64(vol, 40);
                                    vr.readonlyCompatibleFeaturesCandidate = readLe64(vol, 48);
                                    vr.incompatibleFeaturesCandidate = readLe64(vol, 56);
                                    if (vol.size() >= 80) {
                                        vr.unmountTimeCandidate = readLe64(vol, 64);
                                        vr.volumeUuidCandidate = bytesToUuidString(vol, 72);
                                    }
                                } else {
                                    if (vol.size() >= 36) vr.magic.assign(reinterpret_cast<const char*>(vol.data() + 32), reinterpret_cast<const char*>(vol.data() + 36));
                                    vr.interpretation = "Candidate filesystem OID block was read, but APSB magic was not present at +32.";
                                }
                                vr.sampleHex = hexSampleBytes(vol.data(), vol.size() < 64 ? vol.size() : 64);
                            } else {
                                vr.interpretation = "Unable to read candidate APFS volume superblock block through libaff4 virtual read.";
                                vr.notes = volErr.empty() ? "AFF4_read returned no bytes for candidate APFS filesystem OID block." : volErr;
                            }
                            apfsVolumeRows.push_back(vr);
                        }

                        probeApfsObjectId("NX_OMAP_OID", nxSummary.omapOid);
                        probeApfsObjectId("NX_SPACEMAN_OID", nxSummary.spacemanOid);
                        probeApfsObjectId("NX_REAPER_OID", nxSummary.reaperOid);
                        const std::size_t objectIdFsLimit = std::min<std::size_t>(nxSummary.fsOids.size(), 32U);
                        for (std::size_t i = 0; i < objectIdFsLimit; ++i) {
                            probeApfsObjectId(std::string("NX_FS_OID_") + std::to_string(i), nxSummary.fsOids[i]);
                        }
                        buildResolvedVolumeSuperblocksAndVolumeOmapProbes();
                    } else {
                        addApfsRow("apfs_nx_superblock_parse", nxSummary.validationStatus.empty() ? "NXSB_NOT_FOUND" : nxSummary.validationStatus, 0, bytesRead, {}, "NO_NXSB", "APFS container superblock was not parsed from virtual offset 0.", buffer.empty() ? std::string{} : hexSampleBytes(buffer.data(), buffer.size() < 64 ? buffer.size() : 64), nxSummary.notes);
                    }

                    err.clear();
                    std::vector<unsigned char> gpt;
                    long long gptRead = readVirtual(512, 512, gpt, err);
                    if (gptRead >= 8 && std::memcmp(gpt.data(), "EFI PART", 8) == 0) {
                        gptFound = true;
                        addApfsRow("gpt_header_lba1", "GPT_HEADER_FOUND", 512, gptRead, "EFI PART", "HIGH_ALIGNED", "GPT header found at virtual LBA 1.", hexSampleBytes(gpt.data(), gpt.size() < 64 ? gpt.size() : 64), "Next step is parsing GPT partition entries to locate APFS partition type GUIDs.");
                        const std::uint64_t partitionEntryLba = readLe64(gpt, 72);
                        const std::uint32_t partitionEntryCountReported = readLe32(gpt, 80);
                        const std::uint32_t partitionEntrySize = readLe32(gpt, 84);
                        const std::uint64_t tableOffset = partitionEntryLba * 512ULL;
                        const std::uint32_t entriesToRead = partitionEntryCountReported > 128U ? 128U : partitionEntryCountReported;
                        const std::uint64_t tableBytes64 = static_cast<std::uint64_t>(entriesToRead) * static_cast<std::uint64_t>(partitionEntrySize);
                        if (partitionEntryLba == 0 || partitionEntrySize < 56 || partitionEntrySize > 4096 || tableBytes64 == 0 || tableBytes64 > 1024ULL * 1024ULL) {
                            addApfsRow("gpt_partition_table", "GPT_TABLE_UNSANE", tableOffset, -1, {}, "LOW", "GPT header was found but partition table location/count/entry size was not in the bounded probe safety range.", {}, "Reported entries=" + std::to_string(partitionEntryCountReported) + "; entry_size=" + std::to_string(partitionEntrySize) + "; entry_lba=" + std::to_string(partitionEntryLba));
                        } else {
                            std::vector<unsigned char> entries;
                            err.clear();
                            long long tableRead = readVirtual(tableOffset, static_cast<std::size_t>(tableBytes64), entries, err);
                            addApfsRow("gpt_partition_table", tableRead > 0 ? "GPT_TABLE_READ" : "GPT_TABLE_READ_FAILED", tableOffset, tableRead, {}, tableRead > 0 ? "READ_PERFORMED" : "NO_DATA", "Bounded GPT partition-entry table read.", tableRead > 0 ? hexSampleBytes(entries.data(), entries.size() < 64 ? entries.size() : 64) : std::string{}, "Reported entries=" + std::to_string(partitionEntryCountReported) + "; read entries=" + std::to_string(entriesToRead) + "; entry_size=" + std::to_string(partitionEntrySize));
                            auto le32At = [&](std::size_t off) -> std::uint32_t { return readLe32(entries, off); };
                            auto le64At = [&](std::size_t off) -> std::uint64_t { return readLe64(entries, off); };
                            auto guidAt = [&](std::size_t off) -> std::string {
                                if (off + 16 > entries.size()) return {};
                                auto hx = [](unsigned char b) -> std::string { return std::string(1, "0123456789ABCDEF"[(b >> 4) & 0xF]) + std::string(1, "0123456789ABCDEF"[b & 0xF]); };
                                std::string g;
                                g += hx(entries[off + 3]); g += hx(entries[off + 2]); g += hx(entries[off + 1]); g += hx(entries[off + 0]); g += '-';
                                g += hx(entries[off + 5]); g += hx(entries[off + 4]); g += '-';
                                g += hx(entries[off + 7]); g += hx(entries[off + 6]); g += '-';
                                g += hx(entries[off + 8]); g += hx(entries[off + 9]); g += '-';
                                for (int i = 10; i < 16; ++i) g += hx(entries[off + static_cast<std::size_t>(i)]);
                                return g;
                            };
                            const std::string apfsGuid = "7C3457EF-0000-11AA-AA11-00306543ECAC";
                            if (tableRead > 0) {
                                for (std::uint32_t i = 0; i < entriesToRead; ++i) {
                                    const std::size_t base = static_cast<std::size_t>(i) * static_cast<std::size_t>(partitionEntrySize);
                                    if (base + 56 > entries.size()) break;
                                    bool typeZero = true;
                                    for (std::size_t j = 0; j < 16; ++j) if (entries[base + j] != 0) { typeZero = false; break; }
                                    if (typeZero) continue;
                                    ++partitionCount;
                                    const std::string guid = guidAt(base);
                                    const std::uint64_t firstLba = le64At(base + 32);
                                    const std::uint64_t lastLba = le64At(base + 40);
                                    const bool isApfs = (guid == apfsGuid);
                                    if (isApfs) ++apfsPartitionCount;
                                    const std::uint64_t partOffset = firstLba * 512ULL;
                                    addApfsRow("gpt_partition_entry", isApfs ? "APFS_PARTITION_CANDIDATE" : "PARTITION_ENTRY", partOffset, 0, guid, isApfs ? "HIGH_APFS_GUID" : "GUID_REPORTED", isApfs ? "APFS GPT partition type GUID found." : "Non-empty GPT partition entry found.", {}, "index=" + std::to_string(i) + "; first_lba=" + std::to_string(firstLba) + "; last_lba=" + std::to_string(lastLba));
                                    if (isApfs && apfsPartitionCount <= 8) {
                                        std::vector<unsigned char> sb;
                                        err.clear();
                                        long long sbRead = readVirtual(partOffset, 4096, sb, err);
                                        bool nxsb = (sbRead >= 36 && sb.size() >= 36 && std::memcmp(sb.data() + 32, "NXSB", 4) == 0);
                                        if (nxsb) ++nxsbHitCount;
                                        addApfsRow("apfs_container_superblock_probe", nxsb ? "NXSB_FOUND" : (sbRead > 0 ? "NXSB_NOT_AT_PLUS_32" : "READ_FAILED"), partOffset, sbRead, nxsb ? "NXSB" : std::string{}, nxsb ? "HIGH_APFS_ALIGNED" : "NO_NXSB_AT_EXPECTED_OFFSET", nxsb ? "APFS container superblock magic found at partition offset + 32." : "Read APFS candidate partition start but did not observe NXSB at +32.", sbRead > 0 ? hexSampleBytes(sb.data(), sb.size() < 64 ? sb.size() : 64) : std::string{}, err.empty() ? "APFS candidate partition start probed through libaff4 virtual read." : err);
                                    }
                                }
                            }
                        }
                    } else {
                        addApfsRow("gpt_header_lba1", gptRead > 0 ? "GPT_HEADER_NOT_FOUND" : "READ_FAILED", 512, gptRead, gptRead >= 8 ? std::string(reinterpret_cast<const char*>(gpt.data()), reinterpret_cast<const char*>(gpt.data()) + 8) : std::string{}, gptRead > 0 ? "NO_EFI_PART" : "NO_DATA", "No GPT header was observed at virtual LBA 1 in this bounded probe.", gptRead > 0 ? hexSampleBytes(gpt.data(), gpt.size() < 64 ? gpt.size() : 64) : std::string{}, err.empty() ? "If this AFF4 object is a volume rather than whole disk image, APFS may begin at object offset 0." : err);
                        if (bytesRead >= 36 && buffer.size() >= 36 && std::memcmp(buffer.data() + 32, "NXSB", 4) == 0) {
                            ++nxsbHitCount;
                            addApfsRow("apfs_container_superblock_offset_0", "NXSB_FOUND", 0, bytesRead, "NXSB", "HIGH_APFS_ALIGNED", "APFS container superblock magic found at object offset 0 + 32.", hexSampleBytes(buffer.data(), buffer.size() < 64 ? buffer.size() : 64), "This suggests the AFF4 object may expose an APFS container/volume rather than a whole physical disk.");
                        }
                    }

                    auto safeSpotlightNodeOffset = [&](std::uint64_t oid, std::uint64_t& offsetOut) -> bool {
                        if (nxSummary.blockSize == 0) return false;
                        if (oid > ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) return false;
                        offsetOut = oid * static_cast<std::uint64_t>(nxSummary.blockSize);
                        if (finalObjectSize != 0 && offsetOut >= finalObjectSize) return false;
                        return true;
                    };

                    auto isLikelyNumericSpotlightCacheTextName = [&](const std::string& lname) -> bool {
                        if (lname.size() <= 4 || lname.substr(lname.size() - 4) != ".txt") return false;
                        const std::string stem = lname.substr(0, lname.size() - 4);
                        if (stem.empty()) return false;
                        for (const char ch : stem) {
                            const unsigned char u = static_cast<unsigned char>(ch);
                            if (!std::isxdigit(u)) return false;
                        }
                        return true;
                    };

                    auto isSpotlightStoreV2TopLevelComponentName = [&](const std::string& lname) -> bool {
                        if (lname == ".store.db" || lname == "store.db" || lname == "store.db-wal" || lname == "store.db-shm") return true;
                        if (lname == "0.directorystorefile" || lname == "0.directorystorefile.shadow") return true;
                        if (lname.rfind("0.index", 0) == 0 || lname.rfind("0.shadowindex", 0) == 0) return true;
                        if (lname.rfind("live.", 0) == 0) return true;
                        if (lname == "reversestore.updates" || lname == "store.updates" || lname == "store_generation") return true;
                        if (lname == "reversedirectorystore" || lname == "reversedirectorystore.shadow") return true;
                        if (lname == "permstore" || lname == "journalexclusion" || lname == "journals.migration_secondchance") return true;
                        if (lname == "cab.created" || lname == "cab.modified" || lname == "lion.created" || lname == "lion.modified" || lname == "star.created" || lname == "star.modified") return true;
                        if (lname == "tmp.cab" || lname == "tmp.lion" || lname == "tmp.star") return true;
                        if (lname.rfind("dbstr-", 0) == 0 || lname.rfind("dbhdr-", 0) == 0) return true;
                        if (lname.rfind("tmp.spotlight", 0) == 0) return true;
                        return false;
                    };

                    auto isSpotlightTargetName = [&](const std::string& name) -> bool {
                        const std::string lname = asciiLower(name);
                        return lname == ".spotlight-v100" || lname == "store-v2" || lname == "index.db" ||
                               isSpotlightStoreV2TopLevelComponentName(lname) ||
                               isLikelyNumericSpotlightCacheTextName(lname) ||
                               lname.find("spotlight") != std::string::npos || lname.find("corespotlight") != std::string::npos;
                    };

                    auto spotlightTargetKind = [&](const std::string& name) -> std::string {
                        const std::string lname = asciiLower(name);
                        if (lname == ".spotlight-v100") return "SPOTLIGHT_ROOT_DIRECTORY";
                        if (lname == "store-v2") return "SPOTLIGHT_STORE_V2_DIRECTORY";
                        if (lname == "store.db" || lname == ".store.db") return "SPOTLIGHT_STORE_DB_FILE";
                        if (lname == "store.db-wal" || lname == "store.db-shm") return "SPOTLIGHT_STORE_SQLITE_SUPPORT_FILE";
                        if (lname.rfind("dbstr-", 0) == 0) return "SPOTLIGHT_DBSTR_FILE";
                        if (lname.rfind("dbhdr-", 0) == 0) return "SPOTLIGHT_DBHDR_FILE";
                        if (lname == "0.directorystorefile" || lname == "0.directorystorefile.shadow") return "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
                        if (lname.rfind("0.index", 0) == 0 || lname.rfind("0.shadowindex", 0) == 0) return "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
                        if (lname.rfind("live.", 0) == 0 || lname == "permstore" || lname == "journalexclusion" || lname == "journals.migration_secondchance" || lname == "reversestore.updates" || lname == "store.updates" || lname == "store_generation" || lname == "reversedirectorystore" || lname == "reversedirectorystore.shadow") return "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
                        if (lname == "cab.created" || lname == "cab.modified" || lname == "lion.created" || lname == "lion.modified" || lname == "star.created" || lname == "star.modified" || lname == "tmp.cab" || lname == "tmp.lion" || lname == "tmp.star") return "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
                        if (lname.rfind("tmp.spotlight", 0) == 0) return "TMP_SPOTLIGHT_COMPONENT";
                        if (isLikelyNumericSpotlightCacheTextName(lname)) return "SPOTLIGHT_STOREV2_CACHE_TEXT_FILE";
                        if (lname.find("corespotlight") != std::string::npos) return "IOS_CORESPOTLIGHT_NAME";
                        if (lname == "index.db") return "IOS_CORESPOTLIGHT_INDEX_DB_FILE";
                        return "SPOTLIGHT_RELATED_NAME";
                    };

                    std::vector<ApfsSpotlightFileExtentProbeRow> allSpotlightScanFileExtents;
                    std::vector<ApfsSpotlightInodeProbeRow> allSpotlightScanInodes;

                    auto previewStatusForBytes = [&](const std::vector<unsigned char>& bytes) -> std::string {
                        if (bytes.size() >= 16 && std::memcmp(bytes.data(), "SQLite format 3", 15) == 0) return "PREVIEW_SQLITE_HEADER";
                        if (bytes.size() >= 4 && bytes[0] == 0x37 && bytes[1] == 0x7f && bytes[2] == 0x06 && bytes[3] == 0x82) return "PREVIEW_SQLITE_WAL_HEADER";
                        if (bytes.size() >= 4 && bytes[0] == 0x18 && bytes[1] == 0xe2 && bytes[2] == 0x2d && bytes[3] == 0x00) return "PREVIEW_SQLITE_SHM_HEADER";
                        if (!bytes.empty()) return "PREVIEW_READ_NON_SQLITE_SAMPLE";
                        return "NO_PREVIEW";
                    };

                    auto appendCopyAttempt = [&](const ApfsRootTreeRecordSampleRow& rr) {
                        ApfsSpotlightCopyAttemptRow cr;
                        cr.sequence = static_cast<std::uint32_t>(apfsSpotlightCopyAttemptRows.size());
                        cr.volumeSequence = rr.volumeSequence;
                        cr.targetRole = rr.targetRole;
                        cr.fsOid = rr.fsOid;
                        cr.volumeName = rr.volumeName;
                        cr.parentObjectId = rr.keyObjectId;
                        cr.childFileId = rr.valueU64_0;
                        cr.targetName = rr.decodedName;
                        cr.targetKind = spotlightTargetKind(rr.decodedName);
                        if (cr.childFileId == 0) {
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_NO_CHILD_FILE_ID";
                            cr.interpretation = "A Spotlight-related name was decoded, but the directory-record value did not provide a usable child file ID candidate.";
                        } else if (cr.targetKind == "SPOTLIGHT_ROOT_DIRECTORY" || cr.targetKind == "SPOTLIGHT_STORE_V2_DIRECTORY" || cr.targetKind == "IOS_CORESPOTLIGHT_NAME") {
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_DIRECTORY_RECURSION_NOT_READY";
                            cr.interpretation = "A Spotlight-related directory was found. The next extraction step must recursively resolve child paths and file extents before copying bytes.";
                        } else {
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED";
                            cr.interpretation = "A Spotlight-related file name was found. File-byte extraction is blocked until APFS file extent values are decoded and validated for this child file ID.";
                        }
                        cr.notes = "V0_8_62 records a gated copy decision; file-byte extraction requires target path, usable extents, duplicate-free logical extent assembly, and optional inode DSTREAM logical-size trimming.";
                        apfsSpotlightCopyAttemptRows.push_back(cr);
                    };

                    struct SpotlightScanPendingNode {
                        std::uint32_t volumeSequence = 0;
                        std::string targetRole;
                        std::uint64_t fsOid = 0;
                        std::string volumeName;
                        std::uint64_t apfsRootTreeOid = 0;
                        std::uint64_t parentNodeOid = 0;
                        std::uint64_t branchOid = 0;
                        std::uint64_t targetXid = 0;
                        std::uint32_t depth = 0;
                    };

                    std::vector<SpotlightScanPendingNode> spotlightQueue;
                    std::set<std::string> spotlightSeen;
                    auto enqueueSpotlightNode = [&](const ApfsRootTreeRecordSampleRow& r, std::uint32_t depth) {
                        if (r.branchChildOid == 0) return;
                        const std::string key = std::to_string(r.volumeSequence) + ":" + std::to_string(r.branchChildOid);
                        if (!spotlightSeen.insert(key).second) return;
                        SpotlightScanPendingNode pn;
                        pn.volumeSequence = r.volumeSequence;
                        pn.targetRole = r.targetRole;
                        pn.fsOid = r.fsOid;
                        pn.volumeName = r.volumeName;
                        pn.apfsRootTreeOid = r.apfsRootTreeOid;
                        pn.parentNodeOid = r.nodeOid;
                        pn.branchOid = r.branchChildOid;
                        pn.depth = depth;
                        for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                            if (lookup.volumeSequence == r.volumeSequence && lookup.apfsRootTreeOid == r.apfsRootTreeOid) {
                                pn.targetXid = lookup.targetXid;
                                break;
                            }
                        }
                        spotlightQueue.push_back(pn);
                    };
                    auto seedSpotlightQueueForVolume = [&](std::uint32_t volumeSequence) {
                        for (const auto& r : apfsRootTreeRecordSampleRows) if (r.volumeSequence == volumeSequence) enqueueSpotlightNode(r, 1);
                        for (const auto& r : apfsRootTreeChildRecordSampleRows) if (r.volumeSequence == volumeSequence) enqueueSpotlightNode(r, 2);
                        for (const auto& r : apfsRootTreeDescendantRecordSampleRows) if (r.volumeSequence == volumeSequence) enqueueSpotlightNode(r, 3);
                    };
                    // Prioritize the Data volume because macOS Spotlight stores are expected under the Data namespace.
                    seedSpotlightQueueForVolume(4);
                    for (const auto& lookup : apfsVolumeRootTreeLookupRows) if (lookup.volumeSequence != 4) seedSpotlightQueueForVolume(lookup.volumeSequence);

                    for (std::size_t qi = 0; qi < spotlightQueue.size(); ++qi) {
                        const SpotlightScanPendingNode pending = spotlightQueue[qi];
                        if (pending.branchOid == 0 || pending.targetXid == 0) continue;
                        auto omIt = apfsVolumeOmapRowsByVolumeSequence.find(pending.volumeSequence);
                        auto rootIt = apfsVolumeOmapRootNodesByVolumeSequence.find(pending.volumeSequence);
                        if (omIt == apfsVolumeOmapRowsByVolumeSequence.end() || rootIt == apfsVolumeOmapRootNodesByVolumeSequence.end()) continue;
                        ++apfsSpotlightTargetScanMetrics.nodesVisited;
                        ApfsOmapTargetResolution resolved = aff4ResolveVolumeOmapTargetObjectForProbe(omIt->second, rootIt->second, pending.branchOid, pending.targetXid, nxSummary.blockSize, readVirtual, safeSpotlightNodeOffset, "APFS Spotlight-target namespace scan", dynamicProbeCancelled);
                        if (resolved.objectStatus != "OMAP_TARGET_BTREE_READ") continue;
                        ++apfsSpotlightTargetScanMetrics.nodesResolved;
                        if (resolved.resolvedBtnLevel > 0) ++apfsSpotlightTargetScanMetrics.branchNodes;
                        else ++apfsSpotlightTargetScanMetrics.leafNodes;

                        const std::vector<unsigned char>& scanNode = resolved.resolvedBuffer;
                        const std::uint32_t limit = resolved.resolvedBtnNkeys;
                        for (std::uint32_t i = 0; i < limit; ++i) {
                            std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                            std::string detail;
                            if (!aff4GenericBtreeKvAbsForProbe(scanNode, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                            ++apfsSpotlightTargetScanMetrics.recordsScanned;
                            ApfsRootTreeRecordSampleRow rr;
                            rr.sequence = static_cast<std::uint32_t>(apfsSpotlightTargetScanRows.size());
                            rr.volumeSequence = pending.volumeSequence;
                            rr.targetRole = pending.targetRole;
                            rr.fsOid = pending.fsOid;
                            rr.volumeName = pending.volumeName;
                            rr.apfsRootTreeOid = pending.apfsRootTreeOid;
                            rr.nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : pending.branchOid;
                            rr.nodeVirtualOffset = resolved.resolvedVirtualOffset;
                            rr.nodeLevel = resolved.resolvedBtnLevel;
                            rr.nodeNkeys = resolved.resolvedBtnNkeys;
                            rr.entryIndex = i;
                            rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                            rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                            rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                            rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                            rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                            rr.keySampleHex = hexSampleBytes(scanNode.data() + keyAbs, std::min<std::size_t>(keyLen, 64U));
                            if (valLen > 0 && valAbs < scanNode.size()) rr.valueSampleHex = hexSampleBytes(scanNode.data() + valAbs, std::min<std::size_t>(valLen, 64U));
                            if (valLen >= 8U && valAbs + 8U <= scanNode.size()) rr.valueU64_0 = readLe64(scanNode, valAbs);
                            if (valLen >= 16U && valAbs + 16U <= scanNode.size()) rr.valueU64_1 = readLe64(scanNode, valAbs + 8U);
                            if (valLen >= 24U && valAbs + 24U <= scanNode.size()) rr.valueU64_2 = readLe64(scanNode, valAbs + 16U);
                            if (keyLen >= 8U) {
                                rr.keyRaw = readLe64(scanNode, keyAbs);
                                rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                                    rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                                rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                            }
                            if (resolved.resolvedBtnLevel > 0 && valLen >= 8U) {
                                rr.branchChildOid = readLe64(scanNode, valAbs);
                                ++apfsSpotlightTargetScanMetrics.branchCandidatesQueued;
                                if (rr.branchChildOid != 0) enqueueSpotlightNode(rr, pending.depth + 1U);
                            }
                            if (rr.keyTypeRaw == 8U && keyLen >= 16U && valLen >= 16U) {
                                ApfsSpotlightFileExtentProbeRow er;
                                er.sequence = static_cast<std::uint32_t>(allSpotlightScanFileExtents.size());
                                er.volumeSequence = rr.volumeSequence;
                                er.targetRole = rr.targetRole;
                                er.fsOid = rr.fsOid;
                                er.volumeName = rr.volumeName;
                                er.extentFileId = rr.keyObjectId;
                                er.extentLogicalOffset = readLe64(scanNode, keyAbs + 8U);
                                er.lenAndFlags = readLe64(scanNode, valAbs + 0U);
                                er.extentLengthBytes = er.lenAndFlags & 0x00ffffffffffffffULL;
                                er.extentFlags = static_cast<std::uint32_t>((er.lenAndFlags >> 56U) & 0xffU);
                                er.physicalBlock = readLe64(scanNode, valAbs + 8U);
                                if (valLen >= 24U && valAbs + 24U <= scanNode.size()) er.cryptoId = readLe64(scanNode, valAbs + 16U);
                                if (nxSummary.blockSize != 0 && er.physicalBlock <= ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                    er.physicalOffset = er.physicalBlock * static_cast<std::uint64_t>(nxSummary.blockSize);
                                }
                                er.nodeOid = rr.nodeOid;
                                er.nodeVirtualOffset = rr.nodeVirtualOffset;
                                er.nodeLevel = rr.nodeLevel;
                                er.nodeNkeys = rr.nodeNkeys;
                                er.entryIndex = rr.entryIndex;
                                er.extentStatus = "FILE_EXTENT_CANDIDATE_SEEN";
                                er.interpretation = "APFS FILE_EXTENT record decoded during targeted Spotlight namespace scan; it will be matched to target child file IDs after the scan completes.";
                                er.notes = detail + "; scan_depth=" + std::to_string(pending.depth) + "; key_object_id_is_file_id";
                                allSpotlightScanFileExtents.push_back(er);
                            }
                            if (rr.keyTypeRaw == 3U && valLen >= 8U) {
                                ApfsSpotlightInodeProbeRow ir;
                                ir.sequence = static_cast<std::uint32_t>(allSpotlightScanInodes.size());
                                ir.volumeSequence = rr.volumeSequence;
                                ir.targetRole = rr.targetRole;
                                ir.fsOid = rr.fsOid;
                                ir.volumeName = rr.volumeName;
                                ir.inodeObjectId = rr.keyObjectId;
                                if (valLen >= 8U && valAbs + 8U <= scanNode.size()) ir.inodeParentId = readLe64(scanNode, valAbs + 0U);
                                if (valLen >= 16U && valAbs + 16U <= scanNode.size()) ir.inodePrivateId = readLe64(scanNode, valAbs + 8U);
                                if (valLen >= 24U && valAbs + 24U <= scanNode.size()) ir.inodeCreateTimeRaw = readLe64(scanNode, valAbs + 16U);
                                if (valLen >= 32U && valAbs + 32U <= scanNode.size()) ir.inodeModTimeRaw = readLe64(scanNode, valAbs + 24U);
                                if (valLen >= 40U && valAbs + 40U <= scanNode.size()) ir.inodeChangeTimeRaw = readLe64(scanNode, valAbs + 32U);
                                if (valLen >= 48U && valAbs + 48U <= scanNode.size()) ir.inodeAccessTimeRaw = readLe64(scanNode, valAbs + 40U);
                                if (valLen >= 56U && valAbs + 56U <= scanNode.size()) ir.inodeInternalFlags = readLe64(scanNode, valAbs + 48U);
                                if (valLen >= 60U && valAbs + 60U <= scanNode.size()) ir.inodeNchildrenOrNlink = readLe32(scanNode, valAbs + 56U);
                                if (valLen >= 84U && valAbs + 84U <= scanNode.size()) ir.inodeModeCandidate = readLe16(scanNode, valAbs + 82U);
                                if (valLen >= 92U && valAbs + 92U <= scanNode.size()) ir.inodeUncompressedSize = readLe64(scanNode, valAbs + 84U);
                                const ApfsInodeExtendedFieldDecode xf = decodeApfsInodeExtendedFieldsForProbe(scanNode, valAbs, valLen);
                                ir.inodeXfieldStatus = xf.status;
                                if (xf.sawDstream) {
                                    ir.inodeDstreamSize = xf.dstreamSize;
                                    ir.inodeDstreamAllocedSize = xf.dstreamAllocedSize;
                                    ir.inodeDstreamDefaultCryptoId = xf.dstreamDefaultCryptoId;
                                }
                                ir.nodeOid = rr.nodeOid;
                                ir.nodeVirtualOffset = rr.nodeVirtualOffset;
                                ir.nodeLevel = rr.nodeLevel;
                                ir.nodeNkeys = rr.nodeNkeys;
                                ir.entryIndex = rr.entryIndex;
                                ir.inodeStatus = "SCANNED_INODE_CANDIDATE";
                                ir.interpretation = "APFS INODE record decoded during filesystem-tree traversal and cached for later target-guided Store-V2 correlation.";
                                if (valLen > 0 && valAbs < scanNode.size()) {
                                    const std::size_t avail = std::min<std::size_t>(scanNode.size() - valAbs, valLen);
                                    ir.valueSampleHex = hexSampleBytes(scanNode.data() + valAbs, std::min<std::size_t>(avail, 96U));
                                }
                                ir.notes = detail + "; scan_depth=" + std::to_string(pending.depth) + "; cached_for_target_correlation=1";
                                if (!xf.notes.empty()) ir.notes += "; xfields=" + xf.notes;
                                allSpotlightScanInodes.push_back(std::move(ir));
                            }
                            if (rr.keyTypeRaw == 4U && keyLen >= 10U && valLen >= 4U) {
                                const std::uint16_t xnameLenRaw = readLe16(scanNode, keyAbs + 8U);
                                std::size_t xnameLen = static_cast<std::size_t>(xnameLenRaw);
                                if (xnameLen > keyLen - 10U) xnameLen = keyLen - 10U;
                                std::string xname = safePrintableUtf8Fragment(scanNode, keyAbs + 10U, xnameLen);
                                while (!xname.empty() && xname.back() == '\0') xname.pop_back();
                                ApfsSpotlightXattrProbeRow xr;
                                xr.sequence = static_cast<std::uint32_t>(apfsSpotlightXattrProbeRows.size());
                                xr.volumeSequence = rr.volumeSequence;
                                xr.targetRole = rr.targetRole;
                                xr.fsOid = rr.fsOid;
                                xr.volumeName = rr.volumeName;
                                xr.fileObjectId = rr.keyObjectId;
                                xr.xattrName = xname;
                                xr.xattrNameLength = xnameLenRaw;
                                xr.xattrFlags = readLe16(scanNode, valAbs + 0U);
                                xr.xdataLength = readLe16(scanNode, valAbs + 2U);
                                xr.xattrStorage = apfsXattrStorageLabel(xr.xattrFlags);
                                if ((xr.xattrFlags & 0x0001U) != 0 && valLen >= 12U && valAbs + 12U <= scanNode.size()) {
                                    // j_xattr_val.xdata contains j_xattr_dstream_t for stream-backed xattrs:
                                    // uint64_t xattr_obj_id; j_dstream_t dstream { size, alloced_size, default_crypto_id, ... }.
                                    xr.xdataStreamId = readLe64(scanNode, valAbs + 4U);
                                    if (valLen >= 20U && valAbs + 20U <= scanNode.size()) xr.xdataStreamSize = readLe64(scanNode, valAbs + 12U);
                                    if (valLen >= 28U && valAbs + 28U <= scanNode.size()) xr.xdataStreamAllocatedSize = readLe64(scanNode, valAbs + 20U);
                                    if (valLen >= 36U && valAbs + 36U <= scanNode.size()) xr.xdataStreamDefaultCryptoId = readLe64(scanNode, valAbs + 28U);
                                    xr.xdataPreviewStatus = (xr.xdataStreamSize != 0) ? "XATTR_DSTREAM_DECODED" : "XATTR_STREAM_ID_DECODED";
                                } else if ((xr.xattrFlags & 0x0002U) != 0 && valLen > 4U && valAbs + 4U <= scanNode.size()) {
                                    const std::size_t embeddedLen = std::min<std::size_t>(static_cast<std::size_t>(xr.xdataLength), std::min<std::size_t>(valLen - 4U, scanNode.size() - (valAbs + 4U)));
                                    std::vector<unsigned char> previewBytes;
                                    if (embeddedLen > 0) {
                                        const std::size_t previewLen = std::min<std::size_t>(embeddedLen, 96U);
                                        previewBytes.assign(scanNode.begin() + static_cast<std::ptrdiff_t>(valAbs + 4U), scanNode.begin() + static_cast<std::ptrdiff_t>(valAbs + 4U + previewLen));
                                        xr.xdataPreviewStatus = previewStatusForBytes(previewBytes);
                                        xr.xdataPreviewHex = hexSampleBytes(scanNode.data() + valAbs + 4U, previewLen);
                                    } else {
                                        xr.xdataPreviewStatus = "XATTR_EMBEDDED_EMPTY";
                                    }
                                } else {
                                    xr.xdataPreviewStatus = "NO_XATTR_DATA_PREVIEW";
                                }
                                xr.nodeOid = rr.nodeOid;
                                xr.nodeVirtualOffset = rr.nodeVirtualOffset;
                                xr.nodeLevel = rr.nodeLevel;
                                xr.nodeNkeys = rr.nodeNkeys;
                                xr.entryIndex = rr.entryIndex;
                                xr.xattrStatus = isApfsCompressionOrResourceXattrName(xname) ? "COMPRESSION_OR_RSRC_XATTR_SEEN" : "XATTR_SEEN";
                                xr.interpretation = "APFS XATTR record decoded during the bounded Store-V2 filesystem traversal. com.apple.decmpfs and com.apple.ResourceFork rows are used to triage compressed/resource-fork reconstruction needs.";
                                const std::string xnameLower = asciiLower(xname);
                                if (xnameLower == "com.apple.metadata:kmditemwherefroms") {
                                    xr.xattrStatus = "DOWNLOAD_ORIGIN_METADATA_XATTR_SEEN";
                                    xr.interpretation = "APFS WhereFroms metadata XATTR decoded during bounded Store-V2 filesystem traversal. This records source/origin metadata when present; investigator interpretation requires corroboration.";
                                    if (xr.xdataPreviewStatus != "XATTR_EMBEDDED_EMPTY" && !xr.xdataPreviewHex.empty()) {
                                        xr.notes += (xr.notes.empty() ? "" : "; ");
                                        xr.notes += "wherefroms_preview_hex=" + xr.xdataPreviewHex;
                                    }
                                }
                                xr.notes = detail + "; scan_depth=" + std::to_string(pending.depth) + "; key_object_id_is_file_id";
                                apfsSpotlightXattrProbeRows.push_back(std::move(xr));
                            }
                            if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                                const std::uint32_t nameLenAndHash = readLe32(scanNode, keyAbs + 8U);
                                const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                                rr.decodedName = safePrintableUtf8Fragment(scanNode, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                                ++apfsSpotlightTargetScanMetrics.dirRecordsDecoded;
                                if (rr.valueU64_0 != 0 && !rr.decodedName.empty()) {
                                    ApfsDirectoryRecordEntry ns;
                                    ns.volumeSequence = rr.volumeSequence;
                                    ns.targetRole = rr.targetRole;
                                    ns.fsOid = rr.fsOid;
                                    ns.volumeName = rr.volumeName;
                                    ns.parentObjectId = rr.keyObjectId;
                                    ns.childFileId = rr.valueU64_0;
                                    ns.name = rr.decodedName;
                                    apfsDirectoryRecordEntries.push_back(std::move(ns));
                                }
                                {
                                    ApfsRootTreeRecordSampleRow sample = rr;
                                    sample.sequence = static_cast<std::uint32_t>(apfsSpotlightNameScanSampleRows.size());
                                    sample.status = "DECODED_DIRECTORY_NAME";
                                    sample.interpretation = "Decoded APFS directory-record name encountered during the Store-V2 filesystem-tree traversal.";
                                    sample.notes = detail + "; scan_depth=" + std::to_string(pending.depth) + "; parent_node_oid=" + std::to_string(pending.parentNodeOid);
                                    apfsSpotlightNameScanSampleRows.push_back(sample);
                                }
                                if (isSpotlightTargetName(rr.decodedName)) {
                                    ++apfsSpotlightTargetScanMetrics.targetNameHits;
                                    rr.status = "SPOTLIGHT_TARGET_NAME_HIT";
                                    rr.interpretation = "Targeted APFS namespace scan found a Spotlight/CoreSpotlight-related directory-record name. Copy-out remains gated on path and file-extent resolution.";
                                    rr.notes = detail + "; scan_depth=" + std::to_string(pending.depth) + "; parent_node_oid=" + std::to_string(pending.parentNodeOid);
                                    apfsSpotlightTargetScanRows.push_back(rr);
                                    appendCopyAttempt(rr);
                                }
                            }
                        }
                    }

                    // V0_8_62: recursively seed copy attempts for files under Store-V2 group directories.
                    // Earlier versions targeted only filenames that looked like Spotlight components, which missed Cache/* and
                    // other ordinary names underneath the Store-V2 UUID directory.  Directory records are collected during
                    // the same bounded APFS filesystem-tree traversal above; this pass walks only from Store-V2 roots and
                    // keeps the single-AFF4/no-full-image-export policy intact.
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<std::size_t>> childrenByParent;
                        std::map<std::pair<std::uint32_t, std::uint64_t>, std::string> directoryNameByObject;
                        std::map<std::pair<std::uint32_t, std::uint64_t>, std::uint64_t> parentByObject;
                        std::set<std::pair<std::uint32_t, std::uint64_t>> directoriesSeen;
                        for (std::size_t i = 0; i < apfsDirectoryRecordEntries.size(); ++i) {
                            const auto& e = apfsDirectoryRecordEntries[i];
                            childrenByParent[std::make_pair(e.volumeSequence, e.parentObjectId)].push_back(i);
                            directoriesSeen.insert(std::make_pair(e.volumeSequence, e.parentObjectId));
                            if (e.childFileId != 0 && e.parentObjectId != 0) parentByObject[std::make_pair(e.volumeSequence, e.childFileId)] = e.parentObjectId;
                            if (!e.name.empty()) {
                                const auto childKey = std::make_pair(e.volumeSequence, e.childFileId);
                                if (!directoryNameByObject.count(childKey) || isLikelyStoreV2GroupDirectoryName(e.name)) {
                                    directoryNameByObject[childKey] = e.name;
                                }
                            }
                        }
                        auto directoryNameForObject = [&](std::uint32_t volumeSequence, std::uint64_t objectId) -> std::string {
                            const auto it = directoryNameByObject.find(std::make_pair(volumeSequence, objectId));
                            if (it == directoryNameByObject.end()) return {};
                            return it->second;
                        };
                        auto likelyGroupNameForDirectory = [&](std::uint32_t volumeSequence, std::uint64_t dirObjectId) -> std::string {
                            const std::string dirName = directoryNameForObject(volumeSequence, dirObjectId);
                            if (isLikelyStoreV2GroupDirectoryName(dirName)) return dirName;
                            return {};
                        };
                        auto apfsPathComponent = [](const std::string& value) -> std::string {
                            std::string out;
                            out.reserve(value.size());
                            for (const unsigned char ch : value) {
                                if (ch < 0x20 || ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') {
                                    out.push_back('_');
                                } else {
                                    out.push_back(static_cast<char>(ch));
                                }
                            }
                            while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
                            return out.empty() ? std::string("volume_") : out;
                        };
                        auto apfsAbsolutePathForObject = [&](std::uint32_t volumeSequence, std::uint64_t objectId) -> std::string {
                            std::vector<std::string> parts;
                            std::set<std::uint64_t> seenIds;
                            std::uint64_t cur = objectId;
                            for (;;) {
                                if (cur == 0 || !seenIds.insert(cur).second) break;
                                const auto nameIt = directoryNameByObject.find(std::make_pair(volumeSequence, cur));
                                if (nameIt != directoryNameByObject.end() && !nameIt->second.empty()) parts.push_back(nameIt->second);
                                if (cur == 2) break;
                                const auto parentIt = parentByObject.find(std::make_pair(volumeSequence, cur));
                                if (parentIt == parentByObject.end() || parentIt->second == cur) break;
                                cur = parentIt->second;
                            }
                            std::string prefix = (volumeSequence == 4) ? "/System/Volumes/Data" : ("/" + apfsPathComponent(directoryNameForObject(volumeSequence, 2)));
                            if (prefix == "/volume_") prefix = "/volume_" + std::to_string(volumeSequence);
                            std::string path = prefix;
                            for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
                                if (*it == "/" || it->empty()) continue;
                                if (path.empty() || path.back() != '/') path += "/";
                                path += *it;
                            }
                            return path;
                        };
                        std::set<std::tuple<std::uint32_t, std::uint64_t, std::uint64_t, std::string>> attemptSeen;
                        for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                            attemptSeen.insert(std::make_tuple(cr.volumeSequence, cr.parentObjectId, cr.childFileId, asciiLower(cr.targetName)));
                        }
                        struct StoreV2WalkItem {
                            std::uint32_t volumeSequence = 0;
                            std::uint64_t dirObjectId = 0;
                            std::uint64_t groupRootObjectId = 0;
                            std::string groupName;
                            std::string relPrefix;
                            std::uint32_t depth = 0;
                        };
                        std::vector<StoreV2WalkItem> queue;
                        std::set<std::pair<std::uint32_t, std::uint64_t>> queuedDirs;
                        auto enqueueDir = [&](std::uint32_t volumeSequence, std::uint64_t dirObjectId, std::uint64_t rootObjectId, const std::string& groupName, const std::string& relPrefix, std::uint32_t depth) {
                            if (dirObjectId == 0) return;
                            const auto key = std::make_pair(volumeSequence, dirObjectId);
                            if (!childrenByParent.count(key)) return;
                            if (!queuedDirs.insert(key).second) return;
                            StoreV2WalkItem item;
                            item.volumeSequence = volumeSequence;
                            item.dirObjectId = dirObjectId;
                            item.groupRootObjectId = rootObjectId ? rootObjectId : dirObjectId;
                            item.groupName = groupName;
                            item.relPrefix = relPrefix;
                            item.depth = depth;
                            queue.push_back(std::move(item));
                        };
                        for (const auto& e : apfsDirectoryRecordEntries) {
                            const std::string lname = asciiLower(e.name);
                            if (lname == "store-v2") {
                                enqueueDir(e.volumeSequence, e.childFileId, e.childFileId, "", "", 0);
                            }
                            if (isSpotlightStoreV2TopLevelComponentName(lname)) {
                                const std::string parentGroupName = likelyGroupNameForDirectory(e.volumeSequence, e.parentObjectId);
                                enqueueDir(e.volumeSequence, e.parentObjectId, e.parentObjectId, parentGroupName, "", 0);
                            }
                        }
                        std::size_t recursiveAttemptsAdded = 0;
                        std::set<std::pair<std::uint32_t, std::uint64_t>> recursiveDirsSeen;
                        for (std::size_t qi = 0; qi < queue.size(); ++qi) {
                            const auto item = queue[qi];
                            const auto childIt = childrenByParent.find(std::make_pair(item.volumeSequence, item.dirObjectId));
                            if (childIt == childrenByParent.end()) continue;
                            for (const std::size_t idx : childIt->second) {
                                const auto& e = apfsDirectoryRecordEntries[idx];
                                const std::string lname = asciiLower(e.name);
                                if (lname == ".spotlight-v100" || lname == "store-v2") continue;
                                const bool childIsDirectory = childrenByParent.count(std::make_pair(e.volumeSequence, e.childFileId)) > 0;
                                std::uint64_t groupRoot = item.groupRootObjectId;
                                std::string groupName = item.groupName;
                                std::string relPrefix = item.relPrefix;
                                if (groupName.empty() && item.depth == 0) {
                                    const std::string currentDirGroupName = likelyGroupNameForDirectory(item.volumeSequence, item.dirObjectId);
                                    if (!currentDirGroupName.empty()) {
                                        groupRoot = item.dirObjectId;
                                        groupName = currentDirGroupName;
                                        relPrefix.clear();
                                    } else if (childIsDirectory && isLikelyStoreV2GroupDirectoryName(e.name)) {
                                        groupRoot = e.childFileId;
                                        groupName = e.name;
                                        relPrefix.clear();
                                    }
                                }
                                const std::string relPath = relPrefix.empty() ? e.name : (relPrefix + "/" + e.name);
                                if (childIsDirectory) {
                                    const auto dirKey = std::make_pair(e.volumeSequence, e.childFileId);
                                    if (recursiveDirsSeen.insert(dirKey).second) {
                                        enqueueDir(e.volumeSequence, e.childFileId, groupRoot, groupName, relPath, item.depth + 1U);
                                    }
                                    continue;
                                }
                                const auto dedupeKey = std::make_tuple(e.volumeSequence, e.parentObjectId, e.childFileId, lname);
                                if (!attemptSeen.insert(dedupeKey).second) continue;
                                ApfsSpotlightCopyAttemptRow cr;
                                cr.sequence = static_cast<std::uint32_t>(apfsSpotlightCopyAttemptRows.size());
                                cr.volumeSequence = e.volumeSequence;
                                cr.targetRole = e.targetRole;
                                cr.fsOid = e.fsOid;
                                cr.volumeName = e.volumeName;
                                cr.parentObjectId = e.parentObjectId;
                                cr.childFileId = e.childFileId;
                                cr.targetName = e.name;
                                cr.targetKind = spotlightTargetKind(e.name);
                                cr.storeV2RootObjectId = groupRoot;
                                cr.storeV2GroupName = groupName;
                                cr.storeV2RelativePath = relPath;
                                cr.extractionStatus = "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED";
                                cr.interpretation = "Recursive Store-V2 namespace seeding added this ordinary-named file under a Store-V2 group directory so cache/content files can be compared against the external reference extraction.";
                                cr.notes = "V1_0_6 recursive Store-V2 seed with UUID group-name preservation and APFS catalog path context; group_root_object_id=" + std::to_string(groupRoot) + "; group_name=" + groupName + "; rel_path=" + relPath + "; apfs_absolute_path=" + apfsAbsolutePathForObject(e.volumeSequence, e.childFileId);
                                apfsSpotlightCopyAttemptRows.push_back(std::move(cr));
                                ++recursiveAttemptsAdded;
                            }
                        }

                    std::map<std::uint32_t, std::uint64_t> spotlightLogicalSizeByTargetSequence;
                    std::map<std::uint32_t, std::string> spotlightLogicalSizeSourceByTargetSequence;
                    std::map<std::uint32_t, std::uint64_t> spotlightPrivateIdByTargetSequence;

                    auto materializeScannedInodeForCopyAttempt = [&](const ApfsSpotlightCopyAttemptRow& cr, const ApfsSpotlightInodeProbeRow& cached) -> bool {
                        if (cr.childFileId == 0 || cached.volumeSequence != cr.volumeSequence || cached.inodeObjectId != cr.childFileId) return false;
                        ApfsSpotlightInodeProbeRow ir = cached;
                        ir.sequence = static_cast<std::uint32_t>(apfsSpotlightInodeProbeRows.size());
                        ir.targetSequence = cr.sequence;
                        ir.targetRole = cr.targetRole;
                        ir.fsOid = cr.fsOid;
                        ir.volumeName = cr.volumeName;
                        ir.targetParentObjectId = cr.parentObjectId;
                        ir.targetChildFileId = cr.childFileId;
                        ir.targetName = cr.targetName;
                        ir.targetKind = cr.targetKind;
                        ir.inodeStatus = "TARGET_INODE_SCAN_CORRELATION_HIT";
                        ir.interpretation = "Previously scanned APFS INODE row matched this Store-V2 target child file ID. This avoids relying only on later point lookups and enables private-id/dstream extent correlation.";
                        ir.notes += "; correlated_target_sequence=" + std::to_string(cr.sequence) + "; target_apfs_path=" + apfsAbsolutePathForObject(cr.volumeSequence, cr.childFileId);
                        if (ir.inodePrivateId != 0 && cr.targetKind != "APFS_RESOURCE_FORK_STREAM") spotlightPrivateIdByTargetSequence[cr.sequence] = ir.inodePrivateId;
                        if (ir.inodeDstreamSize != 0 && ir.inodeDstreamSize <= ir.inodeDstreamAllocedSize) {
                            spotlightLogicalSizeByTargetSequence[cr.sequence] = ir.inodeDstreamSize;
                            spotlightLogicalSizeSourceByTargetSequence[cr.sequence] = "INO_EXT_TYPE_DSTREAM.size.scan_correlation";
                        } else if (ir.inodeUncompressedSize != 0 && ir.inodeUncompressedSize <= (opt.pressureTestMode ? (std::numeric_limits<std::uint64_t>::max)() : (512ULL * 1024ULL * 1024ULL))) {
                            spotlightLogicalSizeByTargetSequence[cr.sequence] = ir.inodeUncompressedSize;
                            spotlightLogicalSizeSourceByTargetSequence[cr.sequence] = "j_inode_val.uncompressed_size.scan_correlation";
                        }
                        apfsSpotlightInodeProbeRows.push_back(std::move(ir));
                        return true;
                    };
                    struct SpotlightInodeLookupKey {
                        std::uint32_t volumeSequence = 0;
                        std::uint64_t childFileId = 0;
                        bool operator==(const SpotlightInodeLookupKey& other) const noexcept {
                            return volumeSequence == other.volumeSequence && childFileId == other.childFileId;
                        }
                    };
                    struct SpotlightInodeLookupKeyHash {
                        std::size_t operator()(const SpotlightInodeLookupKey& k) const noexcept {
                            const std::size_t a = std::hash<std::uint32_t>{}(k.volumeSequence);
                            const std::size_t b = std::hash<std::uint64_t>{}(k.childFileId);
                            return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
                        }
                    };
                    std::unordered_map<SpotlightInodeLookupKey, const ApfsSpotlightInodeProbeRow*, SpotlightInodeLookupKeyHash> scannedInodeByTargetKey;
                    scannedInodeByTargetKey.reserve(allSpotlightScanInodes.size() * 2U + 1U);
                    for (const auto& cached : allSpotlightScanInodes) {
                        if (cached.inodeObjectId == 0) continue;
                        SpotlightInodeLookupKey key{cached.volumeSequence, cached.inodeObjectId};
                        scannedInodeByTargetKey.emplace(key, &cached);
                    }
                    for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                        if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM" || cr.childFileId == 0) continue;
                        const auto it = scannedInodeByTargetKey.find(SpotlightInodeLookupKey{cr.volumeSequence, cr.childFileId});
                        if (it != scannedInodeByTargetKey.end() && it->second) {
                            materializeScannedInodeForCopyAttempt(cr, *it->second);
                        }
                    }

                    // V0_8_63: add bounded resource-fork stream copy attempts for Store-V2 files that have
                    // com.apple.decmpfs resource-fork compression markers plus a stream-backed com.apple.ResourceFork
                    // XATTR.  These diagnostic stream copies are intentionally not assigned Store-V2 relative paths,
                    // so they are not staged into the external comparison tree until decompression/reconstruction is
                    // implemented and validated.
                    {
                        std::map<std::uint64_t, std::size_t> storeV2AttemptIndexByFileId;
                        for (std::size_t i = 0; i < apfsSpotlightCopyAttemptRows.size(); ++i) {
                            const auto& cr = apfsSpotlightCopyAttemptRows[i];
                            if (cr.childFileId == 0 || cr.storeV2RelativePath.empty()) continue;
                            if (storeV2AttemptIndexByFileId.find(cr.childFileId) == storeV2AttemptIndexByFileId.end()) {
                                storeV2AttemptIndexByFileId[cr.childFileId] = i;
                            }
                        }

                        std::map<std::uint64_t, int> decmpfsResourceForkTypeByFileId;
                        std::map<std::uint64_t, std::uint64_t> resourceForkStreamIdByFileId;
                        std::map<std::uint64_t, std::uint64_t> resourceForkStreamSizeByFileId;
                        std::map<std::uint64_t, std::uint64_t> resourceForkStreamAllocedSizeByFileId;
                        for (const auto& xr : apfsSpotlightXattrProbeRows) {
                            const std::string lname = asciiLower(xr.xattrName);
                            if (lname == "com.apple.decmpfs") {
                                const int ctype = decmpfsCompressionTypeFromPreviewHex(xr.xdataPreviewHex);
                                if (ctype == 4 || ctype == 8 || ctype == 10 || ctype == 12 || ctype == 14) {
                                    decmpfsResourceForkTypeByFileId[xr.fileObjectId] = ctype;
                                }
                            } else if (lname == "com.apple.resourcefork" && xr.xdataStreamId != 0 && xr.xattrStorage == "XATTR_DATA_STREAM") {
                                resourceForkStreamIdByFileId[xr.fileObjectId] = xr.xdataStreamId;
                                if (xr.xdataStreamSize != 0) resourceForkStreamSizeByFileId[xr.fileObjectId] = xr.xdataStreamSize;
                                if (xr.xdataStreamAllocatedSize != 0) resourceForkStreamAllocedSizeByFileId[xr.fileObjectId] = xr.xdataStreamAllocatedSize;
                            }
                        }

                        std::set<std::tuple<std::uint32_t, std::uint64_t, std::uint64_t>> resourceForkAttemptsSeen;
                        for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                            if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM") {
                                resourceForkAttemptsSeen.insert(std::make_tuple(cr.volumeSequence, cr.parentObjectId, cr.childFileId));
                            }
                        }

                        constexpr std::size_t kMaxResourceForkStreamCopyAttempts = 30000U;
                        std::size_t addedResourceForkStreamAttempts = 0;
                        std::vector<std::uint64_t> resourceForkOriginalFileIds;
                        resourceForkOriginalFileIds.reserve(resourceForkStreamIdByFileId.size());
                        for (const auto& kv : resourceForkStreamIdByFileId) resourceForkOriginalFileIds.push_back(kv.first);
                        auto resourceForkAttemptPriority = [&](std::uint64_t originalFileId) -> int {
                            const auto attemptIt = storeV2AttemptIndexByFileId.find(originalFileId);
                            if (attemptIt == storeV2AttemptIndexByFileId.end()) return 1000;
                            const ApfsSpotlightCopyAttemptRow& base = apfsSpotlightCopyAttemptRows[attemptIt->second];
                            const std::string relLower = asciiLower(base.storeV2RelativePath);
                            const std::string nameLower = asciiLower(base.targetName);
                            const auto ctypeIt = decmpfsResourceForkTypeByFileId.find(originalFileId);
                            const int ctype = (ctypeIt == decmpfsResourceForkTypeByFileId.end()) ? 0 : ctypeIt->second;
                            const bool cacheTxt = (relLower.find("cache/") != std::string::npos && nameLower.size() >= 4 && nameLower.rfind(".txt") == nameLower.size() - 4);
                            // V0_8_73: prioritize likely comparison-closing Store-V2 Cache ZLIB_RSRC files first,
                            // then other Cache resource-fork files, before generic Store-V2 resource-fork rows.
                            if (cacheTxt && ctype == 4) return 0;
                            if (cacheTxt && ctype == 10) return 1;
                            if (cacheTxt && (ctype == 8 || ctype == 12 || ctype == 14)) return 2;
                            if (cacheTxt) return 3;
                            if (base.targetKind == "SPOTLIGHT_RELATED_NAME" && ctype == 4) return 10;
                            if (base.targetKind == "SPOTLIGHT_RELATED_NAME") return 20;
                            if (!base.storeV2RelativePath.empty() && ctype == 4) return 30;
                            if (!base.storeV2RelativePath.empty()) return 40;
                            return 100;
                        };
                        std::sort(resourceForkOriginalFileIds.begin(), resourceForkOriginalFileIds.end(), [&](std::uint64_t a, std::uint64_t b) {
                            const int pa = resourceForkAttemptPriority(a);
                            const int pb = resourceForkAttemptPriority(b);
                            if (pa != pb) return pa < pb;
                            return a < b;
                        });
                        for (const std::uint64_t originalFileId : resourceForkOriginalFileIds) {
                            if (addedResourceForkStreamAttempts >= kMaxResourceForkStreamCopyAttempts) break;
                            const auto rfIdIt0 = resourceForkStreamIdByFileId.find(originalFileId);
                            if (rfIdIt0 == resourceForkStreamIdByFileId.end()) continue;
                            const std::uint64_t resourceForkStreamId = rfIdIt0->second;
                            if (resourceForkStreamId == 0) continue;
                            const auto attemptIt = storeV2AttemptIndexByFileId.find(originalFileId);
                            if (attemptIt == storeV2AttemptIndexByFileId.end()) continue;
                            const auto ctypeIt = decmpfsResourceForkTypeByFileId.find(originalFileId);
                            if (ctypeIt == decmpfsResourceForkTypeByFileId.end()) continue;
                            const ApfsSpotlightCopyAttemptRow& base = apfsSpotlightCopyAttemptRows[attemptIt->second];
                            const auto seenKey = std::make_tuple(base.volumeSequence, originalFileId, resourceForkStreamId);
                            if (!resourceForkAttemptsSeen.insert(seenKey).second) continue;

                            ApfsSpotlightCopyAttemptRow cr = base;
                            cr.sequence = static_cast<std::uint32_t>(apfsSpotlightCopyAttemptRows.size());
                            cr.parentObjectId = originalFileId;
                            cr.childFileId = resourceForkStreamId;
                            cr.targetName = base.targetName + ".__RESOURCEFORK_STREAM";
                            cr.targetKind = "APFS_RESOURCE_FORK_STREAM";
                            cr.storeV2RootObjectId = 0;
                            cr.storeV2GroupName.clear();
                            cr.storeV2RelativePath.clear();
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED";
                            cr.interpretation = "Synthetic diagnostic copy attempt for a stream-backed com.apple.ResourceFork associated with a compressed APFS Store-V2 file. The stream is copied separately so later builds can parse/decompress the decmpfs resource-fork payload without polluting the Store-V2 comparison tree.";
                            cr.notes = "V0_8_68 resource-fork stream seed; original_file_id=" + std::to_string(originalFileId) + "; resource_fork_stream_id=" + std::to_string(resourceForkStreamId) + "; decmpfs_compression_type=" + std::to_string(ctypeIt->second) + "; original_storev2_relative_path=" + base.storeV2RelativePath;
                            const auto rfSizeIt = resourceForkStreamSizeByFileId.find(originalFileId);
                            if (rfSizeIt != resourceForkStreamSizeByFileId.end() && rfSizeIt->second != 0) {
                                spotlightLogicalSizeByTargetSequence[cr.sequence] = rfSizeIt->second;
                                spotlightLogicalSizeSourceByTargetSequence[cr.sequence] = "com.apple.ResourceFork.j_xattr_dstream.size";
                                cr.notes += "; resource_fork_stream_size=" + std::to_string(rfSizeIt->second);
                            }
                            const auto rfAllocIt = resourceForkStreamAllocedSizeByFileId.find(originalFileId);
                            if (rfAllocIt != resourceForkStreamAllocedSizeByFileId.end() && rfAllocIt->second != 0) {
                                cr.notes += "; resource_fork_alloced_size=" + std::to_string(rfAllocIt->second);
                            }
                            apfsSpotlightCopyAttemptRows.push_back(std::move(cr));
                            ++addedResourceForkStreamAttempts;
                        }
                        if (addedResourceForkStreamAttempts != 0) {
                            log.info("Added APFS resource-fork stream copy attempts: " + std::to_string(addedResourceForkStreamAttempts));
                        }
                    }

                    auto compareApfsFsKeyForGuidedLookup = [&](std::uint64_t obj, std::uint8_t type, std::uint64_t logical,
                                                                std::uint64_t targetObj, std::uint8_t targetType, std::uint64_t targetLogical) -> int {
                        if (obj < targetObj) return -1;
                        if (obj > targetObj) return 1;
                        if (type < targetType) return -1;
                        if (type > targetType) return 1;
                        if (logical < targetLogical) return -1;
                        if (logical > targetLogical) return 1;
                        return 0;
                    };

                    auto appendGuidedNoMatchExtentRow = [&](const ApfsSpotlightCopyAttemptRow& cr,
                                                            std::uint64_t candidateObjectId,
                                                            const std::string& status,
                                                            const std::string& detail) {
                        ApfsSpotlightFileExtentProbeRow er;
                        er.sequence = static_cast<std::uint32_t>(apfsSpotlightFileExtentProbeRows.size());
                        er.targetSequence = cr.sequence;
                        er.volumeSequence = cr.volumeSequence;
                        er.targetRole = cr.targetRole;
                        er.fsOid = cr.fsOid;
                        er.volumeName = cr.volumeName;
                        er.targetParentObjectId = cr.parentObjectId;
                        er.targetChildFileId = cr.childFileId;
                        er.targetName = cr.targetName;
                        er.targetKind = cr.targetKind;
                        er.extentFileId = candidateObjectId;
                        er.extentStatus = status;
                        er.previewStatus = "NO_PREVIEW";
                        er.interpretation = "Guided APFS filesystem B-tree lookup for a Spotlight target FILE_EXTENT candidate did not produce a copyable extent row.";
                        er.notes = detail;
                        apfsSpotlightFileExtentProbeRows.push_back(er);
                    };

                    auto appendGuidedNoMatchInodeRow = [&](const ApfsSpotlightCopyAttemptRow& cr,
                                                           const std::string& status,
                                                           const std::string& detail) {
                        ApfsSpotlightInodeProbeRow ir;
                        ir.sequence = static_cast<std::uint32_t>(apfsSpotlightInodeProbeRows.size());
                        ir.targetSequence = cr.sequence;
                        ir.volumeSequence = cr.volumeSequence;
                        ir.targetRole = cr.targetRole;
                        ir.fsOid = cr.fsOid;
                        ir.volumeName = cr.volumeName;
                        ir.targetParentObjectId = cr.parentObjectId;
                        ir.targetChildFileId = cr.childFileId;
                        ir.targetName = cr.targetName;
                        ir.targetKind = cr.targetKind;
                        ir.inodeObjectId = cr.childFileId;
                        ir.inodeStatus = status;
                        ir.interpretation = "Guided APFS filesystem B-tree lookup did not resolve an INODE record for the Spotlight/CoreSpotlight target child file ID or derived object-ID candidate.";
                        ir.notes = detail;
                        apfsSpotlightInodeProbeRows.push_back(ir);
                    };

                    // V1.3.2: Reuse APFS B-tree node buffers across guided target lookups.
                    // These guided lookups run in tight loops over staged Spotlight candidates; keeping the
                    // backing storage alive avoids repeated heap allocation while preserving the existing
                    // read and provenance semantics.
                    std::vector<unsigned char> guidedInodeNodeBuffer;
                    std::vector<unsigned char> guidedExtentNodeBuffer;
                    guidedInodeNodeBuffer.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(nxSummary.blockSize ? nxSummary.blockSize : 65536ULL, 1048576ULL)));
                    guidedExtentNodeBuffer.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(nxSummary.blockSize ? nxSummary.blockSize : 65536ULL, 1048576ULL)));

                    auto lookupGuidedTargetInode = [&](const ApfsSpotlightCopyAttemptRow& cr,
                                                         std::uint64_t candidateObjectId,
                                                         const std::string& candidateLabel) -> bool {
                        if (cr.childFileId == 0 || candidateObjectId == 0) return false;
                        const ApfsVolumeRootTreeLookupRow* lookupPtr = nullptr;
                        for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                            if (lookup.volumeSequence == cr.volumeSequence && lookup.rootTreeStatus == "ROOT_TREE_BTREE_READ" && lookup.resolvedVirtualOffset != 0) {
                                lookupPtr = &lookup;
                                break;
                            }
                        }
                        if (lookupPtr == nullptr) {
                            appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_ROOT_TREE_UNAVAILABLE", "candidate=" + candidateLabel + "; no readable root-tree lookup row for this volume");
                            return false;
                        }
                        auto omIt = apfsVolumeOmapRowsByVolumeSequence.find(cr.volumeSequence);
                        auto rootIt = apfsVolumeOmapRootNodesByVolumeSequence.find(cr.volumeSequence);
                        if (omIt == apfsVolumeOmapRowsByVolumeSequence.end() || rootIt == apfsVolumeOmapRootNodesByVolumeSequence.end()) {
                            appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_VOLUME_OMAP_UNAVAILABLE", "candidate=" + candidateLabel + "; volume OMAP root buffer unavailable");
                            return false;
                        }
                        auto& node = guidedInodeNodeBuffer;
                        node.clear();
                        std::string readErr;
                        long long nodeRead = readVirtual(lookupPtr->resolvedVirtualOffset, nxSummary.blockSize, node, readErr);
                        std::uint64_t nodeOid = lookupPtr->resolvedObjectOid ? lookupPtr->resolvedObjectOid : lookupPtr->apfsRootTreeOid;
                        std::uint64_t nodeOffset = lookupPtr->resolvedVirtualOffset;
                        if (nodeRead <= 0 || node.size() < 64) {
                            appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_ROOT_READ_FAILED", "candidate=" + candidateLabel + "; " + (readErr.empty() ? std::string("root tree read failed") : readErr));
                            return false;
                        }

                        constexpr std::uint8_t kTargetTypeInode = 3U;
                        constexpr std::uint32_t kMaxGuidedDepth = 16U;
                        std::set<std::uint64_t> visitedGuidedNodes;
                        std::string branchPath;
                        for (std::uint32_t depth = 0; depth < kMaxGuidedDepth; ++depth) {
                            if (node.size() < 64) break;
                            if (!visitedGuidedNodes.insert(nodeOid).second) {
                                appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_CYCLE_DETECTED", "candidate=" + candidateLabel + "; path=" + branchPath);
                                return false;
                            }
                            const std::uint32_t rawType = readLe32(node, 24);
                            const std::string objectLabel = apfsObjectTypeLabel(rawType);
                            const std::uint16_t level = readLe16(node, 34);
                            const std::uint32_t nkeys = readLe32(node, 36);
                            if (!branchPath.empty()) branchPath += " -> ";
                            branchPath += "oid=" + std::to_string(nodeOid) + ";level=" + std::to_string(level) + ";nkeys=" + std::to_string(nkeys);
                            if (objectLabel != "BTREE" && objectLabel != "BTREE_NODE") {
                                appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_UNEXPECTED_OBJECT_TYPE", "candidate=" + candidateLabel + "; object_type=" + objectLabel + "; path=" + branchPath);
                                return false;
                            }
                            const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
                            if (level == 0) {
                                bool exactObjectSeen = false;
                                for (std::uint32_t i = 0; i < limit; ++i) {
                                    std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                    std::string detail;
                                    if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                                    if (keyLen < 8U) continue;
                                    const std::uint64_t keyRaw = readLe64(node, keyAbs);
                                    const std::uint64_t obj = apfsFsKeyObjectId(keyRaw);
                                    const std::uint8_t typ = apfsFsKeyRecordType(keyRaw);
                                    if (obj == candidateObjectId) exactObjectSeen = true;
                                    if (obj != candidateObjectId || typ != kTargetTypeInode) continue;
                                    ApfsSpotlightInodeProbeRow ir;
                                    ir.sequence = static_cast<std::uint32_t>(apfsSpotlightInodeProbeRows.size());
                                    ir.targetSequence = cr.sequence;
                                    ir.volumeSequence = cr.volumeSequence;
                                    ir.targetRole = cr.targetRole;
                                    ir.fsOid = cr.fsOid;
                                    ir.volumeName = cr.volumeName;
                                    ir.targetParentObjectId = cr.parentObjectId;
                                    ir.targetChildFileId = cr.childFileId;
                                    ir.targetName = cr.targetName;
                                    ir.targetKind = cr.targetKind;
                                    ir.inodeObjectId = obj;
                                    if (valLen >= 8U && valAbs + 8U <= node.size()) ir.inodeParentId = readLe64(node, valAbs + 0U);
                                    if (valLen >= 16U && valAbs + 16U <= node.size()) ir.inodePrivateId = readLe64(node, valAbs + 8U);
                                    if (valLen >= 24U && valAbs + 24U <= node.size()) ir.inodeCreateTimeRaw = readLe64(node, valAbs + 16U);
                                    if (valLen >= 32U && valAbs + 32U <= node.size()) ir.inodeModTimeRaw = readLe64(node, valAbs + 24U);
                                    if (valLen >= 40U && valAbs + 40U <= node.size()) ir.inodeChangeTimeRaw = readLe64(node, valAbs + 32U);
                                    if (valLen >= 48U && valAbs + 48U <= node.size()) ir.inodeAccessTimeRaw = readLe64(node, valAbs + 40U);
                                    if (valLen >= 56U && valAbs + 56U <= node.size()) ir.inodeInternalFlags = readLe64(node, valAbs + 48U);
                                    if (valLen >= 60U && valAbs + 60U <= node.size()) ir.inodeNchildrenOrNlink = readLe32(node, valAbs + 56U);
                                    if (valLen >= 84U && valAbs + 84U <= node.size()) ir.inodeModeCandidate = readLe16(node, valAbs + 82U);
                                    if (valLen >= 92U && valAbs + 92U <= node.size()) ir.inodeUncompressedSize = readLe64(node, valAbs + 84U);
                                    const ApfsInodeExtendedFieldDecode xf = decodeApfsInodeExtendedFieldsForProbe(node, valAbs, valLen);
                                    ir.inodeXfieldStatus = xf.status;
                                    if (xf.sawDstream) {
                                        ir.inodeDstreamSize = xf.dstreamSize;
                                        ir.inodeDstreamAllocedSize = xf.dstreamAllocedSize;
                                        ir.inodeDstreamDefaultCryptoId = xf.dstreamDefaultCryptoId;
                                    }
                                    ir.nodeOid = nodeOid;
                                    ir.nodeVirtualOffset = nodeOffset;
                                    ir.nodeLevel = level;
                                    ir.nodeNkeys = nkeys;
                                    ir.entryIndex = i;
                                    ir.inodeStatus = "TARGET_INODE_GUIDED_LOOKUP_HIT";
                                    ir.interpretation = "Guided APFS filesystem B-tree lookup matched an INODE record for a Spotlight/CoreSpotlight target object-ID candidate. The inode private ID is treated as a data-stream/extent lookup candidate.";
                                    if (valLen > 0 && valAbs < node.size()) {
                                        const std::size_t avail = std::min<std::size_t>(node.size() - valAbs, valLen);
                                        ir.valueSampleHex = hexSampleBytes(node.data() + valAbs, std::min<std::size_t>(avail, 96U));
                                    }
                                    ir.notes = "candidate=" + candidateLabel + "; candidate_object_id=" + std::to_string(candidateObjectId) + "; branch_path=" + branchPath + "; " + detail;
                                    if (!xf.notes.empty()) ir.notes += "; xfields=" + xf.notes;
                                    if (ir.inodePrivateId != 0 && cr.targetKind != "APFS_RESOURCE_FORK_STREAM") spotlightPrivateIdByTargetSequence[cr.sequence] = ir.inodePrivateId;
                                    const auto existingLogicalSourceIt = spotlightLogicalSizeSourceByTargetSequence.find(cr.sequence);
                                    const bool preserveExplicitXattrDstreamSize =
                                        (cr.targetKind == "APFS_RESOURCE_FORK_STREAM" &&
                                         existingLogicalSourceIt != spotlightLogicalSizeSourceByTargetSequence.end() &&
                                         existingLogicalSourceIt->second.find("com.apple.ResourceFork.j_xattr_dstream.size") != std::string::npos);
                                    if (!preserveExplicitXattrDstreamSize) {
                                        if (ir.inodeDstreamSize != 0 && ir.inodeDstreamSize <= ir.inodeDstreamAllocedSize) {
                                            spotlightLogicalSizeByTargetSequence[cr.sequence] = ir.inodeDstreamSize;
                                            spotlightLogicalSizeSourceByTargetSequence[cr.sequence] = "INO_EXT_TYPE_DSTREAM.size";
                                        } else if (ir.inodeUncompressedSize != 0 && ir.inodeUncompressedSize <= (opt.pressureTestMode ? (std::numeric_limits<std::uint64_t>::max)() : (512ULL * 1024ULL * 1024ULL))) {
                                            spotlightLogicalSizeByTargetSequence[cr.sequence] = ir.inodeUncompressedSize;
                                            spotlightLogicalSizeSourceByTargetSequence[cr.sequence] = "j_inode_val.uncompressed_size";
                                        }
                                    } else {
                                        ir.notes += "; preserved_explicit_resource_fork_xattr_dstream_size_for_dissect_apfs_file_stream_model";
                                    }
                                    apfsSpotlightInodeProbeRows.push_back(ir);
                                    return true;
                                }
                                appendGuidedNoMatchInodeRow(cr, exactObjectSeen ? "GUIDED_INODE_LOOKUP_OBJECT_SEEN_NO_INODE" : "GUIDED_INODE_LOOKUP_NO_MATCH_IN_LEAF", "candidate=" + candidateLabel + "; candidate_object_id=" + std::to_string(candidateObjectId) + "; branch_path=" + branchPath);
                                return false;
                            }

                            bool childFound = false;
                            std::uint64_t selectedChildOid = 0;
                            std::uint32_t selectedEntry = 0;
                            std::uint64_t firstChild = 0;
                            std::uint32_t firstEntry = 0;
                            bool haveFirstChild = false;
                            for (std::uint32_t i = 0; i < limit; ++i) {
                                std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                std::string detail;
                                if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                                if (keyLen < 8U || valLen < 8U || valAbs + 8U > node.size()) continue;
                                const std::uint64_t keyRaw = readLe64(node, keyAbs);
                                const std::uint64_t obj = apfsFsKeyObjectId(keyRaw);
                                const std::uint8_t typ = apfsFsKeyRecordType(keyRaw);
                                const std::uint64_t childOid = readLe64(node, valAbs);
                                if (childOid == 0) continue;
                                if (!haveFirstChild) { firstChild = childOid; firstEntry = i; haveFirstChild = true; }
                                const int cmp = compareApfsFsKeyForGuidedLookup(obj, typ, 0, candidateObjectId, kTargetTypeInode, 0);
                                if (cmp <= 0) {
                                    // APFS filesystem B+ tree internal keys index the smallest key in each child.
                                    // Select the last child whose separator key is <= the target key.
                                    selectedChildOid = childOid;
                                    selectedEntry = i;
                                    childFound = true;
                                    continue;
                                }
                                if (!childFound) {
                                    // Target sorts before the first separator; descend to the first child.
                                    selectedChildOid = firstChild;
                                    selectedEntry = firstEntry;
                                    childFound = true;
                                }
                                break;
                            }
                            if (!childFound || selectedChildOid == 0) {
                                appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_BRANCH_CHILD_NOT_SELECTED", "candidate=" + candidateLabel + "; path=" + branchPath);
                                return false;
                            }
                            ApfsOmapTargetResolution resolved = aff4ResolveVolumeOmapTargetObjectForProbe(omIt->second, rootIt->second, selectedChildOid, lookupPtr->targetXid, nxSummary.blockSize, readVirtual, safeSpotlightNodeOffset, "APFS guided Spotlight target INODE lookup", dynamicProbeCancelled);
                            if (resolved.objectStatus != "OMAP_TARGET_BTREE_READ") {
                                appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_CHILD_READ_FAILED", "candidate=" + candidateLabel + "; selected_entry=" + std::to_string(selectedEntry) + "; child_oid=" + std::to_string(selectedChildOid) + "; lookup_status=" + resolved.lookupStatus + "; object_status=" + resolved.objectStatus + "; path=" + branchPath);
                                return false;
                            }
                            node.swap(resolved.resolvedBuffer);
                            nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : selectedChildOid;
                            nodeOffset = resolved.resolvedVirtualOffset;
                        }
                        appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_DEPTH_LIMIT", "candidate=" + candidateLabel + "; branch_path=" + branchPath);
                        return false;
                    };

                    auto lookupGuidedFileExtentCandidate = [&](const ApfsSpotlightCopyAttemptRow& cr,
                                                               std::uint64_t candidateObjectId,
                                                               const std::string& candidateLabel,
                                                               std::uint64_t requestedLogicalOffset) -> bool {
                        if (cr.childFileId == 0 || candidateObjectId == 0) return false;
                        const ApfsVolumeRootTreeLookupRow* lookupPtr = nullptr;
                        for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                            if (lookup.volumeSequence == cr.volumeSequence && lookup.rootTreeStatus == "ROOT_TREE_BTREE_READ" && lookup.resolvedVirtualOffset != 0) {
                                lookupPtr = &lookup;
                                break;
                            }
                        }
                        if (lookupPtr == nullptr) {
                            appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_ROOT_TREE_UNAVAILABLE", "candidate=" + candidateLabel + "; no readable root-tree lookup row for this volume");
                            return false;
                        }
                        auto omIt = apfsVolumeOmapRowsByVolumeSequence.find(cr.volumeSequence);
                        auto rootIt = apfsVolumeOmapRootNodesByVolumeSequence.find(cr.volumeSequence);
                        if (omIt == apfsVolumeOmapRowsByVolumeSequence.end() || rootIt == apfsVolumeOmapRootNodesByVolumeSequence.end()) {
                            appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_VOLUME_OMAP_UNAVAILABLE", "candidate=" + candidateLabel + "; volume OMAP root buffer unavailable");
                            return false;
                        }

                        auto& node = guidedExtentNodeBuffer;
                        node.clear();
                        std::string readErr;
                        long long nodeRead = readVirtual(lookupPtr->resolvedVirtualOffset, nxSummary.blockSize, node, readErr);
                        std::uint64_t nodeOid = lookupPtr->resolvedObjectOid ? lookupPtr->resolvedObjectOid : lookupPtr->apfsRootTreeOid;
                        std::uint64_t nodeOffset = lookupPtr->resolvedVirtualOffset;
                        if (nodeRead <= 0 || node.size() < 64) {
                            appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_ROOT_READ_FAILED", "candidate=" + candidateLabel + "; " + (readErr.empty() ? std::string("root tree read failed") : readErr));
                            return false;
                        }

                        constexpr std::uint8_t kTargetTypeFileExtent = 8U;
                        const std::uint64_t kTargetLogicalOffset = requestedLogicalOffset;
                        constexpr std::uint32_t kMaxGuidedDepth = 16U;
                        std::set<std::uint64_t> visitedGuidedNodes;
                        bool anyHit = false;
                        std::string branchPath;
                        for (std::uint32_t depth = 0; depth < kMaxGuidedDepth; ++depth) {
                            if (node.size() < 64) break;
                            if (!visitedGuidedNodes.insert(nodeOid).second) {
                                appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_CYCLE_DETECTED", "candidate=" + candidateLabel + "; path=" + branchPath);
                                return anyHit;
                            }
                            const std::uint32_t rawType = readLe32(node, 24);
                            const std::string objectLabel = apfsObjectTypeLabel(rawType);
                            const std::uint16_t level = readLe16(node, 34);
                            const std::uint32_t nkeys = readLe32(node, 36);
                            if (!branchPath.empty()) branchPath += " -> ";
                            branchPath += "oid=" + std::to_string(nodeOid) + ";level=" + std::to_string(level) + ";nkeys=" + std::to_string(nkeys);
                            if (objectLabel != "BTREE" && objectLabel != "BTREE_NODE") {
                                appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_UNEXPECTED_OBJECT_TYPE", "candidate=" + candidateLabel + "; object_type=" + objectLabel + "; path=" + branchPath);
                                return anyHit;
                            }
                            const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
                            if (level == 0) {
                                bool exactObjectSeen = false;
                                bool exactTypeSeen = false;
                                for (std::uint32_t i = 0; i < limit; ++i) {
                                    std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                    std::string detail;
                                    if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                                    if (keyLen < 8U) continue;
                                    const std::uint64_t keyRaw = readLe64(node, keyAbs);
                                    const std::uint64_t obj = apfsFsKeyObjectId(keyRaw);
                                    const std::uint8_t typ = apfsFsKeyRecordType(keyRaw);
                                    const std::uint64_t logical = (keyLen >= 16U && keyAbs + 16U <= node.size()) ? readLe64(node, keyAbs + 8U) : 0ULL;
                                    if (obj == candidateObjectId) exactObjectSeen = true;
                                    if (obj == candidateObjectId && typ == kTargetTypeFileExtent) exactTypeSeen = true;
                                    if (obj != candidateObjectId || typ != kTargetTypeFileExtent) continue;
                                    if (valLen < 16U || valAbs + 16U > node.size()) continue;
                                    ApfsSpotlightFileExtentProbeRow er;
                                    er.sequence = static_cast<std::uint32_t>(allSpotlightScanFileExtents.size());
                                    er.targetSequence = cr.sequence;
                                    er.volumeSequence = cr.volumeSequence;
                                    er.targetRole = cr.targetRole;
                                    er.fsOid = cr.fsOid;
                                    er.volumeName = cr.volumeName;
                                    er.targetParentObjectId = cr.parentObjectId;
                                    er.targetChildFileId = cr.childFileId;
                                    er.targetName = cr.targetName;
                                    er.targetKind = cr.targetKind;
                                    // Use the directory-record child id as the match key so the existing copy-attempt updater can attach this row to the target.
                                    er.extentFileId = cr.childFileId;
                                    er.extentLogicalOffset = logical;
                                    er.lenAndFlags = readLe64(node, valAbs + 0U);
                                    er.extentLengthBytes = er.lenAndFlags & 0x00ffffffffffffffULL;
                                    er.extentFlags = static_cast<std::uint32_t>((er.lenAndFlags >> 56U) & 0xffU);
                                    er.physicalBlock = readLe64(node, valAbs + 8U);
                                    if (valLen >= 24U && valAbs + 24U <= node.size()) er.cryptoId = readLe64(node, valAbs + 16U);
                                    if (nxSummary.blockSize != 0 && er.physicalBlock <= ((std::numeric_limits<std::uint64_t>::max)() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                        er.physicalOffset = er.physicalBlock * static_cast<std::uint64_t>(nxSummary.blockSize);
                                    }
                                    er.nodeOid = nodeOid;
                                    er.nodeVirtualOffset = nodeOffset;
                                    er.nodeLevel = level;
                                    er.nodeNkeys = nkeys;
                                    er.entryIndex = i;
                                    er.extentStatus = "TARGET_FILE_EXTENT_GUIDED_LOOKUP_HIT";
                                    er.interpretation = "Guided APFS filesystem B-tree lookup matched a FILE_EXTENT record for a Spotlight/CoreSpotlight target candidate object ID.";
                                    er.notes = "candidate=" + candidateLabel + "; requested_logical_offset=" + std::to_string(kTargetLogicalOffset) + "; actual_key_object_id=" + std::to_string(candidateObjectId) + "; branch_path=" + branchPath + "; " + detail;
                                    allSpotlightScanFileExtents.push_back(er);
                                    anyHit = true;
                                }
                                if (!anyHit) {
                                    std::string status = exactTypeSeen ? "GUIDED_FILE_EXTENT_LOOKUP_TYPE_SEEN_NO_USABLE_VALUE" : (exactObjectSeen ? "GUIDED_FILE_EXTENT_LOOKUP_OBJECT_SEEN_NO_FILE_EXTENT" : "GUIDED_FILE_EXTENT_LOOKUP_NO_MATCH_IN_LEAF");
                                    appendGuidedNoMatchExtentRow(cr, candidateObjectId, status, "candidate=" + candidateLabel + "; branch_path=" + branchPath);
                                }
                                return anyHit;
                            }

                            bool childFound = false;
                            std::uint64_t selectedChildOid = 0;
                            std::uint32_t selectedEntry = 0;
                            std::uint64_t firstChild = 0;
                            std::uint32_t firstEntry = 0;
                            bool haveFirstChild = false;
                            for (std::uint32_t i = 0; i < limit; ++i) {
                                std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                std::string detail;
                                if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                                if (keyLen < 8U || valLen < 8U || valAbs + 8U > node.size()) continue;
                                const std::uint64_t keyRaw = readLe64(node, keyAbs);
                                const std::uint64_t obj = apfsFsKeyObjectId(keyRaw);
                                const std::uint8_t typ = apfsFsKeyRecordType(keyRaw);
                                const std::uint64_t logical = (keyLen >= 16U && keyAbs + 16U <= node.size()) ? readLe64(node, keyAbs + 8U) : 0ULL;
                                const std::uint64_t childOid = readLe64(node, valAbs);
                                if (childOid == 0) continue;
                                if (!haveFirstChild) { firstChild = childOid; firstEntry = i; haveFirstChild = true; }
                                const int cmp = compareApfsFsKeyForGuidedLookup(obj, typ, logical, candidateObjectId, kTargetTypeFileExtent, kTargetLogicalOffset);
                                if (cmp <= 0) {
                                    // APFS filesystem B+ tree internal keys index the smallest key in each child.
                                    // Select the last child whose separator key is <= the target key.
                                    selectedChildOid = childOid;
                                    selectedEntry = i;
                                    childFound = true;
                                    continue;
                                }
                                if (!childFound) {
                                    // Target sorts before the first separator; descend to the first child.
                                    selectedChildOid = firstChild;
                                    selectedEntry = firstEntry;
                                    childFound = true;
                                }
                                break;
                            }
                            if (!childFound || selectedChildOid == 0) {
                                appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_BRANCH_CHILD_NOT_SELECTED", "candidate=" + candidateLabel + "; path=" + branchPath);
                                return anyHit;
                            }
                            ApfsOmapTargetResolution resolved = aff4ResolveVolumeOmapTargetObjectForProbe(omIt->second, rootIt->second, selectedChildOid, lookupPtr->targetXid, nxSummary.blockSize, readVirtual, safeSpotlightNodeOffset, "APFS guided Spotlight target FILE_EXTENT lookup", dynamicProbeCancelled);
                            if (resolved.objectStatus != "OMAP_TARGET_BTREE_READ") {
                                appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_CHILD_READ_FAILED", "candidate=" + candidateLabel + "; selected_entry=" + std::to_string(selectedEntry) + "; child_oid=" + std::to_string(selectedChildOid) + "; lookup_status=" + resolved.lookupStatus + "; object_status=" + resolved.objectStatus + "; path=" + branchPath);
                                return anyHit;
                            }
                            node.swap(resolved.resolvedBuffer);
                            nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : selectedChildOid;
                            nodeOffset = resolved.resolvedVirtualOffset;
                        }
                        appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_DEPTH_LIMIT", "candidate=" + candidateLabel + "; branch_path=" + branchPath);
                        return anyHit;
                    };

                    auto guidedObjectCandidatesForChild = [&](std::uint64_t childFileId, std::uint64_t privateId) -> std::vector<std::pair<std::uint64_t, std::string>> {
                        std::vector<std::pair<std::uint64_t, std::string>> out;
                        std::set<std::uint64_t> seen;
                        auto add = [&](std::uint64_t v, const std::string& label) {
                            if (v == 0) return;
                            if (seen.insert(v).second) out.push_back(std::make_pair(v, label));
                        };
                        add(childFileId, "child_file_id_raw");
                        if (privateId != 0) {
                            add(privateId, "inode_private_id_dstream_candidate");
                            add(privateId >> 4U, "inode_private_id_shifted_right_4");
                        }
                        const std::uint64_t shifted = childFileId >> 4U;
                        add(shifted, "child_file_id_shifted_right_4");
                        for (std::uint64_t prefix = 1; prefix <= 15; ++prefix) {
                            add((prefix << 56U) | shifted, "high_prefix_" + std::to_string(prefix) + "_plus_child_shifted_right_4");
                        }
                        return out;
                    };

                    for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                        if (cr.childFileId == 0) continue;
                        if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM") continue; // dissect.apfs XAttr.open() uses xattr_obj_id directly as a FileStream; do not reinterpret it through inode private/uncompressed-size metadata.
                        if (!(cr.extractionStatus == "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED" || cr.extractionStatus == "COPY_NOT_ATTEMPTED_EXTENTS_FOUND_FULL_ASSEMBLY_NOT_READY")) continue;
                        const auto inodeCandidates = guidedObjectCandidatesForChild(cr.childFileId, 0);
                        for (const auto& cand : inodeCandidates) {
                            if (lookupGuidedTargetInode(cr, cand.first, cand.second)) break;
                        }
                    }

                    for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                        if (cr.childFileId == 0) continue;
                        if (!(cr.extractionStatus == "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED" || cr.extractionStatus == "COPY_NOT_ATTEMPTED_EXTENTS_FOUND_FULL_ASSEMBLY_NOT_READY")) continue;
                        std::vector<std::pair<std::uint64_t, std::string>> candidates;
                        if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM") {
                            // Follow dissect.apfs: XAttr.open() returns FileStream(volume, xattr_obj_id, dstream.size).
                            // The FILE_EXTENT key object is the xattr data-stream object ID, not the original inode/private ID.
                            candidates.push_back(std::make_pair(cr.childFileId, "xattr_dstream_object_id"));
                        } else {
                            const auto privateIt = spotlightPrivateIdByTargetSequence.find(cr.sequence);
                            const std::uint64_t privateId = (privateIt == spotlightPrivateIdByTargetSequence.end()) ? 0ULL : privateIt->second;
                            candidates = guidedObjectCandidatesForChild(cr.childFileId, privateId);
                        }
                        for (const auto& cand : candidates) {
                            lookupGuidedFileExtentCandidate(cr, cand.first, cand.second, 0ULL);
                        }
                    }

                    auto materializeGuidedExtentForCopyAttempt = [&](const ApfsSpotlightCopyAttemptRow& cr, const ApfsSpotlightFileExtentProbeRow& erBase) -> bool {
                        if (erBase.volumeSequence != cr.volumeSequence || erBase.extentFileId != cr.childFileId || cr.childFileId == 0) return false;
                        ApfsSpotlightFileExtentProbeRow er = erBase;
                        er.sequence = static_cast<std::uint32_t>(apfsSpotlightFileExtentProbeRows.size());
                        er.targetSequence = cr.sequence;
                        er.targetParentObjectId = cr.parentObjectId;
                        er.targetChildFileId = cr.childFileId;
                        er.targetName = cr.targetName;
                        er.targetKind = cr.targetKind;
                        er.extentStatus = "TARGET_FILE_EXTENT_CANDIDATE";
                        er.interpretation = "APFS FILE_EXTENT record matched a Spotlight/CoreSpotlight target child file ID. Full copy-out remains gated on complete extent assembly and hash verification.";
                        if (er.extentLogicalOffset == 0 && er.physicalOffset != 0 && er.extentLengthBytes > 0) {
                            const std::uint64_t previewLen = std::min<std::uint64_t>(4096ULL, er.extentLengthBytes);
                            std::vector<unsigned char> preview;
                            std::string previewErr;
                            er.previewBytesRead = readVirtual(er.physicalOffset, previewLen, preview, previewErr);
                            if (er.previewBytesRead > 0) {
                                er.previewStatus = directPreviewStatusForBytes(preview);
                                er.previewSampleHex = hexSampleBytes(preview.data(), preview.size() < 96 ? preview.size() : 96);
                            } else {
                                er.previewStatus = "PREVIEW_READ_FAILED";
                                er.notes += previewErr.empty() ? "; preview_read_failed" : ("; " + previewErr);
                            }
                        } else {
                            er.previewStatus = "PREVIEW_NOT_LOGICAL_ZERO_EXTENT";
                        }
                        apfsSpotlightFileExtentProbeRows.push_back(er);
                        return true;
                    };

                    for (auto& cr : apfsSpotlightCopyAttemptRows) {
                        bool foundExtent = false;
                        for (const auto& erBase : allSpotlightScanFileExtents) {
                            if (materializeGuidedExtentForCopyAttempt(cr, erBase)) foundExtent = true;
                        }
                        if (foundExtent && cr.extractionStatus == "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED") {
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_EXTENTS_FOUND_FULL_ASSEMBLY_NOT_READY";
                            cr.interpretation = "One or more APFS FILE_EXTENT records were matched to this Spotlight-related file. Full file copy-out is still gated until all extents are assembled and the extracted file can be verified.";
                            cr.notes = "V0_8_68 resolves target file extents, bounded previews, recursive Store-V2 namespace copy attempts, duplicate-free extent assembly, inode DSTREAM logical-size metadata, and materialized follow-up FILE_EXTENT probes where available; complete production use still requires full path reconstruction and compression/xattr handling.";
                        }
                    }


                    auto safeExtractionFileName = [&](std::uint32_t seq, std::uint64_t fileId, const std::string& name) -> std::string {
                        std::string out = std::to_string(seq) + "_fid_" + std::to_string(fileId) + "_";
                        for (char ch : name) {
                            const unsigned char c = static_cast<unsigned char>(ch);
                            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || ch == '.' || ch == '_' || ch == '-') out.push_back(ch);
                            else out.push_back('_');
                            if (out.size() >= 180) break;
                        }
                        if (out.empty()) out = std::to_string(seq) + "_fid_" + std::to_string(fileId) + "_unnamed.bin";
                        return out;
                    };

                    auto appendCopyOutRow = [&](const ApfsSpotlightCopyAttemptRow& cr,
                                                const std::vector<ApfsSpotlightFileExtentProbeRow>& extentsForTarget,
                                                const std::string& status,
                                                const std::string& validation,
                                                const std::string& notes) {
                        ApfsSpotlightFileCopyOutRow row;
                        row.sequence = static_cast<std::uint32_t>(apfsSpotlightFileCopyOutRows.size());
                        row.targetSequence = cr.sequence;
                        row.volumeSequence = cr.volumeSequence;
                        row.targetRole = cr.targetRole;
                        row.fsOid = cr.fsOid;
                        row.volumeName = cr.volumeName;
                        row.targetParentObjectId = cr.parentObjectId;
                        row.targetChildFileId = cr.childFileId;
                        row.targetName = cr.targetName;
                        row.targetKind = cr.targetKind;
                        row.storeV2RootObjectId = cr.storeV2RootObjectId;
                        row.storeV2GroupName = cr.storeV2GroupName;
                        row.storeV2RelativePath = cr.storeV2RelativePath;
                        row.extentCount = static_cast<std::uint32_t>(extentsForTarget.size());
                        row.copyStatus = status;
                        row.validationStatus = validation;
                        row.interpretation = "Controlled AFF4/APFS Spotlight copy-out gate decision for a target with decoded FILE_EXTENT rows.";
                        row.notes = notes;
                        apfsSpotlightFileCopyOutRows.push_back(row);
                    };

                    const std::uint64_t kMaxSingleCopyOutBytes = opt.pressureTestMode ? (std::numeric_limits<std::uint64_t>::max)() : (512ULL * 1024ULL * 1024ULL);
                    std::size_t copiedOutFileCount = 0;
                    const fs::path extractedRoot = caseDir / "ExtractedSpotlight" / "aff4_apfs";

                    // V0_8_68: prefer dissect.apfs-style exact stream objects and avoid staging
                    // shifted/private-id false positives when the decoded inode parent does not
                    // match the directory record parent for a Store-V2 target. These maps also
                    // support extent candidate scoring below.
                    std::map<std::uint32_t, bool> inodeParentMatchesByTargetSequence;
                    std::map<std::uint32_t, bool> inodeParentMismatchByTargetSequence;
                    for (const auto& ir : apfsSpotlightInodeProbeRows) {
                        if (ir.inodeStatus != "TARGET_INODE_GUIDED_LOOKUP_HIT") continue;
                        if (ir.targetSequence == 0 || ir.targetChildFileId == 0 || ir.inodeObjectId != ir.targetChildFileId) continue;
                        if (ir.inodeParentId == ir.targetParentObjectId) inodeParentMatchesByTargetSequence[ir.targetSequence] = true;
                        else inodeParentMismatchByTargetSequence[ir.targetSequence] = true;
                    }

                    auto hasConfirmedParentMismatch = [&](std::uint32_t targetSequence) -> bool {
                        const bool matched = inodeParentMatchesByTargetSequence.find(targetSequence) != inodeParentMatchesByTargetSequence.end();
                        const bool mismatched = inodeParentMismatchByTargetSequence.find(targetSequence) != inodeParentMismatchByTargetSequence.end();
                        return mismatched && !matched;
                    };

                    auto extentCandidateScore = [&](const ApfsSpotlightCopyAttemptRow& cr, const ApfsSpotlightFileExtentProbeRow& er) -> long long {
                        long long score = 0;
                        if (er.extentStatus == "TARGET_FILE_EXTENT_CANDIDATE") score += 10000;
                        if (er.physicalBlock != 0 && er.physicalOffset != 0 && er.extentLengthBytes != 0) score += 5000; else score -= 20000;
                        if (er.extentFileId == cr.childFileId) score += 9000;
                        if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM" && er.notes.find("candidate=xattr_dstream_object_id") != std::string::npos) score += 12000;
                        if (er.notes.find("candidate=child_file_id_raw") != std::string::npos) score += 7000;
                        if (er.notes.find("candidate=inode_private_id_dstream_candidate") != std::string::npos) score += 2500;
                        if (er.notes.find("_shifted_right_4") != std::string::npos) score -= 7000;
                        if (er.notes.find("high_prefix_") != std::string::npos) score -= 9000;
                        score += static_cast<long long>(std::min<std::uint64_t>(er.extentLengthBytes, 64ULL * 1024ULL * 1024ULL) / 4096ULL);
                        return score;
                    };

                    auto normalizeExtentsForCopyAttempt = [&](const ApfsSpotlightCopyAttemptRow& cr, std::vector<ApfsSpotlightFileExtentProbeRow>& extents) {
                        std::map<std::uint64_t, ApfsSpotlightFileExtentProbeRow> bestByOffset;
                        for (const auto& er : extents) {
                            if (er.extentStatus != "TARGET_FILE_EXTENT_CANDIDATE") continue;
                            auto it = bestByOffset.find(er.extentLogicalOffset);
                            if (it == bestByOffset.end() || extentCandidateScore(cr, er) > extentCandidateScore(cr, it->second) ||
                                (extentCandidateScore(cr, er) == extentCandidateScore(cr, it->second) && er.extentLengthBytes > it->second.extentLengthBytes)) {
                                bestByOffset[er.extentLogicalOffset] = er;
                            }
                        }
                        extents.clear();
                        for (const auto& kv : bestByOffset) extents.push_back(kv.second);
                    };

                    for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                        if (cr.childFileId == 0) continue;
                        if (!(cr.extractionStatus == "COPY_NOT_ATTEMPTED_EXTENTS_FOUND_FULL_ASSEMBLY_NOT_READY" || cr.extractionStatus == "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED")) continue;
                        std::vector<ApfsSpotlightFileExtentProbeRow> extentsForTarget;
                        for (const auto& er : apfsSpotlightFileExtentProbeRows) {
                            if (er.targetSequence == cr.sequence && er.targetChildFileId == cr.childFileId && er.extentStatus == "TARGET_FILE_EXTENT_CANDIDATE") extentsForTarget.push_back(er);
                        }
                        if (extentsForTarget.empty()) continue;
                        normalizeExtentsForCopyAttempt(cr, extentsForTarget);
                        if (cr.targetKind != "APFS_RESOURCE_FORK_STREAM" && hasConfirmedParentMismatch(cr.sequence)) {
                            appendCopyOutRow(cr, extentsForTarget, "SKIPPED_INODE_PARENT_MISMATCH", "INODE_PARENT_MISMATCH", "Decoded inode parent_id did not match the directory-record parent for this Store-V2 target; skipped to avoid staging shifted/private-id false-positive APFS extents.");
                            continue;
                        }

                        auto rebuildExtentsForCurrentTarget = [&]() {
                            extentsForTarget.clear();
                            for (const auto& er : apfsSpotlightFileExtentProbeRows) {
                                if (er.targetSequence == cr.sequence && er.targetChildFileId == cr.childFileId && er.extentStatus == "TARGET_FILE_EXTENT_CANDIDATE") extentsForTarget.push_back(er);
                            }
                            normalizeExtentsForCopyAttempt(cr, extentsForTarget);
                        };

                        auto computeContiguousExtentBytes = [&]() -> std::uint64_t {
                            std::uint64_t expectedLocal = 0;
                            for (const auto& er : extentsForTarget) {
                                if (er.extentLogicalOffset != expectedLocal || er.extentLengthBytes == 0) break;
                                if (er.extentLengthBytes > (std::numeric_limits<std::uint64_t>::max)() - expectedLocal) break;
                                expectedLocal += er.extentLengthBytes;
                            }
                            return expectedLocal;
                        };

                        const auto desiredLogicalItForProbe = spotlightLogicalSizeByTargetSequence.find(cr.sequence);
                        const std::uint64_t desiredLogicalSizeForProbe = (desiredLogicalItForProbe == spotlightLogicalSizeByTargetSequence.end()) ? 0ULL : desiredLogicalItForProbe->second;
                        if (desiredLogicalSizeForProbe != 0 && desiredLogicalSizeForProbe <= kMaxSingleCopyOutBytes) {
                            std::uint64_t contiguousBefore = computeContiguousExtentBytes();
                            if (contiguousBefore != 0 && contiguousBefore < desiredLogicalSizeForProbe) {
                                std::vector<std::pair<std::uint64_t, std::string>> candidatesForProbe;
                                if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM") {
                                    candidatesForProbe.push_back(std::make_pair(cr.childFileId, "xattr_dstream_object_id"));
                                } else {
                                    const auto privateItForProbe = spotlightPrivateIdByTargetSequence.find(cr.sequence);
                                    const std::uint64_t privateIdForProbe = (privateItForProbe == spotlightPrivateIdByTargetSequence.end()) ? 0ULL : privateItForProbe->second;
                                    candidatesForProbe = guidedObjectCandidatesForChild(cr.childFileId, privateIdForProbe);
                                }
                                std::set<std::uint64_t> requestedOffsets;
                                constexpr std::uint32_t kMaxAdditionalExtentOffsetProbes = 8192U;
                                std::uint32_t additionalProbeCount = 0;
                                while (contiguousBefore < desiredLogicalSizeForProbe && additionalProbeCount < kMaxAdditionalExtentOffsetProbes) {
                                    const std::uint64_t requestOffset = contiguousBefore;
                                    if (!requestedOffsets.insert(requestOffset).second) break;
                                    const std::size_t beforeRows = apfsSpotlightFileExtentProbeRows.size();
                                    const std::size_t beforeScanRows = allSpotlightScanFileExtents.size();
                                    for (const auto& cand : candidatesForProbe) {
                                        lookupGuidedFileExtentCandidate(cr, cand.first, cand.second, requestOffset);
                                    }
                                    // V0_8_62: guided follow-up FILE_EXTENT probes append to the
                                    // scan-level extent vector. Materialize newly discovered extents
                                    // immediately into the copy-out candidate rows; otherwise the
                                    // copy-out loop still sees only the first extent and large Store-V2
                                    // files remain truncated to their first block/extent.
                                    for (std::size_t scanIdx = beforeScanRows; scanIdx < allSpotlightScanFileExtents.size(); ++scanIdx) {
                                        materializeGuidedExtentForCopyAttempt(cr, allSpotlightScanFileExtents[scanIdx]);
                                    }
                                    ++additionalProbeCount;
                                    rebuildExtentsForCurrentTarget();
                                    const std::uint64_t contiguousAfter = computeContiguousExtentBytes();
                                    if (contiguousAfter <= contiguousBefore || apfsSpotlightFileExtentProbeRows.size() == beforeRows) break;
                                    contiguousBefore = contiguousAfter;
                                }
                            }
                        }

                        const auto desiredLogicalItForCopyRange = spotlightLogicalSizeByTargetSequence.find(cr.sequence);
                        const std::uint64_t desiredLogicalSizeForCopyRange = (desiredLogicalItForCopyRange == spotlightLogicalSizeByTargetSequence.end()) ? 0ULL : desiredLogicalItForCopyRange->second;
                        if (desiredLogicalSizeForCopyRange != 0 && desiredLogicalSizeForCopyRange <= kMaxSingleCopyOutBytes) {
                            // Follow the dissect.apfs FileStream read model: only require the extents needed
                            // to cover the requested logical stream size. Guided lookup may discover speculative
                            // or duplicate later extents; those must not make a valid bounded read appear non-gapless.
                            std::vector<ApfsSpotlightFileExtentProbeRow> neededExtents;
                            std::uint64_t coverage = 0;
                            for (const auto& er : extentsForTarget) {
                                if (coverage >= desiredLogicalSizeForCopyRange) break;
                                if (er.extentLogicalOffset != coverage) break;
                                if (er.extentLengthBytes == 0) break;
                                neededExtents.push_back(er);
                                if (er.extentLengthBytes > (std::numeric_limits<std::uint64_t>::max)() - coverage) break;
                                coverage += er.extentLengthBytes;
                            }
                            if (!neededExtents.empty()) {
                                extentsForTarget.swap(neededExtents);
                            }
                        }

                        bool hasSparseGap = false;
                        bool hasZeroPhysicalExtent = false;
                        bool hasOverlapOrInvalidOrder = false;
                        bool saneLengths = true;
                        std::uint64_t expected = 0;
                        std::uint64_t syntheticZeroBytesPlanned = 0;
                        for (const auto& er : extentsForTarget) {
                            if (er.extentLengthBytes == 0 || er.extentLengthBytes > kMaxSingleCopyOutBytes) saneLengths = false;
                            if (er.extentLogicalOffset < expected) hasOverlapOrInvalidOrder = true;
                            if (er.extentLogicalOffset > expected) {
                                hasSparseGap = true;
                                syntheticZeroBytesPlanned += (er.extentLogicalOffset - expected);
                                expected = er.extentLogicalOffset;
                            }
                            if (er.physicalBlock == 0 || er.physicalOffset == 0) {
                                hasZeroPhysicalExtent = true;
                                syntheticZeroBytesPlanned += er.extentLengthBytes;
                            }
                            if (er.extentLengthBytes > (std::numeric_limits<std::uint64_t>::max)() - expected) saneLengths = false;
                            expected += er.extentLengthBytes;
                        }
                        if (hasOverlapOrInvalidOrder) { appendCopyOutRow(cr, extentsForTarget, "SKIPPED_OVERLAPPING_OR_OUT_OF_ORDER_EXTENTS", "OVERLAP_ORDER_GATE_FAILED", "Logical FILE_EXTENT rows overlap or move backwards; extraction is deferred to avoid producing shifted data."); continue; }
                        if (!saneLengths || expected == 0 || expected > kMaxSingleCopyOutBytes) { appendCopyOutRow(cr, extentsForTarget, "SKIPPED_SIZE_LIMIT_OR_INVALID_LENGTH", "SIZE_GATE_FAILED", "The extent chain length is zero, invalid, or exceeds the bounded per-file copy-out limit."); continue; }

                        std::uint64_t logicalSize = expected;
                        std::string logicalSizeSource = "extent_chain_allocated_size";
                        std::uint64_t decodedLogicalSize = 0;
                        std::string decodedLogicalSizeSource;
                        bool decodedLogicalExceedsExtentChain = false;
                        auto logicalIt = spotlightLogicalSizeByTargetSequence.find(cr.sequence);
                        if (logicalIt != spotlightLogicalSizeByTargetSequence.end() && logicalIt->second != 0) {
                            decodedLogicalSize = logicalIt->second;
                            auto srcIt = spotlightLogicalSizeSourceByTargetSequence.find(cr.sequence);
                            decodedLogicalSizeSource = (srcIt != spotlightLogicalSizeSourceByTargetSequence.end()) ? srcIt->second : "INO_EXT_TYPE_DSTREAM.size";
                            if (decodedLogicalSize <= expected) {
                                logicalSize = decodedLogicalSize;
                                logicalSizeSource = decodedLogicalSizeSource;
                            } else if (decodedLogicalSize <= kMaxSingleCopyOutBytes) {
                                // V0_8_62: Many APFS Store-V2 Cache/*.txt rows are compressed/resource-fork
                                // candidates. The inode carries j_inode_val.uncompressed_size, but the normal
                                // FILE_EXTENT chain only exposes a short data fork. Copy the validated data-fork
                                // bytes for diagnostics, but mark the row as partial instead of claiming that the
                                // logical file was fully copied. Full fidelity requires APFS xattr/resource-fork
                                // decompression support.
                                decodedLogicalExceedsExtentChain = true;
                                logicalSize = expected;
                                logicalSizeSource = decodedLogicalSizeSource + "_exceeds_extent_chain";
                            }
                        }
                        if (logicalSize == 0 || logicalSize > expected) { appendCopyOutRow(cr, extentsForTarget, "SKIPPED_LOGICAL_SIZE_INVALID", "LOGICAL_SIZE_GATE_FAILED", "Decoded logical file size was zero or larger than assembled extent bytes."); continue; }

                        fs::path volumeDir = extractedRoot / safeExtractionFileName(cr.volumeSequence, cr.fsOid, cr.volumeName);
                        std::error_code mkEc;
                        fs::create_directories(volumeDir, mkEc);
                        if (mkEc) { appendCopyOutRow(cr, extentsForTarget, "COPY_FAILED_CREATE_DIRECTORY", "OUTPUT_DIRECTORY_FAILED", mkEc.message()); continue; }
                        fs::path outPath = volumeDir / safeExtractionFileName(cr.sequence, cr.childFileId, cr.targetName);
                        EvidenceBinaryWriter outFile(outPath);
                        if (!outFile) { appendCopyOutRow(cr, extentsForTarget, "COPY_FAILED_OPEN_OUTPUT", "OUTPUT_OPEN_FAILED", outFile.error()); continue; }

                        bool copyOk = true;
                        std::string copyNotes;
                        std::vector<unsigned char> firstBytes;
                        std::uint64_t remainingToWrite = logicalSize;
                        std::uint64_t logicalCursor = 0;
                        std::uint64_t syntheticZeroBytesWritten = 0;
                        auto writeZeroRegion = [&](std::uint64_t count, const std::string& reason) -> bool {
                            static const std::vector<unsigned char> zeros(1024 * 1024, 0);
                            std::uint64_t left = count;
                            while (left != 0) {
                                const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(left, zeros.size()));
                                if (firstBytes.empty()) firstBytes.assign(zeros.begin(), zeros.begin() + std::min<std::size_t>(chunk, 96U));
                                if (!outFile.write(zeros.data(), chunk)) { copyNotes = "write failed while zero-filling " + reason + "; " + outFile.error(); return false; }
                                left -= chunk;
                            }
                            syntheticZeroBytesWritten += count;
                            return true;
                        };
                        for (const auto& er : extentsForTarget) {
                            if (remainingToWrite == 0) break;
                            if (er.extentLogicalOffset > logicalCursor) {
                                const std::uint64_t gap = std::min<std::uint64_t>(er.extentLogicalOffset - logicalCursor, remainingToWrite);
                                if (!writeZeroRegion(gap, "logical_sparse_gap")) { copyOk = false; break; }
                                logicalCursor += gap;
                                remainingToWrite -= gap;
                                if (remainingToWrite == 0) break;
                            }
                            const std::uint64_t bytesWanted = std::min<std::uint64_t>(er.extentLengthBytes, remainingToWrite);
                            if (er.physicalBlock == 0 || er.physicalOffset == 0) {
                                if (!writeZeroRegion(bytesWanted, "zero_physical_block")) { copyOk = false; break; }
                                logicalCursor += bytesWanted;
                                remainingToWrite -= bytesWanted;
                                continue;
                            }
                            std::vector<unsigned char> extentBytes;
                            std::string readErr;
                            const long long bytesReadExtent = readVirtual(er.physicalOffset, bytesWanted, extentBytes, readErr);
                            if (bytesReadExtent < 0 || static_cast<std::uint64_t>(bytesReadExtent) != bytesWanted || extentBytes.size() != bytesWanted) {
                                copyOk = false;
                                copyNotes = readErr.empty() ? "extent read failed or returned short data" : readErr;
                                break;
                            }
                            if (firstBytes.empty()) firstBytes.assign(extentBytes.begin(), extentBytes.begin() + std::min<std::size_t>(extentBytes.size(), 96U));
                            if (!outFile.write(extentBytes)) { copyOk = false; copyNotes = "write failed while copying extent data; " + outFile.error(); break; }
                            logicalCursor += bytesWanted;
                            remainingToWrite -= bytesWanted;
                        }
                        if (copyOk && remainingToWrite != 0) { copyOk = false; copyNotes = "logical file size was not fully written from available extents"; }
                        if (!outFile.close()) { copyOk = false; copyNotes = copyNotes.empty() ? outFile.error() : (copyNotes + "; " + outFile.error()); }
                        ApfsSpotlightFileCopyOutRow row;
                        row.sequence = static_cast<std::uint32_t>(apfsSpotlightFileCopyOutRows.size());
                        row.targetSequence = cr.sequence;
                        row.volumeSequence = cr.volumeSequence;
                        row.targetRole = cr.targetRole;
                        row.fsOid = cr.fsOid;
                        row.volumeName = cr.volumeName;
                        row.targetParentObjectId = cr.parentObjectId;
                        row.targetChildFileId = cr.childFileId;
                        row.targetName = cr.targetName;
                        row.targetKind = cr.targetKind;
                        row.storeV2RootObjectId = cr.storeV2RootObjectId;
                        row.storeV2GroupName = cr.storeV2GroupName;
                        row.storeV2RelativePath = cr.storeV2RelativePath;
                        row.extentCount = static_cast<std::uint32_t>(extentsForTarget.size());
                        row.assembledBytes = expected;
                        row.logicalSizeBytes = decodedLogicalExceedsExtentChain ? decodedLogicalSize : logicalSize;
                        row.logicalSizeSource = logicalSizeSource;
                        row.firstPhysicalOffset = extentsForTarget.empty() ? 0ULL : extentsForTarget.front().physicalOffset;
                        row.outputPath = pathString(outPath);
                        row.outputRelativePath = pathString(fs::relative(outPath, caseDir));
                        if (!firstBytes.empty()) {
                            row.firstBytesStatus = directPreviewStatusForBytes(firstBytes);
                            row.firstBytesHex = hexSampleBytes(firstBytes.data(), firstBytes.size());
                        } else {
                            row.firstBytesStatus = "NO_PREVIEW";
                        }
                        if (!copyOk) {
                            row.copyStatus = "COPY_FAILED_READ_OR_WRITE";
                            row.validationStatus = "FAILED_DURING_COPY";
                            row.notes = copyNotes;
                            std::error_code rmEc; fs::remove(outPath, rmEc);
                        } else {
                            std::error_code sizeEc;
                            const auto sz = fs::file_size(outPath, sizeEc);
                            row.outputSizeBytes = sizeEc ? 0ULL : static_cast<std::uint64_t>(sz);
                            if (opt.pressureTestMode) { row.outputSha256 = "SKIPPED_BY_PRESSURE_TEST_MODE"; } else { try { row.outputSha256 = sha256File(outPath); } catch (const std::exception& ex) { row.notes = std::string("sha256_failed: ") + ex.what(); } }
                            if (decodedLogicalExceedsExtentChain) {
                                row.copyStatus = "COPIED_PARTIAL_COMPRESSED_OR_RSRC_FORK_CANDIDATE";
                                row.validationStatus = "PARTIAL_LOGICAL_SIZE_EXCEEDS_EXTENT_CHAIN";
                                row.notes = "decoded_logical_size=" + std::to_string(decodedLogicalSize) + "; assembled_extent_bytes=" + std::to_string(expected) + "; source=" + decodedLogicalSizeSource + "; APFS compressed/resource-fork/xattr handling is required for full logical-file reconstruction";
                                row.interpretation = "APFS FILE_EXTENT rows copied a validated data-fork extent chain, but the decoded inode logical/uncompressed size is larger than the assembled normal extents. Treat this as a compressed/resource-fork candidate, not a complete logical file copy.";
                            } else {
                                if (hasSparseGap || hasZeroPhysicalExtent || syntheticZeroBytesWritten != 0) {
                                    row.copyStatus = "COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS";
                                    row.validationStatus = (row.outputSizeBytes == logicalSize) ? "SIZE_MATCH_WITH_ZERO_FILL_PROVENANCE" : "SIZE_MISMATCH_AFTER_ZERO_FILL_COPY";
                                    row.notes += (row.notes.empty() ? std::string{} : "; ") + std::string("synthetic_zero_bytes=") + std::to_string(syntheticZeroBytesWritten) + "; planned_zero_bytes=" + std::to_string(syntheticZeroBytesPlanned) + "; sparse_gap=" + (hasSparseGap ? "true" : "false") + "; zero_physical_extent=" + (hasZeroPhysicalExtent ? "true" : "false");
                                    row.interpretation = "APFS FILE_EXTENT rows were assembled with explicit synthetic zero regions for sparse gaps and/or zero physical extents. The zero-filled regions are recorded in notes and the output hash is calculated over the reconstructed logical byte stream.";
                                } else {
                                    row.copyStatus = "COPIED_GAPLESS_EXTENT_CHAIN";
                                    row.validationStatus = (row.outputSizeBytes == logicalSize && logicalSize < expected) ? "TRIMMED_TO_INODE_LOGICAL_SIZE" : ((row.outputSizeBytes == expected) ? "SIZE_MATCHES_EXTENT_CHAIN" : "SIZE_MISMATCH_AFTER_COPY");
                                    row.interpretation = "APFS FILE_EXTENT rows formed a gapless nonzero physical chain and were assembled into a staged copy-out file with inode/dstream-aware logical-size validation.";
                                }
                            }
                            ++copiedOutFileCount;
                        }
                        apfsSpotlightFileCopyOutRows.push_back(row);
                    }

                    // V0_8_68: reconstruct decmpfs zlib/plain files whose normal data fork was only a
                    // small partial copy but whose stream-backed com.apple.ResourceFork was copied successfully.
                    // The reconstructed logical file is added as a separate copy-out row with the original
                    // Store-V2 relative path so staging can prefer it over the partial data-fork diagnostic row.
                    {
                        struct DecmpfsInfo { int compressionType = 0; std::uint64_t uncompressedSize = 0; };
                        std::map<std::uint64_t, DecmpfsInfo> decmpfsInfoByFileId;
                        for (const auto& xr : apfsSpotlightXattrProbeRows) {
                            if (asciiLower(xr.xattrName) != "com.apple.decmpfs") continue;
                            const int ctype = decmpfsCompressionTypeFromPreviewHex(xr.xdataPreviewHex);
                            const std::uint64_t usize = decmpfsUncompressedSizeFromPreviewHex(xr.xdataPreviewHex);
                            if (ctype != 0 && usize != 0) decmpfsInfoByFileId[xr.fileObjectId] = DecmpfsInfo{ctype, usize};
                        }

                        std::map<std::uint64_t, ApfsSpotlightFileCopyOutRow> bestResourceForkRowByOriginalFileId;
                        for (const auto& r : apfsSpotlightFileCopyOutRows) {
                            if (r.targetKind != "APFS_RESOURCE_FORK_STREAM") continue;
                            if (r.copyStatus != "COPIED_GAPLESS_EXTENT_CHAIN") continue;
                            if (r.outputPath.empty() || r.outputSizeBytes == 0) continue;
                            auto& cur = bestResourceForkRowByOriginalFileId[r.targetParentObjectId];
                            if (cur.outputPath.empty() || r.outputSizeBytes > cur.outputSizeBytes) cur = r;
                        }

                        auto readFileBytesBounded = [](const fs::path& path, std::uint64_t maxBytes, std::vector<unsigned char>& out, std::string& err) -> bool {
                            std::error_code ec;
                            const auto sz = fs::file_size(path, ec);
                            if (ec) { err = ec.message(); return false; }
                            if (static_cast<std::uint64_t>(sz) > maxBytes) { err = "file exceeds bounded read cap"; return false; }
                            std::ifstream in(path, std::ios::binary);
                            if (!in) { err = "unable to open file"; return false; }
                            out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                            return true;
                        };

                        const std::size_t initialCopyRowCount = apfsSpotlightFileCopyOutRows.size();
                        std::size_t reconstructedCount = 0;
                        std::size_t reconstructionAttemptCount = 0;
                        constexpr std::size_t kMaxDecmpfsReconstructions = 30000U;
                        for (std::size_t i = 0; i < initialCopyRowCount && reconstructionAttemptCount < kMaxDecmpfsReconstructions; ++i) {
                            const auto base = apfsSpotlightFileCopyOutRows[i];
                            if (base.copyStatus != "COPIED_PARTIAL_COMPRESSED_OR_RSRC_FORK_CANDIDATE") continue;
                            if (base.storeV2RelativePath.empty() || base.targetChildFileId == 0) continue;
                            const auto infoIt = decmpfsInfoByFileId.find(base.targetChildFileId);
                            if (infoIt == decmpfsInfoByFileId.end()) continue;
                            const auto rfIt = bestResourceForkRowByOriginalFileId.find(base.targetChildFileId);
                            if (rfIt == bestResourceForkRowByOriginalFileId.end()) continue;
                            ++reconstructionAttemptCount;

                            ApfsSpotlightFileCopyOutRow outRow = base;
                            outRow.sequence = static_cast<std::uint32_t>(apfsSpotlightFileCopyOutRows.size());
                            outRow.assembledBytes = rfIt->second.outputSizeBytes;
                            outRow.extentCount = rfIt->second.extentCount;
                            outRow.firstPhysicalOffset = rfIt->second.firstPhysicalOffset;
                            outRow.logicalSizeBytes = infoIt->second.uncompressedSize;
                            outRow.logicalSizeSource = "com.apple.decmpfs." + decmpfsCompressionTypeLabel(infoIt->second.compressionType) + ".resource_fork";
                            outRow.outputPath.clear();
                            outRow.outputRelativePath.clear();
                            outRow.outputSizeBytes = 0;
                            outRow.outputSha256.clear();
                            outRow.firstBytesStatus = "NO_PREVIEW";
                            outRow.firstBytesHex.clear();
                            outRow.copyStatus = "SKIPPED_DECOMPFS_RESOURCE_FORK_RECONSTRUCTION_FAILED";
                            outRow.validationStatus = "DECOMPFS_RECONSTRUCTION_FAILED";
                            outRow.interpretation = "Attempted APFS decmpfs resource-fork reconstruction for a partial Store-V2 file using the dissect.apfs resource-fork table model.";

                            std::vector<unsigned char> resourceBytes;
                            std::string readErr;
                            if (!readFileBytesBounded(fs::path(rfIt->second.outputPath), kMaxSingleCopyOutBytes, resourceBytes, readErr)) {
                                outRow.notes = "resource_fork_read_failed=" + readErr + "; resource_row_sequence=" + std::to_string(rfIt->second.sequence);
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            const auto recon = reconstructDecmpfsResourceForkDissectStyle(resourceBytes, infoIt->second.uncompressedSize, infoIt->second.compressionType);
                            if (!recon.ok) {
                                outRow.notes = "resource_row_sequence=" + std::to_string(rfIt->second.sequence) + "; status=" + recon.status + "; " + recon.notes;
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }

                            fs::path volumeDir = extractedRoot / safeExtractionFileName(base.volumeSequence, base.fsOid, base.volumeName);
                            std::error_code mkEc;
                            fs::create_directories(volumeDir, mkEc);
                            if (mkEc) {
                                outRow.notes = "reconstruction output directory failed: " + mkEc.message();
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            fs::path outPath = volumeDir / safeExtractionFileName(outRow.sequence, base.targetChildFileId, base.targetName + ".__DECOMPFS_RECONSTRUCTED");
                            std::string writeErr;
                            if (!writeBinaryFilePortable(outPath, recon.data.data(), recon.data.size(), &writeErr)) {
                                outRow.notes = "reconstruction output write failed: " + writeErr;
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            std::error_code sizeEc;
                            const auto sz = fs::file_size(outPath, sizeEc);
                            outRow.outputPath = pathString(outPath);
                            outRow.outputRelativePath = pathString(fs::relative(outPath, caseDir));
                            outRow.outputSizeBytes = sizeEc ? 0ULL : static_cast<std::uint64_t>(sz);
                            if (opt.pressureTestMode) { outRow.outputSha256 = "SKIPPED_BY_PRESSURE_TEST_MODE"; } else { try { outRow.outputSha256 = sha256File(outPath); } catch (const std::exception& ex) { outRow.notes = std::string("sha256_failed: ") + ex.what(); } }
                            if (!recon.data.empty()) {
                                const std::size_t previewLen = std::min<std::size_t>(recon.data.size(), 96U);
                                std::vector<unsigned char> preview(recon.data.begin(), recon.data.begin() + static_cast<std::ptrdiff_t>(previewLen));
                                outRow.firstBytesStatus = previewStatusForBytes(preview);
                                outRow.firstBytesHex = hexSampleBytes(preview.data(), preview.size());
                            }
                            if (infoIt->second.compressionType == 4) outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_ZLIB";
                            else if (infoIt->second.compressionType == 8) outRow.copyStatus = appleLzfseCodecAvailable() ? "COPIED_DECOMPFS_RESOURCE_FORK_LZVN" : "COPIED_DECOMPFS_RESOURCE_FORK_LZVN_UNCOMPRESSED_MARKERS";
                            else if (infoIt->second.compressionType == 12) outRow.copyStatus = appleLzfseCodecAvailable() ? "COPIED_DECOMPFS_RESOURCE_FORK_LZFSE" : "COPIED_DECOMPFS_RESOURCE_FORK_LZFSE_UNCOMPRESSED_MARKERS";
                            else if (infoIt->second.compressionType == 14) outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_LZBITMAP_UNCOMPRESSED_MARKERS";
                            else outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_PLAIN";
                            const std::string algLabel = decmpfsCompressionTypeLabel(infoIt->second.compressionType);
                            outRow.validationStatus = (outRow.outputSizeBytes == infoIt->second.uncompressedSize) ? ("DECOMPFS_RESOURCE_FORK_" + algLabel + "_SIZE_MATCH") : ("DECOMPFS_RESOURCE_FORK_" + algLabel + "_SIZE_MISMATCH");
                            outRow.interpretation = "APFS decmpfs resource-fork data was reconstructed from the copied com.apple.ResourceFork stream and staged as the original logical Store-V2 file.";
                            outRow.notes = "resource_row_sequence=" + std::to_string(rfIt->second.sequence) + "; " + recon.notes;
                            apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                            ++reconstructedCount;
                        }
                        // V0_8_68: also reconstruct when the original data-fork row was not copied as
                        // a partial candidate. This follows dissect.apfs DecmpfsStream more closely: the
                        // logical file can be reconstructed directly from com.apple.decmpfs plus the
                        // stream-backed com.apple.ResourceFork FileStream.
                        std::map<std::uint64_t, const ApfsSpotlightCopyAttemptRow*> originalAttemptByFileId;
                        for (const auto& cr0 : apfsSpotlightCopyAttemptRows) {
                            if (cr0.targetKind == "APFS_RESOURCE_FORK_STREAM") continue;
                            if (cr0.childFileId == 0 || cr0.storeV2RelativePath.empty()) continue;
                            if (decmpfsInfoByFileId.find(cr0.childFileId) == decmpfsInfoByFileId.end()) continue;
                            auto it0 = originalAttemptByFileId.find(cr0.childFileId);
                            if (it0 == originalAttemptByFileId.end() || (inodeParentMatchesByTargetSequence.find(cr0.sequence) != inodeParentMatchesByTargetSequence.end())) {
                                originalAttemptByFileId[cr0.childFileId] = &cr0;
                            }
                        }
                        std::set<std::uint64_t> alreadyReconstructedFileIds;
                        for (const auto& r0 : apfsSpotlightFileCopyOutRows) {
                            if (r0.copyStatus.rfind("COPIED_DECOMPFS_RESOURCE_FORK", 0) == 0) alreadyReconstructedFileIds.insert(r0.targetChildFileId);
                        }
                        for (const auto& kvBase : originalAttemptByFileId) {
                            if (reconstructionAttemptCount >= kMaxDecmpfsReconstructions) break;
                            const std::uint64_t originalFileId = kvBase.first;
                            if (alreadyReconstructedFileIds.count(originalFileId) != 0) continue;
                            const auto infoIt = decmpfsInfoByFileId.find(originalFileId);
                            if (infoIt == decmpfsInfoByFileId.end()) continue;
                            if (!(infoIt->second.compressionType == 4 || infoIt->second.compressionType == 8 || infoIt->second.compressionType == 10 || infoIt->second.compressionType == 12 || infoIt->second.compressionType == 14)) continue;
                            const auto rfIt = bestResourceForkRowByOriginalFileId.find(originalFileId);
                            if (rfIt == bestResourceForkRowByOriginalFileId.end()) continue;
                            ++reconstructionAttemptCount;

                            const auto& crBase = *kvBase.second;
                            ApfsSpotlightFileCopyOutRow outRow;
                            outRow.sequence = static_cast<std::uint32_t>(apfsSpotlightFileCopyOutRows.size());
                            outRow.targetSequence = crBase.sequence;
                            outRow.volumeSequence = crBase.volumeSequence;
                            outRow.targetRole = crBase.targetRole;
                            outRow.fsOid = crBase.fsOid;
                            outRow.volumeName = crBase.volumeName;
                            outRow.targetParentObjectId = crBase.parentObjectId;
                            outRow.targetChildFileId = crBase.childFileId;
                            outRow.targetName = crBase.targetName;
                            outRow.targetKind = crBase.targetKind;
                            outRow.storeV2RootObjectId = crBase.storeV2RootObjectId;
                            outRow.storeV2GroupName = crBase.storeV2GroupName;
                            outRow.storeV2RelativePath = crBase.storeV2RelativePath;
                            outRow.assembledBytes = rfIt->second.outputSizeBytes;
                            outRow.extentCount = rfIt->second.extentCount;
                            outRow.firstPhysicalOffset = rfIt->second.firstPhysicalOffset;
                            outRow.logicalSizeBytes = infoIt->second.uncompressedSize;
                            outRow.logicalSizeSource = "com.apple.decmpfs." + decmpfsCompressionTypeLabel(infoIt->second.compressionType) + ".resource_fork.synthetic_base";
                            outRow.copyStatus = "SKIPPED_DECOMPFS_RESOURCE_FORK_RECONSTRUCTION_FAILED";
                            outRow.validationStatus = "DECOMPFS_RECONSTRUCTION_FAILED";
                            outRow.interpretation = "Attempted APFS decmpfs resource-fork reconstruction directly from com.apple.decmpfs metadata and com.apple.ResourceFork stream, without requiring a prior partial data-fork copy row.";

                            std::vector<unsigned char> resourceBytes;
                            std::string readErr;
                            if (!readFileBytesBounded(fs::path(rfIt->second.outputPath), kMaxSingleCopyOutBytes, resourceBytes, readErr)) {
                                outRow.notes = "synthetic_base=1; resource_fork_read_failed=" + readErr + "; resource_row_sequence=" + std::to_string(rfIt->second.sequence);
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            const auto recon = reconstructDecmpfsResourceForkDissectStyle(resourceBytes, infoIt->second.uncompressedSize, infoIt->second.compressionType);
                            if (!recon.ok) {
                                outRow.notes = "synthetic_base=1; resource_row_sequence=" + std::to_string(rfIt->second.sequence) + "; status=" + recon.status + "; " + recon.notes;
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            fs::path volumeDir = extractedRoot / safeExtractionFileName(crBase.volumeSequence, crBase.fsOid, crBase.volumeName);
                            std::error_code mkEc2;
                            fs::create_directories(volumeDir, mkEc2);
                            if (mkEc2) {
                                outRow.notes = "synthetic_base=1; reconstruction output directory failed: " + mkEc2.message();
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            fs::path outPath = volumeDir / safeExtractionFileName(outRow.sequence, crBase.childFileId, crBase.targetName + ".__DECOMPFS_RECONSTRUCTED");
                            std::string writeErr;
                            if (!writeBinaryFilePortable(outPath, recon.data.data(), recon.data.size(), &writeErr)) {
                                outRow.notes = "synthetic_base=1; reconstruction output write failed: " + writeErr;
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            std::error_code sizeEc2;
                            const auto sz2 = fs::file_size(outPath, sizeEc2);
                            outRow.outputPath = pathString(outPath);
                            outRow.outputRelativePath = pathString(fs::relative(outPath, caseDir));
                            outRow.outputSizeBytes = sizeEc2 ? 0ULL : static_cast<std::uint64_t>(sz2);
                            if (opt.pressureTestMode) { outRow.outputSha256 = "SKIPPED_BY_PRESSURE_TEST_MODE"; } else { try { outRow.outputSha256 = sha256File(outPath); } catch (const std::exception& ex) { outRow.notes = std::string("synthetic_base=1; sha256_failed: ") + ex.what(); } }
                            if (!recon.data.empty()) {
                                const std::size_t previewLen = std::min<std::size_t>(recon.data.size(), 96U);
                                std::vector<unsigned char> preview(recon.data.begin(), recon.data.begin() + static_cast<std::ptrdiff_t>(previewLen));
                                outRow.firstBytesStatus = previewStatusForBytes(preview);
                                outRow.firstBytesHex = hexSampleBytes(preview.data(), preview.size());
                            } else { outRow.firstBytesStatus = "NO_PREVIEW"; }
                            if (infoIt->second.compressionType == 4) outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_ZLIB";
                            else if (infoIt->second.compressionType == 8) outRow.copyStatus = appleLzfseCodecAvailable() ? "COPIED_DECOMPFS_RESOURCE_FORK_LZVN" : "COPIED_DECOMPFS_RESOURCE_FORK_LZVN_UNCOMPRESSED_MARKERS";
                            else if (infoIt->second.compressionType == 12) outRow.copyStatus = appleLzfseCodecAvailable() ? "COPIED_DECOMPFS_RESOURCE_FORK_LZFSE" : "COPIED_DECOMPFS_RESOURCE_FORK_LZFSE_UNCOMPRESSED_MARKERS";
                            else if (infoIt->second.compressionType == 14) outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_LZBITMAP_UNCOMPRESSED_MARKERS";
                            else outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_PLAIN";
                            const std::string algLabel = decmpfsCompressionTypeLabel(infoIt->second.compressionType);
                            outRow.validationStatus = (outRow.outputSizeBytes == infoIt->second.uncompressedSize) ? ("DECOMPFS_RESOURCE_FORK_" + algLabel + "_SIZE_MATCH") : ("DECOMPFS_RESOURCE_FORK_" + algLabel + "_SIZE_MISMATCH");
                            outRow.interpretation = "APFS decmpfs resource-fork data was reconstructed from a copied com.apple.ResourceFork stream and staged as the original logical Store-V2 file.";
                            outRow.notes = "synthetic_base=1; resource_row_sequence=" + std::to_string(rfIt->second.sequence) + "; " + recon.notes;
                            apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                            ++reconstructedCount;
                        }

                        if (reconstructionAttemptCount != 0) {
                            log.info("APFS decmpfs resource-fork reconstruction attempts: " + std::to_string(reconstructionAttemptCount) + "; reconstructed=" + std::to_string(reconstructedCount));
                        }
                    }

                    int closeRc = -9999;
                    try { closeRc = pClose(handle); }
                    catch (...) { addRow("AFF4_close", "EXCEPTION", originalInput, finalObjectSize, 0, -1, {}, "Exception while closing AFF4 handle."); }
                    if (closeRc != -9999) addRow("AFF4_close", closeRc == 0 ? "CLOSED" : "CLOSE_NONZERO", originalInput, finalObjectSize, 0, closeRc, {}, "AFF4_close return code recorded.");
                }
            }
            FreeLibrary(h);
        }
        if (aff4DllDirCookie) RemoveDllDirectory(aff4DllDirCookie);
    }
#else
    addRow("dynamic_load_probe", "NOT_SUPPORTED_ON_THIS_PLATFORM", {}, 0, 0, -1, {}, "The current executable was not built for Windows; V0_8_23 dynamic-load probe targets Windows libaff4.dll.");
    addApfsRow("virtual_apfs_probe", "NOT_SUPPORTED_ON_THIS_PLATFORM", 0, -1, {}, "NOT_RUN", "AFF4 virtual APFS probe requires Windows libaff4 dynamic loading in this build.", {}, "Run the Windows build for this probe.");
#endif

    const bool writeHeavyApfsDiagnostics = shouldWriteAff4ApfsStructuralDiagnostics(opt.verbose, opt.diagnosticFullNativeDb, opt.aff4ApfsDiagnosticOutputs);
    const bool strictAff4PolicyForOutputs = opt.strictSingleAff4 || isAff4SourcePath(originalInput);
    if (writeHeavyApfsDiagnostics) {
        log.info("AFF4/APFS diagnostic output mode enabled: writing structural probe CSV outputs.");
        writeOutputs();
        writeAff4ApfsContainerViewOutputs(caseDir, source, originalInput, nxSummary, apfsDescriptorRows, log);
        writeAff4ApfsVolumeSuperblockOutputs(caseDir, source, originalInput, nxSummary, apfsVolumeRows, log);
        writeAff4ApfsCheckpointMapOutputs(caseDir, source, originalInput, nxSummary, apfsCheckpointMapRows, apfsCheckpointMappedObjectRows, log);
        writeObjectResolutionOutputs();
        writeAff4ApfsResolvedVolumeOutputs(caseDir, source, originalInput, nxSummary, apfsResolvedVolumeSuperblockRows, apfsVolumeOmapProbeRows, apfsVolumeRootTreeLookupRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsVolumeRootTreeLookupOutputs(caseDir, source, originalInput, apfsVolumeRootTreeLookupRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsRootTreeNodeProbeOutputs(caseDir, source, originalInput, apfsRootTreeNodeProbeRows, apfsRootTreeRecordSampleRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsRootTreeTraversalProbeOutputs(caseDir, source, originalInput, apfsRootTreeChildNodeProbeRows, apfsRootTreeChildRecordSampleRows, "child", strictAff4PolicyForOutputs, log);
        writeAff4ApfsRootTreeTraversalProbeOutputs(caseDir, source, originalInput, apfsRootTreeDescendantNodeProbeRows, apfsRootTreeDescendantRecordSampleRows, "descendant", strictAff4PolicyForOutputs, log);
        writeAff4ApfsFilesystemNamespaceSeedOutputs(caseDir, source, originalInput, apfsRootTreeRecordSampleRows, apfsRootTreeChildRecordSampleRows, apfsRootTreeDescendantRecordSampleRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsSpotlightTargetScanOutputs(caseDir, source, originalInput, apfsSpotlightTargetScanRows, apfsSpotlightNameScanSampleRows, apfsSpotlightCopyAttemptRows, apfsSpotlightTargetScanMetrics, strictAff4PolicyForOutputs, log);
        writeAff4ApfsSpotlightInodeProbeOutputs(caseDir, source, originalInput, apfsSpotlightInodeProbeRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsSpotlightXattrProbeOutputs(caseDir, source, originalInput, apfsSpotlightXattrProbeRows, apfsSpotlightCopyAttemptRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsSpotlightFileExtentProbeOutputs(caseDir, source, originalInput, apfsSpotlightFileExtentProbeRows, strictAff4PolicyForOutputs, log);
    } else {
        log.info("Normal AFF4/APFS source-probe mode: structural diagnostic CSV outputs suppressed; writing copy-out/stage outputs only.");
        appendRunStatus(caseDir, aff4ApfsStructuralDiagnosticsSuppressedStatus(), aff4ApfsStructuralDiagnosticsSuppressedGuidance());
    }
    writeAff4ApfsSpotlightFileCopyOutOutputs(caseDir, source, originalInput, apfsSpotlightFileCopyOutRows, strictAff4PolicyForOutputs, log);
    writeAff4ApfsExtractedStoreV2StageOutputs(caseDir, source, originalInput, apfsSpotlightFileCopyOutRows, strictAff4PolicyForOutputs, log);
    log.info("AFF4 APFS Spotlight file copy-out output written: " + pathString(caseDir / "aff4_apfs_spotlight_file_copy_out.csv"));
    log.info("AFF4 APFS extracted Store-V2 stage output written: " + pathString(caseDir / "aff4_apfs_extracted_storev2_stage_summary.json"));
    if (writeHeavyApfsDiagnostics) {
        log.info("AFF4 CPP Lite dynamic-load probe written: " + pathString(csvPath));
        log.info("AFF4 virtual APFS probe written: " + pathString(apfsCsvPath));
        log.info("AFF4 APFS container view written: " + pathString(caseDir / "aff4_apfs_container_superblock.csv"));
        log.info("AFF4 APFS volume superblock probe written: " + pathString(caseDir / "aff4_apfs_volume_superblocks.csv"));
        log.info("AFF4 APFS checkpoint map probe written: " + pathString(caseDir / "aff4_apfs_checkpoint_map.csv"));
        log.info("AFF4 APFS object-resolution probe written: " + pathString(caseDir / "aff4_apfs_object_id_probe.csv"));
        log.info("AFF4 APFS OMAP probe written: " + pathString(caseDir / "aff4_apfs_omap_phys_probe.csv"));
        log.info("AFF4 APFS OMAP B-tree root probe written: " + pathString(caseDir / "aff4_apfs_omap_btree_root_probe.csv"));
        log.info("AFF4 APFS OMAP lookup probe written: " + pathString(caseDir / "aff4_apfs_omap_lookup_probe.csv"));
        log.info("AFF4 APFS OMAP B-tree TOC probe written: " + pathString(caseDir / "aff4_apfs_omap_btree_toc_probe.csv"));
        log.info("AFF4 APFS OMAP leaf key/value decode written: " + pathString(caseDir / "aff4_apfs_omap_leaf_kv_decode.csv"));
        log.info("AFF4 APFS OMAP leaf lookup results written: " + pathString(caseDir / "aff4_apfs_omap_leaf_lookup_results.csv"));
        log.info("AFF4 APFS resolved volume superblocks written: " + pathString(caseDir / "aff4_apfs_resolved_volume_superblocks.csv"));
        log.info("AFF4 APFS volume OMAP probe written: " + pathString(caseDir / "aff4_apfs_volume_omap_probe.csv"));
        log.info("AFF4 APFS volume root-tree lookup written: " + pathString(caseDir / "aff4_apfs_volume_root_tree_lookup.csv"));
        log.info("AFF4 APFS root-tree node probe written: " + pathString(caseDir / "aff4_apfs_root_tree_node_probe.csv"));
        log.info("AFF4 APFS Spotlight target scan written: " + pathString(caseDir / "aff4_apfs_spotlight_target_scan.csv"));
        log.info("AFF4 APFS Spotlight inode probe written: " + pathString(caseDir / "aff4_apfs_spotlight_inode_probe.csv"));
        log.info("AFF4 APFS Spotlight XATTR probe written: " + pathString(caseDir / "aff4_apfs_spotlight_xattr_probe.csv"));
        log.info("AFF4 APFS Spotlight file-extent probe written: " + pathString(caseDir / "aff4_apfs_spotlight_file_extent_probe.csv"));
    }
}

} // namespace vestigant::spotlight
