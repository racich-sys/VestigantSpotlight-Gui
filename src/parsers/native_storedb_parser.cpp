#include "parsers/native_storedb_parser.h"
#include "core/path_utils.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace vestigant::spotlight {
namespace {
constexpr std::uint32_t StoreSignatureV1 = 0x64737437u; // dst7
constexpr std::uint32_t StoreSignatureV2 = 0x64737438u; // dst8
constexpr std::uint32_t Block0SignatureV1 = 0x64626D31u; // 1mbd
constexpr std::uint32_t Block0SignatureV2 = 0x64626D32u; // 2mbd
constexpr std::uint32_t StoreBlockSignature = 0x64627032u; // 2pbd
constexpr std::size_t MaxMetadataItemBytes = 16ull * 1024ull * 1024ull;
constexpr std::size_t MaxDecodedFieldBytes = 4ull * 1024ull * 1024ull;
constexpr std::size_t MaxDecodedArrayValues = 262144ull;

enum class BlockType : std::uint32_t {
    Unknown0 = 0,
    StringsV1 = 0x03,
    Metadata = 0x09,
    Property = 0x11,
    Category = 0x21,
    Unknown41 = 0x41,
    Index = 0x81
};

struct HeaderInfo {
    StoreInfo store;
    int version = 0;
    std::uint32_t flags = 0;
    std::uint32_t headerSize = 0;
    std::uint32_t block0Size = 0;
    std::uint32_t blockSize = 0;
    std::uint32_t idx03 = 0;
    std::uint32_t idx11 = 0;
    std::uint32_t idx21 = 0;
    std::uint32_t idx41 = 0;
    std::uint32_t idx81_1 = 0;
    std::uint32_t idx81_2 = 0;
    bool isValid = false;
    std::string validationError;
};

struct Block0Entry {
    std::uint64_t lastIdInBlock = 0;
    std::uint32_t offsetIndex = 0;
    std::uint32_t destinationBlockSize = 0;
};

struct StoreBlockInfo {
    std::uint32_t signature = 0;
    std::uint32_t physicalSize = 0;
    std::uint32_t logicalSize = 0;
    std::uint32_t blockTypeRaw = 0;
    std::uint32_t unknown = 0;
    std::uint32_t nextBlockIndex = 0;
    std::uint32_t unknown1 = 0;
    std::uint32_t unknown2 = 0;
    BlockType baseType() const { return static_cast<BlockType>(blockTypeRaw & 0xFFu); }
    bool isLz4Compressed() const { return (blockTypeRaw & 0x1000u) == 0x1000u; }
    bool isLzfseCompressed() const { return (blockTypeRaw & 0x2000u) == 0x2000u; }
};

struct PropertyDef {
    std::string name;
    std::uint8_t propType = 0;
    std::uint8_t valueType = 0;
};

struct ParsedItem {
    std::int64_t inodeId = 0;
    std::int64_t itemId = 0;
    std::int64_t parentId = 0;
    std::uint64_t rawDateUpdated = 0;
    std::string lastUpdatedUtc;
    std::map<std::string, std::string, std::less<>> metadata;
};

std::uint32_t readU32(const std::vector<std::uint8_t>& b, std::size_t off) {
    if (off + 4 > b.size()) throw std::runtime_error("readU32 out of range");
    return static_cast<std::uint32_t>(b[off]) |
           (static_cast<std::uint32_t>(b[off + 1]) << 8) |
           (static_cast<std::uint32_t>(b[off + 2]) << 16) |
           (static_cast<std::uint32_t>(b[off + 3]) << 24);
}
std::uint64_t readU64(const std::vector<std::uint8_t>& b, std::size_t off) {
    if (off + 8 > b.size()) throw std::runtime_error("readU64 out of range");
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) { v <<= 8; v |= b[off + static_cast<std::size_t>(i)]; }
    return v;
}
std::int32_t readI32(const std::vector<std::uint8_t>& b, std::size_t off) {
    return static_cast<std::int32_t>(readU32(b, off));
}
std::uint16_t readU16(const std::vector<std::uint8_t>& b, std::size_t off) {
    if (off + 2 > b.size()) throw std::runtime_error("readU16 out of range");
    return static_cast<std::uint16_t>(b[off] | (static_cast<std::uint16_t>(b[off + 1]) << 8));
}

double readDoubleLe(const std::vector<std::uint8_t>& b, std::size_t off) {
    std::uint64_t u = readU64(b, off);
    double d{};
    static_assert(sizeof(d) == sizeof(u));
    std::memcpy(&d, &u, sizeof(d));
    return d;
}
float readFloatLe(const std::vector<std::uint8_t>& b, std::size_t off) {
    std::uint32_t u = readU32(b, off);
    float f{};
    static_assert(sizeof(f) == sizeof(u));
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

std::vector<std::uint8_t> readFilePrefix(const fs::path& p, std::size_t count) {
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("unable to open store: " + pathString(p));
    std::vector<std::uint8_t> b(count);
    in.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(b.size()));
    b.resize(static_cast<std::size_t>(in.gcount()));
    return b;
}
std::vector<std::uint8_t> readFileAt(std::ifstream& in, std::uint64_t off, std::size_t count) {
    in.seekg(static_cast<std::streamoff>(off), std::ios::beg);
    if (!in) throw std::runtime_error("seek failed");
    std::vector<std::uint8_t> b(count);
    in.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(b.size()));
    if (static_cast<std::size_t>(in.gcount()) != count) throw std::runtime_error("short read");
    return b;
}

std::string lowerAscii(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool looksDateField(const std::string& field) {
    const auto f = lowerAscii(field);

    // Keep raw_date_candidates focused on fields that can represent actual
    // date/time events.  The V1.6.9 semantic raw-probe aliases intentionally use
    // names such as __native_probe_url_candidate_01 and
    // __native_probe_basename_candidate_01.  The word "candidate" contains the
    // substring "date", so those aliases must be excluded before the broad
    // date-key test below; otherwise URL/path/basename/plist probe strings are
    // incorrectly inserted into raw_date_candidates with blank parsed_utc.
    if (f.rfind("__native_probe_", 0) == 0) return false;
    if (f.rfind("__native_core_probe_string_", 0) == 0) return false;
    if (f.find("_candidate_") != std::string::npos &&
        f.find("date_candidate") == std::string::npos &&
        f.find("time_candidate") == std::string::npos) return false;

    // Earlier diagnostics intentionally captured broad names such as
    // DurationSeconds, ExposureTimeSeconds, and per-component DateYear/DateMonth
    // fields; those are useful as raw key/value rows but should not inflate
    // timeline rows as if they were timestamps.
    static const char* excluded[] = {
        "duration", "exposuretime", "timestamp",
        "dateday", "datehour", "datemonth", "dateweek",
        "dateweekday", "dateweekdayordinal", "dateweekofmonth",
        "dateweekofyear", "dateyear"
    };
    for (const auto* k : excluded) if (f.find(k) != std::string::npos) return false;

    static const char* keys[] = {
        "date", "created", "creation", "modified", "modification",
        "updated", "lastused", "useddates", "downloaded",
        "sharedsentdate", "spotlightengagementdates"
    };
    for (const auto* k : keys) if (f.find(k) != std::string::npos) return true;
    return false;
}

bool isLikelyIsoDate(const std::string& v) {
    return v.size() >= 10 && std::isdigit(static_cast<unsigned char>(v[0])) && std::isdigit(static_cast<unsigned char>(v[1])) &&
           std::isdigit(static_cast<unsigned char>(v[2])) && std::isdigit(static_cast<unsigned char>(v[3])) && v[4] == '-';
}

std::string trimAscii(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::vector<std::string> splitSemicolonDateList(const std::string& value) {
    if (value.find(';') == std::string::npos) return {value};
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(';', start);
        auto part = trimAscii(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!part.empty()) parts.push_back(std::move(part));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (parts.empty()) return {value};
    for (const auto& part : parts) {
        if (!isLikelyIsoDate(part)) return {value};
    }
    return parts;
}

std::string escapeControlForSqlText(std::string s) {
    for (char& c : s) if (c == '\0') c = ' ';
    return s;
}

std::string cleanDecodedString(std::string s) {
    // Some Spotlight/CoreSpotlight string payloads end with the two-byte marker
    // 0x16 0x02. Null/spacer bytes may appear around the marker, so trim those
    // first and again after marker removal. This prevents the trailer from
    // leaking into investigator-facing names/paths when UTF-8 conversion has
    // already mapped nulls to spaces.
    auto trimTrailingPadding = [&]() {
        while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
    };
    trimTrailingPadding();
    while (s.size() >= 2 &&
           static_cast<unsigned char>(s[s.size() - 2]) == 0x16u &&
           static_cast<unsigned char>(s[s.size() - 1]) == 0x02u) {
        s.resize(s.size() - 2);
        trimTrailingPadding();
    }
    return s;
}

std::string bytesToHex(const std::vector<std::uint8_t>& b) {
    std::ostringstream os;
    os << std::hex << std::uppercase << std::setfill('0');
    for (auto c : b) os << std::setw(2) << static_cast<int>(c);
    return os.str();
}

std::string utf8FromBytes(const std::vector<std::uint8_t>& b) {
    return escapeControlForSqlText(std::string(reinterpret_cast<const char*>(b.data()), b.size()));
}

std::vector<std::string> splitNullUtf8(const std::vector<std::uint8_t>& b) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= b.size(); ++i) {
        if (i == b.size() || b[i] == 0) {
            if (i > start) {
                auto s = cleanDecodedString(utf8FromBytes(std::vector<std::uint8_t>(b.begin() + static_cast<std::ptrdiff_t>(start), b.begin() + static_cast<std::ptrdiff_t>(i))));
                while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
                if (!s.empty()) out.push_back(std::move(s));
            }
            start = i + 1;
        }
    }
    return out;
}

std::string join(const std::vector<std::string>& v, const std::string& sep = "; ") {
    std::ostringstream os;
    for (std::size_t i = 0; i < v.size(); ++i) { if (i) os << sep; os << v[i]; }
    return os.str();
}

template <typename T> std::string joinNums(const std::vector<T>& v) {
    std::ostringstream os;
    for (std::size_t i = 0; i < v.size(); ++i) { if (i) os << "; "; os << v[i]; }
    return os.str();
}

std::string formatUnixUtcFromSeconds(double seconds) {
    if (!std::isfinite(seconds)) return {};
    // Defensive forensic parsing: experimental structured decoding can misread
    // arbitrary bytes as date doubles. Do not pass impossible timestamps into
    // platform CRT time conversion, especially on Windows where gmtime_s can
    // invoke the invalid-parameter handler and terminate the process.
    constexpr double MinReasonableUnixSeconds = -2208988800.0; // 1900-01-01
    constexpr double MaxReasonableUnixSeconds = 4102444800.0;  // 2100-01-01
    if (seconds < MinReasonableUnixSeconds || seconds > MaxReasonableUnixSeconds) return {};
    const auto t = static_cast<std::time_t>(seconds);
    std::tm tm{};
#if defined(_WIN32)
    if (gmtime_s(&tm, &t) != 0) return {};
#else
    if (gmtime_r(&t, &tm) == nullptr) return {};
#endif
    char buf[32]{};
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm) == 0) return {};
    return buf;
}

std::string macAbsoluteDateToUtc(double macAbs) {
    if (!std::isfinite(macAbs)) return {};
    return formatUnixUtcFromSeconds(978307200.0 + macAbs);
}
std::string epochMicrosToUtc(std::uint64_t micros) {
    return formatUnixUtcFromSeconds(static_cast<double>(micros) / 1000000.0);
}

std::uint64_t readVarSizeNum(const std::vector<std::uint8_t>& data, std::size_t pos, std::size_t& bytesRead) {
    if (pos >= data.size()) throw std::runtime_error("varint beyond buffer");
    std::uint8_t first = data[pos];
    int extra = 0;
    bool useLowerNibble = true;
    if (first == 0) { bytesRead = 1; return 0; }
    if ((first & 0xF0u) == 0xF0u) {
        useLowerNibble = false;
        if ((first & 0x0Fu) == 0x0Fu) extra = 8;
        else if ((first & 0x0Eu) == 0x0Eu) extra = 7;
        else if ((first & 0x0Cu) == 0x0Cu) extra = 6;
        else if ((first & 0x08u) == 0x08u) extra = 5;
        else { extra = 4; useLowerNibble = true; first = static_cast<std::uint8_t>(first - 0xF0u); }
    } else if ((first & 0xE0u) == 0xE0u) { extra = 3; first = static_cast<std::uint8_t>(first - 0xE0u); }
    else if ((first & 0xC0u) == 0xC0u) { extra = 2; first = static_cast<std::uint8_t>(first - 0xC0u); }
    else if ((first & 0x80u) == 0x80u) { extra = 1; first = static_cast<std::uint8_t>(first - 0x80u); }
    if (extra > 0) {
        if (pos + static_cast<std::size_t>(extra) >= data.size()) throw std::runtime_error("short varint");
        std::uint64_t num = 0;
        for (int x = 1; x <= extra; ++x) num += static_cast<std::uint64_t>(data[pos + static_cast<std::size_t>(x)]) << ((extra - x) * 8);
        if (useLowerNibble) num += static_cast<std::uint64_t>(first) << (extra * 8);
        bytesRead = static_cast<std::size_t>(extra + 1);
        return num;
    }
    bytesRead = 1;
    return first;
}

std::uint32_t reverseBits(std::uint32_t code, int len) {
    std::uint32_t r = 0;
    for (int i = 0; i < len; ++i) { r = (r << 1) | (code & 1u); code >>= 1; }
    return r;
}

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
    std::uint32_t readBits(int n) {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i) {
            if (bytePos_ >= size_) throw std::runtime_error("deflate bitstream exhausted");
            v |= static_cast<std::uint32_t>((data_[bytePos_] >> bitPos_) & 1u) << i;
            if (++bitPos_ == 8) { bitPos_ = 0; ++bytePos_; }
        }
        return v;
    }
    void alignByte() { if (bitPos_) { bitPos_ = 0; ++bytePos_; } }
    std::uint8_t readByteAligned() {
        alignByte();
        if (bytePos_ >= size_) throw std::runtime_error("deflate byte exhausted");
        return data_[bytePos_++];
    }

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t bytePos_ = 0;
    int bitPos_ = 0;
};

class HuffmanTree {
public:
    void build(const std::vector<int>& lengths) {
        table_.clear();
        maxLen_ = 0;
        std::array<int, 16> blCount{};
        for (int l : lengths) { if (l < 0 || l > 15) throw std::runtime_error("invalid huffman length"); if (l) { blCount[static_cast<std::size_t>(l)]++; maxLen_ = std::max(maxLen_, l); } }
        std::array<int, 16> nextCode{};
        int code = 0;
        for (int bits = 1; bits <= 15; ++bits) { code = (code + blCount[static_cast<std::size_t>(bits - 1)]) << 1; nextCode[static_cast<std::size_t>(bits)] = code; }
        for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
            const int len = lengths[symbol];
            if (!len) continue;
            const std::uint32_t canonical = static_cast<std::uint32_t>(nextCode[static_cast<std::size_t>(len)]++);
            const std::uint32_t rev = reverseBits(canonical, len);
            table_[(static_cast<std::uint32_t>(len) << 16) | rev] = static_cast<int>(symbol);
        }
    }
    int decode(BitReader& br) const {
        std::uint32_t code = 0;
        for (int len = 1; len <= maxLen_; ++len) {
            code |= br.readBits(1) << (len - 1);
            auto it = table_.find((static_cast<std::uint32_t>(len) << 16) | code);
            if (it != table_.end()) return it->second;
        }
        throw std::runtime_error("invalid huffman code");
    }
private:
    std::unordered_map<std::uint32_t, int> table_;
    int maxLen_ = 0;
};

std::vector<std::uint8_t> decompressLz4RawBlock(const std::uint8_t* src, std::size_t srcSize, std::size_t expectedOutputSize) {
    constexpr std::size_t MaxLz4OutputBytes = 128ull * 1024ull * 1024ull;
    if (expectedOutputSize > MaxLz4OutputBytes) throw std::runtime_error("LZ4 output exceeded safety cap");
    std::vector<std::uint8_t> out;
    out.reserve(expectedOutputSize ? expectedOutputSize : std::min<std::size_t>(srcSize * 4ull, MaxLz4OutputBytes));
    std::size_t pos = 0;
    auto readLength = [&](std::size_t base) -> std::size_t {
        std::size_t len = base;
        if (base == 15) {
            for (;;) {
                if (pos >= srcSize) throw std::runtime_error("LZ4 length overrun");
                const std::uint8_t b = src[pos++];
                len += b;
                if (len > MaxLz4OutputBytes) throw std::runtime_error("LZ4 length exceeded safety cap");
                if (b != 255) break;
            }
        }
        return len;
    };
    while (pos < srcSize) {
        const std::uint8_t token = src[pos++];
        const std::size_t literalLen = readLength(static_cast<std::size_t>(token >> 4));
        if (pos + literalLen > srcSize) throw std::runtime_error("LZ4 literal overrun");
        if (out.size() + literalLen > MaxLz4OutputBytes) throw std::runtime_error("LZ4 literal output exceeded safety cap");
        out.insert(out.end(), src + pos, src + pos + literalLen);
        pos += literalLen;
        if (expectedOutputSize != 0 && out.size() == expectedOutputSize) break;
        if (pos >= srcSize) break;
        if (pos + 2 > srcSize) throw std::runtime_error("LZ4 match offset missing");
        const std::size_t matchOffset = static_cast<std::size_t>(src[pos]) | (static_cast<std::size_t>(src[pos + 1]) << 8);
        pos += 2;
        if (matchOffset == 0 || matchOffset > out.size()) throw std::runtime_error("LZ4 invalid match offset");
        std::size_t matchLen = readLength(static_cast<std::size_t>(token & 0x0f)) + 4;
        if (out.size() + matchLen > MaxLz4OutputBytes) throw std::runtime_error("LZ4 match output exceeded safety cap");
        while (matchLen-- > 0) {
            out.push_back(out[out.size() - matchOffset]);
        }
        if (expectedOutputSize != 0 && out.size() == expectedOutputSize) break;
        if (expectedOutputSize != 0 && out.size() > expectedOutputSize) throw std::runtime_error("LZ4 output exceeded expected size");
    }
    if (expectedOutputSize != 0 && out.size() != expectedOutputSize) throw std::runtime_error("LZ4 output size mismatch");
    return out;
}

std::vector<std::uint8_t> decompressBv41OrBv4Block(const std::vector<std::uint8_t>& raw, const StoreBlockInfo& block) {
    if (block.logicalSize <= 20 || block.logicalSize > raw.size()) return {};
    if (block.logicalSize < 28) throw std::runtime_error("LZ4 bv4 block too small");
    const std::string marker(reinterpret_cast<const char*>(raw.data() + 20), 4);
    if (marker == "bv41") {
        if (block.logicalSize < 32) throw std::runtime_error("LZ4 bv41 block too small");
        const std::uint32_t uncompressedSize = readU32(raw, 24);
        const std::uint32_t compressedSize = readU32(raw, 28);
        if (compressedSize == 0) throw std::runtime_error("LZ4 bv41 compressed size is zero");
        if (32ull + static_cast<std::uint64_t>(compressedSize) > static_cast<std::uint64_t>(block.logicalSize)) throw std::runtime_error("LZ4 bv41 compressed size exceeds block logical size");
        return decompressLz4RawBlock(raw.data() + 32, static_cast<std::size_t>(compressedSize), static_cast<std::size_t>(uncompressedSize));
    }
    if (marker == "bv4-") {
        const std::uint32_t uncompressedSize = readU32(raw, 24);
        if (28ull + static_cast<std::uint64_t>(uncompressedSize) > static_cast<std::uint64_t>(block.logicalSize)) throw std::runtime_error("LZ4 bv4- uncompressed size exceeds block logical size");
        return std::vector<std::uint8_t>(raw.begin() + 28, raw.begin() + static_cast<std::ptrdiff_t>(28ull + uncompressedSize));
    }
    throw std::runtime_error("LZ4 block marker not recognized");
}

