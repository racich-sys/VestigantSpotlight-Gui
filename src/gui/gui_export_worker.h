#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <set>

#include "gui/view_registry.h"

namespace vestigant::spotlight {

struct GuiViewExportRequest {
    std::wstring dbPath;
    ViewSpec view{L"", "", {}, {}, "", ViewPlatform::Shared, -1, nullptr};
    std::string search;
    int filterColumn = -1;
    std::string filterValue;
    std::string orderBy;
    int page = 0;
    int pageSize = 1000;
    std::set<long long> checkedArtifactIds;
    std::wstring outPath;
};

struct GuiExportResult {
    bool ok = false;
    std::wstring message;
    std::size_t rows = 0;
    std::wstring manifestPath;
};

class GuiExportWorker {
public:
    static GuiExportResult exportCurrentPage(const GuiViewExportRequest& request);
    static GuiExportResult exportFilteredView(const GuiViewExportRequest& request);

    static GuiExportResult exportCheckedArtifacts(const std::wstring& dbPath,
                                                  const std::vector<long long>& artifactIds,
                                                  const std::wstring& outPath);

    static GuiExportResult exportTaggedArtifacts(const std::wstring& dbPath,
                                                 long long tagId,
                                                 const std::wstring& outPath);
};

} // namespace vestigant::spotlight
