#include "app/app_runner.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace vestigant::spotlight;
namespace fs = std::filesystem;

fs::path testIoPath(const fs::path& p) {
#if defined(_WIN32)
    std::error_code ec;
    fs::path abs = fs::absolute(p, ec);
    if (ec) abs = p;
    std::wstring w = abs.wstring();
    if (w.rfind(LR"(\\?\)", 0) == 0) return abs;
    if (w.rfind(LR"(\\)", 0) == 0) return fs::path(LR"(\\?\UNC\)" + w.substr(2));
    return fs::path(LR"(\\?\)" + w);
#else
    return p;
#endif
}

bool testExists(const fs::path& p) {
    std::error_code ec;
    if (fs::exists(p, ec)) return true;
    ec.clear();
    return fs::exists(testIoPath(p), ec);
}

int main(int argc, char** argv) {
    fs::path out = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "VestigantSpotlight_tests";
    RunOptions opt;
    opt.mode = "self-test";
    opt.output = out;
    opt.verbose = false;
    auto rr = runApplication(opt);
    bool ok = rr.exitCode == 0 && rr.artifactCount == 5 && rr.usageCount >= 1 && rr.orphanCandidateCount == 0
        && testExists(out / "case" / "exports" / "usage_evidence.csv")
        && testExists(out / "case" / "exports" / "object_usage_summary.csv")
        && testExists(out / "case" / "exports" / "upload_samples" / "object_usage_summary_focus.csv")
        && testExists(out / "case" / "investigator_dashboard.html");
    if (!ok) {
        std::cerr << "Self-test failed. artifacts=" << rr.artifactCount << " usage=" << rr.usageCount << " orphan=" << rr.orphanCandidateCount << " exit=" << rr.exitCode << "\n";
        return 1;
    }
    std::cout << "Self-test passed: " << out << "\n";
    return 0;
}