std::vector<std::uint8_t> inflateZlib(const std::vector<std::uint8_t>& z) {
    constexpr std::size_t MaxInflateOutputBytes = 128ull * 1024ull * 1024ull;
    if (z.size() < 6) throw std::runtime_error("zlib stream too small");
    const std::uint8_t cmf = z[0], flg = z[1];
    if ((cmf & 0x0Fu) != 8u) throw std::runtime_error("zlib stream is not deflate");
    if ((((static_cast<int>(cmf) << 8) + flg) % 31) != 0) throw std::runtime_error("zlib header check failed");
    if (flg & 0x20u) throw std::runtime_error("zlib preset dictionary not supported");
    BitReader br(z.data() + 2, z.size() - 6); // ignore trailing Adler32 for decoding
    std::vector<std::uint8_t> out;
    auto pushOut = [&](std::uint8_t v) {
        if (out.size() >= MaxInflateOutputBytes) throw std::runtime_error("deflate output exceeded safety cap");
        out.push_back(v);
    };
    bool final = false;
    static const int lengthBase[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const int lengthExtra[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const int distBase[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const int distExtra[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

    auto decodeCompressed = [&](const HuffmanTree& litLen, const HuffmanTree& dist) {
        for (;;) {
            int sym = litLen.decode(br);
            if (sym < 256) { pushOut(static_cast<std::uint8_t>(sym)); continue; }
            if (sym == 256) break;
            if (sym < 257 || sym > 285) throw std::runtime_error("invalid length symbol");
            int li = sym - 257;
            int length = lengthBase[li] + static_cast<int>(br.readBits(lengthExtra[li]));
            int dsym = dist.decode(br);
            if (dsym < 0 || dsym > 29) throw std::runtime_error("invalid distance symbol");
            int distance = distBase[dsym] + static_cast<int>(br.readBits(distExtra[dsym]));
            if (distance <= 0 || static_cast<std::size_t>(distance) > out.size()) throw std::runtime_error("invalid back-reference distance");
            const std::size_t start = out.size() - static_cast<std::size_t>(distance);
            for (int i = 0; i < length; ++i) pushOut(out[start + (static_cast<std::size_t>(i) % static_cast<std::size_t>(distance))]);
        }
    };

    while (!final) {
        final = br.readBits(1) != 0;
        const int btype = static_cast<int>(br.readBits(2));
        if (btype == 0) {
            br.alignByte();
            int len = br.readByteAligned() | (br.readByteAligned() << 8);
            int nlen = br.readByteAligned() | (br.readByteAligned() << 8);
            if (((len ^ 0xFFFF) & 0xFFFF) != nlen) throw std::runtime_error("stored block length check failed");
            for (int i = 0; i < len; ++i) pushOut(br.readByteAligned());
        } else if (btype == 1) {
            std::vector<int> ll(288, 0), dd(32, 5);
            for (int i = 0; i <= 143; ++i) ll[static_cast<std::size_t>(i)] = 8;
            for (int i = 144; i <= 255; ++i) ll[static_cast<std::size_t>(i)] = 9;
            for (int i = 256; i <= 279; ++i) ll[static_cast<std::size_t>(i)] = 7;
            for (int i = 280; i <= 287; ++i) ll[static_cast<std::size_t>(i)] = 8;
            HuffmanTree litLen, dist; litLen.build(ll); dist.build(dd);
            decodeCompressed(litLen, dist);
        } else if (btype == 2) {
            const int hlit = static_cast<int>(br.readBits(5)) + 257;
            const int hdist = static_cast<int>(br.readBits(5)) + 1;
            const int hclen = static_cast<int>(br.readBits(4)) + 4;
            static const int order[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            std::vector<int> cl(19, 0);
            for (int i = 0; i < hclen; ++i) cl[static_cast<std::size_t>(order[i])] = static_cast<int>(br.readBits(3));
            HuffmanTree clTree; clTree.build(cl);
            std::vector<int> lengths;
            lengths.reserve(static_cast<std::size_t>(hlit + hdist));
            while (static_cast<int>(lengths.size()) < hlit + hdist) {
                int sym = clTree.decode(br);
                if (sym <= 15) lengths.push_back(sym);
                else if (sym == 16) {
                    if (lengths.empty()) throw std::runtime_error("repeat with no previous length");
                    int repeat = static_cast<int>(br.readBits(2)) + 3;
                    int prev = lengths.back();
                    for (int i = 0; i < repeat; ++i) lengths.push_back(prev);
                } else if (sym == 17) {
                    int repeat = static_cast<int>(br.readBits(3)) + 3;
                    for (int i = 0; i < repeat; ++i) lengths.push_back(0);
                } else if (sym == 18) {
                    int repeat = static_cast<int>(br.readBits(7)) + 11;
                    for (int i = 0; i < repeat; ++i) lengths.push_back(0);
                } else throw std::runtime_error("invalid code length symbol");
            }
            std::vector<int> ll(lengths.begin(), lengths.begin() + hlit);
            std::vector<int> dd(lengths.begin() + hlit, lengths.begin() + hlit + hdist);
            if (dd.size() == 1 && dd[0] == 0) dd[0] = 1;
            HuffmanTree litLen, dist; litLen.build(ll); dist.build(dd);
            decodeCompressed(litLen, dist);
        } else {
            throw std::runtime_error("reserved deflate block type");
        }
    }
    return out;
}

HeaderInfo readHeader(const StoreInfo& store) {
    HeaderInfo h;
    h.store = store;
    auto b = readFilePrefix(store.storePath, 0x1000);
    if (b.size() < 0x1000) { h.validationError = "file_smaller_than_0x1000"; return h; }
    const auto sig = readU32(b, 0);
    if (sig != StoreSignatureV1 && sig != StoreSignatureV2) { h.validationError = "unexpected_store_signature"; return h; }
    h.version = sig == StoreSignatureV2 ? 2 : 1;
    h.flags = readU32(b, 4);
    if (h.version == 2) {
        h.headerSize = readU32(b, 36);
        h.block0Size = readU32(b, 40);
        h.blockSize = readU32(b, 44);
        h.idx11 = readU32(b, 48);
        h.idx21 = readU32(b, 52);
        h.idx41 = readU32(b, 56);
        h.idx81_1 = readU32(b, 60);
        h.idx81_2 = readU32(b, 64);
    } else {
        h.headerSize = readU32(b, 40);
        h.block0Size = readU32(b, 44);
        h.blockSize = readU32(b, 48);
        h.idx03 = readU32(b, 52);
    }
    h.isValid = h.headerSize > 0 && h.block0Size > 0 && h.blockSize > 0;
    if (!h.isValid) h.validationError = "invalid_header_sizes";
    return h;
}

StoreBlockInfo readStoreBlock(std::ifstream& in, std::uint64_t offset, std::uint32_t blockSize) {
    auto b = readFileAt(in, offset, blockSize);
    StoreBlockInfo s;
    s.signature = readU32(b, 0);
    if (s.signature != StoreBlockSignature) throw std::runtime_error("unexpected store block signature");
    s.physicalSize = readU32(b, 4);
    s.logicalSize = readU32(b, 8);
    s.blockTypeRaw = readU32(b, 12);
    s.unknown = readU32(b, 16);
    s.nextBlockIndex = readU32(b, 20);
    s.unknown1 = readU32(b, 24);
    s.unknown2 = readU32(b, 28);
    return s;
}

std::vector<Block0Entry> readBlock0(std::ifstream& in, const HeaderInfo& h) {
    auto b = readFileAt(in, h.headerSize, h.block0Size);
    const auto sig = readU32(b, 0);
    if (sig != Block0SignatureV1 && sig != Block0SignatureV2) throw std::runtime_error("unexpected block0 signature");
    const auto itemCount = readU32(b, 8);
    std::vector<Block0Entry> out;
    std::size_t pos = 20;
    for (std::uint32_t i = 0; i < itemCount && pos + 16 <= b.size(); ++i) {
        Block0Entry e;
        e.lastIdInBlock = readU64(b, pos);
        e.offsetIndex = readU32(b, pos + 8);
        e.destinationBlockSize = readU32(b, pos + 12);
        out.push_back(e);
        pos += 16;
    }
    return out;
}

void parsePropertiesV2(const std::vector<std::uint8_t>& raw, std::map<int, PropertyDef>& dict, int logicalSize) {
    std::size_t pos = 32;
    const std::size_t max = std::min<std::size_t>(raw.size(), static_cast<std::size_t>(std::max(logicalSize, 0)));
    while (pos + 6 <= max) {
        const int index = static_cast<int>(readU32(raw, pos));
        const auto valueType = raw[pos + 4];
        const auto propType = raw[pos + 5];
        pos += 6;
        auto it = std::find(raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(max), 0);
        if (it == raw.begin() + static_cast<std::ptrdiff_t>(max)) break;
        std::string name(reinterpret_cast<const char*>(&raw[pos]), static_cast<std::size_t>(std::distance(raw.begin() + static_cast<std::ptrdiff_t>(pos), it)));
        pos = static_cast<std::size_t>(std::distance(raw.begin(), it)) + 1;
        dict[index] = PropertyDef{name, propType, valueType};
    }
}

void parsePropertiesV1(const std::vector<std::uint8_t>& raw, std::map<int, PropertyDef>& dict, int logicalSize) {
    std::size_t pos = 28;
    const std::size_t max = std::min<std::size_t>(raw.size(), static_cast<std::size_t>(std::max(logicalSize, 0)));
    while (pos + 4 <= max) {
        const int index = static_cast<int>(readU16(raw, pos));
        const auto propType = raw[pos + 2];
        pos += 4;
        auto it = std::find(raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(max), 0);
        if (it == raw.begin() + static_cast<std::ptrdiff_t>(max)) break;
        std::string name(reinterpret_cast<const char*>(&raw[pos]), static_cast<std::size_t>(std::distance(raw.begin() + static_cast<std::ptrdiff_t>(pos), it)));
        pos = static_cast<std::size_t>(std::distance(raw.begin(), it)) + 1;
        dict[index] = PropertyDef{name, propType, 0};
    }
}

void parseCategories(const std::vector<std::uint8_t>& raw, std::map<int, std::string>& dict, int logicalSize) {
    std::size_t pos = 32;
    const std::size_t max = std::min<std::size_t>(raw.size(), static_cast<std::size_t>(std::max(logicalSize, 0)));
    while (pos + 4 <= max) {
        const int index = static_cast<int>(readU32(raw, pos));
        pos += 4;
        auto it = std::find(raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(max), 0);
        if (it == raw.begin() + static_cast<std::ptrdiff_t>(max)) break;
        std::string name(reinterpret_cast<const char*>(&raw[pos]), static_cast<std::size_t>(std::distance(raw.begin() + static_cast<std::ptrdiff_t>(pos), it)));
        pos = static_cast<std::size_t>(std::distance(raw.begin(), it)) + 1;
        dict[index] = name;
    }
}

void parseIndexes(const std::vector<std::uint8_t>& raw, std::map<int, std::vector<int>>& dict, int logicalSize) {
    std::size_t pos = 32;
    const std::size_t max = std::min<std::size_t>(raw.size(), static_cast<std::size_t>(std::max(logicalSize, 0)));
    while (pos + 4 <= max) {
        const int index = static_cast<int>(readU32(raw, pos)); pos += 4;
        std::size_t moved = 0;
        const int indexSize = static_cast<int>(readVarSizeNum(raw, pos, moved)); pos += moved;
        pos += static_cast<std::size_t>(indexSize % 4);
        const int byteCount = 4 * (indexSize / 4);
        if (byteCount < 0 || pos + static_cast<std::size_t>(byteCount) > max) break;
        std::vector<int> values;
        for (int i = 0; i < byteCount / 4; ++i) values.push_back(static_cast<int>(readI32(raw, pos + static_cast<std::size_t>(i * 4))));
        pos += static_cast<std::size_t>(byteCount);
        dict[index] = std::move(values);
    }
}


struct DbStrMapLoadResult {
    int mapId = 0;
    fs::path dataPath;
    fs::path offsetsPath;
    fs::path headerPath;
    bool dataExists = false;
    bool offsetsExists = false;
    bool headerExists = false;
    std::uintmax_t dataBytes = 0;
    std::uintmax_t offsetsBytes = 0;
    std::uintmax_t headerBytes = 0;
    std::size_t offsetEntries = 0;
    std::size_t parsedEntries = 0;
    std::size_t skippedEntries = 0;
    std::string status = "NOT_ATTEMPTED";
    std::string message;
};

std::vector<std::uint8_t> readWholeFileBounded(const fs::path& p, std::uintmax_t maxBytes = 512ull * 1024ull * 1024ull) {
    std::error_code ec;
    const auto sz = fs::file_size(p, ec);
    if (ec) throw std::runtime_error("unable to stat file: " + pathString(p) + ": " + ec.message());
    if (sz > maxBytes) throw std::runtime_error("file exceeds dbStr safety cap: " + pathString(p));
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("unable to open file: " + pathString(p));
    std::vector<std::uint8_t> b(static_cast<std::size_t>(sz));
    if (!b.empty()) in.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(b.size()));
    if (static_cast<std::size_t>(in.gcount()) != b.size()) throw std::runtime_error("short read: " + pathString(p));
    return b;
}

bool fileExistsRegular(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}

std::uintmax_t fileSizeNoThrow(const fs::path& p) {
    std::error_code ec;
    auto n = fs::file_size(p, ec);
    return ec ? 0 : n;
}

std::uint64_t readIndexVarSizeNum(const std::vector<std::uint8_t>& data, std::size_t pos, std::size_t& bytesRead) {
    if (pos >= data.size()) throw std::runtime_error("dbStr index varint beyond buffer");
    std::uint8_t byte = data[pos];
    bytesRead = 1;
    std::uint64_t ret = static_cast<std::uint64_t>(byte & 0x7Fu);
    while ((byte & 0x80u) == 0x80u) {
        if (pos + bytesRead >= data.size()) throw std::runtime_error("dbStr index varint truncated");
        byte = data[pos + bytesRead];
        ret |= static_cast<std::uint64_t>(byte & 0x7Fu) << (7u * static_cast<unsigned>(bytesRead));
        ++bytesRead;
        if (bytesRead > 9) throw std::runtime_error("dbStr index varint exceeds safety length");
    }
    return ret;
}

std::vector<std::pair<int, std::uint32_t>> parseDbStrOffsets(const std::vector<std::uint8_t>& offsetsContent, DbStrMapLoadResult& result) {
    std::vector<std::pair<int, std::uint32_t>> offsets;
    std::size_t pos = 4;
    int index = 1;
    while (pos + 4 <= offsetsContent.size()) {
        const auto off = readU32(offsetsContent, pos);
        if (off == 0) break;
        if (off != 1) offsets.emplace_back(index, off); // 1 marks an invalid/deleted entry in observed dbStr maps.
        ++index;
        pos += 4;
    }
    result.offsetEntries = offsets.size();
    return offsets;
}

std::string dbStrUtf8(const std::vector<std::uint8_t>& data, std::size_t begin, std::size_t end) {
    if (begin > data.size()) begin = data.size();
    if (end > data.size()) end = data.size();
    if (end < begin) end = begin;
    auto nul = std::find(data.begin() + static_cast<std::ptrdiff_t>(begin), data.begin() + static_cast<std::ptrdiff_t>(end), 0);
    end = static_cast<std::size_t>(std::distance(data.begin(), nul));
    std::string s(reinterpret_cast<const char*>(data.data() + begin), end - begin);
    return cleanDecodedString(escapeControlForSqlText(std::move(s)));
}

DbStrMapLoadResult makeDbStrResult(const fs::path& folder, int mapId) {
    DbStrMapLoadResult r;
    r.mapId = mapId;
    r.dataPath = folder / ("dbStr-" + std::to_string(mapId) + ".map.data");
    r.offsetsPath = folder / ("dbStr-" + std::to_string(mapId) + ".map.offsets");
    r.headerPath = folder / ("dbStr-" + std::to_string(mapId) + ".map.header");
    r.dataExists = fileExistsRegular(r.dataPath);
    r.offsetsExists = fileExistsRegular(r.offsetsPath);
    r.headerExists = fileExistsRegular(r.headerPath);
    r.dataBytes = r.dataExists ? fileSizeNoThrow(r.dataPath) : 0;
    r.offsetsBytes = r.offsetsExists ? fileSizeNoThrow(r.offsetsPath) : 0;
    r.headerBytes = r.headerExists ? fileSizeNoThrow(r.headerPath) : 0;
    if (!r.dataExists || !r.offsetsExists || !r.headerExists) {
        r.status = "MISSING_COMPONENT";
        r.message = "required dbStr map component missing";
    }
    return r;
}

DbStrMapLoadResult parseDbStrProperties(const fs::path& folder, int mapId, std::map<int, PropertyDef>& properties) {
    auto result = makeDbStrResult(folder, mapId);
    if (result.status == "MISSING_COMPONENT") return result;
    try {
        const auto data = readWholeFileBounded(result.dataPath);
        const auto offsetsData = readWholeFileBounded(result.offsetsPath);
        (void)readWholeFileBounded(result.headerPath, 1024ull * 1024ull);
        const auto offsets = parseDbStrOffsets(offsetsData, result);
        for (const auto& [index, off] : offsets) {
            try {
                if (off >= data.size()) { ++result.skippedEntries; continue; }
                std::size_t moved = 0;
                const auto entrySize = readVarSizeNum(data, off, moved);
                if (entrySize == 0 || entrySize > MaxDecodedFieldBytes) { ++result.skippedEntries; continue; }
                const auto entryEnd64 = static_cast<std::uint64_t>(off) + entrySize;
                if (entryEnd64 > data.size() || moved + 2 > entrySize) { ++result.skippedEntries; continue; }
                const std::size_t valueTypePos = static_cast<std::size_t>(off) + moved;
                const auto valueType = data[valueTypePos];
                const auto propType = data[valueTypePos + 1];
                const auto nameBegin = valueTypePos + 2;
                const auto nameEnd = static_cast<std::size_t>(entryEnd64);
                auto name = dbStrUtf8(data, nameBegin, nameEnd);
                if (name.empty()) { ++result.skippedEntries; continue; }
                properties[index] = PropertyDef{name, propType, valueType};
                ++result.parsedEntries;
            } catch (...) { ++result.skippedEntries; }
        }
        result.status = "PARSED";
        result.message = "dbStr property map parsed";
    } catch (const std::exception& ex) {
        result.status = "FAILED";
        result.message = ex.what();
    }
    return result;
}

DbStrMapLoadResult parseDbStrCategories(const fs::path& folder, int mapId, std::map<int, std::string>& categories) {
    auto result = makeDbStrResult(folder, mapId);
    if (result.status == "MISSING_COMPONENT") return result;
    try {
        const auto data = readWholeFileBounded(result.dataPath);
        const auto offsetsData = readWholeFileBounded(result.offsetsPath);
        (void)readWholeFileBounded(result.headerPath, 1024ull * 1024ull);
        const auto offsets = parseDbStrOffsets(offsetsData, result);
        for (const auto& [index, off] : offsets) {
            try {
                if (off >= data.size()) { ++result.skippedEntries; continue; }
                std::size_t moved = 0;
                const auto entrySize = readVarSizeNum(data, off, moved);
                if (entrySize == 0 || entrySize > MaxDecodedFieldBytes) { ++result.skippedEntries; continue; }
                const auto entryEnd64 = static_cast<std::uint64_t>(off) + entrySize;
                if (entryEnd64 > data.size() || moved > entrySize) { ++result.skippedEntries; continue; }
                auto name = dbStrUtf8(data, static_cast<std::size_t>(off) + moved, static_cast<std::size_t>(entryEnd64));
                if (name.empty()) { ++result.skippedEntries; continue; }
                categories[index] = name;
                ++result.parsedEntries;
            } catch (...) { ++result.skippedEntries; }
        }
        result.status = "PARSED";
        result.message = "dbStr category map parsed";
    } catch (const std::exception& ex) {
        result.status = "FAILED";
        result.message = ex.what();
    }
    return result;
}

DbStrMapLoadResult parseDbStrIndexes(const fs::path& folder, int mapId, std::map<int, std::vector<int>>& indexes, bool hasExtraByte) {
    auto result = makeDbStrResult(folder, mapId);
    if (result.status == "MISSING_COMPONENT") return result;
    try {
        const auto data = readWholeFileBounded(result.dataPath);
        const auto offsetsData = readWholeFileBounded(result.offsetsPath);
        (void)readWholeFileBounded(result.headerPath, 1024ull * 1024ull);
        const auto offsets = parseDbStrOffsets(offsetsData, result);
        for (const auto& [index, off] : offsets) {
            try {
                if (off >= data.size()) { ++result.skippedEntries; continue; }
                std::size_t pos = off;
                std::size_t movedEntry = 0;
                const auto entrySize = readIndexVarSizeNum(data, pos, movedEntry);
                pos += movedEntry;
                std::size_t movedIndex = 0;
                const auto rawIndexSize = readVarSizeNum(data, pos, movedIndex);
                pos += movedIndex;
                if (hasExtraByte) ++pos;
                const auto entryEnd64 = static_cast<std::uint64_t>(off) + entrySize;
                if (entrySize == 0 || entrySize > MaxDecodedFieldBytes || entryEnd64 > data.size()) { ++result.skippedEntries; continue; }
                const std::size_t byteCount = static_cast<std::size_t>((rawIndexSize / 4u) * 4u);
                if (pos + byteCount > static_cast<std::size_t>(entryEnd64) || pos + byteCount > data.size()) { ++result.skippedEntries; continue; }
                std::vector<int> values;
                values.reserve(byteCount / 4);
                for (std::size_t i = 0; i + 4 <= byteCount; i += 4) values.push_back(readI32(data, pos + i));
                indexes[index] = std::move(values);
                ++result.parsedEntries;
            } catch (...) { ++result.skippedEntries; }
        }
        result.status = "PARSED";
        result.message = "dbStr index map parsed";
    } catch (const std::exception& ex) {
        result.status = "FAILED";
        result.message = ex.what();
    }
    return result;
}

bool hasExternalDbStrMapComponents(const fs::path& folder) {
    return fileExistsRegular(folder / "dbStr-1.map.data") &&
           fileExistsRegular(folder / "dbStr-1.map.offsets") &&
           fileExistsRegular(folder / "dbStr-1.map.header") &&
           fileExistsRegular(folder / "dbStr-2.map.data") &&
           fileExistsRegular(folder / "dbStr-2.map.offsets") &&
           fileExistsRegular(folder / "dbStr-2.map.header");
}

bool shouldLoadExternalDbStrMaps(const HeaderInfo& h, const StoreInfo& store) {
    if (h.version != 2 || h.idx11 != 0) return false;
    const fs::path folder = store.storePath.parent_path();
    if (hasExternalDbStrMapComponents(folder)) return true;
    if (store.inferredIosStore) return true;
    std::string s = lowerAscii(pathString(store.storePath));
    std::replace(s.begin(), s.end(), '\\', '/');
    return s.find("/corespotlight/") != std::string::npos || s.find("index.spotlightv2") != std::string::npos || s.find("/store-v2/") != std::string::npos;
}

std::vector<DbStrMapLoadResult> loadExternalDbStrMapsForStore(const fs::path& folder,
                                                          std::map<int, PropertyDef>& properties,
                                                          std::map<int, std::string>& categories,
                                                          std::map<int, std::vector<int>>& indexes1,
                                                          std::map<int, std::vector<int>>& indexes2) {
    std::vector<DbStrMapLoadResult> results;
    results.push_back(parseDbStrProperties(folder, 1, properties));
    results.push_back(parseDbStrCategories(folder, 2, categories));
    results.push_back(parseDbStrIndexes(folder, 4, indexes1, false));
    results.push_back(parseDbStrIndexes(folder, 5, indexes2, true));
    return results;
}

template <typename F> void parseBlockSequence(std::ifstream& in, const HeaderInfo& h, std::uint32_t initialIndex, BlockType expected, F parser) {
    std::set<std::uint32_t> seen;
    std::uint32_t current = initialIndex;
    while (current != 0 && seen.insert(current).second) {
        const auto offset = static_cast<std::uint64_t>(current) * 0x1000ull;
        auto block = readStoreBlock(in, offset, h.blockSize);
        if (block.baseType() != expected) break;
        auto raw = readFileAt(in, offset, h.blockSize);
        parser(raw, static_cast<int>(block.logicalSize));
        current = block.nextBlockIndex;
    }
}

std::vector<std::uint8_t> decompressMetadataBlock(std::ifstream& in, std::uint64_t blockOffset, const StoreBlockInfo& block, std::uint32_t blockSize) {
    auto raw = readFileAt(in, blockOffset, blockSize);
    if (block.logicalSize <= 20 || block.logicalSize > raw.size()) return {};
    if (block.isLz4Compressed()) return decompressBv41OrBv4Block(raw, block);
    if (block.isLzfseCompressed()) throw std::runtime_error("LZFSE/LZVN metadata compression not implemented in native decoder yet");
    std::vector<std::uint8_t> z(raw.begin() + 20, raw.begin() + static_cast<std::ptrdiff_t>(block.logicalSize));
    return inflateZlib(z);
}

bool isCoreNativeField(const std::string& name) {
    static const std::set<std::string> fields = {
        "_kMDItemFileName", "kMDItemDisplayName", "kMDItemContentType", "kMDItemContentTypeTree",
        "kMDItemWhereFroms", "kMDItemDownloadedDate", "kMDItemContentCreationDate",
        "kMDItemContentModificationDate", "kMDItemLastUsedDate", "kMDItemUsedDates",
        "kMDItemFSSize", "kMDItemLogicalSize", "_kMDItemLogicalSize", "kMDItemFileSize",
        "kMDItemPhysicalSize", "_kMDItemPhysicalSize", "kMDItemAuthors", "kMDItemCreator",
        "kMDItemDescription", "kMDItemTextContent", "kMDItemKind", "kMDItemDocumentIdentifier"
    };
    return fields.find(name) != fields.end();
}




std::string hexByte(std::uint8_t v) {
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(v);
    return os.str();
}

std::string nativeDecodeModeName(NativeDecodeMode mode) {
    switch (mode) {
        case NativeDecodeMode::HeaderOnly: return "HeaderOnly";
        case NativeDecodeMode::CoreFields: return "CoreFields";
        case NativeDecodeMode::FullValues: return "FullValues";
    }
    return "Unknown";
}

void insertNativePropertyDictionary(CaseDatabase& db,
                                    const EvidenceSource& source,
                                    const StoreInfo& store,
                                    int spotlightVersion,
                                    const std::map<int, PropertyDef>& properties) {
    auto del = db.prepare("DELETE FROM native_property_dictionary WHERE source_id=? AND store_guid=? AND source_db=?");
    del.bind(1, source.sourceId); del.bind(2, store.storeGuid); del.bind(3, pathString(store.storePath)); del.stepDone();
    auto st = db.prepare("INSERT INTO native_property_dictionary(source_id,store_guid,source_db,spotlight_version,property_index,property_name,prop_type_dec,value_type_dec,prop_type_hex,value_type_hex,is_core_native_field,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
    for (const auto& kv : properties) {
        st.bind(1, source.sourceId);
        st.bind(2, store.storeGuid);
        st.bind(3, pathString(store.storePath));
        st.bind(4, static_cast<long long>(spotlightVersion));
        st.bind(5, static_cast<long long>(kv.first));
        st.bind(6, kv.second.name);
        st.bind(7, static_cast<long long>(kv.second.propType));
        st.bind(8, static_cast<long long>(kv.second.valueType));
        st.bind(9, hexByte(kv.second.propType));
        st.bind(10, hexByte(kv.second.valueType));
        st.bind(11, isCoreNativeField(kv.second.name) ? 1LL : 0LL);
        st.bind(12, nowUtc());
        st.stepDone(); st.reset();
    }
}


void insertNativeCategoryDictionary(CaseDatabase& db,
                                    const EvidenceSource& source,
                                    const StoreInfo& store,
                                    int spotlightVersion,
                                    const std::map<int, std::string>& categories) {
    auto del = db.prepare("DELETE FROM native_category_dictionary WHERE source_id=? AND store_guid=? AND source_db=?");
    del.bind(1, source.sourceId); del.bind(2, store.storeGuid); del.bind(3, pathString(store.storePath)); del.stepDone();
    auto st = db.prepare("INSERT INTO native_category_dictionary(source_id,store_guid,source_db,spotlight_version,category_index,category_name,created_utc) VALUES(?,?,?,?,?,?,?)");
    for (const auto& kv : categories) {
        st.bind(1, source.sourceId);
        st.bind(2, store.storeGuid);
        st.bind(3, pathString(store.storePath));
        st.bind(4, static_cast<long long>(spotlightVersion));
        st.bind(5, static_cast<long long>(kv.first));
        st.bind(6, kv.second);
        st.bind(7, nowUtc());
        st.stepDone(); st.reset();
    }
}

void insertNativeDbStrMapInventory(CaseDatabase& db,
                                   const EvidenceSource& source,
                                   const StoreInfo& store,
                                   const std::vector<DbStrMapLoadResult>& results) {
    auto del = db.prepare("DELETE FROM native_dbstr_map_inventory WHERE source_id=? AND store_guid=? AND source_db=?");
    del.bind(1, source.sourceId); del.bind(2, store.storeGuid); del.bind(3, pathString(store.storePath)); del.stepDone();
    auto st = db.prepare("INSERT INTO native_dbstr_map_inventory(source_id,store_guid,source_db,map_id,data_path,offsets_path,header_path,data_exists,offsets_exists,header_exists,data_bytes,offsets_bytes,header_bytes,offset_entries,parsed_entries,skipped_entries,status,message,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    for (const auto& r : results) {
        st.bind(1, source.sourceId);
        st.bind(2, store.storeGuid);
        st.bind(3, pathString(store.storePath));
        st.bind(4, static_cast<long long>(r.mapId));
        st.bind(5, pathString(r.dataPath));
        st.bind(6, pathString(r.offsetsPath));
        st.bind(7, pathString(r.headerPath));
        st.bind(8, r.dataExists ? 1LL : 0LL);
        st.bind(9, r.offsetsExists ? 1LL : 0LL);
        st.bind(10, r.headerExists ? 1LL : 0LL);
        st.bind(11, static_cast<long long>(r.dataBytes));
        st.bind(12, static_cast<long long>(r.offsetsBytes));
        st.bind(13, static_cast<long long>(r.headerBytes));
        st.bind(14, static_cast<long long>(r.offsetEntries));
        st.bind(15, static_cast<long long>(r.parsedEntries));
        st.bind(16, static_cast<long long>(r.skippedEntries));
        st.bind(17, r.status);
        st.bind(18, r.message);
        st.bind(19, nowUtc());
        st.stepDone(); st.reset();
    }
}

void insertNativeIndexDictionarySummary(CaseDatabase& db,
                                        const EvidenceSource& source,
                                        const StoreInfo& store,
                                        int spotlightVersion,
                                        const std::string& mapName,
                                        const std::map<int, std::vector<int>>& indexes) {
    auto del = db.prepare("DELETE FROM native_index_dictionary_summary WHERE source_id=? AND store_guid=? AND source_db=? AND map_name=?");
    del.bind(1, source.sourceId); del.bind(2, store.storeGuid); del.bind(3, pathString(store.storePath)); del.bind(4, mapName); del.stepDone();
    std::size_t refs = 0;
    std::size_t maxRefs = 0;
    int maxIndex = 0;
    for (const auto& kv : indexes) {
        refs += kv.second.size();
        if (kv.second.size() > maxRefs) { maxRefs = kv.second.size(); maxIndex = kv.first; }
    }
    auto st = db.prepare("INSERT INTO native_index_dictionary_summary(source_id,store_guid,source_db,spotlight_version,map_name,index_rows,value_ref_count,max_refs_per_index,max_ref_index,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?)");
    st.bind(1, source.sourceId);
    st.bind(2, store.storeGuid);
    st.bind(3, pathString(store.storePath));
    st.bind(4, static_cast<long long>(spotlightVersion));
    st.bind(5, mapName);
    st.bind(6, static_cast<long long>(indexes.size()));
    st.bind(7, static_cast<long long>(refs));
    st.bind(8, static_cast<long long>(maxRefs));
    st.bind(9, static_cast<long long>(maxIndex));
    st.bind(10, nowUtc());
    st.stepDone();
}

void insertNativeDecodeAttempt(CaseDatabase& db,
                               const EvidenceSource& source,
                               const StoreInfo& store,
                               NativeDecodeMode decodeMode,
                               int spotlightVersion,
                               std::size_t propertiesCount,
                               std::size_t categoriesCount,
                               std::size_t metadataBlocks,
                               std::size_t decompressedBlocks,
                               std::size_t rawRecords,
                               std::size_t rawKeyValues,
                               std::size_t rawDateCandidates,
                               std::size_t fallbackHeaderOnlyItems,
                               std::size_t failures,
                               const std::string& startedUtc,
                               const std::string& status,
                               const std::string& message) {
    auto st = db.prepare("INSERT INTO native_decode_attempts(source_id,store_guid,source_db,decode_mode,spotlight_version,properties_count,categories_count,metadata_blocks,decompressed_blocks,raw_records,raw_key_values,raw_date_candidates,fallback_header_only_items,failures,started_utc,finished_utc,status,message) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    st.bind(1, source.sourceId);
    st.bind(2, store.storeGuid);
    st.bind(3, pathString(store.storePath));
    st.bind(4, nativeDecodeModeName(decodeMode));
    st.bind(5, static_cast<long long>(spotlightVersion));
    st.bind(6, static_cast<long long>(propertiesCount));
    st.bind(7, static_cast<long long>(categoriesCount));
    st.bind(8, static_cast<long long>(metadataBlocks));
    st.bind(9, static_cast<long long>(decompressedBlocks));
    st.bind(10, static_cast<long long>(rawRecords));
    st.bind(11, static_cast<long long>(rawKeyValues));
    st.bind(12, static_cast<long long>(rawDateCandidates));
    st.bind(13, static_cast<long long>(fallbackHeaderOnlyItems));
    st.bind(14, static_cast<long long>(failures));
    st.bind(15, startedUtc);
    st.bind(16, nowUtc());
    st.bind(17, status);
    st.bind(18, message);
    st.stepDone();
}

bool isLikelyHighValueProbeString(const std::string& s) {
    if (s.size() < 4) return false;
    const auto l = lowerAscii(s);
    if (l.find("/volumes/") != std::string::npos) return true;
    if (l.find("file://") != std::string::npos) return true;
    if (l.find("http://") != std::string::npos || l.find("https://") != std::string::npos) return true;
    if (l.find("icloud") != std::string::npos) return true;
    if (l.find("onedrive") != std::string::npos || l.find("sharepoint") != std::string::npos) return true;
    if (l.find("dropbox") != std::string::npos || l.find("google drive") != std::string::npos) return true;
    if (s.find('/') != std::string::npos && (l.find("/users/") != std::string::npos || l.find("/documents") != std::string::npos || l.find("/desktop") != std::string::npos)) return true;
    return false;
}


bool hasAnyToken(const std::string& haystack, const std::initializer_list<const char*>& tokens) {
    for (const auto* token : tokens) {
        if (haystack.find(token) != std::string::npos) return true;
    }
    return false;
}

bool looksLikeEmailAddress(const std::string& valueLower) {
    const auto at = valueLower.find('@');
    if (at == std::string::npos || at == 0 || at + 3 >= valueLower.size()) return false;
    return valueLower.find('.', at + 1) != std::string::npos;
}

bool isEmailLocalPartChar(char ch) {
    const auto c = static_cast<unsigned char>(ch);
    return std::isalnum(c) || ch == '.' || ch == '_' || ch == '%' || ch == '+' || ch == '-';
}

bool isEmailDomainChar(char ch) {
    const auto c = static_cast<unsigned char>(ch);
    return std::isalnum(c) || ch == '.' || ch == '-';
}

bool isLikelyEmailCandidate(const std::string& candidate) {
    const auto at = candidate.find('@');
    if (at == std::string::npos || at == 0 || at + 1 >= candidate.size()) return false;
    if (candidate.find('@', at + 1) != std::string::npos) return false;
    const auto local = candidate.substr(0, at);
    const auto domain = candidate.substr(at + 1);
    if (local.empty() || domain.empty() || local.front() == '.' || local.back() == '.') return false;
    if (domain.front() == '.' || domain.back() == '.' || domain.front() == '-' || domain.back() == '-') return false;
    const auto dot = domain.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 2 > domain.size()) return false;
    const auto tld = domain.substr(dot + 1);
    if (tld.size() < 2 || tld.size() > 24) return false;
    for (char ch : tld) {
        if (!std::isalpha(static_cast<unsigned char>(ch))) return false;
    }
    for (char ch : local) {
        if (!isEmailLocalPartChar(ch)) return false;
    }
    for (char ch : domain) {
        if (!isEmailDomainChar(ch)) return false;
    }
    return true;
}

std::string extractFirstEmailCandidate(const std::string& value) {
    const auto clean = cleanDecodedString(trimAscii(value));
    if (clean.empty()) return {};
    std::size_t searchFrom = 0;
    while (true) {
        const auto at = clean.find('@', searchFrom);
        if (at == std::string::npos) break;
        std::size_t begin = at;
        while (begin > 0 && isEmailLocalPartChar(clean[begin - 1])) --begin;
        std::size_t end = at + 1;
        while (end < clean.size() && isEmailDomainChar(clean[end])) ++end;
        auto candidate = clean.substr(begin, end - begin);
        while (!candidate.empty() && (candidate.back() == '.' || candidate.back() == ',' || candidate.back() == ';' || candidate.back() == ')' || candidate.back() == ']' || candidate.back() == '"' || candidate.back() == '\'')) {
            candidate.pop_back();
        }
        if (isLikelyEmailCandidate(candidate)) return candidate;
        searchFrom = at + 1;
    }
    return {};
}

bool looksLikeForensicReferenceValue(const std::string& value) {
    if (value.empty()) return false;
    const auto v = lowerAscii(value);
    if (v.find("http://") != std::string::npos || v.find("https://") != std::string::npos || v.find("www.") != std::string::npos) return true;
    if (v.find("file://") != std::string::npos || v.find("/private/var/") != std::string::npos || v.find("/var/mobile/") != std::string::npos) return true;
    if (v.find("/mobile/") != std::string::npos || v.find("/containers/") != std::string::npos || v.find("/library/") != std::string::npos) return true;
    if (!extractFirstEmailCandidate(value).empty()) return true;
    if (hasAnyToken(v, {"whatsapp", "imessage", "mobilesms", "sms", "facetime", "mail", "safari", "chrome", "calendar", "contact", "icloud", "onedrive", "sharepoint", "dropbox", "google drive", "drive.google"})) return true;
    return false;
}


std::string basenameFromSpotlightPath(std::string p);

std::string twoDigitProbeSuffix(int n) {
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << n;
    return oss.str();
}

std::string extractFirstUrlCandidate(const std::string& value) {
    const auto lower = lowerAscii(value);
    std::size_t pos = lower.find("https://");
    if (pos == std::string::npos) pos = lower.find("http://");
    if (pos == std::string::npos) pos = lower.find("file://");
    if (pos == std::string::npos) return {};
    std::size_t end = pos;
    while (end < value.size()) {
        if (end > pos &&
            (lower.compare(end, 8, "https://") == 0 ||
             lower.compare(end, 7, "http://") == 0 ||
             lower.compare(end, 7, "file://") == 0)) {
            break;
        }
        const unsigned char c = static_cast<unsigned char>(value[end]);
        if (c <= 0x20 || value[end] == '"' || value[end] == '\'' || value[end] == '<' || value[end] == '>') break;
        ++end;
    }
    auto out = cleanDecodedString(trimAscii(value.substr(pos, end - pos)));
    while (!out.empty() && (out.back() == ',' || out.back() == ';' || out.back() == ')' || out.back() == ']' || out.back() == '}')) out.pop_back();
    return out;
}

std::string extractFirstFilePathCandidate(const std::string& value) {
    const auto lower = lowerAscii(value);
    std::array<std::string, 8> anchors = {"file:///", "/users/", "/private/", "/volumes/", "/library/", "/system/", "/applications/", "/var/"};
    std::size_t best = std::string::npos;
    for (const auto& a : anchors) {
        const auto p = lower.find(a);
        if (p != std::string::npos && (best == std::string::npos || p < best)) best = p;
    }
    if (best == std::string::npos) return {};
    std::size_t end = best;
    while (end < value.size()) {
        if (end > best &&
            (lower.compare(end, 8, "https://") == 0 ||
             lower.compare(end, 7, "http://") == 0 ||
             lower.compare(end, 7, "file://") == 0)) {
            break;
        }
        const unsigned char c = static_cast<unsigned char>(value[end]);
        if (c <= 0x20 || value[end] == '"' || value[end] == '\'' || value[end] == '<' || value[end] == '>') break;
        ++end;
    }
    auto out = cleanDecodedString(trimAscii(value.substr(best, end - best)));
    if (lowerAscii(out).rfind("file://", 0) == 0) out = out.substr(7);
    while (!out.empty() && (out.back() == ',' || out.back() == ';' || out.back() == ')' || out.back() == ']')) out.pop_back();
    return out;
}

std::string basenameFromReferenceValue(const std::string& value) {
    auto p = extractFirstFilePathCandidate(value);
    if (p.empty()) {
        auto u = extractFirstUrlCandidate(value);
        const auto q = u.find_first_of("?#");
        if (q != std::string::npos) u = u.substr(0, q);
        p = u;
    }
    return basenameFromSpotlightPath(p);
}


bool isUsefulDerivedBasenameCandidate(const std::string& value) {
    // V1.6.17: derived basename aliases are investigative conveniences only.
    // Keep raw __native_core_probe_string_## evidence untouched, but reject
    // binary/escaped/noisy basename derivations before they become aliases.
    const auto clean = cleanDecodedString(trimAscii(value));
    if (clean.size() < 3 || clean.size() > 255) return false;
    const auto lower = lowerAscii(clean);
    if (lower.find("\\x") != std::string::npos) return false;
    if (lower.find("%5cx") != std::string::npos) return false;
    if (lower.find("\ufffd") != std::string::npos) return false;
    if (lower == "url" || lower == "uri" || lower == "zip" || lower == "http" || lower == "https" || lower == "www") return false;
    if (lower == "download" || lower == "downloads" || lower == "login" || lower == "signin") return false;
    if (lower == "file" || lower == "files" || lower == "index" || lower == "data" || lower == "tmp") return false;

    int alnum = 0;
    int alpha = 0;
    int digit = 0;
    int highBytes = 0;
    int controlBytes = 0;
    int filenameSafe = 0;
    int punctuationNoise = 0;
    int longestAlphaRun = 0;
    int currentAlphaRun = 0;
    bool hasDot = false;
    bool hasWordSeparator = false;

    for (char ch : clean) {
        const auto c = static_cast<unsigned char>(ch);
        if (c <= 0x1F || c == 0x7F) {
            ++controlBytes;
            currentAlphaRun = 0;
            continue;
        }
        if (c >= 0x80) {
            ++highBytes;
            currentAlphaRun = 0;
            continue;
        }
        if (std::isalnum(c)) {
            ++alnum;
            ++filenameSafe;
        } else if (ch == '.' || ch == '_' || ch == '-' || ch == ' ' || ch == '%' || ch == '+' || ch == '(' || ch == ')' || ch == '[' || ch == ']') {
            ++filenameSafe;
            if (ch == '.') hasDot = true;
            if (ch == '_' || ch == '-' || ch == ' ') hasWordSeparator = true;
        } else {
            ++punctuationNoise;
        }
        if (std::isalpha(c)) {
            ++alpha;
            ++currentAlphaRun;
            if (currentAlphaRun > longestAlphaRun) longestAlphaRun = currentAlphaRun;
        } else {
            currentAlphaRun = 0;
        }
        if (std::isdigit(c)) ++digit;
    }

    if (controlBytes > 0) return false;
    // V1.6.17: do not promote high-byte or Unicode-separator fragments into basename aliases.
    // Raw probe rows still retain those bytes for evidence review.
    if (highBytes > 0) return false;
    if (alnum < 2) return false;
    if (alpha == 0) return false;
    if (filenameSafe * 2 < static_cast<int>(clean.size())) return false;
    if (punctuationNoise * 3 > static_cast<int>(clean.size())) return false;
    if (clean.size() <= 4 && alpha == 0) return false;
    if (digit > 0 && alpha == 0 && !hasDot) return false;
    if (longestAlphaRun < 3 && !hasDot && !hasWordSeparator) return false;
    return true;
}

std::vector<std::pair<std::string, std::string>> deriveNativeProbeAliases(const std::string& value, int sequence) {
    std::vector<std::pair<std::string, std::string>> out;
    const auto suffix = twoDigitProbeSuffix(sequence);
    const auto clean = cleanDecodedString(trimAscii(value));
    if (clean.empty()) return out;
    const auto lower = lowerAscii(clean);
    const auto url = extractFirstUrlCandidate(clean);
    if (!url.empty()) out.emplace_back("__native_probe_url_candidate_" + suffix, url);
    const auto filePath = extractFirstFilePathCandidate(clean);
    if (!filePath.empty()) out.emplace_back("__native_probe_file_path_candidate_" + suffix, filePath);
    const auto email = extractFirstEmailCandidate(clean);
    if (!email.empty()) out.emplace_back("__native_probe_email_candidate_" + suffix, email);
    const auto base = basenameFromReferenceValue(clean);
    if (!base.empty() && isUsefulDerivedBasenameCandidate(base)) out.emplace_back("__native_probe_basename_candidate_" + suffix, base);
    if (lower.find("<!doctype plist") != std::string::npos || lower.find("<?xml") != std::string::npos || lower.find("<plist") != std::string::npos) {
        out.emplace_back("__native_probe_plist_xml_candidate_" + suffix, clean.size() > 512 ? clean.substr(0, 512) + "...[truncated]" : clean);
    }
    return out;
}

bool isLowSignalNativeField(const std::string& fieldLower) {
    if (fieldLower.empty()) return false;
    if (fieldLower == "kmdstoreproperties" || fieldLower == "_kmdstoreproperties") return true;
    if (fieldLower.find("storeproperties") != std::string::npos) return true;
    if (hasAnyToken(fieldLower, {"ranking", "rank", "score", "boost", "vector", "embedding", "token", "termfrequency", "indexstatistics", "thumbnaildata", "thumbnailbytes", "bitmap", "signature", "checksum"})) return true;
    return false;
}

bool isHighValueNativeFieldName(const std::string& fieldLower) {
    if (fieldLower.empty()) return false;
    if (hasAnyToken(fieldLower, {
        "displayname", "filename", "fsname", "title", "headline", "subject", "snippet", "textcontent", "description",
        "path", "url", "wherefrom", "contenturl", "thumbnailurl", "webpage", "domainidentifier", "uniqueidentifier", "persistentidentifier", "targetcontentidentifier",
        "bundleid", "activitytype", "contenttype", "contenttypetree", "container", "mailbox", "conversation", "thread", "message", "chat", "sms", "imessage",
        "author", "sender", "recipient", "participant", "account", "handle", "email", "phone", "contact", "person", "name", "jid",
        "lastused", "usecount", "useddates", "wherefroms", "downloaded", "creationdate", "modificationdate", "metadata", "calendar", "event", "location"
    })) return true;
    return false;
}

std::string trimStoredNativeValue(const std::string& field, const std::string& value) {
    (void)field;
    constexpr std::size_t DefaultMaxStoredValueBytes = 1024;
    if (value.size() <= DefaultMaxStoredValueBytes) return value;
    return value.substr(0, DefaultMaxStoredValueBytes) + "...[truncated by default iOS Spotlight investigator mode; full native values require --diagnostic-full-native-db]";
}

std::string compactContextText(std::string s) {
    s = cleanDecodedString(std::move(s));
    std::string out;
    out.reserve(s.size());
    bool lastSpace = false;
    for (char ch : s) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x20 || ch == '\t' || ch == '\r' || ch == '\n') {
            if (!lastSpace) { out.push_back(' '); lastSpace = true; }
        } else {
            out.push_back(ch);
            lastSpace = std::isspace(c) != 0;
        }
    }
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.front()))) out.erase(out.begin());
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
    return out;
}

