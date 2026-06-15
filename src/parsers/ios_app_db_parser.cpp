#include "parsers/ios_app_db_parser.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace vestigant::spotlight {

std::string nowUtc();
std::string pathString(const std::filesystem::path& p);
namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string lowerCopy(std::string s) {
    return toLower(std::move(s));
}

std::string trim(const std::string& s) {
    const auto b = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; });
    const auto e = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
    if (b >= e) return {};
    return std::string(b, e);
}

bool contains(const std::string& s, const char* needle) {
    return s.find(needle ? needle : "") != std::string::npos;
}

std::string sqlLiteralLocal(const std::string& value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') out += "''";
        else out.push_back(c);
    }
    out += "'";
    return out;
}


std::string printableWindow(const std::string& s, std::size_t pos, std::size_t width) {
    if (pos >= s.size()) return {};
    const std::size_t end = std::min<std::size_t>(s.size(), pos + width);
    std::string out;
    out.reserve(end - pos);
    for (std::size_t i = pos; i < end; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 0x20 && c < 0x7f) out.push_back(static_cast<char>(c));
        else if (!out.empty() && out.back() != ' ') out.push_back(' ');
    }
    return trim(out);
}

std::string firstMetadataWindow(const std::string& text, const std::vector<const char*>& needles, std::size_t width = 96) {
    const std::string lower = lowerCopy(text);
    for (const char* needle : needles) {
        const std::string n = lowerCopy(needle ? std::string(needle) : std::string());
        const std::size_t pos = lower.find(n);
        if (pos != std::string::npos) return printableWindow(text, pos, width);
    }
    return {};
}


bool isEmailLocalChar(unsigned char c) {
    return std::isalnum(c) || c == '.' || c == '_' || c == '%' || c == '+' || c == '-' || c == '@';
}

std::string firstEmailLikeValue(const std::string& text) {
    const std::size_t at = text.find('@');
    if (at == std::string::npos) return {};
    std::size_t b = at;
    while (b > 0 && isEmailLocalChar(static_cast<unsigned char>(text[b - 1]))) --b;
    std::size_t e = at + 1;
    while (e < text.size() && isEmailLocalChar(static_cast<unsigned char>(text[e]))) ++e;
    std::string candidate = trim(text.substr(b, e - b));
    if (candidate.size() < 6 || candidate.find('.') == std::string::npos || candidate.front() == '@' || candidate.back() == '@') return {};
    while (!candidate.empty() && (candidate.back() == '.' || candidate.back() == ',' || candidate.back() == ';' || candidate.back() == ':')) candidate.pop_back();
    return candidate.size() <= 254 ? candidate : candidate.substr(0, 254);
}

bool looksLikeAppleAbsoluteTimeNumeric(const std::string& raw) {
    std::string s = trim(raw);
    while (!s.empty() && (s.front() == ':' || s.front() == '=' || s.front() == ' ')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ',' || s.back() == ';' || s.back() == ')' || s.back() == ']' || s.back() == '}')) s.pop_back();
    if (s.empty()) return false;
    int dots = 0;
    int leftDigits = 0;
    int rightDigits = 0;
    bool right = false;
    bool sawSeparatorOtherThanDot = false;
    for (char ch : s) {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            if (right) ++rightDigits;
            else ++leftDigits;
        } else if (ch == '.') {
            ++dots;
            right = true;
        } else if (ch == '+' || ch == '-' || ch == '(' || ch == ')' || ch == ' ') {
            sawSeparatorOtherThanDot = true;
        } else {
            return false;
        }
    }
    if (dots != 1 || sawSeparatorOtherThanDot) return false;
    if (leftDigits < 8 || leftDigits > 10 || rightDigits < 1 || rightDigits > 7) return false;
    char* end = nullptr;
    const double value = std::strtod(s.c_str(), &end);
    if (!end || *end != '\0') return false;
    // CFAbsoluteTime around 2016-2035 sits in this range. These values are common in KnowledgeC start/end fields.
    return value >= 473385600.0 && value <= 1072915200.0;
}

std::string firstPhoneLikeValue(const std::string& text) {
    std::string cur;
    int digits = 0;
    auto candidateOrEmpty = [&]() -> std::string {
        const std::string candidate = trim(cur);
        if (digits < 7) return {};
        if (looksLikeAppleAbsoluteTimeNumeric(candidate)) return {};
        return candidate;
    };
    for (unsigned char c : text) {
        const bool phoneChar = std::isdigit(c) || c == '+' || c == '-' || c == '(' || c == ')' || c == ' ' || c == '.';
        if (phoneChar) {
            cur.push_back(static_cast<char>(c));
            if (std::isdigit(c)) ++digits;
            if (cur.size() > 48) {
                cur.clear();
                digits = 0;
            }
        } else {
            const std::string candidate = candidateOrEmpty();
            if (!candidate.empty()) return candidate;
            cur.clear();
            digits = 0;
        }
    }
    return candidateOrEmpty();
}

std::string identityRecoveryHintFromText(const std::string& text) {
    std::string hint;
    const std::string email = firstEmailLikeValue(text);
    if (!email.empty()) hint += "Email: " + email + " ";
    const std::string phone = firstPhoneLikeValue(text);
    if (!phone.empty()) hint += "Phone: " + phone + " ";
    return trim(hint);
}

struct IosCommunicationDerivedFields {
    std::string recordCategory;
    std::string contact;
    std::string itemIdentifier;
    std::string title;
    std::string parseStatus;
    std::string provenance;
};

IosCommunicationDerivedFields deriveIosCommunicationFields(const std::string& recordCategory,
                                                            const std::string& contactOrParticipant,
                                                            const std::string& itemIdentifier,
                                                            const std::string& title,
                                                            const std::string& textSnippet,
                                                            const std::string& parseStatus,
                                                            const std::string& provenance) {
    IosCommunicationDerivedFields out{recordCategory, contactOrParticipant, itemIdentifier, title, parseStatus, provenance};
    const std::string combined = title + " " + textSnippet + " " + itemIdentifier + " " + contactOrParticipant + " " + provenance;
    const std::string lower = lowerCopy(combined);
    const bool suppressIdentityRecovery = lower.find("coreduet_interactionc_generic_row=true") != std::string::npos ||
                                          lower.find("no_direct_communication_conclusion=true") != std::string::npos;
    auto addProv = [&](const std::string& token) {
        if (out.provenance.find(token) == std::string::npos) out.provenance += (out.provenance.empty() ? "" : "; ") + token;
    };
    if (lower.find("kmditemauthor") != std::string::npos || lower.find("kmditemrecipient") != std::string::npos ||
        lower.find("kmditemphonenumbers") != std::string::npos || lower.find("kmditememailaddresses") != std::string::npos) {
        addProv("IDENTITY_BOUND_COMMUNICATION=True");
        if (out.contact.empty()) {
            std::string identity;
            if (lower.find("kmditemphonenumbers") != std::string::npos || lower.find("phone") != std::string::npos) identity += "[Phone_Match] ";
            if (lower.find("kmditememailaddresses") != std::string::npos || lower.find("email") != std::string::npos) identity += "[Email_Match] ";
            const std::string nearby = firstMetadataWindow(combined, {"kMDItemAuthor", "kMDItemRecipient", "kMDItemPhoneNumbers", "kMDItemEmailAddresses", "personHandle", "emailAddress"});
            if (!nearby.empty()) identity += nearby;
            out.contact = trim(identity);
        }
    }
    if (lower.find("kmditemdomainidentifier") != std::string::npos) {
        addProv("THREAD_VOLUME_TRACKING_ENABLED=True");
        if (out.itemIdentifier.empty()) {
            out.itemIdentifier = firstMetadataWindow(combined, {"kMDItemDomainIdentifier"}, 128);
        }
    }
    if (lower.find("kmditemexpirationdate") != std::string::npos || lower.find("_kmditemisdeleted") != std::string::npos) {
        if (out.recordCategory == "MESSAGE_RECORDS" || out.recordCategory == "CHAT_RECORDS" || out.recordCategory == "MAIL_RECORDS" || out.recordCategory == "COMMUNICATIONS_SUPPORT_TABLE") {
            out.recordCategory = "MESSAGE_DELETED_OR_RECOVERABLE";
            if (out.title.find("TOMBSTONE / DELETED COMMUNICATION") == std::string::npos) out.title = "TOMBSTONE / DELETED COMMUNICATION: " + out.title;
            addProv("SPOTLIGHT_DELETED_OR_EXPIRED_COMMUNICATION=True");
        }
    }
    if (!suppressIdentityRecovery && (lower.find("personhandle") != std::string::npos || lower.find("emailaddress") != std::string::npos || lower.find("insendmessageintent") != std::string::npos)) {
        addProv("INTENT_TARGET_HINT_PRESENT=True");
        if (out.contact.empty()) out.contact = "Extracted_Intent_Target: " + firstMetadataWindow(combined, {"personHandle", "emailAddress", "name"}, 96);
    }
    if (!suppressIdentityRecovery && out.contact.empty()) {
        std::string hardMatch;
        const std::size_t telPos = lower.find("tel:");
        if (telPos != std::string::npos) hardMatch += "Phone: " + printableWindow(combined, telPos + 4U, 32) + " ";
        const std::size_t mailPos = lower.find("mailto:");
        if (mailPos != std::string::npos) hardMatch += "Email: " + printableWindow(combined, mailPos + 7U, 80) + " ";
        if (!hardMatch.empty()) {
            out.contact = trim(hardMatch);
            addProv("IDENTITY_REGEX_RECOVERY=True");
        }
    }
    if (!suppressIdentityRecovery && out.contact.empty()) {
        const std::string genericIdentity = identityRecoveryHintFromText(combined);
        if (!genericIdentity.empty()) {
            out.contact = genericIdentity;
            addProv("IDENTITY_TEXT_PATTERN_RECOVERY=True");
        }
    }
    if (!suppressIdentityRecovery && out.itemIdentifier.empty()) {
        const std::string domainLike = firstMetadataWindow(combined, {"domainidentifier", "thread", "chat", "conversation", "message_guid", "guid"}, 128);
        if (!domainLike.empty()) {
            out.itemIdentifier = domainLike;
            addProv("THREAD_OR_IDENTIFIER_TEXT_PATTERN_RECOVERY=True");
        }
    }
    if (suppressIdentityRecovery) {
        addProv("IDENTITY_PROMOTION_SUPPRESSED_FOR_COREDUET_INTERACTIONC=True");
    }
    return out;
}


void appendUtf8Local(std::string& out, std::uint32_t cp) {
    if (cp == 0U) return;
    if (cp <= 0x7fU) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7ffU) {
        out.push_back(static_cast<char>(0xc0U | (cp >> 6U)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3fU)));
    } else if (cp <= 0xffffU) {
        out.push_back(static_cast<char>(0xe0U | (cp >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3fU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3fU)));
    } else if (cp <= 0x10ffffU) {
        out.push_back(static_cast<char>(0xf0U | (cp >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3fU)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3fU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3fU)));
    }
}

std::string ripBplistStrings(const std::string& raw) {
    if (raw.size() < 8 || raw.rfind("bplist", 0) != 0) return raw;
    std::string out = "[Extracted BPLIST Strings]: ";
    std::string cur;
    std::size_t appended = 0;
    auto flushCur = [&]() {
        if (cur.size() >= 4 && out.size() <= 12000) {
            if (appended++ > 0) out += " | ";
            out += cur;
        }
        cur.clear();
    };
    for (std::size_t i = 8; i < raw.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        if ((c >= 32 && c != 0x7fU) || c == '\n' || c == '\t') {
            cur.push_back(static_cast<char>(c));
        } else {
            flushCur();
            if (out.size() > 12000) break;
        }
    }
    flushCur();

    // UTF-16LE fallback: iOS bplists commonly store Unicode strings with alternating null bytes.
    // Extract bounded printable runs so Live Text/OCR names and non-ASCII evidence are not destroyed.
    for (std::size_t i = 8; i + 7 < raw.size() && out.size() <= 12000; ++i) {
        std::string wide;
        std::size_t j = i;
        std::size_t codeUnits = 0;
        for (; j + 1 < raw.size() && codeUnits < 512; j += 2U) {
            const std::uint16_t cp = static_cast<std::uint16_t>(static_cast<unsigned char>(raw[j]) | (static_cast<unsigned char>(raw[j + 1U]) << 8U));
            if (cp == 0U || cp == 0xffffU) break;
            if (cp < 0x20U && cp != '\n' && cp != '\t') break;
            appendUtf8Local(wide, cp);
            ++codeUnits;
        }
        if (codeUnits >= 3 && wide.size() >= 4) {
            if (appended++ > 0) out += " | ";
            out += wide;
            i = j > i ? j - 1U : i;
        }
    }
    return appended == 0 ? std::string("[BPLIST detected; no printable string run >=4 bytes]") : out;
}


