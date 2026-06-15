#if !defined(_WIN32)
#error This file is Windows-only.
#endif

#include "app/app_runner.h"
#include "core/app_info.h"
#include "db/sqlite_compat.h"
#include "db/case_db.h"
#include "gui/view_registry.h"
#include "gui/gui_export_worker.h"
#include "gui/gui_view_helpers.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#ifndef _RICHEDIT_VER
#define _RICHEDIT_VER 0x0500
#endif
#include <richedit.h>
#include <shlobj.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <fstream>
#include <exception>
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <cstdlib>
#include <cstring>
#include <set>
#include <atomic>
#include <map>
#include <mutex>
#include <initializer_list>
#include <chrono>
#include <utility>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "winsqlite3.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace vestigant::spotlight;

namespace {
HINSTANCE gInst{};
HWND gTab{};
HWND gInput{}, gOut{}, gEvidenceRoot{}, gSevenZip{}, gSourceType{}, gProfile{}, gMode{}, gCaseName{}, gCaseNumber{}, gCompany{}, gInvestigator{}, gLog{}, gRun{}, gCancelIngestButton{}, gIngestStatus{}, gIngestProgress{}, gLogo{}, gBrandTitle{}, gBrandSubtitle{};
HWND gVerbose{}, gExportProfile{}, gSuppressCsvExports{};
HWND gCaseDbPath{}, gBrowseCase{}, gBrowseOut{}, gBrowseInput{}, gBrowseRoot{}, gBrowse7z{}, gOpenCase{}, gSaveCaseInfo{}, gCaseAutosaveStatus{}, gOpenDashboard{}, gOpenReviewIndex{}, gOpenCaseFolder{}, gOpenUploadFolder{}, gOpenLogsFolder{}, gReviewViewProfile{}, gViewSetSave{}, gViewSetReset{}, gViewSetHide{}, gViewSetUp{}, gViewSetDown{}, gManageTags{}, gReviewView{}, gSearch{}, gPageSize{}, gRefresh{}, gCancelLoad{}, gReviewBusy{}, gPrev{}, gNext{}, gExportPage{}, gExportFiltered{}, gExportVisible{}, gExportChecked{}, gClearChecked{}, gReviewSummary{}, gList{}, gRowDetailsSplitter{}, gRowDetailsLabel{}, gRowDetails{};

// Forward declaration required because custom view-set helpers are defined before the review summary helper.
void setReviewSummary(const std::wstring& s);
void setDetailsPaneMessage(const std::wstring& message);
void worker();
HWND gTagList{}, gTagName{}, gTagNote{}, gTaggedList{}, gTagSummary{};
HWND gIosStatus{}, gIosReadiness{}, gIosPlan{};
std::vector<HWND> gProcessControls;
std::vector<HWND> gReviewControls;
std::vector<HWND> gTagControls;
std::vector<HWND> gIosControls;
std::vector<size_t> gVisibleViews;
std::wstring gCrashCaseDir;
std::wstring gOpenedCaseDb;
int gCurrentPage = 0;
long long gCurrentTotal = 0;
bool gCurrentHasNext = false;
bool gReviewLoadInProgress = false;
int gSortColumn = -1;
bool gSortDescending = false;
int gFilterColumn = -1;
std::string gFilterValue;
int gContextColumn = -1;
int gContextRow = -1;
std::set<long long> gCheckedArtifactIds;
bool gBulkCheckUpdate = false;
std::vector<std::string> gCurrentRowArtifactIds;
std::vector<std::vector<std::wstring>> gCurrentReviewRowsW;
std::vector<std::wstring> gCurrentVisibleTagCellsW;
std::mutex gReviewStateMutex;
std::atomic<unsigned long long> gReviewRequestSeq{0};
std::atomic_bool gShuttingDown{false};
std::atomic_bool gExportPageActive{false};
std::atomic_bool gExportFilteredActive{false};
std::atomic_bool gExportVisibleActive{false};
std::atomic_bool gExportCheckedActive{false};
std::atomic_bool gExportTaggedActive{false};
std::thread gReviewThread;
std::mutex gExportThreadsMutex;
std::vector<std::thread> gExportThreads;
std::mutex gIngestThreadMutex;
std::thread gIngestThread;
std::atomic_bool gIngestActive{false};
std::atomic_bool gCancelIngestRequested{false};
HFONT gUiFont{};
bool gCaseInfoDirty = false;
bool gIosReviewMode = false;
int gActiveTabIndex = 0;
HWND gReviewViewTooltip{};
HMODULE gRichEditModule{};
const wchar_t* gRichEditClassName = L"EDIT";
bool gRichEditAvailable = false;
HBITMAP gLogoBitmap{};
int gReviewViewProfileMode = 0; // 0 Recommended V1, 1 Timeline, 2 Text, 3 App/KnowledgeC, 4 Diagnostics, 5 Show All, 6 Custom
int gReviewDetailsPaneHeight = 260;
bool gReviewDetailsSplitterDragging = false;
int gReviewDetailsSplitterDragStartY = 0;
int gReviewDetailsPaneHeightAtDragStart = 260;
std::wstring gReviewViewTooltipText;
int gReviewViewHoverListIndex = -1;
struct ContextTagCommand { UINT commandId; long long tagId; int operation; };
std::vector<ContextTagCommand> gContextTagCommands;

struct ReviewCancelContext {
    unsigned long long requestId = 0;
};

int sqliteReviewProgressCancel(void* userData) {
    auto* ctx = reinterpret_cast<ReviewCancelContext*>(userData);
    if (!ctx) return 0;
    return (gShuttingDown.load() || ctx->requestId != gReviewRequestSeq.load()) ? 1 : 0;
}

void cancelAndJoinReviewThreadNoThrow() {
    try {
        ++gReviewRequestSeq;
        if (gReviewThread.joinable()) gReviewThread.join();
    } catch (...) {
    }
}

constexpr int ID_RUN = 1001, ID_CANCEL_INGEST = 1007, ID_BROWSE_INPUT = 1002, ID_BROWSE_OUT = 1003, ID_BROWSE_ROOT = 1004, ID_BROWSE_7Z = 1005, ID_SOURCE_TYPE = 1006, ID_SUPPRESS_CSV_EXPORTS = 1008;
constexpr int ID_BROWSE_CASE = 1101, ID_OPEN_CASE = 1102, ID_REVIEW_REFRESH = 1103, ID_REVIEW_PREV = 1104, ID_REVIEW_NEXT = 1105, ID_EXPORT_PAGE = 1106, ID_EXPORT_FILTERED = 1113, ID_REVIEW_CANCEL_LOAD = 1114, ID_OPEN_CASE_FOLDER = 1115, ID_OPEN_UPLOAD_FOLDER = 1116, ID_OPEN_LOGS_FOLDER = 1117, ID_EXPORT_VISIBLE_VIEWS = 1124;
constexpr int ID_OPEN_DASHBOARD = 1107, ID_OPEN_REVIEW_INDEX = 1108, ID_REVIEW_VIEW = 1109, ID_SAVE_CASE_INFO = 1110, ID_EXPORT_CHECKED = 1111, ID_CLEAR_CHECKED = 1112, ID_REVIEW_VIEW_PROFILE = 1118, ID_VIEWSET_SAVE = 1119, ID_VIEWSET_RESET = 1120, ID_VIEWSET_HIDE = 1121, ID_VIEWSET_UP = 1122, ID_VIEWSET_DOWN = 1123;
constexpr int ID_ADD_TAG = 1301, ID_DELETE_TAG = 1302, ID_APPLY_TAG = 1303, ID_REMOVE_TAG = 1304, ID_SAVE_NOTE = 1305, ID_SHOW_TAGGED = 1306, ID_EXPORT_TAGGED = 1307, ID_REFRESH_TAGS = 1308;
constexpr int ID_CTX_SORT_ASC = 2101, ID_CTX_SORT_DESC = 2102, ID_CTX_FILTER_SEARCH = 2103, ID_CTX_CLEAR_FILTER = 2104;
constexpr int ID_CTX_TOGGLE_CHECK = 2110, ID_CTX_APPLY_TAG_ROW = 2111, ID_CTX_APPLY_TAG_CHECKED = 2112, ID_CTX_REMOVE_TAG_ROW = 2113, ID_CTX_REMOVE_TAG_CHECKED = 2114, ID_CTX_CLEAR_CHECKED = 2115;
constexpr int ID_CTX_CHECK_SELECTED = 2116, ID_CTX_UNCHECK_SELECTED = 2117, ID_CTX_TOGGLE_SELECTED = 2118, ID_CTX_APPLY_TAG_SELECTED = 2119, ID_CTX_REMOVE_TAG_SELECTED = 2120;
constexpr int ID_CTX_MANAGE_TAGS = 2121, ID_IOS_OPEN_READINESS = 1401, ID_IOS_OPEN_PLAN = 1402, ID_IOS_OPEN_STORE_SUMMARY = 1403, ID_IOS_OPEN_STRING_VALUES = 1404, ID_IOS_OPEN_STRING_SUMMARY = 1405, ID_IOS_OPEN_INDEX_TIMELINE = 1406, ID_IOS_OPEN_ARTIFACTS = 1407, ID_IOS_OPEN_KEYWORD_VALUES = 1408, ID_IOS_OPEN_FFS_INVENTORY = 1409, ID_IOS_OPEN_APP_DATABASES = 1410, ID_IOS_OPEN_REFERENCED_PATHS = 1411, ID_IOS_OPEN_MISSING_FFS = 1412, ID_IOS_OPEN_DB_RESIDENCY = 1413, ID_IOS_OPEN_RESIDENCY_SUMMARY = 1414, ID_IOS_OPEN_PARSED_APP_RECORDS = 1415, ID_IOS_OPEN_PARSED_APP_SUMMARY = 1416;
constexpr int ID_CASE_NAME_EDIT = 1501, ID_CASE_NUMBER_EDIT = 1502, ID_INVESTIGATOR_EDIT = 1503, ID_COMPANY_EDIT = 1504, ID_CASE_LOCATION_EDIT = 1505, ID_CASE_DB_EDIT = 1506;
constexpr UINT_PTR ID_AUTOSAVE_TIMER = 5001;
constexpr UINT ID_CTX_TAG_BASE = 30000;
constexpr int TAG_OP_APPLY_ROW = 1, TAG_OP_REMOVE_ROW = 2, TAG_OP_APPLY_SELECTED = 3, TAG_OP_REMOVE_SELECTED = 4, TAG_OP_APPLY_CHECKED = 5, TAG_OP_REMOVE_CHECKED = 6;
constexpr int WM_APPEND_LOG = WM_APP + 1;
constexpr int WM_SET_INGEST_STATUS = WM_APP + 2;
constexpr int WM_SET_INGEST_PROGRESS = WM_APP + 3;
constexpr int WM_REVIEW_PAGE_RESULT = WM_APP + 4;
constexpr int WM_CLEAR_PROCESS_LOG = WM_APP + 5;
constexpr int WM_EXPORT_PAGE_RESULT = WM_APP + 6;
constexpr int WM_EXPORT_FILTERED_RESULT = WM_APP + 7;
constexpr int WM_EXPORT_CHECKED_RESULT = WM_APP + 8;
constexpr int WM_EXPORT_TAGGED_RESULT = WM_APP + 9;
constexpr int WM_EXPORT_VISIBLE_RESULT = WM_APP + 10;
constexpr int kReviewDetailsMinHeight = 120;
constexpr int kReviewDetailsDefaultHeight = 260;
constexpr int kReviewDetailsSplitterHeight = 8;

struct ReviewPageResult {
    unsigned long long requestId = 0;
    int viewIndex = 0;
    int page = 0;
    int pageSize = 0;
    bool hasNext = false;
    unsigned long long elapsedMs = 0;
    std::wstring summary;
    std::wstring error;
    std::vector<std::string> artifactIds;
    std::vector<std::vector<std::string>> rows;
    std::map<std::string, std::wstring> visibleTags;
};

std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}
std::string narrow(const std::wstring& w) {
    if (w.empty()) return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}
void writeGuiCrashFile(const std::wstring& message) {
    try {
        std::wstring base = gCrashCaseDir.empty() ? L"." : gCrashCaseDir;
        CreateDirectoryW((base + L"\\logs").c_str(), nullptr);
        std::wofstream out(base + L"\\logs\\FATAL_CRASH.txt", std::ios::app);
        out << L"Fatal GUI crash: " << message << L"\n";
    } catch (...) {}
}
LONG WINAPI unhandledFilter(EXCEPTION_POINTERS* ep) {
    std::wostringstream os; os << L"Unhandled exception code=0x" << std::hex << (ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0);
    writeGuiCrashFile(os.str());
    return EXCEPTION_EXECUTE_HANDLER;
}
void terminateHandler() { writeGuiCrashFile(L"std::terminate called"); ExitProcess(3); }

std::wstring getText(HWND h) {
    const int len = GetWindowTextLengthW(h);
    std::wstring s(static_cast<size_t>(len + 1), L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    if (!s.empty() && s.back() == L'\0') s.pop_back();
    return s;
}
void setText(HWND h, const std::wstring& s) { SetWindowTextW(h, s.c_str()); }
std::wstring logTimestampPrefix() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buf[64]{};
    swprintf_s(buf, L"%02u:%02u:%02u  ", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

void appendLog(const std::wstring& s) {
    if (!gLog) return;
    const int len = GetWindowTextLengthW(gLog);
    SendMessageW(gLog, EM_SETSEL, len, len);
    const std::wstring line = logTimestampPrefix() + s + L"\r\n";
    SendMessageW(gLog, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageW(gLog, EM_SCROLLCARET, 0, 0);
}
void clearProcessLog() { if (gLog) SetWindowTextW(gLog, L""); }
void postLog(const std::wstring& s) { if (gLog) PostMessageW(GetParent(gLog), WM_APPEND_LOG, 0, reinterpret_cast<LPARAM>(new std::wstring(s))); }
void postClearProcessLog() { if (gLog) PostMessageW(GetParent(gLog), WM_CLEAR_PROCESS_LOG, 0, 0); }
void postStatus(const std::wstring& s) { if (gIngestStatus) PostMessageW(GetParent(gIngestStatus), WM_SET_INGEST_STATUS, 0, reinterpret_cast<LPARAM>(new std::wstring(s))); }
void postProgress(int percent) { if (gIngestProgress) PostMessageW(GetParent(gIngestProgress), WM_SET_INGEST_PROGRESS, static_cast<WPARAM>(percent), 0); }
std::wstring browseFolder(HWND owner) {
    BROWSEINFOW bi{}; bi.hwndOwner = owner; bi.lpszTitle = L"Select folder"; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return L"";
    wchar_t path[MAX_PATH]{}; SHGetPathFromIDListW(pidl, path); CoTaskMemFree(pidl); return path;
}
std::wstring browseExe(HWND owner) {
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Executable files (*.exe)\0*.exe\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&ofn) ? std::wstring(file) : L"";
}
std::wstring browseSqlite(HWND owner) {
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Vestigant case database\0VestigantSpotlight.case.sqlite\0SQLite databases (*.sqlite;*.db)\0*.sqlite;*.db\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&ofn) ? std::wstring(file) : L"";
}
std::wstring browseZip(HWND owner) {
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"ZIP evidence archives (*.zip)\0*.zip\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&ofn) ? std::wstring(file) : L"";
}
std::wstring browseEvidenceContainer(HWND owner, int sourceType) {
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner;
    if (sourceType == 2) ofn.lpstrFilter = L"AFF4 containers (*.aff4)\0*.aff4\0All files (*.*)\0*.*\0";
    else if (sourceType == 3) ofn.lpstrFilter = L"Raw disk images (*.img;*.dd;*.raw)\0*.img;*.dd;*.raw\0All files (*.*)\0*.*\0";
    else ofn.lpstrFilter = L"Evidence containers (*.zip;*.aff4;*.img;*.dd;*.raw)\0*.zip;*.aff4;*.img;*.dd;*.raw\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&ofn) ? std::wstring(file) : L"";
}
std::wstring saveCsv(HWND owner) {
    wchar_t file[MAX_PATH]{};
    wcscpy_s(file, L"current_page.csv");
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"csv";
    return GetSaveFileNameW(&ofn) ? std::wstring(file) : L"";
}
HWND label(HWND parent, const wchar_t* text, int x, int y, int w, int h) { return CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, nullptr, gInst, nullptr); }
HWND edit(HWND parent, int x, int y, int w, int h, const wchar_t* text = L"") { return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, x, y, w, h, parent, nullptr, gInst, nullptr); }
HWND button(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) { return CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), gInst, nullptr); }
void applyUiFont(HWND h) { if (h && gUiFont) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(gUiFont), TRUE); }
HWND addProcess(HWND h) { applyUiFont(h); gProcessControls.push_back(h); return h; }
HWND addReview(HWND h) { applyUiFont(h); gReviewControls.push_back(h); return h; }
HWND addTag(HWND h) { applyUiFont(h); gTagControls.push_back(h); return h; }
HWND labelP(HWND parent, const wchar_t* text, int x, int y, int w, int h) { return addProcess(label(parent, text, x, y, w, h)); }
HWND labelR(HWND parent, const wchar_t* text, int x, int y, int w, int h) { return addReview(label(parent, text, x, y, w, h)); }
HWND editP(HWND parent, int x, int y, int w, int h, const wchar_t* text = L"") { return addProcess(edit(parent, x, y, w, h, text)); }
HWND editR(HWND parent, int x, int y, int w, int h, const wchar_t* text = L"") { return addReview(edit(parent, x, y, w, h, text)); }
HWND buttonP(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) { return addProcess(button(parent, text, id, x, y, w, h)); }
HWND openCaseButtonP(HWND parent, int x, int y, int w, int h) { return addProcess(CreateWindowW(L"BUTTON", L"OPEN CASE", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_OWNERDRAW, x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_OPEN_CASE)), gInst, nullptr)); }
HWND buttonR(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) { return addReview(button(parent, text, id, x, y, w, h)); }
HWND labelT(HWND parent, const wchar_t* text, int x, int y, int w, int h) { return addTag(label(parent, text, x, y, w, h)); }
HWND editT(HWND parent, int x, int y, int w, int h, const wchar_t* text = L"") { return addTag(edit(parent, x, y, w, h, text)); }
HWND buttonT(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) { return addTag(button(parent, text, id, x, y, w, h)); }
HWND addIos(HWND h) { applyUiFont(h); gIosControls.push_back(h); return h; }
HWND labelI(HWND parent, const wchar_t* text, int x, int y, int w, int h) { return addIos(label(parent, text, x, y, w, h)); }
HWND buttonI(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) { return addIos(button(parent, text, id, x, y, w, h)); }

std::string csvEscape(const std::string& s) {
    bool quote = s.find_first_of(",\r\n\"") != std::string::npos;
    if (!quote) return s;
    std::string out = "\"";
    for (char c : s) out += (c == '"') ? "\"\"" : std::string(1, c);
    out += "\"";
    return out;
}


void upgradeCaseSchemaForGuiNoThrow(const std::wstring& dbPath) {
    if (dbPath.empty()) return;
    try {
        CaseDatabase db;
        db.open(fs::path(dbPath));
        db.initializeSchema();
        db.close();
    } catch (...) {
        // Some evidence/case locations may be read-only. The GUI will still open the
        // case using existing tables/views; missing newly-added views will simply be
        // hidden by sqliteObjectExists().
    }
}

void ensureInvestigatorUiSchemaNoThrow(sqlite3* db);

int guiSqliteBusyRetryHandler(void*, int count) {
    if (count > 50) return 0;
    Sleep(100);
    return 1;
}

void configureGuiSqliteConnection(sqlite3* db) {
    if (!db) return;
    sqlite3_busy_handler(db, guiSqliteBusyRetryHandler, nullptr);
    sqlite3_exec(db, "PRAGMA temp_store=MEMORY; PRAGMA cache_size=-65536; PRAGMA case_sensitive_like=OFF;", nullptr, nullptr, nullptr);
}

class ReadOnlyDb {
public:
    explicit ReadOnlyDb(const std::wstring& path) {
        std::lock_guard<std::mutex> lock(poolMutex_);
        if (sharedDb_ && currentPath_ == path) {
            db_ = sharedDb_;
            return;
        }
        if (sharedDb_) {
            sqlite3_close_v2(sharedDb_);
            sharedDb_ = nullptr;
            currentPath_.clear();
        }
        const std::string p = narrow(path);
        if (sqlite3_open_v2(p.c_str(), &sharedDb_, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK) {
            configureGuiSqliteConnection(sharedDb_);
            ensureInvestigatorUiSchemaNoThrow(sharedDb_);
            currentPath_ = path;
            db_ = sharedDb_;
            return;
        }
        std::string writeMsg = sharedDb_ ? sqlite3_errmsg(sharedDb_) : "unknown";
        if (sharedDb_) {
            sqlite3_close_v2(sharedDb_);
            sharedDb_ = nullptr;
        }
        if (sqlite3_open_v2(p.c_str(), &sharedDb_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            std::string msg = sharedDb_ ? sqlite3_errmsg(sharedDb_) : writeMsg;
            if (sharedDb_) sqlite3_close_v2(sharedDb_);
            sharedDb_ = nullptr;
            currentPath_.clear();
            throw std::runtime_error("Unable to open case database: " + msg);
        }
        configureGuiSqliteConnection(sharedDb_);
        currentPath_ = path;
        db_ = sharedDb_;
    }
    ~ReadOnlyDb() = default;
    sqlite3* get() const { return db_; }
    static void closePoolNoThrow() {
        std::lock_guard<std::mutex> lock(poolMutex_);
        if (sharedDb_) {
            sqlite3_close_v2(sharedDb_);
            sharedDb_ = nullptr;
        }
        currentPath_.clear();
    }
private:
    sqlite3* db_ = nullptr;
    static inline sqlite3* sharedDb_ = nullptr;
    static inline std::wstring currentPath_;
    static inline std::mutex poolMutex_;
};

void closeReadOnlyDbPoolNoThrow() {
    try { ReadOnlyDb::closePoolNoThrow(); } catch (...) {}
}

bool sqliteObjectExists(sqlite3* db, const char* name) {
    sqlite3_stmt* st = nullptr;
    const char* sql = "SELECT 1 FROM sqlite_master WHERE name=? AND type IN ('table','view') LIMIT 1";
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return ok;
}


void ensureInvestigatorUiSchemaNoThrow(sqlite3* db) {
    if (!db) return;
    const char* sql = R"SQL(
CREATE TABLE IF NOT EXISTS investigator_tags (
  tag_id INTEGER PRIMARY KEY AUTOINCREMENT,
  tag_name TEXT NOT NULL UNIQUE,
  created_utc TEXT,
  notes TEXT
);
CREATE TABLE IF NOT EXISTS artifact_tags (
  artifact_id INTEGER NOT NULL,
  tag_id INTEGER NOT NULL,
  created_utc TEXT,
  PRIMARY KEY(artifact_id, tag_id)
);
CREATE INDEX IF NOT EXISTS idx_artifact_tags_artifact ON artifact_tags(artifact_id);
CREATE INDEX IF NOT EXISTS idx_artifact_tags_tag ON artifact_tags(tag_id);
CREATE TABLE IF NOT EXISTS investigator_notes (
  note_id INTEGER PRIMARY KEY AUTOINCREMENT,
  target_type TEXT NOT NULL,
  target_id TEXT NOT NULL,
  note_text TEXT NOT NULL,
  created_utc TEXT,
  updated_utc TEXT
);
CREATE INDEX IF NOT EXISTS idx_investigator_notes_target ON investigator_notes(target_type, target_id);
CREATE TABLE IF NOT EXISTS gui_checked_artifacts (
  artifact_id INTEGER PRIMARY KEY,
  checked_utc TEXT,
  notes TEXT
);
CREATE TABLE IF NOT EXISTS review_view_preferences (
  platform TEXT,
  view_name TEXT,
  is_visible INTEGER,
  display_order INTEGER,
  preset_name TEXT,
  updated_utc TEXT,
  PRIMARY KEY(platform, view_name, preset_name)
);
DROP VIEW IF EXISTS vw_ios_communication_frequency;
CREATE VIEW IF NOT EXISTS vw_ios_communication_frequency AS
SELECT
    COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key) AS communication_thread_id,
    COUNT(*) AS total_records_in_thread,
    MIN(record_timestamp_utc) AS first_communication_utc,
    MAX(record_timestamp_utc) AS last_communication_utc,
    GROUP_CONCAT(DISTINCT contact_or_participant) AS involved_identities,
    GROUP_CONCAT(DISTINCT app_hint) AS apps_utilized,
    GROUP_CONCAT(DISTINCT record_category) AS record_categories,
    GROUP_CONCAT(DISTINCT parse_status) AS parse_statuses,
    'Partial committed SQLite preview/analysis view. Thread identifiers are taken from item_identifier when available, otherwise contact/source primary key fallback.' AS interpretation_note
FROM ios_app_parsed_records
WHERE (record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_COMMUNICATION_INTENT')
       OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
       OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
       OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%')
  AND COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key) IS NOT NULL
GROUP BY COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key)
ORDER BY total_records_in_thread DESC;

DROP VIEW IF EXISTS vw_ios_communication_existence_evidence;
CREATE VIEW vw_ios_communication_existence_evidence AS
SELECT
  ios_app_record_id,
  source_id,
  database_category,
  app_hint,
  database_name,
  table_name,
  record_category,
  source_primary_key,
  record_timestamp_utc,
  timestamp_source,
  COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), source_primary_key) AS communication_thread_id,
  contact_or_participant AS identity_hint,
  url,
  title,
  file_path,
  substr(text_snippet,1,500) AS text_snippet_sample,
  parse_status,
  provenance,
  CASE
    WHEN record_category='MESSAGE_DELETED_OR_RECOVERABLE' THEN 'deleted_or_recoverable_message_table_or_spotlight_marker'
    WHEN provenance LIKE '%COMMUNICATION_INTENT_STREAM%' OR provenance LIKE '%INTENT_TARGET%' THEN 'knowledgec_or_intent_communication_marker'
    WHEN provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%' THEN 'domain_identifier_or_thread_marker'
    WHEN provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%' THEN 'author_recipient_or_identity_marker'
    WHEN record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS') THEN 'parsed_communication_app_database_record'
    ELSE 'communication_related_parsed_record'
  END AS existence_basis,
  'Presence/frequency support view. Rows show committed parsed records and provenance markers that support existence or activity frequency; review source row before making final conclusions.' AS interpretation_note
