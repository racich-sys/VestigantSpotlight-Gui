#include "app/app_runner.h"
#include "core/app_info.h"
#include "core/path_utils.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <exception>
#include <sstream>
#include <cstdlib>
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
#include <cstdlib>
#endif

using namespace vestigant::spotlight;

namespace {
RunOptions* gOptForCrash = nullptr;
void writeCrashFile(const std::string& message) {
    try {
        fs::path base = (gOptForCrash && !gOptForCrash->output.empty()) ? gOptForCrash->output : fs::current_path();
        fs::create_directories(base / "logs");
        std::ofstream out(base / "logs" / "FATAL_CRASH.txt", std::ios::app | std::ios::binary);
        out << nowUtc() << " " << message << "\n" << std::flush;
    } catch (...) {}
}
#ifdef _WIN32
LONG WINAPI cliUnhandledFilter(EXCEPTION_POINTERS* ep) {
    std::ostringstream os; os << "Unhandled exception code=0x" << std::hex << (ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0);
    writeCrashFile(os.str());
    return EXCEPTION_EXECUTE_HANDLER;
}
void cliInvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    writeCrashFile("Windows CRT invalid-parameter handler invoked");
}
#endif
void cliTerminateHandler() { writeCrashFile("std::terminate called"); std::abort(); }

void usage() {
    std::cout << appTitle() << "\n\n"
              << "Usage:\n"
              << "  VestigantSpotlightCli --mode discover --profile macos|ios|auto --input <raw-spotlight-root> --out <case-folder> [--full-scan]\n"
              << "  VestigantSpotlightCli --mode source-probe --profile macos|ios|auto --input <folder|zip|aff4|img|dd|raw> --out <case-folder> [--full-scan] [--skip-container-hash] [--force-container-hash] [--reader-tools <folder>] [--strict-single-aff4] [--enable-aff4-dynamic-probe] [--enable-aff4-stream-inventory]\n"
              << "  VestigantSpotlightCli --mode diagnostics --profile macos|ios|auto --input <raw-spotlight-root> --out <case-folder> [--preserve] [--full-scan] [--force-container-hash] [--skip-container-hash] [--max-native-records N] [--max-native-blocks N] [--export-profile minimal|investigator|diagnostics|support|full] [--no-csv-exports]\n"
              << "  VestigantSpotlightCli --mode run --profile macos|ios|auto --input <raw-spotlight-root> --out <case-folder> [--7z <7z.exe>] [--reuse-ios-cache <completed-case-folder>] [--decode-core-native-values] [--force-container-hash] [--skip-container-hash] [--experimental-full-native-values] [--max-native-records N] [--max-native-blocks N] [--export-profile minimal|investigator|diagnostics|support|full] [--no-csv-exports]\n"
              << "  VestigantSpotlightCli --full-validation --input <raw-spotlight-root-or-zip> --out <case-folder>\n"
              << "Workflow:\n"
              << "  identify Spotlight store.db/.store.db evidence -> preserve static case copy -> native C++ decode into SQLite -> enrich -> review/export.\n\n"
              << "Notes:\n"
              << "  --mode diagnostics skips 7z preservation by default for fast parser diagnostics and enables safe core native probes.\n"
              << "  --preserve can be added to diagnostics mode when archive-first testing is needed.\n"
              << "  Active filesystem comparison uses in-case iOS FFS exact-path lookup when available. --evidence-root is accepted for compatibility but direct evidence-root comparison remains pending in V1.6.28.\n"
              << "  Stable native header-only parsing is the default in run mode. Use --decode-core-native-values to test safe native string/path probe decoding.\n"
              << "  --full-validation is an operator-safe shortcut for --mode run --profile auto --experimental-full-native-values --export-profile investigator --verbose.\n"
              << "  --diagnostic-full-native-exports enables support/diagnostic CSV exports that are intentionally skipped in normal investigator runs.\n"
              << "  --diagnostic-full-native-db enables full raw native key/value persistence; use only for bounded support runs because iOS CoreSpotlight dbStr parsing can create millions of rows.\n  --aff4-apfs-diagnostic-outputs writes large AFF4/APFS structural probe CSVs; normal source-probe suppresses those to prioritize staging/copy-out I/O.\n"
              << "  Normal iOS Spotlight-first runs do not materialize full FFS inventory rows or broad app DB parsed-record rows into the active case DB; use --materialize-ios-ffs-inventory and --materialize-ios-app-db-records only for bounded support/correlation runs.\n"
              << "  --reuse-ios-cache reuses a prior completed iOS ZIP intake/cache folder to avoid relisting a very large FFS ZIP; source path/size are recorded in source_cache_manifest.json and mismatches are warned.\n  --mode source-probe registers and reports source readiness. AFF4/APFS is prioritized for image-backed file inventory and active-file comparison readiness; bounded Spotlight Store-V2 copy-out/staging/parsing validation is active for supported guarded layouts, while general full-container filesystem extraction remains staged. AFF4 source-probe defers full-container hashing by default for development speed; use --force-container-hash when an evidentiary hash is needed or --skip-container-hash explicitly for any large container. Use --reader-tools to point at aff4-cpp-lite/libaff4, fsapfs, and fshfs helper binaries. aff4-cpp-lite is the primary AFF4 direction; WinPmem/c-aff4 aff4imager remains fallback only. No full AFF4-to-RAW export is performed by default. Dynamic libaff4 open/read and external aff4imager stream listing are opt-in because they may cause third-party readers to open other AFF4-related files from the same evidence drive. Use --strict-single-aff4 to require one explicit .aff4 file and suppress reader-driven discovery.\n";
}
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(cliUnhandledFilter);
    _set_invalid_parameter_handler(cliInvalidParameterHandler);
