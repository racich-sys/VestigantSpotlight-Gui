#include "ingest/source_profiles.h"
#include "core/path_utils.h"

namespace vestigant::spotlight {

SourceProfileKind parseProfileKind(const std::string& profile) {
    const auto p = toLower(trim(profile));
    if (p == "mac" || p == "macos" || p == "standard-macos") return SourceProfileKind::MacOS;
    if (p == "ios" || p == "corespotlight" || p == "ios-corespotlight") return SourceProfileKind::IOS;
    return SourceProfileKind::Auto;
}

std::string profileKindToString(SourceProfileKind kind) {
    switch (kind) {
        case SourceProfileKind::MacOS: return "macos";
        case SourceProfileKind::IOS: return "ios";
        case SourceProfileKind::Auto: default: return "auto";
    }
}

std::vector<SourceProfile> registeredSourceProfiles() {
    return {
        {"macos", "Standard current macOS Spotlight", {".Spotlight-V100/Store-V2/*/store.db", "System/Volumes/Data/.Spotlight-V100/Store-V2/*/store.db", "private/var/db/Spotlight-V100/*/Store-V2/*/store.db"}, "Mac Spotlight stores from a current macOS filesystem or mounted image."},
        {"ios", "iOS/CoreSpotlight", {"private/var/mobile/Library/Metadata/CoreSpotlight/*/index.spotlightV*/store.db", "Library/Metadata/CoreSpotlight/*/index.spotlightV*/store.db", "index.spotlightV*/store.db"}, "iOS/CoreSpotlight stores, including index-only records that may describe deleted app content."}
    };
}

std::string inferProfileHintFromPath(const fs::path& p) {
    const std::string s = toLower(normalizeSlash(pathString(p)));
    if (s.find("corespotlight") != std::string::npos || s.find("index.spotlightv") != std::string::npos || s.find("/var/mobile/") != std::string::npos || s.find("private/var/mobile") != std::string::npos) return "ios";
    if (s.find(".spotlight-v100") != std::string::npos || s.find("spotlight-v100") != std::string::npos || s.find("store-v2") != std::string::npos || s.find("store-v1") != std::string::npos) return "macos";
    return "unknown";
}

bool profileAllowsPath(const fs::path& p, SourceProfileKind profile, bool fullScan) {
    if (fullScan) return true;
    const auto hint = inferProfileHintFromPath(p);
    if (profile == SourceProfileKind::Auto) return hint == "macos" || hint == "ios" || toLower(p.filename().string()) == "store.db" || toLower(p.filename().string()) == ".store.db";
    if (profile == SourceProfileKind::MacOS) return hint == "macos";
    if (profile == SourceProfileKind::IOS) return hint == "ios";
    return false;
}
}
