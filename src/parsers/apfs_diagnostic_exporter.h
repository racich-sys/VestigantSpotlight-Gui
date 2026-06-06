#pragma once

#include "app/models.h"
#include "core/logger.h"
#include "parsers/apfs_diagnostic_models.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace vestigant::spotlight {

// Centralized policy for deciding whether structural APFS/AFF4 diagnostic CSVs
// should be written. Copy-out/staging evidence outputs are intentionally not
// governed by this helper and should remain enabled in normal runs.
bool shouldWriteAff4ApfsStructuralDiagnostics(bool verbose,
                                              bool diagnosticFullNativeDb,
                                              bool aff4ApfsDiagnosticOutputs);

std::string aff4ApfsStructuralDiagnosticsSuppressedStatus();
std::string aff4ApfsStructuralDiagnosticsSuppressedGuidance();

void writeAff4ApfsV1DiagnosticRerunPlan(const std::filesystem::path& caseDir,
                                        const EvidenceSource& source,
                                        const RunOptions& opt,
                                        const std::filesystem::path& originalInput,
                                        Logger& log);

// APFS/AFF4 diagnostic and staging report writers. These functions format
// derived probe rows into CSV/JSON/Markdown outputs; they do not perform APFS
// traversal, AFF4 reads, Store-V2 parsing, or evidence copy-out.

void writeAff4ApfsContainerViewOutputs(const std::filesystem::path& caseDir,
                                       const EvidenceSource& source,
                                       const std::filesystem::path& originalInput,
                                       const ApfsNxSuperblockSummary& nx,
                                       const std::vector<ApfsCheckpointDescriptorRow>& descriptorRows,
                                       Logger& log);

void writeAff4ApfsVolumeSuperblockOutputs(const std::filesystem::path& caseDir,
                                         const EvidenceSource& source,
                                         const std::filesystem::path& originalInput,
                                         const ApfsNxSuperblockSummary& nx,
                                         const std::vector<ApfsVolumeSuperblockRow>& volumeRows,
                                         Logger& log);

void writeAff4ApfsResolvedVolumeOutputs(const std::filesystem::path& caseDir,
                                        const EvidenceSource& source,
                                        const std::filesystem::path& originalInput,
                                        const ApfsNxSuperblockSummary& nx,
                                        const std::vector<ApfsResolvedVolumeSuperblockRow>& resolvedRows,
                                        const std::vector<ApfsVolumeOmapProbeRow>& volumeOmapRows,
                                        const std::vector<ApfsVolumeRootTreeLookupRow>& rootLookupRows,
                                        bool strictSingleAff4,
                                        Logger& log);

void writeAff4ApfsVolumeRootTreeLookupOutputs(const std::filesystem::path& caseDir,
                                              const EvidenceSource& source,
                                              const std::filesystem::path& originalInput,
                                              const std::vector<ApfsVolumeRootTreeLookupRow>& rows,
                                              bool strictSingleAff4,
                                              Logger& log);

void writeAff4ApfsRootTreeNodeProbeOutputs(const std::filesystem::path& caseDir,
                                           const EvidenceSource& source,
                                           const std::filesystem::path& originalInput,
                                           const std::vector<ApfsRootTreeNodeProbeRow>& nodeRows,
                                           const std::vector<ApfsRootTreeRecordSampleRow>& recordRows,
                                           bool strictSingleAff4,
                                           Logger& log);

void writeAff4ApfsRootTreeTraversalProbeOutputs(const std::filesystem::path& caseDir,
                                                const EvidenceSource& source,
                                                const std::filesystem::path& originalInput,
                                                const std::vector<ApfsRootTreeChildNodeProbeRow>& traversalRows,
                                                const std::vector<ApfsRootTreeRecordSampleRow>& recordRows,
                                                const std::string& traversalLevelName,
                                                bool strictSingleAff4,
                                                Logger& log);

void writeAff4ApfsFilesystemNamespaceSeedOutputs(const std::filesystem::path& caseDir,
                                                const EvidenceSource& source,
                                                const std::filesystem::path& originalInput,
                                                const std::vector<ApfsRootTreeRecordSampleRow>& rootRecords,
                                                const std::vector<ApfsRootTreeRecordSampleRow>& childRecords,
                                                const std::vector<ApfsRootTreeRecordSampleRow>& descendantRecords,
                                                bool strictSingleAff4,
                                                Logger& log);

void writeAff4ApfsSpotlightTargetScanOutputs(const std::filesystem::path& caseDir,
                                             const EvidenceSource& source,
                                             const std::filesystem::path& originalInput,
                                             const std::vector<ApfsRootTreeRecordSampleRow>& targetRows,
                                             const std::vector<ApfsRootTreeRecordSampleRow>& nameSampleRows,
                                             const std::vector<ApfsSpotlightCopyAttemptRow>& copyRows,
                                             const ApfsSpotlightTargetScanMetrics& metrics,
                                             bool strictSingleAff4,
                                             Logger& log);

void writeAff4ApfsSpotlightInodeProbeOutputs(const std::filesystem::path& caseDir,
                                               const EvidenceSource& source,
                                               const std::filesystem::path& originalInput,
                                               const std::vector<ApfsSpotlightInodeProbeRow>& rows,
                                               bool strictSingleAff4,
                                               Logger& log);

void writeAff4ApfsSpotlightXattrProbeOutputs(const std::filesystem::path& caseDir,
                                              const EvidenceSource& source,
                                              const std::filesystem::path& originalInput,
                                              const std::vector<ApfsSpotlightXattrProbeRow>& xattrRows,
                                              const std::vector<ApfsSpotlightCopyAttemptRow>& copyRows,
                                              bool strictSingleAff4,
                                              Logger& log);

void writeAff4ApfsSpotlightFileExtentProbeOutputs(const std::filesystem::path& caseDir,
                                                  const EvidenceSource& source,
                                                  const std::filesystem::path& originalInput,
                                                  const std::vector<ApfsSpotlightFileExtentProbeRow>& extentRows,
                                                  bool strictSingleAff4,
                                                  Logger& log);

void writeAff4ApfsSpotlightFileCopyOutOutputs(const std::filesystem::path& caseDir,
                                              const EvidenceSource& source,
                                              const std::filesystem::path& originalInput,
                                              const std::vector<ApfsSpotlightFileCopyOutRow>& rows,
                                              bool strictSingleAff4,
                                              Logger& log);

void writeAff4ApfsStoreV2CandidateDualProcessCompareOutputs(
        const std::filesystem::path& caseDir,
        const EvidenceSource& source,
        const std::filesystem::path& originalInput,
        const std::vector<ApfsSpotlightFileCopyOutRow>& copyRows,
        const std::vector<std::pair<std::string, const ApfsSpotlightFileCopyOutRow*>>& stagedFiles,
        bool strictSingleAff4,
        Logger& log);

void writeAff4ApfsExtractedStoreV2StageOutputs(const std::filesystem::path& caseDir,
                                               const EvidenceSource& source,
                                               const std::filesystem::path& originalInput,
                                               const std::vector<ApfsSpotlightFileCopyOutRow>& copyRows,
                                               bool strictSingleAff4,
                                               Logger& log);

void writeAff4ApfsCheckpointMapOutputs(const std::filesystem::path& caseDir,
                                       const EvidenceSource& source,
                                       const std::filesystem::path& originalInput,
                                       const ApfsNxSuperblockSummary& nx,
                                       const std::vector<ApfsCheckpointMapEntryRow>& mapRows,
                                       const std::vector<ApfsCheckpointMappedObjectProbeRow>& objectRows,
                                       Logger& log);

} // namespace vestigant::spotlight
