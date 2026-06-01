#include "core/path_utils.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace vestigant::spotlight {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

bool iequals(const std::string& a, const std::string& b) { return toLower(a) == toLower(b); }

bool containsI(const std::string& haystack, const std::string& needle) {
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string nowUtc() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

std::string pathString(const fs::path& p) { return p.generic_string(); }

std::string safeRelativeString(const fs::path& root, const fs::path& p) {
    std::error_code ec;
    auto rel = fs::relative(p, root, ec);
    if (!ec) return pathString(rel);
    return pathString(p);
}

std::string normalizeSlash(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (s.find("//") != std::string::npos) {
        s.replace(s.find("//"), 2, "/");
    }
    return s;
}

std::string ltrimSlashes(std::string s) {
    s = normalizeSlash(s);
    while (!s.empty() && s.front() == '/') s.erase(s.begin());
    return s;
}

std::vector<std::string> splitLooseList(const std::string& s) {
    std::string v = s;
    for (char& c : v) {
        if (c == ';' || c == '|' || c == '\n' || c == '\r' || c == '[' || c == ']') c = ',';
    }
    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(v);
    while (std::getline(is, cur, ',')) {
        cur = trim(cur);
        if (!cur.empty()) out.push_back(cur);
    }
    return out;
}

std::string join(const std::vector<std::string>& values, const std::string& sep) {
    std::ostringstream os;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) os << sep;
        os << values[i];
    }
    return os.str();
}

bool isLikelyTruthyNumber(const std::string& s) {
    auto t = trim(s);
    if (t.empty()) return false;
    try { return std::stoll(t) > 0; } catch (...) { return false; }
}

std::string firstNonEmpty(std::initializer_list<std::string> values) {
    for (const auto& v : values) {
        if (!trim(v).empty()) return v;
    }
    return {};
}

} // namespace vestigant::spotlight
