#pragma once

#include "app/models.h"
#include "core/logger.h"
#include <atomic>
#include <filesystem>

namespace vestigant::spotlight {

class Aff4ProbeWorker {
public:
    static void executeDynamicLoadProbe(const std::filesystem::path& caseDir,
                                        const EvidenceSource& source,
                                        const RunOptions& opt,
                                        const std::filesystem::path& originalInput,
                                        const std::atomic_bool* cancelToken,
                                        Logger& log);

    static void executeDirectMapReaderProbe(const std::filesystem::path& caseDir,
                                            const EvidenceSource& source,
                                            const RunOptions& opt,
                                            const std::filesystem::path& originalInput,
                                            const std::atomic_bool* cancelToken,
                                            Logger& log);
};

} // namespace vestigant::spotlight
