#include "parsers/apfs_diagnostic_exporter.h"

#include "app/case_store.h"
#include "codec/lzfse_codec.h"
#include "core/app_info.h"
#include "core/csv.h"
#include "core/path_utils.h"
#include "parsers/apfs_volume_reader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace vestigant::spotlight {
namespace {

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

std::string asciiLower(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool isApfsCompressionOrResourceXattrName(const std::string& name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return n == "com.apple.decmpfs" ||
           n == "com.apple.resourcefork" ||
           n.find("decmpfs") != std::string::npos ||
           n.find("resourcefork") != std::string::npos;
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

} // namespace

bool shouldWriteAff4ApfsStructuralDiagnostics(bool verbose,
                                              bool diagnosticFullNativeDb,
                                              bool aff4ApfsDiagnosticOutputs) {
    return verbose || diagnosticFullNativeDb || aff4ApfsDiagnosticOutputs;
}

std::string aff4ApfsStructuralDiagnosticsSuppressedStatus() {
    return "aff4_apfs_structural_diagnostics_suppressed";
}

std::string aff4ApfsStructuralDiagnosticsSuppressedGuidance() {
    return "use --aff4-apfs-diagnostic-outputs or --verbose to write structural probe CSVs";
}

void writeAff4ApfsContainerViewOutputs(const fs::path& caseDir,
                                       const EvidenceSource& source,
                                       const fs::path& originalInput,
                                       const ApfsNxSuperblockSummary& nx,
                                       const std::vector<ApfsCheckpointDescriptorRow>& descriptorRows,
                                       Logger& log) {
    const fs::path nxCsvPath = caseDir / "aff4_apfs_container_superblock.csv";
    const fs::path nxJsonPath = caseDir / "aff4_apfs_container_superblock_summary.json";
    const fs::path descCsvPath = caseDir / "aff4_apfs_checkpoint_descriptor_scan.csv";
    const fs::path mdPath = caseDir / "AFF4_APFS_CONTAINER_VIEW.md";

    std::size_t apsbHits = 0;
    std::size_t nxsbHits = 0;
    std::size_t descriptorReadOk = 0;
    for (const auto& r : descriptorRows) {
        if (r.magic == "APSB") ++apsbHits;
        if (r.magic == "NXSB") ++nxsbHits;
        if (r.status == "READ_OK") ++descriptorReadOk;
    }

    try {
        std::ofstream out(nxCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,attempted,found,virtual_offset,bytes_read,validation_status,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,block_size,block_count,container_size_bytes,features,readonly_compatible_features,incompatible_features,container_uuid,next_oid,next_xid,xp_desc_blocks,xp_data_blocks,xp_desc_base,xp_data_base,xp_desc_next,xp_data_next,xp_desc_index,xp_desc_len,xp_data_index,xp_data_len,spaceman_oid,omap_oid,reaper_oid,test_type,max_file_systems,nonzero_fs_oid_count,fs_oids,notes\n";
        out << csvEscape(source.sourceId) << ','
            << csvEscape(pathString(originalInput)) << ','
            << csvEscape(inputSourceType(originalInput)) << ','
            << (nx.attempted ? "true" : "false") << ','
            << (nx.found ? "true" : "false") << ','
            << nx.virtualOffset << ','
            << nx.bytesRead << ','
            << csvEscape(nx.validationStatus) << ','
            << nx.oid << ',' << nx.xid << ',' << nx.objectTypeRaw << ',' << csvEscape(nx.objectTypeLabel) << ',' << nx.objectSubtype << ','
            << nx.blockSize << ',' << nx.blockCount << ',' << nx.containerSizeBytes << ','
            << nx.features << ',' << nx.readonlyCompatibleFeatures << ',' << nx.incompatibleFeatures << ','
            << csvEscape(nx.containerUuid) << ',' << nx.nextOid << ',' << nx.nextXid << ','
            << nx.xpDescBlocks << ',' << nx.xpDataBlocks << ',' << nx.xpDescBase << ',' << nx.xpDataBase << ','
            << nx.xpDescNext << ',' << nx.xpDataNext << ',' << nx.xpDescIndex << ',' << nx.xpDescLen << ',' << nx.xpDataIndex << ',' << nx.xpDataLen << ','
            << nx.spacemanOid << ',' << nx.omapOid << ',' << nx.reaperOid << ',' << nx.testType << ',' << nx.maxFileSystems << ','
            << nx.fsOids.size() << ',' << csvEscape(joinU64List(nx.fsOids, 64)) << ',' << csvEscape(nx.notes) << "\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_container_superblock.csv: ") + ex.what());
    }

    try {
        std::ofstream out(descCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,physical_block,virtual_offset,bytes_read,status,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,magic,interpretation,sample_hex,notes\n";
        for (const auto& r : descriptorRows) {
            out << csvEscape(source.sourceId) << ','
                << csvEscape(pathString(originalInput)) << ','
                << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.physicalBlock << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                << csvEscape(r.status) << ',' << r.oid << ',' << r.xid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ','
                << csvEscape(r.magic) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_checkpoint_descriptor_scan.csv: ") + ex.what());
    }

    try {
        std::ofstream out(nxJsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_VIA_LIBAFF4_APFS_CONTAINER_VIEW\",\n";
        out << "  \"attempted\": " << (nx.attempted ? "true" : "false") << ",\n";
        out << "  \"found\": " << (nx.found ? "true" : "false") << ",\n";
        out << "  \"validation_status\": \"" << jsonEscape(nx.validationStatus) << "\",\n";
        out << "  \"block_size\": " << nx.blockSize << ",\n";
        out << "  \"block_count\": " << nx.blockCount << ",\n";
        out << "  \"container_size_bytes\": " << nx.containerSizeBytes << ",\n";
        out << "  \"container_uuid\": \"" << jsonEscape(nx.containerUuid) << "\",\n";
        out << "  \"object_oid\": " << nx.oid << ",\n";
        out << "  \"object_xid\": " << nx.xid << ",\n";
        out << "  \"spaceman_oid\": " << nx.spacemanOid << ",\n";
        out << "  \"omap_oid\": " << nx.omapOid << ",\n";
        out << "  \"reaper_oid\": " << nx.reaperOid << ",\n";
        out << "  \"xp_desc_base\": " << nx.xpDescBase << ",\n";
        out << "  \"xp_desc_blocks\": " << nx.xpDescBlocks << ",\n";
        out << "  \"xp_desc_index\": " << nx.xpDescIndex << ",\n";
        out << "  \"xp_desc_len\": " << nx.xpDescLen << ",\n";
        out << "  \"max_file_systems\": " << nx.maxFileSystems << ",\n";
        out << "  \"nonzero_fs_oid_count\": " << nx.fsOids.size() << ",\n";
        out << "  \"descriptor_rows\": " << descriptorRows.size() << ",\n";
        out << "  \"descriptor_read_ok\": " << descriptorReadOk << ",\n";
        out << "  \"descriptor_nxsb_hits\": " << nxsbHits << ",\n";
        out << "  \"descriptor_apsb_hits\": " << apsbHits << "\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_container_superblock_summary.json: ") + ex.what());
    }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Container View\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This view uses bounded libaff4 virtual reads against the exact `--input` AFF4 file. It does not search parent folders, does not scan `O:\\\\`, does not call aff4imager, and does not export RAW/DD.\n\n";
        out << "## Container superblock\n\n";
        out << "- Found NXSB: `" << (nx.found ? "true" : "false") << "`\n";
        out << "- Validation: `" << nx.validationStatus << "`\n";
        out << "- Block size: `" << nx.blockSize << "`\n";
        out << "- Block count: `" << nx.blockCount << "`\n";
        out << "- Container size bytes: `" << nx.containerSizeBytes << "`\n";
        out << "- Container UUID: `" << nx.containerUuid << "`\n";
        out << "- Spaceman OID: `" << nx.spacemanOid << "`\n";
        out << "- Object map OID: `" << nx.omapOid << "`\n";
        out << "- Reaper OID: `" << nx.reaperOid << "`\n";
        out << "- Non-zero filesystem OIDs: `" << joinU64List(nx.fsOids, 64) << "`\n\n";
        out << "## Checkpoint descriptor scan\n\n";
        out << "- Rows: `" << descriptorRows.size() << "`\n";
        out << "- Read OK rows: `" << descriptorReadOk << "`\n";
        out << "- NXSB hits: `" << nxsbHits << "`\n";
        out << "- APSB hits: `" << apsbHits << "`\n\n";
        out << "## Interpretation\n\n";
        out << "`NXSB_PARSED` confirms that the selected AFF4 object exposes an APFS container at virtual offset 0. The next implementation step is to resolve object-map/checkpoint metadata enough to translate filesystem object IDs into physical block reads and then enumerate APFS volume superblocks.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_CONTAINER_VIEW.md: ") + ex.what());
    }
}


void writeAff4ApfsVolumeSuperblockOutputs(const fs::path& caseDir,
                                         const EvidenceSource& source,
                                         const fs::path& originalInput,
                                         const ApfsNxSuperblockSummary& nx,
                                         const std::vector<ApfsVolumeSuperblockRow>& volumeRows,
                                         Logger& log) {
    const fs::path csvPath = caseDir / "aff4_apfs_volume_superblocks.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_volume_superblocks_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_VOLUME_SUPERBLOCK_PROBE.md";
    std::size_t readOk = 0;
    std::size_t apsbFound = 0;
    for (const auto& r : volumeRows) {
        if (r.bytesRead > 0) ++readOk;
        if (r.magic == "APSB") ++apsbFound;
    }
    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,fs_oid,virtual_offset,bytes_read,status,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,magic,fs_index_candidate,features_candidate,readonly_compatible_features_candidate,incompatible_features_candidate,unmount_time_candidate,volume_uuid_candidate,interpretation,sample_hex,notes\n";
        for (const auto& r : volumeRows) {
            out << csvEscape(source.sourceId) << ','
                << csvEscape(pathString(originalInput)) << ','
                << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.fsOid << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                << csvEscape(r.status) << ',' << r.oid << ',' << r.xid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ','
                << csvEscape(r.magic) << ',' << r.fsIndexCandidate << ',' << r.featuresCandidate << ',' << r.readonlyCompatibleFeaturesCandidate << ',' << r.incompatibleFeaturesCandidate << ',' << r.unmountTimeCandidate << ','
                << csvEscape(r.volumeUuidCandidate) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_volume_superblocks.csv: ") + ex.what());
    }
    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_VIA_LIBAFF4_APFS_VOLUME_SUPERBLOCK_PROBE\",\n";
        out << "  \"attempted\": " << (nx.found ? "true" : "false") << ",\n";
        out << "  \"container_block_size\": " << nx.blockSize << ",\n";
        out << "  \"container_block_count\": " << nx.blockCount << ",\n";
        out << "  \"nonzero_fs_oid_count\": " << nx.fsOids.size() << ",\n";
        out << "  \"fs_oids\": \"" << jsonEscape(joinU64List(nx.fsOids, 64)) << "\",\n";
        out << "  \"rows\": " << volumeRows.size() << ",\n";
        out << "  \"read_ok\": " << readOk << ",\n";
        out << "  \"apsb_found\": " << apsbFound << "\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_volume_superblocks_summary.json: ") + ex.what());
    }
    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Volume Superblock Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This probe uses the already-open libaff4 handle for the one explicit AFF4 input file. It reads candidate APFS filesystem object IDs from the parsed NX superblock and attempts bounded reads at `fs_oid * nx_block_size`. It does not search `O:\\\\`, does not enumerate parent folders, does not call aff4imager, and does not export RAW/DD.\n\n";
        out << "## Summary\n\n";
        out << "- Container NXSB parsed: `" << (nx.found ? "true" : "false") << "`\n";
        out << "- Block size: `" << nx.blockSize << "`\n";
        out << "- Non-zero filesystem OIDs: `" << joinU64List(nx.fsOids, 64) << "`\n";
        out << "- Probe rows: `" << volumeRows.size() << "`\n";
        out << "- APSB hits: `" << apsbFound << "`\n\n";
        out << "## Interpretation\n\n";
        out << "Rows with `APSB_FOUND` are APFS volume-superblock candidates and should be used as the starting point for the next APFS volume metadata/object-map work. Candidate fields are intentionally labelled as candidates until the APFS volume-superblock parser is broadened and validated.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_VOLUME_SUPERBLOCK_PROBE.md: ") + ex.what());
    }
}


