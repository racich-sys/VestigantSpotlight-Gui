#include "ingest/evidence_intake.h"

#include "core/path_utils.h"
#include <algorithm>
#include <array>
#include <fstream>

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

} // namespace vestigant::spotlight