std::string jsonEscapeLocal(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c >= 0x20 && c != 0x7f) out.push_back(static_cast<char>(c));
                else out += ' ';
                break;
        }
    }
    return out;
}

std::uint64_t readBplistUintBe(const std::string& raw, std::size_t pos, std::size_t width) {
    if (width == 0 || width > 8 || pos > raw.size() || width > raw.size() - pos) return 0;
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < width; ++i) v = (v << 8U) | static_cast<unsigned char>(raw[pos + i]);
    return v;
}

std::uint64_t readBplistUintBe(const std::vector<unsigned char>& raw, std::size_t pos, std::size_t width) {
    if (width == 0 || width > 8 || pos > raw.size() || width > raw.size() - pos) return 0;
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < width; ++i) v = (v << 8U) | raw[pos + i];
    return v;
}

std::uint64_t readBplistObjectCount(const std::vector<unsigned char>& bplist, std::size_t& cursor, std::uint8_t lowNibble) {
    if (lowNibble < 0x0fU) return lowNibble;
    if (cursor >= bplist.size()) return 0;
    const std::uint8_t marker = bplist[cursor++];
    if ((marker & 0xf0U) != 0x10U) return 0;
    const std::size_t intBytes = static_cast<std::size_t>(1U) << (marker & 0x0fU);
    const std::uint64_t count = readBplistUintBe(bplist, cursor, intBytes);
    cursor += intBytes;
    return count;
}

struct BplistDecodeContext {
    std::size_t callCount = 0;
    std::set<std::uint64_t> expandingComplexUids;
};

std::string resolveUid(std::uint64_t uid,
                       int depth,
                       const std::vector<std::uint64_t>& objOffsets,
                       const std::vector<unsigned char>& bplist,
                       std::size_t objectRefSize,
                       BplistDecodeContext& ctx) {
    if (++ctx.callCount > 2500U) return "\"<bplist_expansion_limit>\"";
    if (depth > 16) return "\"<recursion_limit>\"";
    if (uid >= objOffsets.size()) return "\"<invalid_uid>\"";
    const std::uint64_t objOffset64 = objOffsets[static_cast<std::size_t>(uid)];
    if (objOffset64 >= bplist.size()) return "\"<invalid_offset>\"";
    const std::size_t objOffset = static_cast<std::size_t>(objOffset64);
    const std::uint8_t marker = bplist[objOffset];
    const std::uint8_t hi = marker & 0xf0U;
    const std::uint8_t lo = marker & 0x0fU;
    if (marker == 0x00U) return "null";
    if (marker == 0x08U) return "false";
    if (marker == 0x09U) return "true";
    if (hi == 0x10U) {
        const std::size_t intBytes = static_cast<std::size_t>(1U) << lo;
        return std::to_string(readBplistUintBe(bplist, objOffset + 1, intBytes));
    }
    if (hi == 0x50U || hi == 0x60U) {
        std::size_t cursor = objOffset + 1;
        const std::uint64_t count = readBplistObjectCount(bplist, cursor, lo);
        if (count > 65536U) return "\"<string_too_large>\"";
        std::string text;
        if (hi == 0x50U) {
            if (cursor > bplist.size() || count > static_cast<std::uint64_t>(bplist.size() - cursor)) return "\"<invalid_string_bounds>\"";
            for (std::uint64_t i = 0; i < count && cursor + i < bplist.size() && text.size() < 4096; ++i) {
                const unsigned char c = bplist[cursor + static_cast<std::size_t>(i)];
                if ((c >= 0x20U && c != 0x7fU) || c == '\n' || c == '\t') text.push_back(static_cast<char>(c));
                else text.push_back(' ');
            }
        } else {
            if (cursor > bplist.size() || count > static_cast<std::uint64_t>((bplist.size() - cursor) / 2U)) return "\"<invalid_utf16_string_bounds>\"";
            for (std::uint64_t i = 0; i < count && cursor + (static_cast<std::size_t>(i) * 2U) + 1U < bplist.size() && text.size() < 4096; ++i) {
                const std::uint16_t c = static_cast<std::uint16_t>(readBplistUintBe(bplist, cursor + static_cast<std::size_t>(i) * 2U, 2));
                if (c >= 0x20U || c == '\n' || c == '\t') appendUtf8Local(text, c);
                else text.push_back(' ');
            }
        }
        return "\"" + jsonEscapeLocal(trim(text)) + "\"";
    }
    if (hi == 0xa0U) {
        if (!ctx.expandingComplexUids.insert(uid).second) return "\"<seen_uid_" + std::to_string(uid) + ">\"";
        std::size_t cursor = objOffset + 1;
        const std::uint64_t count = readBplistObjectCount(bplist, cursor, lo);
        if (count > 65536U) { ctx.expandingComplexUids.erase(uid); return "\"<array_too_large>\""; }
        if (objectRefSize == 0 || cursor > bplist.size() || count > static_cast<std::uint64_t>((bplist.size() - cursor) / objectRefSize)) {
            ctx.expandingComplexUids.erase(uid);
            return "\"<invalid_array_bounds>\"";
        }
        std::string json = "[";
        const std::uint64_t displayCount = std::min<std::uint64_t>(count, 256U);
        for (std::uint64_t i = 0; i < displayCount; ++i) {
            const std::uint64_t childUid = readBplistUintBe(bplist, cursor + static_cast<std::size_t>(i) * objectRefSize, objectRefSize);
            if (i > 0) json += ", ";
            json += resolveUid(childUid, depth + 1, objOffsets, bplist, objectRefSize, ctx);
        }
        if (count > displayCount) json += ", \"<array_truncated>\"";
        ctx.expandingComplexUids.erase(uid);
        return json + "]";
    }
    if (hi == 0xd0U) {
        if (!ctx.expandingComplexUids.insert(uid).second) return "\"<seen_uid_" + std::to_string(uid) + ">\"";
        std::size_t cursor = objOffset + 1;
        const std::uint64_t count = readBplistObjectCount(bplist, cursor, lo);
        if (count > 65536U) { ctx.expandingComplexUids.erase(uid); return "\"<dictionary_too_large>\""; }
        const std::size_t keyBase = cursor;
        if (objectRefSize == 0 || keyBase > bplist.size() || count > static_cast<std::uint64_t>((bplist.size() - keyBase) / objectRefSize)) {
            ctx.expandingComplexUids.erase(uid);
            return "\"<invalid_dictionary_bounds>\"";
        }
        const std::size_t valBase = keyBase + static_cast<std::size_t>(count) * objectRefSize;
        if (valBase > bplist.size() || count > static_cast<std::uint64_t>((bplist.size() - valBase) / objectRefSize)) {
            ctx.expandingComplexUids.erase(uid);
            return "\"<invalid_dictionary_bounds>\"";
        }
        std::string json = "{";
        const std::uint64_t displayCount = std::min<std::uint64_t>(count, 256U);
        for (std::uint64_t i = 0; i < displayCount; ++i) {
            const std::uint64_t keyUid = readBplistUintBe(bplist, keyBase + static_cast<std::size_t>(i) * objectRefSize, objectRefSize);
            const std::uint64_t valUid = readBplistUintBe(bplist, valBase + static_cast<std::size_t>(i) * objectRefSize, objectRefSize);
            if (i > 0) json += ", ";
            json += resolveUid(keyUid, depth + 1, objOffsets, bplist, objectRefSize, ctx) + ": " + resolveUid(valUid, depth + 1, objOffsets, bplist, objectRefSize, ctx);
        }
        if (count > displayCount) json += ", \"<dictionary_truncated>\": true";
        ctx.expandingComplexUids.erase(uid);
        return json + "}";
    }
    if (hi == 0x80U) {
        return "\"<uid:" + std::to_string(readBplistUintBe(bplist, objOffset + 1, static_cast<std::size_t>(lo) + 1U)) + ">\"";
    }
    return "\"<unparsed_bplist_type_0x" + std::to_string(marker) + ">\"";
}

std::string resolveUid(std::uint64_t uid, int depth, const std::vector<std::uint64_t>& objOffsets, const std::vector<unsigned char>& bplist, std::size_t objectRefSize) {
    BplistDecodeContext ctx;
    return resolveUid(uid, depth, objOffsets, bplist, objectRefSize, ctx);
}

std::string resolveUid(std::uint64_t uid, int depth, const std::vector<std::uint64_t>& objOffsets, const std::vector<unsigned char>& bplist) {
    BplistDecodeContext ctx;
    return resolveUid(uid, depth, objOffsets, bplist, 1, ctx);
}

std::string unflattenNSKeyedArchiverOrBplist(const std::string& raw) {
    if (raw.size() < 40 || raw.rfind("bplist", 0) != 0) return raw;
    const std::size_t trailer = raw.size() - 32U;
    const std::size_t offsetIntSize = static_cast<unsigned char>(raw[trailer + 6U]);
    const std::size_t objectRefSize = static_cast<unsigned char>(raw[trailer + 7U]);
    const std::uint64_t numObjects = readBplistUintBe(raw, trailer + 8U, 8);
    const std::uint64_t topObject = readBplistUintBe(raw, trailer + 16U, 8);
    const std::uint64_t offsetTableOffset = readBplistUintBe(raw, trailer + 24U, 8);
    if (offsetIntSize == 0 || offsetIntSize > 8 || objectRefSize == 0 || objectRefSize > 8 || numObjects == 0 || numObjects > 100000 || offsetTableOffset >= raw.size()) return ripBplistStrings(raw);
    std::vector<std::uint64_t> offsets;
    offsets.reserve(static_cast<std::size_t>(numObjects));
    for (std::uint64_t i = 0; i < numObjects; ++i) {
        const std::uint64_t off = readBplistUintBe(raw, static_cast<std::size_t>(offsetTableOffset) + static_cast<std::size_t>(i) * offsetIntSize, offsetIntSize);
        if (off >= raw.size()) return ripBplistStrings(raw);
        offsets.push_back(off);
    }
    std::vector<unsigned char> bytes(raw.begin(), raw.end());
    std::string resolved = resolveUid(topObject, 0, offsets, bytes, objectRefSize);
    if (resolved.empty() || resolved.find("<unparsed") != std::string::npos) return ripBplistStrings(raw);
    if (resolved.size() > 12000) resolved.resize(12000);
    return "[Decoded BPLIST Top Object]: " + resolved;
}

std::string sqlIdentLocal(const std::string& name) {
    std::string out = "\"";
    for (char c : name) out += (c == '"') ? "\"\"" : std::string(1, c);
    out += "\"";
    return out;
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
        if (t == "zinteractions") return "COREDUET_INTERACTIONS";
        if (t == "zcontacts") return "COREDUET_CONTACTS";
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
        if (contains(t, "download")) return "WEB_DOWNLOADS";
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
    if (contains(c, "notes") || contains(t, "znote") || contains(t, "note")) return "NOTES_RECORDS";
    if (contains(c, "location") || contains(c, "maps") || contains(t, "location") || contains(t, "place") || contains(t, "map")) return "LOCATION_RECORDS";
    return "DATABASE_SUPPORT_TABLE";
}

