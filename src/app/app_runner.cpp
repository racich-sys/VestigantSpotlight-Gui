#include "app/app_runner.h"
#include "app/case_store.h"
#include "core/app_info.h"
#include "core/logger.h"
#include "core/csv.h"
#include "core/hash.h"
#include "core/path_utils.h"
#include "codec/lzfse_codec.h"
#include "db/case_db.h"
#include "enrich_sql/sqlite_enrichment.h"
#include "export_sql/sqlite_exporter.h"
#include "ingest/evidence_preservation.h"
#include "ingest/evidence_intake.h"
#include "ingest/source_profiles.h"
#include "ingest/store_discovery.h"
#include "parsers/native_storedb_parser.h"
#include "parsers/apfs_volume_reader.h"
#include "parsers/apfs_aff4_reader.h"
#include "parsers/apfs_diagnostic_exporter.h"
#include "parsers/apfs_diagnostic_models.h"
#include "parsers/ios_app_db_parser.h"
#include <fstream>
#include <array>
#include <stdexcept>
#include <exception>
#include <sstream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <set>
#include <vector>
#include <cstring>
#include <limits>
#include <functional>
#include <tuple>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <cstdio>
#include <atomic>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <eh.h>
#endif

namespace vestigant::spotlight {

// Forward declaration retained in the runner because APFS diagnostic writer
// relocation moved helper definitions later in this translation unit, while
// dynamic AFF4/APFS probe code still calls asciiLower earlier.
std::string asciiLower(std::string s);

namespace {
#ifdef _WIN32
void installStructuredExceptionTranslator() {
    _set_se_translator([](unsigned int code, EXCEPTION_POINTERS* ep) {
        std::ostringstream os;
        os << "Windows structured exception 0x" << std::hex << code;
        if (ep && ep->ExceptionRecord) {
            os << " at address=" << ep->ExceptionRecord->ExceptionAddress;
        }
        throw std::runtime_error(os.str());
    });
}
#else
void installStructuredExceptionTranslator() {}
#endif

#ifdef _WIN32
std::wstring utf8ToWideCommand(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) {
        n = MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (n <= 0) {
            std::wstring fallback;
            fallback.reserve(s.size());
            for (unsigned char c : s) fallback.push_back(c < 128 ? static_cast<wchar_t>(c) : L'?');
            return fallback;
        }
        std::wstring ws(static_cast<std::size_t>(n), L'\0');
        MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), ws.data(), n);
        return ws;
    }
    std::wstring ws(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), ws.data(), n);
    return ws;
}

constexpr DWORD kExternalProcessTimeoutMs = 12UL * 60UL * 60UL * 1000UL;

HANDLE createKillOnCloseJobObject() {
    HANDLE jobHandle = CreateJobObjectW(nullptr, nullptr);
    if (!jobHandle) return nullptr;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(jobHandle, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        CloseHandle(jobHandle);
        return nullptr;
    }
    return jobHandle;
}

int waitForProcessWithTimeout(HANDLE processHandle, HANDLE jobHandle) {
    const DWORD waitResult = WaitForSingleObject(processHandle, kExternalProcessTimeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        if (jobHandle) {
            TerminateJobObject(jobHandle, 0xEE00U);
        } else {
            TerminateProcess(processHandle, 0xEE00U);
        }
        return -2;
    }
    if (waitResult != WAIT_OBJECT_0) {
        return static_cast<int>(GetLastError() ? GetLastError() : 1);
    }
    DWORD exitCode = 1;
    if (!GetExitCodeProcess(processHandle, &exitCode)) {
        return static_cast<int>(GetLastError() ? GetLastError() : 1);
    }
    return static_cast<int>(exitCode);
}

int runShellCommandNoWindow(const std::string& command) {
    const std::wstring cmdLine = utf8ToWideCommand(command);
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    HANDLE jobHandle = createKillOnCloseJobObject();
    const DWORD creationFlags = CREATE_NO_WINDOW | (jobHandle ? CREATE_SUSPENDED : 0);

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr,
                             mutableCmd.data(),
                             nullptr,
                             nullptr,
                             FALSE,
                             creationFlags,
                             nullptr,
                             nullptr,
                             &si,
                             &pi);
    if (!ok) {
        const DWORD err = GetLastError();
        if (jobHandle) CloseHandle(jobHandle);
        return static_cast<int>(err ? err : 1);
    }

    if (jobHandle) {
        if (!AssignProcessToJobObject(jobHandle, pi.hProcess)) {
            CloseHandle(jobHandle);
            jobHandle = nullptr;
        }
        ResumeThread(pi.hThread);
    }

    const int exitCode = waitForProcessWithTimeout(pi.hProcess, jobHandle);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (jobHandle) CloseHandle(jobHandle);
    return exitCode;
}
#else
int runShellCommandNoWindow(const std::string& command) {
    return std::system(command.c_str());
}
#endif

std::string statusClean(std::string s) {
    for (char& ch : s) {
        if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    }
    if (s.size() > 900) s = s.substr(0, 900) + "...";
    return s;
}

int stagePercent(const std::string& stage) {
    if (stage == "start") return 1;
    if (stage == "initialize_case") return 5;
    if (stage == "discover_stores") return 10;
    if (stage == "stage_zip_source") return 12;
    if (stage == "source_probe_start") return 8;
    if (stage == "source_probe_write") return 18;
    if (stage == "aff4_zip_single_file_probe") return 19;
    if (stage == "aff4_apfs_exact_file_scan") return 20;
    if (stage == "aff4_dynamic_load_probe") return 21;
    if (stage == "preserve_evidence") return 15;
    if (stage == "rediscover_preserved_stores") return 25;
    if (stage == "open_sqlite") return 30;
    if (stage == "native_parse_start") return 35;
    if (stage == "native_parse_call") return 36;
    if (stage == "native_parse_complete") return 75;
    if (stage == "enrichment_start") return 82;
    if (stage == "enrichment_complete") return 88;
    if (stage == "export_start") return 90;
    if (stage == "export_profile_start") return 90;
    if (stage == "export_query_prepare") return 91;
    if (stage == "export_query_execute") return 91;
    if (stage == "export_query_rows") return 92;
    if (stage == "export_query_complete") return 94;
    if (stage == "export_upload_samples_start") return 95;
    if (stage == "export_upload_samples_complete") return 96;
    if (stage == "export_complete") return 97;
    if (stage == "upload_bundle_start") return 98;
    if (stage == "upload_bundle_complete") return 99;
    if (stage == "complete_discover" || stage == "complete_source_probe" || stage == "complete_success") return 100;
    if (stage.rfind("failed", 0) == 0) return -1;
    return -1;
}


std::string commandQuote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
}

std::string commandQuote(const fs::path& p) { return commandQuote(pathString(p)); }

std::string psSingleQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    out += "'";
    return out;
}

std::string platformPathString(const fs::path& p) {
    std::string s = pathString(p);
#if defined(_WIN32)
    std::replace(s.begin(), s.end(), '/', '\\');
#endif
    return s;
}

#ifdef _WIN32
std::wstring windowsQuoteArg(std::wstring arg) {
    if (arg.empty()) return L"\"\"";
    bool needsQuotes = false;
    for (wchar_t ch : arg) {
        if (ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'\v' || ch == L'\"') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return arg;
    std::wstring out = L"\"";
    std::size_t backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'\"') {
            out.append(backslashes * 2U + 1U, L'\\');
            out.push_back(ch);
            backslashes = 0;
        } else {
            out.append(backslashes, L'\\');
            backslashes = 0;
            out.push_back(ch);
        }
    }
    out.append(backslashes * 2U, L'\\');
    out.push_back(L'\"');
    return out;
}

std::wstring wideProcessPath(const fs::path& p) {
    return utf8ToWideCommand(platformPathString(p));
}

int runProcessNoWindowRedirected(const std::wstring& commandLine, const fs::path& combinedOutputLog) {
    fs::create_directories(combinedOutputLog.parent_path());
    const std::wstring logPath = wideProcessPath(combinedOutputLog);
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE logHandle = CreateFileW(logPath.c_str(),
                                   GENERIC_WRITE,
                                   FILE_SHARE_READ,
                                   &sa,
                                   CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
    if (logHandle == INVALID_HANDLE_VALUE) {
        return static_cast<int>(GetLastError() ? GetLastError() : 1);
    }
    SetHandleInformation(logHandle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    std::vector<wchar_t> mutableCmd(commandLine.begin(), commandLine.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = logHandle;
    si.hStdError = logHandle;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    HANDLE jobHandle = createKillOnCloseJobObject();
    const DWORD creationFlags = CREATE_NO_WINDOW | (jobHandle ? CREATE_SUSPENDED : 0);

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr,
                             mutableCmd.data(),
                             nullptr,
                             nullptr,
                             TRUE,
                             creationFlags,
                             nullptr,
                             nullptr,
                             &si,
                             &pi);
    if (!ok) {
        const DWORD err = GetLastError();
        if (jobHandle) CloseHandle(jobHandle);
        CloseHandle(logHandle);
        return static_cast<int>(err ? err : 1);
    }

    // Close the parent's copy immediately after successful process creation.
    // The child process keeps its inherited stdout/stderr handle, while the
    // parent avoids holding the log file open for the duration of the process.
    CloseHandle(logHandle);
    logHandle = INVALID_HANDLE_VALUE;

    if (jobHandle) {
        if (!AssignProcessToJobObject(jobHandle, pi.hProcess)) {
            CloseHandle(jobHandle);
            jobHandle = nullptr;
        }
        ResumeThread(pi.hThread);
    }

    const int exitCode = waitForProcessWithTimeout(pi.hProcess, jobHandle);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (jobHandle) CloseHandle(jobHandle);
    if (logHandle != INVALID_HANDLE_VALUE) CloseHandle(logHandle);
    return exitCode;
}

int runExecutableNoWindowRedirected(const fs::path& exe, const std::vector<std::string>& args, const fs::path& combinedOutputLog) {
    std::wstring cmd = windowsQuoteArg(wideProcessPath(exe));
    for (const auto& arg : args) {
        cmd += L" ";
        cmd += windowsQuoteArg(utf8ToWideCommand(arg));
    }
    return runProcessNoWindowRedirected(cmd, combinedOutputLog);
}

int runPowerShellFileNoWindowRedirected(const fs::path& scriptPath, const fs::path& combinedOutputLog) {
    std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File ";
    cmd += windowsQuoteArg(wideProcessPath(scriptPath));
    return runProcessNoWindowRedirected(cmd, combinedOutputLog);
}

int runPowerShellCommandNoWindowRedirected(const std::string& powerShellCommand, const fs::path& combinedOutputLog) {
    std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ";
    cmd += windowsQuoteArg(utf8ToWideCommand(powerShellCommand));
    return runProcessNoWindowRedirected(cmd, combinedOutputLog);
}
#endif

void appendRunProgress(const fs::path& caseDir, int percent, const std::string& stage, const std::string& message = {}) {
    try {
        fs::create_directories(caseDir / "logs");
        const std::string ts = nowUtc();
        const std::string cleanStage = statusClean(stage);
        const std::string cleanMessage = statusClean(message);
        auto writeOne = [&](const fs::path& path, std::ios::openmode mode) {
            std::ofstream out(path, mode | std::ios::binary);
            out << ts << "\t" << percent << "\t" << cleanStage << "\t" << cleanMessage << "\n" << std::flush;
        };
        writeOne(caseDir / "logs" / "run_progress.tsv", std::ios::app);
        writeOne(caseDir / "logs" / "last_progress.tsv", std::ios::trunc);
        // Root-level mirrors are intentional: they make progress/status files easy to find and include in focused uploads.
        writeOne(caseDir / "run_progress.tsv", std::ios::app);
        writeOne(caseDir / "last_progress.tsv", std::ios::trunc);
    } catch (...) {}
}

void appendRunStatus(const fs::path& caseDir, const std::string& stage, const std::string& message = {}) {
    try {
        fs::create_directories(caseDir / "logs");
        const std::string ts = nowUtc();
        const std::string cleanStage = statusClean(stage);
        const std::string cleanMessage = statusClean(message);
        auto writeStatus = [&](const fs::path& path) {
            std::ofstream out(path, std::ios::app | std::ios::binary);
            out << ts << " stage=" << cleanStage;
            if (!cleanMessage.empty()) out << " message=" << cleanMessage;
            out << "\n" << std::flush;
        };
        auto writeLast = [&](const fs::path& path) {
            std::ofstream last(path, std::ios::binary);
            last << ts << " " << cleanStage << " " << cleanMessage << "\n" << std::flush;
        };
        writeStatus(caseDir / "logs" / "run_status.txt");
        writeLast(caseDir / "logs" / "last_stage.txt");
        // Root-level mirrors are intentional: they make status files easy to find and include in focused uploads.
        writeStatus(caseDir / "run_status.txt");
        writeLast(caseDir / "last_stage.txt");
        appendRunProgress(caseDir, stagePercent(stage), cleanStage, cleanMessage);
    } catch (...) {}
}

bool isDotStoreDbPath(const fs::path& p) {
    return toLower(p.filename().string()) == ".store.db";
}

bool isStoreDbPath(const fs::path& p) {
    return toLower(p.filename().string()) == "store.db";
}

std::string sourceDbRole(const fs::path& p) {
    const auto f = toLower(p.filename().string());
    if (f == ".store.db") return "dot_store_db";
    if (f == "store.db") return "store_db";
    return "other";
}

std::string storeGroupKey(const StoreInfo& s) {
    // Group parser candidates by their actual database parent directory. This
    // keeps store.db/.store.db alternates together while preventing distinct
    // iOS CoreSpotlight protection-class indexes from collapsing merely because
    // they share the parent folder name "index.spotlightV2".
    const std::string parentKey = toLower(pathString(s.storePath.parent_path()));
    if (!parentKey.empty()) return parentKey;
    return toLower(s.storeGuid);
}

std::vector<StoreInfo> selectDatabasesForParsing(const std::vector<StoreInfo>& candidates, SourceProfileKind profile, bool fullScan, Logger& log) {
    (void)fullScan; // Full-scan controls source probing/extraction depth, not duplicate database parsing.
    // Native-first behavior: preserve and inventory all discovered database
    // files, but parse one primary database per logical Store-V2/CoreSpotlight
    // group. This avoids double-counting store.db and .store.db while keeping
    // alternates in inventory/hash output for forensic completeness.
    std::map<std::string, std::vector<StoreInfo>> groups;
    std::size_t validCount = 0;
    std::size_t validStoreDbCount = 0;
    std::size_t validDotStoreCount = 0;
    for (const auto& s : candidates) {
        if (!s.isValid) continue;
        ++validCount;
        if (isStoreDbPath(s.storePath)) ++validStoreDbCount;
        else if (isDotStoreDbPath(s.storePath)) ++validDotStoreCount;
        groups[storeGroupKey(s)].push_back(s);
    }

    std::vector<StoreInfo> out;
    out.reserve(groups.size());
    for (auto& [key, group] : groups) {
        std::sort(group.begin(), group.end(), [](const StoreInfo& a, const StoreInfo& b) {
            const bool aStore = isStoreDbPath(a.storePath);
            const bool bStore = isStoreDbPath(b.storePath);
            if (aStore != bStore) return aStore; // primary preference: store.db
            const bool aDot = isDotStoreDbPath(a.storePath);
            const bool bDot = isDotStoreDbPath(b.storePath);
            if (aDot != bDot) return !aDot;
            if (a.fileSizeBytes != b.fileSizeBytes) return a.fileSizeBytes > b.fileSizeBytes;
            return pathString(a.storePath) < pathString(b.storePath);
        });
        out.push_back(group.front());
    }
    std::sort(out.begin(), out.end(), [](const StoreInfo& a, const StoreInfo& b) {
        if (a.storeGuid != b.storeGuid) return a.storeGuid < b.storeGuid;
        return pathString(a.storePath) < pathString(b.storePath);
    });
    const std::string policy = (profile == SourceProfileKind::IOS)
        ? "ios_prefer_store_db_one_primary_per_corespotlight_group_preserve_dot_store_alternate"
        : "prefer_store_db_one_primary_per_store_group";
    log.info("Parser database selection completed. valid_candidates=" + std::to_string(validCount) +
             " selected_primary_databases=" + std::to_string(out.size()) +
             " valid_store_db=" + std::to_string(validStoreDbCount) +
             " valid_dot_store_db=" + std::to_string(validDotStoreCount) +
             " selection_policy=" + policy);
    return out;
}

void writeStoreSelectionCsv(const fs::path& caseDir,
                            const std::vector<StoreInfo>& candidates,
                            const std::vector<StoreInfo>& selected,
                            Logger& log) {
    std::set<std::string> selectedPaths;
    for (const auto& s : selected) selectedPaths.insert(pathString(s.storePath));
    std::map<std::string, std::vector<StoreInfo>> groups;
    for (const auto& s : candidates) {
        if (!s.isValid) continue;
        groups[storeGroupKey(s)].push_back(s);
    }
    const fs::path outPath = caseDir / "store_selection.csv";
    std::ofstream out(outPath, std::ios::binary);
    if (!out) throw std::runtime_error("Unable to write store selection CSV: " + pathString(outPath));
    out << "source_id,store_guid,store_group_key,database_path,database_role,is_valid,is_selected,selection_reason,file_size_bytes,sha256,validation_error\n";
    for (auto& [key, group] : groups) {
        bool hasValidStoreDb = false;
        for (const auto& s : group) if (isStoreDbPath(s.storePath)) hasValidStoreDb = true;
        for (const auto& s : group) {
            const bool selectedThis = selectedPaths.find(pathString(s.storePath)) != selectedPaths.end();
            std::string reason;
            if (selectedThis && isStoreDbPath(s.storePath)) reason = "SELECTED_PRIMARY_STORE_DB";
            else if (selectedThis && isDotStoreDbPath(s.storePath) && !hasValidStoreDb) reason = "SELECTED_FALLBACK_DOT_STORE_DB_NO_VALID_STORE_DB";
            else if (!selectedThis && isDotStoreDbPath(s.storePath) && hasValidStoreDb) reason = "NOT_SELECTED_ALTERNATE_DOT_STORE_DB_PRESERVED_ONLY";
            else if (!selectedThis) reason = "NOT_SELECTED_ALTERNATE_DATABASE_PRESERVED_ONLY";
            out << csvEscape(s.sourceId) << ','
                << csvEscape(s.storeGuid) << ','
                << csvEscape(key) << ','
                << csvEscape(pathString(s.storePath)) << ','
                << csvEscape(sourceDbRole(s.storePath)) << ','
                << (s.isValid ? "1" : "0") << ','
                << (selectedThis ? "1" : "0") << ','
                << csvEscape(reason) << ','
                << s.fileSizeBytes << ','
                << csvEscape(s.sha256) << ','
                << csvEscape(s.validationError) << "\n";
        }
    }
    log.info("Store selection CSV written: " + pathString(outPath));
}


bool copyFileIfExists(const fs::path& src, const fs::path& dst, std::ofstream& manifest, const std::string& absentStatus = "MISSING") {
    try {
        if (!fs::exists(src) || !fs::is_regular_file(src)) {
            manifest << absentStatus << "," << pathString(src) << "," << pathString(dst) << "\n";
            return false;
        }
        fs::create_directories(dst.parent_path());
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        manifest << "COPIED," << pathString(src) << "," << pathString(dst) << "\n";
        return true;
    } catch (const std::exception& ex) {
        manifest << "ERROR," << pathString(src) << "," << pathString(dst) << "," << ex.what() << "\n";
        return false;
    } catch (...) {
        manifest << "ERROR," << pathString(src) << "," << pathString(dst) << ",unknown copy error\n";
        return false;
    }
}
bool copyDirectoryIfExists(const fs::path& src, const fs::path& dst, std::ofstream& manifest, const std::string& absentStatus = "MISSING_DIR") {
    try {
        if (!fs::exists(src) || !fs::is_directory(src)) {
            manifest << absentStatus << "," << pathString(src) << "," << pathString(dst) << "\n";
            return false;
        }
        fs::create_directories(dst);
        std::size_t copied = 0;
        std::error_code ec;
        for (const auto& de : fs::recursive_directory_iterator(src, ec)) {
            if (ec) break;
            const auto rel = fs::relative(de.path(), src, ec);
            if (ec) continue;
            const auto outPath = dst / rel;
            if (de.is_directory()) {
                fs::create_directories(outPath);
            } else if (de.is_regular_file()) {
                fs::create_directories(outPath.parent_path());
                fs::copy_file(de.path(), outPath, fs::copy_options::overwrite_existing);
                ++copied;
            }
        }
        manifest << "COPIED_DIR," << pathString(src) << "," << pathString(dst) << ",files=" << copied << "\n";
        return true;
    } catch (const std::exception& ex) {
        manifest << "ERROR_DIR," << pathString(src) << "," << pathString(dst) << "," << ex.what() << "\n";
        return false;
    } catch (...) {
        manifest << "ERROR_DIR," << pathString(src) << "," << pathString(dst) << ",unknown directory copy error\n";
        return false;
    }
}


void refreshUploadRunDiagnostics(const fs::path& caseDir) {
    const fs::path uploadDir = caseDir / "Upload";
    if (!fs::exists(uploadDir) || !fs::is_directory(uploadDir)) return;
    const std::vector<fs::path> diagnostics = {
        "run_status.txt", "last_stage.txt", "run_progress.tsv", "last_progress.tsv", "VestigantSpotlight.log"
    };
    for (const auto& name : diagnostics) {
        std::error_code ec;
        const fs::path src = caseDir / "logs" / name;
        if (fs::exists(src, ec) && fs::is_regular_file(src, ec)) {
            fs::create_directories(uploadDir, ec);
            fs::copy_file(src, uploadDir / name.filename(), fs::copy_options::overwrite_existing, ec);
        }
    }
}

void writeUploadReviewIndex(const fs::path& file) {
    try {
        fs::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary);
        if (!out) return;
        out << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Vestigant Spotlight Thin Upload Review Index</title>";
        out << "<style>body{font-family:Segoe UI,Arial,sans-serif;margin:24px;line-height:1.4} table{border-collapse:collapse;margin:12px 0} td,th{border:1px solid #ccc;padding:6px 10px;vertical-align:top} code{background:#eee;padding:2px 4px} .note{background:#eef7ff;border:1px solid #8dbbe8;padding:10px;margin:12px 0}</style>";
        out << "</head><body>";
        out << "<h1>Vestigant Spotlight Thin Upload Review Index</h1>";
        out << "<div class=\"note\"><b>Thin Upload:</b> this folder intentionally excludes the full SQLite database and large full-case CSV exports. Use the local case folder for full investigation, or create targeted CSV slices with <code>Export-SpotlightTargetedData.ps1</code>.</div>";
        auto row = [&](const char* href, const char* desc) { out << "<tr><td><a href=\"" << href << "\">" << href << "</a></td><td>" << desc << "</td></tr>"; };
        out << "<h2>Run summary and helper files</h2><table><tr><th>File</th><th>Purpose</th></tr>";
        row("CASE_REVIEW_SUMMARY.txt", "Plain-language run summary, counts, top fields, and limitations.");
        row("investigator_dashboard.html", "Bounded investigator-facing HTML dashboard with usage, recent activity, WhereFroms, folder, volume, and content-type pivots.");
        row("UPLOAD_README.txt", "Thin-upload package explanation.");
        row("TARGETED_EXPORT_README.txt", "Examples for exporting targeted slices from the local SQLite database.");
        row("INVESTIGATOR_UI_GUIDE.md", "Database-backed review UI workflow and performance notes.");
        row("IOS_CORESPOTLIGHT_PLAN.md", "Current iOS/CoreSpotlight parser status and required next parser route.");
        row("SOURCE_INTAKE_PLAN.md", "Evidence source intake status, including ZIP/folder support and AFF4/raw image readiness notes.");
        row("evidence_source_readiness.csv", "Machine-readable source intake status for folder, ZIP, AFF4, and raw-image inputs.");
        row("source_probe_signatures.csv", "Container/filesystem/Spotlight signature hits from source-probe bounded scanning.");
        row("source_probe_summary.json", "Machine-readable source-probe scan summary.");
        row("image_inventory_readiness.csv", "Image-backed filesystem inventory readiness for AFF4/APFS active-file comparison.");
        row("reader_tool_readiness.csv", "AFF4/APFS/HFS helper tool discovery status for reader integration.");
        row("AFF4_APFS_READER_PLAN.md", "Current AFF4/APFS reader integration plan, hash policy, and tool search order.");
        row("AFF4_APFS_V1_DIAGNOSTIC_RERUN_PLAN.md", "V1.0.0 single-AFF4 diagnostic rerun plan and decision rules before changing APFS reconstruction gates.");
        row("aff4_apfs_v1_diagnostic_checklist.csv", "Machine-readable checklist of expected V1 AFF4/APFS diagnostic outputs and interpretation rules.");
        row("aff4_apfs_v1_diagnostic_plan_summary.json", "Machine-readable summary of the V1 AFF4/APFS diagnostic rerun policy.");
        row("AFF4_STREAM_SELECTION_PLAN.md", "AFF4 stream-list command status and ranked stream candidates when aff4imager is available.");
        row("AFF4_ZIP_SINGLE_FILE_PROBE.md", "Single-file AFF4 ZIP central-directory probe; reads only the explicit AFF4 input path.");
        row("aff4_zip_probe_summary.json", "Machine-readable single-file AFF4 ZIP probe summary.");
        row("aff4_zip_central_directory.csv", "Bounded central-directory entries parsed from the explicit AFF4 file only.");
        row("AFF4_CPP_LITE_RANDOM_ACCESS_PLAN.md", "AFF4 CPP Lite random-access feasibility plan; no full raw export is performed by default.");
        row("aff4_cpp_lite_reader_readiness.csv", "AFF4 CPP Lite executable/library/header readiness for virtual block-reader integration.");
        row("aff4_cpp_lite_dynamic_load_probe.csv", "Runtime libaff4 dynamic-load/open/read/close probe results; no full raw export is performed.");
        row("aff4_virtual_apfs_probe.csv", "Virtual GPT/APFS probe rows from bounded libaff4 reads against the explicit AFF4 input.");
        row("aff4_virtual_apfs_probe_summary.json", "Machine-readable summary of the virtual GPT/APFS probe.");
        row("AFF4_VIRTUAL_APFS_PROBE.md", "Human-readable APFS virtual-read probe report and interpretation.");
        row("aff4_apfs_container_superblock.csv", "Parsed APFS NX superblock fields from the libaff4 virtual object when virtual APFS probing is enabled.");
        row("aff4_apfs_container_superblock_summary.json", "Machine-readable APFS container superblock parse summary.");
        row("aff4_apfs_checkpoint_descriptor_scan.csv", "Bounded checkpoint descriptor block reads using nx_xp_desc_* fields from the APFS container superblock.");
        row("AFF4_APFS_CONTAINER_VIEW.md", "Human-readable APFS container view and next object-map enumeration steps.");
        row("aff4_apfs_checkpoint_map.csv", "Parsed APFS checkpoint-map entries from the NX checkpoint descriptor ring.");
        row("aff4_apfs_checkpoint_mapped_object_probe.csv", "Bounded reads of checkpoint-map physical block candidates for OMAP/APSB/NXSB discovery.");
        row("aff4_apfs_checkpoint_map_summary.json", "Machine-readable APFS checkpoint-map/object-resolution probe summary.");
        row("AFF4_APFS_CHECKPOINT_MAP_PROBE.md", "Human-readable APFS checkpoint-map interpretation and next OMAP/B-tree steps.");
        row("aff4_apfs_object_id_probe.csv", "Bounded direct object-ID reads for NX omap/spaceman/reaper/filesystem OIDs using the exact AFF4 virtual object.");
        row("aff4_apfs_btree_node_probe.csv", "Generic APFS B-tree/BTREE_NODE header inventory from checkpoint-mapped and direct object-ID reads.");
        row("aff4_apfs_object_resolution_probe_summary.json", "Machine-readable summary of APFS object-ID and B-tree inventory probing.");
        row("AFF4_APFS_OBJECT_RESOLUTION_PROBE.md", "Human-readable APFS object-resolution interpretation and next APFS object-map/B-tree steps.");
        row("aff4_apfs_omap_phys_probe.csv", "Parsed APFS object-map physical object fields, including om_tree_oid, from the exact AFF4 virtual reader.");
        row("aff4_apfs_omap_btree_root_probe.csv", "Bounded read and header parse of the object-map B-tree root referenced by om_tree_oid.");
        row("aff4_apfs_omap_lookup_probe.csv", "Current OMAP lookup readiness rows for NX filesystem OIDs and other target object IDs.");
        row("aff4_apfs_omap_btree_toc_probe.csv", "Raw candidate table-of-contents entries from the parsed OMAP B-tree root for branch/leaf decoder planning.");
        row("aff4_apfs_omap_leaf_kv_decode.csv", "Decoded fixed-size OMAP leaf key/value entries and bounded resolved-object probes.");
        row("aff4_apfs_omap_leaf_lookup_results.csv", "Best OMAP leaf lookup result per NX filesystem OID target.");
        row("aff4_apfs_resolved_volume_superblocks.csv", "APFS APSB volume-superblock fields parsed from container-OMAP lookup-selected physical blocks.");
        row("aff4_apfs_resolved_volume_superblocks_summary.json", "Machine-readable summary of OMAP-resolved APFS volume superblock parsing and volume OMAP probing.");
        row("AFF4_APFS_RESOLVED_VOLUME_SUPERBLOCKS.md", "Human-readable interpretation of the OMAP-resolved APSB volume-superblock parse.");
        row("aff4_apfs_volume_omap_probe.csv", "Per-volume object-map physical object and object-map B-tree-root probe from each parsed APSB apfs_omap_oid.");
        row("aff4_apfs_volume_root_tree_lookup.csv", "Target-guided volume OMAP lookup for each APSB apfs_root_tree_oid, including bounded branch traversal and resolved root-tree object header.");
        row("aff4_apfs_root_tree_node_probe.csv", "Bounded APFS filesystem root-tree B-tree header probe rows from resolved volume root-tree objects.");
        row("aff4_apfs_root_tree_record_sample.csv", "First-pass APFS filesystem root-tree key/value samples for branch-child planning and safe directory-record decoding.");
        row("aff4_apfs_spotlight_target_scan.csv", "Targeted APFS namespace scan hits for .Spotlight-V100, Store-V2, store.db, dbStr/dbHdr, and CoreSpotlight names.");
        row("aff4_apfs_spotlight_copy_attempt.csv", "Gated copy-out decisions for any Spotlight/CoreSpotlight target names found during APFS scanning.");
        row("aff4_apfs_spotlight_file_extent_probe.csv", "Targeted APFS file-extent candidates for Spotlight/CoreSpotlight target child file IDs.");
        row("aff4_apfs_spotlight_inode_probe.csv", "Guided APFS inode/private data-stream probe for Spotlight/CoreSpotlight target child file IDs.");
        row("aff4_apfs_spotlight_inode_probe_summary.json", "Machine-readable summary of Spotlight target inode/data-stream readiness.");
        row("aff4_apfs_spotlight_xattr_probe.csv", "Diagnostic APFS XATTR rows for Spotlight/CoreSpotlight targets, including decmpfs/resource-fork candidates.");
        row("aff4_apfs_spotlight_xattr_probe_summary.json", "Machine-readable APFS XATTR diagnostic summary for compressed/resource-fork reconstruction planning.");
        row("AFF4_APFS_SPOTLIGHT_XATTR_PROBE.md", "Human-readable APFS XATTR diagnostic and compression/resource-fork triage report.");
        row("AFF4_APFS_SPOTLIGHT_INODE_PROBE.md", "Human-readable inode/private data-stream probe and extent-readiness report.");
        row("aff4_apfs_spotlight_file_extent_probe_summary.json", "Machine-readable summary of Spotlight target file-extent probe readiness.");
        row("AFF4_APFS_SPOTLIGHT_FILE_EXTENT_PROBE.md", "Human-readable file-extent probe and copy-out readiness report.");
        row("aff4_apfs_spotlight_target_scan_summary.json", "Machine-readable summary of targeted APFS Spotlight namespace scanning.");
        row("AFF4_APFS_SPOTLIGHT_TARGET_SCAN.md", "Human-readable interpretation of targeted APFS Spotlight scan and copy-out gate status.");
        row("AFF4_APFS_ROOT_TREE_NODE_PROBE.md", "Human-readable interpretation of the bounded filesystem root-tree node/key probe.");
        row("AFF4_APFS_VOLUME_OMAP_PROBE.md", "Human-readable interpretation of the per-volume OMAP probe and next catalog/root-tree lookup step.");
        row("aff4_apfs_omap_probe_summary.json", "Machine-readable OMAP probe row counts and next-step status.");
        row("AFF4_APFS_OMAP_TOC_PROBE.md", "Human-readable OMAP B-tree TOC reconnaissance and branch/leaf decoder planning.");
        row("AFF4_APFS_OMAP_PROBE.md", "Human-readable APFS OMAP interpretation and next traversal steps.");
        row("aff4_stream_inventory.csv", "Machine-readable AFF4 stream-list lines and heuristic APFS/disk/Spotlight candidate classification.");
        row("active_file_comparison_readiness.csv", "Whether Spotlight-to-image active-file comparison can run for this source and what reader layer is missing.");
        row("Export-SpotlightTargetedData.ps1", "PowerShell helper for usage, timeline, path, field, artifact, WhereFroms, and count exports.");
        row("Create-UploadZip.ps1", "Verified ZIP helper for creating an upload archive from the Upload folder.");
        row("UPLOAD_MANIFEST.txt", "Manifest of files copied into this Upload folder.");
        row("run_status.txt", "Run-stage status log.");
        row("last_stage.txt", "Final stage marker.");
        row("VestigantSpotlight.log", "Main run log.");
        out << "</table>";
        out << "<h2>Core diagnostics included in Upload</h2><table><tr><th>File</th><th>Purpose</th></tr>";
        row("exports/upload_samples/parser_coverage_summary_sample.csv", "Parser and enrichment coverage metrics sample.");
        row("exports/upload_samples/field_inventory_sample.csv", "Decoded field coverage sample.");
        row("exports/date_field_inventory.csv", "Date field coverage and parse method summary.");
        row("exports/upload_samples/native_decode_attempts_sample.csv", "Native decode attempt summary sample.");
        row("exports/upload_samples/raw_failures_sample.csv", "Native/raw decode failures sample, if any.");
        row("exports/upload_samples/raw_failures_sample.csv", "Partial/raw decode failures sample, if any.");
        row("exports/upload_samples/native_key_values_high_value_sample.csv", "High-value native field/value sample for field-hit review.");
        row("exports/upload_samples/native_property_dictionary_sample.csv", "Native property dictionary and value coverage sample.");
        row("exports/upload_samples/native_key_values_high_value_sample.csv", "High-value decoded metadata field sample.");
        row("exports/usage_evidence.csv", "Usage evidence summary rows.");
        row("exports/object_usage_summary.csv", "Object-centric usage summary: one row per artifact/object with filename/path and fused use metadata.");
        row("exports/image_inventory_sources.csv", "Image source inventory/readiness rows for AFF4/APFS-backed comparison.");
        row("exports/active_file_comparison_readiness.csv", "Image inventory availability and active-file comparison readiness.");
        row("exports/spotlight_active_file_comparison.csv", "Spotlight artifacts compared to image_file_inventory when image filesystem inventory rows are available.");
        row("exports/upload_samples/artifacts_usage_focus.csv", "Artifacts linked to usage evidence sample.");
        row("exports/upload_samples/raw_date_candidates_sample.csv", "Date candidate sample for timeline/date review.");
        out << "</table>";
        out << "<h2>Bounded database samples and focused pivots</h2><table><tr><th>File</th><th>Purpose</th></tr>";
        row("exports/upload_samples/upload_table_counts.csv", "SQLite table row counts.");
        row("exports/upload_samples/upload_samples_manifest.csv", "Sample files, row caps, and table totals.");
        row("exports/upload_samples/artifacts_sample.csv", "Bounded sample from artifacts table.");
        row("exports/upload_samples/raw_key_values_sample.csv", "Bounded sample from raw_key_values table.");
        row("exports/upload_samples/raw_date_candidates_sample.csv", "Bounded sample from raw_date_candidates table.");
        row("exports/upload_samples/timeline_events_sample.csv", "Bounded sample from timeline_events table.");
        row("exports/upload_samples/artifacts_usage_focus.csv", "Usage-bearing artifacts for investigator triage.");
        row("exports/upload_samples/object_usage_summary_focus.csv", "Object-centric usage rows with file name/path, use count, last used, and fused dates.");
        row("exports/upload_samples/artifacts_wherefrom_focus.csv", "Artifacts with WhereFroms/download-origin style fields.");
        row("exports/upload_samples/artifacts_path_focus.csv", "Artifacts with stronger path/name reconstruction data.");
        row("exports/upload_samples/path_reconstruction_focus.csv", "Parent-inode reconstructed path candidates with separate applied-path and candidate-match status.");
        row("exports/upload_samples/same_folder_groups_focus.csv", "Same-folder parent-inode groups with reconstruction counts.");
        row("exports/upload_samples/timeline_usage_focus.csv", "Timeline rows associated with usage/opened fields.");
        row("exports/upload_samples/native_key_values_high_value_sample.csv", "High-value native key/value metadata sample.");
        row("exports/upload_samples/content_type_summary.csv", "Content type counts with usage/path/size rollups.");
        row("exports/upload_samples/store_content_type_summary.csv", "Content type counts by Spotlight store.");
        row("exports/upload_samples/folder_activity_summary.csv", "Same-parent folder activity rollup from Spotlight parent inode data.");
        row("exports/upload_samples/recent_activity_focus.csv", "Recent artifacts by usage/update signals.");
        row("exports/upload_samples/volume_root_focus.csv", "Volume root and mounted-volume indicators.");
        out << "</table>";
        out << "<h2>Local-only files intentionally not in Upload</h2><p>The following are normally present in the local case folder but omitted from Upload: <code>VestigantSpotlight.case.sqlite</code>, <code>spotlight_case.db</code>, full <code>artifact_summary.csv</code>, full <code>investigator_timeline.csv</code>, full <code>native_key_values_part_*.csv</code>, raw AFF4/iOS tool output logs, helper scripts that may contain absolute evidence paths, and other large full-case CSVs.</p>";
        out << "<p>Active file comparison is now modeled through image_file_inventory and vw_spotlight_active_file_comparison. It remains readiness-only until an AFF4/APFS filesystem reader populates image_file_inventory.</p>";
        out << "</body></html>\n";
    } catch (...) {}
}



void writeUiAndIosPlanningFiles(const fs::path& caseDir) {
    try {
        {
            std::ofstream out(caseDir / "INVESTIGATOR_UI_GUIDE.md", std::ios::binary);
            out << "# Vestigant Spotlight Investigator UI Guide\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Purpose\n\n";
            out << "The Windows GUI is the preferred local review layer for the full SQLite case database. The thin Upload bundle is for troubleshooting and remote review; it is not intended to replace local database-backed investigation.\n\n";
            out << "## Review model\n\n";
            out << "- Open `VestigantSpotlight.case.sqlite` from the GUI Review tab.\n";
            out << "- Use paged database views; do not load full CSVs into memory.\n";
            out << "- Start with the `Investigator - *` views before using raw tables.\n";
            out << "- Export only the current page when a subset needs to be preserved or uploaded.\n\n";
            out << "## Primary investigator views\n\n";
            out << "- Investigator - Usage Artifacts\n";
            out << "- Investigator - Usage Timeline\n";
            out << "- Investigator - Recent Activity\n";
            out << "- Investigator - WhereFroms / Downloads\n";
            out << "- Investigator - Content Type Summary\n";
            out << "- Investigator - Store Content Types\n";
            out << "- Investigator - Folder Activity\n";
            out << "- Investigator - Volume Root / Mounted\n";
            out << "- iOS Readiness - Relevant Fields\n\n";
            out << "## UI performance rules\n\n";
            out << "- Query SQLite directly and page results.\n";
            out << "- Keep default page sizes bounded.\n";
            out << "- Search uses SQL LIKE across selected high-value columns.\n";
            out << "- Full raw-key/value review should be targeted by field, path, inode, or date instead of browsed from row one.\n\n";
            out << "## Source intake status\n\n";
            out << "- Folder and ZIP sources are implemented for Store-V2/CoreSpotlight discovery.\n";
            out << "- AFF4 and raw flat-image inputs are identified and registered in source-probe/readiness output, but container/filesystem extraction is not implemented yet.\n";
            out << "- Review `SOURCE_INTAKE_PLAN.md` and `evidence_source_readiness.csv` in the case folder for source-specific status.\n";
        }
        {
            std::ofstream out(caseDir / "IOS_CORESPOTLIGHT_PLAN.md", std::ios::binary);
            out << "# iOS CoreSpotlight Parser Plan\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Current status\n\n";
            out << "The current native parser can discover and attempt Store-V2 style CoreSpotlight `store.db` paths when the iOS profile is selected. It does not yet implement the separate iOS SQLite `index.db` / binary plist / NSKeyedArchiver parser route.\n\n";
            out << "## Expected iOS evidence inputs\n\n";
            out << "- Full file system extraction from tools such as GrayKey, Verakey, or Cellebrite.\n";
            out << "- CoreSpotlight metadata folders under `private/var/mobile/Library/Metadata/CoreSpotlight/` when present.\n";
            out << "- Application-group or app-container CoreSpotlight stores when present.\n";
            out << "- SQLite `index.db` and related binary plist / NSKeyedArchiver blobs where present.\n\n";
            out << "## Required iOS-specific parser work\n\n";
            out << "1. Add source discovery for CoreSpotlight SQLite `index.db` files and related support databases.\n";
            out << "2. Add a separate parser route for SQLite tables rather than forcing all iOS evidence through the Mac Store-V2 parser.\n";
            out << "3. Decode binary plist / NSKeyedArchiver metadata values safely, with per-field exception isolation.\n";
            out << "4. Normalize item identifiers, bundle IDs, domain identifiers, protection classes, dates, text snippets, ranking fields, and app attribution.\n";
            out << "5. Add iOS-focused investigator views for communications, app documents, Safari/web, cloud apps, app bundle/domain pivots, and deleted/index-only content.\n\n";
            out << "## Shared Mac/iOS UI model\n\n";
            out << "The review UI should keep a common artifact/timeline/key-value model while allowing source-specific tabs and pivots. Mac Store-V2 and iOS CoreSpotlight should remain separate parser routes that feed the same SQLite review database.\n";
        }
    } catch (...) {}
}

bool thinUploadDeniedRelativeName(const fs::path& relativeName) {
    std::string s = pathString(relativeName);
    std::replace(s.begin(), s.end(), '\\', '/');
    std::string lower = toLower(s);
    const std::size_t slash = lower.find_last_of('/');
    const std::string leaf = (slash == std::string::npos) ? lower : lower.substr(slash + 1);
    static const std::set<std::string> deniedLeafNames = {
        "aff4_stream_inventory_raw.txt",
        "ios_focused_zip_extract.log",
        "ios_focused_zip_extract_7z.log",
        "ios_focused_zip_extract.ps1",
        "ios_ffs_file_inventory.csv",
        "image_file_inventory.csv"
    };
    return deniedLeafNames.find(leaf) != deniedLeafNames.end();
}

void copyThinUploadFileIfAllowed(const fs::path& src,
                                 const fs::path& dst,
                                 const fs::path& relativeName,
                                 std::ofstream& manifest,
                                 const std::string& missingStatus = "MISSING") {
    if (thinUploadDeniedRelativeName(relativeName)) {
        manifest << "REDACTED_BY_THIN_UPLOAD_POLICY," << pathString(src) << "," << pathString(dst) << "\n";
        return;
    }
    copyFileIfExists(src, dst, manifest, missingStatus);
}

constexpr std::uintmax_t kThinUploadMaxDynamicExportCsvBytes = 50ULL * 1024ULL * 1024ULL;

void copyThinUploadExportCsvIfAllowed(const fs::path& src,
                                      const fs::path& dst,
                                      const fs::path& relativeName,
                                      std::ofstream& manifest) {
    if (thinUploadDeniedRelativeName(relativeName)) {
        manifest << "REDACTED_BY_THIN_UPLOAD_POLICY," << pathString(src) << "," << pathString(dst) << "\n";
        return;
    }
    std::error_code sizeEc;
    const std::uintmax_t bytes = fs::file_size(src, sizeEc);
    if (!sizeEc && bytes > kThinUploadMaxDynamicExportCsvBytes) {
        manifest << "REDACTED_SIZE_LIMIT_EXCEEDED," << pathString(src) << "," << pathString(dst)
                 << ",bytes=" << bytes << "\n";
        return;
    }
    copyFileIfExists(src, dst, manifest);
}

void createUploadBundle(const fs::path& caseDir) {
    try {
        appendRunStatus(caseDir, "upload_bundle_start", "copy focused upload files");
        const fs::path uploadDir = caseDir / "Upload";
        fs::create_directories(uploadDir);
        std::ofstream manifest(uploadDir / "UPLOAD_MANIFEST.txt", std::ios::binary);
        manifest << "generated_utc," << nowUtc() << "\n";
        manifest << "status,source,destination\n";
        const std::vector<fs::path> rootFiles = {
            "CASE_REVIEW_SUMMARY.txt", "investigator_dashboard.html", "INVESTIGATOR_UI_GUIDE.md", "IOS_CORESPOTLIGHT_PLAN.md", "UPLOAD_README.txt", "TARGETED_EXPORT_README.txt", "Export-SpotlightTargetedData.ps1", "Create-UploadZip.ps1",
            "case_info.json", "case_summary.json", "case_summary.csv",
            "SOURCE_INTAKE_PLAN.md", "AFF4_APFS_READER_PLAN.md", "AFF4_APFS_V1_DIAGNOSTIC_RERUN_PLAN.md", "aff4_apfs_v1_diagnostic_checklist.csv", "aff4_apfs_v1_diagnostic_plan_summary.json", "AFF4_STREAM_SELECTION_PLAN.md", "AFF4_CPP_LITE_RANDOM_ACCESS_PLAN.md", "aff4_cpp_lite_reader_readiness.csv", "aff4_cpp_lite_integration_readiness.csv", "aff4_cpp_lite_dynamic_load_probe.csv", "aff4_virtual_apfs_probe.csv", "aff4_virtual_apfs_probe_summary.json", "AFF4_VIRTUAL_APFS_PROBE.md", "aff4_apfs_container_superblock.csv", "aff4_apfs_container_superblock_summary.json", "aff4_apfs_checkpoint_descriptor_scan.csv", "AFF4_APFS_CONTAINER_VIEW.md", "aff4_apfs_checkpoint_map.csv", "aff4_apfs_checkpoint_mapped_object_probe.csv", "aff4_apfs_checkpoint_map_summary.json", "AFF4_APFS_CHECKPOINT_MAP_PROBE.md", "aff4_apfs_object_id_probe.csv", "aff4_apfs_btree_node_probe.csv", "aff4_apfs_omap_phys_probe.csv", "aff4_apfs_omap_btree_root_probe.csv", "aff4_apfs_omap_lookup_probe.csv", "aff4_apfs_omap_btree_toc_probe.csv", "aff4_apfs_omap_leaf_kv_decode.csv", "aff4_apfs_omap_leaf_lookup_results.csv", "aff4_apfs_resolved_volume_superblocks.csv", "aff4_apfs_resolved_volume_superblocks_summary.json", "AFF4_APFS_RESOLVED_VOLUME_SUPERBLOCKS.md", "aff4_apfs_volume_omap_probe.csv", "AFF4_APFS_VOLUME_OMAP_PROBE.md", "aff4_apfs_volume_root_tree_lookup.csv", "aff4_apfs_volume_root_tree_lookup_summary.json", "AFF4_APFS_VOLUME_ROOT_TREE_LOOKUP.md", "aff4_apfs_root_tree_node_probe.csv", "aff4_apfs_root_tree_record_sample.csv", "aff4_apfs_spotlight_target_scan.csv", "aff4_apfs_spotlight_name_scan_sample.csv", "aff4_apfs_spotlight_copy_attempt.csv", "aff4_apfs_logical_directory_walk.csv", "aff4_apfs_logical_directory_walk_summary.json", "aff4_apfs_spotlight_xattr_probe.csv", "aff4_apfs_spotlight_xattr_probe_summary.json", "AFF4_APFS_SPOTLIGHT_XATTR_PROBE.md", "aff4_apfs_spotlight_file_extent_probe.csv", "aff4_apfs_spotlight_file_extent_probe_summary.json", "AFF4_APFS_SPOTLIGHT_FILE_EXTENT_PROBE.md", "aff4_apfs_spotlight_inode_probe.csv", "aff4_apfs_spotlight_inode_probe_summary.json", "AFF4_APFS_SPOTLIGHT_INODE_PROBE.md", "aff4_apfs_spotlight_target_scan_summary.json", "AFF4_APFS_SPOTLIGHT_TARGET_SCAN.md", "aff4_apfs_root_tree_node_probe_summary.json", "AFF4_APFS_ROOT_TREE_NODE_PROBE.md", "aff4_apfs_omap_probe_summary.json", "AFF4_APFS_OMAP_TOC_PROBE.md", "AFF4_APFS_OMAP_PROBE.md", "aff4_apfs_object_resolution_probe_summary.json", "AFF4_APFS_OBJECT_RESOLUTION_PROBE.md", "AFF4_CPP_LITE_DYNAMIC_LOAD_PROBE.md", "aff4_stream_inventory.csv", "aff4_zip_probe_summary.json", "aff4_zip_central_directory.csv", "AFF4_ZIP_SINGLE_FILE_PROBE.md", "aff4_apfs_exact_file_signature_scan.csv", "aff4_apfs_exact_file_signature_scan_summary.json", "AFF4_APFS_EXACT_FILE_SIGNATURE_SCAN.md", "evidence_source_readiness.csv", "reader_tool_readiness.csv", "source_probe_signatures.csv", "source_partition_probe.csv", "source_probe_summary.json", "image_inventory_readiness.csv", "active_file_comparison_readiness.csv", "image_file_inventory.csv",
            "evidence_sources.csv", "store_inventory.csv", "store_selection.csv", "ios_input_store_entry_inventory.csv", "ios_zip_entry_probe.csv", "ios_ffs_file_inventory.csv", "ios_app_database_inventory.csv", "EXPORT_INDEX.csv"
        };

        for (const auto& name : rootFiles) copyThinUploadFileIfAllowed(caseDir / name, uploadDir / name.filename(), name, manifest);
        const std::vector<fs::path> logFiles = {
            "run_status.txt", "last_stage.txt", "run_progress.tsv", "last_progress.tsv", "VestigantSpotlight.log"
        };
        for (const auto& name : logFiles) copyThinUploadFileIfAllowed(caseDir / "logs" / name, uploadDir / name.filename(), name, manifest);
        copyThinUploadFileIfAllowed(caseDir / "logs" / "FATAL_CRASH.txt", uploadDir / "logs" / "FATAL_CRASH.txt", fs::path("FATAL_CRASH.txt"), manifest, "NO_FATAL_CRASH_LOG");
        const fs::path exportsDir = caseDir / "exports";
        std::error_code exportEc;
        if (fs::exists(exportsDir, exportEc) && fs::is_directory(exportsDir, exportEc)) {
            for (const auto& entry : fs::directory_iterator(exportsDir, exportEc)) {
                if (exportEc) {
                    manifest << "ERROR," << pathString(exportsDir) << "," << pathString(uploadDir / "exports") << ",directory iterator failed\n";
                    break;
                }
                std::error_code fileEc;
                if (entry.is_regular_file(fileEc) && entry.path().extension() == ".csv") {
                    copyThinUploadExportCsvIfAllowed(entry.path(), uploadDir / "exports" / entry.path().filename(), fs::path("exports") / entry.path().filename(), manifest);
                }
            }
        } else {
            manifest << "MISSING_DIR," << pathString(exportsDir) << "," << pathString(uploadDir / "exports") << "\n";
        }
        copyDirectoryIfExists(caseDir / "exports" / "upload_samples", uploadDir / "exports" / "upload_samples", manifest);
        writeUploadReviewIndex(uploadDir / "review_index.html");
        manifest << "GENERATED," << pathString(uploadDir / "review_index.html") << "," << pathString(uploadDir / "review_index.html") << "\n";
        appendRunStatus(caseDir, "upload_bundle_complete", "Upload folder ready");

    } catch (...) {}
}

std::size_t countDistinctStoreGroups(const std::vector<StoreInfo>& stores, bool validOnly) {
    std::set<std::string> keys;
    for (const auto& s : stores) {
        if (validOnly && !s.isValid) continue;
        std::string key = toLower(pathString(s.storePath.parent_path()));
        if (key.empty()) key = toLower(s.storeGuid);
        if (!key.empty()) keys.insert(key);
    }
    return keys.size();
}

std::size_t countValidDatabaseCandidates(const std::vector<StoreInfo>& stores) {
    std::size_t n = 0;
    for (const auto& s : stores) if (s.isValid) ++n;
    return n;
}

bool hasExtensionInsensitive(const fs::path& p, const std::string& ext) {
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return e == ext;
}

bool isZipSourcePath(const fs::path& p) { return hasExtensionInsensitive(p, ".zip"); }
bool isAff4SourcePath(const fs::path& p) { return hasExtensionInsensitive(p, ".aff4"); }
bool isRawImageSourcePath(const fs::path& p) {
    return hasExtensionInsensitive(p, ".img") || hasExtensionInsensitive(p, ".dd") || hasExtensionInsensitive(p, ".raw");
}

std::string inputSourceType(const fs::path& p) {
    if (isZipSourcePath(p)) return "ZIP_SPOTLIGHT_OR_FILESYSTEM_CONTAINER";
    if (isAff4SourcePath(p)) return "AFF4_CONTAINER";
    if (isRawImageSourcePath(p)) return "RAW_FLAT_IMAGE";
    std::error_code ec;
    if (fs::is_directory(p, ec)) return "FOLDER_OR_EXTRACTED_FILESYSTEM_ROOT";
    if (fs::is_regular_file(p, ec)) return "UNRECOGNIZED_FILE_SOURCE";
    return "UNKNOWN_OR_MISSING_SOURCE";
}

std::string sourceTypeReaderStatus(const std::string& type) {
    if (type == "ZIP_SPOTLIGHT_OR_FILESYSTEM_CONTAINER") return "IMPLEMENTED_ZIP_EXTRACTION";
    if (type == "AFF4_CONTAINER") return "PLANNED_NOT_IMPLEMENTED_AFF4_READER";
    if (type == "RAW_FLAT_IMAGE") return "IMPLEMENTED_RAW_IMAGE_HEADER_PARTITION_PROBE_ONLY";
    if (type == "FOLDER_OR_EXTRACTED_FILESYSTEM_ROOT") return "NOT_REQUIRED_FOLDER_SOURCE";
    return "NOT_SUPPORTED";
}

std::string sourceTypeFilesystemStatus(const std::string& type) {
    if (type == "ZIP_SPOTLIGHT_OR_FILESYSTEM_CONTAINER") return "ZIP_EXTRACTED_THEN_SPOTLIGHT_DISCOVERY";
    if (type == "AFF4_CONTAINER") return "PRIORITY_AFF4_STREAM_READER_PLUS_APFS_ENUMERATION_REQUIRED";
    if (type == "RAW_FLAT_IMAGE") return "RAW_PARTITION_REPORTING_ONLY_AFF4_APFS_PATH_PRIORITIZED";
    if (type == "FOLDER_OR_EXTRACTED_FILESYSTEM_ROOT") return "DIRECT_FOLDER_SPOTLIGHT_DISCOVERY";
    return "NOT_AVAILABLE";
}

std::string discoveryStatusForStores(const std::vector<StoreInfo>& stores) {
    if (stores.empty()) return "NO_STORE_DATABASE_CANDIDATES";
    std::size_t valid = 0;
    for (const auto& st : stores) if (st.isValid) ++valid;
    if (valid == 0) return "STORE_CANDIDATES_FOUND_NONE_VALID";
    return "VALID_STORE_DATABASES_DISCOVERED";
}


struct SourceProbeHit {
    std::string name;
    std::string category;
    std::uint64_t offset = 0;
    std::string confidence;
    std::string notes;
};

struct SourceProbeFindings {
    std::vector<SourceProbeHit> hits;
    std::size_t bytesScanned = 0;
    std::uintmax_t fileSizeBytes = 0;
    bool scanTruncated = false;
};

struct PartitionProbeEntry {
    std::string scheme;
    int partitionIndex = 0;
    std::uint64_t startLba = 0;
    std::uint64_t sectorCount = 0;
    std::uint64_t offsetBytes = 0;
    std::uint64_t sizeBytes = 0;
    std::string typeCode;
    std::string typeGuid;
    std::string name;
    std::string filesystemHint;
    std::string confidence;
    std::string status;
    std::string notes;
};

struct PartitionProbeFindings {
    std::string scheme = "NONE_DETECTED";
    bool mbrSignatureFound = false;
    bool gptHeaderFound = false;
    std::vector<PartitionProbeEntry> entries;
};


struct ReaderToolStatus {
    std::string role;
    std::string toolName;
    std::string envVar;
    std::string status;
    fs::path resolvedPath;
    std::string priority;
    std::string purpose;
    std::string notes;
};


std::string getenvString(const char* name) {
    if (!name) return {};
#ifdef _WIN32
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) == 0 && value) {
        std::string out(value);
        std::free(value);
        return out;
    }
    if (value) std::free(value);
    return {};
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
#endif
}

void appendPathCandidates(std::vector<fs::path>& out, const fs::path& dir, const std::vector<std::string>& names) {
    if (dir.empty()) return;
    for (const auto& n : names) out.push_back(dir / n);
}

void appendReaderToolRootCandidates(std::vector<fs::path>& out, const fs::path& root, const std::vector<std::string>& names) {
    if (root.empty()) return;
    appendPathCandidates(out, root, names);
    appendPathCandidates(out, root / "x64" / "Release", names);
    appendPathCandidates(out, root / "x64", names);
    appendPathCandidates(out, root / "Release", names);
    appendPathCandidates(out, root / "bin", names);
    appendPathCandidates(out, root / "lib", names);
    appendPathCandidates(out, root / "include", names);
    appendPathCandidates(out, root / "src", names);
}

std::vector<fs::path> splitSearchPath(const std::string& searchPath) {
    std::vector<fs::path> dirs;
    if (searchPath.empty()) return dirs;
#ifdef _WIN32
    const char sep = ';';
#else
    const char sep = ':';
#endif
    std::stringstream ss(searchPath);
    std::string item;
    while (std::getline(ss, item, sep)) {
        if (!item.empty()) dirs.emplace_back(item);
    }
    return dirs;
}

fs::path findToolCandidate(const RunOptions& opt, const std::string& envVar, const std::vector<std::string>& names) {
    std::vector<fs::path> candidates;
    const std::string explicitTool = getenvString(envVar.c_str());
    if (!explicitTool.empty()) candidates.emplace_back(explicitTool);
    appendReaderToolRootCandidates(candidates, opt.readerToolsDir, names);
    const std::string readerRoot = getenvString("VESTIGANT_READER_TOOLS");
    if (!readerRoot.empty()) appendReaderToolRootCandidates(candidates, fs::path(readerRoot), names);
    const std::string aff4CppLiteRoot = getenvString("VESTIGANT_AFF4_CPP_LITE_ROOT");
    if (!aff4CppLiteRoot.empty()) appendReaderToolRootCandidates(candidates, fs::path(aff4CppLiteRoot), names);
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec) {
        appendPathCandidates(candidates, cwd / "tools" / "readers" / "win64", names);
        appendPathCandidates(candidates, cwd / "tools" / "readers", names);
        appendPathCandidates(candidates, cwd / "tools", names);
    }
    for (const auto& dir : splitSearchPath(getenvString("PATH"))) appendPathCandidates(candidates, dir, names);
    for (const auto& c : candidates) {
        std::error_code existsEc;
        if (fs::exists(c, existsEc) && fs::is_regular_file(c, existsEc)) return c;
    }
    return {};
}

std::vector<ReaderToolStatus> discoverReaderToolReadiness(const RunOptions& opt, const fs::path& originalInput) {
    const bool aff4 = isAff4SourcePath(originalInput);
    const bool raw = isRawImageSourcePath(originalInput);
    std::vector<ReaderToolStatus> tools;
    auto add = [&](const std::string& role,
                   const std::string& toolName,
                   const std::string& envVar,
                   std::vector<std::string> names,
                   const std::string& priority,
                   const std::string& purpose,
                   const std::string& missingStatus,
                   const std::string& notes) {
        ReaderToolStatus st;
        st.role = role;
        st.toolName = toolName;
        st.envVar = envVar;
        st.priority = priority;
        st.purpose = purpose;
        st.notes = notes;
        st.resolvedPath = findToolCandidate(opt, envVar, names);
        st.status = st.resolvedPath.empty() ? missingStatus : "FOUND_NOT_INVOKED";
        tools.push_back(st);
    };
#ifdef _WIN32
    const std::vector<std::string> aff4CppLiteInfoNames = {"aff4-info.exe", "aff4-info"};
    const std::vector<std::string> aff4CppLiteExtractNames = {"aff4-extract.exe", "aff4-extract"};
    const std::vector<std::string> aff4CppLiteDllNames = {"libaff4.dll"};
    const std::vector<std::string> aff4CppLiteLibNames = {"libaff4.lib"};
    const std::vector<std::string> aff4CppLiteManifestNames = {"reader_tools_manifest.csv"};
    const std::vector<std::string> aff4CppLiteHeaderNames = {"aff4-c.h", "aff4.h"};
    const std::vector<std::string> aff4CppLiteOpenSslCryptoNames = {"libeay32.dll", "libcrypto-1_1-x64.dll", "libcrypto-3-x64.dll"};
    const std::vector<std::string> aff4CppLiteOpenSslSslNames = {"ssleay32.dll", "libssl-1_1-x64.dll", "libssl-3-x64.dll"};
    const std::vector<std::string> aff4Names = {"aff4imager.exe", "aff4imager"};
    const std::vector<std::string> fsapfsInfoNames = {"fsapfsinfo.exe", "fsapfsinfo"};
    const std::vector<std::string> fsapfsMountNames = {"fsapfsmount.exe", "fsapfsmount"};
    const std::vector<std::string> fshfsInfoNames = {"fshfsinfo.exe", "fshfsinfo"};
    const std::vector<std::string> fshfsMountNames = {"fshfsmount.exe", "fshfsmount"};
#else
    const std::vector<std::string> aff4CppLiteInfoNames = {"aff4-info"};
    const std::vector<std::string> aff4CppLiteExtractNames = {"aff4-extract"};
    const std::vector<std::string> aff4CppLiteDllNames = {"libaff4.so", "libaff4.dylib"};
    const std::vector<std::string> aff4CppLiteLibNames = {"libaff4.a"};
    const std::vector<std::string> aff4CppLiteManifestNames = {"reader_tools_manifest.csv"};
    const std::vector<std::string> aff4CppLiteHeaderNames = {"aff4-c.h", "aff4.h"};
    const std::vector<std::string> aff4CppLiteOpenSslCryptoNames = {"libcrypto.so", "libcrypto.dylib"};
    const std::vector<std::string> aff4CppLiteOpenSslSslNames = {"libssl.so", "libssl.dylib"};
    const std::vector<std::string> aff4Names = {"aff4imager"};
    const std::vector<std::string> fsapfsInfoNames = {"fsapfsinfo"};
    const std::vector<std::string> fsapfsMountNames = {"fsapfsmount"};
    const std::vector<std::string> fshfsInfoNames = {"fshfsinfo"};
    const std::vector<std::string> fshfsMountNames = {"fshfsmount"};
#endif
    add("AFF4_CPP_LITE_INFO_TOOL", "aff4-info", "VESTIGANT_AFF4_CPP_LITE_INFO", aff4CppLiteInfoNames,
        "OPTIONAL_DIAGNOSTIC_ONLY",
        "Optional aff4-cpp-lite metadata helper. It is not required for the default Vestigant library-integration path.",
        "MISSING_OPTIONAL_DIAGNOSTIC_TOOL",
        "Not built by the default dependency-chain helper; keep optional so missing examples/tools do not block libaff4 integration.");
    add("AFF4_CPP_LITE_EXTRACT_TOOL", "aff4-extract", "VESTIGANT_AFF4_CPP_LITE_EXTRACT", aff4CppLiteExtractNames,
        "FALLBACK_EXPLICIT_ONLY",
        "Fallback troubleshooting utility. Do not use as the normal workflow because it exports the first image to RAW/DD.",
        "MISSING_OPTIONAL_FALLBACK",
        "Retained only for diagnostic fallback. Vestigant target architecture is direct AFF4 random-access reads.");
    add("AFF4_CPP_LITE_MANIFEST", "reader_tools_manifest.csv", "VESTIGANT_AFF4_CPP_LITE_MANIFEST", aff4CppLiteManifestNames,
        aff4 ? "REQUIRED_FOR_READER_TOOLS_AUDIT" : "OPTIONAL",
        "Manifest written by the dependency-chain helper listing copied headers, libraries, and runtime DLLs.",
        aff4 ? "MISSING_READER_TOOLS_MANIFEST" : "MISSING_OPTIONAL",
        "Used to audit the reader-tools folder without uploading binaries.");
    add("AFF4_CPP_LITE_C_HEADER", "aff4-c.h", "VESTIGANT_AFF4_CPP_LITE_HEADER", aff4CppLiteHeaderNames,
        aff4 ? "REQUIRED_FOR_DYNAMIC_LOAD_OR_LINK_PROBE" : "OPTIONAL",
        "C-facing header/API needed for the Vestigant AFF4 block-reader shim.",
        aff4 ? "MISSING_AFF4_C_HEADER" : "MISSING_OPTIONAL",
        "Expected under the reader-tools include folder after the dependency-chain build.");
    add("AFF4_CPP_LITE_LIBRARY_DLL", "libaff4.dll", "VESTIGANT_LIBAFF4_DLL", aff4CppLiteDllNames,
        aff4 ? "REQUIRED_FOR_DIRECT_RANDOM_ACCESS" : "OPTIONAL",
        "Runtime DLL for linking a Vestigant AFF4 virtual block reader against aff4-cpp-lite.",
        aff4 ? "MISSING_LIBAFF4_DLL" : "MISSING_OPTIONAL",
        "Needed for direct library integration after the aff4-cpp-lite Windows build succeeds.");
    add("AFF4_CPP_LITE_LIBRARY_IMPORT_LIB", "libaff4.lib", "VESTIGANT_LIBAFF4_LIB", aff4CppLiteLibNames,
        aff4 ? "REQUIRED_FOR_MSVC_LINK" : "OPTIONAL",
        "MSVC import library for linking Vestigant to libaff4.dll.",
        aff4 ? "MISSING_LIBAFF4_IMPORT_LIB" : "MISSING_OPTIONAL",
        "Needed for compile/link integration; can be supplied through --reader-tools or environment variable.");
    add("AFF4_CPP_LITE_DEPENDENCY_ZLIB_DLL", "zlib1", "VESTIGANT_ZLIB1_DLL", std::vector<std::string>{"zlib1.dll", "zlib1.so", "zlib1.dylib"},
        aff4 ? "REQUIRED_RUNTIME_DEPENDENCY" : "OPTIONAL",
        "zlib runtime library produced by the aff4-cpp-lite dependency-chain build.",
        aff4 ? "MISSING_ZLIB1_RUNTIME" : "MISSING_OPTIONAL",
        "Expected under the reader-tools x64\\Release folder after running the AFF4 CPP Lite build helper.");
    add("AFF4_CPP_LITE_DEPENDENCY_SNAPPY_DLL", "snappy", "VESTIGANT_SNAPPY_DLL", std::vector<std::string>{"snappy.dll", "libsnappy.so", "libsnappy.dylib"},
        aff4 ? "REQUIRED_RUNTIME_DEPENDENCY" : "OPTIONAL",
        "snappy runtime library produced by the aff4-cpp-lite dependency-chain build.",
        aff4 ? "MISSING_SNAPPY_RUNTIME" : "MISSING_OPTIONAL",
        "Expected under the reader-tools x64\\Release folder after running the AFF4 CPP Lite build helper.");
    add("AFF4_CPP_LITE_DEPENDENCY_RAPTOR2_DLL", "raptor2", "VESTIGANT_RAPTOR2_DLL", std::vector<std::string>{"raptor2.dll", "libraptor2.so", "libraptor2.dylib"},
        aff4 ? "REQUIRED_RUNTIME_DEPENDENCY" : "OPTIONAL",
        "raptor2 runtime library produced by the aff4-cpp-lite dependency-chain build.",
        aff4 ? "MISSING_RAPTOR2_RUNTIME" : "MISSING_OPTIONAL",
        "Expected under the reader-tools x64\\Release folder after running the AFF4 CPP Lite build helper.");
    add("AFF4_CPP_LITE_DEPENDENCY_OPENSSL_CRYPTO_DLL", "OpenSSL crypto", "VESTIGANT_LIBEAY32_DLL", aff4CppLiteOpenSslCryptoNames,
        aff4 ? "REQUIRED_RUNTIME_DEPENDENCY" : "OPTIONAL",
        "OpenSSL crypto runtime dependency copied from the aff4-cpp-lite build output.",
        aff4 ? "MISSING_OPENSSL_CRYPTO_RUNTIME" : "MISSING_OPTIONAL",
        "Legacy aff4-cpp-lite Windows builds commonly produce libeay32.dll plus ssleay32.dll.");
    add("AFF4_CPP_LITE_DEPENDENCY_OPENSSL_SSL_DLL", "OpenSSL ssl", "VESTIGANT_SSLEAY32_DLL", aff4CppLiteOpenSslSslNames,
        aff4 ? "REQUIRED_RUNTIME_DEPENDENCY" : "OPTIONAL",
        "OpenSSL SSL runtime dependency copied from the aff4-cpp-lite build output.",
        aff4 ? "MISSING_OPENSSL_SSL_RUNTIME" : "MISSING_OPTIONAL",
        "Legacy aff4-cpp-lite Windows builds commonly produce libeay32.dll plus ssleay32.dll.");
    add("AFF4_CONTAINER_READER_FALLBACK", "aff4imager", "VESTIGANT_AFF4IMAGER", aff4Names,
        aff4 ? "FALLBACK_ONLY" : "OPTIONAL",
        "Fallback AFF4 stream-list/export tool if aff4-cpp-lite cannot be built or linked.",
        aff4 ? "MISSING_FALLBACK_AFF4IMAGER" : "MISSING_OPTIONAL",
        "Fallback only. Current project direction is aff4-cpp-lite random-access reading, not WinPmem/c-aff4 export.");
    add("APFS_INFO_READER", "fsapfsinfo", "VESTIGANT_FSAPFSINFO", fsapfsInfoNames,
        (aff4 || raw) ? "REQUIRED_AFTER_STREAM_OR_PARTITION_EXPORT" : "OPTIONAL",
        "Inspect APFS containers/volumes after AFF4 stream extraction or raw partition offset selection.",
        (aff4 || raw) ? "MISSING_REQUIRED_FOR_APFS_ENUMERATION" : "MISSING_OPTIONAL",
        "libfsapfs is experimental; Vestigant must record reader limitations in case outputs.");
    add("APFS_MOUNT_OR_EXPORT_READER", "fsapfsmount", "VESTIGANT_FSAPFSMOUNT", fsapfsMountNames,
        (aff4 || raw) ? "OPTIONAL_EXPORT_PATH" : "OPTIONAL",
        "Potential controlled read-only APFS file access/export path for staging Spotlight artifacts.",
        "MISSING_OPTIONAL",
        "Mount-style access is optional and should not be the only architecture path.");
    add("HFS_INFO_READER", "fshfsinfo", "VESTIGANT_FSHFSINFO", fshfsInfoNames,
        "LOWER_PRIORITY_FALLBACK",
        "Inspect HFS+/HFSX volumes if APFS is not present or legacy evidence requires it.",
        "MISSING_OPTIONAL_HFS_FALLBACK",
        "HFS/HFS+ support remains secondary to AFF4/APFS.");
    add("HFS_MOUNT_OR_EXPORT_READER", "fshfsmount", "VESTIGANT_FSHFSMOUNT", fshfsMountNames,
        "LOWER_PRIORITY_FALLBACK",
        "Potential controlled read-only HFS+/HFSX file access/export path for legacy evidence.",
        "MISSING_OPTIONAL_HFS_FALLBACK",
        "HFS/HFS+ support remains secondary to AFF4/APFS.");
    return tools;
}

void writeReaderToolReadinessArtifacts(const fs::path& caseDir,
                                       const RunOptions& opt,
                                       const EvidenceSource& source,
                                       const fs::path& originalInput,
                                       const SourceProbeFindings& probe,
                                       Logger& log) {
    try {
        const auto tools = discoverReaderToolReadiness(opt, originalInput);
        const fs::path csv = caseDir / "reader_tool_readiness.csv";
        {
            std::ofstream out(csv, std::ios::binary);
            out << "source_id,input_path,input_type,role,tool_name,env_var,status,resolved_path,priority,purpose,notes\n";
            for (const auto& t : tools) {
                out << csvEscape(source.sourceId) << ','
                    << csvEscape(pathString(originalInput)) << ','
                    << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(t.role) << ','
                    << csvEscape(t.toolName) << ','
                    << csvEscape(t.envVar) << ','
                    << csvEscape(t.status) << ','
                    << csvEscape(pathString(t.resolvedPath)) << ','
                    << csvEscape(t.priority) << ','
                    << csvEscape(t.purpose) << ','
                    << csvEscape(t.notes) << "\n";
            }
        }
        const fs::path md = caseDir / "AFF4_APFS_READER_PLAN.md";
        {
            std::ofstream out(md, std::ios::binary);
            out << "# AFF4/APFS Reader Integration Plan\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Current source\n\n";
            out << "- Source ID: `" << source.sourceId << "`\n";
            out << "- Input path: `" << pathString(originalInput) << "`\n";
            out << "- Input type: `" << inputSourceType(originalInput) << "`\n";
            out << "- Probe bytes scanned: `" << probe.bytesScanned << "`\n";
            out << "- Probe signatures found: `" << probe.hits.size() << "`\n\n";
            out << "## Hash policy for ongoing AFF4/APFS testing\n\n";
            out << "Source-probe development runs should defer full-container hashing unless the run is specifically intended to verify source integrity. Use `--force-container-hash` when a full evidentiary hash is required. The stable ZIP/folder Spotlight parser path does not need to be rerun unless parser, enrichment, or export logic changed.\n\n";
            out << "## Tool readiness\n\n";
            out << "| Role | Tool | Status | Resolved path | Priority | Purpose |\n";
            out << "|---|---|---|---|---|---|\n";
            for (const auto& t : tools) {
                out << "| " << t.role << " | `" << t.toolName << "` | " << t.status << " | `" << pathString(t.resolvedPath) << "` | " << t.priority << " | " << t.purpose << " |\n";
            }
            out << "\n## Next implementation sequence\n\n";
            out << "1. Build or supply aff4-cpp-lite reader outputs through `--reader-tools`, `VESTIGANT_AFF4_CPP_LITE_ROOT`, `VESTIGANT_READER_TOOLS`, or `PATH`.\n";
            out << "2. Validate `libaff4.dll`, `libaff4.lib`, `zlib1`, `snappy`, and `raptor2` readiness before direct random-access integration.\n";
            out << "3. Export a selected AFF4 stream or expose stream-backed reads to APFS/HFS probing.\n";
            out << "4. Use APFS as the priority filesystem path and HFS/HFS+ as fallback.\n";
            out << "5. Populate `image_file_inventory` from image-backed filesystem enumeration.\n";
            out << "6. Join Spotlight artifacts to `image_file_inventory` for active/present/missing comparison.\n\n";
            out << "## External-tool search order\n\n";
            out << "1. Explicit per-tool environment variables such as `VESTIGANT_AFF4IMAGER` and `VESTIGANT_FSAPFSINFO`.\n";
            out << "2. `--reader-tools <folder>`.\n";
            out << "3. `VESTIGANT_READER_TOOLS`.\n";
            out << "4. Local `tools/readers/win64`, `tools/readers`, and `tools` folders under the current working directory.\n";
            out << "5. System `PATH`.\n\n";
            out << "## Current stream-inventory harness\n\n";
            out << "V0_8_23 treats aff4-cpp-lite as the primary AFF4 direction and WinPmem/c-aff4 `aff4imager` as fallback only. No full AFF4-to-RAW export is performed by default. The next executable milestone is direct random-access reads from AFF4 into the APFS/HFS probe layer.\n";
        }
        {
            const fs::path liteCsv = caseDir / "aff4_cpp_lite_reader_readiness.csv";
            std::ofstream out(liteCsv, std::ios::binary);
            out << "source_id,input_path,input_type,component,status,resolved_path,interface_or_role,notes\n";
            for (const auto& t : tools) {
                if (t.role.rfind("AFF4_CPP_LITE", 0) != 0) continue;
                out << csvEscape(source.sourceId) << ','
                    << csvEscape(pathString(originalInput)) << ','
                    << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(t.toolName) << ','
                    << csvEscape(t.status) << ','
                    << csvEscape(pathString(t.resolvedPath)) << ','
                    << csvEscape(t.role) << ','
                    << csvEscape(t.notes) << "\n";
            }
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ',' << csvEscape("AFF4_open") << ',' << csvEscape("SOURCE_API_CONFIRMED") << ',' << csvEscape("") << ',' << csvEscape("C_API_OPEN_FIRST_IMAGE") << ',' << csvEscape("aff4-cpp-lite C API opens the first AFF4 image in a container using a UTF-8 filename.") << "\n";
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ',' << csvEscape("AFF4_object_size") << ',' << csvEscape("SOURCE_API_CONFIRMED") << ',' << csvEscape("") << ',' << csvEscape("C_API_SIZE") << ',' << csvEscape("Reports virtual image size for the opened object.") << "\n";
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ',' << csvEscape("AFF4_read") << ',' << csvEscape("SOURCE_API_CONFIRMED") << ',' << csvEscape("") << ',' << csvEscape("C_API_RANDOM_ACCESS_READ") << ',' << csvEscape("Reads from an explicit offset into caller buffer; this is the no-full-export bridge for filesystem probing.") << "\n";
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ',' << csvEscape("AFF4_close") << ',' << csvEscape("SOURCE_API_CONFIRMED") << ',' << csvEscape("") << ',' << csvEscape("C_API_CLOSE") << ',' << csvEscape("Closes the opened AFF4 object handle.") << "\n";
        }
        {
            const fs::path integrationCsv = caseDir / "aff4_cpp_lite_integration_readiness.csv";
            std::map<std::string, ReaderToolStatus> byRole;
            for (const auto& t : tools) byRole[t.role] = t;
            const std::vector<std::string> requiredRoles = {
                "AFF4_CPP_LITE_LIBRARY_DLL",
                "AFF4_CPP_LITE_LIBRARY_IMPORT_LIB",
                "AFF4_CPP_LITE_DEPENDENCY_ZLIB_DLL",
                "AFF4_CPP_LITE_DEPENDENCY_SNAPPY_DLL",
                "AFF4_CPP_LITE_DEPENDENCY_RAPTOR2_DLL",
                "AFF4_CPP_LITE_DEPENDENCY_OPENSSL_CRYPTO_DLL",
                "AFF4_CPP_LITE_DEPENDENCY_OPENSSL_SSL_DLL",
                "AFF4_CPP_LITE_C_HEADER",
                "AFF4_CPP_LITE_MANIFEST"
            };
            bool ready = true;
            for (const auto& role : requiredRoles) {
                const auto it = byRole.find(role);
                if (it == byRole.end() || it->second.resolvedPath.empty()) ready = false;
            }
            std::ofstream out(integrationCsv, std::ios::binary);
            out << "source_id,input_path,input_type,overall_status,component_role,component_name,component_status,resolved_path,blocks_next_step,next_action,notes\n";
            const std::string overall = ready ? "READY_FOR_LIBAFF4_DYNAMIC_LOAD_PROBE" : "MISSING_REQUIRED_LIBAFF4_COMPONENTS";
            for (const auto& role : requiredRoles) {
                const auto it = byRole.find(role);
                ReaderToolStatus t;
                if (it != byRole.end()) t = it->second;
                const bool missing = t.resolvedPath.empty();
                out << csvEscape(source.sourceId) << ','
                    << csvEscape(pathString(originalInput)) << ','
                    << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(overall) << ','
                    << csvEscape(role) << ','
                    << csvEscape(t.toolName.empty() ? role : t.toolName) << ','
                    << csvEscape(t.status.empty() ? "NOT_REPORTED" : t.status) << ','
                    << csvEscape(pathString(t.resolvedPath)) << ','
                    << csvEscape(missing ? "YES" : "NO") << ','
                    << csvEscape(missing ? "Supply or rebuild the AFF4 CPP Lite reader-tools dependency chain before implementing the dynamic-load probe." : "Use this component for the next libaff4 dynamic-load/block-reader probe.") << ','
                    << csvEscape(t.notes) << "\n";
            }
        }

        {
            const fs::path liteMd = caseDir / "AFF4_CPP_LITE_RANDOM_ACCESS_PLAN.md";
            std::ofstream out(liteMd, std::ios::binary);
            out << "# AFF4 CPP Lite Random-Access Reader Plan\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Source\n\n";
            out << "- Source ID: `" << source.sourceId << "`\n";
            out << "- Input path: `" << pathString(originalInput) << "`\n";
            out << "- Input type: `" << inputSourceType(originalInput) << "`\n";
            out << "- Probe bytes scanned: `" << probe.bytesScanned << "`\n";
            out << "- Probe signatures found: `" << probe.hits.size() << "`\n\n";
            out << "## Direction\n\n";
            out << "Use `aff4-cpp-lite` as the primary AFF4 backend. WinPmem/c-aff4 `aff4imager` remains fallback only. The normal workflow must not create a full RAW/DD image from the AFF4.\n\n";
            out << "## Required aff4-cpp-lite interfaces\n\n";
            out << "| Interface | Planned Vestigant use |\n";
            out << "|---|---|\n";
            out << "| `AFF4_open(const char*)` | Open the AFF4 container and select the first image through the C API. |\n";
            out << "| `AFF4_object_size(handle)` | Determine virtual image size for bounded probes and partition/APFS/HFS readers. |\n";
            out << "| `AFF4_object_blocksize(handle)` | Record preferred block size where available. |\n";
            out << "| `AFF4_read(handle, offset, buffer, length)` | Read arbitrary byte ranges into a Vestigant virtual block-reader without exporting full RAW/DD. |\n";
            out << "| `AFF4_close(handle)` | Close the opened AFF4 object handle. |\n\n";
            out << "## APFS/HFS bridge\n\n";
            out << "`libfsapfs` and `libfshfs` both use `libbfio` handle-based open paths, so the likely bridge is an AFF4-backed `libbfio_handle_t` or a small block-cache adapter, not a full raw-image export.\n\n";
            out << "## Next implementation steps\n\n";
            out << "1. Build aff4-cpp-lite x64 Release with a Visual Studio/MSBuild path that does not require `msbuild` to already be in PATH.\n";
            out << "2. Copy `libaff4.dll`, `libaff4.lib`, public headers, and optional `aff4-info.exe` into `T:\\VestigantReaderTools\\aff4-cpp-lite\\x64\\Release`.\n";
            out << "3. Add `Aff4CppLiteBlockReader` inside Vestigant using dynamic loading or explicit MSVC linking.\n";
            out << "4. Reuse the existing source-probe scanner against the AFF4-backed block reader.\n";
            out << "5. Feed the same block-reader into APFS first and HFS/HFS+ fallback readers.\n";
            out << "6. Populate `image_file_inventory` from the filesystem reader and join it to Spotlight artifacts for present/missing/stale comparison.\n\n";
            out << "## Build helper\n\n";
            out << "This source package includes `tools/Build-Aff4CppLite-VS2022.ps1`, which locates Visual Studio/MSBuild and runs the legacy `win32\\libaff4.sln` without requiring a Developer Prompt.\n\n";
            out << "## Current limitation\n\n";
            out << "This build records readiness and API planning only. It does not yet link to `libaff4.dll`, enumerate APFS/HFS filesystems, or export Spotlight artifacts from the image.\n";
        }

        log.info("AFF4/APFS reader tool readiness written: " + pathString(csv));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write reader tool readiness artifacts: ") + ex.what());
    } catch (...) {
        log.warn("Unable to write reader tool readiness artifacts: unknown error");
    }
}



bool hasProbeHit(const SourceProbeFindings& f, const std::string& name);
bool bytesAt(const std::vector<unsigned char>& data, std::size_t off, const char* needle, std::size_t n);

std::uint16_t readLe16(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 2 > data.size()) return 0;
    return static_cast<std::uint16_t>(data[off]) | (static_cast<std::uint16_t>(data[off + 1]) << 8);
}

std::uint32_t readLe32(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 4 > data.size()) return 0;
    return static_cast<std::uint32_t>(data[off]) |
           (static_cast<std::uint32_t>(data[off + 1]) << 8) |
           (static_cast<std::uint32_t>(data[off + 2]) << 16) |
           (static_cast<std::uint32_t>(data[off + 3]) << 24);
}

std::uint64_t readLe64(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 8 > data.size()) return 0;
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | static_cast<std::uint64_t>(data[off + static_cast<std::size_t>(i)]);
    }
    return v;
}

struct ApfsInodeExtendedFieldDecode {
    bool sawXfields = false;
    bool sawDstream = false;
    std::uint64_t dstreamSize = 0;
    std::uint64_t dstreamAllocedSize = 0;
    std::uint64_t dstreamDefaultCryptoId = 0;
    std::string status;
    std::string notes;
};

std::size_t alignUp8ForApfsXfield(std::size_t value) {
    return (value + 7U) & ~static_cast<std::size_t>(7U);
}

ApfsInodeExtendedFieldDecode decodeApfsInodeExtendedFieldsForProbe(const std::vector<unsigned char>& node,
                                                                    std::size_t valueAbs,
                                                                    std::size_t valueLen) {
    ApfsInodeExtendedFieldDecode d;
    constexpr std::size_t kJInodeValFixedSize = 92U;
    constexpr std::uint8_t kInoExtTypeDstream = 8U;
    if (valueLen <= kJInodeValFixedSize) {
        d.status = "NO_INODE_XFIELDS";
        return d;
    }
    if (valueAbs > node.size() || node.size() - valueAbs < valueLen) {
        d.status = "INODE_VALUE_BOUNDS_INVALID";
        return d;
    }
    const std::size_t xbase = valueAbs + kJInodeValFixedSize;
    const std::size_t xlen = valueLen - kJInodeValFixedSize;
    if (xlen < 4U || xbase > node.size() || node.size() - xbase < xlen) {
        d.status = "INODE_XFIELDS_TOO_SHORT";
        return d;
    }
    d.sawXfields = true;
    const std::uint16_t numExts = readLe16(node, xbase + 0U);
    const std::uint16_t usedData = readLe16(node, xbase + 2U);
    const std::size_t tableBytes = static_cast<std::size_t>(numExts) * 4U;
    if (numExts == 0) {
        d.status = "INODE_XFIELDS_EMPTY";
        return d;
    }
    if (4U + tableBytes > xlen) {
        d.status = "INODE_XFIELD_TABLE_OUT_OF_BOUNDS";
        d.notes = "num_exts=" + std::to_string(numExts) + "; used_data=" + std::to_string(usedData);
        return d;
    }
    if (usedData != 0 && static_cast<std::size_t>(usedData) > xlen) {
        d.status = "INODE_XFIELD_USED_DATA_OUT_OF_BOUNDS";
        d.notes = "num_exts=" + std::to_string(numExts) + "; used_data=" + std::to_string(usedData);
        return d;
    }

    struct XfEntryForProbe {
        std::uint8_t type = 0;
        std::uint8_t flags = 0;
        std::uint16_t size = 0;
    };
    std::vector<XfEntryForProbe> fields;
    fields.reserve(numExts);
    for (std::uint16_t i = 0; i < numExts; ++i) {
        const std::size_t fieldAbs = xbase + 4U + static_cast<std::size_t>(i) * 4U;
        if (fieldAbs + 4U > node.size()) {
            d.status = "INODE_XFIELD_ENTRY_OUT_OF_BOUNDS";
            return d;
        }
        XfEntryForProbe xf;
        xf.type = node[fieldAbs + 0U];
        xf.flags = node[fieldAbs + 1U];
        xf.size = readLe16(node, fieldAbs + 2U);
        fields.push_back(xf);
    }

    // V0_8_62: APFS extended-field payload alignment in the wild can be easy to
    // mis-handle because xf_blob_t has a 4-byte header followed by xf_data[],
    // and the x_field_t metadata and payload arrays are aligned to eight-byte
    // boundaries. Try several bounded layout interpretations and accept the
    // first plausible INO_EXT_TYPE_DSTREAM. This is a read-only forensic probe:
    // failed candidates are recorded as diagnostics, not fatal parser errors.
    struct LayoutCandidate {
        const char* name;
        std::size_t startRel;
        bool alignRelativeToXfData;
        bool alignEachField;
    };
    const std::vector<LayoutCandidate> layouts = {
        {"xfdata_table_aligned", 4U + alignUp8ForApfsXfield(tableBytes), true, true},
        {"blob_table_aligned", alignUp8ForApfsXfield(4U + tableBytes), false, true},
        {"xfdata_table_raw", 4U + tableBytes, true, true},
        {"blob_table_raw", 4U + tableBytes, false, true},
        {"xfdata_table_aligned_no_each", 4U + alignUp8ForApfsXfield(tableBytes), true, false},
        {"blob_table_aligned_no_each", alignUp8ForApfsXfield(4U + tableBytes), false, false}
    };

    auto alignRel = [](std::size_t rel, bool relativeToXfData) -> std::size_t {
        if (!relativeToXfData) return alignUp8ForApfsXfield(rel);
        if (rel < 4U) return 4U;
        return 4U + alignUp8ForApfsXfield(rel - 4U);
    };

    std::vector<std::string> failedLayouts;
    for (const auto& layout : layouts) {
        std::size_t dataRel = layout.startRel;
        bool layoutBoundsOk = true;
        std::string layoutFailure;
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (layout.alignEachField) dataRel = alignRel(dataRel, layout.alignRelativeToXfData);
            const auto& field = fields[i];
            if (dataRel > xlen || static_cast<std::size_t>(field.size) > xlen - dataRel) {
                layoutBoundsOk = false;
                layoutFailure = std::string(layout.name) + ":field_index=" + std::to_string(i) + ";type=" + std::to_string(field.type) + ";size=" + std::to_string(field.size) + ";data_rel=" + std::to_string(dataRel);
                break;
            }
            const std::size_t dataAbs = xbase + dataRel;
            if (field.type == kInoExtTypeDstream && field.size >= 40U && dataAbs + 40U <= node.size()) {
                const std::uint64_t candidateSize = readLe64(node, dataAbs + 0U);
                const std::uint64_t candidateAlloced = readLe64(node, dataAbs + 8U);
                const std::uint64_t candidateCrypto = readLe64(node, dataAbs + 16U);
                // A zero-size dstream is valid for empty files, but for trimming
                // copy-out we only persist nonzero logical sizes that fit in the
                // decoded allocated-size. Keep the row status regardless.
                if (candidateAlloced == 0 || candidateSize <= candidateAlloced || candidateSize < (1ULL << 62)) {
                    d.sawDstream = true;
                    d.dstreamSize = candidateSize;
                    d.dstreamAllocedSize = candidateAlloced;
                    d.dstreamDefaultCryptoId = candidateCrypto;
                    d.status = "INODE_XFIELDS_DSTREAM_DECODED";
                    d.notes = "num_exts=" + std::to_string(numExts) + "; used_data=" + std::to_string(usedData) + "; layout=" + layout.name;
                    return d;
                }
            }
            dataRel += static_cast<std::size_t>(field.size);
        }
        if (!layoutBoundsOk) failedLayouts.push_back(layoutFailure);
    }

    d.status = "INODE_XFIELDS_NO_DSTREAM";
    d.notes = "num_exts=" + std::to_string(numExts) + "; used_data=" + std::to_string(usedData);
    if (!failedLayouts.empty()) {
        d.notes += "; layout_failures=";
        for (std::size_t i = 0; i < failedLayouts.size() && i < 3U; ++i) {
            if (i) d.notes += " | ";
            d.notes += failedLayouts[i];
        }
    }
    return d;
}

std::string hexByte(unsigned int v) {
    const char* d = "0123456789ABCDEF";
    std::string s;
    s.push_back(d[(v >> 4) & 0xF]);
    s.push_back(d[v & 0xF]);
    return s;
}

std::string guidFromGptBytes(const unsigned char* b) {
    if (!b) return {};
    auto hx = [](unsigned char c) { return hexByte(static_cast<unsigned int>(c)); };
    std::string out;
    out += hx(b[3]); out += hx(b[2]); out += hx(b[1]); out += hx(b[0]); out += '-';
    out += hx(b[5]); out += hx(b[4]); out += '-';
    out += hx(b[7]); out += hx(b[6]); out += '-';
    out += hx(b[8]); out += hx(b[9]); out += '-';
    for (int i = 10; i < 16; ++i) out += hx(b[i]);
    return out;
}

bool allZeroBytes(const unsigned char* b, std::size_t n) {
    if (!b) return true;
    for (std::size_t i = 0; i < n; ++i) if (b[i] != 0) return false;
    return true;
}

std::string utf16LeNameToAscii(const std::vector<unsigned char>& data, std::size_t off, std::size_t maxBytes) {
    std::string out;
    const std::size_t end = std::min<std::size_t>(data.size(), off + maxBytes);
    for (std::size_t p = off; p + 1 < end; p += 2) {
        const std::uint16_t ch = readLe16(data, p);
        if (ch == 0) break;
        if (ch >= 32 && ch <= 126) out.push_back(static_cast<char>(ch));
        else if (ch == '\t') out.push_back(' ');
        else out.push_back('?');
        if (out.size() >= 128) break;
    }
    return out;
}

std::string describeGptTypeGuid(const std::string& guid) {
    const std::string g = toLower(guid);
    if (g == "7c3457ef-0000-11aa-aa11-00306543ecac") return "APPLE_APFS";
    if (g == "48465300-0000-11aa-aa11-00306543ecac") return "APPLE_HFS_HFSPLUS";
    if (g == "426f6f74-0000-11aa-aa11-00306543ecac") return "APPLE_BOOT";
    if (g == "53746f72-6167-11aa-aa11-00306543ecac") return "APPLE_CORE_STORAGE";
    if (g == "00000000-0000-0000-0000-000000000000") return "UNUSED";
    return guid;
}

std::string describeMbrType(unsigned int type) {
    switch (type) {
        case 0x00: return "UNUSED";
        case 0x07: return "NTFS_EXFAT_OR_HPFS";
        case 0x0B: return "FAT32_CHS";
        case 0x0C: return "FAT32_LBA";
        case 0x0E: return "FAT16_LBA";
        case 0xEE: return "GPT_PROTECTIVE_MBR";
        case 0xAF: return "APPLE_HFS_HFSPLUS";
        case 0xAB: return "APPLE_BOOT";
        default: return std::string("MBR_TYPE_0x") + hexByte(type);
    }
}

std::vector<unsigned char> readBytesAt(const fs::path& p, std::uint64_t off, std::size_t count) {
    std::vector<unsigned char> buf(count, 0);
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    in.seekg(static_cast<std::streamoff>(off), std::ios::beg);
    if (!in) return {};
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    const std::size_t got = static_cast<std::size_t>(std::max<std::streamsize>(0, in.gcount()));
    buf.resize(got);
    return buf;
}

std::string detectFilesystemAtByteOffset(const fs::path& input, std::uint64_t partitionOffset, std::uintmax_t fileSize) {
    if (partitionOffset >= fileSize) return {};
    auto first = readBytesAt(input, partitionOffset, 4096);
    if (first.empty()) return {};
    if (bytesAt(first, 32, "NXSB", 4)) return "APFS_NXSB_AT_PARTITION_PLUS_32";
    if (bytesAt(first, 1024, "H+", 2)) return "HFS_PLUS_AT_PARTITION_PLUS_1024";
    if (bytesAt(first, 1024, "HX", 2)) return "HFSX_AT_PARTITION_PLUS_1024";
    if (bytesAt(first, 0, "ER", 2)) return "HFS_WRAPPER_OR_LEGACY_ER_AT_PARTITION_START";
    return {};
}

std::uint64_t safeMul64(std::uint64_t a, std::uint64_t b) {
    if (a == 0 || b == 0) return 0;
    if (a > (std::numeric_limits<std::uint64_t>::max)() / b) return (std::numeric_limits<std::uint64_t>::max)();
    return a * b;
}

PartitionProbeFindings probeRawImagePartitions(const fs::path& input, const SourceProbeFindings& signatures, Logger& log) {
    PartitionProbeFindings result;
    std::error_code ec;
    if (!fs::exists(input, ec) || !fs::is_regular_file(input, ec)) return result;
    const std::uintmax_t fileSize = fs::file_size(input, ec);
    if (ec || fileSize < 512) return result;
    const std::uint64_t sectorSize = 512;
    auto sector0 = readBytesAt(input, 0, 512);
    if (sector0.size() >= 512 && bytesAt(sector0, 510, "\x55\xAA", 2)) {
        result.mbrSignatureFound = true;
        result.scheme = "MBR_OR_PROTECTIVE_MBR";
        for (int i = 0; i < 4; ++i) {
            const std::size_t e = 446 + static_cast<std::size_t>(i) * 16;
            const unsigned int type = sector0[e + 4];
            const std::uint32_t start = readLe32(sector0, e + 8);
            const std::uint32_t sectors = readLe32(sector0, e + 12);
            if (type == 0 && start == 0 && sectors == 0) continue;
            PartitionProbeEntry pe;
            pe.scheme = "MBR";
            pe.partitionIndex = i + 1;
            pe.startLba = start;
            pe.sectorCount = sectors;
            pe.offsetBytes = safeMul64(start, sectorSize);
            pe.sizeBytes = safeMul64(sectors, sectorSize);
            pe.typeCode = describeMbrType(type);
            pe.typeGuid = "";
            pe.name = "";
            pe.filesystemHint = detectFilesystemAtByteOffset(input, pe.offsetBytes, fileSize);
            pe.confidence = type == 0xEE ? "MEDIUM_PROTECTIVE_MBR" : "MEDIUM";
            pe.status = "PARTITION_ENTRY_REPORTED_NOT_EXTRACTED";
            pe.notes = "MBR partition entry parsed for readiness/provenance only. Filesystem enumeration/extraction is not implemented in this build.";
            result.entries.push_back(pe);
        }
    }

    auto gptHeader = readBytesAt(input, 512, 512);
    if (gptHeader.size() >= 92 && bytesAt(gptHeader, 0, "EFI PART", 8)) {
        result.gptHeaderFound = true;
        result.scheme = "GPT";
        const std::uint64_t entryLba = readLe64(gptHeader, 72);
        std::uint32_t numEntries = readLe32(gptHeader, 80);
        std::uint32_t entrySize = readLe32(gptHeader, 84);
        if (entrySize < 128 || entrySize > 4096) entrySize = 128;
        if (numEntries > 4096) numEntries = 4096;
        const std::uint64_t entryOffset = safeMul64(entryLba, sectorSize);
        const std::uint64_t bytesNeeded64 = safeMul64(static_cast<std::uint64_t>(numEntries), static_cast<std::uint64_t>(entrySize));
        const std::size_t bytesNeeded = static_cast<std::size_t>(std::min<std::uint64_t>(bytesNeeded64, 16ULL * 1024ULL * 1024ULL));
        auto entries = readBytesAt(input, entryOffset, bytesNeeded);
        const std::size_t availableEntries = entrySize == 0 ? 0 : entries.size() / entrySize;
        for (std::size_t i = 0; i < availableEntries; ++i) {
            const std::size_t e = i * static_cast<std::size_t>(entrySize);
            if (e + 56 > entries.size()) break;
            if (allZeroBytes(entries.data() + e, 16)) continue;
            const std::uint64_t firstLba = readLe64(entries, e + 32);
            const std::uint64_t lastLba = readLe64(entries, e + 40);
            if (firstLba == 0 && lastLba == 0) continue;
            PartitionProbeEntry pe;
            pe.scheme = "GPT";
            pe.partitionIndex = static_cast<int>(i + 1);
            pe.startLba = firstLba;
            pe.sectorCount = lastLba >= firstLba ? (lastLba - firstLba + 1) : 0;
            pe.offsetBytes = safeMul64(firstLba, sectorSize);
            pe.sizeBytes = safeMul64(pe.sectorCount, sectorSize);
            const std::string rawGuid = guidFromGptBytes(entries.data() + e);
            pe.typeGuid = rawGuid;
            pe.typeCode = describeGptTypeGuid(rawGuid);
            pe.name = utf16LeNameToAscii(entries, e + 56, static_cast<std::size_t>(entrySize) > 56 ? static_cast<std::size_t>(entrySize) - 56 : 0);
            pe.filesystemHint = detectFilesystemAtByteOffset(input, pe.offsetBytes, fileSize);
            pe.confidence = "HIGH";
            pe.status = "PARTITION_ENTRY_REPORTED_NOT_EXTRACTED";
            pe.notes = "GPT partition entry parsed for readiness/provenance only. APFS/HFS/HFS+ filesystem enumeration/extraction is not implemented in this build.";
            result.entries.push_back(pe);
        }
    }

    if (result.entries.empty()) {
        if (hasProbeHit(signatures, "APFS_NXSB_CONTAINER_SUPERBLOCK") || hasProbeHit(signatures, "HFS_PLUS_VOLUME_HEADER") || hasProbeHit(signatures, "HFSX_VOLUME_HEADER")) {
            PartitionProbeEntry pe;
            pe.scheme = "VOLUME_ALIGNED_NO_PARTITION_TABLE";
            pe.partitionIndex = 0;
            pe.startLba = 0;
            pe.sectorCount = fileSize / sectorSize;
            pe.offsetBytes = 0;
            pe.sizeBytes = static_cast<std::uint64_t>(fileSize);
            pe.typeCode = "VOLUME_ALIGNED_FILESYSTEM_HINT";
            pe.filesystemHint = detectFilesystemAtByteOffset(input, 0, fileSize);
            pe.confidence = "MEDIUM";
            pe.status = "FILESYSTEM_HINT_REPORTED_NOT_EXTRACTED";
            pe.notes = "Filesystem signature appears near the start of the image. Treat as a volume-aligned hint until a filesystem reader validates it.";
            result.entries.push_back(pe);
            result.scheme = pe.scheme;
        }
    }

    log.info("Raw image partition probe completed: scheme=" + result.scheme + " entries=" + std::to_string(result.entries.size()) + (result.gptHeaderFound ? " gpt=1" : " gpt=0") + (result.mbrSignatureFound ? " mbr=1" : " mbr=0"));
    return result;
}

bool hasProbeHit(const SourceProbeFindings& f, const std::string& name) {
    for (const auto& h : f.hits) if (h.name == name) return true;
    return false;
}

void addProbeHit(SourceProbeFindings& f,
                 const std::string& name,
                 const std::string& category,
                 std::uint64_t offset,
                 const std::string& confidence,
                 const std::string& notes) {
    for (const auto& h : f.hits) {
        if (h.name == name && h.offset == offset) return;
    }
    f.hits.push_back(SourceProbeHit{name, category, offset, confidence, notes});
}

std::string joinProbeNames(const SourceProbeFindings& f, const std::string& category) {
    std::string out;
    for (const auto& h : f.hits) {
        if (!category.empty() && h.category != category) continue;
        if (!out.empty()) out += ";";
        out += h.name;
    }
    return out;
}

bool bytesAt(const std::vector<unsigned char>& data, std::size_t off, const char* needle, std::size_t n) {
    if (off + n > data.size()) return false;
    return std::memcmp(data.data() + off, needle, n) == 0;
}

std::vector<std::uint64_t> findAllBytes(const std::vector<unsigned char>& data, const std::string& needle, std::uint64_t baseOffset) {
    std::vector<std::uint64_t> hits;
    if (needle.empty() || data.size() < needle.size()) return hits;
    const unsigned char* begin = data.data();
    const unsigned char* end = data.data() + data.size();
    const unsigned char* pos = begin;
    while (pos < end) {
        const unsigned char* found = std::search(pos, end, needle.begin(), needle.end(), [](unsigned char a, char b) {
            return a == static_cast<unsigned char>(b);
        });
        if (found == end) break;
        hits.push_back(baseOffset + static_cast<std::uint64_t>(found - begin));
        if (hits.size() >= 25) break;
        pos = found + 1;
    }
    return hits;
}

#ifdef _WIN32
std::string appRunnerWindowsErrorMessage(DWORD code) {
    LPWSTR raw = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&raw), 0, nullptr);
    std::wstring ws;
    if (n && raw) ws.assign(raw, raw + n);
    if (raw) LocalFree(raw);
    while (!ws.empty() && (ws.back() == L'\r' || ws.back() == L'\n' || ws.back() == L' ' || ws.back() == L'\t')) ws.pop_back();
    if (ws.empty()) {
        std::ostringstream os;
        os << "Windows error " << code;
        return os.str();
    }
    std::string out;
    out.reserve(ws.size());
    for (wchar_t wc : ws) {
        out.push_back((wc >= 0 && wc <= 0x7f) ? static_cast<char>(wc) : '?');
    }
    return out;
}
#endif

SourceProbeFindings probeEvidenceSourceSignatures(const fs::path& input, bool fullScan, Logger& log) {
    SourceProbeFindings findings;
    std::error_code ec;
    if (!fs::exists(input, ec) || !fs::is_regular_file(input, ec)) return findings;
    findings.fileSizeBytes = fs::file_size(input, ec);
    if (ec) findings.fileSizeBytes = 0;

    std::function<bool(std::uint64_t, std::size_t, std::vector<unsigned char>&, std::string&)> readAt;
#ifdef _WIN32
    HANDLE sourceHandle = CreateFileW(input.wstring().c_str(), GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (sourceHandle == INVALID_HANDLE_VALUE) {
        const DWORD err = GetLastError();
        log.warn("Unable to open source for signature probe: " + pathString(input) + " (" + appRunnerWindowsErrorMessage(err) + ")");
        return findings;
    }
    readAt = [&](std::uint64_t off, std::size_t want, std::vector<unsigned char>& out, std::string& error) -> bool {
        out.assign(want, 0);
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(off);
        if (!SetFilePointerEx(sourceHandle, li, nullptr, FILE_BEGIN)) {
            const DWORD err = GetLastError();
            error = appRunnerWindowsErrorMessage(err);
            out.clear();
            return false;
        }
        DWORD got = 0;
        const DWORD ask = static_cast<DWORD>(std::min<std::size_t>(want, static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        if (!ReadFile(sourceHandle, out.data(), ask, &got, nullptr)) {
            const DWORD err = GetLastError();
            error = appRunnerWindowsErrorMessage(err);
            out.clear();
            return false;
        }
        out.resize(static_cast<std::size_t>(got));
        return true;
    };
#else
    std::ifstream in(input, std::ios::binary);
    if (!in) {
        log.warn("Unable to open source for signature probe: " + pathString(input));
        return findings;
    }
    readAt = [&](std::uint64_t off, std::size_t want, std::vector<unsigned char>& out, std::string& error) -> bool {
        out.assign(want, 0);
        in.clear();
        in.seekg(static_cast<std::streamoff>(off), std::ios::beg);
        if (!in) {
            error = "seek failed";
            out.clear();
            return false;
        }
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
        const std::size_t got = static_cast<std::size_t>(std::max<std::streamsize>(0, in.gcount()));
        if (in.bad()) {
            error = "read failed";
            out.clear();
            return false;
        }
        out.resize(got);
        return true;
    };
#endif

    std::string readError;
    std::vector<unsigned char> first4096;
    if (!readAt(0, 4096, first4096, readError)) {
        log.warn("Unable to read source prefix for signature probe: " + pathString(input) + (readError.empty() ? std::string() : " (" + readError + ")"));
#ifdef _WIN32
        CloseHandle(sourceHandle);
#endif
        return findings;
    }

    const bool sourceLooksLikeZip =
        bytesAt(first4096, 0, "PK\003\004", 4) ||
        bytesAt(first4096, 0, "PK\005\006", 4);

    if (bytesAt(first4096, 0, "PK\003\004", 4)) addProbeHit(findings, "ZIP_LOCAL_FILE_HEADER", "container", 0, "HIGH", "File begins with ZIP local-file header. AFF4 containers are commonly ZIP-structured containers.");
    if (bytesAt(first4096, 0, "PK\005\006", 4)) addProbeHit(findings, "ZIP_EMPTY_ARCHIVE_HEADER", "container", 0, "HIGH", "File begins with ZIP empty-archive end record.");
    if (bytesAt(first4096, 510, "\x55\xAA", 2)) addProbeHit(findings, "MBR_BOOT_SIGNATURE_55AA", "partition", 510, "MEDIUM", "Classic MBR boot signature present at offset 510. This does not prove a valid partition table by itself.");
    if (bytesAt(first4096, 512, "EFI PART", 8)) addProbeHit(findings, "GPT_HEADER_EFI_PART", "partition", 512, "HIGH", "GPT header signature found at LBA1 offset 512.");
    if (bytesAt(first4096, 1024, "H+", 2)) addProbeHit(findings, "HFS_PLUS_VOLUME_HEADER", "filesystem", 1024, "HIGH", "HFS+ volume header signature found at offset 1024 for a volume-aligned image.");
    if (bytesAt(first4096, 1024, "HX", 2)) addProbeHit(findings, "HFSX_VOLUME_HEADER", "filesystem", 1024, "HIGH", "HFSX volume header signature found at offset 1024 for a volume-aligned image.");
    if (bytesAt(first4096, 32, "NXSB", 4)) addProbeHit(findings, "APFS_NXSB_CONTAINER_SUPERBLOCK", "filesystem", 32, "HIGH", "APFS NXSB container superblock signature found at a common volume-aligned offset.");

    const std::uintmax_t boundedProbeLimit =
        sourceLooksLikeZip
            ? 64ULL * 1024ULL * 1024ULL
            : 512ULL * 1024ULL * 1024ULL;
    const std::uintmax_t maxBytes = fullScan ? findings.fileSizeBytes : std::min<std::uintmax_t>(findings.fileSizeBytes, boundedProbeLimit);
    if (sourceLooksLikeZip && !fullScan && findings.fileSizeBytes > boundedProbeLimit) {
        log.info("ZIP source signature probe is intentionally bounded to 64 MiB; ZIP entry enumeration/focused extraction performs Spotlight/CoreSpotlight discovery without scanning the full container bytes.");
    }
    const std::vector<std::pair<std::string, std::string>> patterns = {
        {"AFF4_STRING", "AFF4"},
        {"APFS_NXSB_MAGIC", "NXSB"},
        {"SPOTLIGHT_V100_PATH", ".Spotlight-V100"},
        {"SPOTLIGHT_STORE_V2_PATH", "Store-V2"},
        {"SPOTLIGHT_STORE_DB_NAME", "store.db"},
        {"IOS_CORESPOTLIGHT_PATH", "CoreSpotlight"},
        {"IOS_CORESPOTLIGHT_INDEX_DB", "index.db"}
    };
    const std::size_t chunkSize = 1024 * 1024;
    const std::size_t overlapSize = 128;
    std::vector<unsigned char> overlap;
    std::uint64_t absolute = 0;
    std::uint64_t actualScanned = 0;
    bool readFailed = false;
    while (absolute < maxBytes) {
        const std::uintmax_t remaining = maxBytes - absolute;
        const std::size_t want = static_cast<std::size_t>(std::min<std::uintmax_t>(chunkSize, remaining));
        std::vector<unsigned char> chunk;
        std::string chunkError;
        if (!readAt(absolute, want, chunk, chunkError)) {
            readFailed = true;
            log.warn("Source signature probe read stopped at offset " + std::to_string(absolute) + " for " + pathString(input) + (chunkError.empty() ? std::string() : " (" + chunkError + ")"));
            break;
        }
        const std::size_t n = chunk.size();
        if (n == 0) break;
        std::vector<unsigned char> window;
        window.reserve(overlap.size() + n);
        window.insert(window.end(), overlap.begin(), overlap.end());
        window.insert(window.end(), chunk.begin(), chunk.end());
        const std::uint64_t windowBase = absolute >= overlap.size() ? absolute - static_cast<std::uint64_t>(overlap.size()) : 0;
        for (const auto& pat : patterns) {
            if (hasProbeHit(findings, pat.first) && pat.first != "SPOTLIGHT_STORE_DB_NAME" && pat.first != "SPOTLIGHT_V100_PATH") continue;
            auto offsets = findAllBytes(window, pat.second, windowBase);
            for (std::uint64_t off : offsets) {
                std::string category = "string_hint";
                std::string confidence = "LOW";
                std::string notes = "Bounded byte scan found this ASCII signature/path fragment. Treat as a hint until a container/filesystem reader validates structure.";
                if (pat.first == "APFS_NXSB_MAGIC") { category = "filesystem"; confidence = "MEDIUM"; notes = "APFS NXSB magic found during bounded scan. Offset may indicate an embedded APFS container/superblock but requires partition/filesystem validation."; }
                else if (pat.first == "AFF4_STRING") { category = "container"; confidence = "LOW"; notes = "AFF4 string found during bounded scan. Use with extension/header evidence; not a complete AFF4 validation."; }
                else if (pat.first.find("SPOTLIGHT") != std::string::npos || pat.first.find("CORESPOTLIGHT") != std::string::npos) { category = "spotlight_hint"; confidence = "MEDIUM"; }
                addProbeHit(findings, pat.first, category, off, confidence, notes);
                if (pat.first != "SPOTLIGHT_STORE_DB_NAME" && pat.first != "SPOTLIGHT_V100_PATH") break;
            }
        }
        overlap.clear();
        const std::size_t keep = std::min(overlapSize, window.size());
        if (keep > 0) overlap.insert(overlap.end(), window.end() - static_cast<std::ptrdiff_t>(keep), window.end());
        absolute += n;
        actualScanned += n;
        if (n < want) break;
    }
    findings.bytesScanned = static_cast<std::size_t>(std::min<std::uint64_t>(actualScanned, static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));
    findings.scanTruncated = readFailed || findings.fileSizeBytes > actualScanned;
#ifdef _WIN32
    CloseHandle(sourceHandle);
#endif
    log.info("Source probe signature scan completed: bytes_scanned=" + std::to_string(findings.bytesScanned) + " hits=" + std::to_string(findings.hits.size()) + (findings.scanTruncated ? " truncated=1" : " truncated=0"));
    return findings;
}

void writeSourceProbeSignatureCsv(const fs::path& caseDir, const SourceProbeFindings& findings, Logger& log) {
    try {
        const fs::path outPath = caseDir / "source_probe_signatures.csv";
        std::ofstream out(outPath, std::ios::binary);
        out << "signature_name,category,offset_bytes,confidence,notes\n";
        for (const auto& h : findings.hits) {
            out << csvEscape(h.name) << ','
                << csvEscape(h.category) << ','
                << h.offset << ','
                << csvEscape(h.confidence) << ','
                << csvEscape(h.notes) << "\n";
        }
        log.info("Source probe signatures written: " + pathString(outPath));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write source_probe_signatures.csv: ") + ex.what());
    }
}


void writeSourceProbeJson(const fs::path& caseDir,
                          const EvidenceSource& source,
                          const fs::path& input,
                          const SourceProbeFindings& findings,
                          const PartitionProbeFindings& partitions,
                          Logger& log) {
    auto jsonEscape = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
            }
        }
        return out;
    };
    try {
        const fs::path outPath = caseDir / "source_probe_summary.json";
        std::ofstream out(outPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(input)) << "\",\n";
        out << "  \"input_type\": \"" << jsonEscape(inputSourceType(input)) << "\",\n";
        out << "  \"file_size_bytes\": " << findings.fileSizeBytes << ",\n";
        out << "  \"bytes_scanned\": " << findings.bytesScanned << ",\n";
        out << "  \"scan_truncated\": " << (findings.scanTruncated ? "true" : "false") << ",\n";
        out << "  \"signature_count\": " << findings.hits.size() << ",\n";
        out << "  \"filesystem_hints\": \"" << jsonEscape(joinProbeNames(findings, "filesystem")) << "\",\n";
        out << "  \"spotlight_hints\": \"" << jsonEscape(joinProbeNames(findings, "spotlight_hint")) << "\",\n";
        out << "  \"partition_scheme\": \"" << jsonEscape(partitions.scheme) << "\",\n";
        out << "  \"mbr_signature_found\": " << (partitions.mbrSignatureFound ? "true" : "false") << ",\n";
        out << "  \"gpt_header_found\": " << (partitions.gptHeaderFound ? "true" : "false") << ",\n";
        out << "  \"partition_entry_count\": " << partitions.entries.size() << "\n";
        out << "}\n";
        log.info("Source probe summary JSON written: " + pathString(outPath));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write source_probe_summary.json: ") + ex.what());
    }
}


void writeSourcePartitionProbeCsv(const fs::path& caseDir, const PartitionProbeFindings& partitions, Logger& log) {
    try {
        const fs::path outPath = caseDir / "source_partition_probe.csv";
        std::ofstream out(outPath, std::ios::binary);
        out << "scheme,partition_index,start_lba,sector_count,offset_bytes,size_bytes,type_code,type_guid,name,filesystem_hint,confidence,status,notes\n";
        for (const auto& p : partitions.entries) {
            out << csvEscape(p.scheme) << ','
                << p.partitionIndex << ','
                << p.startLba << ','
                << p.sectorCount << ','
                << p.offsetBytes << ','
                << p.sizeBytes << ','
                << csvEscape(p.typeCode) << ','
                << csvEscape(p.typeGuid) << ','
                << csvEscape(p.name) << ','
                << csvEscape(p.filesystemHint) << ','
                << csvEscape(p.confidence) << ','
                << csvEscape(p.status) << ','
                << csvEscape(p.notes) << "\n";
        }
        log.info("Source partition probe CSV written: " + pathString(outPath) + " entries=" + std::to_string(partitions.entries.size()));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write source_partition_probe.csv: ") + ex.what());
    }
}

std::size_t countDistinctStoreGroups(const std::vector<StoreInfo>& stores, bool validOnly);
std::size_t countValidDatabaseCandidates(const std::vector<StoreInfo>& stores);

void persistSourceProbeInventory(CaseDatabase& db,
                                 const EvidenceSource& source,
                                 const fs::path& originalInput,
                                 const fs::path& workingRoot,
                                 const std::vector<StoreInfo>& stores,
                                 const std::string& parseStatus,
                                 const std::string& nextAction,
                                 const SourceProbeFindings& probe,
                                 const PartitionProbeFindings& partitions,
                                 Logger& log) {
    try {
        const std::string type = inputSourceType(originalInput);
        const std::size_t validCandidates = countValidDatabaseCandidates(stores);
        const std::size_t groups = countDistinctStoreGroups(stores, false);
        const std::size_t validGroups = countDistinctStoreGroups(stores, true);
        const std::string filesystemHints = joinProbeNames(probe, "filesystem");
        const std::string spotlightHints = joinProbeNames(probe, "spotlight_hint");
        db.begin();
        try {
            {
                auto d1 = db.prepare("DELETE FROM source_probe_runs WHERE source_id=?");
                d1.bind(1, source.sourceId); d1.stepDone();
                auto d2 = db.prepare("DELETE FROM source_probe_signatures WHERE source_id=?");
                d2.bind(1, source.sourceId); d2.stepDone();
                auto d3 = db.prepare("DELETE FROM source_partition_probe WHERE source_id=?");
                d3.bind(1, source.sourceId); d3.stepDone();
            }
            auto run = db.prepare("INSERT INTO source_probe_runs(source_id,input_path,input_type,source_kind,size_bytes,working_root,container_reader_status,filesystem_reader_status,spotlight_discovery_status,database_candidates,valid_database_candidates,store_groups,valid_store_groups,probe_bytes_scanned,probe_truncated,probe_signature_count,partition_scheme,partition_entry_count,filesystem_hints,spotlight_hints,parse_status,next_action,notes,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
            int r = 1;
            run.bind(r++, source.sourceId);
            run.bind(r++, pathString(originalInput));
            run.bind(r++, type);
            run.bind(r++, source.sourceKind);
            run.bind(r++, static_cast<long long>(probe.fileSizeBytes));
            run.bind(r++, pathString(workingRoot));
            run.bind(r++, sourceTypeReaderStatus(type));
            run.bind(r++, sourceTypeFilesystemStatus(type));
            run.bind(r++, discoveryStatusForStores(stores));
            run.bind(r++, static_cast<long long>(stores.size()));
            run.bind(r++, static_cast<long long>(validCandidates));
            run.bind(r++, static_cast<long long>(groups));
            run.bind(r++, static_cast<long long>(validGroups));
            run.bind(r++, static_cast<long long>(probe.bytesScanned));
            run.bind(r++, probe.scanTruncated ? 1LL : 0LL);
            run.bind(r++, static_cast<long long>(probe.hits.size()));
            run.bind(r++, partitions.scheme);
            run.bind(r++, static_cast<long long>(partitions.entries.size()));
            run.bind(r++, filesystemHints);
            run.bind(r++, spotlightHints);
            run.bind(r++, parseStatus);
            run.bind(r++, nextAction);
            run.bind(r++, source.notes);
            run.bind(r++, nowUtc());
            run.stepDone();

            auto sig = db.prepare("INSERT INTO source_probe_signatures(source_id,signature_name,category,offset_bytes,confidence,notes,created_utc) VALUES(?,?,?,?,?,?,?)");
            for (const auto& h : probe.hits) {
                int i = 1;
                sig.bind(i++, source.sourceId);
                sig.bind(i++, h.name);
                sig.bind(i++, h.category);
                sig.bind(i++, static_cast<long long>(h.offset));
                sig.bind(i++, h.confidence);
                sig.bind(i++, h.notes);
                sig.bind(i++, nowUtc());
                sig.stepDone();
                sig.reset();
            }

            auto part = db.prepare("INSERT INTO source_partition_probe(source_id,scheme,partition_index,start_lba,sector_count,offset_bytes,size_bytes,type_code,type_guid,name,filesystem_hint,confidence,status,notes,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
            for (const auto& p : partitions.entries) {
                int i = 1;
                part.bind(i++, source.sourceId);
                part.bind(i++, p.scheme);
                part.bind(i++, static_cast<long long>(p.partitionIndex));
                part.bind(i++, static_cast<long long>(p.startLba));
                part.bind(i++, static_cast<long long>(p.sectorCount));
                part.bind(i++, static_cast<long long>(p.offsetBytes));
                part.bind(i++, static_cast<long long>(p.sizeBytes));
                part.bind(i++, p.typeCode);
                part.bind(i++, p.typeGuid);
                part.bind(i++, p.name);
                part.bind(i++, p.filesystemHint);
                part.bind(i++, p.confidence);
                part.bind(i++, p.status);
                part.bind(i++, p.notes);
                part.bind(i++, nowUtc());
                part.stepDone();
                part.reset();
            }
            db.commit();
        } catch (...) {
            db.rollbackNoThrow();
            throw;
        }
        log.info("Source probe inventory persisted to SQLite: signatures=" + std::to_string(probe.hits.size()) + " partitions=" + std::to_string(partitions.entries.size()));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to persist source probe inventory to SQLite: ") + ex.what());
    }
}


std::size_t countProbeNameContains(const SourceProbeFindings& probe, const std::string& needle) {
    const std::string n = toLower(needle);
    std::size_t count = 0;
    for (const auto& h : probe.hits) {
        const std::string combined = toLower(h.name + " " + h.category + " " + h.notes);
        if (combined.find(n) != std::string::npos) ++count;
    }
    return count;
}

std::size_t countPartitionFilesystemHintContains(const PartitionProbeFindings& partitions, const std::string& needle) {
    const std::string n = toLower(needle);
    std::size_t count = 0;
    for (const auto& p : partitions.entries) {
        const std::string combined = toLower(p.filesystemHint + " " + p.typeCode + " " + p.typeGuid + " " + p.notes);
        if (combined.find(n) != std::string::npos) ++count;
    }
    return count;
}

std::string imageContainerTypeForInput(const fs::path& input) {
    if (isAff4SourcePath(input)) return "AFF4";
    if (isRawImageSourcePath(input)) return "RAW_FLAT_IMAGE";
    if (isZipSourcePath(input)) return "ZIP_OR_EXPORTED_FILESYSTEM_CONTAINER";
    std::error_code ec;
    if (fs::is_directory(input, ec)) return "FOLDER_OR_EXTRACTED_FILESYSTEM_ROOT";
    return "UNKNOWN";
}

std::string activeComparisonStatusForInput(const fs::path& input, const PartitionProbeFindings& partitions) {
    if (isAff4SourcePath(input)) return "WAITING_FOR_AFF4_STREAM_READER_AND_APFS_FILE_INVENTORY";
    if (isRawImageSourcePath(input)) {
        const std::size_t apfsHints = countPartitionFilesystemHintContains(partitions, "APFS") + countPartitionFilesystemHintContains(partitions, "APPLE_APFS");
        return apfsHints > 0 ? "WAITING_FOR_APFS_FILE_INVENTORY_FROM_RAW_PARTITION" : "WAITING_FOR_FILESYSTEM_READER_AND_IMAGE_FILE_INVENTORY";
    }
    if (isZipSourcePath(input)) return "ZIP_PARSED_FOR_SPOTLIGHT_NOT_IMAGE_FILE_INVENTORY";
    std::error_code ec;
    if (fs::is_directory(input, ec)) return "FOLDER_PARSED_FOR_SPOTLIGHT_NOT_IMAGE_FILE_INVENTORY";
    return "NOT_READY_UNKNOWN_SOURCE_TYPE";
}

void writeImageInventoryReadinessCsv(const fs::path& caseDir,
                                     const EvidenceSource& source,
                                     const fs::path& originalInput,
                                     const SourceProbeFindings& probe,
                                     const PartitionProbeFindings& partitions,
                                     const std::string& nextAction,
                                     Logger& log) {
    try {
        const std::string inputType = inputSourceType(originalInput);
        const std::string containerType = imageContainerTypeForInput(originalInput);
        const std::size_t apfsHintCount = countProbeNameContains(probe, "APFS") + countPartitionFilesystemHintContains(partitions, "APFS") + countPartitionFilesystemHintContains(partitions, "APPLE_APFS");
        const std::size_t spotlightHintCount = countProbeNameContains(probe, "SPOTLIGHT") + countProbeNameContains(probe, "CORESPOTLIGHT");
        const std::string activeStatus = activeComparisonStatusForInput(originalInput, partitions);
        const fs::path imageCsv = caseDir / "image_inventory_readiness.csv";
        {
            std::ofstream out(imageCsv, std::ios::binary);
            out << "source_id,input_path,input_type,container_type,container_reader_status,partition_reader_status,filesystem_reader_status,spotlight_locator_status,active_comparison_status,preferred_reader_order,size_bytes,partition_scheme,partition_count,apfs_hint_count,spotlight_hint_count,inventory_file_count,comparison_ready,next_action,notes\n";
            out << csvEscape(source.sourceId) << ','
                << csvEscape(pathString(originalInput)) << ','
                << csvEscape(inputType) << ','
                << csvEscape(containerType) << ','
                << csvEscape(sourceTypeReaderStatus(inputType)) << ','
                << csvEscape(partitions.entries.empty() ? "NOT_AVAILABLE_OR_NOT_RAW" : "PARTITION_PROBE_REPORTED") << ','
                << csvEscape(sourceTypeFilesystemStatus(inputType)) << ','
                << csvEscape(spotlightHintCount > 0 ? "SPOTLIGHT_HINTS_FOUND_NOT_EXTRACTED_FROM_IMAGE" : "NO_IMAGE_BACKED_SPOTLIGHT_LOCATION_YET") << ','
                << csvEscape(activeStatus) << ','
                << csvEscape("AFF4->APFS->Spotlight->image_file_inventory->active_file_comparison; raw IMG/DD secondary") << ','
                << probe.fileSizeBytes << ','
                << csvEscape(partitions.scheme) << ','
                << partitions.entries.size() << ','
                << apfsHintCount << ','
                << spotlightHintCount << ','
                << 0 << ','
                << 0 << ','
                << csvEscape(nextAction) << ','
                << csvEscape(source.notes) << "\n";
        }
        const fs::path compareCsv = caseDir / "active_file_comparison_readiness.csv";
        {
            std::ofstream out(compareCsv, std::ios::binary);
            out << "source_id,input_type,container_type,image_inventory_available,spotlight_artifacts_available,comparison_ready,comparison_status,comparison_basis,next_action,notes\n";
            out << csvEscape(source.sourceId) << ','
                << csvEscape(inputType) << ','
                << csvEscape(containerType) << ','
                << 0 << ','
                << 0 << ','
                << 0 << ','
                << csvEscape(activeStatus) << ','
                << csvEscape("Future comparison joins Spotlight artifacts to image_file_inventory by inode/parent when available, then path/name/size fallback.") << ','
                << csvEscape(nextAction) << ','
                << csvEscape("Active file comparison is intentionally not inferred until image_file_inventory contains APFS/HFS filesystem rows from the evidence image.") << "\n";
        }
        const fs::path inventoryCsv = caseDir / "image_file_inventory.csv";
        if (!fs::exists(inventoryCsv)) {
            std::ofstream out(inventoryCsv, std::ios::binary);
            out << "image_file_id,source_id,container_type,container_path,aff4_stream_id,aff4_stream_name,partition_index,partition_scheme,partition_offset_bytes,filesystem_type,apfs_container_id,apfs_volume_name,filesystem_object_id,parent_filesystem_object_id,inode_num,parent_inode_num,full_path,file_name,is_directory,logical_size_bytes,allocated_size_bytes,created_utc,modified_utc,accessed_utc,changed_utc,file_sha256,source_confidence,extraction_status,provenance,created_utc_inventory\n";
        }
        log.info("Image inventory / active-file comparison readiness written: " + pathString(imageCsv));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write image inventory readiness CSVs: ") + ex.what());
    }
}

void persistImageInventoryReadiness(CaseDatabase& db,
                                    const EvidenceSource& source,
                                    const fs::path& originalInput,
                                    const SourceProbeFindings& probe,
                                    const PartitionProbeFindings& partitions,
                                    const std::string& nextAction,
                                    Logger& log) {
    try {
        const std::string inputType = inputSourceType(originalInput);
        const std::string containerType = imageContainerTypeForInput(originalInput);
        const std::size_t apfsHintCount = countProbeNameContains(probe, "APFS") + countPartitionFilesystemHintContains(partitions, "APFS") + countPartitionFilesystemHintContains(partitions, "APPLE_APFS");
        const std::size_t spotlightHintCount = countProbeNameContains(probe, "SPOTLIGHT") + countProbeNameContains(probe, "CORESPOTLIGHT");
        const std::string activeStatus = activeComparisonStatusForInput(originalInput, partitions);
        db.begin();
        try {
            auto d1 = db.prepare("DELETE FROM image_inventory_sources WHERE source_id=?");
            d1.bind(1, source.sourceId); d1.stepDone();
            auto d2 = db.prepare("DELETE FROM active_file_comparison_runs WHERE source_id=? AND run_status='READINESS_ONLY'");
            d2.bind(1, source.sourceId); d2.stepDone();
            auto inv = db.prepare("INSERT INTO image_inventory_sources(source_id,input_path,input_type,container_type,container_reader_status,partition_reader_status,filesystem_reader_status,spotlight_locator_status,active_comparison_status,preferred_reader_order,source_hash_sha256,size_bytes,partition_scheme,partition_count,apfs_hint_count,spotlight_hint_count,inventory_file_count,inventory_directory_count,comparison_candidate_count,comparison_ready,next_action,notes,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
            int i = 1;
            inv.bind(i++, source.sourceId);
            inv.bind(i++, pathString(originalInput));
            inv.bind(i++, inputType);
            inv.bind(i++, containerType);
            inv.bind(i++, sourceTypeReaderStatus(inputType));
            inv.bind(i++, partitions.entries.empty() ? std::string("NOT_AVAILABLE_OR_NOT_RAW") : std::string("PARTITION_PROBE_REPORTED"));
            inv.bind(i++, sourceTypeFilesystemStatus(inputType));
            inv.bind(i++, spotlightHintCount > 0 ? std::string("SPOTLIGHT_HINTS_FOUND_NOT_EXTRACTED_FROM_IMAGE") : std::string("NO_IMAGE_BACKED_SPOTLIGHT_LOCATION_YET"));
            inv.bind(i++, activeStatus);
            inv.bind(i++, std::string("AFF4->APFS->Spotlight->image_file_inventory->active_file_comparison; raw IMG/DD secondary"));
            inv.bind(i++, std::string(""));
            inv.bind(i++, static_cast<long long>(probe.fileSizeBytes));
            inv.bind(i++, partitions.scheme);
            inv.bind(i++, static_cast<long long>(partitions.entries.size()));
            inv.bind(i++, static_cast<long long>(apfsHintCount));
            inv.bind(i++, static_cast<long long>(spotlightHintCount));
            inv.bind(i++, 0LL);
            inv.bind(i++, 0LL);
            inv.bind(i++, 0LL);
            inv.bind(i++, 0LL);
            inv.bind(i++, nextAction);
            inv.bind(i++, source.notes);
            inv.bind(i++, nowUtc());
            inv.stepDone();

            auto run = db.prepare("INSERT INTO active_file_comparison_runs(source_id,image_inventory_available,spotlight_artifact_count,image_file_count,inode_match_count,path_match_count,missing_candidate_count,not_checked_count,run_status,comparison_basis,notes,created_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
            int r = 1;
            run.bind(r++, source.sourceId);
            run.bind(r++, 0LL);
            run.bind(r++, 0LL);
            run.bind(r++, 0LL);
            run.bind(r++, 0LL);
            run.bind(r++, 0LL);
            run.bind(r++, 0LL);
            run.bind(r++, 0LL);
            run.bind(r++, std::string("READINESS_ONLY"));
            run.bind(r++, std::string("image_file_inventory is required before live/present/missing classification can be made."));
            run.bind(r++, activeStatus + "; " + nextAction);
            run.bind(r++, nowUtc());
            run.stepDone();
            db.commit();
        } catch (...) {
            db.rollbackNoThrow();
            throw;
        }
        log.info("Image inventory readiness persisted to SQLite for source_id=" + source.sourceId);
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to persist image inventory readiness to SQLite: ") + ex.what());
    }
}

std::size_t countDistinctStoreGroups(const std::vector<StoreInfo>& stores, bool validOnly);
std::size_t countValidDatabaseCandidates(const std::vector<StoreInfo>& stores);

void writeSourceIntakeArtifacts(const fs::path& caseDir,
                                const EvidenceSource& source,
                                const fs::path& originalInput,
                                const fs::path& workingRoot,
                                const std::vector<StoreInfo>& stores,
                                const std::string& parseStatus,
                                const std::string& nextAction,
                                const SourceProbeFindings& probe,
                                const PartitionProbeFindings& partitions,
                                Logger& log) {
    try {
        const std::string type = inputSourceType(originalInput);
        std::error_code ec;
        const bool exists = fs::exists(originalInput, ec);
        const bool isFile = fs::is_regular_file(originalInput, ec);
        const bool isDir = fs::is_directory(originalInput, ec);
        std::uintmax_t sizeBytes = 0;
        if (isFile) sizeBytes = fs::file_size(originalInput, ec);
        const std::size_t validCandidates = countValidDatabaseCandidates(stores);
        const std::size_t groups = countDistinctStoreGroups(stores, false);
        const std::size_t validGroups = countDistinctStoreGroups(stores, true);
        const std::string filesystemHints = joinProbeNames(probe, "filesystem");
        const std::string spotlightHints = joinProbeNames(probe, "spotlight_hint");
        const fs::path readinessCsv = caseDir / "evidence_source_readiness.csv";
        {
            std::ofstream out(readinessCsv, std::ios::binary);
            out << "source_id,input_path,input_type,source_kind,exists,is_file,is_directory,size_bytes,working_root,container_reader_status,filesystem_reader_status,spotlight_discovery_status,database_candidates,valid_database_candidates,store_groups,valid_store_groups,probe_bytes_scanned,probe_truncated,probe_signature_count,partition_scheme,partition_entry_count,filesystem_hints,spotlight_hints,parse_status,next_action,notes\n";
            out << csvEscape(source.sourceId) << ','
                << csvEscape(pathString(originalInput)) << ','
                << csvEscape(type) << ','
                << csvEscape(source.sourceKind) << ','
                << (exists ? "1" : "0") << ','
                << (isFile ? "1" : "0") << ','
                << (isDir ? "1" : "0") << ','
                << sizeBytes << ','
                << csvEscape(pathString(workingRoot)) << ','
                << csvEscape(sourceTypeReaderStatus(type)) << ','
                << csvEscape(sourceTypeFilesystemStatus(type)) << ','
                << csvEscape(discoveryStatusForStores(stores)) << ','
                << stores.size() << ','
                << validCandidates << ','
                << groups << ','
                << validGroups << ','
                << probe.bytesScanned << ','
                << (probe.scanTruncated ? "1" : "0") << ','
                << probe.hits.size() << ','
                << csvEscape(partitions.scheme) << ','
                << partitions.entries.size() << ','
                << csvEscape(filesystemHints) << ','
                << csvEscape(spotlightHints) << ','
                << csvEscape(parseStatus) << ','
                << csvEscape(nextAction) << ','
                << csvEscape(source.notes) << "\n";
        }
        const fs::path roadmap = caseDir / "SOURCE_INTAKE_PLAN.md";
        {
            std::ofstream out(roadmap, std::ios::binary);
            out << "# Vestigant Spotlight Source Intake Plan\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Source summary\n\n";
            out << "- Source ID: `" << source.sourceId << "`\n";
            out << "- Input path: `" << pathString(originalInput) << "`\n";
            out << "- Input type: `" << type << "`\n";
            out << "- Source kind: `" << source.sourceKind << "`\n";
            out << "- Probe bytes scanned: `" << probe.bytesScanned << "`\n";
            out << "- Probe truncated: `" << (probe.scanTruncated ? "1" : "0") << "`\n";
            out << "- Probe signature count: `" << probe.hits.size() << "`\n";
            out << "- Filesystem hints: `" << filesystemHints << "`\n";
            out << "- Spotlight hints: `" << spotlightHints << "`\n";
            out << "- Partition scheme: `" << partitions.scheme << "`\n";
            out << "- Partition entries: `" << partitions.entries.size() << "`\n";
            out << "- Working root: `" << pathString(workingRoot) << "`\n";
            out << "- Parse status: `" << parseStatus << "`\n";
            out << "- Next action: " << nextAction << "\n\n";
            out << "## Implemented in this build\n\n";
            out << "- Loose folder Store-V2/CoreSpotlight `store.db` discovery and parsing.\n";
            out << "- ZIP source registration, hashing, extraction to controlled staging, and Store-V2/CoreSpotlight discovery from the staged working copy.\n";
            out << "- AFF4-first/APFS-first image inventory readiness reporting without creating a second evidentiary archive.\n";
            out << "- Bounded source signature probing for ZIP/AFF4 hints, MBR/GPT, APFS NXSB, HFS/HFS+, and Spotlight/CoreSpotlight path strings.\n";
            out << "- Raw image partition-map readiness reporting for MBR/protective MBR and GPT entries, retained as secondary to AFF4/APFS work.\n- Image-file-inventory and active-file-comparison readiness artifacts for future Spotlight-indexed versus APFS-present/missing comparison.\n";
            out << "- Clear unsupported-container status for AFF4/raw sources rather than ambiguous zero-artifact cases.\n\n";
            out << "## Not yet implemented\n\n";
            out << "- AFF4 container stream enumeration/reading. This is now the prioritized image path because most expected Spotlight-containing images are AFF4/APFS.\n";
            out << "- Raw image filesystem extraction from detected partition entries. Partition entries are reported for readiness/provenance only.\n";
            out << "- APFS filesystem enumeration from AFF4-backed disk streams; HFS/HFS+ remains lower priority.\n";
            out << "- Extraction of `.Spotlight-V100` / CoreSpotlight folders from image files.\n\n";
            out << "## Source probe signatures\n\n";
            if (probe.hits.empty()) {
                out << "No source signatures were found by the bounded probe. For large raw/AFF4 sources, rerun source-probe with `--full-scan` if a deeper byte scan is needed.\n\n";
            } else {
                out << "| Signature | Category | Offset | Confidence | Notes |\n";
                out << "|---|---|---:|---|---|\n";
                std::size_t shownProbe = 0;
                for (const auto& h : probe.hits) {
                    if (shownProbe++ >= 50) break;
                    out << "| `" << h.name << "` | " << h.category << " | " << h.offset << " | " << h.confidence << " | " << h.notes << " |\n";
                }
                out << "\n";
            }
            out << "## Raw image partition probe\n\n";
            if (partitions.entries.empty()) {
                out << "No raw image partition entries were reported. This is expected for folder, ZIP, AFF4, missing, non-raw, or unrecognized sources.\n\n";
            } else {
                out << "| Scheme | # | Start LBA | Sectors | Offset Bytes | Type | Name | Filesystem Hint | Status |\n";
                out << "|---|---:|---:|---:|---:|---|---|---|---|\n";
                for (const auto& p : partitions.entries) {
                    out << "| " << p.scheme << " | " << p.partitionIndex << " | " << p.startLba << " | " << p.sectorCount << " | " << p.offsetBytes << " | `" << p.typeCode << "` | " << p.name << " | " << p.filesystemHint << " | " << p.status << " |\n";
                }
                out << "\n";
            }

            out << "## Planned architecture\n\n";
            out << "1. Container reader layer: ZIP first, then AFF4, then raw flat image.\n";
            out << "2. Filesystem reader layer: APFS plus HFS/HFS+ enumeration without relying on Windows mounting.\n";
            out << "3. Spotlight artifact locator: find `.Spotlight-V100/Store-V2`, iOS CoreSpotlight stores, and related metadata databases inside staged/extracted filesystems.\n";
            out << "4. Normalized staging: copy extracted Spotlight artifacts to case-controlled working folders with source provenance and hashes.\n";
            out << "5. Existing parser/enrichment/review pipeline consumes staged Spotlight artifacts unchanged.\n\n";
            out << "## Store discovery summary\n\n";
            out << "- Database candidates: " << stores.size() << "\n";
            out << "- Valid database candidates: " << validCandidates << "\n";
            out << "- Store groups: " << groups << "\n";
            out << "- Valid store groups: " << validGroups << "\n\n";
            if (!stores.empty()) {
                out << "## First discovered store candidates\n\n";
                out << "| Valid | Role | Store GUID | Path | Error |\n";
                out << "|---:|---|---|---|---|\n";
                std::size_t shown = 0;
                for (const auto& st : stores) {
                    if (shown++ >= 20) break;
                    out << "| " << (st.isValid ? "1" : "0")
                        << " | " << sourceDbRole(st.storePath)
                        << " | `" << st.storeGuid << "`"
                        << " | `" << pathString(st.storePath) << "`"
                        << " | " << st.validationError << " |\n";
                }
            }
        }
        log.info("Source intake readiness written: " + pathString(readinessCsv));
        log.info("Source intake plan written: " + pathString(roadmap));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write source intake artifacts: ") + ex.what());
    } catch (...) {
        log.warn("Unable to write source intake artifacts: unknown error");
    }
}

fs::path writeIosFocusedZipExtractorScript(const fs::path& caseDir, const fs::path& zipPath, const fs::path& stageRoot, const fs::path& inventoryPath, bool throwOnNoMatch) {
    const fs::path scriptPath = caseDir / "logs" / "ios_focused_zip_extract.ps1";
    fs::create_directories(scriptPath.parent_path());
    const fs::path entryProbePath = caseDir / "ios_zip_entry_probe.csv";
    const fs::path ffsInventoryPath = caseDir / "ios_ffs_file_inventory.csv";
    const fs::path dbInventoryPath = caseDir / "ios_app_database_inventory.csv";
    const fs::path appDbStageRoot = caseDir / "EvidenceStaging" / "ios_app_databases";
    std::ofstream ps(scriptPath, std::ios::binary);
    ps << "$ErrorActionPreference = 'Stop'\n";
    ps << "$ZipPath = " << psSingleQuote(platformPathString(zipPath)) << "\n";
    ps << "$StageRoot = " << psSingleQuote(platformPathString(stageRoot)) << "\n";
    ps << "$InventoryPath = " << psSingleQuote(platformPathString(inventoryPath)) << "\n";
    ps << "$EntryProbePath = " << psSingleQuote(platformPathString(entryProbePath)) << "\n";
    ps << "$FfsInventoryPath = " << psSingleQuote(platformPathString(ffsInventoryPath)) << "\n";
    ps << "$DbInventoryPath = " << psSingleQuote(platformPathString(dbInventoryPath)) << "\n";
    ps << "$AppDbStageRoot = " << psSingleQuote(platformPathString(appDbStageRoot)) << "\n";
    ps << "$ThrowOnNoMatch = " << (throwOnNoMatch ? "$true" : "$false") << "\n";
    ps << R"PS(
New-Item -ItemType Directory -Force -Path $StageRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $InventoryPath -Parent) | Out-Null
"entry_type,length,last_write_time,full_name,extracted_path" | Set-Content -LiteralPath $InventoryPath -Encoding UTF8
"source,entry_type,length,full_name" | Set-Content -LiteralPath $EntryProbePath -Encoding UTF8
"normalized_path,original_zip_entry,file_name,extension,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,is_directory,sha256_status,inventory_notes" | Set-Content -LiteralPath $FfsInventoryPath -Encoding UTF8
"normalized_path,original_zip_entry,database_name,database_category,app_hint,protection_class_hint,size_bytes,zip_modified_utc,parse_status,record_inventory_status,notes" | Set-Content -LiteralPath $DbInventoryPath -Encoding UTF8
function Csv([string]$s) {
  if ($null -eq $s) { $s = "" }
  $s = [string]$s
  if ($s.IndexOfAny([char[]]@(',',[char]34,[char]13,[char]10)) -ge 0) { return '"' + ($s -replace '"','""') + '"' }
  return $s
}
$ZipStageHeartbeatPath = Join-Path (Split-Path $InventoryPath -Parent) "logs\ios_zip_stage_heartbeat.log"
$ZipInventoryProgressPath = Join-Path (Split-Path $InventoryPath -Parent) "logs\ios_zip_inventory_progress.tsv"
$script:KnownAppDatabaseExtractionDone = $false
function Write-ZipStageHeartbeat([string]$message) {
  try {
    New-Item -ItemType Directory -Force -Path (Split-Path $ZipStageHeartbeatPath -Parent) | Out-Null
    $line = (Get-Date).ToUniversalTime().ToString('o') + " `t" + $message
    Add-Content -LiteralPath $ZipStageHeartbeatPath -Value $line -Encoding UTF8
  } catch {}
}
function Write-ZipInventoryProgress([string]$stage, [int64]$count, [string]$message) {
  try {
    New-Item -ItemType Directory -Force -Path (Split-Path $ZipInventoryProgressPath -Parent) | Out-Null
    if (-not (Test-Path -LiteralPath $ZipInventoryProgressPath)) {
      "timestamp_utc`tstage`tentry_count`tmessage" | Set-Content -LiteralPath $ZipInventoryProgressPath -Encoding UTF8
    }
    $line = (Get-Date).ToUniversalTime().ToString('o') + "`t" + $stage + "`t" + $count + "`t" + $message
    Add-Content -LiteralPath $ZipInventoryProgressPath -Value $line -Encoding UTF8
  } catch {}
}
function Normalize-IosPath([string]$fullName) {
  if ([string]::IsNullOrWhiteSpace($fullName)) { return "" }
  $p = $fullName.Replace('\','/')
  $m = [regex]::Match($p, '(?i)(/private/var/.*)$')
  if ($m.Success) { return $m.Groups[1].Value.ToLowerInvariant() }
  $m = [regex]::Match($p, '(?i)(/var/.*)$')
  if ($m.Success) { return ('/private' + $m.Groups[1].Value).ToLowerInvariant() }
  return ('/' + $p.TrimStart('/')).ToLowerInvariant()
}
function Get-ProtectionClassHint([string]$path) {
  $p = $path.ToLowerInvariant()
  if ($p.Contains('nsfileprotectioncompleteuntilfirstuserauthentication')) { return 'NSFileProtectionCompleteUntilFirstUserAuthentication' }
  if ($p.Contains('nsfileprotectioncompleteunlessopen')) { return 'NSFileProtectionCompleteUnlessOpen' }
  if ($p.Contains('nsfileprotectioncompletewhenuserinactive')) { return 'NSFileProtectionCompleteWhenUserInactive' }
  if ($p.Contains('nsfileprotectioncomplete')) { return 'NSFileProtectionComplete' }
  if ($p.Contains('/priority/')) { return 'Priority' }
  return 'Unknown'
}
function Test-IsSignalPath([string]$lowerPath) {
  $pn = $lowerPath.Replace('\','/')
  return ($pn.Contains('org.whispersystems.signal') -or $pn.Contains('signal.messenger') -or $pn.Contains('/signal/'))
}
function Test-IsChromeBrowserPath([string]$lowerPath) {
  $pn = $lowerPath.Replace('\','/')
  return ($pn.Contains('com.google.chrome') -or $pn.Contains('/chrome/') -or $pn.Contains('/google/chrome'))
}
function Get-DomainHint([string]$path) {
  $p = $path.ToLowerInvariant()
  if ($p.Contains('/library/sms/') -or $p.EndsWith('/sms.db')) { return 'Messages' }
  if ($p.Contains('/keychains/') -or $p.Contains('/keychain/')) { return 'Keychain' }
  if ($p.Contains('callhistory')) { return 'CallHistory' }
  if ($p.Contains('whatsapp')) { return 'WhatsApp' }
  if (Test-IsSignalPath $p) { return 'Signal' }
  if ($p.Contains('telegram')) { return 'Telegram' }
  if ($p.Contains('safari')) { return 'Safari' }
  if (Test-IsChromeBrowserPath $p) { return 'Chrome' }
  if ($p.Contains('/mail/')) { return 'Mail' }
  if ($p.Contains('/calendar/')) { return 'Calendar' }
  if ($p.Contains('/addressbook/') -or $p.Contains('contacts')) { return 'Contacts' }
)PS" R"PS(  if ($p.Contains('fileprovider') -or $p.Contains('clouddocs') -or $p.Contains('mobile documents')) { return 'FileProviderOrCloudDocs' }
  return 'Other'
}
function Get-AppContainerHint([string]$path) {
  $p = $path.Replace('\','/')
  $m = [regex]::Match($p, '(?i)/Containers/(Data/)?Application/([^/]+)')
  if ($m.Success) { return 'ApplicationContainer:' + $m.Groups[2].Value }
  $m = [regex]::Match($p, '(?i)/Containers/Shared/AppGroup/([^/]+)')
  if ($m.Success) { return 'AppGroup:' + $m.Groups[1].Value }
  return ''
}
function Get-DatabaseCategory([string]$path, [string]$name) {
  $p = $path.ToLowerInvariant(); $n = $name.ToLowerInvariant()
  $dbLike = ($n -like '*.db' -or $n -like '*.sqlite' -or $n -like '*.sqlite3' -or $n -like '*.sqlitedb' -or $n -like '*.storedata' -or $n -eq 'chatstorage.sqlite' -or $n -eq 'contactsv2.sqlite' -or $n -eq 'callhistory.sqlite')
  if (-not $dbLike) { return @('','') }
  if ($n -eq 'sms.db') { return @('APPLE_MESSAGES','Messages') }
  if ($n -eq 'knowledgec.db' -or $p.Contains('/coreduet/knowledge/')) { return @('KNOWLEDGEC_COREDUET','KnowledgeC') }
  if ($p.Contains('/keychains/') -or $p.Contains('/keychain/') -or $n -eq 'keychain-2.db' -or $n -eq 'keychain-2-debug.db') { return @('KEYCHAIN','Keychain') }
  if ($p.Contains('callhistory') -or $n -like 'callhistory*') { return @('CALL_HISTORY','PhoneFaceTime') }
  if (($p.Contains('group.net.whatsapp') -or $p.Contains('/whatsapp/') -or $p.Contains('whatsapp.shared')) -or $n -eq 'chatstorage.sqlite' -or $n -eq 'contactsv2.sqlite') { return @('WHATSAPP','WhatsApp') }
  if (Test-IsSignalPath $p) { return @('SIGNAL','Signal') }
  if ($p.Contains('telegram')) { return @('TELEGRAM','Telegram') }
  if ($p.Contains('safari')) { return @('SAFARI_WEB','Safari') }
  if (Test-IsChromeBrowserPath $p) { return @('CHROME_WEB','Chrome') }
  if ($p.Contains('webkit')) { return @('WEBKIT','WebKit') }
  if ($p.Contains('/mail/')) { return @('MAIL','Mail') }
  if ($p.Contains('/calendar/') -or $n.Contains('calendar')) { return @('CALENDAR','Calendar') }
  if ($p.Contains('addressbook') -or $p.Contains('contacts')) { return @('CONTACTS','Contacts') }
  return @('OTHER_SQLITE_OR_STORE_DATABASE','Other')
}
function Get-SevenZipPath {
  $candidates = @(
    "C:\Program Files\7-Zip\7z.exe",
    "C:\Program Files (x86)\7-Zip\7z.exe",
    "7z.exe"
  )
  foreach ($candidate in $candidates) {
    try {
      $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
      if ($cmd) { return $cmd.Source }
      if (Test-Path -LiteralPath $candidate) { return $candidate }
    } catch {}
  }
  return $null
}
function Get-LeafNameSafe([string]$path) {
  if ([string]::IsNullOrWhiteSpace($path)) { return '' }
  $p = ([string]$path).Replace('\','/').Trim().TrimEnd('/')
  if ([string]::IsNullOrWhiteSpace($p)) { return '' }
  $parts = $p.Split('/') | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
  if ($parts.Count -eq 0) { return '' }
  return [string]$parts[$parts.Count - 1]
}
function Get-ExtensionSafe([string]$name) {
  if ([string]::IsNullOrWhiteSpace($name)) { return '' }
  $n = [string]$name
  $idx = $n.LastIndexOf('.')
  if ($idx -lt 0 -or $idx -eq ($n.Length - 1)) { return '' }
  return $n.Substring($idx + 1).ToLowerInvariant()
}
function Is-ArchiveMetadataPath([string]$fullName) {
  if ([string]::IsNullOrWhiteSpace($fullName)) { return $true }
  $p = ([string]$fullName).Trim()
  $z = ([string]$ZipPath).Trim()
  if ($p -eq $z) { return $true }
  if ($p.Replace('/','\') -eq $z.Replace('/','\')) { return $true }
  if ($p -match '^[A-Za-z]:[\/]') { return $true }
  if ($p -match '^[\\]{2}') { return $true }
  if ($p -match '^[-]+$') { return $true }
  return $false
}
function New-ZipEntryObject([string]$fullName,[string]$sizeValue,[string]$modifiedValue,[string]$folderValue) {
  if (Is-ArchiveMetadataPath $fullName) { return $null }
  if ($folderValue -eq '+') { return $null }
  $leaf = Get-LeafNameSafe $fullName
  if ([string]::IsNullOrWhiteSpace($leaf)) { return $null }
  $len = 0L
  [void][Int64]::TryParse([string]$sizeValue, [ref]$len)
  $mtime = ''
  if (-not [string]::IsNullOrWhiteSpace($modifiedValue)) {
    try { $mtime = ([DateTimeOffset]::Parse($modifiedValue)).UtcDateTime.ToString('o') } catch { $mtime = [string]$modifiedValue }
  }
  [pscustomobject]@{ FullName = $fullName; Name = $leaf; Length = $len; ModifiedUtc = $mtime }
}
# V0.9.42: The old 7z pipeline/Regex inventory parser was removed. The active 7z inventory path dumps -slt output to raw text and parses the file line-by-line without an external process pipeline.
function Get-ZipEntriesViaDotNet {
  $entries = New-Object System.Collections.ArrayList
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $zip = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
  try {
    foreach ($entry in $zip.Entries) {
      if ([string]::IsNullOrWhiteSpace($entry.FullName) -or [string]::IsNullOrWhiteSpace($entry.Name)) { continue }
      [void]$entries.Add([pscustomobject]@{
        FullName = $entry.FullName
        Name = $entry.Name
        Length = [int64]$entry.Length
        ModifiedUtc = $entry.LastWriteTime.UtcDateTime.ToString('o')
      })
    }
  } finally { $zip.Dispose() }
  return $entries
}
function Get-ExtractedDbPath([string]$fullName) {
  if ([string]::IsNullOrWhiteSpace($fullName)) { return '' }
  $rel = $fullName.Replace('\','/').TrimStart('/').Replace(':','_')
  $rel = $rel -replace '[<>:"|?*]', '_'
  while ($rel.StartsWith('../')) { $rel = $rel.Substring(3) }
  $rel = $rel -replace '\.\./','_/'
  return (Join-Path $AppDbStageRoot ($rel.Replace('/', [IO.Path]::DirectorySeparatorChar)))
}
function Extract-KnownAppDatabases([string]$SevenZipPath) {
  if ([string]::IsNullOrWhiteSpace($SevenZipPath)) { return }
  if ($script:KnownAppDatabaseExtractionDone) { Write-ZipStageHeartbeat "known app database extraction already completed; skipping duplicate extraction"; return }
  $script:KnownAppDatabaseExtractionDone = $true
  Write-ZipStageHeartbeat "starting targeted app database extraction before full FFS inventory"
  New-Item -ItemType Directory -Force -Path $AppDbStageRoot | Out-Null
  $dbExtractLog = Join-Path (Split-Path $InventoryPath -Parent) "logs\ios_app_database_extract_7z.log"
  New-Item -ItemType Directory -Force -Path (Split-Path $dbExtractLog -Parent) | Out-Null
  $dbPatterns = @(
    "-ir!*sms.db",
    "-ir!*CallHistory.storedata",
    "-ir!*CallHistory*.db",
)PS" R"PS(    "-ir!*ChatStorage.sqlite",
    "-ir!*ContactsV2.sqlite",
    "-ir!*WhatsApp*.sqlite",
    "-ir!*group.net.whatsapp*.sqlite",
    "-ir!*keychain-2.db",
    "-ir!*keychain*.db",
    "-ir!*Signal*.sqlite",
    "-ir!*Telegram*.sqlite",
    "-ir!*History.db",
    "-ir!*Browser*.db",
    "-ir!*Safari*.db",
    "-ir!*WebKit*.db",
    "-ir!*Calendar*.sqlite*",
    "-ir!*Calendar*.db",
    "-ir!*AddressBook*.sqlitedb",
    "-ir!*AddressBook*.db",
    "-ir!*Contacts*.sqlite*",
    "-ir!*Contacts*.db",
    "-ir!*knowledgeC.db",
    "-ir!*interactionC.db",
    "-ir!*globalKnowledge.db"
  )
  & $SevenZipPath x $ZipPath "-o$AppDbStageRoot" @dbPatterns -y > $dbExtractLog 2>&1
  $dbExit = $LASTEXITCODE
  Write-ZipStageHeartbeat "targeted app database extraction completed exit=$dbExit log=$dbExtractLog"
  "ios_app_database_extract_exit=$dbExit" | Write-Output
  "ios_app_database_extract_root=$AppDbStageRoot" | Write-Output
}
function Write-InventoryRowsForEntry([object]$entry, [System.IO.StreamWriter]$ffsWriter, [System.IO.StreamWriter]$dbWriter) {
  if ($null -eq $entry -or [string]::IsNullOrWhiteSpace($entry.FullName) -or [string]::IsNullOrWhiteSpace($entry.Name)) { return }
  $norm = Normalize-IosPath $entry.FullName
  $name = Get-LeafNameSafe ([string]$entry.FullName)
  $ext = Get-ExtensionSafe $name
  $prot = Get-ProtectionClassHint $entry.FullName
  $app = Get-AppContainerHint $entry.FullName
  $domain = Get-DomainHint $entry.FullName
  $mtime = [string]$entry.ModifiedUtc
  $isDir = '0'
  $ffsVals = @($norm,$entry.FullName,$name,$ext,[string]$entry.Length,$mtime,$prot,$app,$domain,$isDir,'not_hashed_zip_entry_inventory','streaming_central_directory_inventory_7z_or_dotnet')
  $ffsWriter.WriteLine((($ffsVals | ForEach-Object { Csv ([string]$_) }) -join ','))
  $script:zipInventoryFileCount++
  $cat = Get-DatabaseCategory $entry.FullName $name
  if ($cat[0]) {
    $extractPath = Get-ExtractedDbPath $entry.FullName
    try { if (-not (Test-Path -LiteralPath $extractPath)) { $extractPath = '' } } catch { $extractPath = '' }
    $parseStatus = if ($extractPath) { 'extracted_for_record_inventory_v0_9_6' } else { 'identified_not_extracted_v0_9_6' }
    $recordStatus = if ($extractPath) { 'database_extracted_record_count_pending' } else { 'database_family_present_record_level_parser_pending' }
    $dbVals = @($norm,$entry.FullName,$name,$cat[0],$cat[1],$prot,[string]$entry.Length,$mtime,$parseStatus,$recordStatus,'database-resident records require record-level correlation before PRESENT_AS_RECORD_IN_APP_DB can be asserted',$extractPath)
    $dbWriter.WriteLine((($dbVals | ForEach-Object { Csv ([string]$_) }) -join ','))
    $script:zipInventoryDbCount++
  }
  if (($script:zipInventoryFileCount % 10000) -eq 0) {
    try { $ffsWriter.Flush(); $dbWriter.Flush() } catch {}
    Write-ZipInventoryProgress 'ffs_inventory_streaming' $script:zipInventoryFileCount ("db_rows=" + $script:zipInventoryDbCount)
    Write-ZipStageHeartbeat ("streamed FFS inventory entries=" + $script:zipInventoryFileCount + " db_rows=" + $script:zipInventoryDbCount)
  }
}
function Write-FfsZipInventoryFromSevenZipStreaming([string]$SevenZipPath, [System.IO.StreamWriter]$ffsWriter, [System.IO.StreamWriter]$dbWriter) {
  Write-ZipStageHeartbeat "starting fast 7z ZIP entry inventory dump for C++ parser"
  Write-ZipInventoryProgress 'ffs_inventory_7z_dump_start' 0 'dumping 7z l -slt output to raw text; native C++ parser will build inventory CSVs after script returns'
  $script:zipInventoryFileCount = 0
  $script:zipInventoryDbCount = 0
  $rawListing = Join-Path (Split-Path $InventoryPath -Parent) "logs\ios_ffs_7z_inventory_raw_slt.txt"
  New-Item -ItemType Directory -Force -Path (Split-Path $rawListing -Parent) | Out-Null
  # Important: Windows PowerShell redirection writes UTF-16 text by default.
  # Use cmd.exe redirection so the native C++ parser receives the raw 7z byte stream.
  # V0_9_44 also keeps C++ fallback decoding for older UTF-16 raw listings.
  $cmdLine = '"' + $SevenZipPath + '" l "' + $ZipPath + '" -slt > "' + $rawListing + '" 2>NUL'
  & $env:ComSpec /d /c $cmdLine
  $sevenZipExit = $LASTEXITCODE
  Write-ZipInventoryProgress 'ffs_inventory_7z_dump_complete' 0 ("exit=" + $sevenZipExit + " raw=" + $rawListing)
  if ($sevenZipExit -ne 0 -and -not (Test-Path -LiteralPath $rawListing)) { throw "7z inventory listing failed exit=$sevenZipExit" }
  try { $ffsWriter.Flush(); $dbWriter.Flush() } catch {}
  Write-ZipInventoryProgress 'ffs_inventory_raw_ready_for_cpp' 0 ("raw=" + $rawListing)
  Write-ZipStageHeartbeat ("7z ZIP entry inventory raw listing ready for native C++ parser: " + $rawListing)
  "ios_ffs_inventory_raw_slt=$rawListing" | Write-Output
  "ios_ffs_inventory_file_rows=0" | Write-Output
  "ios_app_database_inventory_rows=0" | Write-Output
}
function Write-FfsZipInventory {
  New-Item -ItemType Directory -Force -Path (Split-Path $FfsInventoryPath -Parent) | Out-Null
  New-Item -ItemType Directory -Force -Path (Split-Path $DbInventoryPath -Parent) | Out-Null
  "normalized_path,original_zip_entry,file_name,extension,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,is_directory,sha256_status,inventory_notes" | Set-Content -LiteralPath $FfsInventoryPath -Encoding UTF8
  "normalized_path,original_zip_entry,database_name,database_category,app_hint,protection_class_hint,size_bytes,zip_modified_utc,parse_status,record_inventory_status,notes,extracted_path" | Set-Content -LiteralPath $DbInventoryPath -Encoding UTF8
  $SevenZipForInventory = Get-SevenZipPath
  $ffsWriter = [System.IO.StreamWriter]::new($FfsInventoryPath, $true, [System.Text.Encoding]::UTF8)
  $dbWriter = [System.IO.StreamWriter]::new($DbInventoryPath, $true, [System.Text.Encoding]::UTF8)
  try {
    if ($SevenZipForInventory) {
      Extract-KnownAppDatabases $SevenZipForInventory
      Write-FfsZipInventoryFromSevenZipStreaming $SevenZipForInventory $ffsWriter $dbWriter
      return
    }
    Write-ZipStageHeartbeat "7z not found; falling back to System.IO.Compression ZIP entry inventory"
    $script:zipInventoryFileCount = 0
    $script:zipInventoryDbCount = 0
    $entries = New-Object System.Collections.ArrayList
    try { $entries = Get-ZipEntriesViaDotNet } catch { "ios_ffs_inventory_dotnet_error=$($_.Exception.Message)" | Write-Output }
    foreach ($entry in $entries) { Write-InventoryRowsForEntry $entry $ffsWriter $dbWriter }
    try { $ffsWriter.Flush(); $dbWriter.Flush() } catch {}
    Write-ZipInventoryProgress 'ffs_inventory_dotnet_complete' $script:zipInventoryFileCount ("db_rows=" + $script:zipInventoryDbCount)
    "ios_ffs_inventory_file_rows=$script:zipInventoryFileCount" | Write-Output
    "ios_app_database_inventory_rows=$script:zipInventoryDbCount" | Write-Output
  } finally { $ffsWriter.Dispose(); $dbWriter.Dispose() }
}
try { Write-FfsZipInventory; "ios_ffs_inventory=$FfsInventoryPath" | Write-Output; "ios_app_database_inventory=$DbInventoryPath" | Write-Output } catch { "ios_ffs_inventory_error=$($_.Exception.Message)" | Write-Output }
$matched = 0
$stores = 0
$usedExtractor = "none"
$patterns = @(
  "-ir!*private\var\mobile\Library\Spotlight\CoreSpotlight\*",
  "-ir!*var\mobile\Library\Spotlight\CoreSpotlight\*",
  "-ir!*private\var\mobile\Library\Spotlight\BundleInfo\*",
  "-ir!*var\mobile\Library\Spotlight\BundleInfo\*",
  "-ir!*private\var\mobile\Library\Metadata\CoreSpotlight\*",
  "-ir!*var\mobile\Library\Metadata\CoreSpotlight\*",
  "-ir!*Library\Spotlight\CoreSpotlight\*",
  "-ir!*Library\Spotlight\BundleInfo\*",
  "-ir!*Library\Metadata\CoreSpotlight\*"
)
$sevenZipCandidates = @(
  "C:\Program Files\7-Zip\7z.exe",
  "C:\Program Files (x86)\7-Zip\7z.exe",
  "7z.exe"
)
$SevenZip = $null
foreach ($candidate in $sevenZipCandidates) {
  try {
    $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($cmd) { $SevenZip = $cmd.Source; break }
    if (Test-Path -LiteralPath $candidate) { $SevenZip = $candidate; break }
  } catch {}
}
function Write-StoreInventoryFromStage {
  param([string]$Root)
  $files = @(Get-ChildItem -LiteralPath $Root -Recurse -File -Force -ErrorAction SilentlyContinue)
  $script:matched = @($files | Where-Object {
      $n = $_.FullName.Replace('\','/').ToLowerInvariant()
)PS" R"PS(      $n.Contains('/library/spotlight/corespotlight/') -or
      $n.Contains('/library/spotlight/bundleinfo/') -or
      $n.Contains('/library/metadata/corespotlight/')
    }).Count
  $storeFiles = @($files | Where-Object { $_.Name -eq 'store.db' -or $_.Name -eq '.store.db' } | Sort-Object FullName)
  $script:stores = $storeFiles.Count
  foreach ($f in $storeFiles) {
    $line = ('{0},{1},{2},{3},{4}' -f $f.Name,$f.Length,$f.LastWriteTimeUtc.ToString('o'),('"' + ($f.FullName -replace '"','""') + '"'),('"' + ($f.FullName -replace '"','""') + '"'))
    Add-Content -LiteralPath $InventoryPath -Value $line -Encoding UTF8
  }
}
if ($SevenZip) {
  $usedExtractor = "7z:$SevenZip"
  "using_extractor=$usedExtractor" | Write-Output
  Write-ZipStageHeartbeat "7z found; starting focused iOS extraction stage"
  Extract-KnownAppDatabases $SevenZip
  $sevenZipExtractLog = Join-Path (Split-Path $InventoryPath -Parent) "logs\ios_focused_zip_extract_7z.log"
  New-Item -ItemType Directory -Force -Path (Split-Path $sevenZipExtractLog -Parent) | Out-Null
  & $SevenZip x $ZipPath "-o$StageRoot" @patterns -y > $sevenZipExtractLog 2>&1
  $sevenZipExit = $LASTEXITCODE
  Write-ZipStageHeartbeat "focused CoreSpotlight extraction completed exit=$sevenZipExit log=$sevenZipExtractLog"
  "7z_extract_exit=$sevenZipExit" | Write-Output
  # 7-Zip often returns success even when no wildcard matches. Inventory the staging folder either way.
  Write-StoreInventoryFromStage -Root $StageRoot
  try {
    $listSample = & $SevenZip l $ZipPath -ba -slt 2>$null | Select-String -Pattern 'Path = ' | Select-Object -First 200
    foreach ($item in $listSample) {
      $pathValue = ($item.Line -replace '^Path = ','')
      $line = '7z_list_sample,file,0,"' + ($pathValue -replace '"','""') + '"'
      Add-Content -LiteralPath $EntryProbePath -Value $line -Encoding UTF8
    }
  } catch {}
} else {
  $usedExtractor = "System.IO.Compression"
  "using_extractor=$usedExtractor" | Write-Output
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $zip = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
  try {
    $sampleCount = 0
    foreach ($entry in $zip.Entries) {
      if ($sampleCount -lt 200) {
        $probeLine = ('zipfile_sample,{0},{1},{2}' -f $entry.Name,$entry.Length,('"' + ($entry.FullName -replace '"','""') + '"'))
        Add-Content -LiteralPath $EntryProbePath -Value $probeLine -Encoding UTF8
        $sampleCount++
      }
      if ([string]::IsNullOrWhiteSpace($entry.FullName) -or [string]::IsNullOrWhiteSpace($entry.Name)) { continue }
      $full = $entry.FullName.Replace('\','/')
      $lower = $full.ToLowerInvariant()
      $isCoreSpotlight = ($lower -like '*library/spotlight/corespotlight/*' -or $lower -like '*library/spotlight/bundleinfo/*' -or $lower -like '*library/metadata/corespotlight/*')
      if (-not $isCoreSpotlight) { continue }
      $rel = $full.TrimStart('/').Replace(':','_')
      while ($rel.StartsWith('../')) { $rel = $rel.Substring(3) }
      $rel = $rel -replace '\.\./','_/'
)PS" R"PS(      $dest = Join-Path $StageRoot ($rel.Replace('/', [IO.Path]::DirectorySeparatorChar))
      New-Item -ItemType Directory -Force -Path (Split-Path $dest -Parent) | Out-Null
      [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dest, $true)
    }
  }
  finally { $zip.Dispose() }
  Write-StoreInventoryFromStage -Root $StageRoot
}
"matched_entries=$matched" | Write-Output
"store_db_entries=$stores" | Write-Output
"entry_probe=$EntryProbePath" | Write-Output
if ($matched -eq 0) {
  "No iOS CoreSpotlight/BundleInfo/Metadata CoreSpotlight entries were extracted from ZIP: $ZipPath" | Write-Output
  "This is now a diagnostic empty result, not an application crash. Review $EntryProbePath and the 7-Zip extraction log if available." | Write-Output
  if ($ThrowOnNoMatch) { exit 3 }
}
)PS";
    return scriptPath;
}


std::string jsonEscapeSimple(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) out += ' ';
            else out += c;
        }
    }
    return out;
}

std::uintmax_t fileSizeNoThrow(const fs::path& p) {
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) return 0;
    const auto n = fs::file_size(p, ec);
    return ec ? 0 : n;
}

std::string fileTimeRawNoThrow(const fs::path& p) {
    std::error_code ec;
    const auto t = fs::last_write_time(p, ec);
    if (ec) return {};
    return std::to_string(t.time_since_epoch().count());
}

bool fileTextContainsNoThrow(const fs::path& p, const std::string& needle) {
    if (needle.empty()) return false;
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str().find(needle) != std::string::npos;
}

bool hasUsableIosSourceCache(const fs::path& cacheDir) {
    std::error_code ec;
    return fs::is_directory(cacheDir, ec) &&
           fs::is_directory(cacheDir / "EvidenceStaging" / "zip_source" / "extracted", ec) &&
           fs::is_regular_file(cacheDir / "ios_input_store_entry_inventory.csv", ec) &&
           fs::is_regular_file(cacheDir / "ios_app_database_inventory.csv", ec);
}

void copySmallCacheFileNoThrow(const fs::path& cacheDir, const fs::path& caseDir, const std::string& name) {
    try {
        const fs::path src = cacheDir / name;
        const fs::path dst = caseDir / name;
        std::error_code ec;
        if (fs::is_regular_file(src, ec)) {
            fs::create_directories(dst.parent_path(), ec);
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        }
    } catch (...) {}
}

void writeSourceCacheManifest(const fs::path& caseDir,
                              const fs::path& sourceZip,
                              const fs::path& stageRoot,
                              const fs::path& reuseCacheDir,
                              const std::string& status,
                              Logger& log) {
    try {
        const fs::path manifest = caseDir / "source_cache_manifest.json";
        const fs::path inventoryPath = !reuseCacheDir.empty() ? (reuseCacheDir / "ios_ffs_file_inventory.csv") : (caseDir / "ios_ffs_file_inventory.csv");
        const fs::path appDbInventoryPath = !reuseCacheDir.empty() ? (reuseCacheDir / "ios_app_database_inventory.csv") : (caseDir / "ios_app_database_inventory.csv");
        std::ofstream out(manifest, std::ios::binary);
        out << "{\n";
        out << "  \"created_utc\": \"" << jsonEscapeSimple(nowUtc()) << "\",\n";
        out << "  \"created_by_version\": \"" << jsonEscapeSimple(appVersion()) << "\",\n";
        out << "  \"status\": \"" << jsonEscapeSimple(status) << "\",\n";
        out << "  \"case_path\": \"" << jsonEscapeSimple(pathString(caseDir)) << "\",\n";
        out << "  \"source_path\": \"" << jsonEscapeSimple(pathString(sourceZip)) << "\",\n";
        out << "  \"source_size_bytes\": " << fileSizeNoThrow(sourceZip) << ",\n";
        out << "  \"source_last_write_time_raw\": \"" << jsonEscapeSimple(fileTimeRawNoThrow(sourceZip)) << "\",\n";
        out << "  \"reuse_cache_path\": \"" << jsonEscapeSimple(pathString(reuseCacheDir)) << "\",\n";
        out << "  \"stage_root_path\": \"" << jsonEscapeSimple(pathString(stageRoot)) << "\",\n";
        out << "  \"ios_ffs_inventory_path\": \"" << jsonEscapeSimple(pathString(inventoryPath)) << "\",\n";
        out << "  \"ios_ffs_inventory_size_bytes\": " << fileSizeNoThrow(inventoryPath) << ",\n";
        out << "  \"ios_app_database_inventory_path\": \"" << jsonEscapeSimple(pathString(appDbInventoryPath)) << "\",\n";
        out << "  \"ios_app_database_inventory_size_bytes\": " << fileSizeNoThrow(appDbInventoryPath) << "\n";
        out << "}\n";
        log.info("Source cache manifest written: " + pathString(manifest));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write source cache manifest: ") + ex.what());
    } catch (...) {
        log.warn("Unable to write source cache manifest: unknown error");
    }
}

fs::path stageZipEvidenceSourceFromCache(const fs::path& cacheDir,
                                         const fs::path& zipPath,
                                         const fs::path& caseDir,
                                         Logger& log,
                                         bool* iosFocusedUsed = nullptr) {
    if (iosFocusedUsed) *iosFocusedUsed = false;
    if (cacheDir.empty()) throw std::runtime_error("reuse iOS cache path is empty");
    if (!hasUsableIosSourceCache(cacheDir)) {
        throw std::runtime_error("reuse iOS cache is missing required files: " + pathString(cacheDir) +
                                 " (expected EvidenceStaging/zip_source/extracted, ios_input_store_entry_inventory.csv, and ios_app_database_inventory.csv)");
    }
    fs::create_directories(caseDir / "logs");
    const fs::path stageRoot = cacheDir / "EvidenceStaging" / "zip_source" / "extracted";
    copySmallCacheFileNoThrow(cacheDir, caseDir, "ios_input_store_entry_inventory.csv");
    copySmallCacheFileNoThrow(cacheDir, caseDir, "ios_zip_entry_probe.csv");
    copySmallCacheFileNoThrow(cacheDir, caseDir, "store_inventory.csv");
    copySmallCacheFileNoThrow(cacheDir, caseDir, "store_selection.csv");
    copySmallCacheFileNoThrow(cacheDir, caseDir, "source_probe_summary.json");
    copySmallCacheFileNoThrow(cacheDir, caseDir, "source_probe_signatures.csv");
    const fs::path logPath = caseDir / "logs" / "ios_reuse_cache.log";
    {
        std::ofstream out(logPath, std::ios::binary);
        out << nowUtc() << " reuse_ios_cache_start\n";
        out << "cache_dir=" << pathString(cacheDir) << "\n";
        out << "source_zip=" << pathString(zipPath) << "\n";
        out << "cached_stage_root=" << pathString(stageRoot) << "\n";
        out << "cached_ffs_inventory=" << pathString(cacheDir / "ios_ffs_file_inventory.csv") << "\n";
        out << "cached_app_db_inventory=" << pathString(cacheDir / "ios_app_database_inventory.csv") << "\n";
        if (fileTextContainsNoThrow(cacheDir / "CASE_REVIEW_SUMMARY.txt", pathString(zipPath))) {
            out << "source_match_hint=CASE_REVIEW_SUMMARY_CONTAINS_CURRENT_SOURCE_PATH\n";
        } else {
            out << "source_match_hint=NOT_CONFIRMED_FROM_CASE_REVIEW_SUMMARY\n";
            out << "warning=Verify that the selected source ZIP is the same source used to create the cache before relying on forensic provenance.\n";
        }
    }
    if (iosFocusedUsed) *iosFocusedUsed = true;
    appendRunProgress(caseDir, 12, "stage_zip_source", "using cached iOS ZIP inventory/staging; skipping large ZIP listing/extraction");
    appendRunStatus(caseDir, "stage_zip_source_reuse_cache", "cache=" + pathString(cacheDir));
    log.info("Reusing iOS cache/staging from prior case: " + pathString(cacheDir));
    log.info("Cached CoreSpotlight stage root: " + pathString(stageRoot));
    log.info("Cached FFS inventory will be imported from: " + pathString(cacheDir / "ios_ffs_file_inventory.csv"));
    writeSourceCacheManifest(caseDir, zipPath, stageRoot, cacheDir, "REUSED_EXISTING_IOS_CACHE", log);
    return stageRoot;
}

struct IosZipInventoryParseResult {
    std::size_t ffsRows = 0;
    std::size_t appDbRows = 0;
    std::size_t rawRecords = 0;
    std::string status = "NOT_RUN";
};

void appendUtf8Codepoint(std::string& out, std::uint32_t cp) {
    if (cp == 0) return;
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0x10FFFFu) {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

std::string decodeSevenZipRawLine(std::string line) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEFu &&
        static_cast<unsigned char>(line[1]) == 0xBBu &&
        static_cast<unsigned char>(line[2]) == 0xBFu) {
        line.erase(0, 3);
    }
    if (line.size() >= 2 &&
        static_cast<unsigned char>(line[0]) == 0xFFu &&
        static_cast<unsigned char>(line[1]) == 0xFEu) {
        line.erase(0, 2);
    } else if (line.size() >= 2 &&
               static_cast<unsigned char>(line[0]) == 0xFEu &&
               static_cast<unsigned char>(line[1]) == 0xFFu) {
        line.erase(0, 2);
    }
    if (line.find('\0') == std::string::npos) return line;

    std::size_t evenNulls = 0, oddNulls = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\0') {
            if ((i % 2) == 0) ++evenNulls;
            else ++oddNulls;
        }
    }
    std::string out;
    out.reserve(line.size());
    if (oddNulls >= evenNulls) {
        for (std::size_t i = 0; i + 1 < line.size(); i += 2) {
            const auto lo = static_cast<unsigned char>(line[i]);
            const auto hi = static_cast<unsigned char>(line[i + 1]);
            appendUtf8Codepoint(out, static_cast<std::uint32_t>(lo) | (static_cast<std::uint32_t>(hi) << 8));
        }
        if ((line.size() % 2) == 1 && line.back() != '\0') out.push_back(line.back());
    } else {
        for (std::size_t i = 0; i + 1 < line.size(); i += 2) {
            const auto hi = static_cast<unsigned char>(line[i]);
            const auto lo = static_cast<unsigned char>(line[i + 1]);
            appendUtf8Codepoint(out, (static_cast<std::uint32_t>(hi) << 8) | static_cast<std::uint32_t>(lo));
        }
        if ((line.size() % 2) == 1 && line.back() != '\0') out.push_back(line.back());
    }
    return out;
}

IosZipInventoryParseResult parseIosSevenZipRawInventoryToCsv(const fs::path& caseDir, const fs::path& zipPath, Logger& log) {
    IosZipInventoryParseResult result;
    const fs::path rawListing = caseDir / "logs" / "ios_ffs_7z_inventory_raw_slt.txt";
    const fs::path ffsInventoryPath = caseDir / "ios_ffs_file_inventory.csv";
    const fs::path dbInventoryPath = caseDir / "ios_app_database_inventory.csv";
    if (!fs::is_regular_file(rawListing)) {
        result.status = "RAW_LISTING_NOT_FOUND";
        return result;
    }
    std::ifstream in(rawListing, std::ios::binary);
    if (!in) {
        result.status = "RAW_LISTING_OPEN_FAILED";
        return result;
    }
    fs::create_directories(ffsInventoryPath.parent_path());
    std::ofstream ffsOut(ffsInventoryPath, std::ios::binary);
    std::ofstream dbOut(dbInventoryPath, std::ios::binary);
    if (!ffsOut || !dbOut) throw std::runtime_error("Unable to write native C++ iOS ZIP inventory CSVs");
    ffsOut << "normalized_path,original_zip_entry,file_name,extension,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,is_directory,sha256_status,inventory_notes\n";
    dbOut << "normalized_path,original_zip_entry,database_name,database_category,app_hint,protection_class_hint,size_bytes,zip_modified_utc,parse_status,record_inventory_status,notes,extracted_path\n";
    std::map<std::string, std::string> rec;
    const std::string zipPathNorm = toLower(pathString(zipPath));
    auto flush = [&]() {
        auto it = rec.find("Path");
        if (it == rec.end() || trim(it->second).empty()) { rec.clear(); return; }
        const std::string fullName = it->second;
        const std::string fullLow = toLower(fullName);
        if ((fullLow.find(":/") != std::string::npos || fullLow.find(":\\") != std::string::npos) && endsWithCpp(fullLow, ".zip")) { rec.clear(); return; }
        if (!zipPathNorm.empty() && toLower(fullName) == zipPathNorm) { rec.clear(); return; }
        const std::string norm = normalizeIosPathFromZipEntryCpp(fullName);
        const std::string name = basenameFromZipEntryCpp(fullName);
        const std::string ext = extensionFromNameCpp(name);
        const std::string size = rec.count("Size") ? rec["Size"] : "";
        const std::string modified = rec.count("Modified") ? rec["Modified"] : "";
        const std::string folder = rec.count("Folder") ? toLower(trim(rec["Folder"])) : "";
        const bool isDir = (folder == "+" || folder == "true" || folder == "1" || folder == "yes");
        const std::string prot = protectionClassHintCpp(norm);
        const std::string app = appContainerHintCpp(fullName);
        const std::string domain = domainHintCpp(norm);
        const std::string note = "native_cpp_7z_slt_inventory_parser";
        ffsOut << csvEscape(norm) << ',' << csvEscape(fullName) << ',' << csvEscape(name) << ',' << csvEscape(ext) << ','
               << csvEscape(size) << ',' << csvEscape(modified) << ',' << csvEscape(prot) << ',' << csvEscape(app) << ','
               << csvEscape(domain) << ',' << (isDir ? "1" : "0") << ",not_hashed_zip_entry_inventory," << csvEscape(note) << "\n";
        ++result.ffsRows;
        auto cat = databaseCategoryAndAppHintCpp(norm, name);
        if (!cat.first.empty()) {
            const fs::path extractedPath = extractedIosAppDbPathForZipEntryCpp(caseDir, fullName);
            std::error_code extractedEc;
            const bool extractedExists = fs::is_regular_file(extractedPath, extractedEc);
            const std::string parseStatus = extractedExists ? "extracted_for_record_inventory_v0_9_45" : "identified_not_extracted_v0_9_45";
            const std::string recordStatus = extractedExists ? "database_extracted_record_count_pending" : "database_family_present_record_level_parser_pending";
            dbOut << csvEscape(norm) << ',' << csvEscape(fullName) << ',' << csvEscape(name) << ',' << csvEscape(cat.first) << ','
                  << csvEscape(cat.second) << ',' << csvEscape(prot) << ',' << csvEscape(size) << ',' << csvEscape(modified) << ','
                  << csvEscape(parseStatus) << ',' << csvEscape(recordStatus) << ','
                  << csvEscape("native_cpp_7z_slt_inventory_parser_database_like_only") << ','
                  << csvEscape(extractedExists ? pathString(extractedPath) : "") << "\n";
            ++result.appDbRows;
        }
        ++result.rawRecords;
        if ((result.ffsRows % 100000) == 0) {
            appendRunStatus(caseDir, "ios_ffs_inventory_cpp_parser_progress", "files=" + std::to_string(result.ffsRows) + " app_databases=" + std::to_string(result.appDbRows));
        }
        rec.clear();
    };
    std::string line;
    while (std::getline(in, line)) {
        line = decodeSevenZipRawLine(std::move(line));
        if (trim(line).empty()) { flush(); continue; }
        const std::string sep = " = ";
        const auto pos = line.find(sep);
        if (pos != std::string::npos) {
            rec[trim(line.substr(0, pos))] = line.substr(pos + sep.size());
        }
    }
    flush();
    result.status = "NATIVE_CPP_7Z_RAW_INVENTORY_PARSED";
    appendRunStatus(caseDir, "ios_ffs_inventory_cpp_parser_complete", "files=" + std::to_string(result.ffsRows) + " app_databases=" + std::to_string(result.appDbRows) + " raw_records=" + std::to_string(result.rawRecords));
    if (result.rawRecords == 0) {
        appendRunStatus(caseDir, "ios_ffs_inventory_cpp_parser_warning", "raw 7z listing parsed but produced zero records; check encoding/redirection and raw listing contents");
        log.warn("Native C++ parsed 7z raw iOS ZIP inventory but produced zero records. Check raw listing encoding/content: " + pathString(rawListing));
    }
    log.info("Native C++ parsed 7z raw iOS ZIP inventory. files=" + std::to_string(result.ffsRows) + " app_databases=" + std::to_string(result.appDbRows) + " raw=" + pathString(rawListing));
    return result;
}

fs::path stageZipEvidenceSource(const fs::path& zipPath, const fs::path& caseDir, SourceProfileKind profile, Logger& log, bool* iosFocusedUsed = nullptr) {
    const fs::path stageRoot = caseDir / "EvidenceStaging" / "zip_source" / "extracted";
    std::error_code ec;
    fs::remove_all(stageRoot, ec);
    fs::create_directories(stageRoot);
#if defined(_WIN32)
    if (iosFocusedUsed) *iosFocusedUsed = false;
    fs::create_directories(caseDir / "logs");
    const bool tryIosFocused = (profile == SourceProfileKind::IOS || profile == SourceProfileKind::Auto);
    bool runGenericZipExtraction = true;
    if (tryIosFocused) {
        const fs::path logPath = caseDir / "logs" / "ios_focused_zip_extract.log";
        const fs::path inventoryPath = caseDir / "ios_input_store_entry_inventory.csv";
        const bool requireIosMatch = (profile == SourceProfileKind::IOS);
        const fs::path scriptPath = writeIosFocusedZipExtractorScript(caseDir, zipPath, stageRoot, inventoryPath, requireIosMatch);
        std::string cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File " + commandQuote(scriptPath) + " > " + commandQuote(logPath) + " 2>&1";
        log.info(std::string(profile == SourceProfileKind::IOS ? "iOS ZIP source detected" : "Auto ZIP source: probing for iOS CoreSpotlight entries") +
                 ". Extracting CoreSpotlight/BundleInfo entries to controlled staging folder before discovery: " + pathString(stageRoot));
        log.info("iOS focused ZIP extraction inventory will be written: " + pathString(inventoryPath));
#if defined(_WIN32)
        const int rc = runPowerShellFileNoWindowRedirected(scriptPath, logPath);
#else
        const int rc = runShellCommandNoWindow(cmd);
#endif
        IosZipInventoryParseResult cppInventoryParse;
        try {
            cppInventoryParse = parseIosSevenZipRawInventoryToCsv(caseDir, zipPath, log);
            if (cppInventoryParse.status == "NATIVE_CPP_7Z_RAW_INVENTORY_PARSED") {
                appendRunStatus(caseDir, "ios_ffs_inventory_native_cpp_ready", "files=" + std::to_string(cppInventoryParse.ffsRows) + " app_databases=" + std::to_string(cppInventoryParse.appDbRows));
            }
        } catch (const std::exception& ex) {
            appendRunStatus(caseDir, "ios_ffs_inventory_native_cpp_warning", ex.what());
            log.warn(std::string("Native C++ iOS ZIP inventory parser did not complete; using script inventory output if present: ") + ex.what());
        } catch (...) {
            appendRunStatus(caseDir, "ios_ffs_inventory_native_cpp_warning", "unknown error");
            log.warn("Native C++ iOS ZIP inventory parser did not complete; using script inventory output if present: unknown error");
        }
        if (rc != 0) {
            if (profile == SourceProfileKind::IOS && rc != 3) {
                throw std::runtime_error("iOS focused ZIP extraction failed. See log: " + pathString(logPath));
            }
            if (profile == SourceProfileKind::IOS && rc == 3) {
                runGenericZipExtraction = false;
                log.warn("iOS focused ZIP extraction found no CoreSpotlight entries. Continuing with focused empty staging folder for explicit diagnostics. See log: " + pathString(logPath));
            } else {
                log.warn("Auto ZIP iOS focused extraction did not complete; falling back to generic ZIP extraction. See log: " + pathString(logPath));
            }
        } else {
            const std::size_t storeEntryRows = countCsvDataRows(inventoryPath);
            if (storeEntryRows > 0) {
                if (iosFocusedUsed) *iosFocusedUsed = true;
                runGenericZipExtraction = false;
                log.info("iOS focused ZIP extraction completed and found store.db/.store.db entries=" + std::to_string(storeEntryRows) + ". Discovery will use the focused CoreSpotlight staging folder.");
            } else if (profile == SourceProfileKind::IOS) {
                runGenericZipExtraction = false;
                log.warn("iOS focused ZIP extraction completed but found zero store.db/.store.db entries. Discovery will continue against the focused staging folder so the empty iOS result is explicit.");
            } else {
                log.info("Auto ZIP iOS focused extraction found no store.db/.store.db entries; falling back to generic ZIP extraction for macOS/folder-style Spotlight discovery.");
            }
        }
    }
    if (runGenericZipExtraction) {
        std::error_code rec;
        fs::remove_all(stageRoot, rec);
        fs::create_directories(stageRoot);
        const fs::path logPath = caseDir / "logs" / "zip_extract_powershell.log";
        fs::create_directories(logPath.parent_path());
        std::string cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath " + psSingleQuote(platformPathString(zipPath)) + " -DestinationPath " + psSingleQuote(platformPathString(stageRoot)) + " -Force\" > " + commandQuote(logPath) + " 2>&1";
        const std::string psCommand = "Expand-Archive -LiteralPath " + psSingleQuote(platformPathString(zipPath)) + " -DestinationPath " + psSingleQuote(platformPathString(stageRoot)) + " -Force";
        log.info("ZIP source detected. Extracting to controlled staging folder before Store-V2 discovery: " + pathString(stageRoot));
#if defined(_WIN32)
        const int rc = runPowerShellCommandNoWindowRedirected(psCommand, logPath);
#else
        const int rc = runShellCommandNoWindow(cmd);
#endif
        if (rc != 0) throw std::runtime_error("ZIP extraction failed. See log: " + pathString(logPath));
    }
#else
    std::string cmd = "unzip -q -o " + commandQuote(zipPath) + " -d " + commandQuote(stageRoot);
    log.info("ZIP source detected. Extracting to controlled staging folder before Store-V2 discovery: " + pathString(stageRoot));
    const int rc = runShellCommandNoWindow(cmd);
    if (rc != 0) throw std::runtime_error("ZIP extraction failed using unzip for source: " + pathString(zipPath));
#endif
    return stageRoot;
}






long long purgeOrphanSourceRows(CaseDatabase& db, const fs::path& caseDir, Logger& log) {
    // Versioned GUI test runs often reuse or copy case folders while the evidence_sources
    // table is rewritten for the active source.  Older source_id rows in raw/enrichment/iOS
    // inventory tables can then contaminate exports and summaries.  Keep rows that still
    // have a registered evidence source and remove orphan rows before export.
    const std::vector<std::string> tables = {
        "raw_records", "raw_key_values", "raw_date_candidates", "raw_failures",
        "native_decode_attempts", "native_property_dictionary",
        "artifacts", "artifact_source_instances", "source_copy_comparison",
        "parent_inode_links", "field_inventory", "parser_coverage_summary",
        "usage_evidence", "timeline_events", "artifact_date_summary",
        "orphaned_deleted_candidates", "external_volume_candidates",
        "ios_ffs_file_inventory", "ios_app_database_inventory",
        "ios_app_database_record_inventory", "ios_app_parsed_records",
        "image_inventory_sources", "image_file_inventory", "active_file_comparison_runs", "source_probe_runs", "source_probe_signatures",
        "source_partition_probe"
    };
    long long total = 0;
    for (const auto& table : tables) {
        try {
            const std::string sql = "DELETE FROM " + table + " WHERE source_id IS NOT NULL AND source_id<>'' AND source_id NOT IN (SELECT source_id FROM evidence_sources)";
            db.exec(sql);
            const long long changes = sqlite3_changes(db.raw());
            if (changes > 0) {
                total += changes;
                log.info("Purged orphan source rows from " + table + ": " + std::to_string(changes));
            }
        } catch (const std::exception& ex) {
            log.warn("Unable to purge orphan source rows from " + table + ": " + ex.what());
        }
    }
    if (total > 0) {
        appendRunStatus(caseDir, "orphan_source_rows_purged", "rows=" + std::to_string(total));
    } else {
        appendRunStatus(caseDir, "orphan_source_rows_purge_checked", "rows=0");
    }
    return total;
}


void parseIosAppDatabaseRecordInventories(CaseDatabase& db, const fs::path& caseDir, const std::string& sourceId, Logger& log) {
    IosAppDbParser::parseRecordInventories(db, caseDir, sourceId, log, appendRunStatus);
}

std::string containerFormatForPath(const fs::path& p) {
    if (isZipSourcePath(p)) return "original_zip";
    if (isAff4SourcePath(p)) return "original_aff4";
    if (isRawImageSourcePath(p)) return "original_raw_image";
    return "original_container";
}

std::string containerRoleForPath(const fs::path& p) {
    if (isZipSourcePath(p)) return "original_zip_container";
    if (isAff4SourcePath(p)) return "original_aff4_container";
    if (isRawImageSourcePath(p)) return "original_raw_image_container";
    return "original_container";
}

void registerOriginalContainerSource(CaseDatabase& db,
                                     const EvidenceSource& source,
                                     const fs::path& originalContainer,
                                     const fs::path& stagedWorkingRoot,
                                     const std::string& notes,
                                     Logger& log,
                                     bool skipHash) {
    std::uintmax_t sizeBytes = 0;
    std::string sha;
    try {
        std::error_code ec;
        if (fs::exists(originalContainer, ec) && fs::is_regular_file(originalContainer, ec)) {
            sizeBytes = fs::file_size(originalContainer, ec);
            if (ec) sizeBytes = 0;
            if (skipHash) {
                log.warn("Original container SHA256 deferred for this source-probe/development run: " + pathString(originalContainer));
            } else {
                sha = sha256File(originalContainer);
            }
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to hash/register original container source: ") + ex.what());
    }

    const std::string format = containerFormatForPath(originalContainer);
    const std::string role = containerRoleForPath(originalContainer);
    const std::string baseNote = notes.empty()
        ? "Original evidence source is already a fixed container/image. No new evidentiary archive was created; original source was hashed and registered."
        : notes;

    db.begin();
    try {
        auto setStmt = db.prepare("INSERT INTO preserved_evidence_sets(source_id,archive_path,archive_format,archive_sha256,archive_size_bytes,created_utc,tool_used,tool_version,original_root_path,preserved_root_path,file_count,total_original_bytes,preservation_status,integrity_status,notes) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        int i = 1;
        setStmt.bind(i++, source.sourceId);
        setStmt.bind(i++, pathString(originalContainer));
        setStmt.bind(i++, format);
        setStmt.bind(i++, sha);
        setStmt.bind(i++, static_cast<long long>(sizeBytes));
        setStmt.bind(i++, nowUtc());
        setStmt.bind(i++, "Vestigant source registrar");
        setStmt.bind(i++, appVersion());
        setStmt.bind(i++, pathString(originalContainer));
        setStmt.bind(i++, pathString(stagedWorkingRoot));
        setStmt.bind(i++, 1LL);
        setStmt.bind(i++, static_cast<long long>(sizeBytes));
        setStmt.bind(i++, "ORIGINAL_CONTAINER_REGISTERED_NO_REARCHIVE");
        setStmt.bind(i++, skipHash ? "HASH_DEFERRED" : (sha.empty() ? "HASH_NOT_AVAILABLE" : "HASHED"));
        setStmt.bind(i++, baseNote);
        setStmt.stepDone();

        auto fileStmt = db.prepare("INSERT INTO preserved_evidence_files(source_id,relative_path,original_absolute_path,preserved_path,file_role,size_bytes,sha256,included_in_archive,created_utc) VALUES(?,?,?,?,?,?,?,?,?)");
        int f = 1;
        fileStmt.bind(f++, source.sourceId);
        fileStmt.bind(f++, originalContainer.filename().string());
        fileStmt.bind(f++, pathString(originalContainer));
        fileStmt.bind(f++, pathString(stagedWorkingRoot));
        fileStmt.bind(f++, role);
        fileStmt.bind(f++, static_cast<long long>(sizeBytes));
        fileStmt.bind(f++, sha);
        fileStmt.bind(f++, 0LL);
        fileStmt.bind(f++, nowUtc());
        fileStmt.stepDone();

        auto checkStmt = db.prepare("INSERT INTO archive_integrity_checks(source_id,archive_path,check_type,check_started_utc,check_finished_utc,result,message) VALUES(?,?,?,?,?,?,?)");
        checkStmt.bind(1, source.sourceId);
        checkStmt.bind(2, pathString(originalContainer));
        checkStmt.bind(3, "original_container_sha256");
        checkStmt.bind(4, nowUtc());
        checkStmt.bind(5, nowUtc());
        checkStmt.bind(6, skipHash ? "DEFERRED_BY_OPERATOR" : (sha.empty() ? "NOT_RUN" : "HASHED"));
        checkStmt.bind(7, baseNote);
        checkStmt.stepDone();

        db.commit();
    } catch (...) {
        db.rollbackNoThrow();
        throw;
    }

    log.info("Original container registered without creating a new evidentiary archive: " + pathString(originalContainer) +
             " size=" + std::to_string(sizeBytes) +
             " sha256=" + (sha.empty() ? std::string("<not available>") : sha));
}

} // namespace

std::string hexSampleBytes(const unsigned char* data, std::size_t n) {
    if (!data || n == 0) return {};
    std::string out;
    const std::size_t limit = std::min<std::size_t>(n, 32);
    for (std::size_t i = 0; i < limit; ++i) {
        if (i) out += ' ';
        out += hexByte(static_cast<unsigned int>(data[i]));
    }
    return out;
}

std::string directPreviewStatusForBytes(const std::vector<unsigned char>& bytes) {
    if (bytes.size() >= 16U && std::memcmp(bytes.data(), "SQLite format 3", 15U) == 0) return "SQLITE_HEADER";
    if (bytes.size() >= 6U && std::memcmp(bytes.data(), "bplist", 6U) == 0) return "BPLIST_HEADER";
    bool anyNonZero = false;
    for (unsigned char b : bytes) {
        if (b != 0U) { anyNonZero = true; break; }
    }
    return anyNonZero ? "NONZERO_BYTES" : "ALL_ZERO_BYTES";
}

struct Aff4DynamicProbeRow {
    std::string step;
    std::string status;
    fs::path path;
    std::uint64_t objectSize = 0;
    std::uint64_t offset = 0;
    long long bytesRead = -1;
    std::string sampleHex;
    std::string notes;
};

std::string bytesToUuidString(const std::vector<unsigned char>& data, std::size_t off) {
    if (off + 16 > data.size()) return {};
    auto hx = [](unsigned char b) -> std::string {
        return std::string(1, "0123456789abcdef"[(b >> 4) & 0xF]) + std::string(1, "0123456789abcdef"[b & 0xF]);
    };
    std::string out;
    out += hx(data[off + 0]); out += hx(data[off + 1]); out += hx(data[off + 2]); out += hx(data[off + 3]); out += '-';
    out += hx(data[off + 4]); out += hx(data[off + 5]); out += '-';
    out += hx(data[off + 6]); out += hx(data[off + 7]); out += '-';
    out += hx(data[off + 8]); out += hx(data[off + 9]); out += '-';
    for (std::size_t i = 10; i < 16; ++i) out += hx(data[off + i]);
    return out;
}

std::string joinU64List(const std::vector<std::uint64_t>& values, std::size_t maxCount = 32) {
    std::string out;
    const std::size_t n = std::min<std::size_t>(values.size(), maxCount);
    for (std::size_t i = 0; i < n; ++i) {
        if (i) out += ";";
        out += std::to_string(values[i]);
    }
    if (values.size() > n) out += ";...";
    return out;
}

std::string apfsObjectTypeLabel(std::uint32_t rawType) {
    const std::uint32_t base = rawType & 0x0000ffffU;
    switch (base) {
        case 0x0001: return "NX_SUPERBLOCK";
        case 0x0002: return "BTREE_NODE";
        case 0x0003: return "BTREE";
        case 0x0005: return "SPACEMAN";
        case 0x0006: return "SPACEMAN_CAB";
        case 0x0007: return "SPACEMAN_CIB";
        case 0x000b: return "OBJECT_MAP";
        case 0x000c: return "CHECKPOINT_MAP";
        case 0x000d: return "VOLUME_SUPERBLOCK";
        case 0x0011: return "REAPER";
        default: return std::string("OBJECT_TYPE_0x") + hexByte((rawType >> 8) & 0xffU) + hexByte(rawType & 0xffU);
    }
}








std::string readFixedUtf8Z(const std::vector<unsigned char>& data, std::size_t off, std::size_t maxLen) {
    if (off >= data.size() || maxLen == 0) return {};
    const std::size_t end = std::min<std::size_t>(data.size(), off + maxLen);
    std::string out;
    out.reserve(end - off);
    for (std::size_t i = off; i < end; ++i) {
        const unsigned char c = data[i];
        if (c == 0) break;
        if (c == '\r' || c == '\n' || c == '\t') out.push_back(' ');
        else out.push_back(static_cast<char>(c));
    }
    while (!out.empty() && static_cast<unsigned char>(out.back()) <= 0x20U) out.pop_back();
    return out;
}

















struct ApfsDirectoryRecordEntry {
    std::uint32_t volumeSequence = 0;
    std::string targetRole;
    std::uint64_t fsOid = 0;
    std::string volumeName;
    std::uint64_t parentObjectId = 0;
    std::uint64_t childFileId = 0;
    std::string name;
};










struct ApfsOmapTargetResolution {
    std::uint64_t targetOid = 0;
    std::uint64_t targetXid = 0;
    std::uint32_t branchDepth = 0;
    std::string branchPath;
    std::uint64_t leafOid = 0;
    std::uint64_t leafVirtualOffset = 0;
    long long leafBytesRead = -1;
    std::uint16_t leafBtnFlags = 0;
    std::uint16_t leafBtnLevel = 0;
    std::uint32_t leafBtnNkeys = 0;
    std::uint32_t matchedEntryIndex = 0;
    std::uint64_t matchedKeyOid = 0;
    std::uint64_t matchedKeyXid = 0;
    std::uint32_t valueFlags = 0;
    std::uint32_t valueSize = 0;
    std::uint64_t valuePaddr = 0;
    std::uint64_t resolvedVirtualOffset = 0;
    long long resolvedBytesRead = -1;
    std::uint64_t resolvedObjectOid = 0;
    std::uint64_t resolvedObjectXid = 0;
    std::uint32_t resolvedObjectTypeRaw = 0;
    std::string resolvedObjectTypeLabel;
    std::uint32_t resolvedObjectSubtype = 0;
    std::string resolvedMagic;
    std::uint16_t resolvedBtnFlags = 0;
    std::uint16_t resolvedBtnLevel = 0;
    std::uint32_t resolvedBtnNkeys = 0;
    std::string lookupStatus;
    std::string objectStatus;
    std::string interpretation;
    std::string sampleHex;
    std::string resolvedSampleHex;
    std::string notes;
    std::vector<unsigned char> resolvedBuffer;
};

bool aff4ApfsFixedKvAbsForProbe(const std::vector<unsigned char>& node,
                                std::uint32_t entryIndex,
                                std::size_t valueLenNeeded,
                                std::size_t& keyAbs,
                                std::size_t& valAbs,
                                std::string& detail) {
    return apfsAff4DecodeFixedKvAbs(node, entryIndex, valueLenNeeded, keyAbs, valAbs, detail);
}


int aff4ApfsCompareOmapKeyForProbe(std::uint64_t oid, std::uint64_t xid, std::uint64_t targetOid, std::uint64_t targetXid) {
    if (oid < targetOid) return -1;
    if (oid > targetOid) return 1;
    if (xid < targetXid) return -1;
    if (xid > targetXid) return 1;
    return 0;
}

void aff4ApfsAppendBranchPathForProbe(std::string& path, const std::string& segment) {
    if (!path.empty()) path += " -> ";
    path += segment;
}

bool aff4GenericBtreeKvAbsForProbe(const std::vector<unsigned char>& node,
                                   std::uint32_t entryIndex,
                                   std::size_t& tocAbs,
                                   std::size_t& keyAbs,
                                   std::size_t& keyLen,
                                   std::size_t& valAbs,
                                   std::size_t& valLen,
                                   std::string& detail) {
    return apfsAff4DecodeGenericBtreeKvAbs(node, entryIndex, tocAbs, keyAbs, keyLen, valAbs, valLen, detail);
}


ApfsOmapTargetResolution aff4ResolveVolumeOmapTargetObjectForProbe(
    const ApfsVolumeOmapProbeRow& omRow,
    const std::vector<unsigned char>& rootNode,
    std::uint64_t targetOid,
    std::uint64_t targetXid,
    std::uint32_t blockSize,
    const std::function<long long(std::uint64_t, std::uint64_t, std::vector<unsigned char>&, std::string&)>& readVirtual,
    const std::function<bool(std::uint64_t, std::uint64_t&)>& safeNodeOffset,
    const std::string& purpose) {
    ApfsOmapTargetResolution out;
    out.targetOid = targetOid;
    out.targetXid = targetXid;
    if (targetOid == 0) {
        out.lookupStatus = "OMAP_TARGET_OID_ZERO";
        out.interpretation = purpose + ": target OID was zero.";
        return out;
    }
    if (rootNode.empty() || omRow.treeStatus != "VOLUME_OMAP_BTREE_ROOT_READ") {
        out.lookupStatus = "VOLUME_OMAP_TREE_UNAVAILABLE";
        out.interpretation = purpose + ": volume OMAP B-tree root was not available.";
        out.notes = "volume_omap_tree_status=" + omRow.treeStatus;
        return out;
    }

    std::vector<unsigned char> node = rootNode;
    std::uint64_t nodeOid = omRow.omTreeOid;
    std::uint64_t nodeOffset = omRow.treeVirtualOffset;
    long long nodeRead = omRow.treeBytesRead;
    constexpr std::uint32_t kMaxDepth = 8;
    for (std::uint32_t depth = 0; depth < kMaxDepth; ++depth) {
        out.branchDepth = depth;
        if (node.size() < 64) {
            out.lookupStatus = "VOLUME_OMAP_NODE_TOO_SMALL";
            out.interpretation = purpose + ": OMAP node read returned too few bytes for B-tree header parsing.";
            break;
        }
        const std::uint32_t rawType = readLe32(node, 24);
        const std::string label = apfsObjectTypeLabel(rawType);
        const std::uint16_t btnFlags = readLe16(node, 32);
        const std::uint16_t btnLevel = readLe16(node, 34);
        const std::uint32_t nkeys = readLe32(node, 36);
        aff4ApfsAppendBranchPathForProbe(out.branchPath, "oid=" + std::to_string(nodeOid) + ";level=" + std::to_string(btnLevel) + ";nkeys=" + std::to_string(nkeys));
        if (label != "BTREE" && label != "BTREE_NODE") {
            out.lookupStatus = "VOLUME_OMAP_NODE_UNEXPECTED_OBJECT_TYPE";
            out.interpretation = purpose + ": node in the volume OMAP path was not an APFS B-tree node.";
            out.notes = "object_type=" + label;
            break;
        }
        if (nkeys > 100000U) {
            out.lookupStatus = "VOLUME_OMAP_NODE_SUSPICIOUS_KEY_COUNT";
            out.interpretation = purpose + ": OMAP node reported an implausibly large key count; traversal stopped.";
            out.notes = "nkeys=" + std::to_string(nkeys);
            break;
        }
        if (btnLevel == 0) {
            out.leafOid = nodeOid;
            out.leafVirtualOffset = nodeOffset;
            out.leafBytesRead = nodeRead;
            out.leafBtnFlags = btnFlags;
            out.leafBtnLevel = btnLevel;
            out.leafBtnNkeys = nkeys;
            out.sampleHex = node.empty() ? std::string{} : hexSampleBytes(node.data(), node.size() < 96 ? node.size() : 96);
            bool found = false;
            std::uint64_t bestXid = 0;
            std::string bestDetail;
            const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
            for (std::uint32_t i = 0; i < limit; ++i) {
                std::size_t keyAbs = 0;
                std::size_t valAbs = 0;
                std::string detail;
                if (!aff4ApfsFixedKvAbsForProbe(node, i, 16U, keyAbs, valAbs, detail)) {
                    if (i == 0 && out.notes.empty()) out.notes = detail;
                    continue;
                }
                const std::uint64_t keyOid = readLe64(node, keyAbs + 0U);
                const std::uint64_t keyXid = readLe64(node, keyAbs + 8U);
                if (keyOid != targetOid || keyXid > targetXid) continue;
                if (!found || keyXid >= bestXid) {
                    found = true;
                    bestXid = keyXid;
                    bestDetail = detail;
                    out.matchedEntryIndex = i;
                    out.matchedKeyOid = keyOid;
                    out.matchedKeyXid = keyXid;
                    out.valueFlags = readLe32(node, valAbs + 0U);
                    out.valueSize = readLe32(node, valAbs + 4U);
                    out.valuePaddr = readLe64(node, valAbs + 8U);
                }
            }
            if (!found) {
                out.lookupStatus = "OMAP_TARGET_LOOKUP_NO_MATCH_IN_LEAF";
                out.interpretation = purpose + ": volume OMAP leaf was reached, but no key with xid <= target transaction was found.";
                break;
            }
            out.lookupStatus = "OMAP_TARGET_LOOKUP_RESOLVED";
            out.notes += out.notes.empty() ? bestDetail : ("; " + bestDetail);
            if ((out.valueFlags & 0x00000001U) != 0U) {
                out.objectStatus = "OMAP_TARGET_VALUE_DELETED";
                out.interpretation = purpose + ": matching OMAP value was marked deleted; object not read as active.";
                break;
            }
            if (!safeNodeOffset(out.valuePaddr, out.resolvedVirtualOffset)) {
                out.objectStatus = "OMAP_TARGET_RESOLVED_OFFSET_UNSAFE";
                out.interpretation = purpose + ": matching OMAP value was found, but value_paddr could not be converted to a safe read offset.";
                break;
            }
            std::vector<unsigned char> resolved;
            std::string resolvedErr;
            out.resolvedBytesRead = readVirtual(out.resolvedVirtualOffset, blockSize, resolved, resolvedErr);
            if (out.resolvedBytesRead > 0 && resolved.size() >= 56) {
                out.resolvedObjectOid = readLe64(resolved, 8);
                out.resolvedObjectXid = readLe64(resolved, 16);
                out.resolvedObjectTypeRaw = readLe32(resolved, 24);
                out.resolvedObjectTypeLabel = apfsObjectTypeLabel(out.resolvedObjectTypeRaw);
                out.resolvedObjectSubtype = readLe32(resolved, 28);
                if (resolved.size() >= 36) out.resolvedMagic.assign(reinterpret_cast<const char*>(resolved.data() + 32), reinterpret_cast<const char*>(resolved.data() + 36));
                if (out.resolvedObjectTypeLabel == "BTREE" || out.resolvedObjectTypeLabel == "BTREE_NODE") {
                    out.resolvedBtnFlags = readLe16(resolved, 32);
                    out.resolvedBtnLevel = readLe16(resolved, 34);
                    out.resolvedBtnNkeys = readLe32(resolved, 36);
                    out.objectStatus = "OMAP_TARGET_BTREE_READ";
                    out.interpretation = purpose + ": target object resolved through the volume OMAP and read as a B-tree node.";
                } else {
                    out.objectStatus = "OMAP_TARGET_UNEXPECTED_OBJECT_TYPE";
                    out.interpretation = purpose + ": target object resolved through the volume OMAP, but was not a B-tree node.";
                }
                out.resolvedSampleHex = hexSampleBytes(resolved.data(), resolved.size() < 96 ? resolved.size() : 96);
                out.resolvedBuffer.swap(resolved);
            } else {
                out.objectStatus = "OMAP_TARGET_READ_FAILED";
                out.interpretation = purpose + ": matching OMAP value was found, but the resolved object read failed.";
                out.notes += out.notes.empty() ? resolvedErr : ("; " + resolvedErr);
            }
            break;
        }

        std::uint64_t childOid = 0;
        std::uint32_t childEntry = 0;
        bool childFound = false;
        bool usedFallbackFirst = false;
        const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
        std::uint64_t fallbackChild = 0;
        std::uint32_t fallbackEntry = 0;
        for (std::uint32_t i = 0; i < limit; ++i) {
            std::size_t keyAbs = 0;
            std::size_t valAbs = 0;
            std::string detail;
            if (!aff4ApfsFixedKvAbsForProbe(node, i, 8U, keyAbs, valAbs, detail)) {
                if (i == 0 && out.notes.empty()) out.notes = detail;
                continue;
            }
            const std::uint64_t keyOid = readLe64(node, keyAbs + 0U);
            const std::uint64_t keyXid = readLe64(node, keyAbs + 8U);
            const std::uint64_t candidateChild = readLe64(node, valAbs + 0U);
            if (i == 0) { fallbackChild = candidateChild; fallbackEntry = i; }
            if (aff4ApfsCompareOmapKeyForProbe(keyOid, keyXid, targetOid, targetXid) <= 0) {
                childOid = candidateChild;
                childEntry = i;
                childFound = true;
            }
        }
        if (!childFound && fallbackChild != 0) {
            childOid = fallbackChild;
            childEntry = fallbackEntry;
            childFound = true;
            usedFallbackFirst = true;
        }
        if (!childFound || childOid == 0) {
            out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_NOT_SELECTED";
            out.interpretation = purpose + ": branch node parsed, but no usable child pointer could be selected for the target key.";
            break;
        }
        aff4ApfsAppendBranchPathForProbe(out.branchPath, "child_entry=" + std::to_string(childEntry) + ";child_oid=" + std::to_string(childOid) + (usedFallbackFirst ? ";fallback_first" : ""));
        std::uint64_t childOffset = 0;
        if (!safeNodeOffset(childOid, childOffset)) {
            out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_OFFSET_UNSAFE";
            out.interpretation = purpose + ": selected branch child OID could not be converted to a safe read offset.";
            break;
        }
        std::vector<unsigned char> child;
        std::string childErr;
        const long long childRead = readVirtual(childOffset, blockSize, child, childErr);
        if (childRead <= 0 || child.size() < 64) {
            out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_READ_FAILED";
            out.interpretation = purpose + ": selected branch child could not be read as a B-tree node.";
            out.notes += out.notes.empty() ? childErr : ("; " + childErr);
            break;
        }
        node.swap(child);
        nodeOid = childOid;
        nodeOffset = childOffset;
        nodeRead = childRead;
    }
    if (out.lookupStatus.empty()) {
        out.lookupStatus = "VOLUME_OMAP_BRANCH_TRAVERSAL_LIMIT_REACHED";
        out.interpretation = purpose + ": bounded branch traversal reached the safety depth limit before a leaf node was resolved.";
    }
    return out;
}



constexpr std::uint64_t kApfsFsObjectIdMask = 0x0fffffffffffffffULL;

std::uint64_t apfsFsKeyObjectId(std::uint64_t keyRaw) {
    return keyRaw & kApfsFsObjectIdMask;
}

std::uint8_t apfsFsKeyRecordType(std::uint64_t keyRaw) {
    return static_cast<std::uint8_t>((keyRaw >> 60U) & 0x0fU);
}

std::string apfsFsRecordTypeLabel(std::uint8_t t) {
    switch (t) {
        case 0: return "ANY";
        case 1: return "SNAP_METADATA";
        case 2: return "EXTENT";
        case 3: return "INODE";
        case 4: return "XATTR";
        case 5: return "SIBLING_LINK";
        case 6: return "DSTREAM_ID";
        case 7: return "CRYPTO_STATE";
        case 8: return "FILE_EXTENT";
        case 9: return "DIR_REC";
        case 10: return "DIR_STATS";
        case 11: return "SNAP_NAME";
        case 12: return "SIBLING_MAP";
        case 13: return "FILE_INFO";
        default: return std::string("FS_RECORD_TYPE_") + std::to_string(static_cast<unsigned int>(t));
    }
}

bool isApfsCompressionOrResourceXattrName(const std::string& name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return n == "com.apple.decmpfs" ||
           n == "com.apple.resourcefork" ||
           n.find("decmpfs") != std::string::npos ||
           n.find("resourcefork") != std::string::npos;
}

std::string apfsXattrStorageLabel(std::uint16_t flags) {
    const bool dataStream = (flags & 0x0001U) != 0;
    const bool embedded = (flags & 0x0002U) != 0;
    if (dataStream && embedded) return "INVALID_BOTH_STREAM_AND_EMBEDDED";
    if (dataStream) return "XATTR_DATA_STREAM";
    if (embedded) return "XATTR_DATA_EMBEDDED";
    return "XATTR_STORAGE_UNKNOWN";
}

int decmpfsCompressionTypeFromPreviewHex(const std::string& hex) {
    // Decmpfs embedded preview begins with little-endian CMPF bytes: 66 70 6D 63,
    // followed by a little-endian compression type.  Type 4 and 8 indicate data
    // stored in the resource fork, so the associated com.apple.ResourceFork stream
    // must be followed before the logical file can be reconstructed.
    std::vector<unsigned int> bytes;
    std::istringstream iss(hex);
    std::string tok;
    while (iss >> tok) {
        if (tok.size() > 2) tok = tok.substr(0, 2);
        try {
            bytes.push_back(static_cast<unsigned int>(std::stoul(tok, nullptr, 16)) & 0xffU);
        } catch (...) {
            break;
        }
        if (bytes.size() >= 8U) break;
    }
    if (bytes.size() < 8U) return 0;
    if (!(bytes[0] == 0x66U && bytes[1] == 0x70U && bytes[2] == 0x6dU && bytes[3] == 0x63U)) return 0;
    return static_cast<int>(bytes[4] | (bytes[5] << 8U) | (bytes[6] << 16U) | (bytes[7] << 24U));
}


std::uint64_t decmpfsUncompressedSizeFromPreviewHex(const std::string& hex) {
    std::vector<unsigned int> bytes;
    std::istringstream iss(hex);
    std::string tok;
    while (iss >> tok) {
        if (tok.size() > 2) tok = tok.substr(0, 2);
        try { bytes.push_back(static_cast<unsigned int>(std::stoul(tok, nullptr, 16)) & 0xffU); }
        catch (...) { break; }
        if (bytes.size() >= 16U) break;
    }
    if (bytes.size() < 16U) return 0;
    if (!(bytes[0] == 0x66U && bytes[1] == 0x70U && bytes[2] == 0x6dU && bytes[3] == 0x63U)) return 0;
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < 8U; ++i) v |= (static_cast<std::uint64_t>(bytes[8U + i]) << (8U * i));
    return v;
}



bool tryParseNumericTxtStem(const std::string& name, std::uint64_t& outValue) {
    std::string base = name;
    const std::size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    const std::string suffix = ".txt";
    if (base.size() <= suffix.size()) return false;
    std::string lower = base;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.substr(lower.size() - suffix.size()) != suffix) return false;
    const std::string stem = base.substr(0, base.size() - suffix.size());
    if (stem.empty()) return false;
    for (char ch : stem) if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    try { outValue = static_cast<std::uint64_t>(std::stoull(stem)); return true; }
    catch (...) { return false; }
}

long long cacheNameFileIdClosenessScore(const std::string& targetName, std::uint64_t childFileId) {
    std::uint64_t stem = 0;
    if (!tryParseNumericTxtStem(targetName, stem)) return 0;
    const std::uint64_t diff = (childFileId > stem) ? (childFileId - stem) : (stem - childFileId);
    if (diff <= 16U) return 60000;
    if (diff <= 128U) return 45000;
    if (diff <= 4096U) return 15000;
    if (diff > 100000U) return -30000;
    return -5000;
}

bool isLikelyStoreV2GroupDirectoryName(const std::string& name) {
    if (name.size() != 36U) return false;
    const std::size_t hyphenPositions[] = {8U, 13U, 18U, 23U};
    for (std::size_t i = 0; i < name.size(); ++i) {
        bool shouldBeHyphen = false;
        for (std::size_t hp : hyphenPositions) { if (i == hp) { shouldBeHyphen = true; break; } }
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (shouldBeHyphen) { if (name[i] != '-') return false; }
        else if (!std::isxdigit(c)) return false;
    }
    return true;
}



std::string safePrintableUtf8Fragment(const std::vector<unsigned char>& data, std::size_t off, std::size_t len) {
    if (off >= data.size()) return {};
    const std::size_t end = std::min<std::size_t>(data.size(), off + len);
    std::string out;
    out.reserve(end - off);
    for (std::size_t i = off; i < end; ++i) {
        unsigned char c = data[i];
        if (c == 0) break;
        if (c >= 0x20 && c != 0x7f) out.push_back(static_cast<char>(c));
        else out.push_back('?');
    }
    return out;
}

void parseResolvedApfsVolumeSuperblock(ApfsResolvedVolumeSuperblockRow& row,
                                       const std::vector<unsigned char>& vol,
                                       const std::string& readNotes) {
    if (row.resolvedBytesRead <= 0 || vol.empty()) {
        row.status = "RESOLVED_APSB_READ_FAILED";
        row.interpretation = "The container OMAP selected a physical address for this filesystem OID, but the bounded read returned no bytes.";
        row.notes = readNotes;
        return;
    }
    if (vol.size() < 36) {
        row.status = "RESOLVED_APSB_BUFFER_TOO_SMALL";
        row.interpretation = "The resolved block was too small to contain an APFS object header and magic.";
        row.notes = readNotes;
        row.sampleHex = hexSampleBytes(vol.data(), vol.size());
        return;
    }
    row.objectOid = readLe64(vol, 8);
    row.objectXid = readLe64(vol, 16);
    row.objectTypeRaw = readLe32(vol, 24);
    row.objectTypeLabel = apfsObjectTypeLabel(row.objectTypeRaw);
    row.objectSubtype = readLe32(vol, 28);
    row.magic.assign(reinterpret_cast<const char*>(vol.data() + 32), reinterpret_cast<const char*>(vol.data() + 36));
    row.sampleHex = hexSampleBytes(vol.data(), vol.size() < 96 ? vol.size() : 96);
    if (row.magic != "APSB") {
        row.status = "RESOLVED_OBJECT_NOT_APSB";
        row.interpretation = "The selected OMAP entry did not resolve to APFS volume-superblock magic at object offset +32.";
        row.notes = readNotes;
        return;
    }

    row.status = "APSB_PARSED_FROM_CONTAINER_OMAP";
    row.fsIndex = readLe32(vol, 36);
    row.features = readLe64(vol, 40);
    row.readonlyCompatibleFeatures = readLe64(vol, 48);
    row.incompatibleFeatures = readLe64(vol, 56);
    row.unmountTime = readLe64(vol, 64);
    row.reserveBlockCount = readLe64(vol, 72);
    row.quotaBlockCount = readLe64(vol, 80);
    row.allocBlockCount = readLe64(vol, 88);
    row.rootTreeType = readLe32(vol, 116);
    row.extentrefTreeType = readLe32(vol, 120);
    row.snapMetaTreeType = readLe32(vol, 124);
    row.apfsOmapOid = readLe64(vol, 128);
    row.rootTreeOid = readLe64(vol, 136);
    row.extentrefTreeOid = readLe64(vol, 144);
    row.snapMetaTreeOid = readLe64(vol, 152);
    row.revertToXid = readLe64(vol, 160);
    row.revertToSblockOid = readLe64(vol, 168);
    row.nextObjId = readLe64(vol, 176);
    row.numFiles = readLe64(vol, 184);
    row.numDirectories = readLe64(vol, 192);
    row.numSymlinks = readLe64(vol, 200);
    row.numOtherFsobjects = readLe64(vol, 208);
    row.numSnapshots = readLe64(vol, 216);
    row.totalBlocksAlloced = readLe64(vol, 224);
    row.totalBlocksFreed = readLe64(vol, 232);
    row.volumeUuid = bytesToUuidString(vol, 240);
    row.lastModTime = readLe64(vol, 256);
    row.fsFlags = readLe64(vol, 264);
    row.volumeName = readFixedUtf8Z(vol, 704, 256);
    row.nextDocId = readLe32(vol, 960);
    row.role = readLe16(vol, 964);
    row.interpretation = "APFS volume superblock parsed from the container OMAP-selected physical block. apfs_omap_oid and apfs_root_tree_oid are now available for the volume object-map/catalog phase.";
    row.notes = readNotes;
}





bool containsU64(const std::vector<std::uint64_t>& values, std::uint64_t v) {
    return std::find(values.begin(), values.end(), v) != values.end();
}


void runAff4ApfsStagedStoreV2ParserProbe(const fs::path& caseDir,
                                         const EvidenceSource& source,
                                         const RunOptions& opt,
                                         CaseDatabase& db,
                                         Logger& log) {
    const fs::path stagedRoot = caseDir / "ExtractedSpotlight" / "StagedStoreV2";
    const fs::path csvPath = caseDir / "aff4_apfs_staged_storev2_parser_probe.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_staged_storev2_parser_probe_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_STAGED_STOREV2_PARSER_PROBE.md";
    std::vector<StoreInfo> candidates;
    std::vector<StoreInfo> selected;
    NativeStoreDbParseCounts parseCounts;
    std::string runStatus = "NOT_RUN";
    std::string notes;
    std::size_t maxRecordsUsed = opt.maxNativeRecords ? opt.maxNativeRecords : 25000U;
    std::size_t maxBlocksUsed = opt.maxNativeBlocks;

    try {
        if (!fs::exists(stagedRoot)) {
            runStatus = "NO_STAGED_STOREV2_ROOT";
            notes = "ExtractedSpotlight/StagedStoreV2 was not present; AFF4/APFS copy-out did not stage Store-V2 candidates.";
        } else {
            EvidenceSource stagedSource = source;
            stagedSource.inputPath = stagedRoot;
            stagedSource.sourceKind = "aff4_apfs_staged_storev2_source";
            stagedSource.notes = "Parser probe source derived from controlled AFF4/APFS copy-out under ExtractedSpotlight/StagedStoreV2; original source remains the selected AFF4 file.";
            appendRunStatus(caseDir, "aff4_apfs_staged_storev2_discover", "discover copied Store-V2 candidates under ExtractedSpotlight/StagedStoreV2");
            candidates = discoverStores(stagedSource, SourceProfileKind::Auto, true, log);
            selected = selectDatabasesForParsing(candidates, SourceProfileKind::MacOS, true, log);
            if (selected.empty()) {
                runStatus = candidates.empty() ? "NO_STAGED_STORE_CANDIDATES_DISCOVERED" : "NO_VALID_STAGED_STORE_SELECTED";
                notes = "Discovery did not produce a valid selected primary Store-V2 database from the staged copy-out folder.";
            } else {
                appendRunStatus(caseDir, "aff4_apfs_staged_storev2_parse_start", "selected_stores=" + std::to_string(selected.size()));
                NativeStoreDbParser parser(NativeDecodeMode::CoreFields, maxRecordsUsed, maxBlocksUsed);
                parser.setProgressPath(caseDir / "logs" / "aff4_apfs_staged_storev2_parse_progress.tsv");
                parseCounts = parser.parseStores(selected, stagedSource, db, log);
                appendRunStatus(caseDir, "aff4_apfs_staged_storev2_parse_complete", "raw_records=" + std::to_string(parseCounts.rawRecords) + " raw_key_values=" + std::to_string(parseCounts.rawKeyValues) + " raw_date_candidates=" + std::to_string(parseCounts.rawDateCandidates));
                runStatus = "PARSE_PROBE_COMPLETED";
                notes = "Native Store-V2 parser was run in bounded CoreFields mode against APFS-extracted staged Store-V2 candidates. Treat results as development validation until APFS path reconstruction and full Store-V2 component completeness are confirmed.";
            }
        }
    } catch (const std::exception& ex) {
        runStatus = "PARSE_PROBE_EXCEPTION";
        notes = ex.what();
        log.warn(std::string("AFF4 APFS staged Store-V2 parser probe failed: ") + ex.what());
    }

    try {
        auto fileHeaderHex = [](const fs::path& p) -> std::string {
            std::ifstream in(p, std::ios::binary);
            if (!in) return {};
            std::vector<unsigned char> b(64, 0);
            in.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(b.size()));
            b.resize(static_cast<std::size_t>(in.gcount()));
            return b.empty() ? std::string{} : hexSampleBytes(b.data(), b.size());
        };
        std::set<std::string> selectedPaths;
        for (const auto& st : selected) selectedPaths.insert(pathString(st.storePath));
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,staged_root,store_guid,database_path,relative_path,is_valid,is_selected,file_size_bytes,sha256,signature_decimal,signature_hex,first_64_bytes_hex,validation_error,parse_probe_status,notes\n";
        std::uint32_t seq = 0;
        for (const auto& st : candidates) {
            const bool isSelected = selectedPaths.find(pathString(st.storePath)) != selectedPaths.end();
            std::ostringstream sigHex;
            sigHex << "0x" << std::hex << std::uppercase << st.signature;
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(source.inputPath)) << ',' << csvEscape(inputSourceType(source.inputPath)) << ','
                << seq++ << ',' << csvEscape(pathString(stagedRoot)) << ',' << csvEscape(st.storeGuid) << ',' << csvEscape(pathString(st.storePath)) << ','
                << csvEscape(st.relativePath) << ',' << (st.isValid ? "1" : "0") << ',' << (isSelected ? "1" : "0") << ','
                << st.fileSizeBytes << ',' << csvEscape(st.sha256) << ',' << st.signature << ',' << csvEscape(sigHex.str()) << ',' << csvEscape(fileHeaderHex(st.storePath)) << ','
                << csvEscape(st.validationError) << ',' << csvEscape(runStatus) << ',' << csvEscape(notes) << "\n";
        }
        if (candidates.empty()) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(source.inputPath)) << ',' << csvEscape(inputSourceType(source.inputPath))
                << ",0," << csvEscape(pathString(stagedRoot)) << ",,,,,0,,0,0x0,,," << csvEscape(runStatus) << ',' << csvEscape(notes) << "\n";
        }
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_staged_storev2_parser_probe.csv: ") + ex.what()); }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(source.inputPath)) << "\",\n";
        out << "  \"staged_root\": \"" << jsonEscape(pathString(stagedRoot)) << "\",\n";
        out << "  \"probe_scope\": \"AFF4_APFS_EXTRACTED_STOREV2_NATIVE_PARSE_PROBE\",\n";
        out << "  \"strict_single_aff4_policy\": " << (opt.strictSingleAff4 ? "true" : "false") << ",\n";
        out << "  \"parse_probe_status\": \"" << jsonEscape(runStatus) << "\",\n";
        out << "  \"candidate_databases\": " << candidates.size() << ",\n";
        out << "  \"selected_databases\": " << selected.size() << ",\n";
        out << "  \"max_records_used\": " << maxRecordsUsed << ",\n";
        out << "  \"max_blocks_used\": " << maxBlocksUsed << ",\n";
        out << "  \"stores_seen\": " << parseCounts.storesSeen << ",\n";
        out << "  \"valid_stores\": " << parseCounts.validStores << ",\n";
        out << "  \"metadata_blocks\": " << parseCounts.metadataBlocks << ",\n";
        out << "  \"decompressed_blocks\": " << parseCounts.decompressedBlocks << ",\n";
        out << "  \"parsed_items\": " << parseCounts.parsedItems << ",\n";
        out << "  \"raw_records\": " << parseCounts.rawRecords << ",\n";
        out << "  \"raw_key_values\": " << parseCounts.rawKeyValues << ",\n";
        out << "  \"raw_date_candidates\": " << parseCounts.rawDateCandidates << ",\n";
        out << "  \"failures\": " << parseCounts.failures << ",\n";
        out << "  \"fallback_header_only_items\": " << parseCounts.fallbackHeaderOnlyItems << ",\n";
        out << "  \"notes\": \"" << jsonEscape(notes) << "\"\n";
        out << "}\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_staged_storev2_parser_probe_summary.json: ") + ex.what()); }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Staged Store-V2 Parser Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This probe discovers Store-V2 candidates copied out of the selected AFF4/APFS image and runs the native Store-V2 parser in bounded CoreFields mode against the staged candidate folders. It does not scan parent drives, does not mount APFS, and does not convert the AFF4 image to RAW/DD.\n\n";
        out << "## Summary\n\n";
        out << "- Status: `" << runStatus << "`\n";
        out << "- Candidate databases: `" << candidates.size() << "`\n";
        out << "- Selected databases: `" << selected.size() << "`\n";
        out << "- Max records used: `" << maxRecordsUsed << "`\n";
        out << "- Raw records: `" << parseCounts.rawRecords << "`\n";
        out << "- Raw key/value rows: `" << parseCounts.rawKeyValues << "`\n";
        out << "- Raw date candidates: `" << parseCounts.rawDateCandidates << "`\n";
        out << "- Parser failures: `" << parseCounts.failures << "`\n\n";
        out << "## Next step\n\n";
        out << "If this probe produces raw records, the next version should run enrichment/export on the APFS-staged parser results and preserve AFF4/APFS provenance fields for each staged store and parsed artifact.\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write AFF4_APFS_STAGED_STOREV2_PARSER_PROBE.md: ") + ex.what()); }

    log.info("AFF4 APFS staged Store-V2 parser probe written: " + pathString(csvPath));
}



long long scalarCountForSource(CaseDatabase& db, const std::string& tableName, const std::string& sourceId) {
    const std::string sql = "SELECT COUNT(*) FROM " + tableName + " WHERE source_id=?";
    auto st = db.prepare(sql);
    st.bind(1, sourceId);
    if (st.stepRow()) return st.colInt64(0);
    return 0;
}

void exportAff4ApfsLimitedRows(CaseDatabase& db,
                               const fs::path& csvPath,
                               const std::vector<std::string>& headers,
                               const std::string& sql,
                               const std::string& sourceId,
                               Logger& log) {
    try {
        std::ofstream out(csvPath, std::ios::binary);
        for (std::size_t i = 0; i < headers.size(); ++i) {
            if (i) out << ',';
            out << csvEscape(headers[i]);
        }
        out << "\n";
        auto st = db.prepare(sql);
        st.bind(1, sourceId);
        while (st.stepRow()) {
            for (int i = 0; i < static_cast<int>(headers.size()); ++i) {
                if (i) out << ',';
                out << csvEscape(st.colText(i));
            }
            out << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn("Unable to write " + pathString(csvPath) + ": " + ex.what());
    }
}

struct Aff4ApfsStagedStoreV2EnrichmentProbeCounts {
    std::string status = "NOT_RUN";
    long long rawRecordsBefore = 0;
    long long rawKeyValuesBefore = 0;
    long long rawDateCandidatesBefore = 0;
    long long rawFailuresBefore = 0;
    long long artifacts = 0;
    long long timelineEvents = 0;
    long long usageEvidence = 0;
    long long parentLinks = 0;
    long long fieldInventory = 0;
    long long parserCoverage = 0;
    long long orphanCandidates = 0;
    std::string notes;
};

Aff4ApfsStagedStoreV2EnrichmentProbeCounts runAff4ApfsStagedStoreV2EnrichmentProbe(const fs::path& caseDir,
                                                                                   const EvidenceSource& source,
                                                                                   CaseDatabase& db,
                                                                                   Logger& log) {
    Aff4ApfsStagedStoreV2EnrichmentProbeCounts c;
    const fs::path jsonPath = caseDir / "aff4_apfs_staged_storev2_enrichment_probe_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_STAGED_STOREV2_ENRICHMENT_PROBE.md";

    try {
        c.rawRecordsBefore = scalarCountForSource(db, "raw_records", source.sourceId);
        c.rawKeyValuesBefore = scalarCountForSource(db, "raw_key_values", source.sourceId);
        c.rawDateCandidatesBefore = scalarCountForSource(db, "raw_date_candidates", source.sourceId);
        c.rawFailuresBefore = scalarCountForSource(db, "raw_failures", source.sourceId);
        if (c.rawRecordsBefore <= 0) {
            c.status = "SKIPPED_NO_APFS_STAGED_RAW_RECORDS";
            c.notes = "No raw_records were produced by the APFS-staged Store-V2 parser probe, so enrichment/export was skipped.";
        } else {
            EvidenceSource stagedSource = source;
            stagedSource.inputPath = caseDir / "ExtractedSpotlight" / "StagedStoreV2";
            stagedSource.sourceKind = "aff4_apfs_staged_storev2_source";
            stagedSource.notes = "Enrichment source derived from controlled AFF4/APFS Store-V2 copy-out and native staged parser probe.";
            appendRunStatus(caseDir, "aff4_apfs_staged_storev2_enrichment_start", "raw_records=" + std::to_string(c.rawRecordsBefore));
            SqliteEnrichment enrichment;
            const auto enrichCounts = enrichment.run(db, stagedSource, log);
            (void)enrichCounts;
            c.artifacts = scalarCountForSource(db, "artifacts", source.sourceId);
            c.timelineEvents = scalarCountForSource(db, "timeline_events", source.sourceId);
            c.usageEvidence = scalarCountForSource(db, "usage_evidence", source.sourceId);
            c.parentLinks = scalarCountForSource(db, "parent_inode_links", source.sourceId);
            c.fieldInventory = scalarCountForSource(db, "field_inventory", source.sourceId);
            c.parserCoverage = scalarCountForSource(db, "parser_coverage_summary", source.sourceId);
            c.orphanCandidates = scalarCountForSource(db, "orphaned_deleted_candidates", source.sourceId);
            c.status = "ENRICHMENT_PROBE_COMPLETED";
            c.notes = "SQLite enrichment was run against APFS-extracted staged Store-V2 parser rows. Outputs are development validation until full APFS path reconstruction and Store-V2 component completeness are confirmed.";
            appendRunStatus(caseDir, "aff4_apfs_staged_storev2_enrichment_complete", "artifacts=" + std::to_string(c.artifacts) + " timeline=" + std::to_string(c.timelineEvents) + " usage=" + std::to_string(c.usageEvidence));
        }
    } catch (const std::exception& ex) {
        c.status = "ENRICHMENT_PROBE_EXCEPTION";
        c.notes = ex.what();
        log.warn(std::string("AFF4 APFS staged Store-V2 enrichment probe failed: ") + ex.what());
    }

    exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_artifacts_sample.csv",
        {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","path_source","path_status","content_type","logical_size_bytes","physical_size_bytes","last_updated_utc","confidence"},
        "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,logical_size_bytes,physical_size_bytes,last_updated_utc,confidence FROM artifacts WHERE source_id=? ORDER BY artifact_id LIMIT 5000",
        source.sourceId, log);

    exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_timeline_sample.csv",
        {"timeline_id","artifact_id","store_guid","inode_num","event_timestamp_utc","event_type","event_source_field","file_name","path"},
        "SELECT timeline_id,artifact_id,store_guid,inode_num,event_timestamp_utc,event_type,event_source_field,file_name,path FROM timeline_events WHERE source_id=? ORDER BY event_timestamp_utc, timeline_id LIMIT 5000",
        source.sourceId, log);

    exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_raw_key_values_sample.csv",
        {"raw_kv_id","store_guid","source_db","inode_num","store_id","parent_inode_num","full_path","field_name","field_value"},
        "SELECT raw_kv_id,store_guid,source_db,inode_num,store_id,parent_inode_num,full_path,field_name,field_value FROM raw_key_values WHERE source_id=? ORDER BY raw_kv_id LIMIT 5000",
        source.sourceId, log);

    exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_raw_date_candidates_sample.csv",
        {"raw_date_id","store_guid","source_db","inode_num","store_id","field_name","field_value","parsed_utc","parse_method","file_name","best_path","date_type","association_status","association_confidence"},
        "SELECT raw_date_id,store_guid,source_db,inode_num,store_id,field_name,field_value,parsed_utc,parse_method,file_name,best_path,date_type,association_status,association_confidence FROM raw_date_candidates WHERE source_id=? ORDER BY raw_date_id LIMIT 5000",
        source.sourceId, log);

    exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_raw_failures_sample.csv",
        {"failure_id","phase","store_guid","source_db","message","created_utc"},
        "SELECT failure_id,phase,store_guid,source_db,message,created_utc FROM raw_failures WHERE source_id=? ORDER BY failure_id LIMIT 5000",
        source.sourceId, log);

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(source.inputPath)) << "\",\n";
        out << "  \"probe_scope\": \"AFF4_APFS_EXTRACTED_STOREV2_ENRICHMENT_PROBE\",\n";
        out << "  \"status\": \"" << jsonEscape(c.status) << "\",\n";
        out << "  \"raw_records\": " << c.rawRecordsBefore << ",\n";
        out << "  \"raw_key_values\": " << c.rawKeyValuesBefore << ",\n";
        out << "  \"raw_date_candidates\": " << c.rawDateCandidatesBefore << ",\n";
        out << "  \"raw_failures\": " << c.rawFailuresBefore << ",\n";
        out << "  \"artifacts\": " << c.artifacts << ",\n";
        out << "  \"timeline_events\": " << c.timelineEvents << ",\n";
        out << "  \"usage_evidence\": " << c.usageEvidence << ",\n";
        out << "  \"parent_inode_links\": " << c.parentLinks << ",\n";
        out << "  \"field_inventory_rows\": " << c.fieldInventory << ",\n";
        out << "  \"parser_coverage_rows\": " << c.parserCoverage << ",\n";
        out << "  \"orphan_candidates\": " << c.orphanCandidates << ",\n";
        out << "  \"sample_row_limit_per_file\": 5000,\n";
        out << "  \"notes\": \"" << jsonEscape(c.notes) << "\"\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_staged_storev2_enrichment_probe_summary.json: ") + ex.what());
    }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Staged Store-V2 Enrichment Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This development probe runs SQLite enrichment over raw Store-V2 rows parsed from APFS-extracted staged Store-V2 candidates. It keeps AFF4/APFS extraction separate from normal folder/ZIP parsing and writes bounded sample CSVs for review instead of requiring upload of the full SQLite database.\n\n";
        out << "## Summary\n\n";
        out << "- Status: `" << c.status << "`\n";
        out << "- Raw records: `" << c.rawRecordsBefore << "`\n";
        out << "- Raw key/value rows: `" << c.rawKeyValuesBefore << "`\n";
        out << "- Raw date candidates: `" << c.rawDateCandidatesBefore << "`\n";
        out << "- Raw failures: `" << c.rawFailuresBefore << "`\n";
        out << "- Artifacts: `" << c.artifacts << "`\n";
        out << "- Timeline events: `" << c.timelineEvents << "`\n";
        out << "- Usage evidence rows: `" << c.usageEvidence << "`\n";
        out << "- Parent inode links: `" << c.parentLinks << "`\n\n";
        out << "## Sample outputs\n\n";
        out << "- `aff4_apfs_staged_storev2_artifacts_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_timeline_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_raw_key_values_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_raw_date_candidates_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_raw_failures_sample.csv`\n\n";
        out << "## Next step\n\n";
        out << "Use these outputs to validate whether APFS-extracted Store-V2 rows are investigator-useful, then add AFF4/APFS provenance columns into object-centric review views.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_STAGED_STOREV2_ENRICHMENT_PROBE.md: ") + ex.what());
    }

    log.info("AFF4 APFS staged Store-V2 enrichment probe written: " + pathString(jsonPath));
    return c;
}



std::string lastWindowsErrorString() {
#ifdef _WIN32
    const DWORD code = GetLastError();
    if (code == 0) return {};
    LPSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD len = FormatMessageA(flags, nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
    std::string msg = "Win32 error " + std::to_string(static_cast<unsigned long>(code));
    if (len && buffer) {
        msg += ": ";
        msg += buffer;
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' ' || msg.back() == '\t')) msg.pop_back();
    }
    if (buffer) LocalFree(buffer);
    return msg;
#else
    return {};
#endif
}

bool shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(const fs::path& caseDir,
                                                        const fs::path& originalInput,
                                                        std::string& reason,
                                                        std::string& metadataSample);

void writeAff4DirectMapReaderProbe(const fs::path& caseDir,
                                   const EvidenceSource& source,
                                   const RunOptions& opt,
                                   const fs::path& originalInput,
                                   Logger& log);

void writeAff4CppLiteDynamicLoadProbe(const fs::path& caseDir,
                                      const EvidenceSource& source,
                                      const RunOptions& opt,
                                      const fs::path& originalInput,
                                      Logger& log) {
    if (!isAff4SourcePath(originalInput)) return;
    std::vector<Aff4DynamicProbeRow> rows;
    struct Aff4VirtualApfsProbeRow {
        std::string step;
        std::string status;
        std::uint64_t offset = 0;
        long long bytesRead = -1;
        std::string signature;
        std::string confidence;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };
    std::vector<Aff4VirtualApfsProbeRow> apfsRows;
    ApfsNxSuperblockSummary nxSummary;
    std::vector<ApfsCheckpointDescriptorRow> apfsDescriptorRows;
    std::vector<ApfsVolumeSuperblockRow> apfsVolumeRows;
    std::vector<ApfsCheckpointMapEntryRow> apfsCheckpointMapRows;
    std::vector<ApfsCheckpointMappedObjectProbeRow> apfsCheckpointMappedObjectRows;

    struct ApfsObjectIdProbeRow {
        std::string role;
        std::uint64_t oid = 0;
        std::uint64_t virtualOffset = 0;
        long long bytesRead = -1;
        std::uint64_t objectOid = 0;
        std::uint64_t objectXid = 0;
        std::uint32_t objectTypeRaw = 0;
        std::uint32_t objectSubtype = 0;
        std::string objectTypeLabel;
        std::string magic;
        std::string status;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };
    struct ApfsBtreeNodeProbeRow {
        std::string sourceRole;
        std::uint64_t sourceOid = 0;
        std::uint64_t sourcePaddr = 0;
        std::uint64_t virtualOffset = 0;
        long long bytesRead = -1;
        std::uint64_t objectOid = 0;
        std::uint64_t objectXid = 0;
        std::uint32_t objectTypeRaw = 0;
        std::uint32_t objectSubtype = 0;
        std::string objectTypeLabel;
        std::uint16_t btnFlags = 0;
        std::uint16_t btnLevel = 0;
        std::uint32_t btnNkeys = 0;
        std::uint16_t tableSpaceOffset = 0;
        std::uint16_t tableSpaceLength = 0;
        std::uint16_t freeSpaceOffset = 0;
        std::uint16_t freeSpaceLength = 0;
        std::string status;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };

    struct ApfsOmapPhysProbeRow {
        std::string sourceRole;
        std::uint64_t sourceOid = 0;
        std::uint64_t virtualOffset = 0;
        long long bytesRead = -1;
        std::uint64_t objectOid = 0;
        std::uint64_t objectXid = 0;
        std::uint32_t objectTypeRaw = 0;
        std::string objectTypeLabel;
        std::uint32_t objectSubtype = 0;
        std::uint32_t omFlags = 0;
        std::uint32_t omSnapshotCount = 0;
        std::uint32_t omTreeType = 0;
        std::uint32_t omSnapshotTreeType = 0;
        std::uint64_t omTreeOid = 0;
        std::uint64_t omSnapshotTreeOid = 0;
        std::uint64_t omMostRecentSnap = 0;
        std::uint64_t omPendingRevertMin = 0;
        std::uint64_t omPendingRevertMax = 0;
        std::string status;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };

    struct ApfsOmapBtreeRootProbeRow {
        std::string sourceRole;
        std::uint64_t omapOid = 0;
        std::uint64_t omTreeOid = 0;
        std::uint64_t virtualOffset = 0;
        long long bytesRead = -1;
        std::uint64_t objectOid = 0;
        std::uint64_t objectXid = 0;
        std::uint32_t objectTypeRaw = 0;
        std::string objectTypeLabel;
        std::uint32_t objectSubtype = 0;
        std::uint16_t btnFlags = 0;
        std::uint16_t btnLevel = 0;
        std::uint32_t btnNkeys = 0;
        std::uint16_t tableSpaceOffset = 0;
        std::uint16_t tableSpaceLength = 0;
        std::uint16_t freeSpaceOffset = 0;
        std::uint16_t freeSpaceLength = 0;
        std::uint32_t btFlagsCandidate = 0;
        std::uint32_t btNodeSizeCandidate = 0;
        std::uint32_t btKeySizeCandidate = 0;
        std::uint32_t btValSizeCandidate = 0;
        std::string status;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };

    struct ApfsOmapLookupProbeRow {
        std::string targetRole;
        std::uint64_t targetOid = 0;
        std::uint64_t targetXid = 0;
        std::uint64_t omapOid = 0;
        std::uint64_t omTreeOid = 0;
        std::uint16_t rootLevel = 0;
        std::uint32_t rootNkeys = 0;
        std::string status;
        std::string interpretation;
        std::string notes;
    };

    struct ApfsOmapBtreeTocProbeRow {
        std::string sourceRole;
        std::uint64_t omapOid = 0;
        std::uint64_t omTreeOid = 0;
        std::uint64_t virtualOffset = 0;
        std::uint16_t rootLevel = 0;
        std::uint32_t rootNkeys = 0;
        std::uint32_t entryIndex = 0;
        std::uint32_t tocOffset = 0;
        std::uint16_t keyOffsetCandidate = 0;
        std::uint16_t keyLengthCandidate = 0;
        std::uint16_t valueOffsetCandidate = 0;
        std::uint16_t valueLengthCandidate = 0;
        std::string keySampleHex;
        std::string valueSampleHex;
        std::string status;
        std::string interpretation;
        std::string notes;
    };

    struct ApfsOmapLeafKvDecodeRow {
        std::string sourceRole;
        std::uint64_t omapOid = 0;
        std::uint64_t omTreeOid = 0;
        std::uint64_t virtualOffset = 0;
        std::uint16_t rootLevel = 0;
        std::uint32_t rootNkeys = 0;
        std::uint32_t entryIndex = 0;
        std::uint32_t tocOffset = 0;
        std::uint16_t keyOffset = 0;
        std::uint16_t valueOffset = 0;
        std::uint64_t keyOid = 0;
        std::uint64_t keyXid = 0;
        std::uint32_t valueFlags = 0;
        std::uint32_t valueSize = 0;
        std::uint64_t valuePaddr = 0;
        std::uint64_t resolvedVirtualOffset = 0;
        long long resolvedBytesRead = -1;
        std::uint64_t resolvedObjectOid = 0;
        std::uint64_t resolvedObjectXid = 0;
        std::uint32_t resolvedObjectTypeRaw = 0;
        std::string resolvedObjectTypeLabel;
        std::uint32_t resolvedObjectSubtype = 0;
        std::string resolvedMagic;
        std::string targetRole;
        std::string lookupStatus;
        std::string status;
        std::string interpretation;
        std::string sampleHex;
        std::string notes;
    };
    std::vector<ApfsObjectIdProbeRow> apfsObjectIdProbeRows;
    std::vector<ApfsBtreeNodeProbeRow> apfsBtreeNodeProbeRows;
    std::vector<ApfsOmapPhysProbeRow> apfsOmapPhysProbeRows;
    std::vector<ApfsOmapBtreeRootProbeRow> apfsOmapBtreeRootProbeRows;
    std::vector<ApfsOmapLookupProbeRow> apfsOmapLookupProbeRows;
    std::vector<ApfsOmapBtreeTocProbeRow> apfsOmapBtreeTocProbeRows;
    std::vector<ApfsOmapLeafKvDecodeRow> apfsOmapLeafKvDecodeRows;
    std::vector<ApfsResolvedVolumeSuperblockRow> apfsResolvedVolumeSuperblockRows;
    std::vector<ApfsVolumeOmapProbeRow> apfsVolumeOmapProbeRows;
    std::vector<ApfsVolumeRootTreeLookupRow> apfsVolumeRootTreeLookupRows;
    std::vector<ApfsRootTreeNodeProbeRow> apfsRootTreeNodeProbeRows;
    std::vector<ApfsRootTreeRecordSampleRow> apfsRootTreeRecordSampleRows;
    std::vector<ApfsRootTreeChildNodeProbeRow> apfsRootTreeChildNodeProbeRows;
    std::vector<ApfsRootTreeRecordSampleRow> apfsRootTreeChildRecordSampleRows;
    std::vector<ApfsRootTreeChildNodeProbeRow> apfsRootTreeDescendantNodeProbeRows;
    std::vector<ApfsRootTreeRecordSampleRow> apfsRootTreeDescendantRecordSampleRows;
    std::vector<ApfsRootTreeRecordSampleRow> apfsSpotlightTargetScanRows;
    std::vector<ApfsRootTreeRecordSampleRow> apfsSpotlightNameScanSampleRows;
    std::vector<ApfsDirectoryRecordEntry> apfsDirectoryRecordEntries;
    std::vector<ApfsSpotlightCopyAttemptRow> apfsSpotlightCopyAttemptRows;
    std::vector<ApfsSpotlightInodeProbeRow> apfsSpotlightInodeProbeRows;
    std::vector<ApfsSpotlightFileExtentProbeRow> apfsSpotlightFileExtentProbeRows;
    std::vector<ApfsSpotlightXattrProbeRow> apfsSpotlightXattrProbeRows;
    std::vector<ApfsSpotlightFileCopyOutRow> apfsSpotlightFileCopyOutRows;
    ApfsSpotlightTargetScanMetrics apfsSpotlightTargetScanMetrics;
    std::map<std::uint32_t, ApfsVolumeOmapProbeRow> apfsVolumeOmapRowsByVolumeSequence;
    std::map<std::uint32_t, std::vector<unsigned char>> apfsVolumeOmapRootNodesByVolumeSequence;

    std::uint64_t finalObjectSize = 0;
    long long finalBlockSize = -1;
    bool gptFound = false;
    std::size_t partitionCount = 0;
    std::size_t apfsPartitionCount = 0;
    std::size_t nxsbHitCount = 0;
    std::size_t virtualReadCount = 0;

    auto addRow = [&](const std::string& step,
                      const std::string& status,
                      const fs::path& p,
                      std::uint64_t objectSize,
                      std::uint64_t offset,
                      long long bytesRead,
                      const std::string& sampleHex,
                      const std::string& notes) {
        Aff4DynamicProbeRow r;
        r.step = step;
        r.status = status;
        r.path = p;
        r.objectSize = objectSize;
        r.offset = offset;
        r.bytesRead = bytesRead;
        r.sampleHex = sampleHex;
        r.notes = notes;
        rows.push_back(r);
    };
    auto addApfsRow = [&](const std::string& step,
                          const std::string& status,
                          std::uint64_t offset,
                          long long bytesRead,
                          const std::string& signature,
                          const std::string& confidence,
                          const std::string& interpretation,
                          const std::string& sampleHex,
                          const std::string& notes) {
        Aff4VirtualApfsProbeRow r;
        r.step = step;
        r.status = status;
        r.offset = offset;
        r.bytesRead = bytesRead;
        r.signature = signature;
        r.confidence = confidence;
        r.interpretation = interpretation;
        r.sampleHex = sampleHex;
        r.notes = notes;
        apfsRows.push_back(r);
    };

    const fs::path csvPath = caseDir / "aff4_cpp_lite_dynamic_load_probe.csv";
    const fs::path planPath = caseDir / "AFF4_CPP_LITE_DYNAMIC_LOAD_PROBE.md";
    const fs::path apfsCsvPath = caseDir / "aff4_virtual_apfs_probe.csv";
    const fs::path apfsJsonPath = caseDir / "aff4_virtual_apfs_probe_summary.json";
    const fs::path apfsPlanPath = caseDir / "AFF4_VIRTUAL_APFS_PROBE.md";
    const fs::path apfsObjectIdCsvPath = caseDir / "aff4_apfs_object_id_probe.csv";
    const fs::path apfsBtreeCsvPath = caseDir / "aff4_apfs_btree_node_probe.csv";
    const fs::path apfsObjectResolutionJsonPath = caseDir / "aff4_apfs_object_resolution_probe_summary.json";
    const fs::path apfsObjectResolutionMdPath = caseDir / "AFF4_APFS_OBJECT_RESOLUTION_PROBE.md";
    const fs::path apfsOmapPhysCsvPath = caseDir / "aff4_apfs_omap_phys_probe.csv";
    const fs::path apfsOmapBtreeRootCsvPath = caseDir / "aff4_apfs_omap_btree_root_probe.csv";
    const fs::path apfsOmapLookupCsvPath = caseDir / "aff4_apfs_omap_lookup_probe.csv";
    const fs::path apfsOmapBtreeTocCsvPath = caseDir / "aff4_apfs_omap_btree_toc_probe.csv";
    const fs::path apfsOmapSummaryJsonPath = caseDir / "aff4_apfs_omap_probe_summary.json";
    const fs::path apfsOmapLeafKvCsvPath = caseDir / "aff4_apfs_omap_leaf_kv_decode.csv";
    const fs::path apfsOmapLeafLookupCsvPath = caseDir / "aff4_apfs_omap_leaf_lookup_results.csv";
    const fs::path apfsOmapLeafKvMdPath = caseDir / "AFF4_APFS_OMAP_LEAF_KV_DECODE.md";
    const fs::path apfsOmapTocMdPath = caseDir / "AFF4_APFS_OMAP_TOC_PROBE.md";
    const fs::path apfsOmapMdPath = caseDir / "AFF4_APFS_OMAP_PROBE.md";

    auto writeObjectResolutionOutputs = [&]() {
        std::size_t readOk = 0, apsbHits = 0, omapHits = 0, btreeRows = 0, btreeNodeRows = 0;
        std::size_t omapPhysRows = apfsOmapPhysProbeRows.size();
        std::size_t omapBtreeRows = apfsOmapBtreeRootProbeRows.size();
        std::size_t omapLookupRows = apfsOmapLookupProbeRows.size();
        for (const auto& r : apfsObjectIdProbeRows) {
            if (r.bytesRead > 0) ++readOk;
            if (r.magic == "APSB") ++apsbHits;
            if (r.magic == "OMAP") ++omapHits;
        }
        for (const auto& r : apfsBtreeNodeProbeRows) {
            if (r.objectTypeLabel == "BTREE") ++btreeRows;
            if (r.objectTypeLabel == "BTREE_NODE") ++btreeNodeRows;
        }
        try {
            std::ofstream out(apfsObjectIdCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,role,oid,virtual_offset,bytes_read,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,magic,status,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsObjectIdProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.role) << ',' << r.oid << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                    << r.objectOid << ',' << r.objectXid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ','
                    << csvEscape(r.magic) << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_object_id_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsBtreeCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,source_role,source_oid,source_paddr,virtual_offset,bytes_read,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,btn_flags,btn_level,btn_nkeys,table_space_offset,table_space_length,free_space_offset,free_space_length,status,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsBtreeNodeProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.sourceRole) << ',' << r.sourceOid << ',' << r.sourcePaddr << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                    << r.objectOid << ',' << r.objectXid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ','
                    << r.btnFlags << ',' << r.btnLevel << ',' << r.btnNkeys << ',' << r.tableSpaceOffset << ',' << r.tableSpaceLength << ','
                    << r.freeSpaceOffset << ',' << r.freeSpaceLength << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_btree_node_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapPhysCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,source_role,source_oid,virtual_offset,bytes_read,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,om_flags,om_snapshot_count,om_tree_type,om_snapshot_tree_type,om_tree_oid,om_snapshot_tree_oid,om_most_recent_snap,om_pending_revert_min,om_pending_revert_max,status,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsOmapPhysProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.sourceRole) << ',' << r.sourceOid << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                    << r.objectOid << ',' << r.objectXid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ','
                    << r.omFlags << ',' << r.omSnapshotCount << ',' << r.omTreeType << ',' << r.omSnapshotTreeType << ','
                    << r.omTreeOid << ',' << r.omSnapshotTreeOid << ',' << r.omMostRecentSnap << ',' << r.omPendingRevertMin << ',' << r.omPendingRevertMax << ','
                    << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_phys_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapBtreeRootCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,source_role,omap_oid,om_tree_oid,virtual_offset,bytes_read,object_oid,object_xid,object_type_raw,object_type_label,object_subtype,btn_flags,btn_level,btn_nkeys,table_space_offset,table_space_length,free_space_offset,free_space_length,btree_info_flags_candidate,btree_info_node_size_candidate,btree_info_key_size_candidate,btree_info_value_size_candidate,status,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsOmapBtreeRootProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.sourceRole) << ',' << r.omapOid << ',' << r.omTreeOid << ',' << r.virtualOffset << ',' << r.bytesRead << ','
                    << r.objectOid << ',' << r.objectXid << ',' << r.objectTypeRaw << ',' << csvEscape(r.objectTypeLabel) << ',' << r.objectSubtype << ','
                    << r.btnFlags << ',' << r.btnLevel << ',' << r.btnNkeys << ',' << r.tableSpaceOffset << ',' << r.tableSpaceLength << ','
                    << r.freeSpaceOffset << ',' << r.freeSpaceLength << ',' << r.btFlagsCandidate << ',' << r.btNodeSizeCandidate << ',' << r.btKeySizeCandidate << ',' << r.btValSizeCandidate << ','
                    << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_btree_root_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapLookupCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,target_role,target_oid,target_xid,omap_oid,om_tree_oid,root_level,root_nkeys,status,interpretation,notes\n";
            for (const auto& r : apfsOmapLookupProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.targetRole) << ',' << r.targetOid << ',' << r.targetXid << ',' << r.omapOid << ',' << r.omTreeOid << ','
                    << r.rootLevel << ',' << r.rootNkeys << ',' << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_lookup_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapBtreeTocCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,source_role,omap_oid,om_tree_oid,virtual_offset,root_level,root_nkeys,entry_index,toc_offset,key_offset_candidate,key_length_candidate,value_offset_candidate,value_length_candidate,key_sample_hex,value_sample_hex,status,interpretation,notes\n";
            for (const auto& r : apfsOmapBtreeTocProbeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.sourceRole) << ',' << r.omapOid << ',' << r.omTreeOid << ',' << r.virtualOffset << ',' << r.rootLevel << ',' << r.rootNkeys << ','
                    << r.entryIndex << ',' << r.tocOffset << ',' << r.keyOffsetCandidate << ',' << r.keyLengthCandidate << ','
                    << r.valueOffsetCandidate << ',' << r.valueLengthCandidate << ',' << csvEscape(r.keySampleHex) << ',' << csvEscape(r.valueSampleHex) << ','
                    << csvEscape(r.status) << ',' << csvEscape(r.interpretation) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_btree_toc_probe.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapLeafKvCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,source_role,omap_oid,om_tree_oid,virtual_offset,root_level,root_nkeys,entry_index,toc_offset,key_offset,value_offset,key_oid,key_xid,value_flags,value_size,value_paddr,resolved_virtual_offset,resolved_bytes_read,resolved_object_oid,resolved_object_xid,resolved_object_type_raw,resolved_object_type_label,resolved_object_subtype,resolved_magic,target_role,lookup_status,status,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsOmapLeafKvDecodeRows) {
                out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.sourceRole) << ',' << r.omapOid << ',' << r.omTreeOid << ',' << r.virtualOffset << ',' << r.rootLevel << ',' << r.rootNkeys << ','
                    << r.entryIndex << ',' << r.tocOffset << ',' << r.keyOffset << ',' << r.valueOffset << ',' << r.keyOid << ',' << r.keyXid << ','
                    << r.valueFlags << ',' << r.valueSize << ',' << r.valuePaddr << ',' << r.resolvedVirtualOffset << ',' << r.resolvedBytesRead << ','
                    << r.resolvedObjectOid << ',' << r.resolvedObjectXid << ',' << r.resolvedObjectTypeRaw << ',' << csvEscape(r.resolvedObjectTypeLabel) << ',' << r.resolvedObjectSubtype << ','
                    << csvEscape(r.resolvedMagic) << ',' << csvEscape(r.targetRole) << ',' << csvEscape(r.lookupStatus) << ',' << csvEscape(r.status) << ','
                    << csvEscape(r.interpretation) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_leaf_kv_decode.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapLeafLookupCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,target_role,target_oid,target_xid,selected_key_oid,selected_key_xid,value_flags,value_size,value_paddr,resolved_virtual_offset,resolved_bytes_read,resolved_object_oid,resolved_object_xid,resolved_object_type_raw,resolved_object_type_label,resolved_object_subtype,resolved_magic,status,interpretation,notes\n";
            std::vector<std::pair<std::string, std::uint64_t>> targets;
            targets.push_back({"NX_OMAP_SELF", nxSummary.omapOid});
            for (std::size_t idx = 0; idx < nxSummary.fsOids.size() && idx < 32U; ++idx) {
                targets.push_back({std::string("NX_FS_OID_") + std::to_string(idx), nxSummary.fsOids[idx]});
            }
            for (const auto& target : targets) {
                const ApfsOmapLeafKvDecodeRow* best = nullptr;
                for (const auto& r : apfsOmapLeafKvDecodeRows) {
                    if (r.keyOid != target.second) continue;
                    if (nxSummary.nextXid != 0 && r.keyXid > nxSummary.nextXid) continue;
                    if (!best || r.keyXid > best->keyXid) best = &r;
                }
                if (best) {
                    out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                        << csvEscape(target.first) << ',' << target.second << ',' << nxSummary.nextXid << ',' << best->keyOid << ',' << best->keyXid << ','
                        << best->valueFlags << ',' << best->valueSize << ',' << best->valuePaddr << ',' << best->resolvedVirtualOffset << ',' << best->resolvedBytesRead << ','
                        << best->resolvedObjectOid << ',' << best->resolvedObjectXid << ',' << best->resolvedObjectTypeRaw << ',' << csvEscape(best->resolvedObjectTypeLabel) << ','
                        << best->resolvedObjectSubtype << ',' << csvEscape(best->resolvedMagic) << ",OMAP_LOOKUP_SELECTED," 
                        << csvEscape("Best leaf entry by target OID and highest key XID not exceeding nx_next_xid.") << ',' << csvEscape(best->notes) << "\n";
                } else {
                    out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                        << csvEscape(target.first) << ',' << target.second << ',' << nxSummary.nextXid << ",0,0,0,0,0,0,-1,0,0,0,,0,,OMAP_LOOKUP_NOT_FOUND,"
                        << csvEscape("No decoded OMAP leaf key matched this target OID with an acceptable transaction ID.") << "," << csvEscape("target_oid=" + std::to_string(target.second)) << "\n";
                }
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_leaf_lookup_results.csv: ") + ex.what());
        }
        try {
            std::ofstream out(apfsObjectResolutionJsonPath, std::ios::binary);
            out << "{\n";
            out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
            out << "  \"app_version\": \"" << appVersion() << "\",\n";
            out << "  \"source_id\": \"" << source.sourceId << "\",\n";
            out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
            out << "  \"probe_scope\": \"EXACT_INPUT_FILE_VIA_LIBAFF4_APFS_OBJECT_RESOLUTION\",\n";
            out << "  \"object_id_probe_rows\": " << apfsObjectIdProbeRows.size() << ",\n";
            out << "  \"object_id_read_ok\": " << readOk << ",\n";
            out << "  \"object_id_omap_hits\": " << omapHits << ",\n";
            out << "  \"object_id_apsb_hits\": " << apsbHits << ",\n";
            out << "  \"btree_probe_rows\": " << apfsBtreeNodeProbeRows.size() << ",\n";
            out << "  \"btree_object_rows\": " << btreeRows << ",\n";
            out << "  \"btree_node_rows\": " << btreeNodeRows << ",\n";
            out << "  \"omap_phys_rows\": " << omapPhysRows << ",\n";
            out << "  \"omap_btree_root_rows\": " << omapBtreeRows << ",\n";
            out << "  \"omap_lookup_rows\": " << omapLookupRows << ",\n";
            out << "  \"omap_btree_toc_rows\": " << apfsOmapBtreeTocProbeRows.size() << ",\n";
            out << "  \"omap_leaf_kv_decode_rows\": " << apfsOmapLeafKvDecodeRows.size() << ",\n";
            out << "  \"resolved_volume_superblock_rows\": " << apfsResolvedVolumeSuperblockRows.size() << ",\n";
            out << "  \"volume_omap_probe_rows\": " << apfsVolumeOmapProbeRows.size() << "\n";
            out << "}\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_object_resolution_probe_summary.json: ") + ex.what());
        }
        try {
            std::ofstream out(apfsObjectResolutionMdPath, std::ios::binary);
            out << "# AFF4 APFS Object Resolution Probe\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Scope\n\n";
            out << "This probe uses bounded reads through libaff4 against only the one explicit AFF4 input file. It adds direct object-ID reads for NX object-map/spaceman/reaper/filesystem OIDs and a generic B-tree/BTREE_NODE header inventory. It does not search `O:\\`, inspect parent folders, call aff4imager, mount, or export RAW/DD.\n\n";
            out << "## Summary\n\n";
            out << "- Object-ID probe rows: `" << apfsObjectIdProbeRows.size() << "`\n";
            out << "- Object-ID reads OK: `" << readOk << "`\n";
            out << "- Object-ID OMAP hits: `" << omapHits << "`\n";
            out << "- Object-ID APSB hits: `" << apsbHits << "`\n";
            out << "- B-tree inventory rows: `" << apfsBtreeNodeProbeRows.size() << "`\n\n";
            out << "## Interpretation\n\n";
            out << "If direct object-ID reads show BTREE or BTREE_NODE objects instead of APSB/OMAP magic, the next step is APFS B-tree/OMAP entry decoding rather than treating object IDs as block addresses. The B-tree inventory is intended to identify node levels, key counts, and candidate table-space layout before implementing OMAP lookup.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_APFS_OBJECT_RESOLUTION_PROBE.md: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapSummaryJsonPath, std::ios::binary);
            std::size_t leafRoots = 0, branchRoots = 0, tocRows = apfsOmapBtreeTocProbeRows.size();
            for (const auto& r : apfsOmapBtreeRootProbeRows) {
                if (r.status == "OMAP_BTREE_ROOT_READ" && r.btnLevel == 0) ++leafRoots;
                if (r.status == "OMAP_BTREE_ROOT_READ" && r.btnLevel > 0) ++branchRoots;
            }
            out << "{\n";
            out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
            out << "  \"app_version\": \"" << appVersion() << "\",\n";
            out << "  \"source_id\": \"" << source.sourceId << "\",\n";
            out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
            out << "  \"probe_scope\": \"EXACT_INPUT_FILE_VIA_LIBAFF4_APFS_OMAP_RECON\",\n";
            out << "  \"omap_phys_rows\": " << apfsOmapPhysProbeRows.size() << ",\n";
            out << "  \"omap_btree_root_rows\": " << apfsOmapBtreeRootProbeRows.size() << ",\n";
            out << "  \"omap_lookup_rows\": " << apfsOmapLookupProbeRows.size() << ",\n";
            out << "  \"omap_leaf_root_rows\": " << leafRoots << ",\n";
            out << "  \"omap_branch_root_rows\": " << branchRoots << ",\n";
            out << "  \"omap_btree_toc_rows\": " << tocRows << ",\n";
            out << "  \"omap_leaf_kv_decode_rows\": " << apfsOmapLeafKvDecodeRows.size() << ",\n";
            out << "  \"resolved_volume_superblock_rows\": " << apfsResolvedVolumeSuperblockRows.size() << ",\n";
            out << "  \"volume_omap_probe_rows\": " << apfsVolumeOmapProbeRows.size() << "\n";
            out << "}\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_apfs_omap_probe_summary.json: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapLeafKvMdPath, std::ios::binary);
            std::size_t apsbResolved = 0;
            std::size_t decodedRows = 0;
            for (const auto& r : apfsOmapLeafKvDecodeRows) {
                if (r.status == "OMAP_LEAF_KV_DECODED") ++decodedRows;
                if (r.resolvedMagic == "APSB") ++apsbResolved;
            }
            out << "# AFF4 APFS OMAP Leaf Key/Value Decode\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Scope\n\n";
            out << "This probe decodes fixed-size OMAP leaf B-tree table-of-contents entries from the object-map root using the exact selected AFF4 file through libaff4. It does not scan folders, mount, call aff4imager, or export RAW/DD.\n\n";
            out << "## Summary\n\n";
            out << "- Leaf key/value rows decoded: `" << decodedRows << "`\n";
            out << "- APSB resolved rows: `" << apsbResolved << "`\n";
            out << "- Total leaf decode rows: `" << apfsOmapLeafKvDecodeRows.size() << "`\n";
            out << "- Resolved APSB superblock rows: `" << apfsResolvedVolumeSuperblockRows.size() << "`\n";
            out << "- Volume OMAP probe rows: `" << apfsVolumeOmapProbeRows.size() << "`\n\n";
            out << "## Interpretation\n\n";
            out << "If APSB rows are present, the next step is to parse the APFS volume superblock(s), then volume object maps and catalog/root directory structures to locate Spotlight artifacts. If no APSB rows are present, verify OMAP TOC offset interpretation and continue branch/leaf decoder refinement.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_APFS_OMAP_LEAF_KV_DECODE.md: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapTocMdPath, std::ios::binary);
            out << "# AFF4 APFS OMAP B-tree TOC Probe\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Scope\n\n";
            out << "This file summarizes raw candidate table-of-contents rows sampled from the OMAP B-tree root. It is reconnaissance only: it records candidate key/value locations and short samples so the next implementation can distinguish branch entries from leaf OMAP key/value entries without mounting or exporting a full RAW/DD image.\n\n";
            out << "## Summary\n\n";
            out << "- OMAP B-tree TOC candidate rows: `" << apfsOmapBtreeTocProbeRows.size() << "`\n";
            out << "- OMAP B-tree root rows: `" << apfsOmapBtreeRootProbeRows.size() << "`\n\n";
            out << "## Next step\n\n";
            out << "If the root level is greater than zero, decode branch-node keys and child OIDs/physical addresses. If the root level is zero, decode OMAP leaf keys and values and select the highest transaction ID at or below the NX transaction ID for each target object ID.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_APFS_OMAP_TOC_PROBE.md: ") + ex.what());
        }
        try {
            std::ofstream out(apfsOmapMdPath, std::ios::binary);
            out << "# AFF4 APFS OMAP Probe\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Scope\n\n";
            out << "This probe parses APFS object-map physical objects (`omap_phys_t`) and reads the B-tree root referenced by `om_tree_oid` using only the explicitly selected AFF4 file through libaff4. It does not scan drives, call aff4imager, mount, or export RAW/DD.\n\n";
            out << "## Summary\n\n";
            out << "- OMAP physical rows: `" << apfsOmapPhysProbeRows.size() << "`\n";
            out << "- OMAP B-tree root rows: `" << apfsOmapBtreeRootProbeRows.size() << "`\n";
            out << "- OMAP lookup readiness rows: `" << apfsOmapLookupProbeRows.size() << "`\n";
            out << "- OMAP B-tree TOC candidate rows: `" << apfsOmapBtreeTocProbeRows.size() << "`\n";
            out << "- Resolved APSB superblock rows: `" << apfsResolvedVolumeSuperblockRows.size() << "`\n";
            out << "- Volume OMAP probe rows: `" << apfsVolumeOmapProbeRows.size() << "`\n\n";
            out << "## Interpretation\n\n";
            out << "If `aff4_apfs_omap_phys_probe.csv` contains an `om_tree_oid`, the AFF4 virtual reader is now reaching the APFS object-map layer. If the B-tree root has `btn_level > 0`, the next implementation step is branch-node traversal before filesystem OIDs can resolve to APFS volume superblocks. If a leaf root is observed, the next step is decoding the OMAP key/value table-of-contents and selecting the highest transaction ID less than or equal to the container transaction ID.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_APFS_OMAP_PROBE.md: ") + ex.what());
        }
    };

    auto writeOutputs = [&]() {
        try {
            std::ofstream out(csvPath, std::ios::binary);
            out << "source_id,input_path,input_type,step,status,path,object_size,offset,bytes_read,sample_hex,notes\n";
            for (const auto& r : rows) {
                out << csvEscape(source.sourceId) << ','
                    << csvEscape(pathString(originalInput)) << ','
                    << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.step) << ','
                    << csvEscape(r.status) << ','
                    << csvEscape(pathString(r.path)) << ','
                    << r.objectSize << ','
                    << r.offset << ','
                    << r.bytesRead << ','
                    << csvEscape(r.sampleHex) << ','
                    << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_cpp_lite_dynamic_load_probe.csv: ") + ex.what());
        }

        try {
            std::ofstream out(apfsCsvPath, std::ios::binary);
            out << "source_id,input_path,input_type,step,status,virtual_offset,bytes_read,signature,confidence,interpretation,sample_hex,notes\n";
            for (const auto& r : apfsRows) {
                out << csvEscape(source.sourceId) << ','
                    << csvEscape(pathString(originalInput)) << ','
                    << csvEscape(inputSourceType(originalInput)) << ','
                    << csvEscape(r.step) << ','
                    << csvEscape(r.status) << ','
                    << r.offset << ','
                    << r.bytesRead << ','
                    << csvEscape(r.signature) << ','
                    << csvEscape(r.confidence) << ','
                    << csvEscape(r.interpretation) << ','
                    << csvEscape(r.sampleHex) << ','
                    << csvEscape(r.notes) << "\n";
            }
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_virtual_apfs_probe.csv: ") + ex.what());
        }

        try {
            std::ofstream out(apfsJsonPath, std::ios::binary);
            out << "{\n";
            out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
            out << "  \"app_version\": \"" << appVersion() << "\",\n";
            out << "  \"source_id\": \"" << source.sourceId << "\",\n";
            out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
            out << "  \"probe_scope\": \"EXACT_INPUT_FILE_VIA_LIBAFF4_ONLY\",\n";
            out << "  \"dynamic_probe_enabled\": " << (opt.enableAff4DynamicProbe ? "true" : "false") << ",\n";
            out << "  \"strict_single_aff4\": " << (opt.strictSingleAff4 ? "true" : "false") << ",\n";
            out << "  \"object_size\": " << finalObjectSize << ",\n";
            out << "  \"object_block_size\": " << finalBlockSize << ",\n";
            out << "  \"virtual_reads_attempted\": " << virtualReadCount << ",\n";
            out << "  \"gpt_found\": " << (gptFound ? "true" : "false") << ",\n";
            out << "  \"partition_count\": " << partitionCount << ",\n";
            out << "  \"apfs_partition_count\": " << apfsPartitionCount << ",\n";
            out << "  \"nxsb_hit_count\": " << nxsbHitCount << ",\n";
            out << "  \"row_count\": " << apfsRows.size() << "\n";
            out << "}\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write aff4_virtual_apfs_probe_summary.json: ") + ex.what());
        }

        try {
            std::ofstream out(planPath, std::ios::binary);
            out << "# AFF4 CPP Lite Dynamic-Load Probe\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Source\n\n";
            out << "- Source ID: `" << source.sourceId << "`\n";
            out << "- Input path: `" << pathString(originalInput) << "`\n";
            out << "- Input type: `" << inputSourceType(originalInput) << "`\n\n";
            out << "## Probe result rows\n\n";
            out << "| Step | Status | Object size | Bytes read | Sample | Notes |\n";
            out << "|---|---|---:|---:|---|---|\n";
            for (const auto& r : rows) {
                std::string note = r.notes;
                std::replace(note.begin(), note.end(), '|', '/');
                std::string sample = r.sampleHex;
                std::replace(sample.begin(), sample.end(), '|', '/');
                out << "| " << r.step << " | " << r.status << " | " << r.objectSize << " | " << r.bytesRead << " | `" << sample << "` | " << note << " |\n";
            }
            out << "\n## Interpretation\n\n";
            out << "`READ_OK` at offset 0 means Vestigant can load libaff4, open the explicit AFF4 image, and perform a bounded random-access read without exporting a full RAW/DD image. V0_8_23 adds a virtual APFS/GPT probe that reuses that read path.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_CPP_LITE_DYNAMIC_LOAD_PROBE.md: ") + ex.what());
        }

        try {
            std::ofstream out(apfsPlanPath, std::ios::binary);
            out << "# AFF4 Virtual APFS Probe\n\n";
            out << "Version: " << appVersion() << "\n\n";
            out << "## Purpose\n\n";
            out << "This probe uses the aff4-cpp-lite C API against the exact `--input` AFF4 file and attempts bounded virtual reads for disk/APFS discovery. It does not search parent folders, does not scan O:\\\\, and does not export RAW/DD.\n\n";
            out << "## Result summary\n\n";
            out << "- Dynamic probe enabled: `" << (opt.enableAff4DynamicProbe ? "true" : "false") << "`\n";
            out << "- Strict single AFF4: `" << (opt.strictSingleAff4 ? "true" : "false") << "`\n";
            out << "- Object size: `" << finalObjectSize << "`\n";
            out << "- Object block size: `" << finalBlockSize << "`\n";
            out << "- GPT found: `" << (gptFound ? "true" : "false") << "`\n";
            out << "- Partitions found: `" << partitionCount << "`\n";
            out << "- APFS partitions found: `" << apfsPartitionCount << "`\n";
            out << "- NXSB hits: `" << nxsbHitCount << "`\n\n";
            out << "## Next step\n\n";
            out << "If GPT/APFS rows are present, the next version should wrap this AFF4 read path in an image block-reader interface and feed it to APFS enumeration rather than scanning ZIP member payload strings.\n";
        } catch (const std::exception& ex) {
            log.warn(std::string("Unable to write AFF4_VIRTUAL_APFS_PROBE.md: ") + ex.what());
        }
    };

    if (!opt.enableAff4DynamicProbe) {
        addRow("dynamic_load_probe", opt.strictSingleAff4 ? "SKIPPED_STRICT_SINGLE_AFF4" : "SKIPPED_BY_DEFAULT", originalInput, 0, 0, -1, {},
               "Dynamic libaff4 open/read is opt-in only because aff4-cpp-lite may discover/open other AFF4-related files from the evidence drive. Use --enable-aff4-dynamic-probe only when deliberately testing libaff4 random access.");
        addApfsRow("virtual_apfs_probe", "SKIPPED_DYNAMIC_PROBE_NOT_ENABLED", 0, -1, {}, "NOT_RUN", "APFS virtual read probe was skipped because --enable-aff4-dynamic-probe was not supplied.", {}, "Run the single-AFF4 helper with -EnableAff4DynamicProbe to perform bounded virtual GPT/APFS reads through libaff4.");
        writeOutputs();
        writeAff4ApfsContainerViewOutputs(caseDir, source, originalInput, nxSummary, apfsDescriptorRows, log);
        writeAff4ApfsVolumeSuperblockOutputs(caseDir, source, originalInput, nxSummary, apfsVolumeRows, log);
        writeAff4ApfsCheckpointMapOutputs(caseDir, source, originalInput, nxSummary, apfsCheckpointMapRows, apfsCheckpointMappedObjectRows, log);
        writeObjectResolutionOutputs();
        log.info("AFF4 CPP Lite dynamic-load probe written: " + pathString(csvPath));
        log.info("AFF4 virtual APFS probe written: " + pathString(apfsCsvPath));
        log.info("AFF4 APFS container view written: " + pathString(caseDir / "aff4_apfs_container_superblock.csv"));
        log.info("AFF4 APFS volume superblock probe written: " + pathString(caseDir / "aff4_apfs_volume_superblocks.csv"));
    log.info("AFF4 APFS checkpoint map probe written: " + pathString(caseDir / "aff4_apfs_checkpoint_map.csv"));
        return;
    }

#ifdef _WIN32
    {
        std::string skipReason;
        std::string metadataSample;
        if (shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(caseDir, originalInput, skipReason, metadataSample)) {
            appendRunStatus(caseDir, "aff4_dynamic_load_probe_guard", "skipped known blocking BlackBag/LZ4 discontiguous AFF4 layout");
            appendRunStatus(caseDir, "aff4_direct_map_reader_probe", "read AFF4 map/index/data ZIP members directly and decode bounded LZ4 chunks");
            log.warn("Skipping libaff4 dynamic AFF4_open probe: " + skipReason);
            writeAff4DirectMapReaderProbe(caseDir, source, opt, originalInput, log);
            addRow("AFF4_open_guard", "SKIPPED_KNOWN_BLOCKING_LAYOUT", originalInput, 0, 0, -1, {}, skipReason);
            addApfsRow("AFF4_open_guard", "SKIPPED_KNOWN_BLOCKING_LAYOUT", 0, -1, {}, "BLOCKED_BY_KNOWN_READER_HANG",
                       "The AFF4 metadata identifies a BlackBag-style APFS DiscontiguousImage with LZ4 ImageStream storage. This build skips libaff4 AFF4_open to avoid the observed Windows hang and records the direct ZIP map/index parser as the next required reader path.",
                       {}, metadataSample);
            try {
                std::ofstream out(caseDir / "AFF4_DIRECT_MAP_READER_REQUIRED.md", std::ios::binary);
                out << "# AFF4 Direct Map Reader Required\n\n";
                out << "Version: " << appVersion() << "\n\n";
                out << "The AFF4 metadata matches a BlackBag-style APFS `DiscontiguousImage` backed by an LZ4 `ImageStream`. The libaff4 dynamic C API was skipped because this layout was observed to block during `AFF4_open` on Windows.\n\n";
                out << "## Next Implementation Step\n\n";
                out << "Decode the stored AFF4 ZIP members directly:\n\n";
                out << "- `information.turtle`\n";
                out << "- `*/map`\n";
                out << "- `*/idx`\n";
                out << "- `*/data/NNNNNNNN.index`\n";
                out << "- `*/data/NNNNNNNN`\n\n";
                out << "The direct reader should map AFF4 virtual offsets to image-stream chunks, decompress LZ4 chunks, and feed bounded reads into the APFS parser without calling `AFF4_open` or exporting a RAW/DD image.\n\n";
                out << "## Guard Reason\n\n";
                out << skipReason << "\n";
            } catch (const std::exception& ex) {
                log.warn(std::string("Unable to write AFF4_DIRECT_MAP_READER_REQUIRED.md: ") + ex.what());
            }
            writeOutputs();
            if (!fs::exists(caseDir / "aff4_apfs_container_superblock.csv")) {
                writeAff4ApfsContainerViewOutputs(caseDir, source, originalInput, nxSummary, apfsDescriptorRows, log);
            }
            if (!fs::exists(caseDir / "aff4_apfs_volume_superblocks.csv")) {
                writeAff4ApfsVolumeSuperblockOutputs(caseDir, source, originalInput, nxSummary, apfsVolumeRows, log);
            }
            if (!fs::exists(caseDir / "aff4_apfs_checkpoint_map.csv")) {
                writeAff4ApfsCheckpointMapOutputs(caseDir, source, originalInput, nxSummary, apfsCheckpointMapRows, apfsCheckpointMappedObjectRows, log);
            }
            writeObjectResolutionOutputs();
            log.info("AFF4 CPP Lite dynamic-load probe skipped by known-layout guard: " + pathString(csvPath));
            return;
        }
    }
    const fs::path libaff4Dll = findToolCandidate(opt, "VESTIGANT_LIBAFF4_DLL", std::vector<std::string>{"libaff4.dll"});
    if (libaff4Dll.empty()) {
        addRow("resolve_libaff4_dll", "MISSING_LIBAFF4_DLL", {}, 0, 0, -1, {}, "Provide --reader-tools or VESTIGANT_LIBAFF4_DLL pointing to libaff4.dll.");
        addApfsRow("resolve_libaff4_dll", "MISSING_LIBAFF4_DLL", 0, -1, {}, "BLOCKED", "Cannot perform virtual GPT/APFS reads without libaff4.dll.", {}, "Reader tools were not found.");
    } else {
        addRow("resolve_libaff4_dll", "FOUND", libaff4Dll, 0, 0, -1, {}, "Resolved libaff4.dll for dynamic-load probe.");
        const fs::path dllDir = libaff4Dll.parent_path();
        DLL_DIRECTORY_COOKIE aff4DllDirCookie = nullptr;
        if (!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS)) {
            addRow("SetDefaultDllDirectories", "WARNING", dllDir, 0, 0, -1, {}, lastWindowsErrorString());
        }
        if (!dllDir.empty()) {
            aff4DllDirCookie = AddDllDirectory(dllDir.wstring().c_str());
            if (!aff4DllDirCookie) addRow("AddDllDirectory", "WARNING", dllDir, 0, 0, -1, {}, lastWindowsErrorString());
        }
        HMODULE h = LoadLibraryExW(libaff4Dll.wstring().c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
        if (!h) {
            addRow("LoadLibraryExW", "LOAD_FAILED", libaff4Dll, 0, 0, -1, {}, lastWindowsErrorString());
            addApfsRow("LoadLibraryExW", "LOAD_FAILED", 0, -1, {}, "BLOCKED", "Cannot perform virtual GPT/APFS reads because libaff4.dll did not load.", {}, lastWindowsErrorString());
        } else {
            addRow("LoadLibraryExW", "LOADED", libaff4Dll, 0, 0, -1, {}, "libaff4.dll loaded with per-module secure DLL search flags.");
            using Aff4InitFn = void (__cdecl *)();
            using Aff4OpenFn = int (__cdecl *)(const char*);
            using Aff4ObjectSizeFn = std::int64_t (__cdecl *)(int);
            using Aff4ObjectBlockSizeFn = std::int64_t (__cdecl *)(int);
            using Aff4ReadFn = int (__cdecl *)(int, std::uint64_t, void*, int);
            using Aff4CloseFn = int (__cdecl *)(int);
            auto pInit = reinterpret_cast<Aff4InitFn>(GetProcAddress(h, "AFF4_init"));
            auto pOpen = reinterpret_cast<Aff4OpenFn>(GetProcAddress(h, "AFF4_open"));
            auto pSize = reinterpret_cast<Aff4ObjectSizeFn>(GetProcAddress(h, "AFF4_object_size"));
            auto pBlockSize = reinterpret_cast<Aff4ObjectBlockSizeFn>(GetProcAddress(h, "AFF4_object_blocksize"));
            auto pRead = reinterpret_cast<Aff4ReadFn>(GetProcAddress(h, "AFF4_read"));
            auto pClose = reinterpret_cast<Aff4CloseFn>(GetProcAddress(h, "AFF4_close"));
            const bool symbolsOk = pOpen && pSize && pRead && pClose;
            addRow("resolve_symbols", symbolsOk ? "SYMBOLS_READY" : "MISSING_REQUIRED_SYMBOL", libaff4Dll, 0, 0, -1, {},
                   std::string("AFF4_init=") + (pInit ? "yes" : "no") + "; AFF4_open=" + (pOpen ? "yes" : "no") + "; AFF4_object_size=" + (pSize ? "yes" : "no") + "; AFF4_object_blocksize=" + (pBlockSize ? "yes" : "no") + "; AFF4_read=" + (pRead ? "yes" : "no") + "; AFF4_close=" + (pClose ? "yes" : "no"));
            if (!symbolsOk) {
                addApfsRow("resolve_symbols", "MISSING_REQUIRED_SYMBOL", 0, -1, {}, "BLOCKED", "Cannot perform virtual GPT/APFS reads because required AFF4 C API symbols are missing.", {}, "Required symbols: AFF4_open, AFF4_object_size, AFF4_read, AFF4_close.");
            } else {
                if (pInit) {
                    try { pInit(); addRow("AFF4_init", "CALLED", libaff4Dll, 0, 0, -1, {}, "AFF4_init completed."); }
                    catch (...) { addRow("AFF4_init", "EXCEPTION", libaff4Dll, 0, 0, -1, {}, "Exception while calling AFF4_init."); }
                }
                const std::string inputUtf8 = pathString(originalInput);
                int handle = -1;
                try { handle = pOpen(inputUtf8.c_str()); }
                catch (...) { addRow("AFF4_open", "EXCEPTION", originalInput, 0, 0, -1, {}, "Exception while calling AFF4_open."); }
                if (handle < 0) {
                    addRow("AFF4_open", "OPEN_FAILED", originalInput, 0, 0, -1, {}, "AFF4_open returned a negative handle. Check whether the container has a first aff4:Image object and whether dependent DLLs are loadable.");
                    addApfsRow("AFF4_open", "OPEN_FAILED", 0, -1, {}, "BLOCKED", "Cannot perform virtual GPT/APFS reads because AFF4_open failed.", {}, "AFF4_open returned a negative handle.");
                } else {
                    addRow("AFF4_open", "OPENED", originalInput, 0, 0, -1, {}, "AFF4_open returned a non-negative handle.");
                    std::int64_t objectSizeSigned = -1;
                    try { objectSizeSigned = pSize(handle); }
                    catch (...) { addRow("AFF4_object_size", "EXCEPTION", originalInput, 0, 0, -1, {}, "Exception while calling AFF4_object_size."); }
                    finalObjectSize = objectSizeSigned > 0 ? static_cast<std::uint64_t>(objectSizeSigned) : 0;
                    addRow("AFF4_object_size", objectSizeSigned > 0 ? "SIZE_REPORTED" : "SIZE_ZERO_OR_UNKNOWN", originalInput, finalObjectSize, 0, objectSizeSigned, {}, "Virtual image size reported by libaff4.");

                    if (pBlockSize) {
                        std::int64_t blockSize = -1;
                        try { blockSize = pBlockSize(handle); }
                        catch (...) { addRow("AFF4_object_blocksize", "EXCEPTION", originalInput, finalObjectSize, 0, -1, {}, "Exception while calling AFF4_object_blocksize."); }
                        finalBlockSize = blockSize;
                        addRow("AFF4_object_blocksize", blockSize > 0 ? "BLOCKSIZE_REPORTED" : "BLOCKSIZE_ZERO_OR_UNKNOWN", originalInput, finalObjectSize, 0, blockSize, {}, "AFF4 object block size reported by libaff4 when available.");
                    }

                    auto readVirtual = [&](std::uint64_t offset, std::size_t length, std::vector<unsigned char>& out, std::string& err) -> long long {
                        out.assign(length, 0);
                        int rc = -1;
                        try { rc = pRead(handle, offset, out.data(), static_cast<int>(length)); }
                        catch (...) { err = "Exception while calling AFF4_read."; return -1; }
                        if (rc < 0) { err = "AFF4_read returned a negative value."; out.clear(); return rc; }
                        if (static_cast<std::size_t>(rc) < out.size()) out.resize(static_cast<std::size_t>(rc));
                        ++virtualReadCount;
                        return static_cast<long long>(rc);
                    };

                    auto addReadRow = [&](const std::string& step, std::uint64_t offset, long long rc, const std::vector<unsigned char>& buf, const std::string& interpretation, const std::string& notes) {
                        const std::size_t sampleLen = buf.size() < 64 ? buf.size() : 64;
                        addApfsRow(step, rc > 0 ? "READ_OK" : (rc == 0 ? "READ_ZERO" : "READ_FAILED"), offset, rc, {}, rc > 0 ? "READ_PERFORMED" : "NO_DATA", interpretation, sampleLen ? hexSampleBytes(buf.data(), sampleLen) : std::string{}, notes);
                    };

                    auto addBtreeProbeFromBuffer = [&](const std::string& sourceRole,
                                                       std::uint64_t sourceOid,
                                                       std::uint64_t sourcePaddr,
                                                       std::uint64_t virtualOffset,
                                                       long long rc,
                                                       const std::vector<unsigned char>& buf,
                                                       const std::string& notes) {
                        if (rc <= 0 || buf.size() < 64) return;
                        const std::uint32_t rawType = readLe32(buf, 24);
                        const std::string label = apfsObjectTypeLabel(rawType);
                        if (label != "BTREE" && label != "BTREE_NODE") return;
                        ApfsBtreeNodeProbeRow br;
                        br.sourceRole = sourceRole;
                        br.sourceOid = sourceOid;
                        br.sourcePaddr = sourcePaddr;
                        br.virtualOffset = virtualOffset;
                        br.bytesRead = rc;
                        br.objectOid = readLe64(buf, 8);
                        br.objectXid = readLe64(buf, 16);
                        br.objectTypeRaw = rawType;
                        br.objectSubtype = readLe32(buf, 28);
                        br.objectTypeLabel = label;
                        br.btnFlags = readLe16(buf, 32);
                        br.btnLevel = readLe16(buf, 34);
                        br.btnNkeys = readLe32(buf, 36);
                        br.tableSpaceOffset = readLe16(buf, 40);
                        br.tableSpaceLength = readLe16(buf, 42);
                        br.freeSpaceOffset = readLe16(buf, 44);
                        br.freeSpaceLength = readLe16(buf, 46);
                        br.status = br.btnNkeys < 100000U ? "BTREE_HEADER_PARSED" : "BTREE_HEADER_SUSPICIOUS";
                        br.interpretation = label + " object header parsed. Values are generic APFS B-tree header candidates pending object-map/catalog-specific decoding.";
                        br.sampleHex = hexSampleBytes(buf.data(), buf.size() < 96 ? buf.size() : 96);
                        br.notes = notes;
                        apfsBtreeNodeProbeRows.push_back(br);
                    };

                    auto addOmapLookupReadinessRow = [&](const std::string& targetRole,
                                                         std::uint64_t targetOid,
                                                         std::uint64_t targetXid,
                                                         std::uint64_t omapOid,
                                                         std::uint64_t omTreeOid,
                                                         std::uint16_t rootLevel,
                                                         std::uint32_t rootNkeys,
                                                         const std::string& status,
                                                         const std::string& interpretation,
                                                         const std::string& notes) {
                        ApfsOmapLookupProbeRow lr;
                        lr.targetRole = targetRole;
                        lr.targetOid = targetOid;
                        lr.targetXid = targetXid;
                        lr.omapOid = omapOid;
                        lr.omTreeOid = omTreeOid;
                        lr.rootLevel = rootLevel;
                        lr.rootNkeys = rootNkeys;
                        lr.status = status;
                        lr.interpretation = interpretation;
                        lr.notes = notes;
                        apfsOmapLookupProbeRows.push_back(lr);
                    };

                    auto addOmapBtreeTocProbeRowsFromBuffer = [&](const std::string& sourceRole,
                                                                  std::uint64_t omapOid,
                                                                  std::uint64_t omTreeOid,
                                                                  std::uint64_t virtualOffset,
                                                                  std::uint16_t rootLevel,
                                                                  std::uint32_t rootNkeys,
                                                                  const std::vector<unsigned char>& buf,
                                                                  const std::string& notes) {
                        if (buf.size() < 64) return;
                        const std::uint16_t btnFlags = readLe16(buf, 32);
                        const bool fixedKv = (btnFlags & 0x0004U) != 0;
                        const std::uint16_t tableSpaceOffset = readLe16(buf, 40);
                        const std::uint16_t tableSpaceLength = readLe16(buf, 42);
                        const std::uint16_t freeSpaceOffset = readLe16(buf, 44);
                        const std::uint16_t freeSpaceLength = readLe16(buf, 46);
                        const std::size_t btnDataStart = 56U;
                        std::size_t tocStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset);
                        if (tocStart >= buf.size()) tocStart = btnDataStart;
                        std::size_t keyAreaStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset) + static_cast<std::size_t>(tableSpaceLength);
                        if (keyAreaStart >= buf.size()) keyAreaStart = tocStart + (fixedKv ? 4U : 8U) * static_cast<std::size_t>(rootNkeys);
                        std::size_t valueAreaEnd = buf.size();
                        if (valueAreaEnd >= 40U && (btnFlags & 0x0001U) != 0) valueAreaEnd -= 40U; // root nodes end before btree_info_t
                        std::size_t valueAreaStart = keyAreaStart + static_cast<std::size_t>(freeSpaceOffset) + static_cast<std::size_t>(freeSpaceLength);
                        if (valueAreaStart > valueAreaEnd) valueAreaStart = valueAreaEnd;
                        std::size_t maxEntries = rootNkeys;
                        if (maxEntries > 128U) maxEntries = 128U;
                        if (maxEntries == 0U) {
                            ApfsOmapBtreeTocProbeRow row;
                            row.sourceRole = sourceRole;
                            row.omapOid = omapOid;
                            row.omTreeOid = omTreeOid;
                            row.virtualOffset = virtualOffset;
                            row.rootLevel = rootLevel;
                            row.rootNkeys = rootNkeys;
                            row.status = "NO_KEYS_REPORTED";
                            row.interpretation = "OMAP B-tree root reported zero keys; no candidate TOC entries sampled.";
                            row.notes = notes;
                            apfsOmapBtreeTocProbeRows.push_back(row);
                            return;
                        }
                        auto sampleAt = [&](std::size_t abs, std::size_t len) -> std::string {
                            if (len == 0 || abs >= buf.size()) return {};
                            std::size_t n = len;
                            if (abs + n > buf.size()) n = buf.size() - abs;
                            if (n > 32U) n = 32U;
                            return n ? hexSampleBytes(buf.data() + abs, n) : std::string{};
                        };
                        for (std::size_t i = 0; i < maxEntries; ++i) {
                            const std::size_t entrySize = fixedKv ? 4U : 8U;
                            const std::size_t entryOff = tocStart + (i * entrySize);
                            ApfsOmapBtreeTocProbeRow row;
                            row.sourceRole = sourceRole;
                            row.omapOid = omapOid;
                            row.omTreeOid = omTreeOid;
                            row.virtualOffset = virtualOffset;
                            row.rootLevel = rootLevel;
                            row.rootNkeys = rootNkeys;
                            row.entryIndex = static_cast<std::uint32_t>(i);
                            row.tocOffset = static_cast<std::uint32_t>(entryOff);
                            if (fixedKv && entryOff + 4U <= buf.size()) {
                                row.keyOffsetCandidate = readLe16(buf, entryOff + 0U);
                                row.keyLengthCandidate = 16;
                                row.valueOffsetCandidate = readLe16(buf, entryOff + 2U);
                                row.valueLengthCandidate = 16;
                                const std::size_t keyAbs = keyAreaStart + static_cast<std::size_t>(row.keyOffsetCandidate);
                                const std::size_t valAbs = (static_cast<std::size_t>(row.valueOffsetCandidate) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(row.valueOffsetCandidate)) : buf.size();
                                row.keySampleHex = sampleAt(keyAbs, 16U);
                                row.valueSampleHex = sampleAt(valAbs, 16U);
                                row.status = (row.keySampleHex.empty() || row.valueSampleHex.empty()) ? "FIXED_KVOFF_OUT_OF_RANGE" : "FIXED_KVOFF_ENTRY_SAMPLED";
                                row.interpretation = "Fixed-size OMAP B-tree TOC entry decoded as kvoff_t using APFS key/value area boundaries.";
                                row.notes = notes + "; btn_flags=" + std::to_string(btnFlags) + "; toc_start=" + std::to_string(tocStart) + "; key_area_start=" + std::to_string(keyAreaStart) + "; value_area_start=" + std::to_string(valueAreaStart) + "; value_area_end=" + std::to_string(valueAreaEnd);
                                if (rootLevel == 0 && keyAbs + 16U <= buf.size() && valAbs + 16U <= buf.size()) {
                                    ApfsOmapLeafKvDecodeRow kv;
                                    kv.sourceRole = sourceRole;
                                    kv.omapOid = omapOid;
                                    kv.omTreeOid = omTreeOid;
                                    kv.virtualOffset = virtualOffset;
                                    kv.rootLevel = rootLevel;
                                    kv.rootNkeys = rootNkeys;
                                    kv.entryIndex = static_cast<std::uint32_t>(i);
                                    kv.tocOffset = static_cast<std::uint32_t>(entryOff);
                                    kv.keyOffset = row.keyOffsetCandidate;
                                    kv.valueOffset = row.valueOffsetCandidate;
                                    kv.keyOid = readLe64(buf, keyAbs + 0U);
                                    kv.keyXid = readLe64(buf, keyAbs + 8U);
                                    kv.valueFlags = readLe32(buf, valAbs + 0U);
                                    kv.valueSize = readLe32(buf, valAbs + 4U);
                                    kv.valuePaddr = readLe64(buf, valAbs + 8U);
                                    kv.status = "OMAP_LEAF_KV_DECODED";
                                    kv.interpretation = "Decoded OMAP leaf key/value entry. value_paddr is a container-relative physical block address.";
                                    kv.sampleHex = row.keySampleHex + " | " + row.valueSampleHex;
                                    kv.notes = row.notes;
                                    if (nxSummary.blockSize != 0 && kv.valuePaddr <= (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                        kv.resolvedVirtualOffset = kv.valuePaddr * static_cast<std::uint64_t>(nxSummary.blockSize);
                                        if (finalObjectSize == 0 || kv.resolvedVirtualOffset < finalObjectSize) {
                                            std::vector<unsigned char> resolved;
                                            std::string resolvedErr;
                                            kv.resolvedBytesRead = readVirtual(kv.resolvedVirtualOffset, nxSummary.blockSize, resolved, resolvedErr);
                                            if (kv.resolvedBytesRead > 0 && resolved.size() >= 36) {
                                                kv.resolvedObjectOid = readLe64(resolved, 8);
                                                kv.resolvedObjectXid = readLe64(resolved, 16);
                                                kv.resolvedObjectTypeRaw = readLe32(resolved, 24);
                                                kv.resolvedObjectTypeLabel = apfsObjectTypeLabel(kv.resolvedObjectTypeRaw);
                                                kv.resolvedObjectSubtype = readLe32(resolved, 28);
                                                if (std::memcmp(resolved.data() + 32, "APSB", 4) == 0) kv.resolvedMagic = "APSB";
                                                else if (std::memcmp(resolved.data() + 32, "OMAP", 4) == 0) kv.resolvedMagic = "OMAP";
                                                else if (std::memcmp(resolved.data() + 32, "NXSB", 4) == 0) kv.resolvedMagic = "NXSB";
                                                else if (std::memcmp(resolved.data() + 32, "BTMR", 4) == 0) kv.resolvedMagic = "BTMR";
                                                if (kv.resolvedMagic == "APSB") kv.interpretation = "Decoded OMAP leaf entry resolved to an APFS volume superblock candidate.";
                                                else kv.interpretation = "Decoded OMAP leaf entry resolved to an APFS object; continue volume/catalog object-map interpretation.";
                                            } else {
                                                kv.notes += "; resolved_read=" + (resolvedErr.empty() ? std::string("no bytes") : resolvedErr);
                                            }
                                        } else {
                                            kv.notes += "; resolved offset beyond AFF4 virtual object size";
                                        }
                                    }
                                    if (kv.keyOid == nxSummary.omapOid) kv.targetRole = "NX_OMAP_SELF";
                                    for (std::size_t idx = 0; idx < nxSummary.fsOids.size() && idx < 32U; ++idx) {
                                        if (kv.keyOid == nxSummary.fsOids[idx]) kv.targetRole = std::string("NX_FS_OID_") + std::to_string(idx);
                                    }
                                    if (!kv.targetRole.empty() && (nxSummary.nextXid == 0 || kv.keyXid <= nxSummary.nextXid)) kv.lookupStatus = "MATCHES_TARGET_OID_XID_BOUNDS";
                                    else if (!kv.targetRole.empty()) kv.lookupStatus = "MATCHES_TARGET_OID_XID_TOO_NEW";
                                    else kv.lookupStatus = "NON_TARGET_OMAP_ENTRY";
                                    apfsOmapLeafKvDecodeRows.push_back(kv);
                                }
                            } else if (!fixedKv && entryOff + 8U <= buf.size()) {
                                row.keyOffsetCandidate = readLe16(buf, entryOff + 0U);
                                row.keyLengthCandidate = readLe16(buf, entryOff + 2U);
                                row.valueOffsetCandidate = readLe16(buf, entryOff + 4U);
                                row.valueLengthCandidate = readLe16(buf, entryOff + 6U);
                                const std::size_t keyAbs = keyAreaStart + static_cast<std::size_t>(row.keyOffsetCandidate);
                                const std::size_t valAbs = (static_cast<std::size_t>(row.valueOffsetCandidate) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(row.valueOffsetCandidate)) : buf.size();
                                row.keySampleHex = sampleAt(keyAbs, row.keyLengthCandidate);
                                row.valueSampleHex = sampleAt(valAbs, row.valueLengthCandidate);
                                row.status = (row.keySampleHex.empty() && row.valueSampleHex.empty()) ? "KVLOC_ENTRY_OUT_OF_RANGE_OR_EMPTY" : "KVLOC_ENTRY_SAMPLED";
                                row.interpretation = "Variable-size OMAP B-tree TOC entry sampled as kvloc_t.";
                                row.notes = notes + "; btn_flags=" + std::to_string(btnFlags) + "; toc_start=" + std::to_string(tocStart) + "; key_area_start=" + std::to_string(keyAreaStart) + "; value_area_end=" + std::to_string(valueAreaEnd);
                            } else {
                                row.status = "TOC_ENTRY_BEYOND_BLOCK";
                                row.interpretation = "Candidate TOC entry offset exceeded the sampled B-tree root block.";
                                row.notes = notes + "; toc_start=" + std::to_string(tocStart) + "; table_space_offset=" + std::to_string(tableSpaceOffset);
                            }
                            apfsOmapBtreeTocProbeRows.push_back(row);
                        }
                    };

                    auto addOmapPhysFromBuffer = [&](const std::string& sourceRole,
                                                     std::uint64_t sourceOid,
                                                     std::uint64_t virtualOffset,
                                                     long long rc,
                                                     const std::vector<unsigned char>& buf,
                                                     const std::string& notes) {
                        if (rc <= 0 || buf.size() < 88) return;
                        const std::uint32_t rawType = readLe32(buf, 24);
                        const std::string label = apfsObjectTypeLabel(rawType);
                        if (label != "OBJECT_MAP") return;

                        ApfsOmapPhysProbeRow om;
                        om.sourceRole = sourceRole;
                        om.sourceOid = sourceOid;
                        om.virtualOffset = virtualOffset;
                        om.bytesRead = rc;
                        om.objectOid = readLe64(buf, 8);
                        om.objectXid = readLe64(buf, 16);
                        om.objectTypeRaw = rawType;
                        om.objectTypeLabel = label;
                        om.objectSubtype = readLe32(buf, 28);
                        om.omFlags = readLe32(buf, 32);
                        om.omSnapshotCount = readLe32(buf, 36);
                        om.omTreeType = readLe32(buf, 40);
                        om.omSnapshotTreeType = readLe32(buf, 44);
                        om.omTreeOid = readLe64(buf, 48);
                        om.omSnapshotTreeOid = readLe64(buf, 56);
                        om.omMostRecentSnap = readLe64(buf, 64);
                        om.omPendingRevertMin = readLe64(buf, 72);
                        om.omPendingRevertMax = readLe64(buf, 80);
                        om.status = om.omTreeOid ? "OMAP_PHYS_PARSED" : "OMAP_PHYS_PARSED_TREE_OID_ZERO";
                        om.interpretation = "APFS object-map physical object parsed. om_tree_oid is the B-tree root needed for object-id to physical-address resolution.";
                        om.sampleHex = hexSampleBytes(buf.data(), buf.size() < 96 ? buf.size() : 96);
                        om.notes = notes;
                        apfsOmapPhysProbeRows.push_back(om);

                        if (om.omTreeOid == 0 || nxSummary.blockSize == 0 || om.omTreeOid > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                            addOmapLookupReadinessRow("OMAP_TREE_ROOT", om.omTreeOid, om.objectXid, om.objectOid, om.omTreeOid, 0, 0, "OMAP_TREE_NOT_READ", "om_tree_oid was zero or overflowed the bounded offset calculation.", "om_tree_oid=" + std::to_string(om.omTreeOid));
                            return;
                        }

                        const std::uint64_t treeOffset = om.omTreeOid * static_cast<std::uint64_t>(nxSummary.blockSize);
                        if (finalObjectSize != 0 && treeOffset >= finalObjectSize) {
                            addOmapLookupReadinessRow("OMAP_TREE_ROOT", om.omTreeOid, om.objectXid, om.objectOid, om.omTreeOid, 0, 0, "OMAP_TREE_OFFSET_BEYOND_OBJECT", "om_tree_oid offset exceeds AFF4 virtual object size.", "offset=" + std::to_string(treeOffset));
                            return;
                        }

                        std::vector<unsigned char> tree;
                        std::string treeErr;
                        const long long treeRead = readVirtual(treeOffset, nxSummary.blockSize, tree, treeErr);
                        ApfsOmapBtreeRootProbeRow tr;
                        tr.sourceRole = sourceRole;
                        tr.omapOid = om.objectOid;
                        tr.omTreeOid = om.omTreeOid;
                        tr.virtualOffset = treeOffset;
                        tr.bytesRead = treeRead;
                        if (treeRead > 0 && tree.size() >= 56) {
                            tr.objectOid = readLe64(tree, 8);
                            tr.objectXid = readLe64(tree, 16);
                            tr.objectTypeRaw = readLe32(tree, 24);
                            tr.objectTypeLabel = apfsObjectTypeLabel(tr.objectTypeRaw);
                            tr.objectSubtype = readLe32(tree, 28);
                            tr.btnFlags = readLe16(tree, 32);
                            tr.btnLevel = readLe16(tree, 34);
                            tr.btnNkeys = readLe32(tree, 36);
                            tr.tableSpaceOffset = readLe16(tree, 40);
                            tr.tableSpaceLength = readLe16(tree, 42);
                            tr.freeSpaceOffset = readLe16(tree, 44);
                            tr.freeSpaceLength = readLe16(tree, 46);
                            if (tree.size() >= 4096) {
                                const std::size_t footer = tree.size() - 40U;
                                tr.btFlagsCandidate = readLe32(tree, footer + 0);
                                tr.btNodeSizeCandidate = readLe32(tree, footer + 4);
                                tr.btKeySizeCandidate = readLe32(tree, footer + 8);
                                tr.btValSizeCandidate = readLe32(tree, footer + 12);
                            }
                            tr.status = (tr.objectTypeLabel == "BTREE" || tr.objectTypeLabel == "BTREE_NODE") ? "OMAP_BTREE_ROOT_READ" : "OMAP_TREE_OBJECT_UNEXPECTED_TYPE";
                            tr.interpretation = "Object-map B-tree root/header read from om_tree_oid. Root level determines whether branch traversal is required before OMAP key/value lookup.";
                            tr.sampleHex = hexSampleBytes(tree.data(), tree.size() < 96 ? tree.size() : 96);
                            tr.notes = treeErr.empty() ? "Read through AFF4 virtual object using om_tree_oid * nx_block_size." : treeErr;
                            addBtreeProbeFromBuffer("OMAP_TREE_ROOT", om.omTreeOid, 0, treeOffset, treeRead, tree, "Read from om_tree_oid parsed from APFS object-map physical object.");
                            addOmapBtreeTocProbeRowsFromBuffer("OMAP_TREE_ROOT", om.objectOid, om.omTreeOid, treeOffset, tr.btnLevel, tr.btnNkeys, tree, "Read from om_tree_oid parsed from APFS object-map physical object.");

                            addOmapLookupReadinessRow("NX_OMAP_SELF", om.objectOid, nxSummary.nextXid, om.objectOid, om.omTreeOid, tr.btnLevel, tr.btnNkeys,
                                                      tr.btnLevel == 0 ? "OMAP_LEAF_ROOT_READY_FOR_KV_DECODE" : "OMAP_BRANCH_TRAVERSAL_REQUIRED",
                                                      tr.btnLevel == 0 ? "The object-map B-tree root is a leaf; next implementation can decode OMAP key/value TOC directly." : "The object-map B-tree root is not a leaf; next implementation must traverse branch nodes toward target OIDs.",
                                                      "btn_flags=" + std::to_string(tr.btnFlags));
                            for (std::size_t idx = 0; idx < nxSummary.fsOids.size() && idx < 32U; ++idx) {
                                addOmapLookupReadinessRow("NX_FS_OID_" + std::to_string(idx), nxSummary.fsOids[idx], nxSummary.nextXid, om.objectOid, om.omTreeOid, tr.btnLevel, tr.btnNkeys,
                                                          tr.btnLevel == 0 ? "TARGET_READY_FOR_LEAF_LOOKUP" : "TARGET_REQUIRES_BRANCH_TRAVERSAL",
                                                          "Filesystem object ID is now queued for real OMAP lookup using the parsed object-map B-tree.",
                                                          "target_oid=" + std::to_string(nxSummary.fsOids[idx]) + "; nx_next_xid=" + std::to_string(nxSummary.nextXid));
                            }
                        } else {
                            tr.status = "OMAP_BTREE_ROOT_READ_FAILED";
                            tr.interpretation = "Unable to read object-map B-tree root through libaff4 virtual read.";
                            tr.notes = treeErr.empty() ? "AFF4_read returned no bytes for om_tree_oid." : treeErr;
                            addOmapLookupReadinessRow("OMAP_TREE_ROOT", om.omTreeOid, om.objectXid, om.objectOid, om.omTreeOid, 0, 0, tr.status, tr.interpretation, tr.notes);
                        }
                        apfsOmapBtreeRootProbeRows.push_back(tr);
                    };

                    auto probeApfsObjectId = [&](const std::string& role, std::uint64_t oid) {
                        ApfsObjectIdProbeRow orow;
                        orow.role = role;
                        orow.oid = oid;
                        if (oid == 0) {
                            orow.status = "OID_ZERO_SKIPPED";
                            orow.interpretation = "Object ID was zero and was not probed.";
                            apfsObjectIdProbeRows.push_back(orow);
                            return;
                        }
                        if (nxSummary.blockSize == 0 || oid > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                            orow.status = "OFFSET_OVERFLOW";
                            orow.interpretation = "Object ID could not be safely converted to a bounded direct object-id virtual offset.";
                            orow.notes = "oid=" + std::to_string(oid) + "; block_size=" + std::to_string(nxSummary.blockSize);
                            apfsObjectIdProbeRows.push_back(orow);
                            return;
                        }
                        orow.virtualOffset = oid * static_cast<std::uint64_t>(nxSummary.blockSize);
                        if (finalObjectSize != 0 && orow.virtualOffset >= finalObjectSize) {
                            orow.status = "OFFSET_BEYOND_OBJECT_SIZE";
                            orow.interpretation = "Direct object-ID virtual offset exceeds AFF4 object size; OMAP lookup is required.";
                            orow.notes = "offset=" + std::to_string(orow.virtualOffset) + "; object_size=" + std::to_string(finalObjectSize);
                            apfsObjectIdProbeRows.push_back(orow);
                            return;
                        }
                        std::vector<unsigned char> obj;
                        std::string objErr;
                        orow.bytesRead = readVirtual(orow.virtualOffset, nxSummary.blockSize, obj, objErr);
                        orow.status = orow.bytesRead > 0 ? "READ_OK" : "READ_FAILED";
                        if (orow.bytesRead > 0 && obj.size() >= 36) {
                            orow.objectOid = readLe64(obj, 8);
                            orow.objectXid = readLe64(obj, 16);
                            orow.objectTypeRaw = readLe32(obj, 24);
                            orow.objectSubtype = readLe32(obj, 28);
                            orow.objectTypeLabel = apfsObjectTypeLabel(orow.objectTypeRaw);
                            if (std::memcmp(obj.data() + 32, "OMAP", 4) == 0) orow.magic = "OMAP";
                            else if (std::memcmp(obj.data() + 32, "APSB", 4) == 0) orow.magic = "APSB";
                            else if (std::memcmp(obj.data() + 32, "NXSB", 4) == 0) orow.magic = "NXSB";
                            else orow.magic.assign(reinterpret_cast<const char*>(obj.data() + 32), reinterpret_cast<const char*>(obj.data() + 36));
                            if (orow.magic == "OMAP") orow.interpretation = "Direct object-ID read appears to expose an APFS object map.";
                            else if (orow.magic == "APSB") orow.interpretation = "Direct object-ID read appears to expose an APFS volume superblock.";
                            else if (orow.objectTypeLabel == "BTREE" || orow.objectTypeLabel == "BTREE_NODE") orow.interpretation = "Direct object-ID read exposed a B-tree object; OMAP/B-tree entry decoding is needed.";
                            else orow.interpretation = "Direct object-ID block read completed but did not expose OMAP/APSB/NXSB magic.";
                            orow.sampleHex = hexSampleBytes(obj.data(), obj.size() < 96 ? obj.size() : 96);
                            addBtreeProbeFromBuffer(std::string("DIRECT_OBJECT_ID_") + role, oid, 0, orow.virtualOffset, orow.bytesRead, obj, "Direct object-ID read from NX metadata or filesystem OID list.");
                            addOmapPhysFromBuffer(std::string("DIRECT_OBJECT_ID_") + role, oid, orow.virtualOffset, orow.bytesRead, obj, "Direct object-ID read from NX metadata or filesystem OID list.");
                        } else {
                            orow.interpretation = "Unable to read direct object-ID candidate through libaff4 virtual read.";
                            orow.notes = objErr.empty() ? "AFF4_read returned no bytes for direct object-ID candidate." : objErr;
                        }
                        apfsObjectIdProbeRows.push_back(orow);
                    };


                    auto buildResolvedVolumeSuperblocksAndVolumeOmapProbes = [&]() {
                        auto appendBranchPath = [](std::string& path, const std::string& item) {
                            if (!path.empty()) path += " > ";
                            path += item;
                        };

                        auto safeNodeOffset = [&](std::uint64_t oid, std::uint64_t& offsetOut) -> bool {
                            if (nxSummary.blockSize == 0) return false;
                            if (oid > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) return false;
                            offsetOut = oid * static_cast<std::uint64_t>(nxSummary.blockSize);
                            if (finalObjectSize != 0 && offsetOut >= finalObjectSize) return false;
                            return true;
                        };

                        auto fixedKvAbs = [&](const std::vector<unsigned char>& node,
                                              std::uint32_t entryIndex,
                                              std::size_t valueLenNeeded,
                                              std::size_t& keyAbs,
                                              std::size_t& valAbs,
                                              std::string& detail) -> bool {
                            if (node.size() < 64) { detail = "node too small"; return false; }
                            const std::uint16_t btnFlags = readLe16(node, 32);
                            const bool fixedKv = (btnFlags & 0x0004U) != 0;
                            if (!fixedKv) { detail = "node does not advertise fixed-size key/value offsets"; return false; }
                            const std::uint16_t tableSpaceOffset = readLe16(node, 40);
                            const std::uint16_t tableSpaceLength = readLe16(node, 42);
                            const std::size_t btnDataStart = 56U;
                            std::size_t tocStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset);
                            if (tocStart >= node.size()) tocStart = btnDataStart;
                            std::size_t keyAreaStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset) + static_cast<std::size_t>(tableSpaceLength);
                            const std::uint32_t nkeys = readLe32(node, 36);
                            if (keyAreaStart >= node.size()) keyAreaStart = tocStart + 4U * static_cast<std::size_t>(nkeys);
                            std::size_t valueAreaEnd = node.size();
                            if (valueAreaEnd >= 40U && (btnFlags & 0x0001U) != 0U) valueAreaEnd -= 40U;
                            const std::size_t entryOff = tocStart + (static_cast<std::size_t>(entryIndex) * 4U);
                            if (entryOff + 4U > node.size()) { detail = "TOC entry beyond node buffer"; return false; }
                            const std::uint16_t keyOff = readLe16(node, entryOff + 0U);
                            const std::uint16_t valOff = readLe16(node, entryOff + 2U);
                            keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
                            valAbs = (static_cast<std::size_t>(valOff) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(valOff)) : node.size();
                            if (keyAbs + 16U > node.size()) { detail = "OMAP key outside node buffer"; return false; }
                            if (valAbs + valueLenNeeded > node.size()) { detail = "OMAP value outside node buffer"; return false; }
                            detail = "toc_start=" + std::to_string(tocStart) + "; key_area_start=" + std::to_string(keyAreaStart) + "; value_area_end=" + std::to_string(valueAreaEnd);
                            return true;
                        };

                        auto compareOmapKey = [](std::uint64_t oid, std::uint64_t xid, std::uint64_t targetOid, std::uint64_t targetXid) -> int {
                            if (oid < targetOid) return -1;
                            if (oid > targetOid) return 1;
                            if (xid < targetXid) return -1;
                            if (xid > targetXid) return 1;
                            return 0;
                        };


                        auto resolveVolumeOmapTargetObject = [&](const ApfsVolumeOmapProbeRow& omRow,
                                                                 const std::vector<unsigned char>& rootNode,
                                                                 std::uint64_t targetOid,
                                                                 std::uint64_t targetXid,
                                                                 const std::string& purpose) -> ApfsOmapTargetResolution {
                            ApfsOmapTargetResolution out;
                            out.targetOid = targetOid;
                            out.targetXid = targetXid;
                            if (targetOid == 0) {
                                out.lookupStatus = "OMAP_TARGET_OID_ZERO";
                                out.interpretation = purpose + ": target OID was zero.";
                                return out;
                            }
                            if (rootNode.empty() || omRow.treeStatus != "VOLUME_OMAP_BTREE_ROOT_READ") {
                                out.lookupStatus = "VOLUME_OMAP_TREE_UNAVAILABLE";
                                out.interpretation = purpose + ": volume OMAP B-tree root was not available.";
                                out.notes = "volume_omap_tree_status=" + omRow.treeStatus;
                                return out;
                            }

                            std::vector<unsigned char> node = rootNode;
                            std::uint64_t nodeOid = omRow.omTreeOid;
                            std::uint64_t nodeOffset = omRow.treeVirtualOffset;
                            long long nodeRead = omRow.treeBytesRead;
                            constexpr std::uint32_t kMaxDepth = 8;
                            for (std::uint32_t depth = 0; depth < kMaxDepth; ++depth) {
                                out.branchDepth = depth;
                                if (node.size() < 64) {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_TOO_SMALL";
                                    out.interpretation = purpose + ": OMAP node read returned too few bytes for B-tree header parsing.";
                                    break;
                                }
                                const std::uint32_t rawType = readLe32(node, 24);
                                const std::string label = apfsObjectTypeLabel(rawType);
                                const std::uint16_t btnFlags = readLe16(node, 32);
                                const std::uint16_t btnLevel = readLe16(node, 34);
                                const std::uint32_t nkeys = readLe32(node, 36);
                                appendBranchPath(out.branchPath, "oid=" + std::to_string(nodeOid) + ";level=" + std::to_string(btnLevel) + ";nkeys=" + std::to_string(nkeys));
                                if (label != "BTREE" && label != "BTREE_NODE") {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_UNEXPECTED_OBJECT_TYPE";
                                    out.interpretation = purpose + ": node in the volume OMAP path was not an APFS B-tree node.";
                                    out.notes = "object_type=" + label;
                                    break;
                                }
                                if (nkeys > 100000U) {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_SUSPICIOUS_KEY_COUNT";
                                    out.interpretation = purpose + ": OMAP node reported an implausibly large key count; traversal stopped.";
                                    out.notes = "nkeys=" + std::to_string(nkeys);
                                    break;
                                }
                                if (btnLevel == 0) {
                                    out.leafOid = nodeOid;
                                    out.leafVirtualOffset = nodeOffset;
                                    out.leafBytesRead = nodeRead;
                                    out.leafBtnFlags = btnFlags;
                                    out.leafBtnLevel = btnLevel;
                                    out.leafBtnNkeys = nkeys;
                                    out.sampleHex = node.empty() ? std::string{} : hexSampleBytes(node.data(), node.size() < 96 ? node.size() : 96);
                                    bool found = false;
                                    std::uint64_t bestXid = 0;
                                    std::string bestDetail;
                                    const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
                                    for (std::uint32_t i = 0; i < limit; ++i) {
                                        std::size_t keyAbs = 0;
                                        std::size_t valAbs = 0;
                                        std::string detail;
                                        if (!fixedKvAbs(node, i, 16U, keyAbs, valAbs, detail)) {
                                            if (i == 0 && out.notes.empty()) out.notes = detail;
                                            continue;
                                        }
                                        const std::uint64_t keyOid = readLe64(node, keyAbs + 0U);
                                        const std::uint64_t keyXid = readLe64(node, keyAbs + 8U);
                                        if (keyOid != targetOid || keyXid > targetXid) continue;
                                        if (!found || keyXid >= bestXid) {
                                            found = true;
                                            bestXid = keyXid;
                                            bestDetail = detail;
                                            out.matchedEntryIndex = i;
                                            out.matchedKeyOid = keyOid;
                                            out.matchedKeyXid = keyXid;
                                            out.valueFlags = readLe32(node, valAbs + 0U);
                                            out.valueSize = readLe32(node, valAbs + 4U);
                                            out.valuePaddr = readLe64(node, valAbs + 8U);
                                        }
                                    }
                                    if (!found) {
                                        out.lookupStatus = "OMAP_TARGET_LOOKUP_NO_MATCH_IN_LEAF";
                                        out.interpretation = purpose + ": volume OMAP leaf was reached, but no key with xid <= target transaction was found.";
                                        break;
                                    }
                                    out.lookupStatus = "OMAP_TARGET_LOOKUP_RESOLVED";
                                    out.notes += out.notes.empty() ? bestDetail : ("; " + bestDetail);
                                    if ((out.valueFlags & 0x00000001U) != 0U) {
                                        out.objectStatus = "OMAP_TARGET_VALUE_DELETED";
                                        out.interpretation = purpose + ": matching OMAP value was marked deleted; object not read as active.";
                                        break;
                                    }
                                    if (!safeNodeOffset(out.valuePaddr, out.resolvedVirtualOffset)) {
                                        out.objectStatus = "OMAP_TARGET_RESOLVED_OFFSET_UNSAFE";
                                        out.interpretation = purpose + ": matching OMAP value was found, but value_paddr could not be converted to a safe read offset.";
                                        break;
                                    }
                                    std::vector<unsigned char> resolved;
                                    std::string resolvedErr;
                                    out.resolvedBytesRead = readVirtual(out.resolvedVirtualOffset, nxSummary.blockSize, resolved, resolvedErr);
                                    if (out.resolvedBytesRead > 0 && resolved.size() >= 56) {
                                        out.resolvedObjectOid = readLe64(resolved, 8);
                                        out.resolvedObjectXid = readLe64(resolved, 16);
                                        out.resolvedObjectTypeRaw = readLe32(resolved, 24);
                                        out.resolvedObjectTypeLabel = apfsObjectTypeLabel(out.resolvedObjectTypeRaw);
                                        out.resolvedObjectSubtype = readLe32(resolved, 28);
                                        if (resolved.size() >= 36) out.resolvedMagic.assign(reinterpret_cast<const char*>(resolved.data() + 32), reinterpret_cast<const char*>(resolved.data() + 36));
                                        if (out.resolvedObjectTypeLabel == "BTREE" || out.resolvedObjectTypeLabel == "BTREE_NODE") {
                                            out.resolvedBtnFlags = readLe16(resolved, 32);
                                            out.resolvedBtnLevel = readLe16(resolved, 34);
                                            out.resolvedBtnNkeys = readLe32(resolved, 36);
                                            out.objectStatus = "OMAP_TARGET_BTREE_READ";
                                            out.interpretation = purpose + ": target object resolved through the volume OMAP and read as a B-tree node.";
                                        } else {
                                            out.objectStatus = "OMAP_TARGET_UNEXPECTED_OBJECT_TYPE";
                                            out.interpretation = purpose + ": target object resolved through the volume OMAP, but was not a B-tree node.";
                                        }
                                        out.resolvedSampleHex = hexSampleBytes(resolved.data(), resolved.size() < 96 ? resolved.size() : 96);
                                        out.resolvedBuffer.swap(resolved);
                                    } else {
                                        out.objectStatus = "OMAP_TARGET_READ_FAILED";
                                        out.interpretation = purpose + ": matching OMAP value was found, but the resolved object read failed.";
                                        out.notes += out.notes.empty() ? resolvedErr : ("; " + resolvedErr);
                                    }
                                    break;
                                }

                                std::uint64_t childOid = 0;
                                std::uint32_t childEntry = 0;
                                bool childFound = false;
                                bool usedFallbackFirst = false;
                                const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
                                std::uint64_t fallbackChild = 0;
                                std::uint32_t fallbackEntry = 0;
                                for (std::uint32_t i = 0; i < limit; ++i) {
                                    std::size_t keyAbs = 0;
                                    std::size_t valAbs = 0;
                                    std::string detail;
                                    if (!fixedKvAbs(node, i, 8U, keyAbs, valAbs, detail)) {
                                        if (i == 0 && out.notes.empty()) out.notes = detail;
                                        continue;
                                    }
                                    const std::uint64_t keyOid = readLe64(node, keyAbs + 0U);
                                    const std::uint64_t keyXid = readLe64(node, keyAbs + 8U);
                                    const std::uint64_t candidateChild = readLe64(node, valAbs + 0U);
                                    if (i == 0) { fallbackChild = candidateChild; fallbackEntry = i; }
                                    if (compareOmapKey(keyOid, keyXid, targetOid, targetXid) <= 0) {
                                        childOid = candidateChild;
                                        childEntry = i;
                                        childFound = true;
                                    }
                                }
                                if (!childFound && fallbackChild != 0) {
                                    childOid = fallbackChild;
                                    childEntry = fallbackEntry;
                                    childFound = true;
                                    usedFallbackFirst = true;
                                }
                                if (!childFound || childOid == 0) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_NOT_SELECTED";
                                    out.interpretation = purpose + ": OMAP branch node parsed, but no usable child pointer could be selected for the target key.";
                                    break;
                                }
                                appendBranchPath(out.branchPath, "child_entry=" + std::to_string(childEntry) + ";child_oid=" + std::to_string(childOid) + (usedFallbackFirst ? ";fallback_first" : ""));
                                std::uint64_t childOffset = 0;
                                if (!safeNodeOffset(childOid, childOffset)) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_OFFSET_UNSAFE";
                                    out.interpretation = purpose + ": selected OMAP branch child OID could not be converted to a safe read offset.";
                                    break;
                                }
                                std::vector<unsigned char> child;
                                std::string childErr;
                                const long long childRead = readVirtual(childOffset, nxSummary.blockSize, child, childErr);
                                if (childRead <= 0 || child.size() < 64) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_READ_FAILED";
                                    out.interpretation = purpose + ": selected OMAP branch child could not be read as a B-tree node.";
                                    out.notes += out.notes.empty() ? childErr : ("; " + childErr);
                                    break;
                                }
                                node.swap(child);
                                nodeOid = childOid;
                                nodeOffset = childOffset;
                                nodeRead = childRead;
                            }
                            if (out.lookupStatus.empty()) {
                                out.lookupStatus = "VOLUME_OMAP_BRANCH_TRAVERSAL_LIMIT_REACHED";
                                out.interpretation = purpose + ": bounded OMAP branch traversal reached the safety depth limit before a leaf node was resolved.";
                            }
                            return out;
                        };

                        auto decodeVolumeOmapRootTreeLookup = [&](const ApfsResolvedVolumeSuperblockRow& volRow,
                                                                  const ApfsVolumeOmapProbeRow& omRow,
                                                                  const std::vector<unsigned char>& rootNode) {
                            ApfsVolumeRootTreeLookupRow out;
                            out.sequence = static_cast<std::uint32_t>(apfsVolumeRootTreeLookupRows.size());
                            out.volumeSequence = omRow.volumeSequence;
                            out.targetRole = omRow.targetRole;
                            out.fsOid = omRow.fsOid;
                            out.volumeName = volRow.volumeName;
                            out.apfsOmapOid = omRow.apfsOmapOid;
                            out.omTreeOid = omRow.omTreeOid;
                            out.apfsRootTreeOid = omRow.apfsRootTreeOid;
                            out.targetXid = volRow.objectXid;

                            if (out.apfsRootTreeOid == 0) {
                                out.lookupStatus = "VOLUME_ROOT_TREE_OID_ZERO";
                                out.interpretation = "Parsed APSB did not contain an apfs_root_tree_oid target.";
                                apfsVolumeRootTreeLookupRows.push_back(out);
                                return;
                            }
                            if (rootNode.empty() || omRow.treeStatus != "VOLUME_OMAP_BTREE_ROOT_READ") {
                                out.lookupStatus = "VOLUME_OMAP_TREE_UNAVAILABLE";
                                out.interpretation = "Volume OMAP B-tree root was not available, so apfs_root_tree_oid could not be resolved.";
                                out.notes = "volume_omap_tree_status=" + omRow.treeStatus;
                                apfsVolumeRootTreeLookupRows.push_back(out);
                                return;
                            }

                            std::vector<unsigned char> node = rootNode;
                            std::uint64_t nodeOid = omRow.omTreeOid;
                            std::uint64_t nodeOffset = omRow.treeVirtualOffset;
                            long long nodeRead = omRow.treeBytesRead;
                            constexpr std::uint32_t kMaxDepth = 8;
                            for (std::uint32_t depth = 0; depth < kMaxDepth; ++depth) {
                                out.branchDepth = depth;
                                if (node.size() < 64) {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_TOO_SMALL";
                                    out.interpretation = "A volume OMAP node read returned too few bytes for B-tree header parsing.";
                                    break;
                                }
                                const std::uint32_t rawType = readLe32(node, 24);
                                const std::string label = apfsObjectTypeLabel(rawType);
                                const std::uint16_t btnFlags = readLe16(node, 32);
                                const std::uint16_t btnLevel = readLe16(node, 34);
                                const std::uint32_t nkeys = readLe32(node, 36);
                                appendBranchPath(out.branchPath, "oid=" + std::to_string(nodeOid) + ";level=" + std::to_string(btnLevel) + ";nkeys=" + std::to_string(nkeys));
                                if (label != "BTREE" && label != "BTREE_NODE") {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_UNEXPECTED_OBJECT_TYPE";
                                    out.interpretation = "A node in the volume OMAP path was not an APFS B-tree node.";
                                    out.notes = "object_type=" + label;
                                    break;
                                }
                                if (nkeys > 100000U) {
                                    out.lookupStatus = "VOLUME_OMAP_NODE_SUSPICIOUS_KEY_COUNT";
                                    out.interpretation = "A volume OMAP node reported an implausibly large key count; traversal stopped.";
                                    out.notes = "nkeys=" + std::to_string(nkeys);
                                    break;
                                }
                                if (btnLevel == 0) {
                                    out.leafOid = nodeOid;
                                    out.leafVirtualOffset = nodeOffset;
                                    out.leafBytesRead = nodeRead;
                                    out.leafBtnFlags = btnFlags;
                                    out.leafBtnLevel = btnLevel;
                                    out.leafBtnNkeys = nkeys;
                                    out.sampleHex = node.empty() ? std::string{} : hexSampleBytes(node.data(), node.size() < 96 ? node.size() : 96);
                                    bool found = false;
                                    std::uint64_t bestXid = 0;
                                    std::string bestDetail;
                                    const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
                                    for (std::uint32_t i = 0; i < limit; ++i) {
                                        std::size_t keyAbs = 0;
                                        std::size_t valAbs = 0;
                                        std::string detail;
                                        if (!fixedKvAbs(node, i, 16U, keyAbs, valAbs, detail)) {
                                            if (i == 0 && out.notes.empty()) out.notes = detail;
                                            continue;
                                        }
                                        const std::uint64_t keyOid = readLe64(node, keyAbs + 0U);
                                        const std::uint64_t keyXid = readLe64(node, keyAbs + 8U);
                                        if (keyOid != out.apfsRootTreeOid || keyXid > out.targetXid) continue;
                                        if (!found || keyXid >= bestXid) {
                                            found = true;
                                            bestXid = keyXid;
                                            bestDetail = detail;
                                            out.matchedEntryIndex = i;
                                            out.matchedKeyOid = keyOid;
                                            out.matchedKeyXid = keyXid;
                                            out.valueFlags = readLe32(node, valAbs + 0U);
                                            out.valueSize = readLe32(node, valAbs + 4U);
                                            out.valuePaddr = readLe64(node, valAbs + 8U);
                                        }
                                    }
                                    if (!found) {
                                        out.lookupStatus = "VOLUME_ROOT_TREE_LOOKUP_NO_MATCH_IN_LEAF";
                                        out.interpretation = "Volume OMAP leaf was reached, but no apfs_root_tree_oid key with xid <= APSB xid was found.";
                                        break;
                                    }
                                    out.lookupStatus = "VOLUME_ROOT_TREE_LOOKUP_RESOLVED";
                                    out.notes += out.notes.empty() ? bestDetail : ("; " + bestDetail);
                                    if ((out.valueFlags & 0x00000001U) != 0U) {
                                        out.rootTreeStatus = "ROOT_TREE_OMAP_VALUE_DELETED";
                                        out.interpretation = "Matching OMAP value was marked deleted; root-tree object not read as active.";
                                        break;
                                    }
                                    if (!safeNodeOffset(out.valuePaddr, out.resolvedVirtualOffset)) {
                                        out.rootTreeStatus = "ROOT_TREE_RESOLVED_OFFSET_UNSAFE";
                                        out.interpretation = "Matching OMAP value was found, but value_paddr could not be converted to a safe read offset.";
                                        break;
                                    }
                                    std::vector<unsigned char> resolved;
                                    std::string resolvedErr;
                                    out.resolvedBytesRead = readVirtual(out.resolvedVirtualOffset, nxSummary.blockSize, resolved, resolvedErr);
                                    if (out.resolvedBytesRead > 0 && resolved.size() >= 56) {
                                        out.resolvedObjectOid = readLe64(resolved, 8);
                                        out.resolvedObjectXid = readLe64(resolved, 16);
                                        out.resolvedObjectTypeRaw = readLe32(resolved, 24);
                                        out.resolvedObjectTypeLabel = apfsObjectTypeLabel(out.resolvedObjectTypeRaw);
                                        out.resolvedObjectSubtype = readLe32(resolved, 28);
                                        if (resolved.size() >= 36) out.resolvedMagic.assign(reinterpret_cast<const char*>(resolved.data() + 32), reinterpret_cast<const char*>(resolved.data() + 36));
                                        if (out.resolvedObjectTypeLabel == "BTREE" || out.resolvedObjectTypeLabel == "BTREE_NODE") {
                                            out.resolvedBtnFlags = readLe16(resolved, 32);
                                            out.resolvedBtnLevel = readLe16(resolved, 34);
                                            out.resolvedBtnNkeys = readLe32(resolved, 36);
                                            out.rootTreeStatus = "ROOT_TREE_BTREE_READ";
                                            out.interpretation = "apfs_root_tree_oid resolved through the volume OMAP and the root-tree B-tree header was read.";
                                        } else {
                                            out.rootTreeStatus = "ROOT_TREE_UNEXPECTED_OBJECT_TYPE";
                                            out.interpretation = "apfs_root_tree_oid resolved through the volume OMAP, but the resolved object was not a B-tree header.";
                                        }
                                        out.resolvedSampleHex = hexSampleBytes(resolved.data(), resolved.size() < 96 ? resolved.size() : 96);
                                    } else {
                                        out.rootTreeStatus = "ROOT_TREE_READ_FAILED";
                                        out.interpretation = "Matching OMAP value was found, but the resolved root-tree object read failed.";
                                        out.notes += out.notes.empty() ? resolvedErr : ("; " + resolvedErr);
                                    }
                                    break;
                                }

                                std::uint64_t childOid = 0;
                                std::uint32_t childEntry = 0;
                                bool childFound = false;
                                bool usedFallbackFirst = false;
                                const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
                                std::uint64_t fallbackChild = 0;
                                std::uint32_t fallbackEntry = 0;
                                for (std::uint32_t i = 0; i < limit; ++i) {
                                    std::size_t keyAbs = 0;
                                    std::size_t valAbs = 0;
                                    std::string detail;
                                    if (!fixedKvAbs(node, i, 8U, keyAbs, valAbs, detail)) {
                                        if (i == 0 && out.notes.empty()) out.notes = detail;
                                        continue;
                                    }
                                    const std::uint64_t keyOid = readLe64(node, keyAbs + 0U);
                                    const std::uint64_t keyXid = readLe64(node, keyAbs + 8U);
                                    const std::uint64_t candidateChild = readLe64(node, valAbs + 0U);
                                    if (i == 0) { fallbackChild = candidateChild; fallbackEntry = i; }
                                    if (compareOmapKey(keyOid, keyXid, out.apfsRootTreeOid, out.targetXid) <= 0) {
                                        childOid = candidateChild;
                                        childEntry = i;
                                        childFound = true;
                                    }
                                }
                                if (!childFound && fallbackChild != 0) {
                                    childOid = fallbackChild;
                                    childEntry = fallbackEntry;
                                    childFound = true;
                                    usedFallbackFirst = true;
                                }
                                if (!childFound || childOid == 0) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_NOT_SELECTED";
                                    out.interpretation = "Branch node parsed, but no usable child pointer could be selected for the target key.";
                                    break;
                                }
                                appendBranchPath(out.branchPath, "child_entry=" + std::to_string(childEntry) + ";child_oid=" + std::to_string(childOid) + (usedFallbackFirst ? ";fallback_first" : ""));
                                std::uint64_t childOffset = 0;
                                if (!safeNodeOffset(childOid, childOffset)) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_OFFSET_UNSAFE";
                                    out.interpretation = "Selected branch child OID could not be converted to a safe read offset.";
                                    break;
                                }
                                std::vector<unsigned char> child;
                                std::string childErr;
                                const long long childRead = readVirtual(childOffset, nxSummary.blockSize, child, childErr);
                                if (childRead <= 0 || child.size() < 64) {
                                    out.lookupStatus = "VOLUME_OMAP_BRANCH_CHILD_READ_FAILED";
                                    out.interpretation = "Selected branch child could not be read as a B-tree node.";
                                    out.notes += out.notes.empty() ? childErr : ("; " + childErr);
                                    break;
                                }
                                node.swap(child);
                                nodeOid = childOid;
                                nodeOffset = childOffset;
                                nodeRead = childRead;
                            }
                            if (out.lookupStatus.empty()) {
                                out.lookupStatus = "VOLUME_OMAP_BRANCH_TRAVERSAL_LIMIT_REACHED";
                                out.interpretation = "Bounded branch traversal reached the safety depth limit before a leaf node was resolved.";
                            }
                            apfsVolumeRootTreeLookupRows.push_back(out);
                        };

                        auto appendVolumeOmapAndRootLookup = [&](const ApfsResolvedVolumeSuperblockRow& volRow,
                                                                 const ApfsVolumeOmapProbeRow& omRow,
                                                                 const std::vector<unsigned char>& rootNode) {
                            apfsVolumeOmapRowsByVolumeSequence[omRow.volumeSequence] = omRow;
                            if (!rootNode.empty()) apfsVolumeOmapRootNodesByVolumeSequence[omRow.volumeSequence] = rootNode;
                            apfsVolumeOmapProbeRows.push_back(omRow);
                            decodeVolumeOmapRootTreeLookup(volRow, omRow, rootNode);
                        };

                        auto genericBtreeKvAbs = [&](const std::vector<unsigned char>& node,
                                                    std::uint32_t entryIndex,
                                                    std::size_t& tocAbs,
                                                    std::size_t& keyAbs,
                                                    std::size_t& keyLen,
                                                    std::size_t& valAbs,
                                                    std::size_t& valLen,
                                                    std::string& detail) -> bool {
                            if (node.size() < 64) { detail = "node too small"; return false; }
                            const std::uint16_t btnFlags = readLe16(node, 32);
                            const bool fixedKv = (btnFlags & 0x0004U) != 0;
                            const std::uint16_t tableSpaceOffset = readLe16(node, 40);
                            const std::uint16_t tableSpaceLength = readLe16(node, 42);
                            const std::size_t btnDataStart = 56U;
                            const std::uint32_t nkeys = readLe32(node, 36);
                            const std::size_t entrySize = fixedKv ? 4U : 8U;
                            std::size_t tocStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset);
                            if (tocStart >= node.size()) tocStart = btnDataStart;
                            std::size_t keyAreaStart = btnDataStart + static_cast<std::size_t>(tableSpaceOffset) + static_cast<std::size_t>(tableSpaceLength);
                            if (keyAreaStart >= node.size()) keyAreaStart = tocStart + (entrySize * static_cast<std::size_t>(nkeys));
                            std::size_t valueAreaEnd = node.size();
                            if (valueAreaEnd >= 40U && (btnFlags & 0x0001U) != 0U) valueAreaEnd -= 40U;
                            tocAbs = tocStart + (static_cast<std::size_t>(entryIndex) * entrySize);
                            if (tocAbs + entrySize > node.size()) { detail = "TOC entry beyond node buffer"; return false; }
                            if (fixedKv) {
                                const std::uint16_t keyOff = readLe16(node, tocAbs + 0U);
                                const std::uint16_t valOff = readLe16(node, tocAbs + 2U);
                                keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
                                keyLen = 16U;
                                valAbs = (static_cast<std::size_t>(valOff) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(valOff)) : node.size();
                                valLen = (valAbs + 16U <= node.size()) ? 16U : 0U;
                            } else {
                                const std::uint16_t keyOff = readLe16(node, tocAbs + 0U);
                                const std::uint16_t keyLength = readLe16(node, tocAbs + 2U);
                                const std::uint16_t valOff = readLe16(node, tocAbs + 4U);
                                const std::uint16_t valLength = readLe16(node, tocAbs + 6U);
                                keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
                                keyLen = keyLength;
                                valAbs = (static_cast<std::size_t>(valOff) <= valueAreaEnd) ? (valueAreaEnd - static_cast<std::size_t>(valOff)) : node.size();
                                valLen = valLength;
                            }
                            if (keyAbs >= node.size()) { detail = "key outside node buffer"; return false; }
                            if (keyLen == 0 || keyAbs + keyLen > node.size()) { detail = "key length outside node buffer"; return false; }
                            if (valLen > 0 && (valAbs >= node.size() || valAbs + valLen > node.size())) { detail = "value length outside node buffer"; return false; }
                            detail = "toc_start=" + std::to_string(tocStart) + "; key_area_start=" + std::to_string(keyAreaStart) + "; value_area_end=" + std::to_string(valueAreaEnd) + (fixedKv ? "; fixed_kv=true" : "; fixed_kv=false");
                            return true;
                        };

                        std::set<std::string> seenResolved;
                        for (const auto& kv : apfsOmapLeafKvDecodeRows) {
                            if (kv.targetRole.rfind("NX_FS_OID_", 0) != 0) continue;
                            if (kv.lookupStatus != "MATCHES_TARGET_OID_XID_BOUNDS") continue;
                            const std::string seenKey = kv.targetRole + ":" + std::to_string(kv.keyOid) + ":" + std::to_string(kv.keyXid) + ":" + std::to_string(kv.valuePaddr);
                            if (!seenResolved.insert(seenKey).second) continue;

                            ApfsResolvedVolumeSuperblockRow row;
                            row.sequence = static_cast<std::uint32_t>(apfsResolvedVolumeSuperblockRows.size());
                            row.targetRole = kv.targetRole;
                            row.fsOid = kv.keyOid;
                            row.containerTargetXid = nxSummary.nextXid;
                            row.omapKeyOid = kv.keyOid;
                            row.omapKeyXid = kv.keyXid;
                            row.omapValueFlags = kv.valueFlags;
                            row.omapValueSize = kv.valueSize;
                            row.omapValuePaddr = kv.valuePaddr;
                            row.resolvedVirtualOffset = kv.resolvedVirtualOffset;
                            if ((kv.valueFlags & 0x00000001U) != 0U) {
                                row.notes = "OMAP value has OMAP_VAL_DELETED set; parsed only for forensic visibility.";
                            }
                            if (nxSummary.blockSize == 0 || kv.valuePaddr > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                row.status = "RESOLVED_APSB_OFFSET_OVERFLOW";
                                row.interpretation = "The selected OMAP value physical address could not be converted to a safe virtual offset.";
                                row.notes += row.notes.empty() ? "" : "; ";
                                row.notes += "value_paddr=" + std::to_string(kv.valuePaddr) + "; block_size=" + std::to_string(nxSummary.blockSize);
                                apfsResolvedVolumeSuperblockRows.push_back(row);
                                continue;
                            }
                            if (row.resolvedVirtualOffset == 0) row.resolvedVirtualOffset = kv.valuePaddr * static_cast<std::uint64_t>(nxSummary.blockSize);
                            if (finalObjectSize != 0 && row.resolvedVirtualOffset >= finalObjectSize) {
                                row.status = "RESOLVED_APSB_OFFSET_BEYOND_OBJECT";
                                row.interpretation = "The selected OMAP value physical address points beyond the reported AFF4 virtual object size.";
                                row.notes += row.notes.empty() ? "" : "; ";
                                row.notes += "resolved_offset=" + std::to_string(row.resolvedVirtualOffset) + "; object_size=" + std::to_string(finalObjectSize);
                                apfsResolvedVolumeSuperblockRows.push_back(row);
                                continue;
                            }
                            std::vector<unsigned char> vol;
                            std::string volErr;
                            row.resolvedBytesRead = readVirtual(row.resolvedVirtualOffset, nxSummary.blockSize, vol, volErr);
                            parseResolvedApfsVolumeSuperblock(row, vol, volErr.empty() ? std::string("Read through container OMAP-selected value_paddr.") : volErr);
                            const std::uint32_t volumeSequence = row.sequence;
                            apfsResolvedVolumeSuperblockRows.push_back(row);

                            if (row.status != "APSB_PARSED_FROM_CONTAINER_OMAP") continue;

                            ApfsVolumeOmapProbeRow omRow;
                            omRow.sequence = static_cast<std::uint32_t>(apfsVolumeOmapProbeRows.size());
                            omRow.volumeSequence = volumeSequence;
                            omRow.targetRole = row.targetRole;
                            omRow.fsOid = row.fsOid;
                            omRow.volumeObjectOid = row.objectOid;
                            omRow.volumeObjectXid = row.objectXid;
                            omRow.apfsOmapOid = row.apfsOmapOid;
                            omRow.apfsRootTreeOid = row.rootTreeOid;
                            if (row.apfsOmapOid == 0) {
                                omRow.omapStatus = "VOLUME_OMAP_OID_ZERO";
                                omRow.interpretation = "Parsed APSB did not contain an apfs_omap_oid value.";
                                appendVolumeOmapAndRootLookup(row, omRow, std::vector<unsigned char>{});
                                continue;
                            }
                            if (nxSummary.blockSize == 0 || row.apfsOmapOid > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                omRow.omapStatus = "VOLUME_OMAP_OFFSET_OVERFLOW";
                                omRow.interpretation = "apfs_omap_oid could not be converted to a bounded physical-block offset.";
                                omRow.notes = "apfs_omap_oid=" + std::to_string(row.apfsOmapOid) + "; block_size=" + std::to_string(nxSummary.blockSize);
                                appendVolumeOmapAndRootLookup(row, omRow, std::vector<unsigned char>{});
                                continue;
                            }
                            omRow.omapVirtualOffset = row.apfsOmapOid * static_cast<std::uint64_t>(nxSummary.blockSize);
                            if (finalObjectSize != 0 && omRow.omapVirtualOffset >= finalObjectSize) {
                                omRow.omapStatus = "VOLUME_OMAP_OFFSET_BEYOND_OBJECT";
                                omRow.interpretation = "apfs_omap_oid physical-block offset exceeds the AFF4 virtual object size.";
                                omRow.notes = "offset=" + std::to_string(omRow.omapVirtualOffset) + "; object_size=" + std::to_string(finalObjectSize);
                                appendVolumeOmapAndRootLookup(row, omRow, std::vector<unsigned char>{});
                                continue;
                            }
                            std::vector<unsigned char> omapBuf;
                            std::string omapErr;
                            omRow.omapBytesRead = readVirtual(omRow.omapVirtualOffset, nxSummary.blockSize, omapBuf, omapErr);
                            std::vector<unsigned char> omapTreeRootBuf;
                            if (omRow.omapBytesRead > 0 && omapBuf.size() >= 88) {
                                omRow.omapObjectOid = readLe64(omapBuf, 8);
                                omRow.omapObjectXid = readLe64(omapBuf, 16);
                                omRow.omapObjectTypeRaw = readLe32(omapBuf, 24);
                                omRow.omapObjectTypeLabel = apfsObjectTypeLabel(omRow.omapObjectTypeRaw);
                                omRow.omapObjectSubtype = readLe32(omapBuf, 28);
                                omRow.omFlags = readLe32(omapBuf, 32);
                                omRow.omSnapshotCount = readLe32(omapBuf, 36);
                                omRow.omTreeType = readLe32(omapBuf, 40);
                                omRow.omSnapshotTreeType = readLe32(omapBuf, 44);
                                omRow.omTreeOid = readLe64(omapBuf, 48);
                                omRow.omSnapshotTreeOid = readLe64(omapBuf, 56);
                                omRow.omMostRecentSnap = readLe64(omapBuf, 64);
                                omRow.omPendingRevertMin = readLe64(omapBuf, 72);
                                omRow.omPendingRevertMax = readLe64(omapBuf, 80);
                                omRow.sampleHex = hexSampleBytes(omapBuf.data(), omapBuf.size() < 96 ? omapBuf.size() : 96);
                                if (omRow.omapObjectTypeLabel == "OBJECT_MAP") {
                                    omRow.omapStatus = "VOLUME_OMAP_PARSED";
                                    omRow.interpretation = "Volume object-map physical object parsed from apfs_omap_oid. om_tree_oid is ready for volume-level object lookup.";
                                } else {
                                    omRow.omapStatus = "VOLUME_OMAP_UNEXPECTED_OBJECT_TYPE";
                                    omRow.interpretation = "apfs_omap_oid was read, but the object header type was not OBJECT_MAP.";
                                }
                                if (omRow.omTreeOid != 0 && nxSummary.blockSize != 0 && omRow.omTreeOid <= (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                    omRow.treeVirtualOffset = omRow.omTreeOid * static_cast<std::uint64_t>(nxSummary.blockSize);
                                    if (finalObjectSize == 0 || omRow.treeVirtualOffset < finalObjectSize) {
                                        std::vector<unsigned char> treeBuf;
                                        std::string treeErr;
                                        omRow.treeBytesRead = readVirtual(omRow.treeVirtualOffset, nxSummary.blockSize, treeBuf, treeErr);
                                        if (omRow.treeBytesRead > 0 && treeBuf.size() >= 56) {
                                            omRow.treeObjectOid = readLe64(treeBuf, 8);
                                            omRow.treeObjectXid = readLe64(treeBuf, 16);
                                            omRow.treeObjectTypeRaw = readLe32(treeBuf, 24);
                                            omRow.treeObjectTypeLabel = apfsObjectTypeLabel(omRow.treeObjectTypeRaw);
                                            omRow.treeObjectSubtype = readLe32(treeBuf, 28);
                                            omRow.treeBtnFlags = readLe16(treeBuf, 32);
                                            omRow.treeBtnLevel = readLe16(treeBuf, 34);
                                            omRow.treeBtnNkeys = readLe32(treeBuf, 36);
                                            omRow.treeTableSpaceOffset = readLe16(treeBuf, 40);
                                            omRow.treeTableSpaceLength = readLe16(treeBuf, 42);
                                            omRow.treeSampleHex = hexSampleBytes(treeBuf.data(), treeBuf.size() < 96 ? treeBuf.size() : 96);
                                            omRow.treeStatus = (omRow.treeObjectTypeLabel == "BTREE" || omRow.treeObjectTypeLabel == "BTREE_NODE") ? "VOLUME_OMAP_BTREE_ROOT_READ" : "VOLUME_OMAP_TREE_UNEXPECTED_OBJECT_TYPE";
                                            if (omRow.treeStatus == "VOLUME_OMAP_BTREE_ROOT_READ") {
                                                omapTreeRootBuf = treeBuf;
                                            }
                                        } else {
                                            omRow.treeStatus = "VOLUME_OMAP_BTREE_ROOT_READ_FAILED";
                                            omRow.notes += omRow.notes.empty() ? "" : "; ";
                                            omRow.notes += treeErr.empty() ? std::string("AFF4_read returned no bytes for volume OMAP om_tree_oid.") : treeErr;
                                        }
                                    } else {
                                        omRow.treeStatus = "VOLUME_OMAP_TREE_OFFSET_BEYOND_OBJECT";
                                    }
                                } else if (omRow.omTreeOid == 0) {
                                    omRow.treeStatus = "VOLUME_OMAP_TREE_OID_ZERO";
                                } else {
                                    omRow.treeStatus = "VOLUME_OMAP_TREE_OFFSET_OVERFLOW";
                                }
                            } else {
                                omRow.omapStatus = "VOLUME_OMAP_READ_FAILED";
                                omRow.interpretation = "Unable to read volume object-map physical object through libaff4 virtual read.";
                                omRow.notes = omapErr.empty() ? "AFF4_read returned no bytes for apfs_omap_oid." : omapErr;
                            }
                            appendVolumeOmapAndRootLookup(row, omRow, omapTreeRootBuf);
                        }

                        constexpr std::uint32_t kMaxRootTreeRecordSamplesPerVolume = 32;
                        for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                            ApfsRootTreeNodeProbeRow nr;
                            nr.sequence = static_cast<std::uint32_t>(apfsRootTreeNodeProbeRows.size());
                            nr.volumeSequence = lookup.volumeSequence;
                            nr.targetRole = lookup.targetRole;
                            nr.fsOid = lookup.fsOid;
                            nr.volumeName = lookup.volumeName;
                            nr.apfsRootTreeOid = lookup.apfsRootTreeOid;
                            nr.targetXid = lookup.targetXid;
                            nr.nodeOid = lookup.resolvedObjectOid ? lookup.resolvedObjectOid : lookup.apfsRootTreeOid;
                            nr.virtualOffset = lookup.resolvedVirtualOffset;
                            nr.bytesRead = lookup.resolvedBytesRead;
                            if (lookup.rootTreeStatus != "ROOT_TREE_BTREE_READ" || lookup.resolvedVirtualOffset == 0) {
                                nr.status = "ROOT_TREE_NODE_NOT_AVAILABLE";
                                nr.interpretation = "No readable APFS filesystem root-tree B-tree header was available for bounded key sampling.";
                                nr.notes = "root_tree_status=" + lookup.rootTreeStatus + "; lookup_status=" + lookup.lookupStatus;
                                apfsRootTreeNodeProbeRows.push_back(nr);
                                continue;
                            }
                            std::vector<unsigned char> node;
                            std::string readErr;
                            const long long rc = readVirtual(lookup.resolvedVirtualOffset, nxSummary.blockSize, node, readErr);
                            nr.bytesRead = rc;
                            if (rc <= 0 || node.size() < 64) {
                                nr.status = "ROOT_TREE_NODE_READ_FAILED";
                                nr.interpretation = "Resolved root-tree offset could not be read again for node/key sampling.";
                                nr.notes = readErr;
                                apfsRootTreeNodeProbeRows.push_back(nr);
                                continue;
                            }
                            nr.objectOid = readLe64(node, 8);
                            nr.objectXid = readLe64(node, 16);
                            nr.objectTypeRaw = readLe32(node, 24);
                            nr.objectTypeLabel = apfsObjectTypeLabel(nr.objectTypeRaw);
                            nr.objectSubtype = readLe32(node, 28);
                            if (node.size() >= 36) nr.magic.assign(reinterpret_cast<const char*>(node.data() + 32), reinterpret_cast<const char*>(node.data() + 36));
                            nr.btnFlags = readLe16(node, 32);
                            nr.btnLevel = readLe16(node, 34);
                            nr.btnNkeys = readLe32(node, 36);
                            nr.tableSpaceOffset = readLe16(node, 40);
                            nr.tableSpaceLength = readLe16(node, 42);
                            nr.freeSpaceOffset = readLe16(node, 44);
                            nr.freeSpaceLength = readLe16(node, 46);
                            nr.sampleHex = hexSampleBytes(node.data(), node.size() < 96 ? node.size() : 96);
                            if (nr.objectTypeLabel == "BTREE" || nr.objectTypeLabel == "BTREE_NODE") {
                                nr.status = "ROOT_TREE_NODE_HEADER_PARSED";
                                nr.interpretation = "APFS filesystem root-tree B-tree node header parsed from the resolved volume root-tree object.";
                            } else {
                                nr.status = "ROOT_TREE_NODE_UNEXPECTED_OBJECT_TYPE";
                                nr.interpretation = "Resolved root-tree object was readable but did not parse as an APFS B-tree node.";
                                apfsRootTreeNodeProbeRows.push_back(nr);
                                continue;
                            }
                            apfsRootTreeNodeProbeRows.push_back(nr);

                            const std::uint32_t limit = std::min<std::uint32_t>(nr.btnNkeys, kMaxRootTreeRecordSamplesPerVolume);
                            for (std::uint32_t i = 0; i < limit; ++i) {
                                std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                std::string detail;
                                ApfsRootTreeRecordSampleRow rr;
                                rr.sequence = static_cast<std::uint32_t>(apfsRootTreeRecordSampleRows.size());
                                rr.volumeSequence = lookup.volumeSequence;
                                rr.targetRole = lookup.targetRole;
                                rr.fsOid = lookup.fsOid;
                                rr.volumeName = lookup.volumeName;
                                rr.apfsRootTreeOid = lookup.apfsRootTreeOid;
                                rr.nodeOid = nr.nodeOid;
                                rr.nodeVirtualOffset = lookup.resolvedVirtualOffset;
                                rr.nodeLevel = nr.btnLevel;
                                rr.nodeNkeys = nr.btnNkeys;
                                rr.entryIndex = i;
                                if (!genericBtreeKvAbs(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) {
                                    rr.status = "ROOT_TREE_RECORD_TOC_DECODE_FAILED";
                                    rr.interpretation = "The root-tree node TOC entry could not be decoded safely.";
                                    rr.notes = detail;
                                    apfsRootTreeRecordSampleRows.push_back(rr);
                                    continue;
                                }
                                rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                                rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                                rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                                rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                                rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                                rr.keySampleHex = hexSampleBytes(node.data() + keyAbs, std::min<std::size_t>(keyLen, 64U));
                                if (valLen > 0 && valAbs < node.size()) rr.valueSampleHex = hexSampleBytes(node.data() + valAbs, std::min<std::size_t>(valLen, 64U));
                                if (valLen >= 8U && valAbs + 8U <= node.size()) rr.valueU64_0 = readLe64(node, valAbs);
                                if (valLen >= 16U && valAbs + 16U <= node.size()) rr.valueU64_1 = readLe64(node, valAbs + 8U);
                                if (valLen >= 24U && valAbs + 24U <= node.size()) rr.valueU64_2 = readLe64(node, valAbs + 16U);
                                if (keyLen >= 8U) {
                                    rr.keyRaw = readLe64(node, keyAbs);
                                    rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                                    rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                                    rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                                }
                                if (nr.btnLevel > 0 && valLen >= 8U) rr.branchChildOid = readLe64(node, valAbs);
                            if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                                    const std::uint32_t nameLenAndHash = readLe32(node, keyAbs + 8U);
                                    const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                                    rr.decodedName = safePrintableUtf8Fragment(node, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                                }
                                rr.status = "ROOT_TREE_RECORD_SAMPLE_DECODED";
                                rr.interpretation = nr.btnLevel > 0 ? "Root-tree branch/root record sampled; branch_child_oid should be resolved through the same volume OMAP in the next traversal step." : "Root-tree leaf record sampled; generic APFS filesystem key object/type fields were decoded where possible.";
                                rr.notes = detail;

                                apfsRootTreeRecordSampleRows.push_back(rr);
                            }
                        }

                        constexpr std::uint32_t kMaxRootTreeChildNodes = 96;
                        constexpr std::uint32_t kMaxChildRecordSamplesPerNode = 16;
                        std::set<std::string> seenChildTargets;
                        for (const auto& parentRecord : apfsRootTreeRecordSampleRows) {
                            if (parentRecord.branchChildOid == 0) continue;
                            if (apfsRootTreeChildNodeProbeRows.size() >= kMaxRootTreeChildNodes) break;
                            const std::string childKey = std::to_string(parentRecord.volumeSequence) + ":" + std::to_string(parentRecord.branchChildOid);
                            if (!seenChildTargets.insert(childKey).second) continue;

                            ApfsRootTreeChildNodeProbeRow cr;
                            cr.sequence = static_cast<std::uint32_t>(apfsRootTreeChildNodeProbeRows.size());
                            cr.sourceRecordSequence = parentRecord.sequence;
                            cr.volumeSequence = parentRecord.volumeSequence;
                            cr.targetRole = parentRecord.targetRole;
                            cr.fsOid = parentRecord.fsOid;
                            cr.volumeName = parentRecord.volumeName;
                            cr.apfsRootTreeOid = parentRecord.apfsRootTreeOid;
                            cr.parentNodeOid = parentRecord.nodeOid;
                            cr.parentNodeVirtualOffset = parentRecord.nodeVirtualOffset;
                            cr.parentNodeLevel = parentRecord.nodeLevel;
                            cr.parentEntryIndex = parentRecord.entryIndex;
                            cr.branchChildOid = parentRecord.branchChildOid;

                            std::uint64_t targetXid = 0;
                            for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                                if (lookup.volumeSequence == parentRecord.volumeSequence && lookup.apfsRootTreeOid == parentRecord.apfsRootTreeOid) {
                                    targetXid = lookup.targetXid;
                                    break;
                                }
                            }
                            cr.targetXid = targetXid;
                            auto omIt = apfsVolumeOmapRowsByVolumeSequence.find(parentRecord.volumeSequence);
                            auto rootIt = apfsVolumeOmapRootNodesByVolumeSequence.find(parentRecord.volumeSequence);
                            if (targetXid == 0) {
                                cr.lookupStatus = "CHILD_TARGET_XID_UNAVAILABLE";
                                cr.childNodeStatus = "CHILD_NODE_NOT_READ";
                                cr.interpretation = "Parent branch-child OID was sampled, but the volume target transaction ID was unavailable.";
                                apfsRootTreeChildNodeProbeRows.push_back(cr);
                                continue;
                            }
                            if (omIt == apfsVolumeOmapRowsByVolumeSequence.end() || rootIt == apfsVolumeOmapRootNodesByVolumeSequence.end()) {
                                cr.lookupStatus = "CHILD_VOLUME_OMAP_UNAVAILABLE";
                                cr.childNodeStatus = "CHILD_NODE_NOT_READ";
                                cr.interpretation = "Parent branch-child OID was sampled, but the matching volume OMAP root buffer was unavailable.";
                                apfsRootTreeChildNodeProbeRows.push_back(cr);
                                continue;
                            }

                            ApfsOmapTargetResolution resolved = resolveVolumeOmapTargetObject(omIt->second, rootIt->second, parentRecord.branchChildOid, targetXid, "APFS filesystem root-tree child-node lookup");
                            cr.omapBranchDepth = resolved.branchDepth;
                            cr.omapBranchPath = resolved.branchPath;
                            cr.omapLeafOid = resolved.leafOid;
                            cr.omapLeafVirtualOffset = resolved.leafVirtualOffset;
                            cr.omapLeafBytesRead = resolved.leafBytesRead;
                            cr.matchedEntryIndex = resolved.matchedEntryIndex;
                            cr.matchedKeyOid = resolved.matchedKeyOid;
                            cr.matchedKeyXid = resolved.matchedKeyXid;
                            cr.valueFlags = resolved.valueFlags;
                            cr.valueSize = resolved.valueSize;
                            cr.valuePaddr = resolved.valuePaddr;
                            cr.resolvedVirtualOffset = resolved.resolvedVirtualOffset;
                            cr.resolvedBytesRead = resolved.resolvedBytesRead;
                            cr.resolvedObjectOid = resolved.resolvedObjectOid;
                            cr.resolvedObjectXid = resolved.resolvedObjectXid;
                            cr.resolvedObjectTypeRaw = resolved.resolvedObjectTypeRaw;
                            cr.resolvedObjectTypeLabel = resolved.resolvedObjectTypeLabel;
                            cr.resolvedObjectSubtype = resolved.resolvedObjectSubtype;
                            cr.resolvedMagic = resolved.resolvedMagic;
                            cr.resolvedBtnFlags = resolved.resolvedBtnFlags;
                            cr.resolvedBtnLevel = resolved.resolvedBtnLevel;
                            cr.resolvedBtnNkeys = resolved.resolvedBtnNkeys;
                            cr.lookupStatus = resolved.lookupStatus;
                            cr.sampleHex = resolved.sampleHex;
                            cr.resolvedSampleHex = resolved.resolvedSampleHex;
                            cr.notes = resolved.notes;
                            if (resolved.objectStatus == "OMAP_TARGET_BTREE_READ") {
                                cr.childNodeStatus = "CHILD_NODE_BTREE_READ";
                                cr.interpretation = "Branch-child OID resolved through the volume OMAP and was read as an APFS filesystem B-tree child node.";
                            } else {
                                cr.childNodeStatus = resolved.objectStatus.empty() ? "CHILD_NODE_NOT_READ" : resolved.objectStatus;
                                cr.interpretation = resolved.interpretation;
                                apfsRootTreeChildNodeProbeRows.push_back(cr);
                                continue;
                            }
                            apfsRootTreeChildNodeProbeRows.push_back(cr);

                            const std::vector<unsigned char>& childNode = resolved.resolvedBuffer;
                            const std::uint32_t childLimit = std::min<std::uint32_t>(resolved.resolvedBtnNkeys, kMaxChildRecordSamplesPerNode);
                            for (std::uint32_t i = 0; i < childLimit; ++i) {
                                std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                std::string detail;
                                ApfsRootTreeRecordSampleRow rr;
                                rr.sequence = static_cast<std::uint32_t>(apfsRootTreeChildRecordSampleRows.size());
                                rr.volumeSequence = parentRecord.volumeSequence;
                                rr.targetRole = parentRecord.targetRole;
                                rr.fsOid = parentRecord.fsOid;
                                rr.volumeName = parentRecord.volumeName;
                                rr.apfsRootTreeOid = parentRecord.apfsRootTreeOid;
                                rr.nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : parentRecord.branchChildOid;
                                rr.nodeVirtualOffset = resolved.resolvedVirtualOffset;
                                rr.nodeLevel = resolved.resolvedBtnLevel;
                                rr.nodeNkeys = resolved.resolvedBtnNkeys;
                                rr.entryIndex = i;
                                if (!genericBtreeKvAbs(childNode, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) {
                                    rr.status = "CHILD_RECORD_TOC_DECODE_FAILED";
                                    rr.interpretation = "The child-node TOC entry could not be decoded safely.";
                                    rr.notes = detail;
                                    apfsRootTreeChildRecordSampleRows.push_back(rr);
                                    continue;
                                }
                                rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                                rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                                rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                                rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                                rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                                rr.keySampleHex = hexSampleBytes(childNode.data() + keyAbs, std::min<std::size_t>(keyLen, 64U));
                                if (valLen > 0 && valAbs < childNode.size()) rr.valueSampleHex = hexSampleBytes(childNode.data() + valAbs, std::min<std::size_t>(valLen, 64U));
                                if (valLen >= 8U && valAbs + 8U <= childNode.size()) rr.valueU64_0 = readLe64(childNode, valAbs);
                                if (valLen >= 16U && valAbs + 16U <= childNode.size()) rr.valueU64_1 = readLe64(childNode, valAbs + 8U);
                                if (valLen >= 24U && valAbs + 24U <= childNode.size()) rr.valueU64_2 = readLe64(childNode, valAbs + 16U);
                                if (keyLen >= 8U) {
                                    rr.keyRaw = readLe64(childNode, keyAbs);
                                    rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                                    rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                                    rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                                }
                                if (resolved.resolvedBtnLevel > 0 && valLen >= 8U) rr.branchChildOid = readLe64(childNode, valAbs);
                                if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                                    const std::uint32_t nameLenAndHash = readLe32(childNode, keyAbs + 8U);
                                    const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                                    rr.decodedName = safePrintableUtf8Fragment(childNode, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                                }
                                rr.status = "CHILD_RECORD_SAMPLE_DECODED";
                                rr.interpretation = resolved.resolvedBtnLevel > 0 ? "Child branch record sampled; branch_child_oid may be used for another bounded traversal step." : "Child leaf record sampled; generic APFS filesystem key object/type fields were decoded where possible.";
                                rr.notes = detail + "; parent_record_sequence=" + std::to_string(parentRecord.sequence);
                                apfsRootTreeChildRecordSampleRows.push_back(rr);
                            }
                        }

                        constexpr std::uint32_t kMaxRootTreeDescendantNodes = 160;
                        constexpr std::uint32_t kMaxDescendantRecordSamplesPerNode = 16;
                        std::set<std::string> seenDescendantTargets;
                        for (const auto& parentRecord : apfsRootTreeChildRecordSampleRows) {
                            if (parentRecord.branchChildOid == 0) continue;
                            if (apfsRootTreeDescendantNodeProbeRows.size() >= kMaxRootTreeDescendantNodes) break;
                            const std::string descendantKey = std::to_string(parentRecord.volumeSequence) + ":" + std::to_string(parentRecord.branchChildOid);
                            if (!seenDescendantTargets.insert(descendantKey).second) continue;

                            ApfsRootTreeChildNodeProbeRow dr;
                            dr.sequence = static_cast<std::uint32_t>(apfsRootTreeDescendantNodeProbeRows.size());
                            dr.sourceRecordSequence = parentRecord.sequence;
                            dr.volumeSequence = parentRecord.volumeSequence;
                            dr.targetRole = parentRecord.targetRole;
                            dr.fsOid = parentRecord.fsOid;
                            dr.volumeName = parentRecord.volumeName;
                            dr.apfsRootTreeOid = parentRecord.apfsRootTreeOid;
                            dr.parentNodeOid = parentRecord.nodeOid;
                            dr.parentNodeVirtualOffset = parentRecord.nodeVirtualOffset;
                            dr.parentNodeLevel = parentRecord.nodeLevel;
                            dr.parentEntryIndex = parentRecord.entryIndex;
                            dr.branchChildOid = parentRecord.branchChildOid;

                            std::uint64_t targetXid = 0;
                            for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                                if (lookup.volumeSequence == parentRecord.volumeSequence && lookup.apfsRootTreeOid == parentRecord.apfsRootTreeOid) {
                                    targetXid = lookup.targetXid;
                                    break;
                                }
                            }
                            dr.targetXid = targetXid;
                            auto omIt = apfsVolumeOmapRowsByVolumeSequence.find(parentRecord.volumeSequence);
                            auto rootIt = apfsVolumeOmapRootNodesByVolumeSequence.find(parentRecord.volumeSequence);
                            if (targetXid == 0) {
                                dr.lookupStatus = "DESCENDANT_TARGET_XID_UNAVAILABLE";
                                dr.childNodeStatus = "DESCENDANT_NODE_NOT_READ";
                                dr.interpretation = "Descendant branch-child OID was sampled, but the volume target transaction ID was unavailable.";
                                apfsRootTreeDescendantNodeProbeRows.push_back(dr);
                                continue;
                            }
                            if (omIt == apfsVolumeOmapRowsByVolumeSequence.end() || rootIt == apfsVolumeOmapRootNodesByVolumeSequence.end()) {
                                dr.lookupStatus = "DESCENDANT_VOLUME_OMAP_UNAVAILABLE";
                                dr.childNodeStatus = "DESCENDANT_NODE_NOT_READ";
                                dr.interpretation = "Descendant branch-child OID was sampled, but the matching volume OMAP root buffer was unavailable.";
                                apfsRootTreeDescendantNodeProbeRows.push_back(dr);
                                continue;
                            }

                            ApfsOmapTargetResolution resolved = resolveVolumeOmapTargetObject(omIt->second, rootIt->second, parentRecord.branchChildOid, targetXid, "APFS filesystem root-tree descendant-node lookup");
                            dr.omapBranchDepth = resolved.branchDepth;
                            dr.omapBranchPath = resolved.branchPath;
                            dr.omapLeafOid = resolved.leafOid;
                            dr.omapLeafVirtualOffset = resolved.leafVirtualOffset;
                            dr.omapLeafBytesRead = resolved.leafBytesRead;
                            dr.matchedEntryIndex = resolved.matchedEntryIndex;
                            dr.matchedKeyOid = resolved.matchedKeyOid;
                            dr.matchedKeyXid = resolved.matchedKeyXid;
                            dr.valueFlags = resolved.valueFlags;
                            dr.valueSize = resolved.valueSize;
                            dr.valuePaddr = resolved.valuePaddr;
                            dr.resolvedVirtualOffset = resolved.resolvedVirtualOffset;
                            dr.resolvedBytesRead = resolved.resolvedBytesRead;
                            dr.resolvedObjectOid = resolved.resolvedObjectOid;
                            dr.resolvedObjectXid = resolved.resolvedObjectXid;
                            dr.resolvedObjectTypeRaw = resolved.resolvedObjectTypeRaw;
                            dr.resolvedObjectTypeLabel = resolved.resolvedObjectTypeLabel;
                            dr.resolvedObjectSubtype = resolved.resolvedObjectSubtype;
                            dr.resolvedMagic = resolved.resolvedMagic;
                            dr.resolvedBtnFlags = resolved.resolvedBtnFlags;
                            dr.resolvedBtnLevel = resolved.resolvedBtnLevel;
                            dr.resolvedBtnNkeys = resolved.resolvedBtnNkeys;
                            dr.lookupStatus = resolved.lookupStatus;
                            dr.sampleHex = resolved.sampleHex;
                            dr.resolvedSampleHex = resolved.resolvedSampleHex;
                            dr.notes = resolved.notes;
                            if (resolved.objectStatus == "OMAP_TARGET_BTREE_READ") {
                                dr.childNodeStatus = "DESCENDANT_NODE_BTREE_READ";
                                dr.interpretation = "Second-level branch-child OID resolved through the volume OMAP and was read as an APFS filesystem B-tree descendant node.";
                            } else {
                                dr.childNodeStatus = resolved.objectStatus.empty() ? "DESCENDANT_NODE_NOT_READ" : resolved.objectStatus;
                                dr.interpretation = resolved.interpretation;
                                apfsRootTreeDescendantNodeProbeRows.push_back(dr);
                                continue;
                            }
                            apfsRootTreeDescendantNodeProbeRows.push_back(dr);

                            const std::vector<unsigned char>& descendantNode = resolved.resolvedBuffer;
                            const std::uint32_t descendantLimit = std::min<std::uint32_t>(resolved.resolvedBtnNkeys, kMaxDescendantRecordSamplesPerNode);
                            for (std::uint32_t i = 0; i < descendantLimit; ++i) {
                                std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                std::string detail;
                                ApfsRootTreeRecordSampleRow rr;
                                rr.sequence = static_cast<std::uint32_t>(apfsRootTreeDescendantRecordSampleRows.size());
                                rr.volumeSequence = parentRecord.volumeSequence;
                                rr.targetRole = parentRecord.targetRole;
                                rr.fsOid = parentRecord.fsOid;
                                rr.volumeName = parentRecord.volumeName;
                                rr.apfsRootTreeOid = parentRecord.apfsRootTreeOid;
                                rr.nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : parentRecord.branchChildOid;
                                rr.nodeVirtualOffset = resolved.resolvedVirtualOffset;
                                rr.nodeLevel = resolved.resolvedBtnLevel;
                                rr.nodeNkeys = resolved.resolvedBtnNkeys;
                                rr.entryIndex = i;
                                if (!genericBtreeKvAbs(descendantNode, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) {
                                    rr.status = "DESCENDANT_RECORD_TOC_DECODE_FAILED";
                                    rr.interpretation = "The descendant-node TOC entry could not be decoded safely.";
                                    rr.notes = detail;
                                    apfsRootTreeDescendantRecordSampleRows.push_back(rr);
                                    continue;
                                }
                                rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                                rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                                rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                                rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                                rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                                rr.keySampleHex = hexSampleBytes(descendantNode.data() + keyAbs, std::min<std::size_t>(keyLen, 64U));
                                if (valLen > 0 && valAbs < descendantNode.size()) rr.valueSampleHex = hexSampleBytes(descendantNode.data() + valAbs, std::min<std::size_t>(valLen, 64U));
                                if (valLen >= 8U && valAbs + 8U <= descendantNode.size()) rr.valueU64_0 = readLe64(descendantNode, valAbs);
                                if (valLen >= 16U && valAbs + 16U <= descendantNode.size()) rr.valueU64_1 = readLe64(descendantNode, valAbs + 8U);
                                if (valLen >= 24U && valAbs + 24U <= descendantNode.size()) rr.valueU64_2 = readLe64(descendantNode, valAbs + 16U);
                                if (keyLen >= 8U) {
                                    rr.keyRaw = readLe64(descendantNode, keyAbs);
                                    rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                                    rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                                    rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                                }
                                if (resolved.resolvedBtnLevel > 0 && valLen >= 8U) rr.branchChildOid = readLe64(descendantNode, valAbs);
                                if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                                    const std::uint32_t nameLenAndHash = readLe32(descendantNode, keyAbs + 8U);
                                    const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                                    rr.decodedName = safePrintableUtf8Fragment(descendantNode, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                                }
                                rr.status = "DESCENDANT_RECORD_SAMPLE_DECODED";
                                rr.interpretation = resolved.resolvedBtnLevel > 0 ? "Descendant branch record sampled; branch_child_oid may be used for another bounded traversal step." : "Descendant leaf record sampled; generic APFS filesystem key object/type/value fields were decoded where possible.";
                                rr.notes = detail + "; parent_child_record_sequence=" + std::to_string(parentRecord.sequence);
                                apfsRootTreeDescendantRecordSampleRows.push_back(rr);
                            }
                        }
                    };
                    std::vector<unsigned char> buffer;
                    std::string err;
                    long long bytesRead = readVirtual(0, 4096, buffer, err);
                    if (bytesRead >= 0) {
                        const std::size_t sampleLen = buffer.size() < 64 ? buffer.size() : 64;
                        addRow("AFF4_read_offset_0", bytesRead > 0 ? "READ_OK" : "READ_ZERO", originalInput, finalObjectSize, 0, bytesRead, sampleLen ? hexSampleBytes(buffer.data(), sampleLen) : std::string{}, "First bounded read from AFF4 object; this is the no-full-export block-reader smoke test.");
                    } else {
                        addRow("AFF4_read_offset_0", "READ_FAILED", originalInput, finalObjectSize, 0, bytesRead, {}, err.empty() ? "AFF4_read returned a negative value." : err);
                    }
                    addReadRow("virtual_read_offset_0", 0, bytesRead, buffer, "Read first 4096 virtual bytes from AFF4 image object.", err.empty() ? "Offset 0 read through libaff4 C API." : err);

                    nxSummary = parseApfsNxSuperblock(buffer, 0, bytesRead);
                    if (nxSummary.found) {
                        addApfsRow("apfs_nx_superblock_parse", nxSummary.validationStatus, 0, bytesRead, "NXSB", nxSummary.validationStatus == "NXSB_PARSED" ? "HIGH_APFS_CONTAINER" : "APFS_CONTAINER_WITH_WARNINGS", "APFS container superblock fields parsed from virtual offset 0.", buffer.empty() ? std::string{} : hexSampleBytes(buffer.data(), buffer.size() < 64 ? buffer.size() : 64), nxSummary.notes);

                        const bool blockSizeOk = nxSummary.blockSize >= 4096 && nxSummary.blockSize <= 65536 && ((nxSummary.blockSize & (nxSummary.blockSize - 1U)) == 0U);
                        const std::uint32_t rawDescCount = nxSummary.xpDescLen ? nxSummary.xpDescLen : nxSummary.xpDescBlocks;
                        const std::uint32_t descBlocksToScan = std::min<std::uint32_t>(rawDescCount, 128U);
                        if (!blockSizeOk || nxSummary.xpDescBase == 0 || nxSummary.xpDescBlocks == 0 || descBlocksToScan == 0) {
                            ApfsCheckpointDescriptorRow dr;
                            dr.status = "CHECKPOINT_DESCRIPTOR_SCAN_SKIPPED";
                            dr.interpretation = "NXSB parsed, but checkpoint descriptor base/count/length was not suitable for bounded scanning.";
                            dr.notes = "block_size=" + std::to_string(nxSummary.blockSize) + "; xp_desc_base=" + std::to_string(nxSummary.xpDescBase) + "; xp_desc_blocks=" + std::to_string(nxSummary.xpDescBlocks) + "; xp_desc_len=" + std::to_string(nxSummary.xpDescLen);
                            apfsDescriptorRows.push_back(dr);
                        } else {
                            for (std::uint32_t seq = 0; seq < descBlocksToScan; ++seq) {
                                const std::uint64_t ringIndex = (static_cast<std::uint64_t>(nxSummary.xpDescIndex) + static_cast<std::uint64_t>(seq)) % static_cast<std::uint64_t>(nxSummary.xpDescBlocks);
                                const std::uint64_t physicalBlock = nxSummary.xpDescBase + ringIndex;
                                ApfsCheckpointDescriptorRow dr;
                                dr.sequence = seq;
                                dr.physicalBlock = physicalBlock;
                                if (physicalBlock > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                    dr.status = "OFFSET_OVERFLOW";
                                    dr.interpretation = "Checkpoint descriptor block offset overflowed uint64 safety bounds.";
                                    dr.notes = "physical_block=" + std::to_string(physicalBlock) + "; block_size=" + std::to_string(nxSummary.blockSize);
                                    apfsDescriptorRows.push_back(dr);
                                    continue;
                                }
                                dr.virtualOffset = physicalBlock * static_cast<std::uint64_t>(nxSummary.blockSize);
                                std::vector<unsigned char> desc;
                                std::string descErr;
                                dr.bytesRead = readVirtual(dr.virtualOffset, nxSummary.blockSize, desc, descErr);
                                dr.status = dr.bytesRead > 0 ? "READ_OK" : "READ_FAILED";
                                if (dr.bytesRead > 0 && desc.size() >= 32) {
                                    dr.oid = readLe64(desc, 8);
                                    dr.xid = readLe64(desc, 16);
                                    dr.objectTypeRaw = readLe32(desc, 24);
                                    dr.objectSubtype = readLe32(desc, 28);
                                    dr.objectTypeLabel = apfsObjectTypeLabel(dr.objectTypeRaw);
                                    if (desc.size() >= 36) {
                                        if (std::memcmp(desc.data() + 32, "NXSB", 4) == 0) dr.magic = "NXSB";
                                        else if (std::memcmp(desc.data() + 32, "APSB", 4) == 0) dr.magic = "APSB";
                                        else if (std::memcmp(desc.data() + 32, "OMAP", 4) == 0) dr.magic = "OMAP";
                                    }
                                    dr.sampleHex = hexSampleBytes(desc.data(), desc.size() < 64 ? desc.size() : 64);
                                    if (dr.magic == "NXSB") dr.interpretation = "Checkpoint descriptor contains an APFS container superblock.";
                                    else if (dr.magic == "APSB") dr.interpretation = "Checkpoint descriptor appears to contain an APFS volume superblock candidate.";
                                    else if (!dr.magic.empty()) dr.interpretation = "Checkpoint descriptor contains a recognized APFS magic value.";
                                    else dr.interpretation = "Checkpoint descriptor object header read; no NXSB/APSB magic at +32.";
                                    if (dr.objectTypeLabel == "CHECKPOINT_MAP" && desc.size() >= 40 && nxSummary.blockSize >= 4096) {
                                        const std::uint32_t cpmFlags = readLe32(desc, 32);
                                        const std::uint32_t cpmCount = readLe32(desc, 36);
                                        const std::uint32_t entriesToParse = std::min<std::uint32_t>(cpmCount, 128U);
                                        for (std::uint32_t entryIndex = 0; entryIndex < entriesToParse; ++entryIndex) {
                                            const std::size_t entryOff = 40U + static_cast<std::size_t>(entryIndex) * 40U;
                                            if (entryOff + 40U > desc.size()) break;
                                            ApfsCheckpointMapEntryRow mr;
                                            mr.sequence = seq;
                                            mr.entryIndex = entryIndex;
                                            mr.checkpointBlock = physicalBlock;
                                            mr.checkpointVirtualOffset = dr.virtualOffset;
                                            mr.checkpointBytesRead = dr.bytesRead;
                                            mr.checkpointFlags = cpmFlags;
                                            mr.checkpointCount = cpmCount;
                                            mr.cpmTypeRaw = readLe32(desc, entryOff + 0);
                                            mr.cpmSubtype = readLe32(desc, entryOff + 4);
                                            mr.cpmSize = readLe32(desc, entryOff + 8);
                                            mr.cpmFsOid = readLe64(desc, entryOff + 16);
                                            mr.cpmOid = readLe64(desc, entryOff + 24);
                                            mr.cpmPaddr = readLe64(desc, entryOff + 32);
                                            mr.cpmTypeLabel = apfsObjectTypeLabel(mr.cpmTypeRaw);
                                            if (mr.cpmOid == nxSummary.omapOid) mr.targetRole = "NX_OBJECT_MAP";
                                            else if (mr.cpmOid == nxSummary.spacemanOid) mr.targetRole = "NX_SPACEMAN";
                                            else if (mr.cpmOid == nxSummary.reaperOid) mr.targetRole = "NX_REAPER";
                                            else if (containsU64(nxSummary.fsOids, mr.cpmOid)) mr.targetRole = "NX_FILESYSTEM_OID";
                                            else if (containsU64(nxSummary.fsOids, mr.cpmFsOid)) mr.targetRole = "VOLUME_SCOPED_OBJECT";
                                            else mr.targetRole = "CHECKPOINT_MAPPED_OBJECT";
                                            mr.interpretation = "APFS checkpoint-map entry parsed. cpm_paddr is treated as the candidate physical APFS block for bounded probing.";
                                            if (cpmCount > entriesToParse) mr.notes = "checkpoint_map_count_truncated_to_128";
                                            apfsCheckpointMapRows.push_back(mr);

                                            if (mr.cpmPaddr != 0 && mr.cpmPaddr <= nxSummary.blockCount && mr.cpmPaddr <= (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                                ApfsCheckpointMappedObjectProbeRow pr;
                                                pr.sequence = seq;
                                                pr.entryIndex = entryIndex;
                                                pr.cpmOid = mr.cpmOid;
                                                pr.cpmFsOid = mr.cpmFsOid;
                                                pr.cpmPaddr = mr.cpmPaddr;
                                                pr.targetRole = mr.targetRole;
                                                pr.virtualOffset = mr.cpmPaddr * static_cast<std::uint64_t>(nxSummary.blockSize);
                                                std::vector<unsigned char> mapped;
                                                std::string mappedErr;
                                                pr.bytesRead = readVirtual(pr.virtualOffset, nxSummary.blockSize, mapped, mappedErr);
                                                pr.status = pr.bytesRead > 0 ? "READ_OK" : "READ_FAILED";
                                                if (pr.bytesRead > 0 && mapped.size() >= 36) {
                                                    pr.mappedOid = readLe64(mapped, 8);
                                                    pr.mappedXid = readLe64(mapped, 16);
                                                    pr.mappedTypeRaw = readLe32(mapped, 24);
                                                    pr.mappedSubtype = readLe32(mapped, 28);
                                                    pr.mappedTypeLabel = apfsObjectTypeLabel(pr.mappedTypeRaw);
                                                    if (std::memcmp(mapped.data() + 32, "OMAP", 4) == 0) pr.magic = "OMAP";
                                                    else if (std::memcmp(mapped.data() + 32, "APSB", 4) == 0) pr.magic = "APSB";
                                                    else if (std::memcmp(mapped.data() + 32, "NXSB", 4) == 0) pr.magic = "NXSB";
                                                    else pr.magic.assign(reinterpret_cast<const char*>(mapped.data() + 32), reinterpret_cast<const char*>(mapped.data() + 36));
                                                    pr.sampleHex = hexSampleBytes(mapped.data(), mapped.size() < 64 ? mapped.size() : 64);
                                                    if (pr.magic == "OMAP") pr.interpretation = "Mapped checkpoint object appears to be an APFS object map. Next step is OMAP B-tree parsing.";
                                                    else if (pr.magic == "APSB") pr.interpretation = "Mapped checkpoint object appears to be an APFS volume superblock.";
                                                    else if (pr.magic == "NXSB") pr.interpretation = "Mapped checkpoint object appears to be an APFS container superblock.";
                                                    else pr.interpretation = "Mapped checkpoint object was read; no OMAP/APSB/NXSB magic at +32.";
                                                    addBtreeProbeFromBuffer(std::string("CHECKPOINT_MAPPED_") + pr.targetRole, pr.cpmOid, pr.cpmPaddr, pr.virtualOffset, pr.bytesRead, mapped, "Mapped through APFS checkpoint map cpm_paddr.");
                                                } else {
                                                    pr.interpretation = "Unable to read mapped checkpoint object through libaff4 virtual read.";
                                                    pr.notes = mappedErr.empty() ? "AFF4_read returned no bytes for mapped checkpoint object." : mappedErr;
                                                }
                                                apfsCheckpointMappedObjectRows.push_back(pr);
                                            }
                                        }
                                    }
                                } else {
                                    dr.interpretation = "Unable to read checkpoint descriptor block through libaff4 virtual read.";
                                    dr.notes = descErr.empty() ? "AFF4_read returned no bytes for checkpoint descriptor block." : descErr;
                                }
                                apfsDescriptorRows.push_back(dr);
                            }
                        }

                        const std::size_t volumeProbeLimit = std::min<std::size_t>(nxSummary.fsOids.size(), 32U);
                        for (std::size_t i = 0; i < volumeProbeLimit; ++i) {
                            const std::uint64_t fsOid = nxSummary.fsOids[i];
                            ApfsVolumeSuperblockRow vr;
                            vr.sequence = static_cast<std::uint32_t>(i);
                            vr.fsOid = fsOid;
                            if (nxSummary.blockSize == 0 || fsOid > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                vr.status = "OFFSET_OVERFLOW";
                                vr.interpretation = "Candidate APFS filesystem object ID could not be converted to a bounded virtual block offset.";
                                vr.notes = "fs_oid=" + std::to_string(fsOid) + "; block_size=" + std::to_string(nxSummary.blockSize);
                                apfsVolumeRows.push_back(vr);
                                continue;
                            }
                            vr.virtualOffset = fsOid * static_cast<std::uint64_t>(nxSummary.blockSize);
                            std::vector<unsigned char> vol;
                            std::string volErr;
                            vr.bytesRead = readVirtual(vr.virtualOffset, nxSummary.blockSize, vol, volErr);
                            vr.status = vr.bytesRead > 0 ? "READ_OK" : "READ_FAILED";
                            if (vr.bytesRead > 0 && vol.size() >= 64) {
                                vr.oid = readLe64(vol, 8);
                                vr.xid = readLe64(vol, 16);
                                vr.objectTypeRaw = readLe32(vol, 24);
                                vr.objectSubtype = readLe32(vol, 28);
                                vr.objectTypeLabel = apfsObjectTypeLabel(vr.objectTypeRaw);
                                if (vol.size() >= 36 && std::memcmp(vol.data() + 32, "APSB", 4) == 0) {
                                    vr.magic = "APSB";
                                    vr.status = "APSB_FOUND";
                                    vr.interpretation = "APFS volume superblock candidate found at fs_oid * nx_block_size.";
                                    vr.fsIndexCandidate = readLe32(vol, 36);
                                    vr.featuresCandidate = readLe64(vol, 40);
                                    vr.readonlyCompatibleFeaturesCandidate = readLe64(vol, 48);
                                    vr.incompatibleFeaturesCandidate = readLe64(vol, 56);
                                    if (vol.size() >= 80) {
                                        vr.unmountTimeCandidate = readLe64(vol, 64);
                                        vr.volumeUuidCandidate = bytesToUuidString(vol, 72);
                                    }
                                } else {
                                    if (vol.size() >= 36) vr.magic.assign(reinterpret_cast<const char*>(vol.data() + 32), reinterpret_cast<const char*>(vol.data() + 36));
                                    vr.interpretation = "Candidate filesystem OID block was read, but APSB magic was not present at +32.";
                                }
                                vr.sampleHex = hexSampleBytes(vol.data(), vol.size() < 64 ? vol.size() : 64);
                            } else {
                                vr.interpretation = "Unable to read candidate APFS volume superblock block through libaff4 virtual read.";
                                vr.notes = volErr.empty() ? "AFF4_read returned no bytes for candidate APFS filesystem OID block." : volErr;
                            }
                            apfsVolumeRows.push_back(vr);
                        }

                        probeApfsObjectId("NX_OMAP_OID", nxSummary.omapOid);
                        probeApfsObjectId("NX_SPACEMAN_OID", nxSummary.spacemanOid);
                        probeApfsObjectId("NX_REAPER_OID", nxSummary.reaperOid);
                        const std::size_t objectIdFsLimit = std::min<std::size_t>(nxSummary.fsOids.size(), 32U);
                        for (std::size_t i = 0; i < objectIdFsLimit; ++i) {
                            probeApfsObjectId(std::string("NX_FS_OID_") + std::to_string(i), nxSummary.fsOids[i]);
                        }
                        buildResolvedVolumeSuperblocksAndVolumeOmapProbes();
                    } else {
                        addApfsRow("apfs_nx_superblock_parse", nxSummary.validationStatus.empty() ? "NXSB_NOT_FOUND" : nxSummary.validationStatus, 0, bytesRead, {}, "NO_NXSB", "APFS container superblock was not parsed from virtual offset 0.", buffer.empty() ? std::string{} : hexSampleBytes(buffer.data(), buffer.size() < 64 ? buffer.size() : 64), nxSummary.notes);
                    }

                    err.clear();
                    std::vector<unsigned char> gpt;
                    long long gptRead = readVirtual(512, 512, gpt, err);
                    if (gptRead >= 8 && std::memcmp(gpt.data(), "EFI PART", 8) == 0) {
                        gptFound = true;
                        addApfsRow("gpt_header_lba1", "GPT_HEADER_FOUND", 512, gptRead, "EFI PART", "HIGH_ALIGNED", "GPT header found at virtual LBA 1.", hexSampleBytes(gpt.data(), gpt.size() < 64 ? gpt.size() : 64), "Next step is parsing GPT partition entries to locate APFS partition type GUIDs.");
                        const std::uint64_t partitionEntryLba = readLe64(gpt, 72);
                        const std::uint32_t partitionEntryCountReported = readLe32(gpt, 80);
                        const std::uint32_t partitionEntrySize = readLe32(gpt, 84);
                        const std::uint64_t tableOffset = partitionEntryLba * 512ULL;
                        const std::uint32_t entriesToRead = partitionEntryCountReported > 128U ? 128U : partitionEntryCountReported;
                        const std::uint64_t tableBytes64 = static_cast<std::uint64_t>(entriesToRead) * static_cast<std::uint64_t>(partitionEntrySize);
                        if (partitionEntryLba == 0 || partitionEntrySize < 56 || partitionEntrySize > 4096 || tableBytes64 == 0 || tableBytes64 > 1024ULL * 1024ULL) {
                            addApfsRow("gpt_partition_table", "GPT_TABLE_UNSANE", tableOffset, -1, {}, "LOW", "GPT header was found but partition table location/count/entry size was not in the bounded probe safety range.", {}, "Reported entries=" + std::to_string(partitionEntryCountReported) + "; entry_size=" + std::to_string(partitionEntrySize) + "; entry_lba=" + std::to_string(partitionEntryLba));
                        } else {
                            std::vector<unsigned char> entries;
                            err.clear();
                            long long tableRead = readVirtual(tableOffset, static_cast<std::size_t>(tableBytes64), entries, err);
                            addApfsRow("gpt_partition_table", tableRead > 0 ? "GPT_TABLE_READ" : "GPT_TABLE_READ_FAILED", tableOffset, tableRead, {}, tableRead > 0 ? "READ_PERFORMED" : "NO_DATA", "Bounded GPT partition-entry table read.", tableRead > 0 ? hexSampleBytes(entries.data(), entries.size() < 64 ? entries.size() : 64) : std::string{}, "Reported entries=" + std::to_string(partitionEntryCountReported) + "; read entries=" + std::to_string(entriesToRead) + "; entry_size=" + std::to_string(partitionEntrySize));
                            auto le32At = [&](std::size_t off) -> std::uint32_t { return readLe32(entries, off); };
                            auto le64At = [&](std::size_t off) -> std::uint64_t { return readLe64(entries, off); };
                            auto guidAt = [&](std::size_t off) -> std::string {
                                if (off + 16 > entries.size()) return {};
                                auto hx = [](unsigned char b) -> std::string { return std::string(1, "0123456789ABCDEF"[(b >> 4) & 0xF]) + std::string(1, "0123456789ABCDEF"[b & 0xF]); };
                                std::string g;
                                g += hx(entries[off + 3]); g += hx(entries[off + 2]); g += hx(entries[off + 1]); g += hx(entries[off + 0]); g += '-';
                                g += hx(entries[off + 5]); g += hx(entries[off + 4]); g += '-';
                                g += hx(entries[off + 7]); g += hx(entries[off + 6]); g += '-';
                                g += hx(entries[off + 8]); g += hx(entries[off + 9]); g += '-';
                                for (int i = 10; i < 16; ++i) g += hx(entries[off + static_cast<std::size_t>(i)]);
                                return g;
                            };
                            const std::string apfsGuid = "7C3457EF-0000-11AA-AA11-00306543ECAC";
                            if (tableRead > 0) {
                                for (std::uint32_t i = 0; i < entriesToRead; ++i) {
                                    const std::size_t base = static_cast<std::size_t>(i) * static_cast<std::size_t>(partitionEntrySize);
                                    if (base + 56 > entries.size()) break;
                                    bool typeZero = true;
                                    for (std::size_t j = 0; j < 16; ++j) if (entries[base + j] != 0) { typeZero = false; break; }
                                    if (typeZero) continue;
                                    ++partitionCount;
                                    const std::string guid = guidAt(base);
                                    const std::uint64_t firstLba = le64At(base + 32);
                                    const std::uint64_t lastLba = le64At(base + 40);
                                    const bool isApfs = (guid == apfsGuid);
                                    if (isApfs) ++apfsPartitionCount;
                                    const std::uint64_t partOffset = firstLba * 512ULL;
                                    addApfsRow("gpt_partition_entry", isApfs ? "APFS_PARTITION_CANDIDATE" : "PARTITION_ENTRY", partOffset, 0, guid, isApfs ? "HIGH_APFS_GUID" : "GUID_REPORTED", isApfs ? "APFS GPT partition type GUID found." : "Non-empty GPT partition entry found.", {}, "index=" + std::to_string(i) + "; first_lba=" + std::to_string(firstLba) + "; last_lba=" + std::to_string(lastLba));
                                    if (isApfs && apfsPartitionCount <= 8) {
                                        std::vector<unsigned char> sb;
                                        err.clear();
                                        long long sbRead = readVirtual(partOffset, 4096, sb, err);
                                        bool nxsb = (sbRead >= 36 && sb.size() >= 36 && std::memcmp(sb.data() + 32, "NXSB", 4) == 0);
                                        if (nxsb) ++nxsbHitCount;
                                        addApfsRow("apfs_container_superblock_probe", nxsb ? "NXSB_FOUND" : (sbRead > 0 ? "NXSB_NOT_AT_PLUS_32" : "READ_FAILED"), partOffset, sbRead, nxsb ? "NXSB" : std::string{}, nxsb ? "HIGH_APFS_ALIGNED" : "NO_NXSB_AT_EXPECTED_OFFSET", nxsb ? "APFS container superblock magic found at partition offset + 32." : "Read APFS candidate partition start but did not observe NXSB at +32.", sbRead > 0 ? hexSampleBytes(sb.data(), sb.size() < 64 ? sb.size() : 64) : std::string{}, err.empty() ? "APFS candidate partition start probed through libaff4 virtual read." : err);
                                    }
                                }
                            }
                        }
                    } else {
                        addApfsRow("gpt_header_lba1", gptRead > 0 ? "GPT_HEADER_NOT_FOUND" : "READ_FAILED", 512, gptRead, gptRead >= 8 ? std::string(reinterpret_cast<const char*>(gpt.data()), reinterpret_cast<const char*>(gpt.data()) + 8) : std::string{}, gptRead > 0 ? "NO_EFI_PART" : "NO_DATA", "No GPT header was observed at virtual LBA 1 in this bounded probe.", gptRead > 0 ? hexSampleBytes(gpt.data(), gpt.size() < 64 ? gpt.size() : 64) : std::string{}, err.empty() ? "If this AFF4 object is a volume rather than whole disk image, APFS may begin at object offset 0." : err);
                        if (bytesRead >= 36 && buffer.size() >= 36 && std::memcmp(buffer.data() + 32, "NXSB", 4) == 0) {
                            ++nxsbHitCount;
                            addApfsRow("apfs_container_superblock_offset_0", "NXSB_FOUND", 0, bytesRead, "NXSB", "HIGH_APFS_ALIGNED", "APFS container superblock magic found at object offset 0 + 32.", hexSampleBytes(buffer.data(), buffer.size() < 64 ? buffer.size() : 64), "This suggests the AFF4 object may expose an APFS container/volume rather than a whole physical disk.");
                        }
                    }

                    auto safeSpotlightNodeOffset = [&](std::uint64_t oid, std::uint64_t& offsetOut) -> bool {
                        if (nxSummary.blockSize == 0) return false;
                        if (oid > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) return false;
                        offsetOut = oid * static_cast<std::uint64_t>(nxSummary.blockSize);
                        if (finalObjectSize != 0 && offsetOut >= finalObjectSize) return false;
                        return true;
                    };

                    auto isSpotlightStoreV2TopLevelComponentName = [&](const std::string& lname) -> bool {
                        if (lname == ".store.db" || lname == "store.db" || lname == "store.db-wal" || lname == "store.db-shm") return true;
                        if (lname == "0.directorystorefile" || lname == "0.directorystorefile.shadow") return true;
                        if (lname.rfind("0.index", 0) == 0 || lname.rfind("0.shadowindex", 0) == 0) return true;
                        if (lname.rfind("live.", 0) == 0) return true;
                        if (lname == "reversestore.updates" || lname == "store.updates" || lname == "store_generation") return true;
                        if (lname == "reversedirectorystore" || lname == "reversedirectorystore.shadow") return true;
                        if (lname == "permstore" || lname == "journalexclusion" || lname == "journals.migration_secondchance") return true;
                        if (lname == "cab.created" || lname == "cab.modified" || lname == "lion.created" || lname == "lion.modified" || lname == "star.created" || lname == "star.modified") return true;
                        if (lname == "tmp.cab" || lname == "tmp.lion" || lname == "tmp.star") return true;
                        if (lname.rfind("dbstr-", 0) == 0 || lname.rfind("dbhdr-", 0) == 0) return true;
                        if (lname.rfind("tmp.spotlight", 0) == 0) return true;
                        return false;
                    };

                    auto isSpotlightTargetName = [&](const std::string& name) -> bool {
                        const std::string lname = asciiLower(name);
                        return lname == ".spotlight-v100" || lname == "store-v2" || lname == "index.db" ||
                               isSpotlightStoreV2TopLevelComponentName(lname) ||
                               lname.find("spotlight") != std::string::npos || lname.find("corespotlight") != std::string::npos;
                    };

                    auto spotlightTargetKind = [&](const std::string& name) -> std::string {
                        const std::string lname = asciiLower(name);
                        if (lname == ".spotlight-v100") return "SPOTLIGHT_ROOT_DIRECTORY";
                        if (lname == "store-v2") return "SPOTLIGHT_STORE_V2_DIRECTORY";
                        if (lname == "store.db" || lname == ".store.db") return "SPOTLIGHT_STORE_DB_FILE";
                        if (lname == "store.db-wal" || lname == "store.db-shm") return "SPOTLIGHT_STORE_SQLITE_SUPPORT_FILE";
                        if (lname.rfind("dbstr-", 0) == 0) return "SPOTLIGHT_DBSTR_FILE";
                        if (lname.rfind("dbhdr-", 0) == 0) return "SPOTLIGHT_DBHDR_FILE";
                        if (lname == "0.directorystorefile" || lname == "0.directorystorefile.shadow") return "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
                        if (lname.rfind("0.index", 0) == 0 || lname.rfind("0.shadowindex", 0) == 0) return "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
                        if (lname.rfind("live.", 0) == 0 || lname == "permstore" || lname == "journalexclusion" || lname == "journals.migration_secondchance" || lname == "reversestore.updates" || lname == "store.updates" || lname == "store_generation" || lname == "reversedirectorystore" || lname == "reversedirectorystore.shadow") return "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
                        if (lname == "cab.created" || lname == "cab.modified" || lname == "lion.created" || lname == "lion.modified" || lname == "star.created" || lname == "star.modified" || lname == "tmp.cab" || lname == "tmp.lion" || lname == "tmp.star") return "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
                        if (lname.rfind("tmp.spotlight", 0) == 0) return "TMP_SPOTLIGHT_COMPONENT";
                        if (lname.find("corespotlight") != std::string::npos) return "IOS_CORESPOTLIGHT_NAME";
                        if (lname == "index.db") return "IOS_CORESPOTLIGHT_INDEX_DB_FILE";
                        return "SPOTLIGHT_RELATED_NAME";
                    };

                    std::vector<ApfsSpotlightFileExtentProbeRow> allSpotlightScanFileExtents;
                    std::vector<ApfsSpotlightInodeProbeRow> allSpotlightScanInodes;

                    auto previewStatusForBytes = [&](const std::vector<unsigned char>& bytes) -> std::string {
                        if (bytes.size() >= 16 && std::memcmp(bytes.data(), "SQLite format 3", 15) == 0) return "PREVIEW_SQLITE_HEADER";
                        if (bytes.size() >= 4 && bytes[0] == 0x37 && bytes[1] == 0x7f && bytes[2] == 0x06 && bytes[3] == 0x82) return "PREVIEW_SQLITE_WAL_HEADER";
                        if (bytes.size() >= 4 && bytes[0] == 0x18 && bytes[1] == 0xe2 && bytes[2] == 0x2d && bytes[3] == 0x00) return "PREVIEW_SQLITE_SHM_HEADER";
                        if (!bytes.empty()) return "PREVIEW_READ_NON_SQLITE_SAMPLE";
                        return "NO_PREVIEW";
                    };

                    auto appendCopyAttempt = [&](const ApfsRootTreeRecordSampleRow& rr) {
                        ApfsSpotlightCopyAttemptRow cr;
                        cr.sequence = static_cast<std::uint32_t>(apfsSpotlightCopyAttemptRows.size());
                        cr.volumeSequence = rr.volumeSequence;
                        cr.targetRole = rr.targetRole;
                        cr.fsOid = rr.fsOid;
                        cr.volumeName = rr.volumeName;
                        cr.parentObjectId = rr.keyObjectId;
                        cr.childFileId = rr.valueU64_0;
                        cr.targetName = rr.decodedName;
                        cr.targetKind = spotlightTargetKind(rr.decodedName);
                        if (cr.childFileId == 0) {
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_NO_CHILD_FILE_ID";
                            cr.interpretation = "A Spotlight-related name was decoded, but the directory-record value did not provide a usable child file ID candidate.";
                        } else if (cr.targetKind == "SPOTLIGHT_ROOT_DIRECTORY" || cr.targetKind == "SPOTLIGHT_STORE_V2_DIRECTORY" || cr.targetKind == "IOS_CORESPOTLIGHT_NAME") {
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_DIRECTORY_RECURSION_NOT_READY";
                            cr.interpretation = "A Spotlight-related directory was found. The next extraction step must recursively resolve child paths and file extents before copying bytes.";
                        } else {
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED";
                            cr.interpretation = "A Spotlight-related file name was found. File-byte extraction is blocked until APFS file extent values are decoded and validated for this child file ID.";
                        }
                        cr.notes = "V0_8_62 records a gated copy decision; file-byte extraction requires target path, usable extents, duplicate-free logical extent assembly, and optional inode DSTREAM logical-size trimming.";
                        apfsSpotlightCopyAttemptRows.push_back(cr);
                    };

                    struct SpotlightScanPendingNode {
                        std::uint32_t volumeSequence = 0;
                        std::string targetRole;
                        std::uint64_t fsOid = 0;
                        std::string volumeName;
                        std::uint64_t apfsRootTreeOid = 0;
                        std::uint64_t parentNodeOid = 0;
                        std::uint64_t branchOid = 0;
                        std::uint64_t targetXid = 0;
                        std::uint32_t depth = 0;
                    };

                    std::vector<SpotlightScanPendingNode> spotlightQueue;
                    std::set<std::string> spotlightSeen;
                    auto enqueueSpotlightNode = [&](const ApfsRootTreeRecordSampleRow& r, std::uint32_t depth) {
                        if (r.branchChildOid == 0) return;
                        const std::string key = std::to_string(r.volumeSequence) + ":" + std::to_string(r.branchChildOid);
                        if (!spotlightSeen.insert(key).second) return;
                        SpotlightScanPendingNode pn;
                        pn.volumeSequence = r.volumeSequence;
                        pn.targetRole = r.targetRole;
                        pn.fsOid = r.fsOid;
                        pn.volumeName = r.volumeName;
                        pn.apfsRootTreeOid = r.apfsRootTreeOid;
                        pn.parentNodeOid = r.nodeOid;
                        pn.branchOid = r.branchChildOid;
                        pn.depth = depth;
                        for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                            if (lookup.volumeSequence == r.volumeSequence && lookup.apfsRootTreeOid == r.apfsRootTreeOid) {
                                pn.targetXid = lookup.targetXid;
                                break;
                            }
                        }
                        spotlightQueue.push_back(pn);
                    };
                    auto seedSpotlightQueueForVolume = [&](std::uint32_t volumeSequence) {
                        for (const auto& r : apfsRootTreeRecordSampleRows) if (r.volumeSequence == volumeSequence) enqueueSpotlightNode(r, 1);
                        for (const auto& r : apfsRootTreeChildRecordSampleRows) if (r.volumeSequence == volumeSequence) enqueueSpotlightNode(r, 2);
                        for (const auto& r : apfsRootTreeDescendantRecordSampleRows) if (r.volumeSequence == volumeSequence) enqueueSpotlightNode(r, 3);
                    };
                    // Prioritize the Data volume because macOS Spotlight stores are expected under the Data namespace.
                    seedSpotlightQueueForVolume(4);
                    for (const auto& lookup : apfsVolumeRootTreeLookupRows) if (lookup.volumeSequence != 4) seedSpotlightQueueForVolume(lookup.volumeSequence);

                    for (std::size_t qi = 0; qi < spotlightQueue.size(); ++qi) {
                        const SpotlightScanPendingNode pending = spotlightQueue[qi];
                        if (pending.branchOid == 0 || pending.targetXid == 0) continue;
                        auto omIt = apfsVolumeOmapRowsByVolumeSequence.find(pending.volumeSequence);
                        auto rootIt = apfsVolumeOmapRootNodesByVolumeSequence.find(pending.volumeSequence);
                        if (omIt == apfsVolumeOmapRowsByVolumeSequence.end() || rootIt == apfsVolumeOmapRootNodesByVolumeSequence.end()) continue;
                        ++apfsSpotlightTargetScanMetrics.nodesVisited;
                        ApfsOmapTargetResolution resolved = aff4ResolveVolumeOmapTargetObjectForProbe(omIt->second, rootIt->second, pending.branchOid, pending.targetXid, nxSummary.blockSize, readVirtual, safeSpotlightNodeOffset, "APFS Spotlight-target namespace scan");
                        if (resolved.objectStatus != "OMAP_TARGET_BTREE_READ") continue;
                        ++apfsSpotlightTargetScanMetrics.nodesResolved;
                        if (resolved.resolvedBtnLevel > 0) ++apfsSpotlightTargetScanMetrics.branchNodes;
                        else ++apfsSpotlightTargetScanMetrics.leafNodes;

                        const std::vector<unsigned char>& scanNode = resolved.resolvedBuffer;
                        const std::uint32_t limit = resolved.resolvedBtnNkeys;
                        for (std::uint32_t i = 0; i < limit; ++i) {
                            std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                            std::string detail;
                            if (!aff4GenericBtreeKvAbsForProbe(scanNode, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                            ++apfsSpotlightTargetScanMetrics.recordsScanned;
                            ApfsRootTreeRecordSampleRow rr;
                            rr.sequence = static_cast<std::uint32_t>(apfsSpotlightTargetScanRows.size());
                            rr.volumeSequence = pending.volumeSequence;
                            rr.targetRole = pending.targetRole;
                            rr.fsOid = pending.fsOid;
                            rr.volumeName = pending.volumeName;
                            rr.apfsRootTreeOid = pending.apfsRootTreeOid;
                            rr.nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : pending.branchOid;
                            rr.nodeVirtualOffset = resolved.resolvedVirtualOffset;
                            rr.nodeLevel = resolved.resolvedBtnLevel;
                            rr.nodeNkeys = resolved.resolvedBtnNkeys;
                            rr.entryIndex = i;
                            rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                            rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                            rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                            rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                            rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                            rr.keySampleHex = hexSampleBytes(scanNode.data() + keyAbs, std::min<std::size_t>(keyLen, 64U));
                            if (valLen > 0 && valAbs < scanNode.size()) rr.valueSampleHex = hexSampleBytes(scanNode.data() + valAbs, std::min<std::size_t>(valLen, 64U));
                            if (valLen >= 8U && valAbs + 8U <= scanNode.size()) rr.valueU64_0 = readLe64(scanNode, valAbs);
                            if (valLen >= 16U && valAbs + 16U <= scanNode.size()) rr.valueU64_1 = readLe64(scanNode, valAbs + 8U);
                            if (valLen >= 24U && valAbs + 24U <= scanNode.size()) rr.valueU64_2 = readLe64(scanNode, valAbs + 16U);
                            if (keyLen >= 8U) {
                                rr.keyRaw = readLe64(scanNode, keyAbs);
                                rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                                    rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                                rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                            }
                            if (resolved.resolvedBtnLevel > 0 && valLen >= 8U) {
                                rr.branchChildOid = readLe64(scanNode, valAbs);
                                ++apfsSpotlightTargetScanMetrics.branchCandidatesQueued;
                                if (rr.branchChildOid != 0) enqueueSpotlightNode(rr, pending.depth + 1U);
                            }
                            if (rr.keyTypeRaw == 8U && keyLen >= 16U && valLen >= 16U) {
                                ApfsSpotlightFileExtentProbeRow er;
                                er.sequence = static_cast<std::uint32_t>(allSpotlightScanFileExtents.size());
                                er.volumeSequence = rr.volumeSequence;
                                er.targetRole = rr.targetRole;
                                er.fsOid = rr.fsOid;
                                er.volumeName = rr.volumeName;
                                er.extentFileId = rr.keyObjectId;
                                er.extentLogicalOffset = readLe64(scanNode, keyAbs + 8U);
                                er.lenAndFlags = readLe64(scanNode, valAbs + 0U);
                                er.extentLengthBytes = er.lenAndFlags & 0x00ffffffffffffffULL;
                                er.extentFlags = static_cast<std::uint32_t>((er.lenAndFlags >> 56U) & 0xffU);
                                er.physicalBlock = readLe64(scanNode, valAbs + 8U);
                                if (valLen >= 24U && valAbs + 24U <= scanNode.size()) er.cryptoId = readLe64(scanNode, valAbs + 16U);
                                if (nxSummary.blockSize != 0 && er.physicalBlock <= (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                    er.physicalOffset = er.physicalBlock * static_cast<std::uint64_t>(nxSummary.blockSize);
                                }
                                er.nodeOid = rr.nodeOid;
                                er.nodeVirtualOffset = rr.nodeVirtualOffset;
                                er.nodeLevel = rr.nodeLevel;
                                er.nodeNkeys = rr.nodeNkeys;
                                er.entryIndex = rr.entryIndex;
                                er.extentStatus = "FILE_EXTENT_CANDIDATE_SEEN";
                                er.interpretation = "APFS FILE_EXTENT record decoded during targeted Spotlight namespace scan; it will be matched to target child file IDs after the scan completes.";
                                er.notes = detail + "; scan_depth=" + std::to_string(pending.depth) + "; key_object_id_is_file_id";
                                allSpotlightScanFileExtents.push_back(er);
                            }
                            if (rr.keyTypeRaw == 3U && valLen >= 8U) {
                                ApfsSpotlightInodeProbeRow ir;
                                ir.sequence = static_cast<std::uint32_t>(allSpotlightScanInodes.size());
                                ir.volumeSequence = rr.volumeSequence;
                                ir.targetRole = rr.targetRole;
                                ir.fsOid = rr.fsOid;
                                ir.volumeName = rr.volumeName;
                                ir.inodeObjectId = rr.keyObjectId;
                                if (valLen >= 8U && valAbs + 8U <= scanNode.size()) ir.inodeParentId = readLe64(scanNode, valAbs + 0U);
                                if (valLen >= 16U && valAbs + 16U <= scanNode.size()) ir.inodePrivateId = readLe64(scanNode, valAbs + 8U);
                                if (valLen >= 24U && valAbs + 24U <= scanNode.size()) ir.inodeCreateTimeRaw = readLe64(scanNode, valAbs + 16U);
                                if (valLen >= 32U && valAbs + 32U <= scanNode.size()) ir.inodeModTimeRaw = readLe64(scanNode, valAbs + 24U);
                                if (valLen >= 40U && valAbs + 40U <= scanNode.size()) ir.inodeChangeTimeRaw = readLe64(scanNode, valAbs + 32U);
                                if (valLen >= 48U && valAbs + 48U <= scanNode.size()) ir.inodeAccessTimeRaw = readLe64(scanNode, valAbs + 40U);
                                if (valLen >= 56U && valAbs + 56U <= scanNode.size()) ir.inodeInternalFlags = readLe64(scanNode, valAbs + 48U);
                                if (valLen >= 60U && valAbs + 60U <= scanNode.size()) ir.inodeNchildrenOrNlink = readLe32(scanNode, valAbs + 56U);
                                if (valLen >= 84U && valAbs + 84U <= scanNode.size()) ir.inodeModeCandidate = readLe16(scanNode, valAbs + 82U);
                                if (valLen >= 92U && valAbs + 92U <= scanNode.size()) ir.inodeUncompressedSize = readLe64(scanNode, valAbs + 84U);
                                const ApfsInodeExtendedFieldDecode xf = decodeApfsInodeExtendedFieldsForProbe(scanNode, valAbs, valLen);
                                ir.inodeXfieldStatus = xf.status;
                                if (xf.sawDstream) {
                                    ir.inodeDstreamSize = xf.dstreamSize;
                                    ir.inodeDstreamAllocedSize = xf.dstreamAllocedSize;
                                    ir.inodeDstreamDefaultCryptoId = xf.dstreamDefaultCryptoId;
                                }
                                ir.nodeOid = rr.nodeOid;
                                ir.nodeVirtualOffset = rr.nodeVirtualOffset;
                                ir.nodeLevel = rr.nodeLevel;
                                ir.nodeNkeys = rr.nodeNkeys;
                                ir.entryIndex = rr.entryIndex;
                                ir.inodeStatus = "SCANNED_INODE_CANDIDATE";
                                ir.interpretation = "APFS INODE record decoded during filesystem-tree traversal and cached for later target-guided Store-V2 correlation.";
                                if (valLen > 0 && valAbs < scanNode.size()) {
                                    const std::size_t avail = std::min<std::size_t>(scanNode.size() - valAbs, valLen);
                                    ir.valueSampleHex = hexSampleBytes(scanNode.data() + valAbs, std::min<std::size_t>(avail, 96U));
                                }
                                ir.notes = detail + "; scan_depth=" + std::to_string(pending.depth) + "; cached_for_target_correlation=1";
                                if (!xf.notes.empty()) ir.notes += "; xfields=" + xf.notes;
                                allSpotlightScanInodes.push_back(std::move(ir));
                            }
                            if (rr.keyTypeRaw == 4U && keyLen >= 10U && valLen >= 4U) {
                                const std::uint16_t xnameLenRaw = readLe16(scanNode, keyAbs + 8U);
                                std::size_t xnameLen = static_cast<std::size_t>(xnameLenRaw);
                                if (xnameLen > keyLen - 10U) xnameLen = keyLen - 10U;
                                std::string xname = safePrintableUtf8Fragment(scanNode, keyAbs + 10U, xnameLen);
                                while (!xname.empty() && xname.back() == '\0') xname.pop_back();
                                ApfsSpotlightXattrProbeRow xr;
                                xr.sequence = static_cast<std::uint32_t>(apfsSpotlightXattrProbeRows.size());
                                xr.volumeSequence = rr.volumeSequence;
                                xr.targetRole = rr.targetRole;
                                xr.fsOid = rr.fsOid;
                                xr.volumeName = rr.volumeName;
                                xr.fileObjectId = rr.keyObjectId;
                                xr.xattrName = xname;
                                xr.xattrNameLength = xnameLenRaw;
                                xr.xattrFlags = readLe16(scanNode, valAbs + 0U);
                                xr.xdataLength = readLe16(scanNode, valAbs + 2U);
                                xr.xattrStorage = apfsXattrStorageLabel(xr.xattrFlags);
                                if ((xr.xattrFlags & 0x0001U) != 0 && valLen >= 12U && valAbs + 12U <= scanNode.size()) {
                                    // j_xattr_val.xdata contains j_xattr_dstream_t for stream-backed xattrs:
                                    // uint64_t xattr_obj_id; j_dstream_t dstream { size, alloced_size, default_crypto_id, ... }.
                                    xr.xdataStreamId = readLe64(scanNode, valAbs + 4U);
                                    if (valLen >= 20U && valAbs + 20U <= scanNode.size()) xr.xdataStreamSize = readLe64(scanNode, valAbs + 12U);
                                    if (valLen >= 28U && valAbs + 28U <= scanNode.size()) xr.xdataStreamAllocatedSize = readLe64(scanNode, valAbs + 20U);
                                    if (valLen >= 36U && valAbs + 36U <= scanNode.size()) xr.xdataStreamDefaultCryptoId = readLe64(scanNode, valAbs + 28U);
                                    xr.xdataPreviewStatus = (xr.xdataStreamSize != 0) ? "XATTR_DSTREAM_DECODED" : "XATTR_STREAM_ID_DECODED";
                                } else if ((xr.xattrFlags & 0x0002U) != 0 && valLen > 4U && valAbs + 4U <= scanNode.size()) {
                                    const std::size_t embeddedLen = std::min<std::size_t>(static_cast<std::size_t>(xr.xdataLength), std::min<std::size_t>(valLen - 4U, scanNode.size() - (valAbs + 4U)));
                                    std::vector<unsigned char> previewBytes;
                                    if (embeddedLen > 0) {
                                        const std::size_t previewLen = std::min<std::size_t>(embeddedLen, 96U);
                                        previewBytes.assign(scanNode.begin() + static_cast<std::ptrdiff_t>(valAbs + 4U), scanNode.begin() + static_cast<std::ptrdiff_t>(valAbs + 4U + previewLen));
                                        xr.xdataPreviewStatus = previewStatusForBytes(previewBytes);
                                        xr.xdataPreviewHex = hexSampleBytes(scanNode.data() + valAbs + 4U, previewLen);
                                    } else {
                                        xr.xdataPreviewStatus = "XATTR_EMBEDDED_EMPTY";
                                    }
                                } else {
                                    xr.xdataPreviewStatus = "NO_XATTR_DATA_PREVIEW";
                                }
                                xr.nodeOid = rr.nodeOid;
                                xr.nodeVirtualOffset = rr.nodeVirtualOffset;
                                xr.nodeLevel = rr.nodeLevel;
                                xr.nodeNkeys = rr.nodeNkeys;
                                xr.entryIndex = rr.entryIndex;
                                xr.xattrStatus = isApfsCompressionOrResourceXattrName(xname) ? "COMPRESSION_OR_RSRC_XATTR_SEEN" : "XATTR_SEEN";
                                xr.interpretation = "APFS XATTR record decoded during the bounded Store-V2 filesystem traversal. com.apple.decmpfs and com.apple.ResourceFork rows are used to triage compressed/resource-fork reconstruction needs.";
                                xr.notes = detail + "; scan_depth=" + std::to_string(pending.depth) + "; key_object_id_is_file_id";
                                apfsSpotlightXattrProbeRows.push_back(std::move(xr));
                            }
                            if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                                const std::uint32_t nameLenAndHash = readLe32(scanNode, keyAbs + 8U);
                                const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                                rr.decodedName = safePrintableUtf8Fragment(scanNode, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                                ++apfsSpotlightTargetScanMetrics.dirRecordsDecoded;
                                if (rr.valueU64_0 != 0 && !rr.decodedName.empty()) {
                                    ApfsDirectoryRecordEntry ns;
                                    ns.volumeSequence = rr.volumeSequence;
                                    ns.targetRole = rr.targetRole;
                                    ns.fsOid = rr.fsOid;
                                    ns.volumeName = rr.volumeName;
                                    ns.parentObjectId = rr.keyObjectId;
                                    ns.childFileId = rr.valueU64_0;
                                    ns.name = rr.decodedName;
                                    apfsDirectoryRecordEntries.push_back(std::move(ns));
                                }
                                {
                                    ApfsRootTreeRecordSampleRow sample = rr;
                                    sample.sequence = static_cast<std::uint32_t>(apfsSpotlightNameScanSampleRows.size());
                                    sample.status = "DECODED_DIRECTORY_NAME";
                                    sample.interpretation = "Decoded APFS directory-record name encountered during the Store-V2 filesystem-tree traversal.";
                                    sample.notes = detail + "; scan_depth=" + std::to_string(pending.depth) + "; parent_node_oid=" + std::to_string(pending.parentNodeOid);
                                    apfsSpotlightNameScanSampleRows.push_back(sample);
                                }
                                if (isSpotlightTargetName(rr.decodedName)) {
                                    ++apfsSpotlightTargetScanMetrics.targetNameHits;
                                    rr.status = "SPOTLIGHT_TARGET_NAME_HIT";
                                    rr.interpretation = "Targeted APFS namespace scan found a Spotlight/CoreSpotlight-related directory-record name. Copy-out remains gated on path and file-extent resolution.";
                                    rr.notes = detail + "; scan_depth=" + std::to_string(pending.depth) + "; parent_node_oid=" + std::to_string(pending.parentNodeOid);
                                    apfsSpotlightTargetScanRows.push_back(rr);
                                    appendCopyAttempt(rr);
                                }
                            }
                        }
                    }

                    // V0_8_62: recursively seed copy attempts for files under Store-V2 group directories.
                    // Earlier versions targeted only filenames that looked like Spotlight components, which missed Cache/* and
                    // other ordinary names underneath the Store-V2 UUID directory.  Directory records are collected during
                    // the same bounded APFS filesystem-tree traversal above; this pass walks only from Store-V2 roots and
                    // keeps the single-AFF4/no-full-image-export policy intact.
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<std::size_t>> childrenByParent;
                        std::map<std::pair<std::uint32_t, std::uint64_t>, std::string> directoryNameByObject;
                        std::map<std::pair<std::uint32_t, std::uint64_t>, std::uint64_t> parentByObject;
                        std::set<std::pair<std::uint32_t, std::uint64_t>> directoriesSeen;
                        for (std::size_t i = 0; i < apfsDirectoryRecordEntries.size(); ++i) {
                            const auto& e = apfsDirectoryRecordEntries[i];
                            childrenByParent[std::make_pair(e.volumeSequence, e.parentObjectId)].push_back(i);
                            directoriesSeen.insert(std::make_pair(e.volumeSequence, e.parentObjectId));
                            if (e.childFileId != 0 && e.parentObjectId != 0) parentByObject[std::make_pair(e.volumeSequence, e.childFileId)] = e.parentObjectId;
                            if (!e.name.empty()) {
                                const auto childKey = std::make_pair(e.volumeSequence, e.childFileId);
                                if (!directoryNameByObject.count(childKey) || isLikelyStoreV2GroupDirectoryName(e.name)) {
                                    directoryNameByObject[childKey] = e.name;
                                }
                            }
                        }
                        auto directoryNameForObject = [&](std::uint32_t volumeSequence, std::uint64_t objectId) -> std::string {
                            const auto it = directoryNameByObject.find(std::make_pair(volumeSequence, objectId));
                            if (it == directoryNameByObject.end()) return {};
                            return it->second;
                        };
                        auto likelyGroupNameForDirectory = [&](std::uint32_t volumeSequence, std::uint64_t dirObjectId) -> std::string {
                            const std::string dirName = directoryNameForObject(volumeSequence, dirObjectId);
                            if (isLikelyStoreV2GroupDirectoryName(dirName)) return dirName;
                            return {};
                        };
                        auto apfsPathComponent = [](const std::string& value) -> std::string {
                            std::string out;
                            out.reserve(value.size());
                            for (const unsigned char ch : value) {
                                if (ch < 0x20 || ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') {
                                    out.push_back('_');
                                } else {
                                    out.push_back(static_cast<char>(ch));
                                }
                            }
                            while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
                            return out.empty() ? std::string("volume_") : out;
                        };
                        auto apfsAbsolutePathForObject = [&](std::uint32_t volumeSequence, std::uint64_t objectId) -> std::string {
                            std::vector<std::string> parts;
                            std::set<std::uint64_t> seenIds;
                            std::uint64_t cur = objectId;
                            for (;;) {
                                if (cur == 0 || !seenIds.insert(cur).second) break;
                                const auto nameIt = directoryNameByObject.find(std::make_pair(volumeSequence, cur));
                                if (nameIt != directoryNameByObject.end() && !nameIt->second.empty()) parts.push_back(nameIt->second);
                                if (cur == 2) break;
                                const auto parentIt = parentByObject.find(std::make_pair(volumeSequence, cur));
                                if (parentIt == parentByObject.end() || parentIt->second == cur) break;
                                cur = parentIt->second;
                            }
                            std::string prefix = (volumeSequence == 4) ? "/System/Volumes/Data" : ("/" + apfsPathComponent(directoryNameForObject(volumeSequence, 2)));
                            if (prefix == "/volume_") prefix = "/volume_" + std::to_string(volumeSequence);
                            std::string path = prefix;
                            for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
                                if (*it == "/" || it->empty()) continue;
                                if (path.empty() || path.back() != '/') path += "/";
                                path += *it;
                            }
                            return path;
                        };
                        std::set<std::tuple<std::uint32_t, std::uint64_t, std::uint64_t, std::string>> attemptSeen;
                        for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                            attemptSeen.insert(std::make_tuple(cr.volumeSequence, cr.parentObjectId, cr.childFileId, asciiLower(cr.targetName)));
                        }
                        struct StoreV2WalkItem {
                            std::uint32_t volumeSequence = 0;
                            std::uint64_t dirObjectId = 0;
                            std::uint64_t groupRootObjectId = 0;
                            std::string groupName;
                            std::string relPrefix;
                            std::uint32_t depth = 0;
                        };
                        std::vector<StoreV2WalkItem> queue;
                        std::set<std::pair<std::uint32_t, std::uint64_t>> queuedDirs;
                        auto enqueueDir = [&](std::uint32_t volumeSequence, std::uint64_t dirObjectId, std::uint64_t rootObjectId, const std::string& groupName, const std::string& relPrefix, std::uint32_t depth) {
                            if (dirObjectId == 0) return;
                            const auto key = std::make_pair(volumeSequence, dirObjectId);
                            if (!childrenByParent.count(key)) return;
                            if (!queuedDirs.insert(key).second) return;
                            StoreV2WalkItem item;
                            item.volumeSequence = volumeSequence;
                            item.dirObjectId = dirObjectId;
                            item.groupRootObjectId = rootObjectId ? rootObjectId : dirObjectId;
                            item.groupName = groupName;
                            item.relPrefix = relPrefix;
                            item.depth = depth;
                            queue.push_back(std::move(item));
                        };
                        for (const auto& e : apfsDirectoryRecordEntries) {
                            const std::string lname = asciiLower(e.name);
                            if (lname == "store-v2") {
                                enqueueDir(e.volumeSequence, e.childFileId, e.childFileId, "", "", 0);
                            }
                            if (isSpotlightStoreV2TopLevelComponentName(lname)) {
                                const std::string parentGroupName = likelyGroupNameForDirectory(e.volumeSequence, e.parentObjectId);
                                enqueueDir(e.volumeSequence, e.parentObjectId, e.parentObjectId, parentGroupName, "", 0);
                            }
                        }
                        std::size_t recursiveAttemptsAdded = 0;
                        std::set<std::pair<std::uint32_t, std::uint64_t>> recursiveDirsSeen;
                        for (std::size_t qi = 0; qi < queue.size(); ++qi) {
                            const auto item = queue[qi];
                            const auto childIt = childrenByParent.find(std::make_pair(item.volumeSequence, item.dirObjectId));
                            if (childIt == childrenByParent.end()) continue;
                            for (const std::size_t idx : childIt->second) {
                                const auto& e = apfsDirectoryRecordEntries[idx];
                                const std::string lname = asciiLower(e.name);
                                if (lname == ".spotlight-v100" || lname == "store-v2") continue;
                                const bool childIsDirectory = childrenByParent.count(std::make_pair(e.volumeSequence, e.childFileId)) > 0;
                                std::uint64_t groupRoot = item.groupRootObjectId;
                                std::string groupName = item.groupName;
                                std::string relPrefix = item.relPrefix;
                                if (groupName.empty() && item.depth == 0) {
                                    const std::string currentDirGroupName = likelyGroupNameForDirectory(item.volumeSequence, item.dirObjectId);
                                    if (!currentDirGroupName.empty()) {
                                        groupRoot = item.dirObjectId;
                                        groupName = currentDirGroupName;
                                        relPrefix.clear();
                                    } else if (childIsDirectory && isLikelyStoreV2GroupDirectoryName(e.name)) {
                                        groupRoot = e.childFileId;
                                        groupName = e.name;
                                        relPrefix.clear();
                                    }
                                }
                                const std::string relPath = relPrefix.empty() ? e.name : (relPrefix + "/" + e.name);
                                if (childIsDirectory) {
                                    const auto dirKey = std::make_pair(e.volumeSequence, e.childFileId);
                                    if (recursiveDirsSeen.insert(dirKey).second) {
                                        enqueueDir(e.volumeSequence, e.childFileId, groupRoot, groupName, relPath, item.depth + 1U);
                                    }
                                    continue;
                                }
                                const auto dedupeKey = std::make_tuple(e.volumeSequence, e.parentObjectId, e.childFileId, lname);
                                if (!attemptSeen.insert(dedupeKey).second) continue;
                                ApfsSpotlightCopyAttemptRow cr;
                                cr.sequence = static_cast<std::uint32_t>(apfsSpotlightCopyAttemptRows.size());
                                cr.volumeSequence = e.volumeSequence;
                                cr.targetRole = e.targetRole;
                                cr.fsOid = e.fsOid;
                                cr.volumeName = e.volumeName;
                                cr.parentObjectId = e.parentObjectId;
                                cr.childFileId = e.childFileId;
                                cr.targetName = e.name;
                                cr.targetKind = spotlightTargetKind(e.name);
                                cr.storeV2RootObjectId = groupRoot;
                                cr.storeV2GroupName = groupName;
                                cr.storeV2RelativePath = relPath;
                                cr.extractionStatus = "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED";
                                cr.interpretation = "Recursive Store-V2 namespace seeding added this ordinary-named file under a Store-V2 group directory so cache/content files can be compared against the external reference extraction.";
                                cr.notes = "V1_0_6 recursive Store-V2 seed with UUID group-name preservation and APFS catalog path context; group_root_object_id=" + std::to_string(groupRoot) + "; group_name=" + groupName + "; rel_path=" + relPath + "; apfs_absolute_path=" + apfsAbsolutePathForObject(e.volumeSequence, e.childFileId);
                                apfsSpotlightCopyAttemptRows.push_back(std::move(cr));
                                ++recursiveAttemptsAdded;
                            }
                        }

                    std::map<std::uint32_t, std::uint64_t> spotlightLogicalSizeByTargetSequence;
                    std::map<std::uint32_t, std::string> spotlightLogicalSizeSourceByTargetSequence;
                    std::map<std::uint32_t, std::uint64_t> spotlightPrivateIdByTargetSequence;

                    auto materializeScannedInodeForCopyAttempt = [&](const ApfsSpotlightCopyAttemptRow& cr, const ApfsSpotlightInodeProbeRow& cached) -> bool {
                        if (cr.childFileId == 0 || cached.volumeSequence != cr.volumeSequence || cached.inodeObjectId != cr.childFileId) return false;
                        ApfsSpotlightInodeProbeRow ir = cached;
                        ir.sequence = static_cast<std::uint32_t>(apfsSpotlightInodeProbeRows.size());
                        ir.targetSequence = cr.sequence;
                        ir.targetRole = cr.targetRole;
                        ir.fsOid = cr.fsOid;
                        ir.volumeName = cr.volumeName;
                        ir.targetParentObjectId = cr.parentObjectId;
                        ir.targetChildFileId = cr.childFileId;
                        ir.targetName = cr.targetName;
                        ir.targetKind = cr.targetKind;
                        ir.inodeStatus = "TARGET_INODE_SCAN_CORRELATION_HIT";
                        ir.interpretation = "Previously scanned APFS INODE row matched this Store-V2 target child file ID. This avoids relying only on later point lookups and enables private-id/dstream extent correlation.";
                        ir.notes += "; correlated_target_sequence=" + std::to_string(cr.sequence) + "; target_apfs_path=" + apfsAbsolutePathForObject(cr.volumeSequence, cr.childFileId);
                        if (ir.inodePrivateId != 0 && cr.targetKind != "APFS_RESOURCE_FORK_STREAM") spotlightPrivateIdByTargetSequence[cr.sequence] = ir.inodePrivateId;
                        if (ir.inodeDstreamSize != 0 && ir.inodeDstreamSize <= ir.inodeDstreamAllocedSize) {
                            spotlightLogicalSizeByTargetSequence[cr.sequence] = ir.inodeDstreamSize;
                            spotlightLogicalSizeSourceByTargetSequence[cr.sequence] = "INO_EXT_TYPE_DSTREAM.size.scan_correlation";
                        } else if (ir.inodeUncompressedSize != 0 && ir.inodeUncompressedSize <= (512ULL * 1024ULL * 1024ULL)) {
                            spotlightLogicalSizeByTargetSequence[cr.sequence] = ir.inodeUncompressedSize;
                            spotlightLogicalSizeSourceByTargetSequence[cr.sequence] = "j_inode_val.uncompressed_size.scan_correlation";
                        }
                        apfsSpotlightInodeProbeRows.push_back(std::move(ir));
                        return true;
                    };
                    for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                        if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM") continue;
                        for (const auto& cached : allSpotlightScanInodes) {
                            if (materializeScannedInodeForCopyAttempt(cr, cached)) break;
                        }
                    }

                    // V0_8_63: add bounded resource-fork stream copy attempts for Store-V2 files that have
                    // com.apple.decmpfs resource-fork compression markers plus a stream-backed com.apple.ResourceFork
                    // XATTR.  These diagnostic stream copies are intentionally not assigned Store-V2 relative paths,
                    // so they are not staged into the external comparison tree until decompression/reconstruction is
                    // implemented and validated.
                    {
                        std::map<std::uint64_t, std::size_t> storeV2AttemptIndexByFileId;
                        for (std::size_t i = 0; i < apfsSpotlightCopyAttemptRows.size(); ++i) {
                            const auto& cr = apfsSpotlightCopyAttemptRows[i];
                            if (cr.childFileId == 0 || cr.storeV2RelativePath.empty()) continue;
                            if (storeV2AttemptIndexByFileId.find(cr.childFileId) == storeV2AttemptIndexByFileId.end()) {
                                storeV2AttemptIndexByFileId[cr.childFileId] = i;
                            }
                        }

                        std::map<std::uint64_t, int> decmpfsResourceForkTypeByFileId;
                        std::map<std::uint64_t, std::uint64_t> resourceForkStreamIdByFileId;
                        std::map<std::uint64_t, std::uint64_t> resourceForkStreamSizeByFileId;
                        std::map<std::uint64_t, std::uint64_t> resourceForkStreamAllocedSizeByFileId;
                        for (const auto& xr : apfsSpotlightXattrProbeRows) {
                            const std::string lname = asciiLower(xr.xattrName);
                            if (lname == "com.apple.decmpfs") {
                                const int ctype = decmpfsCompressionTypeFromPreviewHex(xr.xdataPreviewHex);
                                if (ctype == 4 || ctype == 8 || ctype == 10 || ctype == 12 || ctype == 14) {
                                    decmpfsResourceForkTypeByFileId[xr.fileObjectId] = ctype;
                                }
                            } else if (lname == "com.apple.resourcefork" && xr.xdataStreamId != 0 && xr.xattrStorage == "XATTR_DATA_STREAM") {
                                resourceForkStreamIdByFileId[xr.fileObjectId] = xr.xdataStreamId;
                                if (xr.xdataStreamSize != 0) resourceForkStreamSizeByFileId[xr.fileObjectId] = xr.xdataStreamSize;
                                if (xr.xdataStreamAllocatedSize != 0) resourceForkStreamAllocedSizeByFileId[xr.fileObjectId] = xr.xdataStreamAllocatedSize;
                            }
                        }

                        std::set<std::tuple<std::uint32_t, std::uint64_t, std::uint64_t>> resourceForkAttemptsSeen;
                        for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                            if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM") {
                                resourceForkAttemptsSeen.insert(std::make_tuple(cr.volumeSequence, cr.parentObjectId, cr.childFileId));
                            }
                        }

                        constexpr std::size_t kMaxResourceForkStreamCopyAttempts = 30000U;
                        std::size_t addedResourceForkStreamAttempts = 0;
                        std::vector<std::uint64_t> resourceForkOriginalFileIds;
                        resourceForkOriginalFileIds.reserve(resourceForkStreamIdByFileId.size());
                        for (const auto& kv : resourceForkStreamIdByFileId) resourceForkOriginalFileIds.push_back(kv.first);
                        auto resourceForkAttemptPriority = [&](std::uint64_t originalFileId) -> int {
                            const auto attemptIt = storeV2AttemptIndexByFileId.find(originalFileId);
                            if (attemptIt == storeV2AttemptIndexByFileId.end()) return 1000;
                            const ApfsSpotlightCopyAttemptRow& base = apfsSpotlightCopyAttemptRows[attemptIt->second];
                            const std::string relLower = asciiLower(base.storeV2RelativePath);
                            const std::string nameLower = asciiLower(base.targetName);
                            const auto ctypeIt = decmpfsResourceForkTypeByFileId.find(originalFileId);
                            const int ctype = (ctypeIt == decmpfsResourceForkTypeByFileId.end()) ? 0 : ctypeIt->second;
                            const bool cacheTxt = (relLower.find("cache/") != std::string::npos && nameLower.size() >= 4 && nameLower.rfind(".txt") == nameLower.size() - 4);
                            // V0_8_73: prioritize likely comparison-closing Store-V2 Cache ZLIB_RSRC files first,
                            // then other Cache resource-fork files, before generic Store-V2 resource-fork rows.
                            if (cacheTxt && ctype == 4) return 0;
                            if (cacheTxt && ctype == 10) return 1;
                            if (cacheTxt && (ctype == 8 || ctype == 12 || ctype == 14)) return 2;
                            if (cacheTxt) return 3;
                            if (base.targetKind == "SPOTLIGHT_RELATED_NAME" && ctype == 4) return 10;
                            if (base.targetKind == "SPOTLIGHT_RELATED_NAME") return 20;
                            if (!base.storeV2RelativePath.empty() && ctype == 4) return 30;
                            if (!base.storeV2RelativePath.empty()) return 40;
                            return 100;
                        };
                        std::sort(resourceForkOriginalFileIds.begin(), resourceForkOriginalFileIds.end(), [&](std::uint64_t a, std::uint64_t b) {
                            const int pa = resourceForkAttemptPriority(a);
                            const int pb = resourceForkAttemptPriority(b);
                            if (pa != pb) return pa < pb;
                            return a < b;
                        });
                        for (const std::uint64_t originalFileId : resourceForkOriginalFileIds) {
                            if (addedResourceForkStreamAttempts >= kMaxResourceForkStreamCopyAttempts) break;
                            const auto rfIdIt0 = resourceForkStreamIdByFileId.find(originalFileId);
                            if (rfIdIt0 == resourceForkStreamIdByFileId.end()) continue;
                            const std::uint64_t resourceForkStreamId = rfIdIt0->second;
                            if (resourceForkStreamId == 0) continue;
                            const auto attemptIt = storeV2AttemptIndexByFileId.find(originalFileId);
                            if (attemptIt == storeV2AttemptIndexByFileId.end()) continue;
                            const auto ctypeIt = decmpfsResourceForkTypeByFileId.find(originalFileId);
                            if (ctypeIt == decmpfsResourceForkTypeByFileId.end()) continue;
                            const ApfsSpotlightCopyAttemptRow& base = apfsSpotlightCopyAttemptRows[attemptIt->second];
                            const auto seenKey = std::make_tuple(base.volumeSequence, originalFileId, resourceForkStreamId);
                            if (!resourceForkAttemptsSeen.insert(seenKey).second) continue;

                            ApfsSpotlightCopyAttemptRow cr = base;
                            cr.sequence = static_cast<std::uint32_t>(apfsSpotlightCopyAttemptRows.size());
                            cr.parentObjectId = originalFileId;
                            cr.childFileId = resourceForkStreamId;
                            cr.targetName = base.targetName + ".__RESOURCEFORK_STREAM";
                            cr.targetKind = "APFS_RESOURCE_FORK_STREAM";
                            cr.storeV2RootObjectId = 0;
                            cr.storeV2GroupName.clear();
                            cr.storeV2RelativePath.clear();
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED";
                            cr.interpretation = "Synthetic diagnostic copy attempt for a stream-backed com.apple.ResourceFork associated with a compressed APFS Store-V2 file. The stream is copied separately so later builds can parse/decompress the decmpfs resource-fork payload without polluting the Store-V2 comparison tree.";
                            cr.notes = "V0_8_68 resource-fork stream seed; original_file_id=" + std::to_string(originalFileId) + "; resource_fork_stream_id=" + std::to_string(resourceForkStreamId) + "; decmpfs_compression_type=" + std::to_string(ctypeIt->second) + "; original_storev2_relative_path=" + base.storeV2RelativePath;
                            const auto rfSizeIt = resourceForkStreamSizeByFileId.find(originalFileId);
                            if (rfSizeIt != resourceForkStreamSizeByFileId.end() && rfSizeIt->second != 0) {
                                spotlightLogicalSizeByTargetSequence[cr.sequence] = rfSizeIt->second;
                                spotlightLogicalSizeSourceByTargetSequence[cr.sequence] = "com.apple.ResourceFork.j_xattr_dstream.size";
                                cr.notes += "; resource_fork_stream_size=" + std::to_string(rfSizeIt->second);
                            }
                            const auto rfAllocIt = resourceForkStreamAllocedSizeByFileId.find(originalFileId);
                            if (rfAllocIt != resourceForkStreamAllocedSizeByFileId.end() && rfAllocIt->second != 0) {
                                cr.notes += "; resource_fork_alloced_size=" + std::to_string(rfAllocIt->second);
                            }
                            apfsSpotlightCopyAttemptRows.push_back(std::move(cr));
                            ++addedResourceForkStreamAttempts;
                        }
                        if (addedResourceForkStreamAttempts != 0) {
                            log.info("Added APFS resource-fork stream copy attempts: " + std::to_string(addedResourceForkStreamAttempts));
                        }
                    }

                    auto compareApfsFsKeyForGuidedLookup = [&](std::uint64_t obj, std::uint8_t type, std::uint64_t logical,
                                                                std::uint64_t targetObj, std::uint8_t targetType, std::uint64_t targetLogical) -> int {
                        if (obj < targetObj) return -1;
                        if (obj > targetObj) return 1;
                        if (type < targetType) return -1;
                        if (type > targetType) return 1;
                        if (logical < targetLogical) return -1;
                        if (logical > targetLogical) return 1;
                        return 0;
                    };

                    auto appendGuidedNoMatchExtentRow = [&](const ApfsSpotlightCopyAttemptRow& cr,
                                                            std::uint64_t candidateObjectId,
                                                            const std::string& status,
                                                            const std::string& detail) {
                        ApfsSpotlightFileExtentProbeRow er;
                        er.sequence = static_cast<std::uint32_t>(apfsSpotlightFileExtentProbeRows.size());
                        er.targetSequence = cr.sequence;
                        er.volumeSequence = cr.volumeSequence;
                        er.targetRole = cr.targetRole;
                        er.fsOid = cr.fsOid;
                        er.volumeName = cr.volumeName;
                        er.targetParentObjectId = cr.parentObjectId;
                        er.targetChildFileId = cr.childFileId;
                        er.targetName = cr.targetName;
                        er.targetKind = cr.targetKind;
                        er.extentFileId = candidateObjectId;
                        er.extentStatus = status;
                        er.previewStatus = "NO_PREVIEW";
                        er.interpretation = "Guided APFS filesystem B-tree lookup for a Spotlight target FILE_EXTENT candidate did not produce a copyable extent row.";
                        er.notes = detail;
                        apfsSpotlightFileExtentProbeRows.push_back(er);
                    };

                    auto appendGuidedNoMatchInodeRow = [&](const ApfsSpotlightCopyAttemptRow& cr,
                                                           const std::string& status,
                                                           const std::string& detail) {
                        ApfsSpotlightInodeProbeRow ir;
                        ir.sequence = static_cast<std::uint32_t>(apfsSpotlightInodeProbeRows.size());
                        ir.targetSequence = cr.sequence;
                        ir.volumeSequence = cr.volumeSequence;
                        ir.targetRole = cr.targetRole;
                        ir.fsOid = cr.fsOid;
                        ir.volumeName = cr.volumeName;
                        ir.targetParentObjectId = cr.parentObjectId;
                        ir.targetChildFileId = cr.childFileId;
                        ir.targetName = cr.targetName;
                        ir.targetKind = cr.targetKind;
                        ir.inodeObjectId = cr.childFileId;
                        ir.inodeStatus = status;
                        ir.interpretation = "Guided APFS filesystem B-tree lookup did not resolve an INODE record for the Spotlight/CoreSpotlight target child file ID or derived object-ID candidate.";
                        ir.notes = detail;
                        apfsSpotlightInodeProbeRows.push_back(ir);
                    };

                    auto lookupGuidedTargetInode = [&](const ApfsSpotlightCopyAttemptRow& cr,
                                                         std::uint64_t candidateObjectId,
                                                         const std::string& candidateLabel) -> bool {
                        if (cr.childFileId == 0 || candidateObjectId == 0) return false;
                        const ApfsVolumeRootTreeLookupRow* lookupPtr = nullptr;
                        for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                            if (lookup.volumeSequence == cr.volumeSequence && lookup.rootTreeStatus == "ROOT_TREE_BTREE_READ" && lookup.resolvedVirtualOffset != 0) {
                                lookupPtr = &lookup;
                                break;
                            }
                        }
                        if (lookupPtr == nullptr) {
                            appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_ROOT_TREE_UNAVAILABLE", "candidate=" + candidateLabel + "; no readable root-tree lookup row for this volume");
                            return false;
                        }
                        auto omIt = apfsVolumeOmapRowsByVolumeSequence.find(cr.volumeSequence);
                        auto rootIt = apfsVolumeOmapRootNodesByVolumeSequence.find(cr.volumeSequence);
                        if (omIt == apfsVolumeOmapRowsByVolumeSequence.end() || rootIt == apfsVolumeOmapRootNodesByVolumeSequence.end()) {
                            appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_VOLUME_OMAP_UNAVAILABLE", "candidate=" + candidateLabel + "; volume OMAP root buffer unavailable");
                            return false;
                        }
                        std::vector<unsigned char> node;
                        std::string readErr;
                        long long nodeRead = readVirtual(lookupPtr->resolvedVirtualOffset, nxSummary.blockSize, node, readErr);
                        std::uint64_t nodeOid = lookupPtr->resolvedObjectOid ? lookupPtr->resolvedObjectOid : lookupPtr->apfsRootTreeOid;
                        std::uint64_t nodeOffset = lookupPtr->resolvedVirtualOffset;
                        if (nodeRead <= 0 || node.size() < 64) {
                            appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_ROOT_READ_FAILED", "candidate=" + candidateLabel + "; " + (readErr.empty() ? std::string("root tree read failed") : readErr));
                            return false;
                        }

                        constexpr std::uint8_t kTargetTypeInode = 3U;
                        constexpr std::uint32_t kMaxGuidedDepth = 16U;
                        std::string branchPath;
                        for (std::uint32_t depth = 0; depth < kMaxGuidedDepth; ++depth) {
                            if (node.size() < 64) break;
                            const std::uint32_t rawType = readLe32(node, 24);
                            const std::string objectLabel = apfsObjectTypeLabel(rawType);
                            const std::uint16_t level = readLe16(node, 34);
                            const std::uint32_t nkeys = readLe32(node, 36);
                            if (!branchPath.empty()) branchPath += " -> ";
                            branchPath += "oid=" + std::to_string(nodeOid) + ";level=" + std::to_string(level) + ";nkeys=" + std::to_string(nkeys);
                            if (objectLabel != "BTREE" && objectLabel != "BTREE_NODE") {
                                appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_UNEXPECTED_OBJECT_TYPE", "candidate=" + candidateLabel + "; object_type=" + objectLabel + "; path=" + branchPath);
                                return false;
                            }
                            const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
                            if (level == 0) {
                                bool exactObjectSeen = false;
                                for (std::uint32_t i = 0; i < limit; ++i) {
                                    std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                    std::string detail;
                                    if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                                    if (keyLen < 8U) continue;
                                    const std::uint64_t keyRaw = readLe64(node, keyAbs);
                                    const std::uint64_t obj = apfsFsKeyObjectId(keyRaw);
                                    const std::uint8_t typ = apfsFsKeyRecordType(keyRaw);
                                    if (obj == candidateObjectId) exactObjectSeen = true;
                                    if (obj != candidateObjectId || typ != kTargetTypeInode) continue;
                                    ApfsSpotlightInodeProbeRow ir;
                                    ir.sequence = static_cast<std::uint32_t>(apfsSpotlightInodeProbeRows.size());
                                    ir.targetSequence = cr.sequence;
                                    ir.volumeSequence = cr.volumeSequence;
                                    ir.targetRole = cr.targetRole;
                                    ir.fsOid = cr.fsOid;
                                    ir.volumeName = cr.volumeName;
                                    ir.targetParentObjectId = cr.parentObjectId;
                                    ir.targetChildFileId = cr.childFileId;
                                    ir.targetName = cr.targetName;
                                    ir.targetKind = cr.targetKind;
                                    ir.inodeObjectId = obj;
                                    if (valLen >= 8U && valAbs + 8U <= node.size()) ir.inodeParentId = readLe64(node, valAbs + 0U);
                                    if (valLen >= 16U && valAbs + 16U <= node.size()) ir.inodePrivateId = readLe64(node, valAbs + 8U);
                                    if (valLen >= 24U && valAbs + 24U <= node.size()) ir.inodeCreateTimeRaw = readLe64(node, valAbs + 16U);
                                    if (valLen >= 32U && valAbs + 32U <= node.size()) ir.inodeModTimeRaw = readLe64(node, valAbs + 24U);
                                    if (valLen >= 40U && valAbs + 40U <= node.size()) ir.inodeChangeTimeRaw = readLe64(node, valAbs + 32U);
                                    if (valLen >= 48U && valAbs + 48U <= node.size()) ir.inodeAccessTimeRaw = readLe64(node, valAbs + 40U);
                                    if (valLen >= 56U && valAbs + 56U <= node.size()) ir.inodeInternalFlags = readLe64(node, valAbs + 48U);
                                    if (valLen >= 60U && valAbs + 60U <= node.size()) ir.inodeNchildrenOrNlink = readLe32(node, valAbs + 56U);
                                    if (valLen >= 84U && valAbs + 84U <= node.size()) ir.inodeModeCandidate = readLe16(node, valAbs + 82U);
                                    if (valLen >= 92U && valAbs + 92U <= node.size()) ir.inodeUncompressedSize = readLe64(node, valAbs + 84U);
                                    const ApfsInodeExtendedFieldDecode xf = decodeApfsInodeExtendedFieldsForProbe(node, valAbs, valLen);
                                    ir.inodeXfieldStatus = xf.status;
                                    if (xf.sawDstream) {
                                        ir.inodeDstreamSize = xf.dstreamSize;
                                        ir.inodeDstreamAllocedSize = xf.dstreamAllocedSize;
                                        ir.inodeDstreamDefaultCryptoId = xf.dstreamDefaultCryptoId;
                                    }
                                    ir.nodeOid = nodeOid;
                                    ir.nodeVirtualOffset = nodeOffset;
                                    ir.nodeLevel = level;
                                    ir.nodeNkeys = nkeys;
                                    ir.entryIndex = i;
                                    ir.inodeStatus = "TARGET_INODE_GUIDED_LOOKUP_HIT";
                                    ir.interpretation = "Guided APFS filesystem B-tree lookup matched an INODE record for a Spotlight/CoreSpotlight target object-ID candidate. The inode private ID is treated as a data-stream/extent lookup candidate.";
                                    if (valLen > 0 && valAbs < node.size()) {
                                        const std::size_t avail = std::min<std::size_t>(node.size() - valAbs, valLen);
                                        ir.valueSampleHex = hexSampleBytes(node.data() + valAbs, std::min<std::size_t>(avail, 96U));
                                    }
                                    ir.notes = "candidate=" + candidateLabel + "; candidate_object_id=" + std::to_string(candidateObjectId) + "; branch_path=" + branchPath + "; " + detail;
                                    if (!xf.notes.empty()) ir.notes += "; xfields=" + xf.notes;
                                    if (ir.inodePrivateId != 0 && cr.targetKind != "APFS_RESOURCE_FORK_STREAM") spotlightPrivateIdByTargetSequence[cr.sequence] = ir.inodePrivateId;
                                    const auto existingLogicalSourceIt = spotlightLogicalSizeSourceByTargetSequence.find(cr.sequence);
                                    const bool preserveExplicitXattrDstreamSize =
                                        (cr.targetKind == "APFS_RESOURCE_FORK_STREAM" &&
                                         existingLogicalSourceIt != spotlightLogicalSizeSourceByTargetSequence.end() &&
                                         existingLogicalSourceIt->second.find("com.apple.ResourceFork.j_xattr_dstream.size") != std::string::npos);
                                    if (!preserveExplicitXattrDstreamSize) {
                                        if (ir.inodeDstreamSize != 0 && ir.inodeDstreamSize <= ir.inodeDstreamAllocedSize) {
                                            spotlightLogicalSizeByTargetSequence[cr.sequence] = ir.inodeDstreamSize;
                                            spotlightLogicalSizeSourceByTargetSequence[cr.sequence] = "INO_EXT_TYPE_DSTREAM.size";
                                        } else if (ir.inodeUncompressedSize != 0 && ir.inodeUncompressedSize <= (512ULL * 1024ULL * 1024ULL)) {
                                            spotlightLogicalSizeByTargetSequence[cr.sequence] = ir.inodeUncompressedSize;
                                            spotlightLogicalSizeSourceByTargetSequence[cr.sequence] = "j_inode_val.uncompressed_size";
                                        }
                                    } else {
                                        ir.notes += "; preserved_explicit_resource_fork_xattr_dstream_size_for_dissect_apfs_file_stream_model";
                                    }
                                    apfsSpotlightInodeProbeRows.push_back(ir);
                                    return true;
                                }
                                appendGuidedNoMatchInodeRow(cr, exactObjectSeen ? "GUIDED_INODE_LOOKUP_OBJECT_SEEN_NO_INODE" : "GUIDED_INODE_LOOKUP_NO_MATCH_IN_LEAF", "candidate=" + candidateLabel + "; candidate_object_id=" + std::to_string(candidateObjectId) + "; branch_path=" + branchPath);
                                return false;
                            }

                            bool childFound = false;
                            std::uint64_t selectedChildOid = 0;
                            std::uint32_t selectedEntry = 0;
                            std::uint64_t firstChild = 0;
                            std::uint32_t firstEntry = 0;
                            bool haveFirstChild = false;
                            for (std::uint32_t i = 0; i < limit; ++i) {
                                std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                std::string detail;
                                if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                                if (keyLen < 8U || valLen < 8U || valAbs + 8U > node.size()) continue;
                                const std::uint64_t keyRaw = readLe64(node, keyAbs);
                                const std::uint64_t obj = apfsFsKeyObjectId(keyRaw);
                                const std::uint8_t typ = apfsFsKeyRecordType(keyRaw);
                                const std::uint64_t childOid = readLe64(node, valAbs);
                                if (childOid == 0) continue;
                                if (!haveFirstChild) { firstChild = childOid; firstEntry = i; haveFirstChild = true; }
                                const int cmp = compareApfsFsKeyForGuidedLookup(obj, typ, 0, candidateObjectId, kTargetTypeInode, 0);
                                if (cmp <= 0) {
                                    // APFS filesystem B+ tree internal keys index the smallest key in each child.
                                    // Select the last child whose separator key is <= the target key.
                                    selectedChildOid = childOid;
                                    selectedEntry = i;
                                    childFound = true;
                                    continue;
                                }
                                if (!childFound) {
                                    // Target sorts before the first separator; descend to the first child.
                                    selectedChildOid = firstChild;
                                    selectedEntry = firstEntry;
                                    childFound = true;
                                }
                                break;
                            }
                            if (!childFound || selectedChildOid == 0) {
                                appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_BRANCH_CHILD_NOT_SELECTED", "candidate=" + candidateLabel + "; path=" + branchPath);
                                return false;
                            }
                            ApfsOmapTargetResolution resolved = aff4ResolveVolumeOmapTargetObjectForProbe(omIt->second, rootIt->second, selectedChildOid, lookupPtr->targetXid, nxSummary.blockSize, readVirtual, safeSpotlightNodeOffset, "APFS guided Spotlight target INODE lookup");
                            if (resolved.objectStatus != "OMAP_TARGET_BTREE_READ") {
                                appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_CHILD_READ_FAILED", "candidate=" + candidateLabel + "; selected_entry=" + std::to_string(selectedEntry) + "; child_oid=" + std::to_string(selectedChildOid) + "; lookup_status=" + resolved.lookupStatus + "; object_status=" + resolved.objectStatus + "; path=" + branchPath);
                                return false;
                            }
                            node = resolved.resolvedBuffer;
                            nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : selectedChildOid;
                            nodeOffset = resolved.resolvedVirtualOffset;
                        }
                        appendGuidedNoMatchInodeRow(cr, "GUIDED_INODE_LOOKUP_DEPTH_LIMIT", "candidate=" + candidateLabel + "; branch_path=" + branchPath);
                        return false;
                    };

                    auto lookupGuidedFileExtentCandidate = [&](const ApfsSpotlightCopyAttemptRow& cr,
                                                               std::uint64_t candidateObjectId,
                                                               const std::string& candidateLabel,
                                                               std::uint64_t requestedLogicalOffset) -> bool {
                        if (cr.childFileId == 0 || candidateObjectId == 0) return false;
                        const ApfsVolumeRootTreeLookupRow* lookupPtr = nullptr;
                        for (const auto& lookup : apfsVolumeRootTreeLookupRows) {
                            if (lookup.volumeSequence == cr.volumeSequence && lookup.rootTreeStatus == "ROOT_TREE_BTREE_READ" && lookup.resolvedVirtualOffset != 0) {
                                lookupPtr = &lookup;
                                break;
                            }
                        }
                        if (lookupPtr == nullptr) {
                            appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_ROOT_TREE_UNAVAILABLE", "candidate=" + candidateLabel + "; no readable root-tree lookup row for this volume");
                            return false;
                        }
                        auto omIt = apfsVolumeOmapRowsByVolumeSequence.find(cr.volumeSequence);
                        auto rootIt = apfsVolumeOmapRootNodesByVolumeSequence.find(cr.volumeSequence);
                        if (omIt == apfsVolumeOmapRowsByVolumeSequence.end() || rootIt == apfsVolumeOmapRootNodesByVolumeSequence.end()) {
                            appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_VOLUME_OMAP_UNAVAILABLE", "candidate=" + candidateLabel + "; volume OMAP root buffer unavailable");
                            return false;
                        }

                        std::vector<unsigned char> node;
                        std::string readErr;
                        long long nodeRead = readVirtual(lookupPtr->resolvedVirtualOffset, nxSummary.blockSize, node, readErr);
                        std::uint64_t nodeOid = lookupPtr->resolvedObjectOid ? lookupPtr->resolvedObjectOid : lookupPtr->apfsRootTreeOid;
                        std::uint64_t nodeOffset = lookupPtr->resolvedVirtualOffset;
                        if (nodeRead <= 0 || node.size() < 64) {
                            appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_ROOT_READ_FAILED", "candidate=" + candidateLabel + "; " + (readErr.empty() ? std::string("root tree read failed") : readErr));
                            return false;
                        }

                        constexpr std::uint8_t kTargetTypeFileExtent = 8U;
                        const std::uint64_t kTargetLogicalOffset = requestedLogicalOffset;
                        constexpr std::uint32_t kMaxGuidedDepth = 16U;
                        bool anyHit = false;
                        std::string branchPath;
                        for (std::uint32_t depth = 0; depth < kMaxGuidedDepth; ++depth) {
                            if (node.size() < 64) break;
                            const std::uint32_t rawType = readLe32(node, 24);
                            const std::string objectLabel = apfsObjectTypeLabel(rawType);
                            const std::uint16_t level = readLe16(node, 34);
                            const std::uint32_t nkeys = readLe32(node, 36);
                            if (!branchPath.empty()) branchPath += " -> ";
                            branchPath += "oid=" + std::to_string(nodeOid) + ";level=" + std::to_string(level) + ";nkeys=" + std::to_string(nkeys);
                            if (objectLabel != "BTREE" && objectLabel != "BTREE_NODE") {
                                appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_UNEXPECTED_OBJECT_TYPE", "candidate=" + candidateLabel + "; object_type=" + objectLabel + "; path=" + branchPath);
                                return anyHit;
                            }
                            const std::uint32_t limit = std::min<std::uint32_t>(nkeys, 65536U);
                            if (level == 0) {
                                bool exactObjectSeen = false;
                                bool exactTypeSeen = false;
                                for (std::uint32_t i = 0; i < limit; ++i) {
                                    std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                    std::string detail;
                                    if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                                    if (keyLen < 8U) continue;
                                    const std::uint64_t keyRaw = readLe64(node, keyAbs);
                                    const std::uint64_t obj = apfsFsKeyObjectId(keyRaw);
                                    const std::uint8_t typ = apfsFsKeyRecordType(keyRaw);
                                    const std::uint64_t logical = (keyLen >= 16U && keyAbs + 16U <= node.size()) ? readLe64(node, keyAbs + 8U) : 0ULL;
                                    if (obj == candidateObjectId) exactObjectSeen = true;
                                    if (obj == candidateObjectId && typ == kTargetTypeFileExtent) exactTypeSeen = true;
                                    if (obj != candidateObjectId || typ != kTargetTypeFileExtent) continue;
                                    if (valLen < 16U || valAbs + 16U > node.size()) continue;
                                    ApfsSpotlightFileExtentProbeRow er;
                                    er.sequence = static_cast<std::uint32_t>(allSpotlightScanFileExtents.size());
                                    er.targetSequence = cr.sequence;
                                    er.volumeSequence = cr.volumeSequence;
                                    er.targetRole = cr.targetRole;
                                    er.fsOid = cr.fsOid;
                                    er.volumeName = cr.volumeName;
                                    er.targetParentObjectId = cr.parentObjectId;
                                    er.targetChildFileId = cr.childFileId;
                                    er.targetName = cr.targetName;
                                    er.targetKind = cr.targetKind;
                                    // Use the directory-record child id as the match key so the existing copy-attempt updater can attach this row to the target.
                                    er.extentFileId = cr.childFileId;
                                    er.extentLogicalOffset = logical;
                                    er.lenAndFlags = readLe64(node, valAbs + 0U);
                                    er.extentLengthBytes = er.lenAndFlags & 0x00ffffffffffffffULL;
                                    er.extentFlags = static_cast<std::uint32_t>((er.lenAndFlags >> 56U) & 0xffU);
                                    er.physicalBlock = readLe64(node, valAbs + 8U);
                                    if (valLen >= 24U && valAbs + 24U <= node.size()) er.cryptoId = readLe64(node, valAbs + 16U);
                                    if (nxSummary.blockSize != 0 && er.physicalBlock <= (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(nxSummary.blockSize))) {
                                        er.physicalOffset = er.physicalBlock * static_cast<std::uint64_t>(nxSummary.blockSize);
                                    }
                                    er.nodeOid = nodeOid;
                                    er.nodeVirtualOffset = nodeOffset;
                                    er.nodeLevel = level;
                                    er.nodeNkeys = nkeys;
                                    er.entryIndex = i;
                                    er.extentStatus = "TARGET_FILE_EXTENT_GUIDED_LOOKUP_HIT";
                                    er.interpretation = "Guided APFS filesystem B-tree lookup matched a FILE_EXTENT record for a Spotlight/CoreSpotlight target candidate object ID.";
                                    er.notes = "candidate=" + candidateLabel + "; requested_logical_offset=" + std::to_string(kTargetLogicalOffset) + "; actual_key_object_id=" + std::to_string(candidateObjectId) + "; branch_path=" + branchPath + "; " + detail;
                                    allSpotlightScanFileExtents.push_back(er);
                                    anyHit = true;
                                }
                                if (!anyHit) {
                                    std::string status = exactTypeSeen ? "GUIDED_FILE_EXTENT_LOOKUP_TYPE_SEEN_NO_USABLE_VALUE" : (exactObjectSeen ? "GUIDED_FILE_EXTENT_LOOKUP_OBJECT_SEEN_NO_FILE_EXTENT" : "GUIDED_FILE_EXTENT_LOOKUP_NO_MATCH_IN_LEAF");
                                    appendGuidedNoMatchExtentRow(cr, candidateObjectId, status, "candidate=" + candidateLabel + "; branch_path=" + branchPath);
                                }
                                return anyHit;
                            }

                            bool childFound = false;
                            std::uint64_t selectedChildOid = 0;
                            std::uint32_t selectedEntry = 0;
                            std::uint64_t firstChild = 0;
                            std::uint32_t firstEntry = 0;
                            bool haveFirstChild = false;
                            for (std::uint32_t i = 0; i < limit; ++i) {
                                std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                                std::string detail;
                                if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                                if (keyLen < 8U || valLen < 8U || valAbs + 8U > node.size()) continue;
                                const std::uint64_t keyRaw = readLe64(node, keyAbs);
                                const std::uint64_t obj = apfsFsKeyObjectId(keyRaw);
                                const std::uint8_t typ = apfsFsKeyRecordType(keyRaw);
                                const std::uint64_t logical = (keyLen >= 16U && keyAbs + 16U <= node.size()) ? readLe64(node, keyAbs + 8U) : 0ULL;
                                const std::uint64_t childOid = readLe64(node, valAbs);
                                if (childOid == 0) continue;
                                if (!haveFirstChild) { firstChild = childOid; firstEntry = i; haveFirstChild = true; }
                                const int cmp = compareApfsFsKeyForGuidedLookup(obj, typ, logical, candidateObjectId, kTargetTypeFileExtent, kTargetLogicalOffset);
                                if (cmp <= 0) {
                                    // APFS filesystem B+ tree internal keys index the smallest key in each child.
                                    // Select the last child whose separator key is <= the target key.
                                    selectedChildOid = childOid;
                                    selectedEntry = i;
                                    childFound = true;
                                    continue;
                                }
                                if (!childFound) {
                                    // Target sorts before the first separator; descend to the first child.
                                    selectedChildOid = firstChild;
                                    selectedEntry = firstEntry;
                                    childFound = true;
                                }
                                break;
                            }
                            if (!childFound || selectedChildOid == 0) {
                                appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_BRANCH_CHILD_NOT_SELECTED", "candidate=" + candidateLabel + "; path=" + branchPath);
                                return anyHit;
                            }
                            ApfsOmapTargetResolution resolved = aff4ResolveVolumeOmapTargetObjectForProbe(omIt->second, rootIt->second, selectedChildOid, lookupPtr->targetXid, nxSummary.blockSize, readVirtual, safeSpotlightNodeOffset, "APFS guided Spotlight target FILE_EXTENT lookup");
                            if (resolved.objectStatus != "OMAP_TARGET_BTREE_READ") {
                                appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_CHILD_READ_FAILED", "candidate=" + candidateLabel + "; selected_entry=" + std::to_string(selectedEntry) + "; child_oid=" + std::to_string(selectedChildOid) + "; lookup_status=" + resolved.lookupStatus + "; object_status=" + resolved.objectStatus + "; path=" + branchPath);
                                return anyHit;
                            }
                            node = resolved.resolvedBuffer;
                            nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : selectedChildOid;
                            nodeOffset = resolved.resolvedVirtualOffset;
                        }
                        appendGuidedNoMatchExtentRow(cr, candidateObjectId, "GUIDED_FILE_EXTENT_LOOKUP_DEPTH_LIMIT", "candidate=" + candidateLabel + "; branch_path=" + branchPath);
                        return anyHit;
                    };

                    auto guidedObjectCandidatesForChild = [&](std::uint64_t childFileId, std::uint64_t privateId) -> std::vector<std::pair<std::uint64_t, std::string>> {
                        std::vector<std::pair<std::uint64_t, std::string>> out;
                        std::set<std::uint64_t> seen;
                        auto add = [&](std::uint64_t v, const std::string& label) {
                            if (v == 0) return;
                            if (seen.insert(v).second) out.push_back(std::make_pair(v, label));
                        };
                        add(childFileId, "child_file_id_raw");
                        if (privateId != 0) {
                            add(privateId, "inode_private_id_dstream_candidate");
                            add(privateId >> 4U, "inode_private_id_shifted_right_4");
                        }
                        const std::uint64_t shifted = childFileId >> 4U;
                        add(shifted, "child_file_id_shifted_right_4");
                        for (std::uint64_t prefix = 1; prefix <= 15; ++prefix) {
                            add((prefix << 56U) | shifted, "high_prefix_" + std::to_string(prefix) + "_plus_child_shifted_right_4");
                        }
                        return out;
                    };

                    for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                        if (cr.childFileId == 0) continue;
                        if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM") continue; // dissect.apfs XAttr.open() uses xattr_obj_id directly as a FileStream; do not reinterpret it through inode private/uncompressed-size metadata.
                        if (!(cr.extractionStatus == "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED" || cr.extractionStatus == "COPY_NOT_ATTEMPTED_EXTENTS_FOUND_FULL_ASSEMBLY_NOT_READY")) continue;
                        const auto inodeCandidates = guidedObjectCandidatesForChild(cr.childFileId, 0);
                        for (const auto& cand : inodeCandidates) {
                            if (lookupGuidedTargetInode(cr, cand.first, cand.second)) break;
                        }
                    }

                    for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                        if (cr.childFileId == 0) continue;
                        if (!(cr.extractionStatus == "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED" || cr.extractionStatus == "COPY_NOT_ATTEMPTED_EXTENTS_FOUND_FULL_ASSEMBLY_NOT_READY")) continue;
                        std::vector<std::pair<std::uint64_t, std::string>> candidates;
                        if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM") {
                            // Follow dissect.apfs: XAttr.open() returns FileStream(volume, xattr_obj_id, dstream.size).
                            // The FILE_EXTENT key object is the xattr data-stream object ID, not the original inode/private ID.
                            candidates.push_back(std::make_pair(cr.childFileId, "xattr_dstream_object_id"));
                        } else {
                            const auto privateIt = spotlightPrivateIdByTargetSequence.find(cr.sequence);
                            const std::uint64_t privateId = (privateIt == spotlightPrivateIdByTargetSequence.end()) ? 0ULL : privateIt->second;
                            candidates = guidedObjectCandidatesForChild(cr.childFileId, privateId);
                        }
                        for (const auto& cand : candidates) {
                            lookupGuidedFileExtentCandidate(cr, cand.first, cand.second, 0ULL);
                        }
                    }

                    auto materializeGuidedExtentForCopyAttempt = [&](const ApfsSpotlightCopyAttemptRow& cr, const ApfsSpotlightFileExtentProbeRow& erBase) -> bool {
                        if (erBase.volumeSequence != cr.volumeSequence || erBase.extentFileId != cr.childFileId || cr.childFileId == 0) return false;
                        ApfsSpotlightFileExtentProbeRow er = erBase;
                        er.sequence = static_cast<std::uint32_t>(apfsSpotlightFileExtentProbeRows.size());
                        er.targetSequence = cr.sequence;
                        er.targetParentObjectId = cr.parentObjectId;
                        er.targetChildFileId = cr.childFileId;
                        er.targetName = cr.targetName;
                        er.targetKind = cr.targetKind;
                        er.extentStatus = "TARGET_FILE_EXTENT_CANDIDATE";
                        er.interpretation = "APFS FILE_EXTENT record matched a Spotlight/CoreSpotlight target child file ID. Full copy-out remains gated on complete extent assembly and hash verification.";
                        if (er.extentLogicalOffset == 0 && er.physicalOffset != 0 && er.extentLengthBytes > 0) {
                            const std::uint64_t previewLen = std::min<std::uint64_t>(4096ULL, er.extentLengthBytes);
                            std::vector<unsigned char> preview;
                            std::string previewErr;
                            er.previewBytesRead = readVirtual(er.physicalOffset, previewLen, preview, previewErr);
                            if (er.previewBytesRead > 0) {
                                er.previewStatus = directPreviewStatusForBytes(preview);
                                er.previewSampleHex = hexSampleBytes(preview.data(), preview.size() < 96 ? preview.size() : 96);
                            } else {
                                er.previewStatus = "PREVIEW_READ_FAILED";
                                er.notes += previewErr.empty() ? "; preview_read_failed" : ("; " + previewErr);
                            }
                        } else {
                            er.previewStatus = "PREVIEW_NOT_LOGICAL_ZERO_EXTENT";
                        }
                        apfsSpotlightFileExtentProbeRows.push_back(er);
                        return true;
                    };

                    for (auto& cr : apfsSpotlightCopyAttemptRows) {
                        bool foundExtent = false;
                        for (const auto& erBase : allSpotlightScanFileExtents) {
                            if (materializeGuidedExtentForCopyAttempt(cr, erBase)) foundExtent = true;
                        }
                        if (foundExtent && cr.extractionStatus == "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED") {
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_EXTENTS_FOUND_FULL_ASSEMBLY_NOT_READY";
                            cr.interpretation = "One or more APFS FILE_EXTENT records were matched to this Spotlight-related file. Full file copy-out is still gated until all extents are assembled and the extracted file can be verified.";
                            cr.notes = "V0_8_68 resolves target file extents, bounded previews, recursive Store-V2 namespace copy attempts, duplicate-free extent assembly, inode DSTREAM logical-size metadata, and materialized follow-up FILE_EXTENT probes where available; complete production use still requires full path reconstruction and compression/xattr handling.";
                        }
                    }


                    auto safeExtractionFileName = [&](std::uint32_t seq, std::uint64_t fileId, const std::string& name) -> std::string {
                        std::string out = std::to_string(seq) + "_fid_" + std::to_string(fileId) + "_";
                        for (char ch : name) {
                            const unsigned char c = static_cast<unsigned char>(ch);
                            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || ch == '.' || ch == '_' || ch == '-') out.push_back(ch);
                            else out.push_back('_');
                            if (out.size() >= 180) break;
                        }
                        if (out.empty()) out = std::to_string(seq) + "_fid_" + std::to_string(fileId) + "_unnamed.bin";
                        return out;
                    };

                    auto appendCopyOutRow = [&](const ApfsSpotlightCopyAttemptRow& cr,
                                                const std::vector<ApfsSpotlightFileExtentProbeRow>& extentsForTarget,
                                                const std::string& status,
                                                const std::string& validation,
                                                const std::string& notes) {
                        ApfsSpotlightFileCopyOutRow row;
                        row.sequence = static_cast<std::uint32_t>(apfsSpotlightFileCopyOutRows.size());
                        row.targetSequence = cr.sequence;
                        row.volumeSequence = cr.volumeSequence;
                        row.targetRole = cr.targetRole;
                        row.fsOid = cr.fsOid;
                        row.volumeName = cr.volumeName;
                        row.targetParentObjectId = cr.parentObjectId;
                        row.targetChildFileId = cr.childFileId;
                        row.targetName = cr.targetName;
                        row.targetKind = cr.targetKind;
                        row.storeV2RootObjectId = cr.storeV2RootObjectId;
                        row.storeV2GroupName = cr.storeV2GroupName;
                        row.storeV2RelativePath = cr.storeV2RelativePath;
                        row.extentCount = static_cast<std::uint32_t>(extentsForTarget.size());
                        row.copyStatus = status;
                        row.validationStatus = validation;
                        row.interpretation = "Controlled AFF4/APFS Spotlight copy-out gate decision for a target with decoded FILE_EXTENT rows.";
                        row.notes = notes;
                        apfsSpotlightFileCopyOutRows.push_back(row);
                    };

                    constexpr std::uint64_t kMaxSingleCopyOutBytes = 512ULL * 1024ULL * 1024ULL;
                    std::size_t copiedOutFileCount = 0;
                    const fs::path extractedRoot = caseDir / "ExtractedSpotlight" / "aff4_apfs";

                    // V0_8_68: prefer dissect.apfs-style exact stream objects and avoid staging
                    // shifted/private-id false positives when the decoded inode parent does not
                    // match the directory record parent for a Store-V2 target. These maps also
                    // support extent candidate scoring below.
                    std::map<std::uint32_t, bool> inodeParentMatchesByTargetSequence;
                    std::map<std::uint32_t, bool> inodeParentMismatchByTargetSequence;
                    for (const auto& ir : apfsSpotlightInodeProbeRows) {
                        if (ir.inodeStatus != "TARGET_INODE_GUIDED_LOOKUP_HIT") continue;
                        if (ir.targetSequence == 0 || ir.targetChildFileId == 0 || ir.inodeObjectId != ir.targetChildFileId) continue;
                        if (ir.inodeParentId == ir.targetParentObjectId) inodeParentMatchesByTargetSequence[ir.targetSequence] = true;
                        else inodeParentMismatchByTargetSequence[ir.targetSequence] = true;
                    }

                    auto hasConfirmedParentMismatch = [&](std::uint32_t targetSequence) -> bool {
                        const bool matched = inodeParentMatchesByTargetSequence.find(targetSequence) != inodeParentMatchesByTargetSequence.end();
                        const bool mismatched = inodeParentMismatchByTargetSequence.find(targetSequence) != inodeParentMismatchByTargetSequence.end();
                        return mismatched && !matched;
                    };

                    auto extentCandidateScore = [&](const ApfsSpotlightCopyAttemptRow& cr, const ApfsSpotlightFileExtentProbeRow& er) -> long long {
                        long long score = 0;
                        if (er.extentStatus == "TARGET_FILE_EXTENT_CANDIDATE") score += 10000;
                        if (er.physicalBlock != 0 && er.physicalOffset != 0 && er.extentLengthBytes != 0) score += 5000; else score -= 20000;
                        if (er.extentFileId == cr.childFileId) score += 9000;
                        if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM" && er.notes.find("candidate=xattr_dstream_object_id") != std::string::npos) score += 12000;
                        if (er.notes.find("candidate=child_file_id_raw") != std::string::npos) score += 7000;
                        if (er.notes.find("candidate=inode_private_id_dstream_candidate") != std::string::npos) score += 2500;
                        if (er.notes.find("_shifted_right_4") != std::string::npos) score -= 7000;
                        if (er.notes.find("high_prefix_") != std::string::npos) score -= 9000;
                        score += static_cast<long long>(std::min<std::uint64_t>(er.extentLengthBytes, 64ULL * 1024ULL * 1024ULL) / 4096ULL);
                        return score;
                    };

                    auto normalizeExtentsForCopyAttempt = [&](const ApfsSpotlightCopyAttemptRow& cr, std::vector<ApfsSpotlightFileExtentProbeRow>& extents) {
                        std::map<std::uint64_t, ApfsSpotlightFileExtentProbeRow> bestByOffset;
                        for (const auto& er : extents) {
                            if (er.extentStatus != "TARGET_FILE_EXTENT_CANDIDATE") continue;
                            auto it = bestByOffset.find(er.extentLogicalOffset);
                            if (it == bestByOffset.end() || extentCandidateScore(cr, er) > extentCandidateScore(cr, it->second) ||
                                (extentCandidateScore(cr, er) == extentCandidateScore(cr, it->second) && er.extentLengthBytes > it->second.extentLengthBytes)) {
                                bestByOffset[er.extentLogicalOffset] = er;
                            }
                        }
                        extents.clear();
                        for (const auto& kv : bestByOffset) extents.push_back(kv.second);
                    };

                    for (const auto& cr : apfsSpotlightCopyAttemptRows) {
                        if (cr.childFileId == 0) continue;
                        if (!(cr.extractionStatus == "COPY_NOT_ATTEMPTED_EXTENTS_FOUND_FULL_ASSEMBLY_NOT_READY" || cr.extractionStatus == "COPY_NOT_ATTEMPTED_FILE_EXTENTS_NOT_RESOLVED")) continue;
                        std::vector<ApfsSpotlightFileExtentProbeRow> extentsForTarget;
                        for (const auto& er : apfsSpotlightFileExtentProbeRows) {
                            if (er.targetSequence == cr.sequence && er.targetChildFileId == cr.childFileId && er.extentStatus == "TARGET_FILE_EXTENT_CANDIDATE") extentsForTarget.push_back(er);
                        }
                        if (extentsForTarget.empty()) continue;
                        normalizeExtentsForCopyAttempt(cr, extentsForTarget);
                        if (cr.targetKind != "APFS_RESOURCE_FORK_STREAM" && hasConfirmedParentMismatch(cr.sequence)) {
                            appendCopyOutRow(cr, extentsForTarget, "SKIPPED_INODE_PARENT_MISMATCH", "INODE_PARENT_MISMATCH", "Decoded inode parent_id did not match the directory-record parent for this Store-V2 target; skipped to avoid staging shifted/private-id false-positive APFS extents.");
                            continue;
                        }

                        auto rebuildExtentsForCurrentTarget = [&]() {
                            extentsForTarget.clear();
                            for (const auto& er : apfsSpotlightFileExtentProbeRows) {
                                if (er.targetSequence == cr.sequence && er.targetChildFileId == cr.childFileId && er.extentStatus == "TARGET_FILE_EXTENT_CANDIDATE") extentsForTarget.push_back(er);
                            }
                            normalizeExtentsForCopyAttempt(cr, extentsForTarget);
                        };

                        auto computeContiguousExtentBytes = [&]() -> std::uint64_t {
                            std::uint64_t expectedLocal = 0;
                            for (const auto& er : extentsForTarget) {
                                if (er.extentLogicalOffset != expectedLocal || er.extentLengthBytes == 0) break;
                                if (er.extentLengthBytes > std::numeric_limits<std::uint64_t>::max() - expectedLocal) break;
                                expectedLocal += er.extentLengthBytes;
                            }
                            return expectedLocal;
                        };

                        const auto desiredLogicalItForProbe = spotlightLogicalSizeByTargetSequence.find(cr.sequence);
                        const std::uint64_t desiredLogicalSizeForProbe = (desiredLogicalItForProbe == spotlightLogicalSizeByTargetSequence.end()) ? 0ULL : desiredLogicalItForProbe->second;
                        if (desiredLogicalSizeForProbe != 0 && desiredLogicalSizeForProbe <= kMaxSingleCopyOutBytes) {
                            std::uint64_t contiguousBefore = computeContiguousExtentBytes();
                            if (contiguousBefore != 0 && contiguousBefore < desiredLogicalSizeForProbe) {
                                std::vector<std::pair<std::uint64_t, std::string>> candidatesForProbe;
                                if (cr.targetKind == "APFS_RESOURCE_FORK_STREAM") {
                                    candidatesForProbe.push_back(std::make_pair(cr.childFileId, "xattr_dstream_object_id"));
                                } else {
                                    const auto privateItForProbe = spotlightPrivateIdByTargetSequence.find(cr.sequence);
                                    const std::uint64_t privateIdForProbe = (privateItForProbe == spotlightPrivateIdByTargetSequence.end()) ? 0ULL : privateItForProbe->second;
                                    candidatesForProbe = guidedObjectCandidatesForChild(cr.childFileId, privateIdForProbe);
                                }
                                std::set<std::uint64_t> requestedOffsets;
                                constexpr std::uint32_t kMaxAdditionalExtentOffsetProbes = 8192U;
                                std::uint32_t additionalProbeCount = 0;
                                while (contiguousBefore < desiredLogicalSizeForProbe && additionalProbeCount < kMaxAdditionalExtentOffsetProbes) {
                                    const std::uint64_t requestOffset = contiguousBefore;
                                    if (!requestedOffsets.insert(requestOffset).second) break;
                                    const std::size_t beforeRows = apfsSpotlightFileExtentProbeRows.size();
                                    const std::size_t beforeScanRows = allSpotlightScanFileExtents.size();
                                    for (const auto& cand : candidatesForProbe) {
                                        lookupGuidedFileExtentCandidate(cr, cand.first, cand.second, requestOffset);
                                    }
                                    // V0_8_62: guided follow-up FILE_EXTENT probes append to the
                                    // scan-level extent vector. Materialize newly discovered extents
                                    // immediately into the copy-out candidate rows; otherwise the
                                    // copy-out loop still sees only the first extent and large Store-V2
                                    // files remain truncated to their first block/extent.
                                    for (std::size_t scanIdx = beforeScanRows; scanIdx < allSpotlightScanFileExtents.size(); ++scanIdx) {
                                        materializeGuidedExtentForCopyAttempt(cr, allSpotlightScanFileExtents[scanIdx]);
                                    }
                                    ++additionalProbeCount;
                                    rebuildExtentsForCurrentTarget();
                                    const std::uint64_t contiguousAfter = computeContiguousExtentBytes();
                                    if (contiguousAfter <= contiguousBefore || apfsSpotlightFileExtentProbeRows.size() == beforeRows) break;
                                    contiguousBefore = contiguousAfter;
                                }
                            }
                        }

                        const auto desiredLogicalItForCopyRange = spotlightLogicalSizeByTargetSequence.find(cr.sequence);
                        const std::uint64_t desiredLogicalSizeForCopyRange = (desiredLogicalItForCopyRange == spotlightLogicalSizeByTargetSequence.end()) ? 0ULL : desiredLogicalItForCopyRange->second;
                        if (desiredLogicalSizeForCopyRange != 0 && desiredLogicalSizeForCopyRange <= kMaxSingleCopyOutBytes) {
                            // Follow the dissect.apfs FileStream read model: only require the extents needed
                            // to cover the requested logical stream size. Guided lookup may discover speculative
                            // or duplicate later extents; those must not make a valid bounded read appear non-gapless.
                            std::vector<ApfsSpotlightFileExtentProbeRow> neededExtents;
                            std::uint64_t coverage = 0;
                            for (const auto& er : extentsForTarget) {
                                if (coverage >= desiredLogicalSizeForCopyRange) break;
                                if (er.extentLogicalOffset != coverage) break;
                                if (er.extentLengthBytes == 0) break;
                                neededExtents.push_back(er);
                                if (er.extentLengthBytes > std::numeric_limits<std::uint64_t>::max() - coverage) break;
                                coverage += er.extentLengthBytes;
                            }
                            if (!neededExtents.empty()) {
                                extentsForTarget.swap(neededExtents);
                            }
                        }

                        bool hasSparseGap = false;
                        bool hasZeroPhysicalExtent = false;
                        bool hasOverlapOrInvalidOrder = false;
                        bool saneLengths = true;
                        std::uint64_t expected = 0;
                        std::uint64_t syntheticZeroBytesPlanned = 0;
                        for (const auto& er : extentsForTarget) {
                            if (er.extentLengthBytes == 0 || er.extentLengthBytes > kMaxSingleCopyOutBytes) saneLengths = false;
                            if (er.extentLogicalOffset < expected) hasOverlapOrInvalidOrder = true;
                            if (er.extentLogicalOffset > expected) {
                                hasSparseGap = true;
                                syntheticZeroBytesPlanned += (er.extentLogicalOffset - expected);
                                expected = er.extentLogicalOffset;
                            }
                            if (er.physicalBlock == 0 || er.physicalOffset == 0) {
                                hasZeroPhysicalExtent = true;
                                syntheticZeroBytesPlanned += er.extentLengthBytes;
                            }
                            if (er.extentLengthBytes > std::numeric_limits<std::uint64_t>::max() - expected) saneLengths = false;
                            expected += er.extentLengthBytes;
                        }
                        if (hasOverlapOrInvalidOrder) { appendCopyOutRow(cr, extentsForTarget, "SKIPPED_OVERLAPPING_OR_OUT_OF_ORDER_EXTENTS", "OVERLAP_ORDER_GATE_FAILED", "Logical FILE_EXTENT rows overlap or move backwards; extraction is deferred to avoid producing shifted data."); continue; }
                        if (!saneLengths || expected == 0 || expected > kMaxSingleCopyOutBytes) { appendCopyOutRow(cr, extentsForTarget, "SKIPPED_SIZE_LIMIT_OR_INVALID_LENGTH", "SIZE_GATE_FAILED", "The extent chain length is zero, invalid, or exceeds the bounded per-file copy-out limit."); continue; }

                        std::uint64_t logicalSize = expected;
                        std::string logicalSizeSource = "extent_chain_allocated_size";
                        std::uint64_t decodedLogicalSize = 0;
                        std::string decodedLogicalSizeSource;
                        bool decodedLogicalExceedsExtentChain = false;
                        auto logicalIt = spotlightLogicalSizeByTargetSequence.find(cr.sequence);
                        if (logicalIt != spotlightLogicalSizeByTargetSequence.end() && logicalIt->second != 0) {
                            decodedLogicalSize = logicalIt->second;
                            auto srcIt = spotlightLogicalSizeSourceByTargetSequence.find(cr.sequence);
                            decodedLogicalSizeSource = (srcIt != spotlightLogicalSizeSourceByTargetSequence.end()) ? srcIt->second : "INO_EXT_TYPE_DSTREAM.size";
                            if (decodedLogicalSize <= expected) {
                                logicalSize = decodedLogicalSize;
                                logicalSizeSource = decodedLogicalSizeSource;
                            } else if (decodedLogicalSize <= kMaxSingleCopyOutBytes) {
                                // V0_8_62: Many APFS Store-V2 Cache/*.txt rows are compressed/resource-fork
                                // candidates. The inode carries j_inode_val.uncompressed_size, but the normal
                                // FILE_EXTENT chain only exposes a short data fork. Copy the validated data-fork
                                // bytes for diagnostics, but mark the row as partial instead of claiming that the
                                // logical file was fully copied. Full fidelity requires APFS xattr/resource-fork
                                // decompression support.
                                decodedLogicalExceedsExtentChain = true;
                                logicalSize = expected;
                                logicalSizeSource = decodedLogicalSizeSource + "_exceeds_extent_chain";
                            }
                        }
                        if (logicalSize == 0 || logicalSize > expected) { appendCopyOutRow(cr, extentsForTarget, "SKIPPED_LOGICAL_SIZE_INVALID", "LOGICAL_SIZE_GATE_FAILED", "Decoded logical file size was zero or larger than assembled extent bytes."); continue; }

                        fs::path volumeDir = extractedRoot / safeExtractionFileName(cr.volumeSequence, cr.fsOid, cr.volumeName);
                        std::error_code mkEc;
                        fs::create_directories(volumeDir, mkEc);
                        if (mkEc) { appendCopyOutRow(cr, extentsForTarget, "COPY_FAILED_CREATE_DIRECTORY", "OUTPUT_DIRECTORY_FAILED", mkEc.message()); continue; }
                        fs::path outPath = volumeDir / safeExtractionFileName(cr.sequence, cr.childFileId, cr.targetName);
                        std::ofstream outFile(outPath, std::ios::binary | std::ios::trunc);
                        if (!outFile) { appendCopyOutRow(cr, extentsForTarget, "COPY_FAILED_OPEN_OUTPUT", "OUTPUT_OPEN_FAILED", pathString(outPath)); continue; }

                        bool copyOk = true;
                        std::string copyNotes;
                        std::vector<unsigned char> firstBytes;
                        std::uint64_t remainingToWrite = logicalSize;
                        std::uint64_t logicalCursor = 0;
                        std::uint64_t syntheticZeroBytesWritten = 0;
                        auto writeZeroRegion = [&](std::uint64_t count, const std::string& reason) -> bool {
                            static const std::vector<unsigned char> zeros(1024 * 1024, 0);
                            std::uint64_t left = count;
                            while (left != 0) {
                                const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(left, zeros.size()));
                                if (firstBytes.empty()) firstBytes.assign(zeros.begin(), zeros.begin() + std::min<std::size_t>(chunk, 96U));
                                outFile.write(reinterpret_cast<const char*>(zeros.data()), static_cast<std::streamsize>(chunk));
                                if (!outFile) { copyNotes = "write failed while zero-filling " + reason; return false; }
                                left -= chunk;
                            }
                            syntheticZeroBytesWritten += count;
                            return true;
                        };
                        for (const auto& er : extentsForTarget) {
                            if (remainingToWrite == 0) break;
                            if (er.extentLogicalOffset > logicalCursor) {
                                const std::uint64_t gap = std::min<std::uint64_t>(er.extentLogicalOffset - logicalCursor, remainingToWrite);
                                if (!writeZeroRegion(gap, "logical_sparse_gap")) { copyOk = false; break; }
                                logicalCursor += gap;
                                remainingToWrite -= gap;
                                if (remainingToWrite == 0) break;
                            }
                            const std::uint64_t bytesWanted = std::min<std::uint64_t>(er.extentLengthBytes, remainingToWrite);
                            if (er.physicalBlock == 0 || er.physicalOffset == 0) {
                                if (!writeZeroRegion(bytesWanted, "zero_physical_block")) { copyOk = false; break; }
                                logicalCursor += bytesWanted;
                                remainingToWrite -= bytesWanted;
                                continue;
                            }
                            std::vector<unsigned char> extentBytes;
                            std::string readErr;
                            const long long bytesReadExtent = readVirtual(er.physicalOffset, bytesWanted, extentBytes, readErr);
                            if (bytesReadExtent < 0 || static_cast<std::uint64_t>(bytesReadExtent) != bytesWanted || extentBytes.size() != bytesWanted) {
                                copyOk = false;
                                copyNotes = readErr.empty() ? "extent read failed or returned short data" : readErr;
                                break;
                            }
                            if (firstBytes.empty()) firstBytes.assign(extentBytes.begin(), extentBytes.begin() + std::min<std::size_t>(extentBytes.size(), 96U));
                            outFile.write(reinterpret_cast<const char*>(extentBytes.data()), static_cast<std::streamsize>(extentBytes.size()));
                            if (!outFile) { copyOk = false; copyNotes = "write failed while copying extent data"; break; }
                            logicalCursor += bytesWanted;
                            remainingToWrite -= bytesWanted;
                        }
                        if (copyOk && remainingToWrite != 0) { copyOk = false; copyNotes = "logical file size was not fully written from available extents"; }
                        outFile.close();
                        ApfsSpotlightFileCopyOutRow row;
                        row.sequence = static_cast<std::uint32_t>(apfsSpotlightFileCopyOutRows.size());
                        row.targetSequence = cr.sequence;
                        row.volumeSequence = cr.volumeSequence;
                        row.targetRole = cr.targetRole;
                        row.fsOid = cr.fsOid;
                        row.volumeName = cr.volumeName;
                        row.targetParentObjectId = cr.parentObjectId;
                        row.targetChildFileId = cr.childFileId;
                        row.targetName = cr.targetName;
                        row.targetKind = cr.targetKind;
                        row.storeV2RootObjectId = cr.storeV2RootObjectId;
                        row.storeV2GroupName = cr.storeV2GroupName;
                        row.storeV2RelativePath = cr.storeV2RelativePath;
                        row.extentCount = static_cast<std::uint32_t>(extentsForTarget.size());
                        row.assembledBytes = expected;
                        row.logicalSizeBytes = decodedLogicalExceedsExtentChain ? decodedLogicalSize : logicalSize;
                        row.logicalSizeSource = logicalSizeSource;
                        row.firstPhysicalOffset = extentsForTarget.empty() ? 0ULL : extentsForTarget.front().physicalOffset;
                        row.outputPath = pathString(outPath);
                        row.outputRelativePath = pathString(fs::relative(outPath, caseDir));
                        if (!firstBytes.empty()) {
                            row.firstBytesStatus = directPreviewStatusForBytes(firstBytes);
                            row.firstBytesHex = hexSampleBytes(firstBytes.data(), firstBytes.size());
                        } else {
                            row.firstBytesStatus = "NO_PREVIEW";
                        }
                        if (!copyOk) {
                            row.copyStatus = "COPY_FAILED_READ_OR_WRITE";
                            row.validationStatus = "FAILED_DURING_COPY";
                            row.notes = copyNotes;
                            std::error_code rmEc; fs::remove(outPath, rmEc);
                        } else {
                            std::error_code sizeEc;
                            const auto sz = fs::file_size(outPath, sizeEc);
                            row.outputSizeBytes = sizeEc ? 0ULL : static_cast<std::uint64_t>(sz);
                            try { row.outputSha256 = sha256File(outPath); } catch (const std::exception& ex) { row.notes = std::string("sha256_failed: ") + ex.what(); }
                            if (decodedLogicalExceedsExtentChain) {
                                row.copyStatus = "COPIED_PARTIAL_COMPRESSED_OR_RSRC_FORK_CANDIDATE";
                                row.validationStatus = "PARTIAL_LOGICAL_SIZE_EXCEEDS_EXTENT_CHAIN";
                                row.notes = "decoded_logical_size=" + std::to_string(decodedLogicalSize) + "; assembled_extent_bytes=" + std::to_string(expected) + "; source=" + decodedLogicalSizeSource + "; APFS compressed/resource-fork/xattr handling is required for full logical-file reconstruction";
                                row.interpretation = "APFS FILE_EXTENT rows copied a validated data-fork extent chain, but the decoded inode logical/uncompressed size is larger than the assembled normal extents. Treat this as a compressed/resource-fork candidate, not a complete logical file copy.";
                            } else {
                                if (hasSparseGap || hasZeroPhysicalExtent || syntheticZeroBytesWritten != 0) {
                                    row.copyStatus = "COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS";
                                    row.validationStatus = (row.outputSizeBytes == logicalSize) ? "SIZE_MATCH_WITH_ZERO_FILL_PROVENANCE" : "SIZE_MISMATCH_AFTER_ZERO_FILL_COPY";
                                    row.notes += (row.notes.empty() ? std::string{} : "; ") + std::string("synthetic_zero_bytes=") + std::to_string(syntheticZeroBytesWritten) + "; planned_zero_bytes=" + std::to_string(syntheticZeroBytesPlanned) + "; sparse_gap=" + (hasSparseGap ? "true" : "false") + "; zero_physical_extent=" + (hasZeroPhysicalExtent ? "true" : "false");
                                    row.interpretation = "APFS FILE_EXTENT rows were assembled with explicit synthetic zero regions for sparse gaps and/or zero physical extents. The zero-filled regions are recorded in notes and the output hash is calculated over the reconstructed logical byte stream.";
                                } else {
                                    row.copyStatus = "COPIED_GAPLESS_EXTENT_CHAIN";
                                    row.validationStatus = (row.outputSizeBytes == logicalSize && logicalSize < expected) ? "TRIMMED_TO_INODE_LOGICAL_SIZE" : ((row.outputSizeBytes == expected) ? "SIZE_MATCHES_EXTENT_CHAIN" : "SIZE_MISMATCH_AFTER_COPY");
                                    row.interpretation = "APFS FILE_EXTENT rows formed a gapless nonzero physical chain and were assembled into a staged copy-out file with inode/dstream-aware logical-size validation.";
                                }
                            }
                            ++copiedOutFileCount;
                        }
                        apfsSpotlightFileCopyOutRows.push_back(row);
                    }

                    // V0_8_68: reconstruct decmpfs zlib/plain files whose normal data fork was only a
                    // small partial copy but whose stream-backed com.apple.ResourceFork was copied successfully.
                    // The reconstructed logical file is added as a separate copy-out row with the original
                    // Store-V2 relative path so staging can prefer it over the partial data-fork diagnostic row.
                    {
                        struct DecmpfsInfo { int compressionType = 0; std::uint64_t uncompressedSize = 0; };
                        std::map<std::uint64_t, DecmpfsInfo> decmpfsInfoByFileId;
                        for (const auto& xr : apfsSpotlightXattrProbeRows) {
                            if (asciiLower(xr.xattrName) != "com.apple.decmpfs") continue;
                            const int ctype = decmpfsCompressionTypeFromPreviewHex(xr.xdataPreviewHex);
                            const std::uint64_t usize = decmpfsUncompressedSizeFromPreviewHex(xr.xdataPreviewHex);
                            if (ctype != 0 && usize != 0) decmpfsInfoByFileId[xr.fileObjectId] = DecmpfsInfo{ctype, usize};
                        }

                        std::map<std::uint64_t, ApfsSpotlightFileCopyOutRow> bestResourceForkRowByOriginalFileId;
                        for (const auto& r : apfsSpotlightFileCopyOutRows) {
                            if (r.targetKind != "APFS_RESOURCE_FORK_STREAM") continue;
                            if (r.copyStatus != "COPIED_GAPLESS_EXTENT_CHAIN") continue;
                            if (r.outputPath.empty() || r.outputSizeBytes == 0) continue;
                            auto& cur = bestResourceForkRowByOriginalFileId[r.targetParentObjectId];
                            if (cur.outputPath.empty() || r.outputSizeBytes > cur.outputSizeBytes) cur = r;
                        }

                        auto readFileBytesBounded = [](const fs::path& path, std::uint64_t maxBytes, std::vector<unsigned char>& out, std::string& err) -> bool {
                            std::error_code ec;
                            const auto sz = fs::file_size(path, ec);
                            if (ec) { err = ec.message(); return false; }
                            if (static_cast<std::uint64_t>(sz) > maxBytes) { err = "file exceeds bounded read cap"; return false; }
                            std::ifstream in(path, std::ios::binary);
                            if (!in) { err = "unable to open file"; return false; }
                            out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                            return true;
                        };

                        const std::size_t initialCopyRowCount = apfsSpotlightFileCopyOutRows.size();
                        std::size_t reconstructedCount = 0;
                        std::size_t reconstructionAttemptCount = 0;
                        constexpr std::size_t kMaxDecmpfsReconstructions = 30000U;
                        for (std::size_t i = 0; i < initialCopyRowCount && reconstructionAttemptCount < kMaxDecmpfsReconstructions; ++i) {
                            const auto base = apfsSpotlightFileCopyOutRows[i];
                            if (base.copyStatus != "COPIED_PARTIAL_COMPRESSED_OR_RSRC_FORK_CANDIDATE") continue;
                            if (base.storeV2RelativePath.empty() || base.targetChildFileId == 0) continue;
                            const auto infoIt = decmpfsInfoByFileId.find(base.targetChildFileId);
                            if (infoIt == decmpfsInfoByFileId.end()) continue;
                            const auto rfIt = bestResourceForkRowByOriginalFileId.find(base.targetChildFileId);
                            if (rfIt == bestResourceForkRowByOriginalFileId.end()) continue;
                            ++reconstructionAttemptCount;

                            ApfsSpotlightFileCopyOutRow outRow = base;
                            outRow.sequence = static_cast<std::uint32_t>(apfsSpotlightFileCopyOutRows.size());
                            outRow.assembledBytes = rfIt->second.outputSizeBytes;
                            outRow.extentCount = rfIt->second.extentCount;
                            outRow.firstPhysicalOffset = rfIt->second.firstPhysicalOffset;
                            outRow.logicalSizeBytes = infoIt->second.uncompressedSize;
                            outRow.logicalSizeSource = "com.apple.decmpfs." + decmpfsCompressionTypeLabel(infoIt->second.compressionType) + ".resource_fork";
                            outRow.outputPath.clear();
                            outRow.outputRelativePath.clear();
                            outRow.outputSizeBytes = 0;
                            outRow.outputSha256.clear();
                            outRow.firstBytesStatus = "NO_PREVIEW";
                            outRow.firstBytesHex.clear();
                            outRow.copyStatus = "SKIPPED_DECOMPFS_RESOURCE_FORK_RECONSTRUCTION_FAILED";
                            outRow.validationStatus = "DECOMPFS_RECONSTRUCTION_FAILED";
                            outRow.interpretation = "Attempted APFS decmpfs resource-fork reconstruction for a partial Store-V2 file using the dissect.apfs resource-fork table model.";

                            std::vector<unsigned char> resourceBytes;
                            std::string readErr;
                            if (!readFileBytesBounded(fs::path(rfIt->second.outputPath), 512ULL * 1024ULL * 1024ULL, resourceBytes, readErr)) {
                                outRow.notes = "resource_fork_read_failed=" + readErr + "; resource_row_sequence=" + std::to_string(rfIt->second.sequence);
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            const auto recon = reconstructDecmpfsResourceForkDissectStyle(resourceBytes, infoIt->second.uncompressedSize, infoIt->second.compressionType);
                            if (!recon.ok) {
                                outRow.notes = "resource_row_sequence=" + std::to_string(rfIt->second.sequence) + "; status=" + recon.status + "; " + recon.notes;
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }

                            fs::path volumeDir = extractedRoot / safeExtractionFileName(base.volumeSequence, base.fsOid, base.volumeName);
                            std::error_code mkEc;
                            fs::create_directories(volumeDir, mkEc);
                            if (mkEc) {
                                outRow.notes = "reconstruction output directory failed: " + mkEc.message();
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            fs::path outPath = volumeDir / safeExtractionFileName(outRow.sequence, base.targetChildFileId, base.targetName + ".__DECOMPFS_RECONSTRUCTED");
                            std::ofstream outFile(outPath, std::ios::binary | std::ios::trunc);
                            if (!outFile) {
                                outRow.notes = "reconstruction output open failed: " + pathString(outPath);
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            outFile.write(reinterpret_cast<const char*>(recon.data.data()), static_cast<std::streamsize>(recon.data.size()));
                            outFile.close();
                            if (!outFile) {
                                outRow.notes = "reconstruction output write failed: " + pathString(outPath);
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            std::error_code sizeEc;
                            const auto sz = fs::file_size(outPath, sizeEc);
                            outRow.outputPath = pathString(outPath);
                            outRow.outputRelativePath = pathString(fs::relative(outPath, caseDir));
                            outRow.outputSizeBytes = sizeEc ? 0ULL : static_cast<std::uint64_t>(sz);
                            try { outRow.outputSha256 = sha256File(outPath); } catch (const std::exception& ex) { outRow.notes = std::string("sha256_failed: ") + ex.what(); }
                            if (!recon.data.empty()) {
                                const std::size_t previewLen = std::min<std::size_t>(recon.data.size(), 96U);
                                std::vector<unsigned char> preview(recon.data.begin(), recon.data.begin() + static_cast<std::ptrdiff_t>(previewLen));
                                outRow.firstBytesStatus = previewStatusForBytes(preview);
                                outRow.firstBytesHex = hexSampleBytes(preview.data(), preview.size());
                            }
                            if (infoIt->second.compressionType == 4) outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_ZLIB";
                            else if (infoIt->second.compressionType == 8) outRow.copyStatus = appleLzfseCodecAvailable() ? "COPIED_DECOMPFS_RESOURCE_FORK_LZVN" : "COPIED_DECOMPFS_RESOURCE_FORK_LZVN_UNCOMPRESSED_MARKERS";
                            else if (infoIt->second.compressionType == 12) outRow.copyStatus = appleLzfseCodecAvailable() ? "COPIED_DECOMPFS_RESOURCE_FORK_LZFSE" : "COPIED_DECOMPFS_RESOURCE_FORK_LZFSE_UNCOMPRESSED_MARKERS";
                            else if (infoIt->second.compressionType == 14) outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_LZBITMAP_UNCOMPRESSED_MARKERS";
                            else outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_PLAIN";
                            const std::string algLabel = decmpfsCompressionTypeLabel(infoIt->second.compressionType);
                            outRow.validationStatus = (outRow.outputSizeBytes == infoIt->second.uncompressedSize) ? ("DECOMPFS_RESOURCE_FORK_" + algLabel + "_SIZE_MATCH") : ("DECOMPFS_RESOURCE_FORK_" + algLabel + "_SIZE_MISMATCH");
                            outRow.interpretation = "APFS decmpfs resource-fork data was reconstructed from the copied com.apple.ResourceFork stream and staged as the original logical Store-V2 file.";
                            outRow.notes = "resource_row_sequence=" + std::to_string(rfIt->second.sequence) + "; " + recon.notes;
                            apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                            ++reconstructedCount;
                        }
                        // V0_8_68: also reconstruct when the original data-fork row was not copied as
                        // a partial candidate. This follows dissect.apfs DecmpfsStream more closely: the
                        // logical file can be reconstructed directly from com.apple.decmpfs plus the
                        // stream-backed com.apple.ResourceFork FileStream.
                        std::map<std::uint64_t, const ApfsSpotlightCopyAttemptRow*> originalAttemptByFileId;
                        for (const auto& cr0 : apfsSpotlightCopyAttemptRows) {
                            if (cr0.targetKind == "APFS_RESOURCE_FORK_STREAM") continue;
                            if (cr0.childFileId == 0 || cr0.storeV2RelativePath.empty()) continue;
                            if (decmpfsInfoByFileId.find(cr0.childFileId) == decmpfsInfoByFileId.end()) continue;
                            auto it0 = originalAttemptByFileId.find(cr0.childFileId);
                            if (it0 == originalAttemptByFileId.end() || (inodeParentMatchesByTargetSequence.find(cr0.sequence) != inodeParentMatchesByTargetSequence.end())) {
                                originalAttemptByFileId[cr0.childFileId] = &cr0;
                            }
                        }
                        std::set<std::uint64_t> alreadyReconstructedFileIds;
                        for (const auto& r0 : apfsSpotlightFileCopyOutRows) {
                            if (r0.copyStatus.rfind("COPIED_DECOMPFS_RESOURCE_FORK", 0) == 0) alreadyReconstructedFileIds.insert(r0.targetChildFileId);
                        }
                        for (const auto& kvBase : originalAttemptByFileId) {
                            if (reconstructionAttemptCount >= kMaxDecmpfsReconstructions) break;
                            const std::uint64_t originalFileId = kvBase.first;
                            if (alreadyReconstructedFileIds.count(originalFileId) != 0) continue;
                            const auto infoIt = decmpfsInfoByFileId.find(originalFileId);
                            if (infoIt == decmpfsInfoByFileId.end()) continue;
                            if (!(infoIt->second.compressionType == 4 || infoIt->second.compressionType == 8 || infoIt->second.compressionType == 10 || infoIt->second.compressionType == 12 || infoIt->second.compressionType == 14)) continue;
                            const auto rfIt = bestResourceForkRowByOriginalFileId.find(originalFileId);
                            if (rfIt == bestResourceForkRowByOriginalFileId.end()) continue;
                            ++reconstructionAttemptCount;

                            const auto& crBase = *kvBase.second;
                            ApfsSpotlightFileCopyOutRow outRow;
                            outRow.sequence = static_cast<std::uint32_t>(apfsSpotlightFileCopyOutRows.size());
                            outRow.targetSequence = crBase.sequence;
                            outRow.volumeSequence = crBase.volumeSequence;
                            outRow.targetRole = crBase.targetRole;
                            outRow.fsOid = crBase.fsOid;
                            outRow.volumeName = crBase.volumeName;
                            outRow.targetParentObjectId = crBase.parentObjectId;
                            outRow.targetChildFileId = crBase.childFileId;
                            outRow.targetName = crBase.targetName;
                            outRow.targetKind = crBase.targetKind;
                            outRow.storeV2RootObjectId = crBase.storeV2RootObjectId;
                            outRow.storeV2GroupName = crBase.storeV2GroupName;
                            outRow.storeV2RelativePath = crBase.storeV2RelativePath;
                            outRow.assembledBytes = rfIt->second.outputSizeBytes;
                            outRow.extentCount = rfIt->second.extentCount;
                            outRow.firstPhysicalOffset = rfIt->second.firstPhysicalOffset;
                            outRow.logicalSizeBytes = infoIt->second.uncompressedSize;
                            outRow.logicalSizeSource = "com.apple.decmpfs." + decmpfsCompressionTypeLabel(infoIt->second.compressionType) + ".resource_fork.synthetic_base";
                            outRow.copyStatus = "SKIPPED_DECOMPFS_RESOURCE_FORK_RECONSTRUCTION_FAILED";
                            outRow.validationStatus = "DECOMPFS_RECONSTRUCTION_FAILED";
                            outRow.interpretation = "Attempted APFS decmpfs resource-fork reconstruction directly from com.apple.decmpfs metadata and com.apple.ResourceFork stream, without requiring a prior partial data-fork copy row.";

                            std::vector<unsigned char> resourceBytes;
                            std::string readErr;
                            if (!readFileBytesBounded(fs::path(rfIt->second.outputPath), 512ULL * 1024ULL * 1024ULL, resourceBytes, readErr)) {
                                outRow.notes = "synthetic_base=1; resource_fork_read_failed=" + readErr + "; resource_row_sequence=" + std::to_string(rfIt->second.sequence);
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            const auto recon = reconstructDecmpfsResourceForkDissectStyle(resourceBytes, infoIt->second.uncompressedSize, infoIt->second.compressionType);
                            if (!recon.ok) {
                                outRow.notes = "synthetic_base=1; resource_row_sequence=" + std::to_string(rfIt->second.sequence) + "; status=" + recon.status + "; " + recon.notes;
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            fs::path volumeDir = extractedRoot / safeExtractionFileName(crBase.volumeSequence, crBase.fsOid, crBase.volumeName);
                            std::error_code mkEc2;
                            fs::create_directories(volumeDir, mkEc2);
                            if (mkEc2) {
                                outRow.notes = "synthetic_base=1; reconstruction output directory failed: " + mkEc2.message();
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            fs::path outPath = volumeDir / safeExtractionFileName(outRow.sequence, crBase.childFileId, crBase.targetName + ".__DECOMPFS_RECONSTRUCTED");
                            std::ofstream outFile(outPath, std::ios::binary | std::ios::trunc);
                            if (!outFile) {
                                outRow.notes = "synthetic_base=1; reconstruction output open failed: " + pathString(outPath);
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            outFile.write(reinterpret_cast<const char*>(recon.data.data()), static_cast<std::streamsize>(recon.data.size()));
                            outFile.close();
                            if (!outFile) {
                                outRow.notes = "synthetic_base=1; reconstruction output write failed: " + pathString(outPath);
                                apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                                continue;
                            }
                            std::error_code sizeEc2;
                            const auto sz2 = fs::file_size(outPath, sizeEc2);
                            outRow.outputPath = pathString(outPath);
                            outRow.outputRelativePath = pathString(fs::relative(outPath, caseDir));
                            outRow.outputSizeBytes = sizeEc2 ? 0ULL : static_cast<std::uint64_t>(sz2);
                            try { outRow.outputSha256 = sha256File(outPath); } catch (const std::exception& ex) { outRow.notes = std::string("synthetic_base=1; sha256_failed: ") + ex.what(); }
                            if (!recon.data.empty()) {
                                const std::size_t previewLen = std::min<std::size_t>(recon.data.size(), 96U);
                                std::vector<unsigned char> preview(recon.data.begin(), recon.data.begin() + static_cast<std::ptrdiff_t>(previewLen));
                                outRow.firstBytesStatus = previewStatusForBytes(preview);
                                outRow.firstBytesHex = hexSampleBytes(preview.data(), preview.size());
                            } else { outRow.firstBytesStatus = "NO_PREVIEW"; }
                            if (infoIt->second.compressionType == 4) outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_ZLIB";
                            else if (infoIt->second.compressionType == 8) outRow.copyStatus = appleLzfseCodecAvailable() ? "COPIED_DECOMPFS_RESOURCE_FORK_LZVN" : "COPIED_DECOMPFS_RESOURCE_FORK_LZVN_UNCOMPRESSED_MARKERS";
                            else if (infoIt->second.compressionType == 12) outRow.copyStatus = appleLzfseCodecAvailable() ? "COPIED_DECOMPFS_RESOURCE_FORK_LZFSE" : "COPIED_DECOMPFS_RESOURCE_FORK_LZFSE_UNCOMPRESSED_MARKERS";
                            else if (infoIt->second.compressionType == 14) outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_LZBITMAP_UNCOMPRESSED_MARKERS";
                            else outRow.copyStatus = "COPIED_DECOMPFS_RESOURCE_FORK_PLAIN";
                            const std::string algLabel = decmpfsCompressionTypeLabel(infoIt->second.compressionType);
                            outRow.validationStatus = (outRow.outputSizeBytes == infoIt->second.uncompressedSize) ? ("DECOMPFS_RESOURCE_FORK_" + algLabel + "_SIZE_MATCH") : ("DECOMPFS_RESOURCE_FORK_" + algLabel + "_SIZE_MISMATCH");
                            outRow.interpretation = "APFS decmpfs resource-fork data was reconstructed from a copied com.apple.ResourceFork stream and staged as the original logical Store-V2 file.";
                            outRow.notes = "synthetic_base=1; resource_row_sequence=" + std::to_string(rfIt->second.sequence) + "; " + recon.notes;
                            apfsSpotlightFileCopyOutRows.push_back(std::move(outRow));
                            ++reconstructedCount;
                        }

                        if (reconstructionAttemptCount != 0) {
                            log.info("APFS decmpfs resource-fork reconstruction attempts: " + std::to_string(reconstructionAttemptCount) + "; reconstructed=" + std::to_string(reconstructedCount));
                        }
                    }

                    int closeRc = -9999;
                    try { closeRc = pClose(handle); }
                    catch (...) { addRow("AFF4_close", "EXCEPTION", originalInput, finalObjectSize, 0, -1, {}, "Exception while closing AFF4 handle."); }
                    if (closeRc != -9999) addRow("AFF4_close", closeRc == 0 ? "CLOSED" : "CLOSE_NONZERO", originalInput, finalObjectSize, 0, closeRc, {}, "AFF4_close return code recorded.");
                }
            }
            FreeLibrary(h);
        }
        if (aff4DllDirCookie) RemoveDllDirectory(aff4DllDirCookie);
    }
#else
    addRow("dynamic_load_probe", "NOT_SUPPORTED_ON_THIS_PLATFORM", {}, 0, 0, -1, {}, "The current executable was not built for Windows; V0_8_23 dynamic-load probe targets Windows libaff4.dll.");
    addApfsRow("virtual_apfs_probe", "NOT_SUPPORTED_ON_THIS_PLATFORM", 0, -1, {}, "NOT_RUN", "AFF4 virtual APFS probe requires Windows libaff4 dynamic loading in this build.", {}, "Run the Windows build for this probe.");
#endif

    const bool writeHeavyApfsDiagnostics = shouldWriteAff4ApfsStructuralDiagnostics(opt.verbose, opt.diagnosticFullNativeDb, opt.aff4ApfsDiagnosticOutputs);
    const bool strictAff4PolicyForOutputs = opt.strictSingleAff4 || isAff4SourcePath(originalInput);
    if (writeHeavyApfsDiagnostics) {
        log.info("AFF4/APFS diagnostic output mode enabled: writing structural probe CSV outputs.");
        writeOutputs();
        writeAff4ApfsContainerViewOutputs(caseDir, source, originalInput, nxSummary, apfsDescriptorRows, log);
        writeAff4ApfsVolumeSuperblockOutputs(caseDir, source, originalInput, nxSummary, apfsVolumeRows, log);
        writeAff4ApfsCheckpointMapOutputs(caseDir, source, originalInput, nxSummary, apfsCheckpointMapRows, apfsCheckpointMappedObjectRows, log);
        writeObjectResolutionOutputs();
        writeAff4ApfsResolvedVolumeOutputs(caseDir, source, originalInput, nxSummary, apfsResolvedVolumeSuperblockRows, apfsVolumeOmapProbeRows, apfsVolumeRootTreeLookupRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsVolumeRootTreeLookupOutputs(caseDir, source, originalInput, apfsVolumeRootTreeLookupRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsRootTreeNodeProbeOutputs(caseDir, source, originalInput, apfsRootTreeNodeProbeRows, apfsRootTreeRecordSampleRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsRootTreeTraversalProbeOutputs(caseDir, source, originalInput, apfsRootTreeChildNodeProbeRows, apfsRootTreeChildRecordSampleRows, "child", strictAff4PolicyForOutputs, log);
        writeAff4ApfsRootTreeTraversalProbeOutputs(caseDir, source, originalInput, apfsRootTreeDescendantNodeProbeRows, apfsRootTreeDescendantRecordSampleRows, "descendant", strictAff4PolicyForOutputs, log);
        writeAff4ApfsFilesystemNamespaceSeedOutputs(caseDir, source, originalInput, apfsRootTreeRecordSampleRows, apfsRootTreeChildRecordSampleRows, apfsRootTreeDescendantRecordSampleRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsSpotlightTargetScanOutputs(caseDir, source, originalInput, apfsSpotlightTargetScanRows, apfsSpotlightNameScanSampleRows, apfsSpotlightCopyAttemptRows, apfsSpotlightTargetScanMetrics, strictAff4PolicyForOutputs, log);
        writeAff4ApfsSpotlightInodeProbeOutputs(caseDir, source, originalInput, apfsSpotlightInodeProbeRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsSpotlightXattrProbeOutputs(caseDir, source, originalInput, apfsSpotlightXattrProbeRows, apfsSpotlightCopyAttemptRows, strictAff4PolicyForOutputs, log);
        writeAff4ApfsSpotlightFileExtentProbeOutputs(caseDir, source, originalInput, apfsSpotlightFileExtentProbeRows, strictAff4PolicyForOutputs, log);
    } else {
        log.info("Normal AFF4/APFS source-probe mode: structural diagnostic CSV outputs suppressed; writing copy-out/stage outputs only.");
        appendRunStatus(caseDir, aff4ApfsStructuralDiagnosticsSuppressedStatus(), aff4ApfsStructuralDiagnosticsSuppressedGuidance());
    }
    writeAff4ApfsSpotlightFileCopyOutOutputs(caseDir, source, originalInput, apfsSpotlightFileCopyOutRows, strictAff4PolicyForOutputs, log);
    writeAff4ApfsExtractedStoreV2StageOutputs(caseDir, source, originalInput, apfsSpotlightFileCopyOutRows, strictAff4PolicyForOutputs, log);
    log.info("AFF4 APFS Spotlight file copy-out output written: " + pathString(caseDir / "aff4_apfs_spotlight_file_copy_out.csv"));
    log.info("AFF4 APFS extracted Store-V2 stage output written: " + pathString(caseDir / "aff4_apfs_extracted_storev2_stage_summary.json"));
    if (writeHeavyApfsDiagnostics) {
        log.info("AFF4 CPP Lite dynamic-load probe written: " + pathString(csvPath));
        log.info("AFF4 virtual APFS probe written: " + pathString(apfsCsvPath));
        log.info("AFF4 APFS container view written: " + pathString(caseDir / "aff4_apfs_container_superblock.csv"));
        log.info("AFF4 APFS volume superblock probe written: " + pathString(caseDir / "aff4_apfs_volume_superblocks.csv"));
        log.info("AFF4 APFS checkpoint map probe written: " + pathString(caseDir / "aff4_apfs_checkpoint_map.csv"));
        log.info("AFF4 APFS object-resolution probe written: " + pathString(caseDir / "aff4_apfs_object_id_probe.csv"));
        log.info("AFF4 APFS OMAP probe written: " + pathString(caseDir / "aff4_apfs_omap_phys_probe.csv"));
        log.info("AFF4 APFS OMAP B-tree root probe written: " + pathString(caseDir / "aff4_apfs_omap_btree_root_probe.csv"));
        log.info("AFF4 APFS OMAP lookup probe written: " + pathString(caseDir / "aff4_apfs_omap_lookup_probe.csv"));
        log.info("AFF4 APFS OMAP B-tree TOC probe written: " + pathString(caseDir / "aff4_apfs_omap_btree_toc_probe.csv"));
        log.info("AFF4 APFS OMAP leaf key/value decode written: " + pathString(caseDir / "aff4_apfs_omap_leaf_kv_decode.csv"));
        log.info("AFF4 APFS OMAP leaf lookup results written: " + pathString(caseDir / "aff4_apfs_omap_leaf_lookup_results.csv"));
        log.info("AFF4 APFS resolved volume superblocks written: " + pathString(caseDir / "aff4_apfs_resolved_volume_superblocks.csv"));
        log.info("AFF4 APFS volume OMAP probe written: " + pathString(caseDir / "aff4_apfs_volume_omap_probe.csv"));
        log.info("AFF4 APFS volume root-tree lookup written: " + pathString(caseDir / "aff4_apfs_volume_root_tree_lookup.csv"));
        log.info("AFF4 APFS root-tree node probe written: " + pathString(caseDir / "aff4_apfs_root_tree_node_probe.csv"));
        log.info("AFF4 APFS Spotlight target scan written: " + pathString(caseDir / "aff4_apfs_spotlight_target_scan.csv"));
        log.info("AFF4 APFS Spotlight inode probe written: " + pathString(caseDir / "aff4_apfs_spotlight_inode_probe.csv"));
        log.info("AFF4 APFS Spotlight XATTR probe written: " + pathString(caseDir / "aff4_apfs_spotlight_xattr_probe.csv"));
        log.info("AFF4 APFS Spotlight file-extent probe written: " + pathString(caseDir / "aff4_apfs_spotlight_file_extent_probe.csv"));
    }
}


struct Aff4ZipCentralDirectoryRow {
    std::size_t index = 0;
    std::string entryName;
    std::uint16_t method = 0;
    std::uint64_t compressedSize = 0;
    std::uint64_t uncompressedSize = 0;
    std::uint64_t localHeaderOffset = 0;
    std::string classification;
    std::string spotlightHint;
    std::string apfsHint;
    std::string notes;
};
std::string asciiLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string aff4ZipEntryClassification(const std::string& name) {
    const std::string n = asciiLower(name);
    if (n.find("corespotlight") != std::string::npos || n.find("store-v2") != std::string::npos || n.find(".store.db") != std::string::npos || n.find("index.db") != std::string::npos) return "SPOTLIGHT_PATH_HINT";
    if (n.find("container.description") != std::string::npos || n.find("information.turtle") != std::string::npos || n.find("version.txt") != std::string::npos || n.find("aff4://") != std::string::npos) return "AFF4_METADATA";
    if (n.find("apfs") != std::string::npos) return "APFS_NAME_HINT";
    if (n.find("image") != std::string::npos || n.find("stream") != std::string::npos || n.find("data") != std::string::npos) return "POSSIBLE_IMAGE_STREAM";
    return "ZIP_ENTRY";
}

std::string aff4ZipSpotlightHint(const std::string& name) {
    const std::string n = asciiLower(name);
    if (n.find("corespotlight") != std::string::npos) return "IOS_CORESPOTLIGHT_NAME_HINT";
    if (n.find("store-v2") != std::string::npos || n.find(".store.db") != std::string::npos) return "MACOS_SPOTLIGHT_NAME_HINT";
    if (n.find("index.db") != std::string::npos) return "SPOTLIGHT_SQLITE_NAME_HINT";
    return "";
}

std::string aff4ZipApfsHint(const std::string& name) {
    const std::string n = asciiLower(name);
    if (n.find("apfs") != std::string::npos) return "APFS_NAME_HINT";
    return "";
}

std::string decimalZeroPad(std::uint32_t value, int width) {
    std::ostringstream ss;
    ss << std::setw(width) << std::setfill('0') << value;
    return ss.str();
}

bool readExactFileBytes(const fs::path& path, std::uint64_t offset, std::size_t length, std::vector<unsigned char>& out, std::string& error) {
    out.clear();
#if defined(_WIN32)
    if (offset > static_cast<std::uint64_t>((std::numeric_limits<long long>::max)())) {
        error = "Offset too large for Windows 64-bit file seek.";
        return false;
    }
    FILE* f = nullptr;
    const std::wstring widePath = wideProcessPath(path);
    if (_wfopen_s(&f, widePath.c_str(), L"rb") != 0 || !f) {
        error = "Unable to open file for exact single-file AFF4 ZIP probe: " + pathString(path);
        return false;
    }
    if (_fseeki64(f, static_cast<long long>(offset), SEEK_SET) != 0) {
        error = "64-bit seek failed at offset " + std::to_string(offset);
        fclose(f);
        return false;
    }
    out.assign(length, 0);
    const std::size_t got = std::fread(out.data(), 1, length, f);
    if (got != length) {
        error = "Short read at offset " + std::to_string(offset) + ": requested=" + std::to_string(length) + " got=" + std::to_string(got);
        out.resize(got);
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
#else
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Unable to open file for exact single-file AFF4 ZIP probe: " + pathString(path);
        return false;
    }
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
        error = "Offset too large for platform streamoff.";
        return false;
    }
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) {
        error = "Seek failed at offset " + std::to_string(offset);
        return false;
    }
    out.assign(length, 0);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(length));
    const std::streamsize got = in.gcount();
    if (got < 0) {
        error = "Read failed.";
        return false;
    }
    out.resize(static_cast<std::size_t>(got));
    if (out.size() != length) {
        error = "Short read at offset " + std::to_string(offset) + ": requested=" + std::to_string(length) + " got=" + std::to_string(out.size());
        return false;
    }
    return true;
#endif
}


int aff4ZipDataChunkIndex(const std::string& name) {
    const std::string n = asciiLower(name);
    const std::size_t p = n.rfind("/data/");
    if (p == std::string::npos) return -1;
    std::string tail = n.substr(p + 6);
    if (tail.size() >= 6 && tail.substr(tail.size() - 6) == ".index") return -1;
    if (tail.size() < 8) return -1;
    int v = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(tail[i]))) return -1;
        v = v * 10 + (tail[i] - '0');
    }
    return v;
}

bool aff4ZipIsIndexEntry(const std::string& name) {
    const std::string n = asciiLower(name);
    return n.size() >= 6 && n.substr(n.size() - 6) == ".index";
}

bool readAff4ZipLocalPayloadOffset(const fs::path& path,
                                   const Aff4ZipCentralDirectoryRow& row,
                                   std::uint64_t& payloadOffset,
                                   std::string& error) {
    payloadOffset = 0;
    std::vector<unsigned char> fixed;
    if (!readExactFileBytes(path, row.localHeaderOffset, 30, fixed, error)) return false;
    if (readLe32(fixed, 0) != 0x04034b50U) {
        error = "ZIP local-file-header signature mismatch at offset " + std::to_string(row.localHeaderOffset);
        return false;
    }
    const std::uint16_t nameLen = readLe16(fixed, 26);
    const std::uint16_t extraLen = readLe16(fixed, 28);
    payloadOffset = row.localHeaderOffset + 30ULL + static_cast<std::uint64_t>(nameLen) + static_cast<std::uint64_t>(extraLen);
    return true;
}

bool readAff4StoredZipEntryTextFromProbe(const fs::path& caseDir,
                                         const fs::path& originalInput,
                                         const std::string& wantedEntryName,
                                         std::size_t maxBytes,
                                         std::string& text,
                                         std::string& error) {
    text.clear();
    error.clear();
    const fs::path csvPath = caseDir / "aff4_zip_central_directory.csv";
    if (!fs::exists(csvPath)) {
        error = "AFF4 central-directory probe CSV is not available yet.";
        return false;
    }
    Rows rows;
    try {
        rows = readCsv(csvPath);
    } catch (const std::exception& ex) {
        error = std::string("Unable to read AFF4 central-directory probe CSV: ") + ex.what();
        return false;
    }
    for (const auto& csvRow : rows) {
        if (get(csvRow, "entry_name") != wantedEntryName) continue;
        Aff4ZipCentralDirectoryRow row;
        row.entryName = wantedEntryName;
        try {
            row.method = static_cast<std::uint16_t>(std::stoul(get(csvRow, "compression_method")));
            row.compressedSize = static_cast<std::uint64_t>(std::stoull(get(csvRow, "compressed_size")));
            row.uncompressedSize = static_cast<std::uint64_t>(std::stoull(get(csvRow, "uncompressed_size")));
            row.localHeaderOffset = static_cast<std::uint64_t>(std::stoull(get(csvRow, "local_header_offset")));
        } catch (...) {
            error = "AFF4 central-directory row has invalid numeric values for " + wantedEntryName;
            return false;
        }
        if (row.method != 0) {
            error = wantedEntryName + " is not stored/uncompressed in the AFF4 ZIP container.";
            return false;
        }
        if (row.compressedSize > maxBytes) {
            error = wantedEntryName + " exceeds bounded metadata read cap.";
            return false;
        }
        std::uint64_t payloadOffset = 0;
        if (!readAff4ZipLocalPayloadOffset(originalInput, row, payloadOffset, error)) return false;
        std::vector<unsigned char> bytes;
        if (!readExactFileBytes(originalInput, payloadOffset, static_cast<std::size_t>(row.compressedSize), bytes, error)) return false;
        text.assign(bytes.begin(), bytes.end());
        return true;
    }
    error = "Entry not found in AFF4 central-directory probe CSV: " + wantedEntryName;
    return false;
}

std::vector<unsigned char> aff4DirectLz4DecompressBlock(const unsigned char* src, std::size_t srcSize, std::size_t expectedOutputSize) {
    constexpr std::size_t MaxOutput = 64U * 1024U * 1024U;
    if (expectedOutputSize > MaxOutput) throw std::runtime_error("AFF4 LZ4 output exceeds safety cap");
    std::vector<unsigned char> out(expectedOutputSize);
    std::size_t pos = 0;
    std::size_t outPos = 0;
    auto readLen = [&](std::size_t base) -> std::size_t {
        std::size_t len = base;
        if (base == 15U) {
            for (;;) {
                if (pos >= srcSize) throw std::runtime_error("AFF4 LZ4 length overrun");
                const unsigned char b = src[pos++];
                len += static_cast<std::size_t>(b);
                if (b != 255U) break;
            }
        }
        return len;
    };
    while (pos < srcSize) {
        const unsigned char token = src[pos++];
        const std::size_t literalLen = readLen(static_cast<std::size_t>(token >> 4));
        if (pos + literalLen > srcSize || outPos + literalLen > expectedOutputSize) throw std::runtime_error("AFF4 LZ4 literal overrun");
        std::memcpy(out.data() + outPos, src + pos, literalLen);
        pos += literalLen;
        outPos += literalLen;
        if (outPos == expectedOutputSize || pos >= srcSize) break;
        if (pos + 2U > srcSize) throw std::runtime_error("AFF4 LZ4 match offset missing");
        const std::size_t matchOffset = static_cast<std::size_t>(src[pos]) | (static_cast<std::size_t>(src[pos + 1U]) << 8);
        pos += 2U;
        if (matchOffset == 0U || matchOffset > outPos) throw std::runtime_error("AFF4 LZ4 invalid match offset");
        std::size_t matchLen = readLen(static_cast<std::size_t>(token & 0x0fU)) + 4U;
        if (outPos + matchLen > expectedOutputSize) throw std::runtime_error("AFF4 LZ4 match output overrun");
        while (matchLen-- > 0U) {
            out[outPos] = out[outPos - matchOffset];
            ++outPos;
        }
        if (outPos == expectedOutputSize) break;
    }
    if (outPos != expectedOutputSize) throw std::runtime_error("AFF4 LZ4 output size mismatch");
    return out;
}

struct Aff4DirectMapEntry {
    std::uint64_t virtualOffset = 0;
    std::uint64_t length = 0;
    std::uint64_t streamOffset = 0;
    std::uint32_t streamId = 0;
    std::uint64_t mapEntryIndex = 0;
};

struct Aff4DirectSqliteCandidateRow {
    std::size_t sequence = 0;
    std::string status;
    std::uint64_t virtualOffset = 0;
    std::uint64_t mapEntryIndex = 0;
    std::uint64_t chunkIndex = 0;
    std::uint32_t pageSize = 0;
    std::uint32_t dbPages = 0;
    std::uint64_t requestedBytes = 0;
    std::uint64_t carvedBytes = 0;
    std::string outputRelativePath;
    std::string sqliteOpenStatus;
    std::string sqliteMasterStatus;
    int tableCount = 0;
    std::string tableNames;
    std::string sampleHex;
    std::string notes;
};

void writeAff4DirectMapReaderProbe(const fs::path& caseDir,
                                   const EvidenceSource& source,
                                   const RunOptions& opt,
                                   const fs::path& originalInput,
                                   Logger& log) {
    struct ProbeRow {
        std::string step;
        std::string status;
        std::uint64_t virtualOffset = 0;
        std::uint64_t streamOffset = 0;
        std::uint64_t mapEntryIndex = 0;
        std::uint64_t chunkIndex = 0;
        long long bytesRead = -1;
        std::string magic;
        std::string sampleHex;
        std::string notes;
    };
    std::vector<ProbeRow> probeRows;
    std::vector<Aff4DirectSqliteCandidateRow> sqliteCandidateRows;
    auto add = [&](const std::string& step, const std::string& status, std::uint64_t virtualOffset,
                   std::uint64_t streamOffset, std::uint64_t mapEntryIndex, std::uint64_t chunkIndex,
                   long long bytesRead, const std::string& magic, const std::string& sampleHex,
                   const std::string& notes) {
        ProbeRow r;
        r.step = step;
        r.status = status;
        r.virtualOffset = virtualOffset;
        r.streamOffset = streamOffset;
        r.mapEntryIndex = mapEntryIndex;
        r.chunkIndex = chunkIndex;
        r.bytesRead = bytesRead;
        r.magic = magic;
        r.sampleHex = sampleHex;
        r.notes = notes;
        probeRows.push_back(r);
    };

    const fs::path csvPath = caseDir / "aff4_direct_map_reader_probe.csv";
    const fs::path jsonPath = caseDir / "aff4_direct_map_reader_probe_summary.json";
    const fs::path mdPath = caseDir / "AFF4_DIRECT_MAP_READER_PROBE.md";
    const fs::path sqliteCsvPath = caseDir / "aff4_direct_sqlite_candidate_carve.csv";
    const fs::path sqliteJsonPath = caseDir / "aff4_direct_sqlite_candidate_carve_summary.json";
    const fs::path sqliteMdPath = caseDir / "AFF4_DIRECT_SQLITE_CANDIDATE_CARVE.md";
    const fs::path sqliteCandidateDir = caseDir / "Aff4DirectSqliteCandidates";
    std::size_t mapEntryCount = 0;
    std::size_t mapEntriesScanned = 0;
    std::size_t chunksDecoded = 0;
    std::size_t lz4ChunksDecoded = 0;
    std::size_t alignedApfsHits = 0;
    std::size_t directSignatureHits = 0;
    std::size_t sqliteCandidatesFound = 0;
    std::size_t sqliteCandidatesCarved = 0;
    std::size_t sqliteCandidatesOpened = 0;
    ApfsNxSuperblockSummary directBestNx;
    std::vector<ApfsCheckpointDescriptorRow> directDescriptorRows;
    std::vector<ApfsVolumeSuperblockRow> directVolumeRows;
    std::vector<ApfsResolvedVolumeSuperblockRow> directResolvedVolumeRows;
    std::vector<ApfsVolumeOmapProbeRow> directVolumeOmapRows;
    std::vector<ApfsVolumeRootTreeLookupRow> directVolumeRootTreeLookupRows;
    std::vector<ApfsRootTreeNodeProbeRow> directRootTreeNodeRows;
    std::vector<ApfsRootTreeRecordSampleRow> directRootTreeRecordRows;
    std::vector<ApfsRootTreeRecordSampleRow> directSpotlightTargetRows;
    std::vector<ApfsRootTreeRecordSampleRow> directSpotlightNameSampleRows;
    std::vector<ApfsSpotlightCopyAttemptRow> directSpotlightCopyAttemptRows;
    std::vector<ApfsSpotlightInodeProbeRow> directSpotlightInodeRows;
    std::vector<ApfsSpotlightXattrProbeRow> directSpotlightXattrRows;
    std::vector<ApfsSpotlightFileExtentProbeRow> directSpotlightFileExtentRows;
    std::vector<ApfsSpotlightFileCopyOutRow> directSpotlightFileCopyOutRows;
    std::vector<ApfsDirectoryRecordEntry> directDirectoryRecordEntries;
    std::map<std::pair<std::uint32_t, std::uint64_t>, ApfsSpotlightInodeProbeRow> directIndexedInodeByObject;
    std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<ApfsSpotlightFileExtentProbeRow>> directIndexedFileExtentsByObject;
    ApfsSpotlightTargetScanMetrics directSpotlightScanMetrics;
    std::map<std::uint32_t, ApfsVolumeOmapProbeRow> directVolumeOmapBySequence;
    std::map<std::uint32_t, std::vector<unsigned char>> directVolumeOmapRootBySequence;
    std::vector<ApfsCheckpointMapEntryRow> directCheckpointMapRows;
    std::vector<ApfsCheckpointMappedObjectProbeRow> directCheckpointObjectRows;
    std::string finalStatus = "NOT_RUN";
    std::string finalNotes;

    try {
        const fs::path centralCsv = caseDir / "aff4_zip_central_directory.csv";
        Rows rows = readCsv(centralCsv);
        std::map<std::string, Aff4ZipCentralDirectoryRow> byName;
        for (const auto& csvRow : rows) {
            Aff4ZipCentralDirectoryRow r;
            r.entryName = get(csvRow, "entry_name");
            try {
                r.method = static_cast<std::uint16_t>(std::stoul(get(csvRow, "compression_method")));
                r.compressedSize = static_cast<std::uint64_t>(std::stoull(get(csvRow, "compressed_size")));
                r.uncompressedSize = static_cast<std::uint64_t>(std::stoull(get(csvRow, "uncompressed_size")));
                r.localHeaderOffset = static_cast<std::uint64_t>(std::stoull(get(csvRow, "local_header_offset")));
            } catch (...) {
                continue;
            }
            byName[r.entryName] = r;
        }

        auto readStoredEntry = [&](const std::string& name, std::vector<unsigned char>& out, std::string& err) -> bool {
            out.clear();
            const auto it = byName.find(name);
            if (it == byName.end()) {
                err = "AFF4 ZIP entry not found: " + name;
                return false;
            }
            if (it->second.method != 0) {
                err = "AFF4 ZIP entry is not stored/uncompressed: " + name;
                return false;
            }
            if (it->second.compressedSize > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                err = "AFF4 ZIP entry too large for memory read: " + name;
                return false;
            }
            std::uint64_t payload = 0;
            if (!readAff4ZipLocalPayloadOffset(originalInput, it->second, payload, err)) return false;
            return readExactFileBytes(originalInput, payload, static_cast<std::size_t>(it->second.compressedSize), out, err);
        };

        const std::string mapUrn = "aff4%3A%2F%2F99930a27-3e61-419e-8b6b-65a3a40bedcb";
        std::vector<unsigned char> idxBytes;
        std::vector<unsigned char> mapBytes;
        std::string err;
        if (!readStoredEntry(mapUrn + "/idx", idxBytes, err)) {
            finalStatus = "IDX_READ_FAILED";
            finalNotes = err;
            add("direct_idx_read", finalStatus, 0, 0, 0, 0, -1, {}, {}, err);
        } else if (!readStoredEntry(mapUrn + "/map", mapBytes, err)) {
            finalStatus = "MAP_READ_FAILED";
            finalNotes = err;
            add("direct_map_read", finalStatus, 0, 0, 0, 0, -1, {}, {}, err);
        } else {
            mapEntryCount = mapBytes.size() / 28U;
            add("direct_idx_read", "READ_OK", 0, 0, 0, 0, static_cast<long long>(idxBytes.size()), {}, hexSampleBytes(idxBytes.data(), std::min<std::size_t>(idxBytes.size(), 64U)), "AFF4 map stream index was read directly from the ZIP payload.");
            add("direct_map_read", "READ_OK", 0, 0, 0, 0, static_cast<long long>(mapBytes.size()), {}, hexSampleBytes(mapBytes.data(), std::min<std::size_t>(mapBytes.size(), 64U)), "AFF4 map entries were read directly from the ZIP payload.");

            std::vector<Aff4DirectMapEntry> mapEntries;
            mapEntries.reserve(mapEntryCount);
            for (std::size_t mi = 0; mi < mapEntryCount; ++mi) {
                const std::size_t moff = mi * 28U;
                Aff4DirectMapEntry me;
                me.virtualOffset = readLe64(mapBytes, moff);
                me.length = readLe64(mapBytes, moff + 8U);
                me.streamOffset = readLe64(mapBytes, moff + 16U);
                me.streamId = readLe32(mapBytes, moff + 24U);
                me.mapEntryIndex = static_cast<std::uint64_t>(mi);
                if (me.streamId == 0U && me.length > 0U) mapEntries.push_back(me);
            }
            std::sort(mapEntries.begin(), mapEntries.end(), [](const auto& a, const auto& b) {
                if (a.virtualOffset != b.virtualOffset) return a.virtualOffset < b.virtualOffset;
                return a.mapEntryIndex < b.mapEntryIndex;
            });
            auto findMapEntryForVirtual = [&](std::uint64_t v) -> const Aff4DirectMapEntry* {
                auto it = std::upper_bound(mapEntries.begin(), mapEntries.end(), v, [](std::uint64_t value, const Aff4DirectMapEntry& e) {
                    return value < e.virtualOffset;
                });
                if (it == mapEntries.begin()) return nullptr;
                --it;
                if (v < it->virtualOffset) return nullptr;
                const std::uint64_t rel = v - it->virtualOffset;
                if (rel >= it->length) return nullptr;
                return &(*it);
            };

            std::map<std::uint32_t, std::vector<unsigned char>> indexCache;
            std::map<std::uint32_t, std::uint64_t> dataPayloadCache;
            auto decodeImageChunk = [&](std::uint64_t chunk, std::vector<unsigned char>& dec, std::string& decodeErr, bool& usedLz4) -> bool {
                dec.clear();
                decodeErr.clear();
                usedLz4 = false;
                const std::uint32_t bevvy = static_cast<std::uint32_t>(chunk / 1024ULL);
                const std::uint32_t chunkInBevvy = static_cast<std::uint32_t>(chunk % 1024ULL);
                const std::string seg = decimalZeroPad(bevvy, 8);
                std::vector<unsigned char>& indexBytes = indexCache[bevvy];
                if (indexBytes.empty()) {
                    if (!readStoredEntry(mapUrn + "/data/" + seg + ".index", indexBytes, decodeErr)) return false;
                }
                const std::size_t pointOff = static_cast<std::size_t>(chunkInBevvy) * 12U;
                if (pointOff + 12U > indexBytes.size()) {
                    decodeErr = "AFF4 data index point is beyond index payload.";
                    return false;
                }
                const std::uint64_t dataRel = readLe64(indexBytes, pointOff);
                const std::uint32_t compLen = readLe32(indexBytes, pointOff + 8U);
                if (compLen == 0U || compLen > 1024U * 1024U) {
                    decodeErr = "AFF4 compressed chunk length is zero or above safety cap.";
                    return false;
                }
                std::uint64_t dataPayload = 0;
                const auto dpIt = dataPayloadCache.find(bevvy);
                if (dpIt == dataPayloadCache.end()) {
                    const auto entryIt = byName.find(mapUrn + "/data/" + seg);
                    if (entryIt == byName.end()) {
                        decodeErr = "AFF4 data bevvy entry not found: " + seg;
                        return false;
                    }
                    if (!readAff4ZipLocalPayloadOffset(originalInput, entryIt->second, dataPayload, decodeErr)) return false;
                    dataPayloadCache[bevvy] = dataPayload;
                } else {
                    dataPayload = dpIt->second;
                }
                std::vector<unsigned char> comp;
                if (!readExactFileBytes(originalInput, dataPayload + dataRel, static_cast<std::size_t>(compLen), comp, decodeErr)) return false;
                try {
                    dec = (compLen == 32768U) ? comp : aff4DirectLz4DecompressBlock(comp.data(), comp.size(), 32768U);
                    usedLz4 = compLen != 32768U;
                } catch (const std::exception& ex) {
                    decodeErr = ex.what();
                    return false;
                }
                return true;
            };

            auto readVirtualBytes = [&](std::uint64_t virtualStart, std::uint64_t requested, std::vector<unsigned char>& out, std::string& readErr) -> bool {
                out.clear();
                readErr.clear();
                if (requested > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                    readErr = "Requested virtual read exceeds size_t.";
                    return false;
                }
                out.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(requested, 8ULL * 1024ULL * 1024ULL)));
                std::uint64_t cursor = virtualStart;
                while (out.size() < requested) {
                    const Aff4DirectMapEntry* me = findMapEntryForVirtual(cursor);
                    if (!me) {
                        readErr = "No AFF4 map entry covers virtual offset " + std::to_string(cursor);
                        return false;
                    }
                    const std::uint64_t withinMap = cursor - me->virtualOffset;
                    const std::uint64_t imageStreamOffset = me->streamOffset + withinMap;
                    const std::uint64_t chunk = imageStreamOffset / 32768ULL;
                    const std::size_t chunkOff = static_cast<std::size_t>(imageStreamOffset % 32768ULL);
                    std::vector<unsigned char> dec;
                    bool usedLz4 = false;
                    if (!decodeImageChunk(chunk, dec, readErr, usedLz4)) return false;
                    if (chunkOff >= dec.size()) {
                        readErr = "Decoded AFF4 chunk is shorter than expected.";
                        return false;
                    }
                    const std::uint64_t remainingRequested = requested - static_cast<std::uint64_t>(out.size());
                    const std::uint64_t remainingMap = me->length - withinMap;
                    const std::size_t canCopy = static_cast<std::size_t>(std::min<std::uint64_t>({remainingRequested, remainingMap, static_cast<std::uint64_t>(dec.size() - chunkOff)}));
                    out.insert(out.end(), dec.begin() + static_cast<std::ptrdiff_t>(chunkOff), dec.begin() + static_cast<std::ptrdiff_t>(chunkOff + canCopy));
                    cursor += static_cast<std::uint64_t>(canCopy);
                    if (canCopy == 0U) {
                        readErr = "AFF4 virtual read made no progress.";
                        return false;
                    }
                }
                return true;
            };

            auto readBe16Local = [](const std::vector<unsigned char>& data, std::size_t off) -> std::uint16_t {
                if (off + 2U > data.size()) return 0;
                return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[off]) << 8U) | static_cast<std::uint16_t>(data[off + 1U]));
            };
            auto readBe32Local = [](const std::vector<unsigned char>& data, std::size_t off) -> std::uint32_t {
                if (off + 4U > data.size()) return 0;
                return (static_cast<std::uint32_t>(data[off]) << 24U) |
                       (static_cast<std::uint32_t>(data[off + 1U]) << 16U) |
                       (static_cast<std::uint32_t>(data[off + 2U]) << 8U) |
                       static_cast<std::uint32_t>(data[off + 3U]);
            };
            auto isPowerOfTwo = [](std::uint32_t v) -> bool { return v != 0U && (v & (v - 1U)) == 0U; };
            auto validateSqliteCandidate = [&](const fs::path& candidatePath, Aff4DirectSqliteCandidateRow& row) {
                sqlite3* ext = nullptr;
                int rc = sqlite3_open_v2(pathString(candidatePath).c_str(), &ext, SQLITE_OPEN_READONLY, nullptr);
                if (rc != SQLITE_OK) {
                    row.sqliteOpenStatus = ext ? sqlite3_errmsg(ext) : "sqlite handle not created";
                    if (ext) sqlite3_close(ext);
                    return;
                }
                row.sqliteOpenStatus = "OPEN_OK";
                sqlite3_stmt* st = nullptr;
                rc = sqlite3_prepare_v2(ext, "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name LIMIT 20", -1, &st, nullptr);
                if (rc != SQLITE_OK) {
                    row.sqliteMasterStatus = sqlite3_errmsg(ext);
                    sqlite3_close(ext);
                    return;
                }
                std::vector<std::string> names;
                while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                    const unsigned char* txt = sqlite3_column_text(st, 0);
                    names.push_back(txt ? reinterpret_cast<const char*>(txt) : "");
                }
                sqlite3_finalize(st);
                if (rc == SQLITE_DONE) {
                    row.sqliteMasterStatus = "SQLITE_MASTER_OK";
                    row.tableCount = static_cast<int>(names.size());
                    std::ostringstream joined;
                    for (std::size_t ni = 0; ni < names.size(); ++ni) {
                        if (ni) joined << ';';
                        joined << names[ni];
                    }
                    row.tableNames = joined.str();
                    if (!names.empty()) ++sqliteCandidatesOpened;
                } else {
                    row.sqliteMasterStatus = sqlite3_errmsg(ext);
                }
                sqlite3_close(ext);
            };
            auto carveSqliteCandidate = [&](std::uint64_t hitVirtual, std::uint64_t mapEntryIndex, std::uint64_t chunkIndex) {
                if (sqliteCandidateRows.size() >= 12U) return;
                ++sqliteCandidatesFound;
                Aff4DirectSqliteCandidateRow row;
                row.sequence = sqliteCandidateRows.size() + 1U;
                row.virtualOffset = hitVirtual;
                row.mapEntryIndex = mapEntryIndex;
                row.chunkIndex = chunkIndex;
                std::vector<unsigned char> header;
                std::string readErr;
                if (!readVirtualBytes(hitVirtual, 4096U, header, readErr) || header.size() < 100U) {
                    row.status = "HEADER_READ_FAILED";
                    row.notes = readErr;
                    sqliteCandidateRows.push_back(row);
                    return;
                }
                row.sampleHex = hexSampleBytes(header.data(), std::min<std::size_t>(header.size(), 128U));
                if (!bytesAt(header, 0, "SQLite format 3", 15U)) {
                    row.status = "HEADER_SIGNATURE_MISMATCH";
                    row.notes = "Virtual read at the signature offset did not begin with a SQLite header.";
                    sqliteCandidateRows.push_back(row);
                    return;
                }
                std::uint32_t pageSize = readBe16Local(header, 16U);
                if (pageSize == 1U) pageSize = 65536U;
                row.pageSize = pageSize;
                row.dbPages = readBe32Local(header, 28U);
                constexpr std::uint64_t DefaultCarveBytes = 2ULL * 1024ULL * 1024ULL;
                constexpr std::uint64_t MaxCarveBytes = 8ULL * 1024ULL * 1024ULL;
                std::uint64_t requested = DefaultCarveBytes;
                if (pageSize >= 512U && pageSize <= 65536U && isPowerOfTwo(pageSize) && row.dbPages > 0U) {
                    const std::uint64_t dbBytes = static_cast<std::uint64_t>(pageSize) * static_cast<std::uint64_t>(row.dbPages);
                    if (dbBytes >= pageSize) requested = std::min<std::uint64_t>(dbBytes, MaxCarveBytes);
                }
                row.requestedBytes = requested;
                std::vector<unsigned char> candidate;
                readErr.clear();
                const bool fullRead = readVirtualBytes(hitVirtual, requested, candidate, readErr);
                if (candidate.size() < 100U) {
                    row.status = "CARVE_READ_FAILED";
                    row.notes = readErr;
                    sqliteCandidateRows.push_back(row);
                    return;
                }
                row.carvedBytes = static_cast<std::uint64_t>(candidate.size());
                std::ostringstream name;
                name << "sqlite_candidate_" << std::setw(3) << std::setfill('0') << row.sequence
                     << "_v" << hitVirtual << ".db";
                const fs::path outPath = sqliteCandidateDir / name.str();
                std::error_code mkEc;
                fs::create_directories(sqliteCandidateDir, mkEc);
                std::ofstream out(outPath, std::ios::binary);
                out.write(reinterpret_cast<const char*>(candidate.data()), static_cast<std::streamsize>(candidate.size()));
                out.close();
                row.outputRelativePath = pathString(fs::path("Aff4DirectSqliteCandidates") / name.str());
                row.status = fullRead ? "CARVED_BOUNDED_CANDIDATE" : "CARVED_PARTIAL_CANDIDATE";
                row.notes = fullRead ? "Candidate carved by direct AFF4 virtual-offset reads." : ("Candidate partially carved before read failure: " + readErr);
                ++sqliteCandidatesCarved;
                validateSqliteCandidate(outPath, row);
                sqliteCandidateRows.push_back(row);
            };

            auto rememberNxCandidateFromDecodedChunk = [&](const std::vector<unsigned char>& dec,
                                                           std::size_t magicOffset,
                                                           std::uint64_t blockVirtualOffset,
                                                           std::uint64_t originalMapEntryIndex,
                                                           std::uint64_t chunkIndex) {
                if (magicOffset < 32U) return;
                const std::size_t blockStart = magicOffset - 32U;
                if (blockStart + 4096U > dec.size()) return;
                std::vector<unsigned char> block(dec.begin() + static_cast<std::ptrdiff_t>(blockStart),
                                                 dec.begin() + static_cast<std::ptrdiff_t>(blockStart + 4096U));
                ApfsNxSuperblockSummary candidate = parseApfsNxSuperblock(block, blockVirtualOffset, 4096);
                if (!candidate.found) return;
                candidate.notes = "APFS NXSB parsed from directly decoded AFF4 sorted virtual map. source_map_entry=" +
                                  std::to_string(originalMapEntryIndex) + "; chunk_index=" + std::to_string(chunkIndex);
                if (!directBestNx.found || candidate.xid > directBestNx.xid) {
                    directBestNx = candidate;
                }
            };

            auto probeDirectApfsFromBestNx = [&]() {
                if (!directBestNx.found || directBestNx.blockSize == 0) return;
                std::set<std::string> seenResolvedVolumes;

                auto directReadVirtual = [&](std::uint64_t offset,
                                             std::uint64_t bytes,
                                             std::vector<unsigned char>& out,
                                             std::string& readErr) -> long long {
                    const bool ok = readVirtualBytes(offset, bytes, out, readErr);
                    return ok ? static_cast<long long>(out.size()) : -1;
                };

                auto safeDirectNodeOffset = [&](std::uint64_t oid, std::uint64_t& offsetOut) -> bool {
                    if (directBestNx.blockSize == 0) return false;
                    if (directBestNx.blockCount != 0 && oid >= directBestNx.blockCount) return false;
                    if (oid > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(directBestNx.blockSize))) return false;
                    offsetOut = oid * static_cast<std::uint64_t>(directBestNx.blockSize);
                    return true;
                };

                auto appendDirectRootTreeNodeSample = [&](const ApfsVolumeRootTreeLookupRow& lookup,
                                                          const std::vector<unsigned char>& node) {
                    ApfsRootTreeNodeProbeRow nr;
                    nr.sequence = static_cast<std::uint32_t>(directRootTreeNodeRows.size());
                    nr.volumeSequence = lookup.volumeSequence;
                    nr.targetRole = lookup.targetRole;
                    nr.fsOid = lookup.fsOid;
                    nr.volumeName = lookup.volumeName;
                    nr.apfsRootTreeOid = lookup.apfsRootTreeOid;
                    nr.targetXid = lookup.targetXid;
                    nr.nodeOid = lookup.resolvedObjectOid ? lookup.resolvedObjectOid : lookup.apfsRootTreeOid;
                    nr.virtualOffset = lookup.resolvedVirtualOffset;
                    nr.bytesRead = lookup.resolvedBytesRead;
                    if (node.size() < 64U) {
                        nr.status = "ROOT_TREE_NODE_READ_FAILED";
                        nr.interpretation = "Resolved root-tree object was not large enough for B-tree header sampling.";
                        directRootTreeNodeRows.push_back(nr);
                        return;
                    }
                    nr.objectOid = readLe64(node, 8);
                    nr.objectXid = readLe64(node, 16);
                    nr.objectTypeRaw = readLe32(node, 24);
                    nr.objectTypeLabel = apfsObjectTypeLabel(nr.objectTypeRaw);
                    nr.objectSubtype = readLe32(node, 28);
                    if (node.size() >= 36U) nr.magic.assign(reinterpret_cast<const char*>(node.data() + 32), reinterpret_cast<const char*>(node.data() + 36));
                    nr.btnFlags = readLe16(node, 32);
                    nr.btnLevel = readLe16(node, 34);
                    nr.btnNkeys = readLe32(node, 36);
                    nr.tableSpaceOffset = readLe16(node, 40);
                    nr.tableSpaceLength = readLe16(node, 42);
                    nr.freeSpaceOffset = readLe16(node, 44);
                    nr.freeSpaceLength = readLe16(node, 46);
                    nr.sampleHex = hexSampleBytes(node.data(), std::min<std::size_t>(node.size(), 96U));
                    if (nr.objectTypeLabel == "BTREE" || nr.objectTypeLabel == "BTREE_NODE") {
                        nr.status = "ROOT_TREE_NODE_HEADER_PARSED";
                        nr.interpretation = "APFS filesystem root-tree B-tree node header parsed through the direct AFF4 reader.";
                    } else {
                        nr.status = "ROOT_TREE_NODE_UNEXPECTED_OBJECT_TYPE";
                        nr.interpretation = "Resolved root-tree object was readable but did not parse as an APFS B-tree node.";
                        directRootTreeNodeRows.push_back(nr);
                        return;
                    }
                    directRootTreeNodeRows.push_back(nr);

                    const std::uint32_t limit = std::min<std::uint32_t>(nr.btnNkeys, 32U);
                    for (std::uint32_t i = 0; i < limit; ++i) {
                        std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                        std::string detail;
                        ApfsRootTreeRecordSampleRow rr;
                        rr.sequence = static_cast<std::uint32_t>(directRootTreeRecordRows.size());
                        rr.volumeSequence = lookup.volumeSequence;
                        rr.targetRole = lookup.targetRole;
                        rr.fsOid = lookup.fsOid;
                        rr.volumeName = lookup.volumeName;
                        rr.apfsRootTreeOid = lookup.apfsRootTreeOid;
                        rr.nodeOid = nr.nodeOid;
                        rr.nodeVirtualOffset = lookup.resolvedVirtualOffset;
                        rr.nodeLevel = nr.btnLevel;
                        rr.nodeNkeys = nr.btnNkeys;
                        rr.entryIndex = i;
                        if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) {
                            rr.status = "ROOT_TREE_RECORD_TOC_DECODE_FAILED";
                            rr.interpretation = "The root-tree node TOC entry could not be decoded safely.";
                            rr.notes = detail;
                            directRootTreeRecordRows.push_back(rr);
                            continue;
                        }
                        rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                        rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                        rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                        rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                        rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                        rr.keySampleHex = hexSampleBytes(node.data() + keyAbs, std::min<std::size_t>(keyLen, 64U));
                        if (valLen > 0 && valAbs < node.size()) rr.valueSampleHex = hexSampleBytes(node.data() + valAbs, std::min<std::size_t>(valLen, 64U));
                        if (valLen >= 8U && valAbs + 8U <= node.size()) rr.valueU64_0 = readLe64(node, valAbs);
                        if (valLen >= 16U && valAbs + 16U <= node.size()) rr.valueU64_1 = readLe64(node, valAbs + 8U);
                        if (valLen >= 24U && valAbs + 24U <= node.size()) rr.valueU64_2 = readLe64(node, valAbs + 16U);
                        if (keyLen >= 8U) {
                            rr.keyRaw = readLe64(node, keyAbs);
                            rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                            rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                            rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                        }
                        if (nr.btnLevel > 0 && valLen >= 8U) rr.branchChildOid = readLe64(node, valAbs);
                        if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                            const std::uint32_t nameLenAndHash = readLe32(node, keyAbs + 8U);
                            const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                            rr.decodedName = safePrintableUtf8Fragment(node, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                        }
                        rr.status = "ROOT_TREE_RECORD_SAMPLE_DECODED";
                        rr.interpretation = nr.btnLevel > 0 ? "Root-tree branch record sampled through the direct AFF4 reader." : "Root-tree leaf record sampled through the direct AFF4 reader.";
                        rr.notes = detail;
                        directRootTreeRecordRows.push_back(rr);
                    }
                };

                auto appendDirectVolumeOmapAndRootLookup = [&](const ApfsResolvedVolumeSuperblockRow& row) {
                    ApfsVolumeOmapProbeRow omRow;
                    omRow.sequence = static_cast<std::uint32_t>(directVolumeOmapRows.size());
                    omRow.volumeSequence = row.sequence;
                    omRow.targetRole = row.targetRole;
                    omRow.fsOid = row.fsOid;
                    omRow.volumeObjectOid = row.objectOid;
                    omRow.volumeObjectXid = row.objectXid;
                    omRow.apfsOmapOid = row.apfsOmapOid;
                    omRow.apfsRootTreeOid = row.rootTreeOid;
                    std::vector<unsigned char> omapTreeRootBuf;
                    if (row.apfsOmapOid == 0) {
                        omRow.omapStatus = "VOLUME_OMAP_OID_ZERO";
                        omRow.interpretation = "Parsed APSB did not contain an apfs_omap_oid value.";
                    } else if (!safeDirectNodeOffset(row.apfsOmapOid, omRow.omapVirtualOffset)) {
                        omRow.omapStatus = "VOLUME_OMAP_OFFSET_UNSAFE";
                        omRow.interpretation = "apfs_omap_oid could not be converted to a bounded physical-block offset.";
                    } else {
                        std::vector<unsigned char> omapBuf;
                        std::string omapErr;
                        omRow.omapBytesRead = directReadVirtual(omRow.omapVirtualOffset, directBestNx.blockSize, omapBuf, omapErr);
                        if (omRow.omapBytesRead > 0 && omapBuf.size() >= 88U) {
                            omRow.omapObjectOid = readLe64(omapBuf, 8);
                            omRow.omapObjectXid = readLe64(omapBuf, 16);
                            omRow.omapObjectTypeRaw = readLe32(omapBuf, 24);
                            omRow.omapObjectTypeLabel = apfsObjectTypeLabel(omRow.omapObjectTypeRaw);
                            omRow.omapObjectSubtype = readLe32(omapBuf, 28);
                            omRow.omFlags = readLe32(omapBuf, 32);
                            omRow.omSnapshotCount = readLe32(omapBuf, 36);
                            omRow.omTreeType = readLe32(omapBuf, 40);
                            omRow.omSnapshotTreeType = readLe32(omapBuf, 44);
                            omRow.omTreeOid = readLe64(omapBuf, 48);
                            omRow.omSnapshotTreeOid = readLe64(omapBuf, 56);
                            omRow.omMostRecentSnap = readLe64(omapBuf, 64);
                            omRow.omPendingRevertMin = readLe64(omapBuf, 72);
                            omRow.omPendingRevertMax = readLe64(omapBuf, 80);
                            omRow.sampleHex = hexSampleBytes(omapBuf.data(), std::min<std::size_t>(omapBuf.size(), 96U));
                            if (omRow.omapObjectTypeLabel == "OBJECT_MAP") {
                                omRow.omapStatus = "VOLUME_OMAP_PARSED";
                                omRow.interpretation = "Volume object-map physical object parsed through the direct AFF4 reader.";
                            } else {
                                omRow.omapStatus = "VOLUME_OMAP_UNEXPECTED_OBJECT_TYPE";
                                omRow.interpretation = "apfs_omap_oid was read, but the object header type was not OBJECT_MAP.";
                            }
                            if (omRow.omTreeOid != 0 && safeDirectNodeOffset(omRow.omTreeOid, omRow.treeVirtualOffset)) {
                                std::vector<unsigned char> treeBuf;
                                std::string treeErr;
                                omRow.treeBytesRead = directReadVirtual(omRow.treeVirtualOffset, directBestNx.blockSize, treeBuf, treeErr);
                                if (omRow.treeBytesRead > 0 && treeBuf.size() >= 56U) {
                                    omRow.treeObjectOid = readLe64(treeBuf, 8);
                                    omRow.treeObjectXid = readLe64(treeBuf, 16);
                                    omRow.treeObjectTypeRaw = readLe32(treeBuf, 24);
                                    omRow.treeObjectTypeLabel = apfsObjectTypeLabel(omRow.treeObjectTypeRaw);
                                    omRow.treeObjectSubtype = readLe32(treeBuf, 28);
                                    omRow.treeBtnFlags = readLe16(treeBuf, 32);
                                    omRow.treeBtnLevel = readLe16(treeBuf, 34);
                                    omRow.treeBtnNkeys = readLe32(treeBuf, 36);
                                    omRow.treeTableSpaceOffset = readLe16(treeBuf, 40);
                                    omRow.treeTableSpaceLength = readLe16(treeBuf, 42);
                                    omRow.treeSampleHex = hexSampleBytes(treeBuf.data(), std::min<std::size_t>(treeBuf.size(), 96U));
                                    omRow.treeStatus = (omRow.treeObjectTypeLabel == "BTREE" || omRow.treeObjectTypeLabel == "BTREE_NODE") ? "VOLUME_OMAP_BTREE_ROOT_READ" : "VOLUME_OMAP_TREE_UNEXPECTED_OBJECT_TYPE";
                                    if (omRow.treeStatus == "VOLUME_OMAP_BTREE_ROOT_READ") omapTreeRootBuf = treeBuf;
                                } else {
                                    omRow.treeStatus = "VOLUME_OMAP_BTREE_ROOT_READ_FAILED";
                                    omRow.notes = treeErr;
                                }
                            } else if (omRow.omTreeOid == 0) {
                                omRow.treeStatus = "VOLUME_OMAP_TREE_OID_ZERO";
                            } else {
                                omRow.treeStatus = "VOLUME_OMAP_TREE_OFFSET_UNSAFE";
                            }
                        } else {
                            omRow.omapStatus = "VOLUME_OMAP_READ_FAILED";
                            omRow.interpretation = "Unable to read volume object-map physical object through the direct AFF4 reader.";
                            omRow.notes = omapErr;
                        }
                    }
                    directVolumeOmapRows.push_back(omRow);
                    if (omRow.treeStatus == "VOLUME_OMAP_BTREE_ROOT_READ" && !omapTreeRootBuf.empty()) {
                        directVolumeOmapBySequence[omRow.volumeSequence] = omRow;
                        directVolumeOmapRootBySequence[omRow.volumeSequence] = omapTreeRootBuf;
                    }

                    ApfsVolumeRootTreeLookupRow lookup;
                    lookup.sequence = static_cast<std::uint32_t>(directVolumeRootTreeLookupRows.size());
                    lookup.volumeSequence = row.sequence;
                    lookup.targetRole = row.targetRole;
                    lookup.fsOid = row.fsOid;
                    lookup.volumeName = row.volumeName;
                    lookup.apfsOmapOid = omRow.apfsOmapOid;
                    lookup.omTreeOid = omRow.omTreeOid;
                    lookup.apfsRootTreeOid = row.rootTreeOid;
                    lookup.targetXid = row.objectXid;
                    if (row.rootTreeOid == 0) {
                        lookup.lookupStatus = "VOLUME_ROOT_TREE_OID_ZERO";
                        lookup.interpretation = "Parsed APSB did not contain an apfs_root_tree_oid target.";
                    } else {
                        const ApfsOmapTargetResolution resolved = aff4ResolveVolumeOmapTargetObjectForProbe(
                            omRow,
                            omapTreeRootBuf,
                            row.rootTreeOid,
                            row.objectXid,
                            directBestNx.blockSize,
                            directReadVirtual,
                            safeDirectNodeOffset,
                            "Direct AFF4 APFS volume root-tree lookup");
                        lookup.branchDepth = resolved.branchDepth;
                        lookup.branchPath = resolved.branchPath;
                        lookup.leafOid = resolved.leafOid;
                        lookup.leafVirtualOffset = resolved.leafVirtualOffset;
                        lookup.leafBytesRead = resolved.leafBytesRead;
                        lookup.leafBtnFlags = resolved.leafBtnFlags;
                        lookup.leafBtnLevel = resolved.leafBtnLevel;
                        lookup.leafBtnNkeys = resolved.leafBtnNkeys;
                        lookup.matchedEntryIndex = resolved.matchedEntryIndex;
                        lookup.matchedKeyOid = resolved.matchedKeyOid;
                        lookup.matchedKeyXid = resolved.matchedKeyXid;
                        lookup.valueFlags = resolved.valueFlags;
                        lookup.valueSize = resolved.valueSize;
                        lookup.valuePaddr = resolved.valuePaddr;
                        lookup.resolvedVirtualOffset = resolved.resolvedVirtualOffset;
                        lookup.resolvedBytesRead = resolved.resolvedBytesRead;
                        lookup.resolvedObjectOid = resolved.resolvedObjectOid;
                        lookup.resolvedObjectXid = resolved.resolvedObjectXid;
                        lookup.resolvedObjectTypeRaw = resolved.resolvedObjectTypeRaw;
                        lookup.resolvedObjectTypeLabel = resolved.resolvedObjectTypeLabel;
                        lookup.resolvedObjectSubtype = resolved.resolvedObjectSubtype;
                        lookup.resolvedMagic = resolved.resolvedMagic;
                        lookup.resolvedBtnFlags = resolved.resolvedBtnFlags;
                        lookup.resolvedBtnLevel = resolved.resolvedBtnLevel;
                        lookup.resolvedBtnNkeys = resolved.resolvedBtnNkeys;
                        lookup.lookupStatus = resolved.lookupStatus == "OMAP_TARGET_LOOKUP_RESOLVED" ? "VOLUME_ROOT_TREE_LOOKUP_RESOLVED" : resolved.lookupStatus;
                        lookup.rootTreeStatus = resolved.objectStatus == "OMAP_TARGET_BTREE_READ" ? "ROOT_TREE_BTREE_READ" : resolved.objectStatus;
                        lookup.interpretation = resolved.interpretation;
                        lookup.sampleHex = resolved.sampleHex;
                        lookup.resolvedSampleHex = resolved.resolvedSampleHex;
                        lookup.notes = resolved.notes;
                        if (lookup.rootTreeStatus == "ROOT_TREE_BTREE_READ") appendDirectRootTreeNodeSample(lookup, resolved.resolvedBuffer);
                    }
                    directVolumeRootTreeLookupRows.push_back(lookup);
                };

                auto appendMappedObjectProbe = [&](std::uint32_t entryIndex,
                                                    std::uint64_t oid,
                                                    std::uint64_t fsOid,
                                                    std::uint64_t paddr,
                                                    const std::string& targetRole,
                                                    const std::string& sourceNotes,
                                                    std::uint64_t keyXid,
                                                    std::uint32_t valueFlags,
                                                    std::uint32_t valueSize) {
                    if (directCheckpointObjectRows.size() >= 512U || paddr == 0) return;
                    if (paddr > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(directBestNx.blockSize))) return;
                    const std::uint64_t objVo = paddr * static_cast<std::uint64_t>(directBestNx.blockSize);
                    std::vector<unsigned char> obj;
                    std::string objErr;
                    const bool objOk = readVirtualBytes(objVo, directBestNx.blockSize, obj, objErr);
                    ApfsCheckpointMappedObjectProbeRow pr;
                    pr.sequence = static_cast<std::uint32_t>(directCheckpointObjectRows.size() + 1U);
                    pr.entryIndex = entryIndex;
                    pr.cpmOid = oid;
                    pr.cpmFsOid = fsOid;
                    pr.cpmPaddr = paddr;
                    pr.virtualOffset = objVo;
                    pr.bytesRead = objOk ? static_cast<long long>(obj.size()) : -1;
                    pr.targetRole = targetRole;
                    pr.status = objOk ? "READ_OK" : "READ_FAILED";
                    pr.notes = objErr.empty() ? sourceNotes : (sourceNotes + "; " + objErr);
                    if (objOk && obj.size() >= 36U) {
                        pr.mappedOid = readLe64(obj, 8);
                        pr.mappedXid = readLe64(obj, 16);
                        pr.mappedTypeRaw = readLe32(obj, 24);
                        pr.mappedSubtype = readLe32(obj, 28);
                        pr.mappedTypeLabel = apfsObjectTypeLabel(pr.mappedTypeRaw);
                        if (pr.mappedTypeLabel == "FS") pr.magic.assign(reinterpret_cast<const char*>(obj.data() + 32), reinterpret_cast<const char*>(obj.data() + 36));
                        else if (pr.mappedTypeLabel == "OBJECT_MAP") pr.magic = "OMAP";
                        else if (obj.size() >= 36U) pr.magic.assign(reinterpret_cast<const char*>(obj.data() + 32), reinterpret_cast<const char*>(obj.data() + 36));
                        pr.sampleHex = hexSampleBytes(obj.data(), std::min<std::size_t>(obj.size(), 96U));
                        if (pr.magic == "APSB") pr.interpretation = "OMAP resolved an APFS volume superblock candidate.";
                        else if (pr.mappedTypeLabel == "OBJECT_MAP") pr.interpretation = "Direct object ID resolved an APFS object map.";
                        else pr.interpretation = "Direct APFS object resolution returned an object candidate.";

                        if (pr.magic == "APSB") {
                            ApfsVolumeSuperblockRow vr;
                            vr.sequence = static_cast<std::uint32_t>(directVolumeRows.size() + 1U);
                            vr.fsOid = oid;
                            vr.virtualOffset = objVo;
                            vr.bytesRead = pr.bytesRead;
                            vr.oid = pr.mappedOid;
                            vr.xid = pr.mappedXid;
                            vr.objectTypeRaw = pr.mappedTypeRaw;
                            vr.objectSubtype = pr.mappedSubtype;
                            vr.objectTypeLabel = pr.mappedTypeLabel;
                            vr.magic = pr.magic;
                            vr.status = "APSB_FOUND_VIA_DIRECT_OMAP";
                            vr.fsIndexCandidate = readLe32(obj, 36);
                            vr.featuresCandidate = readLe64(obj, 40);
                            vr.readonlyCompatibleFeaturesCandidate = readLe64(obj, 48);
                            vr.incompatibleFeaturesCandidate = readLe64(obj, 56);
                            vr.unmountTimeCandidate = readLe64(obj, 64);
                            vr.volumeUuidCandidate = bytesToUuidString(obj, 240);
                            vr.interpretation = "APFS volume superblock resolved from the container OMAP leaf using the direct AFF4 reader.";
                            vr.sampleHex = pr.sampleHex;
                            vr.notes = sourceNotes;
                            directVolumeRows.push_back(vr);

                            const std::string resolvedKey = std::to_string(oid) + ":" + std::to_string(paddr);
                            if (seenResolvedVolumes.insert(resolvedKey).second) {
                                ApfsResolvedVolumeSuperblockRow rr;
                                rr.sequence = static_cast<std::uint32_t>(directResolvedVolumeRows.size());
                                rr.targetRole = targetRole;
                                rr.fsOid = oid;
                                rr.containerTargetXid = directBestNx.nextXid;
                                rr.omapKeyOid = oid;
                                rr.omapKeyXid = keyXid;
                                rr.omapValueFlags = valueFlags;
                                rr.omapValueSize = valueSize;
                                rr.omapValuePaddr = paddr;
                                rr.resolvedVirtualOffset = objVo;
                                rr.resolvedBytesRead = pr.bytesRead;
                                parseResolvedApfsVolumeSuperblock(rr, obj, sourceNotes);
                                directResolvedVolumeRows.push_back(rr);
                                if (rr.status == "APSB_PARSED_FROM_CONTAINER_OMAP") appendDirectVolumeOmapAndRootLookup(rr);
                            }
                        }
                    } else {
                        pr.interpretation = "Direct APFS object resolution failed or returned too few bytes.";
                    }
                    directCheckpointObjectRows.push_back(pr);
                };

                auto probeDirectOmapLeaf = [&]() {
                    if (directBestNx.omapOid == 0) return;
                    appendMappedObjectProbe(0, directBestNx.omapOid, 0, directBestNx.omapOid, "DIRECT_NX_OBJECT_MAP_OID", "Direct read of nx_omap_oid as an APFS physical object ID.", directBestNx.nextXid, 0, 0);

                    if (directBestNx.omapOid > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(directBestNx.blockSize))) return;
                    std::vector<unsigned char> omap;
                    std::string omapErr;
                    if (!readVirtualBytes(directBestNx.omapOid * static_cast<std::uint64_t>(directBestNx.blockSize), directBestNx.blockSize, omap, omapErr)) return;
                    if (omap.size() < 88U || apfsObjectTypeLabel(readLe32(omap, 24)) != "OBJECT_MAP") return;
                    const std::uint64_t omTreeOid = readLe64(omap, 48);
                    if (omTreeOid == 0 || omTreeOid > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(directBestNx.blockSize))) return;
                    std::vector<unsigned char> tree;
                    std::string treeErr;
                    if (!readVirtualBytes(omTreeOid * static_cast<std::uint64_t>(directBestNx.blockSize), directBestNx.blockSize, tree, treeErr)) return;
                    if (tree.size() < 96U) return;
                    const std::uint16_t btnFlags = readLe16(tree, 32);
                    const std::uint16_t btnLevel = readLe16(tree, 34);
                    const std::uint32_t btnNkeys = std::min<std::uint32_t>(readLe32(tree, 36), 512U);
                    const std::uint16_t tableSpaceLength = readLe16(tree, 42);
                    const bool fixedKv = (btnFlags & 0x0004U) != 0;
                    const bool isRoot = (btnFlags & 0x0001U) != 0;
                    const bool isLeaf = (btnFlags & 0x0002U) != 0;
                    if (!fixedKv || !isLeaf || btnLevel != 0) return;
                    const std::size_t tocStart = isRoot ? 56U : 40U;
                    const std::size_t keyAreaStart = tocStart + static_cast<std::size_t>(tableSpaceLength);
                    const std::size_t valueAreaEnd = tree.size() - (isRoot ? 40U : 0U);
                    for (std::uint32_t entryIndex = 0; entryIndex < btnNkeys; ++entryIndex) {
                        const std::size_t toc = tocStart + static_cast<std::size_t>(entryIndex) * 4U;
                        if (toc + 4U > tree.size()) break;
                        const std::uint16_t keyOff = readLe16(tree, toc);
                        const std::uint16_t valOff = readLe16(tree, toc + 2U);
                        const std::size_t keyAbs = keyAreaStart + static_cast<std::size_t>(keyOff);
                        const std::size_t valAbs = valueAreaEnd >= static_cast<std::size_t>(valOff) ? valueAreaEnd - static_cast<std::size_t>(valOff) : tree.size();
                        if (keyAbs + 16U > tree.size() || valAbs + 16U > tree.size()) continue;
                        const std::uint64_t keyOid = readLe64(tree, keyAbs);
                        const std::uint64_t keyXid = readLe64(tree, keyAbs + 8U);
                        const std::uint32_t valFlags = readLe32(tree, valAbs);
                        const std::uint32_t valSize = readLe32(tree, valAbs + 4U);
                        const std::uint64_t valPaddr = readLe64(tree, valAbs + 8U);
                        (void)keyXid;
                        (void)valFlags;
                        (void)valSize;
                        if (keyOid == directBestNx.omapOid) {
                            appendMappedObjectProbe(entryIndex, keyOid, 0, valPaddr, "OMAP_LEAF_NX_OBJECT_MAP_SELF", "Container OMAP leaf entry for nx_omap_oid.", keyXid, valFlags, valSize);
                        } else if (containsU64(directBestNx.fsOids, keyOid)) {
                            appendMappedObjectProbe(entryIndex, keyOid, keyOid, valPaddr, "OMAP_LEAF_NX_FILESYSTEM_OID", "Container OMAP leaf entry for APFS filesystem OID.", keyXid, valFlags, valSize);
                        }
                    }
                };

                probeDirectOmapLeaf();

                auto directIsSpotlightStoreV2TopLevelComponentName = [&](const std::string& name) -> bool {
                    const std::string lname = asciiLower(name);
                    if (lname == "0.directorystorefile" || lname == "0.directorystorefile.shadow") return true;
                    if (lname.rfind("0.index", 0) == 0 || lname.rfind("0.shadowindex", 0) == 0) return true;
                    if (lname.rfind("live.", 0) == 0) return true;
                    if (lname == "reversestore.updates" || lname == "store.updates" || lname == "store_generation") return true;
                    if (lname == "reversedirectorystore" || lname == "reversedirectorystore.shadow") return true;
                    if (lname == "permstore" || lname == "journalexclusion" || lname == "journals.migration_secondchance") return true;
                    if (lname == "cab.created" || lname == "cab.modified" || lname == "lion.created" || lname == "lion.modified" || lname == "star.created" || lname == "star.modified") return true;
                    if (lname == "tmp.cab" || lname == "tmp.lion" || lname == "tmp.star") return true;
                    if (lname.rfind("dbstr-", 0) == 0 || lname.rfind("dbhdr-", 0) == 0) return true;
                    if (lname.rfind("tmp.spotlight", 0) == 0) return true;
                    return false;
                };
                auto directIsSpotlightTargetName = [&](const std::string& name) -> bool {
                    const std::string lname = asciiLower(name);
                    return lname == ".spotlight-v100" || lname == "store-v2" || lname == "index.db" ||
                           directIsSpotlightStoreV2TopLevelComponentName(lname) ||
                           lname.find("spotlight") != std::string::npos || lname.find("corespotlight") != std::string::npos;
                };
                auto directSpotlightTargetKind = [&](const std::string& name) -> std::string {
                    const std::string lname = asciiLower(name);
                    if (lname == ".spotlight-v100") return "SPOTLIGHT_ROOT_DIRECTORY";
                    if (lname == "store-v2") return "SPOTLIGHT_STORE_V2_DIRECTORY";
                    if (lname == "store.db" || lname == ".store.db") return "SPOTLIGHT_STORE_DB_FILE";
                    if (lname == "store.db-wal" || lname == "store.db-shm") return "SPOTLIGHT_STORE_SQLITE_SUPPORT_FILE";
                    if (lname.rfind("dbstr-", 0) == 0) return "SPOTLIGHT_DBSTR_FILE";
                    if (lname.rfind("dbhdr-", 0) == 0) return "SPOTLIGHT_DBHDR_FILE";
                    if (directIsSpotlightStoreV2TopLevelComponentName(lname)) return "SPOTLIGHT_STOREV2_TOPLEVEL_COMPONENT";
                    if (lname.find("corespotlight") != std::string::npos) return "IOS_CORESPOTLIGHT_NAME";
                    if (lname == "index.db") return "IOS_CORESPOTLIGHT_INDEX_DB_FILE";
                    return "SPOTLIGHT_RELATED_NAME";
                };
                auto appendDirectSpotlightCopyAttempt = [&](const ApfsRootTreeRecordSampleRow& rr) {
                    ApfsSpotlightCopyAttemptRow cr;
                    cr.sequence = static_cast<std::uint32_t>(directSpotlightCopyAttemptRows.size());
                    cr.volumeSequence = rr.volumeSequence;
                    cr.targetRole = rr.targetRole;
                    cr.fsOid = rr.fsOid;
                    cr.volumeName = rr.volumeName;
                    cr.parentObjectId = rr.keyObjectId;
                    cr.childFileId = rr.valueU64_0;
                    cr.targetName = rr.decodedName;
                    cr.targetKind = directSpotlightTargetKind(rr.decodedName);
                    if (rr.keyTypeRaw != 9U) {
                        cr.extractionStatus = "COPY_NOT_ATTEMPTED_NOT_DIRECTORY_RECORD";
                        cr.interpretation = "A Spotlight-related key/name was observed during bounded APFS scan, but it was not a directory-entry record with a child file ID.";
                    } else if (cr.childFileId == 0) {
                        cr.extractionStatus = "COPY_NOT_ATTEMPTED_NO_CHILD_FILE_ID";
                        cr.interpretation = "A Spotlight-related directory-entry name was decoded, but the value did not provide a usable child file ID candidate.";
                    } else if (cr.targetKind == "SPOTLIGHT_ROOT_DIRECTORY" || cr.targetKind == "SPOTLIGHT_STORE_V2_DIRECTORY" || cr.targetKind == "IOS_CORESPOTLIGHT_NAME") {
                        cr.extractionStatus = "COPY_NOT_ATTEMPTED_DIRECTORY_RECURSION_PENDING";
                        cr.interpretation = "A Spotlight-related directory was found through the direct AFF4/APFS map reader. The next step is path-scoped child enumeration and then file extent copy-out.";
                    } else {
                        cr.extractionStatus = "COPY_NOT_ATTEMPTED_FILE_EXTENTS_PENDING";
                        cr.interpretation = "A Spotlight-related file name was found through the direct AFF4/APFS map reader. File-byte extraction remains gated until inode, xattr, and file extent provenance is resolved.";
                    }
                    cr.notes = "direct_aff4_map_reader=1; bounded_btree_scan=1; copy_out_intentionally_gated_for_forensic_provenance";
                    directSpotlightCopyAttemptRows.push_back(cr);
                };

                struct DirectFsPendingNode {
                    std::uint32_t volumeSequence = 0;
                    std::string targetRole;
                    std::uint64_t fsOid = 0;
                    std::string volumeName;
                    std::uint64_t apfsRootTreeOid = 0;
                    std::uint64_t nodeOid = 0;
                    std::uint64_t nodeVirtualOffset = 0;
                    std::uint64_t targetXid = 0;
                    std::uint32_t depth = 0;
                    bool nodeAlreadyResolved = false;
                };
                std::vector<DirectFsPendingNode> directPending;
                std::set<std::string> directSeenNodes;
                auto enqueueDirectNode = [&](const DirectFsPendingNode& n) {
                    if (n.nodeOid == 0) return;
                    const std::string key = std::to_string(n.volumeSequence) + ":" + std::to_string(n.nodeOid);
                    if (!directSeenNodes.insert(key).second) return;
                    directPending.push_back(n);
                };
                for (const auto& lookup : directVolumeRootTreeLookupRows) {
                    if (lookup.rootTreeStatus != "ROOT_TREE_BTREE_READ" || lookup.resolvedVirtualOffset == 0) continue;
                    DirectFsPendingNode pn;
                    pn.volumeSequence = lookup.volumeSequence;
                    pn.targetRole = lookup.targetRole;
                    pn.fsOid = lookup.fsOid;
                    pn.volumeName = lookup.volumeName;
                    pn.apfsRootTreeOid = lookup.apfsRootTreeOid;
                    pn.nodeOid = lookup.resolvedObjectOid ? lookup.resolvedObjectOid : lookup.apfsRootTreeOid;
                    pn.nodeVirtualOffset = lookup.resolvedVirtualOffset;
                    pn.targetXid = lookup.targetXid;
                    pn.depth = 0;
                    pn.nodeAlreadyResolved = true;
                    // Data volume first; other volumes are queued after it.
                    if (lookup.volumeName == "Data") directPending.insert(directPending.begin(), pn);
                    else enqueueDirectNode(pn);
                }

                constexpr std::size_t kDirectSpotlightDiagnosticNameSampleLimit = 200000U;
                for (std::size_t qi = 0; qi < directPending.size(); ++qi) {
                    const DirectFsPendingNode pending = directPending[qi];
                    std::vector<unsigned char> node;
                    std::string nodeErr;
                    std::uint64_t nodeOffset = pending.nodeVirtualOffset;
                    std::uint64_t nodeOid = pending.nodeOid;
                    long long nodeRead = -1;
                    if (pending.nodeAlreadyResolved) {
                        nodeRead = directReadVirtual(nodeOffset, directBestNx.blockSize, node, nodeErr);
                    } else {
                        const auto omIt = directVolumeOmapBySequence.find(pending.volumeSequence);
                        const auto rootIt = directVolumeOmapRootBySequence.find(pending.volumeSequence);
                        if (omIt == directVolumeOmapBySequence.end() || rootIt == directVolumeOmapRootBySequence.end()) continue;
                        const ApfsOmapTargetResolution resolved = aff4ResolveVolumeOmapTargetObjectForProbe(
                            omIt->second,
                            rootIt->second,
                            pending.nodeOid,
                            pending.targetXid,
                            directBestNx.blockSize,
                            directReadVirtual,
                            safeDirectNodeOffset,
                            "Direct AFF4/APFS bounded filesystem-tree scan");
                        if (resolved.objectStatus != "OMAP_TARGET_BTREE_READ" || resolved.resolvedBuffer.size() < 64U) continue;
                        node = resolved.resolvedBuffer;
                        nodeRead = resolved.resolvedBytesRead;
                        nodeOffset = resolved.resolvedVirtualOffset;
                        nodeOid = resolved.resolvedObjectOid ? resolved.resolvedObjectOid : pending.nodeOid;
                    }
                    if (nodeRead <= 0 || node.size() < 64U) continue;
                    ++directSpotlightScanMetrics.nodesVisited;
                    ++directSpotlightScanMetrics.nodesResolved;
                    const std::uint32_t rawType = readLe32(node, 24);
                    const std::string label = apfsObjectTypeLabel(rawType);
                    if (label != "BTREE" && label != "BTREE_NODE") continue;
                    const std::uint16_t btnLevel = readLe16(node, 34);
                    const std::uint32_t nkeys = std::min<std::uint32_t>(readLe32(node, 36), 65536U);
                    if (btnLevel == 0) ++directSpotlightScanMetrics.leafNodes;
                    else ++directSpotlightScanMetrics.branchNodes;
                    const std::uint32_t recordLimit = nkeys;
                    for (std::uint32_t i = 0; i < recordLimit; ++i) {
                        std::size_t tocAbs = 0, keyAbs = 0, keyLen = 0, valAbs = 0, valLen = 0;
                        std::string detail;
                        if (!aff4GenericBtreeKvAbsForProbe(node, i, tocAbs, keyAbs, keyLen, valAbs, valLen, detail)) continue;
                        ++directSpotlightScanMetrics.recordsScanned;
                        ApfsRootTreeRecordSampleRow rr;
                        rr.sequence = static_cast<std::uint32_t>(directSpotlightNameSampleRows.size() + directSpotlightTargetRows.size());
                        rr.volumeSequence = pending.volumeSequence;
                        rr.targetRole = pending.targetRole;
                        rr.fsOid = pending.fsOid;
                        rr.volumeName = pending.volumeName;
                        rr.apfsRootTreeOid = pending.apfsRootTreeOid;
                        rr.nodeOid = nodeOid;
                        rr.nodeVirtualOffset = nodeOffset;
                        rr.nodeLevel = btnLevel;
                        rr.nodeNkeys = nkeys;
                        rr.entryIndex = i;
                        rr.tocOffset = static_cast<std::uint32_t>(tocAbs > 0xffffffffULL ? 0xffffffffULL : tocAbs);
                        rr.keyOffset = static_cast<std::uint16_t>(keyAbs > 0xffffU ? 0xffffU : keyAbs);
                        rr.keyLength = static_cast<std::uint16_t>(keyLen > 0xffffU ? 0xffffU : keyLen);
                        rr.valueOffset = static_cast<std::uint16_t>(valAbs > 0xffffU ? 0xffffU : valAbs);
                        rr.valueLength = static_cast<std::uint16_t>(valLen > 0xffffU ? 0xffffU : valLen);
                        if (keyLen >= 8U) {
                            rr.keyRaw = readLe64(node, keyAbs);
                            rr.keyObjectId = apfsFsKeyObjectId(rr.keyRaw);
                            rr.keyTypeRaw = apfsFsKeyRecordType(rr.keyRaw);
                            rr.keyTypeLabel = apfsFsRecordTypeLabel(rr.keyTypeRaw);
                        }
                        if (valLen >= 8U && valAbs + 8U <= node.size()) rr.valueU64_0 = readLe64(node, valAbs);
                        if (valLen >= 16U && valAbs + 16U <= node.size()) rr.valueU64_1 = readLe64(node, valAbs + 8U);
                        if (valLen >= 24U && valAbs + 24U <= node.size()) rr.valueU64_2 = readLe64(node, valAbs + 16U);
                        if (btnLevel > 0 && valLen >= 8U && valAbs + 8U <= node.size()) {
                            rr.branchChildOid = readLe64(node, valAbs);
                            if (rr.branchChildOid != 0) {
                                DirectFsPendingNode child;
                                child.volumeSequence = pending.volumeSequence;
                                child.targetRole = pending.targetRole;
                                child.fsOid = pending.fsOid;
                                child.volumeName = pending.volumeName;
                                child.apfsRootTreeOid = pending.apfsRootTreeOid;
                                child.nodeOid = rr.branchChildOid;
                                child.targetXid = pending.targetXid;
                                child.depth = pending.depth + 1U;
                                child.nodeAlreadyResolved = false;
                                enqueueDirectNode(child);
                                ++directSpotlightScanMetrics.branchCandidatesQueued;
                            }
                        }
                        if (rr.keyTypeRaw == 9U && keyLen > 12U) {
                            const std::uint32_t nameLenAndHash = readLe32(node, keyAbs + 8U);
                            const std::size_t nameLen = static_cast<std::size_t>(nameLenAndHash & 0x000003ffU);
                            rr.decodedName = safePrintableUtf8Fragment(node, keyAbs + 12U, std::min<std::size_t>(nameLen, keyLen - 12U));
                            if (!rr.decodedName.empty()) {
                                ++directSpotlightScanMetrics.dirRecordsDecoded;
                                if (rr.valueU64_0 != 0) {
                                    ApfsDirectoryRecordEntry de;
                                    de.volumeSequence = rr.volumeSequence;
                                    de.targetRole = rr.targetRole;
                                    de.fsOid = rr.fsOid;
                                    de.volumeName = rr.volumeName;
                                    de.parentObjectId = rr.keyObjectId;
                                    de.childFileId = rr.valueU64_0;
                                    de.name = rr.decodedName;
                                    directDirectoryRecordEntries.push_back(std::move(de));
                                }
                            }
                        }
                        if (btnLevel == 0 && rr.keyTypeRaw == 3U && valLen >= 16U && valAbs + 16U <= node.size()) {
                            ApfsSpotlightInodeProbeRow ir;
                            ir.sequence = static_cast<std::uint32_t>(directIndexedInodeByObject.size());
                            ir.volumeSequence = rr.volumeSequence;
                            ir.targetRole = rr.targetRole;
                            ir.fsOid = rr.fsOid;
                            ir.volumeName = rr.volumeName;
                            ir.inodeObjectId = rr.keyObjectId;
                            if (valLen >= 8U && valAbs + 8U <= node.size()) ir.inodeParentId = readLe64(node, valAbs + 0U);
                            if (valLen >= 16U && valAbs + 16U <= node.size()) ir.inodePrivateId = readLe64(node, valAbs + 8U);
                            if (valLen >= 24U && valAbs + 24U <= node.size()) ir.inodeCreateTimeRaw = readLe64(node, valAbs + 16U);
                            if (valLen >= 32U && valAbs + 32U <= node.size()) ir.inodeModTimeRaw = readLe64(node, valAbs + 24U);
                            if (valLen >= 40U && valAbs + 40U <= node.size()) ir.inodeChangeTimeRaw = readLe64(node, valAbs + 32U);
                            if (valLen >= 48U && valAbs + 48U <= node.size()) ir.inodeAccessTimeRaw = readLe64(node, valAbs + 40U);
                            if (valLen >= 56U && valAbs + 56U <= node.size()) ir.inodeInternalFlags = readLe64(node, valAbs + 48U);
                            if (valLen >= 60U && valAbs + 60U <= node.size()) ir.inodeNchildrenOrNlink = readLe32(node, valAbs + 56U);
                            if (valLen >= 84U && valAbs + 84U <= node.size()) ir.inodeModeCandidate = readLe16(node, valAbs + 82U);
                            if (valLen >= 92U && valAbs + 92U <= node.size()) ir.inodeUncompressedSize = readLe64(node, valAbs + 84U);
                            const ApfsInodeExtendedFieldDecode xf = decodeApfsInodeExtendedFieldsForProbe(node, valAbs, valLen);
                            ir.inodeXfieldStatus = xf.status;
                            if (xf.sawDstream) {
                                ir.inodeDstreamSize = xf.dstreamSize;
                                ir.inodeDstreamAllocedSize = xf.dstreamAllocedSize;
                                ir.inodeDstreamDefaultCryptoId = xf.dstreamDefaultCryptoId;
                            }
                            ir.nodeOid = nodeOid;
                            ir.nodeVirtualOffset = nodeOffset;
                            ir.nodeLevel = btnLevel;
                            ir.nodeNkeys = nkeys;
                            ir.entryIndex = i;
                            ir.inodeStatus = "DIRECT_INDEXED_INODE_RECORD";
                            ir.interpretation = "INODE record decoded during exhausted direct AFF4/APFS filesystem B-tree traversal and cached for target-guided Store-V2 copy-out.";
                            if (valLen > 0 && valAbs < node.size()) ir.valueSampleHex = hexSampleBytes(node.data() + valAbs, std::min<std::size_t>(std::min<std::size_t>(valLen, node.size() - valAbs), 96U));
                            ir.notes = detail;
                            directIndexedInodeByObject[std::make_pair(rr.volumeSequence, rr.keyObjectId)] = std::move(ir);
                        }
                        if (btnLevel == 0 && rr.keyTypeRaw == 8U && keyLen >= 16U && valLen >= 16U && valAbs + 16U <= node.size()) {
                            ApfsSpotlightFileExtentProbeRow er;
                            er.sequence = static_cast<std::uint32_t>(directIndexedFileExtentsByObject.size());
                            er.volumeSequence = rr.volumeSequence;
                            er.targetRole = rr.targetRole;
                            er.fsOid = rr.fsOid;
                            er.volumeName = rr.volumeName;
                            er.extentFileId = rr.keyObjectId;
                            er.extentLogicalOffset = readLe64(node, keyAbs + 8U);
                            er.lenAndFlags = readLe64(node, valAbs + 0U);
                            er.extentLengthBytes = er.lenAndFlags & 0x00ffffffffffffffULL;
                            er.extentFlags = static_cast<std::uint32_t>((er.lenAndFlags >> 56U) & 0xffU);
                            er.physicalBlock = readLe64(node, valAbs + 8U);
                            if (valLen >= 24U && valAbs + 24U <= node.size()) er.cryptoId = readLe64(node, valAbs + 16U);
                            if (directBestNx.blockSize != 0 && er.physicalBlock <= (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(directBestNx.blockSize))) {
                                er.physicalOffset = er.physicalBlock * static_cast<std::uint64_t>(directBestNx.blockSize);
                            }
                            er.nodeOid = nodeOid;
                            er.nodeVirtualOffset = nodeOffset;
                            er.nodeLevel = btnLevel;
                            er.nodeNkeys = nkeys;
                            er.entryIndex = i;
                            er.extentStatus = "DIRECT_INDEXED_FILE_EXTENT_RECORD";
                            er.interpretation = "FILE_EXTENT record decoded during exhausted direct AFF4/APFS filesystem B-tree traversal and cached for target-guided Store-V2 copy-out.";
                            er.notes = detail;
                            directIndexedFileExtentsByObject[std::make_pair(rr.volumeSequence, rr.keyObjectId)].push_back(std::move(er));
                        }
                        rr.keySampleHex = (keyLen > 0 && keyAbs < node.size()) ? hexSampleBytes(node.data() + keyAbs, std::min<std::size_t>(keyLen, 64U)) : std::string{};
                        rr.valueSampleHex = (valLen > 0 && valAbs < node.size()) ? hexSampleBytes(node.data() + valAbs, std::min<std::size_t>(valLen, 64U)) : std::string{};
                        rr.status = "DIRECT_AFF4_APFS_BTREE_SCAN_RECORD_DECODED";
                        rr.interpretation = btnLevel == 0 ? "Leaf filesystem-tree record decoded during direct AFF4/APFS bounded Spotlight target scan." : "Branch filesystem-tree separator record decoded during direct AFF4/APFS bounded Spotlight target scan.";
                        rr.notes = detail + "; direct_scan_depth=" + std::to_string(pending.depth);
                        if (!rr.decodedName.empty() && directSpotlightNameSampleRows.size() < kDirectSpotlightDiagnosticNameSampleLimit) directSpotlightNameSampleRows.push_back(rr);
                        if (btnLevel == 0 && directIsSpotlightTargetName(rr.decodedName)) {
                            directSpotlightTargetRows.push_back(rr);
                            appendDirectSpotlightCopyAttempt(rr);
                            ++directSpotlightScanMetrics.targetNameHits;
                        }
                    }
                }
                directSpotlightScanMetrics.nodesSkippedByLimit = 0;

                // V1.0.6: The direct AFF4/APFS path now walks the APFS root-tree queue to exhaustion
                // using the visited-node set as the cycle guard.  It also keeps directory-record rows
                // separately from the bounded diagnostic name-sample CSV, so recursive Store-V2 child
                // discovery is no longer blocked by the upload sample size.
                {
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<std::size_t>> childrenByParent;
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::string> nameByObject;
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::uint64_t> parentByObject;
                    for (std::size_t i = 0; i < directDirectoryRecordEntries.size(); ++i) {
                        const auto& e = directDirectoryRecordEntries[i];
                        childrenByParent[std::make_pair(e.volumeSequence, e.parentObjectId)].push_back(i);
                        if (e.childFileId != 0 && e.parentObjectId != 0) parentByObject[std::make_pair(e.volumeSequence, e.childFileId)] = e.parentObjectId;
                        if (e.childFileId != 0 && !e.name.empty()) nameByObject[std::make_pair(e.volumeSequence, e.childFileId)] = e.name;
                    }
                    auto directPathForObject = [&](std::uint32_t volumeSequence, std::uint64_t objectId) -> std::string {
                        std::vector<std::string> parts;
                        std::set<std::uint64_t> seen;
                        std::uint64_t cur = objectId;
                        while (cur != 0 && seen.insert(cur).second) {
                            const auto nameIt = nameByObject.find(std::make_pair(volumeSequence, cur));
                            if (nameIt != nameByObject.end() && !nameIt->second.empty()) parts.push_back(nameIt->second);
                            if (cur == 2) break;
                            const auto parentIt = parentByObject.find(std::make_pair(volumeSequence, cur));
                            if (parentIt == parentByObject.end() || parentIt->second == cur) break;
                            cur = parentIt->second;
                        }
                        std::string path = (volumeSequence == 4) ? "/System/Volumes/Data" : ("/vol_" + std::to_string(volumeSequence));
                        for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
                            if (it->empty() || *it == "/") continue;
                            if (path.empty() || path.back() != '/') path += "/";
                            path += *it;
                        }
                        return path;
                    };
                    std::set<std::tuple<std::uint32_t, std::uint64_t, std::uint64_t, std::string>> seenAttempts;
                    for (const auto& cr : directSpotlightCopyAttemptRows) {
                        seenAttempts.insert(std::make_tuple(cr.volumeSequence, cr.parentObjectId, cr.childFileId, asciiLower(cr.targetName)));
                    }
                    struct DirectStoreWalkItem {
                        std::uint32_t volumeSequence = 0;
                        std::uint64_t dirObjectId = 0;
                        std::uint64_t groupRootObjectId = 0;
                        std::string groupName;
                        std::string relPrefix;
                    };
                    std::vector<DirectStoreWalkItem> storeWalk;
                    std::set<std::pair<std::uint32_t, std::uint64_t>> queuedStoreDirs;
                    auto enqueueStoreDir = [&](std::uint32_t vol, std::uint64_t dir, std::uint64_t root, const std::string& group, const std::string& rel) {
                        if (dir == 0) return;
                        auto key = std::make_pair(vol, dir);
                        if (!childrenByParent.count(key)) return;
                        if (!queuedStoreDirs.insert(key).second) return;
                        DirectStoreWalkItem wi;
                        wi.volumeSequence = vol;
                        wi.dirObjectId = dir;
                        wi.groupRootObjectId = root ? root : dir;
                        wi.groupName = group;
                        wi.relPrefix = rel;
                        storeWalk.push_back(std::move(wi));
                    };
                    auto directGroupNameForDir = [&](std::uint32_t vol, std::uint64_t dir) -> std::string {
                        const auto it = nameByObject.find(std::make_pair(vol, dir));
                        if (it == nameByObject.end()) return {};
                        return isLikelyStoreV2GroupDirectoryName(it->second) ? it->second : std::string{};
                    };
                    for (const auto& e : directDirectoryRecordEntries) {
                        const std::string lname = asciiLower(e.name);
                        if (lname == "store-v2") enqueueStoreDir(e.volumeSequence, e.childFileId, e.childFileId, "", "");
                        if (directIsSpotlightStoreV2TopLevelComponentName(lname)) enqueueStoreDir(e.volumeSequence, e.parentObjectId, e.parentObjectId, directGroupNameForDir(e.volumeSequence, e.parentObjectId), "");
                    }
                    for (std::size_t qi = 0; qi < storeWalk.size(); ++qi) {
                        const auto item = storeWalk[qi];
                        const auto childIt = childrenByParent.find(std::make_pair(item.volumeSequence, item.dirObjectId));
                        if (childIt == childrenByParent.end()) continue;
                        for (const std::size_t idx : childIt->second) {
                            const auto& e = directDirectoryRecordEntries[idx];
                            const std::string lname = asciiLower(e.name);
                            if (lname == ".spotlight-v100" || lname == "store-v2") continue;
                            const bool childIsDir = childrenByParent.count(std::make_pair(e.volumeSequence, e.childFileId)) != 0;
                            std::uint64_t groupRoot = item.groupRootObjectId;
                            std::string groupName = item.groupName;
                            std::string relPrefix = item.relPrefix;
                            if (groupName.empty() && childIsDir && isLikelyStoreV2GroupDirectoryName(e.name)) {
                                groupRoot = e.childFileId;
                                groupName = e.name;
                                relPrefix.clear();
                            }
                            const std::string relPath = relPrefix.empty() ? e.name : (relPrefix + "/" + e.name);
                            if (childIsDir) { enqueueStoreDir(e.volumeSequence, e.childFileId, groupRoot, groupName, relPath); continue; }
                            const auto dedupe = std::make_tuple(e.volumeSequence, e.parentObjectId, e.childFileId, lname);
                            if (!seenAttempts.insert(dedupe).second) continue;
                            ApfsSpotlightCopyAttemptRow cr;
                            cr.sequence = static_cast<std::uint32_t>(directSpotlightCopyAttemptRows.size());
                            cr.volumeSequence = e.volumeSequence;
                            cr.targetRole = e.targetRole;
                            cr.fsOid = e.fsOid;
                            cr.volumeName = e.volumeName;
                            cr.parentObjectId = e.parentObjectId;
                            cr.childFileId = e.childFileId;
                            cr.targetName = e.name;
                            cr.targetKind = directSpotlightTargetKind(e.name);
                            cr.storeV2RootObjectId = groupRoot;
                            cr.storeV2GroupName = groupName;
                            cr.storeV2RelativePath = relPath;
                            cr.extractionStatus = "COPY_NOT_ATTEMPTED_FILE_EXTENTS_PENDING";
                            cr.interpretation = "Direct AFF4/APFS Store-V2 recursive namespace row. This ordinary-named file is under a Store-V2 directory/group and is eligible for target-guided inode/extent lookup.";
                            cr.notes = "direct_aff4_v1_0_4_recursive_storev2_seed=1; group_root_object_id=" + std::to_string(groupRoot) + "; group_name=" + groupName + "; rel_path=" + relPath + "; apfs_absolute_path=" + directPathForObject(e.volumeSequence, e.childFileId);
                            directSpotlightCopyAttemptRows.push_back(std::move(cr));
                        }
                    }
                }

                // V1.0.6: Direct target-guided record correlation and guarded copy-out.
                // The previous direct path found Store-V2 names but did not attach those child IDs to
                // INODE/FILE_EXTENT rows.  Because the exhausted root-tree traversal above now indexes
                // leaf INODE and FILE_EXTENT records, copy attempts can be materialized without another
                // broad APFS scan.  This remains conservative: only ordered, readable extents are staged,
                // and sparse/zero regions are explicitly recorded.
                {
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<std::size_t>> childrenByParent;
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::string> nameByObject;
                    std::map<std::pair<std::uint32_t, std::uint64_t>, std::uint64_t> parentByObject;
                    for (std::size_t i = 0; i < directDirectoryRecordEntries.size(); ++i) {
                        const auto& e = directDirectoryRecordEntries[i];
                        childrenByParent[std::make_pair(e.volumeSequence, e.parentObjectId)].push_back(i);
                        if (e.childFileId != 0 && e.parentObjectId != 0) parentByObject[std::make_pair(e.volumeSequence, e.childFileId)] = e.parentObjectId;
                        if (e.childFileId != 0 && !e.name.empty()) nameByObject[std::make_pair(e.volumeSequence, e.childFileId)] = e.name;
                    }
                    auto directPathForObject = [&](std::uint32_t volumeSequence, std::uint64_t objectId) -> std::string {
                        std::vector<std::string> parts;
                        std::set<std::uint64_t> seen;
                        std::uint64_t cur = objectId;
                        while (cur != 0 && seen.insert(cur).second) {
                            const auto nameIt = nameByObject.find(std::make_pair(volumeSequence, cur));
                            if (nameIt != nameByObject.end() && !nameIt->second.empty()) parts.push_back(nameIt->second);
                            if (cur == 2) break;
                            const auto parentIt = parentByObject.find(std::make_pair(volumeSequence, cur));
                            if (parentIt == parentByObject.end() || parentIt->second == cur) break;
                            cur = parentIt->second;
                        }
                        std::string path = (volumeSequence == 4) ? "/System/Volumes/Data" : ("/vol_" + std::to_string(volumeSequence));
                        for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
                            if (it->empty() || *it == "/") continue;
                            if (path.empty() || path.back() != '/') path += "/";
                            path += *it;
                        }
                        return path;
                    };
                    auto directPreviewStatusForBytes = [](const std::vector<unsigned char>& bytes) -> std::string {
                        if (bytes.size() >= 16U && std::memcmp(bytes.data(), "SQLite format 3", 15U) == 0) return "SQLITE_HEADER";
                        if (bytes.size() >= 6U && std::memcmp(bytes.data(), "bplist", 6U) == 0) return "BPLIST_HEADER";
                        bool anyNonZero = false;
                        for (unsigned char b : bytes) { if (b != 0U) { anyNonZero = true; break; } }
                        return anyNonZero ? "NONZERO_BYTES" : "ALL_ZERO_BYTES";
                    };
                    std::set<std::uint32_t> inodeMaterializedForTarget;
                    std::map<std::uint32_t, std::uint64_t> directPrivateIdByTarget;
                    for (const auto& cr : directSpotlightCopyAttemptRows) {
                        if (cr.childFileId == 0) continue;
                        const auto inodeIt = directIndexedInodeByObject.find(std::make_pair(cr.volumeSequence, cr.childFileId));
                        if (inodeIt == directIndexedInodeByObject.end()) continue;
                        ApfsSpotlightInodeProbeRow ir = inodeIt->second;
                        ir.sequence = static_cast<std::uint32_t>(directSpotlightInodeRows.size());
                        ir.targetSequence = cr.sequence;
                        ir.targetParentObjectId = cr.parentObjectId;
                        ir.targetChildFileId = cr.childFileId;
                        ir.targetName = cr.targetName;
                        ir.targetKind = cr.targetKind;
                        ir.inodeStatus = (ir.inodeParentId == cr.parentObjectId || cr.parentObjectId == 0) ? "TARGET_INODE_DIRECT_INDEX_HIT" : "TARGET_INODE_DIRECT_INDEX_PARENT_MISMATCH";
                        ir.interpretation = "Direct AFF4/APFS exhausted filesystem-tree index matched this Store-V2 directory-entry child ID to an INODE record.";
                        ir.notes += (ir.notes.empty() ? std::string{} : "; ") + std::string("direct_aff4_v1_0_5_indexed_inode=1; apfs_absolute_path=") + directPathForObject(cr.volumeSequence, cr.childFileId);
                        directPrivateIdByTarget[cr.sequence] = ir.inodePrivateId;
                        if (inodeMaterializedForTarget.insert(cr.sequence).second) directSpotlightInodeRows.push_back(std::move(ir));
                    }

                    auto directObjectCandidatesForExtent = [&](const ApfsSpotlightCopyAttemptRow& cr) {
                        std::vector<std::pair<std::uint64_t, std::string>> out;
                        std::set<std::uint64_t> seen;
                        auto add = [&](std::uint64_t v, const std::string& label) {
                            if (v == 0) return;
                            if (seen.insert(v).second) out.push_back(std::make_pair(v, label));
                        };
                        add(cr.childFileId, "child_file_id");
                        const auto pIt = directPrivateIdByTarget.find(cr.sequence);
                        const std::uint64_t privateId = (pIt == directPrivateIdByTarget.end()) ? 0ULL : pIt->second;
                        add(privateId, "inode_private_id_dstream_candidate");
                        if (privateId != 0) add(privateId >> 4U, "inode_private_id_shifted_right_4");
                        const std::uint64_t shiftedChild = cr.childFileId >> 4U;
                        add(shiftedChild, "child_file_id_shifted_right_4");
                        for (std::uint64_t prefix = 1; prefix <= 15; ++prefix) add((prefix << 56U) | shiftedChild, "high_prefix_plus_child_shifted_right_4");
                        return out;
                    };

                    std::map<std::uint32_t, std::vector<ApfsSpotlightFileExtentProbeRow>> extentsByTargetSequence;
                    std::set<std::tuple<std::uint32_t, std::uint64_t, std::uint64_t, std::uint64_t>> seenTargetExtents;
                    for (const auto& cr : directSpotlightCopyAttemptRows) {
                        if (cr.childFileId == 0) continue;
                        if (cr.targetKind == "SPOTLIGHT_ROOT_DIRECTORY" || cr.targetKind == "SPOTLIGHT_STORE_V2_DIRECTORY" || cr.targetKind == "IOS_CORESPOTLIGHT_NAME") continue;
                        for (const auto& cand : directObjectCandidatesForExtent(cr)) {
                            const auto erIt = directIndexedFileExtentsByObject.find(std::make_pair(cr.volumeSequence, cand.first));
                            if (erIt == directIndexedFileExtentsByObject.end()) continue;
                            for (const auto& er0 : erIt->second) {
                                const auto key = std::make_tuple(cr.sequence, er0.extentFileId, er0.extentLogicalOffset, er0.physicalBlock);
                                if (!seenTargetExtents.insert(key).second) continue;
                                ApfsSpotlightFileExtentProbeRow er = er0;
                                er.sequence = static_cast<std::uint32_t>(directSpotlightFileExtentRows.size());
                                er.targetSequence = cr.sequence;
                                er.targetParentObjectId = cr.parentObjectId;
                                er.targetChildFileId = cr.childFileId;
                                er.targetName = cr.targetName;
                                er.targetKind = cr.targetKind;
                                er.extentStatus = "TARGET_FILE_EXTENT_DIRECT_INDEX_HIT";
                                er.interpretation = "Direct AFF4/APFS exhausted filesystem-tree index matched this Store-V2 target to a FILE_EXTENT record.";
                                er.notes += (er.notes.empty() ? std::string{} : "; ") + std::string("candidate_source=") + cand.second + "; candidate_object_id=" + std::to_string(cand.first) + "; original_extent_object_id=" + std::to_string(er0.extentFileId);
                                if (er.extentLogicalOffset == 0 && er.physicalOffset != 0 && er.extentLengthBytes > 0) {
                                    const std::uint64_t previewLen = std::min<std::uint64_t>(4096ULL, er.extentLengthBytes);
                                    std::vector<unsigned char> preview;
                                    std::string previewErr;
                                    er.previewBytesRead = directReadVirtual(er.physicalOffset, previewLen, preview, previewErr);
                                    if (er.previewBytesRead > 0) {
                                        er.previewStatus = directPreviewStatusForBytes(preview);
                                        er.previewSampleHex = hexSampleBytes(preview.data(), preview.size() < 96 ? preview.size() : 96);
                                    } else {
                                        er.previewStatus = "PREVIEW_READ_FAILED";
                                        er.notes += previewErr.empty() ? "; preview_read_failed" : ("; " + previewErr);
                                    }
                                } else {
                                    er.previewStatus = "PREVIEW_NOT_LOGICAL_ZERO_EXTENT";
                                }
                                extentsByTargetSequence[cr.sequence].push_back(er);
                                directSpotlightFileExtentRows.push_back(er);
                            }
                        }
                    }

                    auto safeStageComponent = [](const std::string& in) -> std::string {
                        std::string out;
                        for (char ch : in) {
                            const unsigned char c = static_cast<unsigned char>(ch);
                            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || ch == '.' || ch == '_' || ch == '-') out.push_back(ch);
                            else out.push_back('_');
                            if (out.size() >= 180U) break;
                        }
                        if (out.empty() || out == "." || out == "..") out = "unnamed";
                        return out;
                    };
                    auto safeRelativeStorePath = [&](const ApfsSpotlightCopyAttemptRow& cr) -> fs::path {
                        fs::path rel;
                        std::string raw = cr.storeV2RelativePath.empty() ? cr.targetName : cr.storeV2RelativePath;
                        std::replace(raw.begin(), raw.end(), '\\', '/');
                        std::stringstream ss(raw);
                        std::string part;
                        while (std::getline(ss, part, '/')) {
                            if (part.empty() || part == "." || part == "..") continue;
                            rel /= safeStageComponent(part);
                        }
                        if (rel.empty()) rel = safeStageComponent(std::to_string(cr.sequence) + "_fid_" + std::to_string(cr.childFileId) + "_" + cr.targetName);
                        return rel;
                    };
                    constexpr std::uint64_t kDirectMaxSingleCopyOutBytes = 512ULL * 1024ULL * 1024ULL;
                    const fs::path directStageRoot = caseDir / "ExtractedSpotlight" / "StagedStoreV2";
                    for (const auto& cr : directSpotlightCopyAttemptRows) {
                        const auto extIt = extentsByTargetSequence.find(cr.sequence);
                        if (extIt == extentsByTargetSequence.end() || extIt->second.empty()) continue;
                        std::vector<ApfsSpotlightFileExtentProbeRow> extents = extIt->second;
                        std::sort(extents.begin(), extents.end(), [](const auto& a, const auto& b) {
                            if (a.extentLogicalOffset != b.extentLogicalOffset) return a.extentLogicalOffset < b.extentLogicalOffset;
                            return a.physicalOffset < b.physicalOffset;
                        });
                        std::uint64_t expectedEnd = 0;
                        bool overlap = false;
                        bool invalidLength = false;
                        bool hasSparseGap = false;
                        bool hasZeroPhysical = false;
                        for (const auto& er : extents) {
                            if (er.extentLengthBytes == 0 || er.extentLengthBytes > kDirectMaxSingleCopyOutBytes) invalidLength = true;
                            if (er.extentLogicalOffset < expectedEnd) overlap = true;
                            if (er.extentLogicalOffset > expectedEnd) hasSparseGap = true;
                            if (er.physicalBlock == 0 || er.physicalOffset == 0) hasZeroPhysical = true;
                            const std::uint64_t eEnd = er.extentLogicalOffset + er.extentLengthBytes;
                            if (eEnd < er.extentLogicalOffset) invalidLength = true;
                            expectedEnd = std::max<std::uint64_t>(expectedEnd, eEnd);
                        }
                        std::uint64_t directLogicalSize = expectedEnd;
                        std::string directLogicalSizeSource = "direct_indexed_file_extent_end";
                        const auto inodeForLogicalIt = directIndexedInodeByObject.find(std::make_pair(cr.volumeSequence, cr.childFileId));
                        if (inodeForLogicalIt != directIndexedInodeByObject.end()) {
                            const auto& indexedInode = inodeForLogicalIt->second;
                            if (indexedInode.inodeDstreamSize != 0 && indexedInode.inodeDstreamSize <= expectedEnd) {
                                directLogicalSize = indexedInode.inodeDstreamSize;
                                directLogicalSizeSource = "INO_EXT_TYPE_DSTREAM.size.direct_index";
                            } else if (indexedInode.inodeUncompressedSize != 0 && indexedInode.inodeUncompressedSize <= expectedEnd) {
                                directLogicalSize = indexedInode.inodeUncompressedSize;
                                directLogicalSizeSource = "j_inode_val.uncompressed_size.direct_index";
                            }
                        }
                        ApfsSpotlightFileCopyOutRow row;
                        row.sequence = static_cast<std::uint32_t>(directSpotlightFileCopyOutRows.size());
                        row.targetSequence = cr.sequence;
                        row.volumeSequence = cr.volumeSequence;
                        row.targetRole = cr.targetRole;
                        row.fsOid = cr.fsOid;
                        row.volumeName = cr.volumeName;
                        row.targetParentObjectId = cr.parentObjectId;
                        row.targetChildFileId = cr.childFileId;
                        row.targetName = cr.targetName;
                        row.targetKind = cr.targetKind;
                        row.storeV2RootObjectId = cr.storeV2RootObjectId;
                        row.storeV2GroupName = cr.storeV2GroupName;
                        row.storeV2RelativePath = cr.storeV2RelativePath.empty() ? pathString(safeRelativeStorePath(cr)) : cr.storeV2RelativePath;
                        row.extentCount = static_cast<std::uint32_t>(extents.size());
                        row.assembledBytes = expectedEnd;
                        row.logicalSizeBytes = directLogicalSize;
                        row.logicalSizeSource = directLogicalSizeSource;
                        row.firstPhysicalOffset = extents.empty() ? 0ULL : extents.front().physicalOffset;
                        if (overlap) { row.copyStatus = "SKIPPED_OVERLAPPING_OR_OUT_OF_ORDER_EXTENTS"; row.validationStatus = "OVERLAP_ORDER_GATE_FAILED"; row.notes = "Direct indexed extents overlapped; skipped to avoid shifted output."; directSpotlightFileCopyOutRows.push_back(row); continue; }
                        if (invalidLength || expectedEnd == 0 || directLogicalSize == 0 || expectedEnd > kDirectMaxSingleCopyOutBytes || directLogicalSize > kDirectMaxSingleCopyOutBytes) { row.copyStatus = "SKIPPED_SIZE_LIMIT_OR_INVALID_LENGTH"; row.validationStatus = "SIZE_GATE_FAILED"; row.notes = "Direct indexed extent chain was empty, invalid, or above per-file safety cap."; directSpotlightFileCopyOutRows.push_back(row); continue; }
                        // V1.0.18: write raw APFS copy-out rows to a unique per-target folder.
                        // Earlier builds wrote many duplicate Store-V2 component names into
                        // ExtractedSpotlight/StagedStoreV2/Ungrouped/<name>.  Later rows could
                        // overwrite the file that an earlier, higher-scored staging row referenced,
                        // causing the stage CSV to report the selected row size/hash while the actual
                        // staged file came from the last duplicate writer.  Keep copy-out provenance
                        // separate from the normalized StagedStoreV2 tree so stage selection copies
                        // immutable per-row sources.
                        const fs::path directCopyOutRoot = caseDir / "ExtractedSpotlight" / "ApfsCopyOutByTarget";
                        const std::string rawGroupLabel = safeStageComponent(cr.storeV2GroupName.empty() ? "Ungrouped" : cr.storeV2GroupName);
                        const fs::path groupDir = directCopyOutRoot / ("seq_" + std::to_string(cr.sequence) + "_fid_" + std::to_string(cr.childFileId) + "_parent_" + std::to_string(cr.parentObjectId) + "_" + rawGroupLabel);
                        const fs::path outPath = groupDir / safeRelativeStorePath(cr);
                        std::error_code mkEc;
                        fs::create_directories(outPath.parent_path(), mkEc);
                        if (mkEc) { row.copyStatus = "COPY_FAILED_CREATE_DIRECTORY"; row.validationStatus = "OUTPUT_DIRECTORY_FAILED"; row.notes = mkEc.message(); directSpotlightFileCopyOutRows.push_back(row); continue; }
                        std::ofstream outFile(outPath, std::ios::binary);
                        if (!outFile) { row.copyStatus = "COPY_FAILED_OPEN_OUTPUT"; row.validationStatus = "OUTPUT_OPEN_FAILED"; row.notes = pathString(outPath); directSpotlightFileCopyOutRows.push_back(row); continue; }
                        std::vector<unsigned char> firstBytes;
                        std::uint64_t logicalCursor = 0;
                        std::uint64_t syntheticZeroBytes = 0;
                        bool copyOk = true;
                        std::string copyNotes;
                        auto writeZeros = [&](std::uint64_t count, const std::string& reason) -> bool {
                            std::vector<unsigned char> z(std::min<std::uint64_t>(count, 1024ULL * 1024ULL), 0U);
                            std::uint64_t left = count;
                            while (left != 0) {
                                const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(left, z.size()));
                                if (firstBytes.empty()) firstBytes.assign(z.begin(), z.begin() + std::min<std::size_t>(chunk, 96U));
                                outFile.write(reinterpret_cast<const char*>(z.data()), static_cast<std::streamsize>(chunk));
                                if (!outFile) { copyNotes = "zero-fill write failed: " + reason; return false; }
                                left -= chunk;
                            }
                            syntheticZeroBytes += count;
                            return true;
                        };
                        for (const auto& er : extents) {
                            if (logicalCursor >= directLogicalSize) break;
                            if (er.extentLogicalOffset > logicalCursor) {
                                const std::uint64_t gapRaw = er.extentLogicalOffset - logicalCursor;
                                const std::uint64_t gap = std::min<std::uint64_t>(gapRaw, directLogicalSize - logicalCursor);
                                if (!writeZeros(gap, "logical_sparse_gap")) { copyOk = false; break; }
                                logicalCursor += gap;
                                if (logicalCursor >= directLogicalSize) break;
                            }
                            const std::uint64_t writableExtentBytes = std::min<std::uint64_t>(er.extentLengthBytes, directLogicalSize - logicalCursor);
                            if (writableExtentBytes == 0) break;
                            if (er.physicalBlock == 0 || er.physicalOffset == 0) {
                                if (!writeZeros(writableExtentBytes, "zero_physical_extent")) { copyOk = false; break; }
                                logicalCursor += writableExtentBytes;
                                continue;
                            }
                            std::vector<unsigned char> extentBytes;
                            std::string readErr;
                            const long long got = directReadVirtual(er.physicalOffset, writableExtentBytes, extentBytes, readErr);
                            if (got < 0 || static_cast<std::uint64_t>(got) != writableExtentBytes || extentBytes.size() != writableExtentBytes) { copyOk = false; copyNotes = readErr.empty() ? "extent read failed or returned short data" : readErr; break; }
                            if (firstBytes.empty()) firstBytes.assign(extentBytes.begin(), extentBytes.begin() + std::min<std::size_t>(extentBytes.size(), 96U));
                            outFile.write(reinterpret_cast<const char*>(extentBytes.data()), static_cast<std::streamsize>(extentBytes.size()));
                            if (!outFile) { copyOk = false; copyNotes = "extent write failed"; break; }
                            logicalCursor += writableExtentBytes;
                        }
                        if (copyOk && logicalCursor != directLogicalSize) {
                            copyOk = false;
                            copyNotes = "logical output short after bounded direct copy; wrote=" + std::to_string(logicalCursor) + "; expected=" + std::to_string(directLogicalSize);
                        }
                        outFile.close();
                        row.outputPath = pathString(outPath);
                        row.outputRelativePath = pathString(fs::relative(outPath, caseDir));
                        if (!firstBytes.empty()) { row.firstBytesStatus = directPreviewStatusForBytes(firstBytes); row.firstBytesHex = hexSampleBytes(firstBytes.data(), firstBytes.size()); }
                        else row.firstBytesStatus = "NO_PREVIEW";
                        if (!copyOk) {
                            row.copyStatus = "COPY_FAILED_READ_OR_WRITE";
                            row.validationStatus = "FAILED_DURING_COPY";
                            row.notes = copyNotes;
                            std::error_code rmEc; fs::remove(outPath, rmEc);
                        } else {
                            std::error_code sizeEc;
                            const auto sz = fs::file_size(outPath, sizeEc);
                            row.outputSizeBytes = sizeEc ? 0ULL : static_cast<std::uint64_t>(sz);
                            try { row.outputSha256 = sha256File(outPath); } catch (const std::exception& ex) { row.notes = std::string("sha256_failed: ") + ex.what(); }
                            if (syntheticZeroBytes != 0 || hasSparseGap || hasZeroPhysical) {
                                row.copyStatus = "COPIED_WITH_RECORDED_SYNTHETIC_ZERO_REGIONS";
                                row.validationStatus = (row.outputSizeBytes == directLogicalSize) ? "SIZE_MATCH_WITH_ZERO_FILL_PROVENANCE" : "SIZE_MISMATCH_AFTER_ZERO_FILL_COPY";
                                row.notes += (row.notes.empty() ? std::string{} : "; ") + (std::string("direct_aff4_v1_0_16_copy_out=1; logical_size_source=") + directLogicalSizeSource + "; synthetic_zero_bytes=") + std::to_string(syntheticZeroBytes) + "; sparse_gap=" + (hasSparseGap ? "true" : "false") + "; zero_physical_extent=" + (hasZeroPhysical ? "true" : "false") + "; apfs_absolute_path=" + directPathForObject(cr.volumeSequence, cr.childFileId);
                                row.interpretation = "Direct AFF4/APFS indexed FILE_EXTENT rows were staged with explicit zero-fill provenance for sparse or zero-physical regions.";
                            } else {
                                row.copyStatus = "COPIED_DIRECT_INDEXED_EXTENT_CHAIN";
                                row.validationStatus = (row.outputSizeBytes == directLogicalSize && directLogicalSize < expectedEnd) ? "TRIMMED_TO_INODE_LOGICAL_SIZE" : ((row.outputSizeBytes == expectedEnd) ? "SIZE_MATCHES_EXTENT_CHAIN" : "SIZE_MISMATCH_AFTER_COPY");
                                row.notes += (row.notes.empty() ? std::string{} : "; ") + (std::string("direct_aff4_v1_0_16_copy_out=1; logical_size_source=") + directLogicalSizeSource + "; apfs_absolute_path=") + directPathForObject(cr.volumeSequence, cr.childFileId);
                                row.interpretation = "Direct AFF4/APFS indexed FILE_EXTENT rows were assembled into a staged Store-V2 copy-out file.";
                            }
                        }
                        directSpotlightFileCopyOutRows.push_back(row);
                    }

                    const fs::path walkCsv = caseDir / "aff4_apfs_logical_directory_walk.csv";
                    const fs::path walkJson = caseDir / "aff4_apfs_logical_directory_walk_summary.json";
                    try {
                        std::ofstream out(walkCsv, std::ios::binary);
                        out << "source_id,input_path,input_type,sequence,volume_sequence,volume_name,parent_object_id,child_file_id,name,walk_role,apfs_absolute_path,notes\n";
                        std::size_t seq = 0;
                        for (const auto& e : directDirectoryRecordEntries) {
                            const std::string lower = asciiLower(e.name);
                            std::string role;
                            if (lower == ".spotlight-v100") role = "SPOTLIGHT_ROOT_DIRECTORY";
                            else if (lower == "store-v2") role = "STORE_V2_DIRECTORY";
                            else if (childrenByParent.count(std::make_pair(e.volumeSequence, e.parentObjectId)) && !directPathForObject(e.volumeSequence, e.childFileId).empty() && directPathForObject(e.volumeSequence, e.childFileId).find("Store-V2") != std::string::npos) role = "STORE_V2_DESCENDANT";
                            if (role.empty()) continue;
                            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                                << seq++ << ',' << e.volumeSequence << ',' << csvEscape(e.volumeName) << ',' << e.parentObjectId << ',' << e.childFileId << ','
                                << csvEscape(e.name) << ',' << csvEscape(role) << ',' << csvEscape(directPathForObject(e.volumeSequence, e.childFileId)) << ','
                                << csvEscape("logical_namespace_walk_from_exhausted_apfs_btree_records") << "\n";
                        }
                    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_logical_directory_walk.csv: ") + ex.what()); }
                    try {
                        std::ofstream out(walkJson, std::ios::binary);
                        out << "{\n";
                        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
                        out << "  \"app_version\": \"" << appVersion() << "\",\n";
                        out << "  \"indexed_inode_records\": " << directIndexedInodeByObject.size() << ",\n";
                        std::size_t extentRecordCount = 0; for (const auto& kv : directIndexedFileExtentsByObject) extentRecordCount += kv.second.size();
                        out << "  \"indexed_file_extent_records\": " << extentRecordCount << ",\n";
                        out << "  \"materialized_target_inode_rows\": " << directSpotlightInodeRows.size() << ",\n";
                        out << "  \"materialized_target_file_extent_rows\": " << directSpotlightFileExtentRows.size() << ",\n";
                        out << "  \"copy_out_rows\": " << directSpotlightFileCopyOutRows.size() << "\n";
                        out << "}\n";
                    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_logical_directory_walk_summary.json: ") + ex.what()); }
                }

                const std::uint32_t descBlockCount = directBestNx.xpDescBlocks & ~(1U << 31);
                const std::uint32_t descToRead = std::min<std::uint32_t>(descBlockCount, 256U);
                for (std::uint32_t i = 0; i < descToRead; ++i) {
                    const std::uint64_t physicalBlock = directBestNx.xpDescBase + static_cast<std::uint64_t>(i);
                    if (physicalBlock > (std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(directBestNx.blockSize))) break;
                    const std::uint64_t vo = physicalBlock * static_cast<std::uint64_t>(directBestNx.blockSize);
                    std::vector<unsigned char> buf;
                    std::string readErr;
                    const bool ok = readVirtualBytes(vo, directBestNx.blockSize, buf, readErr);

                    ApfsCheckpointDescriptorRow dr;
                    dr.sequence = static_cast<std::uint32_t>(directDescriptorRows.size() + 1U);
                    dr.physicalBlock = physicalBlock;
                    dr.virtualOffset = vo;
                    dr.bytesRead = ok ? static_cast<long long>(buf.size()) : -1;
                    dr.status = ok ? "READ_OK" : "READ_FAILED";
                    dr.notes = readErr.empty() ? "Direct sorted AFF4 virtual read of NX checkpoint descriptor block." : readErr;
                    if (ok && buf.size() >= 36U) {
                        dr.oid = readLe64(buf, 8);
                        dr.xid = readLe64(buf, 16);
                        dr.objectTypeRaw = readLe32(buf, 24);
                        dr.objectSubtype = readLe32(buf, 28);
                        dr.objectTypeLabel = apfsObjectTypeLabel(dr.objectTypeRaw);
                        dr.magic.assign(reinterpret_cast<const char*>(buf.data() + 32), reinterpret_cast<const char*>(buf.data() + 36));
                        dr.sampleHex = hexSampleBytes(buf.data(), std::min<std::size_t>(buf.size(), 96U));
                        if (dr.magic == "NXSB") dr.interpretation = "Checkpoint descriptor block contains an APFS container superblock copy.";
                        else if (dr.magic == "APSB") dr.interpretation = "Checkpoint descriptor block contains an APFS volume superblock.";
                        else if (dr.objectTypeLabel == "CHECKPOINT_MAP") dr.interpretation = "Checkpoint descriptor block contains an APFS checkpoint map.";
                        else dr.interpretation = "Checkpoint descriptor block read; object type/magic retained for APFS resolution.";
                    } else {
                        dr.interpretation = "Direct read failed for checkpoint descriptor block.";
                    }
                    directDescriptorRows.push_back(dr);

                    if (!(ok && buf.size() >= 80U && dr.objectTypeLabel == "CHECKPOINT_MAP")) continue;
                    const std::uint32_t checkpointFlags = readLe32(buf, 32);
                    const std::uint32_t checkpointCount = std::min<std::uint32_t>(readLe32(buf, 36), 2048U);
                    for (std::uint32_t entryIndex = 0; entryIndex < checkpointCount; ++entryIndex) {
                        const std::size_t entryOff = 40U + static_cast<std::size_t>(entryIndex) * 40U;
                        if (entryOff + 40U > buf.size()) break;
                        ApfsCheckpointMapEntryRow mr;
                        mr.sequence = static_cast<std::uint32_t>(directCheckpointMapRows.size() + 1U);
                        mr.entryIndex = entryIndex;
                        mr.checkpointBlock = physicalBlock;
                        mr.checkpointVirtualOffset = vo;
                        mr.checkpointBytesRead = dr.bytesRead;
                        mr.checkpointFlags = checkpointFlags;
                        mr.checkpointCount = checkpointCount;
                        mr.cpmTypeRaw = readLe32(buf, entryOff + 0U);
                        mr.cpmSubtype = readLe32(buf, entryOff + 4U);
                        mr.cpmSize = readLe32(buf, entryOff + 8U);
                        mr.cpmFsOid = readLe64(buf, entryOff + 16U);
                        mr.cpmOid = readLe64(buf, entryOff + 24U);
                        mr.cpmPaddr = readLe64(buf, entryOff + 32U);
                        mr.cpmTypeLabel = apfsObjectTypeLabel(mr.cpmTypeRaw);
                        if (mr.cpmOid == directBestNx.omapOid) mr.targetRole = "NX_OBJECT_MAP";
                        else if (containsU64(directBestNx.fsOids, mr.cpmOid)) mr.targetRole = "NX_FILESYSTEM_OID";
                        else if (mr.cpmFsOid != 0) mr.targetRole = "VOLUME_SCOPED_OBJECT";
                        else mr.targetRole = "CHECKPOINT_OBJECT";
                        mr.interpretation = "Checkpoint mapping parsed from direct AFF4 APFS descriptor ring.";
                        mr.notes = "Direct sorted AFF4 APFS checkpoint map entry.";
                        directCheckpointMapRows.push_back(mr);

                        if (directCheckpointObjectRows.size() >= 512U || mr.cpmPaddr == 0) continue;
                        if (mr.targetRole != "NX_OBJECT_MAP" && mr.targetRole != "NX_FILESYSTEM_OID" && mr.cpmTypeLabel != "OBJECT_MAP" && mr.cpmTypeLabel != "FS") continue;
                        appendMappedObjectProbe(entryIndex, mr.cpmOid, mr.cpmFsOid, mr.cpmPaddr, mr.targetRole, "Checkpoint-mapped object read through direct sorted AFF4 reader.", directBestNx.nextXid, 0, mr.cpmSize);
                    }
                }
            };

            std::set<std::uint64_t> seenChunks;
            const std::size_t maxMapEntriesToScan = std::min<std::size_t>(mapEntries.size(), 50000U);
            for (std::size_t scanIndex = 0; scanIndex < maxMapEntriesToScan && alignedApfsHits < 50U; ++scanIndex) {
                const auto& scanEntry = mapEntries[scanIndex];
                const std::uint64_t virtualOffset = scanEntry.virtualOffset;
                const std::uint64_t length = scanEntry.length;
                const std::uint64_t streamOffset = scanEntry.streamOffset;
                const std::uint64_t originalMapEntryIndex = scanEntry.mapEntryIndex;
                ++mapEntriesScanned;
                const std::uint64_t startChunk = streamOffset / 32768ULL;
                const std::uint64_t endChunk = (streamOffset + length - 1ULL) / 32768ULL;
                for (std::uint64_t chunk = startChunk; chunk <= endChunk && alignedApfsHits < 50U; ++chunk) {
                    if (!seenChunks.insert(chunk).second) continue;
                    std::vector<unsigned char> dec;
                    bool usedLz4 = false;
                    if (!decodeImageChunk(chunk, dec, err, usedLz4)) {
                        add("direct_lz4_decode", "DECODE_FAILED", virtualOffset, streamOffset, originalMapEntryIndex, chunk, -1, {}, {}, err);
                        continue;
                    }
                    if (usedLz4) ++lz4ChunksDecoded;
                    ++chunksDecoded;
                    if (chunksDecoded == 1U) {
                        add("direct_first_chunk_decode", "DECODE_OK", virtualOffset, streamOffset, originalMapEntryIndex, chunk, static_cast<long long>(dec.size()), {}, hexSampleBytes(dec.data(), std::min<std::size_t>(dec.size(), 64U)), "First AFF4 image-stream chunk decoded directly from map/index/data ZIP members after sorting the AFF4 map by APFS virtual offset.");
                    }
                    if (directSignatureHits < 100U) {
                        const std::vector<std::pair<std::string, std::string>> signatures = {
                            {"NXSB", "APFS_CONTAINER_SUPERBLOCK_MAGIC"},
                            {"APSB", "APFS_VOLUME_SUPERBLOCK_MAGIC"},
                            {".Spotlight-V100", "MACOS_SPOTLIGHT_ROOT_STRING"},
                            {"Store-V2", "MACOS_SPOTLIGHT_STOREV2_STRING"},
                            {"SQLite format 3", "SQLITE_HEADER_STRING"},
                            {"CoreSpotlight", "IOS_CORESPOTLIGHT_STRING"}
                        };
                        for (const auto& sig : signatures) {
                            if (directSignatureHits >= 100U) break;
                            const auto it = std::search(dec.begin(), dec.end(), sig.first.begin(), sig.first.end());
                            if (it == dec.end()) continue;
                            const std::size_t pSig = static_cast<std::size_t>(std::distance(dec.begin(), it));
                            const std::int64_t chunkVirtualStartSigned = static_cast<std::int64_t>(virtualOffset) + static_cast<std::int64_t>(chunk * 32768ULL) - static_cast<std::int64_t>(streamOffset);
                            if (chunkVirtualStartSigned < 0) continue;
                            const std::uint64_t hitVirtual = static_cast<std::uint64_t>(chunkVirtualStartSigned) + static_cast<std::uint64_t>(pSig);
                            const std::size_t sampleStart = pSig >= 32U ? pSig - 32U : pSig;
                            const std::size_t sampleLen = std::min<std::size_t>(128U, dec.size() - sampleStart);
                            ++directSignatureHits;
                            add("direct_decoded_signature_scan", "SIGNATURE_FOUND", hitVirtual, streamOffset, originalMapEntryIndex, chunk, static_cast<long long>(dec.size()), sig.second, hexSampleBytes(dec.data() + sampleStart, sampleLen), "Signature found by scanning directly decoded AFF4 image-stream chunk bytes after sorting AFF4 map entries by APFS virtual offset.");
                            if (sig.second == "SQLITE_HEADER_STRING") {
                                carveSqliteCandidate(hitVirtual, originalMapEntryIndex, chunk);
                            }
                        }
                    }
                    const std::int64_t chunkVirtualStartSigned = static_cast<std::int64_t>(virtualOffset) + static_cast<std::int64_t>(chunk * 32768ULL) - static_cast<std::int64_t>(streamOffset);
                    if (chunkVirtualStartSigned < 0) continue;
                    const std::uint64_t chunkVirtualStart = static_cast<std::uint64_t>(chunkVirtualStartSigned);
                    std::size_t p = 0;
                    const std::uint64_t mod = chunkVirtualStart % 4096ULL;
                    if (mod <= 32ULL) p = static_cast<std::size_t>(32ULL - mod);
                    else p = static_cast<std::size_t>(4096ULL - (mod - 32ULL));
                    for (; p + 4U <= dec.size(); p += 4096U) {
                        std::string magic;
                        if (std::memcmp(dec.data() + p, "NXSB", 4U) == 0) magic = "NXSB";
                        else if (std::memcmp(dec.data() + p, "APSB", 4U) == 0) magic = "APSB";
                        if (magic.empty()) continue;
                        const std::uint64_t hitVirtual = chunkVirtualStart + static_cast<std::uint64_t>(p);
                        const std::size_t sampleStart = p >= 32U ? p - 32U : p;
                        const std::size_t sampleLen = std::min<std::size_t>(96U, dec.size() - sampleStart);
                        ++alignedApfsHits;
                        add("direct_aligned_apfs_magic_scan", "APFS_MAGIC_FOUND", hitVirtual, streamOffset, originalMapEntryIndex, chunk, static_cast<long long>(dec.size()), magic, hexSampleBytes(dec.data() + sampleStart, sampleLen), "APFS object magic found at +32 within a directly decoded AFF4 image-stream chunk after sorting AFF4 map entries by APFS virtual offset.");
                        if (magic == "NXSB") {
                            rememberNxCandidateFromDecodedChunk(dec, p, hitVirtual - 32ULL, originalMapEntryIndex, chunk);
                        }
                    }
                }
            }
            probeDirectApfsFromBestNx();
            finalStatus = chunksDecoded > 0U ? "DIRECT_MAP_READER_SMOKE_OK" : "DIRECT_MAP_READER_NO_CHUNKS_DECODED";
            finalNotes = "scan_entries=" + std::to_string(maxMapEntriesToScan) + "; map_entries_total=" + std::to_string(mapEntryCount);
        }
    } catch (const std::exception& ex) {
        finalStatus = "DIRECT_MAP_READER_EXCEPTION";
        finalNotes = ex.what();
        add("direct_map_reader_probe", finalStatus, 0, 0, 0, 0, -1, {}, {}, finalNotes);
    }

    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,step,status,virtual_offset,stream_offset,map_entry_index,chunk_index,bytes_read,magic,sample_hex,notes\n";
        for (const auto& r : probeRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << csvEscape(r.step) << ',' << csvEscape(r.status) << ',' << r.virtualOffset << ',' << r.streamOffset << ','
                << r.mapEntryIndex << ',' << r.chunkIndex << ',' << r.bytesRead << ',' << csvEscape(r.magic) << ','
                << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_direct_map_reader_probe.csv: ") + ex.what());
    }
    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"status\": \"" << jsonEscape(finalStatus) << "\",\n";
        out << "  \"map_entries_total\": " << mapEntryCount << ",\n";
        out << "  \"map_entries_scanned\": " << mapEntriesScanned << ",\n";
        out << "  \"chunks_decoded\": " << chunksDecoded << ",\n";
        out << "  \"lz4_chunks_decoded\": " << lz4ChunksDecoded << ",\n";
        out << "  \"aligned_apfs_magic_hits\": " << alignedApfsHits << ",\n";
        out << "  \"direct_signature_hits\": " << directSignatureHits << ",\n";
        out << "  \"sqlite_candidates_found\": " << sqliteCandidatesFound << ",\n";
        out << "  \"sqlite_candidates_carved\": " << sqliteCandidatesCarved << ",\n";
        out << "  \"sqlite_candidates_opened_with_tables\": " << sqliteCandidatesOpened << ",\n";
        out << "  \"direct_apfs_nxsb_found\": " << (directBestNx.found ? "true" : "false") << ",\n";
        out << "  \"direct_apfs_best_nx_xid\": " << directBestNx.xid << ",\n";
        out << "  \"direct_apfs_checkpoint_descriptor_rows\": " << directDescriptorRows.size() << ",\n";
        out << "  \"direct_apfs_checkpoint_map_entries\": " << directCheckpointMapRows.size() << ",\n";
        out << "  \"direct_apfs_checkpoint_mapped_object_rows\": " << directCheckpointObjectRows.size() << ",\n";
        out << "  \"direct_apfs_resolved_volume_rows\": " << directResolvedVolumeRows.size() << ",\n";
        out << "  \"direct_apfs_volume_omap_rows\": " << directVolumeOmapRows.size() << ",\n";
        out << "  \"direct_apfs_volume_root_tree_lookup_rows\": " << directVolumeRootTreeLookupRows.size() << ",\n";
        out << "  \"direct_apfs_root_tree_node_rows\": " << directRootTreeNodeRows.size() << ",\n";
        out << "  \"direct_apfs_root_tree_record_sample_rows\": " << directRootTreeRecordRows.size() << ",\n";
        out << "  \"direct_apfs_spotlight_nodes_visited\": " << directSpotlightScanMetrics.nodesVisited << ",\n";
        out << "  \"direct_apfs_spotlight_records_scanned\": " << directSpotlightScanMetrics.recordsScanned << ",\n";
        out << "  \"direct_apfs_spotlight_target_hits\": " << directSpotlightTargetRows.size() << ",\n";
        out << "  \"direct_apfs_spotlight_name_samples\": " << directSpotlightNameSampleRows.size() << ",\n";
        out << "  \"direct_apfs_spotlight_copy_attempt_rows\": " << directSpotlightCopyAttemptRows.size() << ",\n";
        out << "  \"notes\": \"" << jsonEscape(finalNotes) << "\"\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_direct_map_reader_probe_summary.json: ") + ex.what());
    }
    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 Direct Map Reader Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "This probe reads BlackBag/LZ4 AFF4 map/index/data ZIP members directly and avoids `AFF4_open`.\n\n";
        out << "## Summary\n\n";
        out << "- Status: `" << finalStatus << "`\n";
        out << "- Map entries total: `" << mapEntryCount << "`\n";
        out << "- Map entries scanned: `" << mapEntriesScanned << "`\n";
        out << "- Chunks decoded: `" << chunksDecoded << "`\n";
        out << "- LZ4 chunks decoded: `" << lz4ChunksDecoded << "`\n";
        out << "- Aligned APFS magic hits: `" << alignedApfsHits << "`\n\n";
        out << "- Direct decoded signature hits: `" << directSignatureHits << "`\n\n";
        out << "- SQLite candidates found: `" << sqliteCandidatesFound << "`\n";
        out << "- SQLite candidates carved: `" << sqliteCandidatesCarved << "`\n";
        out << "- SQLite candidates opened with tables: `" << sqliteCandidatesOpened << "`\n\n";
        out << "- Direct APFS NXSB found: `" << (directBestNx.found ? "true" : "false") << "`\n";
        out << "- Direct APFS checkpoint descriptor rows: `" << directDescriptorRows.size() << "`\n";
        out << "- Direct APFS checkpoint map entries: `" << directCheckpointMapRows.size() << "`\n";
        out << "- Direct APFS mapped object probes: `" << directCheckpointObjectRows.size() << "`\n\n";
        out << "- Direct APFS resolved volume rows: `" << directResolvedVolumeRows.size() << "`\n";
        out << "- Direct APFS volume OMAP rows: `" << directVolumeOmapRows.size() << "`\n";
        out << "- Direct APFS volume root-tree lookups: `" << directVolumeRootTreeLookupRows.size() << "`\n";
        out << "- Direct APFS root-tree node rows: `" << directRootTreeNodeRows.size() << "`\n";
        out << "- Direct APFS Spotlight scan nodes visited: `" << directSpotlightScanMetrics.nodesVisited << "`\n";
        out << "- Direct APFS Spotlight scan records scanned: `" << directSpotlightScanMetrics.recordsScanned << "`\n";
        out << "- Direct APFS Spotlight target hits: `" << directSpotlightTargetRows.size() << "`\n\n";
        out << "A successful chunk decode proves the native direct AFF4 reader path can reconstruct virtual image bytes without the blocking libaff4 open call. The sorted-map APFS path now parses container metadata, resolves APFS volume object maps, probes filesystem root-tree records, and emits bounded Spotlight target-scan outputs. Copy-out remains gated until targeted path, inode, xattr, and file-extent provenance is complete.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_DIRECT_MAP_READER_PROBE.md: ") + ex.what());
    }
    try {
        std::ofstream out(sqliteCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,sequence,status,virtual_offset,map_entry_index,chunk_index,page_size,db_pages,requested_bytes,carved_bytes,output_relative_path,sqlite_open_status,sqlite_master_status,table_count,table_names,sample_hex,notes\n";
        for (const auto& r : sqliteCandidateRows) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(originalInput)) << ',' << csvEscape(inputSourceType(originalInput)) << ','
                << r.sequence << ',' << csvEscape(r.status) << ',' << r.virtualOffset << ',' << r.mapEntryIndex << ','
                << r.chunkIndex << ',' << r.pageSize << ',' << r.dbPages << ',' << r.requestedBytes << ',' << r.carvedBytes << ','
                << csvEscape(r.outputRelativePath) << ',' << csvEscape(r.sqliteOpenStatus) << ',' << csvEscape(r.sqliteMasterStatus) << ','
                << r.tableCount << ',' << csvEscape(r.tableNames) << ',' << csvEscape(r.sampleHex) << ',' << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_direct_sqlite_candidate_carve.csv: ") + ex.what());
    }
    try {
        std::ofstream out(sqliteJsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"candidate_directory\": \"" << jsonEscape(pathString(sqliteCandidateDir)) << "\",\n";
        out << "  \"sqlite_candidates_found\": " << sqliteCandidatesFound << ",\n";
        out << "  \"sqlite_candidates_carved\": " << sqliteCandidatesCarved << ",\n";
        out << "  \"sqlite_candidates_opened_with_tables\": " << sqliteCandidatesOpened << ",\n";
        out << "  \"candidate_rows\": " << sqliteCandidateRows.size() << ",\n";
        out << "  \"max_candidates\": 12,\n";
        out << "  \"max_bytes_per_candidate\": " << (8ULL * 1024ULL * 1024ULL) << "\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_direct_sqlite_candidate_carve_summary.json: ") + ex.what());
    }
    try {
        std::ofstream out(sqliteMdPath, std::ios::binary);
        out << "# AFF4 Direct SQLite Candidate Carve\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "This diagnostic carves bounded SQLite candidates from decoded AFF4 virtual offsets where `SQLite format 3` headers were found. It is a controlled bridge around the current APFS traversal blocker; candidates are provenance-linked and capped.\n\n";
        out << "## Summary\n\n";
        out << "- Candidates found: `" << sqliteCandidatesFound << "`\n";
        out << "- Candidates carved: `" << sqliteCandidatesCarved << "`\n";
        out << "- Candidates opened with table names: `" << sqliteCandidatesOpened << "`\n";
        out << "- Candidate directory: `Aff4DirectSqliteCandidates`\n\n";
        out << "Rows with `SQLITE_MASTER_OK` and table names should be used as the next parse targets. Rows that open but report malformed/truncated data indicate the SQLite header is real but the file is fragmented or the bounded carve needs APFS file extents.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_DIRECT_SQLITE_CANDIDATE_CARVE.md: ") + ex.what());
    }
    if (directBestNx.attempted || directBestNx.found) {
        const bool writeHeavyApfsDiagnostics = shouldWriteAff4ApfsStructuralDiagnostics(opt.verbose, opt.diagnosticFullNativeDb, opt.aff4ApfsDiagnosticOutputs);
        if (writeHeavyApfsDiagnostics) {
            log.info("AFF4/APFS diagnostic output mode enabled: writing structural probe CSV outputs.");
            writeAff4ApfsContainerViewOutputs(caseDir, source, originalInput, directBestNx, directDescriptorRows, log);
            writeAff4ApfsVolumeSuperblockOutputs(caseDir, source, originalInput, directBestNx, directVolumeRows, log);
            writeAff4ApfsCheckpointMapOutputs(caseDir, source, originalInput, directBestNx, directCheckpointMapRows, directCheckpointObjectRows, log);
            writeAff4ApfsResolvedVolumeOutputs(caseDir, source, originalInput, directBestNx, directResolvedVolumeRows, directVolumeOmapRows, directVolumeRootTreeLookupRows, true, log);
            writeAff4ApfsVolumeRootTreeLookupOutputs(caseDir, source, originalInput, directVolumeRootTreeLookupRows, true, log);
            writeAff4ApfsRootTreeNodeProbeOutputs(caseDir, source, originalInput, directRootTreeNodeRows, directRootTreeRecordRows, true, log);
            writeAff4ApfsFilesystemNamespaceSeedOutputs(caseDir, source, originalInput, directRootTreeRecordRows, std::vector<ApfsRootTreeRecordSampleRow>{}, std::vector<ApfsRootTreeRecordSampleRow>{}, true, log);
            writeAff4ApfsSpotlightTargetScanOutputs(caseDir, source, originalInput, directSpotlightTargetRows, directSpotlightNameSampleRows, directSpotlightCopyAttemptRows, directSpotlightScanMetrics, true, log);
            writeAff4ApfsSpotlightInodeProbeOutputs(caseDir, source, originalInput, directSpotlightInodeRows, true, log);
            writeAff4ApfsSpotlightXattrProbeOutputs(caseDir, source, originalInput, directSpotlightXattrRows, directSpotlightCopyAttemptRows, true, log);
            writeAff4ApfsSpotlightFileExtentProbeOutputs(caseDir, source, originalInput, directSpotlightFileExtentRows, true, log);
        } else {
            log.info("Normal AFF4/APFS source-probe mode: structural diagnostic CSV outputs suppressed; writing copy-out/stage outputs only.");
            appendRunStatus(caseDir, aff4ApfsStructuralDiagnosticsSuppressedStatus(), aff4ApfsStructuralDiagnosticsSuppressedGuidance());
        }
        // Copy-out and staging outputs remain enabled in normal mode because they
        // describe actual extracted evidence and feed the external comparison.
        writeAff4ApfsSpotlightFileCopyOutOutputs(caseDir, source, originalInput, directSpotlightFileCopyOutRows, true, log);
        writeAff4ApfsExtractedStoreV2StageOutputs(caseDir, source, originalInput, directSpotlightFileCopyOutRows, true, log);
    }
    log.info("AFF4 direct map reader probe written: " + pathString(csvPath));
}

bool shouldSkipLibAff4DynamicProbeForKnownBlockingLayout(const fs::path& caseDir,
                                                        const fs::path& originalInput,
                                                        std::string& reason,
                                                        std::string& metadataSample) {
    reason.clear();
    metadataSample.clear();
    std::string turtle;
    std::string error;
    if (!readAff4StoredZipEntryTextFromProbe(caseDir, originalInput, "information.turtle", 256U * 1024U, turtle, error)) {
        reason = error;
        return false;
    }
    metadataSample = turtle.substr(0, 4000);
    const std::string t = asciiLower(turtle);
    const bool blackbagApfs = t.find("bbt:apfscontainerimage") != std::string::npos ||
                              t.find("bbt:apfst2containertype") != std::string::npos;
    const bool discontiguous = t.find("aff4:discontiguousimage") != std::string::npos ||
                               t.find("discontiguousimage") != std::string::npos;
    const bool lz4ImageStream = t.find("compressionmethod") != std::string::npos &&
                                t.find("lz4") != std::string::npos &&
                                t.find("imagestream") != std::string::npos;
    if (blackbagApfs && discontiguous && lz4ImageStream) {
        reason = "Detected BlackBag-style AFF4 APFS DiscontiguousImage with an LZ4 ImageStream. The current libaff4 dynamic C API can block while opening this layout on Windows; skip the dynamic probe and use the direct AFF4 ZIP map/index parser roadmap instead.";
        return true;
    }
    reason = "AFF4 metadata did not match the known blocking BlackBag/LZ4 discontiguous APFS layout.";
    return false;
}

struct Aff4ApfsCandidateHit {
    std::size_t rowIndex = 0;
    std::string entryName;
    int chunkIndex = -1;
    std::uint64_t localHeaderOffset = 0;
    std::uint64_t payloadOffset = 0;
    std::uint64_t entryRelativeOffset = 0;
    std::uint64_t archiveOffset = 0;
    std::string magic;
    std::string confidence;
    std::string interpretation;
    std::string sampleHex;
    std::string notes;
};

std::string aff4ApfsInterpretationForMagic(const std::string& magic, std::uint64_t relOff) {
    if (magic == "EFI PART") return "GPT_HEADER_CANDIDATE_WITHIN_AFF4_IMAGE_CHUNK";
    if (magic == "NXSB") return (relOff % 4096ULL == 32ULL) ? "APFS_CONTAINER_SUPERBLOCK_CANDIDATE_ALIGNED_PLUS_32" : "APFS_CONTAINER_SUPERBLOCK_STRING_CANDIDATE";
    if (magic == "APSB") return "APFS_VOLUME_SUPERBLOCK_STRING_CANDIDATE";
    if (magic == "H+") return "HFS_PLUS_VOLUME_HEADER_STRING_CANDIDATE";
    if (magic == "HX") return "HFSX_VOLUME_HEADER_STRING_CANDIDATE";
    if (magic == "CoreSpotlight") return "IOS_CORESPOTLIGHT_STRING_CANDIDATE_IN_AFF4_PAYLOAD";
    if (magic == ".Spotlight-V100") return "MACOS_SPOTLIGHT_PATH_STRING_CANDIDATE_IN_AFF4_PAYLOAD";
    return "SIGNATURE_CANDIDATE";
}

std::string aff4ApfsConfidenceForMagic(const std::string& magic, std::uint64_t relOff) {
    if (magic == "EFI PART" && relOff % 512ULL == 0ULL) return "HIGH_ALIGNED";
    if (magic == "NXSB" && relOff % 4096ULL == 32ULL) return "HIGH_APFS_ALIGNED";
    if (magic == "APSB" && relOff % 4096ULL == 32ULL) return "MEDIUM_APFS_ALIGNED";
    if (magic == "CoreSpotlight" || magic == ".Spotlight-V100") return "MEDIUM_STRING_HINT";
    return "LOW_STRING_SCAN";
}

void addAff4ApfsSignatureHits(const Aff4ZipCentralDirectoryRow& row,
                              const std::vector<unsigned char>& data,
                              std::uint64_t payloadOffset,
                              int chunkIndex,
                              std::vector<Aff4ApfsCandidateHit>& hits,
                              std::size_t maxHits) {
    struct Pattern { const char* text; std::size_t len; };
    const Pattern patterns[] = {
        {"EFI PART", 8}, {"NXSB", 4}, {"APSB", 4}, {"H+", 2}, {"HX", 2}, {"CoreSpotlight", 13}, {".Spotlight-V100", 15}
    };
    for (const auto& pat : patterns) {
        if (hits.size() >= maxHits) return;
        if (data.size() < pat.len) continue;
        for (std::size_t i = 0; i + pat.len <= data.size(); ++i) {
            if (std::memcmp(data.data() + i, pat.text, pat.len) != 0) continue;
            Aff4ApfsCandidateHit h;
            h.rowIndex = row.index;
            h.entryName = row.entryName;
            h.chunkIndex = chunkIndex;
            h.localHeaderOffset = row.localHeaderOffset;
            h.payloadOffset = payloadOffset;
            h.entryRelativeOffset = static_cast<std::uint64_t>(i);
            h.archiveOffset = payloadOffset + static_cast<std::uint64_t>(i);
            h.magic.assign(pat.text, pat.len);
            h.confidence = aff4ApfsConfidenceForMagic(h.magic, h.entryRelativeOffset);
            h.interpretation = aff4ApfsInterpretationForMagic(h.magic, h.entryRelativeOffset);
            const std::size_t sampleStart = i > 16 ? i - 16 : 0;
            const std::size_t sampleLen = std::min<std::size_t>(64, data.size() - sampleStart);
            h.sampleHex = hexSampleBytes(data.data() + sampleStart, sampleLen);
            h.notes = "Signature scanned from ZIP entry payload inside the explicit AFF4 file only. Virtual disk offset is unresolved until AFF4 image chunk-map decoding is implemented.";
            hits.push_back(h);
            if (hits.size() >= maxHits) return;
        }
    }
}

void writeAff4ApfsExactFileSignatureScan(const fs::path& caseDir,
                                         const EvidenceSource& source,
                                         const fs::path& originalInput,
                                         const std::vector<Aff4ZipCentralDirectoryRow>& rows,
                                         Logger& log) {
    if (!isAff4SourcePath(originalInput)) return;
    const fs::path csvPath = caseDir / "aff4_apfs_exact_file_signature_scan.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_exact_file_signature_scan_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_EXACT_FILE_SIGNATURE_SCAN.md";

    std::vector<Aff4ZipCentralDirectoryRow> candidates;
    for (const auto& r : rows) {
        const int chunk = aff4ZipDataChunkIndex(r.entryName);
        if (r.method != 0) continue;
        if (chunk >= 0 && chunk < 64 && !aff4ZipIsIndexEntry(r.entryName)) {
            candidates.push_back(r);
        } else if (r.uncompressedSize <= 1024ULL * 1024ULL && (r.classification == "AFF4_METADATA" || r.classification == "SPOTLIGHT_PATH_HINT" || !r.spotlightHint.empty() || !r.apfsHint.empty())) {
            candidates.push_back(r);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        const int ca = aff4ZipDataChunkIndex(a.entryName);
        const int cb = aff4ZipDataChunkIndex(b.entryName);
        if (ca != cb) return ca < cb;
        return a.index < b.index;
    });

    const std::uint64_t byteBudget = 768ULL * 1024ULL * 1024ULL;
    const std::uint64_t maxEntryBytes = 64ULL * 1024ULL * 1024ULL;
    const std::size_t maxHits = 1000;
    std::uint64_t bytesScanned = 0;
    std::size_t entriesScanned = 0;
    std::size_t entriesSkipped = 0;
    bool budgetTruncated = false;
    std::vector<Aff4ApfsCandidateHit> hits;

    for (const auto& r : candidates) {
        if (hits.size() >= maxHits) break;
        if (r.uncompressedSize == 0 || r.compressedSize == 0 || r.compressedSize > maxEntryBytes || r.uncompressedSize > maxEntryBytes) { ++entriesSkipped; continue; }
        if (bytesScanned + r.compressedSize > byteBudget) { budgetTruncated = true; break; }
        std::uint64_t payloadOffset = 0;
        std::string error;
        if (!readAff4ZipLocalPayloadOffset(originalInput, r, payloadOffset, error)) { ++entriesSkipped; continue; }
        std::vector<unsigned char> payload;
        if (!readExactFileBytes(originalInput, payloadOffset, static_cast<std::size_t>(r.compressedSize), payload, error)) { ++entriesSkipped; continue; }
        bytesScanned += static_cast<std::uint64_t>(payload.size());
        ++entriesScanned;
        addAff4ApfsSignatureHits(r, payload, payloadOffset, aff4ZipDataChunkIndex(r.entryName), hits, maxHits);
    }

    std::size_t apfsHits = 0, gptHits = 0, spotlightHits = 0;
    for (const auto& h : hits) {
        if (h.magic == "NXSB" || h.magic == "APSB") ++apfsHits;
        if (h.magic == "EFI PART") ++gptHits;
        if (h.magic == "CoreSpotlight" || h.magic == ".Spotlight-V100") ++spotlightHits;
    }

    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,row_index,entry_name,chunk_index,local_header_offset,payload_offset,entry_relative_offset,archive_offset,magic,confidence,interpretation,sample_hex,notes\n";
        for (const auto& h : hits) {
            out << csvEscape(source.sourceId) << ','
                << csvEscape(pathString(originalInput)) << ','
                << csvEscape(inputSourceType(originalInput)) << ','
                << h.rowIndex << ','
                << csvEscape(h.entryName) << ','
                << h.chunkIndex << ','
                << h.localHeaderOffset << ','
                << h.payloadOffset << ','
                << h.entryRelativeOffset << ','
                << h.archiveOffset << ','
                << csvEscape(h.magic) << ','
                << csvEscape(h.confidence) << ','
                << csvEscape(h.interpretation) << ','
                << csvEscape(h.sampleHex) << ','
                << csvEscape(h.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_exact_file_signature_scan.csv: ") + ex.what());
    }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_ONLY\",\n";
        out << "  \"entries_considered\": " << candidates.size() << ",\n";
        out << "  \"entries_scanned\": " << entriesScanned << ",\n";
        out << "  \"entries_skipped\": " << entriesSkipped << ",\n";
        out << "  \"bytes_scanned\": " << bytesScanned << ",\n";
        out << "  \"byte_budget\": " << byteBudget << ",\n";
        out << "  \"budget_truncated\": " << (budgetTruncated ? "true" : "false") << ",\n";
        out << "  \"hit_count\": " << hits.size() << ",\n";
        out << "  \"apfs_hit_count\": " << apfsHits << ",\n";
        out << "  \"gpt_hit_count\": " << gptHits << ",\n";
        out << "  \"spotlight_hit_count\": " << spotlightHits << "\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_exact_file_signature_scan_summary.json: ") + ex.what());
    }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Exact-File Signature Scan\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This scan reads selected uncompressed ZIP entry payloads from the exact AFF4 file supplied through `--input`. It does not search the evidence drive, inspect sibling folders, call libaff4, call aff4imager, mount a volume, or export RAW/DD.\n\n";
        out << "## Result\n\n";
        out << "- Entries considered: `" << candidates.size() << "`\n";
        out << "- Entries scanned: `" << entriesScanned << "`\n";
        out << "- Bytes scanned: `" << bytesScanned << "`\n";
        out << "- APFS hits: `" << apfsHits << "`\n";
        out << "- GPT hits: `" << gptHits << "`\n";
        out << "- Spotlight hits: `" << spotlightHits << "`\n\n";
        out << "## Interpretation\n\n";
        out << "Hits in this file identify signatures inside AFF4 ZIP member payloads. They are not yet final virtual disk offsets because the AFF4 image chunk map still needs to be decoded. The next larger milestone is to decode AFF4 image metadata/index entries enough to map chunk numbers to virtual offsets and feed those mapped reads into APFS/GPT parsing.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_EXACT_FILE_SIGNATURE_SCAN.md: ") + ex.what());
    }
    log.info("AFF4 APFS exact-file signature scan written: " + pathString(csvPath));
}

void writeAff4ZipSingleFileProbe(const fs::path& caseDir,
                                 const EvidenceSource& source,
                                 const RunOptions& opt,
                                 const fs::path& originalInput,
                                 Logger& log) {
    (void)opt;
    if (!isAff4SourcePath(originalInput)) return;

    const fs::path csvPath = caseDir / "aff4_zip_central_directory.csv";
    const fs::path jsonPath = caseDir / "aff4_zip_probe_summary.json";
    const fs::path planPath = caseDir / "AFF4_ZIP_SINGLE_FILE_PROBE.md";

    std::vector<Aff4ZipCentralDirectoryRow> rows;
    std::string status = "NOT_RUN";
    std::string notes;
    bool eocdFound = false;
    bool zip64Used = false;
    bool entriesTruncated = false;
    std::uint64_t fileSize = 0;
    std::uint64_t centralDirOffset = 0;
    std::uint64_t centralDirSize = 0;
    std::uint64_t entriesTotal = 0;
    std::uint64_t eocdOffset = 0;
    const std::size_t maxEntriesToWrite = 10000;

    try {
        std::error_code ec;
        fileSize = static_cast<std::uint64_t>(fs::file_size(originalInput, ec));
        if (ec || fileSize == 0) {
            status = "FILE_SIZE_UNAVAILABLE";
            notes = ec ? ec.message() : "File size is zero.";
        } else {
            const std::uint64_t tailBytes64 = std::min<std::uint64_t>(fileSize, 1024ULL * 1024ULL);
            std::vector<unsigned char> tail;
            std::string error;
            if (!readExactFileBytes(originalInput, fileSize - tailBytes64, static_cast<std::size_t>(tailBytes64), tail, error)) {
                status = "TAIL_READ_FAILED";
                notes = error;
            } else {
                std::size_t eocdPos = tail.size();
                for (std::size_t i = tail.size() >= 4 ? tail.size() - 4 : 0; i + 4 <= tail.size(); --i) {
                    if (readLe32(tail, i) == 0x06054b50U) { eocdPos = i; break; }
                    if (i == 0) break;
                }
                if (eocdPos == tail.size()) {
                    status = "ZIP_EOCD_NOT_FOUND";
                    notes = "No ZIP End Of Central Directory signature was found in the final 1 MiB of the explicit AFF4 file. No parent folder or drive scan was attempted.";
                } else if (eocdPos + 22 > tail.size()) {
                    status = "ZIP_EOCD_TRUNCATED";
                    notes = "ZIP EOCD signature found but fixed EOCD fields were truncated.";
                } else {
                    eocdFound = true;
                    eocdOffset = (fileSize - tailBytes64) + static_cast<std::uint64_t>(eocdPos);
                    const std::uint16_t diskNo = readLe16(tail, eocdPos + 4);
                    const std::uint16_t cdDisk = readLe16(tail, eocdPos + 6);
                    const std::uint16_t entriesDisk16 = readLe16(tail, eocdPos + 8);
                    const std::uint16_t entriesTotal16 = readLe16(tail, eocdPos + 10);
                    const std::uint32_t cdSize32 = readLe32(tail, eocdPos + 12);
                    const std::uint32_t cdOffset32 = readLe32(tail, eocdPos + 16);
                    entriesTotal = entriesTotal16;
                    centralDirSize = cdSize32;
                    centralDirOffset = cdOffset32;
                    const bool needsZip64 = diskNo == 0xffff || cdDisk == 0xffff || entriesDisk16 == 0xffff || entriesTotal16 == 0xffff || cdSize32 == 0xffffffffU || cdOffset32 == 0xffffffffU;
                    if (needsZip64) {
                        if (eocdPos < 20) {
                            status = "ZIP64_LOCATOR_NOT_FOUND";
                            notes = "Classic EOCD uses ZIP64 sentinel values, but ZIP64 locator was not found before EOCD.";
                        } else {
                            std::size_t locatorPos = tail.size();
                            const std::size_t minPos = eocdPos > 1024 ? eocdPos - 1024 : 0;
                            for (std::size_t i = eocdPos - 20; i + 20 <= eocdPos && i >= minPos; --i) {
                                if (readLe32(tail, i) == 0x07064b50U) { locatorPos = i; break; }
                                if (i == 0) break;
                            }
                            if (locatorPos == tail.size()) {
                                status = "ZIP64_LOCATOR_NOT_FOUND";
                                notes = "Classic EOCD uses ZIP64 sentinel values, but no ZIP64 locator signature was found near EOCD.";
                            } else {
                                const std::uint64_t zip64EocdOffset = readLe64(tail, locatorPos + 8);
                                std::vector<unsigned char> z64;
                                std::string zerr;
                                if (!readExactFileBytes(originalInput, zip64EocdOffset, 56, z64, zerr) || readLe32(z64, 0) != 0x06064b50U) {
                                    status = "ZIP64_EOCD_READ_FAILED";
                                    notes = zerr.empty() ? "ZIP64 EOCD signature mismatch." : zerr;
                                } else {
                                    zip64Used = true;
                                    entriesTotal = readLe64(z64, 32);
                                    centralDirSize = readLe64(z64, 40);
                                    centralDirOffset = readLe64(z64, 48);
                                    status = "ZIP64_EOCD_READY";
                                }
                            }
                        }
                    } else {
                        status = "ZIP_EOCD_READY";
                    }

                    if ((status == "ZIP_EOCD_READY" || status == "ZIP64_EOCD_READY") && centralDirOffset < fileSize && centralDirSize > 0) {
                        std::ifstream in(originalInput, std::ios::binary);
                        if (!in) {
                            status = "CENTRAL_DIRECTORY_OPEN_FAILED";
                            notes = "Could not reopen explicit AFF4 file for central directory parsing.";
                        } else {
                            in.seekg(static_cast<std::streamoff>(centralDirOffset), std::ios::beg);
                            std::uint64_t pos = centralDirOffset;
                            std::uint64_t end = std::min<std::uint64_t>(fileSize, centralDirOffset + centralDirSize);
                            std::uint64_t idx = 0;
                            while (pos + 46 <= end && idx < entriesTotal && rows.size() < maxEntriesToWrite) {
                                std::vector<unsigned char> fixed(46, 0);
                                in.read(reinterpret_cast<char*>(fixed.data()), 46);
                                if (in.gcount() != 46) break;
                                if (readLe32(fixed, 0) != 0x02014b50U) break;
                                const std::uint16_t method = readLe16(fixed, 10);
                                const std::uint32_t comp32 = readLe32(fixed, 20);
                                const std::uint32_t uncomp32 = readLe32(fixed, 24);
                                const std::uint16_t nameLen = readLe16(fixed, 28);
                                const std::uint16_t extraLen = readLe16(fixed, 30);
                                const std::uint16_t commentLen = readLe16(fixed, 32);
                                const std::uint32_t local32 = readLe32(fixed, 42);
                                std::vector<unsigned char> nameBytes(nameLen, 0);
                                if (nameLen) in.read(reinterpret_cast<char*>(nameBytes.data()), nameLen);
                                std::vector<unsigned char> extra(extraLen, 0);
                                if (extraLen) in.read(reinterpret_cast<char*>(extra.data()), extraLen);
                                if (commentLen) in.seekg(commentLen, std::ios::cur);
                                pos += 46ULL + nameLen + extraLen + commentLen;
                                if (!in) break;
                                std::string name(nameBytes.begin(), nameBytes.end());
                                std::uint64_t comp = comp32;
                                std::uint64_t uncomp = uncomp32;
                                std::uint64_t local = local32;
                                std::size_t ex = 0;
                                while (ex + 4 <= extra.size()) {
                                    const std::uint16_t headerId = readLe16(extra, ex);
                                    const std::uint16_t dataSize = readLe16(extra, ex + 2);
                                    ex += 4;
                                    if (ex + dataSize > extra.size()) break;
                                    if (headerId == 0x0001) {
                                        std::size_t zp = ex;
                                        if (uncomp32 == 0xffffffffU && zp + 8 <= ex + dataSize) { uncomp = readLe64(extra, zp); zp += 8; }
                                        if (comp32 == 0xffffffffU && zp + 8 <= ex + dataSize) { comp = readLe64(extra, zp); zp += 8; }
                                        if (local32 == 0xffffffffU && zp + 8 <= ex + dataSize) { local = readLe64(extra, zp); zp += 8; }
                                    }
                                    ex += dataSize;
                                }
                                Aff4ZipCentralDirectoryRow row;
                                row.index = static_cast<std::size_t>(idx);
                                row.entryName = name;
                                row.method = method;
                                row.compressedSize = comp;
                                row.uncompressedSize = uncomp;
                                row.localHeaderOffset = local;
                                row.classification = aff4ZipEntryClassification(name);
                                row.spotlightHint = aff4ZipSpotlightHint(name);
                                row.apfsHint = aff4ZipApfsHint(name);
                                row.notes = "Central-directory entry read from the explicit AFF4 file only; no directory or drive enumeration was performed.";
                                rows.push_back(row);
                                ++idx;
                            }
                            entriesTruncated = (entriesTotal > rows.size());
                            if (status == "ZIP_EOCD_READY" || status == "ZIP64_EOCD_READY") status = "CENTRAL_DIRECTORY_PARSED";
                            if (entriesTruncated) notes = "Central directory output was capped for thin upload review; inspect local file or raise cap if needed.";
                        }
                    }
                }
            }
        }
    } catch (const std::exception& ex) {
        status = "EXCEPTION";
        notes = ex.what();
    }

    writeAff4ApfsExactFileSignatureScan(caseDir, source, originalInput, rows, log);

    try {
        std::ofstream out(csvPath, std::ios::binary);
        out << "source_id,input_path,input_type,row_index,entry_name,compression_method,compressed_size,uncompressed_size,local_header_offset,classification,spotlight_hint,apfs_hint,notes\n";
        for (const auto& r : rows) {
            out << csvEscape(source.sourceId) << ','
                << csvEscape(pathString(originalInput)) << ','
                << csvEscape(inputSourceType(originalInput)) << ','
                << r.index << ','
                << csvEscape(r.entryName) << ','
                << r.method << ','
                << r.compressedSize << ','
                << r.uncompressedSize << ','
                << r.localHeaderOffset << ','
                << csvEscape(r.classification) << ','
                << csvEscape(r.spotlightHint) << ','
                << csvEscape(r.apfsHint) << ','
                << csvEscape(r.notes) << "\n";
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_zip_central_directory.csv: ") + ex.what());
    }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << source.sourceId << "\",\n";
        out << "  \"input_path\": \"" << jsonEscape(pathString(originalInput)) << "\",\n";
        out << "  \"probe_scope\": \"EXACT_INPUT_FILE_ONLY\",\n";
        out << "  \"status\": \"" << jsonEscape(status) << "\",\n";
        out << "  \"file_size_bytes\": " << fileSize << ",\n";
        out << "  \"eocd_found\": " << (eocdFound ? "true" : "false") << ",\n";
        out << "  \"zip64_used\": " << (zip64Used ? "true" : "false") << ",\n";
        out << "  \"eocd_offset\": " << eocdOffset << ",\n";
        out << "  \"central_directory_offset\": " << centralDirOffset << ",\n";
        out << "  \"central_directory_size\": " << centralDirSize << ",\n";
        out << "  \"entries_total_reported\": " << entriesTotal << ",\n";
        out << "  \"entries_written\": " << rows.size() << ",\n";
        out << "  \"entries_truncated\": " << (entriesTruncated ? "true" : "false") << ",\n";
        out << "  \"notes\": \"" << jsonEscape(notes) << "\"\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_zip_probe_summary.json: ") + ex.what());
    }

    try {
        std::ofstream out(planPath, std::ios::binary);
        out << "# AFF4 ZIP Single-File Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Purpose\n\n";
        out << "This probe opens only the explicit AFF4 path supplied through `--input`, reads the ZIP End Of Central Directory and central-directory entries from that same file, and writes a bounded inventory for review. It does not recurse the evidence drive, search sibling folders, call libaff4, call aff4imager, or export a RAW/DD image.\n\n";
        out << "## Result\n\n";
        out << "- Status: `" << status << "`\n";
        out << "- Input: `" << pathString(originalInput) << "`\n";
        out << "- File size: `" << fileSize << "`\n";
        out << "- EOCD found: `" << (eocdFound ? "true" : "false") << "`\n";
        out << "- ZIP64 used: `" << (zip64Used ? "true" : "false") << "`\n";
        out << "- Entries reported: `" << entriesTotal << "`\n";
        out << "- Entries written: `" << rows.size() << "`\n\n";
        out << "## Next use\n\n";
        out << "If central-directory entries identify image-stream metadata or Spotlight/CoreSpotlight hints, use that row inventory to design the next constrained AFF4 stream-selection step without asking a third-party reader to discover sibling AFF4 files on the evidence drive.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_ZIP_SINGLE_FILE_PROBE.md: ") + ex.what());
    }

    log.info("AFF4 ZIP single-file probe written: " + pathString(csvPath));
}


bool validateRunOptions(const RunOptions& opt, std::string& error) {
    const auto mode = toLower(opt.mode.empty() ? "run" : opt.mode);
    if (opt.input.empty() && mode != "init-case") { error = "--input is required"; return false; }
    if (opt.output.empty() && opt.caseDir.empty()) { error = "--out or --case is required"; return false; }
    if (opt.strictSingleAff4) {
        if (!isAff4SourcePath(opt.input)) { error = "--strict-single-aff4 requires --input to be one explicit .aff4 file"; return false; }
        std::error_code ec;
        if (fs::is_directory(opt.input, ec)) { error = "--strict-single-aff4 input is a directory; provide one explicit .aff4 file"; return false; }
    }
    if (!opt.reuseIosCache.empty()) {
        std::error_code ec;
        if (!fs::is_directory(opt.reuseIosCache, ec)) { error = "--reuse-ios-cache must point to an existing completed case/cache folder"; return false; }
        if (!isZipSourcePath(opt.input)) { error = "--reuse-ios-cache currently applies only to ZIP iOS FFS sources"; return false; }
    }
    return true;
}


RunResult runApplication(const RunOptions& opt, const std::atomic_bool* cancelToken) {
    installStructuredExceptionTranslator();
    const fs::path caseDir = !opt.caseDir.empty() ? opt.caseDir : opt.output;
    fs::create_directories(caseDir);
    appendRunStatus(caseDir, "start", "application entry");
    Logger log(caseDir / "logs", opt.verbose);
    log.info("Run started. app_version=" + appVersion());
    log.info("Input=" + pathString(opt.input));
    log.info("CaseDir=" + pathString(caseDir));
    log.info("EvidenceRoot=" + pathString(opt.evidenceRoot));
    if (!opt.reuseIosCache.empty()) log.info("ReuseIosCache=" + pathString(opt.reuseIosCache));
    if (opt.experimentalFullNativeValues && toLower(opt.exportProfile) == "investigator") {
        appendRunStatus(caseDir, "validation_profile_full_values_investigator", "full native metadata decoding with investigator exports enabled");
        log.info("Full validation profile active: FullValues decode plus investigator export profile.");
    } else if (!opt.experimentalFullNativeValues && (toLower(opt.exportProfile) == "investigator" || toLower(opt.exportProfile) == "full")) {
        appendRunStatus(caseDir, "validation_warning_metadata_limited_export", "investigator/full exports requested without full native metadata values");
        log.warn("Investigator/full exports requested without full native metadata values. Usage, WhereFroms, path and tag-support views may be incomplete.");
    }
    RunResult result;
    auto returnCancelled = [&](const std::string& stage) -> RunResult {
        appendRunStatus(caseDir, "cancelled", stage);
        log.warn("Ingest cancelled by investigator request at stage: " + stage);
        result.exitCode = 4;
        result.messages = log.messages();
        return result;
    };
    auto cancelRequested = [&]() -> bool {
        return cancelToken && cancelToken->load();
    };
    if (cancelRequested()) return returnCancelled("startup");
    const auto topModeLower = toLower(opt.mode.empty() ? "run" : opt.mode);
    const bool diagnosticsMode = (topModeLower == "diagnostics" || topModeLower == "diagnostic");
    const bool sourceProbeMode = (topModeLower == "source-probe" || topModeLower == "source_probe" || topModeLower == "probe-source" || topModeLower == "source-intake");
    if (diagnosticsMode) {
        log.info("Diagnostics mode enabled: skipping 7z preservation by default and enabling safe core native probe diagnostics.");
    }
    if (sourceProbeMode) {
        appendRunStatus(caseDir, "source_probe_start", "source intake/readiness probe only; no parsing/enrichment will be run");
        log.info("Source-probe mode enabled: source will be identified, registered, and reported without running parser/enrichment.");
    }
    try {
        appendRunStatus(caseDir, "initialize_case");
        CaseStore store(caseDir);
        store.initialize(opt, log);
        EvidenceSource source = store.addSource(opt, log);
        EvidenceSource discoverySource = source;
        const bool zipInputSource = isZipSourcePath(opt.input);
        const bool aff4InputSource = isAff4SourcePath(opt.input);
        const bool rawImageInputSource = isRawImageSourcePath(opt.input);
        auto profile = parseProfileKind(opt.profile);
        const bool sourceProbeFullScan = opt.fullScan && !zipInputSource;
        if (cancelRequested()) return returnCancelled("before_source_probe_signature_scan");
        appendRunStatus(caseDir, "source_probe_signature_scan",
                        zipInputSource
                            ? "bounded ZIP signature probe; ZIP entries will be enumerated during staging/focused extraction"
                            : (sourceProbeFullScan ? "full source signature probe" : "bounded source signature probe"));
        if (zipInputSource && opt.fullScan) {
            log.info("Full-scan parsing remains enabled, but pre-stage source signature probing is bounded for ZIP containers to avoid scanning very large iOS FFS ZIPs before focused CoreSpotlight extraction.");
        }
        SourceProbeFindings sourceProbe = probeEvidenceSourceSignatures(opt.input, sourceProbeFullScan, log);
        if (cancelRequested()) return returnCancelled("after_source_probe_signature_scan");
        appendRunStatus(caseDir, "source_probe_signature_complete",
                        "bytes_scanned=" + std::to_string(sourceProbe.bytesScanned) + " hits=" + std::to_string(sourceProbe.hits.size()));
        PartitionProbeFindings partitionProbe = rawImageInputSource ? probeRawImagePartitions(opt.input, sourceProbe, log) : PartitionProbeFindings{};
        if (cancelRequested()) return returnCancelled("after_partition_probe");
        fs::path stagedContainerWorkingRoot;

        if (cancelRequested()) return returnCancelled("before_source_staging");
        if (zipInputSource) {
            appendRunStatus(caseDir, "stage_zip_source", profile == SourceProfileKind::IOS ? "extract iOS CoreSpotlight entries from ZIP to normalized staging folder" : "auto-detect ZIP Spotlight/CoreSpotlight entries and stage normalized working files");
            bool iosFocusedUsed = false;
            if (!opt.reuseIosCache.empty()) {
                stagedContainerWorkingRoot = stageZipEvidenceSourceFromCache(opt.reuseIosCache, opt.input, caseDir, log, &iosFocusedUsed);
            } else {
                stagedContainerWorkingRoot = stageZipEvidenceSource(opt.input, caseDir, profile, log, &iosFocusedUsed);
                writeSourceCacheManifest(caseDir, opt.input, stagedContainerWorkingRoot, {}, "CREATED_IOS_ZIP_INTAKE_CACHE", log);
            }
            if (iosFocusedUsed && profile == SourceProfileKind::Auto) {
                profile = SourceProfileKind::IOS;
                log.info("Auto ZIP profile promoted to iOS/CoreSpotlight because focused ZIP entry inventory found store.db/.store.db entries or a prior iOS cache was reused.");
                appendRunStatus(caseDir, "zip_profile_auto_promoted_ios", "focused CoreSpotlight store.db/.store.db entries found or cached iOS staging reused");
            }
            if (cancelRequested()) return returnCancelled("after_zip_staging");
            discoverySource.inputPath = stagedContainerWorkingRoot;
            discoverySource.profile = profileKindToString(profile);
            discoverySource.sourceKind = "zip_staged_spotlight_source";
            discoverySource.notes = "Original ZIP registered as fixed evidence source; parser discovery redirected to controlled extracted staging folder: " + pathString(discoverySource.inputPath);
            source.profile = profileKindToString(profile);
            source.sourceKind = "zip_spotlight_source";
            source.notes = discoverySource.notes + "; no new evidentiary archive is created for an original ZIP container.";
            if (!opt.reuseIosCache.empty()) {
                source.notes += "; iOS intake/staging was reused from prior cache case: " + pathString(opt.reuseIosCache);
            }
        } else if (aff4InputSource) {
            source.sourceKind = "aff4_container_source";
            source.notes = "Original AFF4 container registered as fixed evidence source; AFF4 stream extraction plus APFS/HFS/HFS+ filesystem extraction is staged for a later container-reader build.";
            discoverySource = source;
            if (sourceProbeMode) {
                log.info("Single AFF4 source-probe: treating --input as the one explicit AFF4 container. No recursive AFF4 discovery/search of the parent drive is performed.");
            }
        } else if (rawImageInputSource) {
            source.sourceKind = "raw_image_container_source";
            source.notes = "Original raw flat image registered as fixed evidence source; partition scanning plus APFS/HFS/HFS+ filesystem extraction is staged for a later filesystem-reader build.";
            discoverySource = source;
        } else {
            source.sourceKind = "folder_spotlight_source";
            source.notes = "Loose folder source; static evidentiary archive/container should be created before parsing unless explicitly skipped.";
            discoverySource = source;
        }

        std::vector<EvidenceSource> sources{source};
        store.writeSources(sources);
        result.sourceCount = 1;
        writeSourceProbeSignatureCsv(caseDir, sourceProbe, log);
        writeSourceProbeJson(caseDir, source, opt.input, sourceProbe, partitionProbe, log);
        writeSourcePartitionProbeCsv(caseDir, partitionProbe, log);

        CaseDatabase db;
        appendRunStatus(caseDir, "open_sqlite", "single case database handle opened for this run");
        db.open(caseDir / "VestigantSpotlight.case.sqlite");
        db.initializeSchema();
        db.insertCaseInfo(opt);
        db.insertEvidenceSource(source);

        if (aff4InputSource || rawImageInputSource) {
            const bool deferLargeImageHash = (opt.skipContainerHash || (sourceProbeMode && (aff4InputSource || rawImageInputSource) && !opt.forceContainerHash));
            if (deferLargeImageHash && !opt.skipContainerHash && !opt.forceContainerHash) {
                log.warn("Full original-container SHA256 deferred by default for AFF4/raw source-probe development speed. Use --force-container-hash when a full evidentiary hash is needed.");
            }
            registerOriginalContainerSource(db, source, opt.input, {}, source.notes, log, deferLargeImageHash);
            const std::string stage = aff4InputSource ? "aff4_apfs_source_registered" : "unsupported_raw_image_source";
            const std::string message = aff4InputSource ? "AFF4 registered; guarded APFS metadata and Store-V2 staging pipeline active; full native source-discovery handoff still staged" : "raw image registered; partition/filesystem extraction not implemented";
            const std::string nextAction = aff4InputSource
                ? "Implement AFF4 stream enumeration, then APFS filesystem inventory, then Spotlight artifact staging and active-file comparison."
                : "Use V0_8_9 source_partition_probe.csv for partition readiness; raw image extraction remains secondary to AFF4-backed APFS inventory and active-file comparison.";
            log.warn((aff4InputSource ? "AFF4" : "Raw flat image") + std::string(" source selected and registered. Full container/filesystem extraction is not implemented in this build; source-probe output will document the required next reader layer."));
            appendRunStatus(caseDir, stage, message);
            appendRunStatus(caseDir, "source_probe_write", "write source intake readiness and roadmap artifacts");
            writeSourceIntakeArtifacts(caseDir, source, opt.input, {}, {}, "REGISTERED_UNSUPPORTED_CONTAINER", nextAction, sourceProbe, partitionProbe, log);
            writeImageInventoryReadinessCsv(caseDir, source, opt.input, sourceProbe, partitionProbe, nextAction, log);
            writeReaderToolReadinessArtifacts(caseDir, opt, source, opt.input, sourceProbe, log);
            if (aff4InputSource) {
                writeAff4ApfsV1DiagnosticRerunPlan(caseDir, source, opt, opt.input, log);
                appendRunStatus(caseDir, "aff4_zip_single_file_probe", "parse AFF4 ZIP central directory from explicit input file only");
                writeAff4ZipSingleFileProbe(caseDir, source, opt, opt.input, log);
                appendRunStatus(caseDir, "aff4_apfs_exact_file_scan", "scan selected ZIP member payloads from the exact AFF4 file for GPT/APFS/Spotlight signatures");
                appendRunStatus(caseDir, "aff4_dynamic_load_probe", opt.enableAff4DynamicProbe ? "load libaff4 and perform bounded AFF4 random-access smoke test" : "skipped by default to avoid reader-driven access to other AFF4 files");
                writeAff4CppLiteDynamicLoadProbe(caseDir, source, opt, opt.input, log);
                appendRunStatus(caseDir, "aff4_apfs_staged_storev2_parser_probe", "discover and parse copied Store-V2 candidates if APFS extraction staged them");
                runAff4ApfsStagedStoreV2ParserProbe(caseDir, source, opt, db, log);
                appendRunStatus(caseDir, "aff4_apfs_staged_storev2_enrichment_probe", "enrich APFS-staged Store-V2 parser rows when present");
                const auto stagedEnrichmentCounts = runAff4ApfsStagedStoreV2EnrichmentProbe(caseDir, source, db, log);
                result.rawRecordCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.rawRecordsBefore));
                result.rawKeyValueCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.rawKeyValuesBefore));
                result.rawDateCandidateCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.rawDateCandidatesBefore));
                result.artifactCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.artifacts));
                result.timelineCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.timelineEvents));
                result.usageCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.usageEvidence));
                result.nativeDecodeMode = "AFF4_APFS_STAGED_STOREV2_CORE_FIELDS";
                appendRunStatus(caseDir, "aff4_stream_inventory_start", opt.enableAff4StreamInventory ? "list AFF4 streams when aff4imager is available" : "skipped by default to avoid external reader discovery outside selected AFF4");
                const auto aff4Inventory = runAff4StreamInventory(
                    opt, source, opt.input, caseDir, log,
                    [&](const std::string& envVar, const std::vector<std::string>& names) {
                        return findToolCandidate(opt, envVar, names);
                    },
                    [&](const fs::path& exe, const std::vector<std::string>& args, const fs::path& outputPath) {
#if defined(_WIN32)
                        return runExecutableNoWindowRedirected(exe, args, outputPath);
#else
                        (void)exe; (void)args; (void)outputPath;
                        return -1;
#endif
                    },
                    [&](const std::string& command) {
                        return runShellCommandNoWindow(command);
                    });
                appendRunStatus(caseDir, "aff4_stream_inventory_complete", aff4Inventory.status + " lines=" + std::to_string(aff4Inventory.rawLineCount));
            }
            persistSourceProbeInventory(db, source, opt.input, {}, {}, "REGISTERED_UNSUPPORTED_CONTAINER", nextAction, sourceProbe, partitionProbe, log);
            persistImageInventoryReadiness(db, source, opt.input, sourceProbe, partitionProbe, nextAction, log);
            store.writeSummary(result);
            writeUiAndIosPlanningFiles(caseDir);
            createUploadBundle(caseDir);
            if (sourceProbeMode || topModeLower == "discover") {
                appendRunStatus(caseDir, "complete_source_probe", "unsupported container registered and readiness report written");
                refreshUploadRunDiagnostics(caseDir);
                result.messages = log.messages();
                return result;
            }
            result.exitCode = 3;
            appendRunStatus(caseDir, "failed_unsupported_container", message + "; use --mode source-probe for non-failing intake registration only");
            refreshUploadRunDiagnostics(caseDir);
            result.messages = log.messages();
            return result;
        }

        appendRunStatus(caseDir, "discover_stores");
        auto stores = discoverStores(discoverySource, profile, opt.fullScan, log);
        result.databaseCandidateCount = stores.size();
        result.validDatabaseCandidateCount = countValidDatabaseCandidates(stores);
        result.storeCount = countDistinctStoreGroups(stores, false);
        result.validStoreCount = countDistinctStoreGroups(stores, true);
        log.info("Store discovery completed. store_groups=" + std::to_string(result.storeCount) +
                 " valid_store_groups=" + std::to_string(result.validStoreCount) +
                 " database_candidates=" + std::to_string(result.databaseCandidateCount) +
                 " valid_database_candidates=" + std::to_string(result.validDatabaseCandidateCount));
        store.writeStoreInventory(stores);
        appendRunStatus(caseDir, "source_probe_write", "write source intake readiness and roadmap artifacts");
        writeSourceIntakeArtifacts(caseDir, source, opt.input, discoverySource.inputPath, stores, "DISCOVERY_COMPLETE", "Proceed to parser selection and native parsing for supported folder/ZIP sources.", sourceProbe, partitionProbe, log);
        writeImageInventoryReadinessCsv(caseDir, source, opt.input, sourceProbe, partitionProbe, "Folder/ZIP Spotlight parsing can proceed; active image comparison requires image_file_inventory from AFF4/APFS enumeration.", log);

        persistSourceProbeInventory(db, source, opt.input, discoverySource.inputPath, stores, "DISCOVERY_COMPLETE", "Proceed to parser selection and native parsing for supported folder/ZIP sources.", sourceProbe, partitionProbe, log);
        persistImageInventoryReadiness(db, source, opt.input, sourceProbe, partitionProbe, "Folder/ZIP Spotlight parsing can proceed; active image comparison requires image_file_inventory from AFF4/APFS enumeration.", log);
        if (zipInputSource) {
            bool effectiveSkipContainerHash = opt.skipContainerHash;
            if (profile == SourceProfileKind::IOS && !opt.forceContainerHash && !effectiveSkipContainerHash) {
                effectiveSkipContainerHash = true;
                appendRunStatus(caseDir, "original_container_hash_deferred_ios_zip", "large iOS ZIP source registered without full SHA256 for parser/review speed; use --force-container-hash for evidentiary hash run");
                log.warn("Full original-container SHA256 deferred by default for iOS FFS ZIP parser/review speed. Use --force-container-hash when a full evidentiary hash is required.");
            }
            registerOriginalContainerSource(db, source, opt.input, stagedContainerWorkingRoot, source.notes, log, effectiveSkipContainerHash);
            if (profile == SourceProfileKind::IOS) {
                const auto epLowerForIos = toLower(opt.exportProfile);
                const bool supportMaterializationRequested = diagnosticsMode || opt.diagnosticFullNativeDb || epLowerForIos == "diagnostics" || epLowerForIos == "support" || epLowerForIos == "full";
                const bool materializeFfsInventory = opt.materializeIosFfsInventory || supportMaterializationRequested;
                const bool materializeAppDbRecords = opt.materializeIosAppDbRecords || supportMaterializationRequested;
                if (!materializeFfsInventory) {
                    appendRunStatus(caseDir, "ios_ffs_inventory_materialization_skipped", "Spotlight-first normal mode references/counts cached FFS inventory instead of inserting millions of file rows; use --materialize-ios-ffs-inventory for support correlation runs");
                    log.info("Normal iOS Spotlight-first mode will not materialize full FFS inventory rows into the active case DB.");
                }
                EvidenceIntake::importIosInventoryCsvs(db, caseDir, source.sourceId, log, opt.reuseIosCache, materializeFfsInventory,
                    [](const fs::path& statusCaseDir, const std::string& stage, const std::string& message) {
                        appendRunStatus(statusCaseDir, stage, message);
                    });
                if (materializeAppDbRecords) {
                    parseIosAppDatabaseRecordInventories(db, caseDir, source.sourceId, log);
                } else {
                    appendRunStatus(caseDir, "ios_app_db_record_inventory_targeted_normal_mode", "normal Spotlight-first mode parses only already-extracted high-value app databases; full broad app DB materialization still requires --materialize-ios-app-db-records");
                    log.info("Normal iOS Spotlight-first mode will parse only already-extracted high-value app databases for investigator summaries.");
                    parseIosAppDatabaseRecordInventories(db, caseDir, source.sourceId, log);
                }
            }
        }

        const auto modeLower = toLower(opt.mode.empty() ? "run" : opt.mode);
        if (modeLower == "discover" || sourceProbeMode) {
            log.info((sourceProbeMode ? "Source-probe" : "Discover-only") + std::string(" mode completed. No parsing/enrichment was run."));
            store.writeSummary(result);
            writeUiAndIosPlanningFiles(caseDir);
            createUploadBundle(caseDir);
            appendRunStatus(caseDir, sourceProbeMode ? "complete_source_probe" : "complete_discover");
            refreshUploadRunDiagnostics(caseDir);
            result.messages = log.messages();
            return result;
        }

        EvidenceSource parseSource = discoverySource;
        std::vector<StoreInfo> parseStores = stores;
        const bool originalContainerInput = zipInputSource || aff4InputSource || rawImageInputSource;
        const bool shouldPreserveEvidence = opt.preserveEvidence && !originalContainerInput && (!diagnosticsMode || opt.preserveEvidenceExplicit);
        if (zipInputSource) {
            log.info("ZIP input was extracted into EvidenceStaging for working analysis. Archive-first preservation is skipped because the original ZIP is already a fixed evidence container and has been hashed/registered.");
        }
        if (diagnosticsMode && !shouldPreserveEvidence) {
            appendRunStatus(caseDir, "diagnostics_no_preserve", "7z preservation skipped for faster diagnostics");
            log.info("Diagnostics mode: evidence archive and staging creation skipped. Use --preserve with --mode diagnostics to test archive-first behavior.");
        }
        if (shouldPreserveEvidence) {
            appendRunStatus(caseDir, "preserve_evidence");
            EvidencePreserver preserver;
            auto preservation = preserver.preserve(opt, source, stores, db, log);
            if (preservation.preserved && !preservation.parseInputRoot.empty()) {
                parseSource.inputPath = preservation.parseInputRoot;
                log.info("Parsing redirected to preserved staging copy: " + pathString(parseSource.inputPath));
                appendRunStatus(caseDir, "rediscover_preserved_stores");
                // The preservation staging folder may intentionally contain only the Store-V2
                // UUID folders rather than the original .Spotlight-V100/Store-V2 ancestor.
                // In that case a strict macOS/iOS profile path filter can reject valid
                // preserved store.db files because the path no longer contains Store-V2.
                // Force full-scan rediscovery for the preserved staging copy so the parser
                // operates from the preserved evidence rather than falling back to originals.
                parseStores = discoverStores(parseSource, profile, true, log);
                if (parseStores.empty()) {
                    log.warn("No stores were rediscovered in the preservation staging folder with the selected profile; retrying preserved rediscovery with Auto profile.");
                    parseStores = discoverStores(parseSource, SourceProfileKind::Auto, true, log);
                }
                log.info("Preserved store rediscovery completed. stores=" + std::to_string(parseStores.size()));
                if (stores.size() > 0 && parseStores.empty()) {
                    result.exitCode = 2;
                    log.error("Original source discovery found stores, but preserved staging rediscovery found zero stores. Aborting before parse so the issue is visible.");
                    appendRunStatus(caseDir, "failed_no_preserved_stores");
                    store.writeSummary(result);
                    result.messages = log.messages();
                    return result;
                }
            }
        } else if (!diagnosticsMode) {
            log.warn("Evidence preservation was skipped by user option. This is not recommended for case processing.");
        }

        if (cancelRequested()) return returnCancelled("before_native_parse_selection");
        const auto parserCandidates = parseStores;
        parseStores = selectDatabasesForParsing(parserCandidates, profile, opt.fullScan, log);
        writeStoreSelectionCsv(caseDir, parserCandidates, parseStores, log);
        result.selectedParserDatabaseCount = parseStores.size();
        appendRunStatus(caseDir, "native_parse_start", "stores=" + std::to_string(parseStores.size()));
        NativeDecodeMode decodeMode = NativeDecodeMode::HeaderOnly;
        if (opt.experimentalFullNativeValues) decodeMode = NativeDecodeMode::FullValues;
        else if (opt.decodeCoreNativeValues || diagnosticsMode) decodeMode = NativeDecodeMode::CoreFields;
        std::size_t nativeRecordLimit = opt.maxNativeRecords;
        if ((diagnosticsMode || opt.diagnosticFullNativeDb) && opt.experimentalFullNativeValues && !opt.maxNativeRecordsExplicit) {
            nativeRecordLimit = 10000;
            log.info("Diagnostics/full-native mode defaulting to max_native_records=10000 for crash-safe sampling. Override explicitly with --max-native-records after smaller samples succeed.");
        }
        NativeStoreDbParser parser(decodeMode, nativeRecordLimit, opt.maxNativeBlocks);
        parser.setProgressPath(caseDir / "logs" / "run_progress.tsv");
        parser.setPersistAllNativeKeyValues(opt.diagnosticFullNativeDb);
        parser.setDbSizeGuardrailBytes(opt.dbSizeGuardrailBytes);
        if (opt.diagnosticFullNativeDb) {
            appendRunStatus(caseDir, "native_kv_persistence_full", "full native key/value persistence explicitly enabled for diagnostics");
            log.warn("Diagnostic full native key/value persistence is enabled. iOS CoreSpotlight databases can grow very large.");
        } else {
            appendRunStatus(caseDir, "native_kv_persistence_filtered", "default iOS CoreSpotlight mode keeps compact high-value key/value rows and bounded per-record date provenance");
            log.info("Default iOS CoreSpotlight key/value/date persistence is compact: broad dbStr/property rows and broad date candidates are summarized/suppressed. Use --diagnostic-full-native-db only for bounded support runs.");
        }
        if (nativeRecordLimit > 0) log.info("Native parser record limit enabled: max_native_records=" + std::to_string(nativeRecordLimit));
        else log.info("Native parser record limit disabled: max_native_records=0 (unlimited)");
        if (opt.maxNativeBlocks > 0) log.info("Native parser metadata block limit enabled: max_native_blocks=" + std::to_string(opt.maxNativeBlocks));
        else log.info("Native parser metadata block limit disabled: max_native_blocks=0 (unlimited per store)");
        if (opt.dbSizeGuardrailBytes > 0) log.info("SQLite DB/WAL size guardrail enabled: " + std::to_string(static_cast<unsigned long long>(opt.dbSizeGuardrailBytes)) + " bytes");
        else log.warn("SQLite DB/WAL size guardrail is disabled by user option.");
        if (opt.experimentalFullNativeValues) log.warn("Experimental full native metadata value parsing is enabled. This may be unstable on some store.db files.");
        else if (opt.decodeCoreNativeValues || diagnosticsMode) log.info("Core native metadata field decoding is enabled for high-value Spotlight fields and diagnostic probes.");
        else log.warn("Native parser is running in stable header-only mode. Use --decode-core-native-values to test safe native string/path probe decoding.");
        if (cancelRequested()) return returnCancelled("before_native_parse_call");
        appendRunStatus(caseDir, "native_parse_call", decodeMode == NativeDecodeMode::FullValues ? "FullValues" : (decodeMode == NativeDecodeMode::CoreFields ? "CoreFields" : "HeaderOnly"));
        appendRunStatus(caseDir, "native_parse_bulk_pragmas_apply", "regenerable Store-V2 parse inserts use temporary bulk SQLite settings");
        db.exec("PRAGMA synchronous = OFF; PRAGMA journal_mode = MEMORY; PRAGMA temp_store = MEMORY; PRAGMA cache_size = -65536;");
        NativeStoreDbParseCounts parseCounts;
        try {
            parseCounts = parser.parseStores(parseStores, parseSource, db, log);
        } catch (...) {
            try { db.exec("PRAGMA synchronous = NORMAL; PRAGMA journal_mode = WAL;"); } catch (...) {}
            appendRunStatus(caseDir, "native_parse_bulk_pragmas_restore_after_error", "restored WAL/NORMAL settings after parser exception");
            throw;
        }
        db.exec("PRAGMA synchronous = NORMAL; PRAGMA journal_mode = WAL;");
        appendRunStatus(caseDir, "native_parse_bulk_pragmas_restore", "restored WAL/NORMAL settings after native parser");
        if (cancelRequested()) return returnCancelled("after_native_parse_call");
        result.nativeDecodeMode = (decodeMode == NativeDecodeMode::FullValues ? "FullValues" : (decodeMode == NativeDecodeMode::CoreFields ? "CoreFields" : "HeaderOnly"));
        result.rawRecordCount = parseCounts.rawRecords;
        result.rawKeyValueCount = parseCounts.rawKeyValues;
        result.rawDateCandidateCount = parseCounts.rawDateCandidates;
        appendRunStatus(caseDir, "native_parse_complete", "raw_records=" + std::to_string(parseCounts.rawRecords) + " raw_key_values=" + std::to_string(parseCounts.rawKeyValues) + " raw_date_candidates=" + std::to_string(parseCounts.rawDateCandidates));
        if (decodeMode == NativeDecodeMode::HeaderOnly) {
            appendRunStatus(caseDir, "validation_warning_header_only_metadata_skipped", "metadata key/value fields were intentionally skipped; path/name/usage/wherefroms views are not fully validated");
            log.warn("Header-only native mode completed. Metadata key/value fields were intentionally skipped, so filename/path/usage/WhereFroms enrichment and parent-inode path reconstruction may be limited. Use --experimental-full-native-values for full validation after this stable run succeeds.");
        }
        if (parseCounts.rawRecords == 0) {
            log.warn("Native parser completed but produced zero raw_records. Review raw_failures in the SQLite case database and processing log.");
        }
        if (zipInputSource && profile == SourceProfileKind::IOS && !opt.materializeIosFfsInventory && !opt.reuseIosCache.empty()) {
            const std::size_t referencedHits = EvidenceIntake::importReferencedIosPathLookupFromReuseCache(db, caseDir, source.sourceId, log, opt.reuseIosCache,
                        [](const fs::path& statusCaseDir, const std::string& stage, const std::string& message) {
                            appendRunStatus(statusCaseDir, stage, message);
                        });
            result.messages.push_back("referenced_ios_ffs_lookup_hits=" + std::to_string(referencedHits));
        }

        if (cancelRequested()) return returnCancelled("before_enrichment");
        appendRunStatus(caseDir, "enrichment_start");
        SqliteEnrichment enrichment;
        EvidenceSource enrichmentSource = source;
        if (!enrichmentSource.evidenceRoot.empty()) {
            log.info("Active filesystem evidence-root comparison is tabled in v0.6.4. EvidenceRoot was supplied but will not be used for live/missing or deleted/orphaned classification.");
            enrichmentSource.evidenceRoot.clear();
        }
        const auto counts = enrichment.run(db, enrichmentSource, log);
        appendRunStatus(caseDir, "enrichment_complete", "artifacts=" + std::to_string(counts.artifacts) + " timeline=" + std::to_string(counts.timeline) + " usage=" + std::to_string(counts.usage));
        purgeOrphanSourceRows(db, caseDir, log);
        appendRunStatus(caseDir, "export_start");
        SqliteExporter exporter;
        exporter.exportReviewPackage(db, caseDir / "exports", log, opt.exportProfile);
        writeUiAndIosPlanningFiles(caseDir);
        result.artifactCount = counts.artifacts;
        result.usageCount = counts.usage;
        result.timelineCount = counts.timeline;
        result.orphanCandidateCount = counts.orphanCandidates;
        store.writeSummary(result);
        db.close();
        try {
            std::error_code copyEc;
            fs::copy_file(caseDir / "VestigantSpotlight.case.sqlite", caseDir / "spotlight_case.db", fs::copy_options::overwrite_existing, copyEc);
            if (copyEc) log.warn("Unable to create spotlight_case.db convenience copy: " + copyEc.message());
            else log.info("SQLite convenience copy written: " + pathString(caseDir / "spotlight_case.db"));
        } catch (const std::exception& copyEx) {
            log.warn(std::string("Unable to create spotlight_case.db convenience copy: ") + copyEx.what());
        }
        createUploadBundle(caseDir);
        appendRunStatus(caseDir, "complete_success");
        refreshUploadRunDiagnostics(caseDir);
    } catch (const std::exception& ex) {
        result.exitCode = 1;
        log.error(ex.what());
        appendRunStatus(caseDir, "failed_exception", ex.what());
        createUploadBundle(caseDir);
        appendRunStatus(caseDir, "failed_exception", ex.what());
        refreshUploadRunDiagnostics(caseDir);
    } catch (...) {
        result.exitCode = 1;
        log.error("Unknown non-standard exception");
        appendRunStatus(caseDir, "failed_unknown_exception");
        createUploadBundle(caseDir);
        appendRunStatus(caseDir, "failed_unknown_exception");
        refreshUploadRunDiagnostics(caseDir);
    }
    result.messages = log.messages();
    return result;
}
} // namespace vestigant::spotlight
