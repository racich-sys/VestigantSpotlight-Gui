#include "ingest/evidence_preservation.h"
#include "core/hash.h"
#include "core/logger.h"
#include "core/path_utils.h"
#include "db/case_db.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>
#include <cstdio>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace vestigant::spotlight {
namespace {

std::string fileRoleFor(const fs::path& p) {
    const std::string name = toLower(p.filename().string());
    const std::string full = toLower(pathString(p));
    if (name == "store.db") return "store_db";
    if (name == ".store.db") return "dot_store_db";
    if (name == "volumeconfiguration.plist") return "volume_configuration";
    if (full.find("corespotlight") != std::string::npos) return "ios_corespotlight_file";
    if (full.find(".spotlight-v100") != std::string::npos || full.find("spotlight") != std::string::npos) return "spotlight_companion_file";
    return "other_related_file";
}

bool shouldIncludeBroadScan(const fs::path& p) {
    if (!fs::is_regular_file(p)) return false;
    const std::string name = toLower(p.filename().string());
    const std::string full = toLower(pathString(p));
    if (name == "store.db" || name == ".store.db" || name == "volumeconfiguration.plist") return true;
    if (full.find("store-v2") != std::string::npos) return true;
    if ((full.find("corespotlight") != std::string::npos || full.find("spotlight") != std::string::npos) &&
        (name.find("store") != std::string::npos || p.extension() == ".db" || p.extension() == ".sqlite" || p.extension() == ".plist")) return true;
    return false;
}

fs::path find7z() {
    const std::vector<fs::path> candidates = {
        fs::path("Tools") / "7z.exe",
        fs::path("tools") / "7z.exe",
        fs::path("C:/Program Files/7-Zip/7z.exe"),
        fs::path("C:/Program Files (x86)/7-Zip/7z.exe")
    };
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec)) return c;
    }
    return "7z.exe";
}
std::string quoteCmd(const fs::path& p) {
    std::string s = p.string();
#if defined(_WIN32)
    std::replace(s.begin(), s.end(), '/', '\\');
#endif
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
}


std::string windowsPathString(const fs::path& p) {
    std::string s = p.string();
#if defined(_WIN32)
    std::replace(s.begin(), s.end(), '/', '\\');
#endif
    return s;
}

std::string quoteArg(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
}

std::string quoteArg(const fs::path& p) { return quoteArg(windowsPathString(p)); }

struct ProcessRunResult {
    int exitCode = -1;
    bool started = false;
    std::string message;
};

ProcessRunResult runProcessToLog(const fs::path& exe, const std::string& args, const fs::path& workingDirectory, const fs::path& logPath) {
    fs::create_directories(logPath.parent_path());
#if defined(_WIN32)
    const std::string logS = windowsPathString(logPath);
    HANDLE logHandle = CreateFileA(logS.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logHandle == INVALID_HANDLE_VALUE) {
        return { static_cast<int>(GetLastError()), false, "Unable to create process log file: " + logS };
    }

    std::string cmdLine = quoteArg(exe) + " " + args;
    std::vector<char> cmd(cmdLine.begin(), cmdLine.end());
    cmd.push_back('\0');
    std::string cwd = windowsPathString(workingDirectory);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = logHandle;
    si.hStdError = logHandle;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(logHandle);
    if (!ok) {
        const int err = static_cast<int>(GetLastError());
        return { err, false, "CreateProcess failed; command=" + cmdLine + " cwd=" + cwd };
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return { static_cast<int>(exitCode), true, "" };
#else
    const std::string cmd = std::string("cd ") + quoteArg(workingDirectory) + " && " + quoteArg(exe) + " " + args + " > " + quoteArg(logPath) + " 2>&1";
    const int rc = std::system(cmd.c_str());
    return { rc, true, "" };
#endif
}

std::pair<std::uintmax_t, std::size_t> countRegularFilesAndBytes(const fs::path& root) {
    std::uintmax_t bytes = 0;
    std::size_t count = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        ++count;
        bytes += it->file_size(ec);
        if (ec) ec.clear();
    }
    return { bytes, count };
}
std::string csvEscape(const std::string& s) {
    if (s.find_first_of(",\"\r\n") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) out += (c == '"') ? "\"\"" : std::string(1, c);
    out += "\"";
    return out;
}

