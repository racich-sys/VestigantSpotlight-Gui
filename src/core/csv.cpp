#include "core/csv.h"
#include "core/path_utils.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace vestigant::spotlight {

std::vector<std::string> csvParseLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur.push_back('"'); ++i; }
                else inQuotes = false;
            } else cur.push_back(c);
        } else {
            if (c == '"') inQuotes = true;
            else if (c == ',') { fields.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
    }
    fields.push_back(cur);
    return fields;
}

namespace {

std::string hexByte(unsigned char c) {
    const char* h = "0123456789ABCDEF";
    std::string out = "\\x";
    out.push_back(h[(c >> 4) & 0x0F]);
    out.push_back(h[c & 0x0F]);
    return out;
}

bool isContinuation(unsigned char c) { return (c & 0xC0u) == 0x80u; }

std::string sanitizeCsvText(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    const auto* p = reinterpret_cast<const unsigned char*>(value.data());
    const std::size_t n = value.size();
    for (std::size_t i = 0; i < n; ) {
        const unsigned char c = p[i];
        if (c == '\0' || c == '\r' || c == '\n') { out.push_back(' '); ++i; continue; }
        if (c == '\t') { out.push_back('\t'); ++i; continue; }
        if (c < 0x20u || c == 0x7Fu) { out += hexByte(c); ++i; continue; }
        if (c < 0x80u) { out.push_back(static_cast<char>(c)); ++i; continue; }

        std::size_t len = 0;
        bool valid = false;
        if (c >= 0xC2u && c <= 0xDFu && i + 1 < n && isContinuation(p[i + 1])) { len = 2; valid = true; }
        else if (c == 0xE0u && i + 2 < n && p[i + 1] >= 0xA0u && p[i + 1] <= 0xBFu && isContinuation(p[i + 2])) { len = 3; valid = true; }
        else if (c >= 0xE1u && c <= 0xECu && i + 2 < n && isContinuation(p[i + 1]) && isContinuation(p[i + 2])) { len = 3; valid = true; }
        else if (c == 0xEDu && i + 2 < n && p[i + 1] >= 0x80u && p[i + 1] <= 0x9Fu && isContinuation(p[i + 2])) { len = 3; valid = true; }
        else if (c >= 0xEEu && c <= 0xEFu && i + 2 < n && isContinuation(p[i + 1]) && isContinuation(p[i + 2])) { len = 3; valid = true; }
        else if (c == 0xF0u && i + 3 < n && p[i + 1] >= 0x90u && p[i + 1] <= 0xBFu && isContinuation(p[i + 2]) && isContinuation(p[i + 3])) { len = 4; valid = true; }
        else if (c >= 0xF1u && c <= 0xF3u && i + 3 < n && isContinuation(p[i + 1]) && isContinuation(p[i + 2]) && isContinuation(p[i + 3])) { len = 4; valid = true; }
        else if (c == 0xF4u && i + 3 < n && p[i + 1] >= 0x80u && p[i + 1] <= 0x8Fu && isContinuation(p[i + 2]) && isContinuation(p[i + 3])) { len = 4; valid = true; }

        if (valid) {
            for (std::size_t j = 0; j < len; ++j) out.push_back(static_cast<char>(p[i + j]));
            i += len;
        } else {
            out += hexByte(c);
            ++i;
        }
    }
    return out;
}

} // namespace

std::string csvEscape(const std::string& value) {
    const std::string safe = sanitizeCsvText(value);
    const bool needs = safe.find_first_of(",\"") != std::string::npos;
    if (!needs) return safe;
    std::string out = "\"";
    for (char c : safe) out += (c == '"') ? "\"\"" : std::string(1, c);
    out += "\"";
    return out;
}

Rows readCsv(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Unable to open CSV: " + pathString(file));
    std::string line;
    if (!std::getline(in, line)) return {};
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto headers = csvParseLine(line);
    if (!headers.empty() && headers[0].size() >= 3 && static_cast<unsigned char>(headers[0][0]) == 0xEF) headers[0].erase(0, 3);
    Rows rows;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto fields = csvParseLine(line);
        Row r;
        for (std::size_t i = 0; i < headers.size(); ++i) r[headers[i]] = i < fields.size() ? fields[i] : "";
        rows.push_back(std::move(r));
    }
    return rows;
}

void writeCsv(const fs::path& file, const std::vector<std::string>& headers, const Rows& rows) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write CSV: " + pathString(file));
    for (std::size_t i = 0; i < headers.size(); ++i) { if (i) out << ','; out << csvEscape(headers[i]); }
    out << "\n";
    for (const auto& r : rows) {
        for (std::size_t i = 0; i < headers.size(); ++i) { if (i) out << ','; out << csvEscape(get(r, headers[i])); }
        out << "\n";
    }
}

void writeChunkedCsv(const fs::path& outputDir, const std::string& prefix, const std::vector<std::string>& headers, const Rows& rows, std::size_t chunkRows) {
    if (chunkRows == 0) chunkRows = 250000;
    if (rows.size() <= chunkRows) { writeCsv(outputDir / (prefix + ".csv"), headers, rows); return; }
    std::size_t part = 1;
    for (std::size_t start = 0; start < rows.size(); start += chunkRows, ++part) {
        Rows slice;
        const std::size_t end = std::min(rows.size(), start + chunkRows);
        slice.reserve(end - start);
        for (std::size_t i = start; i < end; ++i) slice.push_back(rows[i]);
        std::ostringstream name;
        name << prefix << "_part" << std::setw(4) << std::setfill('0') << part << ".csv";
        writeCsv(outputDir / name.str(), headers, slice);
    }
}

std::string get(const Row& r, const std::string& key) {
    auto it = r.find(key);
    if (it != r.end()) return it->second;
    const auto keyLower = toLower(key);
    for (const auto& kv : r) if (toLower(kv.first) == keyLower) return kv.second;
    return {};
}

void set(Row& r, const std::string& key, const std::string& value) { r[key] = value; }

bool hasCsvFile(const fs::path& dir, const std::string& name) {
    std::error_code ec;
    return fs::is_regular_file(dir / name, ec);
}

} // namespace vestigant::spotlight