void writeAff4ApfsResolvedVolumeOutputs(const fs::path& caseDir,
                                        const EvidenceSource& source,
                                        const fs::path& originalInput,
                                        const ApfsNxSuperblockSummary& nx,
                                        const std::vector<ApfsResolvedVolumeSuperblockRow>& resolvedRows,
                                        const std::vector<ApfsVolumeOmapProbeRow>& volumeOmapRows,
                                        const std::vector<ApfsVolumeRootTreeLookupRow>& rootLookupRows,
                                        bool strictSingleAff4,
                                        Logger& log) {
    const fs::path resolvedCsvPath = caseDir / "aff4_apfs_resolved_volume_superblocks.csv";
    const fs::path resolvedJsonPath = caseDir / "aff4_apfs_resolved_volume_superblocks_summary.json";
    const fs::path resolvedMdPath = caseDir / "AFF4_APFS_RESOLVED_VOLUME_SUPERBLOCKS.md";
    const fs::path volumeOmapCsvPath = caseDir / "aff4_apfs_volume_omap_probe.csv";
    const fs::path volumeOmapMdPath = caseDir / "AFF4_APFS_VOLUME_OMAP_PROBE.md";

    std::size_t apsbParsed = 0;
    std::size_t omittedByDeletedFlag = 0;
    for (const auto& r : resolvedRows) {
        if (r.status == "APSB_PARSED_FROM_CONTAINER_OMAP") ++apsbParsed;
        if ((r.omapValueFlags & 0x00000001U) != 0U) ++omittedByDeletedFlag;
    }
    std::size_t volumeOmapParsed = 0;
    std::size_t volumeOmapTreeRead = 0;
    std::size_t volumeOmapTreeLeaf = 0;
    for (const auto& r : volumeOmapRows) {
        if (r.omapStatus == "VOLUME_OMAP_PARSED") ++volumeOmapParsed;
        if (r.treeStatus == "VOLUME_OMAP_BTREE_ROOT_READ") ++volumeOmapTreeRead;
        if (r.treeStatus == "VOLUME_OMAP_BTREE_ROOT_READ" && r.treeBtnLevel == 0) ++volumeOmapTreeLeaf;
    }

    try {
        std::ofstream out(resolvedCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,target_role,fs_oid,container_target_xid,omap_key_oid,omap_key_xid,omap_value_flags,omap_value_size,omap_value_paddr,resolved_virtual_offset,resolved_bytes_read,status,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,magic,fs_index,features,readonly_compatible_features,incompatible_features,unmount_time,fs_reserve_block_count,fs_quota_block_count,fs_alloc_count,root_tree_type,extentref_tree_type,snap_meta_tree_type,apfs_omap_oid,apfs_root_tree_oid,apfs_extentref_tree_oid,apfs_snap_meta_tree_oid,apfs_revert_to_xid,apfs_revert_to_sblock_oid,apfs_next_obj_id,apfs_num_files,apfs_num_directories,apfs_num_symlinks,apfs_num_other_fsobjects,apfs_num_snapshots,apfs_total_blocks_alloced,apfs_total_blocks_freed,apfs_vol_uuid,apfs_last_mod_time,apfs_fs_flags,apfs_volname,apfs_next_doc_id,apfs_role,interpretation,sample_hex,notes\n";
        for (const auto& r : resolvedRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << r.containerTargetXid << ','
                << r.omapKeyOid << ',' << r.omapKeyXid << ',' << r.omapValueFlags << ',' << r.omapValueSize << ',' << r.omapValuePaddr << ','
                << r.resolvedVirtualOffset << ',' << r.resolvedBytesRead << ',' << csvEscape(r.status) << ','
                << r.objectOid << ',' << r.objectXid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ',' << csvEscape(r.magic) << ','
                << r.fsIndex << ',' << r.features << ',' << r.readonlyCompatibleFeatures << ',' << r.incompatibleFeatures << ',' << r.unmountTime << ','
                << r.reserveBlockCount << ',' << r.quotaBlockCount << ',' << r.allocBlockCount << ',' << r.rootTreeType << ',' << r.extentrefTreeType << ',' << r.snapMetaTreeType << ','
                << r.apfsOmapOid << ',' << r.rootTreeOid << ',' << r.extentrefTreeOid << ',' << r.snapMetaTreeOid << ',' << r.revertToXid << ',' << r.revertToSblockOid << ','
                << r.nextObjId << ',' << r.numFiles << ',' << r.numDirectories << ',' << r.numSymlinks << ',' << r.numOtherFsobjects << ',' << r.numSnapshots << ','
                << r.totalBlocksAlloced << ',' << r.totalBlocksFreed << ',' << csvEscape(r.volumeUuid) << ',' << r.lastModTime << ',' << r.fsFlags << ',' << csvEscape(r.volumeName) << ','
                << r.nextDocId << ',' << r.role << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_resolved_volume_superblocks.csv: ") + ex.what());
    }

    try {
        std::ofstream out(resolvedJsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_CONTAINER_OMAP_RESOLVED_APSB\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"container_block_size\": " << nx.blockSize << ",\n";
        out << "  \"container_next_xid\": " << nx.nextXid << ",\n";
        out << "  \"nonzero_fs_oid_count\": " << nx.fsOids.size() << ",\n";
        out << "  \"resolved_rows\": " << resolvedRows.size() << ",\n";
        out << "  \"apsb_parsed\": " << apsbParsed << ",\n";
        out << "  \"rows_with_omap_deleted_flag\": " << omittedByDeletedFlag << ",\n";
        out << "  \"volume_omap_probe_rows\": " << volumeOmapRows.size() << ",\n";
        out << "  \"volume_omap_parsed\": " << volumeOmapParsed << ",\n";
        out << "  \"volume_omap_btree_root_read\": " << volumeOmapTreeRead << ",\n";
        out << "  \"volume_omap_btree_leaf_roots\": " << volumeOmapTreeLeaf << ",\n";
        out << "  \"volume_root_tree_lookup_rows\": " << rootLookupRows.size() << "\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_resolved_volume_superblocks_summary.json: ") + ex.what());
    }

    try {
        std::ofstream out(resolvedMdPath, std::ios::binary);
        out << "# AFF4 APFS Resolved Volume Superblocks\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This report parses APFS volume superblocks only after an NX filesystem OID is selected through the parsed container object map. It uses bounded virtual reads through the active AFF4 reader path against the one explicit AFF4 input file. It does not scan `O:\\`, enumerate parent folders, mount volumes, call aff4imager, or export RAW/DD.\n\n";
        out << "## Summary\n\n";
        out << "- Container NXSB parsed: `" << (nx.found ? "true" : "false") << "`\n";
        out << "- Container block size: `" << nx.blockSize << "`\n";
        out << "- Non-zero NX filesystem OIDs: `" << joinU64List(nx.fsOids, 64) << "`\n";
        out << "- Resolved volume-superblock rows: `" << resolvedRows.size() << "`\n";
        out << "- APSB rows parsed: `" << apsbParsed << "`\n";
        out << "- Volume object-map rows: `" << volumeOmapRows.size() << "`\n";
        out << "- Volume root-tree lookup rows: `" << rootLookupRows.size() << "`\n\n";
        out << "## Interpretation\n\n";
        out << "Rows with `APSB_PARSED_FROM_CONTAINER_OMAP` are the resolved APFS volume superblocks selected from the container object map. The important next fields are `apfs_omap_oid` and `apfs_root_tree_oid`; those feed the volume object-map and catalog/root-tree phase.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_RESOLVED_VOLUME_SUPERBLOCKS.md: ") + ex.what());
    }

    try {
        std::ofstream out(volumeOmapCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,target_role,fs_oid,volume_object_oid,volume_object_xid,apfs_omap_oid,apfs_root_tree_oid,omap_virtual_offset,omap_bytes_read,omap_status,omap_object_oid,omap_object_xid,omap_object_type_raw,omap_object_type_label,omap_object_subtype,om_flags,om_snapshot_count,om_tree_type,om_snapshot_tree_type,om_tree_oid,om_snapshot_tree_oid,om_most_recent_snap,om_pending_revert_min,om_pending_revert_max,tree_virtual_offset,tree_bytes_read,tree_status,tree_object_oid,tree_object_xid,tree_object_type_raw,tree_object_type_label,tree_object_subtype,tree_btn_flags,tree_btn_level,tree_btn_nkeys,tree_table_space_offset,tree_table_space_length,interpretation,sample_hex,tree_sample_hex,notes\n";
        for (const auto& r : volumeOmapRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << r.volumeObjectOid << ',' << r.volumeObjectXid << ','
                << r.apfsOmapOid << ',' << r.apfsRootTreeOid << ',' << r.omapVirtualOffset << ',' << r.omapBytesRead << ',' << csvEscape(r.omapStatus) << ','
                << r.omapObjectOid << ',' << r.omapObjectXid << ',' << r.omapObjectTypeRaw << ',' << csvEscape(r.omapObjectTypeLabel) << ',' << r.omapObjectSubtype << ','
                << r.omFlags << ',' << r.omSnapshotCount << ',' << r.omTreeType << ',' << r.omSnapshotTreeType << ',' << r.omTreeOid << ',' << r.omSnapshotTreeOid << ','
                << r.omMostRecentSnap << ',' << r.omPendingRevertMin << ',' << r.omPendingRevertMax << ','
                << r.treeVirtualOffset << ',' << r.treeBytesRead << ',' << csvEscape(r.treeStatus) << ',' << r.treeObjectOid << ',' << r.treeObjectXid << ',' << r.treeObjectTypeRaw << ','
                << csvEscape(r.treeObjectTypeLabel) << ',' << r.treeObjectSubtype << ',' << r.treeBtnFlags << ',' << r.treeBtnLevel << ',' << r.treeBtnNkeys << ',' << r.treeTableSpaceOffset << ',' << r.treeTableSpaceLength << ','
                << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.treeSampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_volume_omap_probe.csv: ") + ex.what());
    }

    try {
        std::ofstream out(volumeOmapMdPath, std::ios::binary);
        out << "# AFF4 APFS Volume OMAP Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This probe reads each parsed volume superblock's `apfs_omap_oid` as a physical APFS object through the selected AFF4 file, then performs one bounded read of the volume object-map B-tree root referenced by `om_tree_oid`. It is a controlled reader probe only; it does not traverse catalog B-trees or extract files yet.\n\n";
        out << "## Summary\n\n";
        out << "- Volume OMAP probe rows: `" << volumeOmapRows.size() << "`\n";
        out << "- Volume OMAP physical objects parsed: `" << volumeOmapParsed << "`\n";
        out << "- Volume OMAP B-tree roots read: `" << volumeOmapTreeRead << "`\n";
        out << "- Volume OMAP leaf roots: `" << volumeOmapTreeLeaf << "`\n\n";
        out << "## Next step\n\n";
        out << "Use `apfs_root_tree_oid` with the parsed volume object map. If the volume OMAP B-tree root is not a leaf, branch traversal must be implemented before catalog/root-tree lookup can resolve virtual filesystem objects.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_VOLUME_OMAP_PROBE.md: ") + ex.what());
    }
}

void writeAff4ApfsVolumeRootTreeLookupOutputs(const fs::path& caseDir,
                                              const EvidenceSource& source,
                                              const fs::path& originalInput,
                                              const std::vector<ApfsVolumeRootTreeLookupRow>& rows,
                                              bool strictSingleAff4,
                                              Logger& log) {
    const fs::path csvPath = caseDir / "aff4_apfs_volume_root_tree_lookup.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_volume_root_tree_lookup_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_VOLUME_ROOT_TREE_LOOKUP.md";

    std::size_t resolved = 0;
    std::size_t btreeRead = 0;
    std::size_t branchTraversal = 0;
    std::size_t noMatch = 0;
    for (const auto& r : rows) {
        if (r.lookupStatus == "VOLUME_ROOT_TREE_LOOKUP_RESOLVED") ++resolved;
        if (r.rootTreeStatus == "ROOT_TREE_BTREE_READ") ++btreeRead;
        if (r.branchDepth > 0) ++branchTraversal;
        if (r.lookupStatus.find("NO_MATCH") != std::string::npos) ++noMatch;
    }

    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,target_role,fs_oid,volume_name,apfs_omap_oid,om_tree_oid,apfs_root_tree_oid,target_xid,branch_depth,branch_path,leaf_oid,leaf_virtual_offset,leaf_bytes_read,leaf_btn_flags,leaf_btn_level,leaf_btn_nkeys,matched_entry_index,matched_key_oid,matched_key_xid,value_flags,value_size,value_paddr,resolved_virtual_offset,resolved_bytes_read,resolved_object_oid,resolved_object_xid,resolved_object_type_raw,resolved_object_type_label,resolved_object_subtype,resolved_magic,resolved_btn_flags,resolved_btn_level,resolved_btn_nkeys,lookup_status,root_tree_status,interpretation,sample_hex,resolved_sample_hex,notes\n";
        for (const auto& r : rows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.apfsOmapOid << ',' << r.omTreeOid << ',' << r.apfsRootTreeOid << ',' << r.targetXid << ',' << r.branchDepth << ',' << csvEscape(r.branchPath) << ','
                << r.leafOid << ',' << r.leafVirtualOffset << ',' << r.leafBytesRead << ',' << r.leafBtnFlags << ',' << r.leafBtnLevel << ',' << r.leafBtnNkeys << ','
                << r.matchedEntryIndex << ',' << r.matchedKeyOid << ',' << r.matchedKeyXid << ',' << r.valueFlags << ',' << r.valueSize << ',' << r.valuePaddr << ','
                << r.resolvedVirtualOffset << ',' << r.resolvedBytesRead << ',' << r.resolvedObjectOid << ',' << r.resolvedObjectXid << ',' << r.resolvedObjectTypeRaw << ','
                << csvEscape(r.resolvedObjectTypeLabel) << ',' << r.resolvedObjectSubtype << ',' << csvEscape(r.resolvedMagic) << ',' << r.resolvedBtnFlags << ',' << r.resolvedBtnLevel << ',' << r.resolvedBtnNkeys << ','
                << csvEscape(r.lookupStatus) << ',' << csvEscape(r.rootTreeStatus) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.resolvedSampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_volume_root_tree_lookup.csv: ") + ex.what());
    }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_VOLUME_OMAP_ROOT_TREE_LOOKUP\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"lookup_rows\": " << rows.size() << ",\n";
        out << "  \"resolved_root_tree_objects\": " << resolved << ",\n";
        out << "  \"root_tree_btree_reads\": " << btreeRead << ",\n";
        out << "  \"rows_requiring_branch_traversal\": " << branchTraversal << ",\n";
        out << "  \"no_match_rows\": " << noMatch << "\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_volume_root_tree_lookup_summary.json: ") + ex.what());
    }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Volume Root-Tree Lookup\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This probe uses each parsed APSB `apfs_root_tree_oid` as the target key in that volume's object map. It performs bounded target-guided traversal of the volume OMAP B-tree when the OMAP root is a branch, resolves the matching OMAP value physical address, and reads the resulting root-tree object header. It does not traverse catalog records or extract files yet.\n\n";
        out << "## Summary\n\n";
        out << "- Lookup rows: `" << rows.size() << "`\n";
        out << "- Resolved root-tree objects: `" << resolved << "`\n";
        out << "- Root-tree B-tree headers read: `" << btreeRead << "`\n";
        out << "- Rows using branch traversal: `" << branchTraversal << "`\n";
        out << "- No-match rows: `" << noMatch << "`\n\n";
        out << "## Next step\n\n";
        out << "If root-tree B-tree headers are read successfully, the next implementation step is bounded catalog/root-tree node traversal and safe directory-record decoding, still through the single selected AFF4 file.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_VOLUME_ROOT_TREE_LOOKUP.md: ") + ex.what());
    }
}


void writeAff4ApfsRootTreeNodeProbeOutputs(const fs::path& caseDir,
                                           const EvidenceSource& source,
                                           const fs::path& originalInput,
                                           const std::vector<ApfsRootTreeNodeProbeRow>& nodeRows,
                                           const std::vector<ApfsRootTreeRecordSampleRow>& recordRows,
                                           bool strictSingleAff4,
                                           Logger& log) {
    const fs::path nodeCsvPath = caseDir / "aff4_apfs_root_tree_node_probe.csv";
    const fs::path recordCsvPath = caseDir / "aff4_apfs_root_tree_record_sample.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_root_tree_node_probe_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_ROOT_TREE_NODE_PROBE.md";

    std::size_t btreeRows = 0;
    std::size_t branchRows = 0;
    std::size_t leafRows = 0;
    std::size_t dirRecordSamples = 0;
    std::size_t branchChildSamples = 0;
    for (const auto& r : nodeRows) {
        if (r.status == "ROOT_TREE_NODE_HEADER_PARSED") ++btreeRows;
        if (r.status == "ROOT_TREE_NODE_HEADER_PARSED" && r.btnLevel > 0) ++branchRows;
        if (r.status == "ROOT_TREE_NODE_HEADER_PARSED" && r.btnLevel == 0) ++leafRows;
    }
    for (const auto& r : recordRows) {
        if (r.keyTypeLabel == "DIR_REC") ++dirRecordSamples;
        if (r.branchChildOid != 0) ++branchChildSamples;
    }

    try {
        std::ofstream out(nodeCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,target_role,fs_oid,volume_name,apfs_root_tree_oid,target_xid,node_oid,virtual_offset,bytes_read,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,magic,btn_flags,btn_level,btn_nkeys,table_space_offset,table_space_length,free_space_offset,free_space_length,status,interpretation,sample_hex,notes\n";
        for (const auto& r : nodeRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.apfsRootTreeOid << ',' << r.targetXid << ',' << r.nodeOid << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                << r.objectOid << ',' << r.objectXid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ',' << csvEscape(r.magic) << ','
                << r.btnFlags << ',' << r.btnLevel << ',' << r.btnNkeys << ',' << r.tableSpaceOffset << ',' << r.tableSpaceLength << ',' << r.freeSpaceOffset << ',' << r.freeSpaceLength << ','
                << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_root_tree_node_probe.csv: ") + ex.what()); }

    try {
        std::ofstream out(recordCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,target_role,fs_oid,volume_name,apfs_root_tree_oid,node_oid,node_virtual_offset,node_level,node_nkeys,entry_index,toc_offset,key_offset,key_length,value_offset,value_length,key_raw,key_object_id,key_type_raw,key_type_label,branch_child_oid,decoded_name,status,interpretation,key_sample_hex,value_sample_hex,notes\n";
        for (const auto& r : recordRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.apfsRootTreeOid << ',' << r.nodeOid << ',' << r.nodeVirtualOffset << ',' << r.nodeLevel << ',' << r.nodeNkeys << ',' << r.entryIndex << ',' << r.tocOffset << ','
                << r.keyOffset << ',' << r.keyLength << ',' << r.valueOffset << ',' << r.valueLength << ',' << r.keyRaw << ',' << r.keyObjectId << ',' << static_cast<unsigned int>(r.keyTypeRaw) << ',' << csvEscape(r.keyTypeLabel) << ','
                << r.branchChildOid << ',' << csvEscape(r.decodedName) << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.keySampleHex) << ',' << csvEscape(r.valueSampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_root_tree_record_sample.csv: ") + ex.what()); }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_APFS_ROOT_TREE_NODE_KEY_SAMPLE\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"node_probe_rows\": " << nodeRows.size() << ",\n";
        out << "  \"btree_node_headers_parsed\": " << btreeRows << ",\n";
        out << "  \"branch_root_nodes\": " << branchRows << ",\n";
        out << "  \"leaf_root_nodes\": " << leafRows << ",\n";
        out << "  \"record_sample_rows\": " << recordRows.size() << ",\n";
        out << "  \"branch_child_candidate_samples\": " << branchChildSamples << ",\n";
        out << "  \"directory_record_samples\": " << dirRecordSamples << "\n";
        out << "}\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_root_tree_node_probe_summary.json: ") + ex.what()); }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Root-Tree Node Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This bounded probe reads the APFS filesystem root-tree objects that were already resolved through each volume OMAP. It samples the B-tree root-node table of contents and decodes the generic APFS filesystem key object/type fields. It does not yet perform full filesystem traversal, path reconstruction, or file extraction.\n\n";
        out << "## Summary\n\n";
        out << "- Node probe rows: `" << nodeRows.size() << "`\n";
        out << "- B-tree node headers parsed: `" << btreeRows << "`\n";
        out << "- Branch root nodes: `" << branchRows << "`\n";
        out << "- Leaf root nodes: `" << leafRows << "`\n";
        out << "- Root-node record samples: `" << recordRows.size() << "`\n";
        out << "- Branch-child candidate samples: `" << branchChildSamples << "`\n";
        out << "- Directory-record samples: `" << dirRecordSamples << "`\n\n";
        out << "## Next step\n\n";
        out << "Use branch-child candidates from `aff4_apfs_root_tree_record_sample.csv` as targets for the same volume OMAP resolver, then walk down to leaf nodes and begin bounded directory-record decoding. The same normalized path/inventory abstractions should later be reused by the iOS CoreSpotlight ZIP/full-file-system track.\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write AFF4_APFS_ROOT_TREE_NODE_PROBE.md: ") + ex.what()); }
}


