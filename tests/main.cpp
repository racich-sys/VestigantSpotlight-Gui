#include "core/app_info.h"
#include "db/case_db.h"
#include "parsers/apfs_volume_reader.h"
#include "parsers/apfs_aff4_reader.h"
#include "parsers/ios_app_db_parser.h"
#include <filesystem>
#include <iostream>
#include <string>

using namespace vestigant::spotlight;
namespace fs = std::filesystem;

fs::path testIoPath(const fs::path& p) {
#if defined(_WIN32)
    std::error_code ec;
    fs::path abs = fs::absolute(p, ec);
    if (ec) abs = p;
    std::wstring w = abs.wstring();
    if (w.rfind(LR"(\\?\)", 0) == 0) return abs;
    if (w.rfind(LR"(\\)", 0) == 0) return fs::path(LR"(\\?\UNC\)" + w.substr(2));
    return fs::path(LR"(\\?\)" + w);
#else
    return p;
#endif
}


bool runIosAppDbParserSmokeTest() {
    if (iosAppDbRecordCategory("APPLE_MESSAGES", "message", "guid,text,date") != "MESSAGE_RECORDS") return false;
    if (iosAppDbRecordCategory("WhatsApp", "ZWAMESSAGE", "ZMESSAGEDATE,ZTEXT") != "MESSAGE_RECORDS") return false;
    if (!iosAppDbShouldUseWhatsappSpecialParser("WhatsApp", "ZWAMESSAGE", "MESSAGE_RECORDS")) return false;
    if (!iosAppDbShouldUseAppleMessagesSpecialParser("APPLE_MESSAGES", "message", "MESSAGE_RECORDS")) return false;
    if (!iosAppDbShouldUseKnowledgeCSpecialParser("KNOWLEDGEC_EVENTS")) return false;
    if (iosAppDbBuildKnowledgeCTextSnippet("/app/intents", "com.example", "2024-01-01", "2024-01-02").find("bundle=com.example") == std::string::npos) return false;
    const auto decision = iosAppDbBuildTableParseDecision("KnowledgeC", "ZOBJECT", "ZSTREAMNAME,ZSTARTDATE");
    if (decision.recordCategory != "KNOWLEDGEC_EVENTS" || !decision.useKnowledgeCSpecialParser) return false;
    return true;
}

bool runApfsModuleSmokeTest() {
    const std::uint64_t key = makeApfsSearchKey(15203ULL, kApfsTypeDirRecord);
    if (apfsKeyObjectId(key) != 15203ULL) return false;
    if (apfsKeyRecordType(key) != kApfsTypeDirRecord) return false;
    if (apfsRecordTypeLabel(kApfsTypeInode) != "INODE") return false;
    if (!apfsIsLikelyStoreV2GroupDirectoryName("BADA95B6-4657-4C31-9FBF-C9754AB13701")) return false;
    if (apfsIsLikelyStoreV2GroupDirectoryName("not-a-guid")) return false;
    if (apfsStoreV2ComponentKind("store.db") != "STORE_DB") return false;
    if (apfsStoreV2ComponentKind("dbStr-4.map.data") != "DBSTR_COMPONENT") return false;
    if (!apfsCopyStatusRepresentsCompleteFile("COPIED_DIRECT_INDEXED_EXTENT_CHAIN")) return false;
    if (!apfsCopyStatusRepresentsCompleteFile("COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS")) return false;
    if (!apfsCopyStatusRepresentsPartialCandidate("COPIED_PARTIAL_COMPRESSED_OR_RSRC_FORK_CANDIDATE")) return false;
    if (apfsSanitizePathComponent("bad:/name") != "bad__name") return false;

    ApfsAff4Reader reader;
    const auto unavailable = reader.getDirectoryContents(2);
    if (unavailable.lowerBoundReaderAvailable) return false;
    if (unavailable.warnings.empty()) return false;

    std::vector<unsigned char> node(128, 0);
    // Synthetic variable-length TOC B-tree node with one DIR_REC.
    node[36] = 1; // nkeys
    node[40] = 0; node[41] = 0; // table space offset
    node[42] = 8; node[43] = 0; // table space length
    const std::uint64_t raw = makeApfsSearchKey(2, kApfsTypeDirRecord);
    const std::size_t keyArea = 64;
    const std::size_t valAreaEnd = node.size();
    const std::size_t valueAbs = 104;
    const std::uint16_t keyOff = 0;
    const std::uint16_t keyLen = 15;
    const std::uint16_t valOff = static_cast<std::uint16_t>(valAreaEnd - valueAbs);
    const std::uint16_t valLen = 8;
    auto put16 = [&](std::size_t off, std::uint16_t v){ node[off] = static_cast<unsigned char>(v & 0xffU); node[off + 1] = static_cast<unsigned char>((v >> 8U) & 0xffU); };
    put16(56, keyOff); put16(58, keyLen); put16(60, valOff); put16(62, valLen);
    for (int i = 0; i < 8; ++i) node[keyArea + i] = static_cast<unsigned char>((raw >> (i * 8)) & 0xff);
    node[keyArea + 8] = 3; node[keyArea + 9] = 0; node[keyArea + 10] = 0; node[keyArea + 11] = 0; // name_len_and_hash = 3 including NUL
    node[keyArea + 12] = 'x'; node[keyArea + 13] = 'y'; node[keyArea + 14] = 0;
    const std::uint64_t child = 99;
    for (int i = 0; i < 8; ++i) node[valueAbs + i] = static_cast<unsigned char>((child >> (i * 8)) & 0xff);
    std::size_t tocAbs = 0, decodedKeyAbs = 0, decodedKeyLen = 0, decodedValAbs = 0, decodedValLen = 0;
    std::string detail;
    if (!apfsAff4DecodeGenericBtreeKvAbs(node, 0, tocAbs, decodedKeyAbs, decodedKeyLen, decodedValAbs, decodedValLen, detail)) return false;
    if (decodedKeyAbs != keyArea || decodedKeyLen != keyLen || decodedValAbs != valueAbs || decodedValLen != valLen) return false;
    ApfsBtreeKvLocation kv;
    kv.keyOffset = decodedKeyAbs; kv.keyLength = decodedKeyLen; kv.valueOffset = decodedValAbs; kv.valueLength = decodedValLen; kv.rawKey = raw; kv.objectId = 2; kv.recordType = kApfsTypeDirRecord;
    auto entry = ApfsAff4Reader::decodeDirectoryRecord(node, kv, detail);
    if (!entry || entry->parentId != 2 || entry->childFileId != 99 || entry->name != "xy") return false;
    return true;
}

