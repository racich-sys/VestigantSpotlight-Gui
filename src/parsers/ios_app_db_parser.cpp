#include "parsers/ios_app_db_parser.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace vestigant::spotlight {
namespace {

std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool contains(const std::string& s, const char* needle) {
    return s.find(needle ? needle : "") != std::string::npos;
}

} // namespace

std::string iosAppDbRecordCategory(const std::string& databaseCategory,
                                   const std::string& tableName,
                                   const std::string& columnsCsv) {
    const std::string c = lowerCopy(databaseCategory);
    const std::string t = lowerCopy(tableName);
    const std::string cols = lowerCopy(columnsCsv);
    if (contains(c, "knowledgec") || contains(c, "coreduet")) {
        if (t == "zobject") return "KNOWLEDGEC_EVENTS";
        if (t == "zstructuredmetadata") return "KNOWLEDGEC_METADATA";
        if (contains(t, "zobject") || contains(cols, "zstreamname")) return "KNOWLEDGEC_EVENTS";
        return "KNOWLEDGEC_SUPPORT_TABLE";
    }
    if (contains(c, "apple_messages")) {
        if (t == "message") return "MESSAGE_RECORDS";
        if (t == "attachment") return "MESSAGE_ATTACHMENTS";
        if (t == "handle" || t == "chat") return "MESSAGE_PARTICIPANTS";
        if (t == "recoverable_message_part" || t == "deleted_messages" ||
            t == "sync_deleted_messages" || t == "chat_recoverable_message_join" ||
            t == "unsynced_removed_recoverable_messages" || contains(t, "recoverable")) {
            return "MESSAGE_DELETED_OR_RECOVERABLE";
        }
        if (contains(t, "_join")) return "MESSAGES_JOIN_TABLE";
        if (contains(t, "message") || contains(t, "chat") || contains(t, "sync_deleted")) return "MESSAGES_SUPPORT_TABLE";
        return "MESSAGES_SUPPORT_TABLE";
    }
    if (contains(c, "call_history")) {
        if (contains(t, "call") || contains(cols, "call")) return "CALL_RECORDS";
        if (contains(t, "contact") || contains(t, "handle") || contains(t, "participant")) return "CALL_PARTICIPANTS";
        return "CALL_SUPPORT_TABLE";
    }
    if (contains(c, "whatsapp")) {
        if (t == "zwamessage" || contains(t, "message") || contains(cols, "zmessagedate")) return "MESSAGE_RECORDS";
        if (t == "zwamediaitem" || contains(t, "media") || contains(t, "attachment") || t == "zwavcardmention") return "MESSAGE_ATTACHMENTS";
        if (t == "zwachatsession" || contains(t, "chat") || contains(t, "session") || contains(t, "thread")) return "CHAT_RECORDS";
        if (t == "zwaaddressbookcontact" || t == "zwaprofilepushname" || t == "zwagroupmember" || contains(t, "contact") || contains(t, "member")) return "MESSAGE_PARTICIPANTS";
        if (contains(t, "call") || contains(cols, "zcall")) return "CALL_RECORDS";
        return "COMMUNICATIONS_SUPPORT_TABLE";
    }
    if (contains(c, "signal") || contains(c, "telegram")) {
        if (contains(t, "message") || contains(cols, "message")) return "MESSAGE_RECORDS";
        if (contains(t, "chat") || contains(t, "session") || contains(t, "thread")) return "CHAT_RECORDS";
        if (contains(t, "media") || contains(t, "attachment")) return "MESSAGE_ATTACHMENTS";
        return "COMMUNICATIONS_SUPPORT_TABLE";
    }
    if (contains(c, "safari") || contains(c, "chrome") || contains(c, "webkit")) {
        if (contains(t, "history") || contains(t, "url") || contains(cols, "url")) return "WEB_HISTORY";
        if (contains(t, "visit")) return "WEB_VISITS";
        if (contains(t, "cache")) return "WEB_CACHE";
        return "WEB_SUPPORT_TABLE";
    }
    if (contains(c, "mail")) {
        if (contains(t, "message") || contains(t, "mail")) return "MAIL_RECORDS";
        if (contains(t, "attach")) return "MESSAGE_ATTACHMENTS";
        return "MAIL_SUPPORT_TABLE";
    }
    if (contains(c, "calendar")) {
        if (contains(t, "event") || contains(t, "calendar")) return "CALENDAR_RECORDS";
        return "CALENDAR_SUPPORT_TABLE";
    }
    if (contains(c, "contact")) {
        if (contains(t, "person") || contains(t, "contact") || contains(t, "address")) return "CONTACT_RECORDS";
        return "CONTACT_SUPPORT_TABLE";
    }
    return "DATABASE_SUPPORT_TABLE";
}

