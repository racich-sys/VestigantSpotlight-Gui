#include "codec/lzfse_codec.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <map>
#include <array>

#if defined(VESTIGANT_HAS_LZFSE)
#include <lzfse.h>
#endif

namespace vestigant::spotlight {

bool appleLzfseCodecAvailable() {
#if defined(VESTIGANT_HAS_LZFSE)
    return true;
#else
    return false;
#endif
}

std::string appleLzfseCodecBuildStatus() {
#if defined(VESTIGANT_HAS_LZFSE)
    return "APPLE_LZFSE_REFERENCE_CODEC_ENABLED";
#else
    return "APPLE_LZFSE_REFERENCE_CODEC_NOT_COMPILED";
#endif
}

LzfseDecodeResult decodeAppleLzfseOrLzvnChunk(const std::vector<unsigned char>& chunk,
                                               std::size_t expectedMaxOutputBytes) {
    LzfseDecodeResult rr;
    rr.codecAvailable = appleLzfseCodecAvailable();
    if (chunk.empty()) {
        rr.status = "LZFSE_INPUT_EMPTY";
        rr.notes = "Compressed APFS decmpfs chunk was empty.";
        return rr;
    }
    if (expectedMaxOutputBytes == 0 || expectedMaxOutputBytes > (1024U * 1024U)) {
        rr.status = "LZFSE_EXPECTED_OUTPUT_SIZE_UNSAFE";
        rr.notes = "Expected APFS decmpfs chunk output size was zero or above the per-chunk safety cap.";
        return rr;
    }
#if defined(VESTIGANT_HAS_LZFSE)
    // lzfse_decode_buffer returns dst_size when the destination is too small.
    // Allocate one extra byte so a normal 64 KiB APFS chunk can be distinguished
    // from truncation.  The caller will still cap final logical file output.
    const std::size_t cap = std::min<std::size_t>(expectedMaxOutputBytes + 1U, 1024U * 1024U + 1U);
    std::vector<unsigned char> out(cap, 0U);
    const std::size_t decoded = lzfse_decode_buffer(out.data(), out.size(), chunk.data(), chunk.size(), nullptr);
    if (decoded == 0) {
        rr.status = "LZFSE_DECODE_FAILED";
        rr.notes = "Apple lzfse_decode_buffer returned zero for this APFS decmpfs chunk.";
        return rr;
    }
    if (decoded > expectedMaxOutputBytes) {
        rr.status = "LZFSE_DECODE_OUTPUT_EXCEEDED_EXPECTED_CHUNK_SIZE";
        rr.notes = "Apple lzfse_decode_buffer produced more bytes than the bounded expected APFS chunk size.";
        rr.outputTruncated = true;
        return rr;
    }
    out.resize(decoded);
    rr.ok = true;
    rr.status = "LZFSE_DECODE_OK";
    rr.notes = "Decoded by Apple lzfse/lzvn reference implementation.";
    rr.data.swap(out);
    return rr;
#else
    rr.status = "LZFSE_CODEC_NOT_COMPILED";
    rr.notes = "Apple lzfse reference source was not vendored/enabled at build time; no decompressed bytes emitted.";
    return rr;
#endif
}

// APFS decmpfs resource-fork reconstruction helpers moved from app_runner.cpp in V1.1.1.
std::uint32_t readBe32Bytes(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 4U > data.size()) throw std::runtime_error("readBe32 beyond buffer");
    return (static_cast<std::uint32_t>(data[off]) << 24U) |
           (static_cast<std::uint32_t>(data[off + 1U]) << 16U) |
           (static_cast<std::uint32_t>(data[off + 2U]) << 8U) |
           static_cast<std::uint32_t>(data[off + 3U]);
}

std::uint32_t readLe32Bytes(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 4U > data.size()) throw std::runtime_error("readLe32 beyond buffer");
    return static_cast<std::uint32_t>(data[off]) |
           (static_cast<std::uint32_t>(data[off + 1U]) << 8U) |
           (static_cast<std::uint32_t>(data[off + 2U]) << 16U) |
           (static_cast<std::uint32_t>(data[off + 3U]) << 24U);
}

