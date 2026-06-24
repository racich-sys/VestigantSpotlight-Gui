#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vestigant::spotlight {
namespace fs = std::filesystem;

enum class SourceProfileKind { Auto, MacOS, IOS };

enum class FileExistenceStatus {
    NotChecked,
    LiveFilePresent,
    FileNotFoundUnderEvidenceRoot,
    IndexOnlyNoFilesystemPath,
    NotFilesystemBacked,
    Unknown
};

struct RunOptions {
    std::string mode = "run";          // run, discover, diagnostics, init-case
    std::string profile = "auto";      // auto, macos, ios
    fs::path input;
    fs::path output;
    fs::path caseDir;
    fs::path evidenceRoot;
    fs::path reuseIosCache;          // Optional prior completed iOS case/cache folder; skips large ZIP re-inventory/extraction when source matches.
    std::string caseName = "Spotlight Case";
    std::string caseNumber;
    std::string subjectName;
    std::string company;
    std::string investigator;
    std::size_t chunkRows = 250000;
    std::string exportProfile = "minimal"; // minimal, investigator, diagnostics, full
    bool suppressCsvExports = false; // GUI/CLI option: build SQLite case and summaries without end-of-run CSV review exports.
    bool fullScan = false;
    bool skipContainerHash = false;  // source-probe speed option for very large image containers; preserves registration with HASH_DEFERRED
    bool forceContainerHash = false; // explicitly hash large AFF4/raw containers during source-probe when a full evidentiary hash is needed
    std::string externalSourceSha256; // externally verified source/container SHA256; avoids rehashing very large inputs when documented
    std::string externalSourceHashNote; // provenance/note for externally supplied source hash
    bool strictSingleAff4 = false; // require input to be one explicit .aff4 file and prevent helper/library-driven AFF4 discovery
    bool enableAff4DynamicProbe = false; // opt-in only: libaff4 may perform container/volume discovery outside the selected file
    bool enableAff4StreamInventory = false; // opt-in only: external aff4imager stream listing
    fs::path readerToolsDir;         // optional directory containing AFF4/APFS/HFS helper tools for readiness discovery
    bool verbose = false;
    bool preserveEvidence = true;
    bool preserveEvidenceExplicit = false;
    bool experimentalFullNativeValues = false;
    bool decodeCoreNativeValues = false;
    bool diagnosticFullNativeDb = false; // opt-in: persist all native key/value rows for support/debug; normal iOS runs keep only high-value rows.
    bool aff4ApfsDiagnosticOutputs = false; // opt-in: write heavy AFF4/APFS structural probe CSVs; normal source-probe keeps stage/copy/summary outputs only.
    bool materializeIosFfsInventory = false; // opt-in support mode: persist full iOS FFS inventory rows into the active case DB. Normal Spotlight-first runs keep/cache-reference only.
    bool materializeIosAppDbRecords = false; // opt-in support mode: parse/persist app DB record rows. Normal Spotlight-first runs skip broad app DB materialization.
    std::size_t maxNativeRecords = 0;  // 0 = unlimited; diagnostics+full-native defaults to a bounded sample
    bool maxNativeRecordsExplicit = false;
    std::size_t maxNativeBlocks = 0;   // 0 = unlimited; diagnostic safety valve for metadata blocks
    std::uintmax_t dbSizeGuardrailBytes = 6ull * 1024ull * 1024ull * 1024ull; // default 6 GiB DB/WAL guardrail for investigator/dev runs; 0 disables
    bool parseDotStoreCopies = false;
    std::size_t dbBatchRows = 50000;
    fs::path sevenZipPath;
};

struct RunResult {
    int exitCode = 0;
    std::vector<std::string> messages;
    std::size_t sourceCount = 0;
    std::size_t storeCount = 0;              // distinct Store-V2/CoreSpotlight logical stores
    std::size_t validStoreCount = 0;         // distinct valid logical stores
    std::size_t databaseCandidateCount = 0;  // store.db/.store.db candidates discovered
    std::size_t validDatabaseCandidateCount = 0;
    std::size_t selectedParserDatabaseCount = 0;
    std::string nativeDecodeMode;
    std::size_t rawRecordCount = 0;
    std::size_t rawKeyValueCount = 0;
    std::size_t rawDateCandidateCount = 0;
    std::size_t artifactCount = 0;
    std::size_t usageCount = 0;
    std::size_t timelineCount = 0;
    std::size_t orphanCandidateCount = 0;
};

struct CaseInfo {
    std::string caseId;
    std::string caseName;
    std::string caseNumber;
    std::string subjectName;
    std::string company;
    std::string investigator;
    std::string createdUtc;
    std::string appVersion;
};

struct EvidenceSource {
    std::string sourceId;
    std::string profile;
    fs::path inputPath;
    fs::path evidenceRoot;
    std::string addedUtc;
    std::string sourceKind;
    std::string notes;
};

struct StoreInfo {
    std::string sourceId;
    std::string storeGuid;
    fs::path storePath;
    std::string relativePath;
    bool isValid = false;
    int version = 0;
    std::uint32_t signature = 0;
    std::uint32_t flags = 0;
    std::uint32_t headerSize = 0;
    std::uint32_t block0Size = 0;
    std::uint32_t blockSize = 0;
    bool inferredIosStore = false;
    std::uintmax_t fileSizeBytes = 0;
    std::string sha256;
    std::string validationError;
    std::string profileHint;
};

struct ArtifactRecord {
    std::string artifactId;
    std::string sourceId;
    std::string sourceProfile;
    std::string storeGuid;
    std::string inode;
    std::string parentInode;
    std::string fileName;
    std::string filePath;
    std::string reconstructedPath;
    std::string pathState;
    std::string pathChain;
    std::string displayName;
    std::string contentType;
    std::string contentTypeTree;
    std::string whereFroms;
    std::string authors;
    std::string creator;
    std::string logicalSizeBytes;
    std::string physicalSizeBytes;
    std::string downloadedDateUtc;
    std::string creationDateUtc;
    std::string contentCreationDateUtc;
    std::string contentModificationDateUtc;
    std::string contentChangeDateUtc;
    std::string timestampUtc;
    std::string interestingDateUtc;
    std::string lastUpdatedUtc;
    std::string usedDatesUtc;
    std::string lastUsedDateUtc;
    std::string usedDatesCount;
    std::string openCountEstimate;
    std::string firstUsedDateUtc;
    std::string lastUsedDateFromArrayUtc;
    std::string usageSignal;
    std::string indexTextSnippet;
    std::string kvFieldsPresent;
    FileExistenceStatus existenceStatus = FileExistenceStatus::NotChecked;
    std::string existenceStatusText;
    std::string matchedFilesystemPath;
    bool deletedOrOrphanedCandidate = false;
    std::string orphanReason;
    std::string confidence;
};

struct TimelineEvent {
    std::string artifactId;
    std::string sourceId;
    std::string sourceProfile;
    std::string storeGuid;
    std::string inode;
    std::string timestampUtc;
    std::string eventType;
    std::string sourceField;
    std::string fileName;
    std::string path;
    std::string existenceStatusText;
    std::string deletedOrOrphanedCandidate;
};
}
