#pragma once
#include <cstddef>
#include <filesystem>
#include <string>

namespace vestigant::spotlight {
namespace fs = std::filesystem;
std::string sha256File(const fs::path& file);
std::string sha256Bytes(const unsigned char* data, std::size_t len);
}
