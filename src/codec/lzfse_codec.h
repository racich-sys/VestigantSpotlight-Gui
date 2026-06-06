#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vestigant::spotlight {

struct LzfseDecodeResult {
    bool ok = false;
    bool codecAvailable = false;
    bool outputTruncated = false;
    std::string status;
    std::string notes;
    std::vector<unsigned char> data;
};

// Decode one APFS decmpfs resource-fork chunk using Apple's lzfse/lzvn reference
// implementation when that vetted source tree has been vendored and the build is
// compiled with VESTIGANT_HAS_LZFSE.  The caller supplies an expected maximum
// expanded size.  The function never emits data when the codec is absent.
LzfseDecodeResult decodeAppleLzfseOrLzvnChunk(const std::vector<unsigned char>& chunk,
                                               std::size_t expectedMaxOutputBytes);

bool appleLzfseCodecAvailable();
std::string appleLzfseCodecBuildStatus();

struct ApfsDecmpfsReconstructionResult {
    bool ok = false;
    std::string status;
    std::string notes;
    std::uint32_t descriptorOffset = 0;
    std::uint32_t footerOffset = 0;
    std::uint32_t blockCount = 0;
    std::vector<unsigned char> data;
};

std::string decmpfsCompressionTypeLabel(int compressionType);
std::vector<unsigned char> apfsInflateZlibBounded(const std::vector<unsigned char>& z, std::size_t maxOutputBytes);
ApfsDecmpfsReconstructionResult reconstructDecmpfsResourceForkDissectStyle(const std::vector<unsigned char>& resourceFork,
                                                                           std::uint64_t expectedUncompressedSize,
                                                                           int compressionType);
ApfsDecmpfsReconstructionResult reconstructType4DecmpfsResourceForkZlib(const std::vector<unsigned char>& resourceFork,
                                                                        std::uint64_t expectedUncompressedSize);

} // namespace vestigant::spotlight