std::string upperAsciiCopy(std::string v) {
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return v;
}

void writeAff4ApfsRootTreeTraversalProbeOutputs(const fs::path& caseDir,
                                                const EvidenceSource& source,
                                                const fs::path& originalInput,
                                                const std::vector<ApfsRootTreeChildNodeProbeRow>& traversalRows,
                                                const std::vector<ApfsRootTreeRecordSampleRow>& recordRows,
                                                const std::string& traversalLevelName,
                                                bool strictSingleAff4,
                                                Logger& log) {
    const std::string level = traversalLevelName.empty() ? "node" : traversalLevelName;
    const fs::path csvPath = caseDir / ("aff4_apfs_root_tree_" + level + "_node_probe.csv");
    const fs::path recordCsvPath = caseDir / ("aff4_apfs_root_tree_" + level + "_record_sample.csv");
    const fs::path jsonPath = caseDir / ("aff4_apfs_root_tree_" + level + "_node_probe_summary.json");
    const fs::path mdPath = caseDir / ("AFF4_APFS_ROOT_TREE_" + upperAsciiCopy(level) + "_NODE_PROBE.md");

    std::size_t resolvedRows = 0;
    std::size_t btreeRows = 0;
    std::size_t branchRows = 0;
    std::size_t leafRows = 0;
    std::size_t dirRecordSamples = 0;
    std::size_t branchSamples = 0;
    for (const auto& r : traversalRows) {
        if (r.lookupStatus == "OMAP_TARGET_LOOKUP_RESOLVED") ++resolvedRows;
        if (r.childNodeStatus.find("BTREE_READ") != std::string::npos) {
            ++btreeRows;
            if (r.resolvedBtnLevel > 0) ++branchRows;
            else ++leafRows;
        }
    }
    for (const auto& r : recordRows) {
        if (r.keyTypeLabel == "DIR_REC") ++dirRecordSamples;
        if (r.branchChildOid != 0) ++branchSamples;
    }

    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,source_record_sequence,volume_sequence,target_role,fs_oid,volume_name,apfs_root_tree_oid,parent_node_oid,parent_node_virtual_offset,parent_node_level,parent_entry_index,branch_child_oid,target_xid,omap_branch_depth,omap_branch_path,omap_leaf_oid,omap_leaf_virtual_offset,omap_leaf_bytes_read,matched_entry_index,matched_key_oid,matched_key_xid,value_flags,value_size,value_paddr,resolved_virtual_offset,resolved_bytes_read,resolved_object_oid,resolved_object_xid,resolved_object_type_raw,resolved_object_type_label,resolved_object_subtype,resolved_magic,resolved_btn_flags,resolved_btn_level,resolved_btn_nkeys,lookup_status,child_node_status,interpretation,sample_hex,resolved_sample_hex,notes\n";
        for (const auto& r : traversalRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.sourceRecordSequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.apfsRootTreeOid << ',' << r.parentNodeOid << ',' << r.parentNodeVirtualOffset << ',' << r.parentNodeLevel << ',' << r.parentEntryIndex << ',' << r.branchChildOid << ',' << r.targetXid << ','
                << r.omapBranchDepth << ',' << csvEscape(r.omapBranchPath) << ',' << r.omapLeafOid << ',' << r.omapLeafVirtualOffset << ',' << r.omapLeafBytesRead << ','
                << r.matchedEntryIndex << ',' << r.matchedKeyOid << ',' << r.matchedKeyXid << ',' << r.valueFlags << ',' << r.valueSize << ',' << r.valuePaddr << ','
                << r.resolvedVirtualOffset << ',' << r.resolvedBytesRead << ',' << r.resolvedObjectOid << ',' << r.resolvedObjectXid << ',' << r.resolvedObjectTypeRaw << ',' << csvEscape(r.resolvedObjectTypeLabel) << ',' << r.resolvedObjectSubtype << ',' << csvEscape(r.resolvedMagic) << ','
                << r.resolvedBtnFlags << ',' << r.resolvedBtnLevel << ',' << r.resolvedBtnNkeys << ',' << csvEscape(r.lookupStatus) << ',' << csvEscape(r.childNodeStatus) << ','
                << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.resolvedSampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write APFS root-tree traversal probe CSV: ") + ex.what()); }

    try {
        std::ofstream out(recordCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,target_role,fs_oid,volume_name,apfs_root_tree_oid,node_oid,node_virtual_offset,node_level,node_nkeys,entry_index,toc_offset,key_offset,key_length,value_offset,value_length,key_raw,key_object_id,key_type_raw,key_type_label,branch_child_oid,decoded_name,status,interpretation,key_sample_hex,value_sample_hex,notes\n";
        for (const auto& r : recordRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.apfsRootTreeOid << ',' << r.nodeOid << ',' << r.nodeVirtualOffset << ',' << r.nodeLevel << ',' << r.nodeNkeys << ',' << r.entryIndex << ',' << r.tocOffset << ','
                << r.keyOffset << ',' << r.keyLength << ',' << r.valueOffset << ',' << r.valueLength << ',' << r.keyRaw << ',' << r.keyObjectId << ',' << static_cast<unsigned int>(r.keyTypeRaw) << ',' << csvEscape(r.keyTypeLabel) << ','
                << r.branchChildOid << ',' << csvEscape(r.decodedName) << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.keySampleHex) << ',' << csvEscape(r.valueSampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write APFS root-tree traversal record sample CSV: ") + ex.what()); }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_APFS_ROOT_TREE_" << upperAsciiCopy(level) << "_NODE_SAMPLE\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"traversal_level\": \"" << jsonEscape(level) << "\",\n";
        out << "  \"traversal_probe_rows\": " << traversalRows.size() << ",\n";
        out << "  \"omap_resolved_rows\": " << resolvedRows << ",\n";
        out << "  \"btree_node_headers_parsed\": " << btreeRows << ",\n";
        out << "  \"branch_nodes\": " << branchRows << ",\n";
        out << "  \"leaf_nodes\": " << leafRows << ",\n";
        out << "  \"record_sample_rows\": " << recordRows.size() << ",\n";
        out << "  \"branch_candidate_samples\": " << branchSamples << ",\n";
        out << "  \"directory_record_samples\": " << dirRecordSamples << "\n";
        out << "}\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write APFS root-tree traversal summary JSON: ") + ex.what()); }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Root-Tree " << level << " Node Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This bounded probe resolves APFS filesystem root-tree " << level << " node candidates through the volume OMAP and samples B-tree records. It remains single-AFF4 only and does not yet reconstruct full paths or copy file extents.\n\n";
        out << "## Summary\n\n";
        out << "- Probe rows: `" << traversalRows.size() << "`\n";
        out << "- OMAP-resolved rows: `" << resolvedRows << "`\n";
        out << "- B-tree node headers parsed: `" << btreeRows << "`\n";
        out << "- Branch nodes: `" << branchRows << "`\n";
        out << "- Leaf nodes: `" << leafRows << "`\n";
        out << "- Record samples: `" << recordRows.size() << "`\n";
        out << "- Branch-candidate samples: `" << branchSamples << "`\n";
        out << "- Directory-record samples: `" << dirRecordSamples << "`\n\n";
        out << "## Next step\n\n";
        out << "Use leaf and directory-record samples to seed bounded filesystem namespace work for identifying `.Spotlight-V100` / `Store-V2` paths without bloating normal investigator databases.\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write APFS root-tree traversal markdown summary: ") + ex.what()); }
}

void writeAff4ApfsFilesystemNamespaceSeedOutputs(const fs::path& caseDir,
                                                const EvidenceSource& source,
                                                const fs::path& originalInput,
                                                const std::vector<ApfsRootTreeRecordSampleRow>& rootRecords,
                                                const std::vector<ApfsRootTreeRecordSampleRow>& childRecords,
                                                const std::vector<ApfsRootTreeRecordSampleRow>& descendantRecords,
                                                bool strictSingleAff4,
                                                Logger& log) {
    const fs::path csvPath = caseDir / "aff4_apfs_filesystem_namespace_seed.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_filesystem_namespace_seed_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_FILESYSTEM_NAMESPACE_SEED.md";

    struct SourcePack { const char* label; const std::vector<ApfsRootTreeRecordSampleRow>* rows; };
    const SourcePack packs[] = {
        {"root", &rootRecords},
        {"child", &childRecords},
        {"descendant", &descendantRecords}
    };

    std::size_t totalRows = 0;
    std::size_t emittedRows = 0;
    std::size_t dirRows = 0;
    std::size_t namedDirRows = 0;
    std::size_t inodeRows = 0;
    std::size_t fileExtentRows = 0;
    std::size_t fileInfoRows = 0;
    std::size_t spotlightNameHits = 0;

    auto isInteresting = [](const ApfsRootTreeRecordSampleRow& r) {
        return r.keyTypeLabel == "DIR_REC" || r.keyTypeLabel == "INODE" ||
               r.keyTypeLabel == "FILE_EXTENT" || r.keyTypeLabel == "FILE_INFO" ||
               r.keyTypeLabel == "DSTREAM_ID" || r.keyTypeLabel == "EXTENT";
    };
    auto lowerCopy = [](std::string v) {
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return v;
    };

    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,sample_source,volume_sequence,target_role,fs_oid,volume_name,node_oid,node_virtual_offset,node_level,node_nkeys,entry_index,key_object_id,key_type_raw,key_type_label,decoded_name,parent_object_id_candidate,child_file_id_candidate,object_id_candidate,value_u64_0,value_u64_1,value_u64_2,spotlight_name_hit,extraction_relevance,status,interpretation,key_sample_hex,value_sample_hex,notes\n";
        for (const auto& pack : packs) {
            for (const auto& r : *pack.rows) {
                ++totalRows;
                if (!isInteresting(r)) continue;
                ++emittedRows;
                if (r.keyTypeLabel == "DIR_REC") ++dirRows;
                if (r.keyTypeLabel == "DIR_REC" && !r.decodedName.empty()) ++namedDirRows;
                if (r.keyTypeLabel == "INODE") ++inodeRows;
                if (r.keyTypeLabel == "FILE_EXTENT") ++fileExtentRows;
                if (r.keyTypeLabel == "FILE_INFO") ++fileInfoRows;
                const std::string lname = lowerCopy(r.decodedName);
                const bool spotlightHit = lname.find("spotlight") != std::string::npos || lname.find("store-v2") != std::string::npos || lname == "store.db" || lname == ".store.db";
                if (spotlightHit) ++spotlightNameHits;
                std::string relevance = "FILESYSTEM_METADATA_SAMPLE";
                if (spotlightHit) relevance = "SPOTLIGHT_NAME_HIT";
                else if (r.keyTypeLabel == "DIR_REC") relevance = "PATH_NAMESPACE_CANDIDATE";
                else if (r.keyTypeLabel == "INODE") relevance = "INODE_METADATA_CANDIDATE";
                else if (r.keyTypeLabel == "FILE_EXTENT") relevance = "FILE_EXTENT_CANDIDATE";
                else if (r.keyTypeLabel == "FILE_INFO") relevance = "FILE_INFO_CANDIDATE";

                const std::uint64_t parentCandidate = (r.keyTypeLabel == "DIR_REC") ? r.keyObjectId : 0;
                const std::uint64_t childFileCandidate = (r.keyTypeLabel == "DIR_REC") ? r.valueU64_0 : 0;
                const std::uint64_t objectCandidate = (r.keyTypeLabel == "DIR_REC") ? childFileCandidate : r.keyObjectId;

                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << r.sequence << ',' << csvEscape(pack.label) << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                    << r.nodeOid << ',' << r.nodeVirtualOffset << ',' << r.nodeLevel << ',' << r.nodeNkeys << ',' << r.entryIndex << ','
                    << r.keyObjectId << ',' << static_cast<unsigned int>(r.keyTypeRaw) << ',' << csvEscape(r.keyTypeLabel) << ',' << csvEscape(r.decodedName) << ','
                    << parentCandidate << ',' << childFileCandidate << ',' << objectCandidate << ','
                    << r.valueU64_0 << ',' << r.valueU64_1 << ',' << r.valueU64_2 << ','
                    << (spotlightHit ? "true" : "false") << ',' << csvEscape(relevance) << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ','
                    << csvEscape(r.keySampleHex) << ',' << csvEscape(r.valueSampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_filesystem_namespace_seed.csv: ") + ex.what()); }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_APFS_FILESYSTEM_NAMESPACE_SEED\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"record_samples_seen\": " << totalRows << ",\n";
        out << "  \"namespace_seed_rows\": " << emittedRows << ",\n";
        out << "  \"directory_record_rows\": " << dirRows << ",\n";
        out << "  \"named_directory_record_rows\": " << namedDirRows << ",\n";
        out << "  \"inode_record_rows\": " << inodeRows << ",\n";
        out << "  \"file_extent_record_rows\": " << fileExtentRows << ",\n";
        out << "  \"file_info_record_rows\": " << fileInfoRows << ",\n";
        out << "  \"spotlight_name_hits\": " << spotlightNameHits << "\n";
        out << "}\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_filesystem_namespace_seed_summary.json: ") + ex.what()); }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Filesystem Namespace Seed\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This bounded output consolidates selected APFS filesystem root-tree samples into a lean namespace seed. It does not inventory the full filesystem, does not copy file data, and does not store raw APFS B-tree bytes in SQLite. It is intended to support the future optional filesystem-inventory/comparison layer and later iOS CoreSpotlight reuse of path/provenance abstractions.\n\n";
        out << "## Summary\n\n";
        out << "- Record samples seen: `" << totalRows << "`\n";
        out << "- Namespace seed rows: `" << emittedRows << "`\n";
        out << "- Directory-record rows: `" << dirRows << "`\n";
        out << "- Named directory-record rows: `" << namedDirRows << "`\n";
        out << "- Inode rows: `" << inodeRows << "`\n";
        out << "- File extent rows: `" << fileExtentRows << "`\n";
        out << "- File-info rows: `" << fileInfoRows << "`\n";
        out << "- Spotlight name hits: `" << spotlightNameHits << "`\n\n";
        out << "## Next step\n\n";
        out << "Use the sampled namespace records to validate APFS directory-record child file IDs, inode metadata decoding, and file-extent value decoding. Once these fields are reliable, add targeted path walking for `.Spotlight-V100` / `Store-V2` before any broad filesystem inventory is enabled.\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write AFF4_APFS_FILESYSTEM_NAMESPACE_SEED.md: ") + ex.what()); }
}