std::string decmpfsCompressionTypeLabel(int t) {
    switch (t) {
        case 3: return "ZLIB_ATTR";
        case 4: return "ZLIB_RSRC";
        case 7: return "LZVN_ATTR";
        case 8: return "LZVN_RSRC";
        case 9: return "PLAIN_ATTR";
        case 10: return "PLAIN_RSRC";
        case 11: return "LZFSE_ATTR";
        case 12: return "LZFSE_RSRC";
        case 13: return "LZBITMAP_ATTR";
        case 14: return "LZBITMAP_RSRC";
        default: return t == 0 ? "UNKNOWN" : (std::string("DECOMPFS_TYPE_") + std::to_string(t));
    }
}

class ApfsDeflateBitReader {
public:
    ApfsDeflateBitReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
    std::uint32_t readBits(int n) {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i) {
            if (bytePos_ >= size_) throw std::runtime_error("deflate bitstream exhausted");
            v |= static_cast<std::uint32_t>((data_[bytePos_] >> bitPos_) & 1U) << i;
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

std::uint32_t apfsReverseBits(std::uint32_t code, int len) {
    std::uint32_t r = 0;
    for (int i = 0; i < len; ++i) { r = (r << 1U) | (code & 1U); code >>= 1U; }
    return r;
}

class ApfsHuffmanTree {
public:
    void build(const std::vector<int>& lengths) {
        table_.clear();
        maxLen_ = 0;
        std::array<int, 16> blCount{};
        for (int l : lengths) {
            if (l < 0 || l > 15) throw std::runtime_error("invalid huffman length");
            if (l) { blCount[static_cast<std::size_t>(l)]++; maxLen_ = std::max(maxLen_, l); }
        }
        std::array<int, 16> nextCode{};
        int code = 0;
        for (int bits = 1; bits <= 15; ++bits) { code = (code + blCount[static_cast<std::size_t>(bits - 1)]) << 1; nextCode[static_cast<std::size_t>(bits)] = code; }
        for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
            const int len = lengths[symbol];
            if (!len) continue;
            const std::uint32_t canonical = static_cast<std::uint32_t>(nextCode[static_cast<std::size_t>(len)]++);
            const std::uint32_t rev = apfsReverseBits(canonical, len);
            table_[(static_cast<std::uint32_t>(len) << 16U) | rev] = static_cast<int>(symbol);
        }
    }
    int decode(ApfsDeflateBitReader& br) const {
        std::uint32_t code = 0;
        for (int len = 1; len <= maxLen_; ++len) {
            code |= br.readBits(1) << (len - 1);
            auto it = table_.find((static_cast<std::uint32_t>(len) << 16U) | code);
            if (it != table_.end()) return it->second;
        }
        throw std::runtime_error("invalid huffman code");
    }
private:
    std::map<std::uint32_t, int> table_;
    int maxLen_ = 0;
};

std::vector<unsigned char> apfsInflateZlibBounded(const std::vector<unsigned char>& z, std::size_t maxOutputBytes) {
    if (z.size() < 6) throw std::runtime_error("zlib stream too small");
    const std::uint8_t cmf = z[0], flg = z[1];
    if ((cmf & 0x0FU) != 8U) throw std::runtime_error("zlib stream is not deflate");
    if ((((static_cast<int>(cmf) << 8) + flg) % 31) != 0) throw std::runtime_error("zlib header check failed");
    if (flg & 0x20U) throw std::runtime_error("zlib preset dictionary not supported");
    ApfsDeflateBitReader br(z.data() + 2, z.size() - 6);
    std::vector<unsigned char> out;
    auto pushOut = [&](std::uint8_t v) {
        if (out.size() >= maxOutputBytes) throw std::runtime_error("deflate output exceeded safety cap");
        out.push_back(v);
    };
    bool final = false;
    static const int lengthBase[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const int lengthExtra[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const int distBase[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const int distExtra[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    auto decodeCompressed = [&](const ApfsHuffmanTree& litLen, const ApfsHuffmanTree& dist) {
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
            ApfsHuffmanTree litLen, dist; litLen.build(ll); dist.build(dd);
            decodeCompressed(litLen, dist);
        } else if (btype == 2) {
            const int hlit = static_cast<int>(br.readBits(5)) + 257;
            const int hdist = static_cast<int>(br.readBits(5)) + 1;
            const int hclen = static_cast<int>(br.readBits(4)) + 4;
            static const int order[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            std::vector<int> cl(19, 0);
            for (int i = 0; i < hclen; ++i) cl[static_cast<std::size_t>(order[i])] = static_cast<int>(br.readBits(3));
            ApfsHuffmanTree clTree; clTree.build(cl);
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
            ApfsHuffmanTree litLen, dist; litLen.build(ll); dist.build(dd);
            decodeCompressed(litLen, dist);
        } else {
            throw std::runtime_error("reserved deflate block type");
        }
    }
    return out;
}


ApfsDecmpfsReconstructionResult reconstructDecmpfsResourceForkDissectStyle(const std::vector<unsigned char>& resourceFork,
                                                                                 std::uint64_t expectedUncompressedSize,
                                                                                 int compressionType) {
    ApfsDecmpfsReconstructionResult rr;
    if (expectedUncompressedSize == 0 || expectedUncompressedSize > 512ULL * 1024ULL * 1024ULL) {
        rr.status = "DECOMPFS_EXPECTED_SIZE_UNSAFE";
        rr.notes = "Expected uncompressed size was zero or above the bounded reconstruction cap.";
        return rr;
    }
    const bool dissectHeaderTableModel = (compressionType == 4 || compressionType == 10);
    const bool offsetArrayModel = (compressionType == 8 || compressionType == 12 || compressionType == 14);
    if (!(dissectHeaderTableModel || offsetArrayModel)) {
        rr.status = "DECOMPFS_RESOURCE_FORK_ALGORITHM_UNSUPPORTED";
        rr.notes = "Resource-fork algorithm " + decmpfsCompressionTypeLabel(compressionType) + " is identified but not reconstructed in this build.";
        return rr;
    }
    if (resourceFork.size() < 8U) {
        rr.status = "DECOMPFS_RESOURCE_FORK_TOO_SMALL";
        rr.notes = "Resource fork stream is too small for the decmpfs resource-fork header and block table.";
        return rr;
    }
    try {
        if (offsetArrayModel) {
            // go-apfs/dissect.apfs-compatible model for LZVN/LZFSE/LZBITMAP resource-fork streams:
            // the stream starts with a little-endian array of chunk offsets.  The first offset
            // also gives the length of the offset table.  Without an LZVN/LZFSE/LZBITMAP codec,
            // this build safely reconstructs only chunks explicitly marked uncompressed (0x06)
            // and leaves compressed chunks as explicit unsupported diagnostics.
            const std::uint32_t firstOffset = readLe32Bytes(resourceFork, 0);
            if (firstOffset < 4U || firstOffset > resourceFork.size() || (firstOffset % 4U) != 0U) {
                rr.status = "DECOMPFS_OFFSET_ARRAY_UNSAFE";
                rr.notes = "Resource-fork offset array first offset was outside the copied stream or not 4-byte aligned.";
                return rr;
            }
            rr.blockCount = firstOffset / 4U;
            const std::uint32_t expectedBlocks = static_cast<std::uint32_t>((expectedUncompressedSize + 65535ULL) / 65536ULL);
            if (rr.blockCount == 0 || rr.blockCount > expectedBlocks + 64U || rr.blockCount > 131072U) {
                rr.status = "DECOMPFS_OFFSET_BLOCK_COUNT_UNSAFE";
                rr.notes = "Resource-fork offset-array block count is zero or implausibly large for decoded size.";
                return rr;
            }
            std::vector<std::uint32_t> offsets;
            offsets.reserve(rr.blockCount);
            for (std::uint32_t i = 0; i < rr.blockCount; ++i) offsets.push_back(readLe32Bytes(resourceFork, static_cast<std::size_t>(i) * 4U));
            std::vector<unsigned char> out;
            out.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(expectedUncompressedSize, 64ULL * 1024ULL * 1024ULL)));
            std::size_t uncompressedMarkerChunks = 0;
            for (std::size_t i = 0; i < offsets.size(); ++i) {
                const std::uint32_t off = offsets[i];
                const std::uint32_t next = (i + 1 < offsets.size()) ? offsets[i + 1] : static_cast<std::uint32_t>(resourceFork.size());
                if (off >= next || next > resourceFork.size()) {
                    rr.status = "DECOMPFS_OFFSET_ARRAY_CHUNK_UNSAFE";
                    rr.notes = "Resource-fork offset-array chunk did not fit within the copied stream. entry=" + std::to_string(i);
                    return rr;
                }
                std::vector<unsigned char> chunk(resourceFork.begin() + static_cast<std::ptrdiff_t>(off), resourceFork.begin() + static_cast<std::ptrdiff_t>(next));
                if (chunk.empty()) continue;
                if (chunk[0] == 0x06U) {
                    if (out.size() + (chunk.size() - 1U) > expectedUncompressedSize + 65536ULL) {
                        rr.status = "DECOMPFS_OUTPUT_EXCEEDED_EXPECTED_SIZE";
                        rr.notes = "Uncompressed-marker chunks exceeded decoded size.";
                        return rr;
                    }
                    out.insert(out.end(), chunk.begin() + 1, chunk.end());
                    ++uncompressedMarkerChunks;
                    continue;
                }
                if (compressionType == 8 || compressionType == 12) {
                    const std::uint64_t remaining64 = (out.size() < expectedUncompressedSize) ? (expectedUncompressedSize - static_cast<std::uint64_t>(out.size())) : 0ULL;
                    const std::size_t perChunkExpected = static_cast<std::size_t>(std::min<std::uint64_t>(65536ULL, remaining64));
                    const auto decoded = decodeAppleLzfseOrLzvnChunk(chunk, perChunkExpected == 0 ? 65536U : perChunkExpected);
                    if (!decoded.ok) {
                        rr.status = decoded.codecAvailable ? "DECOMPFS_LZFSE_LZVN_DECODE_FAILED" : "DECOMPFS_LZFSE_LZVN_CODEC_NOT_COMPILED";
                        rr.notes = "Resource-fork algorithm " + decmpfsCompressionTypeLabel(compressionType) + "; entry=" + std::to_string(i) + "; apple_codec_status=" + decoded.status + "; " + decoded.notes;
                        return rr;
                    }
                    if (out.size() + decoded.data.size() > expectedUncompressedSize + 65536ULL) {
                        rr.status = "DECOMPFS_OUTPUT_EXCEEDED_EXPECTED_SIZE";
                        rr.notes = "Decoded LZFSE/LZVN chunks exceeded decoded size. entry=" + std::to_string(i);
                        return rr;
                    }
                    out.insert(out.end(), decoded.data.begin(), decoded.data.end());
                    continue;
                }
                rr.status = "DECOMPFS_RESOURCE_FORK_CODEC_UNSUPPORTED";
                rr.notes = "Resource-fork algorithm " + decmpfsCompressionTypeLabel(compressionType) + " requires a codec for compressed chunks; uncompressed marker chunks before failure=" + std::to_string(uncompressedMarkerChunks) + "; entry=" + std::to_string(i) + "; first_byte=" + std::to_string(static_cast<unsigned int>(chunk[0]));
                return rr;
            }
            if (out.size() > expectedUncompressedSize) out.resize(static_cast<std::size_t>(expectedUncompressedSize));
            rr.data.swap(out);
            rr.ok = (rr.data.size() == expectedUncompressedSize);
            rr.status = rr.ok ? (std::string("DECOMPFS_") + decmpfsCompressionTypeLabel(compressionType) + (appleLzfseCodecAvailable() ? "_RECONSTRUCTED" : "_UNCOMPRESSED_MARKER_RECONSTRUCTED"))
                              : (std::string("DECOMPFS_") + decmpfsCompressionTypeLabel(compressionType) + (appleLzfseCodecAvailable() ? "_PARTIAL_RECONSTRUCTION" : "_PARTIAL_UNCOMPRESSED_MARKER_RECONSTRUCTION"));
            std::ostringstream ns;
            ns << "algorithm=" << decmpfsCompressionTypeLabel(compressionType)
               << "; block_count=" << rr.blockCount
               << "; expected_uncompressed_size=" << expectedUncompressedSize
               << "; reconstructed_size=" << rr.data.size()
               << "; uncompressed_marker_chunks=" << uncompressedMarkerChunks
               << "; apple_lzfse_codec=" << appleLzfseCodecBuildStatus()
               << "; offset_model=go-apfs/dissect.apfs:offset_array_chunk_stream";
            rr.notes = ns.str();
            return rr;
        }

        // dissect.apfs model for zlib/plain resource-fork decmpfs:
        // big-endian: data_offset, mgmt_offset, data_size, mgmt_size
        // at data_offset: little-endian unknown/header and entry_count
        // entries are little-endian offset,length pairs, where chunk_abs = data_offset + 4 + offset.
        rr.descriptorOffset = readBe32Bytes(resourceFork, 0);
        rr.footerOffset = readBe32Bytes(resourceFork, 4);
        const std::uint32_t dataSize = readBe32Bytes(resourceFork, 8);
        const std::uint32_t mgmtSize = readBe32Bytes(resourceFork, 12);
        if (rr.descriptorOffset < 16U || static_cast<std::uint64_t>(rr.descriptorOffset) + 8ULL > resourceFork.size()) {
            rr.status = "DECOMPFS_RESOURCE_DATA_OFFSET_UNSAFE";
            rr.notes = "Resource-fork data offset is outside the copied stream.";
            return rr;
        }
        const std::uint32_t tableHeader0 = readLe32Bytes(resourceFork, rr.descriptorOffset);
        rr.blockCount = readLe32Bytes(resourceFork, static_cast<std::size_t>(rr.descriptorOffset) + 4U);
        const std::uint32_t expectedBlocks = static_cast<std::uint32_t>((expectedUncompressedSize + 65535ULL) / 65536ULL);
        if (rr.blockCount == 0 || rr.blockCount > expectedBlocks + 64U || rr.blockCount > 131072U) {
            rr.status = "DECOMPFS_BLOCK_COUNT_UNSAFE";
            rr.notes = "Resource-fork block count is zero or implausibly large for the decoded file size.";
            return rr;
        }
        const std::size_t tableStart = static_cast<std::size_t>(rr.descriptorOffset) + 8U;
        const std::size_t tableEnd = tableStart + static_cast<std::size_t>(rr.blockCount) * 8U;
        if (tableEnd > resourceFork.size()) {
            rr.status = "DECOMPFS_DESCRIPTOR_TABLE_TRUNCATED";
            rr.notes = "Resource-fork block table extends beyond the copied stream.";
            return rr;
        }
        std::vector<unsigned char> out;
        out.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(expectedUncompressedSize, 64ULL * 1024ULL * 1024ULL)));
        std::size_t zlibChunks = 0, rawMarkerChunks = 0, plainChunks = 0;
        for (std::uint32_t i = 0; i < rr.blockCount; ++i) {
            const std::size_t tupleOff = tableStart + static_cast<std::size_t>(i) * 8U;
            const std::uint32_t entryOffset = readLe32Bytes(resourceFork, tupleOff);
            const std::uint32_t entryLength = readLe32Bytes(resourceFork, tupleOff + 4U);
            if (entryLength == 0) continue;
            const std::uint64_t chunkAbs64 = static_cast<std::uint64_t>(rr.descriptorOffset) + 4ULL + static_cast<std::uint64_t>(entryOffset);
            if (chunkAbs64 > resourceFork.size() || chunkAbs64 + static_cast<std::uint64_t>(entryLength) > resourceFork.size()) {
                rr.status = "DECOMPFS_BLOCK_OFFSET_UNSAFE";
                rr.notes = "Resource-fork chunk offset/length did not fit within the copied stream. entry=" + std::to_string(i) + "; offset=" + std::to_string(entryOffset) + "; length=" + std::to_string(entryLength);
                return rr;
            }
            const std::size_t chunkAbs = static_cast<std::size_t>(chunkAbs64);
            std::vector<unsigned char> chunk(resourceFork.begin() + static_cast<std::ptrdiff_t>(chunkAbs), resourceFork.begin() + static_cast<std::ptrdiff_t>(chunkAbs + entryLength));
            std::vector<unsigned char> decoded;
            if (compressionType == 4) {
                if (!chunk.empty() && chunk[0] == 0x78U && chunk.size() >= 2U) {
                    decoded = apfsInflateZlibBounded(chunk, static_cast<std::size_t>(expectedUncompressedSize));
                    ++zlibChunks;
                } else if (!chunk.empty() && ((chunk[0] & 0x0FU) == 0x0FU)) {
                    decoded.assign(chunk.begin() + 1, chunk.end());
                    ++rawMarkerChunks;
                } else {
                    rr.status = "DECOMPFS_ZLIB_CHUNK_HEADER_UNSUPPORTED";
                    rr.notes = "ZLIB_RSRC chunk did not start with a zlib header or APFS raw marker. entry=" + std::to_string(i) + "; first_byte=" + (chunk.empty() ? std::string("empty") : std::to_string(static_cast<unsigned int>(chunk[0])));
                    return rr;
                }
            } else if (compressionType == 10) {
                if (chunk.empty()) continue;
                decoded.assign(chunk.begin() + 1, chunk.end());
                ++plainChunks;
            }
            if (out.size() + decoded.size() > expectedUncompressedSize + 65536ULL) {
                rr.status = "DECOMPFS_OUTPUT_EXCEEDED_EXPECTED_SIZE";
                rr.notes = "Decoded resource-fork chunks exceeded the decoded decmpfs logical size.";
                return rr;
            }
            out.insert(out.end(), decoded.begin(), decoded.end());
        }
        if (out.size() > expectedUncompressedSize) out.resize(static_cast<std::size_t>(expectedUncompressedSize));
        rr.data.swap(out);
        rr.ok = (rr.data.size() == expectedUncompressedSize);
        rr.status = rr.ok ? (compressionType == 4 ? "DECOMPFS_ZLIB_RSRC_RECONSTRUCTED" : "DECOMPFS_PLAIN_RSRC_RECONSTRUCTED")
                          : (compressionType == 4 ? "DECOMPFS_ZLIB_RSRC_PARTIAL_RECONSTRUCTION" : "DECOMPFS_PLAIN_RSRC_PARTIAL_RECONSTRUCTION");
        std::ostringstream ns;
        ns << "algorithm=" << decmpfsCompressionTypeLabel(compressionType)
           << "; data_offset=" << rr.descriptorOffset
           << "; mgmt_offset=" << rr.footerOffset
           << "; data_size=" << dataSize
           << "; mgmt_size=" << mgmtSize
           << "; table_header0=" << tableHeader0
           << "; block_count=" << rr.blockCount
           << "; expected_uncompressed_size=" << expectedUncompressedSize
           << "; reconstructed_size=" << rr.data.size()
           << "; zlib_chunks=" << zlibChunks
           << "; raw_marker_chunks=" << rawMarkerChunks
           << "; plain_chunks=" << plainChunks
           << "; offset_model=dissect.apfs:data_offset_plus_4_plus_entry_offset";
        rr.notes = ns.str();
    } catch (const std::exception& ex) {
        rr.ok = false;
        rr.status = "DECOMPFS_RESOURCE_FORK_RECONSTRUCTION_FAILED";
        rr.notes = ex.what();
    }
    return rr;
}

ApfsDecmpfsReconstructionResult reconstructType4DecmpfsResourceForkZlib(const std::vector<unsigned char>& resourceFork,
                                                                        std::uint64_t expectedUncompressedSize) {
    return reconstructDecmpfsResourceForkDissectStyle(resourceFork, expectedUncompressedSize, 4);
}

} // namespace vestigant::spotlight
