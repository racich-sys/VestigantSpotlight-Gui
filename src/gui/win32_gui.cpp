#if !defined(_WIN32)
#error This file is Windows-only.
#endif

#include "app/app_runner.h"
#include "core/app_info.h"
#include "db/sqlite_compat.h"
#include "db/case_db.h"
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
#include <initializer_list>
#include <chrono>

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
HWND gInput{}, gOut{}, gEvidenceRoot{}, gSevenZip{}, gSourceType{}, gProfile{}, gMode{}, gCaseName{}, gCaseNumber{}, gCompany{}, gInvestigator{}, gLog{}, gRun{}, gCoreDecode{}, gIngestStatus{}, gIngestProgress{}, gLogo{}, gBrandTitle{}, gBrandSubtitle{};
HWND gFullNative{}, gVerbose{}, gExportProfile{};
HWND gCaseDbPath{}, gBrowseCase{}, gBrowseOut{}, gBrowseInput{}, gBrowseRoot{}, gBrowse7z{}, gOpenCase{}, gSaveCaseInfo{}, gCaseAutosaveStatus{}, gOpenDashboard{}, gOpenReviewIndex{}, gOpenCaseFolder{}, gOpenUploadFolder{}, gOpenLogsFolder{}, gReviewViewProfile{}, gViewSetSave{}, gViewSetReset{}, gViewSetHide{}, gViewSetUp{}, gViewSetDown{}, gManageTags{}, gReviewView{}, gSearch{}, gPageSize{}, gRefresh{}, gCancelLoad{}, gReviewBusy{}, gPrev{}, gNext{}, gExportPage{}, gExportFiltered{}, gExportChecked{}, gClearChecked{}, gReviewSummary{}, gList{}, gRowDetailsSplitter{}, gRowDetailsLabel{}, gRowDetails{};

// Forward declaration required because custom view-set helpers are defined before the review summary helper.
void setReviewSummary(const std::wstring& s);
void setDetailsPaneMessage(const std::wstring& message);
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
std::atomic<unsigned long long> gReviewRequestSeq{0};
std::atomic_bool gShuttingDown{false};
std::thread gReviewThread;
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

constexpr int ID_RUN = 1001, ID_BROWSE_INPUT = 1002, ID_BROWSE_OUT = 1003, ID_BROWSE_ROOT = 1004, ID_BROWSE_7Z = 1005, ID_SOURCE_TYPE = 1006;
constexpr int ID_BROWSE_CASE = 1101, ID_OPEN_CASE = 1102, ID_REVIEW_REFRESH = 1103, ID_REVIEW_PREV = 1104, ID_REVIEW_NEXT = 1105, ID_EXPORT_PAGE = 1106, ID_EXPORT_FILTERED = 1113, ID_REVIEW_CANCEL_LOAD = 1114, ID_OPEN_CASE_FOLDER = 1115, ID_OPEN_UPLOAD_FOLDER = 1116, ID_OPEN_LOGS_FOLDER = 1117;
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

struct ViewSpec {
    const wchar_t* displayName;
    const char* tableName;
    std::vector<const char*> columns;
    std::vector<const char*> searchColumns;
    const char* orderBy;
};