void writeAff4ApfsSpotlightTargetScanOutputs(const fs::path& caseDir,
                                             const EvidenceSource& source,
                                             const fs::path& originalInput,
                                             const std::vector<ApfsRootTreeRecordSampleRow>& targetRows,
                                             const std::vector<ApfsRootTreeRecordSampleRow>& nameSampleRows,
                                             const std::vector<ApfsSpotlightCopyAttemptRow>& copyRows,
                                             const ApfsSpotlightTargetScanMetrics& metrics,
                                             bool strictSingleAff4,
                                             Logger& log) {
    const fs::path scanCsvPath = caseDir / "aff4_apfs_spotlight_target_scan.csv";
    const fs::path copyCsvPath = caseDir / "aff4_apfs_spotlight_copy_attempt.csv";
    const fs::path sampleCsvPath = caseDir / "aff4_apfs_spotlight_name_scan_sample.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_spotlight_target_scan_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_SPOTLIGHT_TARGET_SCAN.md";

    try {
        std::ofstream out(scanCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,target_role,fs_oid,volume_name,node_oid,node_virtual_offset,node_level,node_nkeys,entry_index,key_object_id,key_type_raw,key_type_label,decoded_name,parent_object_id_candidate,child_file_id_candidate,target_kind,status,interpretation,key_sample_hex,value_sample_hex,notes\n";
        for (const auto& r : targetRows) {
            const std::string lname = asciiLower(r.decodedName);
            std::string kind = "SPOTLIGHT_RELATED_NAME";
            if (lname == ".spotlight-v100") kind = "SPOTLIGHT_ROOT_DIRECTORY";
            else if (lname == "store-v2") kind = "SPOTLIGHT_STORE_V2_DIRECTORY";
            else if (lname == "store.db" || lname == ".store.db") kind = "SPOTLIGHT_STORE_DB_FILE";
            else if (lname == "store.db-wal" || lname == "store.db-shm") kind = "SPOTLIGHT_STORE_SQLITE_SUPPORT_FILE";
            else if (lname.rfind("dbstr-", 0) == 0) kind = "SPOTLIGHT_DBSTR_FILE";
            else if (lname.rfind("dbhdr-", 0) == 0) kind = "SPOTLIGHT_DBHDR_FILE";
            else if (lname == "0.directorystorefile" || lname == "0.directorystorefile.shadow") kind = "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
            else if (lname.rfind("0.index", 0) == 0 || lname.rfind("0.shadowindex", 0) == 0) kind = "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
            else if (lname.rfind("live.", 0) == 0 || lname == "permstore" || lname == "journalexclusion" || lname == "journals.migration_secondchance" || lname == "reversestore.updates" || lname == "store.updates" || lname == "store_generation" || lname == "reversedirectorystore" || lname == "reversedirectorystore.shadow") kind = "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
            else if (lname == "cab.created" || lname == "cab.modified" || lname == "lion.created" || lname == "lion.modified" || lname == "star.created" || lname == "star.modified" || lname == "tmp.cab" || lname == "tmp.lion" || lname == "tmp.star") kind = "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
            else if (lname.rfind("tmp.spotlight", 0) == 0) kind = "TMP_SPOTLIGHT_COMPONENT";
            else if (lname.find("corespotlight") != std::string::npos) kind = "IOS_CORESPOTLIGHT_NAME";
            else if (lname == "index.db") kind = "IOS_CORESPOTLIGHT_INDEX_DB_FILE";
            const std::uint64_t parentCandidate = r.keyObjectId;
            const std::uint64_t childCandidate = r.valueU64_0;
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.nodeOid << ',' << r.nodeVirtualOffset << ',' << r.nodeLevel << ',' << r.nodeNkeys << ',' << r.entryIndex << ','
                << r.keyObjectId << ',' << static_cast<unsigned int>(r.keyTypeRaw) << ',' << csvEscape(r.keyTypeLabel) << ',' << csvEscape(r.decodedName) << ','
                << parentCandidate << ',' << childCandidate << ',' << csvEscape(kind) << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ','
                << csvEscape(r.keySampleHex) << ',' << csvEscape(r.valueSampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_spotlight_target_scan.csv: ") + ex.what()); }

    try {
        std::ofstream out(sampleCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,target_role,fs_oid,volume_name,node_oid,node_virtual_offset,node_level,node_nkeys,entry_index,key_object_id,key_type_raw,key_type_label,decoded_name,parent_object_id_candidate,child_file_id_candidate,status,interpretation,key_sample_hex,value_sample_hex,notes\n";
        for (const auto& r : nameSampleRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.nodeOid << ',' << r.nodeVirtualOffset << ',' << r.nodeLevel << ',' << r.nodeNkeys << ',' << r.entryIndex << ','
                << r.keyObjectId << ',' << static_cast<unsigned int>(r.keyTypeRaw) << ',' << csvEscape(r.keyTypeLabel) << ',' << csvEscape(r.decodedName) << ','
                << r.keyObjectId << ',' << r.valueU64_0 << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ','
                << csvEscape(r.keySampleHex) << ',' << csvEscape(r.valueSampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_spotlight_name_scan_sample.csv: ") + ex.what()); }

    try {
        std::ofstream out(copyCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,target_role,fs_oid,volume_name,parent_object_id,child_file_id,target_name,target_kind,extraction_status,output_path,interpretation,notes\n";
        for (const auto& r : copyRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.parentObjectId << ',' << r.childFileId << ',' << csvEscape(r.targetName) << ',' << csvEscape(r.targetKind) << ','
                << csvEscape(r.extractionStatus) << ',' << csvEscape(r.outputPath) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.notes) << "\n";
        }
        if (copyRows.empty()) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ",0,0,,0,,0,0,,NO_TARGET,,NO_SPOTLIGHT_TARGET_NAME_FOUND,,No Spotlight target directory/file name was found in the bounded APFS scan.,Copy-out was not attempted because there was no filesystem target to extract.\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_spotlight_copy_attempt.csv: ") + ex.what()); }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_APFS_SPOTLIGHT_TARGET_SCAN\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"nodes_visited\": " << metrics.nodesVisited << ",\n";
        out << "  \"nodes_resolved\": " << metrics.nodesResolved << ",\n";
        out << "  \"branch_nodes\": " << metrics.branchNodes << ",\n";
        out << "  \"leaf_nodes\": " << metrics.leafNodes << ",\n";
        out << "  \"records_scanned\": " << metrics.recordsScanned << ",\n";
        out << "  \"directory_records_decoded\": " << metrics.dirRecordsDecoded << ",\n";
        out << "  \"branch_candidates_queued\": " << metrics.branchCandidatesQueued << ",\n";
        out << "  \"target_name_hits\": " << metrics.targetNameHits << ",\n";
        out << "  \"decoded_name_sample_rows\": " << nameSampleRows.size() << ",\n";
        out << "  \"copy_attempt_rows\": " << copyRows.size() << ",\n";
        out << "  \"nodes_skipped_by_limit\": " << metrics.nodesSkippedByLimit << "\n";
        out << "}\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_spotlight_target_scan_summary.json: ") + ex.what()); }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Spotlight Target Scan\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This bounded scan walks additional APFS filesystem root-tree branch nodes through the selected AFF4 file only. It searches decoded directory-record names for `.Spotlight-V100`, `Store-V2`, `store.db`, `dbStr-*`, `dbHdr-*`, and iOS CoreSpotlight names. It does not perform broad mounted-volume scanning and does not export RAW/DD from AFF4.\n\n";
        out << "## Summary\n\n";
        out << "- Nodes visited: `" << metrics.nodesVisited << "`\n";
        out << "- Nodes resolved: `" << metrics.nodesResolved << "`\n";
        out << "- Branch nodes: `" << metrics.branchNodes << "`\n";
        out << "- Leaf nodes: `" << metrics.leafNodes << "`\n";
        out << "- Records scanned: `" << metrics.recordsScanned << "`\n";
        out << "- Directory records decoded: `" << metrics.dirRecordsDecoded << "`\n";
        out << "- Branch candidates queued: `" << metrics.branchCandidatesQueued << "`\n";
        out << "- Spotlight/CoreSpotlight target name hits: `" << metrics.targetNameHits << "`\n";
        out << "- Decoded-name sample rows: `" << nameSampleRows.size() << "`\n";
        out << "- Copy-attempt rows: `" << copyRows.size() << "`\n";
        out << "- Nodes skipped by limit: `" << metrics.nodesSkippedByLimit << "`\n\n";
        out << "## Copy-out rule\n\n";
        out << "Copy-out is intentionally gated. The code writes `aff4_apfs_spotlight_copy_attempt.csv` and only advances to byte extraction after a target name, child file ID, and usable file extents are resolved. If those conditions are not met, the output records why extraction was not attempted.\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write AFF4_APFS_SPOTLIGHT_TARGET_SCAN.md: ") + ex.what()); }
}

void writeAff4ApfsSpotlightInodeProbeOutputs(const fs::path& caseDir,
                                               const EvidenceSource& source,
                                               const fs::path& originalInput,
                                               const std::vector<ApfsSpotlightInodeProbeRow>& rows,
                                               bool strictSingleAff4,
                                               Logger& log) {
    const fs::path csvPath = caseDir / "aff4_apfs_spotlight_inode_probe.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_spotlight_inode_probe_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_SPOTLIGHT_INODE_PROBE.md";
    std::size_t hitRows = 0;
    std::size_t privateIdRows = 0;
    std::size_t dstreamSizeRows = 0;
    std::size_t noMatchRows = 0;
    for (const auto& r : rows) {
        if (r.inodeStatus == "TARGET_INODE_GUIDED_LOOKUP_HIT") ++hitRows;
        if (r.inodePrivateId != 0) ++privateIdRows;
        if (r.inodeDstreamSize != 0) ++dstreamSizeRows;
        if (r.inodeStatus.find("NO_MATCH") != std::string::npos || r.inodeStatus.find("FAILED") != std::string::npos || r.inodeStatus.find("UNAVAILABLE") != std::string::npos) ++noMatchRows;
    }
    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,target_sequence,volume_sequence,target_role,fs_oid,volume_name,target_parent_object_id,target_child_file_id,target_name,target_kind,inode_object_id,inode_parent_id,inode_private_id,inode_create_time_raw,inode_mod_time_raw,inode_change_time_raw,inode_access_time_raw,inode_internal_flags,inode_nchildren_or_nlink,inode_mode_candidate,inode_uncompressed_size,inode_dstream_size,inode_dstream_alloced_size,inode_dstream_default_crypto_id,inode_xfield_status,node_oid,node_virtual_offset,node_level,node_nkeys,entry_index,inode_status,interpretation,value_sample_hex,notes\n";
        for (const auto& r : rows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.targetSequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.targetParentObjectId << ',' << r.targetChildFileId << ',' << csvEscape(r.targetName) << ',' << csvEscape(r.targetKind) << ','
                << r.inodeObjectId << ',' << r.inodeParentId << ',' << r.inodePrivateId << ',' << r.inodeCreateTimeRaw << ',' << r.inodeModTimeRaw << ',' << r.inodeChangeTimeRaw << ',' << r.inodeAccessTimeRaw << ','
                << r.inodeInternalFlags << ',' << r.inodeNchildrenOrNlink << ',' << r.inodeModeCandidate << ',' << r.inodeUncompressedSize << ',' << r.inodeDstreamSize << ',' << r.inodeDstreamAllocedSize << ',' << r.inodeDstreamDefaultCryptoId << ',' << csvEscape(r.inodeXfieldStatus) << ',' << r.nodeOid << ',' << r.nodeVirtualOffset << ',' << r.nodeLevel << ',' << r.nodeNkeys << ',' << r.entryIndex << ','
                << csvEscape(r.inodeStatus) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.valueSampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
        if (rows.empty()) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ",0,0,0,,0,,0,0,,NO_TARGET_INODES,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,,0,0,0,0,0,NO_TARGET_INODE_LOOKUPS,No Spotlight/CoreSpotlight target inode rows were available for guided lookup,,Copy-out remains blocked until target inode/private data-stream identifiers and FILE_EXTENT rows are resolved.\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_spotlight_inode_probe.csv: ") + ex.what()); }
    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_APFS_SPOTLIGHT_TARGET_INODE_DSTREAM\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"target_inode_probe_rows\": " << rows.size() << ",\n";
        out << "  \"target_inode_hits\": " << hitRows << ",\n";
        out << "  \"target_private_id_rows\": " << privateIdRows << ",\n";
        out << "  \"target_inode_dstream_size_rows\": " << dstreamSizeRows << ",\n";
        out << "  \"target_inode_no_match_or_failed_rows\": " << noMatchRows << "\n";
        out << "}\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_spotlight_inode_probe_summary.json: ") + ex.what()); }
    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Spotlight Target Inode Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This bounded probe resolves each Spotlight/CoreSpotlight target directory-record child file ID to its APFS INODE record. It records the inode private ID as a likely data-stream identifier for the following FILE_EXTENT lookup step. It remains strict single-AFF4 and does not export file bytes unless validated extents are later assembled.\n\n";
        out << "## Summary\n\n";
        out << "- Inode probe rows: `" << rows.size() << "`\n";
        out << "- Inode hits: `" << hitRows << "`\n";
        out << "- Rows with private/data-stream ID candidates: `" << privateIdRows << "`\n";
        out << "- Rows with decoded INO_EXT_TYPE_DSTREAM size: `" << dstreamSizeRows << "`\n";
        out << "- No-match/failed rows: `" << noMatchRows << "`\n\n";
        out << "## Next step\n\n";
        out << "Use inode private IDs, not only directory-record child IDs, as FILE_EXTENT lookup candidates. V0_8_62 also decodes INO_EXT_TYPE_DSTREAM.size and uses it to trim allocated extent copy-out to logical file size where available.\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write AFF4_APFS_SPOTLIGHT_INODE_PROBE.md: ") + ex.what()); }
}