bool isLikelyUsefulSpotlightTextContext(const std::string& field, const std::string& value, bool dateField) {
    if (dateField || value.empty()) return false;
    const auto f = lowerAscii(field);
    if (field == "__decode_error") return false;
    const bool coreProbeField = field.rfind("__native_core_probe_string_", 0) == 0;
    if (field == "__spotlight_investigator_text_context") return false;
    if (isLowSignalNativeField(f)) return false;
    if (f.find("date") != std::string::npos || f.find("time") != std::string::npos) return false;
    if (hasAnyToken(f, {"size", "bytes", "count", "rank", "score", "uuid", "serial", "checksum", "signature", "thumbnail", "bitmap", "token", "embedding", "vector"})) return false;

    auto v = compactContextText(value);
    if (v.size() < 4) return false;
    if (v.size() > 2048) v.resize(2048);
    const auto vl = lowerAscii(v);
    if (vl == "true" || vl == "false" || vl == "null" || vl == "invalid") return false;

    const bool contextField = hasAnyToken(f, {
        "title", "displayname", "filename", "fsname", "headline", "subject", "snippet", "textcontent", "description", "comment", "keywords",
        "author", "sender", "recipient", "participant", "account", "handle", "email", "phone", "contact", "person", "name", "jid",
        "containerdisplayname", "containertitle", "domainidentifier", "bundleid", "activitytype", "contenttype", "message", "chat", "sms", "imessage"
    });
    if (contextField) return true;
    if (looksLikeEmailAddress(vl)) return true;
    if (coreProbeField && looksLikeForensicReferenceValue(vl)) return true;
    if (hasAnyToken(vl, {"whatsapp", "imessage", "mobilesms", "facetime", "calendar", "contact", "mail", "safari", "chrome"})) return true;
    return false;
}


