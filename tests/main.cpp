#include "core/app_info.h"
#include "db/case_db.h"
#include "parsers/apfs_volume_reader.h"
#include "parsers/apfs_aff4_reader.h"
#include "parsers/ios_app_db_parser.h"
#include "codec/lzfse_codec.h"
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

    ApfsVolumeReader volumeReader("synthetic.aff4", 1234, 5678, nullptr);
    volumeReader.setLeafLocator([](std::uint64_t){ return 77ULL; });
    volumeReader.setNodeReader([&](std::uint64_t oid){ return oid == 77ULL ? node : std::vector<unsigned char>{}; });
    volumeReader.setKvDecoder([&](const std::vector<unsigned char>& n, std::uint32_t idx, ApfsVolumeBtreeKvLocation& out, std::string& d){
        std::size_t ta=0, ka=0, kl=0, va=0, vl=0;
        if (!apfsAff4DecodeGenericBtreeKvAbs(n, idx, ta, ka, kl, va, vl, d)) return false;
        out.entryIndex = idx; out.keyOffset = ka; out.keyLength = kl; out.valueOffset = va; out.valueLength = vl;
        out.rawKey = raw; out.objectId = 2; out.recordType = kApfsTypeDirRecord;
        return true;
    });
    const auto children = volumeReader.enumerateDirectory(2);
    if (children.size() != 1 || children[0].childFileId != 99 || children[0].name != "xy") return false;
    const auto resolved = volumeReader.resolvePathToInode("/xy");
    if (!resolved || *resolved != 99) return false;
    return true;
}