void writeAff4ApfsSpotlightXattrProbeOutputs(const fs::path& caseDir,
                                              const EvidenceSource& source,
                                              const fs::path& originalInput,
                                              const std::vector<ApfsSpotlightXattrProbeRow>& xattrRows,
                                              const std::vector<ApfsSpotlightCopyAttemptRow>& copyRows,
                                              bool strictSingleAff4,
                                              Logger& log) {
    const fs::path csvPath = caseDir / "aff4_apfs_spotlight_xattr_probe.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_spotlight_xattr_probe_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_SPOTLIGHT_XATTR_PROBE.md";

    std::set<std::pair<std::uint32_t, std::uint64_t>> targetIds;
    for (const auto& cr : copyRows) {
        if (cr.childFileId != 0) targetIds.insert(std::make_pair(cr.volumeSequence, cr.childFileId));
    }

    std::vector<const ApfsSpotlightXattrProbeRow*> selected;
    selected.reserve(xattrRows.size());
    std::size_t compressionOrRsrcRows = 0;
    std::size_t embeddedRows = 0;
    std::size_t streamRows = 0;
    std::size_t targetRows = 0;
    for (const auto& r : xattrRows) {
        const bool isTarget = targetIds.count(std::make_pair(r.volumeSequence, r.fileObjectId)) != 0;
        const bool isImportant = isApfsCompressionOrResourceXattrName(r.xattrName);
        if (isImportant) ++compressionOrRsrcRows;
        if ((r.xattrFlags & 0x0002U) != 0) ++embeddedRows;
        if ((r.xattrFlags & 0x0001U) != 0) ++streamRows;
        if (isTarget) ++targetRows;
        if ((isTarget || isImportant) && selected.size() < 200000U) selected.push_back(&r);
    }

    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,target_role,fs_oid,volume_name,file_object_id,xattr_name,xattr_name_length,xattr_flags,xattr_storage,xdata_length,xdata_stream_id,xdata_stream_size,xdata_stream_allocated_size,xdata_stream_default_crypto_id,is_copy_target,is_compression_or_rsrc_xattr,xdata_preview_status,xdata_preview_hex,node_oid,node_virtual_offset,node_level,node_nkeys,entry_index,xattr_status,interpretation,notes\n";
        std::uint32_t seq = 0;
        for (const auto* rp : selected) {
            const auto& r = *rp;
            const bool isTarget = targetIds.count(std::make_pair(r.volumeSequence, r.fileObjectId)) != 0;
            const bool isImportant = isApfsCompressionOrResourceXattrName(r.xattrName);
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << seq++ << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.fileObjectId << ',' << csvEscape(r.xattrName) << ',' << r.xattrNameLength << ',' << r.xattrFlags << ',' << csvEscape(r.xattrStorage) << ','
                << r.xdataLength << ',' << r.xdataStreamId << ',' << r.xdataStreamSize << ',' << r.xdataStreamAllocatedSize << ',' << r.xdataStreamDefaultCryptoId << ',' << (isTarget ? "true" : "false") << ',' << (isImportant ? "true" : "false") << ','
                << csvEscape(r.xdataPreviewStatus) << ',' << csvEscape(r.xdataPreviewHex) << ',' << r.nodeOid << ',' << r.nodeVirtualOffset << ','
                << r.nodeLevel << ',' << r.nodeNkeys << ',' << r.entryIndex << ',' << csvEscape(r.xattrStatus) << ','
                << csvEscape(r.interpretation) << ',' << csvEscape(r.notes) << "\n";
        }
        if (selected.empty()) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ",0,0,,0,,0,,0,0,NO_XATTR_ROWS,0,0,false,false,NO_PREVIEW,,0,0,0,0,0,NO_TARGET_XATTR_ROWS,No APFS XATTR records were selected for Spotlight/CoreSpotlight target IDs.,The bounded scan may not have encountered XATTR records for the target Store-V2 files.\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_spotlight_xattr_probe.csv: ") + ex.what()); }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_APFS_SPOTLIGHT_XATTR_PROBE\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"scanned_xattr_rows\": " << xattrRows.size() << ",\n";
        out << "  \"selected_xattr_rows\": " << selected.size() << ",\n";
        out << "  \"target_xattr_rows\": " << targetRows << ",\n";
        out << "  \"compression_or_resource_xattr_rows\": " << compressionOrRsrcRows << ",\n";
        out << "  \"embedded_xattr_rows\": " << embeddedRows << ",\n";
        out << "  \"stream_xattr_rows\": " << streamRows << "\n";
        out << "}\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_spotlight_xattr_probe_summary.json: ") + ex.what()); }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Spotlight XATTR Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This diagnostic output decodes APFS XATTR records encountered during the bounded Store-V2 filesystem traversal and selects rows associated with Spotlight/CoreSpotlight copy targets plus any `com.apple.decmpfs` or `com.apple.ResourceFork` rows. It does not yet reconstruct or decompress compressed files.\n\n";
        out << "## Summary\n\n";
        out << "- Scanned XATTR rows: `" << xattrRows.size() << "`\n";
        out << "- Selected XATTR rows: `" << selected.size() << "`\n";
        out << "- Target XATTR rows: `" << targetRows << "`\n";
        out << "- Compression/resource-fork XATTR rows: `" << compressionOrRsrcRows << "`\n";
        out << "- Embedded XATTR rows: `" << embeddedRows << "`\n";
        out << "- Stream XATTR rows: `" << streamRows << "`\n\n";
        out << "## Interpretation\n\n";
        out << "`XATTR_DATA_EMBEDDED` rows may contain inline `com.apple.decmpfs` metadata/content. `XATTR_DATA_STREAM` rows contain a data-stream identifier that must be followed through APFS DSTREAM/FILE_EXTENT records in a later build before resource-fork or large compressed content can be reconstructed.\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write AFF4_APFS_SPOTLIGHT_XATTR_PROBE.md: ") + ex.what()); }
}

void writeAff4ApfsSpotlightFileExtentProbeOutputs(const fs::path& caseDir,
                                                  const EvidenceSource& source,
                                                  const fs::path& originalInput,
                                                  const std::vector<ApfsSpotlightFileExtentProbeRow>& extentRows,
                                                  bool strictSingleAff4,
                                                  Logger& log) {
    const fs::path csvPath = caseDir / "aff4_apfs_spotlight_file_extent_probe.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_spotlight_file_extent_probe_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_SPOTLIGHT_FILE_EXTENT_PROBE.md";

    std::size_t validatedRows = 0;
    std::size_t previewRows = 0;
    std::size_t sqlitePreviewRows = 0;
    std::set<std::uint64_t> targetIdsWithExtents;
    for (const auto& r : extentRows) {
        if (r.extentStatus == "TARGET_FILE_EXTENT_CANDIDATE") ++validatedRows;
        if (r.previewBytesRead > 0) ++previewRows;
        if (r.previewStatus == "PREVIEW_SQLITE_HEADER" || r.previewStatus == "PREVIEW_SQLITE_WAL_HEADER" || r.previewStatus == "PREVIEW_SQLITE_SHM_HEADER") ++sqlitePreviewRows;
        if (r.targetChildFileId != 0) targetIdsWithExtents.insert(r.targetChildFileId);
    }

    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,target_sequence,volume_sequence,target_role,fs_oid,volume_name,target_parent_object_id,target_child_file_id,target_name,target_kind,extent_file_id,extent_logical_offset,len_and_flags,extent_length_bytes,extent_flags,physical_block,physical_offset,crypto_id,node_oid,node_virtual_offset,node_level,node_nkeys,entry_index,extent_status,preview_bytes_read,preview_status,preview_sample_hex,interpretation,notes\n";
        for (const auto& r : extentRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.targetSequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                << r.targetParentObjectId << ',' << r.targetChildFileId << ',' << csvEscape(r.targetName) << ',' << csvEscape(r.targetKind) << ','
                << r.extentFileId << ',' << r.extentLogicalOffset << ',' << r.lenAndFlags << ',' << r.extentLengthBytes << ',' << r.extentFlags << ','
                << r.physicalBlock << ',' << r.physicalOffset << ',' << r.cryptoId << ',' << r.nodeOid << ',' << r.nodeVirtualOffset << ',' << r.nodeLevel << ',' << r.nodeNkeys << ',' << r.entryIndex << ','
                << csvEscape(r.extentStatus) << ',' << r.previewBytesRead << ',' << csvEscape(r.previewStatus) << ',' << csvEscape(r.previewSampleHex) << ','
                << csvEscape(r.interpretation) << ',' << csvEscape(r.notes) << "\n";
        }
        if (extentRows.empty()) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ",0,0,0,,0,,0,0,,NO_TARGET_EXTENTS,0,0,0,0,0,0,0,0,0,0,0,0,0,NO_TARGET_FILE_EXTENTS_FOUND,-1,NO_PREVIEW,,No target file extents were found for the currently decoded Spotlight/CoreSpotlight target child file IDs.,Copy-out remains blocked until FILE_EXTENT records for the target child file IDs are resolved.\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_spotlight_file_extent_probe.csv: ") + ex.what()); }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_APFS_SPOTLIGHT_TARGET_FILE_EXTENTS\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"target_file_extent_rows\": " << extentRows.size() << ",\n";
        out << "  \"validated_extent_candidate_rows\": " << validatedRows << ",\n";
        out << "  \"target_child_file_ids_with_extents\": " << targetIdsWithExtents.size() << ",\n";
        out << "  \"preview_rows\": " << previewRows << ",\n";
        out << "  \"sqlite_preview_rows\": " << sqlitePreviewRows << "\n";
        out << "}\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_spotlight_file_extent_probe_summary.json: ") + ex.what()); }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Spotlight File Extent Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This bounded probe uses Spotlight/CoreSpotlight target child file IDs found by the APFS namespace scan, then searches APFS FILE_EXTENT records encountered by the same single-AFF4 traversal. It decodes candidate logical offsets, extent length/flags, physical blocks, and a bounded first-block preview where safe. It does not assemble or export full files yet.\n\n";
        out << "## Summary\n\n";
        out << "- Target file extent rows: `" << extentRows.size() << "`\n";
        out << "- Validated extent candidate rows: `" << validatedRows << "`\n";
        out << "- Target child file IDs with extents: `" << targetIdsWithExtents.size() << "`\n";
        out << "- Preview rows: `" << previewRows << "`\n";
        out << "- SQLite-like preview rows: `" << sqlitePreviewRows << "`\n\n";
        out << "## Copy-out rule\n\n";
        out << "Full copy-out remains gated until extents for a target are complete enough to assemble the file and verify the extracted bytes/hash. This version intentionally limits itself to extent discovery and header preview so it does not export arbitrary or partial file content as if it were validated evidence.\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write AFF4_APFS_SPOTLIGHT_FILE_EXTENT_PROBE.md: ") + ex.what()); }
}

void writeAff4ApfsSpotlightFileCopyOutOutputs(const fs::path& caseDir,
                                              const EvidenceSource& source,
                                              const fs::path& originalInput,
                                              const std::vector<ApfsSpotlightFileCopyOutRow>& rows,
                                              bool strictSingleAff4,
                                              Logger& log) {
    const fs::path csvPath = caseDir / "aff4_apfs_spotlight_file_copy_out.csv";
    {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,target_sequence,volume_sequence,target_role,fs_oid,volume_name,target_parent_object_id,target_child_file_id,target_name,target_kind,storev2_root_object_id,storev2_group_name,storev2_relative_path,extent_count,assembled_bytes,logical_size_bytes,logical_size_source,first_physical_offset,output_relative_path,output_path,output_size_bytes,output_sha256,copy_status,validation_status,first_bytes_status,first_bytes_hex,interpretation,notes\n";
        if (rows.empty()) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ",0,0,0,,0,,0,0,,NO_TARGET_COPY,0,,,0,0,0,,0,,,,0,,NO_COPY_OUT_ROWS,NO_VALIDATED_EXTENT_CHAINS,NO_PREVIEW,,No controlled Spotlight/APFS file copy-out rows were generated.,Copy-out requires at least one target with a gapless nonzero physical FILE_EXTENT chain.\n";
        } else {
            for (const auto& r : rows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << r.sequence << ',' << r.targetSequence << ',' << r.volumeSequence << ',' << csvEscape(r.targetRole) << ',' << r.fsOid << ',' << csvEscape(r.volumeName) << ','
                    << r.targetParentObjectId << ',' << r.targetChildFileId << ',' << csvEscape(r.targetName) << ',' << csvEscape(r.targetKind) << ','
                    << r.storeV2RootObjectId << ',' << csvEscape(r.storeV2GroupName) << ',' << csvEscape(r.storeV2RelativePath) << ','
                    << r.extentCount << ',' << r.assembledBytes << ',' << r.logicalSizeBytes << ',' << csvEscape(r.logicalSizeSource) << ',' << r.firstPhysicalOffset << ',' << csvEscape(r.outputRelativePath) << ',' << csvEscape(r.outputPath) << ','
                    << r.outputSizeBytes << ',' << csvEscape(r.outputSha256) << ',' << csvEscape(r.copyStatus) << ',' << csvEscape(r.validationStatus) << ','
                    << csvEscape(r.firstBytesStatus) << ',' << csvEscape(r.firstBytesHex) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.notes) << "\n";
            }
        }
    }

    std::size_t copiedRows = 0;
    std::size_t skippedRows = 0;
    std::size_t sqliteRows = 0;
    std::size_t decmpfsRows = 0;
    std::size_t lzvnRows = 0;
    std::size_t lzfseRows = 0;
    std::size_t lzfseOrLzvnFailureRows = 0;
    std::uint64_t totalCopiedBytes = 0;
    for (const auto& r : rows) {
        if (apfsCopyStatusRepresentsCompleteFile(r.copyStatus)) {
            ++copiedRows;
            totalCopiedBytes += r.outputSizeBytes;
            if (r.firstBytesStatus.find("SQLITE") != std::string::npos) ++sqliteRows;
        } else {
            ++skippedRows;
        }
        if (r.copyStatus.rfind("COPIED_DECOMPFS_RESOURCE_FORK", 0) == 0) {
            ++decmpfsRows;
            if (r.copyStatus.find("LZVN") != std::string::npos) ++lzvnRows;
            if (r.copyStatus.find("LZFSE") != std::string::npos) ++lzfseRows;
        }
        if (r.validationStatus.find("LZFSE") != std::string::npos || r.validationStatus.find("LZVN") != std::string::npos) {
            if (r.copyStatus.rfind("SKIPPED_", 0) == 0) ++lzfseOrLzvnFailureRows;
        }
    }

    const fs::path jsonPath = caseDir / "aff4_apfs_spotlight_file_copy_out_summary.json";
    {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_APFS_SPOTLIGHT_GAPLESS_EXTENT_COPY_OUT\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"copy_out_rows\": " << rows.size() << ",\n";
        out << "  \"copied_files\": " << copiedRows << ",\n";
        out << "  \"skipped_rows\": " << skippedRows << ",\n";
        out << "  \"sqlite_header_or_support_previews\": " << sqliteRows << ",\n";
        out << "  \"apple_lzfse_codec_status\": \"" << jsonEscape(appleLzfseCodecBuildStatus()) << "\",\n";
        out << "  \"decmpfs_resource_fork_rows\": " << decmpfsRows << ",\n";
        out << "  \"decmpfs_lzvn_resource_fork_rows\": " << lzvnRows << ",\n";
        out << "  \"decmpfs_lzfse_resource_fork_rows\": " << lzfseRows << ",\n";
        out << "  \"decmpfs_lzfse_or_lzvn_failure_rows\": " << lzfseOrLzvnFailureRows << ",\n";
        out << "  \"total_copied_bytes\": " << totalCopiedBytes << "\n";
        out << "}\n";
    }

    const fs::path mdPath = caseDir / "AFF4_APFS_SPOTLIGHT_FILE_COPY_OUT.md";
    {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Spotlight File Copy-Out\n\n";
        out << "This bounded copy-out stage assembles only Spotlight/CoreSpotlight target files whose APFS FILE_EXTENT rows form a gapless logical byte chain and whose physical blocks are nonzero. It reads through the selected AFF4 file only and does not export RAW/DD.\n\n";
        out << "## Summary\n\n";
        out << "- Copy-out rows: `" << rows.size() << "`\n";
        out << "- Copied files: `" << copiedRows << "`\n";
        out << "- Skipped rows: `" << skippedRows << "`\n";
        out << "- SQLite header/support previews: `" << sqliteRows << "`\n";
        out << "- Apple/lzfse codec status: `" << appleLzfseCodecBuildStatus() << "`\n";
        out << "- Decmpfs resource-fork reconstructed rows: `" << decmpfsRows << "`\n";
        out << "- Decmpfs LZVN rows: `" << lzvnRows << "`\n";
        out << "- Decmpfs LZFSE rows: `" << lzfseRows << "`\n";
        out << "- Decmpfs LZFSE/LZVN failure rows: `" << lzfseOrLzvnFailureRows << "`\n";
        out << "- Total copied bytes: `" << totalCopiedBytes << "`\n\n";
        out << "Files are written under `ExtractedSpotlight/` inside the case folder. V1.0.18 vendors Apple/lzfse when supplied under `third_party/lzfse` and records codec availability and decmpfs reconstruction status in the copy-out CSV/summary.\n";
    }


    log.info("AFF4 APFS Spotlight file copy-out written: " + pathString(csvPath));
}