FROM ios_app_parsed_records
WHERE record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_COMMUNICATION_INTENT')
   OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
   OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
)SQL" R"SQL(   OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%'
   OR provenance LIKE '%INTENT_TARGET%';

DROP VIEW IF EXISTS vw_ios_spotlight_comms_missing_from_ffs;
CREATE VIEW vw_ios_spotlight_comms_missing_from_ffs AS
SELECT
    s.record_timestamp_utc,
    s.communication_thread_id AS spotlight_thread_id,
    s.identity_hint AS recovered_identity,
    s.app_hint AS source_app,
    s.title,
    s.text_snippet_sample AS recovered_message_text,
    s.file_path AS original_spotlight_path,
    'COMMUNICATION_PRESENT_IN_SPOTLIGHT_NOT_MATCHED_TO_NATIVE_APP_DB' AS forensic_status,
)SQL" R"SQL(    'This communication-related record is present in CoreSpotlight/index-derived data and was not matched to a parsed native app database row by thread/source key. This may indicate deletion, app removal, encryption/inaccessibility, parser coverage limits, or an unmatched identifier; review source provenance before drawing conclusions.' AS interpretation_note
FROM vw_ios_communication_existence_evidence s
WHERE s.database_name LIKE '%index.db%'
  AND s.existence_basis IN ('communication_related_parsed_record','domain_identifier_or_thread_marker','author_recipient_or_identity_marker','deleted_or_recoverable_message_table_or_spotlight_marker')
  AND COALESCE(s.communication_thread_id, '') <> ''
  AND NOT EXISTS (
      SELECT 1
      FROM ios_app_parsed_records app
      WHERE app.database_name NOT LIKE '%index.db%'
        AND COALESCE(s.communication_thread_id,'') <> ''
        AND (app.item_identifier = s.communication_thread_id OR app.source_primary_key = s.communication_thread_id OR app.contact_or_participant = s.identity_hint)
  )
ORDER BY s.record_timestamp_utc DESC;

DROP VIEW IF EXISTS vw_ios_production_readiness_summary;
CREATE VIEW vw_ios_production_readiness_summary AS
SELECT '01_source_hash' AS readiness_order, 'source_container_hash' AS readiness_area,
       CASE WHEN EXISTS (SELECT 1 FROM preserved_evidence_sets WHERE COALESCE(archive_sha256,'')<>'' AND integrity_status='HASHED') THEN 'PRODUCTION_READY_HASH_RECORDED'
            WHEN (SELECT value FROM case_info WHERE key='force_container_hash')='true' THEN 'REVIEW_HASH_REQUESTED_BUT_NOT_RECORDED'
            WHEN (SELECT value FROM case_info WHERE key='skip_container_hash')='true' THEN 'NOT_PRODUCTION_READY_HASH_SKIPPED'
            ELSE 'REVIEW_HASH_STATUS_NOT_RECORDED' END AS status,
       COALESCE((SELECT MAX(integrity_status || ':' || COALESCE(archive_sha256,'')) FROM preserved_evidence_sets),'no preserved_evidence_sets row') AS evidence,
       'Production iOS ZIP runs should use forced container hashing or an externally documented source hash.' AS recommended_action
UNION ALL SELECT '02_export_profile','export_profile',
       CASE WHEN (SELECT value FROM case_info WHERE key='export_profile') IN ('investigator','support','full') THEN 'PRODUCTION_REVIEW_PROFILE' ELSE 'THIN_OR_DIAGNOSTIC_PROFILE' END,
       COALESCE((SELECT value FROM case_info WHERE key='export_profile'),'not_recorded'),
       'Use investigator profile for production review after thin validation.'
UNION ALL SELECT '03_native_decode','native_decode_mode',
       CASE WHEN COALESCE((SELECT GROUP_CONCAT(DISTINCT decode_mode) FROM native_decode_attempts),'') LIKE '%CoreFields%' OR COALESCE((SELECT GROUP_CONCAT(DISTINCT decode_mode) FROM native_decode_attempts),'') LIKE '%FullValues%' THEN 'CORE_OR_FULL_NATIVE_DECODE_PRESENT' ELSE 'HEADER_ONLY_OR_NOT_RECORDED' END,
       COALESCE((SELECT GROUP_CONCAT(DISTINCT decode_mode) FROM native_decode_attempts),'not_recorded'),
       'Production iOS review should include native CoreSpotlight value decoding.'
)SQL" R"SQL(UNION ALL SELECT '04_record_counts','core_spotlight_records',
       CASE WHEN (SELECT COUNT(*) FROM raw_records)>0 THEN 'RAW_RECORDS_PRESENT' ELSE 'NO_RAW_RECORDS_REVIEW_REQUIRED' END,
       'raw_records=' || (SELECT COUNT(*) FROM raw_records) || '; raw_key_values=' || (SELECT COUNT(*) FROM raw_key_values) || '; timeline_events=' || (SELECT COUNT(*) FROM timeline_events),
       'If counts are zero, review store discovery and native_decode_attempts before relying on the run.'
UNION ALL SELECT '05_text_context','ios_text_context',
       CASE WHEN (SELECT COUNT(*) FROM vw_ios_spotlight_text_context_review)>0 THEN 'TEXT_CONTEXT_PRESENT' ELSE 'TEXT_CONTEXT_NOT_OBSERVED' END,
       'text_context_rows=' || (SELECT COUNT(*) FROM vw_ios_spotlight_text_context_review) || '; string_probe_rows=' || (SELECT COUNT(*) FROM vw_ios_string_probe_values),
       'Text context rows are review surfaces requiring source validation.'
UNION ALL SELECT '06_missing_from_ffs','spotlight_missing_from_ffs',
       CASE WHEN EXISTS (SELECT 1 FROM sqlite_master WHERE type='view' AND name='vw_ios_spotlight_missing_from_ffs_high_value_candidates') THEN 'MISSING_FFS_REVIEW_SURFACE_PRESENT' ELSE 'MISSING_FFS_VIEW_MISSING' END,
       'high_value_missing_ffs_rows=' || (SELECT COUNT(*) FROM vw_ios_spotlight_missing_from_ffs_high_value_candidates),
       'Missing-from-FFS is a lead, not a standalone deletion conclusion.'
UNION ALL SELECT '07_native_db_mismatch','spotlight_not_matched_to_native_db',
       CASE WHEN EXISTS (SELECT 1 FROM sqlite_master WHERE type='view' AND name='vw_ios_spotlight_comms_missing_from_ffs') THEN 'NATIVE_DB_MISMATCH_VIEW_PRESENT' ELSE 'NATIVE_DB_MISMATCH_VIEW_MISSING' END,
       'spotlight_not_matched_rows=' || (SELECT COUNT(*) FROM vw_ios_spotlight_comms_missing_from_ffs),
       'Treat as a lead; possible explanations include deletion, app removal, encryption/inaccessibility, parser coverage limits, or unmatched identifiers.';

DROP VIEW IF EXISTS vw_ios_communication_identity_frequency;
CREATE VIEW vw_ios_communication_identity_frequency AS
SELECT
  COALESCE(NULLIF(contact_or_participant,''), NULLIF(item_identifier,''), '(no explicit identity)') AS identity_or_thread_hint,
  database_category,
  app_hint,
  record_category,
  COUNT(*) AS related_record_count,
  COUNT(DISTINCT COALESCE(NULLIF(item_identifier,''), source_primary_key)) AS distinct_thread_or_record_keys,
  MIN(NULLIF(record_timestamp_utc,'')) AS first_seen_utc,
  MAX(NULLIF(record_timestamp_utc,'')) AS last_seen_utc,
  GROUP_CONCAT(DISTINCT table_name) AS source_tables,
  GROUP_CONCAT(DISTINCT parse_status) AS parse_statuses,
  'Identity/frequency rollup from parsed app records and communication provenance markers.' AS interpretation_note
FROM ios_app_parsed_records
WHERE record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_COMMUNICATION_INTENT')
   OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
   OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
   OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%'
GROUP BY COALESCE(NULLIF(contact_or_participant,''), NULLIF(item_identifier,''), '(no explicit identity)'), database_category, app_hint, record_category
ORDER BY related_record_count DESC, last_seen_utc DESC;

)SQL" R"SQL(DROP VIEW IF EXISTS vw_ios_communication_temporal_frequency;
CREATE VIEW vw_ios_communication_temporal_frequency AS
SELECT
  substr(record_timestamp_utc,1,10) AS communication_date_utc,
  COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), '(no explicit thread)') AS communication_thread_or_identity,
  database_category,
  app_hint,
  record_category,
  COUNT(*) AS records_on_date,
  COUNT(DISTINCT contact_or_participant) AS distinct_identity_hints,
  MIN(record_timestamp_utc) AS first_record_utc,
  MAX(record_timestamp_utc) AS last_record_utc,
  GROUP_CONCAT(DISTINCT parse_status) AS parse_statuses
FROM ios_app_parsed_records
WHERE COALESCE(record_timestamp_utc,'')<>''
  AND (record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_COMMUNICATION_INTENT')
       OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
       OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
       OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%')
GROUP BY substr(record_timestamp_utc,1,10), COALESCE(NULLIF(item_identifier,''), NULLIF(contact_or_participant,''), '(no explicit thread)'), database_category, app_hint, record_category
ORDER BY communication_date_utc DESC, records_on_date DESC;

DROP VIEW IF EXISTS vw_ios_communication_source_coverage;
CREATE VIEW vw_ios_communication_source_coverage AS
SELECT
  database_category,
  app_hint,
  database_name,
  table_name,
  record_category,
  parse_status,
  COUNT(*) AS parsed_record_count,
  SUM(CASE WHEN COALESCE(record_timestamp_utc,'')<>'' THEN 1 ELSE 0 END) AS records_with_timestamp,
  SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS records_with_identity_hint,
  SUM(CASE WHEN COALESCE(item_identifier,'')<>'' THEN 1 ELSE 0 END) AS records_with_thread_or_item_id,
  MIN(NULLIF(record_timestamp_utc,'')) AS first_seen_utc,
  MAX(NULLIF(record_timestamp_utc,'')) AS last_seen_utc,
  'Communication existence/frequency source coverage by database/table/category.' AS interpretation_note
FROM ios_app_parsed_records
WHERE record_category IN ('MESSAGE_RECORDS','CHAT_RECORDS','MAIL_RECORDS','CALL_RECORDS','MESSAGE_PARTICIPANTS','CALL_PARTICIPANTS','MESSAGE_DELETED_OR_RECOVERABLE','KNOWLEDGEC_COMMUNICATION_INTENT')
   OR provenance LIKE '%THREAD_VOLUME_TRACKING_ENABLED%'
   OR provenance LIKE '%IDENTITY_BOUND_COMMUNICATION%'
   OR provenance LIKE '%COMMUNICATION_INTENT_STREAM%'
GROUP BY database_category, app_hint, database_name, table_name, record_category, parse_status
ORDER BY parsed_record_count DESC, records_with_timestamp DESC;

)SQL";
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
    }
}

std::wstring moduleDirectory() {
    wchar_t path[MAX_PATH]{};
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return L"";
    std::wstring p(path);
    std::size_t slash = p.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return L"";
    return p.substr(0, slash);
}

std::wstring logoBitmapPath() {
    std::wstring dir = moduleDirectory();
    if (!dir.empty()) {
        std::wstring exeRelative = dir + L"\\resources\\vestigant_logo.bmp";
        if (GetFileAttributesW(exeRelative.c_str()) != INVALID_FILE_ATTRIBUTES) return exeRelative;
    }
    std::wstring sourceRelative = L"resources\\vestigant_logo.bmp";
    if (GetFileAttributesW(sourceRelative.c_str()) != INVALID_FILE_ATTRIBUTES) return sourceRelative;
    return L"";
}

int iosPrimarySpotlightReviewRank(const ViewSpec& v) {
    const std::wstring name = v.displayName ? v.displayName : L"";
    // V0_9_17: focused default iOS view list. Full app DB/FFS inventories and broad
    // parser diagnostics remain exported, but the interactive iOS list is limited to
    // Spotlight/CoreSpotlight review and direct FFS overlap.
    static const wchar_t* kPrimaryNames[] = {
        L"iOS - Store Parse Summary",
        L"iOS - Spotlight High-Value Timeline",
        L"iOS - Spotlight Entity Review",
        L"iOS - Spotlight Entity Summary",
        L"iOS - Spotlight Investigative Items With Dates",
        L"iOS - Spotlight Investigative Item Date Evidence",
        L"iOS - Spotlight Date Provenance",
        L"iOS - Spotlight Date Field Summary",
        L"iOS - Spotlight File References With Dates",
        L"iOS - Spotlight URL References With Dates",
        L"iOS - Spotlight Account/Contact References With Dates",
        L"iOS - Spotlight Human Text Values",
        L"iOS - Spotlight Human Text Rollup",
        L"iOS - Spotlight Referenced Paths",
        L"iOS - Spotlight to FFS Links",
        L"iOS - Missing From FFS Candidates",
        L"iOS - Residency Summary",
        L"iOS - Spotlight Native Parser Targets",
        L"iOS - Spotlight dbStr Map Inventory",
        L"iOS - Spotlight Dictionary Coverage",
        L"iOS - Spotlight Apple Field Coverage",
        L"iOS - Spotlight Decode Gap Summary",
        L"iOS - Spotlight Decode Coverage Summary",
        L"iOS - Spotlight Field Coverage Summary",
        L"iOS - Spotlight Text Category Summary"
    };
    for (int i = 0; i < static_cast<int>(sizeof(kPrimaryNames) / sizeof(kPrimaryNames[0])); ++i) {
        if (name == kPrimaryNames[i]) return i;
    }
    return -1;
}

bool isIosPrimarySpotlightReviewView(const ViewSpec& v) {
    return iosPrimarySpotlightReviewRank(v) >= 0;
}

std::wstring lowerW(std::wstring s);
bool containsAnyW(const std::wstring& haystackLower, std::initializer_list<const wchar_t*> needles);

int iosReviewViewSortRank(const ViewSpec& v) {
    if (v.sortPriority >= 0) return v.sortPriority;
    const std::wstring name = v.displayName ? v.displayName : L"";
    // V0_9_57: expose all existing iOS views, but keep the most useful V1
    // investigative surfaces at the top so they are easy to find in mature cases.
    static const wchar_t* kV1PriorityNames[] = {
        L"iOS - Production Readiness Summary",
        L"iOS - Investigator Overview",
        L"iOS - Investigator Super Timeline",
        L"iOS - Investigator Time Anomalies",
        L"iOS - Unified Keyword Search Surface",
        L"iOS - Spotlight Text Context Review",
        L"iOS - High-Value Spotlight Text Context",
        L"iOS - Missing From FFS Text Detail",
        L"iOS - High-Value Missing From FFS",
        L"iOS - Direct User Message Review",
        L"iOS - User-Focus Message Body Review",
        L"iOS - Spotlight Message Body Review",
        L"iOS - Spotlight Message Text Review",
        L"iOS - Spotlight Communications Investigator Review",
        L"iOS - Spotlight Comms Missing From Native DB",
        L"iOS - Communications Review Records",
        L"iOS - KnowledgeC Interaction Events",
        L"iOS - Parsed App Records",
        L"iOS - Apple Messages Parsed Records",
        L"iOS - WhatsApp Parsed Records",
        L"iOS - Web History Review",
        L"iOS - Calendar Review",
        L"iOS - Contact Identity Review",
        L"iOS - Bplist/NSKeyedArchiver Detail",
        L"iOS - Store Parse Summary"
    };
    for (int i = 0; i < static_cast<int>(sizeof(kV1PriorityNames) / sizeof(kV1PriorityNames[0])); ++i) {
        if (name == kV1PriorityNames[i]) return i;
    }
    const int primaryRank = iosPrimarySpotlightReviewRank(v);
    if (primaryRank >= 0) return 200 + primaryRank;
    if (name.find(L"Summary") != std::wstring::npos || name.find(L"Dashboard") != std::wstring::npos) return 500;
    if (name.find(L"Parsed") != std::wstring::npos || name.find(L"Review") != std::wstring::npos) return 650;
    if (name.find(L"Timeline") != std::wstring::npos || name.find(L"Date") != std::wstring::npos) return 700;
    if (name.find(L"Inventory") != std::wstring::npos || name.find(L"Diagnostics") != std::wstring::npos) return 900;
    return 800;
}


int macReviewViewSortRank(const ViewSpec& v) {
    const std::wstring name = v.displayName ? v.displayName : L"";
    static const wchar_t* kV1PriorityNames[] = {
        L"Investigator - Keyword Search Values",
        L"Investigator - Usage Timeline",
        L"Investigator - Usage Artifacts",
        L"Investigator - Object Usage Summary",
        L"Investigator - Path Reconstruction",
        L"Investigator - WhereFroms / Downloads",
        L"Investigator - All Artifact Dates",
        L"Investigator - Date Attribution",
        L"Investigator - Snapshot Date Warnings",
        L"Investigator - Content Type Summary"
    };
    for (int i = 0; i < static_cast<int>(sizeof(kV1PriorityNames) / sizeof(kV1PriorityNames[0])); ++i) {
        if (name == kV1PriorityNames[i]) return i;
    }
    const std::wstring n = lowerW(name);
    if (containsAnyW(n, {L"deleted", L"tombstone"})) return 100;
    if (containsAnyW(n, {L"timeline", L"usage", L"date"})) return 300;
    if (containsAnyW(n, {L"path", L"wherefrom", L"download"})) return 400;
    if (containsAnyW(n, {L"summary", L"dashboard"})) return 500;
    if (containsAnyW(n, {L"diagnostic", L"coverage", L"parser", L"raw", L"inventory"})) return 900;
    return 700;
}

bool reviewViewMatchesProfile(const ViewSpec& v) {
    if (gReviewViewProfileMode == 5) return true; // Show All
    const std::wstring name = v.displayName ? v.displayName : L"";
    const std::wstring n = lowerW(name);
    switch (gReviewViewProfileMode) {
    case 1: // Timeline / Activity
        return containsAnyW(n, {L"timeline", L"date", L"time", L"usage", L"used", L"knowledgec", L"interaction", L"anomal", L"deleted", L"tombstone"});
    case 2: // Text / Content
        return containsAnyW(n, {L"text", L"message", L"content", L"keyword", L"entity", L"human", L"bplist", L"url", L"contact"});
    case 3: // App DB / KnowledgeC
        return containsAnyW(n, {L"app", L"knowledgec", L"parsed", L"messages", L"whatsapp", L"web", L"calendar", L"contact", L"database", L"interaction"});
    case 4: // Diagnostics / Coverage
        return containsAnyW(n, {L"diagnostic", L"coverage", L"parser", L"parse", L"inventory", L"summary", L"target", L"gap", L"dbstr", L"dictionary", L"field", L"decode"});
    case 0: // Recommended V1
    default:
        if (gIosReviewMode) return iosReviewViewSortRank(v) < 850;
        return macReviewViewSortRank(v) < 850;
    }
}

const wchar_t* reviewViewProfileDescription() {
    switch (gReviewViewProfileMode) {
    case 1: return L"Timeline / Activity";
    case 2: return L"Text / Content";
    case 3: return L"App DB / KnowledgeC";
    case 4: return L"Diagnostics / Coverage";
    case 5: return L"Show All";
    case 6: return L"Custom";
    default: return L"Recommended V1";
    }
}


std::string currentReviewPlatformKey() {
    return gIosReviewMode ? "ios" : "macos";
}

std::vector<std::wstring> loadCustomViewSetNoThrow(sqlite3* db) {
    std::vector<std::wstring> names;
    if (!db) return names;
    ensureInvestigatorUiSchemaNoThrow(db);
    sqlite3_stmt* st = nullptr;
    const char* sql = "SELECT view_name FROM review_view_preferences WHERE platform=? AND preset_name='Custom' AND is_visible=1 ORDER BY display_order, lower(view_name)";
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return names;
    const std::string platform = currentReviewPlatformKey();
    sqlite3_bind_text(st, 1, platform.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* raw = sqlite3_column_text(st, 0);
        if (raw) names.push_back(widen(reinterpret_cast<const char*>(raw)));
    }
    sqlite3_finalize(st);
    return names;
}

