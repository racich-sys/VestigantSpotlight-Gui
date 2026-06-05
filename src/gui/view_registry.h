#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace vestigant::spotlight {

enum class ViewPlatform {
    MacOS,
    iOS,
    Shared,
    Auto
};

struct ViewSpec {
    const wchar_t* displayName;
    const char* tableName;
    std::vector<const char*> columns;
    std::vector<const char*> searchColumns;
    const char* orderBy;
    ViewPlatform platform;
    int sortPriority;
    const wchar_t* helpText;

    ViewSpec(const wchar_t* display,
             const char* table,
             std::initializer_list<const char*> cols,
             std::initializer_list<const char*> search,
             const char* order,
             ViewPlatform p = ViewPlatform::Auto,
             int priority = -1,
             const wchar_t* help = nullptr);
};

const std::vector<ViewSpec>& views();
std::wstring viewHelpText(const ViewSpec& v);
ViewPlatform inferViewPlatform(const wchar_t* displayName);
int inferViewSortPriority(const wchar_t* displayName, ViewPlatform platform);

} // namespace vestigant::spotlight
