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
#include "parsers/aff4_probe_worker.h"
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
#include <unordered_map>
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
#include <chrono>
#include <iterator>
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
    if (stage == "native_kv_persistence_macos_storev2") return 36;
    if (stage == "native_kv_persistence_ios_corespotlight_compact") return 36;
    if (stage == "native_kv_persistence_auto_path_sensitive") return 36;
    if (stage == "native_parse_configuration") return 36;
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
    if (stage.rfind("export_finalization_", 0) == 0) return 96;
    if (stage.rfind("export_case_summary_", 0) == 0) return 96;
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
        row("exports/upload_samples/ios_coreduet_interactionc_database_status_sample.csv", "CoreDuet People/interactionC source and parse readiness sample.");
        row("exports/upload_samples/ios_coreduet_interactionc_summary_sample.csv", "CoreDuet People/interactionC summary sample.");
        row("exports/upload_samples/ios_coreduet_interactionc_events_sample.csv", "Bounded CoreDuet People/interactionC event review sample.");
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
            out << "- AFF4 and raw flat-image inputs are identified and registered in source-probe/readiness output. General full-container filesystem extraction is still staged, but bounded AFF4/APFS Spotlight Store-V2 copy-out/staging/parsing validation is active for supported guarded layouts.\n";
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
            "case_info.json", "case_summary.json", "case_summary.csv", "three_database_layout_readiness.csv",
            "SOURCE_INTAKE_PLAN.md", "AFF4_APFS_READER_PLAN.md", "AFF4_APFS_V1_DIAGNOSTIC_RERUN_PLAN.md", "aff4_apfs_v1_diagnostic_checklist.csv", "aff4_apfs_v1_diagnostic_plan_summary.json", "AFF4_STREAM_SELECTION_PLAN.md", "AFF4_CPP_LITE_RANDOM_ACCESS_PLAN.md", "aff4_cpp_lite_reader_readiness.csv", "aff4_cpp_lite_integration_readiness.csv", "aff4_cpp_lite_dynamic_load_probe.csv", "aff4_virtual_apfs_probe.csv", "aff4_virtual_apfs_probe_summary.json", "AFF4_VIRTUAL_APFS_PROBE.md", "aff4_apfs_container_superblock.csv", "aff4_apfs_container_superblock_summary.json", "aff4_apfs_checkpoint_descriptor_scan.csv", "AFF4_APFS_CONTAINER_VIEW.md", "aff4_apfs_checkpoint_map.csv", "aff4_apfs_checkpoint_mapped_object_probe.csv", "aff4_apfs_checkpoint_map_summary.json", "AFF4_APFS_CHECKPOINT_MAP_PROBE.md", "aff4_apfs_object_id_probe.csv", "aff4_apfs_btree_node_probe.csv", "aff4_apfs_omap_phys_probe.csv", "aff4_apfs_omap_btree_root_probe.csv", "aff4_apfs_omap_lookup_probe.csv", "aff4_apfs_omap_btree_toc_probe.csv", "aff4_apfs_omap_leaf_kv_decode.csv", "aff4_apfs_omap_leaf_lookup_results.csv", "aff4_apfs_resolved_volume_superblocks.csv", "aff4_apfs_resolved_volume_superblocks_summary.json", "AFF4_APFS_RESOLVED_VOLUME_SUPERBLOCKS.md", "aff4_apfs_volume_omap_probe.csv", "AFF4_APFS_VOLUME_OMAP_PROBE.md", "aff4_apfs_volume_root_tree_lookup.csv", "aff4_apfs_volume_root_tree_lookup_summary.json", "AFF4_APFS_VOLUME_ROOT_TREE_LOOKUP.md", "aff4_apfs_root_tree_node_probe.csv", "aff4_apfs_root_tree_record_sample.csv", "aff4_apfs_spotlight_target_scan.csv", "aff4_apfs_spotlight_name_scan_sample.csv", "aff4_apfs_spotlight_copy_attempt.csv", "aff4_apfs_logical_directory_walk.csv", "aff4_apfs_logical_directory_walk_summary.json", "aff4_apfs_spotlight_xattr_probe.csv", "aff4_apfs_spotlight_xattr_probe_summary.json", "AFF4_APFS_SPOTLIGHT_XATTR_PROBE.md", "aff4_apfs_spotlight_file_extent_probe.csv", "aff4_apfs_spotlight_file_extent_probe_summary.json", "AFF4_APFS_SPOTLIGHT_FILE_EXTENT_PROBE.md", "aff4_apfs_spotlight_inode_probe.csv", "aff4_apfs_spotlight_inode_probe_summary.json", "AFF4_APFS_SPOTLIGHT_INODE_PROBE.md", "aff4_apfs_spotlight_target_scan_summary.json", "AFF4_APFS_SPOTLIGHT_TARGET_SCAN.md", "aff4_apfs_root_tree_node_probe_summary.json", "AFF4_APFS_ROOT_TREE_NODE_PROBE.md", "aff4_apfs_omap_probe_summary.json", "AFF4_APFS_OMAP_TOC_PROBE.md", "AFF4_APFS_OMAP_PROBE.md", "aff4_apfs_object_resolution_probe_summary.json", "AFF4_APFS_OBJECT_RESOLUTION_PROBE.md", "AFF4_CPP_LITE_DYNAMIC_LOAD_PROBE.md", "aff4_stream_inventory.csv", "aff4_zip_probe_summary.json", "aff4_zip_central_directory.csv", "AFF4_ZIP_SINGLE_FILE_PROBE.md", "aff4_apfs_exact_file_signature_scan.csv", "aff4_apfs_exact_file_signature_scan_summary.json", "AFF4_APFS_EXACT_FILE_SIGNATURE_SCAN.md", "evidence_source_readiness.csv", "reader_tool_readiness.csv", "source_probe_signatures.csv", "source_partition_probe.csv", "source_probe_summary.json", "image_inventory_readiness.csv", "active_file_comparison_readiness.csv", "image_file_inventory.csv", "aff4_apfs_unresolved_spotlight_object_resolution_probe.csv", "aff4_apfs_unresolved_spotlight_object_resolution_probe_summary.json", "AFF4_APFS_UNRESOLVED_SPOTLIGHT_OBJECT_RESOLUTION_PROBE.md", "aff4_apfs_staged_storev2_unresolved_after_resolution_sample.csv", "aff4_apfs_directory_record_name_index_sample.csv", "aff4_apfs_directory_record_name_index_summary.json", "AFF4_APFS_DIRECTORY_RECORD_NAME_INDEX.md", "aff4_apfs_spotlight_cache_text_sample.csv", "aff4_apfs_spotlight_cache_text_summary.json", "AFF4_APFS_SPOTLIGHT_CACHE_TEXT.md", "spotlight_external_volume_candidate_summary.csv", "spotlight_external_volume_evidence_review.csv", "spotlight_external_volume_raw_value_hits.csv", "spotlight_external_volume_cache_text_hits.csv", "spotlight_external_volume_dictionary_hits.csv", "spotlight_external_volume_volfs_hits.csv",
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
        const fs::path samplesDir = caseDir / "exports" / "upload_samples";
        std::error_code samplesEc;
        if (fs::exists(samplesDir, samplesEc) && fs::is_directory(samplesDir, samplesEc)) {
            for (const auto& entry : fs::directory_iterator(samplesDir, samplesEc)) {
                if (samplesEc) {
                    manifest << "ERROR," << pathString(samplesDir) << "," << pathString(uploadDir / "exports" / "upload_samples") << ",directory iterator failed\n";
                    break;
                }
                std::error_code fileEc;
                if (entry.is_regular_file(fileEc)) {
                    const fs::path rel = fs::path("exports") / "upload_samples" / entry.path().filename();
                    copyThinUploadExportCsvIfAllowed(entry.path(), uploadDir / "exports" / "upload_samples" / entry.path().filename(), rel, manifest);
                }
            }
        }
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
    if (type == "AFF4_CONTAINER") return "IMPLEMENTED_AFF4_APFS_SPOTLIGHT_READER";
    if (type == "RAW_FLAT_IMAGE") return "IMPLEMENTED_RAW_IMAGE_HEADER_PARTITION_PROBE_ONLY";
    if (type == "FOLDER_OR_EXTRACTED_FILESYSTEM_ROOT") return "NOT_REQUIRED_FOLDER_SOURCE";
    return "NOT_SUPPORTED";
}

std::string sourceTypeFilesystemStatus(const std::string& type) {
    if (type == "ZIP_SPOTLIGHT_OR_FILESYSTEM_CONTAINER") return "ZIP_EXTRACTED_THEN_SPOTLIGHT_DISCOVERY";
    if (type == "AFF4_CONTAINER") return "IMPLEMENTED_AFF4_APFS_SPOTLIGHT_ENUMERATION";
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

fs::path executableDirectoryNoThrow() {
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (n == 0 || n >= buffer.size()) return {};
    std::error_code ec;
    fs::path exePath(buffer.data());
    fs::path dir = exePath.parent_path();
    if (dir.empty() || !fs::exists(dir, ec)) return {};
    return dir;
#else
    return {};
#endif
}

void appendPortableReleaseResourceCandidates(std::vector<fs::path>& out, const std::vector<std::string>& names) {
    std::error_code ec;
    const fs::path exeDir = executableDirectoryNoThrow();
    if (!exeDir.empty()) {
        appendReaderToolRootCandidates(out, exeDir / "resources" / "reader_tools", names);
        appendReaderToolRootCandidates(out, exeDir / "resources" / "aff4_cpp_lite", names);
        appendReaderToolRootCandidates(out, exeDir / "resources" / "apfs_tools", names);
        appendReaderToolRootCandidates(out, exeDir / "reader_tools", names);
    }
    const fs::path cwd = fs::current_path(ec);
    if (!ec && !cwd.empty()) {
        appendReaderToolRootCandidates(out, cwd / "resources" / "reader_tools", names);
        appendReaderToolRootCandidates(out, cwd / "resources" / "aff4_cpp_lite", names);
        appendReaderToolRootCandidates(out, cwd / "resources" / "apfs_tools", names);
        appendReaderToolRootCandidates(out, cwd / "reader_tools", names);
    }
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
    appendPortableReleaseResourceCandidates(candidates, names);
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
    if (isAff4SourcePath(input)) return "AFF4_APFS_FILE_INVENTORY_AND_SPOTLIGHT_COMPARISON_AVAILABLE_WHEN_SOURCE_PROBE_COMPLETES";
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
                << csvEscape(spotlightHintCount > 0 ? "SPOTLIGHT_HINTS_FOUND_OR_EXTRACTED_FROM_IMAGE" : "NO_IMAGE_BACKED_SPOTLIGHT_LOCATION_REPORTED") << ','
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
                << csvEscape("Active file comparison is reported from image_file_inventory/comparison.sqlite when APFS/HFS filesystem rows are materialized; readiness-only rows remain diagnostic placeholders before source-probe completion.") << "\n";
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
            inv.bind(i++, spotlightHintCount > 0 ? std::string("SPOTLIGHT_HINTS_FOUND_OR_EXTRACTED_FROM_IMAGE") : std::string("NO_IMAGE_BACKED_SPOTLIGHT_LOCATION_REPORTED"));
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
            out << "- Guarded AFF4/APFS Spotlight Store-V2 staging, copy-out, parser selection, native parsing, cache-text incorporation, APFS inventory materialization, and active-file comparison for supported APFS images.\n";
            out << "- Bounded source signature probing for ZIP/AFF4 hints, MBR/GPT, APFS NXSB, HFS/HFS+, and Spotlight/CoreSpotlight path strings.\n";
            out << "- Raw image partition-map readiness reporting for MBR/protective MBR and GPT entries, retained as secondary to AFF4/APFS work.\n- Image-file-inventory and active-file-comparison readiness artifacts for Spotlight-indexed versus APFS-present/missing comparison.\n";
            out << "- Clear status distinction between the implemented guided AFF4/APFS Spotlight path and the broader general-purpose container-reader roadmap.\n\n";
            out << "## Remaining roadmap items\n\n";
            out << "- General full-container AFF4 stream enumeration and generic filesystem extraction outside the guided AFF4/APFS Spotlight path.\n";
            out << "- Raw image filesystem extraction from detected partition entries. Partition entries are reported for readiness/provenance only.\n";
            out << "- HFS/HFS+ image filesystem enumeration remains lower priority than AFF4/APFS validation.\n";
            out << "- Broader cross-image validation of `.Spotlight-V100` / CoreSpotlight extraction beyond the current AFF4/APFS validation corpus.\n\n";
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
)PS" R"PS(# V0.9.42: The old 7z pipeline/Regex inventory parser was removed. The active 7z inventory path dumps -slt output to raw text and parses the file line-by-line without an external process pipeline.
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
  & $SevenZipPath x $ZipPath "-o$AppDbStageRoot" @dbPatterns -y 2>&1 | Out-File -FilePath $dbExtractLog -Encoding UTF8
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
)PS" R"PS(  Write-ZipInventoryProgress 'ffs_inventory_raw_ready_for_cpp' 0 ("raw=" + $rawListing)
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
  & $SevenZip x $ZipPath "-o$StageRoot" @patterns -y 2>&1 | Out-File -FilePath $sevenZipExtractLog -Encoding UTF8
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


std::string sqlQuoteLiteral(const std::string& s) {
    std::string out = "'";
    for (char ch : s) {
        if (ch == '\'') out += "''";
        else out.push_back(ch);
    }
    out.push_back('\'');
    return out;
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

struct SevenZipEntryRecord {
    std::string path;
    std::string size;
    std::string modified;
    std::string folder;

    void clear() {
        path.clear();
        size.clear();
        modified.clear();
        folder.clear();
    }
};

bool isBlankSevenZipLine(const std::string& line) {
    return line.find_first_not_of(" \t\r\n") == std::string::npos;
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

    std::vector<char> ffsOutBuffer(1024 * 1024);
    std::vector<char> dbOutBuffer(256 * 1024);
    std::ofstream ffsOut;
    std::ofstream dbOut;
    ffsOut.rdbuf()->pubsetbuf(ffsOutBuffer.data(), static_cast<std::streamsize>(ffsOutBuffer.size()));
    dbOut.rdbuf()->pubsetbuf(dbOutBuffer.data(), static_cast<std::streamsize>(dbOutBuffer.size()));
    ffsOut.open(ffsInventoryPath, std::ios::binary);
    dbOut.open(dbInventoryPath, std::ios::binary);
    if (!ffsOut || !dbOut) throw std::runtime_error("Unable to write native C++ iOS ZIP inventory CSVs");
    ffsOut << "normalized_path,original_zip_entry,file_name,extension,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,is_directory,sha256_status,inventory_notes\n";
    dbOut << "normalized_path,original_zip_entry,database_name,database_category,app_hint,protection_class_hint,size_bytes,zip_modified_utc,parse_status,record_inventory_status,notes,extracted_path\n";

    SevenZipEntryRecord rec;
    const std::string zipPathNorm = toLower(pathString(zipPath));
    auto flush = [&]() {
        if (trim(rec.path).empty()) { rec.clear(); return; }
        const std::string& fullName = rec.path;
        const std::string fullLow = toLower(fullName);
        if ((fullLow.find(":/") != std::string::npos || fullLow.find(":\\") != std::string::npos) && endsWithCpp(fullLow, ".zip")) { rec.clear(); return; }
        if (!zipPathNorm.empty() && fullLow == zipPathNorm) { rec.clear(); return; }
        const std::string norm = normalizeIosPathFromZipEntryCpp(fullName);
        const std::string name = basenameFromZipEntryCpp(fullName);
        const std::string ext = extensionFromNameCpp(name);
        const std::string folder = toLower(trim(rec.folder));
        const bool isDir = (folder == "+" || folder == "true" || folder == "1" || folder == "yes");
        const std::string prot = protectionClassHintCpp(norm);
        const std::string app = appContainerHintCpp(fullName);
        const std::string domain = domainHintCpp(norm);
        const std::string note = "native_cpp_7z_slt_inventory_parser_fast_record_state_v1_6_64";
        ffsOut << csvEscape(norm) << ',' << csvEscape(fullName) << ',' << csvEscape(name) << ',' << csvEscape(ext) << ','
               << csvEscape(rec.size) << ',' << csvEscape(rec.modified) << ',' << csvEscape(prot) << ',' << csvEscape(app) << ','
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
                  << csvEscape(cat.second) << ',' << csvEscape(prot) << ',' << csvEscape(rec.size) << ',' << csvEscape(rec.modified) << ','
                  << csvEscape(parseStatus) << ',' << csvEscape(recordStatus) << ','
                  << csvEscape("native_cpp_7z_slt_inventory_parser_database_like_only_fast_record_state_v1_6_44") << ','
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
        if (isBlankSevenZipLine(line)) { flush(); continue; }
        constexpr const char* sep = " = ";
        constexpr std::size_t sepLen = 3;
        const auto pos = line.find(sep);
        if (pos == std::string::npos) continue;
        const std::string key = trim(line.substr(0, pos));
        const std::string value = line.substr(pos + sepLen);
        if (key == "Path") rec.path = value;
        else if (key == "Size") rec.size = value;
        else if (key == "Modified") rec.modified = value;
        else if (key == "Folder") rec.folder = value;
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
        appendRunProgress(caseDir, 12, "stage_zip_focused_extract_start", "script=" + pathString(scriptPath) + " stage_root=" + pathString(stageRoot));
        const auto focusedExtractStart = std::chrono::steady_clock::now();
#if defined(_WIN32)
        const int rc = runPowerShellFileNoWindowRedirected(scriptPath, logPath);
#else
        const int rc = runShellCommandNoWindow(cmd);
#endif
        const auto focusedExtractSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - focusedExtractStart).count();
        appendRunProgress(caseDir, 12, "stage_zip_focused_extract_complete", "exit_code=" + std::to_string(rc) + " elapsed_seconds=" + std::to_string(focusedExtractSeconds) + " log=" + pathString(logPath));
        IosZipInventoryParseResult cppInventoryParse;
        try {
            appendRunProgress(caseDir, 12, "ios_ffs_inventory_cpp_parser_start", "raw_7z_slt=" + pathString(caseDir / "logs" / "ios_ffs_7z_inventory_raw_slt.txt"));
            const auto cppInventoryStart = std::chrono::steady_clock::now();
            cppInventoryParse = parseIosSevenZipRawInventoryToCsv(caseDir, zipPath, log);
            const auto cppInventorySeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - cppInventoryStart).count();
            appendRunProgress(caseDir, 12, "ios_ffs_inventory_cpp_parser_timing_complete", "elapsed_seconds=" + std::to_string(cppInventorySeconds) + " files=" + std::to_string(cppInventoryParse.ffsRows) + " app_databases=" + std::to_string(cppInventoryParse.appDbRows));
            if (cppInventoryParse.status == "NATIVE_CPP_7Z_RAW_INVENTORY_PARSED") {
                appendRunStatus(caseDir, "ios_ffs_inventory_native_cpp_ready", "files=" + std::to_string(cppInventoryParse.ffsRows) + " app_databases=" + std::to_string(cppInventoryParse.appDbRows));
            }
        } catch (const std::exception& ex) {
            appendRunStatus(caseDir, "ios_ffs_inventory_native_cpp_warning", ex.what());
            appendRunProgress(caseDir, 12, "ios_ffs_inventory_cpp_parser_failed", ex.what());
            log.warn(std::string("Native C++ iOS ZIP inventory parser did not complete; using script inventory output if present: ") + ex.what());
        } catch (...) {
            appendRunStatus(caseDir, "ios_ffs_inventory_native_cpp_warning", "unknown error");
            appendRunProgress(caseDir, 12, "ios_ffs_inventory_cpp_parser_failed", "unknown error");
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
    (void)profile;
    (void)iosFocusedUsed;
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
    bool transactionOpen = false;
    try {
        db.begin();
        transactionOpen = true;
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to begin orphan source-row purge transaction; continuing with SQLite default behavior: ") + ex.what());
    }
    try {
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
        if (transactionOpen) {
            db.commit();
            transactionOpen = false;
        }
    } catch (...) {
        if (transactionOpen) db.rollbackNoThrow();
        throw;
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

bool isHexSha256(const std::string& value) {
    if (value.size() != 64U) return false;
    for (const char c : value) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!std::isxdigit(u)) return false;
    }
    return true;
}

std::string normalizeExternalSha256(std::string value) {
    value = trim(value);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void registerOriginalContainerSource(CaseDatabase& db,
                                     const EvidenceSource& source,
                                     const fs::path& originalContainer,
                                     const fs::path& stagedWorkingRoot,
                                     const std::string& notes,
                                     Logger& log,
                                     bool skipHash,
                                     const fs::path& caseDirForStatus,
                                     const std::string& externalSourceSha256 = {},
                                     const std::string& externalSourceHashNote = {}) {
    std::uintmax_t sizeBytes = 0;
    std::string sha;
    const std::string externalSha = normalizeExternalSha256(externalSourceSha256);
    if (!externalSha.empty() && !isHexSha256(externalSha)) {
        throw std::runtime_error("--external-source-sha256 must be exactly 64 hexadecimal characters");
    }
    const bool useExternalHashOnly = !externalSha.empty() && skipHash;
    bool externalHashMatched = false;
    bool externalHashMismatch = false;
    try {
        std::error_code ec;
        if (fs::exists(originalContainer, ec) && fs::is_regular_file(originalContainer, ec)) {
            sizeBytes = fs::file_size(originalContainer, ec);
            if (ec) sizeBytes = 0;
            if (useExternalHashOnly) {
                sha = externalSha;
                appendRunStatus(caseDirForStatus, "original_container_external_hash_recorded", "sha256_prefix=" + (sha.size() >= 12 ? sha.substr(0, 12) : sha));
                log.info("Original container SHA256 recorded from external evidence. sha256_prefix=" + (sha.size() >= 12 ? sha.substr(0, 12) : sha));
            } else if (skipHash) {
                log.warn("Original container SHA256 deferred for this source-probe/development run: " + pathString(originalContainer));
            } else {
                appendRunStatus(caseDirForStatus, "original_container_hash_start", "size_bytes=" + std::to_string(static_cast<unsigned long long>(sizeBytes)) + " path=" + pathString(originalContainer));
                log.info("Original container SHA256 hashing started. size_bytes=" + std::to_string(static_cast<unsigned long long>(sizeBytes)) + " path=" + pathString(originalContainer));
                std::uintmax_t lastReportedBytes = 0;
                int lastReportedPercent = -1;
                const std::uintmax_t minReportStep = 5ULL * 1024ULL * 1024ULL * 1024ULL;
                sha = sha256FileWithProgress(originalContainer, [&](std::uintmax_t bytesRead) {
                    int percent = -1;
                    if (sizeBytes > 0) percent = static_cast<int>((bytesRead * 100ULL) / sizeBytes);
                    const bool reportByBytes = (lastReportedBytes == 0 || bytesRead >= lastReportedBytes + minReportStep || bytesRead >= sizeBytes);
                    const bool reportByPercent = (percent >= 0 && (lastReportedPercent < 0 || percent >= lastReportedPercent + 5 || percent >= 100));
                    if (!reportByBytes && !reportByPercent) return;
                    lastReportedBytes = bytesRead;
                    if (percent >= 0) lastReportedPercent = percent;
                    appendRunStatus(caseDirForStatus,
                                    "original_container_hash_progress",
                                    "bytes_read=" + std::to_string(static_cast<unsigned long long>(bytesRead)) +
                                    " total_bytes=" + std::to_string(static_cast<unsigned long long>(sizeBytes)) +
                                    " percent=" + (percent >= 0 ? std::to_string(percent) : std::string("unknown")));
                });
                appendRunStatus(caseDirForStatus, "original_container_hash_complete", "size_bytes=" + std::to_string(static_cast<unsigned long long>(sizeBytes)) + " sha256_prefix=" + (sha.size() >= 12 ? sha.substr(0, 12) : sha));
                log.info("Original container SHA256 hashing completed. size_bytes=" + std::to_string(static_cast<unsigned long long>(sizeBytes)) + " sha256_prefix=" + (sha.size() >= 12 ? sha.substr(0, 12) : sha));
                if (!externalSha.empty()) {
                    externalHashMatched = (sha == externalSha);
                    externalHashMismatch = !externalHashMatched;
                    appendRunStatus(caseDirForStatus,
                                    externalHashMatched ? "original_container_external_hash_match" : "original_container_external_hash_mismatch",
                                    "computed_prefix=" + (sha.size() >= 12 ? sha.substr(0, 12) : sha) + " external_prefix=" + externalSha.substr(0, 12));
                    if (externalHashMismatch) log.warn("Computed source hash does not match externally supplied hash; review source integrity before relying on this run.");
                }
            }
        }
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to hash/register original container source: ") + ex.what());
    }

    const std::string format = containerFormatForPath(originalContainer);
    const std::string role = containerRoleForPath(originalContainer);
    std::string baseNote = notes.empty()
        ? "Original evidence source is already a fixed container/image. No new evidentiary archive was created; original source was hashed or registered."
        : notes;
    if (!externalSha.empty()) {
        baseNote += " External source SHA256 supplied by operator.";
        if (!externalSourceHashNote.empty()) baseNote += " External hash note: " + externalSourceHashNote;
    }
    const std::string integrityStatus = externalHashMismatch ? "HASH_MISMATCH_REVIEW" :
        (useExternalHashOnly ? "EXTERNAL_HASH_RECORDED" :
         (!externalSha.empty() && externalHashMatched ? "HASHED_EXTERNAL_MATCH" :
          (skipHash ? "HASH_DEFERRED" : (sha.empty() ? "HASH_NOT_AVAILABLE" : "HASHED"))));
    const std::string integrityCheckResult = externalHashMismatch ? "HASH_MISMATCH_REVIEW" :
        (useExternalHashOnly ? "EXTERNAL_HASH_RECORDED" :
         (!externalSha.empty() && externalHashMatched ? "HASHED_EXTERNAL_MATCH" :
          (skipHash ? "DEFERRED_BY_OPERATOR" : (sha.empty() ? "NOT_RUN" : "HASHED"))));

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
        setStmt.bind(i++, integrityStatus);
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
        checkStmt.bind(6, integrityCheckResult);
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


struct Aff4ApfsStagedStoreV2ParserProbeResult {
    std::vector<StoreInfo> candidates;
    std::vector<StoreInfo> selected;
    NativeStoreDbParseCounts parseCounts;
    std::string status = "NOT_RUN";
    std::string decodeModeName = "NOT_RUN";
};

Aff4ApfsStagedStoreV2ParserProbeResult runAff4ApfsStagedStoreV2ParserProbe(const fs::path& caseDir,
                                                                            const EvidenceSource& source,
                                                                            const RunOptions& opt,
                                                                            CaseDatabase& db,
                                                                            Logger& log) {
    const fs::path stagedRoot = caseDir / "ExtractedSpotlight" / "StagedStoreV2";
    const fs::path csvPath = caseDir / "aff4_apfs_staged_storev2_parser_probe.csv";
    const fs::path coverageCsvPath = caseDir / "aff4_apfs_staged_storev2_parse_selection_coverage.csv";
    const fs::path coverageJsonPath = caseDir / "aff4_apfs_staged_storev2_parse_selection_coverage_summary.json";
    const fs::path jsonPath = caseDir / "aff4_apfs_staged_storev2_parser_probe_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_STAGED_STOREV2_PARSER_PROBE.md";
    Aff4ApfsStagedStoreV2ParserProbeResult result;
    auto& candidates = result.candidates;
    auto& selected = result.selected;
    auto& parseCounts = result.parseCounts;
    std::string runStatus = "NOT_RUN";
    std::string notes;
    std::size_t maxRecordsUsed = opt.maxNativeRecordsExplicit ? opt.maxNativeRecords : 25000U;
    std::size_t maxBlocksUsed = opt.maxNativeBlocks;
    const bool fullNativeValuesRequested = opt.experimentalFullNativeValues || opt.diagnosticFullNativeDb || opt.pressureTestMode;
    // V1.6.119: pressure/full-native AFF4/APFS source-probe runs must use
    // FullValues even when GUI also enables core/native metadata. The prior
    // conditional let GUI runs fall back to CoreFields because GUI sets
    // decodeCoreNativeValues=true by default, producing far fewer raw key/value
    // and date-candidate rows than the validated PowerShell thin workflow.
    const NativeDecodeMode stagedDecodeMode = fullNativeValuesRequested ? NativeDecodeMode::FullValues : NativeDecodeMode::CoreFields;
    result.decodeModeName = (stagedDecodeMode == NativeDecodeMode::FullValues) ? "FullValues" : "CoreFields";

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
                appendRunStatus(caseDir, "aff4_apfs_staged_storev2_parse_start", "selected_stores=" + std::to_string(selected.size()) + " decode_mode=" + result.decodeModeName);
                NativeStoreDbParser parser(stagedDecodeMode, maxRecordsUsed, maxBlocksUsed);
                parser.setProgressPath(caseDir / "logs" / "aff4_apfs_staged_storev2_parse_progress.tsv");
                parser.setDbSizeGuardrailBytes(opt.dbSizeGuardrailBytes);
                if (opt.dbSizeGuardrailBytes == 0) {
                    appendRunStatus(caseDir, "aff4_apfs_staged_storev2_db_guardrail_disabled", "pressure/full-validation run disabled SQLite DB/WAL size guardrail for staged Store-V2 parse");
                    log.warn("AFF4/APFS staged Store-V2 parser SQLite DB/WAL size guardrail is disabled for this run.");
                } else {
                    appendRunStatus(caseDir, "aff4_apfs_staged_storev2_db_guardrail_enabled", "guardrail_bytes=" + std::to_string(static_cast<unsigned long long>(opt.dbSizeGuardrailBytes)));
                }
                parseCounts = parser.parseStores(selected, stagedSource, db, log);
                appendRunStatus(caseDir, "aff4_apfs_staged_storev2_parse_complete", "decode_mode=" + result.decodeModeName + " raw_records=" + std::to_string(parseCounts.rawRecords) + " raw_key_values=" + std::to_string(parseCounts.rawKeyValues) + " raw_date_candidates=" + std::to_string(parseCounts.rawDateCandidates) + " fallback_header_only_items=" + std::to_string(parseCounts.fallbackHeaderOnlyItems));
                runStatus = "PARSE_PROBE_COMPLETED";
                notes = "Native Store-V2 parser was run in " + result.decodeModeName + " mode against APFS-extracted staged Store-V2 candidates. When --max-native-records is explicitly 0, record enumeration is uncapped; otherwise AFF4 validation defaults to a 25,000-record cap. CoreFields is the default to avoid full-value key/value blowups; pressure-test mode and --experimental-full-native-values use the heavier FullValues support path so named fields such as kMDItemUsedDates/kMDItemLastUsedDate can be tested when present.";
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
        std::set<std::string> selectedPaths;
        for (const auto& st : selected) selectedPaths.insert(pathString(st.storePath));
        std::map<std::string, std::vector<StoreInfo>> coverageGroups;
        for (const auto& st : candidates) coverageGroups[storeGroupKey(st)].push_back(st);
        std::ofstream out(coverageCsvPath, std::ios::binary);
        out << "source_id,input_path,input_type,group_key,group_label,candidate_count,valid_candidate_count,invalid_candidate_count,selected_candidate_count,coverage_status,selection_policy,selected_relative_paths,valid_alternate_relative_paths,invalid_relative_paths,validation_errors,notes\n";
        std::size_t groupsWithValid = 0;
        std::size_t groupsSelected = 0;
        std::size_t groupsInvalidOnly = 0;
        for (const auto& kv : coverageGroups) {
            const std::string& key = kv.first;
            const auto& group = kv.second;
            std::size_t validCount = 0;
            std::size_t invalidCount = 0;
            std::size_t selectedCount = 0;
            std::string label;
            std::string selectedRel;
            std::string validAltRel;
            std::string invalidRel;
            std::string errors;
            for (const auto& st : group) {
                if (label.empty()) label = st.storeGuid;
                const bool isSelected = selectedPaths.find(pathString(st.storePath)) != selectedPaths.end();
                if (st.isValid) ++validCount; else ++invalidCount;
                if (isSelected) ++selectedCount;
                auto appendList = [](std::string& dst, const std::string& value) { if (!dst.empty()) dst += "; "; dst += value; };
                const std::string rel = st.relativePath.empty() ? pathString(st.storePath.filename()) : st.relativePath;
                if (isSelected) appendList(selectedRel, rel);
                else if (st.isValid) appendList(validAltRel, rel);
                else appendList(invalidRel, rel);
                if (!st.validationError.empty()) appendList(errors, rel + "=" + st.validationError);
            }
            std::string status;
            std::string notesForRow;
            if (selectedCount > 0) {
                ++groupsSelected;
                if (validCount > selectedCount) status = "PARSED_ONE_PRIMARY_VALID_DATABASE_ALTERNATES_PRESERVED";
                else status = "PARSED_PRIMARY_VALID_DATABASE";
                notesForRow = "One canonical valid Store-V2 database was selected for parsing from this logical group to avoid double-counting store.db/.store.db alternates.";
            } else if (validCount > 0) {
                status = "VALID_GROUP_NOT_SELECTED_REVIEW_REQUIRED";
                notesForRow = "A valid Store-V2 candidate existed but no primary database was selected; this should be reviewed.";
            } else {
                ++groupsInvalidOnly;
                status = "NOT_PARSED_NO_VALID_STORE_SIGNATURE";
                notesForRow = "Candidate files were preserved in staging/inventory, but signature validation did not identify a parseable Store-V2 database.";
            }
            if (validCount > 0) ++groupsWithValid;
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(source.inputPath)) << ',' << csvEscape(inputSourceType(source.inputPath)) << ','
                << csvEscape(key) << ',' << csvEscape(label) << ',' << group.size() << ',' << validCount << ',' << invalidCount << ',' << selectedCount << ','
                << csvEscape(status) << ',' << csvEscape("one_primary_per_logical_store_group_store_db_preferred_dot_store_preserved") << ','
                << csvEscape(selectedRel) << ',' << csvEscape(validAltRel) << ',' << csvEscape(invalidRel) << ',' << csvEscape(errors) << ',' << csvEscape(notesForRow) << "\n";
        }
        if (coverageGroups.empty()) {
            out << csvEscape(source.sourceId) << ',' << csvEscape(pathString(source.inputPath)) << ',' << csvEscape(inputSourceType(source.inputPath)) << ",,NO_CANDIDATES,0,0,0,0,NO_CANDIDATE_DATABASES,,,,,,No staged Store-V2 database candidates were discovered.\n";
        }
        std::ofstream jout(coverageJsonPath, std::ios::binary);
        jout << "{\n";
        jout << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        jout << "  \"app_version\": \"" << appVersion() << "\",\n";
        jout << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
        jout << "  \"candidate_database_groups\": " << coverageGroups.size() << ",\n";
        jout << "  \"groups_with_valid_database\": " << groupsWithValid << ",\n";
        jout << "  \"groups_selected_for_parsing\": " << groupsSelected << ",\n";
        jout << "  \"groups_invalid_only\": " << groupsInvalidOnly << ",\n";
        jout << "  \"all_valid_groups_selected\": " << ((groupsWithValid == groupsSelected) ? "true" : "false") << ",\n";
        jout << "  \"selection_policy\": \"one_primary_per_logical_store_group_store_db_preferred_dot_store_preserved\",\n";
        jout << "  \"notes\": \"All valid staged Store-V2 logical groups should have exactly one selected primary database. Valid alternates are preserved but not parsed to avoid duplicate artifacts. Invalid-only groups are retained in staging diagnostics with signature/error details.\"\n";
        jout << "}\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write aff4_apfs_staged_storev2_parse_selection_coverage outputs: ") + ex.what()); }

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
        out << "  \"native_decode_mode\": \"" << jsonEscape(result.decodeModeName) << "\",\n";
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
        out << "This probe discovers Store-V2 candidates copied out of the selected AFF4/APFS image and runs the native Store-V2 parser in " << result.decodeModeName << " mode against the staged candidate folders. It does not scan parent drives, does not mount APFS, and does not convert the AFF4 image to RAW/DD. CoreFields is the default for AFF4 validation because the uploaded V1.6.70 run showed FullValues could generate hundreds of thousands of raw_key_values rows before the 25,000-record cap; use --max-native-records 0 for uncapped CoreFields enumeration, and use --experimental-full-native-values only for targeted support runs.\n\n";
        out << "## Summary\n\n";
        out << "- Status: `" << runStatus << "`\n";
        out << "- Candidate databases: `" << candidates.size() << "`\n";
        out << "- Selected databases: `" << selected.size() << "`\n";
        out << "- Parse-selection coverage CSV: `" << pathString(coverageCsvPath.filename()) << "`\n";
        out << "- Max records used: `" << maxRecordsUsed << "`\n";
        out << "- Native decode mode: `" << result.decodeModeName << "`\n";
        out << "- Raw records: `" << parseCounts.rawRecords << "`\n";
        out << "- Raw key/value rows: `" << parseCounts.rawKeyValues << "`\n";
        out << "- Raw date candidates: `" << parseCounts.rawDateCandidates << "`\n";
        out << "- Parser failures: `" << parseCounts.failures << "`\n\n";
        out << "## Next step\n\n";
        out << "If this probe produces raw records, the next version should run enrichment/export on the APFS-staged parser results and preserve AFF4/APFS provenance fields for each staged store and parsed artifact.\n";
    } catch (const std::exception& ex) { log.warn(std::string("Unable to write AFF4_APFS_STAGED_STOREV2_PARSER_PROBE.md: ") + ex.what()); }

    result.status = runStatus;
    log.info("AFF4 APFS staged Store-V2 parser probe written: " + pathString(csvPath));
    return result;
}