bool iosAppDbIsTargetRecordCategory(const std::string& category) {
    static const std::set<std::string> target = {
        "MESSAGE_RECORDS", "MESSAGE_ATTACHMENTS", "MESSAGE_PARTICIPANTS", "MESSAGE_DELETED_OR_RECOVERABLE",
        "CALL_RECORDS", "CALL_PARTICIPANTS",
        "WEB_HISTORY", "WEB_VISITS", "WEB_CACHE", "WEB_DOWNLOADS",
        "MAIL_RECORDS", "CALENDAR_RECORDS", "CONTACT_RECORDS",
        "CHAT_RECORDS", "KNOWLEDGEC_EVENTS", "COREDUET_INTERACTIONS",
        "NOTES_RECORDS", "LOCATION_RECORDS"
    };
    return target.find(category) != target.end();
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

namespace {

std::string sqliteColumnText(sqlite3_stmt* st, int index) {
    const unsigned char* txt = sqlite3_column_text(st, index);
    return txt ? reinterpret_cast<const char*>(txt) : std::string();
}

std::string formatUnixSecondsUtc(long long seconds) {
    if (seconds <= 0) return {};
    std::time_t t = static_cast<std::time_t>(seconds);
    std::tm tm{};
#if defined(_WIN32)
    if (gmtime_s(&tm, &t) != 0) return {};
#else
    if (!gmtime_r(&t, &tm)) return {};
#endif
    char buf[32]{};
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm) == 0) return {};
    return std::string(buf);
}

std::pair<std::string, std::string> normalizeIosAppTimestamp(const std::string& raw, const std::string& columnName) {
    const std::string v = trim(raw);
    if (v.empty()) return {};
    if (v.find('-') != std::string::npos && v.find('T') != std::string::npos) return {v, columnName + ":iso"};
    try {
        long double n = std::stold(v);
        if (n <= 0) return {};
        long double seconds = 0;
        std::string method;
        if (n > 10000000000000000.0L) {
            seconds = (n / 1000000000.0L) + 978307200.0L;
            method = columnName + ":apple_epoch_nanoseconds";
        } else if (n > 100000000000000.0L) {
            seconds = n / 1000000.0L;
            method = columnName + ":unix_epoch_microseconds";
        } else if (n > 1000000000000.0L) {
            seconds = n / 1000.0L;
            method = columnName + ":unix_epoch_milliseconds";
        } else if (n > 1000000000.0L) {
            seconds = n;
            method = columnName + ":unix_epoch_seconds";
        } else {
            seconds = n + 978307200.0L;
            method = columnName + ":apple_epoch_seconds";
        }
        const auto utc = formatUnixSecondsUtc(static_cast<long long>(seconds));
        if (utc.empty()) return {};
        return {utc, method};
    } catch (...) {
        return {};
    }
}

int findColumnIndex(const std::map<std::string, int>& lowerColumns, const std::vector<std::string>& candidates, bool allowContains = false) {
    for (const auto& c : candidates) {
        auto it = lowerColumns.find(toLower(c));
        if (it != lowerColumns.end()) return it->second;
    }
    if (allowContains) {
        for (const auto& c : candidates) {
            const std::string needle = toLower(c);
            for (const auto& kv : lowerColumns) {
                if (kv.first.find(needle) != std::string::npos) return kv.second;
            }
        }
    }
    return -1;
}

std::string columnValue(sqlite3_stmt* st, int index, std::size_t maxLen = 2000) {
    if (index < 0) return {};
    std::string v = sqliteColumnText(st, index);
    if (v.size() > maxLen) v.resize(maxLen);
    return v;
}

std::string summarizeSqliteRowValues(sqlite3_stmt* st,
                                     const std::vector<std::string>& preferredColumns,
                                     std::size_t maxLen = 2000) {
    const int colCount = sqlite3_column_count(st);
    std::set<int> used;
    std::string out;
    auto appendCol = [&](int i) {
        if (i < 0 || i >= colCount || used.find(i) != used.end()) return;
        const int typ = sqlite3_column_type(st, i);
        if (typ == SQLITE_NULL || typ == SQLITE_BLOB) return;
        const char* namePtr = sqlite3_column_name(st, i);
        if (!namePtr) return;
        const std::string name = namePtr;
        if (name == "__rowid__") return;
        std::string value = columnValue(st, i, 512);
        value = trim(value);
        if (value.empty()) return;
        if (!out.empty()) out += "; ";
        out += name + "=" + value;
        used.insert(i);
        if (out.size() > maxLen) out.resize(maxLen);
    };
    std::map<std::string,int> lowerCols;
    for (int i = 0; i < colCount; ++i) {
        const char* namePtr = sqlite3_column_name(st, i);
        if (namePtr) lowerCols[toLower(namePtr)] = i;
    }
    for (const auto& pref : preferredColumns) {
        const std::string lp = toLower(pref);
        auto it = lowerCols.find(lp);
        if (it != lowerCols.end()) appendCol(it->second);
    }
    for (int i = 0; i < colCount && out.size() < maxLen; ++i) {
        const char* namePtr = sqlite3_column_name(st, i);
        if (!namePtr) continue;
        const std::string lname = toLower(namePtr);
        if (lname.find("data") != std::string::npos || lname.find("blob") != std::string::npos || lname.find("cache") != std::string::npos) continue;
        appendCol(i);
    }
    return out;
}


bool sqliteTableExistsLocal(sqlite3* ext, const std::string& tableName) {
    sqlite3_stmt* st = nullptr;
    int rc = sqlite3_prepare_v2(ext, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1", -1, &st, nullptr);
    if (rc != SQLITE_OK || !st) return false;
    sqlite3_bind_text(st, 1, tableName.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return ok;
}

bool sqliteColumnExistsLocal(sqlite3* ext, const std::string& tableName, const std::string& columnName) {
    std::string sql = "PRAGMA table_info(" + sqlIdentLocal(tableName) + ")";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr) != SQLITE_OK || !st) return false;
    const std::string target = toLower(columnName);
    bool found = false;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* txt = sqlite3_column_text(st, 1);
        if (txt && toLower(reinterpret_cast<const char*>(txt)) == target) { found = true; break; }
    }
    sqlite3_finalize(st);
    return found;
}

long long sqliteScalarCountLocal(sqlite3* ext, const std::string& tableName) {
    std::string sql = "SELECT COUNT(*) FROM " + sqlIdentLocal(tableName);
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr) != SQLITE_OK || !st) return -1;
    long long count = -1;
    if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return count;
}

std::string sqliteColumnListLocal(sqlite3* ext, const std::string& tableName) {
    std::string sql = "PRAGMA table_info(" + sqlIdentLocal(tableName) + ")";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr) != SQLITE_OK || !st) return "";
    std::string out;
    int shown = 0;
    while (sqlite3_step(st) == SQLITE_ROW && shown < 40) {
        const unsigned char* txt = sqlite3_column_text(st, 1);
        if (txt) {
            if (!out.empty()) out += ",";
            out += reinterpret_cast<const char*>(txt);
            ++shown;
        }
    }
    sqlite3_finalize(st);
    return out;
}

std::string sqliteOptionalColumnExpr(sqlite3* ext, const std::string& tableName, const std::string& alias, const std::string& columnName, const std::string& outAlias) {
    if (sqliteColumnExistsLocal(ext, tableName, columnName)) return alias + "." + sqlIdentLocal(columnName) + " AS " + sqlIdentLocal(outAlias);
    return "NULL AS " + sqlIdentLocal(outAlias);
}

void bindIosParsedRecord(SqlStatement& parsedIns,
                         const std::string& sourceId,
                         const IosAppDbInventory& inv,
                         const std::string& table,
                         const std::string& recordCategory,
                         const std::string& sourcePk,
                         const std::string& recordTimestampUtc,
                         const std::string& timestampSource,
                         const std::string& contactOrParticipant,
                         const std::string& url,
                         const std::string& title,
                         const std::string& filePath,
                         const std::string& itemIdentifier,
                         const std::string& textSnippet,
                         const std::string& parseStatus,
                         const std::string& provenance) {
    std::string augmentedProvenance = provenance;
    auto addAugmentedProvenance = [&](const std::string& token) {
        if (augmentedProvenance.find(token) == std::string::npos) {
            augmentedProvenance += (augmentedProvenance.empty() ? "" : "; ") + token;
        }
    };
    if (filePath.find("/.Trash/") != std::string::npos || filePath.find("/.Trashes/") != std::string::npos) {
        addAugmentedProvenance("TRASH_PATH_COMPONENT_PRESENT=True");
    }
    if (textSnippet.find("LSQuarantine") != std::string::npos || textSnippet.find("com.apple.quarantine") != std::string::npos) {
        addAugmentedProvenance("QUARANTINE_METADATA_REFERENCE_PRESENT=True");
    }
    if (textSnippet.find("kMDItemExpirationDate") != std::string::npos || textSnippet.find("_kMDItemIsDeleted") != std::string::npos) {
        addAugmentedProvenance("SPOTLIGHT_DELETED_OR_EXPIRED_REFERENCE_PRESENT=True");
    }
    const auto derived = deriveIosCommunicationFields(recordCategory, contactOrParticipant, itemIdentifier, title, textSnippet, parseStatus, augmentedProvenance);
    int i = 1;
    parsedIns.bind(i++, sourceId);
    parsedIns.bind(i++, inv.id);
    parsedIns.bind(i++, inv.norm);
    parsedIns.bind(i++, inv.name);
    parsedIns.bind(i++, inv.cat);
    parsedIns.bind(i++, inv.app);
    parsedIns.bind(i++, table);
    parsedIns.bind(i++, derived.recordCategory);
    parsedIns.bind(i++, sourcePk);
    parsedIns.bind(i++, recordTimestampUtc);
    parsedIns.bind(i++, timestampSource);
    parsedIns.bind(i++, derived.contact);
    parsedIns.bind(i++, url);
    parsedIns.bind(i++, derived.title);
    parsedIns.bind(i++, filePath);
    parsedIns.bind(i++, derived.itemIdentifier);
    parsedIns.bind(i++, textSnippet);
    parsedIns.bind(i++, derived.parseStatus);
    parsedIns.bind(i++, derived.provenance);
    parsedIns.bind(i++, sqlNowUtc());
    parsedIns.stepDone();
    parsedIns.reset();
}