bool containsBinaryPlistMarker(const std::string& value) {
    return value.find("bplist00") != std::string::npos;
}

bool containsNsKeyedArchiverMarker(const std::string& value) {
    return value.find("NSKeyedArchiver") != std::string::npos ||
           value.find("$objects") != std::string::npos ||
           value.find("$archiver") != std::string::npos;
}

bool isLikelyIosBplistValue(const std::string& value) {
    return containsBinaryPlistMarker(value) || containsNsKeyedArchiverMarker(value);
}

bool isTokenCharForBplistSummary(unsigned char c) {
    return (c >= 0x20 && c <= 0x7E);
}

bool isLowSignalBplistToken(const std::string& token);

void appendUtf8CodepointNative(std::string& out, std::uint32_t cp) {
    if (cp == 0) return;
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        if (cp >= 0xD800u && cp <= 0xDFFFu) return;
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0x10FFFFu) {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

std::string utf16beToUtf8BplistString(const std::string& bytes, std::size_t off, std::size_t byteLen) {
    std::string out;
    if (off >= bytes.size()) return out;
    const std::size_t end = (std::min)(bytes.size(), off + byteLen);
    for (std::size_t i = off; i + 1 < end; i += 2) {
        const std::uint16_t w1 = (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[i])) << 8) |
                                 static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[i + 1]));
        if (w1 >= 0xD800u && w1 <= 0xDBFFu && i + 3 < end) {
            const std::uint16_t w2 = (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[i + 2])) << 8) |
                                     static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[i + 3]));
            if (w2 >= 0xDC00u && w2 <= 0xDFFFu) {
                const std::uint32_t cp = 0x10000u + (((static_cast<std::uint32_t>(w1) - 0xD800u) << 10) |
                                                     (static_cast<std::uint32_t>(w2) - 0xDC00u));
                appendUtf8CodepointNative(out, cp);
                i += 2;
                continue;
            }
        }
        appendUtf8CodepointNative(out, w1);
    }
    return cleanDecodedString(std::move(out));
}

std::uint64_t readBplistBigEndianInt(const std::string& bytes, std::size_t off, std::size_t len, bool* ok = nullptr) {
    if (ok) *ok = false;
    if (len == 0 || len > 8 || off > bytes.size() || off + len > bytes.size()) return 0;
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < len; ++i) v = (v << 8) | static_cast<unsigned char>(bytes[off + i]);
    if (ok) *ok = true;
    return v;
}

std::string summarizeBplistTrailerAt(const std::string& value, std::size_t bplistOff) {
    constexpr std::size_t MaxBplistBytes = 1024ull * 1024ull;
    constexpr std::uint64_t MaxBplistObjects = 50000ull;
    if (bplistOff == std::string::npos) return "bplist_trailer=not_found";
    if (bplistOff > value.size() || value.size() - bplistOff < 40) return "bplist_trailer=too_short";
    const std::size_t bplistSize = value.size() - bplistOff;
    if (bplistSize > MaxBplistBytes) return "bplist_trailer=too_large";
    if (std::memcmp(value.data() + static_cast<std::ptrdiff_t>(bplistOff), "bplist00", 8) != 0) return "bplist_trailer=bad_magic";

    const std::size_t trailer = value.size() - 32;
    const auto offsetIntSize = static_cast<unsigned char>(value[trailer + 6]);
    const auto objectRefSize = static_cast<unsigned char>(value[trailer + 7]);
    bool ok = false;
    const std::uint64_t numObjects = readBplistBigEndianInt(value, trailer + 8, 8, &ok);
    if (!ok) return "bplist_trailer=invalid_num_objects";
    const std::uint64_t topObject = readBplistBigEndianInt(value, trailer + 16, 8, &ok);
    if (!ok) return "bplist_trailer=invalid_top_object";
    const std::uint64_t offsetTableOffsetRel = readBplistBigEndianInt(value, trailer + 24, 8, &ok);
    if (!ok) return "bplist_trailer=invalid_offset_table";
    if (offsetIntSize == 0 || offsetIntSize > 8 || objectRefSize == 0 || objectRefSize > 8) return "bplist_trailer=invalid_widths";
    if (numObjects == 0 || numObjects > MaxBplistObjects || topObject >= numObjects) return "bplist_trailer=invalid_object_counts";
    if (offsetTableOffsetRel >= bplistSize) return "bplist_trailer=invalid_offset_table_bounds";
    if (numObjects > (std::numeric_limits<std::size_t>::max)() / offsetIntSize) return "bplist_trailer=offset_table_overflow";
    const std::size_t offsetTable = bplistOff + static_cast<std::size_t>(offsetTableOffsetRel);
    const std::size_t offsetTableBytes = static_cast<std::size_t>(numObjects) * offsetIntSize;
    if (offsetTable < bplistOff || offsetTable > value.size() || offsetTableBytes > value.size() - offsetTable) return "bplist_trailer=offset_table_out_of_range";

    std::uint64_t topObjectOffsetRel = 0;
    bool topObjectOffsetOk = false;
    if (topObject < numObjects) {
        const std::size_t topOffsetPos = offsetTable + static_cast<std::size_t>(topObject) * offsetIntSize;
        topObjectOffsetRel = readBplistBigEndianInt(value, topOffsetPos, offsetIntSize, &topObjectOffsetOk);
        if (topObjectOffsetOk && topObjectOffsetRel >= bplistSize) topObjectOffsetOk = false;
    }

    std::ostringstream os;
    os << "bplist_trailer=valid"
       << ";objects=" << numObjects
       << ";top_object=" << topObject
       << ";offset_int_size=" << static_cast<unsigned int>(offsetIntSize)
       << ";object_ref_size=" << static_cast<unsigned int>(objectRefSize)
       << ";offset_table_rel=" << offsetTableOffsetRel
       << ";offset_table_bytes=" << offsetTableBytes
       << ";offset_table_status=parsed"
       << ";top_object_offset_rel=" << (topObjectOffsetOk ? std::to_string(topObjectOffsetRel) : std::string("invalid"));
    return os.str();
}

std::string summarizeBplistTrailer(const std::string& value) {
    const std::size_t bplistOff = value.find("bplist00");
    return summarizeBplistTrailerAt(value, bplistOff);
}

bool addBplistToken(std::vector<std::string>& tokens, std::unordered_set<std::string>& seen, std::string token, std::size_t maxTokenBytes, std::size_t maxTokens) {
    token = cleanDecodedString(trimAscii(std::move(token)));
    if (token.size() > maxTokenBytes) token = token.substr(0, maxTokenBytes) + "...[truncated]";
    if (token.empty() || isLowSignalBplistToken(token)) return tokens.size() < maxTokens;
    if (seen.insert(token).second) tokens.push_back(token);
    return tokens.size() < maxTokens;
}

bool isLowSignalBplistToken(const std::string& token) {
    if (token.size() < 3) return true;
    const std::string t = lowerAscii(token);
    static const char* lowSignal[] = {
        "$null", "$top", "$version", "$archiver", "$objects", "$class", "ns.objects", "ns.keys", "ns.object",
        "null", "root", "bytes"
    };
    for (const auto* x : lowSignal) if (t == x) return true;
    std::size_t useful = 0;
    for (unsigned char c : token) {
        if (std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == '/' || c == '@' || c == ':') ++useful;
    }
    return useful < 3;
}

std::vector<std::string> extractBplistObjectStringTokensAt(const std::string& value,
                                                            std::size_t bplistOff,
                                                            std::size_t maxTokens,
                                                            std::size_t maxTokenBytes) {
    constexpr std::size_t MaxBplistBytes = 1024ull * 1024ull;
    constexpr std::uint64_t MaxBplistObjects = 50000ull;
    std::vector<std::string> tokens;
    std::unordered_set<std::string> seen;
    if (bplistOff > value.size() || value.size() - bplistOff < 40) return tokens;
    const std::size_t bplistSize = value.size() - bplistOff;
    if (bplistSize > MaxBplistBytes) return tokens;
    if (std::memcmp(value.data() + static_cast<std::ptrdiff_t>(bplistOff), "bplist00", 8) != 0) return tokens;

    const std::size_t trailer = value.size() - 32;
    const auto offsetIntSize = static_cast<unsigned char>(value[trailer + 6]);
    const auto objectRefSize = static_cast<unsigned char>(value[trailer + 7]);
    bool ok = false;
    const std::uint64_t numObjects = readBplistBigEndianInt(value, trailer + 8, 8, &ok);
    if (!ok) return tokens;
    const std::uint64_t offsetTableOffsetRel = readBplistBigEndianInt(value, trailer + 24, 8, &ok);
    if (!ok || offsetIntSize == 0 || offsetIntSize > 8 || objectRefSize == 0 || objectRefSize > 8 || numObjects == 0 || numObjects > MaxBplistObjects) return tokens;
    if (offsetTableOffsetRel > bplistSize) return tokens;
    const std::size_t offsetTable = bplistOff + static_cast<std::size_t>(offsetTableOffsetRel);
    if (offsetTable < bplistOff || offsetTable > value.size()) return tokens;
    if (numObjects > (std::numeric_limits<std::size_t>::max)() / offsetIntSize) return tokens;
    const std::size_t offsetTableBytes = static_cast<std::size_t>(numObjects) * offsetIntSize;
    if (offsetTableBytes > value.size() - offsetTable) return tokens;

    auto readObjectLength = [&](std::size_t& payloadOff, std::uint64_t inlineCount, std::uint64_t& countOut) -> bool {
        countOut = inlineCount;
        if (inlineCount != 0x0Fu) return true;
        if (payloadOff >= value.size()) return false;
        const auto intMarker = static_cast<unsigned char>(value[payloadOff++]);
        if ((intMarker & 0xF0u) != 0x10u) return false;
        const std::size_t intBytes = std::size_t{1} << (intMarker & 0x0Fu);
        if (intBytes == 0 || intBytes > 8 || payloadOff > value.size() || intBytes > value.size() - payloadOff) return false;
        bool lenOk = false;
        countOut = readBplistBigEndianInt(value, payloadOff, intBytes, &lenOk);
        payloadOff += intBytes;
        return lenOk;
    };

    for (std::uint64_t objIndex = 0; objIndex < numObjects && tokens.size() < maxTokens; ++objIndex) {
        const std::size_t offsetPos = offsetTable + static_cast<std::size_t>(objIndex) * offsetIntSize;
        const std::uint64_t objRel = readBplistBigEndianInt(value, offsetPos, offsetIntSize, &ok);
        if (!ok || objRel >= bplistSize) continue;
        const std::size_t objOff = bplistOff + static_cast<std::size_t>(objRel);
        if (objOff >= value.size()) continue;
        const auto marker = static_cast<unsigned char>(value[objOff]);
        const std::uint8_t type = marker & 0xF0u;
        std::uint64_t count = marker & 0x0Fu;
        std::size_t payloadOff = objOff + 1;
        if (!readObjectLength(payloadOff, count, count)) continue;
        if (type == 0x50u) { // ASCII string
            if (count == 0 || count > MaxBplistBytes || payloadOff > value.size() || count > value.size() - payloadOff) continue;
            std::string token(value.data() + static_cast<std::ptrdiff_t>(payloadOff), static_cast<std::size_t>(count));
            if (!addBplistToken(tokens, seen, std::move(token), maxTokenBytes, maxTokens)) break;
        } else if (type == 0x60u) { // UTF-16BE string
            if (count == 0 || count > MaxBplistBytes / 2) continue;
            const std::size_t byteLen = static_cast<std::size_t>(count) * 2;
            if (payloadOff > value.size() || byteLen > value.size() - payloadOff) continue;
            std::string token = utf16beToUtf8BplistString(value, payloadOff, byteLen);
            if (!addBplistToken(tokens, seen, std::move(token), maxTokenBytes, maxTokens)) break;
        }
    }
    return tokens;
}


std::string jsonEscapeBplistSample(const std::string& s) {
    std::string out;
    out.reserve(std::min<std::size_t>(s.size() + 8, 512));
    for (unsigned char c : s) {
        if (out.size() >= 512) { out += "...[truncated]"; break; }
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c >= 0x20 && c < 0x7f) out.push_back(static_cast<char>(c));
            else out.push_back(' ');
        }
    }
    return out;
}

