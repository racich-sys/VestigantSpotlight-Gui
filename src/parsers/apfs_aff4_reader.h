#pragma once

#include "parsers/apfs_volume_reader.h"
#include "app/models.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace vestigant::spotlight {


struct Aff4StreamInventoryEntry {
    int lineIndex = 0;
    std::string rawLine;
    std::string candidateType;
    int candidateScore = 0;
    std::string apfsRelevance;
    std::string recommendedAction;
    std::string notes;
};

struct Aff4StreamInventoryResult {
    std::string status;
    std::filesystem::path toolPath;
    std::filesystem::path rawOutputPath;
    int commandExitCode = -1;
    std::size_t rawLineCount = 0;
    std::vector<Aff4StreamInventoryEntry> entries;
};

using Aff4ToolResolver = std::function<std::filesystem::path(const std::string& envVar,
                                                             const std::vector<std::string>& names)>;
using Aff4ExecutableRunner = std::function<int(const std::filesystem::path& exe,
                                               const std::vector<std::string>& args,
                                               const std::filesystem::path& outputFile)>;
using Aff4ShellCommandRunner = std::function<int(const std::string& command)>;

Aff4StreamInventoryEntry classifyAff4StreamLine(int index, const std::string& line);
Aff4StreamInventoryResult runAff4StreamInventory(const RunOptions& opt,
                                                 const EvidenceSource& source,
                                                 const std::filesystem::path& originalInput,
                                                 const std::filesystem::path& caseDir,
                                                 Logger& log,
                                                 Aff4ToolResolver toolResolver,
                                                 Aff4ExecutableRunner executableRunner,
                                                 Aff4ShellCommandRunner shellRunner);

struct ApfsBtreeKvLocation {
    std::uint32_t entryIndex = 0;
    std::size_t keyOffset = 0;
    std::size_t keyLength = 0;
    std::size_t valueOffset = 0;
    std::size_t valueLength = 0;
    std::uint64_t rawKey = 0;
    std::uint64_t objectId = 0;
    std::uint8_t recordType = 0;
};

struct ApfsDirectoryIteratorResult {
    std::vector<ApfsDirectoryEntry> entries;
    ApfsDirectoryIteratorBenchmarks benchmarks;
    std::vector<std::string> warnings;
    bool lowerBoundReaderAvailable = false;
};


// Shared APFS B-tree table-of-contents decoders used by both the live AFF4/APFS
// pipeline and the isolated lower-bound iterator tests.  These functions are
// intentionally side-effect free so app_runner can call them while the rest of
// the APFS extraction state is migrated into ApfsAff4Reader.
bool apfsAff4DecodeFixedKvAbs(const std::vector<unsigned char>& node,
                              std::uint32_t entryIndex,
                              std::size_t valueLenNeeded,
                              std::size_t& keyAbs,
                              std::size_t& valAbs,
                              std::string& detail);

bool apfsAff4DecodeGenericBtreeKvAbs(const std::vector<unsigned char>& node,
                                     std::uint32_t entryIndex,
                                     std::size_t& tocAbs,
                                     std::size_t& keyAbs,
                                     std::size_t& keyLen,
                                     std::size_t& valAbs,
                                     std::size_t& valLen,
                                     std::string& detail);

class ApfsAff4Reader {
public:
    using NodeBytes = std::vector<unsigned char>;
    using LeafLocator = std::function<std::uint64_t(std::uint64_t searchKey)>;
    using NodeReader = std::function<NodeBytes(std::uint64_t nodeOid)>;
    using KvDecoder = std::function<bool(const NodeBytes& node,
                                         std::uint32_t entryIndex,
                                         ApfsBtreeKvLocation& out,
                                         std::string& detail)>;
    using NextLeafReader = std::function<std::uint64_t(const NodeBytes& node,
                                                       std::uint64_t currentLeafOid)>;

    ApfsAff4Reader() = default;

    void setLeafLocator(LeafLocator fn) { leafLocator_ = std::move(fn); }
    void setNodeReader(NodeReader fn) { nodeReader_ = std::move(fn); }
    void setKvDecoder(KvDecoder fn) { kvDecoder_ = std::move(fn); }
    void setNextLeafReader(NextLeafReader fn) { nextLeafReader_ = std::move(fn); }

    ApfsDirectoryIteratorResult getDirectoryContents(std::uint64_t parentInodeId,
                                                     std::uint32_t maxMalformedNodes = 16);

    static std::optional<ApfsDirectoryEntry> decodeDirectoryRecord(const NodeBytes& node,
                                                                   const ApfsBtreeKvLocation& kv,
                                                                   std::string& detail);

private:
    LeafLocator leafLocator_;
    NodeReader nodeReader_;
    KvDecoder kvDecoder_;
    NextLeafReader nextLeafReader_;
};

} // namespace vestigant::spotlight