bool saveCustomViewSetNoThrow(const std::vector<size_t>& orderedViews) {
    if (gOpenedCaseDb.empty()) return false;
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        ensureInvestigatorUiSchemaNoThrow(db.get());
        char* err = nullptr;
        if (sqlite3_exec(db.get(), "BEGIN IMMEDIATE", nullptr, nullptr, &err) != SQLITE_OK) {
            if (err) sqlite3_free(err);
            return false;
        }
        const std::string platform = currentReviewPlatformKey();
        sqlite3_stmt* del = nullptr;
        if (sqlite3_prepare_v2(db.get(), "DELETE FROM review_view_preferences WHERE platform=? AND preset_name='Custom'", -1, &del, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(del, 1, platform.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(del);
        }
        if (del) sqlite3_finalize(del);
        sqlite3_stmt* ins = nullptr;
        const char* sql = "INSERT OR REPLACE INTO review_view_preferences(platform,view_name,is_visible,display_order,preset_name,updated_utc) VALUES(?,?,?,?, 'Custom', datetime('now'))";
        if (sqlite3_prepare_v2(db.get(), sql, -1, &ins, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        int order = 0;
        for (size_t idx : orderedViews) {
            if (idx >= views().size()) continue;
            sqlite3_bind_text(ins, 1, platform.c_str(), -1, SQLITE_TRANSIENT);
            const std::string viewName = narrow(std::wstring(views()[idx].displayName));
            sqlite3_bind_text(ins, 2, viewName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(ins, 3, 1);
            sqlite3_bind_int(ins, 4, order++);
            if (sqlite3_step(ins) != SQLITE_DONE) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(ins); throw std::runtime_error(msg); }
            sqlite3_reset(ins);
            sqlite3_clear_bindings(ins);
        }
        sqlite3_finalize(ins);
        if (sqlite3_exec(db.get(), "COMMIT", nullptr, nullptr, &err) != SQLITE_OK) {
            if (err) sqlite3_free(err);
            sqlite3_exec(db.get(), "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool resetCustomViewSetNoThrow() {
    if (gOpenedCaseDb.empty()) return false;
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        ensureInvestigatorUiSchemaNoThrow(db.get());
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db.get(), "DELETE FROM review_view_preferences WHERE platform=? AND preset_name='Custom'", -1, &st, nullptr) != SQLITE_OK) return false;
        const std::string platform = currentReviewPlatformKey();
        sqlite3_bind_text(st, 1, platform.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
        return true;
    } catch (...) {
        return false;
    }
}

bool viewVisibleForCurrentInvestigationTab(const ViewSpec& v) {
    if (gIosReviewMode) return v.platform == ViewPlatform::iOS || v.platform == ViewPlatform::Shared;
    return v.platform == ViewPlatform::MacOS || v.platform == ViewPlatform::Shared;
}

void populateViewList(sqlite3* db = nullptr) {
    if (!gReviewView) return;
    SendMessageW(gReviewView, LB_RESETCONTENT, 0, 0);
    gVisibleViews.clear();
    std::vector<size_t> candidates;

    if (gReviewViewProfileMode == 6 && db) {
        const std::vector<std::wstring> customNames = loadCustomViewSetNoThrow(db);
        for (const std::wstring& customName : customNames) {
            for (size_t i = 0; i < views().size(); ++i) {
                if (!viewVisibleForCurrentInvestigationTab(views()[i])) continue;
                if (customName != std::wstring(views()[i].displayName)) continue;
                if (!db || sqliteObjectExists(db, views()[i].tableName)) candidates.push_back(i);
                break;
            }
        }
    }

    if (candidates.empty()) {
        for (size_t i = 0; i < views().size(); ++i) {
            if (!viewVisibleForCurrentInvestigationTab(views()[i])) continue;
            if (gReviewViewProfileMode != 6 && !reviewViewMatchesProfile(views()[i])) continue;
            if (!db || sqliteObjectExists(db, views()[i].tableName)) candidates.push_back(i);
        }
        std::stable_sort(candidates.begin(), candidates.end(), [](size_t a, size_t b) {
            const int ra = gIosReviewMode ? iosReviewViewSortRank(views()[a]) : macReviewViewSortRank(views()[a]);
            const int rb = gIosReviewMode ? iosReviewViewSortRank(views()[b]) : macReviewViewSortRank(views()[b]);
            if (ra != rb) return ra < rb;
            return wcscmp(views()[a].displayName, views()[b].displayName) < 0;
        });
    }
    if (candidates.empty()) {
        // Fallback: if a profile is too narrow for this database, show every
        // platform-appropriate existing view instead of leaving the investigator
        // with an empty list.
        for (size_t i = 0; i < views().size(); ++i) {
            if (!viewVisibleForCurrentInvestigationTab(views()[i])) continue;
            if (!db || sqliteObjectExists(db, views()[i].tableName)) candidates.push_back(i);
        }
    }
    for (size_t i : candidates) {
        gVisibleViews.push_back(i);
        SendMessageW(gReviewView, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(views()[i].displayName));
    }
    SendMessageW(gReviewView, LB_SETCURSEL, 0, 0);
}

void populateViewListForCurrentContextNoThrow() {
    try {
        if (!gOpenedCaseDb.empty()) {
            ReadOnlyDb db(gOpenedCaseDb);
            populateViewList(db.get());
        } else {
            populateViewList();
        }
    } catch (...) {
        populateViewList();
    }
}


void refillReviewViewListFromVisible() {
    if (!gReviewView) return;
    SendMessageW(gReviewView, LB_RESETCONTENT, 0, 0);
    for (size_t idx : gVisibleViews) {
        if (idx < views().size()) SendMessageW(gReviewView, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(views()[idx].displayName));
    }
    if (!gVisibleViews.empty()) SendMessageW(gReviewView, LB_SETCURSEL, 0, 0);
}

void saveCurrentVisibleViewsAsCustom() {
    if (gVisibleViews.empty()) { setReviewSummary(L"No visible views are available to save as a custom view set."); return; }
    if (saveCustomViewSetNoThrow(gVisibleViews)) {
        gReviewViewProfileMode = 6;
        if (gReviewViewProfile) SendMessageW(gReviewViewProfile, CB_SETCURSEL, 6, 0);
        setReviewSummary(L"Saved Custom view set for the active platform tab. Use Hide/Move Up/Move Down, then Save Set again to refine it.");
    } else {
        setReviewSummary(L"Unable to save Custom view set. Confirm the case database is writable.");
    }
}

void resetCustomViewSet() {
    if (resetCustomViewSetNoThrow()) {
        setReviewSummary(L"Custom view set reset for the active platform tab.");
    } else {
        setReviewSummary(L"No writable custom view set was available to reset.");
    }
    populateViewListForCurrentContextNoThrow();
}

void hideSelectedViewFromCustom() {
    int sel = static_cast<int>(SendMessageW(gReviewView, LB_GETCURSEL, 0, 0));
    if (sel < 0 || sel >= static_cast<int>(gVisibleViews.size())) { setReviewSummary(L"Select a view to hide from the Custom view set."); return; }
    if (gReviewViewProfileMode != 6) {
        gReviewViewProfileMode = 6;
        if (gReviewViewProfile) SendMessageW(gReviewViewProfile, CB_SETCURSEL, 6, 0);
    }
    gVisibleViews.erase(gVisibleViews.begin() + sel);
    refillReviewViewListFromVisible();
    if (!gVisibleViews.empty()) SendMessageW(gReviewView, LB_SETCURSEL, std::min(sel, static_cast<int>(gVisibleViews.size()) - 1), 0);
    saveCurrentVisibleViewsAsCustom();
}

void moveSelectedViewInCustom(int delta) {
    int sel = static_cast<int>(SendMessageW(gReviewView, LB_GETCURSEL, 0, 0));
    if (sel < 0 || sel >= static_cast<int>(gVisibleViews.size())) { setReviewSummary(L"Select a view to reorder."); return; }
    int dst = sel + delta;
    if (dst < 0 || dst >= static_cast<int>(gVisibleViews.size())) return;
    if (gReviewViewProfileMode != 6) {
        gReviewViewProfileMode = 6;
        if (gReviewViewProfile) SendMessageW(gReviewViewProfile, CB_SETCURSEL, 6, 0);
    }
    std::swap(gVisibleViews[static_cast<size_t>(sel)], gVisibleViews[static_cast<size_t>(dst)]);
    refillReviewViewListFromVisible();
    SendMessageW(gReviewView, LB_SETCURSEL, dst, 0);
    saveCurrentVisibleViewsAsCustom();
}

void bindSearch(sqlite3_stmt* st, const ViewSpec& v, const std::string& search, int& index) {
    vestigant::spotlight::bindViewSearch(st, v, search, gFilterColumn, gFilterValue, index);
}

std::set<long long> checkedArtifactIdsSnapshotNoThrow() {
    try {
        std::lock_guard<std::mutex> lock(gReviewStateMutex);
        return gCheckedArtifactIds;
    } catch (...) {
        return {};
    }
}

std::size_t checkedArtifactCountSnapshotNoThrow() {
    try {
        std::lock_guard<std::mutex> lock(gReviewStateMutex);
        return gCheckedArtifactIds.size();
    } catch (...) {
        return 0;
    }
}

std::string checkedIdListSql() {
    const auto snapshot = checkedArtifactIdsSnapshotNoThrow();
    if (snapshot.empty()) return "";
    std::string ids;
    for (long long id : snapshot) {
        if (!ids.empty()) ids += ",";
        ids += std::to_string(id);
    }
    return ids;
}
std::string reviewOrderBy(const ViewSpec& v) {
    if (gSortColumn == -2) {
        int artifactCol = -1;
        for (size_t i = 0; i < v.columns.size(); ++i) { if (std::string(v.columns[i]) == "artifact_id") { artifactCol = static_cast<int>(i); break; } }
        const std::string ids = checkedIdListSql();
        if (artifactCol >= 0 && !ids.empty()) {
            std::string expr = "CASE WHEN CAST(artifact_id AS INTEGER) IN (" + ids + ") THEN 1 ELSE 0 END";
            return expr + (gSortDescending ? " ASC" : " DESC") + ", " + v.orderBy;
        }
        return v.orderBy;
    }
    if (gSortColumn >= 0 && gSortColumn < static_cast<int>(v.columns.size())) {
        std::string col = v.columns[static_cast<size_t>(gSortColumn)];
        std::string lower = col;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        const bool numericish = lower.find("id") != std::string::npos || lower.find("count") != std::string::npos || lower.find("bytes") != std::string::npos || lower.find("inode") != std::string::npos || lower.find("size") != std::string::npos;
        const bool dateish = lower.find("utc") != std::string::npos || lower.find("date") != std::string::npos || lower.find("time") != std::string::npos;
        std::string expr;
        if (numericish && !dateish) expr = "CAST(" + col + " AS INTEGER)";
        else expr = col;
        return expr + (gSortDescending ? " DESC" : " ASC");
    }
    return v.orderBy;
}

std::string reviewOrderByForPage(const ViewSpec& v, const std::string& search, bool hasColumnFilter) {
    if (gSortColumn >= -1 && gSortColumn != -1) return reviewOrderBy(v);
    if (!search.empty() || hasColumnFilter) return reviewOrderBy(v);
    const std::string table = v.tableName ? v.tableName : "";
    // V0_9_17: for initial GUI page previews, avoid expensive default ORDER BY
    // expressions on large iOS Spotlight views. Manual sorting and searching still
    // use the full requested order. This only changes the default unsorted preview.
    if (table == "vw_ios_spotlight_record_review") return "raw_record_id";
    if (table == "vw_ios_spotlight_date_provenance") return "raw_record_id";
    if (table == "vw_ios_spotlight_decode_gap_records") return "raw_record_id";
    if (table == "vw_ios_spotlight_object_identity") return "raw_record_id";
    if (table == "vw_ios_timeline_index_updates") return "raw_record_id";
    if (table == "vw_ios_spotlight_high_value_timeline") return "raw_kv_id";
    if (table == "vw_ios_spotlight_entity_review") return "raw_kv_id";
    if (table == "vw_ios_spotlight_investigative_items_with_dates") return "raw_kv_id";
    if (table == "vw_ios_spotlight_investigative_item_date_evidence") return "raw_kv_id, raw_date_id";
    if (table == "vw_ios_spotlight_file_reference_review") return "raw_kv_id";
    if (table == "vw_ios_spotlight_url_reference_review") return "raw_kv_id";
    if (table == "vw_ios_spotlight_account_contact_reference_review") return "raw_kv_id";
    if (table == "vw_ios_spotlight_human_text_values") return "raw_kv_id";
    if (table == "vw_ios_spotlight_human_text_rollup") return "raw_record_id";
    if (table == "vw_ios_spotlight_referenced_paths") return "reference_id";
    if (table == "vw_ios_spotlight_missing_from_ffs_candidates") return "reference_id";
    if (table == "vw_ios_spotlight_high_value_text_context_review") return "raw_kv_id";
    if (table == "vw_ios_spotlight_text_context_review") return "raw_kv_id";
    if (table == "vw_ios_spotlight_to_ffs_object_links") return "reference_id";
    if (table == "vw_ios_spotlight_to_app_db_record_links") return "candidate_id";
    return reviewOrderBy(v);
}
long long scalarCount(sqlite3* db, const std::string& sql, const ViewSpec* v = nullptr, const std::string& search = "") {
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    int idx = 1;
    if (v) bindSearch(st, *v, search, idx);
    long long value = 0;
    int rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) value = sqlite3_column_int64(st, 0);
    else if (rc != SQLITE_DONE) { std::string msg = sqlite3_errmsg(db); sqlite3_finalize(st); throw std::runtime_error(msg); }
    sqlite3_finalize(st);
    return value;
}
std::wstring scalarTextNoThrow(sqlite3* db, const std::string& sql) {
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) return L"";
    std::wstring value;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* t = sqlite3_column_text(st, 0);
        value = t ? widen(reinterpret_cast<const char*>(t)) : L"";
    }
    sqlite3_finalize(st);
    return value;
}
long long countTableNoThrow(sqlite3* db, const char* table) {
    try { return scalarCount(db, std::string("SELECT COUNT(*) FROM ") + table); }
    catch (...) { return -1; }
}
long long scalarCountNoThrow(sqlite3* db, const std::string& sql) {
    try { return scalarCount(db, sql); }
    catch (...) { return -1; }
}

void refreshIosStatusPanelNoThrow(sqlite3* db) {
    if (!gIosStatus) return;
    if (!db) {
        setText(gIosStatus, L"iOS Spotlight/CoreSpotlight review is Spotlight-first. Open a case to review recovered Spotlight values, date provenance, entity views, and FFS overlap/missing-unresolved candidates.");
        return;
    }
    const long long iosStoreRows = scalarCountNoThrow(db, "SELECT COUNT(*) FROM vw_ios_store_parse_summary");
    const long long iosRawRecords = scalarCountNoThrow(db, "SELECT COALESCE(SUM(raw_record_count),0) FROM vw_ios_store_parse_summary");
    const long long iosIndexRows = scalarCountNoThrow(db, "SELECT COUNT(*) FROM vw_ios_timeline_index_updates");
    const long long iosStringRows = scalarCountNoThrow(db, "SELECT COUNT(*) FROM vw_ios_string_probe_values");
    const long long iosRecordStringRows = scalarCountNoThrow(db, "SELECT COUNT(*) FROM vw_ios_record_string_probe_summary");
    const long long iosParsedAppRows = scalarCountNoThrow(db, "SELECT COUNT(*) FROM vw_ios_app_parsed_records");
    const long long allRawRecords = countTableNoThrow(db, "raw_records");
    const long long rawKvRows = countTableNoThrow(db, "raw_key_values");
    std::wostringstream os;
    if (iosStoreRows <= 0 && iosRawRecords <= 0) {
        os << L"Opened case has no visible iOS CoreSpotlight parser rows. Check source routing first: run_status stage_zip_source, ios_input_store_entry_inventory.csv, store_inventory.csv, and store_selection.csv. ";
        os << L"Raw Records=" << allRawRecords << L"  Raw Key Values=" << rawKvRows << L".";
    } else {
        os << L"iOS CoreSpotlight data available in opened case: Store Summary rows=" << iosStoreRows
           << L"  iOS Raw Records=" << iosRawRecords
           << L"  Index Timeline rows=" << iosIndexRows
           << L"  String Probe rows=" << iosStringRows
           << L"  Records with String Probes=" << iosRecordStringRows
           << L"  Parsed App DB rows=" << iosParsedAppRows
           << L"  Raw Key Values=" << rawKvRows << L". ";
        if (iosStringRows <= 0) {
            os << L"String views will be empty until bounded probe rows are present; V0_8_93_1 preserves the V0_8_81 fallback probes for iOS full-value parses with empty dictionaries. ";
        }
        os << L"Use Spotlight Timeline, Spotlight Entities, Date Evidence, FFS Overlap, Missing FFS, or Parser Targets for the default Spotlight-first review workflow.";
    }
    setText(gIosStatus, os.str());
}
int selectedViewIndex() {
    int sel = static_cast<int>(SendMessageW(gReviewView, LB_GETCURSEL, 0, 0));
    if (sel < 0) sel = 0;
    if (!gVisibleViews.empty() && sel < static_cast<int>(gVisibleViews.size())) return static_cast<int>(gVisibleViews[static_cast<size_t>(sel)]);
    if (sel >= static_cast<int>(views().size())) sel = 0;
    return sel;
}
int pageSize() {
    int n = _wtoi(getText(gPageSize).c_str());
    if (n < 100) n = 100;
    if (n > 50000) n = 50000;
    return n;
}
std::size_t parseSizeBox(HWND h, std::size_t fallback) {
    const std::wstring text = getText(h);
    if (text.empty()) return fallback;
    wchar_t* end = nullptr;
    unsigned long long value = std::wcstoull(text.c_str(), &end, 10);
    if (end == text.c_str()) return fallback;
    return static_cast<std::size_t>(value);
}
std::wstring siblingOfOpenedDb(const wchar_t* fileName) {
    if (gOpenedCaseDb.empty()) return L"";
    const size_t pos = gOpenedCaseDb.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return fileName;
    return gOpenedCaseDb.substr(0, pos + 1) + fileName;
}
void openSiblingFile(HWND owner, const wchar_t* fileName) {
    const std::wstring p = siblingOfOpenedDb(fileName);
    if (p.empty() || GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(owner, (std::wstring(L"File not found: ") + p).c_str(), L"Vestigant Spotlight", MB_ICONWARNING | MB_OK);
        return;
    }
    ShellExecuteW(owner, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
void openFolderIfExists(HWND owner, const std::wstring& folderPath, const wchar_t* title) {
    if (folderPath.empty() || GetFileAttributesW(folderPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(owner, (std::wstring(L"Folder not found: ") + folderPath).c_str(), title, MB_ICONWARNING | MB_OK);
        return;
    }
    ShellExecuteW(owner, L"open", folderPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
std::wstring openedCaseFolder() {
    if (gOpenedCaseDb.empty()) return L"";
    const size_t pos = gOpenedCaseDb.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L"";
    return gOpenedCaseDb.substr(0, pos);
}
void openCaseFolderAction(HWND owner) { openFolderIfExists(owner, openedCaseFolder(), L"Vestigant Spotlight - Case Folder"); }
void openUploadFolderAction(HWND owner) { openFolderIfExists(owner, openedCaseFolder() + L"\\Upload", L"Vestigant Spotlight - Upload Folder"); }
void openLogsFolderAction(HWND owner) {
    std::wstring logs = openedCaseFolder() + L"\\logs";
    if (GetFileAttributesW(logs.c_str()) == INVALID_FILE_ATTRIBUTES) logs = openedCaseFolder();
    openFolderIfExists(owner, logs, L"Vestigant Spotlight - Logs Folder");
}

void clearListColumns() {
    if (!gList) return;
    ListView_SetItemCountEx(gList, 0, LVSICF_NOSCROLL);
    ListView_DeleteAllItems(gList);
    gCurrentReviewRowsW.clear();
    gCurrentVisibleTagCellsW.clear();
    while (ListView_DeleteColumn(gList, 0)) {}
}
void setReviewSummary(const std::wstring& s) { SetWindowTextW(gReviewSummary, s.c_str()); }


std::wstring reviewViewTooltipForVisibleIndex(int visibleIndex) {
    if (visibleIndex < 0 || visibleIndex >= static_cast<int>(gVisibleViews.size())) return L"";
    const size_t viewIdx = gVisibleViews[static_cast<size_t>(visibleIndex)];
    if (viewIdx >= views().size()) return L"";
    const ViewSpec& v = views()[viewIdx];
    return std::wstring(v.displayName) + L"\r\n\r\n" + viewHelpText(v);
}

bool updateReviewViewTooltipTextFromCursor() {
    if (!gReviewView) return false;
    POINT pt{};
    GetCursorPos(&pt);
    ScreenToClient(gReviewView, &pt);
    DWORD_PTR hit = static_cast<DWORD_PTR>(SendMessageW(gReviewView, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y)));
    const int item = static_cast<int>(LOWORD(hit));
    const bool outside = HIWORD(hit) != 0;
    if (outside || item < 0) {
        gReviewViewTooltipText = L"Choose a review view. Hover over a view name to update the help text above the table.";
        return false;
    }
    gReviewViewTooltipText = reviewViewTooltipForVisibleIndex(item);
    if (gReviewViewTooltipText.empty()) {
        gReviewViewTooltipText = L"No description is available for this view.";
        return false;
    }
    return true;
}

void ensureReviewViewTooltip(HWND owner) {
    if (!owner || !gReviewView || gReviewViewTooltip) return;
    gReviewViewTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX | TTS_BALLOON,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        owner, nullptr, gInst, nullptr);
    if (!gReviewViewTooltip) return;
    SetWindowPos(gReviewViewTooltip, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SendMessageW(gReviewViewTooltip, TTM_SETMAXTIPWIDTH, 0, 520);
    SendMessageW(gReviewViewTooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 350);
    SendMessageW(gReviewViewTooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 20000);
    TOOLINFOW ti{};
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = owner;
    ti.uId = reinterpret_cast<UINT_PTR>(gReviewView);
    ti.lpszText = LPSTR_TEXTCALLBACKW;
    SendMessageW(gReviewViewTooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
}

LRESULT CALLBACK ReviewViewListSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEMOVE) {
        DWORD_PTR hit = static_cast<DWORD_PTR>(SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, lp));
        const int item = static_cast<int>(LOWORD(hit));
        const bool outside = HIWORD(hit) != 0;
        const int hoverItem = outside ? -1 : item;
        if (hoverItem != gReviewViewHoverListIndex) {
            gReviewViewHoverListIndex = hoverItem;
            if (hoverItem >= 0) {
                std::wstring help = reviewViewTooltipForVisibleIndex(hoverItem);
                if (!help.empty()) {
                    const size_t breakPos = help.find(L"\r\n\r\n");
                    std::wstring oneLine = (breakPos == std::wstring::npos) ? help : help.substr(0, breakPos) + L": " + help.substr(breakPos + 4);
                    if (oneLine.size() > 420) oneLine = oneLine.substr(0, 417) + L"...";
                    setReviewSummary(L"View help: " + oneLine);
                }
            }
            // Tooltip balloons are intentionally disabled; the help panel is updated above.
        }
    } else if (msg == WM_MOUSELEAVE) {
        gReviewViewHoverListIndex = -1;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

std::wstring tagsForArtifact(sqlite3* db, const std::string& artifactId) {
    return widen(vestigant::spotlight::tagsForArtifact(db, artifactId));
}

std::map<std::string, std::wstring> tagsForArtifacts(sqlite3* db, const std::vector<std::string>& artifactIds) {
    std::map<std::string, std::wstring> out;
    std::vector<long long> ids;
    ids.reserve(artifactIds.size());
    for (const std::string& idText : artifactIds) {
        if (idText.empty()) continue;
        char* end = nullptr;
        long long id = std::strtoll(idText.c_str(), &end, 10);
        if (end && *end == '\0' && id > 0 && out.find(idText) == out.end()) {
            ids.push_back(id);
            out[idText] = L"";
        }
    }
    if (ids.empty()) return out;
    std::string sql =
        "SELECT at.artifact_id, GROUP_CONCAT(it.tag_name, '; ') "
        "FROM artifact_tags at JOIN investigator_tags it ON it.tag_id=at.tag_id "
        "WHERE at.artifact_id IN (";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) sql += ',';
        sql += '?';
    }
    sql += ") GROUP BY at.artifact_id";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) return out;
    for (size_t i = 0; i < ids.size(); ++i) sqlite3_bind_int64(st, static_cast<int>(i + 1), ids[i]);
    while (sqlite3_step(st) == SQLITE_ROW) {
        const long long artifactId = sqlite3_column_int64(st, 0);
        const unsigned char* raw = sqlite3_column_text(st, 1);
        out[std::to_string(artifactId)] = raw ? widen(reinterpret_cast<const char*>(raw)) : L"";
    }
    sqlite3_finalize(st);
    return out;
}
void persistCheckedArtifactNoThrow(long long artifactId, bool checked) {
    if (gOpenedCaseDb.empty() || artifactId <= 0) return;
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        sqlite3_stmt* st = nullptr;
        const char* sql = checked
            ? "INSERT OR REPLACE INTO gui_checked_artifacts(artifact_id,checked_utc,notes) VALUES(?,datetime('now'),COALESCE((SELECT notes FROM gui_checked_artifacts WHERE artifact_id=?),''))"
            : "DELETE FROM gui_checked_artifacts WHERE artifact_id=?";
        if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) return;
        sqlite3_bind_int64(st, 1, artifactId);
        if (checked) sqlite3_bind_int64(st, 2, artifactId);
        sqlite3_step(st);
        sqlite3_finalize(st);
    } catch (...) {
    }
}
void persistCheckedArtifactListNoThrow(const std::vector<long long>& artifactIds, bool checked) {
    if (gOpenedCaseDb.empty() || artifactIds.empty()) return;
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        char* err = nullptr;
        sqlite3_exec(db.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
        sqlite3_stmt* st = nullptr;
        const char* sql = checked
            ? "INSERT OR REPLACE INTO gui_checked_artifacts(artifact_id,checked_utc,notes) VALUES(?,datetime('now'),COALESCE((SELECT notes FROM gui_checked_artifacts WHERE artifact_id=?),''))"
            : "DELETE FROM gui_checked_artifacts WHERE artifact_id=?";
        if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) return;
        for (long long artifactId : artifactIds) {
            if (artifactId <= 0) continue;
            sqlite3_bind_int64(st, 1, artifactId);
            if (checked) sqlite3_bind_int64(st, 2, artifactId);
            if (sqlite3_step(st) != SQLITE_DONE) { sqlite3_reset(st); sqlite3_clear_bindings(st); continue; }
            sqlite3_reset(st);
            sqlite3_clear_bindings(st);
        }
        sqlite3_finalize(st);
        if (sqlite3_exec(db.get(), "COMMIT", nullptr, nullptr, &err) != SQLITE_OK) {
            if (err) sqlite3_free(err);
            sqlite3_exec(db.get(), "ROLLBACK", nullptr, nullptr, nullptr);
        }
    } catch (...) {
    }
}

