#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace vestigant::spotlight {
namespace fs = std::filesystem;

std::string toLower(std::string s);
std::string toUpper(std::string s);
bool iequals(const std::string& a, const std::string& b);
bool containsI(const std::string& haystack, const std::string& needle);
std::string trim(std::string s);
std::string nowUtc();
std::string pathString(const fs::path& p);

#if defined(_WIN32)
std::wstring makeWin32LongPath(const fs::path& p);
#endif
bool writeBinaryFilePortable(const fs::path& p, const unsigned char* data, std::size_t size, std::string* error = nullptr);
std::string safeRelativeString(const fs::path& root, const fs::path& p);
std::string normalizeSlash(std::string s);
std::string ltrimSlashes(std::string s);
std::vector<std::string> splitLooseList(const std::string& s);
std::string join(const std::vector<std::string>& values, const std::string& sep);
bool isLikelyTruthyNumber(const std::string& s);
std::string firstNonEmpty(std::initializer_list<std::string> values);
}
