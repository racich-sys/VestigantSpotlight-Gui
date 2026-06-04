#include "core/app_info.h"
#include "db/case_db.h"
#include "parsers/apfs_volume_reader.h"
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
    const bool ok = !appVersion().empty() && runSchemaSmokeTest(out) && runApfsModuleSmokeTest();
    if (!ok) {
        std::cerr << "VestigantSpotlightTests failed for version " << appVersion() << "\n";
        return 1;
    }
    std::cout << "Schema/APFS module smoke test passed for Vestigant Spotlight v" << appVersion() << ": " << out << "\n";
    return 0;
}