void clearPersistedCheckedArtifactsNoThrow() {
    if (gOpenedCaseDb.empty()) return;
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        sqlite3_exec(db.get(), "DELETE FROM gui_checked_artifacts", nullptr, nullptr, nullptr);
    } catch (...) {
    }
}
void loadPersistedCheckedArtifactsNoThrow() {
    gCheckedArtifactIds.clear();
    if (gOpenedCaseDb.empty()) return;
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        sqlite3_stmt* st = nullptr;
        const char* sql = "SELECT artifact_id FROM gui_checked_artifacts ORDER BY artifact_id";
        if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) return;
        while (sqlite3_step(st) == SQLITE_ROW) {
            const long long artifactId = sqlite3_column_int64(st, 0);
            if (artifactId > 0) gCheckedArtifactIds.insert(artifactId);
        }
        sqlite3_finalize(st);
    } catch (...) {
        gCheckedArtifactIds.clear();
    }
}
void refreshCheckedArtifactsSummaryNoThrow() {
    if (gReviewSummary) {
        setReviewSummary(L"Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size())) + L". Checked state is saved in this case database until cleared.");
    }
}

std::wstring checkMarkForState(bool checked) { return checked ? L"\u2611" : L"\u2610"; }
void setReviewRowCheckedState(int row, bool checked) {
    if (row < 0 || row >= static_cast<int>(gCurrentRowArtifactIds.size())) return;
    const std::string id = gCurrentRowArtifactIds[static_cast<size_t>(row)];
    if (id.empty()) return;
    const long long aid = std::strtoll(id.c_str(), nullptr, 10);
    if (checked) gCheckedArtifactIds.insert(aid);
    else gCheckedArtifactIds.erase(aid);
    if (!gBulkCheckUpdate) persistCheckedArtifactNoThrow(aid, checked);
    if (row >= 0 && row < static_cast<int>(gList ? ListView_GetItemCount(gList) : 0)) {
        if (gList) {
            ListView_RedrawItems(gList, row, row);
            InvalidateRect(gList, nullptr, FALSE);
        }
    }
}
void toggleReviewRowChecked(int row) {
    if (row < 0 || row >= static_cast<int>(gCurrentRowArtifactIds.size())) return;
    const std::string id = gCurrentRowArtifactIds[static_cast<size_t>(row)];
    if (id.empty()) return;
    const long long aid = std::strtoll(id.c_str(), nullptr, 10);
    setReviewRowCheckedState(row, gCheckedArtifactIds.find(aid) == gCheckedArtifactIds.end());
}
std::vector<int> selectedReviewRows() {
    std::vector<int> rows;
    int sel = -1;
    while ((sel = ListView_GetNextItem(gList, sel, LVNI_SELECTED)) >= 0) rows.push_back(sel);
    return rows;
}
void setReviewRowsChecked(const std::vector<int>& rows, bool checked) {
    std::vector<long long> artifactIds;
    artifactIds.reserve(rows.size());
    gBulkCheckUpdate = true;
    for (int row : rows) {
        if (row >= 0 && row < static_cast<int>(gCurrentRowArtifactIds.size())) {
            const std::string id = gCurrentRowArtifactIds[static_cast<size_t>(row)];
            if (!id.empty()) {
                const long long aid = std::strtoll(id.c_str(), nullptr, 10);
                if (aid > 0) artifactIds.push_back(aid);
            }
        }
        setReviewRowCheckedState(row, checked);
    }
    gBulkCheckUpdate = false;
    persistCheckedArtifactListNoThrow(artifactIds, checked);
}
void toggleReviewRowsAsBatch(const std::vector<int>& rows) {
    if (rows.empty()) return;
    bool anyUnchecked = false;
    for (int row : rows) {
        if (row < 0 || row >= static_cast<int>(gCurrentRowArtifactIds.size())) continue;
        const std::string id = gCurrentRowArtifactIds[static_cast<size_t>(row)];
        if (id.empty()) continue;
        const long long aid = std::strtoll(id.c_str(), nullptr, 10);
        if (gCheckedArtifactIds.find(aid) == gCheckedArtifactIds.end()) { anyUnchecked = true; break; }
    }
    setReviewRowsChecked(rows, anyUnchecked);
}
void focusReviewRow(int row, bool selectOnlyThisRow) {
    const int count = ListView_GetItemCount(gList);
    if (count <= 0) return;
    if (row < 0) row = 0;
    if (row >= count) row = count - 1;
    if (selectOnlyThisRow) {
        for (int i = 0; i < count; ++i) ListView_SetItemState(gList, i, 0, LVIS_SELECTED | LVIS_FOCUSED);
    }
    ListView_SetItemState(gList, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(gList, row, FALSE);
}
std::vector<std::string> checkedArtifactIds() {
    std::vector<std::string> ids;
    for (long long id : gCheckedArtifactIds) ids.push_back(std::to_string(id));
    return ids;
}
std::vector<std::string> selectedArtifactIdsFromReview(bool preferChecked) {
    std::vector<std::string> ids;
    if (preferChecked) ids = checkedArtifactIds();
    if (!ids.empty()) return ids;
    int sel = -1;
    while ((sel = ListView_GetNextItem(gList, sel, LVNI_SELECTED)) >= 0) {
        if (sel >= 0 && sel < static_cast<int>(gCurrentRowArtifactIds.size())) {
            std::string id = gCurrentRowArtifactIds[static_cast<size_t>(sel)];
            if (!id.empty() && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
        }
    }
    if (ids.empty()) throw std::runtime_error("Check or select at least one artifact-backed row in the active Investigation View first.");
    return ids;
}
void refreshVisibleTagCells(sqlite3* db) {
    if (gCurrentVisibleTagCellsW.size() < gCurrentRowArtifactIds.size()) {
        gCurrentVisibleTagCellsW.resize(gCurrentRowArtifactIds.size());
    }
    for (int row = 0; row < static_cast<int>(gCurrentRowArtifactIds.size()); ++row) {
        gCurrentVisibleTagCellsW[static_cast<std::size_t>(row)] = tagsForArtifact(db, gCurrentRowArtifactIds[static_cast<size_t>(row)]);
    }
    if (gList && !gCurrentRowArtifactIds.empty()) {
        ListView_RedrawItems(gList, 0, static_cast<int>(gCurrentRowArtifactIds.size()) - 1);
        InvalidateRect(gList, nullptr, FALSE);
    }
}

void applyTagToArtifacts(long long tagId, const std::vector<std::string>& artifactIds, bool reloadVisible);
void removeTagFromArtifacts(long long tagId, const std::vector<std::string>& artifactIds, bool reloadVisible);
long long selectedTagId();
void setTagSummary(const std::wstring& s);
void loadReviewPage();
void showTab(int index);
struct AvailableTag { long long tagId; std::wstring tagName; };
std::vector<AvailableTag> availableTagsForContextMenu() {
    std::vector<AvailableTag> tags;
    if (gOpenedCaseDb.empty()) return tags;
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        sqlite3_stmt* st = nullptr;
        const char* sql = "SELECT tag_id, tag_name FROM investigator_tags ORDER BY lower(tag_name) LIMIT 500";
        if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) return tags;
        while (sqlite3_step(st) == SQLITE_ROW) {
            AvailableTag t{};
            t.tagId = sqlite3_column_int64(st, 0);
            const unsigned char* raw = sqlite3_column_text(st, 1);
            t.tagName = raw ? widen(reinterpret_cast<const char*>(raw)) : L"(blank tag)";
            tags.push_back(t);
        }
        sqlite3_finalize(st);
    } catch (...) {
    }
    return tags;
}
UINT addContextTagSubmenu(HMENU parent, const wchar_t* title, const std::vector<AvailableTag>& tags, int operation, bool enabled, UINT nextCommandId) {
    HMENU sub = CreatePopupMenu();
    if (!enabled) {
        AppendMenuW(sub, MF_STRING | MF_DISABLED, 0, L"No eligible rows");
    } else if (tags.empty()) {
        AppendMenuW(sub, MF_STRING | MF_DISABLED, 0, L"No tags available; use Tags / Notes to create one");
    } else {
        for (const AvailableTag& tag : tags) {
            if (nextCommandId >= 65000) break;
            AppendMenuW(sub, MF_STRING, static_cast<UINT_PTR>(nextCommandId), tag.tagName.c_str());
            gContextTagCommands.push_back(ContextTagCommand{nextCommandId, tag.tagId, operation});
            ++nextCommandId;
        }
    }
    AppendMenuW(parent, MF_POPUP | (enabled ? 0 : MF_GRAYED), reinterpret_cast<UINT_PTR>(sub), title);
    return nextCommandId;
}
bool handleContextTagCommand(UINT commandId) {
    auto it = std::find_if(gContextTagCommands.begin(), gContextTagCommands.end(), [&](const ContextTagCommand& c){ return c.commandId == commandId; });
    if (it == gContextTagCommands.end()) return false;
    try {
        std::vector<std::string> ids;
        switch (it->operation) {
        case TAG_OP_APPLY_ROW:
        case TAG_OP_REMOVE_ROW:
            if (gContextRow >= 0 && gContextRow < static_cast<int>(gCurrentRowArtifactIds.size()) && !gCurrentRowArtifactIds[static_cast<size_t>(gContextRow)].empty()) ids.push_back(gCurrentRowArtifactIds[static_cast<size_t>(gContextRow)]);
            break;
        case TAG_OP_APPLY_SELECTED:
        case TAG_OP_REMOVE_SELECTED:
            ids = selectedArtifactIdsFromReview(false);
            break;
        case TAG_OP_APPLY_CHECKED:
        case TAG_OP_REMOVE_CHECKED:
            ids = checkedArtifactIds();
            break;
        default:
            break;
        }
        if (ids.empty()) throw std::runtime_error("No artifact-backed rows are eligible for this tag action.");
        if (it->operation == TAG_OP_REMOVE_ROW || it->operation == TAG_OP_REMOVE_SELECTED || it->operation == TAG_OP_REMOVE_CHECKED) removeTagFromArtifacts(it->tagId, ids, true);
        else applyTagToArtifacts(it->tagId, ids, true);
    } catch (const std::exception& ex) {
        setTagSummary(L"ERROR running tag menu action: " + widen(ex.what()));
        setReviewSummary(L"ERROR running tag menu action: " + widen(ex.what()));
    }
    return true;
}
void switchToTagsTab() {
    if (gTab) {
        TabCtrl_SetCurSel(gTab, 3);
        showTab(3);
    }
}
void selectReviewViewByName(const wchar_t* displayName) {
    if (!gReviewView) return;
    for (int i = 0; i < static_cast<int>(gVisibleViews.size()); ++i) {
        size_t viewIdx = gVisibleViews[static_cast<size_t>(i)];
        if (viewIdx < views().size() && wcscmp(views()[viewIdx].displayName, displayName) == 0) {
            SendMessageW(gReviewView, LB_SETCURSEL, i, 0);
            gCurrentPage = 0;
            gSortColumn = -1;
            gFilterColumn = -1;
            gFilterValue.clear();
            loadReviewPage();
            return;
        }
    }
    setReviewSummary(std::wstring(L"Requested view is not available in this database: ") + displayName);
}
void openIosReadinessView() {
    if (gTab) {
        gIosReviewMode = true;
        TabCtrl_SetCurSel(gTab, 2);
        showTab(2);
    }
    populateViewListForCurrentContextNoThrow();
    selectReviewViewByName(L"iOS Readiness - Relevant Fields");
}

void openIosReviewView(const wchar_t* displayName) {
    if (gTab) {
        gIosReviewMode = true;
        TabCtrl_SetCurSel(gTab, 2);
        showTab(2);
    }
    populateViewListForCurrentContextNoThrow();
    selectReviewViewByName(displayName);
}

void showColumnContextMenu(HWND owner, int column, POINT screenPoint) {
    const auto& v = views()[static_cast<size_t>(selectedViewIndex())];
    const int dataColumn = column - 2;
    gContextColumn = dataColumn;
    gContextTagCommands.clear();
    HMENU menu = CreatePopupMenu();
    if (gContextRow >= 0 && gContextRow < static_cast<int>(gCurrentRowArtifactIds.size())) {
        const bool hasSelectedRows = !selectedReviewRows().empty();
        const bool hasCheckedRows = !gCheckedArtifactIds.empty();
        const bool hasContextArtifact = !gCurrentRowArtifactIds[static_cast<size_t>(gContextRow)].empty();
        AppendMenuW(menu, MF_STRING, ID_CTX_TOGGLE_CHECK, L"Toggle checkmark for this row");
        AppendMenuW(menu, MF_STRING | (hasSelectedRows ? 0 : MF_GRAYED), ID_CTX_CHECK_SELECTED, L"Check selected row(s)");
        AppendMenuW(menu, MF_STRING | (hasSelectedRows ? 0 : MF_GRAYED), ID_CTX_UNCHECK_SELECTED, L"Uncheck selected row(s)");
        AppendMenuW(menu, MF_STRING | (hasSelectedRows ? 0 : MF_GRAYED), ID_CTX_TOGGLE_SELECTED, L"Toggle selected row(s)");
        AppendMenuW(menu, MF_STRING | (hasCheckedRows ? 0 : MF_GRAYED), ID_CTX_CLEAR_CHECKED, L"Clear all checked rows");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        const std::vector<AvailableTag> tags = availableTagsForContextMenu();
        UINT nextId = ID_CTX_TAG_BASE;
        nextId = addContextTagSubmenu(menu, L"Apply Tag to This Row", tags, TAG_OP_APPLY_ROW, hasContextArtifact, nextId);
        nextId = addContextTagSubmenu(menu, L"Remove Tag from This Row", tags, TAG_OP_REMOVE_ROW, hasContextArtifact, nextId);
        nextId = addContextTagSubmenu(menu, L"Apply Tag to Selected Row(s)", tags, TAG_OP_APPLY_SELECTED, hasSelectedRows, nextId);
        nextId = addContextTagSubmenu(menu, L"Remove Tag from Selected Row(s)", tags, TAG_OP_REMOVE_SELECTED, hasSelectedRows, nextId);
        nextId = addContextTagSubmenu(menu, L"Apply Tag to Checked Row(s)", tags, TAG_OP_APPLY_CHECKED, hasCheckedRows, nextId);
        nextId = addContextTagSubmenu(menu, L"Remove Tag from Checked Row(s)", tags, TAG_OP_REMOVE_CHECKED, hasCheckedRows, nextId);
        AppendMenuW(menu, MF_STRING, ID_CTX_MANAGE_TAGS, L"Manage Tags...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    if (dataColumn >= 0 && dataColumn < static_cast<int>(v.columns.size())) {
        std::wstring colName = widen(v.columns[static_cast<size_t>(dataColumn)]);
        AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, (L"Column: " + colName).c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_CTX_SORT_ASC, L"Sort ascending");
        AppendMenuW(menu, MF_STRING, ID_CTX_SORT_DESC, L"Sort descending");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        const bool hasSearchText = !getText(gSearch).empty();
        AppendMenuW(menu, MF_STRING | (hasSearchText ? 0 : MF_GRAYED), ID_CTX_FILTER_SEARCH, L"Filter this column using Search text");
        AppendMenuW(menu, MF_STRING | ((gFilterColumn >= 0 || !gFilterValue.empty()) ? 0 : MF_GRAYED), ID_CTX_CLEAR_FILTER, L"Clear column filter");
    }
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, screenPoint.x, screenPoint.y, 0, owner, nullptr);
    DestroyMenu(menu);
}


void setCaseMutationControlsEnabled(bool enabled) {
    const BOOL e = enabled ? TRUE : FALSE;
    if (gCaseName) EnableWindow(gCaseName, e);
    if (gCaseNumber) EnableWindow(gCaseNumber, e);
    if (gInvestigator) EnableWindow(gInvestigator, e);
    if (gCompany) EnableWindow(gCompany, e);
    if (gOut) EnableWindow(gOut, e);
    if (gCaseDbPath) EnableWindow(gCaseDbPath, e);
    if (gBrowseOut) EnableWindow(gBrowseOut, e);
    if (gBrowseCase) EnableWindow(gBrowseCase, e);
    if (gOpenCase) EnableWindow(gOpenCase, e);
    if (gSaveCaseInfo) EnableWindow(gSaveCaseInfo, e);
}

bool blockCaseMutationDuringIngest(HWND hwnd, const wchar_t* action) {
    if (!gIngestActive.load()) return false;
    std::wstring msg = L"The case is currently processing. Wait for ingest to complete before ";
    msg += action ? action : L"changing case information";
    msg += L".";
    postStatus(msg);
    if (hwnd) MessageBoxW(hwnd, msg.c_str(), L"Vestigant Spotlight - Ingest Running", MB_OK | MB_ICONINFORMATION);
    return true;
}

void loadCaseSummary() {
    if (gOpenedCaseDb.empty()) { setReviewSummary(L"No case database opened."); return; }
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        std::wostringstream os;
        os << L"Opened: " << gOpenedCaseDb << L"\r\n";
        os << L"Case: " << scalarTextNoThrow(db.get(), "SELECT value FROM case_info WHERE key='case_name'")
           << L"    App version: " << scalarTextNoThrow(db.get(), "SELECT value FROM case_info WHERE key='app_version'") << L"\r\n";
        os << L"Artifacts=" << countTableNoThrow(db.get(), "artifacts")
           << L"  Raw Records=" << countTableNoThrow(db.get(), "raw_records")
           << L"  Timeline=" << countTableNoThrow(db.get(), "timeline_events")
           << L"  Usage=" << countTableNoThrow(db.get(), "usage_evidence")
           << L"  Orphan/Missing=" << countTableNoThrow(db.get(), "orphaned_deleted_candidates")
           << L"  Store Groups=" << countTableNoThrow(db.get(), "store_groups");
        const std::wstring caseName = scalarTextNoThrow(db.get(), "SELECT value FROM case_info WHERE key='case_name'");
        const std::wstring caseNumber = scalarTextNoThrow(db.get(), "SELECT value FROM case_info WHERE key='case_number'");
        const std::wstring investigator = scalarTextNoThrow(db.get(), "SELECT value FROM case_info WHERE key='investigator'");
        const std::wstring company = scalarTextNoThrow(db.get(), "SELECT value FROM case_info WHERE key='company'");
        if (!caseName.empty()) setText(gCaseName, caseName);
        if (!caseNumber.empty()) setText(gCaseNumber, caseNumber);
        if (!investigator.empty()) setText(gInvestigator, investigator);
        if (!company.empty()) setText(gCompany, company);
        setReviewSummary(os.str());
        refreshIosStatusPanelNoThrow(db.get());
    } catch (const std::exception& ex) {
        setReviewSummary(L"ERROR opening case database: " + widen(ex.what()));
        refreshIosStatusPanelNoThrow(nullptr);
    }
}

void saveCaseInformationCore(bool autosave) {
    if (gIngestActive.load()) {
        if (gCaseAutosaveStatus) setText(gCaseAutosaveStatus, autosave ? L"Autosave deferred during ingest" : L"Save deferred during ingest");
        if (!autosave) setReviewSummary(L"Case information cannot be saved while ingest is running. The running workflow owns the case database handle.");
        return;
    }
    if (gOpenedCaseDb.empty() && !getText(gCaseDbPath).empty()) gOpenedCaseDb = getText(gCaseDbPath);
    if (gOpenedCaseDb.empty()) {
        if (!autosave) setReviewSummary(L"Open a case database before saving case information.");
        return;
    }
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        sqlite3_stmt* st = nullptr;
        const char* sql = "INSERT OR REPLACE INTO case_info(key,value) VALUES(?,?)";
        if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        auto put = [&](const char* key, const std::wstring& value) {
            const std::string v = narrow(value);
            sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, v.c_str(), -1, SQLITE_TRANSIENT);
            int rc = sqlite3_step(st);
            if (rc != SQLITE_DONE) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_reset(st); sqlite3_clear_bindings(st); throw std::runtime_error(msg); }
            sqlite3_reset(st); sqlite3_clear_bindings(st);
        };
        put("case_name", getText(gCaseName));
        put("case_number", getText(gCaseNumber));
        put("investigator", getText(gInvestigator));
        put("company", getText(gCompany));
        put("case_location", getText(gOut));
        put("case_database", gOpenedCaseDb);
        sqlite3_finalize(st);
        gCaseInfoDirty = false;
        if (gCaseAutosaveStatus) setText(gCaseAutosaveStatus, autosave ? L"Autosaved" : L"Saved");
        if (!autosave) setReviewSummary(L"Case information saved to SQLite case database.");
    } catch (const std::exception& ex) {
        if (gCaseAutosaveStatus) setText(gCaseAutosaveStatus, autosave ? L"Autosave failed" : L"Save failed");
        if (!autosave) setReviewSummary(L"ERROR saving case information: " + widen(ex.what()));
    }
}

void saveCaseInformation() { saveCaseInformationCore(false); }
void autosaveCaseInformation(HWND hwnd) {
    if (!gCaseInfoDirty) return;
    KillTimer(hwnd, ID_AUTOSAVE_TIMER);
    saveCaseInformationCore(true);
}
void scheduleCaseInfoAutosave(HWND hwnd) {
    gCaseInfoDirty = true;
    if (gCaseAutosaveStatus) setText(gCaseAutosaveStatus, L"Autosave pending...");
    SetTimer(hwnd, ID_AUTOSAVE_TIMER, 1500, nullptr);
}


void registerExportThread(std::thread worker) {
    std::lock_guard<std::mutex> lock(gExportThreadsMutex);
    gExportThreads.emplace_back(std::move(worker));
}

void joinExportThreadsNoThrow() {
    std::vector<std::thread> threads;
    try {
        std::lock_guard<std::mutex> lock(gExportThreadsMutex);
        threads.swap(gExportThreads);
    } catch (...) {
        return;
    }
    for (auto& t : threads) {
        try {
            if (t.joinable()) t.join();
        } catch (...) {}
    }
}


bool isIngestRunningNoThrow() {
    try { return gIngestActive.load(); } catch (...) { return false; }
}

void joinCompletedIngestThreadNoThrow() {
    try {
        std::lock_guard<std::mutex> lock(gIngestThreadMutex);
        if (!gIngestActive.load() && gIngestThread.joinable()) gIngestThread.join();
    } catch (...) {}
}

void joinIngestThreadNoThrow() {
    try {
        std::lock_guard<std::mutex> lock(gIngestThreadMutex);
        if (gIngestThread.joinable()) gIngestThread.join();
        gIngestActive.store(false);
    } catch (...) {}
}

bool startIngestThreadNoThrow(HWND owner) {
    try {
        std::lock_guard<std::mutex> lock(gIngestThreadMutex);
        bool expected = false;
        if (!gIngestActive.compare_exchange_strong(expected, true)) return false;
        if (gIngestThread.joinable()) gIngestThread.join();
        gCancelIngestRequested.store(false);
        gIngestThread = std::thread([owner]() {
            try {
                worker();
            } catch (const std::exception& ex) {
                postLog(L"ERROR: ingest worker terminated unexpectedly: " + widen(ex.what()));
                postStatus(L"ERROR: ingest worker terminated unexpectedly.");
            } catch (...) {
                postLog(L"ERROR: ingest worker terminated unexpectedly: unknown exception");
                postStatus(L"ERROR: ingest worker terminated unexpectedly.");
            }
            gIngestActive.store(false);
            if (!gShuttingDown.load() && owner && IsWindow(owner)) {
                PostMessageW(owner, WM_SET_INGEST_STATUS, 0, reinterpret_cast<LPARAM>(new std::wstring(L"Ingest worker finished.")));
            }
        });
        return true;
    } catch (const std::exception& ex) {
        gIngestActive.store(false);
        postLog(L"ERROR starting ingest worker: " + widen(ex.what()));
        postStatus(L"ERROR: could not start ingest worker.");
    } catch (...) {
        gIngestActive.store(false);
        postLog(L"ERROR starting ingest worker: unknown exception");
        postStatus(L"ERROR: could not start ingest worker.");
    }
    return false;
}

bool postExportResult(HWND owner, UINT msgId, bool ok, std::wstring message) {
    auto* msg = new std::wstring(std::move(message));
    if (!gShuttingDown.load() && owner && IsWindow(owner) && PostMessageW(owner, msgId, ok ? 1 : 0, reinterpret_cast<LPARAM>(msg))) {
        return true;
    }
    delete msg;
    return false;
}

void setReviewLoadingState(bool loading) {
    gReviewLoadInProgress = loading;
    if (gRefresh) EnableWindow(gRefresh, loading ? FALSE : TRUE);
    if (gCancelLoad) EnableWindow(gCancelLoad, loading ? TRUE : FALSE);
    if (gPrev) EnableWindow(gPrev, (!loading && gCurrentPage > 0) ? TRUE : FALSE);
    if (gNext) EnableWindow(gNext, (!loading && gCurrentHasNext) ? TRUE : FALSE);
    if (gExportPage) EnableWindow(gExportPage, loading ? FALSE : TRUE);
    if (gExportFiltered) EnableWindow(gExportFiltered, loading ? FALSE : TRUE);
    if (gExportVisible) EnableWindow(gExportVisible, loading ? FALSE : TRUE);
    if (gExportChecked) EnableWindow(gExportChecked, loading ? FALSE : TRUE);
    if (gReviewBusy) {
        ShowWindow(gReviewBusy, loading ? SW_SHOW : SW_HIDE);
        SendMessageW(gReviewBusy, PBM_SETMARQUEE, loading ? TRUE : FALSE, loading ? 35 : 0);
    }
    if (loading) SetCursor(LoadCursorW(nullptr, IDC_WAIT));
}

void updateRowDetailsPanel(int forcedRow = -1);
void layoutControls(HWND hwnd);

void populateReviewListFromResult(const ReviewPageResult& result) {
    if (result.viewIndex < 0 || result.viewIndex >= static_cast<int>(views().size())) {
        throw std::runtime_error("review result references an invalid view index");
    }
    const auto& v = views()[static_cast<size_t>(result.viewIndex)];
    clearListColumns();
    gCurrentRowArtifactIds = result.artifactIds;
    gCurrentPage = result.page;
    gCurrentHasNext = result.hasNext;

    gCurrentReviewRowsW.clear();
    gCurrentVisibleTagCellsW.clear();
    gCurrentReviewRowsW.reserve(result.rows.size());
    gCurrentVisibleTagCellsW.reserve(result.rows.size());
    for (std::size_t row = 0; row < result.rows.size(); ++row) {
        std::vector<std::wstring> wideRow;
        wideRow.reserve(result.rows[row].size());
        for (const auto& cell : result.rows[row]) wideRow.push_back(widen(cell));
        gCurrentReviewRowsW.push_back(std::move(wideRow));
        const std::string artifactId = row < result.artifactIds.size() ? result.artifactIds[row] : std::string();
        auto tagIt = result.visibleTags.find(artifactId);
        gCurrentVisibleTagCellsW.push_back(tagIt == result.visibleTags.end() ? L"" : tagIt->second);
    }

    const bool redrawSuspended = (gList != nullptr);
    if (redrawSuspended) {
        SendMessageW(gList, WM_SETREDRAW, FALSE, 0);
        ListView_SetItemCountEx(gList, 0, LVSICF_NOSCROLL);
    }

    const wchar_t* fixedCols[] = {L"\u2713", L"Tags"};
    for (int c = 0; c < 2; ++c) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = const_cast<LPWSTR>(fixedCols[c]);
        col.cx = c == 0 ? 46 : 175;
        col.iSubItem = c;
        ListView_InsertColumn(gList, c, &col);
    }
    for (size_t c = 0; c < v.columns.size(); ++c) {
        std::wstring name = widen(v.columns[c]);
        const std::string colName = v.columns[c];
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = const_cast<LPWSTR>(name.c_str());
        col.cx = (colName.find("path") != std::string::npos || colName.find("message") != std::string::npos || colName.find("value") != std::string::npos || colName.find("summary") != std::string::npos) ? 280 : 135;
        col.iSubItem = static_cast<int>(c + 2);
        ListView_InsertColumn(gList, static_cast<int>(c + 2), &col);
    }

    if (gList) {
        ListView_SetItemCountEx(gList, static_cast<int>(gCurrentReviewRowsW.size()), LVSICF_NOSCROLL);
    }

    if (redrawSuspended) {
        SendMessageW(gList, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(gList, nullptr, TRUE);
    }
}

void completeReviewPageLoad(ReviewPageResult* result) {
    if (!result) return;
    if (result->requestId != gReviewRequestSeq.load()) {
        delete result;
        return;
    }
    try {
        if (!result->error.empty()) {
            clearListColumns();
            gCurrentRowArtifactIds.clear();
            gCurrentHasNext = false;
            setReviewSummary(L"ERROR loading review page: " + result->error);
            updateRowDetailsPanel();
        } else {
            populateReviewListFromResult(*result);
            setReviewSummary(result->summary);
            if (gList && ListView_GetItemCount(gList) > 0) {
                focusReviewRow(0, true);
                updateRowDetailsPanel(0);
            } else {
                updateRowDetailsPanel();
            }
        }
    } catch (const std::exception& ex) {
        clearListColumns();
        gCurrentRowArtifactIds.clear();
        gCurrentHasNext = false;
        setReviewSummary(L"ERROR applying review page result: " + widen(ex.what()));
        updateRowDetailsPanel();
    }
    setReviewLoadingState(false);
    delete result;
}

void loadReviewPage() {
    if (gOpenedCaseDb.empty()) {
        setReviewSummary(L"Open an existing VestigantSpotlight.case.sqlite file first.");
        return;
    }

    if (gReviewThread.joinable()) {
        ++gReviewRequestSeq;
        gReviewThread.join();
    }
    const unsigned long long requestId = ++gReviewRequestSeq;
    const std::wstring dbPath = gOpenedCaseDb;
    const HWND owner = GetParent(gList);
    int viewIndex = selectedViewIndex();
    if (viewIndex < 0 || viewIndex >= static_cast<int>(views().size())) viewIndex = 0;
    const ViewSpec v = views()[static_cast<size_t>(viewIndex)];
    const std::string search = narrow(getText(gSearch));
    const int ps = pageSize();
    if (gCurrentPage < 0) gCurrentPage = 0;
    const int requestedPage = gCurrentPage;
    const int capturedFilterColumn = gFilterColumn;
    const std::string capturedFilterValue = gFilterValue;
    const std::string where = vestigant::spotlight::buildWhere(v, search, capturedFilterColumn, capturedFilterValue);
    const std::string orderBy = reviewOrderByForPage(v, search, (capturedFilterColumn >= 0 && !capturedFilterValue.empty()));
    const std::string sql = "SELECT " + sqlColumns(v) + " FROM " + v.tableName + where + " ORDER BY " + orderBy + " LIMIT ? OFFSET ?";
    const size_t checkedCountAtRequest = checkedArtifactCountSnapshotNoThrow();

    std::vector<std::string> bindPatterns;
    if (!search.empty()) {
        const std::string pattern = "%" + search + "%";
        for (size_t i = 0; i < v.searchColumns.size(); ++i) bindPatterns.push_back(pattern);
    }
    if (capturedFilterColumn >= 0 && capturedFilterColumn < static_cast<int>(v.columns.size()) && !capturedFilterValue.empty()) {
        bindPatterns.push_back("%" + capturedFilterValue + "%");
    }

    setReviewLoadingState(true);
    clearListColumns();
    gCurrentRowArtifactIds.clear();
    gCurrentHasNext = false;
    std::wostringstream loading;
    loading << L"Loading " << v.displayName << L" page " << (requestedPage + 1) << L" from SQLite in background. You can cancel or choose another view/search.";
    setReviewSummary(loading.str());
    setDetailsPaneMessage(L"Loading selected view. Results will appear when the background SQLite query completes.");
    UpdateWindow(gReviewSummary);
    UpdateWindow(gReviewBusy);
    UpdateWindow(gList);

    gReviewThread = std::thread([requestId, owner, dbPath, v, viewIndex, requestedPage, ps, search, capturedFilterValue, sql, bindPatterns, checkedCountAtRequest]() {
        auto* result = new ReviewPageResult();
        result->requestId = requestId;
        result->viewIndex = viewIndex;
        result->page = requestedPage;
        result->pageSize = ps;
        const ULONGLONG startedMs = GetTickCount64();
        try {
            ReadOnlyDb db(dbPath);
            ReviewCancelContext cancelCtx{requestId};
            sqlite3_progress_handler(db.get(), 1000, sqliteReviewProgressCancel, &cancelCtx);
            sqlite3_stmt* st = nullptr;
            if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
            int bind = 1;
            for (const std::string& pattern : bindPatterns) sqlite3_bind_text(st, bind++, pattern.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, bind++, ps + 1);
            sqlite3_bind_int64(st, bind++, static_cast<sqlite3_int64>(requestedPage) * ps);

            int row = 0;
            while (true) {
                if (requestId != gReviewRequestSeq.load() || gShuttingDown.load()) { sqlite3_finalize(st); sqlite3_progress_handler(db.get(), 0, nullptr, nullptr); delete result; return; }
                int rc = sqlite3_step(st);
                if (rc == SQLITE_DONE) break;
                if (rc != SQLITE_ROW) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
                if (row >= ps) { result->hasNext = true; break; }
                result->artifactIds.push_back(resolveArtifactIdForVisibleRow(db.get(), v, st));
                std::vector<std::string> cells;
                cells.reserve(static_cast<size_t>(sqlite3_column_count(st)));
                for (int c = 0; c < sqlite3_column_count(st); ++c) {
                    const unsigned char* raw = sqlite3_column_text(st, c);
                    cells.push_back(raw ? reinterpret_cast<const char*>(raw) : "");
                }
                result->rows.push_back(std::move(cells));
                ++row;
            }
            sqlite3_finalize(st);
            sqlite3_progress_handler(db.get(), 0, nullptr, nullptr);
            if (requestId != gReviewRequestSeq.load() || gShuttingDown.load()) { delete result; return; }
            result->visibleTags = tagsForArtifacts(db.get(), result->artifactIds);
            result->elapsedMs = GetTickCount64() - startedMs;
            const long long firstRow = row == 0 ? 0 : (static_cast<long long>(requestedPage) * ps + 1);
            const long long lastRow = row == 0 ? 0 : (static_cast<long long>(requestedPage) * ps + row);
            std::wostringstream os;
            os << L"View: " << v.displayName << L"    Showing " << firstRow << L"-" << lastRow;
            os << (result->hasNext ? L"    More rows available" : L"    Last page");
            os << L"    Page size=" << ps;
            os << L"    Checked artifacts=" << static_cast<unsigned long long>(checkedCountAtRequest);
            os << L"    Loaded in background in " << result->elapsedMs << L" ms";
            if (!search.empty()) os << L"    Search=\"" << widen(search) << L"\"";
            if (!capturedFilterValue.empty()) os << L"    Column filter=\"" << widen(capturedFilterValue) << L"\"";
            result->summary = os.str();
        } catch (const std::exception& ex) {
            result->elapsedMs = GetTickCount64() - startedMs;
            result->error = widen(ex.what());
        }
        PostMessageW(owner, WM_REVIEW_PAGE_RESULT, 0, reinterpret_cast<LPARAM>(result));
    });
}