std::string storeV2ComponentKind(const std::string& name) {
    const std::string n = asciiLower(name);
    if (n == "store.db" || n == ".store.db") return "STORE_DB";
    if (n.rfind("dbstr-", 0) == 0) return "DBSTR_COMPONENT";
    if (n.rfind("dbhdr-", 0) == 0) return "DBHDR_COMPONENT";
    if (n.rfind("tmp.spotlight", 0) == 0) return "TMP_SPOTLIGHT_COMPONENT";
    if (n == "store.db-wal" || n == "store.db-shm") return "STORE_SQLITE_SUPPORT";
    if (n == "0.directorystorefile" || n == "0.directorystorefile.shadow") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n.rfind("0.index", 0) == 0 || n.rfind("0.shadowindex", 0) == 0) return "STOREV2_TOPLEVEL_COMPONENT";
    if (n.rfind("live.", 0) == 0) return "STOREV2_TOPLEVEL_COMPONENT";
    if (n == "reversestore.updates" || n == "store.updates" || n == "store_generation") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n == "reversedirectorystore" || n == "reversedirectorystore.shadow") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n == "permstore" || n == "journalexclusion" || n == "journals.migration_secondchance") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n == "cab.created" || n == "cab.modified" || n == "lion.created" || n == "lion.modified" || n == "star.created" || n == "star.modified") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n == "tmp.cab" || n == "tmp.lion" || n == "tmp.star") return "STOREV2_TOPLEVEL_COMPONENT";
    if (n.size() > 4 && n.substr(n.size() - 4) == ".txt") {
        const std::string stem = n.substr(0, n.size() - 4);
        const bool numericStem = !stem.empty() && std::all_of(stem.begin(), stem.end(), [](unsigned char ch) { return std::isxdigit(ch) != 0; });
        return numericStem ? "STOREV2_CACHE_TEXT_FILE" : "STOREV2_TEXT_FILE_REVIEW";
    }
    return "";
}

std::string safeStageFileName(std::string name) {
    if (name.empty()) name = "unnamed";
    for (char& ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|' || c < 32) ch = '_';
    }
    if (name == "." || name == "..") name = "unnamed";
    if (name.size() > 160) name = name.substr(0, 160);
    return name;
}

namespace {
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
} // namespace

long long apfsStoreV2StageCandidateScore(const ApfsSpotlightFileCopyOutRow& r) {
    long long score = 0;
    if (r.copyStatus == "COPIED_DECOMPFS_RESOURCE_FORK_ZLIB" ||
        r.copyStatus == "COPIED_DECOMPFS_RESOURCE_FORK_PLAIN" ||
        r.copyStatus == "COPIED_DECOMPFS_RESOURCE_FORK_LZVN_UNCOMPRESSED_MARKERS" ||
        r.copyStatus == "COPIED_DECOMPFS_RESOURCE_FORK_LZFSE_UNCOMPRESSED_MARKERS" ||
        r.copyStatus == "COPIED_DECOMPFS_RESOURCE_FORK_LZBITMAP_UNCOMPRESSED_MARKERS") {
        score += 140000;
    } else if (r.copyStatus == "COPIED_GAPLESS_EXTENT_CHAIN" || r.copyStatus == "COPIED_DIRECT_INDEXED_EXTENT_CHAIN") {
        score += 50000;
    } else if (r.copyStatus == "COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS") {
        score += 48000;
    } else if (apfsCopyStatusRepresentsPartialCandidate(r.copyStatus)) {
        score += 10000;
    }

    score += cacheNameFileIdClosenessScore(r.targetName, r.targetChildFileId);

    if (r.validationStatus == "TRIMMED_TO_INODE_LOGICAL_SIZE") score += 25000;
    if (r.logicalSizeSource == "INO_EXT_TYPE_DSTREAM.size") score += 22000;
    if (r.logicalSizeSource == "extent_chain_allocated_size") score -= 18000;
    if (r.validationStatus.find("SIZE_MATCH") != std::string::npos) score += 8000;
    if (r.validationStatus == "PARTIAL_LOGICAL_SIZE_EXCEEDS_EXTENT_CHAIN") score -= 2000;
    if (r.notes.find("_shifted_right_4") != std::string::npos) score -= 7000;
    if (r.notes.find("INODE_PARENT_MISMATCH") != std::string::npos) score -= 20000;
    score += static_cast<long long>(std::min<std::uint64_t>(r.outputSizeBytes, 1024ULL * 1024ULL) / 65536ULL);
    return score;
}

std::string apfsStoreV2CandidateStageKeyForCompare(const ApfsSpotlightFileCopyOutRow& r) {
    const std::uint64_t groupRoot = r.storeV2RootObjectId ? r.storeV2RootObjectId : r.targetParentObjectId;
    std::string rel = r.storeV2RelativePath.empty() ? r.targetName : r.storeV2RelativePath;
    for (char& ch : rel) { if (ch == '\\') ch = '/'; }
    if (!r.storeV2GroupName.empty()) {
        std::string groupPrefix = r.storeV2GroupName;
        for (char& ch : groupPrefix) { if (ch == '\\') ch = '/'; }
        const std::string prefixed = groupPrefix + "/";
        if (rel == groupPrefix) rel.clear();
        else if (rel.rfind(prefixed, 0) == 0) rel.erase(0, prefixed.size());
    }
    if (rel.empty()) rel = r.targetName;
    return std::to_string(r.volumeSequence) + ":" + std::to_string(r.fsOid) + ":" + std::to_string(groupRoot) + ":" + rel;
}

void writeAff4ApfsStoreV2CandidateDualProcessCompareOutputs(
        const fs::path& caseDir,
        const EvidenceSource& source,
        const fs::path& originalInput,
        const std::vector<ApfsSpotlightFileCopyOutRow>& copyRows,
        const std::vector<std::pair<std::string, const ApfsSpotlightFileCopyOutRow*>>& stagedFiles,
        bool strictSingleAff4,
        Logger& log) {
    struct CandidateStats {
        std::string key;
        std::uint32_t volumeSequence = 0;
        std::uint64_t fsOid = 0;
        std::string volumeName;
        std::uint64_t groupRootObjectId = 0;
        std::string storeV2GroupName;
        std::string storeV2RelativePath;
        std::string targetName;
        std::string componentKind;
        std::size_t candidateRows = 0;
        std::size_t copiedCandidateRows = 0;
        std::size_t partialCandidateRows = 0;
        std::size_t skippedCandidateRows = 0;
        std::uint64_t maxCandidateSizeBytes = 0;
        std::uint64_t totalCandidateBytes = 0;
        long long bestScore = (std::numeric_limits<long long>::min)();
        const ApfsSpotlightFileCopyOutRow* bestRow = nullptr;
        const ApfsSpotlightFileCopyOutRow* stagedRow = nullptr;
        std::string stagedRelativePath;
    };

    std::map<std::string, CandidateStats> byKey;

    auto considerRow = [&](const ApfsSpotlightFileCopyOutRow& r) {
        const std::string kind = storeV2ComponentKind(r.targetName);
        if (kind.empty() && r.storeV2RootObjectId == 0) return;
        const std::string key = apfsStoreV2CandidateStageKeyForCompare(r);
        auto& s = byKey[key];
        if (s.key.empty()) {
            s.key = key;
            s.volumeSequence = r.volumeSequence;
            s.fsOid = r.fsOid;
            s.volumeName = r.volumeName;
            s.groupRootObjectId = r.storeV2RootObjectId ? r.storeV2RootObjectId : r.targetParentObjectId;
            s.storeV2GroupName = r.storeV2GroupName;
            s.storeV2RelativePath = r.storeV2RelativePath.empty() ? r.targetName : r.storeV2RelativePath;
            s.targetName = r.targetName;
            s.componentKind = kind;
        }
        ++s.candidateRows;
        s.totalCandidateBytes += r.outputSizeBytes;
        s.maxCandidateSizeBytes = std::max<std::uint64_t>(s.maxCandidateSizeBytes, r.outputSizeBytes);
        const auto c = classifyApfsExtractionStatus(r.copyStatus);
        if (c.copied && !c.partial) ++s.copiedCandidateRows;
        else if (c.partial) ++s.partialCandidateRows;
        else ++s.skippedCandidateRows;
        const long long score = apfsStoreV2StageCandidateScore(r);
        if (!s.bestRow || score > s.bestScore || (score == s.bestScore && r.outputSizeBytes > s.bestRow->outputSizeBytes)) {
            s.bestScore = score;
            s.bestRow = &r;
        }
    };

    for (const auto& r : copyRows) considerRow(r);

    for (const auto& staged : stagedFiles) {
        const ApfsSpotlightFileCopyOutRow* r = staged.second;
        if (!r) continue;
        const std::string key = apfsStoreV2CandidateStageKeyForCompare(*r);
        auto& s = byKey[key];
        if (s.key.empty()) {
            s.key = key;
            s.volumeSequence = r->volumeSequence;
            s.fsOid = r->fsOid;
            s.volumeName = r->volumeName;
            s.groupRootObjectId = r->storeV2RootObjectId ? r->storeV2RootObjectId : r->targetParentObjectId;
            s.storeV2GroupName = r->storeV2GroupName;
            s.storeV2RelativePath = r->storeV2RelativePath.empty() ? r->targetName : r->storeV2RelativePath;
            s.targetName = r->targetName;
            s.componentKind = storeV2ComponentKind(r->targetName);
        }
        s.stagedRow = r;
        s.stagedRelativePath = staged.first;
    }

    std::size_t rows = 0;
    std::size_t exactBestStaged = 0;
    std::size_t stagedNotBest = 0;
    std::size_t noStaged = 0;
    std::size_t duplicateCandidateKeys = 0;
    std::size_t selectedWithSyntheticZeros = 0;
    std::size_t selectedDecmpfs = 0;
    std::size_t skippedOnlyKeys = 0;

    const fs::path csvPath = caseDir / "aff4_apfs_storev2_candidate_dual_process_compare.csv";
    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,candidate_key,volume_sequence,fs_oid,volume_name,group_root_object_id,storev2_group_name,storev2_relative_path,target_name,component_kind,candidate_rows,copied_candidate_rows,partial_candidate_rows,skipped_candidate_rows,max_candidate_size_bytes,total_candidate_bytes,best_sequence,best_score,best_child_file_id,best_copy_status,best_validation_status,best_output_relative_path,best_output_size_bytes,best_output_sha256,staged,staged_sequence,staged_child_file_id,staged_relative_path,staged_output_size_bytes,staged_output_sha256,selection_status,strict_single_aff4_policy,notes\n";
        std::uint32_t seq = 0;
        for (const auto& kv : byKey) {
            const auto& s = kv.second;
            ++rows;
            if (s.candidateRows > 1) ++duplicateCandidateKeys;
            const bool staged = s.stagedRow != nullptr;
            if (!staged) ++noStaged;
            if (s.copiedCandidateRows == 0 && s.partialCandidateRows == 0) ++skippedOnlyKeys;
            std::string selectionStatus;
            std::string notes;
            if (!s.bestRow && !staged) {
                selectionStatus = "NO_COPIED_OR_STAGED_ROW";
            } else if (staged && s.bestRow && s.stagedRow == s.bestRow) {
                selectionStatus = "STAGED_SELECTED_BEST_COPYOUT_CANDIDATE";
                ++exactBestStaged;
            } else if (staged && s.bestRow && s.stagedRow->sequence == s.bestRow->sequence) {
                selectionStatus = "STAGED_SELECTED_BEST_COPYOUT_SEQUENCE";
                ++exactBestStaged;
            } else if (staged && s.bestRow) {
                selectionStatus = "STAGED_ROW_DIFFERS_FROM_BEST_COPYOUT_CANDIDATE";
                ++stagedNotBest;
                notes = "review scoring/provenance; staged row did not match highest-scored candidate";
            } else if (!staged && s.bestRow) {
                selectionStatus = "BEST_COPYOUT_CANDIDATE_NOT_STAGED";
                notes = "candidate was copied but not selected into normalized Store-V2 stage";
            } else {
                selectionStatus = "STAGED_WITHOUT_COPYOUT_BEST_ROW";
            }
            if (s.stagedRow) {
                if (s.stagedRow->copyStatus == "COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS") ++selectedWithSyntheticZeros;
                if (s.stagedRow->copyStatus.rfind("COPIED_DECOMPFS_RESOURCE_FORK", 0) == 0) ++selectedDecmpfs;
            }

            const auto* b = s.bestRow;
            const auto* st = s.stagedRow;
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << seq++ << ',' << csvEscape(s.key) << ',' << s.volumeSequence << ',' << s.fsOid << ',' << csvEscape(s.volumeName) << ',' << s.groupRootObjectId << ','
                << csvEscape(s.storeV2GroupName) << ',' << csvEscape(s.storeV2RelativePath) << ',' << csvEscape(s.targetName) << ',' << csvEscape(s.componentKind) << ','
                << s.candidateRows << ',' << s.copiedCandidateRows << ',' << s.partialCandidateRows << ',' << s.skippedCandidateRows << ','
                << s.maxCandidateSizeBytes << ',' << s.totalCandidateBytes << ','
                << (b ? b->sequence : 0) << ',' << (b ? s.bestScore : 0) << ',' << (b ? b->targetChildFileId : 0) << ','
                << csvEscape(b ? b->copyStatus : "") << ',' << csvEscape(b ? b->validationStatus : "") << ',' << csvEscape(b ? b->outputRelativePath : "") << ','
                << (b ? b->outputSizeBytes : 0) << ',' << csvEscape(b ? b->outputSha256 : "") << ','
                << (staged ? "1" : "0") << ',' << (st ? st->sequence : 0) << ',' << (st ? st->targetChildFileId : 0) << ','
                << csvEscape(s.stagedRelativePath) << ',' << (st ? st->outputSizeBytes : 0) << ',' << csvEscape(st ? st->outputSha256 : "") << ','
                << csvEscape(selectionStatus) << ',' << (strictSingleAff4 ? "1" : "0") << ',' << csvEscape(notes) << "\n";
        }
        if (byKey.empty()) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ",0,,0,0,,0,,,,,0,0,0,0,0,0,0,0,0,,,,0,,0,0,,0,,NO_STOREV2_CANDIDATES,1,no Store-V2 candidate copy-out rows were available\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_storev2_candidate_dual_process_compare.csv: ") + ex.what());
    }

    try {
        std::ofstream out(caseDir / "aff4_apfs_storev2_candidate_dual_process_compare_summary.json", std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"AFF4_APFS_STOREV2_COPYOUT_VS_STAGE_CANDIDATE_COMPARE\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"candidate_keys\": " << rows << ",\n";
        out << "  \"duplicate_candidate_keys\": " << duplicateCandidateKeys << ",\n";
        out << "  \"staged_selected_best_candidate\": " << exactBestStaged << ",\n";
        out << "  \"staged_row_differs_from_best_candidate\": " << stagedNotBest << ",\n";
        out << "  \"candidate_keys_not_staged\": " << noStaged << ",\n";
        out << "  \"skipped_only_candidate_keys\": " << skippedOnlyKeys << ",\n";
        out << "  \"staged_with_synthetic_zero_provenance\": " << selectedWithSyntheticZeros << ",\n";
        out << "  \"staged_decpmfs_or_resource_fork_rows\": " << selectedDecmpfs << "\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_storev2_candidate_dual_process_compare_summary.json: ") + ex.what());
    }

    try {
        std::ofstream out(caseDir / "AFF4_APFS_STOREV2_CANDIDATE_DUAL_PROCESS_COMPARE.md", std::ios::binary);
        out << "# AFF4/APFS Store-V2 Candidate Dual-Process Compare\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Purpose\n\n";
        out << "This output compares the AFF4/APFS copy-out candidate process against the normalized Store-V2 staging process. It is a regression guard for duplicate APFS directory candidates, staged-file overwrite risk, synthetic zero provenance, and best-candidate selection drift.\n\n";
        out << "## Summary\n\n";
        out << "- Candidate keys: `" << rows << "`\n";
        out << "- Duplicate candidate keys: `" << duplicateCandidateKeys << "`\n";
        out << "- Staged rows selecting best candidate: `" << exactBestStaged << "`\n";
        out << "- Staged rows differing from best candidate: `" << stagedNotBest << "`\n";
        out << "- Candidate keys not staged: `" << noStaged << "`\n";
        out << "- Skipped-only candidate keys: `" << skippedOnlyKeys << "`\n";
        out << "- Staged rows with synthetic-zero provenance: `" << selectedWithSyntheticZeros << "`\n";
        out << "- Staged decmpfs/resource-fork rows: `" << selectedDecmpfs << "`\n\n";
        out << "## Interpretation\n\n";
        out << "`STAGED_ROW_DIFFERS_FROM_BEST_COPYOUT_CANDIDATE` should be reviewed first because it means normalized staging did not choose the highest-scored copied row for the same Store-V2 component key. This does not classify evidence as invalid; it identifies a deterministic candidate-selection discrepancy for support review.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_STOREV2_CANDIDATE_DUAL_PROCESS_COMPARE.md: ") + ex.what());
    }

    log.info("AFF4/APFS Store-V2 candidate dual-process compare written: " + pathString(csvPath));
}