void writeManifest(const fs::path& path, const std::vector<PreservedEvidenceFile>& files) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << "relative_path,original_absolute_path,preserved_path,file_role,size_bytes,sha256\n";
    for (const auto& f : files) {
        out << csvEscape(f.relativePath) << ','
            << csvEscape(pathString(f.originalPath)) << ','
            << csvEscape(pathString(f.preservedPath)) << ','
            << csvEscape(f.fileRole) << ','
            << f.sizeBytes << ','
            << f.sha256 << "\n";
    }
}

std::string jsonEscape(const std::string& s) {
    std::ostringstream os;
    for (char c : s) {
        switch (c) {
        case '"': os << "\\\""; break;
        case '\\': os << "\\\\"; break;
        case '\n': os << "\\n"; break;
        case '\r': os << "\\r"; break;
        case '\t': os << "\\t"; break;
        default: os << c; break;
        }
    }
    return os.str();
}

void writePreservationJson(const fs::path& path, const PreservationResult& res, const EvidenceSource& source) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << "{\n";
    out << "  \"source_id\": \"" << jsonEscape(source.sourceId) << "\",\n";
    out << "  \"original_root_path\": \"" << jsonEscape(pathString(source.inputPath)) << "\",\n";
    out << "  \"staging_root\": \"" << jsonEscape(pathString(res.stagingRoot)) << "\",\n";
    out << "  \"archive_path\": \"" << jsonEscape(pathString(res.archivePath)) << "\",\n";
    out << "  \"archive_sha256\": \"" << jsonEscape(res.archiveSha256) << "\",\n";
    out << "  \"file_count\": " << res.fileCount << ",\n";
    out << "  \"total_original_bytes\": " << res.totalOriginalBytes << ",\n";
    out << "  \"status\": \"" << jsonEscape(res.status) << "\",\n";
    out << "  \"integrity_status\": \"" << jsonEscape(res.integrityStatus) << "\",\n";
    out << "  \"message\": \"" << jsonEscape(res.message) << "\"\n";
    out << "}\n";
}

void insertPreservationRows(CaseDatabase& db, const EvidenceSource& source, const PreservationResult& res, const std::vector<PreservedEvidenceFile>& files) {
    auto setStmt = db.prepare("INSERT INTO preserved_evidence_sets(source_id,archive_path,archive_format,archive_sha256,archive_size_bytes,created_utc,tool_used,tool_version,original_root_path,preserved_root_path,file_count,total_original_bytes,preservation_status,integrity_status,notes) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    int i = 1;
    setStmt.bind(i++, source.sourceId);
    setStmt.bind(i++, pathString(res.archivePath));
    setStmt.bind(i++, res.archiveCreated ? "7z" : "staging_only");
    setStmt.bind(i++, res.archiveSha256);
    setStmt.bind(i++, static_cast<long long>(res.archiveSizeBytes));
    setStmt.bind(i++, nowUtc());
    setStmt.bind(i++, "7-Zip external executable");
    setStmt.bind(i++, "external");
    setStmt.bind(i++, pathString(source.inputPath));
    setStmt.bind(i++, pathString(res.stagingRoot));
    setStmt.bind(i++, static_cast<long long>(res.fileCount));
    setStmt.bind(i++, static_cast<long long>(res.totalOriginalBytes));
    setStmt.bind(i++, res.status);
    setStmt.bind(i++, res.integrityStatus);
    setStmt.bind(i++, res.message);
    setStmt.stepDone();

    auto fileStmt = db.prepare("INSERT INTO preserved_evidence_files(source_id,relative_path,original_absolute_path,preserved_path,file_role,size_bytes,sha256,included_in_archive,created_utc) VALUES(?,?,?,?,?,?,?,?,?)");
    for (const auto& f : files) {
        int k = 1;
        fileStmt.bind(k++, source.sourceId);
        fileStmt.bind(k++, f.relativePath);
        fileStmt.bind(k++, pathString(f.originalPath));
        fileStmt.bind(k++, pathString(f.preservedPath));
        fileStmt.bind(k++, f.fileRole);
        fileStmt.bind(k++, static_cast<long long>(f.sizeBytes));
        fileStmt.bind(k++, f.sha256);
        fileStmt.bind(k++, res.archiveCreated ? 1LL : 0LL);
        fileStmt.bind(k++, nowUtc());
        fileStmt.stepDone(); fileStmt.reset();
    }

    auto checkStmt = db.prepare("INSERT INTO archive_integrity_checks(source_id,archive_path,check_type,check_started_utc,check_finished_utc,result,message) VALUES(?,?,?,?,?,?,?)");
    checkStmt.bind(1, source.sourceId);
    checkStmt.bind(2, pathString(res.archivePath));
    checkStmt.bind(3, "7z_test");
    checkStmt.bind(4, nowUtc());
    checkStmt.bind(5, nowUtc());
    checkStmt.bind(6, res.archiveVerified ? "PASS" : (res.archiveCreated ? "FAIL" : "NOT_RUN"));
    checkStmt.bind(7, res.message);
    checkStmt.stepDone();
}

} // namespace