std::size_t parseAppleMessagesSmsDbMessageRows(const std::string& sourceId,
                                               const IosAppDbInventory& inv,
                                               sqlite3* ext,
                                               SqlStatement& parsedIns) {
    if (!sqliteTableExistsLocal(ext, "message")) return 0;
    constexpr int MaxRowsPerTable = 100000;
    const bool hasHandle = sqliteTableExistsLocal(ext, "handle");
    const bool hasChatJoin = sqliteTableExistsLocal(ext, "chat") && sqliteTableExistsLocal(ext, "chat_message_join");
    const bool hasAttachmentJoin = sqliteTableExistsLocal(ext, "attachment") && sqliteTableExistsLocal(ext, "message_attachment_join");
    std::string sql = "SELECT m.ROWID AS rowid, "
        + sqliteOptionalColumnExpr(ext, "message", "m", "guid", "message_guid") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "text", "message_text") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "date", "message_date") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "date_read", "date_read") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "date_delivered", "date_delivered") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "is_from_me", "is_from_me") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "is_sent", "is_sent") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "is_delivered", "is_delivered") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "is_read", "is_read") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "service", "message_service") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "account", "message_account") + ", "
        + sqliteOptionalColumnExpr(ext, "message", "m", "handle_id", "message_handle_id") + ", ";
    if (hasHandle) {
        sql += "h.id AS resolved_handle, h.service AS handle_service, ";
    } else {
        sql += "NULL AS resolved_handle, NULL AS handle_service, ";
    }
    if (hasChatJoin) {
        sql += "(SELECT group_concat(COALESCE(c.chat_identifier,c.display_name,c.guid), '|') FROM chat_message_join cmj JOIN chat c ON c.ROWID=cmj.chat_id WHERE cmj.message_id=m.ROWID) AS chat_context, ";
    } else {
        sql += "NULL AS chat_context, ";
    }
    if (hasAttachmentJoin) {
        sql += "a.ROWID AS attachment_rowid, "
               "a.guid AS attachment_guid, "
               "a.transfer_name AS attachment_transfer_name, "
               "a.filename AS attachment_filename, "
               "a.mime_type AS attachment_mime_type, "
               "a.uti AS attachment_uti, "
               "a.total_bytes AS attachment_total_bytes, "
               "a.created_date AS attachment_created_date ";
    } else {
        sql += "NULL AS attachment_rowid, NULL AS attachment_guid, NULL AS attachment_transfer_name, NULL AS attachment_filename, NULL AS attachment_mime_type, NULL AS attachment_uti, NULL AS attachment_total_bytes, NULL AS attachment_created_date ";
    }
    sql += "FROM message m ";
    if (hasHandle) sql += "LEFT JOIN handle h ON h.ROWID=m.handle_id ";
    if (hasAttachmentJoin) sql += "LEFT JOIN message_attachment_join maj ON maj.message_id=m.ROWID LEFT JOIN attachment a ON a.ROWID=maj.attachment_id ";
    sql += "ORDER BY m.ROWID LIMIT ?";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr) != SQLITE_OK || !st) return 0;
    sqlite3_bind_int(st, 1, MaxRowsPerTable);
    std::size_t rows = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const std::string rowid = columnValue(st, 0, 128);
        const std::string guid = columnValue(st, 1, 512);
        const std::string text = columnValue(st, 2, 2000);
        const auto msgTs = normalizeIosAppTimestamp(columnValue(st, 3, 128), "message.date");
        const auto readTs = normalizeIosAppTimestamp(columnValue(st, 4, 128), "message.date_read");
        const auto deliveredTs = normalizeIosAppTimestamp(columnValue(st, 5, 128), "message.date_delivered");
        const std::string isFromMe = columnValue(st, 6, 16);
        const std::string isSent = columnValue(st, 7, 16);
        const std::string isDelivered = columnValue(st, 8, 16);
        const std::string isRead = columnValue(st, 9, 16);
        const std::string service = columnValue(st, 10, 128);
        const std::string account = columnValue(st, 11, 512);
        std::string contact = columnValue(st, 13, 512);
        if (contact.empty()) contact = columnValue(st, 12, 128);
        const std::string handleService = columnValue(st, 14, 128);
        const std::string chat = columnValue(st, 15, 1000);
        const std::string attachmentRowId = columnValue(st, 16, 128);
        const std::string attachmentGuid = columnValue(st, 17, 512);
        const std::string attachmentTransferName = columnValue(st, 18, 1000);
        const std::string attachmentFilename = columnValue(st, 19, 1500);
        const std::string attachmentMime = columnValue(st, 20, 256);
        const std::string attachmentUti = columnValue(st, 21, 256);
        const std::string attachmentBytes = columnValue(st, 22, 64);
        const auto attachmentTs = normalizeIosAppTimestamp(columnValue(st, 23, 128), "attachment.created_date");
        std::string direction = "unknown_direction";
        if (isFromMe == "1") direction = "outgoing";
        else if (isFromMe == "0") direction = "incoming";
        std::string title = "Apple Messages " + direction;
        if (!service.empty()) title += " service=" + service;
        if (!attachmentTransferName.empty()) title += " attachment=" + attachmentTransferName;
        std::string itemIdentifier = guid;
        if (!attachmentGuid.empty()) itemIdentifier += "|attachment=" + attachmentGuid;
        std::string snippet = text;
        if (!attachmentFilename.empty() || !attachmentMime.empty() || !attachmentBytes.empty()) {
            if (!snippet.empty()) snippet += " | ";
            snippet += "attachment_name=" + attachmentTransferName + " attachment_path=" + attachmentFilename + " mime_type=" + attachmentMime + " uti=" + attachmentUti + " bytes=" + attachmentBytes;
        }
        std::string provenance = "ios_sms_db_message_joined_like_ileapp message_left_join_handle_chat_attachment direction=" + direction;
        if (!chat.empty()) provenance += " chat=" + chat;
        if (!handleService.empty()) provenance += " handle_service=" + handleService;
        if (!account.empty()) provenance += " account=" + account;
        if (!readTs.first.empty()) provenance += " read_utc=" + readTs.first;
        if (!deliveredTs.first.empty()) provenance += " delivered_utc=" + deliveredTs.first;
        if (!attachmentTs.first.empty()) provenance += " attachment_created_utc=" + attachmentTs.first;
        if (!isSent.empty()) provenance += " is_sent=" + isSent;
        if (!isDelivered.empty()) provenance += " is_delivered=" + isDelivered;
        if (!isRead.empty()) provenance += " is_read=" + isRead;
        if (!attachmentRowId.empty()) provenance += " attachment_rowid=" + attachmentRowId;
        bindIosParsedRecord(parsedIns, sourceId, inv, "message", "MESSAGE_RECORDS", rowid,
                            msgTs.first, msgTs.second, contact, "", title, attachmentFilename,
                            itemIdentifier, snippet, "parsed_apple_messages_smsdb_message_joined", provenance);
        ++rows;
    }
    sqlite3_finalize(st);
    return rows;
}

std::size_t parseAppleMessagesSmsDbParticipantRows(const std::string& sourceId,
                                                   const IosAppDbInventory& inv,
                                                   sqlite3* ext,
                                                   const std::string& table,
                                                   SqlStatement& parsedIns) {
    const std::string t = toLower(table);
    if (t != "handle" && t != "chat") return 0;
    if (!sqliteTableExistsLocal(ext, table)) return 0;
    constexpr int MaxRowsPerTable = 100000;
    std::string sql;
    if (t == "handle") {
        sql = "SELECT h.ROWID AS rowid, "
            + sqliteOptionalColumnExpr(ext, "handle", "h", "id", "handle_id_value") + ", "
            + sqliteOptionalColumnExpr(ext, "handle", "h", "service", "handle_service") + ", "
            + sqliteOptionalColumnExpr(ext, "handle", "h", "country", "country") + ", "
            + sqliteOptionalColumnExpr(ext, "handle", "h", "uncanonicalized_id", "uncanonicalized_id") + ", "
            + sqliteOptionalColumnExpr(ext, "handle", "h", "person_centric_id", "person_centric_id")
            + " FROM handle h ORDER BY h.ROWID LIMIT ?";
    } else {
        sql = "SELECT c.ROWID AS rowid, "
            + sqliteOptionalColumnExpr(ext, "chat", "c", "guid", "chat_guid") + ", "
            + sqliteOptionalColumnExpr(ext, "chat", "c", "chat_identifier", "chat_identifier") + ", "
            + sqliteOptionalColumnExpr(ext, "chat", "c", "service_name", "service_name") + ", "
            + sqliteOptionalColumnExpr(ext, "chat", "c", "display_name", "display_name") + ", "
            + sqliteOptionalColumnExpr(ext, "chat", "c", "group_id", "group_id") + ", "
            + sqliteOptionalColumnExpr(ext, "chat", "c", "account_login", "account_login") + ", "
            + sqliteOptionalColumnExpr(ext, "chat", "c", "last_read_message_timestamp", "last_read_message_timestamp")
            + " FROM chat c ORDER BY c.ROWID LIMIT ?";
    }
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr) != SQLITE_OK || !st) return 0;
    sqlite3_bind_int(st, 1, MaxRowsPerTable);
    std::size_t rows = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const std::string rowid = columnValue(st, 0, 128);
        if (t == "handle") {
            const std::string handleId = columnValue(st, 1, 512);
            const std::string service = columnValue(st, 2, 128);
            const std::string country = columnValue(st, 3, 64);
            const std::string uncanon = columnValue(st, 4, 512);
            const std::string personId = columnValue(st, 5, 256);
            std::string title = "Apple Messages handle";
            if (!service.empty()) title += " service=" + service;
            std::string snippet = "country=" + country + " uncanonicalized_id=" + uncanon + " person_centric_id=" + personId;
            bindIosParsedRecord(parsedIns, sourceId, inv, "handle", "MESSAGE_PARTICIPANTS", rowid,
                                "", "", handleId, "", title, "", handleId, snippet,
                                "parsed_apple_messages_smsdb_handle", "ios_sms_db_handle_participant");
        } else {
            const std::string guid = columnValue(st, 1, 512);
            const std::string chatIdentifier = columnValue(st, 2, 512);
            const std::string service = columnValue(st, 3, 128);
            const std::string displayName = columnValue(st, 4, 512);
            const std::string groupId = columnValue(st, 5, 512);
            const std::string accountLogin = columnValue(st, 6, 512);
            const auto lastReadTs = normalizeIosAppTimestamp(columnValue(st, 7, 128), "chat.last_read_message_timestamp");
            std::string title = displayName.empty() ? "Apple Messages chat" : displayName;
            if (!service.empty()) title += " service=" + service;
            std::string snippet = "group_id=" + groupId + " account_login=" + accountLogin;
            bindIosParsedRecord(parsedIns, sourceId, inv, "chat", "MESSAGE_PARTICIPANTS", rowid,
                                lastReadTs.first, lastReadTs.second, chatIdentifier, "", title, "", guid, snippet,
                                "parsed_apple_messages_smsdb_chat", "ios_sms_db_chat_participant");
        }
        ++rows;
    }
    sqlite3_finalize(st);
    return rows;
}

std::size_t parseAppleMessagesSmsDbAttachmentRows(const std::string& sourceId,
                                                  const IosAppDbInventory& inv,
                                                  sqlite3* ext,
                                                  SqlStatement& parsedIns) {
    if (!sqliteTableExistsLocal(ext, "attachment")) return 0;
    constexpr int MaxRowsPerTable = 100000;
    const bool hasJoin = sqliteTableExistsLocal(ext, "message_attachment_join") && sqliteTableExistsLocal(ext, "message");
    std::string sql = "SELECT a.ROWID AS rowid, "
        + sqliteOptionalColumnExpr(ext, "attachment", "a", "guid", "attachment_guid") + ", "
        + sqliteOptionalColumnExpr(ext, "attachment", "a", "filename", "filename") + ", "
        + sqliteOptionalColumnExpr(ext, "attachment", "a", "transfer_name", "transfer_name") + ", "
        + sqliteOptionalColumnExpr(ext, "attachment", "a", "mime_type", "mime_type") + ", "
        + sqliteOptionalColumnExpr(ext, "attachment", "a", "uti", "uti") + ", "
        + sqliteOptionalColumnExpr(ext, "attachment", "a", "total_bytes", "total_bytes") + ", "
        + sqliteOptionalColumnExpr(ext, "attachment", "a", "created_date", "created_date") + ", ";
    if (hasJoin) {
        sql += "(SELECT group_concat(m.guid, '|') FROM message_attachment_join maj JOIN message m ON m.ROWID=maj.message_id WHERE maj.attachment_id=a.ROWID) AS message_guids ";
    } else {
        sql += "NULL AS message_guids ";
    }
    sql += "FROM attachment a ORDER BY a.ROWID LIMIT ?";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr) != SQLITE_OK || !st) return 0;
    sqlite3_bind_int(st, 1, MaxRowsPerTable);
    std::size_t rows = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const std::string rowid = columnValue(st, 0, 128);
        const std::string guid = columnValue(st, 1, 512);
        const std::string filename = columnValue(st, 2, 1500);
        const std::string transferName = columnValue(st, 3, 1000);
        const std::string mime = columnValue(st, 4, 256);
        const std::string uti = columnValue(st, 5, 256);
        const std::string totalBytes = columnValue(st, 6, 64);
        const auto ts = normalizeIosAppTimestamp(columnValue(st, 7, 128), "attachment.created_date");
        const std::string msgGuids = columnValue(st, 8, 1000);
        std::string snippet = "mime_type=" + mime + " uti=" + uti + " total_bytes=" + totalBytes;
        std::string provenance = "ios_sms_db_attachment";
        if (!msgGuids.empty()) provenance += " message_guids=" + msgGuids;
        bindIosParsedRecord(parsedIns, sourceId, inv, "attachment", "MESSAGE_ATTACHMENTS", rowid,
                            ts.first, ts.second, "", "", transferName, filename, guid, snippet,
                            "parsed_apple_messages_smsdb_attachment", provenance);
        ++rows;
    }
    sqlite3_finalize(st);
    return rows;
}


std::string firstColumnValue(sqlite3_stmt* st,
                             const std::map<std::string, int>& cols,
                             const std::vector<std::string>& candidates,
                             std::size_t maxLen = 2000) {
    int idx = findColumnIndex(cols, candidates, false);
    if (idx < 0) idx = findColumnIndex(cols, candidates, true);
    return columnValue(st, idx, maxLen);
}