void writeAff4ApfsExtractedStoreV2StageOutputs(const fs::path& caseDir,
                                               const EvidenceSource& source,
                                               const fs::path& originalInput,
                                               const std::vector<ApfsSpotlightFileCopyOutRow>& copyRows,
                                               bool strictSingleAff4,
                                               Logger& log) {
    struct GroupStats {
        std::uint32_t volumeSequence = 0;
        std::uint64_t fsOid = 0;
        std::string volumeName;
        std::uint64_t parentObjectId = 0;
        std::uint64_t storeV2RootObjectId = 0;
        std::string storeV2GroupName;
        std::vector<const ApfsSpotlightFileCopyOutRow*> files;
        std::size_t storeDb = 0;
        std::size_t dbStr = 0;
        std::size_t dbHdr = 0;
        std::size_t tmp = 0;
        std::size_t partialCompressedOrRsrc = 0;
        std::uint64_t totalBytes = 0;
        std::string stagedRelativeDir;
        std::string stagedAbsoluteDir;
        std::string status;
        std::string notes;
    };

    auto stagePreferenceKey = [](const ApfsSpotlightFileCopyOutRow& r) -> std::string {
        const std::uint64_t groupRoot = r.storeV2RootObjectId ? r.storeV2RootObjectId : r.targetParentObjectId;
        std::string rel = r.storeV2RelativePath.empty() ? r.targetName : r.storeV2RelativePath;
        for (char& ch : rel) { if (ch == '\\') ch = '/'; }
        return std::to_string(r.volumeSequence) + ":" + std::to_string(r.fsOid) + ":" + std::to_string(groupRoot) + ":" + rel;
    };

    std::set<std::string> reconstructedStageKeys;
    for (const auto& r : copyRows) {
        if (r.copyStatus.rfind("COPIED_DECOMPFS_RESOURCE_FORK", 0) == 0) {
            reconstructedStageKeys.insert(stagePreferenceKey(r));
        }
    }

    std::map<std::string, GroupStats> groups;
    for (const auto& r : copyRows) {
        const bool isCompleteCopy = apfsCopyStatusRepresentsCompleteFile(r.copyStatus);
        const bool isPartialCompressedOrRsrc = apfsCopyStatusRepresentsPartialCandidate(r.copyStatus);
        if (!isCompleteCopy && !isPartialCompressedOrRsrc) continue;
        if (isPartialCompressedOrRsrc && reconstructedStageKeys.count(stagePreferenceKey(r)) != 0) continue;
        const std::string kind = storeV2ComponentKind(r.targetName);
        if (kind.empty() && r.storeV2RootObjectId == 0) continue;
        const std::uint64_t groupRoot = r.storeV2RootObjectId ? r.storeV2RootObjectId : r.targetParentObjectId;
        const std::string key = std::to_string(r.volumeSequence) + ":" + std::to_string(r.fsOid) + ":" + std::to_string(groupRoot);
        auto& g = groups[key];
        g.volumeSequence = r.volumeSequence;
        g.fsOid = r.fsOid;
        g.volumeName = r.volumeName;
        g.parentObjectId = groupRoot;
        g.storeV2RootObjectId = groupRoot;
        if (!r.storeV2GroupName.empty()) g.storeV2GroupName = r.storeV2GroupName;
        g.files.push_back(&r);
        g.totalBytes += r.outputSizeBytes;
        if (isPartialCompressedOrRsrc) ++g.partialCompressedOrRsrc;
        if (kind == "STORE_DB") ++g.storeDb;
        else if (kind == "DBSTR_COMPONENT") ++g.dbStr;
        else if (kind == "DBHDR_COMPONENT") ++g.dbHdr;
        else if (kind == "TMP_SPOTLIGHT_COMPONENT") ++g.tmp;
    }

    const fs::path stageRoot = caseDir / "ExtractedSpotlight" / "StagedStoreV2";
    std::error_code ec;
    fs::create_directories(stageRoot, ec);
    if (ec) log.warn("Unable to create APFS extracted Store-V2 stage root: " + pathString(stageRoot) + ": " + ec.message());

    std::vector<std::pair<std::string, const ApfsSpotlightFileCopyOutRow*>> stagedFiles;
    for (auto& kv : groups) {
        auto& g = kv.second;
        const std::string dirName = !g.storeV2GroupName.empty() ? safeStageFileName(g.storeV2GroupName) : ("vol_" + std::to_string(g.volumeSequence) + "_fs_" + std::to_string(g.fsOid) + "_parent_" + std::to_string(g.parentObjectId) + "_" + safeStageFileName(g.volumeName.empty() ? "volume" : g.volumeName));
        fs::path groupDir = stageRoot / dirName;
        std::error_code mkEc;
        fs::create_directories(groupDir, mkEc);
        g.stagedAbsoluteDir = pathString(groupDir);
        g.stagedRelativeDir = pathString(fs::relative(groupDir, caseDir));
        if (mkEc) {
            g.status = "STAGE_DIRECTORY_CREATE_FAILED";
            g.notes = mkEc.message();
            continue;
        }
        std::set<std::string> usedNames;
        auto stageRowPreferenceScore = [](const ApfsSpotlightFileCopyOutRow& r) -> long long {
            return apfsStoreV2StageCandidateScore(r);
        };
        auto stagePathKeyForRow = [&](const ApfsSpotlightFileCopyOutRow& r) -> std::string {
            std::string rel = r.storeV2RelativePath.empty() ? r.targetName : r.storeV2RelativePath;
            for (char& ch : rel) { if (ch == '\\') ch = '/'; }
            if (!g.storeV2GroupName.empty()) {
                std::string groupPrefix = g.storeV2GroupName;
                for (char& ch : groupPrefix) { if (ch == '\\') ch = '/'; }
                const std::string prefixed = groupPrefix + "/";
                if (rel == groupPrefix) rel.clear();
                else if (rel.rfind(prefixed, 0) == 0) rel.erase(0, prefixed.size());
            }
            return rel.empty() ? r.targetName : rel;
        };
        auto isPreferredStageRow = [&](const ApfsSpotlightFileCopyOutRow& candidate, const ApfsSpotlightFileCopyOutRow& incumbent) -> bool {
            const long long candidateScore = stageRowPreferenceScore(candidate);
            const long long incumbentScore = stageRowPreferenceScore(incumbent);
            if (candidateScore != incumbentScore) return candidateScore > incumbentScore;

            // V0_8_70: V0_8_69 fixed most wrong same-path Cache selections, but several
            // BADA95B6 Cache N.txt duplicates remained exact score ties.  In those cases the
            // later APFS directory-entry/target row matched the external reference while the
            // earlier equal-scored row was staged only because std::map replacement used a
            // strict greater-than comparison.  Keep the main forensic scoring unchanged and
            // apply this deterministic tie-break only after scores are equal.
            std::uint64_t candidateStem = 0;
            std::uint64_t incumbentStem = 0;
            const bool candidateIsNumericCache = tryParseNumericTxtStem(candidate.targetName, candidateStem);
            const bool incumbentIsNumericCache = tryParseNumericTxtStem(incumbent.targetName, incumbentStem);
            if (candidateIsNumericCache && incumbentIsNumericCache && candidateStem == incumbentStem) {
                if (candidate.targetSequence != incumbent.targetSequence) return candidate.targetSequence > incumbent.targetSequence;
                if (candidate.sequence != incumbent.sequence) return candidate.sequence > incumbent.sequence;
                if (candidate.firstPhysicalOffset != incumbent.firstPhysicalOffset) return candidate.firstPhysicalOffset > incumbent.firstPhysicalOffset;
                if (candidate.targetChildFileId != incumbent.targetChildFileId) return candidate.targetChildFileId > incumbent.targetChildFileId;
            }

            if (candidate.outputSizeBytes != incumbent.outputSizeBytes) return candidate.outputSizeBytes > incumbent.outputSizeBytes;
            if (candidate.targetSequence != incumbent.targetSequence) return candidate.targetSequence > incumbent.targetSequence;
            if (candidate.sequence != incumbent.sequence) return candidate.sequence > incumbent.sequence;
            if (candidate.targetChildFileId != incumbent.targetChildFileId) return candidate.targetChildFileId > incumbent.targetChildFileId;
            return false;
        };
        std::map<std::string, const ApfsSpotlightFileCopyOutRow*> bestStageRowByPath;
        for (const auto* r : g.files) {
            const std::string key = stagePathKeyForRow(*r);
            auto itBest = bestStageRowByPath.find(key);
            if (itBest == bestStageRowByPath.end() || isPreferredStageRow(*r, *itBest->second)) bestStageRowByPath[key] = r;
        }
        std::vector<const ApfsSpotlightFileCopyOutRow*> selectedStageRows;
        for (const auto& sel : bestStageRowByPath) selectedStageRows.push_back(sel.second);
        for (const auto* r : selectedStageRows) {
            fs::path srcPath = fs::path(r->outputPath);
            fs::path dstPath;
            if (!r->storeV2RelativePath.empty()) {
                fs::path relCandidate;
                for (char ch : r->storeV2RelativePath) {
                    if (ch == '/' || ch == '\\') {
                        if (!relCandidate.empty()) { /* keep separator through path append below */ }
                    }
                }
                std::string rel = r->storeV2RelativePath;
                for (char& ch : rel) { if (ch == '\\') ch = '/'; }
                if (!g.storeV2GroupName.empty()) {
                    std::string groupPrefix = g.storeV2GroupName;
                    for (char& ch : groupPrefix) { if (ch == '\\') ch = '/'; }
                    if (!groupPrefix.empty()) {
                        const std::string prefixed = groupPrefix + "/";
                        if (rel == groupPrefix) rel.clear();
                        else if (rel.rfind(prefixed, 0) == 0) rel.erase(0, prefixed.size());
                    }
                }
                fs::path cleaned;
                std::size_t start = 0;
                while (start <= rel.size()) {
                    const std::size_t pos = rel.find('/', start);
                    std::string part = rel.substr(start, pos == std::string::npos ? std::string::npos : pos - start);
                    if (!part.empty()) cleaned /= safeStageFileName(part);
                    if (pos == std::string::npos) break;
                    start = pos + 1;
                }
                if (cleaned.empty()) cleaned /= safeStageFileName(r->targetName);
                dstPath = groupDir / cleaned;
            } else {
                std::string outName = safeStageFileName(r->targetName);
                if (usedNames.count(outName)) {
                    outName = std::to_string(r->sequence) + "_" + outName;
                }
                usedNames.insert(outName);
                dstPath = groupDir / outName;
            }
            std::error_code dstMkEc;
            fs::create_directories(dstPath.parent_path(), dstMkEc);
            std::error_code cpEc;
            fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing, cpEc);
            if (cpEc) {
                g.notes += (g.notes.empty() ? "" : "; ") + std::string("copy failed for ") + r->targetName + ": " + cpEc.message();
            } else {
                stagedFiles.push_back({pathString(fs::relative(dstPath, caseDir)), r});
            }
        }
        if (!g.notes.empty()) g.status = "STAGED_WITH_COPY_WARNINGS";
        else if (g.storeDb > 0) g.status = "STAGED_STORE_DB_GROUP";
        else g.status = "STAGED_AUXILIARY_STORE_COMPONENT_GROUP";
    }

    writeAff4ApfsStoreV2CandidateDualProcessCompareOutputs(caseDir, source, originalInput, copyRows, stagedFiles, strictSingleAff4, log);

    const fs::path groupCsv = caseDir / "aff4_apfs_extracted_storev2_stage_groups.csv";
    try {
        std::ofstream out(groupCsv, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,fs_oid,volume_name,parent_object_id,staged_relative_dir,staged_absolute_dir,store_db_count,dbstr_component_count,dbhdr_component_count,tmp_spotlight_component_count,partial_compressed_or_rsrc_file_count,total_store_component_files,total_store_component_bytes,stage_status,interpretation,notes\n";
        std::uint32_t seq = 0;
        for (const auto& kv : groups) {
            const auto& g = kv.second;
            std::string interp = g.storeDb > 0 ?
                "Copied APFS Store-V2 components were normalized into one staged Store-V2 candidate folder for later native parser intake." :
                "Copied APFS Spotlight auxiliary components were grouped by parent directory, but no store.db was copied for this group.";
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << seq++ << ',' << g.volumeSequence << ',' << g.fsOid << ',' << csvEscape(g.volumeName) << ',' << g.parentObjectId << ','
                << csvEscape(g.stagedRelativeDir) << ',' << csvEscape(g.stagedAbsoluteDir) << ',' << g.storeDb << ',' << g.dbStr << ',' << g.dbHdr << ',' << g.tmp << ',' << g.partialCompressedOrRsrc << ','
                << g.files.size() << ',' << g.totalBytes << ',' << csvEscape(g.status) << ',' << csvEscape(interp) << ',' << csvEscape(g.notes) << "\n";
        }
        if (groups.empty()) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ",0,0,0,,0,,,,0,0,0,0,0,0,0,NO_STORE_V2_COMPONENTS_STAGED,No copied APFS Store-V2 component files were available to stage.,\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_extracted_storev2_stage_groups.csv: ") + ex.what()); }

    const fs::path fileCsv = caseDir / "aff4_apfs_extracted_storev2_stage_files.csv";
    try {
        std::ofstream out(fileCsv, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,volume_sequence,fs_oid,volume_name,parent_object_id,child_file_id,target_name,component_kind,storev2_root_object_id,storev2_group_name,storev2_relative_path,original_copy_relative_path,staged_relative_path,output_sha256,output_size_bytes,source_copy_status,source_validation_status,stage_status,notes\n";
        std::uint32_t seq = 0;
        for (const auto& pair : stagedFiles) {
            const auto* r = pair.second;
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << seq++ << ',' << r->volumeSequence << ',' << r->fsOid << ',' << csvEscape(r->volumeName) << ',' << r->targetParentObjectId << ',' << r->targetChildFileId << ','
                << csvEscape(r->targetName) << ',' << csvEscape(storeV2ComponentKind(r->targetName)) << ',' << r->storeV2RootObjectId << ',' << csvEscape(r->storeV2GroupName) << ',' << csvEscape(r->storeV2RelativePath) << ',' << csvEscape(r->outputRelativePath) << ',' << csvEscape(pair.first) << ','
                << csvEscape(r->outputSha256) << ',' << r->outputSizeBytes << ',' << csvEscape(r->copyStatus) << ',' << csvEscape(r->validationStatus) << ','
                << csvEscape(apfsCopyStatusRepresentsPartialCandidate(r->copyStatus) ? "STAGED_PARTIAL_COMPRESSED_OR_RSRC_FORK_CANDIDATE" : (r->copyStatus.rfind("COPIED_DECOMPFS_RESOURCE_FORK", 0) == 0 ? "STAGED_RECONSTRUCTED_DECOMPFS_RESOURCE_FORK" : (r->copyStatus == "COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS" ? "STAGED_FILE_WITH_SYNTHETIC_ZERO_PROVENANCE" : "STAGED_FILE"))) << ','
                << csvEscape(r->notes) << "\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_extracted_storev2_stage_files.csv: ") + ex.what()); }

    std::size_t storeDbGroups = 0;
    std::size_t auxGroups = 0;
    std::size_t warningGroups = 0;
    std::size_t stagedFileCount = stagedFiles.size();
    std::size_t stagedPartialCompressedOrRsrcFileCount = 0;
    std::uint64_t stagedBytes = 0;
    for (const auto& kv : groups) {
        const auto& g = kv.second;
        if (g.storeDb > 0) ++storeDbGroups; else ++auxGroups;
        if (!g.notes.empty()) ++warningGroups;
        stagedPartialCompressedOrRsrcFileCount += g.partialCompressedOrRsrc;
        stagedBytes += g.totalBytes;
    }

    const fs::path jsonPath = caseDir / "aff4_apfs_extracted_storev2_stage_summary.json";
    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_APFS_EXTRACTED_STOREV2_STAGE\",\n";
        out << "  \"strict_single_aff4_policy\": " << (strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"staged_groups\": " << groups.size() << ",\n";
        out << "  \"staged_store_db_groups\": " << storeDbGroups << ",\n";
        out << "  \"staged_auxiliary_groups\": " << auxGroups << ",\n";
        out << "  \"groups_with_warnings\": " << warningGroups << ",\n";
        out << "  \"staged_files\": " << stagedFileCount << ",\n";
        out << "  \"staged_partial_compressed_or_rsrc_file_count\": " << stagedPartialCompressedOrRsrcFileCount << ",\n";
        out << "  \"staged_bytes\": " << stagedBytes << "\n";
        out << "}\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_extracted_storev2_stage_summary.json: ") + ex.what()); }

    const fs::path mdPath = caseDir / "AFF4_APFS_EXTRACTED_STOREV2_STAGE.md";
    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Extracted Store-V2 Stage\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This stage normalizes copied APFS Spotlight Store-V2 component files into parent-directory grouped staging folders under `ExtractedSpotlight/StagedStoreV2/`. It does not broad-scan the AFF4 image and does not parse the staged Store-V2 files yet. V0_8_62 preserves Store-V2 group-relative paths, removes duplicated group-name path components, and records inode DSTREAM logical-size validation where available. The purpose is to make the controlled APFS copy-out usable by the existing native Store-V2 parser in a later version.\n\n";
        out << "## Summary\n\n";
        out << "- Staged groups: `" << groups.size() << "`\n";
        out << "- Staged groups with `store.db`: `" << storeDbGroups << "`\n";
        out << "- Auxiliary groups without `store.db`: `" << auxGroups << "`\n";
        out << "- Staged files: `" << stagedFileCount << "`\n";
        out << "- Staged partial compressed/resource-fork candidates: `" << stagedPartialCompressedOrRsrcFileCount << "`\n";
        out << "- Staged bytes: `" << stagedBytes << "`\n\n";
        out << "## Next step\n\n";
        out << "Point the native Store-V2 parser at the staged candidate folders that contain `store.db`, then preserve source provenance back to the AFF4 path, volume, parent APFS object ID, child file ID, physical extents, and SHA-256 hashes.\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write AFF4_APFS_EXTRACTED_STOREV2_STAGE.md: ") + ex.what()); }

    log.info("AFF4 APFS extracted Store-V2 stage groups written: " + pathString(groupCsv));
}


