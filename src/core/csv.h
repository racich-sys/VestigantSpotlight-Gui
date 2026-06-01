#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace vestigant::spotlight {
namespace fs = std::filesystem;
using Row = std::map<std::string, std::string>;
using Rows = std::vector<Row>;

std::vector<std::string> csvParseLine(const std::string& line);
std::string csvEscape(const std::string& value);
Rows readCsv(const fs::path& file);
void writeCsv(const fs::path& file, const std::vector<std::string>& headers, const Rows& rows);
void writeChunkedCsv(const fs::path& outputDir, const std::string& prefix, const std::vector<std::string>& headers, const Rows& rows, std::size_t chunkRows);
std::string get(const Row& r, const std::string& key);
void set(Row& r, const std::string& key, const std::string& value);
bool hasCsvFile(const fs::path& dir, const std::string& name);
}
