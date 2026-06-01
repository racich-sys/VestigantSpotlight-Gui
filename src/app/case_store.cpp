#include "app/case_store.h"
#include "core/app_info.h"
#include "core/hash.h"
#include "core/path_utils.h"
#include <fstream>
#include <sstream>

namespace vestigant::spotlight {

std::string jsonEscape(const std::string& s) {
    std::ostringstream os;
    for (char c : s) {
        switch (c) {
            case '\\': os << "\\\\"; break;
            case '"': os << "\\\""; break;
            case '\n': os << "\\n"; break;
            case '\r': os << "\\r"; break;
            case '\t': os << "\\t"; break;
            default: os << c; break;
        }
    }
    return os.str();
}

CaseStore::CaseStore(fs::path caseDir) : caseDir_(std::move(caseDir)) {}
fs::path CaseStore::exportsDir() const { return caseDir_ / "exports"; }
fs::path CaseStore::logsDir() const { return caseDir_ / "logs"; }

CaseInfo CaseStore::initialize(const RunOptions& opt, Logger& log) const {
    fs::create_directories(caseDir_);
    fs::create_directories(exportsDir());
    fs::create_directories(logsDir());
    CaseInfo c;
    const auto seed = opt.caseName + "|" + opt.caseNumber + "|" + nowUtc() + "|" + pathString(caseDir_);
    c.caseId = sha256Bytes(reinterpret_cast<const unsigned char*>(seed.data()), seed.size()).substr(0, 16);
    c.caseName = opt.caseName.empty() ? "Spotlight Case" : opt.caseName;
    c.caseNumber = opt.caseNumber;
    c.subjectName = opt.subjectName;
    c.company = opt.company;
    c.investigator = opt.investigator;
    c.createdUtc = nowUtc();
    c.appVersion = appVersion();
    std::ofstream out(caseDir_ / "case_info.json", std::ios::binary);
    out << "{\n";
    out << "  \"case_id\": \"" << jsonEscape(c.caseId) << "\",\n";
    out << "  \"case_name\": \"" << jsonEscape(c.caseName) << "\",\n";
    out << "  \"case_number\": \"" << jsonEscape(c.caseNumber) << "\",\n";
    out << "  \"subject_name\": \"" << jsonEscape(c.subjectName) << "\",\n";
    out << "  \"company\": \"" << jsonEscape(c.company) << "\",\n";
    out << "  \"investigator\": \"" << jsonEscape(c.investigator) << "\",\n";
    out << "  \"created_utc\": \"" << jsonEscape(c.createdUtc) << "\",\n";
    out << "  \"app_version\": \"" << jsonEscape(c.appVersion) << "\"\n";
    out << "}\n";
    log.info("Initialized case: " + pathString(caseDir_));
    return c;
}

EvidenceSource CaseStore::addSource(const RunOptions& opt, Logger&) const {
    EvidenceSource s;
    const auto seed = pathString(opt.input) + "|" + opt.profile + "|" + nowUtc();
    s.sourceId = sha256Bytes(reinterpret_cast<const unsigned char*>(seed.data()), seed.size()).substr(0, 16);
    s.profile = profileKindToString(parseProfileKind(opt.profile));
    s.inputPath = opt.input;
    s.evidenceRoot = opt.evidenceRoot;
    s.addedUtc = nowUtc();
    s.sourceKind = "spotlight_source";
    return s;
}

void CaseStore::writeSources(const std::vector<EvidenceSource>& sources) const {
    Rows rows;
    for (const auto& s : sources) {
        Row r;
        r["source_id"] = s.sourceId; r["profile"] = s.profile; r["input_path"] = pathString(s.inputPath); r["evidence_root"] = pathString(s.evidenceRoot); r["source_kind"] = s.sourceKind; r["added_utc"] = s.addedUtc; r["notes"] = s.notes;
        rows.push_back(std::move(r));
    }
    writeCsv(caseDir_ / "evidence_sources.csv", {"source_id","profile","input_path","evidence_root","source_kind","added_utc","notes"}, rows);
}

void CaseStore::writeStoreInventory(const std::vector<StoreInfo>& stores) const {
    writeCsv(caseDir_ / "store_inventory.csv", {"source_id","store_guid","store_path","relative_path","is_valid","version","signature","flags","header_size","block0_size","block_size","file_size_bytes","sha256","profile_hint","inferred_ios_store","validation_error"}, storeInventoryRows(stores));
}

void CaseStore::writeSummary(const RunResult& result) const {
    Rows rows;
    auto add = [&](const std::string& k, std::size_t v) { Row r; r["metric"] = k; r["value"] = std::to_string(v); rows.push_back(std::move(r)); };
    add("source_count", result.sourceCount);
    add("store_count", result.storeCount);
    add("valid_store_count", result.validStoreCount);
    add("database_candidate_count", result.databaseCandidateCount);
    add("valid_database_candidate_count", result.validDatabaseCandidateCount);
    add("parser_selected_database_count", result.selectedParserDatabaseCount);
    add("raw_record_count", result.rawRecordCount);
    add("raw_key_value_count", result.rawKeyValueCount);
    add("raw_date_candidate_count", result.rawDateCandidateCount);
    add("artifact_count", result.artifactCount);
    add("usage_evidence_count", result.usageCount);
    add("timeline_event_count", result.timelineCount);
    add("orphaned_or_deleted_candidate_count", result.orphanCandidateCount);
    writeCsv(caseDir_ / "case_summary.csv", {"metric","value"}, rows);
    std::ofstream out(caseDir_ / "case_summary.json", std::ios::binary);
    out << "{\n";
    out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
    out << "  \"source_count\": " << result.sourceCount << ",\n";
    out << "  \"store_count\": " << result.storeCount << ",\n";
    out << "  \"valid_store_count\": " << result.validStoreCount << ",\n";
    out << "  \"database_candidate_count\": " << result.databaseCandidateCount << ",\n";
    out << "  \"valid_database_candidate_count\": " << result.validDatabaseCandidateCount << ",\n";
    out << "  \"parser_selected_database_count\": " << result.selectedParserDatabaseCount << ",\n";
    out << "  \"native_decode_mode\": \"" << jsonEscape(result.nativeDecodeMode.empty() ? "unknown" : result.nativeDecodeMode) << "\",\n";
    out << "  \"metadata_values_decoded\": " << (result.rawKeyValueCount > 0 ? "true" : "false") << ",\n";
    out << "  \"raw_record_count\": " << result.rawRecordCount << ",\n";
    out << "  \"raw_key_value_count\": " << result.rawKeyValueCount << ",\n";
    out << "  \"raw_date_candidate_count\": " << result.rawDateCandidateCount << ",\n";
    out << "  \"artifact_count\": " << result.artifactCount << ",\n";
    out << "  \"usage_evidence_count\": " << result.usageCount << ",\n";
    out << "  \"timeline_event_count\": " << result.timelineCount << ",\n";
    out << "  \"orphaned_or_deleted_candidate_count\": " << result.orphanCandidateCount << "\n";
    out << "}\n";
}

} // namespace vestigant::spotlight
