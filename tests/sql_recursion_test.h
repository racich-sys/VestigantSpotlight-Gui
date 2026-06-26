#pragma once
#include <filesystem>

namespace vestigant::spotlight {
bool runRecursiveCteSafetySmokeTest(const std::filesystem::path& out);
}