long long scalarCountForSource(CaseDatabase& db, const std::string& tableName, const std::string& sourceId) {
    const std::string sql = "SELECT COUNT(*) FROM " + tableName + " WHERE source_id=?";
    auto st = db.prepare(sql);
    st.bind(1, sourceId);
    if (st.stepRow()) return st.colInt64(0);
    return 0;
}


std::string sqliteRunnerLiteral(const std::string& s) {
    std::string out = "'";
    for (char c : s) out += (c == '\'') ? "''" : std::string(1, c);
    out += "'";
    return out;
}

long long scalarCountInAttachedTable(CaseDatabase& db, const std::string& schemaName, const std::string& tableName) {
    auto st = db.prepare("SELECT COUNT(*) FROM " + schemaName + "." + tableName);
    if (st.stepRow()) return st.colInt64(0);
    return 0;
}

void detachSqliteSidecarNoThrow(CaseDatabase& db, const std::string& schemaName) {
    try { db.exec("DETACH DATABASE " + schemaName + ";"); } catch (...) {}
}

void removeSqliteSidecarFilesNoThrow(const fs::path& p) {
    try {
        std::error_code ec;
        fs::remove(p, ec);
        fs::remove(fs::path(p.string() + "-wal"), ec);
        fs::remove(fs::path(p.string() + "-shm"), ec);
        fs::remove(fs::path(p.string() + "-journal"), ec);
    } catch (...) {}
}

void writeThreeDatabaseLayoutReadiness(const fs::path& caseDir,
                                       const std::string& sourceId,
                                       const fs::path& spotlightDb,
                                       const fs::path& filesystemDb,
                                       const fs::path& comparisonDb,
                                       long long imageRows,
                                       long long comparisonRows,
                                       long long missingRows,
                                       const std::string& status,
                                       const std::string& notes) {
    try {
        std::ofstream out(caseDir / "three_database_layout_readiness.csv", std::ios::binary);
        out << "component,path,status,row_count,source_id,notes\n";
        out << "spotlight_primary," << csvEscape(pathString(spotlightDb)) << ",PRIMARY_CASE_DB," << "" << "," << csvEscape(sourceId) << "," << csvEscape("V1.6.119 keeps the current primary case SQLite as the Spotlight evidence database to avoid duplicating the multi-GB raw key/value corpus in thin pressure runs.") << "\n";
        out << "filesystem_inventory," << csvEscape(pathString(filesystemDb)) << "," << csvEscape(status) << "," << imageRows << "," << csvEscape(sourceId) << "," << csvEscape("Sidecar SQLite contains image_inventory_sources and APFS-derived image_file_inventory rows for this source.") << "\n";
        out << "comparison," << csvEscape(pathString(comparisonDb)) << "," << csvEscape(status) << "," << comparisonRows << "," << csvEscape(sourceId) << "," << csvEscape("Sidecar SQLite contains active_file_comparison_runs, artifact comparison rows, and orphan/missing-candidate lead rows.") << "\n";
        out << "comparison_missing_candidates," << csvEscape(pathString(comparisonDb)) << "," << csvEscape(status) << "," << missingRows << "," << csvEscape(sourceId) << "," << csvEscape("Missing candidates are investigative leads only, not deletion proof.") << "\n";
        out << "roadmap," << csvEscape(pathString(caseDir)) << "," << csvEscape(status) << ",0," << csvEscape(sourceId) << "," << csvEscape(notes) << "\n";
    } catch (...) {}
}