bool runSchemaSmokeTest(const fs::path& out) {
    fs::path dbPath = out / "schema_smoke" / "schema_smoke.case.sqlite";
    std::error_code ec;
    fs::create_directories(dbPath.parent_path(), ec);
    fs::remove(dbPath, ec);
    CaseDatabase db;
    db.open(dbPath);
    db.initializeSchema();
    const char* views[] = {
        "vw_ios_spotlight_message_body_review",
        "vw_ios_spotlight_user_focus_message_review",
        "vw_ios_spotlight_message_contact_summary",
        "vw_ios_spotlight_message_contact_thread_detail_sample",
        "vw_ios_spotlight_message_body_focus_summary",
        "vw_ios_spotlight_normalized_timeline",
        "vw_ios_spotlight_timeline_anomaly_summary",
        "vw_ios_spotlight_plaso_l2tcsv_timeline_sample",
        "vw_ios_spotlight_case_quality_dashboard",
        "vw_case_provenance_summary",
        "vw_ios_spotlight_noise_reduction_summary",
        "vw_ios_spotlight_message_text_review",
        "vw_ios_spotlight_communication_record_review",
        "vw_ios_spotlight_direct_user_message_review",
        "vw_ios_spotlight_direct_user_message_thread_summary",
        "vw_ios_spotlight_timeline_month_summary",
        "vw_ios_spotlight_investigator_overview",
        "vw_ios_spotlight_missing_from_ffs_text_detail",
        "vw_ios_spotlight_missing_from_ffs_text_coverage_summary"
    };
    try {
        for (const char* view : views) {
            auto stmt = db.prepare(std::string("SELECT * FROM ") + view + " LIMIT 1");
            while (stmt.stepRow()) {}
        }
    } catch (const std::exception& ex) {
        std::cerr << "Schema smoke test failed: " << ex.what() << "\n";
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    fs::path out = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "VestigantSpotlight_tests";
    std::error_code ec;
    fs::remove_all(out, ec);
    const bool ok = !appVersion().empty() && runSchemaSmokeTest(out) && runIosAppDbParserSmokeTest() && runApfsModuleSmokeTest();
    if (!ok) {
        std::cerr << "VestigantSpotlightTests failed for version " << appVersion() << "\n";
        return 1;
    }
    std::cout << "Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v" << appVersion() << ": " << out << "\n";
    return 0;
}