const std::vector<ViewSpec>& views() {
    static const std::vector<ViewSpec> v = {
        {L"Investigator - Usage Artifacts", "vw_usage_artifacts", {"file_name","best_path","last_used_date_utc","first_used_candidate_utc","used_dates_count","use_count_value","usage_field_summary","artifact_id","store_guid","inode_num","parent_inode_num","display_name","content_type","where_froms","confidence"}, {"store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","content_type","usage_field_summary","where_froms","confidence"}, "COALESCE(NULLIF(last_used_date_utc,''), NULLIF(first_used_candidate_utc,''), artifact_id) DESC"},
        {L"Investigator - Object Usage Summary", "vw_object_usage_summary", {"file_name","best_path","usage_latest_utc","usage_earliest_utc","fused_usage_dates_utc","use_count_value","open_count_estimate","used_dates_count","artifact_id","store_guid","inode_num","parent_inode_num","display_name","path_source","path_status","content_type","logical_size_bytes","physical_size_bytes","usage_date_row_count","usage_evidence_row_count","usage_source_fields","object_usage_basis","where_froms","confidence"}, {"store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","path_source","path_status","content_type","usage_source_fields","object_usage_basis","where_froms","confidence"}, "COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,''), artifact_id) DESC"},
        {L"Investigator - Usage Timeline", "vw_usage_timeline_attributed", {"file_name","best_path","event_utc","date_type","source_field","usage_reason","usage_latest_utc","usage_earliest_utc","fused_usage_dates_utc","likely_snapshot_or_index_date","snapshot_warning_reason","artifact_id","timeline_id","store_guid","inode_num","parent_inode_num","display_name","path_source","path_status","content_type","logical_size_bytes","physical_size_bytes","use_count_value","open_count_estimate","used_dates_count","usage_date_row_count","usage_evidence_row_count","where_froms","confidence"}, {"event_utc","date_type","source_field","usage_reason","snapshot_warning_reason","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","path_source","path_status","content_type","where_froms","confidence"}, "event_utc DESC, artifact_id DESC"},
        {L"Investigator - Usage Event Details (Raw)", "vw_usage_event_detail_attributed", {"file_name","best_path","event_utc","date_type","source_field","usage_reason","likely_snapshot_or_index_date","snapshot_warning_reason","artifact_id","usage_event_id","store_guid","inode_num","parent_inode_num","display_name","content_type","where_froms"}, {"event_utc","date_type","source_field","usage_reason","snapshot_warning_reason","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","content_type","where_froms"}, "event_utc DESC, usage_event_id DESC"},
        {L"Investigator - Recent Activity", "vw_recent_activity", {"file_name","best_path","last_used_date_utc","first_used_candidate_utc","last_updated_utc","used_dates_count","use_count_value","artifact_id","store_guid","inode_num","parent_inode_num","display_name","content_type","where_froms","confidence"}, {"store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","content_type","where_froms","confidence"}, "COALESCE(NULLIF(last_used_date_utc,''), NULLIF(first_used_candidate_utc,''), NULLIF(last_updated_utc,''), artifact_id) DESC"},
        {L"Investigator - All Artifact Dates", "vw_artifact_dates_wide", {"file_name","best_path","usage_latest_utc","usage_earliest_utc","modified_latest_utc","modified_earliest_utc","created_latest_utc","created_earliest_utc","downloaded_latest_utc","downloaded_earliest_utc","interesting_or_index_latest_utc","interesting_or_index_earliest_utc","likely_snapshot_date_count","associated_date_count","unassociated_date_count","artifact_id","store_guid","inode_num","parent_inode_num","display_name","path_source","path_status","logical_size_bytes","physical_size_bytes","content_type","available_date_fields","association_confidence_summary","snapshot_warning_reasons"}, {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","path_source","path_status","content_type","available_date_fields","association_confidence_summary","snapshot_warning_reasons"}, "usage_latest_utc DESC, modified_latest_utc DESC, created_latest_utc DESC, artifact_id DESC"},
        {L"Investigator - Object Date Summary", "vw_object_date_summary", {"file_name","best_path","last_date_utc","first_date_utc","total_date_count","usage_date_count","downloaded_date_count","modified_date_count","created_date_count","interesting_or_index_date_count","likely_snapshot_or_index_date_count","date_association_status","date_association_confidence","artifact_id","store_guid","inode_num","parent_inode_num","display_name","path_source","path_status","logical_size_bytes","physical_size_bytes","content_type","available_date_fields","interpreted_date_types","snapshot_warning_reasons"}, {"artifact_id","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","path_source","path_status","content_type","available_date_fields","interpreted_date_types","snapshot_warning_reasons","date_association_status","date_association_confidence"}, "COALESCE(NULLIF(last_date_utc,''), artifact_id) DESC"},
        {L"Investigator - Date Attribution", "vw_date_field_attribution", {"file_name","best_path","parsed_utc","event_type_interpretation","raw_spotlight_field","interpretation_note","association_status","association_confidence","object_context_status","likely_snapshot_or_index_date","snapshot_warning_reason","snapshot_warning_detail","common_date_row_count","artifact_id","raw_date_id","store_guid","inode_num","parent_inode_num","display_name","path_source","path_status","logical_size_bytes","physical_size_bytes","content_type","raw_spotlight_value"}, {"store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","path_source","path_status","content_type","raw_spotlight_field","parsed_utc","event_type_interpretation","interpretation_note","association_status","association_confidence","object_context_status","snapshot_warning_reason","snapshot_warning_detail","raw_spotlight_value"}, "parsed_utc DESC, raw_date_id DESC"},
        {L"Investigator - Snapshot Date Warnings", "vw_snapshot_date_warnings", {"file_name","best_path","parsed_utc","event_type_interpretation","raw_spotlight_field","snapshot_warning_reason","snapshot_warning_detail","association_status","association_confidence","object_context_status","common_date_row_count","artifact_id","raw_date_id","store_guid","inode_num","parent_inode_num","display_name","path_source","path_status","logical_size_bytes","physical_size_bytes","content_type","interpretation_note"}, {"store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","path_source","path_status","content_type","raw_spotlight_field","parsed_utc","event_type_interpretation","interpretation_note","association_status","association_confidence","object_context_status","snapshot_warning_reason","snapshot_warning_detail"}, "parsed_utc DESC, raw_date_id DESC"},
        {L"Investigator - WhereFroms / Downloads", "vw_wherefroms_downloads", {"file_name","best_path","downloaded_date_utc","last_updated_utc","where_froms","artifact_id","store_guid","inode_num","parent_inode_num","display_name","content_type","existence_status","confidence"}, {"store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","content_type","where_froms","downloaded_date_utc","existence_status"}, "COALESCE(NULLIF(downloaded_date_utc,''), NULLIF(last_updated_utc,''), artifact_id) DESC"},
        {L"Investigator - Content Type Summary", "vw_content_type_summary", {"content_type","artifact_count","usage_artifact_count","path_artifact_count","first_last_updated_utc","last_last_updated_utc","total_logical_size_bytes","total_physical_size_bytes"}, {"content_type"}, "artifact_count DESC, content_type"},
        {L"Investigator - Store Content Types", "vw_store_content_type_summary", {"store_guid","content_type","artifact_count","usage_artifact_count","first_last_updated_utc","last_last_updated_utc"}, {"store_guid","content_type"}, "store_guid, artifact_count DESC, content_type"},
        {L"Investigator - Folder Activity", "vw_folder_activity", {"store_guid","parent_inode_num","child_count","usage_child_count","first_last_updated_utc","last_last_updated_utc","sample_child_path","folder_child_count"}, {"store_guid","parent_inode_num","sample_child_path"}, "usage_child_count DESC, child_count DESC, store_guid, CAST(parent_inode_num AS INTEGER)"},
        {L"Investigator - Path Reconstruction", "vw_path_reconstruction", {"file_name","best_path","reconstructed_path_candidate","relationship_status","path_reconstruction_method","confidence","applied_to_artifact_path","candidate_matches_artifact_path","artifact_id","store_guid","inode_num","parent_inode_num","display_name","path_source","path_status","parent_artifact_id","resolved_parent_inode_num","parent_file_name","parent_best_path","sibling_count","sibling_group_key"}, {"store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","reconstructed_path_candidate","relationship_status","path_reconstruction_method","confidence","path_source","path_status","parent_file_name","parent_best_path"}, "applied_to_artifact_path DESC, candidate_matches_artifact_path DESC, confidence DESC, store_guid, CAST(parent_inode_num AS INTEGER), CAST(inode_num AS INTEGER)"},
        {L"Investigator - Same Folder Groups", "vw_same_folder_groups", {"store_guid","parent_inode_num","parent_file_name","parent_best_path","child_count","resolved_parent_link_count","reconstructed_child_path_count","child_name_count","first_child_name","last_child_name","folder_group_status","max_confidence","sibling_group_key"}, {"store_guid","parent_inode_num","parent_file_name","parent_best_path","first_child_name","last_child_name","folder_group_status","max_confidence"}, "child_count DESC, reconstructed_child_path_count DESC, store_guid, CAST(parent_inode_num AS INTEGER)"},

        {L"Investigator - Checked Artifacts", "vw_checked_artifacts", {"checked_utc","file_name","best_path","tags","note_count","last_note_utc","last_note_text","artifact_id","store_guid","inode_num","parent_inode_num","display_name","path_source","path_status","content_type","usage_latest_utc","last_used_date_utc","modified_latest_utc","created_latest_utc","downloaded_latest_utc","where_froms","confidence"}, {"file_name","display_name","best_path","tags","last_note_text","store_guid","inode_num","parent_inode_num","path_source","path_status","content_type","where_froms","confidence"}, "checked_utc DESC, COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(modified_latest_utc,''), artifact_id) DESC"},
        {L"Investigator - Tagged Artifacts", "vw_tagged_artifacts", {"tag_name","file_name","best_path","note_count","last_note_utc","last_note_text","artifact_id","store_guid","inode_num","parent_inode_num","display_name","path_source","path_status","content_type","usage_latest_utc","last_used_date_utc","modified_latest_utc","created_latest_utc","where_froms","confidence"}, {"tag_name","file_name","display_name","best_path","last_note_text","store_guid","inode_num","parent_inode_num","path_source","path_status","content_type","where_froms","confidence"}, "lower(tag_name), COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(modified_latest_utc,''), artifact_id) DESC"},
        {L"Investigator - Notes", "vw_artifact_notes", {"created_utc","updated_utc","note_text","artifact_id","file_name","best_path","store_guid","inode_num","parent_inode_num","display_name","content_type","tags"}, {"note_text","file_name","display_name","best_path","store_guid","inode_num","parent_inode_num","content_type","tags"}, "updated_utc DESC, created_utc DESC, note_id DESC"},
        {L"Investigator - Export Ready Artifacts", "vw_export_ready_artifacts", {"artifact_id","tags","note_count","file_name","best_path","usage_latest_utc","last_used_date_utc","modified_latest_utc","created_latest_utc","downloaded_latest_utc","store_guid","inode_num","parent_inode_num","display_name","path_source","path_status","content_type","where_froms","confidence"}, {"tags","file_name","display_name","best_path","store_guid","inode_num","parent_inode_num","path_source","path_status","content_type","where_froms","confidence"}, "COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(modified_latest_utc,''), NULLIF(created_latest_utc,''), artifact_id) DESC"},
        {L"Investigator - Volume Root / Mounted", "vw_volume_root_focus", {"file_name","best_path","last_updated_utc","is_mounted_volume_path","mounted_volume_name","external_volume_reason","artifact_id","store_guid","inode_num","parent_inode_num","display_name","content_type","confidence"}, {"store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","content_type","mounted_volume_name","external_volume_reason"}, "store_guid, CAST(inode_num AS INTEGER), artifact_id"},
        {L"Investigator - Keyword Search Values", "vw_investigator_keyword_search_values", {"platform","source_table","source_id","store_guid","source_db","artifact_id","inode_num","parent_inode_num","field_name","search_value","date_utc","file_name","best_path","content_type","provenance"}, {"platform","source_table","source_id","store_guid","source_db","inode_num","parent_inode_num","field_name","search_value","file_name","best_path","content_type","provenance"}, "platform, source_table, store_guid, CAST(inode_num AS INTEGER), field_name"},
        {L"iOS Readiness - Relevant Fields", "vw_ios_relevant_fields", {"raw_kv_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","full_path","record_state","field_name","field_value"}, {"source_id","store_guid","source_db","inode_num","full_path","record_state","field_name","field_value"}, "raw_kv_id"},
        {L"iOS - Store Parse Summary", "vw_ios_store_parse_summary", {"store_guid","source_db","raw_record_count","record_file_name_count","record_full_path_count","placeholder_file_name_count","placeholder_root_path_count","earliest_last_updated_utc","latest_last_updated_utc"}, {"store_guid","source_db"}, "raw_record_count DESC, store_guid, source_db"},
        {L"iOS - Protection Class Summary", "vw_ios_protection_class_summary", {"protection_class","raw_record_count","records_with_string_probes","string_probe_rows","selected_database_count","earliest_last_updated_utc","latest_last_updated_utc"}, {"protection_class"}, "raw_record_count DESC, protection_class"},
        {L"iOS - Artifact Hint Summary", "vw_ios_artifact_hint_summary", {"artifact_hint","string_probe_rows","store_count","distinct_record_count","distinct_value_count","min_sample_value","max_sample_value"}, {"artifact_hint","min_sample_value","max_sample_value"}, "string_probe_rows DESC, artifact_hint"},
        {L"iOS - Record Investigation Hints", "vw_ios_record_investigation_hints", {"raw_record_id","source_id","store_guid","protection_class","primary_investigation_hint","artifact_hints","string_probe_rows","source_db","inode_num","store_id","parent_inode_num","file_name","content_type","display_name","full_path","last_updated_utc","time_interpretation","record_state"}, {"source_id","store_guid","protection_class","primary_investigation_hint","artifact_hints","source_db","inode_num","file_name","content_type","display_name","full_path","time_interpretation","record_state"}, "last_updated_utc DESC, protection_class, primary_investigation_hint, store_guid, CAST(inode_num AS INTEGER)"},
        {L"iOS - String Probe Values", "vw_ios_string_probe_values", {"probe_category","raw_kv_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","field_name","field_value_sample"}, {"probe_category","source_id","store_guid","source_db","inode_num","field_name","field_value_sample"}, "probe_category, store_guid, CAST(inode_num AS INTEGER), raw_kv_id"},
        {L"iOS - Record String Probe Summary", "vw_ios_record_string_probe_summary", {"raw_record_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","file_name","content_type","display_name","full_path","last_updated_utc","time_interpretation","string_probe_rows","distinct_probe_field_count","probe_categories","string_probe_sample","record_state"}, {"source_id","store_guid","source_db","inode_num","file_name","content_type","display_name","full_path","time_interpretation","probe_categories","string_probe_sample","record_state"}, "last_updated_utc DESC, store_guid, CAST(inode_num AS INTEGER), raw_record_id"},
        {L"iOS - String Category Summary", "vw_ios_string_probe_category_summary", {"probe_category","row_count","store_count","distinct_record_count","distinct_value_count","min_sample_value","max_sample_value"}, {"probe_category","min_sample_value","max_sample_value"}, "row_count DESC, probe_category"},
        {L"iOS - Spotlight Date Provenance", "vw_ios_spotlight_date_provenance", {"raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_date_utc","spotlight_date_source_field","spotlight_date_source_table","spotlight_date_raw_value","spotlight_date_parse_method","spotlight_date_type","spotlight_date_source_fields","spotlight_date_parse_methods","spotlight_date_candidate_count","spotlight_date_source_evidence","date_validation_hint","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","spotlight_date_utc","spotlight_date_source_field","spotlight_date_type","date_validation_hint","interpretation_note"}, "spotlight_date_utc DESC, store_guid, CAST(spotlight_inode_or_object_id AS INTEGER), raw_record_id"},
        {L"iOS - Spotlight Investigative Items With Dates", "vw_ios_spotlight_investigative_items_with_dates", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_value_source_field","human_text_category","original_value_length","readable_text_sample","review_priority","spotlight_date_utc","spotlight_date_source_field","spotlight_date_source_table","spotlight_date_raw_value","spotlight_date_parse_method","spotlight_date_type","spotlight_date_semantic_class","spotlight_date_source_evidence","date_validation_hint","date_reporting_caution","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","spotlight_value_source_field","human_text_category","readable_text_sample","review_priority","spotlight_date_utc","spotlight_date_source_field","spotlight_date_semantic_class","date_reporting_caution","date_validation_hint","interpretation_note"}, "review_priority, spotlight_date_utc DESC, raw_kv_id"},
        {L"iOS - Spotlight Investigative Item Date Evidence", "vw_ios_spotlight_investigative_item_date_evidence", {"raw_kv_id","raw_record_id","raw_date_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_value_source_field","human_text_category","review_priority","original_value_length","readable_text_sample","spotlight_date_utc","spotlight_date_source_field","spotlight_date_source_table","spotlight_date_raw_value","spotlight_date_parse_method","spotlight_date_type","spotlight_date_semantic_class","date_association_status","date_association_confidence","value_validation_locator","date_validation_locator","spotlight_record_locator","date_reporting_caution","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","spotlight_value_source_field","human_text_category","readable_text_sample","spotlight_date_utc","spotlight_date_source_field","spotlight_date_semantic_class","date_reporting_caution","date_validation_locator","value_validation_locator","spotlight_record_locator","interpretation_note"}, "review_priority, spotlight_date_utc DESC, raw_kv_id, raw_date_id"},
        {L"iOS - Spotlight Date Field Summary", "vw_ios_spotlight_date_field_summary", {"source_id","store_guid","source_db","spotlight_date_source_field","spotlight_date_semantic_class","raw_date_type","parse_method","date_candidate_count","distinct_spotlight_record_count","earliest_parsed_utc","latest_parsed_utc","min_raw_value_sample","max_raw_value_sample","reporting_caution"}, {"store_guid","source_db","spotlight_date_source_field","spotlight_date_semantic_class","date_candidate_count","distinct_spotlight_record_count","earliest_parsed_utc","latest_parsed_utc","reporting_caution"}, "date_candidate_count DESC, store_guid, spotlight_date_source_field"},
        {L"iOS - Spotlight High-Value Timeline", "vw_ios_spotlight_high_value_timeline", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_value_source_field","human_text_category","original_value_length","readable_text_sample","review_priority","spotlight_date_utc","spotlight_date_source_field","spotlight_date_raw_value","spotlight_date_parse_method","spotlight_date_semantic_class","date_validation_hint","date_reporting_caution","ffs_residency_status","ffs_match_confidence","matched_file_name","matched_size_bytes","matched_zip_modified_utc","matched_protection_class","matched_app_container","matched_domain","app_db_link_status","app_database_category","app_database_name","app_hint","app_family_parsed_record_count","app_family_earliest_record_timestamp_utc","app_family_latest_record_timestamp_utc","investigative_timeline_basis","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","human_text_category","readable_text_sample","spotlight_date_utc","spotlight_date_source_field","spotlight_date_semantic_class","ffs_residency_status","app_db_link_status","investigative_timeline_basis","date_reporting_caution","interpretation_note"}, "review_priority, spotlight_date_utc DESC, raw_record_id, raw_kv_id"},
        {L"iOS - Spotlight File References With Dates", "vw_ios_spotlight_file_reference_review", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_value_source_field","spotlight_file_reference","spotlight_date_utc","spotlight_date_source_field","spotlight_date_raw_value","spotlight_date_parse_method","spotlight_date_semantic_class","date_validation_hint","ffs_residency_status","ffs_match_confidence","matched_file_name","matched_size_bytes","matched_zip_modified_utc","matched_protection_class","matched_app_container","matched_domain","file_reference_status","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","spotlight_file_reference","spotlight_date_utc","spotlight_date_source_field","spotlight_date_semantic_class","ffs_residency_status","file_reference_status","date_validation_hint","interpretation_note"}, "file_reference_status, spotlight_date_utc DESC, raw_record_id, raw_kv_id"},
        {L"iOS - Spotlight URL References With Dates", "vw_ios_spotlight_url_reference_review", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_value_source_field","human_text_category","spotlight_url_or_web_reference","normalized_url_reference_sample","spotlight_date_utc","spotlight_date_source_field","spotlight_date_raw_value","spotlight_date_parse_method","spotlight_date_semantic_class","date_validation_hint","app_db_link_status","app_database_category","app_database_name","app_hint","app_family_parsed_record_count","app_family_earliest_record_timestamp_utc","app_family_latest_record_timestamp_utc","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","human_text_category","spotlight_url_or_web_reference","normalized_url_reference_sample","spotlight_date_utc","spotlight_date_source_field","spotlight_date_semantic_class","app_db_link_status","date_validation_hint","interpretation_note"}, "human_text_category, spotlight_date_utc DESC, raw_record_id, raw_kv_id"},
        {L"iOS - Spotlight Account/Contact References With Dates", "vw_ios_spotlight_account_contact_reference_review", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_value_source_field","human_text_category","spotlight_account_or_contact_reference","spotlight_date_utc","spotlight_date_source_field","spotlight_date_raw_value","spotlight_date_parse_method","spotlight_date_semantic_class","date_validation_hint","app_db_link_status","app_database_category","app_database_name","app_hint","app_family_parsed_record_count","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","human_text_category","spotlight_account_or_contact_reference","spotlight_date_utc","spotlight_date_source_field","spotlight_date_semantic_class","app_db_link_status","date_validation_hint","interpretation_note"}, "human_text_category, spotlight_date_utc DESC, raw_record_id, raw_kv_id"},
        {L"iOS - Spotlight Entity Review", "vw_ios_spotlight_entity_review", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","entity_type","human_text_category","review_priority","spotlight_value_source_field","normalized_entity_value","readable_text_sample","original_value_length","spotlight_date_utc","spotlight_date_source_field","spotlight_date_raw_value","spotlight_date_parse_method","spotlight_date_semantic_class","date_validation_hint","date_reporting_caution","ffs_residency_status","ffs_match_confidence","matched_file_name","matched_zip_modified_utc","matched_protection_class","matched_app_container","matched_domain","app_db_link_status","app_database_category","app_database_name","app_hint","matched_record_category","matched_table_name","sample_app_db_value","reference_validation_locator","date_validation_locator","interpretation_note"}, {"entity_type","human_text_category","normalized_entity_value","readable_text_sample","spotlight_date_utc","spotlight_date_source_field","spotlight_date_semantic_class","ffs_residency_status","app_db_link_status","reference_validation_locator","date_validation_locator","interpretation_note"}, "review_priority, entity_type, spotlight_date_utc DESC, raw_record_id, raw_kv_id"},
        {L"iOS - Spotlight Entity Summary", "vw_ios_spotlight_entity_summary", {"entity_type","human_text_category","review_priority","store_guid","source_db","spotlight_value_source_field","spotlight_date_semantic_class","entity_row_count","distinct_spotlight_record_count","distinct_normalized_entity_count","ffs_present_context_count","app_db_present_context_count","earliest_spotlight_date_utc","latest_spotlight_date_utc","min_sample_entity","max_sample_entity","interpretation_note"}, {"entity_type","human_text_category","review_priority","store_guid","spotlight_value_source_field","entity_row_count","distinct_spotlight_record_count","distinct_normalized_entity_count","earliest_spotlight_date_utc","latest_spotlight_date_utc","interpretation_note"}, "entity_row_count DESC, entity_type, store_guid, spotlight_value_source_field"},
        {L"iOS - Spotlight Native Parser Targets", "vw_ios_spotlight_native_parser_targets", {"source_id","store_guid","source_db","parser_target_type","target_name","target_count","store_raw_record_count","recovered_key_value_count","human_text_value_count","pct_records_with_human_text","native_decode_failures","parser_priority","recommended_next_step","interpretation_note"}, {"store_guid","source_db","parser_target_type","target_name","target_count","parser_priority","recommended_next_step","interpretation_note"}, "parser_priority, target_count DESC, parser_target_type, store_guid"},
        {L"iOS - Spotlight dbStr Map Inventory", "vw_ios_spotlight_dbstr_map_inventory", {"source_id","store_guid","source_db","map_id","map_role","data_exists","offsets_exists","header_exists","data_bytes","offsets_bytes","header_bytes","offset_entries","parsed_entries","skipped_entries","status","message","parser_use_status","created_utc"}, {"store_guid","source_db","map_id","map_role","parsed_entries","offset_entries","status","parser_use_status","message"}, "store_guid, source_db, map_id"},
        {L"iOS - Spotlight Dictionary Coverage", "vw_ios_spotlight_dictionary_coverage", {"source_id","store_guid","source_db","raw_record_count","property_count","category_count","index_rows","index_value_ref_count","dbstr_property_entries","dbstr_category_entries","apple_or_md_named_property_count","generic_probe_named_property_count","property_decode_status","dbstr_status_summary"}, {"store_guid","source_db","raw_record_count","property_count","category_count","index_rows","dbstr_property_entries","dbstr_category_entries","property_decode_status","dbstr_status_summary"}, "raw_record_count DESC, store_guid, source_db"},
        {L"iOS - Bplist/NSKeyedArchiver Summary", "vw_ios_spotlight_bplist_nskeyedarchiver_summary", {"source_id","store_guid","source_db","bplist_detection_status","spotlight_record_count","distinct_spotlight_object_count","earliest_last_updated_utc","latest_last_updated_utc","min_context_sample","max_context_sample","interpretation_note"}, {"store_guid","source_db","bplist_detection_status","spotlight_record_count","distinct_spotlight_object_count","min_context_sample","max_context_sample","interpretation_note"}, "spotlight_record_count DESC, store_guid, source_db, bplist_detection_status"},
        {L"iOS - Bplist/NSKeyedArchiver Detail", "vw_ios_spotlight_bplist_nskeyedarchiver_detail", {"raw_kv_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","raw_record_id","last_updated_utc","file_name","display_name","content_type","full_path","parser_context_field","bplist_context","bplist_detection_status","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","last_updated_utc","bplist_context","bplist_detection_status","interpretation_note"}, "last_updated_utc DESC, store_guid, raw_kv_id"},
        {L"iOS - Investigator Time Anomalies", "vw_investigator_time_anomalies", {"artifact_id","source_id","store_guid","spotlight_inode_or_object_id","file_name","display_name","best_path","content_type","last_updated_utc","downloaded_date_utc","first_used_candidate_utc","last_used_date_utc","anomaly_type","interpretation_note"}, {"store_guid","spotlight_inode_or_object_id","file_name","display_name","best_path","last_updated_utc","downloaded_date_utc","first_used_candidate_utc","last_used_date_utc","anomaly_type","interpretation_note"}, "anomaly_type, last_updated_utc DESC, artifact_id"},
        {L"iOS - KnowledgeC Interaction Summary", "vw_ios_knowledgec_interaction_summary", {"database_category","app_hint","knowledge_stream_name","app_bundle_id","parse_status","event_count","earliest_event_utc","latest_event_utc","min_event_sample","max_event_sample","interpretation_note"}, {"database_category","app_hint","knowledge_stream_name","app_bundle_id","parse_status","min_event_sample","max_event_sample","interpretation_note"}, "event_count DESC, latest_event_utc DESC, app_bundle_id"},
        {L"iOS - KnowledgeC Interaction Events", "vw_ios_knowledgec_interaction_events", {"ios_app_record_id","source_id","ios_db_id","database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","app_bundle_id","interaction_type","knowledge_stream_name","text_snippet","parse_status","provenance","created_utc","interpretation_note"}, {"database_normalized_path","database_name","database_category","app_hint","table_name","record_timestamp_utc","timestamp_source","app_bundle_id","interaction_type","knowledge_stream_name","text_snippet","parse_status","provenance","interpretation_note"}, "record_timestamp_utc DESC, ios_app_record_id"},
        {L"iOS - Investigator Super Timeline", "vw_investigator_super_timeline", {"event_utc","source_module","category","action","app_context","target","details","provenance","interpretation_note"}, {"event_utc","source_module","category","action","app_context","target","details","provenance","interpretation_note"}, "event_utc DESC, source_module, category"},
        {L"iOS - Spotlight Apple Field Coverage", "vw_ios_spotlight_apple_field_coverage", {"source_id","store_guid","source_db","field_name","apple_semantic_group","value_row_count","distinct_record_count","sample_value","interpretation_note"}, {"store_guid","source_db","apple_semantic_group","field_name","value_row_count","distinct_record_count","sample_value","interpretation_note"}, "value_row_count DESC, apple_semantic_group, field_name"},
        {L"iOS - Spotlight Decode Gap Summary", "vw_ios_spotlight_decode_gap_summary", {"source_id","store_guid","source_db","decode_gap_status","gap_record_count","earliest_gap_last_updated_utc","latest_gap_last_updated_utc","store_raw_record_count","recovered_key_value_count","human_text_value_count","pct_records_with_human_text","native_decode_failures","native_decode_status","interpretation_note"}, {"store_guid","source_db","decode_gap_status","gap_record_count","store_raw_record_count","recovered_key_value_count","human_text_value_count","pct_records_with_human_text","native_decode_failures","native_decode_status","interpretation_note"}, "gap_record_count DESC, store_guid"},
        {L"iOS - Spotlight Decode Coverage Summary", "vw_ios_spotlight_decode_coverage_summary", {"source_id","store_guid","source_db","decode_mode","spotlight_version","raw_record_count","recovered_key_value_count","recovered_field_name_count","records_with_recovered_values","human_text_value_count","records_with_human_text","human_text_category_count","pct_records_with_human_text","native_property_count","native_category_count","metadata_blocks","decompressed_blocks","decode_failures","decode_status","earliest_last_updated_utc","latest_last_updated_utc","spotlight_decode_interpretation","interpretation_note"}, {"store_guid","source_db","decode_mode","raw_record_count","recovered_key_value_count","records_with_human_text","pct_records_with_human_text","native_property_count","decode_status","spotlight_decode_interpretation","interpretation_note"}, "raw_record_count DESC, store_guid"},
        {L"iOS - Spotlight Field Coverage Summary", "vw_ios_spotlight_field_coverage_summary", {"source_id","store_guid","source_db","field_name","value_row_count","distinct_record_count","min_value_length","max_value_length","min_sample_value","max_sample_value","field_decode_status","interpretation_note"}, {"store_guid","source_db","field_name","value_row_count","distinct_record_count","field_decode_status","min_sample_value","max_sample_value","interpretation_note"}, "value_row_count DESC, store_guid, field_name"},
        {L"iOS - Spotlight Text Category Summary", "vw_ios_spotlight_text_category_summary", {"human_text_category","review_priority","text_value_count","distinct_spotlight_record_count","store_count","min_original_value_length","max_original_value_length","min_sample_text","max_sample_text","interpretation_note"}, {"human_text_category","review_priority","text_value_count","distinct_spotlight_record_count","min_sample_text","max_sample_text","interpretation_note"}, "text_value_count DESC, human_text_category"},
        {L"iOS - Spotlight Object/Inode Diagnostic Summary", "vw_ios_spotlight_object_inode_diagnostic_summary", {"source_id","store_guid","source_db","object_record_bucket","object_count","raw_record_count","min_records_per_object","max_records_per_object","interpretation_note"}, {"store_guid","source_db","object_record_bucket","object_count","raw_record_count","min_records_per_object","max_records_per_object","interpretation_note"}, "raw_record_count DESC, object_count DESC, source_id, store_guid, object_record_bucket"},
        {L"iOS - Spotlight Object/Inode Summary", "vw_ios_spotlight_object_inode_summary", {"source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","raw_record_count","distinct_parent_id_count","raw_key_value_rows","distinct_spotlight_field_count","date_candidate_rows","earliest_last_updated_utc","latest_last_updated_utc","earliest_spotlight_date_utc","latest_spotlight_date_utc","first_raw_record_id","last_raw_record_id","spotlight_text_context_sample","object_materialization_status","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","raw_record_count","raw_key_value_rows","spotlight_text_context_sample","object_materialization_status","interpretation_note"}, "raw_record_count DESC, raw_key_value_rows DESC, latest_last_updated_utc DESC, source_id, store_guid, spotlight_inode_or_object_id, spotlight_store_id"},
        {L"iOS - Spotlight Record Review", "vw_ios_spotlight_record_review", {"raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_date_utc","spotlight_date_source_field","spotlight_date_source_table","spotlight_date_raw_value","spotlight_date_parse_method","spotlight_date_type","spotlight_date_validation_hint","collapsed_date_candidate_count","last_updated_utc","time_interpretation","spotlight_text_value_count","spotlight_text_category_count","spotlight_text_categories","spotlight_text_rollup_sample","ffs_reference_count","ffs_present_reference_count","ffs_missing_or_unresolved_reference_count","app_db_candidate_count","app_db_present_candidate_count","app_db_unresolved_candidate_count","spotlight_review_priority","spotlight_decode_status","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","spotlight_date_utc","spotlight_date_source_field","spotlight_date_type","collapsed_date_candidate_count","last_updated_utc","spotlight_text_categories","spotlight_text_rollup_sample","spotlight_review_priority","spotlight_decode_status","interpretation_note"}, "spotlight_review_priority, last_updated_utc DESC, raw_record_id"},
        {L"iOS - Spotlight Decode Gap Records", "vw_ios_spotlight_decode_gap_records", {"raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","last_updated_utc","file_name","display_name","full_path","content_type","record_state","decode_gap_status","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","last_updated_utc","decode_gap_status","interpretation_note"}, "last_updated_utc DESC, store_guid, raw_record_id"},
        {L"iOS - Index Update Timeline", "vw_ios_timeline_index_updates", {"raw_record_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","file_name","content_type","display_name","full_path","last_updated_utc","time_interpretation","record_state"}, {"source_id","store_guid","source_db","inode_num","file_name","content_type","display_name","full_path","time_interpretation","record_state"}, "last_updated_utc DESC, store_guid, CAST(inode_num AS INTEGER)"},
        {L"iOS - Parsed Artifacts", "vw_ios_artifacts", {"artifact_id","source_id","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","content_type","last_updated_utc","confidence"}, {"source_id","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","content_type","confidence"}, "COALESCE(NULLIF(last_updated_utc,''), artifact_id) DESC"},
        {L"iOS - FFS File Inventory", "vw_ios_ffs_file_inventory", {"ios_file_id","source_id","normalized_path","original_zip_entry","file_name","extension","size_bytes","zip_modified_utc","protection_class_hint","app_container_hint","domain_hint","sha256_status","inventory_notes"}, {"normalized_path","original_zip_entry","file_name","extension","protection_class_hint","app_container_hint","domain_hint","inventory_notes"}, "normalized_path"},
        {L"iOS - App Database Inventory", "vw_ios_database_artifact_inventory", {"ios_db_id","source_id","normalized_path","original_zip_entry","database_name","database_category","app_hint","protection_class_hint","size_bytes","zip_modified_utc","parse_status","record_inventory_status","notes"}, {"normalized_path","original_zip_entry","database_name","database_category","app_hint","protection_class_hint","parse_status","record_inventory_status","notes"}, "database_category, app_hint, normalized_path"},
        {L"iOS - Spotlight Referenced Paths", "vw_ios_spotlight_referenced_paths", {"reference_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","field_name","reference_type","normalized_ios_path","confidence","raw_reference_value","notes"}, {"store_guid","source_db","inode_num","field_name","reference_type","normalized_ios_path","confidence","raw_reference_value","notes"}, "reference_type, normalized_ios_path, reference_id"},
        {L"iOS - Missing From FFS Text Detail", "vw_ios_spotlight_missing_from_ffs_text_detail", {"reference_id","raw_record_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","spotlight_file_name","spotlight_display_name","spotlight_content_type","spotlight_last_updated_utc","missing_reference_source_field","reference_type","raw_reference_value","normalized_ios_path","missing_candidate_category","investigative_priority","investigative_reason","spotlight_text_preview","spotlight_text_full_or_sample","spotlight_text_length","spotlight_text_source_field","spotlight_text_visibility_status","spotlight_text_context_status","ffs_lookup_source","ffs_lookup_status","residency_status","confidence","missing_reference_validation_locator","spotlight_record_locator","content_visibility_note","interpretation_note"}, {"store_guid","source_db","spotlight_last_updated_utc","missing_reference_source_field","normalized_ios_path","missing_candidate_category","investigative_priority","spotlight_text_preview","spotlight_text_visibility_status","ffs_lookup_source","ffs_lookup_status","residency_status","confidence","raw_reference_value","missing_reference_validation_locator","spotlight_record_locator","content_visibility_note"}, "investigative_priority_sort, spotlight_text_visibility_status DESC, residency_status, confidence, normalized_ios_path, reference_id"},
        {L"iOS - Missing From FFS Text Coverage", "vw_ios_spotlight_missing_from_ffs_text_coverage_summary", {"source_id","store_guid","source_db","missing_candidate_category","investigative_priority","spotlight_text_visibility_status","missing_candidate_count","distinct_spotlight_object_count","candidates_with_visible_text","candidates_without_visible_text","earliest_spotlight_last_updated_utc","latest_spotlight_last_updated_utc","min_missing_path_sample","max_missing_path_sample","spotlight_text_sample","interpretation_note"}, {"store_guid","source_db","missing_candidate_category","investigative_priority","spotlight_text_visibility_status","missing_candidate_count","candidates_with_visible_text","candidates_without_visible_text","spotlight_text_sample","interpretation_note"}, "investigative_priority, missing_candidate_count DESC, store_guid, missing_candidate_category"},
        {L"iOS - Missing From FFS Candidates", "vw_ios_spotlight_missing_from_ffs_candidates", {"reference_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","field_name","raw_reference_value","reference_type","normalized_ios_path","missing_candidate_category","investigative_priority","investigative_reason","spotlight_text_context_sample","spotlight_text_context_status","matched_file_name","matched_size_bytes","matched_zip_modified_utc","matched_protection_class","matched_app_container","matched_domain","ffs_lookup_source","ffs_lookup_status","residency_status","confidence","interpretation_note"}, {"store_guid","source_db","inode_num","field_name","reference_type","normalized_ios_path","missing_candidate_category","investigative_priority","investigative_reason","spotlight_text_context_sample","spotlight_text_context_status","ffs_lookup_source","ffs_lookup_status","residency_status","confidence","matched_file_name","matched_app_container","matched_domain","raw_reference_value","interpretation_note"}, "investigative_priority_sort, residency_status, confidence, normalized_ios_path, reference_id"},
        {L"iOS - High-Value Missing From FFS", "vw_ios_spotlight_missing_from_ffs_high_value_candidates", {"reference_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","field_name","raw_reference_value","reference_type","normalized_ios_path","missing_candidate_category","investigative_priority","investigative_reason","spotlight_text_context_sample","spotlight_text_context_status","matched_file_name","matched_size_bytes","matched_zip_modified_utc","matched_protection_class","matched_app_container","matched_domain","ffs_lookup_source","ffs_lookup_status","residency_status","confidence","interpretation_note"}, {"store_guid","source_db","inode_num","field_name","reference_type","normalized_ios_path","missing_candidate_category","investigative_priority","investigative_reason","spotlight_text_context_sample","spotlight_text_context_status","ffs_lookup_source","ffs_lookup_status","residency_status","confidence","matched_file_name","matched_app_container","matched_domain","raw_reference_value","interpretation_note"}, "investigative_priority_sort, residency_status, confidence, normalized_ios_path, reference_id"},
        {L"iOS - Missing From FFS Summary", "vw_ios_spotlight_missing_from_ffs_summary", {"source_id","store_guid","source_db","field_name","reference_type","missing_candidate_category","investigative_priority","spotlight_text_context_status","ffs_lookup_status","missing_candidate_count","distinct_spotlight_object_count","distinct_missing_path_count","min_missing_path_sample","max_missing_path_sample","spotlight_text_context_sample","investigative_reason","interpretation_note"}, {"store_guid","source_db","field_name","reference_type","missing_candidate_category","investigative_priority","spotlight_text_context_status","ffs_lookup_status","missing_candidate_count","distinct_missing_path_count","min_missing_path_sample","max_missing_path_sample","spotlight_text_context_sample","investigative_reason","interpretation_note"}, "investigative_priority_sort, missing_candidate_count DESC, store_guid, field_name, reference_type"},
        {L"iOS - High-Value Missing From FFS Summary", "vw_ios_spotlight_missing_from_ffs_high_value_summary", {"source_id","store_guid","source_db","field_name","reference_type","missing_candidate_category","investigative_priority","spotlight_text_context_status","ffs_lookup_status","missing_candidate_count","distinct_spotlight_object_count","distinct_missing_path_count","min_missing_path_sample","max_missing_path_sample","spotlight_text_context_sample","investigative_reason","interpretation_note"}, {"store_guid","source_db","field_name","reference_type","missing_candidate_category","investigative_priority","spotlight_text_context_status","ffs_lookup_status","missing_candidate_count","distinct_missing_path_count","min_missing_path_sample","max_missing_path_sample","spotlight_text_context_sample","investigative_reason","interpretation_note"}, "investigative_priority_sort, missing_candidate_count DESC, store_guid, field_name, reference_type"},
        {L"iOS - Spotlight Text Context Review", "vw_ios_spotlight_text_context_review", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","last_updated_utc","file_name","display_name","content_type","text_context_category","review_priority","text_context_reason","classification_evidence","spotlight_text_context_sample","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","last_updated_utc","file_name","display_name","content_type","text_context_category","review_priority","text_context_reason","classification_evidence","spotlight_text_context_sample","interpretation_note"}, "review_priority_sort, last_updated_utc DESC, raw_record_id DESC"},
        {L"iOS - High-Value Spotlight Text Context", "vw_ios_spotlight_high_value_text_context_review", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","last_updated_utc","file_name","display_name","content_type","text_context_category","review_priority","text_context_reason","classification_evidence","spotlight_text_context_sample","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","last_updated_utc","file_name","display_name","content_type","text_context_category","review_priority","text_context_reason","classification_evidence","spotlight_text_context_sample","interpretation_note"}, "review_priority_sort, last_updated_utc DESC, raw_record_id DESC"},
        {L"iOS - Spotlight Chat App Attribution Summary", "vw_ios_spotlight_chat_app_attribution_summary", {"text_context_category","review_priority","classification_evidence","context_record_count","distinct_spotlight_object_count","earliest_last_updated_utc","latest_last_updated_utc","min_text_context_sample","max_text_context_sample","text_context_reason","interpretation_note"}, {"text_context_category","review_priority","classification_evidence","context_record_count","distinct_spotlight_object_count","min_text_context_sample","max_text_context_sample","text_context_reason","interpretation_note"}, "text_context_category, context_record_count DESC"},

        {L"iOS - Spotlight Communications Investigator Review", "vw_ios_spotlight_communication_record_review", {"raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_date_utc","spotlight_date_source_field","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","message_domain_handle_or_chat","message_service","best_title_or_name","investigator_visible_text","description_or_snippet","account_identifier","phone_or_callback","callback_url","url_or_content_reference","attachment_or_media_path","message_identifier","mailbox_or_thread","mail_participants","saved_from_app","spotlight_text_context_sample","interpretation_note","validation_locator"}, {"store_guid","source_db","spotlight_date_utc","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","message_domain_handle_or_chat","message_service","best_title_or_name","investigator_visible_text","description_or_snippet","account_identifier","phone_or_callback","url_or_content_reference","attachment_or_media_path","message_identifier","mail_participants","saved_from_app","spotlight_text_context_sample","interpretation_note","validation_locator"}, "review_priority_sort, spotlight_date_utc DESC, raw_record_id DESC"},
        {L"iOS - Spotlight Message Text Review", "vw_ios_spotlight_message_text_review", {"raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_date_utc","spotlight_date_source_field","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","message_domain_handle_or_chat","best_title_or_name","investigator_visible_text","description_or_snippet","snippet","account_identifier","message_service","phone_or_callback","callback_url","url_or_content_reference","attachment_or_media_path","message_identifier","mailbox_or_thread","mail_participants","spotlight_text_context_sample","interpretation_note","validation_locator"}, {"store_guid","source_db","spotlight_date_utc","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","message_domain_handle_or_chat","best_title_or_name","investigator_visible_text","account_identifier","message_service","url_or_content_reference","attachment_or_media_path","message_identifier","mail_participants","spotlight_text_context_sample","interpretation_note","validation_locator"}, "review_priority_sort, spotlight_date_utc DESC, raw_record_id DESC"},
        {L"iOS - Spotlight Message Body Review", "vw_ios_spotlight_message_body_review", {"raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_date_utc","spotlight_date_source_field","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","message_domain_handle_or_chat","suggested_contact_name","conversation_or_thread_title","extracted_message_text_or_subject","body_review_bucket","noise_hint","investigator_visible_text","account_identifier","message_service","phone_or_callback","url_or_content_reference","attachment_or_media_path","message_identifier","mail_participants","spotlight_text_context_sample","validation_locator","interpretation_note"}, {"store_guid","source_db","spotlight_date_utc","communication_context_type","bundle_id","domain_identifier","message_domain_handle_or_chat","suggested_contact_name","conversation_or_thread_title","extracted_message_text_or_subject","body_review_bucket","noise_hint","spotlight_text_context_sample","validation_locator","interpretation_note"}, "noise_hint, spotlight_date_utc DESC, raw_record_id DESC"},
        {L"iOS - User-Focus Message Body Review", "vw_ios_spotlight_user_focus_message_review", {"raw_record_id","source_id","store_guid","source_db","spotlight_date_utc","communication_context_type","bundle_id","domain_identifier","message_domain_handle_or_chat","suggested_contact_name","conversation_or_thread_title","extracted_message_text_or_subject","body_review_bucket","noise_hint","account_identifier","message_service","url_or_content_reference","attachment_or_media_path","spotlight_text_context_sample","validation_locator","interpretation_note"}, {"store_guid","source_db","spotlight_date_utc","communication_context_type","bundle_id","domain_identifier","message_domain_handle_or_chat","suggested_contact_name","conversation_or_thread_title","extracted_message_text_or_subject","body_review_bucket","noise_hint","spotlight_text_context_sample","validation_locator","interpretation_note"}, "spotlight_date_utc DESC, raw_record_id DESC"},
        {L"iOS - Message Contact/Thread Summary", "vw_ios_spotlight_message_contact_summary", {"communication_context_type","bundle_id","content_type","body_review_bucket","noise_hint","handle_bucket","spotlight_record_count","distinct_spotlight_object_count","rows_with_extracted_message_text","rows_with_thread_or_snippet_text","distinct_handle_or_thread_count","earliest_spotlight_date_utc","latest_spotlight_date_utc","min_message_sample","max_message_sample","interpretation_note"}, {"communication_context_type","bundle_id","content_type","body_review_bucket","noise_hint","handle_bucket","spotlight_record_count","distinct_handle_or_thread_count","min_message_sample","max_message_sample","interpretation_note"}, "noise_hint, spotlight_record_count DESC, latest_spotlight_date_utc DESC"},
        {L"iOS - Message Contact/Thread Detail Sample", "vw_ios_spotlight_message_contact_thread_detail_sample", {"communication_context_type","bundle_id","domain_identifier","message_domain_handle_or_chat","suggested_contact_name","conversation_or_thread_title","body_review_bucket","noise_hint","spotlight_record_count","distinct_spotlight_object_count","rows_with_extracted_message_text","earliest_spotlight_date_utc","latest_spotlight_date_utc","min_message_sample","max_message_sample","interpretation_note"}, {"communication_context_type","bundle_id","domain_identifier","message_domain_handle_or_chat","suggested_contact_name","conversation_or_thread_title","body_review_bucket","noise_hint","min_message_sample","max_message_sample","interpretation_note"}, "noise_hint, rows_with_extracted_message_text DESC, spotlight_record_count DESC"},
        {L"iOS - Message Body Focus Summary", "vw_ios_spotlight_message_body_focus_summary", {"noise_hint","body_review_bucket","communication_context_type","bundle_id","content_type","spotlight_record_count","distinct_spotlight_object_count","rows_with_extracted_text","rows_with_supporting_text","earliest_spotlight_date_utc","latest_spotlight_date_utc","min_review_sample","max_review_sample","interpretation_note"}, {"noise_hint","body_review_bucket","communication_context_type","bundle_id","content_type","spotlight_record_count","min_review_sample","max_review_sample","interpretation_note"}, "spotlight_record_count DESC, noise_hint, body_review_bucket"},
        {L"iOS - Parser Diagnostics Action Summary", "vw_parser_diagnostics_action_summary", {"diagnostic_source","diagnostic_category","diagnostic_count","diagnostic_severity","recommended_action","first_seen_utc","last_seen_utc","sample_min_message","sample_max_message","interpretation_note"}, {"diagnostic_source","diagnostic_category","diagnostic_count","diagnostic_severity","recommended_action","sample_min_message","sample_max_message","interpretation_note"}, "diagnostic_severity, diagnostic_count DESC"},
        {L"iOS - Plaso/L2T Timeline Sample", "vw_ios_spotlight_plaso_l2tcsv_timeline_sample", {"date","time","timezone","MACB","source","sourcetype","type","user","host","short","desc","version","filename","inode","notes","format","extra"}, {"date","time","timezone","MACB","source","sourcetype","type","short","desc","filename","inode","notes","extra"}, "date DESC, time DESC"},
        {L"iOS - Case Quality Dashboard", "vw_ios_spotlight_case_quality_dashboard", {"quality_area","metric","value","interpretation_note"}, {"quality_area","metric","value","interpretation_note"}, "quality_area, metric"},
        {L"iOS - Investigator Overview", "vw_ios_spotlight_investigator_overview", {"review_order","review_area","gui_view","sqlite_view","row_count","why_review_this"}, {"review_order","review_area","gui_view","row_count","why_review_this"}, "review_order"},
        {L"iOS - Direct User Message Review", "vw_ios_spotlight_direct_user_message_review", {"raw_record_id","spotlight_date_utc","message_domain_handle_or_chat","suggested_contact_name","conversation_or_thread_title","message_text","message_text_length","content_type","bundle_id","domain_identifier","spotlight_date_source_field","validation_locator","interpretation_note"}, {"spotlight_date_utc","message_domain_handle_or_chat","suggested_contact_name","conversation_or_thread_title","message_text","content_type","validation_locator","interpretation_note"}, "spotlight_date_utc DESC, raw_record_id DESC"},
        {L"iOS - Direct User Message Thread Summary", "vw_ios_spotlight_direct_user_message_thread_summary", {"thread_or_contact_key","suggested_contact_name","message_domain_handle_or_chat","conversation_or_thread_title","bundle_id","content_type","spotlight_message_record_count","distinct_spotlight_object_count","earliest_spotlight_date_utc","latest_spotlight_date_utc","min_message_text_length","max_message_text_length","avg_message_text_length","min_message_sample","max_message_sample","interpretation_note"}, {"thread_or_contact_key","spotlight_message_record_count","earliest_spotlight_date_utc","latest_spotlight_date_utc","min_message_sample","max_message_sample","interpretation_note"}, "spotlight_message_record_count DESC, latest_spotlight_date_utc DESC"},
        {L"iOS - Timeline Month Summary", "vw_ios_spotlight_timeline_month_summary", {"event_month_utc","review_category","event_type","bundle_id","content_type","date_anomaly_flag","timeline_event_count","distinct_spotlight_record_count","earliest_event_time_utc","latest_event_time_utc","min_event_sample","max_event_sample","interpretation_note"}, {"event_month_utc","review_category","event_type","date_anomaly_flag","timeline_event_count","earliest_event_time_utc","latest_event_time_utc","interpretation_note"}, "event_month_utc DESC, timeline_event_count DESC"},
        {L"iOS - Normalized Spotlight Timeline", "vw_ios_spotlight_normalized_timeline", {"event_time_utc","event_type","source_field","review_category","event_summary","contact_or_thread","bundle_id","content_type","date_anomaly_flag","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","validation_locator","interpretation_note"}, {"event_time_utc","event_type","source_field","review_category","event_summary","contact_or_thread","bundle_id","date_anomaly_flag","validation_locator","interpretation_note"}, "event_time_utc DESC, raw_record_id DESC"},
        {L"iOS - Timeline Anomaly Summary", "vw_ios_spotlight_timeline_anomaly_summary", {"date_anomaly_flag","event_type","review_category","timeline_row_count","distinct_spotlight_object_count","earliest_event_utc","latest_event_utc","min_event_sample","max_event_sample","interpretation_note"}, {"date_anomaly_flag","event_type","review_category","min_event_sample","max_event_sample","interpretation_note"}, "date_anomaly_flag, timeline_row_count DESC"},
        {L"iOS - Parser Diagnostics Summary", "vw_parser_diagnostics_summary", {"diagnostic_source","diagnostic_category","diagnostic_count","first_seen_utc","last_seen_utc","sample_min_message","sample_max_message","interpretation_note"}, {"diagnostic_source","diagnostic_category","sample_min_message","sample_max_message","interpretation_note"}, "diagnostic_count DESC, diagnostic_source, diagnostic_category"},
        {L"iOS - Parser Diagnostics Detail Sample", "vw_parser_diagnostics_detail_sample", {"diagnostic_source","diagnostic_row_id","created_utc","diagnostic_category","store_guid","source_db","spotlight_record_locator","diagnostic_message","interpretation_note"}, {"diagnostic_source","diagnostic_category","store_guid","source_db","spotlight_record_locator","diagnostic_message","interpretation_note"}, "diagnostic_source, diagnostic_row_id"},
        {L"iOS - Case Provenance Summary", "vw_case_provenance_summary", {"provenance_scope","provenance_key","provenance_value","interpretation_note"}, {"provenance_scope","provenance_key","provenance_value","interpretation_note"}, "provenance_scope, provenance_key"},
        {L"iOS - Spotlight Message Media Review", "vw_ios_spotlight_message_media_review", {"raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_date_utc","spotlight_date_source_field","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","saved_from_app","best_title_or_name","investigator_visible_text","description_or_snippet","attachment_or_media_path","url_or_content_reference","spotlight_text_context_sample","validation_locator","interpretation_note"}, {"store_guid","source_db","spotlight_date_utc","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","saved_from_app","best_title_or_name","investigator_visible_text","attachment_or_media_path","url_or_content_reference","spotlight_text_context_sample","validation_locator","interpretation_note"}, "spotlight_date_utc DESC, raw_record_id DESC"},
        {L"iOS - Spotlight Communication Summary", "vw_ios_spotlight_communication_summary", {"communication_context_type","review_priority","bundle_id","domain_identifier","message_service","content_type","spotlight_record_count","distinct_spotlight_object_count","rows_with_title_or_name","rows_with_investigator_visible_text","rows_with_description_or_snippet","rows_with_attachment_or_media_path","rows_with_url_or_content_reference","rows_with_phone_or_callback","earliest_spotlight_date_utc","latest_spotlight_date_utc","min_context_sample","max_context_sample","interpretation_note"}, {"communication_context_type","review_priority","bundle_id","domain_identifier","message_service","content_type","spotlight_record_count","rows_with_title_or_name","rows_with_investigator_visible_text","rows_with_description_or_snippet","rows_with_attachment_or_media_path","rows_with_url_or_content_reference","rows_with_phone_or_callback","min_context_sample","max_context_sample","interpretation_note"}, "review_priority_sort, spotlight_record_count DESC, communication_context_type"},
        {L"iOS - Spotlight Attachment/Media References", "vw_ios_spotlight_attachment_reference_review", {"raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_date_utc","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","best_title_or_name","investigator_visible_text","description_or_snippet","attachment_or_media_path","url_or_content_reference","spotlight_text_context_sample","attachment_reference_basis","validation_locator","interpretation_note"}, {"store_guid","source_db","spotlight_date_utc","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","best_title_or_name","investigator_visible_text","description_or_snippet","attachment_or_media_path","url_or_content_reference","spotlight_text_context_sample","attachment_reference_basis","validation_locator","interpretation_note"}, "spotlight_date_utc DESC, communication_context_type, raw_record_id DESC"},
        {L"iOS - Spotlight Text Context Priority Summary", "vw_ios_spotlight_text_context_priority_summary", {"text_context_category","review_priority","text_context_record_count","distinct_spotlight_object_count","rows_with_last_updated","earliest_last_updated_utc","latest_last_updated_utc","min_text_context_sample","max_text_context_sample","classification_evidence","text_context_reason","interpretation_note"}, {"text_context_category","review_priority","text_context_record_count","distinct_spotlight_object_count","min_text_context_sample","max_text_context_sample","classification_evidence","text_context_reason","interpretation_note"}, "review_priority_sort, text_context_record_count DESC"},
        {L"iOS - Residency Summary", "vw_ios_spotlight_residency_summary", {"residency_status","confidence","reference_count","distinct_record_count","first_path_sample","last_path_sample"}, {"residency_status","confidence","first_path_sample","last_path_sample"}, "residency_status, confidence"},
        {L"iOS - App Database Record Inventory", "vw_ios_app_database_record_inventory", {"ios_record_inventory_id","source_id","ios_db_id","database_normalized_path","database_name","database_category","app_hint","table_name","row_count","sample_columns","record_category","parse_status","notes","created_utc"}, {"database_normalized_path","database_name","database_category","app_hint","table_name","sample_columns","record_category","parse_status","notes"}, "database_category,database_name,table_name"},
        {L"iOS - App Database Record Summary", "vw_ios_app_database_record_summary", {"database_category","app_hint","record_category","parse_status","table_count","total_rows","first_database","last_database"}, {"database_category","app_hint","record_category","parse_status","first_database","last_database"}, "database_category,record_category"},
        {L"iOS - Parsed App Records", "vw_ios_app_parsed_records", {"ios_app_record_id","source_id","ios_db_id","database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","url","title","file_path","item_identifier","text_snippet","parse_status","provenance","created_utc"}, {"database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","url","title","file_path","item_identifier","text_snippet","parse_status"}, "database_category,record_category,record_timestamp_utc,database_name,table_name,ios_app_record_id"},
        {L"iOS - Apple Messages Database Status", "vw_ios_apple_messages_database_status", {"ios_db_id","source_id","normalized_path","database_name","app_hint","parse_status","record_inventory_status","message_rows","chat_rows","attachment_rows","handle_rows","join_rows","parsed_message_rows","relevant_table_counts","apple_messages_residency_status","interpretation_note"}, {"normalized_path","database_name","app_hint","parse_status","record_inventory_status","relevant_table_counts","apple_messages_residency_status","interpretation_note"}, "apple_messages_residency_status, normalized_path"},
        {L"iOS - App Live Activity Timeline", "vw_ios_app_live_activity_timeline", {"ios_app_record_id","source_id","ios_db_id","database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","url","title","file_path","item_identifier","text_snippet","parse_status","provenance","timeline_basis","interpretation_note"}, {"database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","url","title","file_path","item_identifier","text_snippet","parse_status","provenance","timeline_basis","interpretation_note"}, "record_timestamp_utc DESC, database_category, record_category, ios_app_record_id"},
        {L"iOS - Communications Review Records", "vw_ios_communications_review_records", {"ios_app_record_id","source_id","ios_db_id","communication_source","database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","url","title","file_path","item_identifier","text_snippet","parse_status","provenance","communication_record_type","timeline_basis","interpretation_note"}, {"communication_source","database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","url","title","file_path","item_identifier","text_snippet","parse_status","provenance","communication_record_type","timeline_basis","interpretation_note"}, "communication_source, record_timestamp_utc DESC, communication_record_type, ios_app_record_id"},
        {L"iOS - Communications Review Summary", "vw_ios_communications_review_summary", {"communication_source","database_category","app_hint","record_category","communication_record_type","parse_status","parsed_record_count","database_count","earliest_record_timestamp_utc","latest_record_timestamp_utc","records_with_contact_or_participant","records_with_text","records_with_url","records_with_file_path","first_database_path","last_database_path"}, {"communication_source","database_category","app_hint","record_category","communication_record_type","parse_status","first_database_path","last_database_path"}, "communication_source, record_category, communication_record_type"},
        {L"iOS - Spotlight Communication Candidates", "vw_ios_spotlight_communication_candidates", {"raw_kv_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","spotlight_parent_id","field_name","communication_candidate_type","spotlight_last_updated_utc","string_value_sample","communication_residency_context","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","field_name","communication_candidate_type","spotlight_last_updated_utc","string_value_sample","communication_residency_context","interpretation_note"}, "communication_candidate_type, raw_kv_id"},
        {L"iOS - Contact Identity Review", "vw_ios_contact_identity_records", {"ios_app_record_id","source_id","ios_db_id","database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","url","title","file_path","item_identifier","text_snippet","parse_status","provenance","contact_identity_type","identity_value_sample","interpretation_note"}, {"database_normalized_path","database_name","table_name","record_category","contact_or_participant","url","title","file_path","item_identifier","text_snippet","contact_identity_type","identity_value_sample","interpretation_note"}, "contact_identity_type, database_name, table_name, ios_app_record_id"},
        {L"iOS - Contact Identity Summary", "vw_ios_contact_identity_summary", {"database_name","database_normalized_path","table_name","contact_identity_type","parse_status","contact_review_row_count","distinct_item_identifier_count","rows_with_contact_text","rows_with_title","rows_with_text_snippet","earliest_record_timestamp_utc","latest_record_timestamp_utc"}, {"database_name","database_normalized_path","table_name","contact_identity_type","parse_status"}, "contact_review_row_count DESC, database_name, table_name"},
        {L"iOS - Web History Review", "vw_ios_web_history_review_records", {"ios_app_record_id","source_id","ios_db_id","database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","url","title","item_identifier","text_snippet","parse_status","provenance","web_record_type","web_review_value_sample","interpretation_note"}, {"database_normalized_path","database_name","table_name","record_category","url","title","item_identifier","text_snippet","web_record_type","web_review_value_sample","interpretation_note"}, "record_timestamp_utc DESC, database_name, table_name, ios_app_record_id"},
        {L"iOS - Web History Summary", "vw_ios_web_history_review_summary", {"database_category","app_hint","database_name","table_name","web_record_type","parse_status","web_review_row_count","rows_with_url","rows_with_title","earliest_record_timestamp_utc","latest_record_timestamp_utc","first_database_path","last_database_path"}, {"database_category","app_hint","database_name","table_name","web_record_type","parse_status","first_database_path","last_database_path"}, "web_review_row_count DESC, database_name, table_name"},
        {L"iOS - Calendar Review", "vw_ios_calendar_review_records", {"ios_app_record_id","source_id","ios_db_id","database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","url","title","file_path","item_identifier","text_snippet","parse_status","provenance","calendar_record_type","calendar_review_value_sample","interpretation_note"}, {"database_normalized_path","database_name","table_name","record_category","contact_or_participant","url","title","file_path","item_identifier","text_snippet","calendar_record_type","calendar_review_value_sample","interpretation_note"}, "record_timestamp_utc DESC, database_name, table_name, ios_app_record_id"},
        {L"iOS - Calendar Summary", "vw_ios_calendar_review_summary", {"database_name","database_normalized_path","table_name","calendar_record_type","parse_status","calendar_review_row_count","rows_with_title","rows_with_contact_or_account","rows_with_timestamp","earliest_record_timestamp_utc","latest_record_timestamp_utc"}, {"database_name","database_normalized_path","table_name","calendar_record_type","parse_status"}, "calendar_review_row_count DESC, database_name, table_name"},
        {L"iOS - Unified Keyword Search Surface", "vw_ios_investigation_keyword_surface", {"surface_source","source_id","source_record_id","review_category","source_container","source_location","field_or_table","record_timestamp_utc","searchable_value_sample","path_or_url","contact_or_identity","review_priority","residency_context","interpretation_note"}, {"surface_source","source_record_id","review_category","source_container","source_location","field_or_table","record_timestamp_utc","searchable_value_sample","path_or_url","contact_or_identity","review_priority","residency_context","interpretation_note"}, "review_priority, surface_source, record_timestamp_utc DESC, source_record_id"},
        {L"iOS - Apple Messages Parsed Records", "vw_ios_apple_messages_parsed_records", {"ios_app_record_id","source_id","ios_db_id","database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","title","file_path","item_identifier","text_snippet","parse_status","provenance","created_utc"}, {"database_normalized_path","database_name","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","title","file_path","item_identifier","text_snippet","parse_status","provenance"}, "record_timestamp_utc,ios_app_record_id"},
        {L"iOS - Apple Messages Parsed Summary", "vw_ios_apple_messages_parsed_summary", {"record_category","parse_status","parsed_record_count","database_count","earliest_record_timestamp_utc","latest_record_timestamp_utc","records_with_contact_or_handle","records_with_file_path","records_with_text_or_metadata","first_database_path","last_database_path"}, {"record_category","parse_status","first_database_path","last_database_path"}, "record_category,parse_status"},
        {L"iOS - WhatsApp Database Status", "vw_ios_whatsapp_database_status", {"ios_db_id","source_id","normalized_path","database_name","app_hint","parse_status","record_inventory_status","extracted_path","message_rows","media_rows","chat_rows","contact_or_member_rows","call_rows","parsed_whatsapp_rows","relevant_table_counts","whatsapp_residency_status","interpretation_note"}, {"normalized_path","database_name","app_hint","parse_status","record_inventory_status","relevant_table_counts","whatsapp_residency_status","interpretation_note"}, "whatsapp_residency_status, normalized_path"},
        {L"iOS - WhatsApp Parsed Records", "vw_ios_whatsapp_parsed_records", {"ios_app_record_id","source_id","ios_db_id","database_normalized_path","database_name","database_category","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","url","title","file_path","item_identifier","text_snippet","parse_status","provenance","created_utc"}, {"database_normalized_path","database_name","app_hint","table_name","record_category","source_primary_key","record_timestamp_utc","timestamp_source","contact_or_participant","url","title","file_path","item_identifier","text_snippet","parse_status","provenance"}, "record_timestamp_utc,ios_app_record_id"},
        {L"iOS - WhatsApp Parsed Summary", "vw_ios_whatsapp_parsed_summary", {"record_category","parse_status","parsed_record_count","database_count","earliest_record_timestamp_utc","latest_record_timestamp_utc","records_with_contact_or_jid","records_with_media_or_file_path","records_with_text_or_metadata","first_database_path","last_database_path"}, {"record_category","parse_status","first_database_path","last_database_path"}, "record_category,parse_status"},
        {L"iOS - Keychain Material Inventory", "vw_ios_keychain_material_inventory", {"source_id","normalized_path","original_zip_entry","file_name","extension","size_bytes","zip_modified_utc","protection_class_hint","app_container_hint","domain_hint","sha256_status","inventory_notes","keychain_material_type","interpretation_note"}, {"normalized_path","original_zip_entry","file_name","domain_hint","inventory_notes","keychain_material_type","interpretation_note"}, "keychain_material_type, normalized_path"},
        {L"iOS - Keychain Support References", "vw_ios_keychain_support_reference_inventory", {"source_id","normalized_path","original_zip_entry","file_name","extension","size_bytes","zip_modified_utc","protection_class_hint","app_container_hint","domain_hint","sha256_status","inventory_notes","keychain_reference_type","interpretation_note"}, {"normalized_path","original_zip_entry","file_name","domain_hint","inventory_notes","keychain_reference_type","interpretation_note"}, "normalized_path"},
        {L"iOS - Parsed App Record Summary", "vw_ios_app_parsed_record_summary", {"database_category","app_hint","record_category","parse_status","parsed_record_count","database_count","earliest_record_timestamp_utc","latest_record_timestamp_utc","first_database","last_database"}, {"database_category","app_hint","record_category","parse_status","first_database","last_database"}, "database_category,record_category"},
        {L"iOS - Spotlight Human Text Values", "vw_ios_spotlight_human_text_values", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","field_name","human_text_category","original_value_length","readable_text_sample","review_priority","interpretation_note"}, {"store_guid","source_db","inode_num","store_id","field_name","human_text_category","readable_text_sample","review_priority","interpretation_note"}, "review_priority, human_text_category, raw_kv_id"},
        {L"iOS - Spotlight Human Text Rollup", "vw_ios_spotlight_human_text_rollup", {"raw_record_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","text_value_count","distinct_text_category_count","human_text_categories","has_high_review_value_text","last_updated_utc","time_interpretation","readable_text_rollup_sample","interpretation_note"}, {"store_guid","source_db","inode_num","store_id","human_text_categories","last_updated_utc","readable_text_rollup_sample","interpretation_note"}, "has_high_review_value_text DESC, last_updated_utc DESC, raw_record_id"},
        {L"iOS - Database Residency Candidates", "vw_ios_database_residency_candidates", {"candidate_id","source_id","store_guid","source_db","inode_num","store_id","field_name","object_category","database_residency_status","database_name","database_category","app_hint","candidate_database_path","record_inventory_status","matched_record_category","matched_table_name","matched_table_row_count","string_value_sample","interpretation_note"}, {"store_guid","source_db","inode_num","field_name","object_category","database_residency_status","database_name","database_category","app_hint","candidate_database_path","record_inventory_status","matched_record_category","matched_table_name","string_value_sample","interpretation_note"}, "object_category, database_residency_status, candidate_id"},
        {L"iOS - Spotlight Object Identity", "vw_ios_spotlight_object_identity", {"raw_record_id","source_id","store_guid","source_db","protection_class","spotlight_inode_or_object_id","spotlight_parent_id","spotlight_store_id","file_name","display_name","full_path","content_type","last_updated_utc","string_probe_count","sample_url_or_web","sample_path_or_file_ref","sample_email_or_account","sample_decoded_string","identity_basis","interpretation_note"}, {"store_guid","source_db","protection_class","spotlight_inode_or_object_id","spotlight_parent_id","spotlight_store_id","file_name","display_name","full_path","content_type","sample_url_or_web","sample_path_or_file_ref","sample_email_or_account","sample_decoded_string","identity_basis","interpretation_note"}, "protection_class, store_guid, CAST(spotlight_inode_or_object_id AS INTEGER), raw_record_id"},
        {L"iOS - Spotlight to FFS Links", "vw_ios_spotlight_to_ffs_object_links", {"reference_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","spotlight_parent_id","field_name","reference_type","normalized_ios_path","residency_status","confidence","matched_file_name","matched_size_bytes","matched_zip_modified_utc","matched_protection_class","matched_app_container","matched_domain","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","field_name","reference_type","normalized_ios_path","residency_status","confidence","matched_file_name","matched_protection_class","matched_app_container","matched_domain","interpretation_note"}, "residency_status, confidence, normalized_ios_path, reference_id"},
        {L"iOS - Spotlight to App DB Links", "vw_ios_spotlight_to_app_db_record_links", {"candidate_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","spotlight_parent_id","field_name","object_category","database_residency_status","database_name","database_category","app_hint","candidate_database_path","record_inventory_status","matched_record_category","matched_table_name","matched_table_row_count","parsed_record_count","earliest_record_timestamp_utc","latest_record_timestamp_utc","sample_database_path","sample_table_name","sample_record_category","sample_parsed_value","string_value_sample","app_db_link_status","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","field_name","object_category","database_residency_status","database_name","database_category","app_hint","candidate_database_path","matched_record_category","matched_table_name","sample_parsed_value","string_value_sample","app_db_link_status","interpretation_note"}, "app_db_link_status, database_category, candidate_id"},
        {L"Artifacts", "artifacts", {"file_name","best_path","last_used_date_utc","first_used_candidate_utc","last_updated_utc","downloaded_date_utc","artifact_id","store_guid","inode_num","parent_inode_num","display_name","spotlight_display_path","normalized_mac_path","path_status","path_source","is_mounted_volume_path","mounted_volume_name","external_volume_reason","content_type","used_dates_count","use_count_value","usage_field_summary","existence_status","open_count_estimate"}, {"store_guid","inode_num","parent_inode_num","file_name","display_name","best_path","spotlight_display_path","normalized_mac_path","path_status","mounted_volume_name","external_volume_reason","content_type","existence_status","usage_field_summary"}, "artifact_id"},
        {L"Timeline", "timeline_events", {"file_name","path","event_timestamp_utc","event_type","event_source_field","artifact_id","timeline_id","store_guid","inode_num","existence_status","deleted_or_orphaned_candidate"}, {"event_timestamp_utc","event_type","event_source_field","store_guid","inode_num","file_name","path","existence_status"}, "timeline_id"},
        {L"Raw Date Candidates", "raw_date_candidates", {"raw_date_id","source_id","store_guid","source_db","inode_num","store_id","field_name","field_value","parsed_utc","parse_method"}, {"source_id","store_guid","source_db","inode_num","store_id","field_name","field_value","parsed_utc","parse_method"}, "raw_date_id"},
        {L"Parent Inode Links", "parent_inode_links", {"link_id","source_id","store_guid","child_inode_num","child_parent_inode_num","child_file_name","child_best_path","parent_inode_num","parent_file_name","parent_best_path","sibling_count","relationship_status","path_reconstruction_method","reconstructed_path_candidate","confidence"}, {"source_id","store_guid","child_inode_num","child_parent_inode_num","child_file_name","parent_file_name","parent_best_path","relationship_status","path_reconstruction_method","reconstructed_path_candidate","confidence"}, "link_id"},
        {L"Usage Evidence", "usage_evidence", {"artifact_id","parsed_utc","field_name","field_value","usage_id","source_id","store_guid","inode_num"}, {"store_guid","inode_num","field_name","field_value","parsed_utc"}, "usage_id"},
        {L"Orphaned / Deleted Candidates", "orphaned_deleted_candidates", {"file_name","best_path","existence_status","orphan_reason","artifact_id","candidate_id","store_guid","inode_num","content_type","index_text_snippet"}, {"store_guid","inode_num","file_name","best_path","content_type","existence_status","orphan_reason","index_text_snippet"}, "candidate_id"},
        {L"External / Mounted Volume Candidates", "external_volume_candidates", {"file_name","best_path","mounted_volume_name","reason","artifact_id","candidate_id","source_id","store_guid","inode_num","confidence","detection_source_field","detection_source_value"}, {"source_id","store_guid","inode_num","file_name","best_path","mounted_volume_name","reason","confidence","detection_source_field","detection_source_value"}, "candidate_id"},
        {L"Source Copy Comparison", "source_copy_comparison", {"comparison_id","source_id","store_guid","inode_num","source_instance_count","has_store_db","has_dotstore_db","comparison_status","preferred_source_db"}, {"source_id","store_guid","inode_num","comparison_status","preferred_source_db"}, "comparison_id"},
        {L"Artifact Source Instances", "artifact_source_instances", {"instance_id","artifact_id","raw_record_id","source_id","store_guid","inode_num","source_db_role","source_db","last_updated_utc","file_name","best_path"}, {"source_id","store_guid","inode_num","source_db_role","source_db","file_name","best_path"}, "instance_id"},
        {L"Field Inventory", "field_inventory", {"field_inventory_id","source_id","field_name","row_count","populated_count","sample_value"}, {"source_id","field_name","sample_value"}, "field_inventory_id"},
        {L"Parser Coverage", "parser_coverage_summary", {"summary_id","source_id","metric_name","metric_value","created_utc"}, {"source_id","metric_name","metric_value","created_utc"}, "summary_id"},
        {L"Source Probe Inventory", "vw_source_probe_inventory", {"created_utc","source_id","input_type","source_kind","parse_status","container_reader_status","filesystem_reader_status","spotlight_discovery_status","partition_scheme","partition_entry_count","probe_signature_count","filesystem_hints","spotlight_hints","valid_database_candidates","valid_store_groups","input_path","working_root","next_action","notes"}, {"source_id","input_type","source_kind","parse_status","container_reader_status","filesystem_reader_status","spotlight_discovery_status","partition_scheme","filesystem_hints","spotlight_hints","input_path","next_action","notes"}, "created_utc DESC, source_probe_run_id DESC"},
        {L"Source Probe Signatures", "vw_source_probe_signatures", {"created_utc","source_id","input_type","source_kind","signature_name","category","offset_bytes","confidence","notes"}, {"source_id","input_type","source_kind","signature_name","category","confidence","notes"}, "source_id, offset_bytes, signature_name"},
        {L"Source Partition Probe", "vw_source_partition_inventory", {"source_id","input_type","source_kind","scheme","partition_index","start_lba","sector_count","offset_bytes","size_bytes","type_code","type_guid","name","filesystem_hint","confidence","status","notes"}, {"source_id","input_type","source_kind","scheme","type_code","type_guid","name","filesystem_hint","confidence","status","notes"}, "source_id, scheme, partition_index, offset_bytes"},
        {L"Image Inventory Sources", "vw_image_inventory_sources", {"created_utc","source_id","input_type","container_type","active_comparison_status","container_reader_status","partition_reader_status","filesystem_reader_status","spotlight_locator_status","partition_scheme","partition_count","apfs_hint_count","spotlight_hint_count","inventory_file_count","comparison_ready","input_path","next_action","notes"}, {"source_id","input_type","container_type","active_comparison_status","container_reader_status","partition_reader_status","filesystem_reader_status","spotlight_locator_status","partition_scheme","input_path","next_action","notes"}, "created_utc DESC, image_inventory_source_id DESC"},
        {L"Image File Inventory", "vw_image_file_inventory", {"file_name","full_path","filesystem_type","apfs_volume_name","filesystem_object_id","parent_filesystem_object_id","inode_num","parent_inode_num","logical_size_bytes","created_utc","modified_utc","accessed_utc","changed_utc","container_type","aff4_stream_id","aff4_stream_name","partition_index","partition_offset_bytes","source_confidence","extraction_status","provenance","image_file_id","source_id"}, {"file_name","full_path","filesystem_type","apfs_volume_name","filesystem_object_id","parent_filesystem_object_id","inode_num","parent_inode_num","container_type","aff4_stream_id","extraction_status","provenance"}, "source_id, apfs_volume_name, full_path, image_file_id"},
        {L"Active File Comparison Readiness", "vw_active_file_comparison_readiness", {"created_utc","source_id","input_type","container_type","active_comparison_status","comparison_ready","spotlight_artifact_count","image_inventory_rows","inventory_file_count","comparison_candidate_count","container_reader_status","partition_reader_status","filesystem_reader_status","spotlight_locator_status","next_action","notes"}, {"source_id","input_type","container_type","active_comparison_status","container_reader_status","partition_reader_status","filesystem_reader_status","spotlight_locator_status","next_action","notes"}, "created_utc DESC, image_inventory_source_id DESC"},
        {L"Spotlight vs Image Active File Comparison", "vw_spotlight_active_file_comparison", {"file_name","best_path","active_file_comparison_status","match_basis","matched_image_path","matched_image_file_name","matched_filesystem_type","matched_apfs_volume_name","matched_filesystem_object_id","matched_logical_size_bytes","artifact_id","source_id","store_guid","inode_num","parent_inode_num","image_inventory_rows","image_file_id","comparison_notes"}, {"file_name","best_path","active_file_comparison_status","match_basis","matched_image_path","matched_image_file_name","matched_filesystem_type","matched_apfs_volume_name","matched_filesystem_object_id","source_id","store_guid","inode_num","parent_inode_num","comparison_notes"}, "artifact_id"},
        {L"Preservation Sets", "preserved_evidence_sets", {"preservation_id","source_id","archive_path","archive_sha256","archive_size_bytes","original_root_path","preserved_root_path","file_count","total_original_bytes","preservation_status","integrity_status","notes"}, {"source_id","archive_path","archive_sha256","original_root_path","preserved_root_path","preservation_status","integrity_status","notes"}, "preservation_id"},
        {L"Store Groups", "store_groups", {"source_id","store_guid","store_path","store_file_count","has_store_db","has_dotstore_db"}, {"source_id","store_guid","store_path"}, "store_guid"},
        {L"Raw Records", "raw_records", {"raw_record_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","flags","last_updated_utc","file_name","display_name","full_path","record_state"}, {"source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","file_name","display_name","full_path","record_state"}, "raw_record_id"},
        {L"Processing Log", "processing_log", {"log_id","created_utc","level","message"}, {"created_utc","level","message"}, "log_id"}
    };
    return v;
}

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

class ReadOnlyDb {
public:
    explicit ReadOnlyDb(const std::wstring& path) {
        const std::string p = narrow(path);
        if (sqlite3_open_v2(p.c_str(), &db_, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK) {
            sqlite3_busy_timeout(db_, 30000);
            sqlite3_exec(db_, "PRAGMA temp_store=MEMORY; PRAGMA cache_size=-65536;", nullptr, nullptr, nullptr);
            ensureInvestigatorUiSchemaNoThrow(db_);
            return;
        }
        if (db_) { sqlite3_close_v2(db_); db_ = nullptr; }
        if (sqlite3_open_v2(p.c_str(), &db_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            std::string msg = db_ ? sqlite3_errmsg(db_) : "unknown";
            if (db_) sqlite3_close_v2(db_);
            db_ = nullptr;
            throw std::runtime_error("Unable to open case database: " + msg);
        }
        sqlite3_busy_timeout(db_, 30000);
        sqlite3_exec(db_, "PRAGMA temp_store=MEMORY; PRAGMA cache_size=-65536;", nullptr, nullptr, nullptr);
    }
    ~ReadOnlyDb() { if (db_) sqlite3_close_v2(db_); }
    sqlite3* get() const { return db_; }
private:
    sqlite3* db_ = nullptr;
};

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

bool startsWithNoCase(const wchar_t* text, const wchar_t* prefix) {
    if (!text || !prefix) return false;
    while (*prefix) {
        if (!*text) return false;
        if (std::towlower(*text) != std::towlower(*prefix)) return false;
        ++text;
        ++prefix;
    }
    return true;
}

bool isIosReviewView(const ViewSpec& v) {
    return startsWithNoCase(v.displayName, L"iOS ") ||
           startsWithNoCase(v.displayName, L"iOS -") ||
           startsWithNoCase(v.displayName, L"iOS Readiness");
}

bool isIosOnlySharedReviewView(const ViewSpec& v) {
    // This view contains platform-mixed keyword rows. Keep it out of the MacOS tab.
    return wcscmp(v.displayName, L"Investigator - Keyword Search Values") == 0;
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
    const std::wstring name = v.displayName ? v.displayName : L"";
    // V0_9_57: expose all existing iOS views, but keep the most useful V1
    // investigative surfaces at the top so they are easy to find in mature cases.
    static const wchar_t* kV1PriorityNames[] = {
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
        return containsAnyW(n, {L"timeline", L"date", L"time", L"usage", L"used", L"knowledgec", L"interaction", L"anomal"});
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
    // V0_9_57: the iOS tab is now mature enough to show every useful iOS review
    // surface that exists in the case DB. The macOS tab remains isolated from iOS
    // and shared iOS-only views.
    if (gIosReviewMode) return isIosReviewView(v) || isIosOnlySharedReviewView(v);
    return !isIosReviewView(v) && !isIosOnlySharedReviewView(v);
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

std::string sqlColumns(const ViewSpec& v) {
    std::string out;
    for (size_t i = 0; i < v.columns.size(); ++i) {
        if (i) out += ",";
        out += v.columns[i];
    }
    return out;
}
std::string buildWhere(const ViewSpec& v, const std::string& search) {
    std::vector<std::string> clauses;
    if (!search.empty() && !v.searchColumns.empty()) {
        std::string q = "(";
        for (size_t i = 0; i < v.searchColumns.size(); ++i) {
            if (i) q += " OR ";
            q += "COALESCE(CAST(";
            q += v.searchColumns[i];
            q += " AS TEXT),'') LIKE ?";
        }
        q += ")";
        clauses.push_back(q);
    }
    if (gFilterColumn >= 0 && gFilterColumn < static_cast<int>(v.columns.size()) && !gFilterValue.empty()) {
        std::string q = "COALESCE(CAST(";
        q += v.columns[static_cast<size_t>(gFilterColumn)];
        q += " AS TEXT),'') LIKE ?";
        clauses.push_back(q);
    }
    if (clauses.empty()) return "";
    std::string where = " WHERE ";
    for (size_t i = 0; i < clauses.size(); ++i) {
        if (i) where += " AND ";
        where += clauses[i];
    }
    return where;
}
void bindSearch(sqlite3_stmt* st, const ViewSpec& v, const std::string& search, int& index) {
    if (!search.empty()) {
        std::string pattern = "%" + search + "%";
        for (size_t i = 0; i < v.searchColumns.size(); ++i) sqlite3_bind_text(st, index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (gFilterColumn >= 0 && gFilterColumn < static_cast<int>(v.columns.size()) && !gFilterValue.empty()) {
        std::string pattern = "%" + gFilterValue + "%";
        sqlite3_bind_text(st, index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    }
}
std::string checkedIdListSql() {
    if (gCheckedArtifactIds.empty()) return "";
    std::string ids;
    for (long long id : gCheckedArtifactIds) {
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
    ListView_DeleteAllItems(gList);
    while (ListView_DeleteColumn(gList, 0)) {}
}
void setReviewSummary(const std::wstring& s) { SetWindowTextW(gReviewSummary, s.c_str()); }


std::wstring viewHelpText(const ViewSpec& v) {
    const std::wstring name = v.displayName ? v.displayName : L"";
    const std::string table = v.tableName ? v.tableName : "";
    if (name.find(L"iOS - Parsed App Records") == 0) return L"Parsed rows extracted from staged iOS app SQLite databases such as Calendar, Contacts, CallHistory, Safari/WebKit, and Messages. These are app database records, not Spotlight records.";
    if (name.find(L"iOS - Parsed App Record Summary") == 0) return L"Counts and date coverage for parsed iOS app database rows grouped by database family, app hint, record category, and parse status.";
    if (name.find(L"iOS - Apple Messages Database Status") == 0) return L"SMS.db/iMessage database presence and table counts. Shows whether live message/chat/attachment rows exist and warns when Spotlight message-like text has no matching live Messages row.";
    if (name.find(L"iOS - Apple Messages Parsed Records") == 0) return L"Rows parsed from SMS.db/iMessage tables. V0_9_6 joins message, handle, chat, and attachment tables where present; empty live tables still mean no live message rows were parsed.";
    if (name.find(L"iOS - Communications Review Records") == 0) return L"Unified communications review rows from parsed iOS app databases. Includes Messages/SMS, WhatsApp when present, Phone/FaceTime call history, and other message/chat/call-like records with timestamp provenance.";
    if (name.find(L"iOS - Communications Review Summary") == 0) return L"Grouped counts for parsed iOS communications records by source, category, record type, and timestamp coverage.";
    if (name.find(L"iOS - Spotlight Communication Candidates") == 0) return L"CoreSpotlight string probes that look communication-related. These are Spotlight candidates only until linked to a parsed app database row or file path.";
    if (name.find(L"iOS - Apple Messages Parsed Summary") == 0) return L"Summary of parsed SMS.db/iMessage message, attachment, participant, and recoverable/deleted candidate categories.";
    if (name.find(L"iOS - Contact Identity Review") == 0) return L"Contact/address-book review rows from AddressBook and contact cache databases. Cache and FTS rows may duplicate or tokenize contact values; use table and provenance before reporting.";
    if (name.find(L"iOS - Contact Identity Summary") == 0) return L"Grouped counts for contact identity review rows by database and table, with counts for contact text, titles, snippets, identifiers, and timestamp coverage.";
    if (name.find(L"iOS - Web History Review") == 0) return L"Parsed Safari/WebKit/Chrome-style web rows, including history, bookmarks, titles, URLs, and timestamp provenance where available.";
    if (name.find(L"iOS - Web History Summary") == 0) return L"Grouped counts for parsed web/browser rows by database, table, and record type.";
    if (name.find(L"iOS - Calendar Review") == 0) return L"Parsed Calendar database rows with event/support, invitee/account, attachment, and location context. Use table/provenance to separate events from support rows.";
    if (name.find(L"iOS - Calendar Summary") == 0) return L"Grouped counts for parsed calendar rows by database, table, record type, and timestamp coverage.";
    if (name.find(L"iOS - Unified Keyword Search Surface") == 0) return L"Unified iOS keyword-review surface combining Spotlight text, parsed app records, high-value FFS paths, and app database inventory. Use the search box to pivot names, domains, phone numbers, paths, apps, or keywords.";
    if (name.find(L"iOS - WhatsApp Database Status") == 0) return L"WhatsApp iOS database presence, table counts, and parsing status. Uses uploaded iLEAPP/WhatsApp-reference schema patterns; encrypted or absent databases remain inventory-only.";
    if (name.find(L"iOS - WhatsApp Parsed Records") == 0) return L"Rows parsed from iOS WhatsApp ChatStorage/Contacts/CallHistory SQLite tables where present. Includes message, chat/contact, media, and call fields with timestamp provenance.";
    if (name.find(L"iOS - WhatsApp Parsed Summary") == 0) return L"Summary counts and date coverage for parsed WhatsApp messages, media, chats, contacts, and calls.";
    if (name.find(L"iOS - Keychain Material Inventory") == 0) return L"Inventory of keychain/keybag-related files found in the iOS FFS ZIP root. This is presence/context only and does not claim decryption capability.";
    if (name.find(L"iOS - Keychain Support References") == 0) return L"Lower-priority paths that contain keychain in app framework/code names outside core keychain/keybag locations. Usually not credential material.";
    if (name.find(L"iOS - App Live Activity Timeline") == 0) return L"Timeline from parsed iOS app database records. These are app-record timestamps; they are separate from CoreSpotlight Last_Updated index timing.";
    if (name.find(L"iOS - FFS File Inventory") == 0) return L"Full-file-system ZIP path inventory used for file-presence correlation. Presence here means the path was found in the acquisition ZIP; absence is not proof of deletion.";
    if (name.find(L"iOS - App Database Inventory") == 0) return L"iOS SQLite/store database candidates found in the FFS ZIP, with family classification and extraction/record-inventory status.";
    if (name.find(L"iOS - App Database Record Inventory") == 0) return L"Per-table row counts and schema samples from extracted iOS app databases. Use this to determine whether a database has live rows before interpreting parsed records.";
    if (name.find(L"iOS - Spotlight to FFS Object Links") == 0) return L"Conservative correlation between Spotlight path references and FFS inventory paths. Matching methods and confidence should be reviewed before reporting.";
    if (name.find(L"iOS - Spotlight to App DB Record Links") == 0) return L"Conservative correlation between Spotlight values/path fragments and parsed app database rows. Use as a lead-generation view, not automatic proof.";
    if (name.find(L"iOS - High-Value Missing From FFS") == 0) return L"Prioritized Spotlight path references not matched to the available FFS lookup, excluding likely app thumbnail/brand/cache-only references. Includes same-record Spotlight text context so investigators can assess likely value first.";
    if (name.find(L"iOS - Missing From FFS Candidates") == 0) return L"All Spotlight path references not matched to FFS inventory or the slim path lookup. Includes priority/category, lookup source, and compact text context recovered from the same Spotlight record where available. Treat as missing/unresolved candidates only; acquisition scope, protection, cloud/offload, cache cleanup, and parser limitations remain possible.";
    if (name.find(L"iOS - High-Value Missing From FFS Summary") == 0) return L"Compact high/medium-priority rollup of Missing From FFS candidates. Start here before opening the full candidate list because thumbnail/brand/cache references are excluded.";
    if (name.find(L"iOS - Missing From FFS Summary") == 0) return L"Compact rollup of all Missing From FFS candidates with priority/category and same-record Spotlight text context samples. Low-priority app thumbnail/cache rows may provide app context but usually have lower deleted-item value.";
    if (name.find(L"iOS - Residency Summary") == 0) return L"Rollup of iOS Spotlight-to-FFS and Spotlight-to-app-database residency classifications.";
    if (name.find(L"iOS - Database Residency Candidates") == 0) return L"Database-family leads derived from Spotlight strings and app database inventory. Useful for finding where app-resident records may need deeper parsing.";
    if (name.find(L"iOS - Spotlight Investigative Items With Dates") == 0) return L"Recovered CoreSpotlight text/probe items with directly linked Spotlight date provenance. Date fields include the raw Spotlight source field, raw value, parse method, semantic caution, and validation hint.";
    if (name.find(L"iOS - Spotlight Entity Review") == 0) return L"Normalized Spotlight/CoreSpotlight entity review. Groups recovered Spotlight text into URL/web, file/attachment, account/contact, communication, and other entity types with raw value/date validation locators and supporting FFS/app context.";
    if (name.find(L"iOS - Spotlight Entity Summary") == 0) return L"Grouped counts for normalized Spotlight entities by entity type, source store, source field, and date class. This is a Spotlight-first summary; FFS/app counts are corroborating context.";
    if (name.find(L"iOS - Spotlight Native Parser Targets") == 0) return L"Prioritized native CoreSpotlight parser target list. Shows stores with header-level records lacking recovered text and generic string probe fields that need property-name mapping.";
    if (name.find(L"iOS - Spotlight dbStr Map Inventory") == 0) return L"Shows whether iOS CoreSpotlight dbStr map files were found and parsed for properties, categories, and index maps. Missing dbStr components usually explain generic probe-only decoding.";
    if (name.find(L"iOS - Spotlight Dictionary Coverage") == 0) return L"Per-store dictionary coverage: raw records, property/category counts, index-map counts, and dbStr parser status. Use this to validate whether CoreSpotlight field-name decoding improved.";
    if (name.find(L"iOS - Spotlight Apple Field Coverage") == 0) return L"Recovered CoreSpotlight field names grouped against Apple public Core Spotlight/Spotlight metadata semantics where names match. Generic probe fields remain parser targets.";
    if (name.find(L"iOS - Spotlight Human Text Values") == 0) return L"Human-readable CoreSpotlight string/probe values categorized for review. These are decoded/indexed values, not necessarily live app records.";
    if (name.find(L"iOS - Spotlight Human Text Rollup") == 0) return L"Per-Spotlight-record rollup of readable text values and categories for faster triage.";
    if (name.find(L"iOS - String Probe Values") == 0) return L"Bounded raw string-probe values recovered from iOS CoreSpotlight Store-V2 records. Use field/source columns for provenance.";
    if (name.find(L"iOS - Record String Probe Summary") == 0) return L"Per-record summary of iOS string-probe categories and sample text. Helps identify communications, URLs, app/document hints, and review priority.";
    if (name.find(L"iOS - Index Timeline") == 0 || name.find(L"iOS - Timeline") == 0) return L"CoreSpotlight Last_Updated index/update timestamps. These are index metadata timestamps unless another app/file timestamp supports activity interpretation.";
    if (name.find(L"iOS - Store Parse Summary") == 0) return L"Per-iOS CoreSpotlight store parse counts, selected store role, protection-class context, and parser coverage.";
    if (name.find(L"iOS - Protection Class Summary") == 0) return L"CoreSpotlight store/record counts grouped by protection-class hints from source paths.";
    if (name.find(L"iOS - Artifact Hint Summary") == 0) return L"Counts of inferred iOS artifact categories from decoded/probed text such as URLs, names, cloud/document indicators, and message-like values.";
    if (name.find(L"iOS - Record Investigation Hints") == 0) return L"Per-record iOS investigation hint rollup that summarizes artifact categories, protection class, and review priorities.";
    if (name.find(L"iOS Readiness") == 0) return L"iOS intake/readiness view showing whether source routing, focused ZIP extraction, Store-V2 selection, and high-value field discovery succeeded.";
    if (name.find(L"Investigator - Usage") == 0) return L"macOS Spotlight usage/date views. Review source_field and usage_reason before treating an event as user activity.";
    if (name.find(L"Investigator - All Artifact Dates") == 0 || name.find(L"Investigator - Object Date") == 0 || name.find(L"Investigator - Date Attribution") == 0) return L"macOS date attribution views. They expose raw Spotlight field names, interpreted date type, snapshot/index warnings, and association confidence.";
    if (name.find(L"Investigator - Snapshot Date Warnings") == 0) return L"Rows where dates look like index/export/snapshot timing rather than meaningful file/user activity.";
    if (name.find(L"Investigator - Path Reconstruction") == 0 || name.find(L"Parent Inode") != std::wstring::npos || name.find(L"Same Folder") != std::wstring::npos) return L"Parent-inode/path reconstruction context. Confidence and method columns explain whether paths are directly present, inferred, or unresolved.";
    if (name.find(L"Investigator - WhereFroms") == 0) return L"Download/source URL and WhereFroms-related Spotlight metadata where available.";
    if (name.find(L"Image") != std::wstring::npos || name.find(L"Active File Comparison") != std::wstring::npos) return L"Image/container inventory and active-file comparison readiness. These views support later AFF4/APFS/raw-image comparison workflows.";
    if (name.find(L"Raw Records") == 0) return L"Raw parsed Spotlight/CoreSpotlight records before investigator enrichment. Use for parser validation and provenance checks.";
    if (name.find(L"Raw Date") == 0) return L"Raw date candidates extracted before date attribution. Use for troubleshooting date parsing and source-field interpretation.";
    if (name.find(L"Processing Log") == 0) return L"Case processing log entries written during source probing, staging, parsing, enrichment, and export.";
    return L"Review view backed by SQLite object " + widen(table) + L". Search/filter/export is available; inspect column names for source and interpretation limits.";
}

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

int viewColumnIndex(const ViewSpec& v, const char* columnName) {
    for (size_t i = 0; i < v.columns.size(); ++i) {
        if (std::string(v.columns[i]) == columnName) return static_cast<int>(i);
    }
    return -1;
}
std::string stmtText(sqlite3_stmt* st, int col) {
    if (col < 0 || col >= sqlite3_column_count(st)) return "";
    const unsigned char* raw = sqlite3_column_text(st, col);
    return raw ? reinterpret_cast<const char*>(raw) : "";
}
std::string resolveArtifactIdForVisibleRow(sqlite3* db, const ViewSpec& v, sqlite3_stmt* st) {
    int artifactCol = viewColumnIndex(v, "artifact_id");
    if (artifactCol >= 0) {
        std::string id = stmtText(st, artifactCol);
        if (!id.empty()) return id;
    }
    int storeCol = viewColumnIndex(v, "store_guid");
    int inodeCol = viewColumnIndex(v, "inode_num");
    if (inodeCol < 0) inodeCol = viewColumnIndex(v, "child_inode_num");
    if (inodeCol < 0) return "";
    const std::string storeGuid = storeCol >= 0 ? stmtText(st, storeCol) : "";
    const std::string inode = stmtText(st, inodeCol);
    if (inode.empty()) return "";
    sqlite3_stmt* q = nullptr;
    const char* sqlWithStore = "SELECT artifact_id FROM artifacts WHERE COALESCE(store_guid,'')=? AND CAST(inode_num AS TEXT)=? ORDER BY artifact_id LIMIT 1";
    const char* sqlNoStore = "SELECT artifact_id FROM artifacts WHERE CAST(inode_num AS TEXT)=? ORDER BY artifact_id LIMIT 1";
    const char* sql = storeGuid.empty() ? sqlNoStore : sqlWithStore;
    if (sqlite3_prepare_v2(db, sql, -1, &q, nullptr) != SQLITE_OK) return "";
    int b = 1;
    if (!storeGuid.empty()) sqlite3_bind_text(q, b++, storeGuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(q, b++, inode.c_str(), -1, SQLITE_TRANSIENT);
    std::string id;
    if (sqlite3_step(q) == SQLITE_ROW) id = stmtText(q, 0);
    sqlite3_finalize(q);
    return id;
}
std::wstring tagsForArtifact(sqlite3* db, const std::string& artifactId) {
    if (artifactId.empty()) return L"";
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT GROUP_CONCAT(tag_name, '; ') FROM ("
        "SELECT it.tag_name FROM artifact_tags at "
        "JOIN investigator_tags it ON it.tag_id=at.tag_id "
        "WHERE at.artifact_id=? ORDER BY lower(it.tag_name))";
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) return L"";
    sqlite3_bind_int64(st, 1, std::strtoll(artifactId.c_str(), nullptr, 10));
    std::wstring out;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* raw = sqlite3_column_text(st, 0);
        if (raw) out = widen(reinterpret_cast<const char*>(raw));
    }
    sqlite3_finalize(st);
    return out;
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
    std::wstring mark = checkMarkForState(checked);
    ListView_SetItemText(gList, row, 0, const_cast<LPWSTR>(mark.c_str()));
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
    for (int row = 0; row < static_cast<int>(gCurrentRowArtifactIds.size()); ++row) {
        std::wstring tags = tagsForArtifact(db, gCurrentRowArtifactIds[static_cast<size_t>(row)]);
        ListView_SetItemText(gList, row, 1, const_cast<LPWSTR>(tags.c_str()));
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
        if (gCaseAutosaveStatus) setText(gCaseAutosaveStatus, L"Autosave failed");
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

void setReviewLoadingState(bool loading) {
    gReviewLoadInProgress = loading;
    if (gRefresh) EnableWindow(gRefresh, loading ? FALSE : TRUE);
    if (gCancelLoad) EnableWindow(gCancelLoad, loading ? TRUE : FALSE);
    if (gPrev) EnableWindow(gPrev, (!loading && gCurrentPage > 0) ? TRUE : FALSE);
    if (gNext) EnableWindow(gNext, (!loading && gCurrentHasNext) ? TRUE : FALSE);
    if (gExportPage) EnableWindow(gExportPage, loading ? FALSE : TRUE);
    if (gExportFiltered) EnableWindow(gExportFiltered, loading ? FALSE : TRUE);
    if (gExportChecked) EnableWindow(gExportChecked, loading ? FALSE : TRUE);
    if (gReviewBusy) {
        ShowWindow(gReviewBusy, loading ? SW_SHOW : SW_HIDE);
        SendMessageW(gReviewBusy, PBM_SETMARQUEE, loading ? TRUE : FALSE, loading ? 35 : 0);
    }
    if (loading) SetCursor(LoadCursorW(nullptr, IDC_WAIT));
}

void updateRowDetailsPanel();
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

    for (int row = 0; row < static_cast<int>(result.rows.size()); ++row) {
        const std::string artifactId = row < static_cast<int>(result.artifactIds.size()) ? result.artifactIds[static_cast<size_t>(row)] : std::string();
        const bool checked = !artifactId.empty() && gCheckedArtifactIds.find(std::strtoll(artifactId.c_str(), nullptr, 10)) != gCheckedArtifactIds.end();
        std::wstring mark = checkMarkForState(checked);
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = const_cast<LPWSTR>(mark.c_str());
        ListView_InsertItem(gList, &item);
        auto tagIt = result.visibleTags.find(artifactId);
        const std::wstring tagText = tagIt == result.visibleTags.end() ? L"" : tagIt->second;
        ListView_SetItemText(gList, row, 1, const_cast<LPWSTR>(tagText.c_str()));
        const auto& cells = result.rows[static_cast<size_t>(row)];
        for (int c = 0; c < static_cast<int>(cells.size()); ++c) {
            std::wstring cell = widen(cells[static_cast<size_t>(c)]);
            ListView_SetItemText(gList, row, c + 2, const_cast<LPWSTR>(cell.c_str()));
        }
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
            updateRowDetailsPanel();
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
    const std::string where = buildWhere(v, search);
    const std::string orderBy = reviewOrderByForPage(v, search, (capturedFilterColumn >= 0 && !capturedFilterValue.empty()));
    const std::string sql = "SELECT " + sqlColumns(v) + " FROM " + v.tableName + where + " ORDER BY " + orderBy + " LIMIT ? OFFSET ?";
    const size_t checkedCountAtRequest = gCheckedArtifactIds.size();

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
    std::wstring out = saveCsv(owner);
    if (out.empty()) return;
    try {
        const ULONGLONG startedMs = GetTickCount64();
        const auto& v = views()[static_cast<size_t>(selectedViewIndex())];
        const std::string search = narrow(getText(gSearch));
        const int ps = pageSize();
        ReadOnlyDb db(gOpenedCaseDb);
        const std::string where = buildWhere(v, search);
        std::string sql = "SELECT " + sqlColumns(v) + " FROM " + v.tableName + where + " ORDER BY " + reviewOrderByForPage(v, search, (gFilterColumn >= 0 && !gFilterValue.empty())) + " LIMIT ? OFFSET ?";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        int bind = 1;
        bindSearch(st, v, search, bind);
        sqlite3_bind_int(st, bind++, ps);
        sqlite3_bind_int64(st, bind++, static_cast<sqlite3_int64>(gCurrentPage) * ps);
        std::ofstream f(narrow(out), std::ios::binary);
        f << "\xEF\xBB\xBF";
        f << csvEscape("checked") << ',' << csvEscape("tags");
        for (size_t c = 0; c < v.columns.size(); ++c) {
            f << ',' << csvEscape(v.columns[c]);
        }
        f << "\n";
        while (true) {
            int rc = sqlite3_step(st);
            if (rc == SQLITE_DONE) break;
            if (rc != SQLITE_ROW) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
            const std::string artifactId = resolveArtifactIdForVisibleRow(db.get(), v, st);
            const bool checked = !artifactId.empty() && gCheckedArtifactIds.find(std::strtoll(artifactId.c_str(), nullptr, 10)) != gCheckedArtifactIds.end();
            f << csvEscape(checked ? "1" : "0") << ',' << csvEscape(narrow(tagsForArtifact(db.get(), artifactId)));
            for (int c = 0; c < sqlite3_column_count(st); ++c) {
                f << ',';
                const unsigned char* raw = sqlite3_column_text(st, c);
                f << csvEscape(raw ? reinterpret_cast<const char*>(raw) : "");
            }
            f << "\n";
        }
        sqlite3_finalize(st);
        const ULONGLONG elapsedMs = GetTickCount64() - startedMs;
        setReviewSummary(L"Exported current page in " + std::to_wstring(elapsedMs) + L" ms to: " + out);
    } catch (const std::exception& ex) {
        setReviewSummary(L"ERROR exporting current page: " + widen(ex.what()));
    }
}


void exportFilteredView(HWND owner) {
    if (gOpenedCaseDb.empty()) { setReviewSummary(L"Open a case database before exporting filtered view."); return; }
    const std::wstring searchText = getText(gSearch);
    if (searchText.empty()) {
        int answer = MessageBoxW(owner,
            L"This will export the entire selected view because no search/filter text is active. Large views may produce very large CSV files. Continue?",
            L"Vestigant Spotlight - Export Filtered View",
            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
        if (answer != IDYES) return;
    }
    std::wstring out = saveCsv(owner);
    if (out.empty()) return;
    try {
        const ULONGLONG startedMs = GetTickCount64();
        const auto& v = views()[static_cast<size_t>(selectedViewIndex())];
        const std::string search = narrow(searchText);
        ReadOnlyDb db(gOpenedCaseDb);
        const std::string where = buildWhere(v, search);
        std::string sql = "SELECT " + sqlColumns(v) + " FROM " + v.tableName + where + " ORDER BY " + reviewOrderBy(v);
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        int bind = 1;
        bindSearch(st, v, search, bind);
        std::ofstream f(narrow(out), std::ios::binary);
        f << "\xEF\xBB\xBF";
        f << csvEscape("checked") << ',' << csvEscape("tags");
        for (size_t c = 0; c < v.columns.size(); ++c) f << ',' << csvEscape(v.columns[c]);
        f << "\n";
        unsigned long long rows = 0;
        while (true) {
            int rc = sqlite3_step(st);
            if (rc == SQLITE_DONE) break;
            if (rc != SQLITE_ROW) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
            const std::string artifactId = resolveArtifactIdForVisibleRow(db.get(), v, st);
            const bool checked = !artifactId.empty() && gCheckedArtifactIds.find(std::strtoll(artifactId.c_str(), nullptr, 10)) != gCheckedArtifactIds.end();
            f << csvEscape(checked ? "1" : "0") << ',' << csvEscape(narrow(tagsForArtifact(db.get(), artifactId)));
            for (int c = 0; c < sqlite3_column_count(st); ++c) {
                f << ',';
                const unsigned char* raw = sqlite3_column_text(st, c);
                f << csvEscape(raw ? reinterpret_cast<const char*>(raw) : "");
            }
            f << "\n";
            ++rows;
            if ((rows % 5000ULL) == 0ULL) {
                setReviewSummary(L"Export Filtered in progress: " + std::to_wstring(rows) + L" rows written...");
                UpdateWindow(gReviewSummary);
            }
        }
        sqlite3_finalize(st);
        const ULONGLONG elapsedMs = GetTickCount64() - startedMs;
        setReviewSummary(L"Exported filtered view rows=" + std::to_wstring(rows) + L" in " + std::to_wstring(elapsedMs) + L" ms to: " + out);
    } catch (const std::exception& ex) {
        setReviewSummary(L"ERROR exporting filtered view: " + widen(ex.what()));
    }
}


std::wstring withSuffixBeforeExtension(const std::wstring& path, const std::wstring& suffix) {
    const size_t slash = path.find_last_of(L"\\/");
    const size_t dot = path.find_last_of(L'.');
    if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)) return path.substr(0, dot) + suffix + path.substr(dot);
    return path + suffix;
}

std::size_t writeSqlCsv(sqlite3* db, const std::wstring& outPath, const std::string& sql) {
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
    std::ofstream f(narrow(outPath), std::ios::binary);
    if (!f) { sqlite3_finalize(st); throw std::runtime_error("Unable to open CSV for writing"); }
    f << "\xEF\xBB\xBF";
    const int cols = sqlite3_column_count(st);
    for (int c = 0; c < cols; ++c) {
        if (c) f << ',';
        f << csvEscape(sqlite3_column_name(st, c) ? sqlite3_column_name(st, c) : "");
    }
    f << "\n";
    std::size_t rows = 0;
    while (true) {
        const int rc = sqlite3_step(st);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) { std::string msg = sqlite3_errmsg(db); sqlite3_finalize(st); throw std::runtime_error(msg); }
        for (int c = 0; c < cols; ++c) {
            if (c) f << ',';
            const unsigned char* raw = sqlite3_column_text(st, c);
            f << csvEscape(raw ? reinterpret_cast<const char*>(raw) : "");
        }
        f << "\n";
        ++rows;
    }
    sqlite3_finalize(st);
    return rows;
}

void writeSupportManifest(const std::wstring& outPath, const std::vector<std::pair<std::wstring, std::size_t>>& files) {
    std::ofstream f(narrow(outPath), std::ios::binary);
    if (!f) throw std::runtime_error("Unable to open support manifest for writing");
    f << "\xEF\xBB\xBF";
    f << "file,row_count\n";
    for (const auto& item : files) f << csvEscape(narrow(item.first)) << ',' << item.second << "\n";
}

void exportSupportForArtifactIdSql(sqlite3* db, const std::string& idSql, const std::wstring& baseCsvPath, std::vector<std::pair<std::wstring, std::size_t>>& manifestRows) {
    const std::wstring rawKvPath = withSuffixBeforeExtension(baseCsvPath, L"_raw_key_values");
    const std::wstring rawDatesPath = withSuffixBeforeExtension(baseCsvPath, L"_raw_date_candidates");
    const std::wstring usagePath = withSuffixBeforeExtension(baseCsvPath, L"_usage_evidence");
    const std::wstring timelinePath = withSuffixBeforeExtension(baseCsvPath, L"_timeline_events");

    const std::string rawKvSql =
        "SELECT r.raw_kv_id,r.source_id,r.store_guid,r.source_db,r.inode_num,r.store_id,r.parent_inode_num,r.full_path,r.record_state,r.field_name,r.field_value "
        "FROM raw_key_values r JOIN artifacts a ON a.source_id=r.source_id AND a.store_guid=r.store_guid AND a.inode_num=r.inode_num "
        "WHERE a.artifact_id IN (" + idSql + ") ORDER BY a.artifact_id,r.raw_kv_id";
    manifestRows.push_back({rawKvPath, writeSqlCsv(db, rawKvPath, rawKvSql)});

    const std::string rawDatesSql =
        "SELECT raw_date_id,artifact_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,file_name,best_path,field_name,field_value,parsed_utc,parse_method,date_type,association_status,association_confidence "
        "FROM raw_date_candidates WHERE artifact_id IN (" + idSql + ") ORDER BY artifact_id,parsed_utc,raw_date_id";
    manifestRows.push_back({rawDatesPath, writeSqlCsv(db, rawDatesPath, rawDatesSql)});

    const std::string usageSql =
        "SELECT usage_id,artifact_id,source_id,store_guid,inode_num,field_name,field_value,parsed_utc FROM usage_evidence "
        "WHERE artifact_id IN (" + idSql + ") ORDER BY artifact_id,parsed_utc,usage_id";
    manifestRows.push_back({usagePath, writeSqlCsv(db, usagePath, usageSql)});

    const std::string timelineSql =
        "SELECT timeline_id,artifact_id,source_id,store_guid,inode_num,file_name,path,event_timestamp_utc,event_type,event_source_field,existence_status,deleted_or_orphaned_candidate "
        "FROM timeline_events WHERE artifact_id IN (" + idSql + ") ORDER BY artifact_id,event_timestamp_utc,timeline_id";
    manifestRows.push_back({timelinePath, writeSqlCsv(db, timelinePath, timelineSql)});
}

void exportCheckedArtifacts(HWND owner) {
    if (gOpenedCaseDb.empty()) { setReviewSummary(L"Open a case database before exporting checked artifacts."); return; }
    if (gCheckedArtifactIds.empty()) { setReviewSummary(L"No checked artifacts to export."); return; }
    std::wstring out = saveCsv(owner);
    if (out.empty()) return;
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        std::ofstream f(narrow(out), std::ios::binary);
        f << "\xEF\xBB\xBF";
        const std::vector<const char*> cols = {
            "artifact_id","tags","store_guid","inode_num","parent_inode_num","file_name","display_name","best_path",
            "spotlight_display_path","normalized_mac_path","content_type","content_type_tree","last_updated_utc",
            "first_used_candidate_utc","last_used_date_utc","used_dates_count","use_count_value","usage_field_summary",
            "downloaded_date_utc","where_froms","is_mounted_volume_path","mounted_volume_name","external_volume_reason",
            "existence_status","confidence"
        };
        for (size_t i = 0; i < cols.size(); ++i) { if (i) f << ','; f << csvEscape(cols[i]); }
        f << "\n";
        const std::string ids = checkedIdListSql();
        std::string sql =
            "SELECT artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,spotlight_display_path,normalized_mac_path,"
            "content_type,content_type_tree,last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,usage_field_summary,"
            "downloaded_date_utc,where_froms,is_mounted_volume_path,mounted_volume_name,external_volume_reason,existence_status,confidence "
            "FROM artifacts WHERE artifact_id IN (" + ids + ") ORDER BY artifact_id";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
        size_t rows = 0;
        while (true) {
            int rc = sqlite3_step(st);
            if (rc == SQLITE_DONE) break;
            if (rc != SQLITE_ROW) { std::string msg = sqlite3_errmsg(db.get()); sqlite3_finalize(st); throw std::runtime_error(msg); }
            const std::string artifactId = stmtText(st, 0);
            f << csvEscape(artifactId) << ',' << csvEscape(narrow(tagsForArtifact(db.get(), artifactId)));
            for (int c = 1; c < sqlite3_column_count(st); ++c) {
                f << ',';
                const unsigned char* raw = sqlite3_column_text(st, c);
                f << csvEscape(raw ? reinterpret_cast<const char*>(raw) : "");
            }
            f << "\n";
            ++rows;
        }
        sqlite3_finalize(st);
        std::vector<std::pair<std::wstring, std::size_t>> supportFiles;
        exportSupportForArtifactIdSql(db.get(), ids, out, supportFiles);
        const std::wstring manifestPath = withSuffixBeforeExtension(out, L"_support_manifest");
        supportFiles.insert(supportFiles.begin(), {out, rows});
        writeSupportManifest(manifestPath, supportFiles);
        setReviewSummary(L"Exported " + std::to_wstring(static_cast<unsigned long long>(rows)) + L" checked artifact(s) plus raw support files. Manifest: " + manifestPath);
    } catch (const std::exception& ex) {
        setReviewSummary(L"ERROR exporting checked artifacts: " + widen(ex.what()));
    }
}

void exportTaggedArtifacts(HWND owner) {
    if (gOpenedCaseDb.empty()) { setTagSummary(L"Open a case database before exporting tagged artifacts."); return; }
    const long long tagId = selectedTagId();
    if (tagId < 0) { setTagSummary(L"Select a tag to export."); return; }
    std::wstring out = saveCsv(owner);
    if (out.empty()) return;
    try {
        ReadOnlyDb db(gOpenedCaseDb);
        const std::string tagIdSql = std::to_string(tagId);
        const std::string artifactIdSql = "SELECT artifact_id FROM artifact_tags WHERE tag_id=" + tagIdSql;
        const std::string artifactSql =
            "SELECT tag_id,tag_name,tagged_utc,artifact_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,content_type,content_type_tree,logical_size_bytes,physical_size_bytes,usage_latest_utc,last_used_date_utc,modified_latest_utc,created_latest_utc,downloaded_latest_utc,where_froms,note_count,last_note_utc,last_note_text,confidence "
            "FROM vw_tagged_artifacts WHERE tag_id=" + tagIdSql + " ORDER BY COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(modified_latest_utc,''), artifact_id) DESC";
        std::vector<std::pair<std::wstring, std::size_t>> supportFiles;
        const std::size_t artifactRows = writeSqlCsv(db.get(), out, artifactSql);
        supportFiles.push_back({out, artifactRows});
        exportSupportForArtifactIdSql(db.get(), artifactIdSql, out, supportFiles);
        const std::wstring notesPath = withSuffixBeforeExtension(out, L"_notes");
        const std::string notesSql =
            "SELECT n.note_id,n.created_utc,n.updated_utc,n.note_text,n.artifact_id,n.store_guid,n.inode_num,n.parent_inode_num,n.file_name,n.display_name,n.best_path,n.content_type,n.tags "
            "FROM vw_artifact_notes n WHERE n.artifact_id IN (" + artifactIdSql + ") ORDER BY n.updated_utc DESC,n.created_utc DESC,n.note_id DESC";
        supportFiles.push_back({notesPath, writeSqlCsv(db.get(), notesPath, notesSql)});
        const std::wstring manifestPath = withSuffixBeforeExtension(out, L"_support_manifest");
        writeSupportManifest(manifestPath, supportFiles);
        setTagSummary(L"Exported tagged artifacts plus raw support files. Manifest: " + manifestPath);
    } catch (const std::exception& ex) {
        setTagSummary(L"ERROR exporting tagged artifacts: " + widen(ex.what()));
    }
}



std::wstring listViewText(HWND list, int row, int col) {
    if (!list || row < 0 || col < 0) return L"";
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
    out.reserve(value.size() + 32);
    bool lastWasSpace = false;
    for (wchar_t ch : value) {
        if (ch == L'\r') continue;
        if (ch == L'\n' || ch == L'\t') ch = L' ';
        if (ch == L' ') {
            if (lastWasSpace) continue;
            lastWasSpace = true;
        } else {
            lastWasSpace = false;
        }
        out += ch;
    }
    while (!out.empty() && out.front() == L' ') out.erase(out.begin());
    while (!out.empty() && out.back() == L' ') out.pop_back();
    if (out.empty()) return L"(empty)";
    return out;
}

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
    int width = std::max(500, static_cast<int>(rc.right - rc.left) - GetSystemMetrics(SM_CXVSCROLL) - 8);
    int fieldW = std::min(360, std::max(210, width / 4));
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
    std::wstring v = section ? L"" : normalizeDetailValueForTable(value);
    ListView_SetItemText(gRowDetails, row, 1, const_cast<LPWSTR>(v.c_str()));
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

void updateRowDetailsPanel() {
    if (!gRowDetails) return;
    if (!gList) {
        setDetailsPaneMessage(L"Open a case and select an investigation row to view all fields here.");
        return;
    }

    const int sel = selectedOrFocusedReviewRow();
    if (sel < 0) {
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
    double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    double rate = mb / static_cast<double>(seconds);
    std::wostringstream os;
    os.setf(std::ios::fixed);
    os.precision(2);
    os << L"source size " << mb << L" MiB; elapsed average " << rate << L" MiB/s";
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
    postClearProcessLog();
    postProgress(1);
    const auto ingestStart = std::chrono::steady_clock::now();
    const unsigned long long inputBytesForRate = fileSizeBytesNoThrow(getText(gInput));
    postStatus(L"Ingest running: initializing options.");
    postLog(L"Starting native C++ Spotlight workflow...");
    postLog(L"Processing telemetry: elapsed time and progress stages will be mirrored here for review. Throughput is calculated when the selected source is a measurable file such as a ZIP.");
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
                        postProgress(pct);
                        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ingestStart).count();
                        std::wstring status = L"Ingest running: " + std::to_wstring(pct) + L"% | elapsed " + formatElapsedSeconds(static_cast<unsigned long long>(elapsed));
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
                    if (inputBytesForRate > 0) heartbeat += L" | source-size/time average " + formatBytesPerSecond(inputBytesForRate, elapsedNow);
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
        opt.decodeCoreNativeValues = SendMessageW(gCoreDecode, BM_GETCHECK, 0, 0) == BST_CHECKED;
        opt.experimentalFullNativeValues = SendMessageW(gFullNative, BM_GETCHECK, 0, 0) == BST_CHECKED;
        opt.maxNativeRecords = 0;
        opt.maxNativeBlocks = 0;
        opt.verbose = SendMessageW(gVerbose, BM_GETCHECK, 0, 0) == BST_CHECKED;
        int exportProfile = static_cast<int>(SendMessageW(gExportProfile, CB_GETCURSEL, 0, 0));
        opt.exportProfile = exportProfile == 1 ? "investigator" : exportProfile == 2 ? "diagnostics" : exportProfile == 3 ? "full" : "minimal";
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
        auto rr = runApplication(opt);
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
    gLogo = addProcess(CreateWindowW(L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE, 16, y0, 72, 72, hwnd, nullptr, gInst, nullptr));
    const std::wstring logoPath = logoBitmapPath();
    if (!logoPath.empty()) {
        gLogoBitmap = static_cast<HBITMAP>(LoadImageW(nullptr, logoPath.c_str(), IMAGE_BITMAP, 72, 72, LR_LOADFROMFILE));
        if (gLogoBitmap) SendMessageW(gLogo, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(gLogoBitmap));
    }
    gBrandTitle = addProcess(CreateWindowW(L"STATIC", L"Vestigant Spotlight", WS_CHILD | WS_VISIBLE | SS_LEFT, 104, y0, 420, 28, hwnd, nullptr, gInst, nullptr));
    gBrandSubtitle = addProcess(CreateWindowW(L"STATIC", L"Forensic Spotlight/CoreSpotlight investigation workflow", WS_CHILD | WS_VISIBLE | SS_LEFT, 104, y0 + 28, 720, 22, hwnd, nullptr, gInst, nullptr));

    addProcess(CreateWindowW(L"STATIC", L"Case Information", WS_CHILD | WS_VISIBLE, 16, y0 + 82, 720, 24, hwnd, nullptr, gInst, nullptr));
    labelP(hwnd, L"Case Name", 16, y0 + 116, 120, 22); gCaseName = editP(hwnd, 140, y0 + 112, 330, 26, L"Spotlight Case"); SetWindowLongPtrW(gCaseName, GWLP_ID, ID_CASE_NAME_EDIT);
    labelP(hwnd, L"Case Number", 490, y0 + 116, 120, 22); gCaseNumber = editP(hwnd, 612, y0 + 112, 220, 26); SetWindowLongPtrW(gCaseNumber, GWLP_ID, ID_CASE_NUMBER_EDIT);
    labelP(hwnd, L"Investigator", 16, y0 + 152, 120, 22); gInvestigator = editP(hwnd, 140, y0 + 148, 330, 26); SetWindowLongPtrW(gInvestigator, GWLP_ID, ID_INVESTIGATOR_EDIT);
    labelP(hwnd, L"Company", 490, y0 + 152, 120, 22); gCompany = editP(hwnd, 612, y0 + 148, 220, 26); SetWindowLongPtrW(gCompany, GWLP_ID, ID_COMPANY_EDIT);
    labelP(hwnd, L"Case Location", 16, y0 + 188, 120, 22); gOut = editP(hwnd, 140, y0 + 184, 692, 26); SetWindowLongPtrW(gOut, GWLP_ID, ID_CASE_LOCATION_EDIT); gBrowseOut = buttonP(hwnd, L"Browse", ID_BROWSE_OUT, 842, y0 + 184, 90, 28);
    labelP(hwnd, L"Case Database", 16, y0 + 224, 120, 22); gCaseDbPath = editP(hwnd, 140, y0 + 220, 650, 26); SetWindowLongPtrW(gCaseDbPath, GWLP_ID, ID_CASE_DB_EDIT); gBrowseCase = buttonP(hwnd, L"Browse", ID_BROWSE_CASE, 800, y0 + 220, 90, 28); gOpenCase = openCaseButtonP(hwnd, 898, y0 + 218, 96, 32);
    gSaveCaseInfo = buttonP(hwnd, L"Save Case Info", ID_SAVE_CASE_INFO, 140, y0 + 254, 140, 28);
    gCaseAutosaveStatus = addProcess(CreateWindowW(L"STATIC", L"Autosave ready", WS_CHILD | WS_VISIBLE | SS_LEFT, 292, y0 + 258, 420, 22, hwnd, nullptr, gInst, nullptr));

    addProcess(CreateWindowW(L"STATIC", L"Build / Processing", WS_CHILD | WS_VISIBLE, 16, y0 + 292, 720, 24, hwnd, nullptr, gInst, nullptr));
    labelP(hwnd, L"Source type", 16, y0 + 328, 90, 22); gSourceType = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 112, y0 + 324, 180, 160, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SOURCE_TYPE)), gInst, nullptr));
    for (const wchar_t* s : {L"Folder", L"ZIP"}) SendMessageW(gSourceType, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gSourceType, CB_SETCURSEL, 0, 0);
    labelP(hwnd, L"Raw Spotlight evidence source", 300, y0 + 328, 190, 22); gInput = editP(hwnd, 492, y0 + 324, 380, 26); gBrowseInput = buttonP(hwnd, L"Browse", ID_BROWSE_INPUT, 882, y0 + 324, 100, 28);
    labelP(hwnd, L"Evidence root / iOS cache case", 16, y0 + 364, 220, 22); gEvidenceRoot = editP(hwnd, 236, y0 + 360, 636, 26); gBrowseRoot = buttonP(hwnd, L"Browse", ID_BROWSE_ROOT, 882, y0 + 360, 100, 28);
    labelP(hwnd, L"7z.exe path (optional)", 16, y0 + 400, 210, 22); gSevenZip = editP(hwnd, 236, y0 + 396, 636, 26); gBrowse7z = buttonP(hwnd, L"Browse", ID_BROWSE_7Z, 882, y0 + 396, 100, 28);
    addProcess(CreateWindowW(L"STATIC", L"Folder and ZIP workflows are the V1 investigator intake paths. AFF4/APFS and raw image support remain documented roadmap items.", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, y0 + 430, 966, 24, hwnd, nullptr, gInst, nullptr));

    labelP(hwnd, L"Profile", 16, y0 + 464, 90, 22); gProfile = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 106, y0 + 460, 180, 160, hwnd, nullptr, gInst, nullptr));
    for (const wchar_t* s : {L"Auto", L"Standard macOS", L"iOS/CoreSpotlight"}) SendMessageW(gProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gProfile, CB_SETCURSEL, 0, 0);
    labelP(hwnd, L"Mode", 300, y0 + 464, 56, 22); gMode = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 356, y0 + 460, 270, 140, hwnd, nullptr, gInst, nullptr));
    for (const wchar_t* s : {L"Process Raw Spotlight Evidence", L"Diagnostics / Bounded Native Parse", L"Discover Stores Only"}) SendMessageW(gMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gMode, CB_SETCURSEL, 0, 0);
    gRun = buttonP(hwnd, L"Build / Process Case", ID_RUN, 740, y0 + 456, 240, 36);
    gCoreDecode = addProcess(CreateWindowW(L"BUTTON", L"Enable safe core string/path probe", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 420, y0 + 500, 270, 24, hwnd, nullptr, gInst, nullptr));
    gFullNative = addProcess(CreateWindowW(L"BUTTON", L"Full native metadata values", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 700, y0 + 500, 250, 24, hwnd, nullptr, gInst, nullptr));
    SendMessageW(gFullNative, BM_SETCHECK, BST_CHECKED, 0);
    gVerbose = addProcess(CreateWindowW(L"BUTTON", L"Verbose log", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 106, y0 + 530, 120, 24, hwnd, nullptr, gInst, nullptr));
    SendMessageW(gVerbose, BM_SETCHECK, BST_CHECKED, 0);
    labelP(hwnd, L"Export profile", 715, y0 + 534, 100, 22); gExportProfile = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 815, y0 + 530, 154, 130, hwnd, nullptr, gInst, nullptr));
    for (const wchar_t* s : {L"Minimal", L"Investigator", L"Diagnostics", L"Full CSV"}) SendMessageW(gExportProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gExportProfile, CB_SETCURSEL, 1, 0);

    labelP(hwnd, L"Ingest status", 16, y0 + 568, 120, 22);
    gIngestStatus = addProcess(CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Waiting: choose/create a case location and evidence source, then run ingest. Progress, elapsed time, and throughput estimates appear here and in the processing log below.", WS_CHILD | WS_VISIBLE | SS_LEFT, 140, y0 + 564, 842, 64, hwnd, nullptr, gInst, nullptr));
    labelP(hwnd, L"Progress", 16, y0 + 640, 120, 22);
    gIngestProgress = addProcess(CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 140, y0 + 636, 842, 26, hwnd, nullptr, gInst, nullptr));
    SendMessageW(gIngestProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(gIngestProgress, PBM_SETPOS, 0, 0);
    gLog = addProcess(CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 16, y0 + 672, 966, 184, hwnd, nullptr, gInst, nullptr));
    appendLog(L"V0_9_57 V1 GUI polish: branded case header, processing telemetry log, custom view sets, and repaired tag-management schema handling.");
    appendLog(L"Processing log mirrors status/progress updates with elapsed time; ZIP/file source throughput is estimated when measurable.");
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
            focusReviewRow(hit.iItem + 1, true);
            setReviewSummary(L"Toggled checked state. Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size())));
            updateRowDetailsPanel();
            return 0;
        }
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
    addReview(CreateWindowW(L"STATIC", L"Investigation Results Grid: choose a platform-scoped review view from the left, then search/filter/sort/export database-backed results.", WS_CHILD | WS_VISIBLE, 16, y0, 960, 22, hwnd, nullptr, gInst, nullptr));
    labelR(hwnd, L"View Set", 16, y0 + 32, 90, 22);
    gReviewViewProfile = addReview(CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 86, y0 + 28, 160, 160, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_REVIEW_VIEW_PROFILE)), gInst, nullptr));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Recommended V1"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Timeline / Activity"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Text / Content"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"App DB / KnowledgeC"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Diagnostics / Coverage"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Show All"));
    SendMessageW(gReviewViewProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Custom"));
    SendMessageW(gReviewViewProfile, CB_SETCURSEL, 0, 0);
    applyUiFont(gReviewViewProfile);
    labelR(hwnd, L"Views", 16, y0 + 62, 220, 22);
    gReviewView = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY, 16, y0 + 88, 230, 580, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_REVIEW_VIEW)), gInst, nullptr));
    SetWindowSubclass(gReviewView, ReviewViewListSubclassProc, 2, 0);
    gViewSetUp = buttonR(hwnd, L"Move Up", ID_VIEWSET_UP, 16, y0 + 674, 70, 26);
    gViewSetDown = buttonR(hwnd, L"Move Down", ID_VIEWSET_DOWN, 92, y0 + 674, 80, 26);
    gViewSetHide = buttonR(hwnd, L"Hide", ID_VIEWSET_HIDE, 178, y0 + 674, 68, 26);
    gViewSetSave = buttonR(hwnd, L"Save Set", ID_VIEWSET_SAVE, 16, y0 + 706, 92, 26);
    gViewSetReset = buttonR(hwnd, L"Reset Set", ID_VIEWSET_RESET, 114, y0 + 706, 92, 26);
    // V0_9_17: do not create balloon tooltips for the view list. Hover updates
    // the help/summary panel above the table instead, which is less intrusive.
    populateViewList();

    labelR(hwnd, L"Search", 262, y0 + 34, 60, 22); gSearch = editR(hwnd, 326, y0 + 30, 250, 26);
    gRefresh = buttonR(hwnd, L"Update", ID_REVIEW_REFRESH, 586, y0 + 29, 86, 28);
    gCancelLoad = buttonR(hwnd, L"Cancel Load", ID_REVIEW_CANCEL_LOAD, 680, y0 + 29, 104, 28);
    EnableWindow(gCancelLoad, FALSE);
    labelR(hwnd, L"Rows", 794, y0 + 34, 42, 22); gPageSize = editR(hwnd, 838, y0 + 30, 60, 26, L"250");
    gPrev = buttonR(hwnd, L"Previous", ID_REVIEW_PREV, 262, y0 + 66, 104, 30);
    gNext = buttonR(hwnd, L"Next", ID_REVIEW_NEXT, 372, y0 + 66, 104, 30);
    gExportPage = buttonR(hwnd, L"Export Page", ID_EXPORT_PAGE, 482, y0 + 66, 112, 30);
    gExportFiltered = buttonR(hwnd, L"Export Filtered", ID_EXPORT_FILTERED, 600, y0 + 66, 126, 30);
    gExportChecked = buttonR(hwnd, L"Export Checked", ID_EXPORT_CHECKED, 732, y0 + 66, 126, 30);
    gClearChecked = buttonR(hwnd, L"Clear Checked", ID_CLEAR_CHECKED, 864, y0 + 66, 116, 30);
    gManageTags = buttonR(hwnd, L"Tags", ID_CTX_MANAGE_TAGS, 986, y0 + 66, 68, 30);
    gOpenCaseFolder = buttonR(hwnd, L"Case Folder", ID_OPEN_CASE_FOLDER, 262, y0 + 102, 116, 26);
    gOpenUploadFolder = buttonR(hwnd, L"Upload Folder", ID_OPEN_UPLOAD_FOLDER, 384, y0 + 102, 128, 26);
    gOpenLogsFolder = buttonR(hwnd, L"Logs", ID_OPEN_LOGS_FOLDER, 518, y0 + 102, 76, 26);
    gOpenDashboard = buttonR(hwnd, L"Dashboard", ID_OPEN_DASHBOARD, 732, y0 + 102, 116, 26);
    gOpenReviewIndex = buttonR(hwnd, L"Review Index", ID_OPEN_REVIEW_INDEX, 854, y0 + 102, 126, 26);
    gReviewSummary = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"No case opened. Open a database from the Case Information tab.", WS_CHILD | WS_VISIBLE | SS_LEFT, 262, y0 + 136, 720, 58, hwnd, nullptr, gInst, nullptr));
    gReviewBusy = addReview(CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | PBS_MARQUEE, 262, y0 + 198, 720, 6, hwnd, nullptr, gInst, nullptr));
    if (gReviewBusy) ShowWindow(gReviewBusy, SW_HIDE);
    gList = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, 262, y0 + 206, 720, 462, hwnd, nullptr, gInst, nullptr));
    ListView_SetExtendedListViewStyle(gList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    SetWindowSubclass(gList, ReviewListSubclassProc, 1, 0);
    gRowDetailsSplitter = addReview(CreateWindowW(L"STATIC", L"↕ Drag to resize selected-row details", WS_CHILD | SS_CENTER | SS_NOTIFY, 262, y0 + 666, 720, 8, hwnd, nullptr, gInst, nullptr));
    SetWindowSubclass(gRowDetailsSplitter, ReviewDetailsSplitterSubclassProc, 1, 0);
    gRowDetailsLabel = addReview(CreateWindowW(L"STATIC", L"Selected Row Details - true two-column Field / Metadata table", WS_CHILD, 262, y0 + 674, 720, 20, hwnd, nullptr, gInst, nullptr));
    gRowDetails = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
        262, y0 + 696, 720, 130, hwnd, nullptr, gInst, nullptr));
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

    moveIf(gLogo, 16, y0, 72, 72);
    moveIf(gBrandTitle, 104, y0, 420, 28);
    moveIf(gBrandSubtitle, 104, y0 + 28, std::max(400, right - 120), 22);
    moveIf(gCaseName, 140, y0 + 112, 330, 26);
    moveIf(gCaseNumber, 612, y0 + 112, std::max(220, right - 612 - 150), 26);
    moveIf(gInvestigator, 140, y0 + 148, 330, 26);
    moveIf(gCompany, 612, y0 + 148, std::max(220, right - 612 - 150), 26);
    moveIf(gBrowseOut, rightBrowseX, y0 + 184, browseW, 28);
    moveIf(gOut, 140, y0 + 184, std::max(250, rightBrowseX - 150), 26);
    moveIf(gOpenCase, right - 96, y0 + 218, 96, 32);
    moveIf(gBrowseCase, right - 198, y0 + 220, 90, 28);
    moveIf(gCaseDbPath, 140, y0 + 220, std::max(250, right - 350), 26);
    moveIf(gCaseAutosaveStatus, 292, y0 + 258, right - 308, 22);

    moveIf(gRun, right - 240, y0 + 456, 240, 36);
    moveIf(gBrowseInput, rightBrowseX, y0 + 324, browseW, 28);
    moveIf(gInput, 492, y0 + 324, std::max(220, rightBrowseX - 502), 26);
    moveIf(gBrowseRoot, rightBrowseX, y0 + 360, browseW, 28);
    moveIf(gEvidenceRoot, 236, y0 + 360, std::max(220, rightBrowseX - 246), 26);
    moveIf(gBrowse7z, rightBrowseX, y0 + 396, browseW, 28);
    moveIf(gSevenZip, 236, y0 + 396, std::max(220, rightBrowseX - 246), 26);
    moveIf(gIngestStatus, 140, y0 + 564, right - 140, 64);
    moveIf(gIngestProgress, 140, y0 + 636, right - 140, 26);
    moveIf(gLog, 16, y0 + 672, right - 16, std::max(120, bottom - (y0 + 672)));

    moveIf(gReviewViewProfile, 86, y0 + 28, 160, 160);
    const int viewSetButtonsY = bottom - 72;
    moveIf(gReviewView, 16, y0 + 88, 230, std::max(220, viewSetButtonsY - (y0 + 88) - 8));
    moveIf(gViewSetUp, 16, viewSetButtonsY, 70, 26);
    moveIf(gViewSetDown, 92, viewSetButtonsY, 80, 26);
    moveIf(gViewSetHide, 178, viewSetButtonsY, 68, 26);
    moveIf(gViewSetSave, 16, viewSetButtonsY + 32, 92, 26);
    moveIf(gViewSetReset, 114, viewSetButtonsY + 32, 92, 26);
    const int reviewX = 262;
    const int reviewW = right - reviewX;
    // Keep the search box from expanding over the Update/Cancel/Rows controls on wide windows.
    moveIf(gSearch, 326, y0 + 30, 250, 26);
    moveIf(gReviewSummary, reviewX, y0 + 136, reviewW, 58);
    moveIf(gReviewBusy, reviewX, y0 + 198, reviewW, 6);
    const int reviewBodyY = y0 + 206;
    const int reviewBodyH = std::max(300, bottom - reviewBodyY - 20);
    const int detailsLabelH = 20;
    const int detailGap = 6;
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
    moveIf(gManageTags, std::min(right - 210, 986), y0 + 66, 68, 30);
    moveIf(gOpenDashboard, right - 248, y0 + 102, 116, 26);
    moveIf(gOpenReviewIndex, right - 126, y0 + 102, 126, 26);

    moveIf(gTagList, 16, y0 + 60, 230, std::max(190, bottom - (y0 + 60) - 370));
    moveIf(gTagNote, 262, y0 + 130, std::max(360, right - 262 - 250), 92);
    moveIf(gTagSummary, 262, y0 + 236, std::max(360, right - 262 - 90), 54);
    moveIf(gTaggedList, 262, y0 + 304, right - 262, std::max(230, bottom - (y0 + 304) - 20));

    moveIf(gIosStatus, 16, y0 + 38, right - 16, 112);
    moveIf(gIosReadiness, 16, y0 + 268, std::max(360, (right - 40) / 2), std::max(180, bottom - (y0 + 268) - 20));
    moveIf(gIosPlan, std::max(500, cw / 2), y0 + 268, std::max(360, right - std::max(500, cw / 2)), std::max(180, bottom - (y0 + 268) - 20));
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
        if (hdr && hdr->hwndFrom == gList && hdr->code == LVN_ITEMCHANGED) {
            auto* pnmv = reinterpret_cast<LPNMLISTVIEW>(lp);
            if ((pnmv->uChanged & LVIF_STATE) && ((pnmv->uNewState ^ pnmv->uOldState) & LVIS_SELECTED)) {
                updateRowDetailsPanel();
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
                else postStatus(L"Source type: ZIP. Original ZIP will be hashed/registered and extracted to a controlled working staging folder; no second evidentiary archive is created.");
            }
            return 0;
        }
        case ID_BROWSE_INPUT: {
            int srcType = gSourceType ? static_cast<int>(SendMessageW(gSourceType, CB_GETCURSEL, 0, 0)) : 0;
            auto p = (srcType == 0) ? browseFolder(hwnd) : (srcType == 1 ? browseZip(hwnd) : browseEvidenceContainer(hwnd, srcType));
            if (!p.empty()) setText(gInput, p);
            return 0;
        }
        case ID_BROWSE_OUT: { auto p = browseFolder(hwnd); if (!p.empty()) { setText(gOut, p); if (getText(gCaseDbPath).empty()) setText(gCaseDbPath, p + L"\\VestigantSpotlight.case.sqlite"); postStatus(L"Case location selected: " + p); } return 0; }
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
            const bool fullNativeChecked = SendMessageW(gFullNative, BM_GETCHECK, 0, 0) == BST_CHECKED;
            const int exportProfileSel = gExportProfile ? static_cast<int>(SendMessageW(gExportProfile, CB_GETCURSEL, 0, 0)) : 1;
            const int modeSel = gMode ? static_cast<int>(SendMessageW(gMode, CB_GETCURSEL, 0, 0)) : 0;
            if (!fullNativeChecked && modeSel == 0 && (exportProfileSel == 1 || exportProfileSel == 3)) {
                const int rc = MessageBoxW(hwnd,
                    L"Full native metadata values are not enabled. Investigator/full exports will be header/core limited and may not show usage, WhereFroms, paths, or tag support details. Continue anyway?",
                    L"Vestigant Spotlight - Full Metadata Disabled",
                    MB_OKCANCEL | MB_ICONWARNING);
                if (rc != IDOK) {
                    postStatus(L"Waiting: full native metadata values were disabled and ingest was cancelled by operator.");
                    return 0;
                }
            }
            int srcType = gSourceType ? static_cast<int>(SendMessageW(gSourceType, CB_GETCURSEL, 0, 0)) : 0;
            postProgress(0);
            postStatus(L"Queued: ingest worker starting.");
            std::thread(worker).detach();
            return 0;
        }
        case ID_BROWSE_CASE: { auto p = browseSqlite(hwnd); if (!p.empty()) { setText(gCaseDbPath, p); size_t pos = p.find_last_of(L"\\/"); if (pos != std::wstring::npos) setText(gOut, p.substr(0, pos)); } return 0; }
        case ID_OPEN_CASE: { openCaseFromPath(); return 0; }
        case ID_SAVE_CASE_INFO: { saveCaseInformation(); return 0; }
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
    case WM_SET_INGEST_STATUS: { auto* s = reinterpret_cast<std::wstring*>(lp); if (s) { setText(gIngestStatus, *s); delete s; } return 0; }
    case WM_SET_INGEST_PROGRESS: { int pct = static_cast<int>(wp); if (pct < 0) pct = 0; if (pct > 100) pct = 100; if (gIngestProgress) SendMessageW(gIngestProgress, PBM_SETPOS, static_cast<WPARAM>(pct), 0); return 0; }
    case WM_REVIEW_PAGE_RESULT: { completeReviewPageLoad(reinterpret_cast<ReviewPageResult*>(lp)); return 0; }
    case WM_SIZE: { layoutControls(hwnd); return 0; }
    case WM_TIMER: { if (wp == ID_AUTOSAVE_TIMER) { autosaveCaseInformation(hwnd); return 0; } break; }
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        if (mmi) { mmi->ptMinTrackSize.x = 1040; mmi->ptMinTrackSize.y = 900; }
        return 0;
    }
    case WM_DESTROY: { gShuttingDown.store(true); cancelAndJoinReviewThreadNoThrow(); KillTimer(hwnd, ID_AUTOSAVE_TIMER); saveCaseInformationCore(true); if (gUiFont) { DeleteObject(gUiFont); gUiFont = nullptr; } PostQuitMessage(0); return 0; }
    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    SetUnhandledExceptionFilter(unhandledFilter);
    std::set_terminate(terminateHandler);
    gInst = hInstance;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    gUiFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    WNDCLASSW wc{}; wc.lpfnWndProc = wndProc; wc.hInstance = hInstance; wc.lpszClassName = L"VestigantSpotlightWnd"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, widen(appTitle()).c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1180, 900, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow); UpdateWindow(hwnd);
    MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    CoUninitialize(); return static_cast<int>(msg.wParam);
}