void materializeThreeDatabaseSidecars(CaseDatabase& db,
                                      const fs::path& caseDir,
                                      const EvidenceSource& source,
                                      Logger& log) {
    const fs::path filesystemDb = caseDir / "filesystem_inventory.sqlite";
    const fs::path comparisonDb = caseDir / "comparison.sqlite";
    const fs::path spotlightDb = db.path();
    const std::string sid = sqliteRunnerLiteral(source.sourceId);
    const std::string sourceIdText = source.sourceId;
    const long long imageRows = scalarCountForSource(db, "image_file_inventory", source.sourceId);
    const long long comparisonRows = scalarCountForSource(db, "artifacts", source.sourceId);
    const long long missingRows = scalarCountForSource(db, "orphaned_deleted_candidates", source.sourceId);
    appendRunStatus(caseDir, "three_database_sidecar_start", "filesystem_inventory.sqlite and comparison.sqlite materialization; primary case DB remains Spotlight evidence DB for this transition build");
    try {
        removeSqliteSidecarFilesNoThrow(filesystemDb);
        removeSqliteSidecarFilesNoThrow(comparisonDb);
        detachSqliteSidecarNoThrow(db, "filesystem_db");
        detachSqliteSidecarNoThrow(db, "comparison_db");

        db.exec("ATTACH DATABASE " + sqliteRunnerLiteral(pathString(filesystemDb)) + " AS filesystem_db;");
        db.exec("PRAGMA filesystem_db.journal_mode=DELETE;");
        db.exec("DROP TABLE IF EXISTS filesystem_db.three_database_manifest;");
        db.exec("CREATE TABLE filesystem_db.three_database_manifest(component TEXT, source_id TEXT, created_utc TEXT, row_count INTEGER, notes TEXT);");
        db.exec("DROP TABLE IF EXISTS filesystem_db.image_inventory_sources;");
        db.exec("CREATE TABLE filesystem_db.image_inventory_sources AS SELECT * FROM main.image_inventory_sources WHERE source_id=" + sid + ";");
        db.exec("DROP TABLE IF EXISTS filesystem_db.image_file_inventory;");
        db.exec("CREATE TABLE filesystem_db.image_file_inventory AS SELECT * FROM main.image_file_inventory WHERE source_id=" + sid + ";");
        db.exec("CREATE INDEX filesystem_db.idx_filesystem_inventory_source_path ON image_file_inventory(source_id, full_path);");
        db.exec("CREATE INDEX filesystem_db.idx_filesystem_inventory_source_inode ON image_file_inventory(source_id, inode_num, parent_inode_num);");
        db.exec("CREATE INDEX filesystem_db.idx_filesystem_inventory_object ON image_file_inventory(source_id, filesystem_object_id, parent_filesystem_object_id);");
        db.exec("INSERT INTO filesystem_db.three_database_manifest VALUES('filesystem_inventory'," + sid + ",datetime('now'),(SELECT COUNT(*) FROM filesystem_db.image_file_inventory),'APFS-derived filesystem inventory sidecar; source Spotlight evidence remains in the primary case DB.');");
        detachSqliteSidecarNoThrow(db, "filesystem_db");

        db.exec("ATTACH DATABASE " + sqliteRunnerLiteral(pathString(comparisonDb)) + " AS comparison_db;");
        db.exec("PRAGMA comparison_db.journal_mode=DELETE;");
        db.exec("DROP TABLE IF EXISTS comparison_db.three_database_manifest;");
        db.exec("CREATE TABLE comparison_db.three_database_manifest(component TEXT, source_id TEXT, created_utc TEXT, row_count INTEGER, notes TEXT);");
        db.exec("DROP TABLE IF EXISTS comparison_db.active_file_comparison_runs;");
        db.exec("CREATE TABLE comparison_db.active_file_comparison_runs AS SELECT * FROM main.active_file_comparison_runs WHERE source_id=" + sid + ";");
        db.exec("DROP TABLE IF EXISTS comparison_db.artifact_filesystem_comparison;");
        db.exec("CREATE TABLE comparison_db.artifact_filesystem_comparison AS SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,filesystem_lookup_path,matched_filesystem_path,existence_status,deleted_or_orphaned_candidate,orphan_reason,confidence,content_type,last_updated_utc,downloaded_date_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count FROM main.artifacts WHERE source_id=" + sid + ";");
        db.exec("DROP TABLE IF EXISTS comparison_db.orphaned_deleted_candidates;");
        db.exec("CREATE TABLE comparison_db.orphaned_deleted_candidates AS SELECT * FROM main.orphaned_deleted_candidates WHERE source_id=" + sid + ";");
        db.exec("DROP TABLE IF EXISTS comparison_db.investigator_points_of_interest;");
        db.exec("CREATE TABLE comparison_db.investigator_points_of_interest AS SELECT * FROM main.vw_investigator_points_of_interest WHERE source_id=" + sid + ";");
        db.exec("DROP TABLE IF EXISTS comparison_db.investigator_points_of_interest_summary;");
        db.exec("CREATE TABLE comparison_db.investigator_points_of_interest_summary AS SELECT poi_priority,poi_category,COUNT(*) AS lead_count,MAX(poi_score) AS max_score,ROUND(AVG(poi_score),1) AS avg_score,SUM(CASE WHEN missing_candidate_rows>0 THEN 1 ELSE 0 END) AS missing_leads,SUM(CASE WHEN usage_date_count>0 THEN 1 ELSE 0 END) AS usage_leads,SUM(CASE WHEN cache_text_rows>0 THEN 1 ELSE 0 END) AS cache_text_leads,SUM(CASE WHEN COALESCE(NULLIF(where_froms,''),'')<>'' OR downloaded_date_count>0 THEN 1 ELSE 0 END) AS where_from_leads,SUM(CASE WHEN external_volume_rows>0 THEN 1 ELSE 0 END) AS external_volume_leads,'UNVALIDATED_INVESTIGATIVE_LEAD' AS validation_status FROM main.vw_investigator_points_of_interest WHERE source_id=" + sid + " GROUP BY poi_priority,poi_category;");
        db.exec("DROP TABLE IF EXISTS comparison_db.investigator_points_of_interest_validation;");
        db.exec("CREATE TABLE comparison_db.investigator_points_of_interest_validation AS SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,best_path,poi_score,poi_priority,poi_category,validation_evidence_tables,validation_workflow,validation_status,interpretation_note FROM main.vw_investigator_points_of_interest WHERE source_id=" + sid + ";");
        appendRunStatus(caseDir, "three_database_high_priority_queue_start", "materializing high-priority validation queue sidecar");
        db.exec("DROP TABLE IF EXISTS comparison_db.investigator_high_priority_validation_queue;");
        db.exec("CREATE TABLE comparison_db.investigator_high_priority_validation_queue AS SELECT * FROM main.vw_investigator_high_priority_validation_queue WHERE source_id=" + sid + ";");
        appendRunStatus(caseDir, "three_database_high_priority_queue_complete", "rows=" + std::to_string(scalarCountInAttachedTable(db, "comparison_db", "investigator_high_priority_validation_queue")));
        appendRunStatus(caseDir, "three_database_high_priority_evidence_packet_start", "materializing bounded evidence packet from sidecar queue and indexed source evidence tables");
        db.exec("DROP TABLE IF EXISTS comparison_db.high_priority_validation_evidence_packet;");
        db.exec(R"SQL(
CREATE TABLE comparison_db.high_priority_validation_evidence_packet AS
SELECT q.artifact_id,q.source_id,q.store_guid,q.inode_num,q.parent_inode_num,q.file_name,q.best_path,
       q.poi_score,q.poi_priority,q.poi_category,'raw_date_candidates' AS evidence_table,
       CAST(d.raw_date_id AS TEXT) AS evidence_id,d.field_name AS evidence_field,
       COALESCE(NULLIF(d.parsed_utc,''),d.field_value) AS evidence_value,d.parsed_utc AS evidence_utc,
       COALESCE(d.date_type,'date') AS evidence_category,
       'raw_date_candidates.raw_date_id=' || CAST(d.raw_date_id AS TEXT) || '; parse_method=' || COALESCE(d.parse_method,'') AS validation_locator,
       q.validation_status
FROM comparison_db.investigator_high_priority_validation_queue q
JOIN main.raw_date_candidates d ON d.source_id=q.source_id AND d.artifact_id=q.artifact_id
UNION ALL
SELECT q.artifact_id,q.source_id,q.store_guid,q.inode_num,q.parent_inode_num,q.file_name,q.best_path,
       q.poi_score,q.poi_priority,q.poi_category,'raw_key_values' AS evidence_table,
       CAST(kv.raw_kv_id AS TEXT) AS evidence_id,kv.field_name AS evidence_field,
       substr(kv.field_value,1,1000) AS evidence_value,'' AS evidence_utc,
       'metadata_field' AS evidence_category,
       'raw_key_values.raw_kv_id=' || CAST(kv.raw_kv_id AS TEXT) AS validation_locator,
       q.validation_status
FROM comparison_db.investigator_high_priority_validation_queue q
JOIN main.raw_key_values kv ON kv.source_id=q.source_id AND kv.store_guid=q.store_guid AND kv.inode_num=q.inode_num
WHERE lower(kv.field_name) LIKE '%used%' OR lower(kv.field_name) LIKE '%download%' OR lower(kv.field_name) LIKE '%wherefrom%' OR lower(kv.field_name) LIKE '%display%' OR lower(kv.field_name) LIKE '%contenttype%' OR lower(kv.field_name) LIKE '%path%'
UNION ALL
SELECT q.artifact_id,q.source_id,q.store_guid,q.inode_num,q.parent_inode_num,q.file_name,q.best_path,
       q.poi_score,q.poi_priority,q.poi_category,'orphaned_deleted_candidates' AS evidence_table,
       CAST(o.candidate_id AS TEXT) AS evidence_id,o.existence_status AS evidence_field,
       COALESCE(NULLIF(o.orphan_reason,''),o.best_path,o.file_name) AS evidence_value,'' AS evidence_utc,
       'active_inventory_non_match' AS evidence_category,
       'orphaned_deleted_candidates.candidate_id=' || CAST(o.candidate_id AS TEXT) AS validation_locator,
       q.validation_status
FROM comparison_db.investigator_high_priority_validation_queue q
JOIN main.orphaned_deleted_candidates o ON o.source_id=q.source_id AND o.artifact_id=q.artifact_id
UNION ALL
SELECT q.artifact_id,q.source_id,q.store_guid,q.inode_num,q.parent_inode_num,q.file_name,q.best_path,
       q.poi_score,q.poi_priority,q.poi_category,'spotlight_cache_text' AS evidence_table,
       CAST(c.cache_text_id AS TEXT) AS evidence_id,c.cache_text_type AS evidence_field,
       substr(COALESCE(NULLIF(c.decoded_text,''),c.cache_relative_path),1,1000) AS evidence_value,'' AS evidence_utc,
       'cache_text' AS evidence_category,
       'spotlight_cache_text.cache_text_id=' || CAST(c.cache_text_id AS TEXT) AS validation_locator,
       q.validation_status
FROM comparison_db.investigator_high_priority_validation_queue q
JOIN main.spotlight_cache_text c ON c.source_id=q.source_id AND c.linked_artifact_id=q.artifact_id
UNION ALL
SELECT q.artifact_id,q.source_id,q.store_guid,q.inode_num,q.parent_inode_num,q.file_name,q.best_path,
       q.poi_score,q.poi_priority,q.poi_category,'artifacts' AS evidence_table,
       CAST(a.artifact_id AS TEXT) AS evidence_id,a.existence_status AS evidence_field,
       COALESCE(NULLIF(a.orphan_reason,''),a.best_path,a.file_name) AS evidence_value,'' AS evidence_utc,
       'active_file_comparison' AS evidence_category,
       'artifacts.artifact_id=' || CAST(a.artifact_id AS TEXT) AS validation_locator,
       q.validation_status
FROM comparison_db.investigator_high_priority_validation_queue q
JOIN main.artifacts a ON a.source_id=q.source_id AND a.artifact_id=q.artifact_id
)SQL");
        appendRunStatus(caseDir, "three_database_high_priority_evidence_packet_complete", "rows=" + std::to_string(scalarCountInAttachedTable(db, "comparison_db", "high_priority_validation_evidence_packet")));
        db.exec("CREATE INDEX comparison_db.idx_comparison_artifact_id ON artifact_filesystem_comparison(artifact_id);");
        db.exec("CREATE INDEX comparison_db.idx_comparison_status ON artifact_filesystem_comparison(source_id, existence_status);");
        db.exec("CREATE INDEX comparison_db.idx_comparison_missing_artifact ON orphaned_deleted_candidates(source_id, artifact_id);");
        db.exec("CREATE INDEX comparison_db.idx_poi_score ON investigator_points_of_interest(source_id, poi_score, artifact_id);");
        db.exec("CREATE INDEX comparison_db.idx_poi_category ON investigator_points_of_interest(source_id, poi_category);");
        db.exec("CREATE INDEX comparison_db.idx_poi_validation_priority ON investigator_points_of_interest_validation(source_id, poi_priority, poi_score);");
        db.exec("CREATE INDEX comparison_db.idx_high_poi_validation_priority ON investigator_high_priority_validation_queue(source_id, poi_score, artifact_id);");
        db.exec("CREATE INDEX comparison_db.idx_high_poi_evidence_packet ON high_priority_validation_evidence_packet(source_id, artifact_id, evidence_table);");
        db.exec("INSERT INTO comparison_db.three_database_manifest VALUES('comparison'," + sid + ",datetime('now'),(SELECT COUNT(*) FROM comparison_db.artifact_filesystem_comparison),'Materialized APFS active filesystem comparison sidecar. Missing rows are investigative leads only, not deletion proof.');");
        db.exec("INSERT INTO comparison_db.three_database_manifest VALUES('points_of_interest'," + sid + ",datetime('now'),(SELECT COUNT(*) FROM comparison_db.investigator_points_of_interest),'Validation-oriented points of interest; all rows are unvalidated investigative leads.');");
        db.exec("INSERT INTO comparison_db.three_database_manifest VALUES('points_of_interest_summary'," + sid + ",datetime('now'),(SELECT COUNT(*) FROM comparison_db.investigator_points_of_interest_summary),'Grouped POI summary by priority/category for triage review.');");
        db.exec("INSERT INTO comparison_db.three_database_manifest VALUES('points_of_interest_validation'," + sid + ",datetime('now'),(SELECT COUNT(*) FROM comparison_db.investigator_points_of_interest_validation),'POI validation workflow sidecar rows; evidence tables and validation workflow are advisory.');");
        db.exec("INSERT INTO comparison_db.three_database_manifest VALUES('high_priority_validation_queue'," + sid + ",datetime('now'),(SELECT COUNT(*) FROM comparison_db.investigator_high_priority_validation_queue),'High-priority POI queue for independent application validation. Rows remain unvalidated leads.');");
        db.exec("INSERT INTO comparison_db.three_database_manifest VALUES('high_priority_validation_evidence_packet'," + sid + ",datetime('now'),(SELECT COUNT(*) FROM comparison_db.high_priority_validation_evidence_packet),'Raw validation evidence packet for high-priority POI rows; includes dates, key/value fields, missing-candidate rows, cache text, and comparison evidence.');");
        detachSqliteSidecarNoThrow(db, "comparison_db");

        writeThreeDatabaseLayoutReadiness(caseDir, sourceIdText, spotlightDb, filesystemDb, comparisonDb, imageRows, comparisonRows, missingRows, "MATERIALIZED", "Transitional three-database layout is active for filesystem inventory and comparison sidecars; full GUI attach/open workflow remains roadmap work.");
        appendRunStatus(caseDir, "three_database_sidecar_complete", "filesystem_rows=" + std::to_string(imageRows) + " comparison_rows=" + std::to_string(comparisonRows) + " missing_candidates=" + std::to_string(missingRows));
        log.info("Three-database sidecars materialized: filesystem_inventory.sqlite rows=" + std::to_string(imageRows) + " comparison.sqlite comparison_rows=" + std::to_string(comparisonRows));
    } catch (const std::exception& ex) {
        detachSqliteSidecarNoThrow(db, "filesystem_db");
        detachSqliteSidecarNoThrow(db, "comparison_db");
        writeThreeDatabaseLayoutReadiness(caseDir, sourceIdText, spotlightDb, filesystemDb, comparisonDb, imageRows, comparisonRows, missingRows, "FAILED", ex.what());
        appendRunStatus(caseDir, "three_database_sidecar_failed", ex.what());
        log.warn(std::string("Three-database sidecar materialization failed; continuing primary case output: ") + ex.what());
    } catch (...) {
        detachSqliteSidecarNoThrow(db, "filesystem_db");
        detachSqliteSidecarNoThrow(db, "comparison_db");
        writeThreeDatabaseLayoutReadiness(caseDir, sourceIdText, spotlightDb, filesystemDb, comparisonDb, imageRows, comparisonRows, missingRows, "FAILED", "unknown exception");
        appendRunStatus(caseDir, "three_database_sidecar_failed", "unknown exception");
        log.warn("Three-database sidecar materialization failed with unknown exception; continuing primary case output.");
    }
}

void writeActiveFileComparisonReadinessFromDb(CaseDatabase& db,
                                             const fs::path& caseDir,
                                             const std::string& sourceId,
                                             Logger& log) {
    try {
        const fs::path outPath = caseDir / "active_file_comparison_readiness.csv";
        std::ofstream out(outPath, std::ios::binary);
        out << "image_inventory_source_id,created_utc,source_id,input_type,container_type,container_reader_status,partition_reader_status,filesystem_reader_status,spotlight_locator_status,active_comparison_status,inventory_file_count,comparison_candidate_count,comparison_ready,spotlight_artifact_count,image_inventory_rows,next_action,notes\n";
        auto st = db.prepare(R"SQL(
SELECT image_inventory_source_id,created_utc,source_id,input_type,container_type,container_reader_status,partition_reader_status,filesystem_reader_status,spotlight_locator_status,active_comparison_status,inventory_file_count,comparison_candidate_count,comparison_ready,spotlight_artifact_count,image_inventory_rows,next_action,notes
FROM vw_active_file_comparison_readiness
WHERE source_id=?
ORDER BY created_utc DESC, image_inventory_source_id DESC
LIMIT 1
)SQL");
        st.bind(1, sourceId);
        if (st.stepRow()) {
            for (int i = 0; i < 17; ++i) {
                if (i) out << ',';
                out << csvEscape(st.colText(i));
            }
            out << "\n";
        }
        log.info("Active file comparison readiness refreshed from SQLite view: " + pathString(outPath));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to refresh active_file_comparison_readiness.csv from SQLite view: ") + ex.what());
    }
}

void writeImageInventoryReadinessFromDb(CaseDatabase& db,
                                        const fs::path& caseDir,
                                        const std::string& sourceId,
                                        Logger& log) {
    try {
        const fs::path outPath = caseDir / "image_inventory_readiness.csv";
        std::ofstream out(outPath, std::ios::binary);
        out << "source_id,input_path,input_type,container_type,container_reader_status,partition_reader_status,filesystem_reader_status,spotlight_locator_status,active_comparison_status,preferred_reader_order,size_bytes,partition_scheme,partition_count,apfs_hint_count,spotlight_hint_count,inventory_file_count,comparison_ready,next_action,notes\n";
        auto st = db.prepare(R"SQL(
SELECT source_id,input_path,input_type,container_type,container_reader_status,partition_reader_status,filesystem_reader_status,spotlight_locator_status,active_comparison_status,preferred_reader_order,size_bytes,partition_scheme,partition_count,apfs_hint_count,spotlight_hint_count,inventory_file_count,comparison_ready,next_action,notes
FROM vw_image_inventory_sources
WHERE source_id=?
ORDER BY created_utc DESC, image_inventory_source_id DESC
LIMIT 1
)SQL");
        st.bind(1, sourceId);
        if (st.stepRow()) {
            for (int i = 0; i < 19; ++i) {
                if (i) out << ',';
                out << csvEscape(st.colText(i));
            }
            out << "\n";
        }
        log.info("Image inventory readiness refreshed from SQLite view: " + pathString(outPath));
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to refresh image_inventory_readiness.csv from SQLite view: ") + ex.what());
    }
}