bool iosAppDbShouldUseWhatsappSpecialParser(const std::string& databaseCategory,
                                            const std::string& tableName,
                                            const std::string& recordCategory) {
    const std::string c = lowerCopy(databaseCategory);
    const std::string t = lowerCopy(tableName);
    return contains(c, "whatsapp") && (t == "zwamessage" || recordCategory == "MESSAGE_RECORDS");
}

bool iosAppDbShouldUseAppleMessagesSpecialParser(const std::string& databaseCategory,
                                                 const std::string& tableName,
                                                 const std::string& recordCategory) {
    const std::string c = lowerCopy(databaseCategory);
    const std::string t = lowerCopy(tableName);
    return contains(c, "apple_messages") &&
           (t == "message" || t == "attachment" || t == "handle" || t == "chat" || recordCategory == "MESSAGE_DELETED_OR_RECOVERABLE");
}

bool iosAppDbShouldUseKnowledgeCSpecialParser(const std::string& recordCategory) {
    return recordCategory == "KNOWLEDGEC_EVENTS";
}

std::string iosAppDbBuildKnowledgeCTextSnippet(const std::string& stream,
                                               const std::string& bundleId,
                                               const std::string& startOrCreateDate,
                                               const std::string& endDate) {
    std::ostringstream os;
    if (!stream.empty()) os << "stream=" << stream;
    if (!bundleId.empty()) {
        if (os.tellp() > 0) os << "; ";
        os << "bundle=" << bundleId;
    }
    if (!startOrCreateDate.empty()) {
        if (os.tellp() > 0) os << "; ";
        os << "start=" << startOrCreateDate;
    }
    if (!endDate.empty()) {
        if (os.tellp() > 0) os << "; ";
        os << "end=" << endDate;
    }
    return os.str();
}

IosAppDbTableParseDecision iosAppDbBuildTableParseDecision(const std::string& databaseCategory,
                                                           const std::string& tableName,
                                                           const std::string& columnsCsv) {
    IosAppDbTableParseDecision d;
    d.recordCategory = iosAppDbRecordCategory(databaseCategory, tableName, columnsCsv);
    d.useWhatsAppSpecialParser = iosAppDbShouldUseWhatsappSpecialParser(databaseCategory, tableName, d.recordCategory);
    d.useAppleMessagesSpecialParser = iosAppDbShouldUseAppleMessagesSpecialParser(databaseCategory, tableName, d.recordCategory);
    d.useKnowledgeCSpecialParser = iosAppDbShouldUseKnowledgeCSpecialParser(d.recordCategory);
    if (d.useWhatsAppSpecialParser) d.parserFamily = "WHATSAPP_SPECIAL";
    else if (d.useAppleMessagesSpecialParser) d.parserFamily = "APPLE_MESSAGES_SPECIAL";
    else if (d.useKnowledgeCSpecialParser) d.parserFamily = "KNOWLEDGEC_SPECIAL";
    else d.parserFamily = "GENERIC_SQLITE_TABLE";
    return d;
}

} // namespace vestigant::spotlight