std::vector<PreservedEvidenceFile> EvidencePreserver::identifyFiles(const RunOptions& opt,
                                                                     const EvidenceSource& source,
                                                                     const std::vector<StoreInfo>& stores,
                                                                     Logger& log) const {
    std::set<std::string> seen;
    std::vector<fs::path> raw;
    std::error_code ec;

    auto addFile = [&](const fs::path& p) {
        std::error_code lec;
        if (!fs::is_regular_file(p, lec)) return;
        auto key = fs::weakly_canonical(p, lec);
        const std::string keyStr = lec ? pathString(p) : pathString(key);
        if (seen.insert(toLower(keyStr)).second) raw.push_back(p);
    };

    if (fs::is_regular_file(source.inputPath, ec)) {
        addFile(source.inputPath);
    }

    for (const auto& s : stores) {
        if (!fs::exists(s.storePath, ec)) continue;
        addFile(s.storePath);
        const auto storeDir = s.storePath.parent_path();
        if (fs::is_directory(storeDir, ec)) {
            for (auto it = fs::directory_iterator(storeDir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
                if (it->is_regular_file(ec)) addFile(it->path());
            }
        }
        const auto volCfg = storeDir.parent_path().parent_path() / "VolumeConfiguration.plist";
        addFile(volCfg);
    }

    if (raw.empty() && fs::is_directory(source.inputPath, ec)) {
        const bool broadAllowed = opt.fullScan || stores.empty();
        for (auto it = fs::recursive_directory_iterator(source.inputPath, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            if (broadAllowed && shouldIncludeBroadScan(it->path())) addFile(it->path());
        }
    }

    std::sort(raw.begin(), raw.end());
    std::vector<PreservedEvidenceFile> out;
    for (const auto& p : raw) {
        PreservedEvidenceFile f;
        f.originalPath = p;
        f.relativePath = safeRelativeString(source.inputPath, p);
        if (f.relativePath.empty() || f.relativePath.rfind("..", 0) == 0) f.relativePath = p.filename().string();
        f.fileRole = fileRoleFor(p);
        f.sizeBytes = fs::file_size(p, ec);
        if (ec) f.sizeBytes = 0;
        out.push_back(std::move(f));
    }
    log.info("Identified " + std::to_string(out.size()) + " Spotlight evidence files for preservation.");
    return out;
}

PreservationResult EvidencePreserver::preserve(const RunOptions& opt,
                                               const EvidenceSource& source,
                                               const std::vector<StoreInfo>& stores,
                                               CaseDatabase& db,
                                               Logger& log) const {
    PreservationResult res;
    res.attempted = true;
    res.status = "NOT_STARTED";
    res.integrityStatus = "NOT_RUN";

    auto files = identifyFiles(opt, source, stores, log);
    if (files.empty()) {
        res.status = "NO_SPOTLIGHT_FILES_IDENTIFIED";
        res.message = "No Spotlight evidence files were identified for preservation.";
        log.warn(res.message);
        return res;
    }

    const fs::path caseBase = !opt.caseDir.empty() ? opt.caseDir : opt.output;
    res.preservationDir = caseBase / "EvidencePreservation" / source.sourceId;
    res.stagingRoot = res.preservationDir / "IdentifiedSpotlightEvidence";
    res.archivePath = res.preservationDir / ("SpotlightEvidence_" + source.sourceId + ".7z");
    res.parseInputRoot = res.stagingRoot;
    fs::create_directories(res.stagingRoot);

    log.info("Copying identified Spotlight evidence into preservation staging folder: " + pathString(res.stagingRoot));
    auto stagingRelativePath = [&](const PreservedEvidenceFile& f) -> fs::path {
        const std::string relNorm = normalizeSlash(f.relativePath);
        const std::string relLower = toLower(relNorm);
        const std::string inputLeaf = toLower(source.inputPath.filename().string());
        if (inputLeaf == "store-v2" && relLower.rfind("store-v2/", 0) != 0 && relLower != "store-v2") {
            return fs::path("Store-V2") / fs::path(f.relativePath);
        }
        return fs::path(f.relativePath);
    };
    for (auto& f : files) {
        f.preservedPath = res.stagingRoot / stagingRelativePath(f);
        fs::create_directories(f.preservedPath.parent_path());
        std::error_code ec;
        fs::copy_file(f.originalPath, f.preservedPath, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            log.warn("Unable to copy preservation file: " + pathString(f.originalPath) + " :: " + ec.message());
            continue;
        }
        try { f.sha256 = sha256File(f.originalPath); } catch (const std::exception& ex) { log.warn(ex.what()); }
        res.totalOriginalBytes += f.sizeBytes;
        ++res.fileCount;
    }

    writeManifest(res.preservationDir / "hash_manifest.csv", files);
    res.preserved = true;
    res.status = "STAGED";

    const fs::path sevenZip = opt.sevenZipPath.empty() ? find7z() : opt.sevenZipPath;
    const fs::path addLog = res.preservationDir / "7z_add.log";
    const fs::path testLog = res.preservationDir / "7z_test.log";
    const auto stagedCounts = countRegularFilesAndBytes(res.stagingRoot);
    log.info("Creating 7z preservation archive: " + pathString(res.archivePath));
    log.info("7z executable candidate: " + pathString(sevenZip));
    log.info("7z archive source directory: " + pathString(res.stagingRoot));
    log.info("7z archive destination: " + pathString(res.archivePath));
    log.info("7z archive staged source count: files=" + std::to_string(stagedCounts.second) + " bytes=" + std::to_string(stagedCounts.first));
    log.info("7z archive log: " + pathString(addLog));

    const std::string addArgs = std::string("a -t7z -mx=9 -r ") + quoteArg(res.archivePath) + " " + quoteArg(std::string(".\\*"));
    const auto addRun = runProcessToLog(sevenZip, addArgs, res.stagingRoot, addLog);
    int addRc = addRun.exitCode;
    if (!addRun.started) {
        log.warn("7z archive process did not start: " + addRun.message);
    }
    log.info("7z archive create return code: " + std::to_string(addRc));

    if ((addRc == 0 || addRc == 1) && fs::exists(res.archivePath)) {
        res.archiveCreated = true;
        res.archiveSizeBytes = fs::file_size(res.archivePath);
        try { res.archiveSha256 = sha256File(res.archivePath); } catch (const std::exception& ex) { log.warn(ex.what()); }
        log.info("7z verification log: " + pathString(testLog));
        const std::string testArgs = std::string("t ") + quoteArg(res.archivePath);
        const auto testRun = runProcessToLog(sevenZip, testArgs, res.preservationDir, testLog);
        int testRc = testRun.exitCode;
        if (!testRun.started) {
            log.warn("7z verification process did not start: " + testRun.message);
        }
        log.info("7z archive test return code: " + std::to_string(testRc));
        res.archiveVerified = (testRc == 0);
        res.status = "PRESERVED_ARCHIVE_CREATED";
        res.integrityStatus = res.archiveVerified ? "PASS" : "VERIFY_FAILED";
        res.message = res.archiveVerified ? "7z archive created from preservation staging directory and verified." : "7z archive was created but verification failed.";
    } else {
        res.status = "STAGED_BUT_ARCHIVE_NOT_CREATED";
        res.integrityStatus = "NOT_RUN";
        res.message = "7z archive was not created. Evidence remains copied in the preservation staging folder.";
        log.warn(res.message);
    }

    writePreservationJson(res.preservationDir / "preservation_manifest.json", res, source);
    db.begin();
    try {
        insertPreservationRows(db, source, res, files);
        db.commit();
    } catch (...) {
        db.rollbackNoThrow();
        throw;
    }
    log.info("Preservation status: " + res.status + "; files=" + std::to_string(res.fileCount));
    return res;
}

} // namespace vestigant::spotlight