std::size_t parseWhatsAppIosTableRows(const std::string& sourceId,
                                      const IosAppDbInventory& inv,
                                      sqlite3* ext,
                                      const std::string& table,
                                      const std::string& recordCategory,
                                      SqlStatement& parsedIns) {
    const std::string t = toLower(table);
    const std::string c = toLower(inv.cat);
    if (c.find("whatsapp") == std::string::npos) return 0;
    if (!sqliteTableExistsLocal(ext, table)) return 0;
    constexpr int MaxRowsPerTable = 100000;
    std::string sql = "SELECT rowid AS __rowid__, * FROM " + sqlIdentLocal(table) + " LIMIT ?";
    sqlite3_stmt* st = nullptr;
    int rc = sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr);
    if (rc != SQLITE_OK) {
        sql = "SELECT * FROM " + sqlIdentLocal(table) + " LIMIT ?";
        rc = sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr);
    }
    if (rc != SQLITE_OK || !st) return 0;
    sqlite3_bind_int(st, 1, MaxRowsPerTable);
    const int colCount = sqlite3_column_count(st);
    std::map<std::string, int> cols;
    for (int i = 0; i < colCount; ++i) {
        const char* name = sqlite3_column_name(st, i);
        if (name) cols[toLower(name)] = i;
    }
    std::size_t rows = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        std::string recCat = recordCategory;
        std::string sourcePk = firstColumnValue(st, cols, {"__rowid__", "z_pk", "rowid", "id"}, 128);
        std::string dateRaw;
        std::string dateSource;
        std::string contact;
        std::string url;
        std::string title;
        std::string filePath;
        std::string itemIdentifier;
        std::string textSnippet;
        std::string parseStatus = "parsed_whatsapp_ios_row";
        std::string provenance = "ios_whatsapp_schema_reference=whatsapp_chat_exporter_0_13_0 table=" + table;
        if (t == "zwamessage" || t.find("message") != std::string::npos) {
            recCat = "MESSAGE_RECORDS";
            dateRaw = firstColumnValue(st, cols, {"zmessagedate", "zdate", "date"}, 128);
            dateSource = "ZWAMESSAGE.ZMESSAGEDATE";
            const std::string fromMe = firstColumnValue(st, cols, {"zisfromme", "zfromme"}, 32);
            std::string direction = "unknown_direction";
            if (fromMe == "1") direction = "outgoing";
            else if (fromMe == "0") direction = "incoming";
            contact = firstColumnValue(st, cols, {"zfromjid", "ztojid", "zparticipantjid", "zsenderjid", "zcontactjid", "zgroupmember"}, 512);
            itemIdentifier = firstColumnValue(st, cols, {"zstanzaid", "zuniqueid", "zguid", "zuuid"}, 512);
            textSnippet = firstColumnValue(st, cols, {"ztext", "zmessage", "zcaption", "ztitle"}, 2000);
            title = "WhatsApp message " + direction;
            const std::string msgType = firstColumnValue(st, cols, {"zmessagetype", "zmediatype", "ztype"}, 64);
            const std::string msgStatus = firstColumnValue(st, cols, {"zmessagestatus", "zstatus"}, 64);
            if (!msgType.empty()) title += " type=" + msgType;
            if (!msgStatus.empty()) provenance += " status=" + msgStatus;
            provenance += " direction=" + direction;
        } else if (t == "zwamediaitem" || t.find("media") != std::string::npos || t == "zwavcardmention") {
            recCat = "MESSAGE_ATTACHMENTS";
            dateRaw = firstColumnValue(st, cols, {"zcreationdate", "zmediadate", "zdate", "zmediarefreshtimestamp"}, 128);
            dateSource = table + ".media_date";
            contact = firstColumnValue(st, cols, {"zownerjid", "zsenderjid", "zcontactjid", "zmemberjid"}, 512);
            url = firstColumnValue(st, cols, {"zmediaurl", "zurl", "zthumbnailurl"}, 1000);
            title = firstColumnValue(st, cols, {"ztitle", "zvcardsstring", "zcaption", "zfilename"}, 1000);
            filePath = firstColumnValue(st, cols, {"zlocalpath", "zfileurl", "zfilename", "zpath", "zmedialocalpath"}, 1500);
            itemIdentifier = firstColumnValue(st, cols, {"zmediaid", "zstanzaid", "zguid", "zuuid", "zsha256"}, 512);
            textSnippet = "media_type=" + firstColumnValue(st, cols, {"zmediatype", "zmimetype", "zmime_type", "zuti"}, 256) +
                          " media_size=" + firstColumnValue(st, cols, {"zfilesize", "zsize", "zbytes"}, 64);
            if (title.empty()) title = "WhatsApp media/attachment";
        } else if (t == "zwachatsession" || t.find("chat") != std::string::npos || t.find("session") != std::string::npos) {
            recCat = "CHAT_RECORDS";
            dateRaw = firstColumnValue(st, cols, {"zlastmessagedate", "zdate", "zcreationdate"}, 128);
            dateSource = table + ".chat_date";
            contact = firstColumnValue(st, cols, {"zcontactjid", "zpartnername", "zgroupinfo", "zjid"}, 512);
            title = firstColumnValue(st, cols, {"zpartnername", "zgroupname", "zdisplayname", "ztitle"}, 1000);
            itemIdentifier = firstColumnValue(st, cols, {"zcontactjid", "zgroupinfo", "zguid", "zuuid"}, 512);
            textSnippet = "archived=" + firstColumnValue(st, cols, {"zarchived"}, 32) +
                          " unread=" + firstColumnValue(st, cols, {"zunreadcount", "zunread"}, 32);
            if (title.empty()) title = "WhatsApp chat/session";
        } else if (t == "zwaaddressbookcontact" || t == "zwaprofilepushname" || t == "zwagroupmember" || t.find("contact") != std::string::npos || t.find("member") != std::string::npos) {
            recCat = "MESSAGE_PARTICIPANTS";
            dateRaw = firstColumnValue(st, cols, {"zdate", "zlastupdated", "zmodificationdate"}, 128);
            dateSource = table + ".contact_date";
            contact = firstColumnValue(st, cols, {"zwhatsappid", "zjid", "zmemberjid", "zcontactjid", "zphone", "zfullphone"}, 512);
            title = firstColumnValue(st, cols, {"zfullname", "zpushname", "zfirstname", "zlastname", "znickname", "zdisplayname"}, 1000);
            itemIdentifier = contact;
            textSnippet = "about=" + firstColumnValue(st, cols, {"zabouttext", "zstatus", "znotes"}, 1000);
            if (title.empty()) title = "WhatsApp contact/member";
        } else if (t.find("call") != std::string::npos) {
            recCat = "CALL_RECORDS";
            dateRaw = firstColumnValue(st, cols, {"zdate", "zstartdate", "zcalltime", "ztimestamp"}, 128);
            dateSource = table + ".call_date";
            contact = firstColumnValue(st, cols, {"zfromjid", "ztojid", "zpeerjid", "zcontactjid", "zparticipantjid"}, 512);
            itemIdentifier = firstColumnValue(st, cols, {"zcallidstring", "zcallid", "zguid", "zuuid"}, 512);
            title = "WhatsApp call";
            textSnippet = "duration=" + firstColumnValue(st, cols, {"zduration", "zcallduration"}, 64) +
                          " type=" + firstColumnValue(st, cols, {"zcalltype", "ztype"}, 64) +
                          " result=" + firstColumnValue(st, cols, {"zresult", "zoutcome"}, 64);
        } else {
            sqlite3_finalize(st);
            return 0;
        }
        const auto ts = normalizeIosAppTimestamp(dateRaw, dateSource);
        bindIosParsedRecord(parsedIns, sourceId, inv, table, recCat, sourcePk,
                            ts.first, ts.second, contact, url, title, filePath,
                            itemIdentifier, textSnippet, parseStatus, provenance);
        ++rows;
    }
    sqlite3_finalize(st);
    return rows;
}

