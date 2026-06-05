#include "codec/lzfse_codec.h"

#include <algorithm>
#include <limits>

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

} // namespace vestigant::spotlight
