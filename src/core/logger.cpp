#include "core/logger.h"
#include "core/path_utils.h"
#include <iostream>

namespace vestigant::spotlight {

Logger::Logger(const fs::path& outputDir, bool verbose) { open(outputDir, verbose); }

void Logger::open(const fs::path& outputDir, bool verbose) {
    verbose_ = verbose;
    fs::create_directories(outputDir);
    file_.open(outputDir / "VestigantSpotlight.log", std::ios::app | std::ios::binary);
    if (file_) file_ << nowUtc() << " INFO: Logger opened at " << pathString(outputDir / "VestigantSpotlight.log") << "\n" << std::flush;
}

void Logger::info(const std::string& message) { write("INFO", message); }
void Logger::warn(const std::string& message) { write("WARN", message); }
void Logger::error(const std::string& message) { write("ERROR", message); }

void Logger::write(const std::string& level, const std::string& message) {
    const std::string line = nowUtc() + " " + level + ": " + message;
    messages_.push_back(line);
    if (file_) file_ << line << "\n" << std::flush;
    if (verbose_ || level != "INFO") std::cerr << line << "\n" << std::flush;
}

} // namespace vestigant::spotlight