std::size_t parseKnowledgeCIosZObjectRows(const std::string& sourceId,
                                          const IosAppDbInventory& inv,
                                          sqlite3* ext,
                                          const std::string& table,
                                          SqlStatement& parsedIns) {
    const std::string lowerTable = toLower(table);
    if (lowerTable != "zobject") return 0;
    constexpr int MaxRowsPerTable = 100000;

    const bool hasStructuredMetadata = sqliteTableExistsLocal(ext, "ZSTRUCTUREDMETADATA") || sqliteTableExistsLocal(ext, "zstructuredmetadata");
    const bool hasJoinColumn = sqliteColumnExistsLocal(ext, table, "ZSTRUCTUREDMETADATA") || sqliteColumnExistsLocal(ext, table, "zstructuredmetadata");

    std::string sql;
    if (hasStructuredMetadata && hasJoinColumn) {
        const std::string metaTable = sqliteTableExistsLocal(ext, "ZSTRUCTUREDMETADATA") ? "ZSTRUCTUREDMETADATA" : "zstructuredmetadata";
        sql = "SELECT o.rowid AS __rowid__, "
              + sqliteOptionalColumnExpr(ext, table, "o", "ZSTREAMNAME", "zstreamname") + ", "
              + sqliteOptionalColumnExpr(ext, table, "o", "ZVALUESTRING", "zvaluestring") + ", "
              + sqliteOptionalColumnExpr(ext, table, "o", "ZSTARTDATE", "zstartdate") + ", "
              + sqliteOptionalColumnExpr(ext, table, "o", "ZENDDATE", "zenddate") + ", "
              + sqliteOptionalColumnExpr(ext, table, "o", "ZCREATIONDATE", "zcreationdate") + ", "
              + sqliteOptionalColumnExpr(ext, metaTable, "m", "Z_DKINTENTMETADATAKEY__INTENTCLASS", "metadata_intent_class") + ", "
              + sqliteOptionalColumnExpr(ext, metaTable, "m", "Z_DKINTENTMETADATAKEY__INTENTVERB", "metadata_intent_verb") + ", "
              + sqliteOptionalColumnExpr(ext, metaTable, "m", "Z_DKDOCUMENTMETADATAKEY__TITLE", "metadata_document_title") + ", "
              + sqliteOptionalColumnExpr(ext, metaTable, "m", "Z_DKINTENTMETADATAKEY__SERIALIZEDINTERACTION", "metadata_serialized_interaction")
              + " FROM " + sqlIdentLocal(table) + " o LEFT JOIN " + sqlIdentLocal(metaTable) + " m ON o.ZSTRUCTUREDMETADATA=m.Z_PK "
              + "WHERE COALESCE(o.ZSTREAMNAME,'') IN ('/app/inFocus','/document/open','/app/intents','/display/isBacklit','/app/activity','/item/interactions') LIMIT ?";
    } else {
        sql = "SELECT rowid AS __rowid__, * FROM " + sqlIdentLocal(table) + " WHERE COALESCE(ZSTREAMNAME,zstreamname,'') IN ('/app/inFocus','/document/open','/app/intents','/display/isBacklit','/app/activity','/item/interactions') LIMIT ?";
    }

    sqlite3_stmt* st = nullptr;
    int rc = sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr);
    if (rc != SQLITE_OK || !st) {
        sql = "SELECT rowid AS __rowid__, * FROM " + sqlIdentLocal(table) + " LIMIT ?";
        rc = sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr);
    }
    if (rc != SQLITE_OK || !st) return 0;
    sqlite3_bind_int(st, 1, MaxRowsPerTable);
    const int colCount = sqlite3_column_count(st);
    std::map<std::string, int> cols;
    for (int i = 0; i < colCount; ++i) {
        const char* name = sqlite3_column_name(st, i);
        if (name) cols[toLower(name)] = i;
    }
    const int rowidCol = findColumnIndex(cols, {"__rowid__", "z_pk", "zuuid", "uuid", "identifier"}, true);
    const int streamNameCol = findColumnIndex(cols, {"zstreamname"}, true);
    const int valueStringCol = findColumnIndex(cols, {"zvaluestring"}, true);
    const int startDateCol = findColumnIndex(cols, {"zstartdate"}, true);
    const int endDateCol = findColumnIndex(cols, {"zenddate"}, true);
    const int creationDateCol = findColumnIndex(cols, {"zcreationdate"}, true);
    const int intentClassCol = findColumnIndex(cols, {"metadata_intent_class", "z_dkintentmetadatakey__intentclass"}, true);
    const int intentVerbCol = findColumnIndex(cols, {"metadata_intent_verb", "z_dkintentmetadatakey__intentverb"}, true);
    const int docTitleCol = findColumnIndex(cols, {"metadata_document_title", "z_dkdocumentmetadatakey__title"}, true);
    const int serializedInteractionCol = findColumnIndex(cols, {"metadata_serialized_interaction", "z_dkintentmetadatakey__serializedinteraction"}, true);
    std::size_t rows = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const std::string stream = columnValue(st, streamNameCol, 256);
        if (stream != "/app/inFocus" && stream != "/document/open" && stream != "/app/intents" && stream != "/display/isBacklit" && stream != "/app/activity" && stream != "/item/interactions") continue;
        const std::string bundleId = columnValue(st, valueStringCol, 512);
        const auto startTs = normalizeIosAppTimestamp(columnValue(st, startDateCol, 128), "ZSTARTDATE");
        const auto creationTs = normalizeIosAppTimestamp(columnValue(st, creationDateCol, 128), "ZCREATIONDATE");
        const std::string intentClass = columnValue(st, intentClassCol, 512);
        const std::string intentVerb = columnValue(st, intentVerbCol, 256);
        const std::string docTitle = columnValue(st, docTitleCol, 1000);
        const std::string serialized = columnValue(st, serializedInteractionCol, 1000);
        std::string title = docTitle.empty() ? ("User Interaction: " + stream) : ("Opened Document: " + docTitle);
        std::string snippet = iosAppDbBuildKnowledgeCTextSnippet(stream, bundleId, columnValue(st, creationDateCol, 128), columnValue(st, endDateCol, 128));
        if (!intentClass.empty()) snippet += (snippet.empty() ? "" : "; ") + std::string("intent_class=") + intentClass;
        if (!intentVerb.empty()) snippet += (snippet.empty() ? "" : "; ") + std::string("intent_verb=") + intentVerb;
        if (!docTitle.empty()) snippet += (snippet.empty() ? "" : "; ") + std::string("document_title=") + docTitle;
        if (!serialized.empty()) snippet += (snippet.empty() ? "" : "; ") + std::string("serialized_interaction_preview=") + serialized;
        std::string contactHint;
        const std::string serializedLower = lowerCopy(serialized);
        if (serializedLower.find("personhandle") != std::string::npos || serializedLower.find("emailaddress") != std::string::npos || serializedLower.find("name") != std::string::npos) {
            contactHint = "KnowledgeC target hint: " + firstMetadataWindow(serialized, {"personHandle", "emailAddress", "name"}, 128);
        }
        std::string parseStatus = "parsed_knowledgec_zobject_interaction_joined_metadata";
        std::string provenance = hasStructuredMetadata ? "read_only_sqlite_coreduet_knowledgec joined_zstructuredmetadata_v1_3_2" : "read_only_sqlite_coreduet_knowledgec table=" + table;
        auto isCommunicationIntentClass = [](const std::string& cls) {
            return cls == "INSendMessageIntent" || cls == "INStartCallIntent" ||
                   cls == "INStartAudioCallIntent" || cls == "INStartVideoCallIntent" ||
                   cls == "INSendPaymentIntent";
        };
        const bool communicationIntent = (stream == "/app/intents" && isCommunicationIntentClass(intentClass)) ||
                                         (stream == "/item/interactions" && bundleId == "com.apple.sharingd");
        const bool deviceOrStateStream = (stream == "/display/isBacklit" || stream == "/app/inFocus" || stream == "/document/open");
        std::string knowledgeRecordCategory = communicationIntent ? "KNOWLEDGEC_COMMUNICATION_INTENT" :
                                               (deviceOrStateStream ? "KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY" : "KNOWLEDGEC_EVENTS");
        std::string contactForRecord = communicationIntent ? contactHint : "";
        std::string itemIdentifierForRecord = communicationIntent ? stream : "";
        if (communicationIntent) {
            title = "COMMUNICATION INTENT: Document Shared/Sent";
            parseStatus = "parsed_knowledgec_communication_intent";
            provenance += "; COMMUNICATION_INTENT_STREAM=True";
            if (serialized.find("personHandle") != std::string::npos || serialized.find("emailAddress") != std::string::npos || serialized.find("name") != std::string::npos) {
                provenance += "; INTENT_TARGET_IDENTIFIED=True";
            }
        } else if (deviceOrStateStream) {
            provenance += "; KNOWLEDGEC_DEVICE_STATE_STREAM=True; IDENTITY_PROMOTION_SUPPRESSED=True";
        }
        if (snippet.size() > 2000) snippet.resize(2000);
        bindIosParsedRecord(parsedIns, sourceId, inv, table, knowledgeRecordCategory,
                            columnValue(st, rowidCol, 256),
                            !startTs.first.empty() ? startTs.first : creationTs.first,
                            !startTs.second.empty() ? startTs.second : creationTs.second,
                            contactForRecord,
                            "",
                            title,
                            "",
                            itemIdentifierForRecord,
                            snippet,
                            parseStatus,
                            provenance);
        ++rows;
    }
    sqlite3_finalize(st);
    return rows;
}

std::size_t parseIosAppDbTableRows(const std::string& sourceId,
                                   const IosAppDbInventory& inv,
                                   sqlite3* ext,
                                   const std::string& table,
                                   const std::string& recordCategory,
                                   SqlStatement& parsedIns) {
    if (!iosAppDbIsTargetRecordCategory(recordCategory)) return 0;
    constexpr int MaxRowsPerPage = 50000;
    std::string sql = "SELECT rowid AS __rowid__, * FROM " + sqlIdentLocal(table) + " LIMIT ? OFFSET ?";
    sqlite3_stmt* st = nullptr;
    int rc = sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr);
    if (rc != SQLITE_OK) {
        sql = "SELECT * FROM " + sqlIdentLocal(table) + " LIMIT ? OFFSET ?";
        rc = sqlite3_prepare_v2(ext, sql.c_str(), -1, &st, nullptr);
    }
    if (rc != SQLITE_OK || !st) return 0;
    const int colCount = sqlite3_column_count(st);
    std::map<std::string, int> cols;
    for (int i = 0; i < colCount; ++i) {
        const char* name = sqlite3_column_name(st, i);
        if (name) cols[toLower(name)] = i;
    }
    const int pkCol = findColumnIndex(cols, {"__rowid__", "ROWID", "id", "guid", "Z_PK", "ZUUID", "uuid", "identifier"}, true);
    const int dateCol = findColumnIndex(cols, {"date", "timestamp", "time", "created_date", "creation_date", "start_date", "end_date", "message_date", "visit_time", "last_visit_time", "zdate", "zstartdate", "zenddate", "zcreationdate", "zmodificationdate"}, true);
    const int contactCol = findColumnIndex(cols, {"handle_id", "handle", "address", "phone", "email", "sender", "recipient", "destination", "caller", "callee", "zaddress", "zdisplayname"}, true);
    const int urlCol = findColumnIndex(cols, {"url", "url_string", "request_url", "redirect_source", "zurl", "webpageurl"}, true);
    const int titleCol = findColumnIndex(cols, {"title", "subject", "summary", "display_name", "name", "ztitle", "zsummary"}, true);
    const int pathCol = findColumnIndex(cols, {"path", "filename", "file_name", "transfer_name", "uti", "mime_type", "zfilename", "zpath", "zlocalpath"}, true);
    const int itemCol = findColumnIndex(cols, {"guid", "uuid", "identifier", "message_id", "chat_id", "handle_id", "persistent_id", "zidentifier"}, true);
    const int textCol = findColumnIndex(cols, {"text", "body", "message", "snippet", "preview", "comment", "notes", "ztext", "zbody", "zcontent", "zsummary", "data", "payload"}, true);
    std::size_t rows = 0;
    sqlite3_int64 offset = 0;
    for (;;) {
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
        sqlite3_bind_int(st, 1, MaxRowsPerPage);
        sqlite3_bind_int64(st, 2, offset);
        std::size_t pageRows = 0;
        while (sqlite3_step(st) == SQLITE_ROW) {
        const std::string dateRaw = columnValue(st, dateCol, 128);
        const char* dateNamePtr = dateCol >= 0 ? sqlite3_column_name(st, dateCol) : "";
        const auto ts = normalizeIosAppTimestamp(dateRaw, dateNamePtr ? std::string(dateNamePtr) : std::string());
        if (recordCategory == "COREDUET_INTERACTIONS") {
            const int participantCol = findColumnIndex(cols, {"zsender", "zrecipient", "zrecipients", "zcontact", "zperson", "zdisplayname", "zname", "zaccount", "zhandle", "zaddress"}, true);
            const int bundleCol = findColumnIndex(cols, {"zbundleid", "zbundleidentifier", "zapplication", "zapp", "zdomainidentifier", "zdirection"}, true);
            const int typeCol = findColumnIndex(cols, {"zinteractiontype", "ztype", "zmechanism", "zdirection", "zverb", "zkind", "zcontext"}, true);
            const int urlLikeCol = findColumnIndex(cols, {"zcontenturl", "zurl", "url", "zwebpageurl"}, true);
            const int identifierCol = findColumnIndex(cols, {"zidentifier", "zuniqueidentifier", "zuuid", "zguid", "zcontentidentifier", "zexternalidentifier", "z_pk"}, true);
            const int titleLikeCol = findColumnIndex(cols, {"ztitle", "zdisplayname", "zsummary", "zcontenttext", "zcontentdescription", "zdomainidentifier"}, true);
            const std::string participant = columnValue(st, participantCol, 512);
            const std::string bundleHint = columnValue(st, bundleCol, 512);
            const std::string typeHint = columnValue(st, typeCol, 256);
            const std::string urlHint = columnValue(st, urlLikeCol, 1000);
            const std::string identifier = columnValue(st, identifierCol, 512);
            std::string title = columnValue(st, titleLikeCol, 1000);
            if (title.empty()) title = typeHint.empty() ? "CoreDuet interactionC row" : "CoreDuet interactionC row: " + typeHint;
            std::string snippet = summarizeSqliteRowValues(st, {"ZSTARTDATE", "ZENDDATE", "ZCREATIONDATE", "ZBUNDLEID", "ZBUNDLEIDENTIFIER", "ZDOMAINIDENTIFIER", "ZINTERACTIONTYPE", "ZTYPE", "ZDIRECTION", "ZSENDER", "ZRECIPIENT", "ZRECIPIENTS", "ZCONTACT", "ZPERSON", "ZCONTENTURL", "ZTITLE", "ZDISPLAYNAME"}, 2000);
            if (!bundleHint.empty()) snippet = (snippet.empty() ? std::string() : snippet + "; ") + "app_or_domain_hint=" + bundleHint;
            std::string contactHint = participant;
            std::string provenance = "read_only_sqlite_dynamic_schema table=" + table + "; COREDUET_INTERACTIONC_GENERIC_ROW=True; no_direct_communication_conclusion=True; schema_tolerant_v1_6_24";
            if (snippet.empty()) provenance += "; row_had_no_textual_previewable_columns=True";
            bindIosParsedRecord(parsedIns, sourceId, inv, table, recordCategory,
                                columnValue(st, pkCol, 256), ts.first, ts.second,
                                contactHint, urlHint, title, "", identifier,
                                snippet,
                                "parsed_coreduet_interactionc_generic_row",
                                provenance);
            ++rows;
            ++pageRows;
            continue;
        }
        if (recordCategory == "KNOWLEDGEC_EVENTS") {
            const int streamNameCol = findColumnIndex(cols, {"zstreamname"}, true);
            const int valueStringCol = findColumnIndex(cols, {"zvaluestring"}, true);
            const std::string stream = columnValue(st, streamNameCol, 256);
            if (stream != "/app/inFocus" && stream != "/document/open" && stream != "/app/intents" && stream != "/display/isBacklit" && stream != "/app/activity" && stream != "/item/interactions") continue;
            const std::string bundleId = columnValue(st, valueStringCol, 512);
            const bool deviceOrStateStream = (stream == "/display/isBacklit" || stream == "/app/inFocus" || stream == "/document/open");
            const std::string fallbackKnowledgeCategory = deviceOrStateStream ? "KNOWLEDGEC_DEVICE_OR_APP_ACTIVITY" : recordCategory;
            std::string fallbackProvenance = "read_only_sqlite_dynamic_schema table=" + table + "; specialized_knowledgec_mapping=v0_9_48";
            if (deviceOrStateStream) fallbackProvenance += "; KNOWLEDGEC_DEVICE_STATE_STREAM=True; IDENTITY_PROMOTION_SUPPRESSED=True";
            bindIosParsedRecord(parsedIns, sourceId, inv, table, fallbackKnowledgeCategory,
                                columnValue(st, pkCol, 256), ts.first, ts.second, "", "",
                                "User Interaction: " + stream, "", stream,
                                iosAppDbBuildKnowledgeCTextSnippet(stream, bundleId, dateRaw, columnValue(st, findColumnIndex(cols, {"zenddate"}, true), 128)),
                                "parsed_knowledgec_generic_row",
                                fallbackProvenance);
            ++rows;
            ++pageRows;
            continue;
        }
        if (recordCategory == "MESSAGE_DELETED_OR_RECOVERABLE") {
            const int guidCol = findColumnIndex(cols, {"guid", "message_guid", "chat_id", "message_id", "zmessage", "zmessageguid"}, true);
            const int deleteDateCol = findColumnIndex(cols, {"delete_date", "deleted_date", "date", "zdate", "zdeleteddate", "zcreatedate"}, true);
            const auto delTs = normalizeIosAppTimestamp(columnValue(st, deleteDateCol, 128), "deleted_or_recoverable_date");
            bindIosParsedRecord(parsedIns, sourceId, inv, table, recordCategory,
                                columnValue(st, pkCol, 256), delTs.first, delTs.second, "", "",
                                "Apple Messages deleted/recoverable record", "", columnValue(st, guidCol, 512),
                                "Deleted/recoverable Apple Messages table row; table=" + table,
                                "parsed_apple_deleted_or_recoverable_record",
                                "read_only_sqlite_dynamic_schema table=" + table + "; deleted_or_recoverable_table_v0_9_48");
            ++rows;
            ++pageRows;
            continue;
        }
        std::string genericTitle = columnValue(st, titleCol, 1000);
        std::string genericFilePath = columnValue(st, pathCol, 1000);
        std::string genericText = columnValue(st, textCol, 50000);
        if (genericText.size() >= 6 && genericText.rfind("bplist", 0) == 0) {
            genericText = unflattenNSKeyedArchiverOrBplist(genericText);
        }
        if (genericText.size() > 2000) genericText.resize(2000);
        std::string genericProvenance = "read_only_sqlite_dynamic_schema table=" + table;
        if (recordCategory == "CONTACT_RECORDS") {
            const int isMeCol = findColumnIndex(cols, {"zismecontact", "is_me", "isme"}, true);
            if (isMeCol >= 0 && columnValue(st, isMeCol, 16) == "1") {
                genericProvenance += "; primary_identity_attribution=device_owner_me_contact";
                if (genericTitle.empty()) genericTitle = "Device owner identity contact";
                else genericTitle = "Device owner identity contact: " + genericTitle;
            }
        }
        if (genericFilePath.find("/.Trash/") != std::string::npos || genericFilePath.find("/.Trashes/") != std::string::npos) {
            genericProvenance += "; trash_path_activity=file_path_contains_trash_component";
        }
        if (genericText.find("LSQuarantine") != std::string::npos || genericFilePath.find("LSQuarantine") != std::string::npos) {
            genericProvenance += "; quarantine_metadata_reference=LSQuarantine_string_detected";
        }
        if (genericText.find("kMDItemDomainIdentifier") != std::string::npos) genericProvenance += "; THREAD_VOLUME_TRACKING_ENABLED=True";
        if (genericText.find("kMDItemAuthor") != std::string::npos || genericText.find("kMDItemRecipient") != std::string::npos) genericProvenance += "; IDENTITY_BOUND_COMMUNICATION=True";
        if (!identityRecoveryHintFromText(genericTitle + " " + genericText + " " + genericFilePath).empty()) genericProvenance += "; IDENTITY_PATTERN_AVAILABLE=True";
        if (recordCategory == "WEB_DOWNLOADS") {
            genericProvenance += "; web_download_table_record=True";
            if (genericTitle.empty()) genericTitle = "Browser download record";
        }
        if (recordCategory == "NOTES_RECORDS" && genericTitle.empty()) genericTitle = "Notes record";
        if (recordCategory == "LOCATION_RECORDS" && genericTitle.empty()) genericTitle = "Location/Maps record";
        bindIosParsedRecord(parsedIns, sourceId, inv, table, recordCategory,
                            columnValue(st, pkCol, 256), ts.first, ts.second,
                            columnValue(st, contactCol, 512),
                            columnValue(st, urlCol, 1000),
                            genericTitle,
                            genericFilePath,
                            columnValue(st, itemCol, 512),
                            genericText,
                            "parsed_generic_app_db_row",
                            genericProvenance);
        ++rows;
            ++pageRows;
        }
        if (pageRows < static_cast<std::size_t>(MaxRowsPerPage)) break;
        offset += static_cast<sqlite3_int64>(pageRows);
    }
    sqlite3_finalize(st);
    return rows;
}


} // namespace