void exportCurrentPage(HWND owner) {
    if (gOpenedCaseDb.empty()) { setReviewSummary(L"Open a case database before exporting."); return; }
    bool expected = false;
    if (!gExportPageActive.compare_exchange_strong(expected, true)) {
        setReviewSummary(L"Export Page is already running. Wait for the current export to finish.");
        return;
    }
    std::wstring out = saveCsv(owner);
    if (out.empty()) { gExportPageActive.store(false); return; }
    const int viewIdx = selectedViewIndex();
    if (viewIdx < 0 || viewIdx >= static_cast<int>(views().size())) { gExportPageActive.store(false); setReviewSummary(L"No valid view is selected for export."); return; }
    const auto v = views()[static_cast<size_t>(viewIdx)];
    GuiViewExportRequest request;
    request.dbPath = gOpenedCaseDb;
    request.view = v;
    request.search = narrow(getText(gSearch));
    request.filterColumn = gFilterColumn;
    request.filterValue = gFilterValue;
    request.orderBy = reviewOrderByForPage(v, request.search, (gFilterColumn >= 0 && !gFilterValue.empty()));
    request.page = gCurrentPage;
    request.pageSize = pageSize();
    request.checkedArtifactIds = checkedArtifactIdsSnapshotNoThrow();
    request.outPath = out;
    request.shouldCancel = []() { return gShuttingDown.load(); };

    setReviewSummary(L"Export Current Page in progress... GUI remains active.");
    if (gExportPage) EnableWindow(gExportPage, FALSE);
    registerExportThread(std::thread([owner, request]() {
        const GuiExportResult result = GuiExportWorker::exportCurrentPage(request);
        postExportResult(owner, WM_EXPORT_PAGE_RESULT, result.ok, result.message);
    }));
}



void exportFilteredView(HWND owner) {
    if (gOpenedCaseDb.empty()) { setReviewSummary(L"Open a case database before exporting filtered view."); return; }
    bool expected = false;
    if (!gExportFilteredActive.compare_exchange_strong(expected, true)) {
        setReviewSummary(L"Export Filtered is already running. Wait for the current export to finish.");
        return;
    }
    const std::wstring searchText = getText(gSearch);
    if (searchText.empty() && gFilterValue.empty()) {
        int answer = MessageBoxW(owner,
            L"This will export the entire selected view because no search/filter text is active. Large views may produce very large CSV files. Continue?",
            L"Vestigant Spotlight - Export Filtered View",
            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
        if (answer != IDYES) { gExportFilteredActive.store(false); return; }
    }
    std::wstring out = saveCsv(owner);
    if (out.empty()) { gExportFilteredActive.store(false); return; }
    const int viewIdx = selectedViewIndex();
    if (viewIdx < 0 || viewIdx >= static_cast<int>(views().size())) { gExportFilteredActive.store(false); setReviewSummary(L"No valid view is selected for export."); return; }
    const auto v = views()[static_cast<size_t>(viewIdx)];
    GuiViewExportRequest request;
    request.dbPath = gOpenedCaseDb;
    request.view = v;
    request.search = narrow(searchText);
    request.filterColumn = gFilterColumn;
    request.filterValue = gFilterValue;
    request.orderBy = reviewOrderBy(v);
    request.checkedArtifactIds = checkedArtifactIdsSnapshotNoThrow();
    request.outPath = out;
    request.shouldCancel = []() { return gShuttingDown.load(); };

    setReviewSummary(L"Export Filtered in progress... GUI remains active.");
    if (gExportFiltered) EnableWindow(gExportFiltered, FALSE);
    registerExportThread(std::thread([owner, request]() {
        const GuiExportResult result = GuiExportWorker::exportFilteredView(request);
        postExportResult(owner, WM_EXPORT_FILTERED_RESULT, result.ok, result.message);
    }));
}




void exportVisibleViews(HWND owner) {
    if (gOpenedCaseDb.empty()) { setReviewSummary(L"Open a case database before exporting visible views."); return; }
    if (gVisibleViews.empty()) { setReviewSummary(L"No visible review views are available to export."); return; }
    bool expected = false;
    if (!gExportVisibleActive.compare_exchange_strong(expected, true)) {
        setReviewSummary(L"Export Visible Views is already running. Wait for the current export to finish.");
        return;
    }
    int answer = MessageBoxW(owner,
        L"This exports every view currently listed in the view panel to separate CSV files in one folder. Active search text will be applied to each view. Column-specific filters are not applied across different views. Large view sets may take time. Continue?",
        L"Vestigant Spotlight - Export Visible Views",
        MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
    if (answer != IDYES) { gExportVisibleActive.store(false); return; }
    std::wstring outFolder = browseFolder(owner);
    if (outFolder.empty()) { gExportVisibleActive.store(false); return; }
    std::vector<ViewSpec> viewSpecs;
    viewSpecs.reserve(gVisibleViews.size());
    for (size_t idx : gVisibleViews) {
        if (idx < views().size()) viewSpecs.push_back(views()[idx]);
    }
    if (viewSpecs.empty()) { gExportVisibleActive.store(false); setReviewSummary(L"No valid visible views were available to export."); return; }
    const std::wstring dbPath = gOpenedCaseDb;
    const std::string search = narrow(getText(gSearch));

    setReviewSummary(L"Export Visible Views in progress... GUI remains active.");
    if (gExportVisible) EnableWindow(gExportVisible, FALSE);
    registerExportThread(std::thread([owner, dbPath, viewSpecs, search, outFolder]() {
        const GuiExportResult result = GuiExportWorker::exportVisibleViews(dbPath, viewSpecs, search, outFolder, []() { return gShuttingDown.load(); });
        postExportResult(owner, WM_EXPORT_VISIBLE_RESULT, result.ok, result.message);
    }));
}

void exportCheckedArtifacts(HWND owner) {
    if (gOpenedCaseDb.empty()) { setReviewSummary(L"Open a case database before exporting checked artifacts."); return; }
    if (checkedArtifactCountSnapshotNoThrow() == 0) { setReviewSummary(L"No checked artifacts to export."); return; }
    bool expected = false;
    if (!gExportCheckedActive.compare_exchange_strong(expected, true)) {
        setReviewSummary(L"Export Checked is already running. Wait for the current export to finish.");
        return;
    }
    std::wstring out = saveCsv(owner);
    if (out.empty()) { gExportCheckedActive.store(false); return; }

    const std::wstring dbPath = gOpenedCaseDb;
    const auto checkedSnapshot = checkedArtifactIdsSnapshotNoThrow();
    const std::vector<long long> idsToExport(checkedSnapshot.begin(), checkedSnapshot.end());
    setReviewSummary(L"Export Checked in progress... GUI remains active.");
    if (gExportChecked) EnableWindow(gExportChecked, FALSE);

    registerExportThread(std::thread([owner, dbPath, idsToExport, out]() {
        const GuiExportResult result = GuiExportWorker::exportCheckedArtifacts(dbPath, idsToExport, out, []() { return gShuttingDown.load(); });
        postExportResult(owner, WM_EXPORT_CHECKED_RESULT, result.ok, result.message);
    }));
}

void exportTaggedArtifacts(HWND owner) {
    if (gOpenedCaseDb.empty()) { setTagSummary(L"Open a case database before exporting tagged artifacts."); return; }
    const long long tagId = selectedTagId();
    if (tagId < 0) { setTagSummary(L"Select a tag to export."); return; }
    bool expected = false;
    if (!gExportTaggedActive.compare_exchange_strong(expected, true)) {
        setTagSummary(L"Export Tagged is already running. Wait for the current export to finish.");
        return;
    }
    std::wstring out = saveCsv(owner);
    if (out.empty()) { gExportTaggedActive.store(false); return; }

    const std::wstring dbPath = gOpenedCaseDb;
    setTagSummary(L"Export Tagged in progress... GUI remains active.");
    if (gTaggedList) EnableWindow(gTaggedList, FALSE);

    registerExportThread(std::thread([owner, dbPath, tagId, out]() {
        const GuiExportResult result = GuiExportWorker::exportTaggedArtifacts(dbPath, tagId, out, []() { return gShuttingDown.load(); });
        postExportResult(owner, WM_EXPORT_TAGGED_RESULT, result.ok, result.message);
    }));
}



std::wstring reviewCellText(int row, int col) {
    if (row < 0 || col < 0) return L"";
    if (col == 0) {
        if (row >= static_cast<int>(gCurrentRowArtifactIds.size())) return L"";
        const std::string& id = gCurrentRowArtifactIds[static_cast<std::size_t>(row)];
        if (id.empty()) return L"";
        const long long aid = std::strtoll(id.c_str(), nullptr, 10);
        return checkMarkForState(gCheckedArtifactIds.find(aid) != gCheckedArtifactIds.end());
    }
    if (col == 1) {
        return row < static_cast<int>(gCurrentVisibleTagCellsW.size()) ? gCurrentVisibleTagCellsW[static_cast<std::size_t>(row)] : L"";
    }
    const int dataCol = col - 2;
    if (row < static_cast<int>(gCurrentReviewRowsW.size()) && dataCol >= 0 && dataCol < static_cast<int>(gCurrentReviewRowsW[static_cast<std::size_t>(row)].size())) {
        return gCurrentReviewRowsW[static_cast<std::size_t>(row)][static_cast<std::size_t>(dataCol)];
    }
    return L"";
}

std::wstring listViewText(HWND list, int row, int col) {
    if (!list || row < 0 || col < 0) return L"";
    if (list == gList) return reviewCellText(row, col);
    std::vector<wchar_t> buf(65536);
    ListView_GetItemText(list, row, col, buf.data(), static_cast<int>(buf.size()));
    return std::wstring(buf.data());
}

std::wstring lowerW(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return s;
}

bool containsAnyW(const std::wstring& haystackLower, std::initializer_list<const wchar_t*> needles) {
    for (const wchar_t* n : needles) {
        if (n && haystackLower.find(n) != std::wstring::npos) return true;
    }
    return false;
}

bool looksEmptyDetailsValue(const std::wstring& value) {
    if (value.empty()) return true;
    const std::wstring v = lowerW(value);
    return v == L"(empty)" || v == L"null" || v == L"<null>";
}

enum class DetailGroup {
    Header = -1,
    Text = 0,
    Dates,
    Paths,
    PeopleApps,
    StatusInterpretation,
    IdentifiersProvenance,
    CountsSizes,
    Other
};

struct DetailField {
    std::wstring column;
    std::wstring value;
    DetailGroup group = DetailGroup::Other;
    std::size_t originalIndex = 0;
};

DetailGroup classifyDetailField(const std::wstring& columnName) {
    const std::wstring c = lowerW(columnName);
    if (containsAnyW(c, {L"text", L"snippet", L"body", L"message", L"details", L"summary", L"sample", L"value", L"title", L"subject", L"content", L"searchable"})) return DetailGroup::Text;
    if (containsAnyW(c, {L"utc", L"date", L"time", L"timestamp", L"created", L"modified", L"accessed", L"downloaded", L"used", L"event_utc", L"start", L"end"})) return DetailGroup::Dates;
    if (containsAnyW(c, {L"path", L"url", L"file", L"folder", L"directory", L"source_db", L"database", L"normalized_path", L"zip_entry", L"location"})) return DetailGroup::Paths;
    if (containsAnyW(c, {L"contact", L"participant", L"thread", L"sender", L"recipient", L"phone", L"email", L"bundle", L"app", L"domain", L"category", L"source_module", L"target"})) return DetailGroup::PeopleApps;
    if (containsAnyW(c, {L"status", L"confidence", L"interpretation", L"warning", L"reason", L"anomaly", L"note", L"residency", L"deleted", L"missing", L"matched"})) return DetailGroup::StatusInterpretation;
    if (containsAnyW(c, {L"id", L"guid", L"inode", L"store", L"row", L"primary", L"source", L"table", L"locator", L"provenance", L"hash", L"sha"})) return DetailGroup::IdentifiersProvenance;
    if (containsAnyW(c, {L"count", L"bytes", L"size", L"rows", L"records", L"duration", L"score"})) return DetailGroup::CountsSizes;
    return DetailGroup::Other;
}

const wchar_t* detailGroupTitle(DetailGroup g) {
    switch (g) {
    case DetailGroup::Text: return L"TEXT / HUMAN-READABLE CONTENT";
    case DetailGroup::Dates: return L"DATES / TIME PROVENANCE";
    case DetailGroup::Paths: return L"PATHS / URLS / DATABASE LOCATIONS";
    case DetailGroup::PeopleApps: return L"PEOPLE / APPS / RECORD CONTEXT";
    case DetailGroup::StatusInterpretation: return L"STATUS / INTERPRETATION / WARNINGS";
    case DetailGroup::IdentifiersProvenance: return L"IDENTIFIERS / PROVENANCE";
    case DetailGroup::CountsSizes: return L"COUNTS / SIZES / METRICS";
    default: return L"OTHER VISIBLE FIELDS";
    }
}

COLORREF detailGroupColor(DetailGroup g) {
    switch (g) {
    case DetailGroup::Text: return RGB(232, 241, 252);
    case DetailGroup::Dates: return RGB(252, 244, 228);
    case DetailGroup::Paths: return RGB(230, 246, 241);
    case DetailGroup::PeopleApps: return RGB(242, 236, 250);
    case DetailGroup::StatusInterpretation: return RGB(252, 232, 232);
    case DetailGroup::IdentifiersProvenance: return RGB(241, 241, 241);
    case DetailGroup::CountsSizes: return RGB(235, 248, 235);
    default: return RGB(248, 248, 248);
    }
}

int selectedOrFocusedReviewRow() {
    if (!gList) return -1;
    int sel = ListView_GetNextItem(gList, -1, LVNI_SELECTED);
    if (sel >= 0) return sel;
    sel = ListView_GetNextItem(gList, -1, LVNI_FOCUSED);
    if (sel >= 0) return sel;
    return -1;
}

std::wstring normalizeDetailValueForTable(std::wstring value) {
    if (value.empty()) return L"(empty)";
    std::wstring out;
    out.reserve(value.size());
    bool lastWasSpace = false;
    for (wchar_t ch : value) {
        if (ch == L'\r') continue;
        const wchar_t current = (ch == L'\n' || ch == L'\t') ? L' ' : ch;
        if (current == L' ') {
            if (lastWasSpace) continue;
            lastWasSpace = true;
        } else {
            lastWasSpace = false;
        }
        out.push_back(current);
    }
    if (!out.empty() && out.front() == L' ') out.erase(out.begin());
    if (!out.empty() && out.back() == L' ') out.pop_back();
    return out.empty() ? L"(empty)" : out;
}

// Detail-list helpers remain in the GUI translation unit because they operate
// directly on the live Win32 ListView handle.  V1.0.22 restores only these
// small rendering helpers; export/database work stays in gui_export_worker.
void ensureDetailsListColumns() {
    if (!gRowDetails) return;
    HWND header = ListView_GetHeader(gRowDetails);
    if (header && Header_GetItemCount(header) >= 2) return;
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.pszText = const_cast<LPWSTR>(L"Field");
    col.cx = 260;
    col.iSubItem = 0;
    ListView_InsertColumn(gRowDetails, 0, &col);
    col.pszText = const_cast<LPWSTR>(L"Metadata / Value");
    col.cx = 900;
    col.iSubItem = 1;
    ListView_InsertColumn(gRowDetails, 1, &col);
}

void resizeDetailsListColumns() {
    if (!gRowDetails) return;
    RECT rc{};
    GetClientRect(gRowDetails, &rc);
    const int width = std::max(500, static_cast<int>(rc.right - rc.left) - GetSystemMetrics(SM_CXVSCROLL) - 8);
    const int fieldW = std::min(360, std::max(210, width / 4));
    ListView_SetColumnWidth(gRowDetails, 0, fieldW);
    ListView_SetColumnWidth(gRowDetails, 1, std::max(260, width - fieldW - 4));
}

void clearDetailsList() {
    if (!gRowDetails) return;
    ensureDetailsListColumns();
    ListView_DeleteAllItems(gRowDetails);
}

void addDetailsListRow(const std::wstring& field, const std::wstring& value, bool section, DetailGroup group = DetailGroup::Other) {
    if (!gRowDetails) return;
    ensureDetailsListColumns();
    const int row = ListView_GetItemCount(gRowDetails);
    LVITEMW item{};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = row;
    item.iSubItem = 0;
    item.pszText = const_cast<LPWSTR>(field.c_str());
    item.lParam = section ? (1000 + static_cast<LPARAM>(group)) : 0;
    ListView_InsertItem(gRowDetails, &item);
    std::wstring normalizedValue = section ? L"" : normalizeDetailValueForTable(value);
    ListView_SetItemText(gRowDetails, row, 1, const_cast<LPWSTR>(normalizedValue.c_str()));
}

void setDetailsPaneMessage(const std::wstring& message) {
    clearDetailsList();
    addDetailsListRow(L"Status", message, false);
}

void appendDetailsSectionToList(DetailGroup g, const std::vector<DetailField>& fields) {
    addDetailsListRow(detailGroupTitle(g), L"", true, g);
    if (fields.empty()) {
        addDetailsListRow(L"(No visible fields)", L"", false);
        return;
    }
    for (const auto& f : fields) {
        addDetailsListRow(f.column, f.value, false, f.group);
    }
}

void updateRowDetailsPanel(int forcedRow) {
    if (!gRowDetails) return;
    if (!gList) {
        setDetailsPaneMessage(L"Open a case and select an investigation row to view all fields here.");
        return;
    }

    int sel = forcedRow;
    const int rowCount = gList ? ListView_GetItemCount(gList) : 0;
    if (sel < 0) sel = selectedOrFocusedReviewRow();
    if (sel < 0 || sel >= rowCount) {
        setDetailsPaneMessage(L"Select a row to view all fields for the current investigation result. This pane is a two-column Field / Metadata table and can be resized by dragging the divider above it.");
        return;
    }

    const int viewIdx = selectedViewIndex();
    if (viewIdx < 0 || viewIdx >= static_cast<int>(views().size())) {
        setDetailsPaneMessage(L"Selected view is unavailable.");
        return;
    }
    const auto& v = views()[static_cast<std::size_t>(viewIdx)];

    std::map<DetailGroup, std::vector<DetailField>> grouped;
    for (std::size_t c = 0; c < v.columns.size(); ++c) {
        DetailField f;
        f.column = widen(v.columns[c]);
        f.value = listViewText(gList, sel, static_cast<int>(c + 2));
        if (f.value.empty()) f.value = L"(empty)";
        f.group = classifyDetailField(f.column);
        f.originalIndex = c;
        grouped[f.group].push_back(std::move(f));
    }

    const DetailGroup groupOrder[] = {
        DetailGroup::Text,
        DetailGroup::Dates,
        DetailGroup::Paths,
        DetailGroup::PeopleApps,
        DetailGroup::StatusInterpretation,
        DetailGroup::IdentifiersProvenance,
        DetailGroup::CountsSizes,
        DetailGroup::Other
    };

    clearDetailsList();
    const std::wstring viewName = v.displayName ? v.displayName : L"";
    const std::wstring artifactId = (sel < static_cast<int>(gCurrentRowArtifactIds.size())) ? widen(gCurrentRowArtifactIds[static_cast<std::size_t>(sel)]) : L"";
    addDetailsListRow(L"View", viewName, false);
    addDetailsListRow(L"Row", std::to_wstring(sel + 1), false);
    if (!artifactId.empty()) addDetailsListRow(L"Artifact ID", artifactId, false);
    addDetailsListRow(L"Checked", listViewText(gList, sel, 0), false);
    const std::wstring tags = listViewText(gList, sel, 1);
    if (!tags.empty()) addDetailsListRow(L"Tags", tags, false);

    for (DetailGroup g : groupOrder) appendDetailsSectionToList(g, grouped[g]);
    resizeDetailsListColumns();
    if (ListView_GetItemCount(gRowDetails) > 0) ListView_EnsureVisible(gRowDetails, 0, FALSE);
}

std::string selectedArtifactIdFromReview() {
    std::vector<std::string> ids = selectedArtifactIdsFromReview(false);
    return ids.front();
}
long long selectedTagId() {

    int sel = static_cast<int>(SendMessageW(gTagList, LB_GETCURSEL, 0, 0));
    if (sel < 0) return -1;
    return static_cast<long long>(SendMessageW(gTagList, LB_GETITEMDATA, sel, 0));
}
void setTagSummary(const std::wstring& s) { if (gTagSummary) SetWindowTextW(gTagSummary, s.c_str()); }
void refreshTagList() {
    if (!gTagList) return;
    SendMessageW(gTagList, LB_RESETCONTENT, 0, 0);
    if (gOpenedCaseDb.empty()) { setTagSummary(L"Open a case database before managing tags."); return; }
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        sqlite3_stmt* st = nullptr;
        const char* sql = "SELECT tag_id, tag_name FROM investigator_tags ORDER BY lower(tag_name)";
        if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        while (sqlite3_step(st) == SQLITE_ROW) {
            long long id = sqlite3_column_int64(st, 0);
            const unsigned char* raw = sqlite3_column_text(st, 1);
            std::wstring name = raw ? widen(reinterpret_cast<const char*>(raw)) : L"";
            int idx = static_cast<int>(SendMessageW(gTagList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str())));
            SendMessageW(gTagList, LB_SETITEMDATA, idx, static_cast<LPARAM>(id));
        }
        sqlite3_finalize(st);
        setTagSummary(L"Tags refreshed.");
    } catch (const std::exception& ex) {
        setTagSummary(L"ERROR refreshing tags: " + widen(ex.what()));
    }
}
void addTagAction() {
    if (gOpenedCaseDb.empty()) { setTagSummary(L"Open a case database before adding tags."); return; }
    const std::wstring tagNameW = getText(gTagName);
    if (tagNameW.empty()) { setTagSummary(L"Enter a tag name first."); return; }
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        sqlite3_stmt* st = nullptr;
        const char* sql = "INSERT OR IGNORE INTO investigator_tags(tag_name,created_utc,notes) VALUES(?,datetime('now'), '')";
        if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        std::string tagName = narrow(tagNameW);
        sqlite3_bind_text(st, 1, tagName.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) != SQLITE_DONE) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
        sqlite3_finalize(st);
        refreshTagList();
    } catch (const std::exception& ex) { setTagSummary(L"ERROR adding tag: " + widen(ex.what())); }
}
void deleteTagAction() {
    long long tagId = selectedTagId();
    if (tagId < 0) { setTagSummary(L"Select a tag to delete."); return; }
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        char* err = nullptr;
        std::string sql = "DELETE FROM artifact_tags WHERE tag_id=" + std::to_string(tagId) + "; DELETE FROM investigator_tags WHERE tag_id=" + std::to_string(tagId) + ";";
        if (sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) { std::string msg = err ? err : sqlite3_errmsg(db.get()); sqlite3_free(err); throw std::runtime_error(msg); }
        refreshTagList();
    } catch (const std::exception& ex) { setTagSummary(L"ERROR deleting tag: " + widen(ex.what())); }
}
void applyTagToArtifacts(long long tagId, const std::vector<std::string>& artifactIds, bool reloadVisible) {
    if (tagId < 0) throw std::runtime_error("Select a tag first.");
    if (artifactIds.empty()) throw std::runtime_error("No artifact-backed rows were selected or checked.");
    ReadOnlyDb db(gOpenedCaseDb);
    sqlite3_stmt* st = nullptr;
    const char* sql = "INSERT OR IGNORE INTO artifact_tags(artifact_id,tag_id,created_utc) VALUES(?,?,datetime('now'))";
    if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
    size_t applied = 0;
    for (const std::string& artifactId : artifactIds) {
        if (artifactId.empty()) continue;
        sqlite3_bind_int64(st, 1, std::strtoll(artifactId.c_str(), nullptr, 10));
        sqlite3_bind_int64(st, 2, tagId);
        if (sqlite3_step(st) != SQLITE_DONE) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
        sqlite3_reset(st); sqlite3_clear_bindings(st); ++applied;
    }
    sqlite3_finalize(st);
    if (reloadVisible) refreshVisibleTagCells(db.get());
    setTagSummary(L"Tag applied to " + std::to_wstring(static_cast<unsigned long long>(applied)) + L" artifact(s). Checked artifacts remain checked until cleared.");
}
void removeTagFromArtifacts(long long tagId, const std::vector<std::string>& artifactIds, bool reloadVisible) {
    if (tagId < 0) throw std::runtime_error("Select a tag first.");
    if (artifactIds.empty()) throw std::runtime_error("No artifact-backed rows were selected or checked.");
    ReadOnlyDb db(gOpenedCaseDb);
    sqlite3_stmt* st = nullptr;
    const char* sql = "DELETE FROM artifact_tags WHERE artifact_id=? AND tag_id=?";
    if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
    size_t removed = 0;
    for (const std::string& artifactId : artifactIds) {
        if (artifactId.empty()) continue;
        sqlite3_bind_int64(st, 1, std::strtoll(artifactId.c_str(), nullptr, 10));
        sqlite3_bind_int64(st, 2, tagId);
        if (sqlite3_step(st) != SQLITE_DONE) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
        sqlite3_reset(st); sqlite3_clear_bindings(st); ++removed;
    }
    sqlite3_finalize(st);
    if (reloadVisible) refreshVisibleTagCells(db.get());
    setTagSummary(L"Tag removed from " + std::to_wstring(static_cast<unsigned long long>(removed)) + L" artifact(s).");
}
void applyTagAction() {
    try { applyTagToArtifacts(selectedTagId(), selectedArtifactIdsFromReview(true), true); }
    catch (const std::exception& ex) { setTagSummary(L"ERROR applying tag: " + widen(ex.what())); }
}
void removeTagAction() {
    try { removeTagFromArtifacts(selectedTagId(), selectedArtifactIdsFromReview(true), true); }
    catch (const std::exception& ex) { setTagSummary(L"ERROR removing tag: " + widen(ex.what())); }
}
void saveNoteAction() {
    try {
        std::vector<std::string> artifactIds = selectedArtifactIdsFromReview(true);
        const std::string note = narrow(getText(gTagNote));
        if (note.empty()) { setTagSummary(L"Enter note text first."); return; }
        ReadOnlyDb db(gOpenedCaseDb);
        sqlite3_stmt* st = nullptr;
        const char* sql = "INSERT INTO investigator_notes(target_type,target_id,note_text,created_utc,updated_utc) VALUES('artifact',?,?,datetime('now'),datetime('now'))";
        if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        size_t saved = 0;
        for (const std::string& artifactId : artifactIds) {
            if (artifactId.empty()) continue;
            sqlite3_bind_text(st, 1, artifactId.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, note.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) != SQLITE_DONE) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
            sqlite3_reset(st); sqlite3_clear_bindings(st); ++saved;
        }
        sqlite3_finalize(st);
        setTagSummary(L"Note saved for " + std::to_wstring(static_cast<unsigned long long>(saved)) + L" artifact(s).");
    } catch (const std::exception& ex) { setTagSummary(L"ERROR saving note: " + widen(ex.what())); }
}
void clearTaggedList() {
    if (!gTaggedList) return;
    ListView_DeleteAllItems(gTaggedList);
    while (ListView_DeleteColumn(gTaggedList, 0)) {}
}
void showTaggedAction() {
    long long tagId = selectedTagId();
    clearTaggedList();
    if (tagId < 0) { setTagSummary(L"Select a tag to view tagged entries."); return; }
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        const std::vector<const char*> cols = {"artifact_id","tag_name","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","content_type","last_updated_utc","first_used_candidate_utc","last_used_date_utc"};
        for (size_t c = 0; c < cols.size(); ++c) {
            std::wstring name = widen(cols[c]);
            LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM; col.pszText = const_cast<LPWSTR>(name.c_str()); col.cx = (std::string(cols[c]).find("path") != std::string::npos) ? 280 : 135; col.iSubItem = static_cast<int>(c);
            ListView_InsertColumn(gTaggedList, static_cast<int>(c), &col);
        }
        const char* sql = "SELECT a.artifact_id,t.tag_name,a.store_guid,a.inode_num,a.parent_inode_num,a.file_name,a.display_name,a.best_path,a.content_type,a.last_updated_utc,a.first_used_candidate_utc,a.last_used_date_utc FROM artifact_tags at JOIN investigator_tags t ON t.tag_id=at.tag_id JOIN artifacts a ON a.artifact_id=at.artifact_id WHERE at.tag_id=? ORDER BY a.artifact_id LIMIT 5000";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db.get(), sql, -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        sqlite3_bind_int64(st, 1, tagId);
        int row = 0;
        while (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char* firstRaw = sqlite3_column_text(st, 0);
            std::wstring first = firstRaw ? widen(reinterpret_cast<const char*>(firstRaw)) : L"";
            LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = row; item.iSubItem = 0; item.pszText = const_cast<LPWSTR>(first.c_str()); ListView_InsertItem(gTaggedList, &item);
            for (int c = 1; c < sqlite3_column_count(st); ++c) {
                const unsigned char* raw = sqlite3_column_text(st, c);
                std::wstring cell = raw ? widen(reinterpret_cast<const char*>(raw)) : L"";
                ListView_SetItemText(gTaggedList, row, c, const_cast<LPWSTR>(cell.c_str()));
            }
            ++row;
        }
        sqlite3_finalize(st);
        setTagSummary(L"Tagged entries loaded. Showing up to 5,000 rows.");
    } catch (const std::exception& ex) { setTagSummary(L"ERROR loading tagged entries: " + widen(ex.what())); }
}


