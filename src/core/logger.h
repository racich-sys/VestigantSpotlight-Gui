#pragma once
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace vestigant::spotlight {
namespace fs = std::filesystem;

class Logger {
public:
    Logger() = default;
    explicit Logger(const fs::path& outputDir, bool verbose = false);
    void open(const fs::path& outputDir, bool verbose = false);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    const std::vector<std::string>& messages() const { return messages_; }
private:
    std::ofstream file_;
    mutable std::mutex mutex_;
    bool verbose_ = false;
    std::vector<std::string> messages_;
    void write(const std::string& level, const std::string& message);
};
}