std::size_t iosAppDbParseTable(const std::string& sourceId,
                               const IosAppDbInventory& inv,
                               sqlite3* ext,
                               const std::string& table,
                               const IosAppDbTableParseDecision& parseDecision,
                               SqlStatement& parsedIns) {
    const std::string& recCat = parseDecision.recordCategory;
    if (!iosAppDbIsTargetRecordCategory(recCat)) return 0;

    if (parseDecision.useWhatsAppSpecialParser) {
        const std::size_t specialRows = parseWhatsAppIosTableRows(sourceId, inv, ext, table, recCat, parsedIns);
        if (specialRows > 0) return specialRows;
        return parseIosAppDbTableRows(sourceId, inv, ext, table, recCat, parsedIns);
    }

    if (parseDecision.useAppleMessagesSpecialParser) {
        std::size_t specialRows = 0;
        const std::string lowerTable = toLower(table);
        if (lowerTable == "message") {
            specialRows = parseAppleMessagesSmsDbMessageRows(sourceId, inv, ext, parsedIns);
        } else if (lowerTable == "attachment") {
            specialRows = parseAppleMessagesSmsDbAttachmentRows(sourceId, inv, ext, parsedIns);
        } else if (lowerTable == "handle" || lowerTable == "chat") {
            specialRows = parseAppleMessagesSmsDbParticipantRows(sourceId, inv, ext, table, parsedIns);
        }
        if (specialRows > 0) return specialRows;
        return parseIosAppDbTableRows(sourceId, inv, ext, table, recCat, parsedIns);
    }

    if (parseDecision.useKnowledgeCSpecialParser) {
        const std::size_t specialRows = parseKnowledgeCIosZObjectRows(sourceId, inv, ext, table, parsedIns);
        if (specialRows > 0) return specialRows;
        return parseIosAppDbTableRows(sourceId, inv, ext, table, recCat, parsedIns);
    }

    return parseIosAppDbTableRows(sourceId, inv, ext, table, recCat, parsedIns);
}


std::string IosAppDbParser::recordCategory(const std::string& databaseCategory,
                                           const std::string& tableName,
                                           const std::string& columnsCsv) {
    return iosAppDbRecordCategory(databaseCategory, tableName, columnsCsv);
}

bool IosAppDbParser::isTargetRecordCategory(const std::string& category) {
    return iosAppDbIsTargetRecordCategory(category);
}

IosAppDbTableParseDecision IosAppDbParser::buildTableParseDecision(const std::string& databaseCategory,
                                                                   const std::string& tableName,
                                                                   const std::string& columnsCsv) {
    return iosAppDbBuildTableParseDecision(databaseCategory, tableName, columnsCsv);
}

std::size_t IosAppDbParser::parseTable(const std::string& sourceId,
                                       const IosAppDbInventory& inv,
                                       sqlite3* ext,
                                       const std::string& table,
                                       const IosAppDbTableParseDecision& parseDecision,
                                       SqlStatement& parsedIns) {
    return iosAppDbParseTable(sourceId, inv, ext, table, parseDecision, parsedIns);
}