std::wstring lastNonEmptyLine(const std::wstring& path) {
    std::ifstream in(narrow(path), std::ios::binary);
    std::string line;
    std::string last;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (!line.empty()) last = line;
    }
    return widen(last);
}

int parseProgressPercent(const std::wstring& line, std::wstring& stage, std::wstring& message) {
    stage.clear();
    message.clear();
    if (line.empty()) return -1;
    std::vector<std::wstring> parts;
    std::size_t start = 0;
    while (start <= line.size()) {
        std::size_t pos = line.find(L'\t', start);
        if (pos == std::wstring::npos) {
            parts.push_back(line.substr(start));
            break;
        }
        parts.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    if (parts.size() < 3) return -1;
    int pct = _wtoi(parts[1].c_str());
    if (pct < 0) return -1;
    if (pct > 100) pct = 100;
    stage = parts.size() >= 3 ? parts[2] : L"";
    message = parts.size() >= 4 ? parts[3] : L"";
    return pct;
}

std::wstring formatElapsedSeconds(unsigned long long seconds) {
    unsigned long long h = seconds / 3600;
    unsigned long long m = (seconds % 3600) / 60;
    unsigned long long sec = seconds % 60;
    std::wostringstream os;
    if (h) os << h << L"h ";
    os << m << L"m " << sec << L"s";
    return os.str();
}

unsigned long long fileSizeBytesNoThrow(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return 0;
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return 0;
    ULARGE_INTEGER uli{};
    uli.HighPart = data.nFileSizeHigh;
    uli.LowPart = data.nFileSizeLow;
    return static_cast<unsigned long long>(uli.QuadPart);
}

std::wstring formatBytesPerSecond(unsigned long long bytes, unsigned long long seconds) {
    if (bytes == 0 || seconds == 0) return L"not measurable for this source";
    double mb = static_cast<double>(bytes) / 1000000.0;
    double rate = mb / static_cast<double>(seconds);
    std::wostringstream os;
    os.setf(std::ios::fixed);
    os.precision(2);
    os << mb << L" MB source; elapsed average " << rate << L" MB/s";
    return os.str();
}

std::wstring formatGbProcessedOfTotal(unsigned long long totalBytes, int percent) {
    if (totalBytes == 0 || percent < 0) return L"";
    if (percent > 100) percent = 100;
    const double totalGb = static_cast<double>(totalBytes) / 1000000000.0;
    const double doneGb = totalGb * (static_cast<double>(percent) / 100.0);
    std::wostringstream os;
    os.setf(std::ios::fixed);
    os.precision(1);
    os << doneGb << L" GB of " << totalGb << L" GB processed (stage estimate)";
    return os.str();
}

std::wstring stageDisplayName(const std::wstring& stage) {
    if (stage == L"enrichment_start") return L"enrichment started";
    if (stage == L"enrichment_complete") return L"enrichment complete";
    if (stage == L"export_start") return L"export started";
    if (stage == L"export_profile_start") return L"export profile started";
    if (stage == L"export_query_prepare") return L"preparing export";
    if (stage == L"export_query_execute") return L"running export SQL";
    if (stage == L"export_query_rows") return L"writing export rows";
    if (stage == L"export_query_complete") return L"export file complete";
    if (stage == L"export_upload_samples_start") return L"upload sample exports started";
    if (stage == L"export_upload_samples_complete") return L"upload sample exports complete";
    if (stage == L"export_complete") return L"exports complete";
    if (stage == L"upload_bundle_start") return L"building focused Upload folder";
    if (stage == L"upload_bundle_complete") return L"focused Upload folder ready";
    if (stage == L"complete_success") return L"complete";
    return stage;
}

void worker() {
    EnableWindow(gRun, FALSE);
    setCaseMutationControlsEnabled(false);
    postClearProcessLog();
    postProgress(1);
    const auto ingestStart = std::chrono::steady_clock::now();
    const unsigned long long inputBytesForRate = fileSizeBytesNoThrow(getText(gInput));
    postStatus(L"Ingest running: initializing options.");
    postLog(L"Starting native C++ Spotlight workflow...");
    postLog(L"Processing telemetry: elapsed time and progress stages will be mirrored here for review. Throughput is shown in decimal MB when the selected source is a measurable file such as a ZIP, AFF4, IMG, DD, or RAW.");
    if (inputBytesForRate > 0) postLog(L"Input size for throughput estimate: " + std::to_wstring(inputBytesForRate) + L" bytes.");
    std::atomic_bool monitorDone{false};
    std::thread monitor;
    try {
        RunOptions opt;
        opt.input = narrow(getText(gInput));
        opt.output = narrow(getText(gOut));
        gCrashCaseDir = getText(gOut);
        const std::wstring statusPath = getText(gOut) + L"\\logs\\run_status.txt";
        const std::wstring progressPath = getText(gOut) + L"\\logs\\last_progress.tsv";
        const std::wstring rootProgressPath = getText(gOut) + L"\\last_progress.tsv";
        monitor = std::thread([statusPath, progressPath, rootProgressPath, ingestStart, inputBytesForRate, &monitorDone]() {
            std::wstring lastStatus;
            std::wstring lastProgress;
            int lastProgressPercent = -1;
            unsigned long long lastHeartbeatSecond = 0;
            while (!monitorDone.load()) {
                const std::wstring currentStatus = lastNonEmptyLine(statusPath);
                if (!currentStatus.empty() && currentStatus != lastStatus) {
                    lastStatus = currentStatus;
                    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ingestStart).count();
                    postStatus(L"Ingest status: " + currentStatus + L" | elapsed " + formatElapsedSeconds(static_cast<unsigned long long>(elapsed)));
                    postLog(L"Status: " + currentStatus + L" | elapsed " + formatElapsedSeconds(static_cast<unsigned long long>(elapsed)));
                }
                std::wstring currentProgress = lastNonEmptyLine(progressPath);
                if (currentProgress.empty()) currentProgress = lastNonEmptyLine(rootProgressPath);
                if (!currentProgress.empty() && currentProgress != lastProgress) {
                    lastProgress = currentProgress;
                    std::wstring stage, message;
                    int pct = parseProgressPercent(currentProgress, stage, message);
                    if (pct >= 0) {
                        lastProgressPercent = pct;
                        postProgress(pct);
                        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ingestStart).count();
                        std::wstring status = L"Ingest running: " + std::to_wstring(pct) + L"% | elapsed " + formatElapsedSeconds(static_cast<unsigned long long>(elapsed));
                        const std::wstring gbProcessed = formatGbProcessedOfTotal(inputBytesForRate, pct);
                        if (!gbProcessed.empty()) status += L" | " + gbProcessed;
                        if (!stage.empty()) status += L" - " + stageDisplayName(stage);
                        if (!message.empty()) status += L" - " + message;
                        postStatus(status);
                        postLog(status);
                    }
                }
                const auto elapsedNow = static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ingestStart).count());
                if (elapsedNow >= 5 && elapsedNow / 5 != lastHeartbeatSecond / 5) {
                    lastHeartbeatSecond = elapsedNow;
                    std::wstring heartbeat = L"Still processing | elapsed " + formatElapsedSeconds(elapsedNow);
                    const std::wstring gbProcessed = formatGbProcessedOfTotal(inputBytesForRate, lastProgressPercent);
                    if (!gbProcessed.empty()) heartbeat += L" | " + gbProcessed;
                    if (inputBytesForRate > 0) heartbeat += L" | " + formatBytesPerSecond(inputBytesForRate, elapsedNow);
                    if (!lastStatus.empty()) heartbeat += L" | last stage " + stageDisplayName(lastStatus);
                    postLog(heartbeat);
                }
                Sleep(750);
            }
        });
        opt.caseDir = opt.output;
        opt.evidenceRoot = narrow(getText(gEvidenceRoot));
        opt.sevenZipPath = narrow(getText(gSevenZip));
        opt.caseName = narrow(getText(gCaseName));
        opt.caseNumber = narrow(getText(gCaseNumber));
        opt.company = narrow(getText(gCompany));
        opt.investigator = narrow(getText(gInvestigator));
        opt.preserveEvidence = true;
        opt.preserveEvidenceExplicit = true;
        opt.decodeCoreNativeValues = true;
        opt.experimentalFullNativeValues = true;
        opt.maxNativeRecords = 0;
        opt.maxNativeBlocks = 0;
        opt.verbose = SendMessageW(gVerbose, BM_GETCHECK, 0, 0) == BST_CHECKED;
        int exportProfile = static_cast<int>(SendMessageW(gExportProfile, CB_GETCURSEL, 0, 0));
        opt.exportProfile = exportProfile == 1 ? "investigator" : exportProfile == 2 ? "diagnostics" : exportProfile == 3 ? "full" : "minimal";
        opt.suppressCsvExports = (gSuppressCsvExports && SendMessageW(gSuppressCsvExports, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (opt.suppressCsvExports) {
            opt.exportProfile = "none";
            postLog(L"CSV review exports disabled for this run. SQLite case database will remain available for GUI review.");
        }
        int prof = static_cast<int>(SendMessageW(gProfile, CB_GETCURSEL, 0, 0));
        int mode = static_cast<int>(SendMessageW(gMode, CB_GETCURSEL, 0, 0));
        opt.profile = prof == 1 ? "macos" : prof == 2 ? "ios" : "auto";
        opt.mode = mode == 1 ? "diagnostics" : mode == 2 ? "discover" : "run";
        if (prof == 2 && !opt.evidenceRoot.empty()) {
            opt.reuseIosCache = opt.evidenceRoot;
            opt.evidenceRoot.clear();
            opt.skipContainerHash = true;
            postLog(L"iOS/CoreSpotlight profile: Evidence root / iOS cache case field will be used as --reuse-ios-cache for staged source reuse. Full source ZIP hashing is skipped for this cached review run.");
        }
        if (prof == 2) {
            opt.fullScan = true;
            postLog(L"iOS/CoreSpotlight profile selected: GUI enables full-scan/focused extraction behavior; parser selects one primary database per CoreSpotlight store group and preserves store.db/.store.db alternates for review to avoid duplicate record counts.");
        }
        if (opt.experimentalFullNativeValues) postLog(L"Full native metadata parsing is enabled for the selected workflow.");
        else postLog(L"Stable native header/core parsing is enabled. Full native metadata parsing is available through the checkbox above.");
        postStatus(L"Ingest running: parser/enrichment/export workflow active. See log and status line for stages.");
        auto rr = runApplication(opt, &gCancelIngestRequested);
        for (const auto& m : rr.messages) postLog(widen(m));
        std::ostringstream os;
        os << "Complete: store_groups=" << rr.storeCount << " database_candidates=" << rr.databaseCandidateCount << " selected_databases=" << rr.selectedParserDatabaseCount << " artifacts=" << rr.artifactCount << " usage=" << rr.usageCount << " orphan/deleted candidates=" << rr.orphanCandidateCount;
        postLog(widen(os.str()));
        postProgress(100);
        const auto ingestSeconds = static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ingestStart).count());
        postLog(L"Processing elapsed time: " + formatElapsedSeconds(ingestSeconds));
        postLog(L"Processing throughput estimate: " + formatBytesPerSecond(inputBytesForRate, ingestSeconds));
        postStatus(L"Ingest complete: " + widen(os.str()) + L" | elapsed " + formatElapsedSeconds(ingestSeconds));
        const std::wstring dbPath = getText(gOut) + L"\\VestigantSpotlight.case.sqlite";
        postLog(L"SQLite case DB: " + dbPath);
        postLog(L"Evidence preservation folder: " + getText(gOut) + L"\\EvidencePreservation");
        postLog(L"Review exports: " + getText(gOut) + L"\\exports");
        setText(gCaseDbPath, dbPath);
        gOpenedCaseDb = dbPath;
        try { ReadOnlyDb db(gOpenedCaseDb); populateViewList(db.get()); } catch (...) { populateViewList(); }
        loadCaseSummary();
        refreshTagList();
    } catch (const std::exception& ex) { postLog(L"ERROR: " + widen(ex.what())); postStatus(L"Ingest failed: " + widen(ex.what())); writeGuiCrashFile(L"Caught std::exception: " + widen(ex.what())); }
    catch (...) { postLog(L"ERROR: Unknown non-standard exception"); postStatus(L"Ingest failed: unknown non-standard exception"); writeGuiCrashFile(L"Caught unknown non-standard exception"); }
    monitorDone.store(true);
    if (monitor.joinable()) monitor.join();
    EnableWindow(gRun, TRUE);
    setCaseMutationControlsEnabled(true);
}


bool isInvestigationTabIndex(int index) {
    return index == 1 || index == 2;
}

void enforceDetailsPaneTabVisibility() {
    const int cmd = isInvestigationTabIndex(gActiveTabIndex) ? SW_SHOW : SW_HIDE;
    if (gRowDetailsSplitter) ShowWindow(gRowDetailsSplitter, cmd);
    if (gRowDetailsLabel) ShowWindow(gRowDetailsLabel, cmd);
    if (gRowDetails) ShowWindow(gRowDetails, cmd);
}

void showTab(int index) {
    gActiveTabIndex = index;
    if (index == 1) gIosReviewMode = false;
    else if (index == 2) gIosReviewMode = true;
    for (HWND h : gProcessControls) ShowWindow(h, index == 0 ? SW_SHOW : SW_HIDE);
    const bool showSharedReview = isInvestigationTabIndex(index);
    for (HWND h : gReviewControls) ShowWindow(h, showSharedReview ? SW_SHOW : SW_HIDE);
    enforceDetailsPaneTabVisibility();
    for (HWND h : gIosControls) ShowWindow(h, SW_HIDE);
    for (HWND h : gTagControls) ShowWindow(h, index == 3 ? SW_SHOW : SW_HIDE);
    if (index == 1 || index == 2) populateViewListForCurrentContextNoThrow();
    if (index == 3) refreshTagList();
}

void createProcessControls(HWND hwnd) {
    const int y0 = 58;
    // V1.6.18 compact Case Information / Build Processing header: keep the same controls, but reduce
    // row heights, drop-down extents, explanatory text height, and action-row
    // button sizes so the ingest log/table area starts materially higher.
    gLogo = addProcess(CreateWindowW(L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE, 16, y0, 56, 56, hwnd, nullptr, gInst, nullptr));
    const std::wstring logoPath = logoBitmapPath();
    if (!logoPath.empty()) {
        gLogoBitmap = static_cast<HBITMAP>(LoadImageW(nullptr, logoPath.c_str(), IMAGE_BITMAP, 56, 56, LR_LOADFROMFILE));
        if (gLogoBitmap) SendMessageW(gLogo, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(gLogoBitmap));
    }
    gBrandTitle = addProcess(CreateWindowW(L"STATIC", L"Vestigant Spotlight", WS_CHILD | WS_VISIBLE | SS_LEFT, 88, y0, 420, 24, hwnd, nullptr, gInst, nullptr));
    gBrandSubtitle = addProcess(CreateWindowW(L"STATIC", L"Forensic Spotlight/CoreSpotlight investigation workflow", WS_CHILD | WS_VISIBLE | SS_LEFT, 88, y0 + 24, 720, 20, hwnd, nullptr, gInst, nullptr));

    addProcess(CreateWindowW(L"STATIC", L"Case Information", WS_CHILD | WS_VISIBLE, 16, y0 + 58, 720, 20, hwnd, nullptr, gInst, nullptr));
    labelP(hwnd, L"Case Name", 16, y0 + 84, 112, 20); gCaseName = editP(hwnd, 128, y0 + 80, 330, 22, L"Spotlight Case"); SetWindowLongPtrW(gCaseName, GWLP_ID, ID_CASE_NAME_EDIT);
    labelP(hwnd, L"Case Number", 478, y0 + 84, 112, 20); gCaseNumber = editP(hwnd, 590, y0 + 80, 220, 22); SetWindowLongPtrW(gCaseNumber, GWLP_ID, ID_CASE_NUMBER_EDIT);
    labelP(hwnd, L"Investigator", 16, y0 + 112, 112, 20); gInvestigator = editP(hwnd, 128, y0 + 108, 330, 22); SetWindowLongPtrW(gInvestigator, GWLP_ID, ID_INVESTIGATOR_EDIT);
    labelP(hwnd, L"Company", 478, y0 + 112, 112, 20); gCompany = editP(hwnd, 590, y0 + 108, 220, 22); SetWindowLongPtrW(gCompany, GWLP_ID, ID_COMPANY_EDIT);
    labelP(hwnd, L"Case Location", 16, y0 + 140, 112, 20); gOut = editP(hwnd, 128, y0 + 136, 704, 22); SetWindowLongPtrW(gOut, GWLP_ID, ID_CASE_LOCATION_EDIT); gBrowseOut = buttonP(hwnd, L"Browse", ID_BROWSE_OUT, 842, y0 + 135, 90, 24);
    labelP(hwnd, L"Case Database", 16, y0 + 168, 112, 20); gCaseDbPath = editP(hwnd, 128, y0 + 164, 662, 22); SetWindowLongPtrW(gCaseDbPath, GWLP_ID, ID_CASE_DB_EDIT); gBrowseCase = buttonP(hwnd, L"Browse", ID_BROWSE_CASE, 800, y0 + 163, 90, 24); gOpenCase = openCaseButtonP(hwnd, 898, y0 + 161, 96, 28);
    gSaveCaseInfo = buttonP(hwnd, L"Save Case Info", ID_SAVE_CASE_INFO, 128, y0 + 192, 132, 24);
    gCaseAutosaveStatus = addProcess(CreateWindowW(L"STATIC", L"Autosave ready", WS_CHILD | WS_VISIBLE | SS_LEFT, 272, y0 + 195, 420, 20, hwnd, nullptr, gInst, nullptr));

    addProcess(CreateWindowW(L"STATIC", L"Build / Processing", WS_CHILD | WS_VISIBLE, 16, y0 + 224, 720, 20, hwnd, nullptr, gInst, nullptr));
    labelP(hwnd, L"Source type", 16, y0 + 252, 86, 20); gSourceType = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 104, y0 + 248, 168, 96, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SOURCE_TYPE)), gInst, nullptr));
    for (const wchar_t* s : {L"Folder", L"ZIP", L"AFF4/APFS image (staged)", L"Raw IMG/DD image (staged)"}) SendMessageW(gSourceType, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gSourceType, CB_SETCURSEL, 0, 0);
    labelP(hwnd, L"Raw evidence source", 282, y0 + 252, 136, 20); gInput = editP(hwnd, 420, y0 + 248, 452, 22); gBrowseInput = buttonP(hwnd, L"Browse", ID_BROWSE_INPUT, 882, y0 + 247, 100, 24);
    labelP(hwnd, L"Evidence root / iOS cache", 16, y0 + 282, 208, 20); gEvidenceRoot = editP(hwnd, 224, y0 + 278, 648, 22); gBrowseRoot = buttonP(hwnd, L"Browse", ID_BROWSE_ROOT, 882, y0 + 277, 100, 24);
    labelP(hwnd, L"7z.exe path", 16, y0 + 312, 204, 20); gSevenZip = editP(hwnd, 224, y0 + 308, 648, 22); gBrowse7z = buttonP(hwnd, L"Browse", ID_BROWSE_7Z, 882, y0 + 307, 100, 24);
    addProcess(CreateWindowW(L"STATIC", L"Source support: Folder/ZIP are production intake; AFF4/APFS and Raw IMG/DD are staged macOS image workflows.", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, y0 + 336, 966, 26, hwnd, nullptr, gInst, nullptr));

    labelP(hwnd, L"Profile", 16, y0 + 374, 70, 20); gProfile = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 88, y0 + 370, 160, 96, hwnd, nullptr, gInst, nullptr));
    for (const wchar_t* s : {L"Auto", L"Standard macOS", L"iOS/CoreSpotlight"}) SendMessageW(gProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gProfile, CB_SETCURSEL, 0, 0);
    labelP(hwnd, L"Mode", 258, y0 + 374, 48, 20); gMode = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 306, y0 + 370, 246, 96, hwnd, nullptr, gInst, nullptr));
    for (const wchar_t* s : {L"Process Raw Spotlight Evidence", L"Diagnostics / Bounded Native Parse", L"Discover Stores Only"}) SendMessageW(gMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gMode, CB_SETCURSEL, 0, 0);
    gRun = buttonP(hwnd, L"Build / Process", ID_RUN, 570, y0 + 367, 172, 30);
    gCancelIngestButton = buttonP(hwnd, L"Cancel", ID_CANCEL_INGEST, 750, y0 + 367, 116, 30);
    EnableWindow(gCancelIngestButton, FALSE);
    addProcess(CreateWindowW(L"STATIC", L"Evidence preservation and core/native metadata decoding are always enabled for GUI runs.", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, y0 + 404, 560, 20, hwnd, nullptr, gInst, nullptr));
    gVerbose = addProcess(CreateWindowW(L"BUTTON", L"Verbose log", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 16, y0 + 428, 110, 22, hwnd, nullptr, gInst, nullptr));
    SendMessageW(gVerbose, BM_SETCHECK, BST_CHECKED, 0);
    gSuppressCsvExports = addProcess(CreateWindowW(L"BUTTON", L"SQLite only", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 136, y0 + 428, 110, 22, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SUPPRESS_CSV_EXPORTS)), gInst, nullptr));
    labelP(hwnd, L"Export profile", 590, y0 + 430, 92, 20); gExportProfile = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 684, y0 + 426, 144, 92, hwnd, nullptr, gInst, nullptr));
    for (const wchar_t* s : {L"Minimal", L"Investigator", L"Diagnostics", L"Full CSV"}) SendMessageW(gExportProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gExportProfile, CB_SETCURSEL, 1, 0);

    labelP(hwnd, L"Ingest status", 16, y0 + 462, 112, 20);
    gIngestStatus = addProcess(CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Waiting: choose/create a case location and evidence source, then run ingest. Progress, elapsed time, and throughput estimates appear here and in the processing log below.", WS_CHILD | WS_VISIBLE | SS_LEFT, 128, y0 + 456, 854, 42, hwnd, nullptr, gInst, nullptr));
    labelP(hwnd, L"Progress", 16, y0 + 508, 112, 20);
    gIngestProgress = addProcess(CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 128, y0 + 504, 854, 20, hwnd, nullptr, gInst, nullptr));
    SendMessageW(gIngestProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(gIngestProgress, PBM_SETPOS, 0, 0);
    gLog = addProcess(CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 16, y0 + 532, 966, 224, hwnd, nullptr, gInst, nullptr));
    appendLog(L"V1.6.18 compact GUI layout: case/build controls use shorter rows, smaller action buttons, and reduced combo drop-down extents.");
    appendLog(L"Processing log mirrors status/progress updates with elapsed time; ZIP/image/file source throughput is estimated in MB when measurable.");
}

