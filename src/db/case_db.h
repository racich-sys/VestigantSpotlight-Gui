#pragma once
#include "app/models.h"
#include "db/sqlite_compat.h"
#include <filesystem>
#include <string>
#include <vector>

namespace vestigant::spotlight {

class Logger;

class SqlStatement {
public:
    SqlStatement() = default;
    SqlStatement(sqlite3* db, const std::string& sql);
    ~SqlStatement();
    SqlStatement(const SqlStatement&) = delete;
    SqlStatement& operator=(const SqlStatement&) = delete;
    SqlStatement(SqlStatement&& other) noexcept;
    SqlStatement& operator=(SqlStatement&& other) noexcept;

    void bind(int index, const std::string& value);
    void bind(int index, long long value);
    void bindNull(int index);
    bool stepRow();
    void stepDone();
    void reset();
    std::string colText(int index) const;
    long long colInt64(int index) const;
    sqlite3_stmt* raw() const { return stmt_; }
private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

class CaseDatabase {
public:
    CaseDatabase() = default;
    ~CaseDatabase();
    CaseDatabase(const CaseDatabase&) = delete;
    CaseDatabase& operator=(const CaseDatabase&) = delete;

    void open(const fs::path& dbPath);
    void close();
    void initializeSchema();
    void ensureGuiReviewViews();
    void exec(const std::string& sql);
    SqlStatement prepare(const std::string& sql);
    void begin();
    void commit();
    void rollbackNoThrow();
    sqlite3* raw() const { return db_; }
    fs::path path() const { return dbPath_; }

    void insertCaseInfo(const RunOptions& opt);
    void insertEvidenceSource(const EvidenceSource& source);
    void insertProcessingLog(const std::string& level, const std::string& message);

private:
    sqlite3* db_ = nullptr;
    fs::path dbPath_;
};

std::string sqlNowUtc();
std::string sqlQuoteIdentifier(const std::string& s);

} // namespace vestigant::spotlight