void exportAff4ApfsLimitedRows(CaseDatabase& db,
                               const fs::path& csvPath,
                               const std::vector<std::string>& headers,
                               const std::string& sql,
                               const std::string& sourceId,
                               Logger& log) {
    const std::string fileName = csvPath.filename().string();
    const fs::path caseDir = csvPath.parent_path();
    long long rowsWritten = 0;
    appendRunStatus(caseDir, "aff4_apfs_sample_export_start", "file=" + fileName);
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
            ++rowsWritten;
        }
        appendRunStatus(caseDir, "aff4_apfs_sample_export_complete", "file=" + fileName + " rows=" + std::to_string(rowsWritten));
    } catch (const std::exception& ex) {
        appendRunStatus(caseDir, "aff4_apfs_sample_export_failed", "file=" + fileName + " error=" + ex.what());
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


struct Aff4ApfsUnresolvedObjectResolutionCounts {
    std::string status = "NOT_RUN";
    long long unresolvedBefore = 0;
    long long directCandidateRows = 0;
    long long parentCandidateRows = 0;
    long long artifactsNameUpdated = 0;
    long long artifactsPathUpdated = 0;
    long long nameScanPathsReconstructed = 0;
    long long noDirectChildMatchRows = 0;
    long long parentOnlyContextRows = 0;
    long long directAmbiguousRows = 0;
    std::string notes;
};

struct ApfsNameScanCandidate {
    std::string childFileId;
    std::string parentObjectId;
    std::string decodedName;
    std::string volumeName;
    std::string targetRole;
    std::string status;
    std::string sequence;
};

struct UnresolvedArtifactForApfsNameProbe {
    long long artifactId = 0;
    std::string storeGuid;
    std::string inode;
    std::string parentInode;
    std::string currentName;
    std::string currentPath;
};

bool isUsefulApfsDecodedName(const std::string& name) {
    if (name.empty()) return false;
    if (name == "." || name == ".." || name == "------NONAME------" || name == "(null)" || name == "NULL") return false;
    if (name.find("UNRESOLVED_SPOTLIGHT_OBJECT_INODE_") != std::string::npos) return false;
    return true;
}

Aff4ApfsUnresolvedObjectResolutionCounts runAff4ApfsUnresolvedObjectResolutionProbe(const fs::path& caseDir,
                                                                                     const EvidenceSource& source,
                                                                                     CaseDatabase& db,
                                                                                     Logger& log) {
    Aff4ApfsUnresolvedObjectResolutionCounts c;
    const fs::path nameScanCsv = caseDir / "aff4_apfs_spotlight_name_scan_sample.csv";
    const fs::path logicalWalkCsv = caseDir / "aff4_apfs_logical_directory_walk.csv";
    const fs::path outCsv = caseDir / "aff4_apfs_unresolved_spotlight_object_resolution_probe.csv";
    const fs::path jsonPath = caseDir / "aff4_apfs_unresolved_spotlight_object_resolution_probe_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_UNRESOLVED_SPOTLIGHT_OBJECT_RESOLUTION_PROBE.md";

    try {
        auto unresolvedCountStmt = db.prepare("SELECT COUNT(*) FROM artifacts WHERE source_id=? AND path_status='UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL'");
        unresolvedCountStmt.bind(1, source.sourceId);
        if (unresolvedCountStmt.stepRow()) c.unresolvedBefore = unresolvedCountStmt.colInt64(0);

        if (c.unresolvedBefore <= 0) {
            c.status = "SKIPPED_NO_UNRESOLVED_OBJECT_LABELS";
            c.notes = "No unresolved native Store-V2 object labels were present after enrichment.";
        } else if (!fs::exists(nameScanCsv) && !fs::exists(caseDir / "aff4_apfs_directory_record_name_index.csv")) {
            c.status = "SKIPPED_NO_APFS_NAME_SCAN";
            c.notes = "No APFS directory-record name index or aff4_apfs_spotlight_name_scan_sample.csv was present, so APFS directory-record name matching could not be attempted.";
        } else {
            appendRunStatus(caseDir, "aff4_apfs_unresolved_object_resolution_start", "unresolved=" + std::to_string(c.unresolvedBefore));
            std::unordered_map<std::string, std::vector<ApfsNameScanCandidate>> byChildId;
            std::unordered_map<std::string, std::vector<ApfsNameScanCandidate>> byParentId;

            const fs::path directoryIndexCsv = fs::exists(caseDir / "aff4_apfs_directory_record_name_index.csv")
                ? (caseDir / "aff4_apfs_directory_record_name_index.csv")
                : nameScanCsv;
            std::ifstream indexIn(directoryIndexCsv, std::ios::binary);
            if (!indexIn) throw std::runtime_error("Unable to open APFS directory-record name index: " + pathString(directoryIndexCsv));
            std::string headerLine;
            if (std::getline(indexIn, headerLine)) {
                if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
                auto headers = csvParseLine(headerLine);
                if (!headers.empty() && headers[0].size() >= 3 &&
                    static_cast<unsigned char>(headers[0][0]) == 0xEF &&
                    static_cast<unsigned char>(headers[0][1]) == 0xBB &&
                    static_cast<unsigned char>(headers[0][2]) == 0xBF) {
                    headers[0].erase(0, 3);
                }
                auto colIndex = [&](const std::string& name) -> int {
                    for (std::size_t i = 0; i < headers.size(); ++i) if (headers[i] == name) return static_cast<int>(i);
                    return -1;
                };
                const int childCol = colIndex("child_file_id_candidate");
                const int parentCol = colIndex("parent_object_id_candidate");
                const int nameCol = colIndex("decoded_name");
                const int volumeCol = colIndex("volume_name");
                const int roleCol = colIndex("target_role");
                const int statusCol = colIndex("status");
                const int sequenceCol = colIndex("sequence");
                if (childCol < 0 || parentCol < 0 || nameCol < 0) {
                    throw std::runtime_error("APFS directory-record name index is missing child_file_id_candidate, parent_object_id_candidate, or decoded_name columns: " + pathString(directoryIndexCsv));
                }
                auto fieldAt = [](const std::vector<std::string>& fields, int idx) -> std::string {
                    return idx >= 0 && static_cast<std::size_t>(idx) < fields.size() ? fields[static_cast<std::size_t>(idx)] : std::string();
                };
                std::string line;
                while (std::getline(indexIn, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.empty()) continue;
                    auto fields = csvParseLine(line);
                    ApfsNameScanCandidate cand;
                    cand.childFileId = fieldAt(fields, childCol);
                    cand.parentObjectId = fieldAt(fields, parentCol);
                    cand.decodedName = fieldAt(fields, nameCol);
                    cand.volumeName = fieldAt(fields, volumeCol);
                    cand.targetRole = fieldAt(fields, roleCol);
                    cand.status = fieldAt(fields, statusCol);
                    cand.sequence = fieldAt(fields, sequenceCol);
                    if (!isUsefulApfsDecodedName(cand.decodedName)) continue;
                    if (!cand.childFileId.empty()) byChildId[cand.childFileId].push_back(cand);
                    if (!cand.parentObjectId.empty()) byParentId[cand.parentObjectId].push_back(cand);
                }
            }

            std::unordered_map<std::string, std::string> logicalPathByChildId;
            if (fs::exists(logicalWalkCsv)) {
                try {
                    const Rows walkRows = readCsv(logicalWalkCsv);
                    for (const auto& r : walkRows) {
                        const std::string childId = get(r, "child_file_id");
                        const std::string path = get(r, "apfs_absolute_path");
                        if (!childId.empty() && !path.empty() && !logicalPathByChildId.count(childId)) logicalPathByChildId[childId] = path;
                    }
                } catch (const std::exception& ex) {
                    log.warn(std::string("Unable to read aff4_apfs_logical_directory_walk.csv for unresolved object probe: ") + ex.what());
                }
            }

            std::unordered_map<std::string, std::string> nameScanPathCache;
            std::function<std::string(const std::string&, std::set<std::string>&)> reconstructNameScanPath =
                [&](const std::string& childId, std::set<std::string>& seen) -> std::string {
                    if (childId.empty()) return {};
                    auto cached = nameScanPathCache.find(childId);
                    if (cached != nameScanPathCache.end()) return cached->second;
                    if (seen.count(childId) || seen.size() > 256) return {};
                    seen.insert(childId);

                    auto it = byChildId.find(childId);
                    if (it == byChildId.end() || it->second.empty()) return {};

                    std::vector<const ApfsNameScanCandidate*> ordered;
                    ordered.reserve(it->second.size());
                    for (const auto& cand : it->second) {
                        if (cand.volumeName == "Data") ordered.push_back(&cand);
                    }
                    for (const auto& cand : it->second) {
                        if (cand.volumeName != "Data") ordered.push_back(&cand);
                    }

                    for (const ApfsNameScanCandidate* cand : ordered) {
                        if (!cand || !isUsefulApfsDecodedName(cand->decodedName)) continue;
                        const std::string& parentId = cand->parentObjectId;
                        std::string candidatePath;
                        if (parentId.empty() || parentId == "0" || parentId == "1" || parentId == "2" || parentId == cand->childFileId) {
                            candidatePath = "/" + cand->decodedName;
                        } else {
                            std::string parentPath = reconstructNameScanPath(parentId, seen);
                            if (!parentPath.empty()) {
                                if (parentPath.size() > 1 && parentPath.back() == '/') parentPath.pop_back();
                                candidatePath = parentPath + "/" + cand->decodedName;
                            }
                        }
                        if (!candidatePath.empty()) {
                            nameScanPathCache[childId] = candidatePath;
                            seen.erase(childId);
                            return candidatePath;
                        }
                    }

                    seen.erase(childId);
                    return {};
                };

            std::ofstream out(outCsv, std::ios::binary);
            out << "artifact_id,store_guid,inode_num,parent_inode_num,current_file_name,current_best_path,direct_match_count,parent_match_count,selected_apfs_name,selected_apfs_path,selected_apfs_volume,selected_apfs_parent_object_id,selected_apfs_child_file_id,resolution_status,applied_to_artifact,confidence,notes\n";

            auto q = db.prepare(R"SQL(
SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,best_path
FROM artifacts
WHERE source_id=? AND path_status='UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL'
ORDER BY CAST(inode_num AS INTEGER), artifact_id
)SQL");
            q.bind(1, source.sourceId);
            std::vector<UnresolvedArtifactForApfsNameProbe> unresolvedRows;
            while (q.stepRow()) {
                UnresolvedArtifactForApfsNameProbe row;
                row.artifactId = q.colInt64(0);
                row.storeGuid = q.colText(1);
                row.inode = q.colText(2);
                row.parentInode = q.colText(3);
                row.currentName = q.colText(4);
                row.currentPath = q.colText(5);
                unresolvedRows.push_back(std::move(row));
            }

            auto updateName = db.prepare(R"SQL(
UPDATE artifacts
SET file_name=?,
    display_name=?,
    path_source='APFS_NAME_SCAN_CHILD_FILE_ID',
    path_status='APFS_NAME_RESOLVED_NO_FULL_PATH',
    confidence='MEDIUM_APFS_CHILD_FILE_ID_NAME_MATCH'
WHERE source_id=? AND artifact_id=? AND path_status='UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL'
)SQL");
            auto updatePath = db.prepare(R"SQL(
UPDATE artifacts
SET file_name=?,
    display_name=?,
    best_path=?,
    spotlight_display_path=?,
    normalized_mac_path=?,
    path_source='APFS_LOGICAL_DIRECTORY_CHILD_FILE_ID',
    path_status='APFS_LOGICAL_DIRECTORY_PATH_RESOLVED',
    confidence='HIGH_APFS_CHILD_FILE_ID_PATH_MATCH'
WHERE source_id=? AND artifact_id=? AND path_status IN ('UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL','APFS_NAME_RESOLVED_NO_FULL_PATH')
)SQL");

            db.begin();
            for (const auto& unresolved : unresolvedRows) {
                const long long artifactId = unresolved.artifactId;
                const std::string& storeGuid = unresolved.storeGuid;
                const std::string& inode = unresolved.inode;
                const std::string& parentInode = unresolved.parentInode;
                const std::string& currentName = unresolved.currentName;
                const std::string& currentPath = unresolved.currentPath;
                const auto directIt = byChildId.find(inode);
                const auto parentIt = byParentId.find(parentInode);
                const std::size_t directCount = directIt == byChildId.end() ? 0 : directIt->second.size();
                const std::size_t parentCount = parentIt == byParentId.end() ? 0 : parentIt->second.size();
                if (directCount > 0) ++c.directCandidateRows;
                if (parentCount > 0) ++c.parentCandidateRows;

                std::string selectedName;
                std::string selectedVolume;
                std::string selectedParent;
                std::string selectedChild;
                std::string selectedPath;
                std::string resolutionStatus = "NO_APFS_CHILD_FILE_ID_MATCH";
                std::string applied = "0";
                std::string confidence = "LOW_NO_MATCH";
                std::string notes = "No APFS name-scan row matched the Store-V2 object identifier as child_file_id_candidate.";

                if (directCount > 0) {
                    const auto& candidates = directIt->second;
                    const ApfsNameScanCandidate* chosen = nullptr;
                    for (const auto& cand : candidates) {
                        if (!parentInode.empty() && cand.parentObjectId == parentInode) { chosen = &cand; break; }
                    }
                    if (!chosen && candidates.size() == 1) chosen = &candidates.front();
                    if (!chosen) {
                        for (const auto& cand : candidates) {
                            if (cand.volumeName == "Data") { chosen = &cand; break; }
                        }
                    }
                    if (!chosen && !candidates.empty()) chosen = &candidates.front();

                    selectedName = chosen ? chosen->decodedName : std::string();
                    selectedVolume = chosen ? chosen->volumeName : std::string();
                    selectedParent = chosen ? chosen->parentObjectId : std::string();
                    selectedChild = chosen ? chosen->childFileId : std::string();
                    auto pathIt = logicalPathByChildId.find(selectedChild);
                    bool pathFromLogicalWalk = false;
                    bool pathFromNameScanChain = false;
                    if (pathIt != logicalPathByChildId.end()) {
                        selectedPath = pathIt->second;
                        pathFromLogicalWalk = true;
                    }
                    if (selectedPath.empty() && !selectedChild.empty()) {
                        std::set<std::string> seen;
                        selectedPath = reconstructNameScanPath(selectedChild, seen);
                        pathFromNameScanChain = !selectedPath.empty();
                    }

                    const bool parentMatches = !parentInode.empty() && selectedParent == parentInode;
                    if (!selectedPath.empty()) {
                        if (pathFromNameScanChain && !pathFromLogicalWalk) ++c.nameScanPathsReconstructed;
                        resolutionStatus = parentMatches
                            ? (pathFromLogicalWalk ? "DIRECT_APFS_CHILD_AND_PARENT_PATH_MATCH" : "DIRECT_APFS_CHILD_AND_PARENT_RECONSTRUCTED_NAME_SCAN_PATH_MATCH")
                            : (pathFromLogicalWalk ? "DIRECT_APFS_CHILD_PATH_MATCH_PARENT_UNCONFIRMED" : "DIRECT_APFS_CHILD_RECONSTRUCTED_NAME_SCAN_PATH_PARENT_UNCONFIRMED");
                        confidence = parentMatches
                            ? (pathFromLogicalWalk ? "HIGH_APFS_CHILD_PARENT_PATH_MATCH" : "MEDIUM_APFS_CHILD_PARENT_RECONSTRUCTED_NAME_SCAN_PATH_MATCH")
                            : (pathFromLogicalWalk ? "MEDIUM_APFS_CHILD_PATH_MATCH_PARENT_UNCONFIRMED" : "LOW_APFS_RECONSTRUCTED_NAME_SCAN_PATH_PARENT_UNCONFIRMED");
                        updatePath.bind(1, selectedName);
                        updatePath.bind(2, selectedName);
                        updatePath.bind(3, selectedPath);
                        updatePath.bind(4, selectedPath);
                        updatePath.bind(5, selectedPath);
                        updatePath.bind(6, source.sourceId);
                        updatePath.bind(7, artifactId);
                        updatePath.stepDone();
                        updatePath.reset();
                        ++c.artifactsPathUpdated;
                        applied = "1";
                        notes = pathFromLogicalWalk
                            ? "Applied APFS child-file-ID path candidate from logical directory walk."
                            : "Applied APFS child-file-ID path reconstructed from APFS name-scan parent chain; review as candidate current path.";
                    } else if (!selectedName.empty() && (parentMatches || candidates.size() == 1)) {
                        resolutionStatus = parentMatches ? "DIRECT_APFS_CHILD_AND_PARENT_NAME_MATCH" : "DIRECT_APFS_CHILD_NAME_MATCH_PARENT_UNCONFIRMED";
                        confidence = parentMatches ? "MEDIUM_APFS_CHILD_PARENT_NAME_MATCH" : "LOW_APFS_UNIQUE_CHILD_NAME_MATCH_PARENT_UNCONFIRMED";
                        updateName.bind(1, selectedName);
                        updateName.bind(2, selectedName);
                        updateName.bind(3, source.sourceId);
                        updateName.bind(4, artifactId);
                        updateName.stepDone();
                        updateName.reset();
                        ++c.artifactsNameUpdated;
                        applied = "1";
                        notes = "Applied APFS decoded name from child_file_id_candidate match; no full APFS path was available from logical directory walk.";
                    } else {
                        resolutionStatus = "DIRECT_APFS_CHILD_MATCH_AMBIGUOUS";
                        confidence = "LOW_REVIEW_DIRECT_MATCH_AMBIGUOUS";
                        notes = "Direct APFS child-file-ID candidates were present but were not applied because the match was ambiguous.";
                    }
                } else if (parentCount > 0) {
                    resolutionStatus = "PARENT_OBJECT_HAS_APFS_CHILDREN_ONLY";
                    confidence = "LOW_PARENT_CONTEXT_ONLY";
                    notes = "The Store-V2 parent object identifier appears in APFS name-scan rows, but the child object identifier was not directly resolved.";
                    ++c.parentOnlyContextRows;
                } else {
                    ++c.noDirectChildMatchRows;
                }
                if (resolutionStatus == "DIRECT_APFS_CHILD_MATCH_AMBIGUOUS") ++c.directAmbiguousRows;

                out << csvEscape(std::to_string(artifactId)) << ','
                    << csvEscape(storeGuid) << ','
                    << csvEscape(inode) << ','
                    << csvEscape(parentInode) << ','
                    << csvEscape(currentName) << ','
                    << csvEscape(currentPath) << ','
                    << directCount << ','
                    << parentCount << ','
                    << csvEscape(selectedName) << ','
                    << csvEscape(selectedPath) << ','
                    << csvEscape(selectedVolume) << ','
                    << csvEscape(selectedParent) << ','
                    << csvEscape(selectedChild) << ','
                    << csvEscape(resolutionStatus) << ','
                    << applied << ','
                    << csvEscape(confidence) << ','
                    << csvEscape(notes) << "\n";
            }
            db.commit();
            c.status = "RESOLUTION_PROBE_COMPLETED";
            c.notes = "APFS directory-record name index rows were matched against unresolved Store-V2 object identifiers. Direct child-file-ID matches may now use logical-directory paths or reconstructed APFS name-index parent chains; parent-only context is exported for review and is not applied.";
            const long long unresolvedAfterEstimate = c.unresolvedBefore - c.artifactsNameUpdated - c.artifactsPathUpdated;
            appendRunStatus(caseDir, "aff4_apfs_unresolved_object_resolution_complete", "unresolved=" + std::to_string(c.unresolvedBefore) + " estimated_unresolved_after=" + std::to_string(unresolvedAfterEstimate < 0 ? 0 : unresolvedAfterEstimate) + " direct_candidates=" + std::to_string(c.directCandidateRows) + " names_updated=" + std::to_string(c.artifactsNameUpdated) + " paths_updated=" + std::to_string(c.artifactsPathUpdated) + " no_direct_child_match=" + std::to_string(c.noDirectChildMatchRows) + " parent_only_context=" + std::to_string(c.parentOnlyContextRows) + " direct_ambiguous=" + std::to_string(c.directAmbiguousRows) + " name_scan_paths_reconstructed=" + std::to_string(c.nameScanPathsReconstructed));
            log.info("AFF4 APFS unresolved Spotlight object resolution probe written: " + pathString(outCsv));
        }
    } catch (const std::exception& ex) {
        db.rollbackNoThrow();
        if (c.status == "NOT_RUN") c.status = "RESOLUTION_PROBE_EXCEPTION";
        c.notes = ex.what();
        log.warn(std::string("AFF4 APFS unresolved Spotlight object resolution probe failed: ") + ex.what());
    }

    try {
        std::ofstream out(jsonPath, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
        out << "  \"status\": \"" << jsonEscape(c.status) << "\",\n";
        out << "  \"unresolved_before\": " << c.unresolvedBefore << ",\n";
        out << "  \"direct_candidate_rows\": " << c.directCandidateRows << ",\n";
        out << "  \"parent_candidate_rows\": " << c.parentCandidateRows << ",\n";
        const long long unresolvedAfterEstimate = c.unresolvedBefore - c.artifactsNameUpdated - c.artifactsPathUpdated;
        out << "  \"estimated_unresolved_after\": " << (unresolvedAfterEstimate < 0 ? 0 : unresolvedAfterEstimate) << ",\n";
        out << "  \"artifacts_name_updated\": " << c.artifactsNameUpdated << ",\n";
        out << "  \"artifacts_path_updated\": " << c.artifactsPathUpdated << ",\n";
        out << "  \"name_scan_paths_reconstructed\": " << c.nameScanPathsReconstructed << ",\n";
        out << "  \"no_direct_child_match_rows\": " << c.noDirectChildMatchRows << ",\n";
        out << "  \"parent_only_context_rows\": " << c.parentOnlyContextRows << ",\n";
        out << "  \"direct_ambiguous_rows\": " << c.directAmbiguousRows << ",\n";
        out << "  \"notes\": \"" << jsonEscape(c.notes) << "\"\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_unresolved_spotlight_object_resolution_probe_summary.json: ") + ex.what());
    }

    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Unresolved Spotlight Object Resolution Probe\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This diagnostic attempts to reduce `UNRESOLVED_SPOTLIGHT_OBJECT_INODE_*` labels by comparing Store-V2 object identifiers to APFS directory-record `child_file_id_candidate` values. V1.6.84 prefers the full local `aff4_apfs_directory_record_name_index.csv` when available and falls back to `aff4_apfs_spotlight_name_scan_sample.csv`. When the logical directory walk does not contain a full path, it also attempts a guarded APFS name-index parent-chain reconstruction.\n\n";
        out << "## Summary\n\n";
        out << "- Status: `" << c.status << "`\n";
        const long long unresolvedAfterEstimate = c.unresolvedBefore - c.artifactsNameUpdated - c.artifactsPathUpdated;
        out << "- Unresolved artifacts before probe: `" << c.unresolvedBefore << "`\n";
        out << "- Estimated unresolved artifacts after applied name/path updates: `" << (unresolvedAfterEstimate < 0 ? 0 : unresolvedAfterEstimate) << "`\n";
        out << "- Direct APFS child-file-ID candidate rows: `" << c.directCandidateRows << "`\n";
        out << "- Parent-context candidate rows: `" << c.parentCandidateRows << "`\n";
        out << "- Artifact names updated: `" << c.artifactsNameUpdated << "`\n";
        out << "- Artifact paths updated: `" << c.artifactsPathUpdated << "`\n";
        out << "- APFS name-scan parent-chain paths reconstructed: `" << c.nameScanPathsReconstructed << "`\n";
        out << "- Rows with no direct APFS child-file-ID match: `" << c.noDirectChildMatchRows << "`\n";
        out << "- Parent-only context rows: `" << c.parentOnlyContextRows << "`\n";
        out << "- Ambiguous direct child matches: `" << c.directAmbiguousRows << "`\n\n";
        out << "## Output\n\n";
        out << "- `aff4_apfs_unresolved_spotlight_object_resolution_probe.csv`\n";
        out << "- `aff4_apfs_unresolved_spotlight_object_resolution_probe_summary.json`\n\n";
        out << "## Interpretation caution\n\n";
        out << "Direct child-file-ID matches are applied as APFS-derived name/path candidates. Reconstructed name-scan paths are candidate current paths and should be reviewed against APFS provenance. Parent-only matches are exported for review and are not treated as proof of a current path.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_UNRESOLVED_SPOTLIGHT_OBJECT_RESOLUTION_PROBE.md: ") + ex.what());
    }

    return c;
}


struct SpotlightCacheTextIncorporationCounts {
    long long cacheFilesFound = 0;
    long long numericCacheFiles = 0;
    long long bucketMatches = 0;
    long long copiedStageMetadataMatched = 0;
    long long cacheFilenameEqualsCacheFileInode = 0;
    long long cacheFilenameDiffersFromCacheFileInode = 0;
    long long artifactInodeMatches = 0;
    long long rawRecordInodeMatches = 0;
    long long apfsDirectoryIndexMatches = 0;
    long long apfsPathsReconstructed = 0;
    long long artifactsTextUpdated = 0;
    long long artifactsPathUpdated = 0;
    long long cacheTextArtifactsCreated = 0;
    long long insertedRows = 0;
    long long plistXmlRows = 0;
    long long utf16Rows = 0;
    long long plainTextRows = 0;
    long long decodeFailures = 0;
    std::string status = "NOT_RUN";
    std::string notes;
};

struct SpotlightCacheStageMetadata {
    std::string cacheFileInode;
    std::string cacheFileParentInode;
    std::string cacheFileApfsPath;
    std::string sha256;
    long long sizeBytes = 0;
    std::uint64_t volumeSequence = 0;
};

struct SpotlightCacheApfsNameNode {
    std::uint64_t parent = 0;
    std::string name;
};

std::string spotlightCacheMetaKey(const std::string& storeGuid, const std::string& rel) {
    return asciiLower(storeGuid) + "|" + asciiLower(rel);
}

std::string spotlightCacheApfsKey(std::uint64_t volumeSequence, std::uint64_t fileId) {
    return std::to_string(volumeSequence) + ":" + std::to_string(fileId);
}

bool parseUnsigned64Strict(const std::string& s, std::uint64_t& out) {
    if (s.empty()) return false;
    for (char ch : s) if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    try { out = static_cast<std::uint64_t>(std::stoull(s)); return true; }
    catch (...) { return false; }
}

std::string extractApfsAbsolutePathFromNotes(const std::string& notes) {
    const std::string markerText = "apfs_absolute_path=";
    const auto pos = notes.find(markerText);
    if (pos == std::string::npos) return {};
    std::string value = notes.substr(pos + markerText.size());
    const auto semi = value.find(';');
    if (semi != std::string::npos) value = value.substr(0, semi);
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) value.pop_back();
    return value;
}

std::string normalizeSlashCopy(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.front() == '/') s.erase(s.begin());
    return s;
}

std::string relativePathForCacheFile(const fs::path& storeRoot, const fs::path& filePath) {
    std::error_code ec;
    fs::path rel = fs::relative(filePath, storeRoot, ec);
    if (ec) rel = filePath.filename();
    return normalizeSlashCopy(pathString(rel));
}

std::string detectCacheTextType(const std::string& text, const std::vector<unsigned char>& bytes, bool utf16) {
    std::string prefix = text.substr(0, std::min<std::size_t>(text.size(), 256U));
    const std::string lower = asciiLower(prefix);
    if (utf16) return "UTF16_TEXT";
    if (lower.find("<?xml") != std::string::npos || lower.find("<plist") != std::string::npos) return "PLIST_OR_XML_TEXT";
    if (bytes.size() >= 8 && std::memcmp(bytes.data(), "bplist00", 8) == 0) return "BPLIST_BYTES";
    if (lower.find("text found") != std::string::npos) return "SPOTLIGHT_EXTRACTED_TEXT_FRAGMENT";
    return "PLAINTEXT_OR_EXTRACTED_TEXT";
}

std::string decodeCacheTextBytes(const std::vector<unsigned char>& bytes, bool& utf16Likely) {
    utf16Likely = false;
    if (bytes.empty()) return {};
    std::size_t zerosOdd = 0, zerosEven = 0;
    const std::size_t inspect = std::min<std::size_t>(bytes.size(), 2048U);
    for (std::size_t i = 0; i < inspect; ++i) {
        if (bytes[i] == 0) {
            if ((i % 2U) == 0U) ++zerosEven; else ++zerosOdd;
        }
    }
    const bool bomLe = bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xfe;
    const bool bomBe = bytes.size() >= 2 && bytes[0] == 0xfe && bytes[1] == 0xff;
    utf16Likely = bomLe || bomBe || (inspect >= 64U && (zerosOdd > inspect / 4U || zerosEven > inspect / 4U));
    std::string out;
    out.reserve(bytes.size());
    if (utf16Likely) {
        const bool be = bomBe;
        std::size_t i = (bomLe || bomBe) ? 2U : 0U;
        for (; i + 1U < bytes.size(); i += 2U) {
            const unsigned char lo = be ? bytes[i + 1U] : bytes[i];
            const unsigned char hi = be ? bytes[i] : bytes[i + 1U];
            const std::uint16_t wc = static_cast<std::uint16_t>(lo) | (static_cast<std::uint16_t>(hi) << 8);
            if (wc == 0) continue;
            if (wc == '\r') { out.push_back('\n'); continue; }
            if (wc < 0x80U) out.push_back(static_cast<char>(wc));
            else out.push_back('?');
        }
    } else {
        for (unsigned char c : bytes) {
            if (c == 0) continue;
            if (c == '\r') out.push_back('\n');
            else out.push_back(static_cast<char>(c));
        }
    }
    constexpr std::size_t kMaxStoredCacheText = 256U * 1024U;
    if (out.size() > kMaxStoredCacheText) out.resize(kMaxStoredCacheText);
    return out;
}

std::string compactCacheTextSnippet(std::string s, std::size_t maxLen = 1000U) {
    for (char& ch : s) {
        if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
    }
    while (s.find("  ") != std::string::npos) s.replace(s.find("  "), 2, " ");
    if (s.size() > maxLen) s = s.substr(0, maxLen);
    return s;
}

std::unordered_map<std::string, SpotlightCacheStageMetadata> loadSpotlightCacheStageMetadata(const fs::path& caseDir) {
    std::unordered_map<std::string, SpotlightCacheStageMetadata> out;
    const fs::path csvPath = caseDir / "aff4_apfs_extracted_storev2_stage_files.csv";
    if (!fs::exists(csvPath)) return out;
    std::ifstream in(csvPath, std::ios::binary);
    std::string line;
    if (!std::getline(in, line)) return out;
    const auto headers = csvParseLine(line);
    auto idx = [&](const std::string& h) -> int {
        for (std::size_t i = 0; i < headers.size(); ++i) if (headers[i] == h) return static_cast<int>(i);
        return -1;
    };
    const int iGuid = idx("storev2_group_name"), iRel = idx("storev2_relative_path"), iChild = idx("child_file_id"), iParent = idx("parent_object_id"), iSha = idx("output_sha256"), iSize = idx("output_size_bytes"), iNotes = idx("notes"), iVol = idx("volume_sequence");
    while (std::getline(in, line)) {
        const auto cols = csvParseLine(line);
        auto col = [&](int i) -> std::string { return (i >= 0 && static_cast<std::size_t>(i) < cols.size()) ? cols[static_cast<std::size_t>(i)] : std::string(); };
        std::string rel = normalizeSlashCopy(col(iRel));
        if (rel.find("Cache/") == std::string::npos || rel.size() < 4 || rel.substr(rel.size() - 4) != ".txt") continue;
        SpotlightCacheStageMetadata m;
        m.cacheFileInode = col(iChild);
        m.cacheFileParentInode = col(iParent);
        m.sha256 = col(iSha);
        try { m.sizeBytes = col(iSize).empty() ? 0LL : std::stoll(col(iSize)); } catch (...) {}
        try { m.volumeSequence = col(iVol).empty() ? 0ULL : static_cast<std::uint64_t>(std::stoull(col(iVol))); } catch (...) {}
        m.cacheFileApfsPath = extractApfsAbsolutePathFromNotes(col(iNotes));
        out[spotlightCacheMetaKey(col(iGuid), rel)] = std::move(m);
    }
    return out;
}


struct ApfsImageInventoryMaterializationCounts {
    long long logicalWalkRows = 0;
    long long directoryIndexRows = 0;
    long long insertedRows = 0;
    long long finalInventoryRows = 0;
    std::string status = "NOT_RUN";
    std::string notes;
};