bool runLzfseCodecSmokeTest() {
    if (!appleLzfseCodecAvailable()) return true;
    const unsigned char encodedBytes[] = {
        0x62,0x76,0x78,0x6e,0xc4,0x09,0x00,0x00,0x4b,0x00,0x00,0x00,0xe0,0x17,0x56,0x65,
        0x73,0x74,0x69,0x67,0x61,0x6e,0x74,0x20,0x4c,0x5a,0x46,0x53,0x45,0x20,0x66,0x6f,
        0x72,0x65,0x6e,0x73,0x69,0x63,0x20,0x73,0x6d,0x6f,0x6b,0x65,0x20,0x76,0x65,0x63,
        0x74,0x6f,0x72,0x2e,0x20,0x38,0x27,0xf0,0xff,0xf0,0xff,0xf0,0xff,0xf0,0xff,0xf0,
        0xff,0xf0,0xff,0xf0,0xff,0xf0,0xff,0xf0,0xff,0xf8,0xe4,0x45,0x4e,0x44,0x0a,0x06,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x62,0x76,0x78,0x24
    };
    std::vector<unsigned char> encoded(encodedBytes, encodedBytes + sizeof(encodedBytes));
    const auto decoded = decodeAppleLzfseOrLzvnChunk(encoded, 2500U);
    if (!decoded.ok || decoded.data.size() != 2500U) return false;
    const std::string prefix(decoded.data.begin(), decoded.data.begin() + 38);
    if (prefix != "Vestigant LZFSE forensic smoke vector.") return false;
    const std::string suffix(decoded.data.end() - 4, decoded.data.end());
    if (suffix != "END\n") return false;
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

bool runKnowledgeCIdentitySuppressionSmokeTest(const fs::path& out) {
    fs::path dbPath = out / "knowledgec_identity_suppression" / "knowledgec_identity_suppression.case.sqlite";
    std::error_code ec;
    fs::create_directories(dbPath.parent_path(), ec);
    fs::remove(dbPath, ec);
    try {
        CaseDatabase db;
        db.open(dbPath);
        db.initializeSchema();
        db.ensureGuiReviewViews();
        db.exec(R"SQL(
INSERT INTO ios_app_parsed_records(source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc) VALUES
('s',1,'/private/var/mobile/Library/CoreDuet/Knowledge/knowledgeC.db','knowledgeC.db','KNOWLEDGEC_COREDUET','KnowledgeC','ZOBJECT','KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY','dev1','2024-01-01T00:00:00Z','ZSTARTDATE','','','User Interaction: /app/inFocus','','/app/inFocus','stream=/app/inFocus; bundle=com.example','parsed_knowledgec_generic_row','read_only_sqlite_dynamic_schema table=ZOBJECT; KNOWLEDGEC_DEVICE_STATE_STREAM=True; IDENTITY_PROMOTION_SUPPRESSED=True','2024-01-01T00:00:00Z'),
('s',1,'/private/var/mobile/Library/CoreDuet/Knowledge/knowledgeC.db','knowledgeC.db','KNOWLEDGEC_COREDUET','KnowledgeC','ZOBJECT','KNOWLEDGEC_EVENTS','evt1','2024-01-01T00:01:00Z','ZSTARTDATE','','','User Interaction: /app/activity','','/app/activity','stream=/app/activity; bundle=com.example','parsed_knowledgec_generic_row','read_only_sqlite_dynamic_schema table=ZOBJECT','2024-01-01T00:01:00Z'),
('s',1,'/private/var/mobile/Library/CoreDuet/Knowledge/knowledgeC.db','knowledgeC.db','KNOWLEDGEC_COREDUET','KnowledgeC','ZOBJECT','KNOWLEDGEC_COMMUNICATION_INTENT','comm1','2024-01-01T00:02:00Z','ZSTARTDATE','target@example.com','','COMMUNICATION INTENT: Document Shared/Sent','','/app/intents','intent_class=INSendMessageIntent; personHandle=target@example.com','parsed_knowledgec_communication_intent','read_only_sqlite_coreduet_knowledgec joined_zstructuredmetadata_v1_3_2; COMMUNICATION_INTENT_STREAM=True; INTENT_TARGET_IDENTIFIED=True','2024-01-01T00:02:00Z');
)SQL");
        auto scalar = [&](const std::string& sql) -> long long {
            auto st = db.prepare(sql);
            if (!st.stepRow()) return -1;
            return st.colInt64(0);
        };
        if (scalar("SELECT COUNT(*) FROM vw_ios_communication_frequency WHERE communication_thread_id='/app/inFocus'") != 0) return false;
        if (scalar("SELECT COUNT(*) FROM vw_ios_communication_frequency WHERE communication_thread_id='/app/activity'") != 0) return false;
        if (scalar("SELECT COUNT(*) FROM vw_ios_communication_frequency WHERE communication_thread_id='/app/intents'") != 1) return false;
        if (scalar("SELECT COUNT(*) FROM vw_ios_communication_existence_evidence WHERE record_category='KNOWLEDGEC_EVENTS'") != 0) return false;
        if (scalar("SELECT COUNT(*) FROM vw_ios_identity_activity_linkage WHERE record_category IN ('KNOWLEDGEC_EVENTS','KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY')") != 0) return false;
        if (scalar("SELECT COUNT(*) FROM vw_ios_identity_activity_linkage WHERE record_category='KNOWLEDGEC_COMMUNICATION_INTENT' AND identity_kind='EMAIL_OR_ACCOUNT'") != 1) return false;
        if (scalar("SELECT COUNT(*) FROM vw_ios_identity_pivot_surface WHERE record_category='KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY'") != 0) return false;
    } catch (const std::exception& ex) {
        std::cerr << "KnowledgeC identity suppression smoke test failed: " << ex.what() << "\n";
        return false;
    }
    return true;
}


bool runIosCoreProbeTextContextSmokeTest(const fs::path& out) {
    fs::path dbPath = out / "ios_core_probe_text_context" / "ios_core_probe_text_context.case.sqlite";
    std::error_code ec;
    fs::create_directories(dbPath.parent_path(), ec);
    fs::remove(dbPath, ec);
    try {
        CaseDatabase db;
        db.open(dbPath);
        db.initializeSchema();
        db.ensureGuiReviewViews();
        db.exec(R"SQL(
INSERT INTO raw_records(source_id,store_guid,store_path,source_db,inode_num,store_id,parent_inode_num,flags,last_updated_raw,last_updated_utc,file_name,content_type,content_type_tree,where_froms,display_name,full_path,record_state,logical_size_bytes,physical_size_bytes) VALUES
('s','ios_private_var_mobile_Library_Spotlight_CoreSpotlight_Priority_index.spotlightV2','/private/var/mobile/Library/Spotlight/CoreSpotlight/Priority/index.spotlightV2','/private/var/mobile/Library/Spotlight/CoreSpotlight/Priority/index.spotlightV2/store.db','100','200','2','','','2025-01-01T00:00:00Z','------NONAME------','','','','','/','PARTIAL_OR_NO_PATH','','');
INSERT INTO raw_key_values(source_id,store_guid,store_path,source_db,inode_num,store_id,parent_inode_num,full_path,record_state,field_name,field_value) VALUES
('s','ios_private_var_mobile_Library_Spotlight_CoreSpotlight_Priority_index.spotlightV2','/private/var/mobile/Library/Spotlight/CoreSpotlight/Priority/index.spotlightV2','/private/var/mobile/Library/Spotlight/CoreSpotlight/Priority/index.spotlightV2/store.db','100','200','2','/','PARTIAL_OR_NO_PATH','__native_core_probe_string_001','#iMessage;-;target@example.com; file:///var/mobile/Library/SMS/Attachments/aa/bb/example.jpeg');
)SQL");
        auto scalar = [&](const std::string& sql) -> long long {
            auto st = db.prepare(sql);
            if (!st.stepRow()) return -1;
            return st.colInt64(0);
        };
        if (scalar("SELECT COUNT(*) FROM vw_ios_spotlight_text_context_review WHERE source_field_name LIKE '__native_core_probe_string_%' AND text_context_category='MESSAGE_OR_ATTACHMENT_CONTEXT'") != 1) return false;
        if (scalar("SELECT COUNT(*) FROM vw_ios_spotlight_communication_record_review WHERE communication_context_type='SPOTLIGHT_MESSAGE_OR_ATTACHMENT_TEXT_PROBE' AND native_probe_context_count=1") != 1) return false;
        if (scalar("SELECT COUNT(*) FROM vw_ios_spotlight_message_body_review WHERE body_review_bucket='SPOTLIGHT_MESSAGE_OR_ATTACHMENT_PROBE_TEXT' AND extracted_message_text_or_subject LIKE '%iMessage%'") != 1) return false;
        if (scalar("SELECT COUNT(*) FROM vw_ios_spotlight_investigator_overview WHERE review_order='10_string_probes' AND CAST(row_count AS INTEGER)>=1") != 1) return false;
    } catch (const std::exception& ex) {
        std::cerr << "iOS core probe text context smoke test failed: " << ex.what() << "\n";
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    fs::path out = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "VestigantSpotlight_tests";
    std::error_code ec;
    fs::remove_all(out, ec);
    bool ok = true;
    if (appVersion().empty()) { std::cerr << "App version empty\n"; ok = false; }
    if (!runSchemaSmokeTest(out)) { std::cerr << "Schema smoke test failed\n"; ok = false; }
    if (!runKnowledgeCIdentitySuppressionSmokeTest(out)) { std::cerr << "KnowledgeC identity suppression smoke test failed\n"; ok = false; }
    if (!runIosCoreProbeTextContextSmokeTest(out)) { std::cerr << "iOS core probe text context smoke test failed\n"; ok = false; }
    if (!runIosAppDbParserSmokeTest()) { std::cerr << "iOS app DB parser smoke test failed\n"; ok = false; }
    if (!runApfsModuleSmokeTest()) { std::cerr << "APFS module smoke test failed\n"; ok = false; }
    if (!runLzfseCodecSmokeTest()) { std::cerr << "Apple/lzfse codec smoke test failed\n"; ok = false; }
    if (!ok) {
        std::cerr << "VestigantSpotlightTests failed for version " << appVersion() << "\n";
        return 1;
    }
    std::cout << "Schema/iOS/APFS module smoke test passed for Vestigant Spotlight v" << appVersion() << ": " << out << "\n";
    return 0;
}