bool readBplistObjectLengthBounded(const std::string& value, std::size_t& payloadOff, std::uint64_t inlineCount, std::uint64_t& countOut) {
    countOut = inlineCount;
    if (inlineCount != 0x0Fu) return true;
    if (payloadOff >= value.size()) return false;
    const auto intMarker = static_cast<unsigned char>(value[payloadOff++]);
    if ((intMarker & 0xF0u) != 0x10u) return false;
    const std::size_t intBytes = std::size_t{1} << (intMarker & 0x0Fu);
    if (intBytes == 0 || intBytes > 8 || payloadOff > value.size() || intBytes > value.size() - payloadOff) return false;
    bool ok = false;
    countOut = readBplistBigEndianInt(value, payloadOff, intBytes, &ok);
    payloadOff += intBytes;
    return ok;
}

std::string resolveBplistUidGraphSample(const std::string& value,
                                        std::size_t bplistOff,
                                        const std::vector<std::size_t>& objOffsets,
                                        std::uint8_t objectRefSize,
                                        std::uint64_t uid,
                                        int depth,
                                        std::set<std::uint64_t>& stack,
                                        std::size_t& budget) {
    if (budget == 0) return "\"<budget_exhausted>\"";
    if (depth > 10) return "\"<recursion_limit>\"";
    if (uid >= objOffsets.size()) return "\"<invalid_uid>\"";
    if (!stack.insert(uid).second) return "\"<cycle>\"";
    auto finish = [&](std::string out) {
        stack.erase(uid);
        if (out.size() > budget) {
            out.resize(budget);
            out += "...[truncated]";
            budget = 0;
        } else {
            budget -= out.size();
        }
        return out;
    };
    const std::size_t objOff = objOffsets[static_cast<std::size_t>(uid)];
    if (objOff < bplistOff || objOff >= value.size()) return finish("\"<object_out_of_range>\"");
    const auto marker = static_cast<unsigned char>(value[objOff]);
    const std::uint8_t type = marker & 0xF0u;
    std::uint64_t count = marker & 0x0Fu;
    std::size_t payloadOff = objOff + 1;
    if (!readBplistObjectLengthBounded(value, payloadOff, count, count)) return finish("\"<bad_length>\"");
    if (type == 0x00u) {
        if (marker == 0x00u) return finish("null");
        if (marker == 0x08u) return finish("false");
        if (marker == 0x09u) return finish("true");
        return finish("\"<simple>\"");
    }
    if (type == 0x10u) {
        const std::size_t bytes = std::size_t{1} << (marker & 0x0Fu);
        if (bytes == 0 || bytes > 8 || payloadOff > value.size() || bytes > value.size() - payloadOff) return finish("\"<bad_int>\"");
        bool ok = false;
        const std::uint64_t v = readBplistBigEndianInt(value, payloadOff, bytes, &ok);
        return finish(ok ? std::to_string(v) : "\"<bad_int>\"");
    }
    if (type == 0x50u) {
        if (count > 2048 || payloadOff > value.size() || count > value.size() - payloadOff) return finish("\"<bad_ascii>\"");
        return finish("\"" + jsonEscapeBplistSample(std::string(value.data() + static_cast<std::ptrdiff_t>(payloadOff), static_cast<std::size_t>(count))) + "\"");
    }
    if (type == 0x60u) {
        if (count > 1024) return finish("\"<utf16_too_large>\"");
        const std::size_t byteLen = static_cast<std::size_t>(count) * 2;
        if (payloadOff > value.size() || byteLen > value.size() - payloadOff) return finish("\"<bad_utf16>\"");
        return finish("\"" + jsonEscapeBplistSample(utf16beToUtf8BplistString(value, payloadOff, byteLen)) + "\"");
    }
    if (type == 0x80u) { // UID object; treat as a pointer into $objects when possible.
        const std::size_t bytes = static_cast<std::size_t>(count) + 1U;
        if (bytes == 0 || bytes > 8 || payloadOff > value.size() || bytes > value.size() - payloadOff) return finish("\"<bad_uid>\"");
        bool ok = false;
        const std::uint64_t ref = readBplistBigEndianInt(value, payloadOff, bytes, &ok);
        if (!ok) return finish("\"<bad_uid>\"");
        stack.erase(uid);
        return resolveBplistUidGraphSample(value, bplistOff, objOffsets, objectRefSize, ref, depth + 1, stack, budget);
    }
    if (type == 0xA0u) { // Array
        if (count > 32 || payloadOff > value.size() || static_cast<std::size_t>(count) > (value.size() - payloadOff) / objectRefSize) return finish("\"<array_too_large_or_bad>\"");
        std::string out = "[";
        for (std::uint64_t i = 0; i < count && budget > 0; ++i) {
            bool ok = false;
            const std::uint64_t ref = readBplistBigEndianInt(value, payloadOff + static_cast<std::size_t>(i) * objectRefSize, objectRefSize, &ok);
            if (i) out += ",";
            out += ok ? resolveBplistUidGraphSample(value, bplistOff, objOffsets, objectRefSize, ref, depth + 1, stack, budget) : "\"<bad_ref>\"";
            if (out.size() > 4096) { out += ",\"...[truncated]\""; break; }
        }
        out += "]";
        return finish(out);
    }
    if (type == 0xD0u) { // Dictionary
        if (count > 32 || payloadOff > value.size() || static_cast<std::size_t>(count) > (value.size() - payloadOff) / (objectRefSize * 2U)) return finish("\"<dict_too_large_or_bad>\"");
        std::string out = "{";
        for (std::uint64_t i = 0; i < count && budget > 0; ++i) {
            bool ok1 = false, ok2 = false;
            const std::uint64_t keyRef = readBplistBigEndianInt(value, payloadOff + static_cast<std::size_t>(i) * objectRefSize, objectRefSize, &ok1);
            const std::uint64_t valRef = readBplistBigEndianInt(value, payloadOff + static_cast<std::size_t>(count + i) * objectRefSize, objectRefSize, &ok2);
            if (i) out += ",";
            out += ok1 ? resolveBplistUidGraphSample(value, bplistOff, objOffsets, objectRefSize, keyRef, depth + 1, stack, budget) : "\"<bad_key>\"";
            out += ":";
            out += ok2 ? resolveBplistUidGraphSample(value, bplistOff, objOffsets, objectRefSize, valRef, depth + 1, stack, budget) : "\"<bad_value>\"";
            if (out.size() > 4096) { out += ",\"...[truncated]\":true"; break; }
        }
        out += "}";
        return finish(out);
    }
    return finish("\"<unparsed_type_" + std::to_string(type) + ">\"");
}

std::string reconstructBplistTopObjectGraphSampleAt(const std::string& value, std::size_t bplistOff) {
    constexpr std::size_t MaxBplistBytes = 1024ull * 1024ull;
    constexpr std::uint64_t MaxBplistObjects = 50000ull;
    if (bplistOff == std::string::npos || bplistOff > value.size() || value.size() - bplistOff < 40) return {};
    const std::size_t bplistSize = value.size() - bplistOff;
    if (bplistSize > MaxBplistBytes) return "graph_decode_status=skipped_too_large";
    if (std::memcmp(value.data() + static_cast<std::ptrdiff_t>(bplistOff), "bplist00", 8) != 0) return {};
    const std::size_t trailer = value.size() - 32;
    const auto offsetIntSize = static_cast<unsigned char>(value[trailer + 6]);
    const auto objectRefSize = static_cast<unsigned char>(value[trailer + 7]);
    bool ok = false;
    const std::uint64_t numObjects = readBplistBigEndianInt(value, trailer + 8, 8, &ok);
    if (!ok) return "graph_decode_status=invalid_num_objects";
    const std::uint64_t topObject = readBplistBigEndianInt(value, trailer + 16, 8, &ok);
    if (!ok) return "graph_decode_status=invalid_top_object";
    const std::uint64_t offsetTableOffsetRel = readBplistBigEndianInt(value, trailer + 24, 8, &ok);
    if (!ok || offsetIntSize == 0 || offsetIntSize > 8 || objectRefSize == 0 || objectRefSize > 8 || numObjects == 0 || numObjects > MaxBplistObjects || topObject >= numObjects) return "graph_decode_status=invalid_trailer";
    if (offsetTableOffsetRel >= bplistSize || numObjects > (std::numeric_limits<std::size_t>::max)() / offsetIntSize) return "graph_decode_status=invalid_offset_table";
    const std::size_t offsetTable = bplistOff + static_cast<std::size_t>(offsetTableOffsetRel);
    const std::size_t offsetTableBytes = static_cast<std::size_t>(numObjects) * offsetIntSize;
    if (offsetTable < bplistOff || offsetTable > value.size() || offsetTableBytes > value.size() - offsetTable) return "graph_decode_status=offset_table_out_of_range";
    std::vector<std::size_t> objOffsets;
    objOffsets.reserve(static_cast<std::size_t>(numObjects));
    for (std::uint64_t i = 0; i < numObjects; ++i) {
        const std::uint64_t rel = readBplistBigEndianInt(value, offsetTable + static_cast<std::size_t>(i) * offsetIntSize, offsetIntSize, &ok);
        if (!ok || rel >= bplistSize) return "graph_decode_status=object_offset_out_of_range";
        objOffsets.push_back(bplistOff + static_cast<std::size_t>(rel));
    }
    std::set<std::uint64_t> stack;
    std::size_t budget = 4096;
    std::string sample = resolveBplistUidGraphSample(value, bplistOff, objOffsets, static_cast<std::uint8_t>(objectRefSize), topObject, 0, stack, budget);
    if (sample.empty()) return "graph_decode_status=no_sample";
    return "graph_decode_status=bounded_top_object_sample; graph_sample=" + sample;
}

std::vector<std::string> extractPrintableBplistTokens(const std::string& value) {
    constexpr std::size_t MaxInputScanBytes = 16384;
    constexpr std::size_t MaxTokens = 48;
    constexpr std::size_t MaxTokenBytes = 120;

    const std::size_t bplistOff = value.find("bplist00");
    if (bplistOff != std::string::npos) {
        auto parsed = extractBplistObjectStringTokensAt(value, bplistOff, MaxTokens, MaxTokenBytes);
        if (!parsed.empty()) return parsed;
    }

    // Conservative fallback for damaged/truncated bplist payloads: preserve the
    // prior bounded printable-token scan, but use hash-based deduplication.
    std::vector<std::string> tokens;
    std::unordered_set<std::string> seen;
    std::string cur;
    const std::size_t n = std::min<std::size_t>(value.size(), MaxInputScanBytes);
    auto flush = [&]() {
        if (cur.size() >= 3) {
            std::string token = cur;
            if (token.size() > MaxTokenBytes) token = token.substr(0, MaxTokenBytes) + "...[truncated]";
            if (!isLowSignalBplistToken(token) && seen.insert(token).second) tokens.push_back(token);
        }
        cur.clear();
    };
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (isTokenCharForBplistSummary(c)) {
            cur.push_back(static_cast<char>(c));
            if (cur.size() >= MaxTokenBytes) flush();
        } else {
            flush();
        }
        if (tokens.size() >= MaxTokens) break;
    }
    flush();
    return tokens;
}

std::string buildIosBplistNsKeyedArchiverContext(const std::map<std::string, std::string, std::less<>>& metadata) {
    constexpr std::size_t MaxFields = 3;
    constexpr std::size_t MaxContextBytes = 1800;
    std::vector<std::string> pieces;
    std::size_t fieldsSeen = 0;
    std::size_t keyedFields = 0;
    for (const auto& kv : metadata) {
        if (!isLikelyIosBplistValue(kv.second)) continue;
        ++fieldsSeen;
        if (containsNsKeyedArchiverMarker(kv.second)) ++keyedFields;
        if (pieces.size() >= MaxFields) continue;
        auto tokens = extractPrintableBplistTokens(kv.second);
        std::string tokenText;
        for (const auto& token : tokens) {
            std::string candidate = tokenText.empty() ? token : tokenText + ";" + token;
            if (candidate.size() > 1000) break;
            tokenText = std::move(candidate);
        }
        if (tokenText.empty()) tokenText = "no_printable_tokens_recovered";
        const std::size_t graphBplistOff = kv.second.find("bplist00");
        const std::string graphSample = reconstructBplistTopObjectGraphSampleAt(kv.second, graphBplistOff);
        std::ostringstream os;
        os << kv.first << "[format=" << (containsBinaryPlistMarker(kv.second) ? "bplist00" : "unknown")
           << ";nskeyed=" << (containsNsKeyedArchiverMarker(kv.second) ? "1" : "0")
           << ";raw_bytes=" << kv.second.size()
           << ";" << summarizeBplistTrailer(kv.second);
        if (!graphSample.empty()) os << ";" << graphSample;
        os << ";tokens=" << tokenText << "]";
        pieces.push_back(os.str());
    }
    if (fieldsSeen == 0) return {};
    std::ostringstream out;
    out << "bplist_field_count=" << fieldsSeen << "; nskeyedarchiver_field_count=" << keyedFields
        << "; decode_status=bounded_bplist_object_string_and_top_object_graph_discovery; note=bounded_NSKeyedArchiver_graph_sample_not_full_app_semantics";
    for (const auto& p : pieces) {
        const std::string add = " | " + p;
        if (out.str().size() + add.size() > MaxContextBytes) { out << " | ...[truncated]"; break; }
        out << add;
    }
    return out.str();
}

std::string buildIosSpotlightInvestigatorTextContext(const std::map<std::string, std::string, std::less<>>& metadata) {
    constexpr std::size_t MaxContextBytes = 1200;
    constexpr std::size_t MaxContextPieces = 6;
    std::vector<std::pair<int, std::string>> ranked;
    ranked.reserve(metadata.size());
    for (const auto& kv : metadata) {
        const auto& field = kv.first;
        const auto& value = kv.second;
        if (!isLikelyUsefulSpotlightTextContext(field, value, looksDateField(field))) continue;
        const auto f = lowerAscii(field);
        int rank = 50;
        if (hasAnyToken(f, {"title", "displayname", "filename", "fsname", "subject", "headline"})) rank = 10;
        else if (hasAnyToken(f, {"textcontent", "snippet", "description", "comment"})) rank = 20;
        else if (hasAnyToken(f, {"sender", "recipient", "participant", "author", "account", "handle", "email", "phone", "contact", "person", "jid"})) rank = 30;
        else if (hasAnyToken(f, {"bundleid", "domainidentifier", "activitytype", "container"})) rank = 40;
        ranked.emplace_back(rank, field);
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second < b.second;
    });
    std::string out;
    std::vector<std::string> seenValues;
    std::size_t pieces = 0;
    for (const auto& rf : ranked) {
        auto it = metadata.find(rf.second);
        if (it == metadata.end()) continue;
        std::string v = compactContextText(it->second);
        if (v.empty()) continue;
        if (v.size() > 240) v = v.substr(0, 240) + "...[truncated]";
        if (std::find(seenValues.begin(), seenValues.end(), v) != seenValues.end()) continue;
        seenValues.push_back(v);
        std::string fieldLabel = rf.second;
        if (fieldLabel.rfind("__native_core_probe_string_", 0) == 0) fieldLabel = "__native_core_probe_string";
        std::string piece = fieldLabel + "=" + v;
        if (!out.empty()) piece = " | " + piece;
        if (out.size() + piece.size() > MaxContextBytes) break;
        out += piece;
        if (++pieces >= MaxContextPieces) break;
    }
    return out;
}

bool isRankingOrDerivedDateField(const std::string& fieldLower) {
    if (fieldLower.find("ranking") != std::string::npos) return true;
    if (hasAnyToken(fieldLower, {"dateday", "datehour", "datemonth", "dateweek", "dateweekday", "dateweekdayordinal", "dateweekofmonth", "dateweekofyear", "dateyear"})) return true;
    return false;
}

bool isHighValueDateFieldName(const std::string& fieldLower) {
    if (fieldLower.empty()) return false;
    if (isRankingOrDerivedDateField(fieldLower)) return false;
    if (hasAnyToken(fieldLower, {
        "lastuseddate", "useddates", "downloadeddate", "dateadded", "addeddate",
        "contentcreationdate", "creationdate", "createddate",
        "contentmodificationdate", "modificationdate", "modifieddate",
        "datereceived", "datesent", "startdate", "enddate", "duedate", "completiondate",
        "photoscontentaddeddate", "highlightedcontentserverdate", "relatedactivitylastlaunchdate",
        "last_updated", "lastupdated"
    })) return true;
    // InterestingDate is common and can be useful, but in normal mode it should not
    // crowd out clearer activity/content dates for the same record.
    if (fieldLower.find("interestingdate") != std::string::npos) return true;
    return false;
}

bool shouldPersistDefaultIosDateCandidate(const std::string& field,
                                          const std::string& value,
                                          bool parsedIso,
                                          std::size_t persistedDatesForRecord) {
    (void)value;
    // V0.9.20: normal iOS investigator mode must not create a date row for every
    // CoreSpotlight item.  raw_records.last_updated_utc remains the compact per-record
    // index/update timestamp.  raw_date_candidates is reserved for one clearly high-value
    // activity/content date per record; full date expansion is diagnostics-only.
    constexpr std::size_t MaxDefaultDateCandidatesPerIosRecord = 1;
    if (field == "Last_Updated") return false;
    if (!parsedIso) return false;
    const auto f = lowerAscii(field);
    if (f.find("lastupdated") != std::string::npos || f.find("last_updated") != std::string::npos) return false;
    if (!isHighValueDateFieldName(f)) return false;
    if (persistedDatesForRecord >= MaxDefaultDateCandidatesPerIosRecord) return false;
    return true;
}

bool shouldPersistDefaultIosKeyValue(const std::string& field, const std::string& value, bool isDateField, std::size_t persistedForRecord) {
    // V0.9.20: default iOS CoreSpotlight mode is reference-only for raw_key_values.
    // Display names, generic titles, scores, and date parts are decoded for derived
    // investigator rows but are not materialized one-row-per-property in SQLite.
    constexpr std::size_t MaxDefaultKeyValuesPerIosRecord = 2; // keep two reference rows; value bytes are tightly bounded in V0.9.42
    const auto f = lowerAscii(field);
    if (field == "__decode_error") return true;
    if (persistedForRecord >= MaxDefaultKeyValuesPerIosRecord) return false;
    if (isDateField) return false;
    const bool referenceValue = looksLikeForensicReferenceValue(value);
    if (field.rfind("__native_core_probe_string_", 0) == 0) return referenceValue;
    if (!referenceValue) return false;
    if (isLowSignalNativeField(f)) return false;
    if (hasAnyToken(f, {"wherefrom", "contenturl", "webpage", "url", "path", "container", "domainidentifier", "uniqueidentifier", "persistentidentifier", "targetcontentidentifier", "bundleid", "activitytype", "message", "chat", "sms", "imessage", "author", "sender", "recipient", "participant", "account", "handle", "email", "phone", "contact", "person", "jid"})) return true;
    return true;
}

bool isFatalNativeAbort(const std::exception& ex) {
    const std::string msg = ex.what() ? ex.what() : "";
    return msg.find("SQLite size guardrail hit") != std::string::npos ||
           msg.find("native parser abort") != std::string::npos;
}

std::uintmax_t safeFileSizeBytes(const fs::path& p) {
    try {
        std::error_code ec;
        const auto n = fs::file_size(p, ec);
        return ec ? 0 : n;
    } catch (...) {
        return 0;
    }
}

std::string bytesSummary(std::uintmax_t n) {
    std::ostringstream oss;
    oss << n << " bytes";
    return oss.str();
}

std::string progressClean(std::string s);
void appendNativeProgress(const fs::path& progressPath, int percent, const std::string& stage, const std::string& message);

void appendNativeRunStatus(const fs::path& progressPath, const std::string& stage, const std::string& message) {
    if (progressPath.empty()) return;
    try {
        const auto logDir = progressPath.parent_path();
        const auto caseDir = logDir.parent_path();
        if (caseDir.empty()) return;
        const std::string ts = nowUtc();
        const std::string cleanStage = progressClean(stage);
        const std::string cleanMessage = progressClean(message);
        for (const auto& statusPath : {caseDir / "run_status.txt", logDir / "run_status.txt"}) {
            std::ofstream out(statusPath, std::ios::app | std::ios::binary);
            out << ts << " stage=" << cleanStage << " message=" << cleanMessage << "\n" << std::flush;
        }
        for (const auto& lastStagePath : {caseDir / "last_stage.txt", logDir / "last_stage.txt"}) {
            std::ofstream out(lastStagePath, std::ios::binary);
            out << ts << " " << cleanStage << " " << cleanMessage << "\n" << std::flush;
        }
    } catch (...) {}
}