long long parseLongLongOrZero(const std::string& s) {
    try {
        if (s.empty()) return 0;
        return std::stoll(s);
    } catch (...) {
        return 0;
    }
}

std::string csvColumn(const std::vector<std::string>& cols, int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= cols.size()) return {};
    return cols[static_cast<std::size_t>(index)];
}

int csvHeaderIndex(const std::vector<std::string>& headers, const std::string& name) {
    for (std::size_t i = 0; i < headers.size(); ++i) if (headers[i] == name) return static_cast<int>(i);
    return -1;
}

std::string inventoryFileNameFromPathOrName(const std::string& path, const std::string& name) {
    if (!name.empty()) return name;
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    while (!p.empty() && p.back() == '/') p.pop_back();
    const auto pos = p.find_last_of('/');
    return pos == std::string::npos ? p : p.substr(pos + 1);
}

void bindApfsInventoryRow(SqlStatement& st,
                          const EvidenceSource& source,
                          const fs::path& originalInput,
                          const std::string& volumeName,
                          const std::string& volumeSequence,
                          const std::string& childId,
                          const std::string& parentId,
                          const std::string& fullPath,
                          const std::string& name,
                          long long isDirectory,
                          const std::string& provenance) {
    int i = 1;
    st.bind(i++, source.sourceId);
    st.bind(i++, std::string("AFF4"));
    st.bind(i++, pathString(originalInput));
    st.bind(i++, std::string(""));
    st.bind(i++, std::string("AFF4/APFS direct-map volume_sequence=") + volumeSequence);
    st.bind(i++, 0LL);
    st.bind(i++, std::string(""));
    st.bind(i++, 0LL);
    st.bind(i++, std::string("APFS"));
    st.bind(i++, std::string("volume_sequence=") + volumeSequence);
    st.bind(i++, volumeName);
    st.bind(i++, childId);
    st.bind(i++, parentId);
    st.bind(i++, childId);
    st.bind(i++, parentId);
    st.bind(i++, fullPath);
    st.bind(i++, inventoryFileNameFromPathOrName(fullPath, name));
    st.bind(i++, isDirectory);
    st.bind(i++, 0LL);
    st.bind(i++, 0LL);
    st.bind(i++, std::string(""));
    st.bind(i++, std::string(""));
    st.bind(i++, std::string(""));
    st.bind(i++, std::string(""));
    st.bind(i++, std::string(""));
    st.bind(i++, std::string("HIGH_AFF4_APFS_DIRECTORY_RECORD"));
    st.bind(i++, std::string("ACTIVE_APFS_DIRECTORY_RECORD"));
    st.bind(i++, provenance);
    st.bind(i++, nowUtc());
    st.stepDone();
    st.reset();
}

ApfsImageInventoryMaterializationCounts materializeAff4ApfsImageInventoryFromCsvs(const fs::path& caseDir,
                                                                                  const EvidenceSource& source,
                                                                                  const fs::path& originalInput,
                                                                                  CaseDatabase& db,
                                                                                  Logger& log) {
    ApfsImageInventoryMaterializationCounts c;
    const fs::path logicalWalkCsv = caseDir / "aff4_apfs_logical_directory_walk.csv";
    const fs::path directoryIndexCsv = fs::exists(caseDir / "aff4_apfs_directory_record_name_index.csv")
        ? (caseDir / "aff4_apfs_directory_record_name_index.csv")
        : (caseDir / "aff4_apfs_directory_record_name_index_sample.csv");
    const bool hasLogicalWalk = fs::exists(logicalWalkCsv);
    const bool hasDirectoryIndex = fs::exists(directoryIndexCsv);
    if (!hasLogicalWalk && !hasDirectoryIndex) {
        c.status = "SKIPPED_NO_APFS_DIRECTORY_CSVS";
        c.notes = "No aff4_apfs_logical_directory_walk.csv or aff4_apfs_directory_record_name_index.csv was available to populate image_file_inventory.";
        appendRunStatus(caseDir, "aff4_apfs_image_inventory_skipped", c.notes);
        log.info("AFF4/APFS image inventory materialization skipped: no APFS directory walk/name-index CSVs are present.");
        return c;
    }

    const std::string sid = sqlQuoteLiteral(source.sourceId);
    std::set<std::string> insertedKeys;
    try {
        appendRunStatus(caseDir, "aff4_apfs_image_inventory_materialize_start", "populate image_file_inventory from APFS logical directory/name-index rows");
        db.begin();
        db.exec("DELETE FROM image_file_inventory WHERE source_id=" + sid + " AND (container_type='AFF4' OR provenance LIKE 'AFF4_APFS_%');");
        auto ins = db.prepare("INSERT INTO image_file_inventory(source_id,container_type,container_path,aff4_stream_id,aff4_stream_name,partition_index,partition_scheme,partition_offset_bytes,filesystem_type,apfs_container_id,apfs_volume_name,filesystem_object_id,parent_filesystem_object_id,inode_num,parent_inode_num,full_path,file_name,is_directory,logical_size_bytes,allocated_size_bytes,created_utc,modified_utc,accessed_utc,changed_utc,file_sha256,source_confidence,extraction_status,provenance,created_utc_inventory) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");

        if (hasLogicalWalk) {
            std::ifstream in(logicalWalkCsv, std::ios::binary);
            std::string line;
            if (std::getline(in, line)) {
                const auto h = csvParseLine(line);
                const int iVolSeq = csvHeaderIndex(h, "volume_sequence");
                const int iVolName = csvHeaderIndex(h, "volume_name");
                const int iParent = csvHeaderIndex(h, "parent_object_id");
                const int iChild = csvHeaderIndex(h, "child_file_id");
                const int iName = csvHeaderIndex(h, "name");
                const int iRole = csvHeaderIndex(h, "walk_role");
                const int iPath = csvHeaderIndex(h, "apfs_absolute_path");
                while (std::getline(in, line)) {
                    const auto cols = csvParseLine(line);
                    const std::string child = csvColumn(cols, iChild);
                    if (child.empty() || child == "0") continue;
                    const std::string volSeq = csvColumn(cols, iVolSeq);
                    const std::string key = volSeq + ":" + child;
                    if (insertedKeys.count(key)) continue;
                    const std::string fullPath = csvColumn(cols, iPath);
                    if (fullPath.empty()) continue;
                    const std::string role = csvColumn(cols, iRole);
                    const long long isDir = (role.find("DIRECTORY") != std::string::npos) ? 1LL : 0LL;
                    bindApfsInventoryRow(ins, source, originalInput, csvColumn(cols, iVolName), volSeq, child, csvColumn(cols, iParent), fullPath, csvColumn(cols, iName), isDir, "AFF4_APFS_LOGICAL_DIRECTORY_WALK");
                    insertedKeys.insert(key);
                    ++c.logicalWalkRows;
                    ++c.insertedRows;
                }
            }
        }

        if (hasDirectoryIndex) {
            std::ifstream in(directoryIndexCsv, std::ios::binary);
            std::string line;
            if (std::getline(in, line)) {
                const auto h = csvParseLine(line);
                const int iVolSeq = csvHeaderIndex(h, "volume_sequence");
                const int iVolName = csvHeaderIndex(h, "volume_name");
                const int iParent = csvHeaderIndex(h, "parent_object_id_candidate");
                const int iChild = csvHeaderIndex(h, "child_file_id_candidate");
                const int iName = csvHeaderIndex(h, "decoded_name");
                while (std::getline(in, line)) {
                    const auto cols = csvParseLine(line);
                    const std::string child = csvColumn(cols, iChild);
                    if (child.empty() || child == "0") continue;
                    const std::string volSeq = csvColumn(cols, iVolSeq);
                    const std::string key = volSeq + ":" + child;
                    if (insertedKeys.count(key)) continue;
                    bindApfsInventoryRow(ins, source, originalInput, csvColumn(cols, iVolName), volSeq, child, csvColumn(cols, iParent), "", csvColumn(cols, iName), 0LL, "AFF4_APFS_DIRECTORY_RECORD_NAME_INDEX");
                    insertedKeys.insert(key);
                    ++c.directoryIndexRows;
                    ++c.insertedRows;
                }
            }
        }

        c.finalInventoryRows = scalarCountForSource(db, "image_file_inventory", source.sourceId);
        db.exec("UPDATE image_inventory_sources SET inventory_file_count=(SELECT COUNT(*) FROM image_file_inventory WHERE source_id=" + sid + " AND COALESCE(is_directory,0)=0), inventory_directory_count=(SELECT COUNT(*) FROM image_file_inventory WHERE source_id=" + sid + " AND COALESCE(is_directory,0)<>0), comparison_candidate_count=(SELECT COUNT(*) FROM artifacts WHERE source_id=" + sid + "), comparison_ready=CASE WHEN (SELECT COUNT(*) FROM image_file_inventory WHERE source_id=" + sid + ")>0 THEN 1 ELSE comparison_ready END, active_comparison_status=CASE WHEN (SELECT COUNT(*) FROM image_file_inventory WHERE source_id=" + sid + ")>0 THEN 'APFS_IMAGE_FILE_INVENTORY_READY' ELSE active_comparison_status END, next_action='Run enrichment active-file comparison against APFS-derived image_file_inventory rows.' WHERE source_id=" + sid + ";");
        db.commit();
        c.status = "APFS_IMAGE_FILE_INVENTORY_MATERIALIZED";
        c.notes = "Populated image_file_inventory from APFS logical-directory walk and directory-record name-index rows. Missing/not-present classifications remain investigative leads only, not deletion proof.";
        appendRunStatus(caseDir, "aff4_apfs_image_inventory_materialize_complete", "inserted_rows=" + std::to_string(c.insertedRows) + " final_inventory_rows=" + std::to_string(c.finalInventoryRows) + " logical_walk_rows=" + std::to_string(c.logicalWalkRows) + " directory_index_rows=" + std::to_string(c.directoryIndexRows));
        log.info("AFF4/APFS image_file_inventory materialized: inserted_rows=" + std::to_string(c.insertedRows) + " final_inventory_rows=" + std::to_string(c.finalInventoryRows));
    } catch (const std::exception& ex) {
        db.rollbackNoThrow();
        c.status = "APFS_IMAGE_FILE_INVENTORY_MATERIALIZE_EXCEPTION";
        c.notes = ex.what();
        appendRunStatus(caseDir, "aff4_apfs_image_inventory_materialize_failed", c.notes);
        log.warn(std::string("AFF4/APFS image inventory materialization failed: ") + ex.what());
    }
    return c;
}

std::unordered_map<std::string, SpotlightCacheApfsNameNode> loadApfsDirectoryNameIndexForCache(const fs::path& caseDir) {
    std::unordered_map<std::string, SpotlightCacheApfsNameNode> out;
    const fs::path csvPath = caseDir / "aff4_apfs_directory_record_name_index.csv";
    const fs::path fallback = caseDir / "aff4_apfs_directory_record_name_index_sample.csv";
    const fs::path usePath = fs::exists(csvPath) ? csvPath : fallback;
    if (!fs::exists(usePath)) return out;
    std::ifstream in(usePath, std::ios::binary);
    std::string line;
    if (!std::getline(in, line)) return out;
    const auto headers = csvParseLine(line);
    auto idx = [&](const std::string& h) -> int {
        for (std::size_t i = 0; i < headers.size(); ++i) if (headers[i] == h) return static_cast<int>(i);
        return -1;
    };
    const int iVol = idx("volume_sequence"), iParent = idx("parent_object_id_candidate"), iChild = idx("child_file_id_candidate"), iName = idx("decoded_name");
    while (std::getline(in, line)) {
        const auto cols = csvParseLine(line);
        auto col = [&](int i) -> std::string { return (i >= 0 && static_cast<std::size_t>(i) < cols.size()) ? cols[static_cast<std::size_t>(i)] : std::string(); };
        std::uint64_t vol = 0, parent = 0, child = 0;
        if (!parseUnsigned64Strict(col(iVol), vol) || !parseUnsigned64Strict(col(iParent), parent) || !parseUnsigned64Strict(col(iChild), child) || child == 0) continue;
        const std::string name = col(iName);
        if (name.empty()) continue;
        out.emplace(spotlightCacheApfsKey(vol, child), SpotlightCacheApfsNameNode{parent, name});
    }
    return out;
}

std::string reconstructCacheApfsPath(std::uint64_t volumeSequence,
                                     std::uint64_t childFileId,
                                     const std::unordered_map<std::string, SpotlightCacheApfsNameNode>& nodes) {
    std::vector<std::string> parts;
    std::set<std::uint64_t> seen;
    std::uint64_t cur = childFileId;
    for (std::size_t depth = 0; depth < 64U && cur != 0; ++depth) {
        if (!seen.insert(cur).second) break;
        const auto it = nodes.find(spotlightCacheApfsKey(volumeSequence, cur));
        if (it == nodes.end()) break;
        if (!it->second.name.empty()) parts.push_back(it->second.name);
        if (it->second.parent == 0 || it->second.parent == 2 || it->second.parent == cur) break;
        cur = it->second.parent;
    }
    if (parts.empty()) return {};
    std::reverse(parts.begin(), parts.end());
    std::string out = "/";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out += "/";
        out += parts[i];
    }
    return out;
}

