#include "ingest/evidence_intake.h"

#include "core/path_utils.h"
#include "core/csv.h"
#include "core/logger.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>

namespace vestigant::spotlight {
namespace {

bool isSignalPathCppLocal(const std::string& lowerPath) {
    return lowerPath.find("org.whispersystems.signal") != std::string::npos ||
           lowerPath.find("signal.messenger") != std::string::npos ||
           lowerPath.find("/signal/") != std::string::npos;
}

bool isChromeBrowserPathCppLocal(const std::string& lowerPath) {
    return lowerPath.find("com.google.chrome") != std::string::npos ||
           lowerPath.find("/chrome/") != std::string::npos ||
           lowerPath.find("/google/chrome") != std::string::npos;
}

} // namespace

bool endsWithCpp(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::size_t countCsvDataRows(const std::filesystem::path& csvPath) {
    std::ifstream in(csvPath, std::ios::binary);
    if (!in) return 0;

    std::array<char, 1024 * 1024> buffer{};
    std::size_t newlineCount = 0;
    bool sawAnyBytes = false;
    char lastByte = '\0';

    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize n = in.gcount();
        if (n <= 0) break;
        sawAnyBytes = true;
        lastByte = buffer[static_cast<std::size_t>(n) - 1U];
        newlineCount += static_cast<std::size_t>(std::count(buffer.data(), buffer.data() + n, '\n'));
    }

    if (!sawAnyBytes) return 0;
    std::size_t physicalLines = newlineCount;
    if (lastByte != '\n') ++physicalLines;
    return physicalLines > 0 ? physicalLines - 1U : 0U;
}

std::string normalizeIosPathFromZipEntryCpp(const std::string& fullName) {
    if (trim(fullName).empty()) return "";
    std::string p = fullName;
    std::replace(p.begin(), p.end(), '\\', '/');
    while (p.find("//") != std::string::npos) p.replace(p.find("//"), 2, "/");
    const std::string low = toLower(p);
    auto pos = low.find("/private/var/");
    if (pos != std::string::npos) return toLower(p.substr(pos));
    pos = low.find("/var/");
    if (pos != std::string::npos) return "/private" + toLower(p.substr(pos));
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    return "/" + toLower(p);
}

std::string basenameFromZipEntryCpp(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    while (!p.empty() && p.back() == '/') p.pop_back();
    auto pos = p.find_last_of('/');
    return pos == std::string::npos ? p : p.substr(pos + 1);
}

std::string extensionFromNameCpp(const std::string& name) {
    const auto pos = name.find_last_of('.');
    if (pos == std::string::npos || pos + 1 >= name.size()) return "";
    return toLower(name.substr(pos));
}

std::string protectionClassHintCpp(const std::string& path) {
    const std::string p = toLower(path);
    if (p.find("nsfileprotectioncompleteuntilfirstuserauthentication") != std::string::npos) return "NSFileProtectionCompleteUntilFirstUserAuthentication";
    if (p.find("nsfileprotectioncompleteunlessopen") != std::string::npos) return "NSFileProtectionCompleteUnlessOpen";
    if (p.find("nsfileprotectioncompletewhenuserinactive") != std::string::npos) return "NSFileProtectionCompleteWhenUserInactive";
    if (p.find("nsfileprotectioncomplete") != std::string::npos) return "NSFileProtectionComplete";
    if (p.find("/priority/") != std::string::npos) return "Priority";
    return "Unknown";
}

std::string domainHintCpp(const std::string& path) {
    const std::string p = toLower(path);
    if (p.find("/library/sms/") != std::string::npos || endsWithCpp(p, "/sms.db")) return "Messages";
    if (p.find("/keychains/") != std::string::npos || p.find("/keychain/") != std::string::npos) return "Keychain";
    if (p.find("callhistory") != std::string::npos) return "CallHistory";
    if (p.find("whatsapp") != std::string::npos) return "WhatsApp";
    if (isSignalPathCppLocal(p)) return "Signal";
    if (p.find("telegram") != std::string::npos) return "Telegram";
    if (p.find("safari") != std::string::npos) return "Safari";
    if (isChromeBrowserPathCppLocal(p)) return "Chrome";
    if (p.find("/mail/") != std::string::npos) return "Mail";
    if (p.find("/calendar/") != std::string::npos) return "Calendar";
    if (p.find("/addressbook/") != std::string::npos || p.find("contacts") != std::string::npos) return "Contacts";
    if (p.find("fileprovider") != std::string::npos || p.find("clouddocs") != std::string::npos || p.find("mobile documents") != std::string::npos) return "FileProviderOrCloudDocs";
    return "Other";
}

std::string appContainerHintCpp(const std::string& path) {
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    const std::string low = toLower(p);
    const std::string appNeedle1 = "/containers/data/application/";
    const std::string appNeedle2 = "/containers/application/";
    const std::string groupNeedle = "/containers/shared/appgroup/";
    auto extractSeg = [&](std::size_t pos, std::size_t n) -> std::string {
        std::size_t start = pos + n;
        std::size_t end = p.find('/', start);
        return p.substr(start, end == std::string::npos ? std::string::npos : end - start);
    };
    auto pos = low.find(appNeedle1);
    if (pos != std::string::npos) return "ApplicationContainer:" + extractSeg(pos, appNeedle1.size());
    pos = low.find(appNeedle2);
    if (pos != std::string::npos) return "ApplicationContainer:" + extractSeg(pos, appNeedle2.size());
    pos = low.find(groupNeedle);
    if (pos != std::string::npos) return "AppGroup:" + extractSeg(pos, groupNeedle.size());
    return "";
}

std::filesystem::path extractedIosAppDbPathForZipEntryCpp(const std::filesystem::path& caseDir, const std::string& fullName) {
    std::string rel = fullName;
    std::replace(rel.begin(), rel.end(), '\\', '/');
    while (!rel.empty() && rel.front() == '/') rel.erase(rel.begin());

    const std::filesystem::path normalized = std::filesystem::path(rel).lexically_normal();
    std::filesystem::path out = caseDir / "EvidenceStaging" / "ios_app_databases";

    for (const auto& part : normalized) {
        std::string safePart = part.string();
        if (safePart.empty() || safePart == "." || safePart == ".." || safePart == "/" || safePart == "\\") continue;
        for (char& ch : safePart) {
            if (ch == ':' || ch == '<' || ch == '>' || ch == '"' || ch == '|' || ch == '?' || ch == '*' || ch == '\r' || ch == '\n' || ch == '\t') ch = '_';
        }
        if (!safePart.empty()) out /= safePart;
    }
    return out;
}

std::pair<std::string, std::string> databaseCategoryAndAppHintCpp(const std::string& path, const std::string& name) {
    const std::string p = toLower(path);
    const std::string n = toLower(name);
    const bool dbLike = endsWithCpp(n, ".db") || endsWithCpp(n, ".sqlite") || endsWithCpp(n, ".sqlite3") || endsWithCpp(n, ".sqlitedb") || endsWithCpp(n, ".storedata") || n == "chatstorage.sqlite" || n == "contactsv2.sqlite" || n == "callhistory.sqlite";
    if (!dbLike) return {"", ""};
    if (n == "sms.db") return {"APPLE_MESSAGES", "Messages"};
    if (n == "knowledgec.db" || p.find("/coreduet/knowledge/") != std::string::npos) return {"KNOWLEDGEC_COREDUET", "KnowledgeC"};
    if (n == "interactionc.db" || p.find("/coreduet/people/") != std::string::npos) return {"COREDUET_INTERACTIONS", "CoreDuet"};
    if (n == "globalknowledge.db" || p.find("/intelligenceplatform/globalknowledge.db") != std::string::npos) return {"KNOWLEDGEC_COREDUET", "KnowledgeC"};
    if (p.find("/keychains/") != std::string::npos || p.find("/keychain/") != std::string::npos || n == "keychain-2.db" || n == "keychain-2-debug.db") return {"KEYCHAIN", "Keychain"};
    if (p.find("callhistory") != std::string::npos || n.rfind("callhistory", 0) == 0) return {"CALL_HISTORY", "PhoneFaceTime"};
    if ((p.find("group.net.whatsapp") != std::string::npos || p.find("/whatsapp/") != std::string::npos || p.find("whatsapp.shared") != std::string::npos) || n == "chatstorage.sqlite" || n == "contactsv2.sqlite") return {"WHATSAPP", "WhatsApp"};
    if (isSignalPathCppLocal(p)) return {"SIGNAL", "Signal"};
    if (p.find("telegram") != std::string::npos) return {"TELEGRAM", "Telegram"};
    if (p.find("safari") != std::string::npos) return {"SAFARI_WEB", "Safari"};
    if (isChromeBrowserPathCppLocal(p)) return {"CHROME_WEB", "Chrome"};
    if (p.find("webkit") != std::string::npos) return {"WEBKIT", "WebKit"};
    if (p.find("/mail/") != std::string::npos) return {"MAIL", "Mail"};
    if (p.find("/calendar/") != std::string::npos || n.find("calendar") != std::string::npos) return {"CALENDAR", "Calendar"};
    if (p.find("addressbook") != std::string::npos || p.find("contacts") != std::string::npos) return {"CONTACTS", "Contacts"};
    return {"OTHER_SQLITE_OR_STORE_DATABASE", "Other"};
}

void applyIosCsvBulkImportPragmas(CaseDatabase& db) {
    db.exec("PRAGMA synchronous=OFF;");
    db.exec("PRAGMA journal_mode=MEMORY;");
    db.exec("PRAGMA temp_store=MEMORY;");
    db.exec("PRAGMA cache_size=-200000;");
}

void restoreIosCsvBulkImportPragmasNoThrow(CaseDatabase& db) {
    try { db.exec("PRAGMA synchronous=NORMAL;"); } catch (...) {}
    try { db.exec("PRAGMA journal_mode=WAL;"); } catch (...) {}
    try { db.exec("PRAGMA temp_store=MEMORY;"); } catch (...) {}
    try { db.exec("PRAGMA cache_size=-200000;"); } catch (...) {}
}


long long parseInt64Safe(const std::string& s) {
    try {
        if (trim(s).empty()) return 0;
        return std::stoll(trim(s));
    } catch (...) {
        return 0;
    }
}

namespace {

long long sqliteChangesNoThrow(CaseDatabase& db) {
    try { return sqlite3_changes(db.raw()); } catch (...) { return 0; }
}

void detachCacheDbNoThrow(CaseDatabase& db) {
    try { db.exec("DETACH DATABASE cache;"); } catch (...) {}
}

bool attachedCacheTableExists(CaseDatabase& db, const std::string& tableName) {
    auto st = db.prepare("SELECT 1 FROM cache.sqlite_master WHERE type='table' AND name=? LIMIT 1");
    st.bind(1, tableName);
    return st.stepRow();
}

std::size_t cacheTableCountNoThrow(CaseDatabase& db, const std::string& tableName) {
    try {
        auto st = db.prepare("SELECT COUNT(*) FROM cache." + sqlQuoteIdentifier(tableName));
        if (st.stepRow()) return static_cast<std::size_t>(std::max<long long>(0, st.colInt64(0)));
    } catch (...) {}
    return 0;
}

void writeStatus(const EvidenceIntake::RunStatusWriter& statusWriter,
                 const fs::path& caseDir,
                 const std::string& stage,
                 const std::string& message = std::string()) {
    if (statusWriter) statusWriter(caseDir, stage, message);
}

bool importIosInventoryFromCacheDatabase(CaseDatabase& db,
                                         const fs::path& caseDir,
                                         const std::string& sourceId,
                                         Logger& log,
                                         const fs::path& reuseCacheDir,
                                         std::size_t* fileRowsOut,
                                         std::size_t* dbRowsOut,
                                         bool materializeFullFfsInventory,
                                         const EvidenceIntake::RunStatusWriter& statusWriter) {
    if (fileRowsOut) *fileRowsOut = 0;
    if (dbRowsOut) *dbRowsOut = 0;
    if (reuseCacheDir.empty()) return false;
    std::error_code ec;
    fs::path cacheDb = reuseCacheDir / "VestigantSpotlight.case.sqlite";
    if (!fs::is_regular_file(cacheDb, ec)) cacheDb = reuseCacheDir / "spotlight_case.db";
    if (!fs::is_regular_file(cacheDb, ec)) {
        log.warn("Reuse cache has no SQLite case database; falling back to CSV inventory import: " + pathString(reuseCacheDir));
        writeStatus(statusWriter, caseDir, "ios_ffs_inventory_cache_db_missing", "falling back to CSV import");
        return false;
    }
    bool attached = false;
    try {
        writeStatus(statusWriter, caseDir, "ios_ffs_inventory_cache_db_import_start", "cache_db=" + pathString(cacheDb));
        {
            auto attach = db.prepare("ATTACH DATABASE ? AS cache");
            attach.bind(1, pathString(cacheDb));
            attach.stepDone();
            attached = true;
        }
        if (!attachedCacheTableExists(db, "ios_ffs_file_inventory") || !attachedCacheTableExists(db, "ios_app_database_inventory")) {
            detachCacheDbNoThrow(db);
            log.warn("Reuse cache database is missing iOS inventory tables; falling back to CSV import: " + pathString(cacheDb));
            writeStatus(statusWriter, caseDir, "ios_ffs_inventory_cache_db_unusable", "missing required tables; falling back to CSV import");
            return false;
        }

        const std::string created = nowUtc();
        db.begin();
        {
            auto del = db.prepare("DELETE FROM ios_ffs_file_inventory WHERE source_id=?");
            del.bind(1, sourceId);
            del.stepDone();
        }
        {
            auto del = db.prepare("DELETE FROM ios_app_database_inventory WHERE source_id=?");
            del.bind(1, sourceId);
            del.stepDone();
        }
        {
            auto del = db.prepare("DELETE FROM ios_ffs_path_lookup WHERE source_id=?");
            del.bind(1, sourceId);
            del.stepDone();
        }
        if (materializeFullFfsInventory) {
            auto ins = db.prepare(
                "INSERT INTO ios_ffs_file_inventory(source_id,original_zip_entry,normalized_path,file_name,extension,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,is_directory,sha256_status,inventory_notes,created_utc) "
                "SELECT ?,original_zip_entry,normalized_path,file_name,extension,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,is_directory,sha256_status,inventory_notes,? "
                "FROM cache.ios_ffs_file_inventory");
            ins.bind(1, sourceId);
            ins.bind(2, created);
            ins.stepDone();
            if (fileRowsOut) *fileRowsOut = static_cast<std::size_t>(std::max<long long>(0, sqliteChangesNoThrow(db)));
        } else {
            // V0.9.42: do not copy the entire 1M+ row FFS inventory into the active case DB during
            // normal Spotlight-first reuse-cache runs. Insert a one-row sentinel so Missing From FFS
            // views know an FFS lookup source exists, then import only exact referenced-path hits after
            // native Spotlight parsing has produced vw_ios_spotlight_referenced_paths.
            const std::size_t cacheFfsRows = cacheTableCountNoThrow(db, "ios_ffs_file_inventory");
            auto insLookupSentinel = db.prepare(
                "INSERT INTO ios_ffs_path_lookup(source_id,normalized_path,file_name,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,is_directory,lookup_source,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
            insLookupSentinel.bind(1, sourceId);
            insLookupSentinel.bind(2, "__vestigant_lookup_available__");
            insLookupSentinel.bind(3, "__vestigant_lookup_available__");
            insLookupSentinel.bind(4, 0LL);
            insLookupSentinel.bind(5, "");
            insLookupSentinel.bind(6, "");
            insLookupSentinel.bind(7, "");
            insLookupSentinel.bind(8, "");
            insLookupSentinel.bind(9, 1LL);
            insLookupSentinel.bind(10, "reuse_cache_sqlite_referenced_path_lookup_pending");
            insLookupSentinel.bind(11, created);
            insLookupSentinel.stepDone();
            if (fileRowsOut) *fileRowsOut = cacheFfsRows;
            writeStatus(statusWriter, caseDir, "ios_ffs_inventory_cache_slim_lookup_deferred", "full FFS inventory rows not materialized; cache_rows=" + std::to_string(cacheFfsRows) + " referenced-path lookup will be imported after native parse");
        }
        {
            auto ins = db.prepare(
                "INSERT INTO ios_app_database_inventory(source_id,original_zip_entry,normalized_path,database_name,database_category,app_hint,protection_class_hint,size_bytes,zip_modified_utc,parse_status,record_inventory_status,notes,extracted_path,created_utc) "
                "SELECT ?,original_zip_entry,normalized_path,database_name,database_category,app_hint,protection_class_hint,size_bytes,zip_modified_utc,parse_status,record_inventory_status,notes,extracted_path,? "
                "FROM cache.ios_app_database_inventory");
            ins.bind(1, sourceId);
            ins.bind(2, created);
            ins.stepDone();
            if (dbRowsOut) *dbRowsOut = static_cast<std::size_t>(std::max<long long>(0, sqliteChangesNoThrow(db)));
        }
        db.commit();
        detachCacheDbNoThrow(db);
        log.info(std::string(materializeFullFfsInventory ? "Imported" : "Referenced") + " iOS FFS/database inventory from cache SQLite. files=" + std::to_string(fileRowsOut ? *fileRowsOut : 0) +
                 " app_databases=" + std::to_string(dbRowsOut ? *dbRowsOut : 0) + " cache_db=" + pathString(cacheDb));
        writeStatus(statusWriter, caseDir, "ios_ffs_inventory_cache_db_import_complete", std::string("files=") + std::to_string(fileRowsOut ? *fileRowsOut : 0) +
                        " app_databases=" + std::to_string(dbRowsOut ? *dbRowsOut : 0) +
                        (materializeFullFfsInventory ? " materialized_ffs=1" : " materialized_ffs=0"));
        return true;
    } catch (const std::exception& ex) {
        db.rollbackNoThrow();
        if (attached) detachCacheDbNoThrow(db);
        log.warn(std::string("Unable to import iOS inventory from cache SQLite; falling back to CSV import: ") + ex.what());
        writeStatus(statusWriter, caseDir, "ios_ffs_inventory_cache_db_import_warning", ex.what());
        return false;
    } catch (...) {
        db.rollbackNoThrow();
        if (attached) detachCacheDbNoThrow(db);
        log.warn("Unable to import iOS inventory from cache SQLite; falling back to CSV import: unknown error");
        writeStatus(statusWriter, caseDir, "ios_ffs_inventory_cache_db_import_warning", "unknown error");
        return false;
    }
}

Row csvRowFromFields(const std::vector<std::string>& headers, const std::vector<std::string>& fields) {
    Row r;
    for (std::size_t i = 0; i < headers.size(); ++i) r[headers[i]] = i < fields.size() ? fields[i] : "";
    return r;
}

std::vector<std::string> readCsvHeaders(std::ifstream& in) {
    std::string line;
    if (!std::getline(in, line)) return {};
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto headers = csvParseLine(line);
    if (!headers.empty() && headers[0].size() >= 3 &&
        static_cast<unsigned char>(headers[0][0]) == 0xEF &&
        static_cast<unsigned char>(headers[0][1]) == 0xBB &&
        static_cast<unsigned char>(headers[0][2]) == 0xBF) {
        headers[0].erase(0, 3);
    }
    return headers;
}


std::size_t importReferencedIosPathLookupFromReuseCacheImpl(CaseDatabase& db,
                                                            const fs::path& caseDir,
                                                            const std::string& sourceId,
                                                            Logger& log,
                                                            const fs::path& reuseCacheDir,
                                                            const EvidenceIntake::RunStatusWriter& statusWriter) {
    if (reuseCacheDir.empty()) return 0;
    std::error_code ec;
    fs::path cacheDb = reuseCacheDir / "VestigantSpotlight.case.sqlite";
    if (!fs::is_regular_file(cacheDb, ec)) cacheDb = reuseCacheDir / "spotlight_case.db";
    if (!fs::is_regular_file(cacheDb, ec)) {
        writeStatus(statusWriter, caseDir, "ios_ffs_referenced_path_lookup_cache_missing", "reuse cache SQLite not available");
        return 0;
    }
    bool attached = false;
    try {
        writeStatus(statusWriter, caseDir, "ios_ffs_referenced_path_lookup_start", "cache_db=" + pathString(cacheDb));
        {
            auto attach = db.prepare("ATTACH DATABASE ? AS cache");
            attach.bind(1, pathString(cacheDb));
            attach.stepDone();
            attached = true;
        }
        if (!attachedCacheTableExists(db, "ios_ffs_file_inventory")) {
            writeStatus(statusWriter, caseDir, "ios_ffs_referenced_path_lookup_unavailable", "cache.ios_ffs_file_inventory missing");
            detachCacheDbNoThrow(db);
            return 0;
        }
        const std::string created = nowUtc();
        db.begin();
        {
            auto upd = db.prepare("UPDATE ios_ffs_path_lookup SET lookup_source='reuse_cache_sqlite_referenced_path_lookup_available' WHERE source_id=? AND normalized_path='__vestigant_lookup_available__'");
            upd.bind(1, sourceId);
            upd.stepDone();
        }
        auto ins = db.prepare(
            "INSERT INTO ios_ffs_path_lookup(source_id,normalized_path,file_name,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,is_directory,lookup_source,created_utc) "
            "SELECT ?,c.normalized_path,c.file_name,c.size_bytes,c.zip_modified_utc,c.protection_class_hint,c.app_container_hint,c.domain_hint,c.is_directory,'reuse_cache_sqlite_referenced_path_hit',? "
            "FROM cache.ios_ffs_file_inventory c "
            "JOIN (SELECT DISTINCT normalized_ios_path AS normalized_path FROM vw_ios_spotlight_referenced_paths WHERE source_id=? AND COALESCE(normalized_ios_path,'')<>'' AND normalized_ios_path<>'__vestigant_lookup_available__') r "
            "ON c.normalized_path=r.normalized_path "
            "WHERE COALESCE(c.normalized_path,'')<>''");
        ins.bind(1, sourceId);
        ins.bind(2, created);
        ins.bind(3, sourceId);
        ins.stepDone();
        const std::size_t rows = static_cast<std::size_t>(std::max<long long>(0, sqliteChangesNoThrow(db)));
        db.commit();
        detachCacheDbNoThrow(db);
        writeStatus(statusWriter, caseDir, "ios_ffs_referenced_path_lookup_complete", "referenced_path_hits=" + std::to_string(rows));
        log.info("Imported referenced-only iOS FFS path lookup rows from reuse cache: " + std::to_string(rows));
        return rows;
    } catch (const std::exception& ex) {
        db.rollbackNoThrow();
        if (attached) detachCacheDbNoThrow(db);
        writeStatus(statusWriter, caseDir, "ios_ffs_referenced_path_lookup_warning", ex.what());
        log.warn(std::string("Referenced-only iOS FFS lookup import failed: ") + ex.what());
        return 0;
    } catch (...) {
        db.rollbackNoThrow();
        if (attached) detachCacheDbNoThrow(db);
        writeStatus(statusWriter, caseDir, "ios_ffs_referenced_path_lookup_warning", "unknown error");
        log.warn("Referenced-only iOS FFS lookup import failed: unknown error");
        return 0;
    }
}

} // namespace

void EvidenceIntake::importIosInventoryCsvs(CaseDatabase& db,
                                            const fs::path& caseDir,
                                            const std::string& sourceId,
                                            Logger& log,
                                            const fs::path& reuseCacheDir,
                                            bool materializeFullFfsInventory,
                                            RunStatusWriter statusWriter) {
    auto status = [&](const std::string& stage, const std::string& message = std::string()) {
        if (statusWriter) statusWriter(caseDir, stage, message);
    };
    fs::path fileCsv = caseDir / "ios_ffs_file_inventory.csv";
    fs::path dbCsv = caseDir / "ios_app_database_inventory.csv";
    std::error_code csvEc;
    if (!reuseCacheDir.empty() && !fs::is_regular_file(fileCsv, csvEc) && fs::is_regular_file(reuseCacheDir / "ios_ffs_file_inventory.csv", csvEc)) {
        fileCsv = reuseCacheDir / "ios_ffs_file_inventory.csv";
        log.info("Using cached iOS FFS inventory CSV: " + pathString(fileCsv));
    }
    if (!reuseCacheDir.empty() && !fs::is_regular_file(dbCsv, csvEc) && fs::is_regular_file(reuseCacheDir / "ios_app_database_inventory.csv", csvEc)) {
        dbCsv = reuseCacheDir / "ios_app_database_inventory.csv";
        log.info("Using cached iOS app database inventory CSV: " + pathString(dbCsv));
    }
    std::size_t fileRows = 0;
    std::size_t dbRows = 0;

    if (!reuseCacheDir.empty() && importIosInventoryFromCacheDatabase(db, caseDir, sourceId, log, reuseCacheDir, &fileRows, &dbRows, materializeFullFfsInventory, statusWriter)) {
        log.info(std::string(materializeFullFfsInventory ? "Imported" : "Referenced") + " iOS FFS inventory rows=" + std::to_string(fileRows) + " app_database_rows=" + std::to_string(dbRows) + " using cache SQLite fast path");
        status("ios_ffs_inventory_import", "files=" + std::to_string(fileRows) + " app_databases=" + std::to_string(dbRows) + " method=cache_sqlite" + (materializeFullFfsInventory ? " materialized_ffs=1" : " materialized_ffs=0"));
        return;
    }

    bool fastImportPragmasApplied = false;
    try {
        status("ios_ffs_inventory_csv_stream_import_start", "streaming CSV import without loading full inventories into memory");
        applyIosCsvBulkImportPragmas(db);
        fastImportPragmasApplied = true;
        status("ios_ffs_inventory_csv_stream_import_pragmas", "synchronous=OFF journal_mode=MEMORY for regenerable CSV intake only; restored after import");
        db.begin();
        {
            auto del = db.prepare("DELETE FROM ios_ffs_file_inventory WHERE source_id=?");
            del.bind(1, sourceId);
            del.stepDone();
        }
        {
            auto del = db.prepare("DELETE FROM ios_app_database_inventory WHERE source_id=?");
            del.bind(1, sourceId);
            del.stepDone();
        }
        {
            auto del = db.prepare("DELETE FROM ios_ffs_path_lookup WHERE source_id=?");
            del.bind(1, sourceId);
            del.stepDone();
        }
        if (fs::is_regular_file(fileCsv)) {
            if (materializeFullFfsInventory) {
                std::ifstream in(fileCsv, std::ios::binary);
                if (!in) throw std::runtime_error("Unable to open iOS FFS inventory CSV: " + pathString(fileCsv));
                const auto headers = readCsvHeaders(in);
                auto ins = db.prepare("INSERT INTO ios_ffs_file_inventory(source_id,original_zip_entry,normalized_path,file_name,extension,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,is_directory,sha256_status,inventory_notes,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
                std::string line;
                const std::string created = nowUtc();
                while (std::getline(in, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (trim(line).empty()) continue;
                    Row r = csvRowFromFields(headers, csvParseLine(line));
                    ins.bind(1, sourceId);
                    ins.bind(2, get(r, "original_zip_entry"));
                    ins.bind(3, get(r, "normalized_path"));
                    ins.bind(4, get(r, "file_name"));
                    ins.bind(5, get(r, "extension"));
                    ins.bind(6, parseInt64Safe(get(r, "size_bytes")));
                    ins.bind(7, get(r, "zip_modified_utc"));
                    ins.bind(8, get(r, "protection_class_hint"));
                    ins.bind(9, get(r, "app_container_hint"));
                    ins.bind(10, get(r, "domain_hint"));
                    ins.bind(11, parseInt64Safe(get(r, "is_directory")));
                    ins.bind(12, get(r, "sha256_status"));
                    ins.bind(13, get(r, "inventory_notes"));
                    ins.bind(14, created);
                    ins.stepDone();
                    ins.reset();
                    ++fileRows;
                    if ((fileRows % 100000) == 0) {
                        status("ios_ffs_inventory_csv_stream_progress", "files=" + std::to_string(fileRows));
                        log.info("Streaming iOS FFS inventory import progress files=" + std::to_string(fileRows));
                    }
                }
            } else {
                std::ifstream in(fileCsv, std::ios::binary);
                if (!in) throw std::runtime_error("Unable to open iOS FFS inventory CSV for slim lookup import: " + pathString(fileCsv));
                const auto headers = readCsvHeaders(in);
                auto insLookup = db.prepare("INSERT INTO ios_ffs_path_lookup(source_id,normalized_path,file_name,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,is_directory,lookup_source,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
                std::string line;
                const std::string created = nowUtc();
                while (std::getline(in, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (trim(line).empty()) continue;
                    Row r = csvRowFromFields(headers, csvParseLine(line));
                    const auto normalized = get(r, "normalized_path");
                    if (trim(normalized).empty()) continue;
                    insLookup.bind(1, sourceId);
                    insLookup.bind(2, normalized);
                    insLookup.bind(3, get(r, "file_name"));
                    insLookup.bind(4, parseInt64Safe(get(r, "size_bytes")));
                    insLookup.bind(5, get(r, "zip_modified_utc"));
                    insLookup.bind(6, get(r, "protection_class_hint"));
                    insLookup.bind(7, get(r, "app_container_hint"));
                    insLookup.bind(8, get(r, "domain_hint"));
                    insLookup.bind(9, parseInt64Safe(get(r, "is_directory")));
                    insLookup.bind(10, "reuse_cache_csv_slim_lookup");
                    insLookup.bind(11, created);
                    insLookup.stepDone();
                    insLookup.reset();
                    ++fileRows;
                    if ((fileRows % 100000) == 0) {
                        status("ios_ffs_inventory_csv_slim_lookup_progress", "paths=" + std::to_string(fileRows));
                        log.info("Streaming iOS FFS slim path lookup import progress paths=" + std::to_string(fileRows));
                    }
                }
                status("ios_ffs_inventory_csv_slim_lookup_imported", "full FFS inventory CSV not materialized; slim_lookup_rows=" + std::to_string(fileRows));
            }
        }
        if (fs::is_regular_file(dbCsv)) {
            std::ifstream in(dbCsv, std::ios::binary);
            if (!in) throw std::runtime_error("Unable to open iOS app database inventory CSV: " + pathString(dbCsv));
            const auto headers = readCsvHeaders(in);
            auto ins = db.prepare("INSERT INTO ios_app_database_inventory(source_id,original_zip_entry,normalized_path,database_name,database_category,app_hint,protection_class_hint,size_bytes,zip_modified_utc,parse_status,record_inventory_status,notes,extracted_path,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
            std::string line;
            const std::string created = nowUtc();
            while (std::getline(in, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (trim(line).empty()) continue;
                Row r = csvRowFromFields(headers, csvParseLine(line));
                ins.bind(1, sourceId);
                ins.bind(2, get(r, "original_zip_entry"));
                ins.bind(3, get(r, "normalized_path"));
                ins.bind(4, get(r, "database_name"));
                ins.bind(5, get(r, "database_category"));
                ins.bind(6, get(r, "app_hint"));
                ins.bind(7, get(r, "protection_class_hint"));
                ins.bind(8, parseInt64Safe(get(r, "size_bytes")));
                ins.bind(9, get(r, "zip_modified_utc"));
                ins.bind(10, get(r, "parse_status"));
                ins.bind(11, get(r, "record_inventory_status"));
                ins.bind(12, get(r, "notes"));
                ins.bind(13, get(r, "extracted_path"));
                ins.bind(14, created);
                ins.stepDone();
                ins.reset();
                ++dbRows;
                if ((dbRows % 5000) == 0) {
                    status("ios_app_database_inventory_csv_stream_progress", "app_databases=" + std::to_string(dbRows));
                    log.info("Streaming iOS app DB inventory import progress app_databases=" + std::to_string(dbRows));
                }
            }
        }
        db.commit();
        if (fastImportPragmasApplied) {
            restoreIosCsvBulkImportPragmasNoThrow(db);
            fastImportPragmasApplied = false;
        }
        log.info(std::string(materializeFullFfsInventory ? "Imported" : "Referenced") + " iOS FFS inventory rows=" + std::to_string(fileRows) + " app_database_rows=" + std::to_string(dbRows) + " using streaming CSV fallback");
        status("ios_ffs_inventory_import", "files=" + std::to_string(fileRows) + " app_databases=" + std::to_string(dbRows) + " method=csv_stream" + (materializeFullFfsInventory ? " materialized_ffs=1" : " materialized_ffs=0"));
    } catch (const std::exception& ex) {
        db.rollbackNoThrow();
        if (fastImportPragmasApplied) {
            restoreIosCsvBulkImportPragmasNoThrow(db);
            fastImportPragmasApplied = false;
        }
        log.warn(std::string("Unable to import iOS FFS/database inventory CSVs: ") + ex.what());
        status("ios_ffs_inventory_import_warning", ex.what());
    }
}


std::size_t EvidenceIntake::importReferencedIosPathLookupFromReuseCache(CaseDatabase& db,
                                                                       const fs::path& caseDir,
                                                                       const std::string& sourceId,
                                                                       Logger& log,
                                                                       const fs::path& reuseCacheDir,
                                                                       RunStatusWriter statusWriter) {
    return importReferencedIosPathLookupFromReuseCacheImpl(db, caseDir, sourceId, log, reuseCacheDir, statusWriter);
}

} // namespace vestigant::spotlight