void checkSqliteSizeGuardrail(CaseDatabase& db,
                              const fs::path& progressPath,
                              Logger& log,
                              std::uintmax_t guardrailBytes,
                              const std::string& context) {
    if (guardrailBytes == 0) return;
    const auto dbPath = db.path();
    const auto dbBytes = safeFileSizeBytes(dbPath);
    const auto walBytes = safeFileSizeBytes(fs::path(pathString(dbPath) + "-wal"));
    const std::string msg = context + " db=" + bytesSummary(dbBytes) + " wal=" + bytesSummary(walBytes) + " guardrail=" + bytesSummary(guardrailBytes);
    appendNativeProgress(progressPath, 74, "sqlite_size_guardrail_check", msg);
    if (dbBytes > guardrailBytes || walBytes > guardrailBytes) {
        appendNativeRunStatus(progressPath, "failed_sqlite_size_guardrail", msg);
        log.error("SQLite size guardrail hit: " + msg);
        throw std::runtime_error("SQLite size guardrail hit: " + msg);
    }
}

std::vector<std::string> extractHighValueProbeStrings(const std::vector<std::uint8_t>& data,
                                                       std::size_t startPos = 0,
                                                       std::size_t endPos = (std::numeric_limits<std::size_t>::max)(),
                                                       std::size_t minLen = 4,
                                                       std::size_t maxLen = 512,
                                                       std::size_t maxStrings = 12) {
    if (startPos > data.size()) startPos = data.size();
    endPos = (std::min)(endPos, data.size());
    if (endPos < startPos) endPos = startPos;
    std::vector<std::string> out;
    std::unordered_set<std::string> seen;
    std::string cur;
    auto flush = [&]() {
        if (cur.size() >= minLen) {
            while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.back()))) cur.pop_back();
            while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.front()))) cur.erase(cur.begin());
            if (isLikelyHighValueProbeString(cur) && seen.insert(cur).second) out.push_back(cur);
        }
        cur.clear();
    };
    for (std::size_t i = startPos; i < endPos; ++i) {
        const auto ch = data[i];
        const bool printable = (ch >= 0x20 && ch <= 0x7Eu) || ch == '\t';
        if (printable) {
            if (cur.size() < maxLen) cur.push_back(static_cast<char>(ch));
            else flush();
        } else if (ch == 0x00u && i + 1 < endPos && data[i + 1] >= 0x20u && data[i + 1] <= 0x7Eu) {
            // UTF-16BE ASCII-ish run: null followed by printable byte. Preserve
            // the printable byte when the next loop iteration sees it.
            continue;
        } else if (ch >= 0xC0u) {
            // Preserve UTF-8 lead bytes in bounded probes instead of discarding
            // all non-ASCII text. Continuation bytes will be appended as the loop
            // advances when they are >= 0x80.
            if (cur.size() < maxLen) cur.push_back(static_cast<char>(ch));
            else flush();
        } else if (ch >= 0x80u && !cur.empty()) {
            if (cur.size() < maxLen) cur.push_back(static_cast<char>(ch));
            else flush();
        } else {
            flush();
            if (out.size() >= maxStrings) break;
        }
    }
    flush();
    if (out.size() > maxStrings) out.resize(maxStrings);
    return out;
}

void addCoreProbeMetadata(ParsedItem& item, const std::vector<std::uint8_t>& data,
                          std::size_t startPos, std::size_t endPos) {
    auto probes = extractHighValueProbeStrings(data, startPos, endPos);
    int i = 1;
    for (const auto& p : probes) {
        std::ostringstream name;
        do {
            name.str("");
            name.clear();
            name << "__native_core_probe_string_" << std::setw(2) << std::setfill('0') << i++;
        } while (item.metadata.find(name.str()) != item.metadata.end());
        const auto primaryName = name.str();
        item.metadata[primaryName] = p;
        const auto aliases = deriveNativeProbeAliases(p, i - 1);
        for (const auto& alias : aliases) {
            if (item.metadata.find(alias.first) == item.metadata.end()) item.metadata[alias.first] = alias.second;
        }
        if (lowerAscii(p).find("/volumes/") != std::string::npos && item.metadata.find("__native_probe_mounted_volume_path") == item.metadata.end()) {
            item.metadata["__native_probe_mounted_volume_path"] = p;
        }
    }
}

void addCoreProbeMetadata(ParsedItem& item, const std::vector<std::uint8_t>& data) {
    addCoreProbeMetadata(item, data, 0, data.size());
}

bool isLikelyIosCoreSpotlightStorePath(const fs::path& p) {
    std::string s = lowerAscii(pathString(p));
    std::replace(s.begin(), s.end(), '\\', '/');
    if (s.find("/corespotlight/") != std::string::npos) return true;
    if (s.find("/var/mobile/library/spotlight/") != std::string::npos) return true;
    if (s.find("nsfileprotectioncomplete") != std::string::npos && s.find("index.spotlightv2") != std::string::npos) return true;
    if (s.find("nsfileprotectioncompleteuntilfirstuserauthentication") != std::string::npos && s.find("index.spotlightv2") != std::string::npos) return true;
    if (s.find("nsfileprotectioncompleteunlessopen") != std::string::npos && s.find("index.spotlightv2") != std::string::npos) return true;
    if (s.find("nsfileprotectioncompletewhenuserinactive") != std::string::npos && s.find("index.spotlightv2") != std::string::npos) return true;
    return false;
}

class MetadataItemParser {
public:
    MetadataItemParser(const std::vector<std::uint8_t>& data,
                       std::size_t startPos,
                       std::size_t endPos,
                       const std::map<int, PropertyDef>& properties,
                       const std::map<int, std::string>& categories,
                       const std::map<int, std::vector<int>>& indexes1,
                       const std::map<int, std::vector<int>>& indexes2)
        : data_(data), startPos_(startPos), pos_(startPos), endPos_((std::min)(endPos, data.size())),
          properties_(properties), categories_(categories), indexes1_(indexes1), indexes2_(indexes2) {
        if (startPos_ > endPos_) startPos_ = pos_ = endPos_;
    }

    MetadataItemParser(std::vector<std::uint8_t> data,
                       const std::map<int, PropertyDef>& properties,
                       const std::map<int, std::string>& categories,
                       const std::map<int, std::vector<int>>& indexes1,
                       const std::map<int, std::vector<int>>& indexes2)
        : ownedData_(std::move(data)), data_(ownedData_), startPos_(0), pos_(0), endPos_(ownedData_.size()),
          properties_(properties), categories_(categories), indexes1_(indexes1), indexes2_(indexes2) {}

    ParsedItem parseV2() {
        ParsedItem item = parseV2HeaderOnly();
        int propIndex = 0;

        while (pos_ < endPos_) {
            try {
                const auto rawPropSkip = readVar("v2 property skip");
                if (rawPropSkip == 0 || rawPropSkip == 0xFFFFFFFFull) break;
                if (rawPropSkip > static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
                    throw std::runtime_error("property skip exceeds int range");
                }

                const int propSkip = static_cast<int>(rawPropSkip);
                if (propIndex > (std::numeric_limits<int>::max)() - propSkip) {
                    throw std::runtime_error("property index overflow");
                }
                propIndex += propSkip;

                auto it = properties_.find(propIndex);
                if (it == properties_.end()) break;

                item.metadata[it->second.name] = readValue(it->second, 0);
            } catch (const std::exception& ex) {
                item.metadata["__decode_error"] = std::string("Partial V2 decode aborted at property index ") +
                                                  std::to_string(propIndex) + ": " + ex.what();
                break;
            }
        }
        return item;
    }

    ParsedItem parseV2CoreOnly() {
        // V0.6.4 safety change: do not walk every Spotlight value payload in
        // core mode. Core mode decodes the stable record header and adds only
        // bounded high-value printable string/path probes from the raw item.
        // Full structured value decoding remains behind --experimental-full-native-values.
        ParsedItem item = parseV2HeaderOnly();
        addCoreProbeMetadata(item, data_, startPos_, endPos_);
        return item;
    }

    ParsedItem parseV1(std::uint64_t rawId) {
        ParsedItem item = parseV1HeaderOnly(rawId);

        // Preserve the original full V1 parser alignment: the full-value V1 path
        // consumed one additional 64-bit value after the header fields before
        // walking typed property records. Header/core-only mode intentionally
        // does not consume this value because it never enters the property loop.
        try {
            if (remaining() >= 8) readUInt64();
        } catch (const std::exception& ex) {
            item.metadata["__decode_error"] = std::string("Partial V1 decode aborted before property loop: ") + ex.what();
            return item;
        }

        while (remaining() >= 12) {
            try {
                const auto dataType = readUInt32();
                const auto propIndex = readUInt32();
                if (propIndex == 0) break;
                if (propIndex > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
                    throw std::runtime_error("V1 property index exceeds int range");
                }

                auto it = properties_.find(static_cast<int>(propIndex));
                if (it == properties_.end()) break;

                const auto dataLen = readUInt32();
                if (static_cast<std::size_t>(dataLen) > remaining()) {
                    throw std::runtime_error("V1 value length exceeds remaining metadata item bytes");
                }
                if (static_cast<std::size_t>(dataLen) > MaxDecodedFieldBytes) {
                    throw std::runtime_error("V1 value length exceeds field byte safety cap");
                }

                item.metadata[it->second.name] = readValueV1(dataType, it->second, dataLen, 0);
            } catch (const std::exception& ex) {
                item.metadata["__decode_error"] = std::string("Partial V1 decode aborted: ") + ex.what();
                break;
            }
        }
        return item;
    }

    ParsedItem parseV1CoreOnly(std::uint64_t rawId) {
        // V0.6.4 safety change mirrors V2: stable header plus bounded raw probes only.
        ParsedItem item = parseV1HeaderOnly(rawId);
        addCoreProbeMetadata(item, data_, startPos_, endPos_);
        return item;
    }

    ParsedItem parseV2HeaderOnly() {
        ParsedItem item;
        item.inodeId = readInt64Var("v2 inode id");
        readByte();
        item.itemId = readInt64Var("v2 item id");
        item.parentId = readInt64Var("v2 parent id");
        item.rawDateUpdated = readVar("v2 raw date updated");
        item.lastUpdatedUtc = epochMicrosToUtc(item.rawDateUpdated);
        return item;
    }

    ParsedItem parseV1HeaderOnly(std::uint64_t rawId) {
        ParsedItem item;
        item.inodeId = static_cast<std::int64_t>(rawId);
        item.rawDateUpdated = readUInt64();
        item.lastUpdatedUtc = epochMicrosToUtc(item.rawDateUpdated);
        if (remaining() >= 8) readUInt64();
        if (remaining() >= 8) item.itemId = static_cast<std::int64_t>(readUInt64());
        item.parentId = 0;
        return item;
    }

private:
    static constexpr int MaxRecursionDepth = 10;

    std::size_t remaining() const { return pos_ <= endPos_ ? endPos_ - pos_ : 0; }

    void requireAvailable(std::size_t n, const char* context) const {
        if (n > remaining()) throw std::runtime_error(std::string(context) + " beyond metadata item limits");
    }

    std::uint64_t readVar(const char* context) {
        std::size_t moved = 0;
        const auto v = readVarSizeNum(data_, pos_, moved);
        if (moved == 0 || moved > remaining()) throw std::runtime_error(std::string(context) + " invalid varint advance");
        pos_ += moved;
        return v;
    }

    std::int64_t readInt64Var(const char* context) {
        const auto v = readVar(context);
        if (v > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
            throw std::runtime_error(std::string(context) + " exceeds int64 range");
        }
        return static_cast<std::int64_t>(v);
    }

    std::size_t readBoundedByteCount(const char* context, std::size_t alignment = 1) {
        const auto raw = readVar(context);
        if (raw > static_cast<std::uint64_t>(MaxDecodedFieldBytes)) throw std::runtime_error(std::string(context) + " exceeds field byte safety cap");
        const auto n = static_cast<std::size_t>(raw);
        if (n > remaining()) throw std::runtime_error(std::string(context) + " exceeds remaining metadata item bytes");
        if (alignment > 1 && (n % alignment) != 0) throw std::runtime_error(std::string(context) + " has invalid byte alignment");
        return n;
    }

    void checkDepth(int depth) const {
        if (depth > MaxRecursionDepth) throw std::runtime_error("exceeded maximum metadata value recursion depth");
    }

    std::uint8_t readByte() { requireAvailable(1, "read byte"); return data_[pos_++]; }
    std::uint32_t readUInt32() { requireAvailable(4, "read u32"); auto v = readU32(data_, pos_); pos_ += 4; return v; }
    std::uint64_t readUInt64() { requireAvailable(8, "read u64"); auto v = readU64(data_, pos_); pos_ += 8; return v; }
    float readFloat() { requireAvailable(4, "read float"); auto v = readFloatLe(data_, pos_); pos_ += 4; return v; }
    double readDouble() { requireAvailable(8, "read double"); auto v = readDoubleLe(data_, pos_); pos_ += 8; return v; }

    std::vector<std::uint8_t> readBytes(std::size_t n) {
        if (n > MaxDecodedFieldBytes) throw std::runtime_error("decoded field exceeds byte safety cap");
        requireAvailable(n, "read bytes");
        std::vector<std::uint8_t> b(data_.begin() + static_cast<std::ptrdiff_t>(pos_), data_.begin() + static_cast<std::ptrdiff_t>(pos_ + n));
        pos_ += n; return b;
    }

    std::string readMacAbsoluteDate() { return macAbsoluteDateToUtc(readDouble()); }

    std::string readValue(const PropertyDef& prop, int depth) {
        checkDepth(depth);
        switch (prop.valueType) {
            case 0x00:
            case 0x02: { auto v = readVar("unsigned value"); return std::to_string(v); }
            case 0x06: return readSimpleOrPair(prop.propType, depth + 1);
            case 0x07: return readSignedOrArray(prop.propType, depth + 1);
            case 0x08: return readSpecial8(prop.propType, depth + 1);
            case 0x09: return readFloatOrArray(prop.propType, depth + 1);
            case 0x0A: return readDoubleOrArray(prop.propType, depth + 1);
            case 0x0B: return readStringOrStrings(prop.propType, depth + 1);
            case 0x0C: return readDateOrDates(prop.propType, depth + 1);
            case 0x0E: return readBinaryOrUtf8(prop, depth + 1);
            case 0x0F: return readCategoryValue(prop, depth + 1);
            default: return {};
        }
    }

    std::string readValueV1(std::uint32_t dataType, const PropertyDef&, std::uint32_t dataLen, int depth) {
        checkDepth(depth);
        if (static_cast<std::size_t>(dataLen) > remaining()) throw std::runtime_error("V1 value length exceeds remaining metadata item bytes");
        if (static_cast<std::size_t>(dataLen) > MaxDecodedFieldBytes) throw std::runtime_error("V1 value length exceeds field byte safety cap");

        switch (dataType & 0xFu) {
            case 0x01: return dataLen == 1 ? (readByte() ? "true" : "false") : skip(dataLen);
            case 0x02: return dataLen == 1 ? std::to_string(readByte()) : skip(dataLen);
            case 0x06: return dataLen == 4 ? std::to_string(readUInt32()) : skip(dataLen);
            case 0x07: {
                if ((dataLen % 8u) != 0u) throw std::runtime_error("V1 signed integer array has invalid byte alignment");
                if (dataLen == 8) return std::to_string(static_cast<std::int64_t>(readUInt64()));
                const auto count = static_cast<std::size_t>(dataLen / 8u);
                if (count > MaxDecodedArrayValues) throw std::runtime_error("V1 signed integer array exceeds value safety cap");
                std::vector<std::int64_t> values; values.reserve(count);
                for (std::size_t i = 0; i < count; ++i) values.push_back(static_cast<std::int64_t>(readUInt64()));
                return joinNums(values);
            }
            case 0x0A: {
                if ((dataLen % 8u) != 0u) throw std::runtime_error("V1 double array has invalid byte alignment");
                if (dataLen == 8) { std::ostringstream os; os << readDouble(); return os.str(); }
                const auto count = static_cast<std::size_t>(dataLen / 8u);
                if (count > MaxDecodedArrayValues) throw std::runtime_error("V1 double array exceeds value safety cap");
                std::vector<double> values; values.reserve(count);
                for (std::size_t i = 0; i < count; ++i) values.push_back(readDouble());
                return joinNums(values);
            }
            case 0x0B: return join(splitNullUtf8(readBytes(dataLen)));
            case 0x0C: {
                if ((dataLen % 8u) != 0u) throw std::runtime_error("V1 date array has invalid byte alignment");
                if (dataLen == 8) return readMacAbsoluteDate();
                const auto count = static_cast<std::size_t>(dataLen / 8u);
                if (count > MaxDecodedArrayValues) throw std::runtime_error("V1 date array exceeds value safety cap");
                std::vector<std::string> dates; dates.reserve(count);
                for (std::size_t i = 0; i < count; ++i) dates.push_back(readMacAbsoluteDate());
                return join(dates);
            }
            case 0x0E: return bytesToHex(readBytes(dataLen));
            default: return skip(dataLen);
        }
    }

    std::string skip(std::uint32_t n) {
        const auto nn = static_cast<std::size_t>(n);
        requireAvailable(nn, "skip value");
        pos_ += nn;
        return {};
    }

    std::string readSimpleOrPair(std::uint8_t propType, int depth) {
        checkDepth(depth);
        if ((propType & 0x02u) == 0x02u) {
            std::vector<std::uint64_t> a;
            for (int i = 0; i < 2; ++i) a.push_back(readVar("simple pair value"));
            return joinNums(a);
        }
        auto v = readVar("simple value"); return std::to_string(v);
    }

    std::string readSignedOrArray(std::uint8_t propType, int depth) {
        checkDepth(depth);
        if ((propType & 0x02u) == 0x02u) {
            auto number = static_cast<std::uint64_t>(readVar("signed array count"));
            std::size_t numValues = static_cast<std::size_t>(number >> 3);
            if (numValues > MaxDecodedArrayValues) throw std::runtime_error("signed array exceeds value safety cap");
            std::vector<std::int64_t> values;
            values.reserve(numValues);
            for (std::size_t i = 0; i < numValues; ++i) values.push_back(static_cast<std::int64_t>(readVar("signed array value")));
            return joinNums(values);
        }
        auto v = static_cast<std::int64_t>(readVar("signed value")); return std::to_string(v);
    }

    std::string readSpecial8(std::uint8_t propType, int depth) {
        checkDepth(depth);
        if ((propType & 0x02u) == 0x02u) {
            std::vector<std::uint64_t> a;
            for (int i = 0; i < 4; ++i) a.push_back(readVar("special tuple value"));
            return joinNums(a);
        }
        auto v = readVar("special value"); return std::to_string(v);
    }

    std::string readFloatOrArray(std::uint8_t propType, int depth) {
        checkDepth(depth);
        if ((propType & 0x02u) == 0x02u) { auto byteCount = readBoundedByteCount("float array byte count", 4); const auto count = byteCount / 4; if (count > MaxDecodedArrayValues) throw std::runtime_error("float array exceeds value safety cap"); std::vector<float> a; a.reserve(count); for (std::size_t i = 0; i < count; ++i) a.push_back(readFloat()); return joinNums(a); }
        std::ostringstream os; os << readFloat(); return os.str();
    }

    std::string readDoubleOrArray(std::uint8_t propType, int depth) {
        checkDepth(depth);
        if ((propType & 0x02u) == 0x02u) { auto byteCount = readBoundedByteCount("double array byte count", 8); const auto count = byteCount / 8; if (count > MaxDecodedArrayValues) throw std::runtime_error("double array exceeds value safety cap"); std::vector<double> a; a.reserve(count); for (std::size_t i = 0; i < count; ++i) a.push_back(readDouble()); return joinNums(a); }
        std::ostringstream os; os << readDouble(); return os.str();
    }

    std::string readStringOrStrings(std::uint8_t propType, int depth) {
        checkDepth(depth);
        auto size = readBoundedByteCount("string byte count");
        auto values = splitNullUtf8(readBytes(size));
        if ((propType & 0x02u) == 0x02u) return join(values);
        return values.empty() ? std::string() : values.size() == 1 ? values[0] : join(values);
    }

    std::string readDateOrDates(std::uint8_t propType, int depth) {
        checkDepth(depth);
        if ((propType & 0x02u) == 0x02u) { auto byteCount = readBoundedByteCount("date array byte count", 8); const auto count = byteCount / 8; if (count > MaxDecodedArrayValues) throw std::runtime_error("date array exceeds value safety cap"); std::vector<std::string> a; a.reserve(count); for (std::size_t i = 0; i < count; ++i) a.push_back(readMacAbsoluteDate()); return join(a); }
        return readMacAbsoluteDate();
    }