void IosAppDbParser::parseRecordInventories(CaseDatabase& db,
                                            const std::filesystem::path& caseDir,
                                            const std::string& sourceId,
                                            Logger& log,
                                            const IosAppDbStatusWriter& statusWriter) {
    auto writeStatus = [&](const std::string& stage, const std::string& message) {
        if (statusWriter) statusWriter(caseDir, stage, message);
    };

    writeStatus("ios_app_db_record_inventory_start", "enumerate extracted iOS app database candidates");

    std::vector<IosAppDbInventory> invs;
    try {
        auto q = db.prepare("SELECT ios_db_id,normalized_path,database_name,database_category,app_hint,extracted_path FROM ios_app_database_inventory WHERE source_id=? AND COALESCE(extracted_path,'')<>'' ORDER BY ios_db_id");
        q.bind(1, sourceId);
        while (q.stepRow()) {
            IosAppDbInventory inv;
            inv.id = q.colInt64(0);
            inv.norm = q.colText(1);
            inv.name = q.colText(2);
            inv.cat = q.colText(3);
            inv.app = q.colText(4);
            inv.extracted = std::filesystem::path(q.colText(5));
            invs.push_back(inv);
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to enumerate extracted iOS app databases for record inventory: ") + ex.what());
        writeStatus("ios_app_db_record_inventory_warning", ex.what());
        return;
    }

    writeStatus("ios_app_db_record_inventory_candidates", "extracted_databases=" + std::to_string(invs.size()));

    std::size_t tableRows = 0;
    std::size_t opened = 0;
    std::size_t parsedRows = 0;
    try {
        db.begin();
        writeStatus("ios_app_db_record_inventory_db_transaction", "started metadata import and bounded row parsing transaction");
        {
            auto del = db.prepare("DELETE FROM ios_app_database_record_inventory WHERE source_id=?");
            del.bind(1, sourceId);
            del.stepDone();
        }
        {
            auto del = db.prepare("DELETE FROM ios_app_parsed_records WHERE source_id=?");
            del.bind(1, sourceId);
            del.stepDone();
        }
        auto ins = db.prepare("INSERT INTO ios_app_database_record_inventory(source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,table_name,row_count,sample_columns,record_category,parse_status,notes,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
        auto parsedIns = db.prepare("INSERT INTO ios_app_parsed_records(source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        auto upd = db.prepare("UPDATE ios_app_database_inventory SET parse_status=?, record_inventory_status=? WHERE ios_db_id=?");

        std::size_t dbIndex = 0;
        for (const auto& inv : invs) {
            ++dbIndex;
            if ((dbIndex % 25) == 1 || dbIndex == invs.size()) {
                writeStatus("ios_app_db_record_inventory_progress", "database=" + std::to_string(dbIndex) + "/" + std::to_string(invs.size()) + " opened=" + std::to_string(opened) + " table_rows=" + std::to_string(tableRows) + " parsed_app_records=" + std::to_string(parsedRows));
            }
            std::string parseStatus = "not_opened";
            std::string recordStatus = "no_tables_counted";
            if (!std::filesystem::is_regular_file(inv.extracted)) {
                parseStatus = "extracted_path_missing";
                recordStatus = "database_file_not_found_after_extract";
                upd.bind(1, parseStatus); upd.bind(2, recordStatus); upd.bind(3, inv.id); upd.stepDone(); upd.reset();
                continue;
            }
            sqlite3* ext = nullptr;
            int rc = sqlite3_open_v2(pathString(inv.extracted).c_str(), &ext, SQLITE_OPEN_READONLY, nullptr);
            if (rc != SQLITE_OK || !ext) {
                parseStatus = "sqlite_open_failed";
                recordStatus = ext ? sqlite3_errmsg(ext) : "sqlite handle not created";
                if (ext) sqlite3_close(ext);
                upd.bind(1, parseStatus); upd.bind(2, recordStatus); upd.bind(3, inv.id); upd.stepDone(); upd.reset();
                continue;
            }
            ++opened;
            sqlite3_stmt* tables = nullptr;
            rc = sqlite3_prepare_v2(ext, "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name", -1, &tables, nullptr);
            if (rc != SQLITE_OK) {
                parseStatus = "sqlite_master_query_failed";
                recordStatus = sqlite3_errmsg(ext);
                sqlite3_close(ext);
                upd.bind(1, parseStatus); upd.bind(2, recordStatus); upd.bind(3, inv.id); upd.stepDone(); upd.reset();
                continue;
            }
            int tableCount = 0;
            while (sqlite3_step(tables) == SQLITE_ROW && tableCount < 250) {
                const unsigned char* nameTxt = sqlite3_column_text(tables, 0);
                if (!nameTxt) continue;
                const std::string table = reinterpret_cast<const char*>(nameTxt);
                const std::string cols = sqliteColumnListLocal(ext, table);
                const long long rowCount = sqliteScalarCountLocal(ext, table);
                const IosAppDbTableParseDecision parseDecision = IosAppDbParser::buildTableParseDecision(inv.cat, table, cols);
                const std::string recCat = parseDecision.recordCategory;
                ins.bind(1, sourceId);
                ins.bind(2, inv.id);
                ins.bind(3, inv.norm);
                ins.bind(4, inv.name);
                ins.bind(5, inv.cat);
                ins.bind(6, inv.app);
                ins.bind(7, table);
                ins.bind(8, rowCount);
                ins.bind(9, cols);
                ins.bind(10, recCat);
                ins.bind(11, rowCount >= 0 ? "table_counted" : "table_count_failed");
                ins.bind(12, IosAppDbParser::isTargetRecordCategory(recCat) ? "schema/table-counted and parser-module row extraction attempted" : "schema/table-count inventory only; table category not selected for row extraction");
                ins.bind(13, nowUtc());
                ins.stepDone();
                ins.reset();
                if (rowCount > 0 && IosAppDbParser::isTargetRecordCategory(recCat)) {
                    try {
                        parsedRows += IosAppDbParser::parseTable(sourceId, inv, ext, table, parseDecision, parsedIns);
                    } catch (const std::exception& ex) {
                        writeStatus("ios_app_db_table_parse_warning", "ios_db_id=" + std::to_string(inv.id) + " table=" + table + " error=" + ex.what());
                    } catch (...) {
                        writeStatus("ios_app_db_table_parse_warning", "ios_db_id=" + std::to_string(inv.id) + " table=" + table + " error=unknown");
                    }
                }
                ++tableRows;
                ++tableCount;
            }
            sqlite3_finalize(tables);
            sqlite3_close(ext);
            parseStatus = "sqlite_opened_record_inventory_counts";
            recordStatus = "tables_counted=" + std::to_string(tableCount);
            upd.bind(1, parseStatus); upd.bind(2, recordStatus); upd.bind(3, inv.id); upd.stepDone(); upd.reset();
        }
        writeStatus("ios_app_db_timeline_promotion_start", "promote dated parsed app records into timeline/usage evidence");
        const std::string sourceSql = sqlLiteralLocal(sourceId);
        db.exec("DELETE FROM timeline_events WHERE source_id=" + sourceSql + " AND event_source_field LIKE 'ios_app_parsed_records.%'");
        db.exec("DELETE FROM usage_evidence WHERE source_id=" + sourceSql + " AND field_name LIKE 'ios_app_parsed_records.%'");
        db.exec("INSERT INTO timeline_events(artifact_id,source_id,store_guid,inode_num,event_timestamp_utc,event_type,event_source_field,file_name,path,existence_status,deleted_or_orphaned_candidate) "
                "SELECT NULL,source_id,'ios_app_db',COALESCE(NULLIF(source_primary_key,''),CAST(ios_app_record_id AS TEXT)),record_timestamp_utc, "
                "CASE WHEN record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','MESSAGE_DELETED_OR_RECOVERABLE') THEN 'IOS_COMMUNICATION_RECORD' "
                "WHEN record_category='KNOWLEDGEC_EVENTS' THEN 'IOS_KNOWLEDGEC_EVENT' "
                "WHEN record_category IN ('CALL_RECORDS','WEB_HISTORY','WEB_VISITS','WEB_DOWNLOADS','CALENDAR_RECORDS','NOTES_RECORDS','LOCATION_RECORDS') THEN 'IOS_APP_ACTIVITY_RECORD' "
                "ELSE 'IOS_APP_DATABASE_RECORD' END, "
                "'ios_app_parsed_records.' || COALESCE(record_category,''), "
                "COALESCE(NULLIF(title,''),NULLIF(contact_or_participant,''),database_name), "
                "COALESCE(NULLIF(file_path,''),NULLIF(url,''),database_normalized_path), "
                "'APP_DB_RECORD_PRESENT', "
                "CASE WHEN record_category='MESSAGE_DELETED_OR_RECOVERABLE' OR provenance LIKE '%DELETED%' OR provenance LIKE '%EXPIRED%' THEN 1 ELSE 0 END "
                "FROM ios_app_parsed_records WHERE source_id=" + sourceSql + " AND COALESCE(record_timestamp_utc,'')<>''");
        db.exec("INSERT INTO usage_evidence(artifact_id,source_id,store_guid,inode_num,field_name,field_value,parsed_utc) "
                "SELECT NULL,source_id,'ios_app_db',COALESCE(NULLIF(source_primary_key,''),CAST(ios_app_record_id AS TEXT)), "
                "'ios_app_parsed_records.' || COALESCE(record_category,''), "
                "substr(COALESCE(NULLIF(title,''),NULLIF(text_snippet,''),NULLIF(url,''),NULLIF(file_path,''),NULLIF(contact_or_participant,''),database_name),1,2000), "
                "record_timestamp_utc "
                "FROM ios_app_parsed_records WHERE source_id=" + sourceSql + " AND COALESCE(record_timestamp_utc,'')<>'' "
                "AND (record_category IN ('KNOWLEDGEC_EVENTS','WEB_HISTORY','WEB_VISITS','WEB_DOWNLOADS','CALL_RECORDS','MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','NOTES_RECORDS','LOCATION_RECORDS') "
                "OR provenance LIKE '%COMMUNICATION_INTENT%' OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%')");
        writeStatus("ios_app_db_timeline_promotion_complete", "timeline/usage promotion complete from parsed app records");
        db.commit();
        log.info("iOS app database record inventory: extracted_databases=" + std::to_string(invs.size()) + " opened=" + std::to_string(opened) + " table_rows=" + std::to_string(tableRows) + " parsed_app_records=" + std::to_string(parsedRows));
        writeStatus("ios_app_db_record_inventory", "extracted_databases=" + std::to_string(invs.size()) + " opened=" + std::to_string(opened) + " table_rows=" + std::to_string(tableRows) + " parsed_app_records=" + std::to_string(parsedRows));
    } catch (const std::exception& ex) {
        db.rollbackNoThrow();
        log.warn(std::string("Unable to parse iOS app database record inventory: ") + ex.what());
        writeStatus("ios_app_db_record_inventory_warning", ex.what());
    }
}


void IosAppDbParser::promoteParsedRecordsToTimelineUsage(CaseDatabase& db,
                                                         const std::filesystem::path& caseDir,
                                                         const std::string& sourceId,
                                                         Logger& log,
                                                         const IosAppDbStatusWriter& statusWriter) {
    auto writeStatus = [&](const std::string& stage, const std::string& message) {
        if (statusWriter) statusWriter(caseDir, stage, message);
    };
    try {
        writeStatus("ios_app_db_timeline_promotion_start", "promote dated parsed app records into timeline/usage evidence");
        const std::string sourceSql = sqlLiteralLocal(sourceId);
        db.exec("DELETE FROM timeline_events WHERE source_id=" + sourceSql + " AND event_source_field LIKE 'ios_app_parsed_records.%'");
        db.exec("DELETE FROM usage_evidence WHERE source_id=" + sourceSql + " AND field_name LIKE 'ios_app_parsed_records.%'");
        db.exec("INSERT INTO timeline_events(artifact_id,source_id,store_guid,inode_num,event_timestamp_utc,event_type,event_source_field,file_name,path,existence_status,deleted_or_orphaned_candidate) "
                "SELECT NULL,source_id,'ios_app_db',COALESCE(NULLIF(source_primary_key,''),CAST(ios_app_record_id AS TEXT)),record_timestamp_utc, "
                "CASE WHEN record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','MESSAGE_DELETED_OR_RECOVERABLE','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS') THEN 'IOS_COMMUNICATION_RECORD' "
                "WHEN record_category='KNOWLEDGEC_EVENTS' THEN 'IOS_KNOWLEDGEC_EVENT' "
                "WHEN record_category IN ('CALL_RECORDS','WEB_HISTORY','WEB_VISITS','WEB_DOWNLOADS','CALENDAR_RECORDS','CONTACT_RECORDS','NOTES_RECORDS','LOCATION_RECORDS') THEN 'IOS_APP_ACTIVITY_RECORD' "
                "ELSE 'IOS_APP_DATABASE_RECORD' END, "
                "'ios_app_parsed_records.' || COALESCE(record_category,''), "
                "COALESCE(NULLIF(title,''),NULLIF(contact_or_participant,''),database_name), "
                "COALESCE(NULLIF(file_path,''),NULLIF(url,''),database_normalized_path), "
                "'APP_DB_RECORD_PRESENT', "
                "CASE WHEN record_category='MESSAGE_DELETED_OR_RECOVERABLE' OR provenance LIKE '%DELETED%' OR provenance LIKE '%EXPIRED%' OR provenance LIKE '%trash_path_activity%' THEN 1 ELSE 0 END "
                "FROM ios_app_parsed_records WHERE source_id=" + sourceSql + " AND COALESCE(record_timestamp_utc,'')<>''");
        db.exec("INSERT INTO usage_evidence(artifact_id,source_id,store_guid,inode_num,field_name,field_value,parsed_utc) "
                "SELECT NULL,source_id,'ios_app_db',COALESCE(NULLIF(source_primary_key,''),CAST(ios_app_record_id AS TEXT)), "
                "'ios_app_parsed_records.' || COALESCE(record_category,''), "
                "substr(COALESCE(NULLIF(title,''),NULLIF(text_snippet,''),NULLIF(url,''),NULLIF(file_path,''),NULLIF(contact_or_participant,''),database_name),1,2000), "
                "record_timestamp_utc "
                "FROM ios_app_parsed_records WHERE source_id=" + sourceSql + " AND COALESCE(record_timestamp_utc,'')<>'' "
                "AND (record_category IN ('KNOWLEDGEC_EVENTS','WEB_HISTORY','WEB_VISITS','WEB_DOWNLOADS','CALL_RECORDS','CALL_PARTICIPANTS','MESSAGE_RECORDS','MESSAGE_PARTICIPANTS','CHAT_RECORDS','MAIL_RECORDS','CONTACT_RECORDS','CALENDAR_RECORDS','NOTES_RECORDS','LOCATION_RECORDS') "
                "OR provenance LIKE '%COMMUNICATION_INTENT%' OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%' OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%' OR provenance LIKE '%primary_identity_attribution%' OR provenance LIKE '%quarantine_metadata_reference%' OR provenance LIKE '%trash_path_activity%')");
        long long timelineRows = 0;
        long long usageRows = 0;
        try {
            auto st = db.prepare("SELECT COUNT(*) FROM timeline_events WHERE source_id=" + sourceSql + " AND event_source_field LIKE 'ios_app_parsed_records.%'");
            if (st.stepRow()) timelineRows = st.colInt64(0);
        } catch (...) {}
        try {
            auto st = db.prepare("SELECT COUNT(*) FROM usage_evidence WHERE source_id=" + sourceSql + " AND field_name LIKE 'ios_app_parsed_records.%'");
            if (st.stepRow()) usageRows = st.colInt64(0);
        } catch (...) {}
        writeStatus("ios_app_db_timeline_promotion_complete", "timeline=" + std::to_string(timelineRows) + " usage=" + std::to_string(usageRows));
        log.info("iOS app DB timeline/usage promotion: timeline=" + std::to_string(timelineRows) + " usage=" + std::to_string(usageRows));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to promote iOS app parsed records into timeline/usage evidence: ") + ex.what());
        writeStatus("ios_app_db_timeline_promotion_warning", ex.what());
    }
}

} // namespace vestigant::spotlight