void writeAff4ApfsCheckpointMapOutputs(const fs::path& caseDir,
                                       const EvidenceSource& source,
                                       const fs::path& originalInput,
                                       const ApfsNxSuperblockSummary& nx,
                                       const std::vector<ApfsCheckpointMapEntryRow>& mapRows,
                                       const std::vector<ApfsCheckpointMappedObjectProbeRow>& objectRows,
                                       Logger& log) {
    const fs::path mapCsvPath = caseDir / "aff4_apfs_checkpoint_map.csv";
    const fs::path objCsvPath = caseDir / "aff4_apfs_checkpoint_mapped_object_probe.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_checkpoint_map_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_CHECKPOINT_MAP_PROBE.md";

    std::size_t omapRows = 0, fsRows = 0, apsbHits = 0, omapHits = 0, nxsbHits = 0, readOk = 0;
    for (const auto& r : mapRows) {
        if (r.targetRole == "NX_OBJECT_MAP") ++omapRows;
        if (r.targetRole == "NX_FILESYSTEM_OID" || r.targetRole == "VOLUME_SCOPED_OBJECT") ++fsRows;
    }
    for (const auto& r : objectRows) {
        if (r.bytesRead > 0) ++readOk;
        if (r.magic == "APSB") ++apsbHits;
        if (r.magic == "OMAP") ++omapHits;
        if (r.magic == "NXSB") ++nxsbHits;
    }

    try {
        std::ofstream out(mapCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,entry_index,checkpoint_block,checkpoint_virtual_offset,checkpoint_bytes_read,checkpoint_flags,checkpoint_count,cpm_type_raw,cpm_type_label,cpm_subtype,cpm_size,cpm_fs_oid,cpm_oid,cpm_paddr,target_role,interpretation,notes\n";
        for (const auto& r : mapRows) {
            out << csvEscape(source.sourceId) << ','
                << csvEscape(pathString(originalInput)) << ','
                << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.entryIndex << ',' << r.checkpointBlock << ',' << r.checkpointVirtualOffset << ',' << r.checkpointBytesRead << ','
                << r.checkpointFlags << ',' << r.checkpointCount << ',' << r.cpmTypeRaw << ',' << csvEscape(r.cpmTypeLabel) << ',' << r.cpmSubtype << ',' << r.cpmSize << ','
                << r.cpmFsOid << ',' << r.cpmOid << ',' << r.cpmPaddr << ',' << csvEscape(r.targetRole) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_checkpoint_map.csv: ") + ex.what());
    }

    try {
        std::ofstream out(objCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,entry_index,cpm_oid,cpm_fs_oid,cpm_paddr,virtual_offset,bytes_read,mapped_oid,mapped_xid,mapped_type_raw,mapped_type_label,mapped_subtype,magic,target_role,status,interpretation,sample_hex,notes\n";
        for (const auto& r : objectRows) {
            out << csvEscape(source.sourceId) << ','
                << csvEscape(pathString(originalInput)) << ','
                << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << r.entryIndex << ',' << r.cpmOid << ',' << r.cpmFsOid << ',' << r.cpmPaddr << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                << r.mappedOid << ',' << r.mappedXid << ',' << r.mappedTypeRaw << ',' << csvEscape(r.mappedTypeLabel) << ',' << r.mappedSubtype << ','
                << csvEscape(r.magic) << ',' << csvEscape(r.targetRole) << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_checkpoint_mapped_object_probe.csv: ") + ex.what());
    }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_VIA_LIBAFF4_APFS_CHECKPOINT_MAP\",\n";
        out << "  \"nx_omap_oid\": " << nx.omapOid << ",\n";
        out << "  \"nx_block_size\": " << nx.blockSize << ",\n";
        out << "  \"checkpoint_map_entries\": " << mapRows.size() << ",\n";
        out << "  \"object_probe_rows\": " << objectRows.size() << ",\n";
        out << "  \"object_probe_read_ok\": " << readOk << ",\n";
        out << "  \"object_map_target_entries\": " << omapRows << ",\n";
        out << "  \"filesystem_related_entries\": " << fsRows << ",\n";
        out << "  \"mapped_omap_hits\": " << omapHits << ",\n";
        out << "  \"mapped_apsb_hits\": " << apsbHits << ",\n";
        out << "  \"mapped_nxsb_hits\": " << nxsbHits << "\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_checkpoint_map_summary.json: ") + ex.what());
    }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Checkpoint Map Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This probe uses bounded reads through the already-open libaff4 handle for the one explicit AFF4 input file. It parses APFS checkpoint-map entries from the NX checkpoint descriptor ring and probes the mapped physical blocks referenced by those entries. It does not search `O:\\`, does not inspect parent folders, does not call aff4imager, and does not export RAW/DD.\n\n";
        out << "## Summary\n\n";
        out << "- NX object map OID: `" << nx.omapOid << "`\n";
        out << "- Checkpoint-map entries parsed: `" << mapRows.size() << "`\n";
        out << "- Mapped object probe rows: `" << objectRows.size() << "`\n";
        out << "- Mapped OMAP hits: `" << omapHits << "`\n";
        out << "- Mapped APSB hits: `" << apsbHits << "`\n\n";
        out << "## Interpretation\n\n";
        out << "The V0_8_25 `fs_oid * block_size` volume-superblock probe showed that APFS filesystem OIDs are object identifiers, not direct physical block addresses. This V0_8_26 checkpoint-map probe begins the required object-resolution layer by parsing checkpoint mappings and testing their physical block addresses. If OMAP rows are found, the next step is OMAP B-tree parsing for filesystem OID to physical block resolution. If APSB rows are found, the next step is to parse APFS volume-superblock metadata and start catalog/file-system object discovery.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_CHECKPOINT_MAP_PROBE.md: ") + ex.what());
    }
}

void writeAff4ApfsV1DiagnosticRerunPlan(const fs::path& caseDir,
                                         const EvidenceSource& source,
                                         const RunOptions& opt,
                                         const fs::path& originalInput,
                                         Logger& log) {
    if (!isAff4SourcePath(originalInput)) return;
    const fs::path mdPath = caseDir / "AFF4_APFS_V1_DIAGNOSTIC_RERUN_PLAN.md";
    const fs::path csvPath = caseDir / "aff4_apfs_v1_diagnostic_checklist.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_v1_diagnostic_plan_summary.json";
    struct Row { const char* stage; const char* expected; const char* purpose; const char* rule; };
    const Row rows[] = {
        {"aff4_zip_single_file_probe", "aff4_zip_central_directory.csv; aff4_zip_probe_summary.json", "Confirm the exact selected AFF4 file is readable as the AFF4 container and no parent-folder AFF4 discovery was used.", "If this fails, fix AFF4 container access before APFS logic."},
        {"aff4_dynamic_load_probe", "aff4_cpp_lite_dynamic_load_probe.csv; aff4_virtual_apfs_probe.csv", "Confirm the guarded AFF4 reader can perform bounded random reads from the selected image stream.", "If virtual reads fail or use the wrong object, do not interpret APFS output."},
        {"apfs_container_checkpoint", "aff4_apfs_container_superblock.csv; aff4_apfs_checkpoint_map.csv", "Confirm APFS NXSB, latest valid checkpoint, and checkpoint-mapped ephemeral objects.", "APFS decisions must use a valid checkpoint-backed container view, not only block-zero assumptions."},
        {"apfs_omap_volume_resolution", "aff4_apfs_omap_leaf_lookup_results.csv; aff4_apfs_resolved_volume_superblocks.csv", "Resolve container and volume object maps to the APFS volume superblocks.", "If OMAP lookups are partial, report the missing OID/XID and keep extraction gated."},
        {"apfs_root_tree_namespace", "aff4_apfs_root_tree_*; aff4_apfs_spotlight_target_scan.csv", "Walk enough filesystem tree records to locate .Spotlight-V100 / Store-V2 targets and parent/child file IDs.", "Prefer path/object provenance over broad raw scanning; preserve volume, object ID, parent ID, and record source."},
        {"apfs_inode_xattr_extents", "aff4_apfs_spotlight_inode_probe.csv; aff4_apfs_spotlight_xattr_probe.csv; aff4_apfs_spotlight_file_extent_probe.csv", "Correlate target directory records to inode, private data-stream ID, xattrs, resource forks, decmpfs metadata, and file extents.", "Do not treat a target as copyable until inode size, extent chain, xattr/resource-fork state, and physical reads are classified."},
        {"apfs_copy_out", "aff4_apfs_spotlight_file_copy_out.csv; aff4_apfs_spotlight_file_copy_out_summary.json", "Assemble only files whose reconstruction status is explicit and reviewable.", "Every output row must state copied, partial, sparse gap, zero physical block, unresolved read failure, compressed/resource-fork pending, or skipped."},
        {"staged_storev2_parse", "aff4_apfs_extracted_storev2_stage_*.csv; aff4_apfs_staged_storev2_*", "Normalize copied Store-V2 components into staged groups and parse them through the native Store-V2 parser.", "Report parser counts separately from APFS copy counts so filesystem extraction failures are not confused with Spotlight parsing failures."},
        {"external_compare", "aff4_apfs_external_spotlight_compare_summary.json; aff4_apfs_external_spotlight_file_compare.csv", "Compare Vestigant extraction with the known-good external Spotlight reference when available.", "Use relative path/hash status counts to select the next fix category; do not rely on visual folder inspection alone."}
    };
    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "stage,expected_outputs,purpose,interpretation_rule\n";
        for (const auto& r : rows) {
            out << csvEscape(r.stage) << ',' << csvEscape(r.expected) << ',' << csvEscape(r.purpose) << ',' << csvEscape(r.rule) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_v1_diagnostic_checklist.csv: ") + ex.what());
    }
    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_policy\": \"STRICT_SINGLE_AFF4_DIAGNOSTIC_RERUN\",\n";
        out << "  \"container_hash_policy\": \"" << (opt.forceContainerHash ? "FORCED_BY_OPERATOR" : (opt.skipContainerHash ? "SKIPPED_BY_OPERATOR" : "DEFERRED_FOR_DEVELOPMENT_SOURCE_PROBE")) << "\",\n";
        out << "  \"diagnostic_stage_count\": " << (sizeof(rows) / sizeof(rows[0])) << ",\n";
        out << "  \"notes\": \"V1.0.0 prioritizes a fresh, reproducible AFF4/APFS diagnostic run before changing APFS reconstruction gates.\"\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_v1_diagnostic_plan_summary.json: ") + ex.what());
    }
    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4/APFS V1.0.0 Diagnostic Rerun Plan\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Purpose\n\n";
        out << "This V1.0.0 run is intended to recreate current AFF4/APFS evidence from the exact selected AFF4 image before any broader APFS reconstruction changes are made. The older V0.8.x external-compare bundle is useful historical evidence, but it should not be treated as the current source of truth for V1 decisions.\n\n";
        out << "## Scope and safety policy\n\n";
        out << "- Input is one explicit AFF4 file: `" << pathString(originalInput) << "`.\n";
        out << "- Strict single-AFF4 mode should be used for the rerun.\n";
        out << "- Full-container hashing is deferred by default for development speed unless `--force-container-hash` is supplied.\n";
        out << "- No broad parent-folder AFF4 search, full raw export, or unverifiable decompression stub should be used.\n";
        out << "- APFS copy-out must classify each target as copied, partial, sparse gap, zero physical block, unresolved read failure, compressed/resource-fork pending, or skipped.\n\n";
        out << "## Diagnostic checklist\n\n";
        out << "See `aff4_apfs_v1_diagnostic_checklist.csv` for the required outputs and interpretation rules.\n\n";
        out << "## Decision rule after upload\n\n";
        out << "After the fresh thin upload is reviewed, choose the next implementation target from the evidence: AFF4 container access, APFS checkpoint/OMAP traversal, root-tree namespace walking, inode/xattr/extent correlation, sparse/gap handling, resource-fork/decmpfs reconstruction, staged Store-V2 parsing, or investigator-facing macOS views.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_V1_DIAGNOSTIC_RERUN_PLAN.md: ") + ex.what());
    }
    log.info("AFF4/APFS V1 diagnostic rerun plan written: " + pathString(mdPath));
}


} // namespace vestigant::spotlight
