#pragma once
#include "app/models.h"
#include <string>
#include <vector>

namespace vestigant::spotlight {
struct SourceProfile {
    std::string id;
    std::string displayName;
    std::vector<std::string> pathHints;
    std::string description;
};

SourceProfileKind parseProfileKind(const std::string& profile);
std::string profileKindToString(SourceProfileKind kind);
std::vector<SourceProfile> registeredSourceProfiles();
std::string inferProfileHintFromPath(const fs::path& p);
bool profileAllowsPath(const fs::path& p, SourceProfileKind profile, bool fullScan);
}