#endif
    std::set_terminate(cliTerminateHandler);
    RunOptions opt;
    gOptForCrash = &opt;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            auto need = [&](const std::string& name) -> std::string {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for " + name);
                return argv[++i];
            };
            if (a == "--help" || a == "-h" || a == "/?") { usage(); return 0; }
            else if (a == "--version" || a == "-V") { std::cout << appTitle() << "\n"; return 0; }
            else if (a == "--full-validation" || a == "--investigator-full-validation") {
                opt.mode = "run";
                opt.profile = "auto";
                opt.experimentalFullNativeValues = true;
                opt.exportProfile = "investigator";
                opt.verbose = true;
            }
            else if (a == "--source-probe" || a == "--source-intake") opt.mode = "source-probe";
            else if (a == "--mode") opt.mode = need(a);
            else if (a == "--profile") opt.profile = need(a);
            else if (a == "--input") opt.input = need(a);
            else if (a == "--out" || a == "--output") opt.output = need(a);
            else if (a == "--case") opt.caseDir = need(a);
            else if (a == "--evidence-root") opt.evidenceRoot = need(a);
            else if (a == "--reuse-ios-cache" || a == "--reuse-ios-intake" || a == "--ios-cache") opt.reuseIosCache = need(a);
            else if (a == "--case-name") opt.caseName = need(a);
            else if (a == "--case-number") opt.caseNumber = need(a);
            else if (a == "--subject") opt.subjectName = need(a);
            else if (a == "--company") opt.company = need(a);
            else if (a == "--investigator") opt.investigator = need(a);
            else if (a == "--7z" || a == "--sevenzip") opt.sevenZipPath = need(a);
            else if (a == "--no-preserve") { opt.preserveEvidence = false; opt.preserveEvidenceExplicit = true; }
            else if (a == "--preserve") { opt.preserveEvidence = true; opt.preserveEvidenceExplicit = true; }
            else if (a == "--decode-core-native-values") opt.decodeCoreNativeValues = true;
            else if (a == "--experimental-full-native-values") opt.experimentalFullNativeValues = true;
            else if (a == "--diagnostic-full-native-db" || a == "--diagnostic-full-native-kv") { opt.diagnosticFullNativeDb = true; opt.exportProfile = "diagnostics"; }
            else if (a == "--aff4-apfs-diagnostic-outputs" || a == "--diagnostic-apfs-csvs") { opt.aff4ApfsDiagnosticOutputs = true; }
            else if (a == "--materialize-ios-ffs-inventory" || a == "--full-ios-ffs-inventory-db") { opt.materializeIosFfsInventory = true; }
            else if (a == "--materialize-ios-app-db-records" || a == "--full-ios-app-db-records") { opt.materializeIosAppDbRecords = true; }
            else if (a == "--materialize-ios-support-db") { opt.materializeIosFfsInventory = true; opt.materializeIosAppDbRecords = true; }
            else if (a == "--max-native-records" || a == "--native-item-limit") { opt.maxNativeRecords = static_cast<std::size_t>(std::stoull(need(a))); opt.maxNativeRecordsExplicit = true; }
            else if (a == "--max-native-blocks" || a == "--native-block-limit") { opt.maxNativeBlocks = static_cast<std::size_t>(std::stoull(need(a))); }
            else if (a == "--db-size-guardrail-gb") { const auto gb = std::stoull(need(a)); opt.dbSizeGuardrailBytes = static_cast<std::uintmax_t>(gb) * 1024ull * 1024ull * 1024ull; }
            else if (a == "--disable-db-size-guardrail") { opt.dbSizeGuardrailBytes = 0; }
            else if (a == "--export-profile") { opt.exportProfile = need(a); }
            else if (a == "--no-csv-exports") { opt.suppressCsvExports = true; opt.exportProfile = "none"; }
            else if (a == "--diagnostic-full-native-exports") { opt.exportProfile = "diagnostics"; }
            else if (a == "--full-scan") opt.fullScan = true;
            else if (a == "--skip-container-hash") opt.skipContainerHash = true;
            else if (a == "--force-container-hash" || a == "--hash-container") opt.forceContainerHash = true;
            else if (a == "--reader-tools") opt.readerToolsDir = need(a);
            else if (a == "--strict-single-aff4") opt.strictSingleAff4 = true;
            else if (a == "--enable-aff4-dynamic-probe") opt.enableAff4DynamicProbe = true;
            else if (a == "--disable-aff4-dynamic-probe") opt.enableAff4DynamicProbe = false;
            else if (a == "--enable-aff4-stream-inventory") opt.enableAff4StreamInventory = true;
            else if (a == "--disable-aff4-stream-inventory") opt.enableAff4StreamInventory = false;
            else if (a == "--verbose") opt.verbose = true;
            else if (a == "--stage1-parser" || a == "--support-parser" || a == "--python-exe") {
                throw std::runtime_error(a + " is no longer used. This version decodes raw Spotlight stores natively in C++.");
            }
            else throw std::runtime_error("Unknown argument: " + a);
        }
        std::string error;
        if (!validateRunOptions(opt, error)) {
            std::cerr << "ERROR: " << error << "\n\n";
            usage();
            return 1;
        }
        auto result = runApplication(opt);
        for (const auto& m : result.messages) std::cout << m << "\n";
        std::cout << "summary: sources=" << result.sourceCount
                  << " store_groups=" << result.storeCount
                  << " valid_store_groups=" << result.validStoreCount
                  << " database_candidates=" << result.databaseCandidateCount
                  << " valid_database_candidates=" << result.validDatabaseCandidateCount
                  << " selected_databases=" << result.selectedParserDatabaseCount
                  << " artifacts=" << result.artifactCount
                  << " usage=" << result.usageCount
                  << " timeline=" << result.timelineCount
                  << " orphan_deleted_candidates=" << result.orphanCandidateCount << "\n";
        return result.exitCode;
    } catch (const std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << "\n";
        usage();
        return 1;
    }
}