    std::string readBinaryOrUtf8(const PropertyDef& prop, int depth) {
        checkDepth(depth);
        const bool binaryOnly = (prop.propType & 0x80u) == 0x80u;
        const bool multi = (prop.propType & 0x02u) == 0x02u;
        auto size = readBoundedByteCount("binary/string byte count");
        auto b = readBytes(size);
        if (binaryOnly) {
            if (!b.empty()) b.pop_back();
            return bytesToHex(b);
        }
        if (b.size() >= 2 && b[b.size() - 2] == 0x16 && b[b.size() - 1] == 0x02) b.resize(b.size() - 2);
        return multi ? join(splitNullUtf8(b)) : cleanDecodedString(utf8FromBytes(b));
    }

    std::string readCategoryValue(const PropertyDef& prop, int depth) {
        checkDepth(depth);
        const auto rawValue = readVar("category value");
        if (rawValue > static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) throw std::runtime_error("category value exceeds int range");
        int value = static_cast<int>(rawValue);
        if (value < 0) return value == -16777217 ? std::string() : "INVALID";
        if ((prop.propType & 0x03u) == 0x03u) {
            auto ii = indexes2_.find(value); if (ii != indexes2_.end()) for (int id : ii->second) { auto c = categories_.find(id); if (c != categories_.end()) return c->second; }
            return {};
        }
        if ((prop.propType & 0x02u) == 0x02u) {
            std::vector<std::string> tree; auto ii = indexes1_.find(value); if (ii != indexes1_.end()) for (int id : ii->second) { auto c = categories_.find(id); if (c != categories_.end()) tree.push_back(c->second); }
            return join(tree);
        }
        auto c = categories_.find(value); return c == categories_.end() ? std::string() : c->second;
    }

    std::vector<std::uint8_t> ownedData_;
    const std::vector<std::uint8_t>& data_;
    std::size_t startPos_ = 0;
    std::size_t pos_ = 0;
    std::size_t endPos_ = 0;
    const std::map<int, PropertyDef>& properties_;
    const std::map<int, std::string>& categories_;
    const std::map<int, std::vector<int>>& indexes1_;
    const std::map<int, std::vector<int>>& indexes2_;
};


std::vector<ParsedItem> parseMetadataItems(const std::vector<std::uint8_t>& payload,
                                           int version,
                                           const std::map<int, PropertyDef>& properties,
                                           const std::map<int, std::string>& categories,
                                           const std::map<int, std::vector<int>>& indexes1,
                                           const std::map<int, std::vector<int>>& indexes2) {
    std::vector<ParsedItem> items;
    std::size_t pos = 0;
    if (version == 1) {
        while (pos + 16 <= payload.size()) {
            const auto rawId = readU64(payload, pos);
            const auto itemSize2 = readI32(payload, pos + 12);
            if (itemSize2 <= 16 || static_cast<std::size_t>(itemSize2) > MaxMetadataItemBytes || pos + static_cast<std::size_t>(itemSize2) > payload.size()) break;
            std::vector<std::uint8_t> itemData(payload.begin() + static_cast<std::ptrdiff_t>(pos + 16), payload.begin() + static_cast<std::ptrdiff_t>(pos + static_cast<std::size_t>(itemSize2)));
            MetadataItemParser parser(std::move(itemData), properties, categories, indexes1, indexes2);
            items.push_back(parser.parseV1(rawId));
            pos += static_cast<std::size_t>(itemSize2);
        }
    } else {
        while (pos + 4 <= payload.size()) {
            const auto itemSize = readI32(payload, pos);
            if (itemSize <= 0 || static_cast<std::size_t>(itemSize) > MaxMetadataItemBytes || pos + 4 + static_cast<std::size_t>(itemSize) > payload.size()) break;
            std::vector<std::uint8_t> itemData(payload.begin() + static_cast<std::ptrdiff_t>(pos + 4), payload.begin() + static_cast<std::ptrdiff_t>(pos + 4 + static_cast<std::size_t>(itemSize)));
            MetadataItemParser parser(std::move(itemData), properties, categories, indexes1, indexes2);
            items.push_back(parser.parseV2());
            pos += 4 + static_cast<std::size_t>(itemSize);
        }
    }
    return items;
}

std::string meta(const ParsedItem& item, const std::string& key) {
    auto it = item.metadata.find(key);
    return it == item.metadata.end() ? std::string() : it->second;
}

std::string firstMetadataValue(const ParsedItem& item, std::initializer_list<const char*> keys) {
    for (const auto* key : keys) {
        auto v = cleanDecodedString(meta(item, key));
        if (!trimAscii(v).empty()) return v;
    }
    return {};
}

std::string basenameFromSpotlightPath(std::string p) {
    p = cleanDecodedString(trimAscii(std::move(p)));
    if (p.empty() || p == "/") return {};
    while (!p.empty() && (p.back() == '/' || p.back() == '\\')) p.pop_back();
    const auto pos = p.find_last_of("/\\");
    std::string base = (pos == std::string::npos) ? p : p.substr(pos + 1);
    base = cleanDecodedString(trimAscii(std::move(base)));
    if (base.empty() || base == "." || base == "..") return {};
    return base;
}

std::string metadataFullPathOf(const ParsedItem& item) {
    auto p = firstMetadataValue(item, {
        "kMDItemPath", "_kMDItemPath", "kMDItemFilePath", "_kMDItemFilePath",
        "kMDItemURL", "kMDItemContentURL", "contentURL", "URL", "path",
        "__native_probe_file_path_candidate_01", "__native_probe_file_path_candidate_02", "__native_probe_file_path_candidate_03",
        "__native_probe_file_path_candidate_04", "__native_probe_file_path_candidate_05", "__native_probe_file_path_candidate_06"
    });
    p = trimAscii(p);
    if (p.empty()) return {};
    const auto lower = lowerAscii(p);
    if (lower.rfind("file://", 0) == 0) p = p.substr(7);
    return p;
}

std::string displayNameOf(const ParsedItem& item) {
    return firstMetadataValue(item, {
        "kMDItemDisplayName", "_kMDItemDisplayName", "displayName", "title",
        "kMDItemTitle", "_kMDItemTitle", "kMDItemFSName", "_kMDItemFSName", "_kMDItemFileName",
        "__native_probe_basename_candidate_01", "__native_probe_basename_candidate_02", "__native_probe_basename_candidate_03",
        "__native_probe_basename_candidate_04", "__native_probe_basename_candidate_05", "__native_probe_basename_candidate_06"
    });
}

std::string fileNameOf(const ParsedItem& item) {
    if (item.metadata.find("_kStoreMetadataVersion") != item.metadata.end()) return "------PLIST------";
    auto name = firstMetadataValue(item, {
        "_kMDItemFileName", "kMDItemFSName", "_kMDItemFSName", "kMDItemFileName",
        "kMDItemDisplayName", "_kMDItemDisplayName", "kMDItemTitle", "_kMDItemTitle",
        "displayName", "title", "name",
        "__native_probe_basename_candidate_01", "__native_probe_basename_candidate_02", "__native_probe_basename_candidate_03",
        "__native_probe_basename_candidate_04", "__native_probe_basename_candidate_05", "__native_probe_basename_candidate_06"
    });
    if (name.empty()) name = basenameFromSpotlightPath(metadataFullPathOf(item));
    return name.empty() ? "------NONAME------" : name;
}

std::string contentTypeOf(const ParsedItem& item) {
    return firstMetadataValue(item, {"kMDItemContentType", "_kMDItemContentType", "contentType"});
}

std::string contentTypeTreeOf(const ParsedItem& item) {
    return firstMetadataValue(item, {"kMDItemContentTypeTree", "_kMDItemContentTypeTree", "contentTypeTree"});
}

std::string whereFromsOf(const ParsedItem& item) {
    return firstMetadataValue(item, {
        "kMDItemWhereFroms", "_kMDItemWhereFroms", "whereFroms",
        "__native_probe_url_candidate_01", "__native_probe_url_candidate_02", "__native_probe_url_candidate_03",
        "__native_probe_url_candidate_04", "__native_probe_url_candidate_05", "__native_probe_url_candidate_06"
    });
}

std::string logicalSizeOf(const ParsedItem& item) {
    return firstMetadataValue(item, {"kMDItemFSSize", "_kMDItemFSSize", "kMDItemLogicalSize", "_kMDItemLogicalSize", "kMDItemFileSize", "fileSize"});
}

std::string physicalSizeOf(const ParsedItem& item) {
    return firstMetadataValue(item, {"kMDItemPhysicalSize", "_kMDItemPhysicalSize", "physicalSize"});
}
std::string reconstructPath(std::int64_t inode, const std::unordered_map<std::int64_t, std::pair<std::int64_t, std::string>>& nodes) {
    auto it0 = nodes.find(inode);
    if (it0 == nodes.end()) return {};
    if (inode == 2) return "/";
    std::vector<std::string> parts;
    std::int64_t current = inode;
    std::set<std::int64_t> seen;
    while (true) {
        auto it = nodes.find(current);
        if (it == nodes.end()) break;
        if (!seen.insert(current).second) break;
        const auto& name = it->second.second;
        if (!name.empty() && name != "------NONAME------" && name != "------PLIST------") parts.insert(parts.begin(), name);
        if (current == 2 || it->second.first == 0 || it->second.first == 2) break;
        current = it->second.first;
    }
    if (parts.empty()) return {};
    return "/" + join(parts, "/");
}

void insertFailure(CaseDatabase& db, const EvidenceSource& source, const StoreInfo& store, const std::string& phase, const std::string& message) {
    auto st = db.prepare("INSERT INTO raw_failures(source_id,phase,store_guid,source_db,message,created_utc) VALUES(?,?,?,?,?,?)");
    st.bind(1, source.sourceId); st.bind(2, phase); st.bind(3, store.storeGuid); st.bind(4, pathString(store.storePath)); st.bind(5, message); st.bind(6, nowUtc()); st.stepDone();
}

void insertStoreGroup(CaseDatabase& db, const EvidenceSource& source, const StoreInfo& store) {
    auto st = db.prepare("INSERT OR IGNORE INTO store_groups(source_id,store_guid,store_path,store_file_count,has_store_db,has_dotstore_db) VALUES(?,?,?,?,?,?)");
    st.bind(1, source.sourceId); st.bind(2, store.storeGuid); st.bind(3, pathString(store.storePath.parent_path())); st.bind(4, 0LL);
    st.bind(5, lowerAscii(store.storePath.filename().string()) == "store.db" ? 1LL : 0LL);
    st.bind(6, lowerAscii(store.storePath.filename().string()) == ".store.db" ? 1LL : 0LL);
    st.stepDone();
}


std::string progressClean(std::string s) {
    for (char& ch : s) {
        if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    }
    if (s.size() > 900) s = s.substr(0, 900) + "...";
    return s;
}

void appendNativeProgress(const fs::path& progressPath, int percent, const std::string& stage, const std::string& message) {
    if (progressPath.empty()) return;
    try {
        fs::create_directories(progressPath.parent_path());
        const std::string ts = nowUtc();
        const std::string cleanStage = progressClean(stage);
        const std::string cleanMessage = progressClean(message);
        auto writeProgressPair = [&](const fs::path& path) {
            if (path.empty()) return;
            fs::create_directories(path.parent_path());
            std::ofstream out(path, std::ios::app | std::ios::binary);
            out << ts << "\t" << percent << "\t" << cleanStage << "\t" << cleanMessage << "\n" << std::flush;
            std::ofstream last(path.parent_path() / "last_progress.tsv", std::ios::binary);
            last << ts << "\t" << percent << "\t" << cleanStage << "\t" << cleanMessage << "\n" << std::flush;
        };
        writeProgressPair(progressPath);
        const fs::path parent = progressPath.parent_path();
        if (parent.filename().string() == "logs" && !parent.parent_path().empty()) {
            writeProgressPair(parent.parent_path() / progressPath.filename());
        }
    } catch (...) {}
}

} // namespace