LRESULT CALLBACK ReviewListSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_KEYDOWN && wp == VK_SPACE) {
        std::vector<int> rows = selectedReviewRows();
        if (!rows.empty()) {
            toggleReviewRowsAsBatch(rows);
            if (rows.size() == 1) focusReviewRow(rows.front() + 1, true);
            setReviewSummary(L"Toggled selected row checkmark(s). Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size())));
            updateRowDetailsPanel();
            return 0;
        }
    }
    if (msg == WM_LBUTTONDOWN) {
        POINT pt{static_cast<LONG>(GET_X_LPARAM(lp)), static_cast<LONG>(GET_Y_LPARAM(lp))};
        LVHITTESTINFO hit{}; hit.pt = pt;
        ListView_SubItemHitTest(hwnd, &hit);
        if (hit.iItem >= 0 && hit.iSubItem == 0) {
            toggleReviewRowChecked(hit.iItem);
            focusReviewRow(hit.iItem, true);
            setReviewSummary(L"Toggled checked state. Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size())));
            updateRowDetailsPanel(hit.iItem);
            return 0;
        }
    }
    if (msg == WM_LBUTTONUP) {
        LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);
        POINT pt{static_cast<LONG>(GET_X_LPARAM(lp)), static_cast<LONG>(GET_Y_LPARAM(lp))};
        LVHITTESTINFO hit{}; hit.pt = pt;
        ListView_SubItemHitTest(hwnd, &hit);
        if (hit.iItem >= 0) updateRowDetailsPanel(hit.iItem);
        return res;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}


LRESULT CALLBACK ReviewDetailsSplitterSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        return TRUE;
    case WM_LBUTTONDOWN: {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ClientToScreen(hwnd, &pt);
        gReviewDetailsSplitterDragging = true;
        gReviewDetailsSplitterDragStartY = pt.y;
        gReviewDetailsPaneHeightAtDragStart = gReviewDetailsPaneHeight;
        SetCapture(hwnd);
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        return 0;
    }
    case WM_MOUSEMOVE:
        if (gReviewDetailsSplitterDragging && (wp & MK_LBUTTON)) {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ClientToScreen(hwnd, &pt);
            const int delta = pt.y - gReviewDetailsSplitterDragStartY;
            gReviewDetailsPaneHeight = std::max(kReviewDetailsMinHeight, gReviewDetailsPaneHeightAtDragStart - delta);
            if (HWND parent = GetParent(hwnd)) layoutControls(parent);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
        gReviewDetailsSplitterDragging = false;
        if (GetCapture() == hwnd) ReleaseCapture();
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void createReviewControls(HWND hwnd) {
    const int y0 = 58;
    addReview(CreateWindowW(L"STATIC", L"Investigation Results Grid: choose a platform-scoped review view from the left, then search/filter/sort/export database-backed results.", WS_CHILD | WS_VISIBLE, 16, y0, 960, 20, hwnd, nullptr, gInst, nullptr));
    labelR(hwnd, L"View Set", 16, y0 + 26, 64, 20);
    gReviewViewProfile = addReview(CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 96, y0 + 22, 150, 96, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_REVIEW_VIEW_PROFILE)), gInst, nullptr));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Recommended V1"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Timeline / Activity"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Text / Content"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"App DB / KnowledgeC"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Diagnostics / Coverage"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Show All"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Custom"));
    SendMessageW(gReviewViewProfile, CB_SETCURSEL, 0, 0);
    applyUiFont(gReviewViewProfile);
    labelR(hwnd, L"Views", 16, y0 + 50, 220, 20);
    gReviewView = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY, 16, y0 + 72, 220, 580, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_REVIEW_VIEW)), gInst, nullptr));
    SetWindowSubclass(gReviewView, ReviewViewListSubclassProc, 2, 0);
    gViewSetUp = buttonR(hwnd, L"Move Up", ID_VIEWSET_UP, 16, y0 + 674, 70, 24);
    gViewSetDown = buttonR(hwnd, L"Move Down", ID_VIEWSET_DOWN, 92, y0 + 674, 80, 24);
    gViewSetHide = buttonR(hwnd, L"Hide", ID_VIEWSET_HIDE, 178, y0 + 674, 58, 24);
    gViewSetSave = buttonR(hwnd, L"Save Set", ID_VIEWSET_SAVE, 16, y0 + 704, 86, 24);
    gViewSetReset = buttonR(hwnd, L"Reset Set", ID_VIEWSET_RESET, 108, y0 + 704, 86, 24);
    // V0_9_17: do not create balloon tooltips for the view list. Hover updates
    // the help/summary panel above the table instead, which is less intrusive.
    populateViewList();

    labelR(hwnd, L"Search", 248, y0 + 26, 54, 20); gSearch = editR(hwnd, 304, y0 + 22, 250, 22);
    gRefresh = buttonR(hwnd, L"Update", ID_REVIEW_REFRESH, 564, y0 + 21, 80, 24);
    gCancelLoad = buttonR(hwnd, L"Cancel Load", ID_REVIEW_CANCEL_LOAD, 650, y0 + 21, 96, 24);
    EnableWindow(gCancelLoad, FALSE);
    labelR(hwnd, L"Rows", 756, y0 + 26, 42, 20); gPageSize = editR(hwnd, 798, y0 + 22, 56, 22, L"250");
    gPrev = buttonR(hwnd, L"Previous", ID_REVIEW_PREV, 248, y0 + 52, 96, 24);
    gNext = buttonR(hwnd, L"Next", ID_REVIEW_NEXT, 350, y0 + 52, 88, 24);
    gExportPage = buttonR(hwnd, L"Export Page", ID_EXPORT_PAGE, 444, y0 + 52, 104, 24);
    gExportFiltered = buttonR(hwnd, L"Export Filtered", ID_EXPORT_FILTERED, 554, y0 + 52, 118, 24);
    gExportVisible = buttonR(hwnd, L"Export Views", ID_EXPORT_VISIBLE_VIEWS, 678, y0 + 52, 106, 24);
    gExportChecked = buttonR(hwnd, L"Export Checked", ID_EXPORT_CHECKED, 790, y0 + 52, 118, 24);
    gClearChecked = buttonR(hwnd, L"Clear Checked", ID_CLEAR_CHECKED, 248, y0 + 80, 108, 24);
    gManageTags = buttonR(hwnd, L"Tags", ID_CTX_MANAGE_TAGS, 362, y0 + 80, 60, 24);
    gOpenCaseFolder = buttonR(hwnd, L"Case Folder", ID_OPEN_CASE_FOLDER, 428, y0 + 80, 106, 24);
    gOpenUploadFolder = buttonR(hwnd, L"Upload Folder", ID_OPEN_UPLOAD_FOLDER, 540, y0 + 80, 118, 24);
    gOpenLogsFolder = buttonR(hwnd, L"Logs", ID_OPEN_LOGS_FOLDER, 664, y0 + 80, 70, 24);
    gOpenDashboard = buttonR(hwnd, L"Dashboard", ID_OPEN_DASHBOARD, 740, y0 + 80, 108, 24);
    gOpenReviewIndex = buttonR(hwnd, L"Review Index", ID_OPEN_REVIEW_INDEX, 854, y0 + 80, 118, 24);
    gReviewSummary = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"No case opened. Open a database from the Case Information tab.", WS_CHILD | WS_VISIBLE | SS_LEFT, 248, y0 + 110, 734, 44, hwnd, nullptr, gInst, nullptr));
    gReviewBusy = addReview(CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | PBS_MARQUEE, 248, y0 + 158, 734, 6, hwnd, nullptr, gInst, nullptr));
    if (gReviewBusy) ShowWindow(gReviewBusy, SW_HIDE);
    gList = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS, 248, y0 + 168, 734, 500, hwnd, nullptr, gInst, nullptr));
    ListView_SetExtendedListViewStyle(gList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    SetWindowSubclass(gList, ReviewListSubclassProc, 1, 0);
    gRowDetailsSplitter = addReview(CreateWindowW(L"STATIC", L"", WS_CHILD | SS_ETCHEDHORZ | SS_NOTIFY, 248, y0 + 666, 734, 8, hwnd, nullptr, gInst, nullptr));
    SetWindowSubclass(gRowDetailsSplitter, ReviewDetailsSplitterSubclassProc, 1, 0);
    gRowDetailsLabel = addReview(CreateWindowW(L"STATIC", L"Selected Row Details - true two-column Field / Metadata table", WS_CHILD, 248, y0 + 674, 734, 20, hwnd, nullptr, gInst, nullptr));
    gRowDetails = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
        248, y0 + 696, 734, 130, hwnd, nullptr, gInst, nullptr));
    if (gRowDetails) {
        ListView_SetExtendedListViewStyle(gRowDetails, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        ensureDetailsListColumns();
        setDetailsPaneMessage(L"Select a row to view all fields for the current investigation result. Field names are shown in the left column and metadata values are shown in the right column.");
    }
}

void createTagControls(HWND hwnd) {
    const int y0 = 58;
    addTag(CreateWindowW(L"STATIC", L"Tags / Notes: create tags, apply them to selected investigation rows, save artifact notes, and review tagged entries.", WS_CHILD | WS_VISIBLE, 16, y0, 960, 22, hwnd, nullptr, gInst, nullptr));
    labelT(hwnd, L"Tags", 16, y0 + 34, 220, 22);
    gTagList = addTag(CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY, 16, y0 + 60, 230, 230, hwnd, nullptr, gInst, nullptr));
    labelT(hwnd, L"New Tag", 16, y0 + 302, 80, 22); gTagName = editT(hwnd, 100, y0 + 298, 146, 26);
    buttonT(hwnd, L"Add", ID_ADD_TAG, 16, y0 + 332, 70, 28);
    buttonT(hwnd, L"Delete", ID_DELETE_TAG, 94, y0 + 332, 70, 28);
    buttonT(hwnd, L"Refresh", ID_REFRESH_TAGS, 172, y0 + 332, 74, 28);
    buttonT(hwnd, L"Apply Tag to Checked/Selected", ID_APPLY_TAG, 262, y0 + 60, 220, 30);
    buttonT(hwnd, L"Remove Tag from Checked/Selected", ID_REMOVE_TAG, 492, y0 + 60, 240, 30);
    buttonT(hwnd, L"View Tagged Entries", ID_SHOW_TAGGED, 742, y0 + 60, 150, 30);
    buttonT(hwnd, L"Export Tag + Support", ID_EXPORT_TAGGED, 742, y0 + 96, 180, 30);
    labelT(hwnd, L"Note for checked/selected artifact(s)", 262, y0 + 104, 200, 22);
    gTagNote = addTag(CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 262, y0 + 130, 470, 92, hwnd, nullptr, gInst, nullptr));
    buttonT(hwnd, L"Save Note", ID_SAVE_NOTE, 742, y0 + 130, 150, 30);
    gTagSummary = addTag(CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Open a case database before using tags.", WS_CHILD | WS_VISIBLE | SS_LEFT, 262, y0 + 236, 630, 54, hwnd, nullptr, gInst, nullptr));
    gTaggedList = addTag(CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, 262, y0 + 304, 720, 274, hwnd, nullptr, gInst, nullptr));
    ListView_SetExtendedListViewStyle(gTaggedList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
}