SpotlightCacheTextIncorporationCounts runSpotlightCacheTextIncorporation(const fs::path& caseDir,
                                                                          const EvidenceSource& source,
                                                                          CaseDatabase& db,
                                                                          Logger& log) {
    SpotlightCacheTextIncorporationCounts c;
    const fs::path stagedRoot = caseDir / "ExtractedSpotlight" / "StagedStoreV2";
    const fs::path sampleCsv = caseDir / "aff4_apfs_spotlight_cache_text_sample.csv";
    const fs::path summaryJson = caseDir / "aff4_apfs_spotlight_cache_text_summary.json";
    const fs::path mdPath = caseDir / "AFF4_APFS_SPOTLIGHT_CACHE_TEXT.md";
    try {
        appendRunStatus(caseDir, "aff4_apfs_spotlight_cache_text_start", "scan StagedStoreV2 Cache/**/*.txt and link numeric names to Store-V2/APFS inodes");
        db.exec("DELETE FROM spotlight_cache_text WHERE source_id=" + sqlQuoteLiteral(source.sourceId) + ";");
        if (!fs::exists(stagedRoot)) {
            c.status = "NO_STAGED_STOREV2_ROOT";
            c.notes = "ExtractedSpotlight/StagedStoreV2 not present.";
        } else {
            auto stageMeta = loadSpotlightCacheStageMetadata(caseDir);
            auto apfsNodes = loadApfsDirectoryNameIndexForCache(caseDir);
            auto artifactStmt = db.prepare("SELECT artifact_id,file_name,best_path FROM artifacts WHERE source_id=? AND store_guid=? AND inode_num=? ORDER BY artifact_id LIMIT 1");
            auto rawStmt = db.prepare("SELECT raw_record_id FROM raw_records WHERE source_id=? AND store_guid=? AND inode_num=? ORDER BY raw_record_id LIMIT 1");
            auto ins = db.prepare("INSERT INTO spotlight_cache_text(source_id,store_guid,cache_relative_path,cache_numeric_id,cache_bucket_hex,bucket_matches_numeric_id,cache_file_inode_num,cache_file_parent_inode_num,cache_file_apfs_path,cache_file_sha256,cache_file_size_bytes,linked_artifact_id,linked_raw_record_id,inode_link_status,linked_artifact_name,linked_artifact_path,apfs_index_name,apfs_index_path,cache_text_type,decoded_text,decoded_text_length,decoded_text_sha256,ingestion_status,notes) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
            auto updateText = db.prepare("UPDATE artifacts SET index_text_snippet=CASE WHEN COALESCE(index_text_snippet,'')='' THEN ? ELSE index_text_snippet END WHERE artifact_id=?");
            auto updatePath = db.prepare("UPDATE artifacts SET best_path=?, normalized_mac_path=COALESCE(NULLIF(normalized_mac_path,''), ?), path_source='SPOTLIGHT_CACHE_FILENAME_INODE_APFS_DIRECTORY_INDEX', path_status='APFS_PATH_FROM_SPOTLIGHT_CACHE_FILENAME_INODE', confidence='MEDIUM' WHERE artifact_id=? AND ?<>'' AND (COALESCE(best_path,'')='' OR path_status='UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL' OR COALESCE(path_status,'')='')");
            auto insertCacheArtifact = db.prepare("INSERT INTO artifacts(source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,normalized_mac_path,path_source,path_status,content_type,content_type_tree,index_text_snippet,confidence) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
            auto insertCacheSourceInstance = db.prepare("INSERT INTO artifact_source_instances(artifact_id,raw_record_id,source_id,store_guid,inode_num,source_db,source_db_role,file_name,best_path) VALUES(?,?,?,?,?,?,?,?,?)");
            db.begin();
            fs::create_directories(sampleCsv.parent_path());
            std::ofstream sample(sampleCsv, std::ios::binary);
            sample << "cache_text_id,store_guid,cache_numeric_id,cache_relative_path,bucket_matches_numeric_id,cache_file_inode_num,cache_file_apfs_path,linked_artifact_id,linked_raw_record_id,inode_link_status,linked_artifact_name,linked_artifact_path,apfs_index_name,apfs_index_path,cache_text_type,decoded_text_length,decoded_text_preview,notes\n";
            constexpr long long kSampleLimit = 5000;
            std::error_code ec;
            for (const auto& storeEntry : fs::directory_iterator(stagedRoot, ec)) {
                if (ec) break;
                if (!storeEntry.is_directory()) continue;
                const std::string storeGuid = storeEntry.path().filename().string();
                const fs::path cacheRoot = storeEntry.path() / "Cache";
                if (!fs::exists(cacheRoot)) continue;
                std::error_code recEc;
                for (const auto& entry : fs::recursive_directory_iterator(cacheRoot, recEc)) {
                    if (recEc) break;
                    if (!entry.is_regular_file()) continue;
                    if (asciiLower(entry.path().extension().string()) != ".txt") continue;
                    ++c.cacheFilesFound;
                    const std::string rel = relativePathForCacheFile(storeEntry.path(), entry.path());
                    const std::string stem = entry.path().stem().string();
                    std::uint64_t cacheId = 0;
                    if (!parseUnsigned64Strict(stem, cacheId)) continue;
                    ++c.numericCacheFiles;
                    const std::string bucketHex = entry.path().parent_path().filename().string();
                    bool bucketMatches = false;
                    try { bucketMatches = (static_cast<std::uint64_t>(std::stoull(bucketHex, nullptr, 16)) == (cacheId >> 16)); } catch (...) {}
                    if (bucketMatches) ++c.bucketMatches;
                    SpotlightCacheStageMetadata meta;
                    const auto metaIt = stageMeta.find(spotlightCacheMetaKey(storeGuid, rel));
                    if (metaIt != stageMeta.end()) { meta = metaIt->second; ++c.copiedStageMetadataMatched; }
                    std::uint64_t cacheFileInodeValue = 0;
                    if (parseUnsigned64Strict(meta.cacheFileInode, cacheFileInodeValue)) {
                        if (cacheFileInodeValue == cacheId) ++c.cacheFilenameEqualsCacheFileInode;
                        else ++c.cacheFilenameDiffersFromCacheFileInode;
                    }
                    long long artifactId = 0, rawRecordId = 0;
                    std::string artifactName, artifactPath;
                    artifactStmt.bind(1, source.sourceId); artifactStmt.bind(2, storeGuid); artifactStmt.bind(3, std::to_string(cacheId));
                    if (artifactStmt.stepRow()) { artifactId = artifactStmt.colInt64(0); artifactName = artifactStmt.colText(1); artifactPath = artifactStmt.colText(2); ++c.artifactInodeMatches; }
                    artifactStmt.reset();
                    rawStmt.bind(1, source.sourceId); rawStmt.bind(2, storeGuid); rawStmt.bind(3, std::to_string(cacheId));
                    if (rawStmt.stepRow()) { rawRecordId = rawStmt.colInt64(0); ++c.rawRecordInodeMatches; }
                    rawStmt.reset();
                    std::string apfsName, apfsPath;
                    std::uint64_t apfsParentId = 0;
                    if (meta.volumeSequence != 0) {
                        const auto nodeIt = apfsNodes.find(spotlightCacheApfsKey(meta.volumeSequence, cacheId));
                        if (nodeIt != apfsNodes.end()) {
                            apfsName = nodeIt->second.name;
                            apfsParentId = nodeIt->second.parent;
                            ++c.apfsDirectoryIndexMatches;
                            apfsPath = reconstructCacheApfsPath(meta.volumeSequence, cacheId, apfsNodes);
                            if (!apfsPath.empty()) ++c.apfsPathsReconstructed;
                        }
                    }
                    std::vector<unsigned char> bytes;
                    try {
                        std::ifstream in(entry.path(), std::ios::binary);
                        constexpr std::size_t kMaxRead = 256U * 1024U;
                        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                        if (bytes.size() > kMaxRead) bytes.resize(kMaxRead);
                    } catch (...) {}
                    bool utf16 = false;
                    std::string text = decodeCacheTextBytes(bytes, utf16);
                    const std::string textType = detectCacheTextType(text, bytes, utf16);
                    if (text.empty() && !bytes.empty()) ++c.decodeFailures;
                    if (textType == "PLIST_OR_XML_TEXT") ++c.plistXmlRows;
                    else if (textType == "UTF16_TEXT") ++c.utf16Rows;
                    else ++c.plainTextRows;
                    const std::string textHash = text.empty() ? std::string() : sha256Bytes(reinterpret_cast<const unsigned char*>(text.data()), text.size());
                    const std::string snippet = compactCacheTextSnippet(text);
                    bool createdCacheArtifact = false;
                    if (artifactId == 0 && !apfsPath.empty()) {
                        const std::string createdName = apfsName.empty() ? entry.path().filename().string() : apfsName;
                        insertCacheArtifact.bind(1, source.sourceId);
                        insertCacheArtifact.bind(2, storeGuid);
                        insertCacheArtifact.bind(3, std::to_string(cacheId));
                        if (apfsParentId != 0) insertCacheArtifact.bind(4, std::to_string(apfsParentId)); else insertCacheArtifact.bindNull(4);
                        insertCacheArtifact.bind(5, createdName);
                        insertCacheArtifact.bind(6, createdName);
                        insertCacheArtifact.bind(7, apfsPath);
                        insertCacheArtifact.bind(8, apfsPath);
                        insertCacheArtifact.bind(9, "SPOTLIGHT_CACHE_FILENAME_INODE_APFS_DIRECTORY_INDEX");
                        insertCacheArtifact.bind(10, "APFS_PATH_FROM_SPOTLIGHT_CACHE_FILENAME_INODE");
                        insertCacheArtifact.bind(11, "Spotlight Store-V2 Cache Text");
                        insertCacheArtifact.bind(12, "public.text;com.apple.metadata.spotlight-cache");
                        insertCacheArtifact.bind(13, snippet);
                        insertCacheArtifact.bind(14, "MEDIUM_CACHE_FILENAME_INODE_APFS_MATCH");
                        insertCacheArtifact.stepDone();
                        insertCacheArtifact.reset();
                        artifactId = static_cast<long long>(sqlite3_last_insert_rowid(db.raw()));
                        artifactName = createdName;
                        artifactPath = apfsPath;
                        ++c.cacheTextArtifactsCreated;
                        createdCacheArtifact = true;
                        insertCacheSourceInstance.bind(1, artifactId);
                        if (rawRecordId != 0) insertCacheSourceInstance.bind(2, rawRecordId); else insertCacheSourceInstance.bindNull(2);
                        insertCacheSourceInstance.bind(3, source.sourceId);
                        insertCacheSourceInstance.bind(4, storeGuid);
                        insertCacheSourceInstance.bind(5, std::to_string(cacheId));
                        insertCacheSourceInstance.bind(6, "SpotlightCacheText");
                        insertCacheSourceInstance.bind(7, "CACHE_FILENAME_INODE_APFS_DIRECTORY_MATCH");
                        insertCacheSourceInstance.bind(8, createdName);
                        insertCacheSourceInstance.bind(9, apfsPath);
                        insertCacheSourceInstance.stepDone();
                        insertCacheSourceInstance.reset();
                    }
                    std::string status;
                    if (createdCacheArtifact) status = "CACHE_FILENAME_MATCHES_APFS_DIRECTORY_RECORD_CREATED_CACHE_TEXT_ARTIFACT";
                    else if (artifactId != 0 && !apfsPath.empty()) status = "CACHE_FILENAME_MATCHES_ARTIFACT_AND_APFS_INODE";
                    else if (artifactId != 0) status = "CACHE_FILENAME_MATCHES_ARTIFACT_INODE";
                    else if (rawRecordId != 0) status = "CACHE_FILENAME_MATCHES_RAW_RECORD_INODE";
                    else if (!apfsPath.empty()) status = "CACHE_FILENAME_MATCHES_APFS_DIRECTORY_RECORD_ONLY";
                    else status = "CACHE_TEXT_STANDALONE_UNLINKED";
                    std::string notes = "cache filename numeric stem treated as candidate indexed-file inode; cache_file_inode_num records the APFS inode of the cache .txt file itself";
                    if (createdCacheArtifact) notes += "; synthetic artifact created from cache filename inode plus APFS directory-record path so the extracted text is searchable in artifact review";
                    if (cacheFileInodeValue != 0 && cacheFileInodeValue != cacheId) notes += "; filename_does_not_equal_cache_file_inode";
                    ins.bind(1, source.sourceId); ins.bind(2, storeGuid); ins.bind(3, rel); ins.bind(4, static_cast<long long>(cacheId)); ins.bind(5, bucketHex); ins.bind(6, bucketMatches ? 1LL : 0LL);
                    ins.bind(7, meta.cacheFileInode); ins.bind(8, meta.cacheFileParentInode); ins.bind(9, meta.cacheFileApfsPath); ins.bind(10, meta.sha256); ins.bind(11, meta.sizeBytes);
                    if (artifactId != 0) ins.bind(12, artifactId); else ins.bindNull(12);
                    if (rawRecordId != 0) ins.bind(13, rawRecordId); else ins.bindNull(13);
                    ins.bind(14, status); ins.bind(15, artifactName); ins.bind(16, artifactPath); ins.bind(17, apfsName); ins.bind(18, apfsPath); ins.bind(19, textType); ins.bind(20, text); ins.bind(21, static_cast<long long>(text.size())); ins.bind(22, textHash); ins.bind(23, text.empty() ? "NO_DECODED_TEXT" : "DECODED_TEXT_STORED"); ins.bind(24, notes); ins.stepDone(); ins.reset();
                    ++c.insertedRows;
                    if (artifactId != 0 && !snippet.empty()) {
                        updateText.bind(1, snippet);
                        updateText.bind(2, artifactId);
                        updateText.stepDone();
                        const int changed = sqlite3_changes(db.raw());
                        updateText.reset();
                        if (changed > 0) ++c.artifactsTextUpdated;
                    }
                    if (artifactId != 0 && !apfsPath.empty() && !createdCacheArtifact) {
                        updatePath.bind(1, apfsPath);
                        updatePath.bind(2, apfsPath);
                        updatePath.bind(3, artifactId);
                        updatePath.bind(4, apfsPath);
                        updatePath.stepDone();
                        const int changed = sqlite3_changes(db.raw());
                        updatePath.reset();
                        if (changed > 0) ++c.artifactsPathUpdated;
                    }
                    if (c.insertedRows <= kSampleLimit) {
                        sample << c.insertedRows << ',' << csvEscape(storeGuid) << ',' << cacheId << ',' << csvEscape(rel) << ',' << (bucketMatches ? 1 : 0) << ','
                               << csvEscape(meta.cacheFileInode) << ',' << csvEscape(meta.cacheFileApfsPath) << ',' << artifactId << ',' << rawRecordId << ',' << csvEscape(status) << ','
                               << csvEscape(artifactName) << ',' << csvEscape(artifactPath) << ',' << csvEscape(apfsName) << ',' << csvEscape(apfsPath) << ',' << csvEscape(textType) << ',' << text.size() << ',' << csvEscape(snippet) << ',' << csvEscape(notes) << "\n";
                    }
                }
            }
            db.commit();
            c.status = "CACHE_TEXT_INCORPORATION_COMPLETED";
            c.notes = "Spotlight Store-V2 Cache .txt files were decoded and linked by numeric filename as a candidate indexed-file inode; cache file APFS inode is recorded separately.";
        }
    } catch (const std::exception& ex) {
        db.rollbackNoThrow();
        c.status = "CACHE_TEXT_INCORPORATION_EXCEPTION";
        c.notes = ex.what();
        log.warn(std::string("Spotlight cache text incorporation failed: ") + ex.what());
    }
    try {
        std::ofstream out(summaryJson, std::ios::binary);
        out << "{\n";
        out << "  \"generated_utc\": \"" << nowUtc() << "\",\n";
        out << "  \"app_version\": \"" << appVersion() << "\",\n";
        out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
        out << "  \"status\": \"" << jsonEscape(c.status) << "\",\n";
        out << "  \"cache_files_found\": " << c.cacheFilesFound << ",\n";
        out << "  \"numeric_cache_files\": " << c.numericCacheFiles << ",\n";
        out << "  \"bucket_matches_numeric_id\": " << c.bucketMatches << ",\n";
        out << "  \"copied_stage_metadata_matched\": " << c.copiedStageMetadataMatched << ",\n";
        out << "  \"cache_filename_equals_cache_file_inode\": " << c.cacheFilenameEqualsCacheFileInode << ",\n";
        out << "  \"cache_filename_differs_from_cache_file_inode\": " << c.cacheFilenameDiffersFromCacheFileInode << ",\n";
        out << "  \"artifact_inode_matches\": " << c.artifactInodeMatches << ",\n";
        out << "  \"raw_record_inode_matches\": " << c.rawRecordInodeMatches << ",\n";
        out << "  \"apfs_directory_index_matches\": " << c.apfsDirectoryIndexMatches << ",\n";
        out << "  \"apfs_paths_reconstructed\": " << c.apfsPathsReconstructed << ",\n";
        out << "  \"artifacts_text_updated\": " << c.artifactsTextUpdated << ",\n";
        out << "  \"artifacts_path_updated\": " << c.artifactsPathUpdated << ",\n";
        out << "  \"cache_text_artifacts_created\": " << c.cacheTextArtifactsCreated << ",\n";
        out << "  \"inserted_rows\": " << c.insertedRows << ",\n";
        out << "  \"plist_xml_rows\": " << c.plistXmlRows << ",\n";
        out << "  \"utf16_rows\": " << c.utf16Rows << ",\n";
        out << "  \"plain_text_rows\": " << c.plainTextRows << ",\n";
        out << "  \"decode_failures\": " << c.decodeFailures << ",\n";
        out << "  \"sample_csv\": \"aff4_apfs_spotlight_cache_text_sample.csv\",\n";
        out << "  \"notes\": \"" << jsonEscape(c.notes) << "\"\n";
        out << "}\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write aff4_apfs_spotlight_cache_text_summary.json: ") + ex.what());
    }
    try {
        std::ofstream out(mdPath, std::ios::binary);
        out << "# AFF4 APFS Spotlight Cache Text\n\n";
        out << "Version: " << appVersion() << "\n\n";
        out << "## Scope\n\n";
        out << "This pass scans `ExtractedSpotlight/StagedStoreV2/<GUID>/Cache/**/*.txt`, decodes text/plist/XML fragments, stores them in `spotlight_cache_text`, and treats the numeric filename as a candidate inode for the indexed object. The APFS inode of the cache `.txt` file itself is recorded separately so the relationship can be validated rather than assumed.\n\n";
        out << "## Outputs\n\n";
        out << "- `spotlight_cache_text` SQLite table\n";
        out << "- `vw_spotlight_cache_text_review` SQLite view\n";
        out << "- `aff4_apfs_spotlight_cache_text_sample.csv`\n";
        out << "- `aff4_apfs_spotlight_cache_text_summary.json`\n\n";
        out << "## Summary\n\n";
        out << "- Status: `" << c.status << "`\n";
        out << "- Cache `.txt` files found: `" << c.cacheFilesFound << "`\n";
        out << "- Artifact inode matches: `" << c.artifactInodeMatches << "`\n";
        out << "- APFS directory index matches: `" << c.apfsDirectoryIndexMatches << "`\n";
        out << "- Cache-text artifacts created from APFS-only matches: `" << c.cacheTextArtifactsCreated << "`\n";
        out << "- Cache filename equals cache-file APFS inode: `" << c.cacheFilenameEqualsCacheFileInode << "`\n";
        out << "- Cache filename differs from cache-file APFS inode: `" << c.cacheFilenameDiffersFromCacheFileInode << "`\n\n";
        out << "## Interpretation caution\n\n";
        out << "A numeric cache filename is treated as a candidate indexed-file inode/object identifier only when it links to Store-V2 artifacts/raw records or APFS directory-record rows. Unlinked cache text remains standalone Spotlight cached text with Store-V2 provenance.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_SPOTLIGHT_CACHE_TEXT.md: ") + ex.what());
    }
    appendRunStatus(caseDir, "aff4_apfs_spotlight_cache_text_complete", "cache_files=" + std::to_string(c.cacheFilesFound) + " artifact_matches=" + std::to_string(c.artifactInodeMatches) + " apfs_matches=" + std::to_string(c.apfsDirectoryIndexMatches));
    return c;
}

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
            const auto unresolvedResolutionCounts = runAff4ApfsUnresolvedObjectResolutionProbe(caseDir, source, db, log);
            (void)unresolvedResolutionCounts;
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

    try {
        appendRunStatus(caseDir, "aff4_apfs_staged_storev2_post_enrichment_exports_start", "bounded AFF4/APFS sample exports after enrichment_complete");
        appendRunStatus(caseDir, "aff4_apfs_high_priority_queue_temp_start", "materialize small high-priority POI queue before validation evidence export");
        db.exec("DROP TABLE IF EXISTS temp_aff4_high_priority_validation_queue;");
        db.exec("CREATE TEMP TABLE temp_aff4_high_priority_validation_queue AS SELECT * FROM vw_investigator_high_priority_validation_queue WHERE source_id=" + sqliteRunnerLiteral(source.sourceId) + " ORDER BY poi_score DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_date_utc,''), artifact_id) DESC LIMIT 5000;");
        db.exec("CREATE INDEX IF NOT EXISTS idx_temp_aff4_high_poi_artifact ON temp_aff4_high_priority_validation_queue(source_id, artifact_id);");
        db.exec("CREATE INDEX IF NOT EXISTS idx_temp_aff4_high_poi_inode ON temp_aff4_high_priority_validation_queue(source_id, store_guid, inode_num);");
        const long long highPriorityQueueRows = scalarCountForSource(db, "temp_aff4_high_priority_validation_queue", source.sourceId);
        appendRunStatus(caseDir, "aff4_apfs_high_priority_queue_temp_complete", "rows=" + std::to_string(highPriorityQueueRows));
        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_artifacts_sample.csv",
            {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","path_source","path_status","content_type","logical_size_bytes","physical_size_bytes","last_updated_utc","confidence"},
            "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,logical_size_bytes,physical_size_bytes,last_updated_utc,confidence FROM artifacts WHERE source_id=? ORDER BY artifact_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_timeline_sample.csv",
            {"timeline_id","artifact_id","store_guid","inode_num","event_timestamp_utc","event_type","event_source_field","file_name","path"},
            "SELECT timeline_id,artifact_id,store_guid,inode_num,event_timestamp_utc,event_type,event_source_field,file_name,path FROM timeline_events WHERE source_id=? ORDER BY event_timestamp_utc, timeline_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_points_of_interest_summary.csv",
            {"poi_priority","poi_category","lead_count","max_score","avg_score","missing_leads","usage_leads","cache_text_leads","where_from_leads","external_volume_leads","validation_status"},
            "SELECT poi_priority,poi_category,COUNT(*) AS lead_count,MAX(poi_score) AS max_score,ROUND(AVG(poi_score),1) AS avg_score,SUM(CASE WHEN missing_candidate_rows>0 THEN 1 ELSE 0 END) AS missing_leads,SUM(CASE WHEN usage_date_count>0 THEN 1 ELSE 0 END) AS usage_leads,SUM(CASE WHEN cache_text_rows>0 THEN 1 ELSE 0 END) AS cache_text_leads,SUM(CASE WHEN COALESCE(NULLIF(where_froms,''),'')<>'' OR downloaded_date_count>0 THEN 1 ELSE 0 END) AS where_from_leads,SUM(CASE WHEN external_volume_rows>0 THEN 1 ELSE 0 END) AS external_volume_leads,'UNVALIDATED_INVESTIGATIVE_LEAD' AS validation_status FROM vw_investigator_points_of_interest WHERE source_id=? GROUP BY poi_priority,poi_category ORDER BY max_score DESC, lead_count DESC, poi_category LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_points_of_interest_sample.csv",
            {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","best_path","content_type","existence_status","poi_score","poi_priority","poi_category","usage_latest_utc","usage_date_count","missing_candidate_rows","cache_text_rows","evidence_text_sample","validation_status"},
            "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,best_path,content_type,existence_status,poi_score,poi_priority,poi_category,usage_latest_utc,usage_date_count,missing_candidate_rows,cache_text_rows,evidence_text_sample,validation_status FROM vw_investigator_points_of_interest WHERE source_id=? ORDER BY poi_score DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_date_utc,''), artifact_id) DESC LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_points_of_interest_validation_sample.csv",
            {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","best_path","poi_score","poi_priority","poi_category","validation_evidence_tables","validation_workflow","validation_status","interpretation_note"},
            "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,best_path,poi_score,poi_priority,poi_category,validation_evidence_tables,validation_workflow,validation_status,interpretation_note FROM vw_investigator_points_of_interest WHERE source_id=? ORDER BY poi_score DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_date_utc,''), artifact_id) DESC LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_candidate_summary.csv",
            {"source_id","volume_name_or_token","high_confidence_hits","medium_confidence_hits","low_confidence_hits","path_hit_count","raw_value_hit_count","cache_text_hit_count","dictionary_hit_count","volfs_hit_count","first_date_utc","last_date_utc","sample_path_or_value","validation_status","interpretation_note"},
            "SELECT source_id,volume_name_or_token,high_confidence_hits,medium_confidence_hits,low_confidence_hits,path_hit_count,raw_value_hit_count,cache_text_hit_count,dictionary_hit_count,volfs_hit_count,first_date_utc,last_date_utc,sample_path_or_value,validation_status,interpretation_note FROM spotlight_external_volume_candidate_summary WHERE source_id=? ORDER BY high_confidence_hits DESC, medium_confidence_hits DESC, low_confidence_hits DESC, volume_name_or_token LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_evidence_review.csv",
            {"evidence_family","hit_id","source_id","artifact_id","store_guid","inode_num","source_db","source_table","source_field","evidence_type","volume_name_or_token","path_or_value","first_date_utc","last_date_utc","confidence","reason","validation_note"},
            "SELECT evidence_family,hit_id,source_id,artifact_id,store_guid,inode_num,source_db,source_table,source_field,evidence_type,volume_name_or_token,path_or_value,first_date_utc,last_date_utc,confidence,reason,validation_note FROM vw_spotlight_external_volume_evidence_review WHERE source_id=? ORDER BY CASE WHEN confidence LIKE 'HIGH%' THEN 0 WHEN confidence LIKE 'MEDIUM%' THEN 1 ELSE 2 END, evidence_family, volume_name_or_token, hit_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_raw_value_hits.csv",
            {"hit_id","source_id","raw_kv_id","artifact_id","store_guid","source_db","inode_num","store_id","source_field","evidence_type","volume_name_or_token","path_or_value","confidence","reason","validation_note","created_utc"},
            "SELECT hit_id,source_id,raw_kv_id,artifact_id,store_guid,source_db,inode_num,store_id,source_field,evidence_type,volume_name_or_token,path_or_value,confidence,reason,validation_note,created_utc FROM spotlight_external_volume_raw_value_hits WHERE source_id=? ORDER BY CASE WHEN confidence LIKE 'HIGH%' THEN 0 WHEN confidence LIKE 'MEDIUM%' THEN 1 ELSE 2 END, volume_name_or_token, hit_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_cache_text_hits.csv",
            {"hit_id","source_id","cache_text_id","artifact_id","store_guid","cache_numeric_id","source_field","evidence_type","volume_name_or_token","path_or_value","confidence","reason","validation_note","created_utc"},
            "SELECT hit_id,source_id,cache_text_id,artifact_id,store_guid,cache_numeric_id,source_field,evidence_type,volume_name_or_token,path_or_value,confidence,reason,validation_note,created_utc FROM spotlight_external_volume_cache_text_hits WHERE source_id=? ORDER BY CASE WHEN confidence LIKE 'HIGH%' THEN 0 WHEN confidence LIKE 'MEDIUM%' THEN 1 ELSE 2 END, volume_name_or_token, hit_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_dictionary_hits.csv",
            {"hit_id","source_id","store_guid","source_db","dictionary_table","dictionary_field","dictionary_value","evidence_type","confidence","reason","validation_note","created_utc"},
            "SELECT hit_id,source_id,store_guid,source_db,dictionary_table,dictionary_field,dictionary_value,evidence_type,confidence,reason,validation_note,created_utc FROM spotlight_external_volume_dictionary_hits WHERE source_id=? ORDER BY CASE WHEN confidence LIKE 'HIGH%' THEN 0 WHEN confidence LIKE 'MEDIUM%' THEN 1 ELSE 2 END, dictionary_table, dictionary_value, hit_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_volfs_hits.csv",
            {"hit_id","source_id","artifact_id","raw_kv_id","store_guid","source_db","inode_num","source_table","source_field","volume_name_or_token","path_or_value","confidence","reason","validation_note","created_utc"},
            "SELECT hit_id,source_id,artifact_id,raw_kv_id,store_guid,source_db,inode_num,source_table,source_field,volume_name_or_token,path_or_value,confidence,reason,validation_note,created_utc FROM spotlight_external_volume_volfs_hits WHERE source_id=? ORDER BY hit_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_high_priority_validation_queue.csv",
            {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","best_path","poi_score","poi_priority","poi_category","usage_latest_utc","usage_date_count","missing_candidate_rows","cache_text_rows","validation_queue_status","validation_queue_instruction","validation_evidence_tables","validation_workflow","validation_status"},
            "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,best_path,poi_score,poi_priority,poi_category,usage_latest_utc,usage_date_count,missing_candidate_rows,cache_text_rows,validation_queue_status,validation_queue_instruction,validation_evidence_tables,validation_workflow,validation_status FROM temp_aff4_high_priority_validation_queue WHERE source_id=? ORDER BY poi_score DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_date_utc,''), artifact_id) DESC LIMIT 5000",
            source.sourceId, log);

        appendRunStatus(caseDir, "aff4_apfs_high_priority_evidence_compact_export_start", "write compact high-priority validation packet index; full row-level evidence is materialized later in comparison.sqlite");
        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_high_priority_validation_evidence_packet.csv",
            {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","best_path","poi_score","poi_priority","poi_category","evidence_table","evidence_id","evidence_field","evidence_value","evidence_utc","evidence_category","validation_locator","validation_status"},
            R"SQL(
SELECT q.artifact_id,q.store_guid,q.inode_num,q.parent_inode_num,q.file_name,q.best_path,
       q.poi_score,q.poi_priority,q.poi_category,
       'validation_evidence_packet_index' AS evidence_table,
       CAST(q.artifact_id AS TEXT) AS evidence_id,
       'compact_validation_packet_summary' AS evidence_field,
       'thin_csv_compact=1; raw_date_candidate_count=' ||
          CAST((SELECT COUNT(*) FROM raw_date_candidates d WHERE d.source_id=q.source_id AND d.artifact_id=q.artifact_id) AS TEXT) ||
          '; missing_candidate_rows=' || CAST(COALESCE(q.missing_candidate_rows,0) AS TEXT) ||
          '; cache_text_rows=' || CAST(COALESCE(q.cache_text_rows,0) AS TEXT) ||
          '; usage_date_count=' || CAST(COALESCE(q.usage_date_count,0) AS TEXT) ||
          '; full_row_level_packet=comparison.sqlite.high_priority_validation_evidence_packet' AS evidence_value,
       COALESCE(NULLIF(q.usage_latest_utc,''), NULLIF(q.last_date_utc,''), '') AS evidence_utc,
       'bounded_validation_index' AS evidence_category,
       'temp_aff4_high_priority_validation_queue.artifact_id=' || CAST(q.artifact_id AS TEXT) ||
          '; use comparison.sqlite.high_priority_validation_evidence_packet and primary raw tables for row-level validation' AS validation_locator,
       q.validation_status
FROM temp_aff4_high_priority_validation_queue q
WHERE q.source_id=?
ORDER BY q.poi_score DESC, COALESCE(NULLIF(q.usage_latest_utc,''), NULLIF(q.last_date_utc,''), q.artifact_id) DESC
LIMIT 5000
)SQL",
            source.sourceId, log);
        appendRunStatus(caseDir, "aff4_apfs_high_priority_evidence_compact_export_complete", "compact thin evidence packet export complete; detailed sidecar packet follows after cache text processing");

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

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_field_inventory_sample.csv",
            {"field_inventory_id","field_name","row_count","populated_count","sample_value"},
            "SELECT field_inventory_id,field_name,row_count,populated_count,sample_value FROM field_inventory WHERE source_id=? ORDER BY row_count DESC, field_name LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_parser_coverage_summary_sample.csv",
            {"summary_id","metric_name","metric_value","created_utc"},
            "SELECT summary_id,metric_name,metric_value,created_utc FROM parser_coverage_summary WHERE source_id=? ORDER BY summary_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_dbstr_map_inventory_sample.csv",
            {"native_dbstr_map_id","store_guid","source_db","map_id","offset_entries","parsed_entries","skipped_entries","status","message","data_bytes","offsets_bytes","header_bytes"},
            "SELECT native_dbstr_map_id,store_guid,source_db,map_id,offset_entries,parsed_entries,skipped_entries,status,message,data_bytes,offsets_bytes,header_bytes FROM native_dbstr_map_inventory WHERE source_id=? ORDER BY store_guid,map_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_index_dictionary_summary_sample.csv",
            {"native_index_summary_id","store_guid","source_db","map_name","index_rows","value_ref_count","max_refs_per_index","max_ref_index"},
            "SELECT native_index_summary_id,store_guid,source_db,map_name,index_rows,value_ref_count,max_refs_per_index,max_ref_index FROM native_index_dictionary_summary WHERE source_id=? ORDER BY store_guid,map_name LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_property_dictionary_sample.csv",
            {"native_property_id","store_guid","source_db","property_index","property_name","prop_type_dec","value_type_dec","prop_type_hex","value_type_hex","is_core_native_field"},
            "SELECT native_property_id,store_guid,source_db,property_index,property_name,prop_type_dec,value_type_dec,prop_type_hex,value_type_hex,is_core_native_field FROM native_property_dictionary WHERE source_id=? ORDER BY store_guid,property_index LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_category_dictionary_sample.csv",
            {"native_category_id","store_guid","source_db","category_index","category_name"},
            "SELECT native_category_id,store_guid,source_db,category_index,category_name FROM native_category_dictionary WHERE source_id=? ORDER BY store_guid,category_index LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_path_reconstruction_sample.csv",
            {"link_id","store_guid","artifact_id","inode_num","parent_inode_num","file_name","best_path","path_source","path_status","parent_artifact_id","parent_file_name","parent_best_path","reconstructed_path_candidate","applied_to_artifact_path","candidate_matches_artifact_path","path_candidate_status","existing_path_context_only","new_reconstructed_path","sibling_count","relationship_status","path_reconstruction_method","confidence"},
            "SELECT link_id,store_guid,artifact_id,inode_num,parent_inode_num,file_name,best_path,path_source,path_status,parent_artifact_id,parent_file_name,parent_best_path,reconstructed_path_candidate,applied_to_artifact_path,candidate_matches_artifact_path,path_candidate_status,existing_path_context_only,new_reconstructed_path,sibling_count,relationship_status,path_reconstruction_method,confidence FROM vw_path_reconstruction WHERE source_id=? ORDER BY applied_to_artifact_path DESC, new_reconstructed_path DESC, existing_path_context_only DESC, link_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_path_reconstruction_metrics_sample.csv",
            {"summary_id","metric_name","metric_value","created_utc"},
            "SELECT summary_id,metric_name,metric_value,created_utc FROM parser_coverage_summary WHERE source_id=? AND metric_name LIKE 'parent_inode_%' ORDER BY summary_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_unresolved_after_resolution_sample.csv",
            {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","path_source","path_status","confidence"},
            "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,confidence FROM artifacts WHERE source_id=? AND (path_status LIKE 'APFS_%' OR path_status='UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL') ORDER BY CASE WHEN path_status='UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL' THEN 1 ELSE 0 END, artifact_id LIMIT 5000",
            source.sourceId, log);

        appendRunStatus(caseDir, "aff4_apfs_staged_storev2_post_enrichment_exports_complete", "bounded sample exports complete");
    } catch (const std::exception& ex) {
        log.warn(std::string("AFF4 APFS staged Store-V2 diagnostic sample export failed: ") + ex.what());
        appendRunStatus(caseDir, "aff4_apfs_staged_storev2_sample_export_failed", ex.what());
    }

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
        out << "- `aff4_apfs_staged_storev2_points_of_interest_summary.csv`\n";
        out << "- `aff4_apfs_staged_storev2_points_of_interest_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_points_of_interest_validation_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_high_priority_validation_queue.csv`\n";
        out << "- `aff4_apfs_staged_storev2_high_priority_validation_evidence_packet.csv`\n";
        out << "- `aff4_apfs_staged_storev2_raw_key_values_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_raw_date_candidates_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_raw_failures_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_field_inventory_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_parser_coverage_summary_sample.csv`\n";
        out << "- `aff4_apfs_staged_storev2_path_reconstruction_sample.csv`\n";
        out << "- `aff4_apfs_unresolved_spotlight_object_resolution_probe.csv`\n";
        out << "- `aff4_apfs_staged_storev2_unresolved_after_resolution_sample.csv`\n\n";
        out << "## Next step\n\n";
        out << "Use these outputs to validate whether APFS-extracted Store-V2 rows are investigator-useful, then add AFF4/APFS provenance columns into object-centric review views.\n";
    } catch (const std::exception& ex) {
        log.warn(std::string("Unable to write AFF4_APFS_STAGED_STOREV2_ENRICHMENT_PROBE.md: ") + ex.what());
    }

    log.info("AFF4 APFS staged Store-V2 enrichment probe written: " + pathString(jsonPath));
    return c;
}