NativeStoreDbParseCounts NativeStoreDbParser::parseStores(const std::vector<StoreInfo>& stores,
                                                          const EvidenceSource& source,
                                                          CaseDatabase& db,
                                                          Logger& log) {
    NativeStoreDbParseCounts counts;
    counts.storesSeen = stores.size();
    appendNativeProgress(progressPath_, 35, "native_parse_start", "stores=" + std::to_string(stores.size()));
    log.info("Native parser entered parseStores. stores_seen=" + std::to_string(counts.storesSeen));
    db.begin();
    log.info("Native parser SQLite transaction opened.");
    try {
        auto recStmt = db.prepare("INSERT INTO raw_records(source_id,store_guid,store_path,source_db,inode_num,store_id,parent_inode_num,flags,last_updated_raw,last_updated_utc,file_name,content_type,content_type_tree,where_froms,display_name,full_path,record_state,logical_size_bytes,physical_size_bytes) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        auto kvStmt = db.prepare("INSERT INTO raw_key_values(source_id,store_guid,store_path,source_db,inode_num,store_id,parent_inode_num,full_path,record_state,field_name,field_value) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
        auto dateStmt = db.prepare("INSERT INTO raw_date_candidates(source_id,store_guid,store_path,source_db,inode_num,store_id,field_name,field_value,parsed_utc,parse_method) VALUES(?,?,?,?,?,?,?,?,?,?)");

        auto insertItem = [&](const StoreInfo& store, const ParsedItem& item, const std::string& fullPath) {
            const auto inode = std::to_string(item.inodeId);
            const auto parent = std::to_string(item.parentId);
            const auto fileName = fileNameOf(item);
            const auto metadataPath = metadataFullPathOf(item);
            const auto effectiveFullPath = !fullPath.empty() ? fullPath : metadataPath;
            const auto recordState = effectiveFullPath.empty() ? std::string("PARTIAL_OR_NO_PATH") : std::string("ACTIVE_OR_RESOLVED");
            recStmt.bind(1, source.sourceId); recStmt.bind(2, store.storeGuid); recStmt.bind(3, pathString(store.storePath.parent_path())); recStmt.bind(4, pathString(store.storePath));
            recStmt.bind(5, inode); recStmt.bind(6, std::to_string(item.itemId)); recStmt.bind(7, parent); recStmt.bind(8, ""); recStmt.bind(9, std::to_string(item.rawDateUpdated)); recStmt.bind(10, item.lastUpdatedUtc);
            recStmt.bind(11, fileName); recStmt.bind(12, contentTypeOf(item)); recStmt.bind(13, contentTypeTreeOf(item)); recStmt.bind(14, whereFromsOf(item)); recStmt.bind(15, cleanDecodedString(displayNameOf(item)));
            recStmt.bind(16, effectiveFullPath); recStmt.bind(17, recordState); recStmt.bind(18, logicalSizeOf(item)); recStmt.bind(19, physicalSizeOf(item)); recStmt.stepDone(); recStmt.reset();
            ++counts.rawRecords;

            const bool iosPathSensitiveMatch = isLikelyIosCoreSpotlightStorePath(store.storePath);
            const bool iosDefaultFilteredKvMode = !persistAllNativeKeyValues_ &&
                (nativePersistenceMode_ == NativePersistenceMode::IosCoreSpotlightCompact ||
                 (nativePersistenceMode_ == NativePersistenceMode::AutoPathSensitive && iosPathSensitiveMatch));
            std::size_t persistedKeyValuesForRecord = 0;
            std::size_t persistedDateCandidatesForRecord = 0;
            bool persistedReferenceForRecord = false;
            const std::string spotlightTextContext = iosDefaultFilteredKvMode ? buildIosSpotlightInvestigatorTextContext(item.metadata) : std::string();
            for (const auto& [field, value] : item.metadata) {
                const bool dateField = looksDateField(field);
                const bool persistKeyValue = !iosDefaultFilteredKvMode || shouldPersistDefaultIosKeyValue(field, value, dateField, persistedKeyValuesForRecord);
                if (persistKeyValue) {
                    const std::string storedValue = iosDefaultFilteredKvMode ? trimStoredNativeValue(field, value) : value;
                    kvStmt.bind(1, source.sourceId); kvStmt.bind(2, store.storeGuid); kvStmt.bind(3, pathString(store.storePath.parent_path())); kvStmt.bind(4, pathString(store.storePath)); kvStmt.bind(5, inode); kvStmt.bind(6, std::to_string(item.itemId)); kvStmt.bind(7, parent); kvStmt.bind(8, effectiveFullPath); kvStmt.bind(9, recordState); kvStmt.bind(10, field); kvStmt.bind(11, storedValue); kvStmt.stepDone(); kvStmt.reset();
                    ++counts.rawKeyValues;
                    ++persistedKeyValuesForRecord;
                    if (iosDefaultFilteredKvMode && looksLikeForensicReferenceValue(value)) persistedReferenceForRecord = true;
                }
                if (dateField) {
                    const auto dateValues = splitSemicolonDateList(value);
                    for (const auto& dateValue : dateValues) {
                        const bool parsedIso = isLikelyIsoDate(dateValue);
                        const bool persistDate = !iosDefaultFilteredKvMode || shouldPersistDefaultIosDateCandidate(field, dateValue, parsedIso, persistedDateCandidatesForRecord);
                        if (!persistDate) continue;
                        dateStmt.bind(1, source.sourceId); dateStmt.bind(2, store.storeGuid); dateStmt.bind(3, pathString(store.storePath.parent_path())); dateStmt.bind(4, pathString(store.storePath)); dateStmt.bind(5, inode); dateStmt.bind(6, std::to_string(item.itemId)); dateStmt.bind(7, field); dateStmt.bind(8, dateValue); dateStmt.bind(9, parsedIso ? dateValue : ""); dateStmt.bind(10, parsedIso ? "native_iso" : "native_candidate"); dateStmt.stepDone(); dateStmt.reset();
                        ++counts.rawDateCandidates;
                        ++persistedDateCandidatesForRecord;
                    }
                }
            }
            if (iosDefaultFilteredKvMode && persistedReferenceForRecord && !spotlightTextContext.empty()) {
                kvStmt.bind(1, source.sourceId); kvStmt.bind(2, store.storeGuid); kvStmt.bind(3, pathString(store.storePath.parent_path())); kvStmt.bind(4, pathString(store.storePath)); kvStmt.bind(5, inode); kvStmt.bind(6, std::to_string(item.itemId)); kvStmt.bind(7, parent); kvStmt.bind(8, effectiveFullPath); kvStmt.bind(9, recordState); kvStmt.bind(10, "__spotlight_investigator_text_context"); kvStmt.bind(11, spotlightTextContext); kvStmt.stepDone(); kvStmt.reset();
                ++counts.rawKeyValues;
            }
            if (iosDefaultFilteredKvMode) {
                const std::string bplistContext = buildIosBplistNsKeyedArchiverContext(item.metadata);
                if (!bplistContext.empty()) {
                    kvStmt.bind(1, source.sourceId); kvStmt.bind(2, store.storeGuid); kvStmt.bind(3, pathString(store.storePath.parent_path())); kvStmt.bind(4, pathString(store.storePath)); kvStmt.bind(5, inode); kvStmt.bind(6, std::to_string(item.itemId)); kvStmt.bind(7, parent); kvStmt.bind(8, effectiveFullPath); kvStmt.bind(9, recordState); kvStmt.bind(10, "__spotlight_bplist_nskeyedarchiver_context"); kvStmt.bind(11, bplistContext); kvStmt.stepDone(); kvStmt.reset();
                    ++counts.rawKeyValues;
                }
            }
            if (!item.lastUpdatedUtc.empty() && !iosDefaultFilteredKvMode) {
                dateStmt.bind(1, source.sourceId); dateStmt.bind(2, store.storeGuid); dateStmt.bind(3, pathString(store.storePath.parent_path())); dateStmt.bind(4, pathString(store.storePath)); dateStmt.bind(5, inode); dateStmt.bind(6, std::to_string(item.itemId)); dateStmt.bind(7, "Last_Updated"); dateStmt.bind(8, item.lastUpdatedUtc); dateStmt.bind(9, item.lastUpdatedUtc); dateStmt.bind(10, "native_epoch_microseconds"); dateStmt.stepDone(); dateStmt.reset();
                ++counts.rawDateCandidates;
            }
            ++counts.parsedItems;
        };

        std::size_t storeOrdinal = 0;
        const std::size_t totalStores = stores.empty() ? 1 : stores.size();
        for (const auto& store : stores) {
            ++storeOrdinal;
            const std::string storeAttemptStartedUtc = nowUtc();
            appendNativeProgress(progressPath_, 36, "native_parse_store_start", "store=" + std::to_string(storeOrdinal) + "/" + std::to_string(totalStores) + " guid=" + store.storeGuid);
            appendNativeProgress(progressPath_, 36, "native_parse_store_persistence_mode",
                                 std::string("mode=") +
                                 (nativePersistenceMode_ == NativePersistenceMode::MacOSStoreV2 ? "macos_storev2" :
                                  (nativePersistenceMode_ == NativePersistenceMode::IosCoreSpotlightCompact ? "ios_corespotlight_compact" : "auto_path_sensitive")) +
                                 "; path_looks_ios=" + (isLikelyIosCoreSpotlightStorePath(store.storePath) ? "1" : "0"));
            const std::size_t beforeMetadataBlocks = counts.metadataBlocks;
            const std::size_t beforeDecompressedBlocks = counts.decompressedBlocks;
            const std::size_t beforeRawRecords = counts.rawRecords;
            const std::size_t beforeRawKeyValues = counts.rawKeyValues;
            const std::size_t beforeRawDateCandidates = counts.rawDateCandidates;
            const std::size_t beforeFallbackHeaderOnlyItems = counts.fallbackHeaderOnlyItems;
            const std::size_t beforeFailures = counts.failures;
            int attemptSpotlightVersion = 0;
            std::size_t attemptPropertiesCount = 0;
            std::size_t attemptCategoriesCount = 0;
            insertStoreGroup(db, source, store);
            if (!store.isValid) {
                ++counts.failures;
                const std::string msg = store.validationError.empty() ? "invalid store" : store.validationError;
                insertFailure(db, source, store, "header", msg);
                insertNativeDecodeAttempt(db, source, store, decodeMode_, attemptSpotlightVersion, attemptPropertiesCount, attemptCategoriesCount,
                                          counts.metadataBlocks - beforeMetadataBlocks, counts.decompressedBlocks - beforeDecompressedBlocks,
                                          counts.rawRecords - beforeRawRecords, counts.rawKeyValues - beforeRawKeyValues,
                                          counts.rawDateCandidates - beforeRawDateCandidates, counts.fallbackHeaderOnlyItems - beforeFallbackHeaderOnlyItems,
                                          counts.failures - beforeFailures, storeAttemptStartedUtc, "FAILED_INVALID_STORE", msg);
                continue;
            }
            log.info("Native parser starting store: " + pathString(store.storePath) + " bytes=" + std::to_string(store.fileSizeBytes));
            try {
                const auto h = readHeader(store);
                attemptSpotlightVersion = h.version;
                if (!h.isValid) {
                    ++counts.failures;
                    insertFailure(db, source, store, "header", h.validationError);
                    insertNativeDecodeAttempt(db, source, store, decodeMode_, attemptSpotlightVersion, attemptPropertiesCount, attemptCategoriesCount,
                                              counts.metadataBlocks - beforeMetadataBlocks, counts.decompressedBlocks - beforeDecompressedBlocks,
                                              counts.rawRecords - beforeRawRecords, counts.rawKeyValues - beforeRawKeyValues,
                                              counts.rawDateCandidates - beforeRawDateCandidates, counts.fallbackHeaderOnlyItems - beforeFallbackHeaderOnlyItems,
                                              counts.failures - beforeFailures, storeAttemptStartedUtc, "FAILED_HEADER", h.validationError);
                    continue;
                }
                ++counts.validStores;
                std::ifstream in(store.storePath, std::ios::binary);
                if (!in) throw std::runtime_error("unable to open store for native parsing");
                const auto block0 = readBlock0(in, h);
                log.info("Block0 entries=" + std::to_string(block0.size()) + " for " + pathString(store.storePath));
                appendNativeProgress(progressPath_, 36, "native_parse_store_blocks", "store=" + std::to_string(storeOrdinal) + "/" + std::to_string(totalStores) + " blocks=" + std::to_string(block0.size()) + " guid=" + store.storeGuid);
                std::map<int, PropertyDef> properties;
                std::map<int, std::string> categories;
                std::map<int, std::vector<int>> indexes1, indexes2;
                std::vector<DbStrMapLoadResult> dbStrResults;
                if (h.version == 1) {
                    parseBlockSequence(in, h, h.idx03, BlockType::StringsV1, [&](const auto& raw, int logical){ parsePropertiesV1(raw, properties, logical); });
                } else if (shouldLoadExternalDbStrMaps(h, store)) {
                    dbStrResults = loadExternalDbStrMapsForStore(store.storePath.parent_path(), properties, categories, indexes1, indexes2);
                    insertNativeDbStrMapInventory(db, source, store, dbStrResults);
                    std::ostringstream dbStrMsg;
                    for (const auto& r : dbStrResults) {
                        if (dbStrMsg.tellp() > 0) dbStrMsg << "; ";
                        dbStrMsg << "dbStr-" << r.mapId << "=" << r.status << ":" << r.parsedEntries << "/" << r.offsetEntries;
                    }
                    log.info("External Store-V2 dbStr map load: store=" + pathString(store.storePath) + " " + dbStrMsg.str());
                } else {
                    parseBlockSequence(in, h, h.idx11, BlockType::Property, [&](const auto& raw, int logical){ parsePropertiesV2(raw, properties, logical); });
                    parseBlockSequence(in, h, h.idx21, BlockType::Category, [&](const auto& raw, int logical){ parseCategories(raw, categories, logical); });
                    parseBlockSequence(in, h, h.idx81_1, BlockType::Index, [&](const auto& raw, int logical){ parseIndexes(raw, indexes1, logical); });
                    parseBlockSequence(in, h, h.idx81_2, BlockType::Index, [&](const auto& raw, int logical){ parseIndexes(raw, indexes2, logical); });
                }
                attemptPropertiesCount = properties.size();
                attemptCategoriesCount = categories.size();
                insertNativePropertyDictionary(db, source, store, h.version, properties);
                insertNativeCategoryDictionary(db, source, store, h.version, categories);
                insertNativeIndexDictionarySummary(db, source, store, h.version, "index_1", indexes1);
                insertNativeIndexDictionarySummary(db, source, store, h.version, "index_2", indexes2);
                if (shouldLoadExternalDbStrMaps(h, store) && dbStrResults.empty()) {
                    insertFailure(db, source, store, "external_dbstr_maps", "Store-V2 header idx11=0 but external dbStr map loading was not attempted");
                }
                log.info("Dictionaries: properties=" + std::to_string(properties.size()) + " categories=" + std::to_string(categories.size()) +
                         " index1=" + std::to_string(indexes1.size()) + " index2=" + std::to_string(indexes2.size()) +
                         " native_property_dictionary_rows_written=" + std::to_string(properties.size()));
                if (decodeMode_ == NativeDecodeMode::HeaderOnly) {
                    log.warn("Native decoder is running in conservative header-only mode. Metadata key/value decoding is disabled by default until the native value parser is fully stabilized.");
                    insertFailure(db, source, store, "native_decoder_notice", "conservative_header_only_mode: raw record headers and last-updated dates are decoded; metadata key/values are intentionally skipped for stability");
                } else if (decodeMode_ == NativeDecodeMode::CoreFields) {
                    log.info("Native decoder is running in V0.6.4 safe core-probe mode. Record headers are decoded and bounded high-value raw string/path probes are retained; structured value decoding is experimental only.");
                } else {
                    log.warn("Experimental full native metadata value parsing is enabled. This may be unstable on some store.db files.");
                }

                std::unordered_map<std::int64_t, std::pair<std::int64_t, std::string>> pathNodes;
                std::size_t storeItems = 0;
                std::size_t storeMetadataBlocks = 0;
                std::size_t blockOrdinal = 0;
                bool stopDueToNativeRecordLimit = false;
                for (const auto& entry : block0) {
                    ++blockOrdinal;
                    const auto blockOffset = static_cast<std::uint64_t>(entry.offsetIndex) * 0x1000ull;
                    if (blockOrdinal <= 3 || (blockOrdinal % 1000) == 0) {
                        log.info("Native parser block scan: store_guid=" + store.storeGuid +
                                 " block_ordinal=" + std::to_string(blockOrdinal) +
                                 " offset_index=" + std::to_string(entry.offsetIndex) +
                                 " byte_offset=" + std::to_string(blockOffset));
                    }
                    if (blockOrdinal <= 3 || (blockOrdinal % 1000) == 0 || blockOrdinal == block0.size()) {
                        const double storePart = (static_cast<double>(storeOrdinal - 1) + (block0.empty() ? 1.0 : (static_cast<double>(blockOrdinal) / static_cast<double>(block0.size())))) / static_cast<double>(totalStores);
                        int pct = 36 + static_cast<int>(storePart * 39.0);
                        if (pct < 36) pct = 36;
                        if (pct > 74) pct = 74;
                        appendNativeProgress(progressPath_, pct, "native_parse_blocks",
                            "store=" + std::to_string(storeOrdinal) + "/" + std::to_string(totalStores) +
                            " block=" + std::to_string(blockOrdinal) + "/" + std::to_string(block0.size()) +
                            " parsed_items=" + std::to_string(counts.parsedItems) +
                            " raw_records=" + std::to_string(counts.rawRecords) +
                            " guid=" + store.storeGuid);
                    }
                    if (blockOffset == 0 || blockOffset >= store.fileSizeBytes) continue;
                    StoreBlockInfo block{};
                    try { block = readStoreBlock(in, blockOffset, h.blockSize); } catch (const std::exception& ex) { ++counts.failures; insertFailure(db, source, store, "block_header", ex.what()); continue; }
                    if (block.baseType() != BlockType::Metadata) continue;
                    if (maxMetadataBlocks_ > 0 && storeMetadataBlocks >= maxMetadataBlocks_) {
                        log.info("Native parser per-store metadata block limit reached: store_guid=" + store.storeGuid +
                                 " max_native_blocks=" + std::to_string(maxMetadataBlocks_) +
                                 " store_metadata_blocks_parsed=" + std::to_string(storeMetadataBlocks) +
                                 " total_metadata_blocks_parsed=" + std::to_string(counts.metadataBlocks));
                        break;
                    }
                    ++counts.metadataBlocks;
                    ++storeMetadataBlocks;
                    if (counts.metadataBlocks <= 3 || (counts.metadataBlocks % 1000) == 0) {
                        log.info("Native parser metadata block: store_guid=" + store.storeGuid +
                                 " block_ordinal=" + std::to_string(blockOrdinal) +
                                 " type_raw=" + std::to_string(block.blockTypeRaw) +
                                 " physical=" + std::to_string(block.physicalSize) +
                                 " logical=" + std::to_string(block.logicalSize));
                    }
                    std::vector<std::uint8_t> payload;
                    try { payload = decompressMetadataBlock(in, blockOffset, block, h.blockSize); }
                    catch (const std::exception& ex) { ++counts.failures; insertFailure(db, source, store, "metadata_decompress", ex.what()); continue; }
                    if (payload.empty()) continue;
                    ++counts.decompressedBlocks;
                    if (counts.decompressedBlocks <= 3 || (counts.decompressedBlocks % 1000) == 0) {
                        log.info("Native parser decompressed metadata block: payload_bytes=" + std::to_string(payload.size()));
                    }
                    try {
                        std::size_t pos = 0;
                        while (true) {
                            std::uint64_t v1RawId = 0;
                            std::size_t itemAdvance = 0;
                            std::size_t itemStart = 0;
                            std::size_t itemEnd = 0;
                            if (h.version == 1) {
                                if (pos + 16 > payload.size()) break;
                                v1RawId = readU64(payload, pos);
                                const auto itemSize = readI32(payload, pos + 12);
                                if (itemSize <= 16 || static_cast<std::size_t>(itemSize) > MaxMetadataItemBytes || pos + static_cast<std::size_t>(itemSize) > payload.size()) break;
                                itemStart = pos + 16;
                                itemEnd = pos + static_cast<std::size_t>(itemSize);
                                itemAdvance = static_cast<std::size_t>(itemSize);
                            } else {
                                if (pos + 4 > payload.size()) break;
                                const auto itemSize = readI32(payload, pos);
                                if (itemSize <= 0 || static_cast<std::size_t>(itemSize) > MaxMetadataItemBytes || pos + 4 + static_cast<std::size_t>(itemSize) > payload.size()) break;
                                itemStart = pos + 4;
                                itemEnd = pos + 4 + static_cast<std::size_t>(itemSize);
                                itemAdvance = 4 + static_cast<std::size_t>(itemSize);
                            }
                            try {
                                ParsedItem item;
                                try {
                                    MetadataItemParser itemParser(payload, itemStart, itemEnd, properties, categories, indexes1, indexes2);
                                    if (decodeMode_ == NativeDecodeMode::FullValues) item = (h.version == 1) ? itemParser.parseV1(v1RawId) : itemParser.parseV2();
                                    else if (decodeMode_ == NativeDecodeMode::CoreFields) item = (h.version == 1) ? itemParser.parseV1CoreOnly(v1RawId) : itemParser.parseV2CoreOnly();
                                    else item = (h.version == 1) ? itemParser.parseV1HeaderOnly(v1RawId) : itemParser.parseV2HeaderOnly();
                                } catch (const std::exception& decodeEx) {
                                    if (decodeMode_ == NativeDecodeMode::HeaderOnly) throw;
                                    MetadataItemParser fallbackParser(payload, itemStart, itemEnd, properties, categories, indexes1, indexes2);
                                    item = (h.version == 1) ? fallbackParser.parseV1HeaderOnly(v1RawId) : fallbackParser.parseV2HeaderOnly();
                                    addCoreProbeMetadata(item, payload, itemStart, itemEnd);
                                    ++counts.fallbackHeaderOnlyItems;
                                    insertFailure(db, source, store, "metadata_item_decode_fallback", std::string("block=") + std::to_string(blockOrdinal) + " offset=" + std::to_string(pos) + " error=" + decodeEx.what());
                                }
                                if (decodeMode_ == NativeDecodeMode::FullValues && item.metadata.empty()) {
                                    // Validation mode must remain evidence-preserving: if the private Store-V2 structured
                                    // value decoder cannot promote named kMDItem/CSSearchableItem fields, keep the bounded
                                    // raw string probes instead of emitting a record with no reviewable metadata.
                                    addCoreProbeMetadata(item, payload, itemStart, itemEnd);
                                }
                                auto name = fileNameOf(item);
                                if (!name.empty()) pathNodes[item.inodeId] = {item.parentId, name};
                                const auto fullPath = reconstructPath(item.inodeId, pathNodes);
                                insertItem(store, item, fullPath);
                                ++storeItems;
                                if (counts.parsedItems > 0 && (counts.parsedItems % 10000) == 0) {
                                    db.commit();
                                    try { db.exec("PRAGMA wal_checkpoint(TRUNCATE);"); } catch (const std::exception& ex) { log.warn(std::string("WAL checkpoint/truncate warning: ") + ex.what()); }
                                    checkSqliteSizeGuardrail(db, progressPath_, log, dbSizeGuardrailBytes_,
                                                             "native_parse parsed_items=" + std::to_string(counts.parsedItems) +
                                                             " raw_key_values=" + std::to_string(counts.rawKeyValues) +
                                                             " raw_date_candidates=" + std::to_string(counts.rawDateCandidates));
                                    appendNativeProgress(progressPath_, 73, "native_parse_heartbeat",
                                                         "parsed_items=" + std::to_string(counts.parsedItems) +
                                                         " raw_key_values=" + std::to_string(counts.rawKeyValues) +
                                                         " raw_date_candidates=" + std::to_string(counts.rawDateCandidates));
                                    db.begin();
                                }
                            } catch (const std::exception& ex) {
                                if (isFatalNativeAbort(ex)) throw;
                                ++counts.failures;
                                insertFailure(db, source, store, "metadata_item_parse", std::string("block=") + std::to_string(blockOrdinal) + " offset=" + std::to_string(pos) + " error=" + ex.what());
                            }
                            pos += itemAdvance;
                            if (maxRecords_ > 0 && counts.parsedItems >= maxRecords_) {
                                stopDueToNativeRecordLimit = true;
                                log.info("Native parser record diagnostic limit reached. parsed_items=" + std::to_string(counts.parsedItems) +
                                         " limit=" + std::to_string(maxRecords_));
                                break;
                            }
                        }
                    } catch (const std::exception& ex) {
                        if (isFatalNativeAbort(ex)) throw;
                        ++counts.failures;
                        insertFailure(db, source, store, "metadata_parse", ex.what());
                    }
                    if (stopDueToNativeRecordLimit) break;
                    if ((counts.parsedItems % 50000) == 0 && counts.parsedItems != 0) log.info("Native parser progress: parsed_items=" + std::to_string(counts.parsedItems));
                }
                log.info("Native parsed " + std::to_string(storeItems) + " metadata items from " + pathString(store.storePath));
                appendNativeProgress(progressPath_, 74, "native_parse_store_complete", "store=" + std::to_string(storeOrdinal) + "/" + std::to_string(totalStores) + " parsed_store_items=" + std::to_string(storeItems) + " total_raw_records=" + std::to_string(counts.rawRecords));
                insertNativeDecodeAttempt(db, source, store, decodeMode_, attemptSpotlightVersion, attemptPropertiesCount, attemptCategoriesCount,
                                          counts.metadataBlocks - beforeMetadataBlocks, counts.decompressedBlocks - beforeDecompressedBlocks,
                                          counts.rawRecords - beforeRawRecords, counts.rawKeyValues - beforeRawKeyValues,
                                          counts.rawDateCandidates - beforeRawDateCandidates, counts.fallbackHeaderOnlyItems - beforeFallbackHeaderOnlyItems,
                                          counts.failures - beforeFailures, storeAttemptStartedUtc, "SUCCESS", "");
                db.commit();
                try { db.exec("PRAGMA wal_checkpoint(TRUNCATE);"); } catch (const std::exception& ex) { log.warn(std::string("WAL checkpoint/truncate warning: ") + ex.what()); }
                checkSqliteSizeGuardrail(db, progressPath_, log, dbSizeGuardrailBytes_,
                                         "native_parse_store_complete store=" + std::to_string(storeOrdinal) +
                                         " raw_key_values=" + std::to_string(counts.rawKeyValues) +
                                         " raw_date_candidates=" + std::to_string(counts.rawDateCandidates));
                db.begin();
                if (stopDueToNativeRecordLimit) break;
            } catch (const std::exception& ex) {
                if (isFatalNativeAbort(ex)) throw;
                ++counts.failures;
                insertFailure(db, source, store, "store_parse", ex.what());
                insertNativeDecodeAttempt(db, source, store, decodeMode_, attemptSpotlightVersion, attemptPropertiesCount, attemptCategoriesCount,
                                          counts.metadataBlocks - beforeMetadataBlocks, counts.decompressedBlocks - beforeDecompressedBlocks,
                                          counts.rawRecords - beforeRawRecords, counts.rawKeyValues - beforeRawKeyValues,
                                          counts.rawDateCandidates - beforeRawDateCandidates, counts.fallbackHeaderOnlyItems - beforeFallbackHeaderOnlyItems,
                                          counts.failures - beforeFailures, storeAttemptStartedUtc, "FAILED_STORE_PARSE", ex.what());
                log.warn("Native store parse failed for " + pathString(store.storePath) + ": " + ex.what());
            } catch (...) {
                ++counts.failures;
                insertFailure(db, source, store, "store_parse", "unknown non-standard exception");
                insertNativeDecodeAttempt(db, source, store, decodeMode_, attemptSpotlightVersion, attemptPropertiesCount, attemptCategoriesCount,
                                          counts.metadataBlocks - beforeMetadataBlocks, counts.decompressedBlocks - beforeDecompressedBlocks,
                                          counts.rawRecords - beforeRawRecords, counts.rawKeyValues - beforeRawKeyValues,
                                          counts.rawDateCandidates - beforeRawDateCandidates, counts.fallbackHeaderOnlyItems - beforeFallbackHeaderOnlyItems,
                                          counts.failures - beforeFailures, storeAttemptStartedUtc, "FAILED_STORE_PARSE", "unknown non-standard exception");
                log.warn("Native store parse failed for " + pathString(store.storePath) + ": unknown non-standard exception");
            }
        }
        appendNativeProgress(progressPath_, 75, "native_parse_complete", "raw_records=" + std::to_string(counts.rawRecords) + " raw_key_values=" + std::to_string(counts.rawKeyValues) + " raw_date_candidates=" + std::to_string(counts.rawDateCandidates));
        db.commit();
    } catch (...) {
        db.rollbackNoThrow();
        throw;
    }
    log.info("Native parser summary: stores=" + std::to_string(counts.storesSeen) + " valid=" + std::to_string(counts.validStores) + " records=" + std::to_string(counts.rawRecords) + " kv=" + std::to_string(counts.rawKeyValues) + " date_candidates=" + std::to_string(counts.rawDateCandidates) + " failures=" + std::to_string(counts.failures) + " fallback_header_only=" + std::to_string(counts.fallbackHeaderOnlyItems));
    return counts;
}

} // namespace vestigant::spotlight
