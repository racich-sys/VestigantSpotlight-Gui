#include "ingest/store_discovery.h"
#include "core/csv.h"
#include "core/hash.h"
#include "core/path_utils.h"
#include "ingest/source_profiles.h"
#include <fstream>
#include <vector>

namespace vestigant::spotlight {
namespace {
constexpr std::uint32_t StoreSignatureV1 = 0x64737437u;
constexpr std::uint32_t StoreSignatureV2 = 0x64737438u;

std::uint32_t readU32Le(const std::vector<unsigned char>& b, std::size_t offset) {
    if (offset + 4 > b.size()) return 0;
    return static_cast<std::uint32_t>(b[offset]) |
           (static_cast<std::uint32_t>(b[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(b[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(b[offset + 3]) << 24);
}

bool isStoreFileName(const fs::path& p) {
    const auto f = toLower(p.filename().string());
    return f == "store.db" || f == ".store.db";
}

std::string sanitizeGuidComponent(std::string s) {
    for (char& ch : s) {
        if (ch == '/' || ch == '\\' || ch == ':' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') ch = '_';
    }
    while (!s.empty() && s.front() == '_') s.erase(s.begin());
    while (!s.empty() && s.back() == '_') s.pop_back();
    if (s.size() > 180) s.resize(180);
    return s.empty() ? std::string("unknown_store") : s;
}

std::string inferStoreGuid(const fs::path& p, const fs::path& root) {
    if (!p.has_parent_path() || p.parent_path().filename().empty()) return "unknown_store";
    const fs::path parent = p.parent_path();
    const std::string parentNameLower = toLower(parent.filename().string());
    if (parentNameLower == "index.spotlightv2") {
        // iOS CoreSpotlight stores commonly live under:
        // .../CoreSpotlight/<protection-class>/index.spotlightV2/(.store.db|store.db).
        // Using only the parent folder name collapses every protection class into
        // one logical store. Preserve enough relative path to keep each sample and
        // protection-class index distinct.
        return std::string("ios_") + sanitizeGuidComponent(safeRelativeString(root, parent));
    }
    return p.parent_path().filename().string();
}
}

StoreInfo inspectStoreHeader(const fs::path& storePath, const fs::path& root, const EvidenceSource& source, Logger& log) {
    StoreInfo si;
    si.sourceId = source.sourceId;
    si.storePath = storePath;
    si.relativePath = safeRelativeString(root, storePath);
    si.storeGuid = inferStoreGuid(storePath, root);
    si.profileHint = inferProfileHintFromPath(storePath);
    si.inferredIosStore = si.profileHint == "ios";
    std::error_code ec;
    si.fileSizeBytes = fs::file_size(storePath, ec);
    if (ec) si.fileSizeBytes = 0;

    std::ifstream in(storePath, std::ios::binary);
    if (!in) { si.validationError = "unable_to_open"; return si; }
    std::vector<unsigned char> b(256);
    in.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(b.size()));
    b.resize(static_cast<std::size_t>(in.gcount()));
    if (b.size() < 16) { si.validationError = "too_small_for_header"; return si; }
    si.signature = readU32Le(b, 0);
    si.isValid = si.signature == StoreSignatureV1 || si.signature == StoreSignatureV2;
    if (!si.isValid) {
        si.validationError = "unrecognized_spotlight_signature";
    }
    si.version = si.signature == StoreSignatureV2 ? 2 : si.signature == StoreSignatureV1 ? 1 : 0;
    si.flags = readU32Le(b, 4);
    if (si.version == 2) {
        si.headerSize = readU32Le(b, 36);
        si.block0Size = readU32Le(b, 40);
        si.blockSize = readU32Le(b, 44);
    } else if (si.version == 1) {
        si.headerSize = readU32Le(b, 40);
        si.block0Size = readU32Le(b, 44);
        si.blockSize = readU32Le(b, 48);
    }
    try { si.sha256 = sha256File(storePath); }
    catch (const std::exception& ex) { log.warn(std::string("Could not hash store: ") + ex.what()); }
    return si;
}

std::vector<StoreInfo> discoverStores(const EvidenceSource& source, SourceProfileKind profile, bool fullScan, Logger& log) {
    std::vector<StoreInfo> stores;
    const auto root = source.inputPath;
    std::error_code ec;
    if (!fs::exists(root, ec)) { log.error("Input path does not exist: " + pathString(root)); return stores; }
    if (fs::is_regular_file(root, ec) && isStoreFileName(root)) {
        stores.push_back(inspectStoreHeader(root, root.parent_path(), source, log));
        return stores;
    }
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        if (ec) { log.warn("Directory traversal warning: " + ec.message()); ec.clear(); continue; }
        if (!it->is_regular_file(ec)) continue;
        const auto p = it->path();
        if (!isStoreFileName(p)) continue;
        if (!profileAllowsPath(p, profile, fullScan)) continue;
        stores.push_back(inspectStoreHeader(p, root, source, log));
    }
    log.info("Discovered store.db candidates: " + std::to_string(stores.size()));
    return stores;
}

Rows storeInventoryRows(const std::vector<StoreInfo>& stores) {
    Rows rows;
    for (const auto& s : stores) {
        Row r;
        r["source_id"] = s.sourceId;
        r["store_guid"] = s.storeGuid;
        r["store_path"] = pathString(s.storePath);
        r["relative_path"] = s.relativePath;
        r["is_valid"] = s.isValid ? "true" : "false";
        r["version"] = std::to_string(s.version);
        r["signature"] = std::to_string(s.signature);
        r["flags"] = std::to_string(s.flags);
        r["header_size"] = std::to_string(s.headerSize);
        r["block0_size"] = std::to_string(s.block0Size);
        r["block_size"] = std::to_string(s.blockSize);
        r["file_size_bytes"] = std::to_string(s.fileSizeBytes);
        r["sha256"] = s.sha256;
        r["profile_hint"] = s.profileHint;
        r["inferred_ios_store"] = s.inferredIosStore ? "true" : "false";
        r["validation_error"] = s.validationError;
        rows.push_back(std::move(r));
    }
    return rows;
}
}
