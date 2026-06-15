#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace vestigant::spotlight {
namespace fs = std::filesystem;
std::string sha256File(const fs::path& file);
std::string sha256FileWithProgress(const fs::path& file, const std::function<void(std::uintmax_t)>& progressCallback);
std::string sha256Bytes(const unsigned char* data, std::size_t len);
}