void refreshAff4ApfsPostCacheValidationExports(const fs::path& caseDir,
                                               const EvidenceSource& source,
                                               CaseDatabase& db,
                                               Logger& log) {
    try {
        appendRunStatus(caseDir, "aff4_apfs_post_cache_validation_exports_start", "refresh POI/high-priority validation CSVs after Spotlight Cache text incorporation");
        appendRunStatus(caseDir, "spotlight_external_volume_cache_refresh_start", "refresh Spotlight-only external-volume cache text hits after Spotlight Cache text incorporation");
        db.exec("DELETE FROM spotlight_external_volume_cache_text_hits WHERE source_id=" + sqliteRunnerLiteral(source.sourceId) + ";");
        db.exec("DELETE FROM spotlight_external_volume_candidate_summary WHERE source_id=" + sqliteRunnerLiteral(source.sourceId) + ";");
        db.exec(R"SQL(
INSERT INTO spotlight_external_volume_cache_text_hits(source_id,cache_text_id,artifact_id,store_guid,cache_numeric_id,source_field,evidence_type,volume_name_or_token,path_or_value,confidence,reason,validation_note,created_utc)
SELECT c.source_id,c.cache_text_id,c.linked_artifact_id,c.store_guid,c.cache_numeric_id,'decoded_text',
       CASE WHEN lower(c.decoded_text) LIKE '%file:///volumes/%' THEN 'CACHE_TEXT_FILE_URL_VOLUMES_PATH'
            WHEN lower(c.decoded_text) LIKE '%/volumes/%' THEN 'CACHE_TEXT_VOLUMES_PATH'
            WHEN lower(c.decoded_text) LIKE '%/.vol/%' THEN 'CACHE_TEXT_VOLFS_REFERENCE'
            ELSE 'CACHE_TEXT_VOLUME_RELATED_TOKEN' END,
       CASE WHEN lower(c.decoded_text) LIKE '%/.vol/%' THEN '.vol' ELSE 'CACHE_TEXT_VOLUME_TOKEN' END,
       substr(c.decoded_text,1,1200),
       CASE WHEN lower(c.decoded_text) LIKE '%/volumes/%' OR lower(c.decoded_text) LIKE '%file:///volumes/%' THEN 'MEDIUM_CACHE_TEXT_PATH_INDICATOR' ELSE 'LOW_CACHE_TEXT_TOKEN_REVIEW_REQUIRED' END,
       'Spotlight Cache text contains explicit /Volumes, file:///Volumes, or .vol path evidence.',
       'Spotlight-only investigative lead; cache text may be content text rather than filesystem proof.',
       strftime('%Y-%m-%dT%H:%M:%SZ','now')
FROM spotlight_cache_text c
WHERE c.source_id=)SQL" + sqliteRunnerLiteral(source.sourceId) + R"SQL(
  AND (lower(c.decoded_text) LIKE '%/volumes/%'
       OR lower(c.decoded_text) LIKE '%file:///volumes/%'
       OR lower(c.decoded_text) LIKE '%/.vol/%')
  AND NOT (lower(c.decoded_text) LIKE '%/system/volumes/data/%' AND lower(c.decoded_text) NOT LIKE '%/system/volumes/data/volumes/%')
LIMIT 50000;
)SQL");
        db.exec(R"SQL(
INSERT INTO spotlight_external_volume_candidate_summary(source_id,volume_name_or_token,high_confidence_hits,medium_confidence_hits,low_confidence_hits,path_hit_count,raw_value_hit_count,cache_text_hit_count,dictionary_hit_count,volfs_hit_count,first_date_utc,last_date_utc,sample_path_or_value,validation_status,interpretation_note,created_utc)
WITH all_hits AS (
  SELECT source_id,volume_name_or_token,confidence,path_or_value,first_date_utc,last_date_utc,'path' AS family FROM spotlight_external_volume_path_hits WHERE source_id=)SQL" + sqliteRunnerLiteral(source.sourceId) + R"SQL(
  UNION ALL SELECT source_id,volume_name_or_token,confidence,path_or_value,NULL,NULL,'raw' FROM spotlight_external_volume_raw_value_hits WHERE source_id=)SQL" + sqliteRunnerLiteral(source.sourceId) + R"SQL(
  UNION ALL SELECT source_id,volume_name_or_token,confidence,path_or_value,NULL,NULL,'cache' FROM spotlight_external_volume_cache_text_hits WHERE source_id=)SQL" + sqliteRunnerLiteral(source.sourceId) + R"SQL(
  UNION ALL SELECT source_id,dictionary_value,confidence,dictionary_value,NULL,NULL,'dictionary' FROM spotlight_external_volume_dictionary_hits WHERE source_id=)SQL" + sqliteRunnerLiteral(source.sourceId) + R"SQL(
  UNION ALL SELECT source_id,volume_name_or_token,confidence,path_or_value,NULL,NULL,'volfs' FROM spotlight_external_volume_volfs_hits WHERE source_id=)SQL" + sqliteRunnerLiteral(source.sourceId) + R"SQL(
)
SELECT source_id,COALESCE(NULLIF(volume_name_or_token,''),'UNKNOWN_VOLUME_TOKEN'),
       SUM(CASE WHEN confidence LIKE 'HIGH%' THEN 1 ELSE 0 END),
       SUM(CASE WHEN confidence LIKE 'MEDIUM%' THEN 1 ELSE 0 END),
       SUM(CASE WHEN confidence LIKE 'LOW%' THEN 1 ELSE 0 END),
       SUM(CASE WHEN family='path' THEN 1 ELSE 0 END),
       SUM(CASE WHEN family='raw' THEN 1 ELSE 0 END),
       SUM(CASE WHEN family='cache' THEN 1 ELSE 0 END),
       SUM(CASE WHEN family='dictionary' THEN 1 ELSE 0 END),
       SUM(CASE WHEN family='volfs' THEN 1 ELSE 0 END),
       MIN(first_date_utc),MAX(last_date_utc),substr(MAX(path_or_value),1,1200),
       'SPOTLIGHT_ONLY_INVESTIGATIVE_LEAD',
       'Summary uses only Spotlight-derived tables. It is not proof of external-media use without validation/correlation.',
       strftime('%Y-%m-%dT%H:%M:%SZ','now')
FROM all_hits
GROUP BY source_id,COALESCE(NULLIF(volume_name_or_token,''),'UNKNOWN_VOLUME_TOKEN');
)SQL");
        appendRunStatus(caseDir, "spotlight_external_volume_cache_refresh_complete", "summary_rows=" + std::to_string(scalarCountForSource(db, "spotlight_external_volume_candidate_summary", source.sourceId)) + " cache_text_hits=" + std::to_string(scalarCountForSource(db, "spotlight_external_volume_cache_text_hits", source.sourceId)));
        appendRunStatus(caseDir, "aff4_apfs_post_cache_high_priority_queue_temp_start", "rebuild high-priority validation queue after cache text rows are available");
        db.exec("DROP TABLE IF EXISTS temp_aff4_high_priority_validation_queue;");
        db.exec("CREATE TEMP TABLE temp_aff4_high_priority_validation_queue AS SELECT * FROM vw_investigator_high_priority_validation_queue WHERE source_id=" + sqliteRunnerLiteral(source.sourceId) + " ORDER BY poi_score DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_date_utc,''), artifact_id) DESC LIMIT 5000;");
        db.exec("CREATE INDEX IF NOT EXISTS idx_temp_aff4_high_poi_artifact ON temp_aff4_high_priority_validation_queue(source_id, artifact_id);");
        db.exec("CREATE INDEX IF NOT EXISTS idx_temp_aff4_high_poi_inode ON temp_aff4_high_priority_validation_queue(source_id, store_guid, inode_num);");
        const long long highPriorityQueueRows = scalarCountForSource(db, "temp_aff4_high_priority_validation_queue", source.sourceId);
        appendRunStatus(caseDir, "aff4_apfs_post_cache_high_priority_queue_temp_complete", "rows=" + std::to_string(highPriorityQueueRows));

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_points_of_interest_summary.csv",
            {"poi_priority","poi_category","lead_count","max_score","avg_score","missing_leads","usage_leads","cache_text_leads","where_from_leads","external_volume_leads","validation_status"},
            "SELECT poi_priority,poi_category,COUNT(*) AS lead_count,MAX(poi_score) AS max_score,ROUND(AVG(poi_score),1) AS avg_score,SUM(CASE WHEN missing_candidate_rows>0 THEN 1 ELSE 0 END) AS missing_leads,SUM(CASE WHEN usage_date_count>0 THEN 1 ELSE 0 END) AS usage_leads,SUM(CASE WHEN cache_text_rows>0 THEN 1 ELSE 0 END) AS cache_text_leads,SUM(CASE WHEN COALESCE(NULLIF(where_froms,''),'')<>'' OR downloaded_date_count>0 THEN 1 ELSE 0 END) AS where_from_leads,SUM(CASE WHEN external_volume_rows>0 THEN 1 ELSE 0 END) AS external_volume_leads,'UNVALIDATED_INVESTIGATIVE_LEAD' AS validation_status FROM vw_investigator_points_of_interest WHERE source_id=? GROUP BY poi_priority,poi_category ORDER BY max_score DESC, lead_count DESC, poi_category LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_points_of_interest_sample.csv",
            {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","best_path","content_type","existence_status","poi_score","poi_priority","poi_category","usage_latest_utc","usage_date_count","missing_candidate_rows","cache_text_rows","evidence_text_sample","validation_status"},
            "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,best_path,content_type,existence_status,poi_score,poi_priority,poi_category,usage_latest_utc,usage_date_count,missing_candidate_rows,cache_text_rows,evidence_text_sample,validation_status FROM vw_investigator_points_of_interest WHERE source_id=? ORDER BY poi_score DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_date_utc,''), artifact_id) DESC LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_points_of_interest_validation_sample.csv",
            {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","best_path","poi_score","poi_priority","poi_category","validation_evidence_tables","validation_workflow","validation_status","interpretation_note"},
            "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,best_path,poi_score,poi_priority,poi_category,validation_evidence_tables,validation_workflow,validation_status,interpretation_note FROM vw_investigator_points_of_interest WHERE source_id=? ORDER BY poi_score DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_date_utc,''), artifact_id) DESC LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_candidate_summary.csv",
            {"source_id","volume_name_or_token","high_confidence_hits","medium_confidence_hits","low_confidence_hits","path_hit_count","raw_value_hit_count","cache_text_hit_count","dictionary_hit_count","volfs_hit_count","first_date_utc","last_date_utc","sample_path_or_value","validation_status","interpretation_note"},
            "SELECT source_id,volume_name_or_token,high_confidence_hits,medium_confidence_hits,low_confidence_hits,path_hit_count,raw_value_hit_count,cache_text_hit_count,dictionary_hit_count,volfs_hit_count,first_date_utc,last_date_utc,sample_path_or_value,validation_status,interpretation_note FROM spotlight_external_volume_candidate_summary WHERE source_id=? ORDER BY high_confidence_hits DESC, medium_confidence_hits DESC, low_confidence_hits DESC, volume_name_or_token LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_evidence_review.csv",
            {"evidence_family","hit_id","source_id","artifact_id","store_guid","inode_num","source_db","source_table","source_field","evidence_type","volume_name_or_token","path_or_value","first_date_utc","last_date_utc","confidence","reason","validation_note"},
            "SELECT evidence_family,hit_id,source_id,artifact_id,store_guid,inode_num,source_db,source_table,source_field,evidence_type,volume_name_or_token,path_or_value,first_date_utc,last_date_utc,confidence,reason,validation_note FROM vw_spotlight_external_volume_evidence_review WHERE source_id=? ORDER BY CASE WHEN confidence LIKE 'HIGH%' THEN 0 WHEN confidence LIKE 'MEDIUM%' THEN 1 ELSE 2 END, evidence_family, volume_name_or_token, hit_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_raw_value_hits.csv",
            {"hit_id","source_id","raw_kv_id","artifact_id","store_guid","source_db","inode_num","store_id","source_field","evidence_type","volume_name_or_token","path_or_value","confidence","reason","validation_note","created_utc"},
            "SELECT hit_id,source_id,raw_kv_id,artifact_id,store_guid,source_db,inode_num,store_id,source_field,evidence_type,volume_name_or_token,path_or_value,confidence,reason,validation_note,created_utc FROM spotlight_external_volume_raw_value_hits WHERE source_id=? ORDER BY CASE WHEN confidence LIKE 'HIGH%' THEN 0 WHEN confidence LIKE 'MEDIUM%' THEN 1 ELSE 2 END, volume_name_or_token, hit_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_cache_text_hits.csv",
            {"hit_id","source_id","cache_text_id","artifact_id","store_guid","cache_numeric_id","source_field","evidence_type","volume_name_or_token","path_or_value","confidence","reason","validation_note","created_utc"},
            "SELECT hit_id,source_id,cache_text_id,artifact_id,store_guid,cache_numeric_id,source_field,evidence_type,volume_name_or_token,path_or_value,confidence,reason,validation_note,created_utc FROM spotlight_external_volume_cache_text_hits WHERE source_id=? ORDER BY CASE WHEN confidence LIKE 'HIGH%' THEN 0 WHEN confidence LIKE 'MEDIUM%' THEN 1 ELSE 2 END, volume_name_or_token, hit_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_dictionary_hits.csv",
            {"hit_id","source_id","store_guid","source_db","dictionary_table","dictionary_field","dictionary_value","evidence_type","confidence","reason","validation_note","created_utc"},
            "SELECT hit_id,source_id,store_guid,source_db,dictionary_table,dictionary_field,dictionary_value,evidence_type,confidence,reason,validation_note,created_utc FROM spotlight_external_volume_dictionary_hits WHERE source_id=? ORDER BY CASE WHEN confidence LIKE 'HIGH%' THEN 0 WHEN confidence LIKE 'MEDIUM%' THEN 1 ELSE 2 END, dictionary_table, dictionary_value, hit_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "spotlight_external_volume_volfs_hits.csv",
            {"hit_id","source_id","artifact_id","raw_kv_id","store_guid","source_db","inode_num","source_table","source_field","volume_name_or_token","path_or_value","confidence","reason","validation_note","created_utc"},
            "SELECT hit_id,source_id,artifact_id,raw_kv_id,store_guid,source_db,inode_num,source_table,source_field,volume_name_or_token,path_or_value,confidence,reason,validation_note,created_utc FROM spotlight_external_volume_volfs_hits WHERE source_id=? ORDER BY hit_id LIMIT 5000",
            source.sourceId, log);

        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_high_priority_validation_queue.csv",
            {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","best_path","poi_score","poi_priority","poi_category","usage_latest_utc","usage_date_count","missing_candidate_rows","cache_text_rows","validation_queue_status","validation_queue_instruction","validation_evidence_tables","validation_workflow","validation_status"},
            "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,best_path,poi_score,poi_priority,poi_category,usage_latest_utc,usage_date_count,missing_candidate_rows,cache_text_rows,validation_queue_status,validation_queue_instruction,validation_evidence_tables,validation_workflow,validation_status FROM temp_aff4_high_priority_validation_queue WHERE source_id=? ORDER BY poi_score DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_date_utc,''), artifact_id) DESC LIMIT 5000",
            source.sourceId, log);

        appendRunStatus(caseDir, "aff4_apfs_post_cache_high_priority_evidence_compact_export_start", "refresh compact evidence packet after cache text rows are available");
        exportAff4ApfsLimitedRows(db, caseDir / "aff4_apfs_staged_storev2_high_priority_validation_evidence_packet.csv",
            {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","best_path","poi_score","poi_priority","poi_category","evidence_table","evidence_id","evidence_field","evidence_value","evidence_utc","evidence_category","validation_locator","validation_status"},
            R"SQL(
SELECT q.artifact_id,q.store_guid,q.inode_num,q.parent_inode_num,q.file_name,q.best_path,
       q.poi_score,q.poi_priority,q.poi_category,
       'validation_evidence_packet_index' AS evidence_table,
       CAST(q.artifact_id AS TEXT) AS evidence_id,
       'post_cache_compact_validation_packet_summary' AS evidence_field,
       'thin_csv_compact=1; post_cache_refresh=1; raw_date_candidate_count=' ||
          CAST((SELECT COUNT(*) FROM raw_date_candidates d WHERE d.source_id=q.source_id AND d.artifact_id=q.artifact_id) AS TEXT) ||
          '; missing_candidate_rows=' || CAST(COALESCE(q.missing_candidate_rows,0) AS TEXT) ||
          '; cache_text_rows=' || CAST(COALESCE(q.cache_text_rows,0) AS TEXT) ||
          '; usage_date_count=' || CAST(COALESCE(q.usage_date_count,0) AS TEXT) ||
          '; full_row_level_packet=comparison.sqlite.high_priority_validation_evidence_packet' AS evidence_value,
       COALESCE(NULLIF(q.usage_latest_utc,''), NULLIF(q.last_date_utc,''), '') AS evidence_utc,
       'bounded_post_cache_validation_index' AS evidence_category,
       'temp_aff4_high_priority_validation_queue.artifact_id=' || CAST(q.artifact_id AS TEXT) ||
          '; use comparison.sqlite.high_priority_validation_evidence_packet and primary raw/cache/comparison tables for row-level validation' AS validation_locator,
       q.validation_status
FROM temp_aff4_high_priority_validation_queue q
WHERE q.source_id=?
ORDER BY q.poi_score DESC, COALESCE(NULLIF(q.usage_latest_utc,''), NULLIF(q.last_date_utc,''), q.artifact_id) DESC
LIMIT 5000
)SQL",
            source.sourceId, log);
        appendRunStatus(caseDir, "aff4_apfs_post_cache_high_priority_evidence_compact_export_complete", "compact post-cache validation packet export complete");
        appendRunStatus(caseDir, "aff4_apfs_post_cache_validation_exports_complete", "post-cache POI and high-priority validation CSVs refreshed");
    } catch (const std::exception& ex) {
        appendRunStatus(caseDir, "aff4_apfs_post_cache_validation_exports_failed", ex.what());
        log.warn(std::string("Post-cache validation export refresh failed: ") + ex.what());
    }
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

// V1.1.7: AFF4 dynamic-load APFS probe moved to src/parsers/aff4_probe_worker.cpp.




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
    if (offset > static_cast<std::uint64_t>((std::numeric_limits<std::streamoff>::max)())) {
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
    RunResult result;
    const fs::path caseDir = !opt.caseDir.empty() ? opt.caseDir : opt.output;
    std::error_code caseDirEc;
    fs::create_directories(caseDir, caseDirEc);
    if (caseDirEc) {
        result.exitCode = 1;
        result.messages.push_back("FATAL: Unable to create case directory: " + pathString(caseDir) + " (" + caseDirEc.message() + ")");
        return result;
    }
    const fs::path writeProbe = caseDir / ".vestigant_write_probe.tmp";
    {
        std::ofstream probe(writeProbe, std::ios::binary);
        if (!probe) {
            result.exitCode = 1;
            result.messages.push_back("FATAL: Case directory is not writable: " + pathString(caseDir));
            return result;
        }
        probe << "ok\n";
    }
    std::error_code removeProbeEc;
    fs::remove(writeProbe, removeProbeEc);
    appendRunStatus(caseDir, "start", "application entry");
    Logger log(caseDir / "logs", opt.verbose);
    log.info("Run started. app_version=" + appVersion());
    log.info("Input=" + pathString(opt.input));
    log.info("CaseDir=" + pathString(caseDir));
    log.info("EvidenceRoot=" + pathString(opt.evidenceRoot));
    log.info("Effective parser caps: effective_max_native_records=" + std::to_string(opt.maxNativeRecords) +
             " max_native_records_explicit=" + std::string(opt.maxNativeRecordsExplicit ? "true" : "false") +
             " effective_max_native_blocks=" + std::to_string(opt.maxNativeBlocks) +
             " full_no_guardrails=" + std::string((opt.fullScan && opt.dbSizeGuardrailBytes == 0 && opt.maxNativeRecords == 0 && opt.maxNativeBlocks == 0) ? "true" : "false") +
             " gui_full_no_guardrails=" + std::string(opt.guiFullNoGuardrails ? "true" : "false") +
             " pressure_test_mode=" + std::string(opt.pressureTestMode ? "true" : "false") +
             " skip_container_hash=" + std::string(opt.skipContainerHash ? "true" : "false") +
             " force_container_hash=" + std::string(opt.forceContainerHash ? "true" : "false") +
             " preserve_evidence=" + std::string(opt.preserveEvidence ? "true" : "false") +
             " cache_text_enabled=" + std::string(isAff4SourcePath(opt.input) ? "true" : "normal_workflow") +
             " active_filesystem_inventory_enabled=" + std::string((opt.materializeIosFfsInventory || isAff4SourcePath(opt.input)) ? "true" : "deferred_or_not_requested"));
    if (opt.pressureTestMode) {
        appendRunStatus(caseDir, "pressure_test_mode_enabled", "hashing opt-in only; static preservation disabled; parser/native limits and DB guardrail disabled for triage validation");
        log.warn("Pressure Test / Triage Mode enabled: source-container SHA256 and static evidence preservation are skipped unless an explicit production/full-validation workflow is used; parser and DB guardrails are disabled for this run; FullValues native metadata decoding is enabled to test recovery of named usage/date fields.");
    }
    if (!opt.reuseIosCache.empty()) log.info("ReuseIosCache=" + pathString(opt.reuseIosCache));
    if (opt.experimentalFullNativeValues && toLower(opt.exportProfile) == "investigator") {
        appendRunStatus(caseDir, "validation_profile_full_values_investigator", "full native metadata decoding with investigator exports enabled");
        log.info("Full validation profile active: FullValues decode plus investigator export profile.");
    } else if (!opt.experimentalFullNativeValues && (toLower(opt.exportProfile) == "investigator" || toLower(opt.exportProfile) == "full")) {
        appendRunStatus(caseDir, "validation_warning_metadata_limited_export", "investigator/full exports requested without full native metadata values");
        log.warn("Investigator/full exports requested without full native metadata values. Usage, WhereFroms, path and tag-support views may be incomplete.");
    }
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
        appendRunStatus(caseDir, "source_probe_start", "source intake/readiness probe with bounded validation; AFF4/APFS Store-V2 copy-out, staging, parsing, and enrichment run when the guarded AFF4/APFS validation pipeline is enabled");
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
        // AFF4/APFS direct-map validation already performs exact AFF4/APFS structural scans after intake.
        // Keep the generic source-signature scan bounded for explicit AFF4 runs so the 70+ GB container
        // is not read once for generic string hints and then read again for the evidentiary SHA256 hash.
        const bool aff4BoundedGenericSignatureScan = aff4InputSource && opt.strictSingleAff4;
        const bool sourceProbeFullScan = opt.fullScan && !zipInputSource && !aff4BoundedGenericSignatureScan;
        if (cancelRequested()) return returnCancelled("before_source_probe_signature_scan");
        appendRunStatus(caseDir, "source_probe_signature_scan",
                        zipInputSource
                            ? "bounded ZIP signature probe; ZIP entries will be enumerated during staging/focused extraction"
                            : (sourceProbeFullScan ? "full source signature probe" : "bounded source signature probe"));
        if (zipInputSource && opt.fullScan) {
            log.info("Full-scan parsing remains enabled, but pre-stage source signature probing is bounded for ZIP containers to avoid scanning very large iOS FFS ZIPs before focused CoreSpotlight extraction.");
        }
        if (aff4BoundedGenericSignatureScan && opt.fullScan) {
            log.info("AFF4/APFS full-no-guardrails parsing remains enabled, but the generic pre-stage source signature probe is bounded; exact AFF4 ZIP/APFS direct-map scans and the evidentiary SHA256 hash run later.");
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
            appendRunProgress(caseDir, 13, "stage_zip_source_complete", "staged_root=" + pathString(stagedContainerWorkingRoot) + " ios_focused_used=" + std::string(iosFocusedUsed ? "1" : "0"));
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
            source.notes = "Original AFF4 container registered as fixed evidence source; guarded AFF4/APFS Spotlight Store-V2 staging, parsing, cache-text incorporation, APFS inventory, and active-file comparison are active for supported APFS images; general full-container filesystem extraction outside the guided APFS/Spotlight path remains staged.";
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
            const bool deferLargeImageHash = !opt.forceContainerHash;
            if (deferLargeImageHash) {
                appendRunStatus(caseDir, "original_container_hash_deferred_default", "source container SHA256 skipped by default for trial/thin runs; use --force-container-hash for full validation or production hashing");
                log.warn("Full original-container SHA256 skipped by default. Use --force-container-hash when a full evidentiary hash is needed.");
            }
            registerOriginalContainerSource(db, source, opt.input, {}, source.notes, log, deferLargeImageHash, caseDir, opt.externalSourceSha256, opt.externalSourceHashNote);
            const std::string stage = aff4InputSource ? "aff4_apfs_source_registered" : "unsupported_raw_image_source";
            const std::string message = aff4InputSource ? "AFF4 registered; guided APFS metadata, Store-V2 staging, native parsing, cache text, inventory, and comparison pipeline is active for supported APFS images; broad generic container-reader expansion remains staged" : "raw image registered; partition/filesystem extraction not implemented";
            const std::string nextAction = aff4InputSource
                ? "Review AFF4/APFS Store-V2 parse-selection coverage, parsed artifacts, cache text, APFS inventory, and active-file comparison outputs; use full-container hashing for production validation when required."
                : "Use V0_8_9 source_partition_probe.csv for partition readiness; raw image extraction remains secondary to AFF4-backed APFS inventory and active-file comparison.";
            log.warn((aff4InputSource ? "AFF4" : "Raw flat image") + std::string(" source selected and registered. Guided AFF4/APFS Spotlight Store-V2 copy-out/staging/parsing is active for supported APFS images; broader generic container-reader expansion remains staged."));
            appendRunStatus(caseDir, stage, message);
            appendRunStatus(caseDir, "source_probe_write", "write source intake readiness and roadmap artifacts");
            writeSourceIntakeArtifacts(caseDir, source, opt.input, {}, {}, "REGISTERED_UNSUPPORTED_CONTAINER", nextAction, sourceProbe, partitionProbe, log);
            writeImageInventoryReadinessCsv(caseDir, source, opt.input, sourceProbe, partitionProbe, nextAction, log);
            writeReaderToolReadinessArtifacts(caseDir, opt, source, opt.input, sourceProbe, log);
            persistImageInventoryReadiness(db, source, opt.input, sourceProbe, partitionProbe, nextAction, log);
            if (aff4InputSource) {
                writeAff4ApfsV1DiagnosticRerunPlan(caseDir, source, opt, opt.input, log);
                appendRunStatus(caseDir, "aff4_zip_single_file_probe", "parse AFF4 ZIP central directory from explicit input file only");
                writeAff4ZipSingleFileProbe(caseDir, source, opt, opt.input, log);
                appendRunStatus(caseDir, "aff4_apfs_exact_file_scan", "scan selected ZIP member payloads from the exact AFF4 file for GPT/APFS/Spotlight signatures");
                appendRunStatus(caseDir, "aff4_dynamic_load_probe", opt.enableAff4DynamicProbe ? "load libaff4 and perform bounded AFF4 random-access smoke test" : "skipped by default to avoid reader-driven access to other AFF4 files");
                Aff4ProbeWorker::executeDynamicLoadProbe(caseDir, source, opt, opt.input, cancelToken, log);
                appendRunStatus(caseDir, "aff4_apfs_staged_storev2_parser_probe", "discover and parse copied Store-V2 candidates if APFS extraction staged them");
                const auto stagedParserResult = runAff4ApfsStagedStoreV2ParserProbe(caseDir, source, opt, db, log);
                result.databaseCandidateCount = stagedParserResult.candidates.size();
                result.validDatabaseCandidateCount = countValidDatabaseCandidates(stagedParserResult.candidates);
                result.storeCount = countDistinctStoreGroups(stagedParserResult.candidates, false);
                result.validStoreCount = countDistinctStoreGroups(stagedParserResult.candidates, true);
                result.selectedParserDatabaseCount = stagedParserResult.selected.size();
                appendRunStatus(caseDir, "aff4_apfs_image_inventory_materialize", "materialize APFS-derived image_file_inventory rows before active filesystem comparison");
                const auto apfsInventoryCounts = materializeAff4ApfsImageInventoryFromCsvs(caseDir, source, opt.input, db, log);
                (void)apfsInventoryCounts;
                appendRunStatus(caseDir, "aff4_apfs_staged_storev2_enrichment_probe", "enrich APFS-staged Store-V2 parser rows when present");
                const auto stagedEnrichmentCounts = runAff4ApfsStagedStoreV2EnrichmentProbe(caseDir, source, db, log);
                appendRunStatus(caseDir, "aff4_apfs_spotlight_cache_text_probe", "decode and link Store-V2 Cache .txt extracted text");
                const auto cacheTextCounts = runSpotlightCacheTextIncorporation(caseDir, source, db, log);
                log.info("Spotlight Cache text GUI/search summary: cache_files_found=" + std::to_string(cacheTextCounts.cacheFilesFound) +
                         " spotlight_cache_text_rows=" + std::to_string(cacheTextCounts.insertedRows) +
                         " artifacts_text_updated=" + std::to_string(cacheTextCounts.artifactsTextUpdated) +
                         " cache_text_artifacts_created=" + std::to_string(cacheTextCounts.cacheTextArtifactsCreated) +
                         " cache_text_search_view_registered=true");
                refreshAff4ApfsPostCacheValidationExports(caseDir, source, db, log);
                writeActiveFileComparisonReadinessFromDb(db, caseDir, source.sourceId, log);
                writeImageInventoryReadinessFromDb(db, caseDir, source.sourceId, log);
                materializeThreeDatabaseSidecars(db, caseDir, source, log);
                const std::string aff4ParseStatus = stagedParserResult.selected.empty()
                    ? "AFF4_APFS_STAGED_STOREV2_NO_PARSEABLE_DATABASE_SELECTED"
                    : "AFF4_APFS_STAGED_STOREV2_PARSE_COMPLETE";
                const std::string aff4NextAction = stagedParserResult.selected.empty()
                    ? "Review AFF4/APFS staging and signature diagnostics; no parseable Store-V2 database was selected from the staged candidate groups."
                    : "Review AFF4/APFS Store-V2 parse-selection coverage, parsed artifacts, cache text, APFS inventory, and active-file comparison outputs; use full-container hashing for production validation when required.";
                writeSourceIntakeArtifacts(caseDir, source, opt.input, caseDir / "ExtractedSpotlight" / "StagedStoreV2", stagedParserResult.candidates, aff4ParseStatus, aff4NextAction, sourceProbe, partitionProbe, log);
                persistSourceProbeInventory(db, source, opt.input, caseDir / "ExtractedSpotlight" / "StagedStoreV2", stagedParserResult.candidates, aff4ParseStatus, aff4NextAction, sourceProbe, partitionProbe, log);
                result.rawRecordCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.rawRecordsBefore));
                result.rawKeyValueCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.rawKeyValuesBefore));
                result.rawDateCandidateCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.rawDateCandidatesBefore));
                result.artifactCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.artifacts));
                result.timelineCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.timelineEvents));
                result.usageCount = static_cast<std::size_t>(std::max<long long>(0, stagedEnrichmentCounts.usageEvidence));
                result.nativeDecodeMode = "AFF4_APFS_STAGED_STOREV2_" + stagedParserResult.decodeModeName;
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
            if (!aff4InputSource) {
                persistSourceProbeInventory(db, source, opt.input, {}, {}, "REGISTERED_UNSUPPORTED_CONTAINER", nextAction, sourceProbe, partitionProbe, log);
            }
            store.writeSummary(result);
            writeUiAndIosPlanningFiles(caseDir);
            createUploadBundle(caseDir);
            if (sourceProbeMode || topModeLower == "discover") {
                if (aff4InputSource && result.rawRecordCount > 0) {
                    appendRunStatus(caseDir, "complete_aff4_apfs_staged_storev2_validation_probe", "AFF4/APFS staged Store-V2 validation probe completed; raw_records=" + std::to_string(result.rawRecordCount) + " selected_databases=" + std::to_string(result.selectedParserDatabaseCount));
                } else {
                    appendRunStatus(caseDir, "complete_source_probe", "unsupported container registered and readiness report written");
                }
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
            bool effectiveSkipContainerHash = !opt.forceContainerHash;
            if (effectiveSkipContainerHash) {
                appendRunStatus(caseDir, "original_container_hash_deferred_default", "source ZIP SHA256 skipped by default for trial/thin runs; use --force-container-hash for full validation or production hashing");
                log.warn("Full original-container SHA256 skipped by default. Use --force-container-hash when a full evidentiary hash is required.");
            }
            registerOriginalContainerSource(db, source, opt.input, stagedContainerWorkingRoot, source.notes, log, effectiveSkipContainerHash, caseDir, opt.externalSourceSha256, opt.externalSourceHashNote);
            if (profile == SourceProfileKind::IOS) {
                const auto epLowerForIos = toLower(opt.exportProfile);
                const bool supportMaterializationRequested = opt.diagnosticFullNativeDb || epLowerForIos == "diagnostics" || epLowerForIos == "support" || epLowerForIos == "full";
                const bool materializeFfsInventory = opt.materializeIosFfsInventory || supportMaterializationRequested;
                const bool materializeAppDbRecords = opt.materializeIosAppDbRecords || supportMaterializationRequested;
                if (!materializeFfsInventory) {
                    appendRunStatus(caseDir, "ios_ffs_inventory_materialization_skipped", "Spotlight-first normal/minimal mode uses slim FFS path lookup instead of inserting millions of file rows; use --materialize-ios-ffs-inventory or --export-profile diagnostics/support/full for support correlation runs");
                    log.info("Normal/minimal iOS Spotlight-first mode will not materialize full FFS inventory rows into the active case DB.");
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
            } else {
                appendRunStatus(caseDir, "ios_source_specific_parsers_skipped_non_ios_profile", "non-iOS source profile; iOS FFS and iOS app-database parser stages were not run");
                log.info("Source profile is not iOS; skipping iOS-specific FFS inventory and app-database parser stages.");
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
        // Store discovery counts in case_summary must describe the database set that
        // will actually be parsed. When preservation redirects parsing to a staged copy,
        // the original discovery counts can be zero even though rediscovery found valid
        // Store-V2 databases. Refresh counts after preservation rediscovery and before
        // parser selection so validation bundles do not under-report parsed sources.
        result.databaseCandidateCount = parserCandidates.size();
        result.validDatabaseCandidateCount = countValidDatabaseCandidates(parserCandidates);
        result.storeCount = countDistinctStoreGroups(parserCandidates, false);
        result.validStoreCount = countDistinctStoreGroups(parserCandidates, true);
        parseStores = selectDatabasesForParsing(parserCandidates, profile, opt.fullScan, log);
        writeStoreSelectionCsv(caseDir, parserCandidates, parseStores, log);
        result.selectedParserDatabaseCount = parseStores.size();
        appendRunStatus(caseDir, "native_parse_start", "stores=" + std::to_string(parseStores.size()));
        NativeDecodeMode decodeMode = NativeDecodeMode::HeaderOnly;
        if (opt.experimentalFullNativeValues || opt.pressureTestMode) decodeMode = NativeDecodeMode::FullValues;
        else if (opt.decodeCoreNativeValues || diagnosticsMode) decodeMode = NativeDecodeMode::CoreFields;
        std::size_t nativeRecordLimit = opt.maxNativeRecords;
        if ((diagnosticsMode || opt.diagnosticFullNativeDb) && opt.experimentalFullNativeValues && !opt.maxNativeRecordsExplicit) {
            nativeRecordLimit = 10000;
            log.info("Diagnostics/full-native mode defaulting to max_native_records=10000 for crash-safe sampling. Override explicitly with --max-native-records after smaller samples succeed.");
        }
        NativeStoreDbParser parser(decodeMode, nativeRecordLimit, opt.maxNativeBlocks);
        parser.setProgressPath(caseDir / "logs" / "run_progress.tsv");
        parser.setPersistAllNativeKeyValues(opt.diagnosticFullNativeDb);
        const NativePersistenceMode nativePersistenceMode =
            (profile == SourceProfileKind::IOS) ? NativePersistenceMode::IosCoreSpotlightCompact :
            ((profile == SourceProfileKind::MacOS) ? NativePersistenceMode::MacOSStoreV2 : NativePersistenceMode::AutoPathSensitive);
        parser.setNativePersistenceMode(nativePersistenceMode);
        parser.setDbSizeGuardrailBytes(opt.dbSizeGuardrailBytes);
        if (opt.diagnosticFullNativeDb) {
            appendRunStatus(caseDir, "native_kv_persistence_full", "full native key/value persistence explicitly enabled for diagnostics");
            log.warn("Diagnostic full native key/value persistence is enabled. Native Store-V2/CoreSpotlight databases can grow very large.");
        } else if (nativePersistenceMode == NativePersistenceMode::IosCoreSpotlightCompact) {
            appendRunStatus(caseDir, "native_kv_persistence_ios_corespotlight_compact", "iOS CoreSpotlight compact mode keeps high-value key/value rows and bounded per-record date provenance");
            log.info("iOS CoreSpotlight key/value/date persistence is compact: broad dbStr/property rows and broad date candidates are summarized/suppressed. Use --diagnostic-full-native-db only for bounded support runs.");
        } else if (nativePersistenceMode == NativePersistenceMode::MacOSStoreV2) {
            appendRunStatus(caseDir, "native_kv_persistence_macos_storev2", "macOS Store-V2 mode uses macOS native metadata persistence; iOS CoreSpotlight compact filtering is disabled");
            log.info("macOS Store-V2 native metadata persistence selected. iOS CoreSpotlight compact filtering is disabled for this source profile.");
        } else {
            appendRunStatus(caseDir, "native_kv_persistence_auto_path_sensitive", "auto profile uses path-sensitive native persistence mode selection");
            log.info("Auto profile native persistence selected. iOS compact filtering is applied only to iOS-looking CoreSpotlight paths.");
        }
        appendRunStatus(caseDir, "native_parse_configuration",
                        std::string("decode_mode=") + (decodeMode == NativeDecodeMode::FullValues ? "FullValues" : (decodeMode == NativeDecodeMode::CoreFields ? "CoreFields" : "HeaderOnly")) +
                        "; pressure_test_mode=" + std::string(opt.pressureTestMode ? "true" : "false") +
                        "; max_native_records=" + std::to_string(nativeRecordLimit) +
                        "; max_native_blocks=" + std::to_string(opt.maxNativeBlocks));
        if (nativeRecordLimit > 0) log.info("Native parser record limit enabled: max_native_records=" + std::to_string(nativeRecordLimit));
        else log.info("Native parser record limit disabled: max_native_records=0 (unlimited)");
        if (opt.maxNativeBlocks > 0) log.info("Native parser metadata block limit enabled: max_native_blocks=" + std::to_string(opt.maxNativeBlocks));
        else log.info("Native parser metadata block limit disabled: max_native_blocks=0 (unlimited per store)");
        if (opt.dbSizeGuardrailBytes > 0) log.info("SQLite DB/WAL size guardrail enabled: " + std::to_string(static_cast<unsigned long long>(opt.dbSizeGuardrailBytes)) + " bytes");
        else log.warn("SQLite DB/WAL size guardrail is disabled by user option.");
        if (opt.experimentalFullNativeValues || opt.pressureTestMode) log.warn("Experimental full native metadata value parsing is enabled. Pressure Test / Triage mode enables this to recover named usage/date fields such as kMDItemUsedDates when present; this may be unstable on some store.db files.");
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
            log.info("Direct --evidence-root comparison is not used in V1.6.84. Active filesystem comparison uses validated in-case iOS FFS lookup rows when available; AFF4/APFS image-inventory comparison remains pending.");
            enrichmentSource.evidenceRoot.clear();
        }
        auto counts = enrichment.run(db, enrichmentSource, log);
        appendRunStatus(caseDir, "enrichment_complete", "artifacts=" + std::to_string(counts.artifacts) + " timeline=" + std::to_string(counts.timeline) + " usage=" + std::to_string(counts.usage));
        if (zipInputSource && profile == SourceProfileKind::IOS) {
            appendRunStatus(caseDir, "ios_app_db_post_enrichment_promotion_start", "re-promote parsed iOS app records after Store-V2 enrichment clears source-level timeline/usage rows");
            IosAppDbParser::promoteParsedRecordsToTimelineUsage(db, caseDir, source.sourceId, log,
                [](const fs::path& statusCaseDir, const std::string& stage, const std::string& message) {
                    appendRunStatus(statusCaseDir, stage, message);
                });
            counts.timeline = static_cast<std::size_t>(scalarCountForSource(db, "timeline_events", source.sourceId));
            counts.usage = static_cast<std::size_t>(scalarCountForSource(db, "usage_evidence", source.sourceId));
            appendRunStatus(caseDir, "ios_app_db_post_enrichment_promotion_complete", "timeline=" + std::to_string(counts.timeline) + " usage=" + std::to_string(counts.usage));
        }
        purgeOrphanSourceRows(db, caseDir, log);
        writeActiveFileComparisonReadinessFromDb(db, caseDir, source.sourceId, log);
        appendRunStatus(caseDir, "export_start");
        if (opt.suppressCsvExports) {
            const fs::path exportDir = caseDir / "exports";
            fs::create_directories(exportDir);
            {
                std::ofstream notice(exportDir / "CSV_EXPORTS_DISABLED.txt", std::ios::binary);
                notice << "CSV review exports were disabled for this run. The SQLite case database remains the primary review artifact.\n";
                notice << "Use the GUI Review tab for investigation, or rerun/export selected pages when CSV files are needed.\n";
            }
            {
                std::ofstream index(exportDir / "EXPORT_INDEX.csv", std::ios::binary);
                index << "export_name,path,source_path,row_count,chunked,generated_utc\n";
                index << "CSV_EXPORTS_DISABLED.txt," << pathString(exportDir / "CSV_EXPORTS_DISABLED.txt") << ",,0,0," << nowUtc() << "\n";
            }
            appendRunStatus(caseDir, "export_skipped_csv_disabled", "CSV review exports disabled by operator; SQLite case database and summaries retained");
            log.info("CSV review exports disabled by operator option. SQLite case database remains available for GUI review.");
        } else {
            SqliteExporter exporter;
            exporter.exportReviewPackage(db, caseDir / "exports", log, opt.exportProfile, cancelToken);
        }
        if (cancelRequested()) return returnCancelled("during_export");
        writeUiAndIosPlanningFiles(caseDir);
        result.artifactCount = counts.artifacts;
        result.usageCount = counts.usage;
        result.timelineCount = counts.timeline;
        result.orphanCandidateCount = counts.orphanCandidates;
        store.writeSummary(result);
        appendRunStatus(caseDir, "sqlite_wal_checkpoint", "flushing WAL to main database before upload packaging");
        try { db.exec("PRAGMA wal_checkpoint(TRUNCATE);"); }
        catch (const std::exception& ex) { log.warn(std::string("WAL checkpoint/truncate warning before upload packaging: ") + ex.what()); }
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