void createIosControls(HWND hwnd) {
    const int y0 = 58;
    addIos(CreateWindowW(L"STATIC", L"iOS Investigation View", WS_CHILD | WS_VISIBLE, 16, y0, 720, 26, hwnd, nullptr, gInst, nullptr));
    gIosStatus = addIos(CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC",
        L"iOS Spotlight/CoreSpotlight review is Spotlight-first. Use this tab to open recovered Spotlight text, dates, entities, and FFS-overlap views. Full FFS/app DB inventories remain supporting exports and are not default review views.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 16, y0 + 38, 960, 112, hwnd, nullptr, gInst, nullptr));
    buttonI(hwnd, L"Spotlight Timeline", ID_IOS_OPEN_STORE_SUMMARY, 16, y0 + 166, 160, 32);
    buttonI(hwnd, L"Spotlight Entities", ID_IOS_OPEN_STRING_VALUES, 186, y0 + 166, 160, 32);
    buttonI(hwnd, L"Entity Summary", ID_IOS_OPEN_STRING_SUMMARY, 356, y0 + 166, 150, 32);
    buttonI(hwnd, L"Date Evidence", ID_IOS_OPEN_INDEX_TIMELINE, 516, y0 + 166, 150, 32);
    buttonI(hwnd, L"FFS Overlap", ID_IOS_OPEN_ARTIFACTS, 676, y0 + 166, 130, 32);
    buttonI(hwnd, L"Parser Targets", ID_IOS_OPEN_READINESS, 816, y0 + 166, 150, 32);
    buttonI(hwnd, L"File Refs", ID_IOS_OPEN_KEYWORD_VALUES, 16, y0 + 204, 120, 28);
    buttonI(hwnd, L"URL Refs", ID_IOS_OPEN_FFS_INVENTORY, 146, y0 + 204, 110, 28);
    buttonI(hwnd, L"Account Refs", ID_IOS_OPEN_APP_DATABASES, 266, y0 + 204, 130, 28);
    buttonI(hwnd, L"Referenced Paths", ID_IOS_OPEN_REFERENCED_PATHS, 406, y0 + 204, 150, 28);
    buttonI(hwnd, L"Missing FFS", ID_IOS_OPEN_MISSING_FFS, 566, y0 + 204, 120, 28);
    buttonI(hwnd, L"Residency", ID_IOS_OPEN_DB_RESIDENCY, 696, y0 + 204, 110, 28);
    buttonI(hwnd, L"Store Summary", ID_IOS_OPEN_PARSED_APP_RECORDS, 816, y0 + 204, 130, 28);
    buttonI(hwnd, L"View Focus", ID_IOS_OPEN_PLAN, 16, y0 + 238, 120, 28);
    addIos(CreateWindowW(L"STATIC", L"iOS Spotlight review focus", WS_CHILD | WS_VISIBLE, 16, y0 + 274, 360, 24, hwnd, nullptr, gInst, nullptr));
    gIosReadiness = addIos(CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC",
        L"Primary review path:\r\n"
        L"  - Spotlight/CoreSpotlight recovered values\r\n"
        L"  - Spotlight date provenance and validation locators\r\n"
        L"  - Spotlight entities: URL, file/path, account/contact, communication text\r\n"
        L"  - FFS overlap/missing-unresolved candidates for Spotlight references\r\n\r\n"
        L"Supporting data:\r\n"
        L"  - FFS and app DB inventories are retained for correlation/export but are not the main review surface.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 16, y0 + 304, 460, 270, hwnd, nullptr, gInst, nullptr));
    addIos(CreateWindowW(L"STATIC", L"Current interpretation cautions", WS_CHILD | WS_VISIBLE, 500, y0 + 274, 440, 24, hwnd, nullptr, gInst, nullptr));
    gIosPlan = addIos(CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC",
        L"Cautions:\r\n"
        L"  - CoreSpotlight Last_Updated is index/update metadata unless another decoded field supports activity.\r\n"
        L"  - Missing from FFS includes same-record Spotlight text context where available; use it to assess investigative value. Missing from FFS is not proof of deletion; acquisition scope, cloud/offload, protection, and parser limits remain possible.\r\n"
        L"  - App database records are corroborating context; Spotlight records remain the primary data source in this tab.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 500, y0 + 304, 476, 270, hwnd, nullptr, gInst, nullptr));
}

void moveIf(HWND h, int x, int y, int w, int hgt) {
    if (!h) return;
    MoveWindow(h, x, y, std::max(10, w), std::max(10, hgt), TRUE);
}

void stabilizeReviewControlZOrder(HWND hwnd) {
    if (!isInvestigationTabIndex(gActiveTabIndex)) return;
    if (gList) SetWindowPos(gList, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (gRowDetailsSplitter) SetWindowPos(gRowDetailsSplitter, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (gRowDetailsLabel) SetWindowPos(gRowDetailsLabel, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (gRowDetails) SetWindowPos(gRowDetails, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (gReviewViewProfile) {
        ShowWindow(gReviewViewProfile, SW_SHOW);
        SetWindowPos(gReviewViewProfile, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    InvalidateRect(hwnd, nullptr, TRUE);
}

void layoutControls(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int cw = std::max(1030, static_cast<int>(rc.right - rc.left));
    const int ch = std::max(780, static_cast<int>(rc.bottom - rc.top));
    const int right = cw - 16;
    const int bottom = ch - 18;
    if (gTab) MoveWindow(gTab, 8, 8, cw - 16, ch - 16, TRUE);

    const int y0 = 58;
    const int browseW = 98;
    const int rightBrowseX = right - browseW;

    moveIf(gLogo, 16, y0, 56, 56);
    moveIf(gBrandTitle, 88, y0, 420, 24);
    moveIf(gBrandSubtitle, 88, y0 + 24, std::max(400, right - 104), 20);
    moveIf(gCaseName, 128, y0 + 80, 330, 22);
    moveIf(gCaseNumber, 590, y0 + 80, std::max(220, right - 590 - 150), 22);
    moveIf(gInvestigator, 128, y0 + 108, 330, 22);
    moveIf(gCompany, 590, y0 + 108, std::max(220, right - 590 - 150), 22);
    moveIf(gBrowseOut, rightBrowseX, y0 + 135, browseW, 24);
    moveIf(gOut, 128, y0 + 136, std::max(250, rightBrowseX - 138), 22);
    moveIf(gOpenCase, right - 96, y0 + 161, 96, 28);
    moveIf(gBrowseCase, right - 198, y0 + 163, 90, 24);
    moveIf(gCaseDbPath, 128, y0 + 164, std::max(250, right - 338), 22);
    moveIf(gCaseAutosaveStatus, 272, y0 + 195, right - 288, 20);

    // V1.6.18: compact, right-aligned build action row. Keep the controls
    // smaller than the previous 36-pixel action buttons to reduce top crowding.
    moveIf(gCancelIngestButton, right - 116, y0 + 367, 116, 30);
    moveIf(gRun, right - 296, y0 + 367, 172, 30);
    moveIf(gBrowseInput, rightBrowseX, y0 + 247, browseW, 24);
    moveIf(gInput, 420, y0 + 248, std::max(220, rightBrowseX - 430), 22);
    moveIf(gBrowseRoot, rightBrowseX, y0 + 277, browseW, 24);
    moveIf(gEvidenceRoot, 224, y0 + 278, std::max(220, rightBrowseX - 234), 22);
    moveIf(gBrowse7z, rightBrowseX, y0 + 307, browseW, 24);
    moveIf(gSevenZip, 224, y0 + 308, std::max(220, rightBrowseX - 234), 22);
    moveIf(gSuppressCsvExports, 136, y0 + 428, 110, 22);
    moveIf(gExportProfile, right - 144, y0 + 426, 144, 92);
    moveIf(gIngestStatus, 128, y0 + 456, right - 128, 42);
    moveIf(gIngestProgress, 128, y0 + 504, right - 128, 20);
    moveIf(gLog, 16, y0 + 532, right - 16, std::max(120, bottom - (y0 + 532)));

    moveIf(gReviewViewProfile, 96, y0 + 22, 150, 96);
    const int viewSetButtonsY = bottom - 64;
    moveIf(gReviewView, 16, y0 + 72, 220, std::max(220, viewSetButtonsY - (y0 + 72) - 8));
    moveIf(gViewSetUp, 16, viewSetButtonsY, 70, 24);
    moveIf(gViewSetDown, 92, viewSetButtonsY, 80, 24);
    moveIf(gViewSetHide, 178, viewSetButtonsY, 58, 24);
    moveIf(gViewSetSave, 16, viewSetButtonsY + 30, 86, 24);
    moveIf(gViewSetReset, 108, viewSetButtonsY + 30, 86, 24);
    const int reviewX = 248;
    const int reviewW = right - reviewX;
    // Compact review action bar: two short rows, kept clear of Dashboard/Index.
    moveIf(gSearch, 304, y0 + 22, 250, 22);
    moveIf(gRefresh, 564, y0 + 21, 80, 24);
    moveIf(gCancelLoad, 650, y0 + 21, 96, 24);
    moveIf(gPageSize, 798, y0 + 22, 56, 22);
    moveIf(gPrev, reviewX, y0 + 52, 96, 24);
    moveIf(gNext, reviewX + 102, y0 + 52, 88, 24);
    moveIf(gExportPage, reviewX + 196, y0 + 52, 104, 24);
    moveIf(gExportFiltered, reviewX + 306, y0 + 52, 118, 24);
    moveIf(gExportVisible, reviewX + 430, y0 + 52, 106, 24);
    moveIf(gExportChecked, reviewX + 542, y0 + 52, 118, 24);
    moveIf(gClearChecked, reviewX, y0 + 80, 108, 24);
    moveIf(gManageTags, reviewX + 114, y0 + 80, 60, 24);
    moveIf(gOpenCaseFolder, reviewX + 180, y0 + 80, 106, 24);
    moveIf(gOpenUploadFolder, reviewX + 292, y0 + 80, 118, 24);
    moveIf(gOpenLogsFolder, reviewX + 416, y0 + 80, 70, 24);
    moveIf(gReviewSummary, reviewX, y0 + 110, reviewW, 44);
    moveIf(gReviewBusy, reviewX, y0 + 158, reviewW, 6);
    const int reviewBodyY = y0 + 168;
    const int reviewBodyH = std::max(300, bottom - reviewBodyY - 20);
    const int detailsLabelH = 20;
    const int detailGap = 12;
    const int splitterH = kReviewDetailsSplitterHeight;
    const int maxDetailsPaneH = std::max(kReviewDetailsMinHeight, reviewBodyH - 170 - detailsLabelH - splitterH - detailGap);
    if (gReviewDetailsPaneHeight <= 0) gReviewDetailsPaneHeight = kReviewDetailsDefaultHeight;
    gReviewDetailsPaneHeight = std::min(std::max(gReviewDetailsPaneHeight, kReviewDetailsMinHeight), maxDetailsPaneH);
    const int listH = std::max(150, reviewBodyH - gReviewDetailsPaneHeight - detailsLabelH - splitterH - detailGap);
    const int splitterY = reviewBodyY + listH + detailGap;
    moveIf(gList, reviewX, reviewBodyY, reviewW, listH);
    moveIf(gRowDetailsSplitter, reviewX, splitterY, reviewW, splitterH);
    moveIf(gRowDetailsLabel, reviewX, splitterY + splitterH, reviewW, detailsLabelH);
    moveIf(gRowDetails, reviewX, splitterY + splitterH + detailsLabelH, reviewW, gReviewDetailsPaneHeight);
    resizeDetailsListColumns();
    enforceDetailsPaneTabVisibility();
    moveIf(gOpenDashboard, right - 234, y0 + 80, 108, 24);
    moveIf(gOpenReviewIndex, right - 118, y0 + 80, 118, 24);


    moveIf(gTagList, 16, y0 + 60, 230, std::max(190, bottom - (y0 + 60) - 370));
    moveIf(gTagNote, 262, y0 + 130, std::max(360, right - 262 - 250), 92);
    moveIf(gTagSummary, 262, y0 + 236, std::max(360, right - 262 - 90), 54);
    moveIf(gTaggedList, 262, y0 + 304, right - 262, std::max(230, bottom - (y0 + 304) - 20));

    moveIf(gIosStatus, 16, y0 + 38, right - 16, 112);
    moveIf(gIosReadiness, 16, y0 + 268, std::max(360, (right - 40) / 2), std::max(180, bottom - (y0 + 268) - 20));
    moveIf(gIosPlan, std::max(500, cw / 2), y0 + 268, std::max(360, right - std::max(500, cw / 2)), std::max(180, bottom - (y0 + 268) - 20));

    stabilizeReviewControlZOrder(hwnd);
}


void createControls(HWND hwnd) {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS}; InitCommonControlsEx(&icc);
    gTab = CreateWindowExW(0, WC_TABCONTROLW, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 8, 8, 990, 730, hwnd, nullptr, gInst, nullptr);
    applyUiFont(gTab);
    TCITEMW item{}; item.mask = TCIF_TEXT;
    item.pszText = const_cast<LPWSTR>(L"Case Information"); TabCtrl_InsertItem(gTab, 0, &item);
    item.pszText = const_cast<LPWSTR>(L"MacOS Investigation View"); TabCtrl_InsertItem(gTab, 1, &item);
    item.pszText = const_cast<LPWSTR>(L"iOS Investigation View"); TabCtrl_InsertItem(gTab, 2, &item);
    item.pszText = const_cast<LPWSTR>(L"Tags / Notes"); TabCtrl_InsertItem(gTab, 3, &item);
    createProcessControls(hwnd);
    createReviewControls(hwnd);
    createIosControls(hwnd);
    createTagControls(hwnd);
    layoutControls(hwnd);
    showTab(0);
}

void openCaseFromPath() {
    gOpenedCaseDb = getText(gCaseDbPath);
    gCrashCaseDir.clear();
    size_t pos = gOpenedCaseDb.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        gCrashCaseDir = gOpenedCaseDb.substr(0, pos);
        if (getText(gOut).empty()) setText(gOut, gCrashCaseDir);
    }
    gCurrentPage = 0;
    // Opening an older completed case should make newly-added GUI review views
    // available without requiring a re-ingest. This runs schema/view upgrades only;
    // it does not alter parsed evidence rows.
    upgradeCaseSchemaForGuiNoThrow(gOpenedCaseDb);
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        populateViewList(db.get());
        loadPersistedCheckedArtifactsNoThrow();
    } catch (...) {
        populateViewList();
    }
    loadCaseSummary();
    if (!gCheckedArtifactIds.empty()) refreshCheckedArtifactsSummaryNoThrow();
    loadReviewPage();
    refreshTagList();
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: createControls(hwnd); return 0;
    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (dis && dis->CtlID == ID_OPEN_CASE) {
            const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
            const COLORREF fill = pressed ? RGB(0, 90, 160) : RGB(0, 120, 215);
            HBRUSH brush = CreateSolidBrush(fill);
            FillRect(dis->hDC, &dis->rcItem, brush);
            DeleteObject(brush);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, RGB(255, 255, 255));
            HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dis->hDC, GetStockObject(DEFAULT_GUI_FONT)));
            RECT textRect = dis->rcItem;
            DrawTextW(dis->hDC, L"OPEN CASE", -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dis->hDC, oldFont);
            if (dis->itemState & ODS_FOCUS) DrawFocusRect(dis->hDC, &dis->rcItem);
            return TRUE;
        }
        break;
    }
    case WM_NOTIFY: {
        LPNMHDR hdr = reinterpret_cast<LPNMHDR>(lp);
        if (hdr && hdr->hwndFrom == gList && hdr->code == LVN_GETDISPINFOW) {
            auto* di = reinterpret_cast<NMLVDISPINFOW*>(lp);
            if (di && (di->item.mask & LVIF_TEXT) && di->item.pszText && di->item.cchTextMax > 0) {
                const std::wstring text = reviewCellText(di->item.iItem, di->item.iSubItem);
                lstrcpynW(di->item.pszText, text.c_str(), di->item.cchTextMax);
            }
            return 0;
        }
        if (hdr && hdr->hwndFrom == gList && hdr->code == LVN_ITEMCHANGED) {
            auto* pnmv = reinterpret_cast<LPNMLISTVIEW>(lp);
            if ((pnmv->uChanged & LVIF_STATE) && ((pnmv->uNewState ^ pnmv->uOldState) & (LVIS_SELECTED | LVIS_FOCUSED))) {
                if ((pnmv->uNewState & (LVIS_SELECTED | LVIS_FOCUSED)) && pnmv->iItem >= 0) updateRowDetailsPanel(pnmv->iItem);
            }
            return 0;
        }
        if (hdr && hdr->hwndFrom == gRowDetails && hdr->code == NM_CUSTOMDRAW) {
            auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(lp);
            if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                const LPARAM rowType = cd->nmcd.lItemlParam;
                if (rowType >= 1000) {
                    DetailGroup g = static_cast<DetailGroup>(rowType - 1000);
                    cd->clrTextBk = detailGroupColor(g);
                    cd->clrText = RGB(20, 20, 20);
                    return CDRF_NEWFONT;
                }
                if ((cd->nmcd.dwItemSpec % 2) == 0) cd->clrTextBk = RGB(252, 252, 252);
                else cd->clrTextBk = RGB(246, 246, 246);
                return CDRF_DODEFAULT;
            }
        }
        if (gReviewViewTooltip && hdr->hwndFrom == gReviewViewTooltip && hdr->code == TTN_GETDISPINFOW) {
            auto* di = reinterpret_cast<NMTTDISPINFOW*>(lp);
            updateReviewViewTooltipTextFromCursor();
            di->lpszText = const_cast<LPWSTR>(gReviewViewTooltipText.c_str());
            return 0;
        }
        if (hdr->hwndFrom == gTab && hdr->code == TCN_SELCHANGE) {
            int selectedTab = TabCtrl_GetCurSel(gTab);
            if (selectedTab == 1) {
                gIosReviewMode = false;
                populateViewListForCurrentContextNoThrow();
            } else if (selectedTab == 2) {
                gIosReviewMode = true;
                populateViewListForCurrentContextNoThrow();
            }
            showTab(selectedTab);
            return 0;
        }
        if (hdr->hwndFrom == gList && hdr->code == LVN_COLUMNCLICK) {
            auto* lv = reinterpret_cast<NMLISTVIEW*>(lp);
            if (lv->iSubItem == 0) {
                if (gSortColumn == -2) gSortDescending = !gSortDescending;
                else { gSortColumn = -2; gSortDescending = false; }
                gCurrentPage = 0;
                loadReviewPage();
            } else {
                const int dataCol = lv->iSubItem - 2;
                if (dataCol >= 0) {
                    if (gSortColumn == dataCol) gSortDescending = !gSortDescending;
                    else { gSortColumn = dataCol; gSortDescending = false; }
                    gCurrentPage = 0;
                    loadReviewPage();
                }
            }
            return 0;
        }

        HWND header = gList ? ListView_GetHeader(gList) : nullptr;
        if (header && hdr->hwndFrom == header && hdr->code == NM_RCLICK) {
            gContextRow = -1;
            POINT pt{}; GetCursorPos(&pt);
            POINT clientPt = pt; ScreenToClient(header, &clientPt);
            HDHITTESTINFO hit{}; hit.pt = clientPt;
            int col = static_cast<int>(SendMessageW(header, HDM_HITTEST, 0, reinterpret_cast<LPARAM>(&hit)));
            showColumnContextMenu(hwnd, col, pt);
            return 0;
        }

        if (hdr->hwndFrom == gList && hdr->code == NM_RCLICK) {
            POINT pt{}; GetCursorPos(&pt);
            POINT clientPt = pt; ScreenToClient(gList, &clientPt);
            LVHITTESTINFO hit{}; hit.pt = clientPt;
            ListView_SubItemHitTest(gList, &hit);
            gContextRow = hit.iItem;
            if (hit.iItem >= 0) {
                const UINT state = ListView_GetItemState(gList, hit.iItem, LVIS_SELECTED);
                if ((state & LVIS_SELECTED) == 0) {
                    const int count = ListView_GetItemCount(gList);
                    for (int i = 0; i < count; ++i) ListView_SetItemState(gList, i, 0, LVIS_SELECTED | LVIS_FOCUSED);
                    ListView_SetItemState(gList, hit.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                } else {
                    ListView_SetItemState(gList, hit.iItem, LVIS_FOCUSED, LVIS_FOCUSED);
                }
            }
            showColumnContextMenu(hwnd, hit.iSubItem, pt);
            return 0;
        }

        return 0;
    }

    case WM_COMMAND:
        if (handleContextTagCommand(static_cast<UINT>(LOWORD(wp)))) return 0;
        if (HIWORD(wp) == EN_CHANGE) {
            const int id = LOWORD(wp);
            if (id == ID_CASE_NAME_EDIT || id == ID_CASE_NUMBER_EDIT || id == ID_INVESTIGATOR_EDIT || id == ID_COMPANY_EDIT || id == ID_CASE_LOCATION_EDIT || id == ID_CASE_DB_EDIT) {
                scheduleCaseInfoAutosave(hwnd);
            }
        }
        switch (LOWORD(wp)) {
        case ID_SOURCE_TYPE: {
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int srcType = gSourceType ? static_cast<int>(SendMessageW(gSourceType, CB_GETCURSEL, 0, 0)) : 0;
                if (srcType == 0) postStatus(L"Source type: Folder. Loose folder sources will be preserved into a static evidence archive before working-copy parsing.");
                else if (srcType == 1) postStatus(L"Source type: ZIP. Original ZIP will be hashed/registered and extracted to a controlled working staging folder; no second evidentiary archive is created.");
                else if (srcType == 2) postStatus(L"Source type: AFF4/APFS image. Staged image workflow for macOS forensic images; current support may emit readiness/probe outputs where full extraction is not yet complete.");
                else if (srcType == 3) postStatus(L"Source type: Raw IMG/DD image. Staged image workflow for raw/external media, including Mac-attached devices with Spotlight indexes.");
                else postStatus(L"Source type selected.");
            }
            return 0;
        }
        case ID_BROWSE_INPUT: {
            int srcType = gSourceType ? static_cast<int>(SendMessageW(gSourceType, CB_GETCURSEL, 0, 0)) : 0;
            auto p = (srcType == 0) ? browseFolder(hwnd) : (srcType == 1 ? browseZip(hwnd) : browseEvidenceContainer(hwnd, srcType));
            if (!p.empty()) setText(gInput, p);
            return 0;
        }
        case ID_BROWSE_OUT: { if (blockCaseMutationDuringIngest(hwnd, L"changing the case location")) return 0; auto p = browseFolder(hwnd); if (!p.empty()) { setText(gOut, p); if (getText(gCaseDbPath).empty()) setText(gCaseDbPath, p + L"\\VestigantSpotlight.case.sqlite"); postStatus(L"Case location selected: " + p); } return 0; }
        case ID_BROWSE_ROOT: { auto p = browseFolder(hwnd); if (!p.empty()) setText(gEvidenceRoot, p); return 0; }
        case ID_BROWSE_7Z: { auto p = browseExe(hwnd); if (!p.empty()) setText(gSevenZip, p); return 0; }
        case ID_RUN: {
            if (getText(gOut).empty()) {
                MessageBoxW(hwnd, L"Select or create a case output location before ingest starts. The case database, logs, exports, and upload folder will be written there.", L"Vestigant Spotlight - Case Location Required", MB_OK | MB_ICONINFORMATION);
                auto p = browseFolder(hwnd);
                if (p.empty()) { postStatus(L"Waiting: case location selection was cancelled."); return 0; }
                setText(gOut, p);
                setText(gCaseDbPath, p + L"\\VestigantSpotlight.case.sqlite");
            }
            int srcType = gSourceType ? static_cast<int>(SendMessageW(gSourceType, CB_GETCURSEL, 0, 0)) : 0;
            if (srcType == 2) postLog(L"Selected source type: AFF4/APFS image (staged). The run will preserve/register the selected evidence and perform currently implemented image/readiness/probe workflow steps.");
            else if (srcType == 3) postLog(L"Selected source type: Raw IMG/DD image (staged). The run will preserve/register the selected evidence and perform currently implemented raw-image/readiness workflow steps.");
            joinCompletedIngestThreadNoThrow();
            if (isIngestRunningNoThrow()) {
                postStatus(L"Ingest already running. Wait for the current run to finish before starting another.");
                MessageBoxW(hwnd, L"An ingest/build worker is already running. Wait for it to finish before starting another run.", L"Vestigant Spotlight - Ingest Running", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            postProgress(0);
            gCancelIngestRequested.store(false);
            if (gCancelIngestButton) EnableWindow(gCancelIngestButton, TRUE);
            postStatus(L"Queued: ingest worker starting.");
            if (!startIngestThreadNoThrow(hwnd)) {
                postStatus(L"ERROR: ingest worker could not be started.");
            }
            return 0;
        }
        case ID_CANCEL_INGEST: {
            gCancelIngestRequested.store(true);
            if (gCancelIngestButton) EnableWindow(gCancelIngestButton, FALSE);
            postStatus(L"Cancellation requested. Waiting for the current extraction loop to reach a safe stop point...");
            postLog(L"Cancellation requested by investigator. The worker will stop at the next implemented safe checkpoint.");
            return 0;
        }
        case ID_BROWSE_CASE: { if (blockCaseMutationDuringIngest(hwnd, L"changing the case database")) return 0; auto p = browseSqlite(hwnd); if (!p.empty()) { setText(gCaseDbPath, p); size_t pos = p.find_last_of(L"\\/"); if (pos != std::wstring::npos) setText(gOut, p.substr(0, pos)); } return 0; }
        case ID_OPEN_CASE: { if (blockCaseMutationDuringIngest(hwnd, L"opening a different case database")) return 0; openCaseFromPath(); return 0; }
        case ID_SAVE_CASE_INFO: { if (blockCaseMutationDuringIngest(hwnd, L"saving case information")) return 0; saveCaseInformation(); return 0; }
        case ID_OPEN_DASHBOARD: { openSiblingFile(hwnd, L"investigator_dashboard.html"); return 0; }
        case ID_OPEN_REVIEW_INDEX: { openSiblingFile(hwnd, L"review_index.html"); return 0; }
        case ID_OPEN_CASE_FOLDER: { openCaseFolderAction(hwnd); return 0; }
        case ID_OPEN_UPLOAD_FOLDER: { openUploadFolderAction(hwnd); return 0; }
        case ID_OPEN_LOGS_FOLDER: { openLogsFolderAction(hwnd); return 0; }
        case ID_REVIEW_VIEW_PROFILE: {
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int sel = static_cast<int>(SendMessageW(gReviewViewProfile, CB_GETCURSEL, 0, 0));
                if (sel < 0) sel = 0;
                gReviewViewProfileMode = sel;
                gCurrentPage = 0; gSortColumn = -1; gFilterColumn = -1; gFilterValue.clear();
                populateViewListForCurrentContextNoThrow();
                setReviewSummary(std::wstring(L"View Set: ") + reviewViewProfileDescription() + L". Select a view or click Update to load results.");
                loadReviewPage();
            }
            return 0;
        }
        case ID_VIEWSET_SAVE: { saveCurrentVisibleViewsAsCustom(); return 0; }
        case ID_VIEWSET_RESET: { resetCustomViewSet(); return 0; }
        case ID_VIEWSET_HIDE: { hideSelectedViewFromCustom(); return 0; }
        case ID_VIEWSET_UP: { moveSelectedViewInCustom(-1); return 0; }
        case ID_VIEWSET_DOWN: { moveSelectedViewInCustom(1); return 0; }
        case ID_REVIEW_VIEW: { if (HIWORD(wp) == LBN_SELCHANGE) { gCurrentPage = 0; gSortColumn = -1; gFilterColumn = -1; gFilterValue.clear(); loadReviewPage(); } return 0; }
        case ID_REVIEW_REFRESH: { gCurrentPage = 0; loadReviewPage(); return 0; }
        case ID_REVIEW_CANCEL_LOAD: { cancelAndJoinReviewThreadNoThrow(); gReviewLoadInProgress = false; setReviewLoadingState(false); setReviewSummary(L"Cancelled current review-page load. Start another view/search when ready."); return 0; }
        case ID_CTX_SORT_ASC: { if (gContextColumn >= 0) { gSortColumn = gContextColumn; gSortDescending = false; gCurrentPage = 0; loadReviewPage(); } return 0; }
        case ID_CTX_SORT_DESC: { if (gContextColumn >= 0) { gSortColumn = gContextColumn; gSortDescending = true; gCurrentPage = 0; loadReviewPage(); } return 0; }
        case ID_CTX_FILTER_SEARCH: { if (gContextColumn >= 0) { gFilterColumn = gContextColumn; gFilterValue = narrow(getText(gSearch)); gCurrentPage = 0; loadReviewPage(); } return 0; }
        case ID_CTX_CLEAR_FILTER: { gFilterColumn = -1; gFilterValue.clear(); gCurrentPage = 0; loadReviewPage(); return 0; }
        case ID_CTX_TOGGLE_CHECK: { if (gContextRow >= 0) toggleReviewRowChecked(gContextRow); setReviewSummary(L"Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size()))); return 0; }
        case ID_CTX_CHECK_SELECTED: { setReviewRowsChecked(selectedReviewRows(), true); setReviewSummary(L"Checked selected row(s). Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size()))); return 0; }
        case ID_CTX_UNCHECK_SELECTED: { setReviewRowsChecked(selectedReviewRows(), false); setReviewSummary(L"Unchecked selected row(s). Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size()))); return 0; }
        case ID_CTX_TOGGLE_SELECTED: { toggleReviewRowsAsBatch(selectedReviewRows()); setReviewSummary(L"Toggled selected row(s). Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size()))); updateRowDetailsPanel(); return 0; }
        case ID_CTX_APPLY_TAG_ROW: { try { if (gContextRow >= 0 && gContextRow < static_cast<int>(gCurrentRowArtifactIds.size())) applyTagToArtifacts(selectedTagId(), std::vector<std::string>{gCurrentRowArtifactIds[static_cast<size_t>(gContextRow)]}, true); } catch (const std::exception& ex) { setTagSummary(L"ERROR applying row tag: " + widen(ex.what())); } return 0; }
        case ID_CTX_REMOVE_TAG_ROW: { try { if (gContextRow >= 0 && gContextRow < static_cast<int>(gCurrentRowArtifactIds.size())) removeTagFromArtifacts(selectedTagId(), std::vector<std::string>{gCurrentRowArtifactIds[static_cast<size_t>(gContextRow)]}, true); } catch (const std::exception& ex) { setTagSummary(L"ERROR removing row tag: " + widen(ex.what())); } return 0; }
        case ID_CTX_APPLY_TAG_SELECTED: { try { applyTagToArtifacts(selectedTagId(), selectedArtifactIdsFromReview(false), true); } catch (const std::exception& ex) { setTagSummary(L"ERROR applying selected-row tag: " + widen(ex.what())); } return 0; }
        case ID_CTX_REMOVE_TAG_SELECTED: { try { removeTagFromArtifacts(selectedTagId(), selectedArtifactIdsFromReview(false), true); } catch (const std::exception& ex) { setTagSummary(L"ERROR removing selected-row tag: " + widen(ex.what())); } return 0; }
        case ID_CTX_APPLY_TAG_CHECKED: { try { applyTagToArtifacts(selectedTagId(), checkedArtifactIds(), true); } catch (const std::exception& ex) { setTagSummary(L"ERROR applying checked-row tag: " + widen(ex.what())); } return 0; }
        case ID_CTX_REMOVE_TAG_CHECKED: { try { removeTagFromArtifacts(selectedTagId(), checkedArtifactIds(), true); } catch (const std::exception& ex) { setTagSummary(L"ERROR removing checked-row tag: " + widen(ex.what())); } return 0; }
        case ID_CTX_CLEAR_CHECKED: { gCheckedArtifactIds.clear(); clearPersistedCheckedArtifactsNoThrow(); loadReviewPage(); return 0; }
        case ID_CTX_MANAGE_TAGS: { switchToTagsTab(); return 0; }
        case ID_REVIEW_PREV: { if (gCurrentPage > 0) --gCurrentPage; loadReviewPage(); return 0; }
        case ID_REVIEW_NEXT: { ++gCurrentPage; loadReviewPage(); return 0; }
        case ID_EXPORT_PAGE: { exportCurrentPage(hwnd); return 0; }
        case ID_EXPORT_FILTERED: { exportFilteredView(hwnd); return 0; }
        case ID_EXPORT_VISIBLE_VIEWS: { exportVisibleViews(hwnd); return 0; }
        case ID_EXPORT_CHECKED: { exportCheckedArtifacts(hwnd); return 0; }
        case ID_CLEAR_CHECKED: { gCheckedArtifactIds.clear(); clearPersistedCheckedArtifactsNoThrow(); loadReviewPage(); setReviewSummary(L"Cleared all checked artifacts from the current case database."); return 0; }
        case ID_REFRESH_TAGS: { refreshTagList(); return 0; }
        case ID_ADD_TAG: { addTagAction(); return 0; }
        case ID_DELETE_TAG: { deleteTagAction(); return 0; }
        case ID_APPLY_TAG: { applyTagAction(); return 0; }
        case ID_REMOVE_TAG: { removeTagAction(); return 0; }
        case ID_SAVE_NOTE: { saveNoteAction(); return 0; }
        case ID_SHOW_TAGGED: { showTaggedAction(); return 0; }
        case ID_EXPORT_TAGGED: { exportTaggedArtifacts(hwnd); return 0; }
        case ID_IOS_OPEN_READINESS: { openIosReviewView(L"iOS - Spotlight Native Parser Targets"); return 0; }
        case ID_IOS_OPEN_STORE_SUMMARY: { openIosReviewView(L"iOS - Spotlight High-Value Timeline"); return 0; }
        case ID_IOS_OPEN_STRING_VALUES: { openIosReviewView(L"iOS - Spotlight Entity Review"); return 0; }
        case ID_IOS_OPEN_STRING_SUMMARY: { openIosReviewView(L"iOS - Spotlight Entity Summary"); return 0; }
        case ID_IOS_OPEN_INDEX_TIMELINE: { openIosReviewView(L"iOS - Spotlight Investigative Item Date Evidence"); return 0; }
        case ID_IOS_OPEN_ARTIFACTS: { openIosReviewView(L"iOS - Spotlight to FFS Links"); return 0; }
        case ID_IOS_OPEN_KEYWORD_VALUES: { openIosReviewView(L"iOS - Spotlight File References With Dates"); return 0; }
        case ID_IOS_OPEN_FFS_INVENTORY: { openIosReviewView(L"iOS - Spotlight URL References With Dates"); return 0; }
        case ID_IOS_OPEN_APP_DATABASES: { openIosReviewView(L"iOS - Spotlight Account/Contact References With Dates"); return 0; }
        case ID_IOS_OPEN_PARSED_APP_RECORDS: { openIosReviewView(L"iOS - Store Parse Summary"); return 0; }
        case ID_IOS_OPEN_PARSED_APP_SUMMARY: { openIosReviewView(L"iOS - Spotlight Native Parser Targets"); return 0; }
        case ID_IOS_OPEN_REFERENCED_PATHS: { openIosReviewView(L"iOS - Spotlight Referenced Paths"); return 0; }
        case ID_IOS_OPEN_MISSING_FFS: { openIosReviewView(L"iOS - Missing From FFS Candidates"); return 0; }
        case ID_IOS_OPEN_DB_RESIDENCY: { openIosReviewView(L"iOS - Residency Summary"); return 0; }
        case ID_IOS_OPEN_RESIDENCY_SUMMARY: { openIosReviewView(L"iOS - Residency Summary"); return 0; }
        case ID_IOS_OPEN_PLAN: { MessageBoxW(hwnd, L"V0_9_17 focuses the iOS Investigation tab on Spotlight/CoreSpotlight records and direct FFS overlap. Full FFS/app database inventories remain exported for support, but are intentionally hidden from the default view list to reduce confusion and improve interactive review. Last_Updated remains metadata/index timing unless another decoded Spotlight field supports a stronger interpretation.", L"Vestigant Spotlight - iOS View Focus", MB_OK | MB_ICONINFORMATION); return 0; }
        default: break;
        }
        return 0;
    case WM_APPEND_LOG: { auto* s = reinterpret_cast<std::wstring*>(lp); if (s) { appendLog(*s); delete s; } return 0; }
    case WM_CLEAR_PROCESS_LOG: { clearProcessLog(); return 0; }
    case WM_SET_INGEST_STATUS: { auto* s = reinterpret_cast<std::wstring*>(lp); if (s) { setText(gIngestStatus, *s); delete s; } if (!gIngestActive.load() && gCancelIngestButton) EnableWindow(gCancelIngestButton, FALSE); return 0; }
    case WM_SET_INGEST_PROGRESS: { int pct = static_cast<int>(wp); if (pct < 0) pct = 0; if (pct > 100) pct = 100; if (gIngestProgress) SendMessageW(gIngestProgress, PBM_SETPOS, static_cast<WPARAM>(pct), 0); return 0; }
    case WM_REVIEW_PAGE_RESULT: { completeReviewPageLoad(reinterpret_cast<ReviewPageResult*>(lp)); return 0; }
    case WM_EXPORT_PAGE_RESULT: {
        auto* s = reinterpret_cast<std::wstring*>(lp);
        if (s) { setReviewSummary(*s); delete s; }
        gExportPageActive.store(false);
        if (gExportPage) EnableWindow(gExportPage, TRUE);
        return 0;
    }
    case WM_EXPORT_FILTERED_RESULT: {
        auto* s = reinterpret_cast<std::wstring*>(lp);
        if (s) { setReviewSummary(*s); delete s; }
        gExportFilteredActive.store(false);
        if (gExportFiltered) EnableWindow(gExportFiltered, TRUE);
        return 0;
    }
    case WM_EXPORT_VISIBLE_RESULT: {
        auto* s = reinterpret_cast<std::wstring*>(lp);
        if (s) { setReviewSummary(*s); delete s; }
        gExportVisibleActive.store(false);
        if (gExportVisible) EnableWindow(gExportVisible, TRUE);
        return 0;
    }
    case WM_EXPORT_CHECKED_RESULT: {
        auto* s = reinterpret_cast<std::wstring*>(lp);
        if (s) { setReviewSummary(*s); delete s; }
        gExportCheckedActive.store(false);
        if (gExportChecked) EnableWindow(gExportChecked, TRUE);
        return 0;
    }
    case WM_EXPORT_TAGGED_RESULT: {
        auto* s = reinterpret_cast<std::wstring*>(lp);
        if (s) { setTagSummary(*s); delete s; }
        gExportTaggedActive.store(false);
        if (gTaggedList) EnableWindow(gTaggedList, TRUE);
        return 0;
    }
    case WM_SIZE: { layoutControls(hwnd); return 0; }
    case WM_TIMER: { if (wp == ID_AUTOSAVE_TIMER) { autosaveCaseInformation(hwnd); return 0; } break; }
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        if (mmi) { mmi->ptMinTrackSize.x = 1040; mmi->ptMinTrackSize.y = 900; }
        return 0;
    }
    case WM_DESTROY: { gShuttingDown.store(true); gCancelIngestRequested.store(true); cancelAndJoinReviewThreadNoThrow(); joinExportThreadsNoThrow(); joinIngestThreadNoThrow(); closeReadOnlyDbPoolNoThrow(); KillTimer(hwnd, ID_AUTOSAVE_TIMER); saveCaseInformationCore(true); if (gLogoBitmap) { DeleteObject(gLogoBitmap); gLogoBitmap = nullptr; } if (gUiFont) { DeleteObject(gUiFont); gUiFont = nullptr; } if (gRichEditModule) { FreeLibrary(gRichEditModule); gRichEditModule = nullptr; gRichEditAvailable = false; gRichEditClassName = L"EDIT"; } PostQuitMessage(0); return 0; }
    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    SetUnhandledExceptionFilter(unhandledFilter);
    std::set_terminate(terminateHandler);
    gInst = hInstance;
    gRichEditModule = LoadLibraryExW(L"Msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (gRichEditModule) { gRichEditClassName = L"RichEdit50W"; gRichEditAvailable = true; }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    gUiFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    WNDCLASSW wc{}; wc.lpfnWndProc = wndProc; wc.hInstance = hInstance; wc.lpszClassName = L"VestigantSpotlightWnd"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, widen(appTitle()).c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1180, 900, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow); UpdateWindow(hwnd);
    MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    CoUninitialize(); return static_cast<int>(msg.wParam);
}
