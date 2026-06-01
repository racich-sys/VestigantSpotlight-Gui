#if !defined(_WIN32)
#error This file is Windows-only.
#endif

#include "app/app_runner.h"
#include "core/app_info.h"
#include "db/sqlite_compat.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
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
HWND gInput{}, gOut{}, gEvidenceRoot{}, gSevenZip{}, gSourceType{}, gProfile{}, gMode{}, gCaseName{}, gCaseNumber{}, gCompany{}, gInvestigator{}, gLog{}, gRun{}, gNoPreserve{}, gCoreDecode{}, gIngestStatus{}, gIngestProgress{};
HWND gFullNative{}, gMaxRecords{}, gMaxBlocks{}, gVerbose{}, gExportProfile{};
HWND gCaseDbPath{}, gBrowseCase{}, gBrowseOut{}, gBrowseInput{}, gBrowseRoot{}, gBrowse7z{}, gOpenCase{}, gSaveCaseInfo{}, gCaseAutosaveStatus{}, gOpenDashboard{}, gOpenReviewIndex{}, gOpenCaseFolder{}, gOpenUploadFolder{}, gOpenLogsFolder{}, gReviewView{}, gSearch{}, gPageSize{}, gRefresh{}, gCancelLoad{}, gPrev{}, gNext{}, gExportPage{}, gExportFiltered{}, gExportChecked{}, gClearChecked{}, gReviewSummary{}, gList{};
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
HFONT gUiFont{};
bool gCaseInfoDirty = false;
bool gIosReviewMode = false;
HWND gReviewViewTooltip{};
std::wstring gReviewViewTooltipText;
int gReviewViewHoverListIndex = -1;
struct ContextTagCommand { UINT commandId; long long tagId; int operation; };
std::vector<ContextTagCommand> gContextTagCommands;
constexpr int ID_RUN = 1001, ID_BROWSE_INPUT = 1002, ID_BROWSE_OUT = 1003, ID_BROWSE_ROOT = 1004, ID_BROWSE_7Z = 1005, ID_SOURCE_TYPE = 1006;
constexpr int ID_BROWSE_CASE = 1101, ID_OPEN_CASE = 1102, ID_REVIEW_REFRESH = 1103, ID_REVIEW_PREV = 1104, ID_REVIEW_NEXT = 1105, ID_EXPORT_PAGE = 1106, ID_EXPORT_FILTERED = 1113, ID_REVIEW_CANCEL_LOAD = 1114, ID_OPEN_CASE_FOLDER = 1115, ID_OPEN_UPLOAD_FOLDER = 1116, ID_OPEN_LOGS_FOLDER = 1117;
constexpr int ID_OPEN_DASHBOARD = 1107, ID_OPEN_REVIEW_INDEX = 1108, ID_REVIEW_VIEW = 1109, ID_SAVE_CASE_INFO = 1110, ID_EXPORT_CHECKED = 1111, ID_CLEAR_CHECKED = 1112;
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
        {L"iOS - Missing From FFS Candidates", "vw_ios_spotlight_missing_from_ffs_candidates", {"reference_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","field_name","raw_reference_value","reference_type","normalized_ios_path","missing_candidate_category","investigative_priority","investigative_reason","spotlight_text_context_sample","spotlight_text_context_status","matched_file_name","matched_size_bytes","matched_zip_modified_utc","matched_protection_class","matched_app_container","matched_domain","ffs_lookup_source","ffs_lookup_status","residency_status","confidence","interpretation_note"}, {"store_guid","source_db","inode_num","field_name","reference_type","normalized_ios_path","missing_candidate_category","investigative_priority","investigative_reason","spotlight_text_context_sample","spotlight_text_context_status","ffs_lookup_source","ffs_lookup_status","residency_status","confidence","matched_file_name","matched_app_container","matched_domain","raw_reference_value","interpretation_note"}, "investigative_priority_sort, residency_status, confidence, normalized_ios_path, reference_id"},
        {L"iOS - High-Value Missing From FFS", "vw_ios_spotlight_missing_from_ffs_high_value_candidates", {"reference_id","source_id","store_guid","source_db","inode_num","store_id","parent_inode_num","field_name","raw_reference_value","reference_type","normalized_ios_path","missing_candidate_category","investigative_priority","investigative_reason","spotlight_text_context_sample","spotlight_text_context_status","matched_file_name","matched_size_bytes","matched_zip_modified_utc","matched_protection_class","matched_app_container","matched_domain","ffs_lookup_source","ffs_lookup_status","residency_status","confidence","interpretation_note"}, {"store_guid","source_db","inode_num","field_name","reference_type","normalized_ios_path","missing_candidate_category","investigative_priority","investigative_reason","spotlight_text_context_sample","spotlight_text_context_status","ffs_lookup_source","ffs_lookup_status","residency_status","confidence","matched_file_name","matched_app_container","matched_domain","raw_reference_value","interpretation_note"}, "investigative_priority_sort, residency_status, confidence, normalized_ios_path, reference_id"},
        {L"iOS - Missing From FFS Summary", "vw_ios_spotlight_missing_from_ffs_summary", {"source_id","store_guid","source_db","field_name","reference_type","missing_candidate_category","investigative_priority","spotlight_text_context_status","ffs_lookup_status","missing_candidate_count","distinct_spotlight_object_count","distinct_missing_path_count","min_missing_path_sample","max_missing_path_sample","spotlight_text_context_sample","investigative_reason","interpretation_note"}, {"store_guid","source_db","field_name","reference_type","missing_candidate_category","investigative_priority","spotlight_text_context_status","ffs_lookup_status","missing_candidate_count","distinct_missing_path_count","min_missing_path_sample","max_missing_path_sample","spotlight_text_context_sample","investigative_reason","interpretation_note"}, "investigative_priority_sort, missing_candidate_count DESC, store_guid, field_name, reference_type"},
        {L"iOS - High-Value Missing From FFS Summary", "vw_ios_spotlight_missing_from_ffs_high_value_summary", {"source_id","store_guid","source_db","field_name","reference_type","missing_candidate_category","investigative_priority","spotlight_text_context_status","ffs_lookup_status","missing_candidate_count","distinct_spotlight_object_count","distinct_missing_path_count","min_missing_path_sample","max_missing_path_sample","spotlight_text_context_sample","investigative_reason","interpretation_note"}, {"store_guid","source_db","field_name","reference_type","missing_candidate_category","investigative_priority","spotlight_text_context_status","ffs_lookup_status","missing_candidate_count","distinct_missing_path_count","min_missing_path_sample","max_missing_path_sample","spotlight_text_context_sample","investigative_reason","interpretation_note"}, "investigative_priority_sort, missing_candidate_count DESC, store_guid, field_name, reference_type"},
        {L"iOS - Spotlight Text Context Review", "vw_ios_spotlight_text_context_review", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","last_updated_utc","file_name","display_name","content_type","text_context_category","review_priority","text_context_reason","classification_evidence","spotlight_text_context_sample","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","last_updated_utc","file_name","display_name","content_type","text_context_category","review_priority","text_context_reason","classification_evidence","spotlight_text_context_sample","interpretation_note"}, "review_priority_sort, last_updated_utc DESC, raw_record_id DESC"},
        {L"iOS - High-Value Spotlight Text Context", "vw_ios_spotlight_high_value_text_context_review", {"raw_kv_id","raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","last_updated_utc","file_name","display_name","content_type","text_context_category","review_priority","text_context_reason","classification_evidence","spotlight_text_context_sample","interpretation_note"}, {"store_guid","source_db","spotlight_inode_or_object_id","last_updated_utc","file_name","display_name","content_type","text_context_category","review_priority","text_context_reason","classification_evidence","spotlight_text_context_sample","interpretation_note"}, "review_priority_sort, last_updated_utc DESC, raw_record_id DESC"},
        {L"iOS - Spotlight Chat App Attribution Summary", "vw_ios_spotlight_chat_app_attribution_summary", {"text_context_category","review_priority","classification_evidence","context_record_count","distinct_spotlight_object_count","earliest_last_updated_utc","latest_last_updated_utc","min_text_context_sample","max_text_context_sample","text_context_reason","interpretation_note"}, {"text_context_category","review_priority","classification_evidence","context_record_count","distinct_spotlight_object_count","min_text_context_sample","max_text_context_sample","text_context_reason","interpretation_note"}, "text_context_category, context_record_count DESC"},

        {L"iOS - Spotlight Communications Investigator Review", "vw_ios_spotlight_communication_record_review", {"raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_date_utc","spotlight_date_source_field","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","message_domain_handle_or_chat","message_service","best_title_or_name","investigator_visible_text","description_or_snippet","account_identifier","phone_or_callback","callback_url","url_or_content_reference","attachment_or_media_path","message_identifier","mailbox_or_thread","mail_participants","saved_from_app","spotlight_text_context_sample","interpretation_note","validation_locator"}, {"store_guid","source_db","spotlight_date_utc","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","message_domain_handle_or_chat","message_service","best_title_or_name","investigator_visible_text","description_or_snippet","account_identifier","phone_or_callback","url_or_content_reference","attachment_or_media_path","message_identifier","mail_participants","saved_from_app","spotlight_text_context_sample","interpretation_note","validation_locator"}, "review_priority_sort, spotlight_date_utc DESC, raw_record_id DESC"},
        {L"iOS - Spotlight Message Text Review", "vw_ios_spotlight_message_text_review", {"raw_record_id","source_id","store_guid","source_db","spotlight_inode_or_object_id","spotlight_store_id","parent_inode_num","spotlight_date_utc","spotlight_date_source_field","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","message_domain_handle_or_chat","best_title_or_name","investigator_visible_text","description_or_snippet","snippet","account_identifier","message_service","phone_or_callback","callback_url","url_or_content_reference","attachment_or_media_path","message_identifier","mailbox_or_thread","mail_participants","spotlight_text_context_sample","interpretation_note","validation_locator"}, {"store_guid","source_db","spotlight_date_utc","communication_context_type","review_priority","content_type","bundle_id","domain_identifier","message_domain_handle_or_chat","best_title_or_name","investigator_visible_text","account_identifier","message_service","url_or_content_reference","attachment_or_media_path","message_identifier","mail_participants","spotlight_text_context_sample","interpretation_note","validation_locator"}, "review_priority_sort, spotlight_date_utc DESC, raw_record_id DESC"},
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
void appendLog(const std::wstring& s) {
    if (!gLog) return;
    const int len = GetWindowTextLengthW(gLog);
    SendMessageW(gLog, EM_SETSEL, len, len);
    const std::wstring line = s + L"\r\n";
    SendMessageW(gLog, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
}
void postLog(const std::wstring& s) { if (gLog) PostMessageW(GetParent(gLog), WM_APPEND_LOG, 0, reinterpret_cast<LPARAM>(new std::wstring(s))); }
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


void ensureGuiReviewViews(sqlite3* db) {
    if (!db) return;

    auto execGuiSql = [db](const char* sql) {
        char* err = nullptr;
        const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::string msg = err ? err : "unknown SQLite error creating review views";
            sqlite3_free(err);
            throw std::runtime_error("Unable to create/update GUI review views: " + msg);
        }
    };

    auto execGuiSqlParts = [&](std::initializer_list<const char*> parts) {
        std::string sql;
        size_t total = 0;
        for (const char* part : parts) {
            if (part) total += std::strlen(part);
        }
        sql.reserve(total);
        for (const char* part : parts) {
            if (part) sql += part;
        }
        execGuiSql(sql.c_str());
    };

    auto guiColumnExists = [db](const char* table, const char* column) -> bool {
        std::string sql = std::string("PRAGMA table_info(") + table + ")";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) return false;
        bool exists = false;
        while (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char* name = sqlite3_column_text(st, 1);
            if (name && std::string(reinterpret_cast<const char*>(name)) == column) { exists = true; break; }
        }
        sqlite3_finalize(st);
        return exists;
    };
    auto ensureGuiColumn = [&](const char* table, const char* column, const char* type) {
        if (!guiColumnExists(table, column)) {
            std::string sql = std::string("ALTER TABLE ") + table + " ADD COLUMN " + column + " " + type;
            execGuiSql(sql.c_str());
        }
    };
    ensureGuiColumn("raw_date_candidates", "artifact_id", "INTEGER");
    ensureGuiColumn("raw_date_candidates", "parent_inode_num", "TEXT");
    ensureGuiColumn("raw_date_candidates", "file_name", "TEXT");
    ensureGuiColumn("raw_date_candidates", "best_path", "TEXT");
    ensureGuiColumn("raw_date_candidates", "date_type", "TEXT");
    ensureGuiColumn("raw_date_candidates", "association_status", "TEXT");
    ensureGuiColumn("raw_date_candidates", "association_confidence", "TEXT");


    execGuiSql(R"SQL(
CREATE TABLE IF NOT EXISTS artifact_date_summary (
  artifact_id INTEGER PRIMARY KEY,
  source_id TEXT,
  store_guid TEXT,
  inode_num TEXT,
  parent_inode_num TEXT,
  file_name TEXT,
  display_name TEXT,
  best_path TEXT,
  path_source TEXT,
  path_status TEXT,
  logical_size_bytes INTEGER,
  physical_size_bytes INTEGER,
  content_type TEXT,
  where_froms TEXT,
  created_earliest_utc TEXT,
  created_latest_utc TEXT,
  modified_earliest_utc TEXT,
  modified_latest_utc TEXT,
  downloaded_earliest_utc TEXT,
  downloaded_latest_utc TEXT,
  usage_earliest_utc TEXT,
  usage_latest_utc TEXT,
  interesting_or_index_earliest_utc TEXT,
  interesting_or_index_latest_utc TEXT,
  likely_snapshot_date_count INTEGER DEFAULT 0,
  associated_date_count INTEGER DEFAULT 0,
  unassociated_date_count INTEGER DEFAULT 0,
  available_date_fields TEXT,
  association_confidence_summary TEXT,
  snapshot_warning_reasons TEXT,
  first_date_utc TEXT,
  last_date_utc TEXT,
  total_date_count INTEGER DEFAULT 0,
  created_date_count INTEGER DEFAULT 0,
  modified_date_count INTEGER DEFAULT 0,
  downloaded_date_count INTEGER DEFAULT 0,
  usage_date_count INTEGER DEFAULT 0,
  interesting_or_index_date_count INTEGER DEFAULT 0,
  metadata_seen_or_index_updated_count INTEGER DEFAULT 0,
  other_date_count INTEGER DEFAULT 0,
  likely_snapshot_or_index_date_count INTEGER DEFAULT 0,
  interpreted_date_types TEXT,
  date_association_status TEXT,
  date_association_confidence TEXT,
  refreshed_utc TEXT
);
CREATE INDEX IF NOT EXISTS idx_artifact_date_summary_source_artifact ON artifact_date_summary(source_id, artifact_id);
CREATE INDEX IF NOT EXISTS idx_artifact_date_summary_last_date ON artifact_date_summary(last_date_utc, artifact_id);
)SQL");

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_usage_artifacts;
CREATE VIEW vw_usage_artifacts AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,
       first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,usage_field_summary,open_count_estimate,
       existence_status,deleted_or_orphaned_candidate,confidence
FROM artifacts
WHERE COALESCE(usage_field_summary,'')<>''
   OR COALESCE(last_used_date_utc,'')<>''
   OR COALESCE(first_used_candidate_utc,'')<>''
   OR COALESCE(used_dates_count,0)>0
   OR COALESCE(use_count_value,'')<>'';

DROP VIEW IF EXISTS vw_timeline_usage_focus;
CREATE VIEW vw_timeline_usage_focus AS
SELECT t.timeline_id,t.event_timestamp_utc AS event_utc,t.event_type,t.event_source_field,t.artifact_id,t.source_id,t.store_guid,t.inode_num,
       a.parent_inode_num,a.file_name,a.display_name,a.best_path,a.content_type,a.where_froms,'MEDIUM_USAGE_FIELD' AS confidence,'' AS notes
FROM timeline_events t
LEFT JOIN artifacts a ON a.artifact_id=t.artifact_id
WHERE lower(COALESCE(t.event_source_field,'')) LIKE '%used%'
   OR lower(COALESCE(t.event_type,'')) LIKE '%used%'
   OR lower(COALESCE(t.event_source_field,'')) LIKE '%usage%'
   OR lower(COALESCE(t.event_source_field,'')) LIKE '%open%';

DROP VIEW IF EXISTS vw_wherefroms_downloads;
CREATE VIEW vw_wherefroms_downloads AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,where_froms,
       downloaded_date_utc,last_updated_utc,existence_status,deleted_or_orphaned_candidate,confidence
FROM artifacts
WHERE COALESCE(where_froms,'')<>'' OR COALESCE(downloaded_date_utc,'')<>'';

DROP VIEW IF EXISTS vw_recent_activity;
CREATE VIEW vw_recent_activity AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,
       last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,where_froms,confidence
FROM artifacts
WHERE COALESCE(last_updated_utc,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'';

DROP VIEW IF EXISTS vw_content_type_summary;
CREATE VIEW vw_content_type_summary AS
SELECT COALESCE(NULLIF(content_type,''),'(blank)') AS content_type,
       COUNT(*) AS artifact_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_artifact_count,
       SUM(CASE WHEN COALESCE(best_path,'')<>'' THEN 1 ELSE 0 END) AS path_artifact_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
)SQL" R"SQL(       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc,
       SUM(CAST(COALESCE(NULLIF(logical_size_bytes,''),'0') AS INTEGER)) AS total_logical_size_bytes,
       SUM(CAST(COALESCE(NULLIF(physical_size_bytes,''),'0') AS INTEGER)) AS total_physical_size_bytes
FROM artifacts
GROUP BY COALESCE(NULLIF(content_type,''),'(blank)');

DROP VIEW IF EXISTS vw_store_content_type_summary;
CREATE VIEW vw_store_content_type_summary AS
SELECT store_guid,
       COALESCE(NULLIF(content_type,''),'(blank)') AS content_type,
       COUNT(*) AS artifact_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_artifact_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc
FROM artifacts
GROUP BY store_guid, COALESCE(NULLIF(content_type,''),'(blank)');

DROP VIEW IF EXISTS vw_folder_activity;
CREATE VIEW vw_folder_activity AS
SELECT store_guid,
       parent_inode_num,
       COUNT(*) AS child_count,
       SUM(CASE WHEN COALESCE(usage_field_summary,'')<>'' OR COALESCE(last_used_date_utc,'')<>'' OR COALESCE(first_used_candidate_utc,'')<>'' OR COALESCE(used_dates_count,0)>0 OR COALESCE(use_count_value,'')<>'' THEN 1 ELSE 0 END) AS usage_child_count,
       MIN(NULLIF(last_updated_utc,'')) AS first_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS last_last_updated_utc,
       MIN(NULLIF(best_path,'')) AS sample_child_path,
       SUM(CASE WHEN COALESCE(content_type,'')='public.folder' THEN 1 ELSE 0 END) AS folder_child_count
FROM artifacts
WHERE COALESCE(parent_inode_num,'')<>''
GROUP BY store_guid, parent_inode_num
HAVING COUNT(*) > 1;

DROP VIEW IF EXISTS vw_path_reconstruction;
CREATE VIEW vw_path_reconstruction AS
SELECT pl.link_id,
       pl.source_id,
       pl.store_guid,
       pl.child_artifact_id AS artifact_id,
       pl.child_inode_num AS inode_num,
       pl.child_parent_inode_num AS parent_inode_num,
       COALESCE(NULLIF(a.file_name,''), pl.child_file_name) AS file_name,
       COALESCE(NULLIF(a.display_name,''), pl.child_file_name) AS display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       pl.parent_artifact_id,
       pl.parent_inode_num AS resolved_parent_inode_num,
       pl.parent_file_name,
       pl.parent_best_path,
       pl.reconstructed_path_candidate,
       CASE WHEN COALESCE(a.path_source,'')='PARENT_INODE_RECONSTRUCTION' THEN 1 ELSE 0 END AS applied_to_artifact_path,
)SQL" R"SQL(       CASE WHEN COALESCE(a.best_path,'')=COALESCE(pl.reconstructed_path_candidate,'') AND COALESCE(pl.reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END AS candidate_matches_artifact_path,
       pl.sibling_group_key,
       pl.sibling_count,
       pl.relationship_status,
       pl.path_reconstruction_method,
       pl.confidence
FROM parent_inode_links pl
LEFT JOIN artifacts a ON a.artifact_id=pl.child_artifact_id
ORDER BY pl.source_id, pl.store_guid, CAST(pl.child_parent_inode_num AS INTEGER), CAST(pl.child_inode_num AS INTEGER), pl.link_id;

DROP VIEW IF EXISTS vw_same_folder_groups;
)SQL");
    execGuiSql(R"SQL(CREATE VIEW vw_same_folder_groups AS
SELECT source_id,
       store_guid,
       child_parent_inode_num AS parent_inode_num,
       MAX(parent_artifact_id) AS parent_artifact_id,
       MAX(CASE WHEN COALESCE(NULLIF(trim(parent_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN parent_file_name ELSE '' END) AS parent_file_name,
       MAX(CASE WHEN COALESCE(NULLIF(trim(parent_best_path),''),'') NOT IN ('/','------NONAME------','(null)','NULL','.') THEN parent_best_path ELSE '' END) AS parent_best_path,
       COUNT(*) AS child_count,
       SUM(CASE WHEN relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE' THEN 1 ELSE 0 END) AS resolved_parent_link_count,
       SUM(CASE WHEN COALESCE(reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END) AS reconstructed_child_path_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) AS child_name_count,
       MIN(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END) AS first_child_name,
       MAX(CASE WHEN COALESCE(NULLIF(trim(child_file_name),''),'') NOT IN ('------NONAME------','(null)','NULL','.') THEN child_file_name ELSE NULL END) AS last_child_name,
       CASE
         WHEN SUM(CASE WHEN COALESCE(reconstructed_path_candidate,'')<>'' THEN 1 ELSE 0 END)>0 THEN 'RECONSTRUCTED_CHILD_PATHS_PRESENT'
         WHEN SUM(CASE WHEN relationship_status='PARENT_INODE_MATCHED_IN_SAME_STORE' THEN 1 ELSE 0 END)>0 THEN 'PARENT_LINKS_WITHOUT_RECONSTRUCTED_PATH'
         ELSE 'SAME_PARENT_INODE_GROUP_ONLY'
       END AS folder_group_status,
       MAX(confidence) AS max_confidence,
       sibling_group_key
FROM parent_inode_links
GROUP BY source_id, store_guid, child_parent_inode_num, sibling_group_key
HAVING COUNT(*) > 1
ORDER BY child_count DESC, source_id, store_guid, CAST(child_parent_inode_num AS INTEGER);

DROP VIEW IF EXISTS vw_volume_root_focus;
CREATE VIEW vw_volume_root_focus AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,content_type_tree,
       last_updated_utc,first_used_candidate_utc,last_used_date_utc,used_dates_count,use_count_value,is_mounted_volume_path,
       mounted_volume_name,external_volume_reason,confidence
FROM artifacts
WHERE COALESCE(content_type,'')='public.volume'
   OR COALESCE(is_mounted_volume_path,0)<>0
   OR COALESCE(mounted_volume_name,'')<>''
   OR COALESCE(external_volume_reason,'')<>'';
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_investigator_keyword_search_values;
CREATE VIEW vw_investigator_keyword_search_values AS
SELECT CASE WHEN store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%' THEN 'ios' ELSE 'macos_or_unknown' END AS platform,
       'raw_key_values' AS source_table,
       source_id, store_guid, source_db,
       NULL AS artifact_id,
       inode_num, parent_inode_num,
       field_name,
       field_value AS search_value,
       '' AS date_utc,
       '' AS file_name,
       full_path AS best_path,
       '' AS content_type,
       store_path AS provenance
FROM raw_key_values
WHERE COALESCE(field_value,'')<>''
UNION ALL
SELECT CASE WHEN store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%' THEN 'ios' ELSE 'macos_or_unknown' END AS platform,
       'raw_records' AS source_table,
       source_id, store_guid, source_db,
       NULL AS artifact_id,
       inode_num, parent_inode_num,
       'record_summary' AS field_name,
       trim(COALESCE(file_name,'') || ' ' || COALESCE(display_name,'') || ' ' || COALESCE(full_path,'') || ' ' || COALESCE(content_type,'') || ' ' || COALESCE(where_froms,'')) AS search_value,
       last_updated_utc AS date_utc,
       file_name,
       full_path AS best_path,
       content_type,
       store_path AS provenance
FROM raw_records
WHERE trim(COALESCE(file_name,'') || COALESCE(display_name,'') || COALESCE(full_path,'') || COALESCE(content_type,'') || COALESCE(where_froms,''))<>''
UNION ALL
SELECT CASE WHEN store_guid LIKE 'ios_%' THEN 'ios' ELSE 'macos_or_unknown' END AS platform,
       'artifacts' AS source_table,
       source_id,
       store_guid,
       '' AS source_db,
       artifact_id,
       inode_num,
       parent_inode_num,
       'artifact_summary' AS field_name,
       trim(COALESCE(file_name,'') || ' ' || COALESCE(display_name,'') || ' ' || COALESCE(best_path,'') || ' ' || COALESCE(content_type,'') || ' ' || COALESCE(where_froms,'') || ' ' || COALESCE(index_text_snippet,'')) AS search_value,
       COALESCE(NULLIF(last_updated_utc,''), NULLIF(last_used_date_utc,''), NULLIF(first_used_candidate_utc,''), NULLIF(downloaded_date_utc,'')) AS date_utc,
       file_name,
       best_path,
       content_type,
       path_source || ';' || path_status AS provenance
FROM artifacts
WHERE trim(COALESCE(file_name,'') || COALESCE(display_name,'') || COALESCE(best_path,'') || COALESCE(content_type,'') || COALESCE(where_froms,'') || COALESCE(index_text_snippet,''))<>'';

CREATE TABLE IF NOT EXISTS ios_ffs_file_inventory (
  ios_file_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT, original_zip_entry TEXT, normalized_path TEXT, file_name TEXT, extension TEXT, size_bytes INTEGER,
)SQL" R"SQL(  zip_modified_utc TEXT, protection_class_hint TEXT, app_container_hint TEXT, domain_hint TEXT, is_directory INTEGER DEFAULT 0,
  sha256_status TEXT, inventory_notes TEXT, created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_ffs_path_lookup (
  ios_path_lookup_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT, normalized_path TEXT, file_name TEXT, size_bytes INTEGER, zip_modified_utc TEXT,
  protection_class_hint TEXT, app_container_hint TEXT, domain_hint TEXT, is_directory INTEGER DEFAULT 0, lookup_source TEXT, created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_app_database_inventory (
  ios_db_id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id TEXT, original_zip_entry TEXT, normalized_path TEXT, database_name TEXT, database_category TEXT, app_hint TEXT,
  protection_class_hint TEXT, size_bytes INTEGER, zip_modified_utc TEXT, parse_status TEXT, record_inventory_status TEXT, notes TEXT, extracted_path TEXT, created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_app_database_record_inventory (
  ios_record_inventory_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, ios_db_id INTEGER, database_normalized_path TEXT, database_name TEXT, database_category TEXT, app_hint TEXT,
  table_name TEXT, row_count INTEGER, sample_columns TEXT, record_category TEXT, parse_status TEXT, notes TEXT, created_utc TEXT
);
CREATE TABLE IF NOT EXISTS ios_app_parsed_records (
  ios_app_record_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, ios_db_id INTEGER, database_normalized_path TEXT, database_name TEXT, database_category TEXT, app_hint TEXT,
  table_name TEXT, record_category TEXT, source_primary_key TEXT, record_timestamp_utc TEXT, timestamp_source TEXT, contact_or_participant TEXT, url TEXT, title TEXT,
  file_path TEXT, item_identifier TEXT, text_snippet TEXT, parse_status TEXT, provenance TEXT, created_utc TEXT
);
CREATE INDEX IF NOT EXISTS idx_ios_ffs_path ON ios_ffs_file_inventory(source_id, normalized_path);
CREATE INDEX IF NOT EXISTS idx_ios_ffs_lookup_path ON ios_ffs_path_lookup(source_id, normalized_path);
CREATE INDEX IF NOT EXISTS idx_ios_db_category ON ios_app_database_inventory(source_id, database_category, app_hint);
CREATE INDEX IF NOT EXISTS idx_ios_db_record_source ON ios_app_database_record_inventory(source_id, database_category, table_name);
CREATE INDEX IF NOT EXISTS idx_ios_app_parsed_source ON ios_app_parsed_records(source_id, database_category, record_category);
CREATE INDEX IF NOT EXISTS idx_ios_app_parsed_db ON ios_app_parsed_records(ios_db_id, table_name);

DROP VIEW IF EXISTS vw_ios_relevant_fields;
CREATE VIEW vw_ios_relevant_fields AS
SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,full_path,record_state,field_name,field_value
FROM raw_key_values
WHERE lower(field_name) LIKE '%content%'
   OR lower(field_name) LIKE '%text%'
   OR lower(field_name) LIKE '%message%'
   OR lower(field_name) LIKE '%conversation%'
   OR lower(field_name) LIKE '%sender%'
   OR lower(field_name) LIKE '%recipient%'
   OR lower(field_name) LIKE '%author%'
   OR lower(field_name) LIKE '%creator%'
   OR lower(field_name) LIKE '%account%'
   OR lower(field_name) LIKE '%phone%'
   OR lower(field_name) LIKE '%mail%'
   OR lower(field_name) LIKE '%bundle%'
   OR lower(field_name) LIKE '%domain%'
   OR lower(field_name) LIKE '%used%'
   OR lower(field_name) LIKE '%wherefrom%'
   OR lower(field_name) LIKE '%download%'
   OR lower(field_name) LIKE '%path%'
   OR lower(field_name) LIKE '%filename%'
   OR lower(field_name) LIKE '%displayname%';

DROP VIEW IF EXISTS vw_ios_store_parse_summary;
CREATE VIEW vw_ios_store_parse_summary AS
SELECT store_guid,
       source_db,
       COUNT(*) AS raw_record_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(file_name),''),'') NOT IN ('','------NONAME------','------PLIST------','(null)','NULL','.') THEN 1 ELSE 0 END) AS record_file_name_count,
)SQL" R"SQL(       SUM(CASE WHEN COALESCE(NULLIF(trim(full_path),''),'') NOT IN ('','/','------NONAME------','(null)','NULL','.') THEN 1 ELSE 0 END) AS record_full_path_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(file_name),''),'') IN ('------NONAME------','------PLIST------','(null)','NULL','.') OR COALESCE(NULLIF(trim(file_name),''),'')='' THEN 1 ELSE 0 END) AS placeholder_file_name_count,
       SUM(CASE WHEN COALESCE(NULLIF(trim(full_path),''),'')='/' THEN 1 ELSE 0 END) AS placeholder_root_path_count,
       MIN(NULLIF(last_updated_utc,'')) AS earliest_last_updated_utc,
       MAX(NULLIF(last_updated_utc,'')) AS latest_last_updated_utc
FROM raw_records
WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
GROUP BY store_guid, source_db;

DROP VIEW IF EXISTS vw_ios_string_probe_category_summary;
CREATE VIEW vw_ios_string_probe_category_summary AS
WITH probe AS (
  SELECT store_guid, source_db, inode_num, field_name, field_value, LOWER(field_value) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), categorized AS (
  SELECT CASE
    WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'URL_OR_WEB_LINK'
    WHEN v LIKE '%@%' AND v LIKE '%.%' THEN 'EMAIL_ADDRESS_OR_ACCOUNT'
    WHEN v LIKE '%imessage%' OR v LIKE '%sms%' OR v LIKE '%message%' THEN 'MESSAGE_TEXT_OR_MESSAGE_APP'
    WHEN v LIKE '%icloud%' OR v LIKE '%onedrive%' OR v LIKE '%dropbox%' OR v LIKE '%google drive%' OR v LIKE '%drive.google%' THEN 'CLOUD_STORAGE_OR_SYNC'
    WHEN v LIKE '%calendar%' OR v LIKE '%invite%' OR v LIKE '%rsvp%' OR v LIKE '%event%' THEN 'CALENDAR_OR_INVITATION'
    WHEN v LIKE 'file:%' OR v LIKE '/private/var/%' OR v LIKE '%/mobile/%' THEN 'FILE_OR_IOS_PATH'
    ELSE 'OTHER_STRING_PROBE' END AS probe_category,
    store_guid, source_db, inode_num, field_value
  FROM probe
)
SELECT probe_category,
       COUNT(*) AS row_count,
       COUNT(DISTINCT store_guid) AS store_count,
       COUNT(DISTINCT inode_num) AS distinct_record_count,
       COUNT(DISTINCT field_value) AS distinct_value_count,
       substr(MIN(field_value),1,500) AS min_sample_value,
       substr(MAX(field_value),1,500) AS max_sample_value
FROM categorized
GROUP BY probe_category;

DROP VIEW IF EXISTS vw_ios_string_probe_values;
)SQL");
    execGuiSqlParts({
        R"VSGUI(CREATE VIEW vw_ios_string_probe_values AS
SELECT CASE
    WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN 'URL_OR_WEB_LINK'
    WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN 'EMAIL_ADDRESS_OR_ACCOUNT'
    WHEN LOWER(field_value) LIKE '%imessage%' OR LOWER(field_value) LIKE '%sms%' OR LOWER(field_value) LIKE '%message%' THEN 'MESSAGE_TEXT_OR_MESSAGE_APP'
    WHEN LOWER(field_value) LIKE '%icloud%' OR LOWER(field_value) LIKE '%onedrive%' OR LOWER(field_value) LIKE '%dropbox%' OR LOWER(field_value) LIKE '%google drive%' OR LOWER(field_value) LIKE '%drive.google%' THEN 'CLOUD_STORAGE_OR_SYNC'
    WHEN LOWER(field_value) LIKE '%calendar%' OR LOWER(field_value) LIKE '%invite%' OR LOWER(field_value) LIKE '%rsvp%' OR LOWER(field_value) LIKE '%event%' THEN 'CALENDAR_OR_INVITATION'
    WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN 'FILE_OR_IOS_PATH'
    ELSE 'OTHER_STRING_PROBE' END AS probe_category,
       raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,substr(field_value,1,2000) AS field_value_sample
FROM raw_key_values
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(field_value,'')<>'';

DROP VIEW IF EXISTS vw_ios_record_string_probe_summary;
CREATE VIEW vw_ios_record_string_probe_summary AS
WITH kv AS (
  SELECT source_id, store_guid, source_db, inode_num, store_id, parent_inode_num,
         CASE
           WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN 'URL_OR_WEB_LINK'
           WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN 'EMAIL_ADDRESS_OR_ACCOUNT'
           WHEN LOWER(field_value) LIKE '%imessage%' OR LOWER(field_value) LIKE '%sms%' OR LOWER(field_value) LIKE '%message%' THEN 'MESSAGE_TEXT_OR_MESSAGE_APP'
           WHEN LOWER(field_value) LIKE '%icloud%' OR LOWER(field_value) LIKE '%onedrive%' OR LOWER(field_value) LIKE '%dropbox%' OR LOWER(field_value) LIKE '%google drive%' OR LOWER(field_value) LIKE '%drive.google%' THEN 'CLOUD_STORAGE_OR_SYNC'
           WHEN LOWER(field_value) LIKE '%calendar%' OR LOWER(field_value) LIKE '%invite%' OR LOWER(field_value) LIKE '%rsvp%' OR LOWER(field_value) LIKE '%event%' THEN 'CALENDAR_OR_INVITATION'
           WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN 'FILE_OR_IOS_PATH'
           ELSE 'OTHER_STRING_PROBE'
         END AS probe_category,
         field_name,
         field_value
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), agg AS (
  SELECT source_id, store_guid, source_db, inode_num, store_id,
         COUNT(*) AS string_probe_rows,
         COUNT(DISTINCT field_name) AS distinct_probe_field_count,
         GROUP_CONCAT(DISTINCT probe_category) AS probe_categories,
         substr(GROUP_CONCAT(substr(field_name || '=' || REPLACE(REPLACE(field_value, char(13),' '), char(10),' '),1,500), ' || '),1,4000) AS string_probe_sample
  FROM kv
  GROUP BY source_id, store_guid, source_db, inode_num, store_id
)
SELECT r.raw_record_id,
       r.source_id,
       r.store_guid,
       r.source_db,
       r.inode_num,
       r.store_id,
       r.parent_inode_num,
       r.file_name,
       r.content_type,
       r.display_name,
       r.full_path,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       a.string_probe_rows,
       a.distinct_probe_field_count,
       a.probe_categories,
       a.string_probe_sample,
       r.record_state
FROM raw_records r
JOIN agg a
  ON a.source_id = r.source_id
 AND a.store_guid = r.store_guid
 AND a.source_db = r.source_db
 AND a.inode_num = r.inode_num
 AND a.store_id = r.store_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_timeline_index_updates;
CREATE VIEW vw_ios_timeline_index_updates AS
SELECT raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,file_name,content_type,display_name,full_path,last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       record_state
FROM raw_records
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(last_updated_utc,'')<>'';


DROP VIEW IF EXISTS vw_ios_spotlight_date_provenance;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_date_provenance AS
WITH dc AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         COUNT(*) AS date_candidate_count,
         MAX(CASE WHEN field_name='Last_Updated' THEN parsed_utc ELSE parsed_utc END) AS primary_date_utc,
         MAX(CASE WHEN field_name='Last_Updated' THEN field_name ELSE field_name END) AS primary_date_field,
         MAX(CASE WHEN field_name='Last_Updated' THEN field_value ELSE field_value END) AS primary_raw_value,
         MAX(CASE WHEN field_name='Last_Updated' THEN parse_method ELSE parse_method END) AS primary_parse_method,
         GROUP_CONCAT(DISTINCT field_name) AS date_source_fields,
         GROUP_CONCAT(DISTINCT parse_method) AS date_parse_methods,
         GROUP_CONCAT(DISTINCT date_type) AS date_type_summary,
         substr(GROUP_CONCAT(COALESCE(field_name,'') || '=' || COALESCE(field_value,'') || ' -> ' || COALESCE(parsed_utc,'') || ' [' || COALESCE(parse_method,'') || ']',' || '),1,4000) AS date_source_evidence
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.raw_record_id,
       r.source_id,
       r.store_guid,
       r.source_db,
       r.store_path,
       r.inode_num AS spotlight_inode_or_object_id,
       r.store_id AS spotlight_store_id,
       r.parent_inode_num,
       r.file_name,
       r.display_name,
       r.full_path,
       r.content_type,
       r.record_state,
       COALESCE(dc.primary_date_utc, r.last_updated_utc) AS spotlight_date_utc,
       COALESCE(dc.primary_date_field, 'Last_Updated') AS spotlight_date_source_field,
       CASE WHEN dc.primary_date_field IS NOT NULL THEN 'raw_date_candidates' ELSE 'raw_records' END AS spotlight_date_source_table,
       COALESCE(dc.primary_raw_value, r.last_updated_raw) AS spotlight_date_raw_value,
       COALESCE(dc.primary_parse_method, 'native_epoch_microseconds') AS spotlight_date_parse_method,
       COALESCE(NULLIF(dc.date_type_summary,''), 'metadata_seen_or_index_updated') AS spotlight_date_type,
       COALESCE(dc.date_source_fields, 'Last_Updated') AS spotlight_date_source_fields,
       COALESCE(dc.date_parse_methods, 'native_epoch_microseconds') AS spotlight_date_parse_methods,
       COALESCE(dc.date_candidate_count, CASE WHEN COALESCE(r.last_updated_utc,'')<>'' THEN 1 ELSE 0 END) AS spotlight_date_candidate_count,
       COALESCE(dc.date_source_evidence, 'Last_Updated=' || COALESCE(r.last_updated_raw,'') || ' -> ' || COALESCE(r.last_updated_utc,'')) AS spotlight_date_source_evidence,
       'Validate against raw_date_candidates.field_name/field_value/parsed_utc/parse_method or raw_records.last_updated_raw/last_updated_utc for this Store-V2 record.' AS date_validation_hint,
       'CoreSpotlight date provenance. Last_Updated is metadata/index timing unless another decoded field supports created/modified/accessed/user-activity semantics.' AS interpretation_note
FROM raw_records r
LEFT JOIN dc ON dc.source_id=r.source_id
            AND dc.store_guid=r.store_guid
            AND dc.source_db=r.source_db
            AND dc.inode_num=r.inode_num
            AND COALESCE(dc.store_id,'')=COALESCE(r.store_id,'')
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_artifacts;
CREATE VIEW vw_ios_artifacts AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,content_type,last_updated_utc,confidence
FROM artifacts
WHERE store_guid LIKE 'ios_%' OR source_id IN (SELECT source_id FROM raw_records WHERE source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%');


DROP VIEW IF EXISTS vw_ios_ffs_file_inventory;
CREATE VIEW vw_ios_ffs_file_inventory AS
SELECT ios_file_id,source_id,normalized_path,original_zip_entry,file_name,extension,size_bytes,zip_modified_utc,
       protection_class_hint,app_container_hint,domain_hint,is_directory,sha256_status,inventory_notes,created_utc
FROM ios_ffs_file_inventory
ORDER BY normalized_path;

DROP VIEW IF EXISTS vw_ios_database_artifact_inventory;
CREATE VIEW vw_ios_database_artifact_inventory AS
SELECT ios_db_id,source_id,normalized_path,original_zip_entry,database_name,database_category,app_hint,
       protection_class_hint,size_bytes,zip_modified_utc,parse_status,record_inventory_status,notes,extracted_path,created_utc
FROM ios_app_database_inventory
ORDER BY database_category,app_hint,normalized_path;

DROP VIEW IF EXISTS vw_ios_app_database_record_inventory;
CREATE VIEW vw_ios_app_database_record_inventory AS
SELECT ios_record_inventory_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,row_count,sample_columns,record_category,parse_status,notes,created_utc
FROM ios_app_database_record_inventory
ORDER BY database_category,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_app_database_record_summary;
CREATE VIEW vw_ios_app_database_record_summary AS
SELECT database_category,app_hint,record_category,parse_status,COUNT(*) AS table_count,
       SUM(COALESCE(row_count,0)) AS total_rows,MIN(database_name) AS first_database,MAX(database_name) AS last_database
FROM ios_app_database_record_inventory
GROUP BY database_category,app_hint,record_category,parse_status
ORDER BY database_category,record_category;

DROP VIEW IF EXISTS vw_ios_app_parsed_records;
CREATE VIEW vw_ios_app_parsed_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc
FROM ios_app_parsed_records
ORDER BY database_category,record_category,record_timestamp_utc,database_name,table_name,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_app_parsed_record_summary;
CREATE VIEW vw_ios_app_parsed_record_summary AS
SELECT database_category,app_hint,record_category,parse_status,COUNT(*) AS parsed_record_count,
       COUNT(DISTINCT database_name) AS database_count,
       MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,
       MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
       MIN(database_name) AS first_database,
       MAX(database_name) AS last_database
FROM ios_app_parsed_records
GROUP BY database_category,app_hint,record_category,parse_status
ORDER BY database_category,record_category;

DROP VIEW IF EXISTS vw_ios_apple_messages_parsed_records;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_apple_messages_parsed_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc
FROM ios_app_parsed_records
WHERE database_category='APPLE_MESSAGES' OR lower(database_name) IN ('sms.db','chat.db') OR lower(database_normalized_path) LIKE '%/sms.db'
ORDER BY record_timestamp_utc,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_apple_messages_parsed_summary;
CREATE VIEW vw_ios_apple_messages_parsed_summary AS
SELECT record_category,parse_status,COUNT(*) AS parsed_record_count,COUNT(DISTINCT ios_db_id) AS database_count,
       MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
       COUNT(NULLIF(contact_or_participant,'')) AS records_with_contact_or_handle,
       COUNT(NULLIF(file_path,'')) AS records_with_file_path,
       COUNT(NULLIF(text_snippet,'')) AS records_with_text_or_metadata,
       MIN(database_normalized_path) AS first_database_path,MAX(database_normalized_path) AS last_database_path
FROM vw_ios_apple_messages_parsed_records
GROUP BY record_category,parse_status;


DROP VIEW IF EXISTS vw_ios_whatsapp_parsed_records;
CREATE VIEW vw_ios_whatsapp_parsed_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,created_utc
FROM ios_app_parsed_records
WHERE database_category='WHATSAPP'
   OR lower(database_normalized_path) LIKE '%group.net.whatsapp%'
   OR lower(database_normalized_path) LIKE '%/whatsapp/%'
   OR lower(database_name) IN ('chatstorage.sqlite','contactsv2.sqlite','callhistory.sqlite')
ORDER BY record_timestamp_utc,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_whatsapp_parsed_summary;
CREATE VIEW vw_ios_whatsapp_parsed_summary AS
SELECT record_category,parse_status,COUNT(*) AS parsed_record_count,COUNT(DISTINCT ios_db_id) AS database_count,
       MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
       COUNT(NULLIF(contact_or_participant,'')) AS records_with_contact_or_jid,
       COUNT(NULLIF(file_path,'')) AS records_with_media_or_file_path,
       COUNT(NULLIF(text_snippet,'')) AS records_with_text_or_metadata,
       MIN(database_normalized_path) AS first_database_path,MAX(database_normalized_path) AS last_database_path
FROM vw_ios_whatsapp_parsed_records
GROUP BY record_category,parse_status;

DROP VIEW IF EXISTS vw_ios_spotlight_referenced_paths;
CREATE VIEW vw_ios_spotlight_referenced_paths AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,field_value,LOWER(field_value) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), extracted AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,field_value,
         CASE
           WHEN v LIKE 'file:///private/var/%' THEN substr(field_value,8)
           WHEN v LIKE 'file:///var/%' THEN '/private' || substr(field_value,8)
           WHEN v LIKE '/private/var/%' THEN field_value
           WHEN v LIKE '/var/%' THEN '/private' || field_value
           WHEN instr(v,'/private/var/')>0 THEN substr(field_value,instr(v,'/private/var/'))
           WHEN instr(v,'/var/mobile/')>0 THEN '/private' || substr(field_value,instr(v,'/var/mobile/'))
           ELSE ''
         END AS extracted_path,
         CASE
           WHEN v LIKE 'file:%' OR v LIKE '%/private/var/%' OR v LIKE '%/var/mobile/%' THEN 'IOS_FILE_PATH_OR_FILE_URL'
           WHEN v LIKE '%http://%' OR v LIKE '%https://%' THEN 'WEB_URL_NO_LOCAL_PATH'
           ELSE 'NON_PATH_REFERENCE'
         END AS reference_type
  FROM probes
)
SELECT raw_kv_id AS reference_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,
       substr(field_value,1,2000) AS raw_reference_value,
       reference_type,
       CASE WHEN extracted_path<>'' THEN lower(replace(replace(replace(extracted_path,'file://',''),'%20',' '),'\','/')) ELSE '' END AS normalized_ios_path,
       CASE WHEN extracted_path<>'' THEN 'MEDIUM_STRING_PROBE_PATH' ELSE 'LOW_NO_LOCAL_PATH' END AS confidence,
       'Spotlight string probe reference; formal CoreSpotlight property mapping remains parser roadmap work' AS notes
FROM extracted
WHERE reference_type<>'NON_PATH_REFERENCE';

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_candidates;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_missing_from_ffs_candidates AS
WITH refs AS (
  SELECT * FROM vw_ios_spotlight_referenced_paths WHERE COALESCE(normalized_ios_path,'')<>''
), files AS (
  SELECT source_id,normalized_path,file_name,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,'full_inventory' AS lookup_source
  FROM ios_ffs_file_inventory
  UNION ALL
  SELECT l.source_id,l.normalized_path,l.file_name,l.size_bytes,l.zip_modified_utc,l.protection_class_hint,l.app_container_hint,l.domain_hint,COALESCE(NULLIF(l.lookup_source,''),'slim_path_lookup') AS lookup_source
  FROM ios_ffs_path_lookup l
  WHERE NOT EXISTS (SELECT 1 FROM ios_ffs_file_inventory f WHERE f.source_id=l.source_id LIMIT 1)
), lookup_sources AS (
  SELECT DISTINCT source_id FROM files
), ctx AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         substr(MAX(field_value),1,1800) AS spotlight_text_context_sample
  FROM raw_key_values
  WHERE field_name='__spotlight_investigator_text_context'
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.reference_id,r.source_id,r.store_guid,r.source_db,r.inode_num,r.store_id,r.parent_inode_num,r.field_name,
       r.raw_reference_value,r.reference_type,r.normalized_ios_path,
       COALESCE(ctx.spotlight_text_context_sample,'') AS spotlight_text_context_sample,
       CASE WHEN COALESCE(ctx.spotlight_text_context_sample,'')<>'' THEN 'TEXT_CONTEXT_RECOVERED_FROM_SAME_SPOTLIGHT_RECORD' ELSE 'NO_TEXT_CONTEXT_RECOVERED_IN_COMPACT_MODE' END AS spotlight_text_context_status,
       COALESCE(f.file_name,'') AS matched_file_name,COALESCE(f.size_bytes,0) AS matched_size_bytes,COALESCE(f.zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.protection_class_hint,'') AS matched_protection_class,COALESCE(f.app_container_hint,'') AS matched_app_container,COALESCE(f.domain_hint,'') AS matched_domain,
       COALESCE(f.lookup_source,'') AS ffs_lookup_source,
       'FFS_LOOKUP_AVAILABLE' AS ffs_lookup_status,
       'SPOTLIGHT_ONLY_FILE_MISSING_OR_UNRESOLVED' AS residency_status,
       'MEDIUM_PATH_ABSENT_FROM_FFS_LOOKUP' AS confidence,
       'Missing means absent from the full FFS inventory or the slim FFS path lookup available in this case. The text_context_sample is recovered from the same Spotlight record to help assess investigative value; absence from FFS does not by itself prove user deletion or app-level deletion.' AS interpretation_note
FROM refs r
JOIN lookup_sources ls ON ls.source_id=r.source_id
LEFT JOIN files f ON f.source_id=r.source_id AND f.normalized_path=r.normalized_ios_path
LEFT JOIN ctx ON ctx.source_id=r.source_id AND ctx.store_guid=r.store_guid AND ctx.source_db=r.source_db AND ctx.inode_num=r.inode_num AND COALESCE(ctx.store_id,'')=COALESCE(r.store_id,'')
WHERE f.normalized_path IS NULL;

DROP VIEW IF EXISTS vw_ios_spotlight_missing_from_ffs_summary;
CREATE VIEW vw_ios_spotlight_missing_from_ffs_summary AS
SELECT source_id,store_guid,source_db,field_name,reference_type,spotlight_text_context_status,ffs_lookup_status,
       COUNT(*) AS missing_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_object_count,
       COUNT(DISTINCT normalized_ios_path) AS distinct_missing_path_count,
       MIN(normalized_ios_path) AS min_missing_path_sample,
       MAX(normalized_ios_path) AS max_missing_path_sample,
       substr(MAX(spotlight_text_context_sample),1,1800) AS spotlight_text_context_sample,
       'Compact normal-mode Missing From FFS summary. Full missing candidate rows are visible in the GUI view and support exports; this summary is safe for default investigator export.' AS interpretation_note
FROM vw_ios_spotlight_missing_from_ffs_candidates
GROUP BY source_id,store_guid,source_db,field_name,reference_type,spotlight_text_context_status,ffs_lookup_status;


DROP VIEW IF EXISTS vw_ios_spotlight_text_context_review;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_text_context_review AS
WITH base AS (
  SELECT kv.raw_kv_id,
         COALESCE(rr.raw_record_id,0) AS raw_record_id,
         kv.source_id,
         kv.store_guid,
         kv.source_db,
         kv.inode_num AS spotlight_inode_or_object_id,
         kv.store_id AS spotlight_store_id,
         kv.parent_inode_num,
         rr.file_name,
         rr.display_name,
         rr.content_type,
         rr.last_updated_utc,
         kv.field_value AS spotlight_text_context_sample,
         lower(COALESCE(kv.field_value,'')) AS v,
         lower(COALESCE(rr.content_type,'')) AS ct
  FROM raw_key_values kv
  LEFT JOIN raw_records rr ON rr.source_id=kv.source_id AND rr.store_guid=kv.store_guid AND rr.source_db=kv.source_db AND rr.inode_num=kv.inode_num AND COALESCE(rr.store_id,'')=COALESCE(kv.store_id,'')
  WHERE kv.field_name='__spotlight_investigator_text_context'
), labeled AS (
  SELECT *,
       CASE
         WHEN ct='public.message' OR v LIKE '%kmditemmessageservice%' OR v LIKE '%/sms/attachments/%' OR v LIKE '%com.apple.mobilesms%' THEN 'MESSAGE_OR_ATTACHMENT_CONTEXT'
         WHEN ct='public.email-message' OR v LIKE '%com_apple_mail_%' OR v LIKE '%kmditememail%' OR v LIKE '%from:%' OR v LIKE '%to:%' THEN 'MAIL_OR_EMAIL_CONTEXT'
         WHEN v LIKE '%_kmditembundleid=net.whatsapp.whatsapp%' OR v LIKE '%_kmditemexternalid=net.whatsapp.whatsapp%' OR v LIKE '%_kmditemdomainidentifier=net.whatsapp%' OR v LIKE '%_kmditemalternatenames=whatsapp%' THEN 'WHATSAPP_APP_OR_CHAT_CONTEXT'
         WHEN v LIKE '%_kmditembundleid=org.whispersystems.signal%' OR v LIKE '%_kmditemexternalid=org.whispersystems.signal%' OR v LIKE '%_kmditemdomainidentifier=org.whispersystems.signal%' OR v LIKE '%signal.messenger%' THEN 'SIGNAL_APP_OR_CHAT_CONTEXT'
         WHEN v LIKE '%_kmditembundleid=ph.telegra.telegraph%' OR v LIKE '%_kmditemexternalid=ph.telegra.telegraph%' OR v LIKE '%telegram.messenger%' OR v LIKE '%org.telegram%' THEN 'TELEGRAM_APP_OR_CHAT_CONTEXT'
         WHEN v LIKE '%whatsapp group%' OR v LIKE '%chat.whatsapp.com%' OR v LIKE '%wa.me/%' OR v LIKE '%api.whatsapp.com%' THEN 'WHATSAPP_LINK_OR_TEXT_MENTION'
         WHEN v LIKE '%t.me/%' OR v LIKE '%telegram.me/%' THEN 'TELEGRAM_LINK_OR_TEXT_MENTION'
         WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' OR v LIKE '%kmditemcontenturl%' THEN 'URL_OR_WEB_CONTEXT'
         WHEN ct='kspotlightitemtypecall' OR v LIKE '%tel://%' OR v LIKE '%callbackurl=tel%' THEN 'CALL_LOG_OR_PHONE_CONTEXT'
         WHEN ct='public.contact' OR v LIKE '%kmditemphonenumbers%' OR v LIKE '%contact%' THEN 'CONTACT_CONTEXT'
         WHEN ct='public.calendar-event' OR v LIKE '%calendar%' OR v LIKE '%event%' OR v LIKE '%.ics%' THEN 'CALENDAR_OR_EVENT_CONTEXT'
         WHEN ct LIKE 'public.%image%' OR ct IN ('public.jpeg','public.heic','public.png','com.compuserve.gif') THEN 'PHOTO_OR_MEDIA_CONTEXT'
         WHEN ct LIKE '%movie%' OR ct LIKE '%video%' OR ct IN ('public.mpeg-4','public.3gpp') THEN 'VIDEO_OR_MEDIA_CONTEXT'
         WHEN v LIKE '%/var/mobile/%' OR v LIKE '%file://%' THEN 'LOCAL_FILE_OR_PATH_CONTEXT'
         WHEN v LIKE '%@%' THEN 'ACCOUNT_OR_IDENTIFIER_CONTEXT'
         ELSE 'GENERAL_SPOTLIGHT_TEXT_CONTEXT'
       END AS text_context_category
  FROM base
), scored AS (
  SELECT *,
       CASE
         WHEN text_context_category IN ('MESSAGE_OR_ATTACHMENT_CONTEXT','MAIL_OR_EMAIL_CONTEXT') THEN 'HIGH_SPOTLIGHT_TEXT_VALUE'
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 'HIGH_APP_ATTRIBUTION_VALUE'
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 'MEDIUM_CHAT_LINK_OR_TEXT_VALUE'
         WHEN text_context_category IN ('URL_OR_WEB_CONTEXT','CALL_LOG_OR_PHONE_CONTEXT','CONTACT_CONTEXT','CALENDAR_OR_EVENT_CONTEXT','LOCAL_FILE_OR_PATH_CONTEXT','ACCOUNT_OR_IDENTIFIER_CONTEXT') THEN 'MEDIUM_SPOTLIGHT_TEXT_VALUE'
         WHEN text_context_category IN ('PHOTO_OR_MEDIA_CONTEXT','VIDEO_OR_MEDIA_CONTEXT') THEN 'MEDIUM_MEDIA_CONTEXT_VALUE'
         ELSE 'LOW_GENERAL_TEXT_VALUE'
       END AS review_priority,
       CASE
         WHEN text_context_category='MESSAGE_OR_ATTACHMENT_CONTEXT' THEN 1
         WHEN text_context_category='MAIL_OR_EMAIL_CONTEXT' THEN 2
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 3
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 9
         WHEN text_context_category='URL_OR_WEB_CONTEXT' THEN 4
         WHEN text_context_category='CALL_LOG_OR_PHONE_CONTEXT' THEN 5
         WHEN text_context_category='CONTACT_CONTEXT' THEN 6
         WHEN text_context_category='CALENDAR_OR_EVENT_CONTEXT' THEN 7
         WHEN text_context_category='LOCAL_FILE_OR_PATH_CONTEXT' THEN 8
         WHEN text_context_category='ACCOUNT_OR_IDENTIFIER_CONTEXT' THEN 10
         WHEN text_context_category IN ('PHOTO_OR_MEDIA_CONTEXT','VIDEO_OR_MEDIA_CONTEXT') THEN 10
         ELSE 20
       END AS review_priority_sort,
       CASE
         WHEN text_context_category='MESSAGE_OR_ATTACHMENT_CONTEXT' THEN 'Same-record Spotlight context indicates message, SMS/RCS/iMessage, or attachment evidence; prioritize for communications review and Missing From FFS triage.'
         WHEN text_context_category='MAIL_OR_EMAIL_CONTEXT' THEN 'Same-record Spotlight context indicates mail/email content or identifiers; prioritize for communication and account review.'
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 'Same-record Spotlight context contains explicit bundle/domain/external-id evidence for a chat application; prioritize for app attribution and indexed-only/deleted candidate review.'
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 'Same-record Spotlight context contains a chat-app link or textual mention only; review as possible communication/link evidence, not as installed-app attribution by itself.'
         WHEN text_context_category='URL_OR_WEB_CONTEXT' THEN 'Same-record Spotlight context includes URL/web content; review for browsing, shared links, or app deep-link evidence.'
         WHEN text_context_category='CALL_LOG_OR_PHONE_CONTEXT' THEN 'Same-record Spotlight context indicates call/phone callback evidence; correlate with CallHistory if support parsing is enabled.'
         WHEN text_context_category='CONTACT_CONTEXT' THEN 'Same-record Spotlight context indicates contact/person/account metadata.'
         WHEN text_context_category='CALENDAR_OR_EVENT_CONTEXT' THEN 'Same-record Spotlight context indicates calendar or event-like metadata.'
)VSGUI",
        R"VSGUI(         WHEN text_context_category IN ('PHOTO_OR_MEDIA_CONTEXT','VIDEO_OR_MEDIA_CONTEXT') THEN 'Same-record Spotlight context indicates media; review with dates/path context before drawing usage conclusions.'
         ELSE 'General same-record Spotlight text context retained in compact normal mode.'
       END AS text_context_reason
  FROM labeled
)
SELECT raw_kv_id,raw_record_id,source_id,store_guid,source_db,spotlight_inode_or_object_id,spotlight_store_id,parent_inode_num,
       file_name,display_name,content_type,last_updated_utc,text_context_category,review_priority,review_priority_sort,text_context_reason,
       CASE
         WHEN text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT') THEN 'EXPLICIT_CHAT_APP_BUNDLE_DOMAIN_OR_EXTERNAL_ID'
         WHEN text_context_category IN ('WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION') THEN 'CHAT_LINK_OR_TEXT_MENTION_ONLY_NOT_APP_ATTRIBUTION'
         WHEN text_context_category='MESSAGE_OR_ATTACHMENT_CONTEXT' THEN 'APPLE_MESSAGE_CONTENT_TYPE_OR_MOBILESMS_FIELD'
         WHEN text_context_category='MAIL_OR_EMAIL_CONTEXT' THEN 'MAIL_CONTENT_TYPE_OR_MAIL_SEARCH_INDEXER_FIELD'
         ELSE 'CATEGORY_BY_CONTENT_TYPE_FIELD_OR_VALUE_PATTERN'
       END AS classification_evidence,
       spotlight_text_context_sample,
       'Compact same-record Spotlight text context retained in normal iOS mode; this is not a full raw property dump. Use raw_kv_id/raw_record_id/source_db to validate against the local SQLite database.' AS interpretation_note
FROM scored;

DROP VIEW IF EXISTS vw_ios_spotlight_high_value_text_context_review;
CREATE VIEW vw_ios_spotlight_high_value_text_context_review AS
SELECT *
FROM vw_ios_spotlight_text_context_review
WHERE review_priority_sort <= 9
ORDER BY review_priority_sort,last_updated_utc DESC,raw_record_id DESC;

DROP VIEW IF EXISTS vw_ios_spotlight_text_context_priority_summary;
CREATE VIEW vw_ios_spotlight_text_context_priority_summary AS
SELECT text_context_category,review_priority,review_priority_sort,
       COUNT(*) AS text_context_record_count,
       COUNT(DISTINCT source_id || ':' || store_guid || ':' || COALESCE(spotlight_inode_or_object_id,'') || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       COUNT(NULLIF(last_updated_utc,'')) AS rows_with_last_updated,
       MIN(last_updated_utc) AS earliest_last_updated_utc,
       MAX(last_updated_utc) AS latest_last_updated_utc,
       substr(MIN(spotlight_text_context_sample),1,1000) AS min_text_context_sample,
       substr(MAX(spotlight_text_context_sample),1,1000) AS max_text_context_sample,
       MIN(classification_evidence) AS classification_evidence,
       MIN(text_context_reason) AS text_context_reason,
       'Priority summary for compact same-record Spotlight text retained in normal iOS mode. Priorities are triage labels, not final Apple schema classifications.' AS interpretation_note
FROM vw_ios_spotlight_text_context_review
GROUP BY text_context_category,review_priority,review_priority_sort;

DROP VIEW IF EXISTS vw_ios_spotlight_chat_app_attribution_summary;
CREATE VIEW vw_ios_spotlight_chat_app_attribution_summary AS
SELECT text_context_category,review_priority,classification_evidence,
       COUNT(*) AS context_record_count,
       COUNT(DISTINCT source_id || ':' || store_guid || ':' || COALESCE(spotlight_inode_or_object_id,'') || ':' || COALESCE(spotlight_store_id,'')) AS distinct_spotlight_object_count,
       MIN(last_updated_utc) AS earliest_last_updated_utc,
       MAX(last_updated_utc) AS latest_last_updated_utc,
       substr(MIN(spotlight_text_context_sample),1,1000) AS min_text_context_sample,
       substr(MAX(spotlight_text_context_sample),1,1000) AS max_text_context_sample,
       MIN(text_context_reason) AS text_context_reason,
       'V0_9_26 separates explicit chat-app bundle/domain/external-id attribution from plain keyword/link mentions so words like Signal Hill do not inflate Signal app evidence.' AS interpretation_note
FROM vw_ios_spotlight_text_context_review
WHERE text_context_category IN ('WHATSAPP_APP_OR_CHAT_CONTEXT','SIGNAL_APP_OR_CHAT_CONTEXT','TELEGRAM_APP_OR_CHAT_CONTEXT','WHATSAPP_LINK_OR_TEXT_MENTION','TELEGRAM_LINK_OR_TEXT_MENTION')
GROUP BY text_context_category,review_priority,classification_evidence;

DROP VIEW IF EXISTS vw_ios_spotlight_human_text_values;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_human_text_values AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,store_path,inode_num,store_id,parent_inode_num,field_name,field_value,
         LOWER(COALESCE(field_value,'')) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), labeled AS (
  SELECT *,
    CASE
      WHEN v LIKE '%file://%' OR v LIKE '%/private/var/mobile/%' OR v LIKE '%/var/mobile/%' THEN 'FILE_PATH_OR_ATTACHMENT'
      WHEN v LIKE '%zoom.%' OR v LIKE '%meet.google.%' OR v LIKE '%teams.microsoft.%' OR v LIKE '%webex.%' THEN 'MEETING_OR_CONFERENCE'
      WHEN v LIKE '%.ics%' OR v LIKE '%text/calendar%' OR v LIKE '%vevent%' OR v LIKE '%calendar.google.com%' OR v LIKE '%/calendar/%' THEN 'CALENDAR_OR_INVITATION'
      WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'WEB_OR_URL'
      WHEN v LIKE 'from:%' OR v LIKE 'to:%' OR v LIKE 'cc:%' OR v LIKE '%mailto:%' OR (v LIKE '%@%' AND v NOT LIKE '%http://%' AND v NOT LIKE '%https://%') THEN 'EMAIL_OR_ACCOUNT_TEXT'
      WHEN v LIKE '%net.whatsapp.whatsapp%' OR v LIKE '%chat.whatsapp.com%' OR v LIKE '%wa.me/%' OR v LIKE '%api.whatsapp.com%' OR v LIKE '%whatsapp group%' THEN 'WHATSAPP_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.whispersystems.signal%' OR v LIKE '%signal.messenger%' OR v LIKE '%signal.org/%' THEN 'SIGNAL_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.telegram%' OR v LIKE '%telegram.messenger%' OR v LIKE '%t.me/%' OR v LIKE '%telegram.me/%' THEN 'TELEGRAM_TEXT_OR_REFERENCE'
      ELSE 'OTHER_HUMAN_READABLE_TEXT'
    END AS human_text_category,
    TRIM(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(COALESCE(field_value,''),
      char(13),' '), char(10),' '), char(9),' '), '<br>',' '), '<br/>',' '), '<br />',' '),
      '&amp;','&'), '&lt;','<'), '&gt;','>'), '&nbsp;',' '), '&quot;','"')) AS readable_text
  FROM probes
)
SELECT l.raw_kv_id,COALESCE(r.raw_record_id,0) AS raw_record_id,l.source_id,l.store_guid,l.source_db,l.inode_num,l.store_id,l.parent_inode_num,l.field_name,
       l.human_text_category,
       LENGTH(COALESCE(l.field_value,'')) AS original_value_length,
       substr(l.readable_text,1,3000) AS readable_text_sample,
       CASE
         WHEN l.human_text_category IN ('FILE_PATH_OR_ATTACHMENT','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION','WEB_OR_URL','EMAIL_OR_ACCOUNT_TEXT') THEN 'HIGH_HUMAN_REVIEW_VALUE'
         WHEN LENGTH(l.readable_text)>=24 THEN 'MEDIUM_HUMAN_REVIEW_VALUE'
         ELSE 'LOW_HUMAN_REVIEW_VALUE'
       END AS review_priority,
       'Generic iOS CoreSpotlight text recovery; formal CoreSpotlight property names/dbStr maps remain a later parser phase.' AS interpretation_note
FROM labeled l
LEFT JOIN (
  SELECT source_id,store_guid,source_db,inode_num,COALESCE(store_id,'') AS store_id_key,MIN(raw_record_id) AS raw_record_id
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,inode_num,COALESCE(store_id,'')
) r ON r.source_id=l.source_id AND r.store_guid=l.store_guid AND r.source_db=l.source_db AND r.inode_num=l.inode_num AND r.store_id_key=COALESCE(l.store_id,'')
WHERE LENGTH(l.readable_text)>=4;

DROP VIEW IF EXISTS vw_ios_spotlight_human_text_rollup;
CREATE VIEW vw_ios_spotlight_human_text_rollup AS
WITH text_values AS (
  SELECT v.*, r.last_updated_utc, r.record_state
  FROM vw_ios_spotlight_human_text_values v
  LEFT JOIN raw_records r ON r.raw_record_id=v.raw_record_id
)
SELECT raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT human_text_category) AS distinct_text_category_count,
       GROUP_CONCAT(DISTINCT human_text_category) AS human_text_categories,
       MAX(CASE WHEN review_priority='HIGH_HUMAN_REVIEW_VALUE' THEN 1 ELSE 0 END) AS has_high_review_value_text,
       MIN(NULLIF(last_updated_utc,'')) AS last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       substr(GROUP_CONCAT(field_name || '=' || readable_text_sample, ' || '),1,6000) AS readable_text_rollup_sample,
       'Record-level human-readable text rollup from iOS CoreSpotlight string probes.' AS interpretation_note
FROM text_values
GROUP BY raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num;


DROP VIEW IF EXISTS vw_ios_spotlight_investigative_items_with_dates;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_investigative_items_with_dates AS
SELECT v.raw_kv_id,
       v.raw_record_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.original_value_length,
       v.readable_text_sample,
       v.review_priority,
       dp.spotlight_date_utc,
       dp.spotlight_date_source_field,
       dp.spotlight_date_source_table,
       dp.spotlight_date_raw_value,
       dp.spotlight_date_parse_method,
       dp.spotlight_date_type,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%creation%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%created%' THEN 'created_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modification%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modified%' THEN 'modified_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%access%' THEN 'accessed_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%open%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'metadata_seen_or_index_updated'
         ELSE 'unclassified_spotlight_date_candidate'
       END AS spotlight_date_semantic_class,
       dp.spotlight_date_source_evidence,
       dp.date_validation_hint,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'Do not report as created/modified/accessed/opened. This is CoreSpotlight metadata/index update timing unless another decoded field supports activity semantics.'
         WHEN COALESCE(dp.spotlight_date_source_field,'')<>'' THEN 'Report only with the listed raw Spotlight source field, raw value, parse method, and validation hint.'
         ELSE 'No direct Spotlight date was recovered for this text value.'
       END AS date_reporting_caution,
       'Spotlight/CoreSpotlight extracted text item with attached date provenance where directly linkable by raw_record_id. FFS/app database data is supporting context only.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
LEFT JOIN vw_ios_spotlight_date_provenance dp ON dp.raw_record_id=v.raw_record_id;

DROP VIEW IF EXISTS vw_ios_spotlight_date_field_summary;
CREATE VIEW vw_ios_spotlight_date_field_summary AS
WITH dates AS (
  SELECT source_id, store_guid, source_db, field_name, parse_method, date_type,
         parsed_utc, field_value, inode_num, store_id,
         CASE
           WHEN lower(COALESCE(field_name,'')) LIKE '%creation%' OR lower(COALESCE(field_name,'')) LIKE '%created%' THEN 'created_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%modification%' OR lower(COALESCE(field_name,'')) LIKE '%modified%' THEN 'modified_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%access%' THEN 'accessed_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%open%' OR lower(COALESCE(field_name,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
           WHEN lower(COALESCE(field_name,''))='last_updated' THEN 'metadata_seen_or_index_updated'
           ELSE 'unclassified_spotlight_date_candidate'
         END AS spotlight_date_semantic_class
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
)
SELECT source_id, store_guid, source_db, field_name AS spotlight_date_source_field,
       spotlight_date_semantic_class, COALESCE(date_type,'') AS raw_date_type,
       COALESCE(parse_method,'') AS parse_method,
       COUNT(*) AS date_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_record_count,
       MIN(parsed_utc) AS earliest_parsed_utc,
       MAX(parsed_utc) AS latest_parsed_utc,
       substr(MIN(field_value),1,500) AS min_raw_value_sample,
       substr(MAX(field_value),1,500) AS max_raw_value_sample,
       CASE
         WHEN spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'CoreSpotlight metadata/index update timing; do not report as created/modified/accessed/opened usage without another decoded field.'
         WHEN spotlight_date_semantic_class LIKE '%candidate' THEN 'Candidate semantic class inferred from Spotlight raw date field name; validate field meaning before reporting.'
         ELSE 'Unclassified Spotlight date candidate; validate against raw_date_candidates and source Store-V2 record.'
       END AS reporting_caution
FROM dates
GROUP BY source_id, store_guid, source_db, field_name, spotlight_date_semantic_class, date_type, parse_method;

DROP VIEW IF EXISTS vw_ios_spotlight_investigative_item_date_evidence;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_investigative_item_date_evidence AS
WITH date_evidence AS (
  SELECT raw_date_id, source_id, store_guid, source_db, store_path, inode_num, store_id,
         parent_inode_num, file_name, best_path,
         field_name AS spotlight_date_source_field,
         field_value AS spotlight_date_raw_value,
         parsed_utc AS spotlight_date_utc,
         parse_method AS spotlight_date_parse_method,
         date_type AS spotlight_date_type,
         association_status,
         association_confidence,
         CASE
           WHEN lower(COALESCE(field_name,'')) LIKE '%creation%' OR lower(COALESCE(field_name,'')) LIKE '%created%' THEN 'created_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%modification%' OR lower(COALESCE(field_name,'')) LIKE '%modified%' THEN 'modified_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%access%' THEN 'accessed_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%open%' OR lower(COALESCE(field_name,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
           WHEN lower(COALESCE(field_name,''))='last_updated' THEN 'metadata_seen_or_index_updated'
           ELSE 'unclassified_spotlight_date_candidate'
         END AS spotlight_date_semantic_class
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
)
SELECT v.raw_kv_id,
       v.raw_record_id,
       d.raw_date_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.review_priority,
       v.original_value_length,
       v.readable_text_sample,
       d.spotlight_date_utc,
       d.spotlight_date_source_field,
       'raw_date_candidates' AS spotlight_date_source_table,
       d.spotlight_date_raw_value,
       d.spotlight_date_parse_method,
       d.spotlight_date_type,
       d.spotlight_date_semantic_class,
       d.association_status AS date_association_status,
       d.association_confidence AS date_association_confidence,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(v.raw_kv_id AS TEXT),'') || '; field_name=' || COALESCE(v.field_name,'') AS value_validation_locator,
       'raw_date_candidates.raw_date_id=' || COALESCE(CAST(d.raw_date_id AS TEXT),'') || '; field_name=' || COALESCE(d.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(d.spotlight_date_raw_value,'') || '; parsed_utc=' || COALESCE(d.spotlight_date_utc,'') || '; parse_method=' || COALESCE(d.spotlight_date_parse_method,'') AS date_validation_locator,
       'source_db=' || COALESCE(v.source_db,'') || '; store_guid=' || COALESCE(v.store_guid,'') || '; raw_record_id=' || COALESCE(CAST(v.raw_record_id AS TEXT),'') || '; inode_or_object_id=' || COALESCE(v.inode_num,'') || '; store_id=' || COALESCE(v.store_id,'') AS spotlight_record_locator,
       CASE
         WHEN d.spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'Date is linked to this Spotlight record but represents CoreSpotlight metadata/index update timing unless a separately decoded field supports user activity.'
         WHEN d.spotlight_date_semantic_class IN ('created_date_candidate','modified_date_candidate','accessed_date_candidate','opened_or_used_date_candidate') THEN 'Date is directly linked to this recovered Spotlight value through the same Store-V2 record; validate raw field semantics before reporting as activity.'
         ELSE 'Date is directly linked to this recovered Spotlight value through the same Store-V2 record, but semantic meaning is not yet classified.'
       END AS date_reporting_caution,
       'Each row links one recovered human-readable Spotlight value to one raw Spotlight date candidate from the same Store-V2 record. Use validation locator fields to verify in the parsed raw tables and original store.db.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
JOIN date_evidence d ON d.source_id=v.source_id
                    AND d.store_guid=v.store_guid
                    AND d.source_db=v.source_db
                    AND d.inode_num=v.inode_num
                    AND COALESCE(d.store_id,'')=COALESCE(v.store_id,'');

)VSGUI"
    });

    execGuiSql(R"SQL(

DROP VIEW IF EXISTS vw_ios_spotlight_high_value_timeline;
CREATE VIEW vw_ios_spotlight_high_value_timeline AS
WITH base AS (
  SELECT * FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE review_priority IN ('HIGH_HUMAN_REVIEW_VALUE','MEDIUM_HUMAN_REVIEW_VALUE')
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.original_value_length,
       b.readable_text_sample,b.review_priority,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_source_table,
       b.spotlight_date_raw_value,b.spotlight_date_parse_method,b.spotlight_date_type,
       b.spotlight_date_semantic_class,b.date_validation_hint,b.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FILE_PATH_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       CASE
         WHEN b.spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'SPOTLIGHT_INDEX_TIME_WITH_VALUE_CONTEXT'
         WHEN b.spotlight_date_semantic_class LIKE '%candidate' THEN 'SPOTLIGHT_ACTIVITY_DATE_CANDIDATE_WITH_VALUE_CONTEXT'
         ELSE 'SPOTLIGHT_VALUE_WITH_UNCLASSIFIED_DATE_CONTEXT'
       END AS investigative_timeline_basis,
       'Spotlight-first high-value timeline. FFS and app database fields are context/corroboration only; the Spotlight value/date fields remain the primary evidence to validate.' AS interpretation_note
FROM base b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_file_reference_review;
CREATE VIEW vw_ios_spotlight_file_reference_review AS
WITH ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.readable_text_sample AS spotlight_file_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(f.residency_status,'NO_EXACT_FFS_PATH_LINK') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       CASE WHEN COALESCE(f.residency_status,'')='PRESENT_AS_FILE_IN_FFS' THEN 'SPOTLIGHT_PATH_PRESENT_IN_FFS_INVENTORY'
            WHEN COALESCE(f.residency_status,'')<>'' THEN f.residency_status
            ELSE 'SPOTLIGHT_FILE_REFERENCE_NO_EXACT_FFS_MATCH_IN_CURRENT_LINK_VIEW' END AS file_reference_status,
       'Spotlight/CoreSpotlight file/path reference with date provenance. FFS presence supports current-file existence only and is not proof of use or deletion by itself.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
WHERE b.human_text_category='FILE_PATH_OR_ATTACHMENT';

)SQL");

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_url_reference_review;
CREATE VIEW vw_ios_spotlight_url_reference_review AS
WITH vals AS (
  SELECT *, lower(COALESCE(readable_text_sample,'')) AS v
  FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION')
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT v.raw_kv_id,v.raw_record_id,v.source_id,v.store_guid,v.source_db,
       v.spotlight_inode_or_object_id,v.spotlight_store_id,v.parent_inode_num,
       v.spotlight_value_source_field,v.human_text_category,v.readable_text_sample AS spotlight_url_or_web_reference,
       CASE
         WHEN instr(v.v,'https://')>0 THEN substr(v.v,instr(v.v,'https://'),300)
         WHEN instr(v.v,'http://')>0 THEN substr(v.v,instr(v.v,'http://'),300)
         WHEN instr(v.v,'www.')>0 THEN substr(v.v,instr(v.v,'www.'),300)
         ELSE substr(v.v,1,300)
       END AS normalized_url_reference_sample,
       v.spotlight_date_utc,v.spotlight_date_source_field,v.spotlight_date_raw_value,
       v.spotlight_date_parse_method,v.spotlight_date_semantic_class,v.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       'Spotlight/CoreSpotlight URL/web-like reference with date provenance. Browser/app database fields are supporting context and not an exact value match unless separately validated.' AS interpretation_note
FROM vals v
LEFT JOIN app a ON a.candidate_id=v.raw_kv_id;

)SQL");

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_account_contact_reference_review;
CREATE VIEW vw_ios_spotlight_account_contact_reference_review AS
WITH app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.readable_text_sample AS spotlight_account_or_contact_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       'Spotlight/CoreSpotlight account/contact-like reference with date provenance. Treat as a Spotlight value first; app database context is family-level unless exact string matching is later added.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id
WHERE b.human_text_category IN ('EMAIL_OR_ACCOUNT_TEXT','WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE');

DROP VIEW IF EXISTS vw_ios_spotlight_decode_gap_summary;
CREATE VIEW vw_ios_spotlight_decode_gap_summary AS
WITH gaps AS (
  SELECT source_id,store_guid,source_db,decode_gap_status,last_updated_utc
  FROM vw_ios_spotlight_decode_gap_records
)
SELECT g.source_id,g.store_guid,g.source_db,g.decode_gap_status,
       COUNT(*) AS gap_record_count,
       MIN(NULLIF(g.last_updated_utc,'')) AS earliest_gap_last_updated_utc,
       MAX(NULLIF(g.last_updated_utc,'')) AS latest_gap_last_updated_utc,
       COALESCE(dc.raw_record_count,0) AS store_raw_record_count,
       COALESCE(dc.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(dc.human_text_value_count,0) AS human_text_value_count,
       COALESCE(dc.pct_records_with_human_text,'') AS pct_records_with_human_text,
       COALESCE(dc.decode_failures,0) AS native_decode_failures,
       COALESCE(dc.decode_status,'') AS native_decode_status,
       'Summary of Spotlight/CoreSpotlight records parsed at header level but lacking recovered key/value or human-readable text values. This is the primary native parser improvement target list.' AS interpretation_note
FROM gaps g
LEFT JOIN vw_ios_spotlight_decode_coverage_summary dc ON dc.source_id=g.source_id AND dc.store_guid=g.store_guid AND dc.source_db=g.source_db
GROUP BY g.source_id,g.store_guid,g.source_db,g.decode_gap_status;

)SQL");

    execGuiSqlParts({
        R"VSGUI(

DROP VIEW IF EXISTS vw_ios_spotlight_entity_review;
CREATE VIEW vw_ios_spotlight_entity_review AS
WITH base AS (
  SELECT b.*,
         lower(COALESCE(b.readable_text_sample,'')) AS lower_text
  FROM vw_ios_spotlight_investigative_items_with_dates b
  WHERE COALESCE(b.readable_text_sample,'')<>''
), typed AS (
  SELECT b.*,
         CASE
           WHEN b.human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION') THEN 'URL_OR_WEB_REFERENCE'
           WHEN b.human_text_category='FILE_PATH_OR_ATTACHMENT' THEN 'FILE_OR_ATTACHMENT_REFERENCE'
           WHEN b.human_text_category='EMAIL_OR_ACCOUNT_TEXT' THEN 'ACCOUNT_OR_EMAIL_REFERENCE'
           WHEN b.human_text_category IN ('WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE') THEN 'COMMUNICATION_APP_REFERENCE'
           WHEN b.human_text_category LIKE '%MESSAGE%' THEN 'MESSAGE_OR_COMMUNICATION_TEXT'
           ELSE 'OTHER_SPOTLIGHT_TEXT_REFERENCE'
         END AS entity_type
  FROM base b
), normalized AS (
  SELECT t.*,
         CASE
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'https://')>0 THEN substr(t.lower_text,instr(t.lower_text,'https://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'http://')>0 THEN substr(t.lower_text,instr(t.lower_text,'http://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'www.')>0 THEN substr(t.lower_text,instr(t.lower_text,'www.'),512)
           WHEN t.entity_type='FILE_OR_ATTACHMENT_REFERENCE' THEN replace(replace(replace(replace(t.lower_text,'file://',''),'<',''),'>',''),'\\','/')
           ELSE trim(t.lower_text)
         END AS normalized_entity_value
  FROM typed t
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,matched_record_category,matched_table_name,parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc,sample_parsed_value
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT n.raw_kv_id,
       n.raw_record_id,
       n.source_id,
       n.store_guid,
       n.source_db,
       n.spotlight_inode_or_object_id,
       n.spotlight_store_id,
       n.parent_inode_num,
       n.entity_type,
       n.human_text_category,
       n.review_priority,
       n.spotlight_value_source_field,
       n.normalized_entity_value,
       n.readable_text_sample,
       n.original_value_length,
       n.spotlight_date_utc,
       n.spotlight_date_source_field,
       n.spotlight_date_raw_value,
       n.spotlight_date_parse_method,
       n.spotlight_date_semantic_class,
       n.date_validation_hint,
       n.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FFS_LINK_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_LINK_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.matched_record_category,'') AS matched_record_category,
       COALESCE(a.matched_table_name,'') AS matched_table_name,
       COALESCE(a.sample_parsed_value,'') AS sample_app_db_value,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(n.raw_kv_id AS TEXT),'') || '; raw_records.raw_record_id=' || COALESCE(CAST(n.raw_record_id AS TEXT),'') || '; field_name=' || COALESCE(n.spotlight_value_source_field,'') AS reference_validation_locator,
       'raw_date_candidates.field_name=' || COALESCE(n.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(n.spotlight_date_raw_value,'') || '; parse_method=' || COALESCE(n.spotlight_date_parse_method,'') AS date_validation_locator,
       'Spotlight-first entity view. The entity/value and date columns originate from CoreSpotlight/Spotlight parsed records; app DB and FFS fields are corroborating context only.' AS interpretation_note
FROM normalized n
LEFT JOIN ffs f ON f.reference_id=n.raw_kv_id
LEFT JOIN app a ON a.candidate_id=n.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_entity_summary;
CREATE VIEW vw_ios_spotlight_entity_summary AS
SELECT entity_type,
       human_text_category,
       review_priority,
       store_guid,
       source_db,
       spotlight_value_source_field,
       spotlight_date_semantic_class,
       COUNT(*) AS entity_row_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT normalized_entity_value) AS distinct_normalized_entity_count,
       SUM(CASE WHEN ffs_residency_status='PRESENT_AS_FILE_IN_FFS' THEN 1 ELSE 0 END) AS ffs_present_context_count,
       SUM(CASE WHEN app_db_link_status LIKE 'PRESENT%' OR app_db_link_status LIKE '%PRESENT%' THEN 1 ELSE 0 END) AS app_db_present_context_count,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       MIN(substr(normalized_entity_value,1,240)) AS min_sample_entity,
       MAX(substr(normalized_entity_value,1,240)) AS max_sample_entity,
       'Spotlight entity summary. Counts are derived from recovered CoreSpotlight text/probe values and their Spotlight date provenance; app/FFS context is only supporting context.' AS interpretation_note
FROM vw_ios_spotlight_entity_review
GROUP BY entity_type,human_text_category,review_priority,store_guid,source_db,spotlight_value_source_field,spotlight_date_semantic_class;

DROP VIEW IF EXISTS vw_ios_spotlight_native_parser_targets;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_native_parser_targets AS
SELECT source_id,
       store_guid,
       source_db,
       'RECORDS_WITHOUT_RECOVERED_TEXT' AS parser_target_type,
       decode_gap_status AS target_name,
       gap_record_count AS target_count,
       store_raw_record_count AS store_raw_record_count,
       recovered_key_value_count AS recovered_key_value_count,
       human_text_value_count AS human_text_value_count,
       pct_records_with_human_text AS pct_records_with_human_text,
       native_decode_failures AS native_decode_failures,
       CASE WHEN gap_record_count>100000 THEN 'HIGH' WHEN gap_record_count>10000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Improve native CoreSpotlight property/dictionary/value decoding for records that parse at header level but do not yield recovered text/key-value rows.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_decode_gap_summary
UNION ALL
SELECT source_id,
       store_guid,
       source_db,
       'GENERIC_STRING_PROBE_FIELD' AS parser_target_type,
       field_name AS target_name,
       value_row_count AS target_count,
       NULL AS store_raw_record_count,
       value_row_count AS recovered_key_value_count,
       distinct_record_count AS human_text_value_count,
       '' AS pct_records_with_human_text,
       NULL AS native_decode_failures,
       CASE WHEN value_row_count>10000 THEN 'HIGH' WHEN value_row_count>1000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Map generic __native_core_probe_string_* fields back to real CoreSpotlight property names/types where possible.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_field_coverage_summary
WHERE field_decode_status='GENERIC_NATIVE_STRING_PROBE';

DROP VIEW IF EXISTS vw_ios_spotlight_decode_coverage_summary;
CREATE VIEW vw_ios_spotlight_decode_coverage_summary AS
WITH rr AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS raw_record_count,
         SUM(CASE WHEN COALESCE(last_updated_utc,'')<>'' THEN 1 ELSE 0 END) AS records_with_last_updated,
         MIN(NULLIF(last_updated_utc,'')) AS earliest_last_updated_utc,
         MAX(NULLIF(last_updated_utc,'')) AS latest_last_updated_utc
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), kv AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS recovered_key_value_count,
         COUNT(DISTINCT field_name) AS recovered_field_name_count,
         COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS records_with_recovered_values
  FROM raw_key_values
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), ht AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS human_text_value_count,
         COUNT(DISTINCT raw_record_id) AS records_with_human_text,
         COUNT(DISTINCT human_text_category) AS human_text_category_count
  FROM vw_ios_spotlight_human_text_values
  GROUP BY source_id,store_guid,source_db
), nd AS (
  SELECT source_id,store_guid,source_db,
         MAX(decode_mode) AS decode_mode,
         MAX(spotlight_version) AS spotlight_version,
         MAX(properties_count) AS native_property_count,
         MAX(categories_count) AS native_category_count,
         MAX(metadata_blocks) AS metadata_blocks,
         MAX(decompressed_blocks) AS decompressed_blocks,
         MAX(failures) AS decode_failures,
         MAX(status) AS decode_status,
         MAX(message) AS decode_message
  FROM native_decode_attempts
  GROUP BY source_id,store_guid,source_db
)
SELECT rr.source_id,rr.store_guid,rr.source_db,
       COALESCE(nd.decode_mode,'') AS decode_mode,
       COALESCE(nd.spotlight_version,0) AS spotlight_version,
       rr.raw_record_count,
       COALESCE(kv.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(kv.recovered_field_name_count,0) AS recovered_field_name_count,
       COALESCE(kv.records_with_recovered_values,0) AS records_with_recovered_values,
       COALESCE(ht.human_text_value_count,0) AS human_text_value_count,
       COALESCE(ht.records_with_human_text,0) AS records_with_human_text,
       COALESCE(ht.human_text_category_count,0) AS human_text_category_count,
       CASE WHEN rr.raw_record_count>0 THEN printf('%.2f', 100.0 * COALESCE(ht.records_with_human_text,0) / rr.raw_record_count) ELSE '0.00' END AS pct_records_with_human_text,
       COALESCE(nd.native_property_count,0) AS native_property_count,
       COALESCE(nd.native_category_count,0) AS native_category_count,
       COALESCE(nd.metadata_blocks,0) AS metadata_blocks,
       COALESCE(nd.decompressed_blocks,0) AS decompressed_blocks,
       COALESCE(nd.decode_failures,0) AS decode_failures,
       COALESCE(nd.decode_status,'NO_NATIVE_DECODE_ATTEMPT_ROW') AS decode_status,
       rr.earliest_last_updated_utc,rr.latest_last_updated_utc,
       CASE WHEN COALESCE(nd.native_property_count,0)=0 THEN 'PROPERTY_DICTIONARY_NOT_DECODED_GENERIC_PROBES_ONLY'
            WHEN COALESCE(kv.recovered_key_value_count,0)=0 THEN 'NO_KEY_VALUES_RECOVERED'
            ELSE 'GENERIC_TEXT_VALUES_RECOVERED' END AS spotlight_decode_interpretation,
       'Spotlight-first coverage view. App/FFS correlation is supporting context; this row measures native CoreSpotlight record/value recovery.' AS interpretation_note
FROM rr
LEFT JOIN kv ON kv.source_id=rr.source_id AND kv.store_guid=rr.store_guid AND kv.source_db=rr.source_db
LEFT JOIN ht ON ht.source_id=rr.source_id AND ht.store_guid=rr.store_guid AND ht.source_db=rr.source_db
LEFT JOIN nd ON nd.source_id=rr.source_id AND nd.store_guid=rr.store_guid AND nd.source_db=rr.source_db;

DROP VIEW IF EXISTS vw_ios_spotlight_field_coverage_summary;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_field_coverage_summary AS
SELECT source_id,store_guid,source_db,field_name,
       COUNT(*) AS value_row_count,
       COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS distinct_record_count,
       MIN(LENGTH(COALESCE(field_value,''))) AS min_value_length,
       MAX(LENGTH(COALESCE(field_value,''))) AS max_value_length,
       substr(MIN(COALESCE(field_value,'')),1,1000) AS min_sample_value,
       substr(MAX(COALESCE(field_value,'')),1,1000) AS max_sample_value,
       CASE WHEN field_name LIKE '__native_core_probe_string_%' THEN 'GENERIC_NATIVE_STRING_PROBE'
            WHEN field_name LIKE '__native_%' THEN 'GENERIC_NATIVE_FIELD'
            ELSE 'NAMED_SPOTLIGHT_FIELD' END AS field_decode_status,
       'Field coverage summary from recovered Spotlight key/value rows. Generic probe names indicate values recovered before formal property-name mapping.' AS interpretation_note
FROM raw_key_values
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(field_value,'')<>''
GROUP BY source_id,store_guid,source_db,field_name;

DROP VIEW IF EXISTS vw_ios_spotlight_text_category_summary;
CREATE VIEW vw_ios_spotlight_text_category_summary AS
SELECT human_text_category,review_priority,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT store_guid) AS store_count,
       MIN(original_value_length) AS min_original_value_length,
       MAX(original_value_length) AS max_original_value_length,
       substr(MIN(readable_text_sample),1,1000) AS min_sample_text,
       substr(MAX(readable_text_sample),1,1000) AS max_sample_text,
       'Spotlight recovered text category summary. Categories are triage labels over Spotlight text values, not final CoreSpotlight property names.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values
GROUP BY human_text_category,review_priority;

DROP VIEW IF EXISTS vw_ios_spotlight_record_review;
CREATE VIEW vw_ios_spotlight_record_review AS
WITH text_roll AS (
  SELECT raw_record_id,text_value_count,distinct_text_category_count,human_text_categories,has_high_review_value_text,readable_text_rollup_sample
  FROM vw_ios_spotlight_human_text_rollup
), date_one AS (
  SELECT raw_record_id,
         MAX(spotlight_date_utc) AS spotlight_date_utc,
         MAX(spotlight_date_source_field) AS spotlight_date_source_field,
         MAX(spotlight_date_source_table) AS spotlight_date_source_table,
         MAX(spotlight_date_raw_value) AS spotlight_date_raw_value,
         MAX(spotlight_date_parse_method) AS spotlight_date_parse_method,
         MAX(spotlight_date_type) AS spotlight_date_type,
         MAX(spotlight_date_source_evidence) AS spotlight_date_source_evidence,
         MAX(date_validation_hint) AS date_validation_hint,
         COUNT(*) AS collapsed_date_candidate_count
  FROM vw_ios_spotlight_date_provenance
  GROUP BY raw_record_id
)
SELECT r.raw_record_id,r.source_id,r.store_guid,r.source_db,r.inode_num AS spotlight_inode_or_object_id,r.store_id AS spotlight_store_id,r.parent_inode_num,
       COALESCE(dp.spotlight_date_utc,r.last_updated_utc) AS spotlight_date_utc,
       COALESCE(dp.spotlight_date_source_field,'Last_Updated') AS spotlight_date_source_field,
       COALESCE(dp.spotlight_date_source_table,'raw_records') AS spotlight_date_source_table,
       COALESCE(dp.spotlight_date_raw_value,r.last_updated_raw) AS spotlight_date_raw_value,
       COALESCE(dp.spotlight_date_parse_method,'native_epoch_microseconds') AS spotlight_date_parse_method,
       COALESCE(dp.spotlight_date_type,'metadata_seen_or_index_updated') AS spotlight_date_type,
       COALESCE(dp.spotlight_date_source_evidence,'Last_Updated=' || COALESCE(r.last_updated_raw,'') || ' -> ' || COALESCE(r.last_updated_utc,'')) AS spotlight_date_source_evidence,
       COALESCE(dp.date_validation_hint,'Validate against raw_records.last_updated_raw/last_updated_utc for this Store-V2 record.') AS spotlight_date_validation_hint,
       COALESCE(dp.collapsed_date_candidate_count,0) AS collapsed_date_candidate_count,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       COALESCE(t.text_value_count,0) AS spotlight_text_value_count,
       COALESCE(t.distinct_text_category_count,0) AS spotlight_text_category_count,
       COALESCE(t.human_text_categories,'') AS spotlight_text_categories,
       COALESCE(t.readable_text_rollup_sample,'') AS spotlight_text_rollup_sample,
       0 AS ffs_reference_count,
       0 AS ffs_present_reference_count,
       0 AS ffs_missing_or_unresolved_reference_count,
       0 AS app_db_candidate_count,
       0 AS app_db_present_candidate_count,
       0 AS app_db_unresolved_candidate_count,
       CASE WHEN COALESCE(t.has_high_review_value_text,0)=1 THEN 'HIGH_SPOTLIGHT_TEXT_VALUE'
            WHEN COALESCE(t.text_value_count,0)>0 THEN 'SPOTLIGHT_TEXT_VALUE'
            ELSE 'SPOTLIGHT_RECORD_NO_RECOVERED_TEXT' END AS spotlight_review_priority,
       CASE WHEN COALESCE(t.text_value_count,0)>0 THEN 'TEXT_VALUES_RECOVERED_FROM_SPOTLIGHT'
            ELSE 'NO_TEXT_VALUES_RECOVERED_FOR_RECORD' END AS spotlight_decode_status,
       'Spotlight-first record review. V0_9_21 keeps GUI rows raw_record anchored and avoids broad FFS/app joins; full per-record exports are support/diagnostic-only to prevent long SQL materialization. Use Missing From FFS and object/object-summary views for residency pivots.' AS interpretation_note
FROM raw_records r
LEFT JOIN text_roll t ON t.raw_record_id=r.raw_record_id
LEFT JOIN date_one dp ON dp.raw_record_id=r.raw_record_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_summary;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_object_inode_summary AS
WITH rec AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count,
         COUNT(DISTINCT COALESCE(parent_inode_num,'')) AS distinct_parent_id_count,
         MIN(last_updated_utc) AS earliest_last_updated_utc,
         MAX(last_updated_utc) AS latest_last_updated_utc,
         MIN(raw_record_id) AS first_raw_record_id,
         MAX(raw_record_id) AS last_raw_record_id
  FROM raw_records
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), kv AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_key_value_rows,
         COUNT(DISTINCT field_name) AS distinct_spotlight_field_count,
         substr(MAX(CASE WHEN field_name='__spotlight_investigator_text_context' THEN field_value ELSE '' END),1,1800) AS spotlight_text_context_sample
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
)
SELECT rec.source_id,rec.store_guid,rec.source_db,rec.spotlight_inode_or_object_id,rec.spotlight_store_id,
       rec.raw_record_count,rec.distinct_parent_id_count,
       COALESCE(kv.raw_key_value_rows,0) AS raw_key_value_rows,
       COALESCE(kv.distinct_spotlight_field_count,0) AS distinct_spotlight_field_count,
       0 AS date_candidate_rows,
       rec.earliest_last_updated_utc,rec.latest_last_updated_utc,
       '' AS earliest_spotlight_date_utc,
       '' AS latest_spotlight_date_utc,
       rec.first_raw_record_id,rec.last_raw_record_id,
       COALESCE(kv.spotlight_text_context_sample,'') AS spotlight_text_context_sample,
       CASE WHEN rec.raw_record_count>1 THEN 'MULTIPLE_SPOTLIGHT_RECORDS_SHARE_OBJECT_ID'
            WHEN COALESCE(kv.raw_key_value_rows,0)>20 THEN 'SINGLE_RECORD_MANY_FIELDS'
            ELSE 'SINGLE_OR_LOW_EXPANSION_OBJECT' END AS object_materialization_status,
       'Object/inode-centric rollup. V0_9_21 normal exports summarize this view because the full per-object listing is support/diagnostic-only.' AS interpretation_note
FROM rec
LEFT JOIN kv ON kv.source_id=rec.source_id AND kv.store_guid=rec.store_guid AND kv.source_db=rec.source_db AND kv.spotlight_inode_or_object_id=rec.spotlight_inode_or_object_id AND kv.spotlight_store_id=rec.spotlight_store_id;

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_diagnostic_summary;
CREATE VIEW vw_ios_spotlight_object_inode_diagnostic_summary AS
WITH obj AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), buckets AS (
  SELECT source_id,store_guid,source_db,
         CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
              WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
              WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
              ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END AS object_record_bucket,
         COUNT(*) AS object_count,
         SUM(raw_record_count) AS raw_record_count,
         MIN(raw_record_count) AS min_records_per_object,
         MAX(raw_record_count) AS max_records_per_object
  FROM obj
  GROUP BY source_id,store_guid,source_db,
           CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
                WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
                WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
                ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END
)
SELECT source_id,store_guid,source_db,object_record_bucket,object_count,raw_record_count,min_records_per_object,max_records_per_object,
       'Compact object/inode materialization diagnostic. Use this normal export to decide whether the case should pivot to object-centric aggregation; full per-object rows require support/diagnostic export.' AS interpretation_note
FROM buckets;

DROP VIEW IF EXISTS vw_ios_spotlight_object_identity;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_object_identity AS
WITH kv AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         COUNT(*) AS string_probe_count,
         MAX(CASE WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN substr(field_value,1,1500) ELSE '' END) AS sample_url_or_web,
         MAX(CASE WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN substr(field_value,1,1500) ELSE '' END) AS sample_path_or_file_ref,
         MAX(CASE WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN substr(field_value,1,700) ELSE '' END) AS sample_email_or_account,
         MAX(substr(field_value,1,1500)) AS sample_decoded_string
  FROM raw_key_values
  WHERE source_id IN (SELECT source_id FROM evidence_sources)
    AND (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.raw_record_id,r.source_id,r.store_guid,r.source_db,
       CASE
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' OR r.store_guid LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' THEN 'NSFileProtectionCompleteUntilFirstUserAuthentication'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUnlessOpen%' OR r.store_guid LIKE '%NSFileProtectionCompleteUnlessOpen%' THEN 'NSFileProtectionCompleteUnlessOpen'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteWhenUserInactive%' OR r.store_guid LIKE '%NSFileProtectionCompleteWhenUserInactive%' THEN 'NSFileProtectionCompleteWhenUserInactive'
         WHEN r.source_db LIKE '%NSFileProtectionComplete%' OR r.store_guid LIKE '%NSFileProtectionComplete%' THEN 'NSFileProtectionComplete'
         WHEN r.source_db LIKE '%/Priority/%' OR r.store_guid LIKE '%Priority%' THEN 'Priority'
         ELSE 'UnknownOrUnparsedProtectionClass'
       END AS protection_class,
       r.inode_num AS spotlight_inode_or_object_id,
       r.parent_inode_num AS spotlight_parent_id,
       r.store_id AS spotlight_store_id,
       r.file_name,r.display_name,r.full_path,r.content_type,r.content_type_tree,r.record_state,
       r.last_updated_utc,
       COALESCE(k.string_probe_count,0) AS string_probe_count,
       COALESCE(NULLIF(k.sample_url_or_web,''),'') AS sample_url_or_web,
       COALESCE(NULLIF(k.sample_path_or_file_ref,''),'') AS sample_path_or_file_ref,
       COALESCE(NULLIF(k.sample_email_or_account,''),'') AS sample_email_or_account,
       COALESCE(k.sample_decoded_string,'') AS sample_decoded_string,
       CASE WHEN COALESCE(r.full_path,'')<>'' AND r.full_path<>'/' THEN 'PATH_FIELD_PRESENT'
            WHEN COALESCE(NULLIF(k.sample_path_or_file_ref,''),'')<>'' THEN 'STRING_PATH_REFERENCE_PRESENT'
            WHEN COALESCE(k.string_probe_count,0)>0 THEN 'STRING_PROBES_ONLY'
            ELSE 'IDENTIFIERS_ONLY' END AS identity_basis,
       'Use Spotlight IDs/path fragments with FFS inventory and parsed app DB records; Last_Updated remains index/update timing.' AS interpretation_note
FROM raw_records r
LEFT JOIN kv k ON k.source_id=r.source_id AND k.store_guid=r.store_guid AND k.source_db=r.source_db AND k.inode_num=r.inode_num AND COALESCE(k.store_id,'')=COALESCE(r.store_id,'')
WHERE r.source_id IN (SELECT source_id FROM evidence_sources)
  AND (r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%');

DROP VIEW IF EXISTS vw_ios_spotlight_to_ffs_object_links;
CREATE VIEW vw_ios_spotlight_to_ffs_object_links AS
WITH refs AS (
  SELECT * FROM vw_ios_spotlight_referenced_paths WHERE COALESCE(normalized_ios_path,'')<>''
), files AS (
  SELECT source_id,normalized_path,file_name,size_bytes,zip_modified_utc,protection_class_hint,app_container_hint,domain_hint,'full_inventory' AS lookup_source
  FROM ios_ffs_file_inventory
  UNION ALL
  SELECT l.source_id,l.normalized_path,l.file_name,l.size_bytes,l.zip_modified_utc,l.protection_class_hint,l.app_container_hint,l.domain_hint,COALESCE(NULLIF(l.lookup_source,''),'slim_path_lookup') AS lookup_source
  FROM ios_ffs_path_lookup l
  WHERE NOT EXISTS (SELECT 1 FROM ios_ffs_file_inventory f WHERE f.source_id=l.source_id LIMIT 1)
)
SELECT r.reference_id,r.source_id,r.store_guid,r.source_db,r.inode_num AS spotlight_inode_or_object_id,
       r.store_id AS spotlight_store_id,r.parent_inode_num AS spotlight_parent_id,
       r.field_name,r.reference_type,r.normalized_ios_path,
       CASE WHEN f.normalized_path IS NOT NULL THEN 'PRESENT_AS_FILE_IN_FFS'
            ELSE 'SPOTLIGHT_ONLY_FILE_MISSING_OR_UNRESOLVED' END AS residency_status,
       CASE WHEN f.normalized_path IS NOT NULL THEN 'HIGH_PATH_MATCH'
            ELSE 'MEDIUM_PATH_ABSENT_FROM_ZIP_INVENTORY' END AS confidence,
       COALESCE(f.file_name,'') AS matched_file_name,
       COALESCE(f.size_bytes,0) AS matched_size_bytes,
       COALESCE(f.zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.protection_class_hint,'') AS matched_protection_class,
       COALESCE(f.app_container_hint,'') AS matched_app_container,
       COALESCE(f.domain_hint,'') AS matched_domain,
       CASE WHEN f.normalized_path IS NOT NULL
            THEN 'Exact normalized Spotlight path matched an enumerated FFS ZIP path. This supports current-file presence, not usage.'
            ELSE 'Path was absent from enumerated FFS ZIP path inventory. This is a lead only and does not by itself prove user deletion.' END AS interpretation_note
FROM refs r
LEFT JOIN files f ON f.source_id=r.source_id AND f.normalized_path=r.normalized_ios_path;

DROP VIEW IF EXISTS vw_ios_spotlight_to_app_db_record_links;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_to_app_db_record_links AS
WITH parsed AS (
  SELECT source_id,
         CASE
           WHEN database_category='APPLE_MESSAGES' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
           WHEN database_category='CALL_HISTORY' THEN 'CALL_OR_FACETIME_RELATED'
           WHEN database_category='WHATSAPP' THEN 'WHATSAPP_RELATED'
           WHEN database_category='SIGNAL' THEN 'SIGNAL_RELATED'
           WHEN database_category='TELEGRAM' THEN 'TELEGRAM_RELATED'
           WHEN database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT') THEN 'WEB_OR_BROWSER_RELATED'
           WHEN database_category='MAIL' THEN 'MAIL_OR_ACCOUNT_RELATED'
           WHEN database_category='CONTACTS' THEN 'CONTACT_OR_ADDRESS_BOOK_RELATED'
           WHEN database_category='CALENDAR' THEN 'CALENDAR_OR_INVITATION_RELATED'
           ELSE '' END AS object_category,
         COUNT(*) AS parsed_record_count,
         MIN(record_timestamp_utc) AS earliest_record_timestamp_utc,
         MAX(record_timestamp_utc) AS latest_record_timestamp_utc,
         MIN(database_normalized_path) AS sample_database_path,
         MIN(table_name) AS sample_table_name,
         MIN(record_category) AS sample_record_category,
         MIN(substr(COALESCE(NULLIF(url,''),NULLIF(title,''),NULLIF(text_snippet,''),NULLIF(file_path,''),''),1,1200)) AS sample_parsed_value
  FROM ios_app_parsed_records
  WHERE source_id IN (SELECT source_id FROM evidence_sources)
  GROUP BY source_id,object_category
)
SELECT c.candidate_id,c.source_id,c.store_guid,c.source_db,c.inode_num AS spotlight_inode_or_object_id,c.store_id AS spotlight_store_id,c.parent_inode_num AS spotlight_parent_id,
       c.field_name,c.object_category,c.database_residency_status,c.database_name,c.database_category,c.app_hint,c.candidate_database_path,
       c.record_inventory_status,c.matched_record_category,c.matched_table_name,c.matched_table_row_count,
       COALESCE(p.parsed_record_count,0) AS parsed_record_count,
       COALESCE(p.earliest_record_timestamp_utc,'') AS earliest_record_timestamp_utc,
       COALESCE(p.latest_record_timestamp_utc,'') AS latest_record_timestamp_utc,
       COALESCE(p.sample_database_path,'') AS sample_database_path,
       COALESCE(p.sample_table_name,'') AS sample_table_name,
       COALESCE(p.sample_record_category,'') AS sample_record_category,
       COALESCE(p.sample_parsed_value,'') AS sample_parsed_value,
       c.string_value_sample,
       CASE WHEN COALESCE(p.parsed_record_count,0)>0 THEN 'POTENTIAL_APP_DB_RECORD_FAMILY_PRESENT'
            WHEN c.database_residency_status LIKE 'POTENTIAL_RECORD_TABLE%' THEN 'APP_DB_TABLE_PRESENT_RECORD_LEVEL_MATCH_NOT_PROVEN'
            ELSE c.database_residency_status END AS app_db_link_status,
       'Family-level correlation only. Exact Spotlight string-to-app database row matching remains a later phase.' AS interpretation_note
FROM vw_ios_database_residency_candidates c
LEFT JOIN parsed p ON p.source_id=c.source_id AND p.object_category=c.object_category;

DROP VIEW IF EXISTS vw_ios_spotlight_residency_summary;
CREATE VIEW vw_ios_spotlight_residency_summary AS
SELECT residency_status,confidence,COUNT(*) AS reference_count,
       COUNT(DISTINCT spotlight_inode_or_object_id) AS distinct_record_count,
       MIN(normalized_ios_path) AS first_path_sample,MAX(normalized_ios_path) AS last_path_sample
FROM vw_ios_spotlight_to_ffs_object_links
GROUP BY residency_status,confidence
UNION ALL
SELECT database_residency_status AS residency_status,'DATABASE_FAMILY_HEURISTIC' AS confidence,COUNT(*) AS reference_count,
       COUNT(DISTINCT inode_num) AS distinct_record_count,MIN(candidate_database_path) AS first_path_sample,MAX(candidate_database_path) AS last_path_sample
FROM vw_ios_database_residency_candidates
GROUP BY database_residency_status;

DROP VIEW IF EXISTS vw_ios_protection_class_summary;
)VSGUI"
    });
    execGuiSql(R"SQL(CREATE VIEW vw_ios_protection_class_summary AS
WITH ios_records AS (
  SELECT raw_record_id,source_id,store_guid,source_db,store_path,inode_num,store_id,last_updated_utc,
         CASE
           WHEN source_db LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' OR store_guid LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' THEN 'NSFileProtectionCompleteUntilFirstUserAuthentication'
           WHEN source_db LIKE '%NSFileProtectionCompleteUnlessOpen%' OR store_guid LIKE '%NSFileProtectionCompleteUnlessOpen%' THEN 'NSFileProtectionCompleteUnlessOpen'
           WHEN source_db LIKE '%NSFileProtectionCompleteWhenUserInactive%' OR store_guid LIKE '%NSFileProtectionCompleteWhenUserInactive%' THEN 'NSFileProtectionCompleteWhenUserInactive'
           WHEN source_db LIKE '%NSFileProtectionComplete%' OR store_guid LIKE '%NSFileProtectionComplete%' THEN 'NSFileProtectionComplete'
           WHEN source_db LIKE '%/Priority/%' OR store_guid LIKE '%Priority%' THEN 'Priority'
           ELSE 'UnknownOrUnparsedProtectionClass'
         END AS protection_class
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
), kv AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,COUNT(*) AS string_probe_rows
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.protection_class,
       COUNT(*) AS raw_record_count,
       SUM(CASE WHEN COALESCE(k.string_probe_rows,0)>0 THEN 1 ELSE 0 END) AS records_with_string_probes,
       COALESCE(SUM(k.string_probe_rows),0) AS string_probe_rows,
       COUNT(DISTINCT r.source_db) AS selected_database_count,
       MIN(NULLIF(r.last_updated_utc,'')) AS earliest_last_updated_utc,
       MAX(NULLIF(r.last_updated_utc,'')) AS latest_last_updated_utc
FROM ios_records r
LEFT JOIN kv k ON k.source_id=r.source_id AND k.store_guid=r.store_guid AND k.source_db=r.source_db AND k.inode_num=r.inode_num AND k.store_id=r.store_id
GROUP BY r.protection_class;

DROP VIEW IF EXISTS vw_ios_artifact_hint_summary;
CREATE VIEW vw_ios_artifact_hint_summary AS
WITH probe AS (
  SELECT store_guid,source_db,inode_num,store_id,LOWER(field_value) AS v,field_value
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), categorized AS (
  SELECT store_guid,source_db,inode_num,store_id,field_value,
         CASE
           WHEN v LIKE '%/mail/attachmentdata/%' THEN 'MAIL_ATTACHMENT_PATH'
)SQL" R"SQL(           WHEN v LIKE '%com.apple.clouddocs.iclouddrivefileprovider%' OR v LIKE '%icloud drive%' OR v LIKE '%/mobile documents/%' THEN 'ICLOUD_DRIVE_OR_CLOUDDOCS'
           WHEN v LIKE '%drive.google.com%' THEN 'GOOGLE_DRIVE_LINK'
           WHEN v LIKE '%docs.google.com%' THEN 'GOOGLE_DOCS_LINK'
           WHEN v LIKE '%teams.microsoft.com%' THEN 'MICROSOFT_TEAMS_LINK'
           WHEN v LIKE '%onedrive%' THEN 'ONEDRIVE_LINK_OR_TEXT'
           WHEN v LIKE '%zoom.us/%' THEN 'ZOOM_LINK'
           WHEN v LIKE '%maps.app.goo.gl%' OR v LIKE '%maps.google.%' THEN 'MAP_LINK'
           WHEN v LIKE '%invite.ics%' OR v LIKE '%calendar%' OR v LIKE '%rsvp%' THEN 'CALENDAR_INVITATION'
           WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'WEB_URL_OR_HTML_LINK'
           WHEN v LIKE 'file:%' OR v LIKE '/private/var/%' OR v LIKE '%/mobile/%' THEN 'IOS_FILE_PATH'
           WHEN v LIKE '%@%' AND v LIKE '%.%' THEN 'EMAIL_OR_ACCOUNT_TEXT'
           WHEN v LIKE '%imessage%' OR v LIKE '%sms%' OR v LIKE '%message%' THEN 'MESSAGE_OR_MESSAGE_ATTACHMENT_TEXT'
           ELSE 'OTHER_STRING_PROBE'
         END AS artifact_hint
  FROM probe
)
SELECT artifact_hint,
       COUNT(*) AS string_probe_rows,
       COUNT(DISTINCT store_guid) AS store_count,
       COUNT(DISTINCT inode_num) AS distinct_record_count,
       COUNT(DISTINCT field_value) AS distinct_value_count,
       substr(MIN(field_value),1,250) AS min_sample_value,
       substr(MAX(field_value),1,250) AS max_sample_value
FROM categorized
GROUP BY artifact_hint
ORDER BY string_probe_rows DESC, artifact_hint;

DROP VIEW IF EXISTS vw_ios_record_investigation_hints;
)SQL");
    execGuiSql(R"SQL(CREATE VIEW vw_ios_record_investigation_hints AS
WITH kv AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         CASE
           WHEN LOWER(field_value) LIKE '%/mail/attachmentdata/%' THEN 'MAIL_ATTACHMENT_PATH'
           WHEN LOWER(field_value) LIKE '%com.apple.clouddocs.iclouddrivefileprovider%' OR LOWER(field_value) LIKE '%icloud drive%' OR LOWER(field_value) LIKE '%/mobile documents/%' THEN 'ICLOUD_DRIVE_OR_CLOUDDOCS'
           WHEN LOWER(field_value) LIKE '%drive.google.com%' THEN 'GOOGLE_DRIVE_LINK'
           WHEN LOWER(field_value) LIKE '%docs.google.com%' THEN 'GOOGLE_DOCS_LINK'
           WHEN LOWER(field_value) LIKE '%teams.microsoft.com%' THEN 'MICROSOFT_TEAMS_LINK'
           WHEN LOWER(field_value) LIKE '%onedrive%' THEN 'ONEDRIVE_LINK_OR_TEXT'
           WHEN LOWER(field_value) LIKE '%zoom.us/%' THEN 'ZOOM_LINK'
           WHEN LOWER(field_value) LIKE '%maps.app.goo.gl%' OR LOWER(field_value) LIKE '%maps.google.%' THEN 'MAP_LINK'
           WHEN LOWER(field_value) LIKE '%invite.ics%' OR LOWER(field_value) LIKE '%calendar%' OR LOWER(field_value) LIKE '%rsvp%' THEN 'CALENDAR_INVITATION'
           WHEN LOWER(field_value) LIKE '%http://%' OR LOWER(field_value) LIKE '%https://%' OR LOWER(field_value) LIKE '%www.%' THEN 'WEB_URL_OR_HTML_LINK'
           WHEN LOWER(field_value) LIKE 'file:%' OR LOWER(field_value) LIKE '/private/var/%' OR LOWER(field_value) LIKE '%/mobile/%' THEN 'IOS_FILE_PATH'
           WHEN LOWER(field_value) LIKE '%@%' AND LOWER(field_value) LIKE '%.%' THEN 'EMAIL_OR_ACCOUNT_TEXT'
           WHEN LOWER(field_value) LIKE '%imessage%' OR LOWER(field_value) LIKE '%sms%' OR LOWER(field_value) LIKE '%message%' THEN 'MESSAGE_OR_MESSAGE_ATTACHMENT_TEXT'
           ELSE 'OTHER_STRING_PROBE'
         END AS artifact_hint
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), agg AS (
  SELECT source_id,store_guid,source_db,inode_num,store_id,
         COUNT(*) AS string_probe_rows,
         GROUP_CONCAT(DISTINCT artifact_hint) AS artifact_hints,
         MAX(CASE WHEN artifact_hint='MAIL_ATTACHMENT_PATH' THEN 1 ELSE 0 END) AS has_mail_attachment,
         MAX(CASE WHEN artifact_hint='ICLOUD_DRIVE_OR_CLOUDDOCS' THEN 1 ELSE 0 END) AS has_icloud_docs,
         MAX(CASE WHEN artifact_hint IN ('GOOGLE_DRIVE_LINK','GOOGLE_DOCS_LINK') THEN 1 ELSE 0 END) AS has_google_workspace,
         MAX(CASE WHEN artifact_hint IN ('MICROSOFT_TEAMS_LINK','ONEDRIVE_LINK_OR_TEXT') THEN 1 ELSE 0 END) AS has_microsoft_cloud,
         MAX(CASE WHEN artifact_hint='CALENDAR_INVITATION' THEN 1 ELSE 0 END) AS has_calendar_invite,
         MAX(CASE WHEN artifact_hint='EMAIL_OR_ACCOUNT_TEXT' THEN 1 ELSE 0 END) AS has_email_text,
)SQL" R"SQL(         MAX(CASE WHEN artifact_hint='WEB_URL_OR_HTML_LINK' THEN 1 ELSE 0 END) AS has_web_url
  FROM kv
  GROUP BY source_id,store_guid,source_db,inode_num,store_id
)
SELECT r.raw_record_id,
       r.source_id,
       r.store_guid,
       CASE
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' OR r.store_guid LIKE '%NSFileProtectionCompleteUntilFirstUserAuthentication%' THEN 'NSFileProtectionCompleteUntilFirstUserAuthentication'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteUnlessOpen%' OR r.store_guid LIKE '%NSFileProtectionCompleteUnlessOpen%' THEN 'NSFileProtectionCompleteUnlessOpen'
         WHEN r.source_db LIKE '%NSFileProtectionCompleteWhenUserInactive%' OR r.store_guid LIKE '%NSFileProtectionCompleteWhenUserInactive%' THEN 'NSFileProtectionCompleteWhenUserInactive'
         WHEN r.source_db LIKE '%NSFileProtectionComplete%' OR r.store_guid LIKE '%NSFileProtectionComplete%' THEN 'NSFileProtectionComplete'
         WHEN r.source_db LIKE '%/Priority/%' OR r.store_guid LIKE '%Priority%' THEN 'Priority'
         ELSE 'UnknownOrUnparsedProtectionClass'
       END AS protection_class,
       CASE
         WHEN a.has_mail_attachment=1 THEN 'Mail attachment / Mail AttachmentData path'
         WHEN a.has_icloud_docs=1 THEN 'iCloud Drive or CloudDocs provider content'
         WHEN a.has_google_workspace=1 THEN 'Google Docs or Google Drive link/content'
         WHEN a.has_microsoft_cloud=1 THEN 'Microsoft Teams or OneDrive link/content'
         WHEN a.has_calendar_invite=1 THEN 'Calendar invitation or ICS-like content'
         WHEN a.has_email_text=1 THEN 'Email address/account-like text'
         WHEN a.has_web_url=1 THEN 'Web URL or HTML link content'
         ELSE 'Other decoded string probe'
       END AS primary_investigation_hint,
       a.artifact_hints,
       a.string_probe_rows,
       r.source_db,
       r.inode_num,
       r.store_id,
       r.parent_inode_num,
       r.file_name,
       r.content_type,
       r.display_name,
       r.full_path,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       r.record_state
FROM raw_records r
JOIN agg a ON a.source_id=r.source_id AND a.store_guid=r.store_guid AND a.source_db=r.source_db AND a.inode_num=r.inode_num AND a.store_id=r.store_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';
)SQL");
    execGuiSql(R"SQL(
CREATE TABLE IF NOT EXISTS investigator_tags (
  tag_id INTEGER PRIMARY KEY AUTOINCREMENT,
  tag_name TEXT UNIQUE NOT NULL,
  tag_color TEXT,
  created_utc TEXT,
  notes TEXT
);
CREATE TABLE IF NOT EXISTS artifact_tags (
  artifact_tag_id INTEGER PRIMARY KEY AUTOINCREMENT,
  artifact_id INTEGER NOT NULL,
  tag_id INTEGER NOT NULL,
  created_utc TEXT,
  UNIQUE(artifact_id, tag_id)
);
CREATE TABLE IF NOT EXISTS investigator_notes (
  note_id INTEGER PRIMARY KEY AUTOINCREMENT,
  target_type TEXT NOT NULL,
  target_id TEXT NOT NULL,
  note_text TEXT,
  created_utc TEXT,
  updated_utc TEXT
);
CREATE INDEX IF NOT EXISTS idx_artifact_tags_artifact ON artifact_tags(artifact_id);
CREATE INDEX IF NOT EXISTS idx_artifact_tags_tag ON artifact_tags(tag_id);
CREATE INDEX IF NOT EXISTS idx_investigator_notes_target ON investigator_notes(target_type, target_id);
CREATE TABLE IF NOT EXISTS gui_checked_artifacts (
  artifact_id INTEGER PRIMARY KEY,
  checked_utc TEXT,
  notes TEXT
);
CREATE INDEX IF NOT EXISTS idx_gui_checked_artifacts_checked ON gui_checked_artifacts(checked_utc);

DROP VIEW IF EXISTS vw_checked_artifacts;
CREATE VIEW vw_checked_artifacts AS
WITH tag_summary AS (
  SELECT at.artifact_id, GROUP_CONCAT(it.tag_name, '; ') AS tags, COUNT(*) AS tag_count
  FROM artifact_tags at
  JOIN investigator_tags it ON it.tag_id=at.tag_id
  GROUP BY at.artifact_id
), note_summary AS (
  SELECT CAST(target_id AS INTEGER) AS artifact_id,
         COUNT(*) AS note_count,
         MAX(COALESCE(NULLIF(updated_utc,''), created_utc)) AS last_note_utc,
         (SELECT note_text FROM investigator_notes n2 WHERE n2.target_type='artifact' AND CAST(n2.target_id AS INTEGER)=CAST(n.target_id AS INTEGER) ORDER BY COALESCE(NULLIF(n2.updated_utc,''), n2.created_utc) DESC, n2.note_id DESC LIMIT 1) AS last_note_text
  FROM investigator_notes n
  WHERE target_type='artifact'
  GROUP BY CAST(target_id AS INTEGER)
), date_summary AS (
  SELECT artifact_id, usage_latest_utc, modified_latest_utc, created_latest_utc, downloaded_latest_utc, last_date_utc
  FROM artifact_date_summary
)
SELECT c.checked_utc,
       a.artifact_id,
       COALESCE(ts.tags,'') AS tags,
       COALESCE(ts.tag_count,0) AS tag_count,
       COALESCE(ns.note_count,0) AS note_count,
       COALESCE(ns.last_note_utc,'') AS last_note_utc,
       COALESCE(ns.last_note_text,'') AS last_note_text,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
)SQL" R"SQL(       a.use_count_value,
       a.usage_field_summary,
       ds.usage_latest_utc,
       ds.modified_latest_utc,
       ds.created_latest_utc,
       ds.downloaded_latest_utc,
       ds.last_date_utc,
       a.confidence
FROM gui_checked_artifacts c
JOIN artifacts a ON a.artifact_id=c.artifact_id
LEFT JOIN tag_summary ts ON ts.artifact_id=a.artifact_id
LEFT JOIN note_summary ns ON ns.artifact_id=a.artifact_id
LEFT JOIN date_summary ds ON ds.artifact_id=a.artifact_id;

DROP VIEW IF EXISTS vw_tagged_artifacts;
CREATE VIEW vw_tagged_artifacts AS
WITH note_summary AS (
  SELECT target_id AS artifact_id,
         COUNT(*) AS note_count,
         MAX(COALESCE(NULLIF(updated_utc,''), created_utc)) AS last_note_utc,
         (SELECT note_text FROM investigator_notes n2 WHERE n2.target_type='artifact' AND n2.target_id=n.target_id ORDER BY COALESCE(NULLIF(n2.updated_utc,''), n2.created_utc) DESC, n2.note_id DESC LIMIT 1) AS last_note_text
  FROM investigator_notes n
  WHERE target_type='artifact'
  GROUP BY target_id
), date_summary AS (
  SELECT artifact_id, usage_latest_utc, modified_latest_utc, created_latest_utc, downloaded_latest_utc
  FROM artifact_date_summary
)
SELECT t.tag_id,
       t.tag_name,
       at.created_utc AS tagged_utc,
       a.artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.use_count_value,
       a.usage_field_summary,
       a.open_count_estimate,
       ds.usage_latest_utc,
       ds.modified_latest_utc,
       ds.created_latest_utc,
       ds.downloaded_latest_utc,
       COALESCE(ns.note_count,0) AS note_count,
       COALESCE(ns.last_note_utc,'') AS last_note_utc,
       COALESCE(ns.last_note_text,'') AS last_note_text,
       a.confidence
FROM artifact_tags at
JOIN investigator_tags t ON t.tag_id=at.tag_id
JOIN artifacts a ON a.artifact_id=at.artifact_id
LEFT JOIN note_summary ns ON ns.artifact_id=CAST(a.artifact_id AS TEXT)
LEFT JOIN date_summary ds ON ds.artifact_id=a.artifact_id;

DROP VIEW IF EXISTS vw_artifact_notes;
CREATE VIEW vw_artifact_notes AS
WITH tag_summary AS (
  SELECT at.artifact_id, GROUP_CONCAT(it.tag_name, '; ') AS tags
  FROM artifact_tags at
  JOIN investigator_tags it ON it.tag_id=at.tag_id
  GROUP BY at.artifact_id
)
SELECT n.note_id,
       n.created_utc,
       n.updated_utc,
       n.note_text,
       CAST(n.target_id AS INTEGER) AS artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
)SQL" R"SQL(       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       COALESCE(ts.tags,'') AS tags
FROM investigator_notes n
LEFT JOIN artifacts a ON a.artifact_id=CAST(n.target_id AS INTEGER)
LEFT JOIN tag_summary ts ON ts.artifact_id=a.artifact_id
WHERE n.target_type='artifact';

DROP VIEW IF EXISTS vw_export_ready_artifacts;
)SQL");
    execGuiSql(R"SQL(CREATE VIEW vw_export_ready_artifacts AS
WITH tag_summary AS (
  SELECT at.artifact_id, GROUP_CONCAT(it.tag_name, '; ') AS tags, COUNT(*) AS tag_count
  FROM artifact_tags at
  JOIN investigator_tags it ON it.tag_id=at.tag_id
  GROUP BY at.artifact_id
), note_summary AS (
  SELECT CAST(target_id AS INTEGER) AS artifact_id, COUNT(*) AS note_count, MAX(COALESCE(NULLIF(updated_utc,''), created_utc)) AS last_note_utc
  FROM investigator_notes
  WHERE target_type='artifact'
  GROUP BY CAST(target_id AS INTEGER)
)
SELECT a.artifact_id,
       COALESCE(ts.tags,'') AS tags,
       COALESCE(ts.tag_count,0) AS tag_count,
       COALESCE(ns.note_count,0) AS note_count,
       COALESCE(ns.last_note_utc,'') AS last_note_utc,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.use_count_value,
       a.usage_field_summary,
       ads.usage_latest_utc,
       ads.modified_latest_utc,
       ads.created_latest_utc,
       ads.downloaded_latest_utc,
       a.confidence
FROM artifacts a
LEFT JOIN tag_summary ts ON ts.artifact_id=a.artifact_id
LEFT JOIN note_summary ns ON ns.artifact_id=a.artifact_id
LEFT JOIN artifact_date_summary ads ON ads.artifact_id=a.artifact_id
WHERE COALESCE(ts.tag_count,0)>0 OR COALESCE(ns.note_count,0)>0;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_date_field_attribution;
CREATE VIEW vw_date_field_attribution AS
WITH common_date_counts AS (
  SELECT substr(parsed_utc,1,10) AS date_only, COUNT(*) AS row_count
  FROM raw_date_candidates
  WHERE COALESCE(parsed_utc,'')<>''
  GROUP BY substr(parsed_utc,1,10)
  HAVING COUNT(*) >= 1000
), top_common_dates AS (
  SELECT date_only FROM common_date_counts ORDER BY row_count DESC, date_only DESC LIMIT 5
), attributed AS (
  SELECT d.raw_date_id,
         d.source_id,
         d.store_guid,
         d.source_db,
         d.inode_num,
         d.store_id,
         COALESCE(d.artifact_id, a.artifact_id) AS artifact_id,
         COALESCE(NULLIF(d.parent_inode_num,''), a.parent_inode_num) AS parent_inode_num,
         COALESCE(NULLIF(d.file_name,''), a.file_name) AS file_name,
         a.display_name,
         COALESCE(NULLIF(d.best_path,''), a.best_path) AS best_path,
         a.path_source,
         a.path_status,
         a.logical_size_bytes,
         a.physical_size_bytes,
         a.content_type,
         a.where_froms,
         d.field_name AS raw_spotlight_field,
         d.field_value AS raw_spotlight_value,
         d.parsed_utc,
         d.parse_method,
         substr(d.parsed_utc,1,10) AS parsed_date_only,
         COALESCE(cdc.row_count,0) AS common_date_row_count,
         CASE
           WHEN lower(d.field_name) LIKE '%lastuseddate%' THEN 'opened_last'
           WHEN lower(d.field_name) LIKE '%useddates%' THEN 'used_date'
           WHEN lower(d.field_name) LIKE '%recent%spotlight%engagement%' THEN 'spotlight_engagement'
           WHEN lower(d.field_name) LIKE '%download%' THEN 'downloaded'
           WHEN lower(d.field_name) LIKE '%contentcreation%' OR lower(d.field_name) LIKE '%creationdate%' THEN 'created'
           WHEN lower(d.field_name) LIKE '%contentmodification%' OR lower(d.field_name) LIKE '%contentchange%' OR lower(d.field_name) LIKE '%modificationdate%' THEN 'modified'
           WHEN lower(d.field_name) LIKE '%interestingdate%' THEN 'interesting_or_index_date'
           WHEN lower(d.field_name) LIKE '%gps%' THEN 'gps_date'
           WHEN lower(d.field_name) LIKE '%startdate%' THEN 'start_date'
           WHEN lower(d.field_name) LIKE '%enddate%' THEN 'end_date'
           WHEN lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 'metadata_seen_or_index_updated'
           ELSE 'other_date'
         END AS event_type_interpretation,
         CASE
           WHEN lower(d.field_name) LIKE '%lastuseddate%' OR lower(d.field_name) LIKE '%useddates%' THEN 'usage/open evidence from Spotlight usage field'
           WHEN lower(d.field_name) LIKE '%download%' THEN 'download/origin date field'
)SQL" R"SQL(           WHEN lower(d.field_name) LIKE '%contentcreation%' OR lower(d.field_name) LIKE '%creationdate%' THEN 'file/content creation date candidate'
           WHEN lower(d.field_name) LIKE '%contentmodification%' OR lower(d.field_name) LIKE '%contentchange%' OR lower(d.field_name) LIKE '%modificationdate%' THEN 'file/content modification date candidate'
           WHEN lower(d.field_name) LIKE '%interestingdate%' THEN 'Spotlight interesting/index date; review carefully'
           WHEN lower(d.field_name) LIKE '%ranking%' THEN 'Spotlight ranking date; not direct user activity by itself'
           WHEN lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 'Spotlight record/index update time; not direct user activity by itself'
           ELSE 'date candidate from decoded Spotlight metadata'
         END AS interpretation_note,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'ARTIFACT_MATCHED_BY_SOURCE_STORE_INODE'
           ELSE 'NO_ARTIFACT_MATCH_BY_SOURCE_STORE_INODE'
         END AS association_status,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND COALESCE(NULLIF(COALESCE(d.best_path,a.best_path),''),'')<>'' AND COALESCE(NULLIF(COALESCE(d.file_name,a.file_name),''),'')<>'' THEN 'HIGH_OBJECT_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND (COALESCE(NULLIF(COALESCE(d.best_path,a.best_path),''),'')<>'' OR COALESCE(NULLIF(COALESCE(d.file_name,a.file_name),''),'')<>'') THEN 'MEDIUM_OBJECT_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'LOW_OBJECT_CONTEXT'
           ELSE 'NONE'
         END AS association_confidence,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'source_id + store_guid + inode_num'
           ELSE ''
         END AS association_method,
         CASE
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND COALESCE(NULLIF(COALESCE(d.best_path,a.best_path),''),'')<>'' THEN 'HAS_ARTIFACT_AND_PATH_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL AND COALESCE(NULLIF(COALESCE(d.file_name,a.file_name),''),'')<>'' THEN 'HAS_ARTIFACT_AND_NAME_CONTEXT'
           WHEN COALESCE(d.artifact_id, a.artifact_id) IS NOT NULL THEN 'HAS_ARTIFACT_ONLY'
           ELSE 'DATE_ONLY_NO_ARTIFACT_CONTEXT'
         END AS object_context_status,
         CASE
           WHEN lower(d.field_name) LIKE '%interestingdate%' OR lower(d.field_name) LIKE '%ranking%' OR lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 1
)SQL" R"SQL(           WHEN substr(d.parsed_utc,1,10) IN (SELECT date_only FROM top_common_dates) AND lower(d.field_name) NOT LIKE '%creation%' AND lower(d.field_name) NOT LIKE '%modification%' AND lower(d.field_name) NOT LIKE '%used%' AND lower(d.field_name) NOT LIKE '%download%' THEN 1
           ELSE 0
         END AS likely_snapshot_or_index_date,
         CASE
           WHEN lower(d.field_name) LIKE '%interestingdate%' THEN 'FIELD_IS_SPOTLIGHT_INTERESTING_DATE'
           WHEN lower(d.field_name) LIKE '%ranking%' THEN 'FIELD_IS_SPOTLIGHT_RANKING_DATE'
           WHEN lower(d.field_name) LIKE '%last_updated%' OR lower(d.field_name) LIKE '%lastupdated%' THEN 'FIELD_IS_RECORD_LAST_UPDATED_OR_INDEX_DATE'
           WHEN substr(d.parsed_utc,1,10) IN (SELECT date_only FROM top_common_dates) THEN 'DATE_IS_COMMON_ACROSS_INDEX_SAMPLE'
           ELSE ''
         END AS snapshot_warning_reason
  FROM raw_date_candidates d
  LEFT JOIN artifacts a ON a.source_id=d.source_id AND a.store_guid=d.store_guid AND a.inode_num=d.inode_num
  LEFT JOIN common_date_counts cdc ON cdc.date_only=substr(d.parsed_utc,1,10)
  WHERE COALESCE(d.parsed_utc,'')<>''
)
SELECT *,
       CASE
         WHEN likely_snapshot_or_index_date=1 AND common_date_row_count>0 THEN snapshot_warning_reason || '; common date row count=' || common_date_row_count
         ELSE snapshot_warning_reason
       END AS snapshot_warning_detail
FROM attributed;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_usage_event_detail_attributed;
CREATE VIEW vw_usage_event_detail_attributed AS
SELECT ROW_NUMBER() OVER (ORDER BY d.parsed_utc, d.raw_date_id) AS usage_event_id,
       d.parsed_utc AS event_utc,
       d.event_type_interpretation AS date_type,
       d.raw_spotlight_field AS source_field,
       d.raw_spotlight_value AS raw_value,
       d.interpretation_note AS usage_reason,
       d.likely_snapshot_or_index_date,
       d.snapshot_warning_reason,
       d.snapshot_warning_detail,
       d.association_status,
       d.association_confidence,
       d.object_context_status,
       d.artifact_id,
       d.source_id,
       d.store_guid,
       d.inode_num,
       d.parent_inode_num,
       d.file_name,
       d.display_name,
       d.best_path,
       d.path_source,
       d.path_status,
       d.logical_size_bytes,
       d.physical_size_bytes,
       d.content_type,
       d.where_froms
FROM vw_date_field_attribution d
WHERE d.event_type_interpretation IN ('opened_last','used_date','spotlight_engagement')
   OR lower(d.raw_spotlight_field) LIKE '%used%'
   OR lower(d.raw_spotlight_field) LIKE '%open%';
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_artifact_dates_wide;
CREATE VIEW vw_artifact_dates_wide AS
SELECT artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,logical_size_bytes,physical_size_bytes,content_type,where_froms,
       created_earliest_utc,created_latest_utc,modified_earliest_utc,modified_latest_utc,downloaded_earliest_utc,downloaded_latest_utc,usage_earliest_utc,usage_latest_utc,interesting_or_index_earliest_utc,interesting_or_index_latest_utc,
       likely_snapshot_date_count,associated_date_count,unassociated_date_count,available_date_fields,association_confidence_summary,snapshot_warning_reasons
FROM artifact_date_summary;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_snapshot_date_warnings;
CREATE VIEW vw_snapshot_date_warnings AS
SELECT raw_date_id,artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name,display_name,best_path,path_source,path_status,logical_size_bytes,physical_size_bytes,content_type,raw_spotlight_field,parsed_utc,event_type_interpretation,interpretation_note,association_status,association_confidence,object_context_status,common_date_row_count,snapshot_warning_reason,snapshot_warning_detail
FROM vw_date_field_attribution
WHERE likely_snapshot_or_index_date=1;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_object_date_summary;
CREATE VIEW vw_object_date_summary AS
SELECT artifact_id,
       source_id,
       store_guid,
       inode_num,
       parent_inode_num,
       file_name,
       display_name,
       best_path,
       path_source,
       path_status,
       logical_size_bytes,
       physical_size_bytes,
       content_type,
       first_date_utc,
       last_date_utc,
       total_date_count,
       created_date_count,
       modified_date_count,
       downloaded_date_count,
       usage_date_count,
       interesting_or_index_date_count,
       metadata_seen_or_index_updated_count,
       other_date_count,
       likely_snapshot_or_index_date_count,
       available_date_fields,
       interpreted_date_types,
       snapshot_warning_reasons,
       date_association_status,
       date_association_confidence
FROM artifact_date_summary;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_object_usage_summary;
CREATE VIEW vw_object_usage_summary AS
WITH usage_artifacts AS (
  SELECT artifact_id
  FROM artifacts
  WHERE artifact_id IS NOT NULL
    AND (
         COALESCE(used_dates_count,0)>0
      OR COALESCE(NULLIF(last_used_date_utc,''),'')<>''
      OR COALESCE(NULLIF(first_used_candidate_utc,''),'')<>''
      OR COALESCE(NULLIF(use_count_value,''),'')<>''
      OR COALESCE(open_count_estimate,0)>0
    )
  UNION
  SELECT DISTINCT artifact_id
  FROM usage_evidence
  WHERE artifact_id IS NOT NULL
  UNION
  SELECT DISTINCT artifact_id
  FROM raw_date_candidates
  WHERE artifact_id IS NOT NULL
    AND COALESCE(parsed_utc,'')<>''
    AND date_type IN ('opened_last','used_date','spotlight_engagement')
), usage_dates AS (
  SELECT d.artifact_id,
         MIN(d.parsed_utc) AS usage_earliest_utc,
         MAX(d.parsed_utc) AS usage_latest_utc,
         COUNT(*) AS usage_date_row_count,
         GROUP_CONCAT(DISTINCT d.field_name) AS usage_date_source_fields,
         GROUP_CONCAT(d.parsed_utc, '; ') AS fused_usage_dates_utc,
         SUM(CASE WHEN d.date_type IN ('interesting_or_index_date','metadata_seen_or_index_updated') THEN 1 ELSE 0 END) AS likely_snapshot_or_index_usage_date_count,
         GROUP_CONCAT(DISTINCT CASE WHEN d.date_type IN ('interesting_or_index_date','metadata_seen_or_index_updated') THEN d.field_name ELSE NULL END) AS snapshot_warning_reasons
  FROM raw_date_candidates d
  JOIN usage_artifacts ua ON ua.artifact_id=d.artifact_id
  WHERE d.artifact_id IS NOT NULL
    AND COALESCE(d.parsed_utc,'')<>''
    AND d.date_type IN ('opened_last','used_date','spotlight_engagement')
  GROUP BY d.artifact_id
), usage_rows AS (
  SELECT artifact_id,
         COUNT(*) AS usage_evidence_row_count,
         GROUP_CONCAT(DISTINCT field_name) AS usage_evidence_fields,
         GROUP_CONCAT(field_name || '=' || COALESCE(NULLIF(parsed_utc,''), field_value), '; ') AS usage_evidence_values
  FROM usage_evidence
  WHERE artifact_id IS NOT NULL
  GROUP BY artifact_id
), date_wide AS (
  SELECT d.artifact_id,
         MIN(CASE WHEN d.date_type='created' THEN d.parsed_utc END) AS created_earliest_utc,
         MAX(CASE WHEN d.date_type='created' THEN d.parsed_utc END) AS created_latest_utc,
         MIN(CASE WHEN d.date_type='modified' THEN d.parsed_utc END) AS modified_earliest_utc,
         MAX(CASE WHEN d.date_type='modified' THEN d.parsed_utc END) AS modified_latest_utc,
         MIN(CASE WHEN d.date_type='downloaded' THEN d.parsed_utc END) AS downloaded_earliest_utc,
         MAX(CASE WHEN d.date_type='downloaded' THEN d.parsed_utc END) AS downloaded_latest_utc,
         MIN(CASE WHEN d.date_type='interesting_or_index_date' THEN d.parsed_utc END) AS interesting_or_index_earliest_utc,
)SQL" R"SQL(         MAX(CASE WHEN d.date_type='interesting_or_index_date' THEN d.parsed_utc END) AS interesting_or_index_latest_utc,
         SUM(CASE WHEN d.date_type IN ('interesting_or_index_date','metadata_seen_or_index_updated') THEN 1 ELSE 0 END) AS likely_snapshot_date_count,
         GROUP_CONCAT(DISTINCT d.field_name) AS available_date_fields
  FROM raw_date_candidates d
  JOIN usage_artifacts ua ON ua.artifact_id=d.artifact_id
  WHERE d.artifact_id IS NOT NULL
    AND COALESCE(d.parsed_utc,'')<>''
  GROUP BY d.artifact_id
)
SELECT a.artifact_id,
       a.source_id,
       a.store_guid,
       a.inode_num,
       a.parent_inode_num,
       a.file_name,
       a.display_name,
       a.best_path,
       a.spotlight_display_path,
       a.normalized_mac_path,
       a.path_source,
       a.path_status,
       a.content_type,
       a.content_type_tree,
       a.logical_size_bytes,
       a.physical_size_bytes,
       a.where_froms,
       a.authors,
       a.creator,
       a.existence_status,
       a.deleted_or_orphaned_candidate,
       a.confidence,
       a.first_used_candidate_utc,
       a.last_used_date_utc,
       a.used_dates_count,
       a.used_dates_utc,
       a.use_count_value,
       a.open_count_estimate,
       COALESCE(ud.usage_earliest_utc, NULLIF(a.first_used_candidate_utc,'')) AS usage_earliest_utc,
       COALESCE(ud.usage_latest_utc, NULLIF(a.last_used_date_utc,''), NULLIF(a.first_used_candidate_utc,'')) AS usage_latest_utc,
       COALESCE(ud.fused_usage_dates_utc, NULLIF(a.used_dates_utc,''), NULLIF(a.last_used_date_utc,''), NULLIF(a.first_used_candidate_utc,'')) AS fused_usage_dates_utc,
       COALESCE(ud.usage_date_row_count, 0) AS usage_date_row_count,
       COALESCE(ur.usage_evidence_row_count, 0) AS usage_evidence_row_count,
       COALESCE(NULLIF(ud.usage_date_source_fields,''), NULLIF(ur.usage_evidence_fields,''), '') AS usage_source_fields,
       COALESCE(NULLIF(a.usage_field_summary,''), NULLIF(ur.usage_evidence_values,''), '') AS usage_field_summary,
       COALESCE(NULLIF(ur.usage_evidence_values,''), NULLIF(a.usage_field_summary,''), '') AS usage_supporting_values,
       COALESCE(ud.likely_snapshot_or_index_usage_date_count, 0) AS likely_snapshot_or_index_usage_date_count,
       COALESCE(ud.snapshot_warning_reasons, '') AS snapshot_warning_reasons,
       dw.created_earliest_utc,
       dw.created_latest_utc,
       dw.modified_earliest_utc,
       dw.modified_latest_utc,
       dw.downloaded_earliest_utc,
       dw.downloaded_latest_utc,
       dw.interesting_or_index_earliest_utc,
       dw.interesting_or_index_latest_utc,
       COALESCE(dw.likely_snapshot_date_count,0) AS likely_snapshot_date_count,
       COALESCE(dw.available_date_fields,'') AS available_date_fields,
       CASE
)SQL" R"SQL(         WHEN COALESCE(ud.usage_date_row_count,0)>0 AND COALESCE(ur.usage_evidence_row_count,0)>0 THEN 'DATE_ATTRIBUTION_AND_USAGE_EVIDENCE'
         WHEN COALESCE(ud.usage_date_row_count,0)>0 THEN 'DATE_ATTRIBUTION_USAGE_FIELDS'
         WHEN COALESCE(ur.usage_evidence_row_count,0)>0 THEN 'USAGE_EVIDENCE_ROWS'
         WHEN COALESCE(a.used_dates_count,0)>0 OR COALESCE(NULLIF(a.last_used_date_utc,''),'')<>'' OR COALESCE(NULLIF(a.first_used_candidate_utc,''),'')<>'' THEN 'ARTIFACT_USAGE_COLUMNS'
         WHEN COALESCE(NULLIF(a.use_count_value,''),'')<>'' OR COALESCE(a.open_count_estimate,0)>0 THEN 'USAGE_COUNT_ONLY'
         ELSE 'NO_USAGE_SIGNAL'
       END AS object_usage_basis
FROM usage_artifacts ua
JOIN artifacts a ON a.artifact_id=ua.artifact_id
LEFT JOIN usage_dates ud ON ud.artifact_id=a.artifact_id
LEFT JOIN usage_rows ur ON ur.artifact_id=a.artifact_id
LEFT JOIN date_wide dw ON dw.artifact_id=a.artifact_id;
)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_usage_timeline_attributed;
CREATE VIEW vw_usage_timeline_attributed AS
SELECT ROW_NUMBER() OVER (ORDER BY COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,''), artifact_id) DESC, artifact_id DESC) AS timeline_id,
       COALESCE(NULLIF(usage_latest_utc,''), NULLIF(last_used_date_utc,''), NULLIF(usage_earliest_utc,'')) AS event_utc,
       'usage_summary' AS date_type,
       usage_source_fields AS source_field,
       object_usage_basis AS usage_reason,
       CASE WHEN COALESCE(likely_snapshot_or_index_usage_date_count,0)>0 THEN 1 ELSE 0 END AS likely_snapshot_or_index_date,
       snapshot_warning_reasons AS snapshot_warning_reason,
       artifact_id,
       source_id,
       store_guid,
       inode_num,
       parent_inode_num,
       file_name,
       display_name,
       best_path,
       path_source,
       path_status,
       content_type,
       logical_size_bytes,
       physical_size_bytes,
       use_count_value,
       open_count_estimate,
       used_dates_count,
       usage_earliest_utc,
       usage_latest_utc,
       fused_usage_dates_utc,
       usage_date_row_count,
       usage_evidence_row_count,
       where_froms,
       confidence
FROM vw_object_usage_summary;

)SQL");
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_apple_messages_database_status;
CREATE VIEW vw_ios_apple_messages_database_status AS
WITH sms_db AS (
  SELECT ios_db_id,source_id,normalized_path,database_name,database_category,app_hint,parse_status,record_inventory_status,extracted_path
  FROM ios_app_database_inventory
  WHERE database_category='APPLE_MESSAGES'
    AND lower(database_name) IN ('sms.db','chat.db')
    AND lower(normalized_path) NOT LIKE '%-wal'
    AND lower(normalized_path) NOT LIKE '%-shm'
), ri AS (
  SELECT ios_db_id,
         SUM(CASE WHEN table_name='message' THEN COALESCE(row_count,0) ELSE 0 END) AS message_rows,
         SUM(CASE WHEN table_name='chat' THEN COALESCE(row_count,0) ELSE 0 END) AS chat_rows,
         SUM(CASE WHEN table_name='attachment' THEN COALESCE(row_count,0) ELSE 0 END) AS attachment_rows,
         SUM(CASE WHEN table_name='handle' THEN COALESCE(row_count,0) ELSE 0 END) AS handle_rows,
         SUM(CASE WHEN table_name IN ('chat_message_join','message_attachment_join','chat_handle_join') THEN COALESCE(row_count,0) ELSE 0 END) AS join_rows,
         GROUP_CONCAT(CASE WHEN record_category IN ('MESSAGE_RECORDS','MESSAGE_ATTACHMENTS','MESSAGE_PARTICIPANTS') THEN table_name || ':' || COALESCE(row_count,0) END) AS relevant_table_counts
  FROM ios_app_database_record_inventory
  GROUP BY ios_db_id
), pr AS (
  SELECT ios_db_id,COUNT(*) AS parsed_message_rows
  FROM ios_app_parsed_records
  WHERE database_category='APPLE_MESSAGES'
  GROUP BY ios_db_id
)
SELECT d.ios_db_id,d.source_id,d.normalized_path,d.database_name,d.app_hint,d.parse_status,d.record_inventory_status,d.extracted_path,
       COALESCE(ri.message_rows,0) AS message_rows,
       COALESCE(ri.chat_rows,0) AS chat_rows,
       COALESCE(ri.attachment_rows,0) AS attachment_rows,
       COALESCE(ri.handle_rows,0) AS handle_rows,
       COALESCE(ri.join_rows,0) AS join_rows,
       COALESCE(pr.parsed_message_rows,0) AS parsed_message_rows,
       COALESCE(ri.relevant_table_counts,'') AS relevant_table_counts,
       CASE
         WHEN COALESCE(pr.parsed_message_rows,0)>0 THEN 'SMS_DB_PRESENT_WITH_PARSED_ROWS'
         WHEN COALESCE(ri.message_rows,0)>0 OR COALESCE(ri.attachment_rows,0)>0 THEN 'SMS_DB_PRESENT_RELEVANT_ROWS_NOT_PARSED'
         WHEN d.ios_db_id IS NOT NULL THEN 'SMS_DB_PRESENT_NO_LIVE_MESSAGE_CHAT_ATTACHMENT_ROWS'
         ELSE 'SMS_DB_NOT_FOUND'
       END AS apple_messages_residency_status,
       'Spotlight may contain message-like text even when the live SMS.db message/chat/attachment tables are empty; treat as Spotlight-only candidate unless a matching app database row is parsed.' AS interpretation_note
FROM sms_db d
LEFT JOIN ri ON ri.ios_db_id=d.ios_db_id
LEFT JOIN pr ON pr.ios_db_id=d.ios_db_id;
)SQL");

    execGuiSqlParts({
        R"VSGUI(
DROP VIEW IF EXISTS vw_ios_whatsapp_database_status;
CREATE VIEW vw_ios_whatsapp_database_status AS
WITH src AS (
  SELECT source_id FROM evidence_sources ORDER BY added_utc LIMIT 1
), wa_db AS (
  SELECT ios_db_id,source_id,normalized_path,database_name,database_category,app_hint,parse_status,record_inventory_status,extracted_path
  FROM ios_app_database_inventory
  WHERE database_category='WHATSAPP'
    AND lower(normalized_path) NOT LIKE '%-wal'
    AND lower(normalized_path) NOT LIKE '%-shm'
), ri AS (
  SELECT ios_db_id,
         SUM(CASE WHEN lower(table_name)='zwamessage' THEN COALESCE(row_count,0) ELSE 0 END) AS message_rows,
         SUM(CASE WHEN lower(table_name)='zwamediaitem' THEN COALESCE(row_count,0) ELSE 0 END) AS media_rows,
         SUM(CASE WHEN lower(table_name)='zwachatsession' THEN COALESCE(row_count,0) ELSE 0 END) AS chat_rows,
         SUM(CASE WHEN lower(table_name) IN ('zwaaddressbookcontact','zwaprofilepushname','zwagroupmember','zwavcardmention') THEN COALESCE(row_count,0) ELSE 0 END) AS contact_or_member_rows,
         SUM(CASE WHEN lower(table_name) LIKE '%call%' THEN COALESCE(row_count,0) ELSE 0 END) AS call_rows,
         GROUP_CONCAT(CASE WHEN record_category IN ('MESSAGE_RECORDS','MESSAGE_ATTACHMENTS','MESSAGE_PARTICIPANTS','CHAT_RECORDS','CALL_RECORDS') THEN table_name || ':' || COALESCE(row_count,0) END) AS relevant_table_counts
  FROM ios_app_database_record_inventory
  GROUP BY ios_db_id
), pr AS (
  SELECT ios_db_id,COUNT(*) AS parsed_whatsapp_rows
  FROM ios_app_parsed_records
  WHERE database_category='WHATSAPP'
  GROUP BY ios_db_id
), status_rows AS (
  SELECT d.ios_db_id,d.source_id,d.normalized_path,d.database_name,d.app_hint,d.parse_status,d.record_inventory_status,d.extracted_path,
         COALESCE(ri.message_rows,0) AS message_rows,
         COALESCE(ri.media_rows,0) AS media_rows,
         COALESCE(ri.chat_rows,0) AS chat_rows,
         COALESCE(ri.contact_or_member_rows,0) AS contact_or_member_rows,
         COALESCE(ri.call_rows,0) AS call_rows,
         COALESCE(pr.parsed_whatsapp_rows,0) AS parsed_whatsapp_rows,
         COALESCE(ri.relevant_table_counts,'') AS relevant_table_counts,
         CASE
           WHEN COALESCE(pr.parsed_whatsapp_rows,0)>0 THEN 'WHATSAPP_DB_PRESENT_WITH_PARSED_ROWS'
           WHEN COALESCE(ri.message_rows,0)>0 OR COALESCE(ri.media_rows,0)>0 OR COALESCE(ri.chat_rows,0)>0 OR COALESCE(ri.call_rows,0)>0 THEN 'WHATSAPP_DB_PRESENT_RELEVANT_ROWS_NOT_PARSED'
           ELSE 'WHATSAPP_DB_PRESENT_NO_RELEVANT_LIVE_ROWS'
         END AS whatsapp_residency_status,
         'WhatsApp rows are parsed from staged iOS app SQLite databases using schema patterns from the uploaded local iLEAPP and WhatsApp references; encrypted or absent databases remain inventory-only.' AS interpretation_note
  FROM wa_db d
  LEFT JOIN ri ON ri.ios_db_id=d.ios_db_id
  LEFT JOIN pr ON pr.ios_db_id=d.ios_db_id
)
SELECT * FROM status_rows
UNION ALL
SELECT NULL AS ios_db_id, COALESCE((SELECT source_id FROM src),'') AS source_id, '' AS normalized_path,
       'ChatStorage.sqlite / ContactsV2.sqlite / CallHistory.sqlite' AS database_name,
       'WhatsApp' AS app_hint, '' AS parse_status, '' AS record_inventory_status, '' AS extracted_path,
       0 AS message_rows, 0 AS media_rows, 0 AS chat_rows, 0 AS contact_or_member_rows, 0 AS call_rows, 0 AS parsed_whatsapp_rows,
       '' AS relevant_table_counts, 'WHATSAPP_DB_NOT_FOUND' AS whatsapp_residency_status,
       'No iOS WhatsApp ChatStorage/Contacts/CallHistory database was identified in the current FFS inventory; Spotlight WhatsApp-like strings, if any, remain Spotlight-only candidates unless another app database or file path supports them.' AS interpretation_note
WHERE NOT EXISTS (SELECT 1 FROM wa_db);

DROP VIEW IF EXISTS vw_ios_keychain_material_inventory;
CREATE VIEW vw_ios_keychain_material_inventory AS
SELECT source_id, normalized_path, original_zip_entry, file_name, extension, size_bytes, zip_modified_utc,
       protection_class_hint, app_container_hint, domain_hint, sha256_status, inventory_notes,
       CASE
         WHEN lower(file_name) IN ('keychain-2.db','keychain-2-debug.db') THEN 'KEYCHAIN_DATABASE'
         WHEN lower(normalized_path) LIKE '%/private/var/keychains/%' THEN 'KEYCHAIN_DIRECTORY_ARTIFACT'
         WHEN lower(normalized_path) LIKE '%keybag%' THEN 'KEYBAG_OR_KEYCHAIN_SUPPORT'
         ELSE 'KEYCHAIN_CORE_MATERIAL'
       END AS keychain_material_type,
       'Inventory only. Presence of keychain/keybag material in the FFS root does not mean WhatsApp or other app data can be decrypted unless parser-specific key extraction and validation are implemented.' AS interpretation_note
FROM ios_ffs_file_inventory
WHERE lower(normalized_path) LIKE '%/private/var/keychains/%'
   OR lower(file_name) IN ('keychain-2.db','keychain-2-debug.db')
   OR lower(normalized_path) LIKE '%keybag%'
ORDER BY keychain_material_type, normalized_path;

DROP VIEW IF EXISTS vw_ios_keychain_support_reference_inventory;
CREATE VIEW vw_ios_keychain_support_reference_inventory AS
SELECT source_id, normalized_path, original_zip_entry, file_name, extension, size_bytes, zip_modified_utc,
       protection_class_hint, app_container_hint, domain_hint, sha256_status, inventory_notes,
       'KEYCHAIN_LIBRARY_OR_CODE_REFERENCE' AS keychain_reference_type,
       'Lower-priority reference: path contains keychain text outside the core /private/var/Keychains or keybag locations. Usually framework/code support, not keychain material.' AS interpretation_note
FROM ios_ffs_file_inventory
WHERE lower(normalized_path) LIKE '%keychain%'
  AND lower(normalized_path) NOT LIKE '%/private/var/keychains/%'
  AND lower(file_name) NOT IN ('keychain-2.db','keychain-2-debug.db')
  AND lower(normalized_path) NOT LIKE '%keybag%'
ORDER BY normalized_path;

DROP VIEW IF EXISTS vw_ios_communications_review_records;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_communications_review_records AS
SELECT ios_app_record_id, source_id, ios_db_id,
       CASE
         WHEN database_category='APPLE_MESSAGES' THEN 'Apple Messages / SMS.db'
         WHEN database_category='WHATSAPP' THEN 'WhatsApp'
         WHEN database_category='CALL_HISTORY' THEN 'Phone / FaceTime Call History'
         WHEN lower(database_category) LIKE '%mail%' OR lower(app_hint) LIKE '%mail%' THEN 'Mail'
         ELSE COALESCE(NULLIF(app_hint,''), database_category)
       END AS communication_source,
       database_normalized_path, database_name, database_category, app_hint, table_name, record_category, source_primary_key,
       record_timestamp_utc, timestamp_source, contact_or_participant, url, title, file_path, item_identifier, text_snippet, parse_status, provenance,
       CASE
         WHEN lower(record_category) LIKE '%call%' THEN 'CALL'
         WHEN lower(record_category) LIKE '%attachment%' OR COALESCE(file_path,'')<>'' THEN 'ATTACHMENT_OR_MEDIA'
         WHEN lower(record_category) LIKE '%participant%' OR lower(record_category) LIKE '%contact%' THEN 'PARTICIPANT_OR_CONTACT'
         WHEN lower(record_category) LIKE '%chat%' THEN 'CHAT_OR_THREAD'
         WHEN lower(record_category) LIKE '%message%' THEN 'MESSAGE'
         ELSE 'COMMUNICATION_RELATED_RECORD'
       END AS communication_record_type,
       CASE WHEN COALESCE(record_timestamp_utc,'')<>'' THEN 'APP_DB_TIMESTAMP' ELSE 'APP_DB_RECORD_NO_NORMALIZED_TIMESTAMP' END AS timeline_basis,
       'Parsed local app database record. Treat as live/acquired app data only for the staged database listed; correlate with Spotlight separately before drawing deletion or residency conclusions.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category IN ('APPLE_MESSAGES','WHATSAPP','CALL_HISTORY')
   OR lower(record_category) LIKE '%message%'
   OR lower(record_category) LIKE '%call%'
   OR lower(record_category) LIKE '%chat%'
   OR lower(record_category) LIKE '%participant%'
   OR lower(table_name) LIKE '%message%'
   OR lower(table_name) LIKE '%chat%'
   OR lower(table_name) LIKE '%call%';

DROP VIEW IF EXISTS vw_ios_communications_review_summary;
CREATE VIEW vw_ios_communications_review_summary AS
SELECT communication_source, database_category, app_hint, record_category, communication_record_type, parse_status,
       COUNT(*) AS parsed_record_count, COUNT(DISTINCT ios_db_id) AS database_count,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc, MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc,
       SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS records_with_contact_or_participant,
       SUM(CASE WHEN COALESCE(text_snippet,'')<>'' THEN 1 ELSE 0 END) AS records_with_text,
       SUM(CASE WHEN COALESCE(url,'')<>'' THEN 1 ELSE 0 END) AS records_with_url,
       SUM(CASE WHEN COALESCE(file_path,'')<>'' THEN 1 ELSE 0 END) AS records_with_file_path,
       MIN(database_normalized_path) AS first_database_path, MAX(database_normalized_path) AS last_database_path
FROM vw_ios_communications_review_records
GROUP BY communication_source, database_category, app_hint, record_category, communication_record_type, parse_status
ORDER BY communication_source, record_category, communication_record_type;

DROP VIEW IF EXISTS vw_ios_spotlight_communication_candidates;
CREATE VIEW vw_ios_spotlight_communication_candidates AS
WITH probe AS (
  SELECT kv.raw_kv_id, kv.source_id, kv.store_guid, kv.source_db, kv.inode_num, kv.store_id, kv.parent_inode_num, kv.field_name, kv.field_value,
         lower(kv.field_value) AS v, rr.last_updated_utc
  FROM raw_key_values kv
  LEFT JOIN raw_records rr ON rr.source_id=kv.source_id AND rr.store_guid=kv.store_guid AND rr.source_db=kv.source_db AND rr.inode_num=kv.inode_num AND COALESCE(rr.store_id,'')=COALESCE(kv.store_id,'')
  WHERE COALESCE(kv.field_value,'')<>''
    AND (kv.store_guid LIKE 'ios_%' OR kv.source_db LIKE '%CoreSpotlight%' OR kv.store_path LIKE '%CoreSpotlight%')
), categorized AS (
  SELECT *,
         CASE
           WHEN v LIKE '%whatsapp%' THEN 'WHATSAPP_TEXT_OR_REFERENCE'
           WHEN v LIKE '%imessage%' OR v LIKE '%sms.db%' OR v LIKE '%/sms/%' OR v LIKE '%com.apple.mobilesms%' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
           WHEN v LIKE '%message%' OR v LIKE '%chat%' OR v LIKE '%conversation%' THEN 'MESSAGE_OR_CHAT_TEXT_CANDIDATE'
           WHEN v LIKE '%facetime%' OR v LIKE '%callhistory%' OR v LIKE '%call history%' THEN 'PHONE_OR_FACETIME_RELATED'
           WHEN v LIKE '%mailto:%' OR (v LIKE '%@%' AND v LIKE '%.%') THEN 'EMAIL_OR_ACCOUNT_RELATED'
           ELSE 'OTHER_COMMUNICATION_RELATED'
         END AS communication_candidate_type
  FROM probe
  WHERE v LIKE '%whatsapp%' OR v LIKE '%imessage%' OR v LIKE '%sms.db%' OR v LIKE '%/sms/%' OR v LIKE '%com.apple.mobilesms%'
     OR v LIKE '%message%' OR v LIKE '%chat%' OR v LIKE '%conversation%' OR v LIKE '%facetime%' OR v LIKE '%callhistory%' OR v LIKE '%call history%'
     OR v LIKE '%mailto:%' OR (v LIKE '%@%' AND v LIKE '%.%')
)
SELECT raw_kv_id, source_id, store_guid, source_db, inode_num AS spotlight_inode_or_object_id, store_id AS spotlight_store_id, parent_inode_num AS spotlight_parent_id,
       field_name, communication_candidate_type, last_updated_utc AS spotlight_last_updated_utc, substr(field_value,1,600) AS string_value_sample,
       CASE
         WHEN communication_candidate_type='WHATSAPP_TEXT_OR_REFERENCE' AND EXISTS (SELECT 1 FROM vw_ios_whatsapp_database_status WHERE whatsapp_residency_status<>'WHATSAPP_DB_NOT_FOUND') THEN 'WHATSAPP_DATABASE_FAMILY_PRESENT'
         WHEN communication_candidate_type='APPLE_MESSAGES_OR_SMS_RELATED' AND EXISTS (SELECT 1 FROM vw_ios_apple_messages_database_status WHERE apple_messages_residency_status<>'SMS_DB_NOT_FOUND') THEN 'SMS_DATABASE_FAMILY_PRESENT'
         WHEN communication_candidate_type='PHONE_OR_FACETIME_RELATED' AND EXISTS (SELECT 1 FROM ios_app_database_inventory WHERE database_category='CALL_HISTORY') THEN 'CALL_HISTORY_DATABASE_FAMILY_PRESENT'
         ELSE 'NO_CONFIRMED_MATCHING_APP_DATABASE_ROW'
       END AS communication_residency_context,
       'Spotlight string candidate only. Review parsed app database views and exact links before treating this as a live message/call/chat record.' AS interpretation_note
FROM categorized
ORDER BY communication_candidate_type, raw_kv_id;

)VSGUI"
    });

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_app_live_activity_timeline;
CREATE VIEW vw_ios_app_live_activity_timeline AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,
       'app_database_record_time' AS timeline_basis,
       'Parsed local app database row; stronger than Spotlight Last_Updated but still requires schema-specific interpretation.' AS interpretation_note
FROM ios_app_parsed_records
WHERE COALESCE(record_timestamp_utc,'')<>''
ORDER BY record_timestamp_utc DESC, database_category, record_category, ios_app_record_id;
)SQL");

    execGuiSqlParts({
        R"VSGUI(
DROP VIEW IF EXISTS vw_ios_spotlight_human_text_values;
CREATE VIEW vw_ios_spotlight_human_text_values AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,store_path,inode_num,store_id,parent_inode_num,field_name,field_value,
         LOWER(COALESCE(field_value,'')) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), labeled AS (
  SELECT *,
    CASE
      WHEN v LIKE '%file://%' OR v LIKE '%/private/var/mobile/%' OR v LIKE '%/var/mobile/%' THEN 'FILE_PATH_OR_ATTACHMENT'
      WHEN v LIKE '%zoom.%' OR v LIKE '%meet.google.%' OR v LIKE '%teams.microsoft.%' OR v LIKE '%webex.%' THEN 'MEETING_OR_CONFERENCE'
      WHEN v LIKE '%.ics%' OR v LIKE '%text/calendar%' OR v LIKE '%vevent%' OR v LIKE '%calendar.google.com%' OR v LIKE '%/calendar/%' THEN 'CALENDAR_OR_INVITATION'
      WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' THEN 'WEB_OR_URL'
      WHEN v LIKE 'from:%' OR v LIKE 'to:%' OR v LIKE 'cc:%' OR v LIKE '%mailto:%' OR (v LIKE '%@%' AND v NOT LIKE '%http://%' AND v NOT LIKE '%https://%') THEN 'EMAIL_OR_ACCOUNT_TEXT'
      WHEN v LIKE '%net.whatsapp.whatsapp%' OR v LIKE '%chat.whatsapp.com%' OR v LIKE '%wa.me/%' OR v LIKE '%api.whatsapp.com%' OR v LIKE '%whatsapp group%' THEN 'WHATSAPP_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.whispersystems.signal%' OR v LIKE '%signal.messenger%' OR v LIKE '%signal.org/%' THEN 'SIGNAL_TEXT_OR_REFERENCE'
      WHEN v LIKE '%org.telegram%' OR v LIKE '%telegram.messenger%' OR v LIKE '%t.me/%' OR v LIKE '%telegram.me/%' THEN 'TELEGRAM_TEXT_OR_REFERENCE'
      ELSE 'OTHER_HUMAN_READABLE_TEXT'
    END AS human_text_category,
    TRIM(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(REPLACE(COALESCE(field_value,''),
      char(13),' '), char(10),' '), char(9),' '), '<br>',' '), '<br/>',' '), '<br />',' '),
      '&amp;','&'), '&lt;','<'), '&gt;','>'), '&nbsp;',' '), '&quot;','"')) AS readable_text
  FROM probes
)
SELECT l.raw_kv_id,COALESCE(r.raw_record_id,0) AS raw_record_id,l.source_id,l.store_guid,l.source_db,l.inode_num,l.store_id,l.parent_inode_num,l.field_name,
       l.human_text_category,
       LENGTH(COALESCE(l.field_value,'')) AS original_value_length,
       substr(l.readable_text,1,3000) AS readable_text_sample,
       CASE
         WHEN l.human_text_category IN ('FILE_PATH_OR_ATTACHMENT','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION','WEB_OR_URL','EMAIL_OR_ACCOUNT_TEXT') THEN 'HIGH_HUMAN_REVIEW_VALUE'
         WHEN LENGTH(l.readable_text)>=24 THEN 'MEDIUM_HUMAN_REVIEW_VALUE'
         ELSE 'LOW_HUMAN_REVIEW_VALUE'
       END AS review_priority,
       'Generic iOS CoreSpotlight text recovery; formal CoreSpotlight property names/dbStr maps remain a later parser phase.' AS interpretation_note
FROM labeled l
LEFT JOIN (
  SELECT source_id,store_guid,source_db,inode_num,COALESCE(store_id,'') AS store_id_key,MIN(raw_record_id) AS raw_record_id
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,inode_num,COALESCE(store_id,'')
) r ON r.source_id=l.source_id AND r.store_guid=l.store_guid AND r.source_db=l.source_db AND r.inode_num=l.inode_num AND r.store_id_key=COALESCE(l.store_id,'')
WHERE LENGTH(l.readable_text)>=4;

DROP VIEW IF EXISTS vw_ios_spotlight_human_text_rollup;
CREATE VIEW vw_ios_spotlight_human_text_rollup AS
WITH text_values AS (
  SELECT v.*, r.last_updated_utc, r.record_state
  FROM vw_ios_spotlight_human_text_values v
  LEFT JOIN raw_records r ON r.raw_record_id=v.raw_record_id
)
SELECT raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT human_text_category) AS distinct_text_category_count,
       GROUP_CONCAT(DISTINCT human_text_category) AS human_text_categories,
       MAX(CASE WHEN review_priority='HIGH_HUMAN_REVIEW_VALUE' THEN 1 ELSE 0 END) AS has_high_review_value_text,
       MIN(NULLIF(last_updated_utc,'')) AS last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       substr(GROUP_CONCAT(field_name || '=' || readable_text_sample, ' || '),1,6000) AS readable_text_rollup_sample,
       'Record-level human-readable text rollup from iOS CoreSpotlight string probes.' AS interpretation_note
FROM text_values
GROUP BY raw_record_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num;


DROP VIEW IF EXISTS vw_ios_spotlight_investigative_items_with_dates;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_investigative_items_with_dates AS
SELECT v.raw_kv_id,
       v.raw_record_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.original_value_length,
       v.readable_text_sample,
       v.review_priority,
       dp.spotlight_date_utc,
       dp.spotlight_date_source_field,
       dp.spotlight_date_source_table,
       dp.spotlight_date_raw_value,
       dp.spotlight_date_parse_method,
       dp.spotlight_date_type,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%creation%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%created%' THEN 'created_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modification%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%modified%' THEN 'modified_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%access%' THEN 'accessed_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%open%' OR lower(COALESCE(dp.spotlight_date_source_fields,dp.spotlight_date_source_field,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'metadata_seen_or_index_updated'
         ELSE 'unclassified_spotlight_date_candidate'
       END AS spotlight_date_semantic_class,
       dp.spotlight_date_source_evidence,
       dp.date_validation_hint,
       CASE
         WHEN lower(COALESCE(dp.spotlight_date_source_field,''))='last_updated' THEN 'Do not report as created/modified/accessed/opened. This is CoreSpotlight metadata/index update timing unless another decoded field supports activity semantics.'
         WHEN COALESCE(dp.spotlight_date_source_field,'')<>'' THEN 'Report only with the listed raw Spotlight source field, raw value, parse method, and validation hint.'
         ELSE 'No direct Spotlight date was recovered for this text value.'
       END AS date_reporting_caution,
       'Spotlight/CoreSpotlight extracted text item with attached date provenance where directly linkable by raw_record_id. FFS/app database data is supporting context only.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
LEFT JOIN vw_ios_spotlight_date_provenance dp ON dp.raw_record_id=v.raw_record_id;

DROP VIEW IF EXISTS vw_ios_spotlight_date_field_summary;
CREATE VIEW vw_ios_spotlight_date_field_summary AS
WITH dates AS (
  SELECT source_id, store_guid, source_db, field_name, parse_method, date_type,
         parsed_utc, field_value, inode_num, store_id,
         CASE
           WHEN lower(COALESCE(field_name,'')) LIKE '%creation%' OR lower(COALESCE(field_name,'')) LIKE '%created%' THEN 'created_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%modification%' OR lower(COALESCE(field_name,'')) LIKE '%modified%' THEN 'modified_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%access%' THEN 'accessed_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%open%' OR lower(COALESCE(field_name,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
           WHEN lower(COALESCE(field_name,''))='last_updated' THEN 'metadata_seen_or_index_updated'
           ELSE 'unclassified_spotlight_date_candidate'
         END AS spotlight_date_semantic_class
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
)
SELECT source_id, store_guid, source_db, field_name AS spotlight_date_source_field,
       spotlight_date_semantic_class, COALESCE(date_type,'') AS raw_date_type,
       COALESCE(parse_method,'') AS parse_method,
       COUNT(*) AS date_candidate_count,
       COUNT(DISTINCT COALESCE(inode_num,'') || ':' || COALESCE(store_id,'')) AS distinct_spotlight_record_count,
       MIN(parsed_utc) AS earliest_parsed_utc,
       MAX(parsed_utc) AS latest_parsed_utc,
       substr(MIN(field_value),1,500) AS min_raw_value_sample,
       substr(MAX(field_value),1,500) AS max_raw_value_sample,
       CASE
         WHEN spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'CoreSpotlight metadata/index update timing; do not report as created/modified/accessed/opened usage without another decoded field.'
         WHEN spotlight_date_semantic_class LIKE '%candidate' THEN 'Candidate semantic class inferred from Spotlight raw date field name; validate field meaning before reporting.'
         ELSE 'Unclassified Spotlight date candidate; validate against raw_date_candidates and source Store-V2 record.'
       END AS reporting_caution
FROM dates
GROUP BY source_id, store_guid, source_db, field_name, spotlight_date_semantic_class, date_type, parse_method;

DROP VIEW IF EXISTS vw_ios_spotlight_investigative_item_date_evidence;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_investigative_item_date_evidence AS
WITH date_evidence AS (
  SELECT raw_date_id, source_id, store_guid, source_db, store_path, inode_num, store_id,
         parent_inode_num, file_name, best_path,
         field_name AS spotlight_date_source_field,
         field_value AS spotlight_date_raw_value,
         parsed_utc AS spotlight_date_utc,
         parse_method AS spotlight_date_parse_method,
         date_type AS spotlight_date_type,
         association_status,
         association_confidence,
         CASE
           WHEN lower(COALESCE(field_name,'')) LIKE '%creation%' OR lower(COALESCE(field_name,'')) LIKE '%created%' THEN 'created_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%modification%' OR lower(COALESCE(field_name,'')) LIKE '%modified%' THEN 'modified_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%access%' THEN 'accessed_date_candidate'
           WHEN lower(COALESCE(field_name,'')) LIKE '%open%' OR lower(COALESCE(field_name,'')) LIKE '%used%' THEN 'opened_or_used_date_candidate'
           WHEN lower(COALESCE(field_name,''))='last_updated' THEN 'metadata_seen_or_index_updated'
           ELSE 'unclassified_spotlight_date_candidate'
         END AS spotlight_date_semantic_class
  FROM raw_date_candidates
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(parsed_utc,'')<>''
)
SELECT v.raw_kv_id,
       v.raw_record_id,
       d.raw_date_id,
       v.source_id,
       v.store_guid,
       v.source_db,
       v.inode_num AS spotlight_inode_or_object_id,
       v.store_id AS spotlight_store_id,
       v.parent_inode_num,
       v.field_name AS spotlight_value_source_field,
       v.human_text_category,
       v.review_priority,
       v.original_value_length,
       v.readable_text_sample,
       d.spotlight_date_utc,
       d.spotlight_date_source_field,
       'raw_date_candidates' AS spotlight_date_source_table,
       d.spotlight_date_raw_value,
       d.spotlight_date_parse_method,
       d.spotlight_date_type,
       d.spotlight_date_semantic_class,
       d.association_status AS date_association_status,
       d.association_confidence AS date_association_confidence,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(v.raw_kv_id AS TEXT),'') || '; field_name=' || COALESCE(v.field_name,'') AS value_validation_locator,
       'raw_date_candidates.raw_date_id=' || COALESCE(CAST(d.raw_date_id AS TEXT),'') || '; field_name=' || COALESCE(d.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(d.spotlight_date_raw_value,'') || '; parsed_utc=' || COALESCE(d.spotlight_date_utc,'') || '; parse_method=' || COALESCE(d.spotlight_date_parse_method,'') AS date_validation_locator,
       'source_db=' || COALESCE(v.source_db,'') || '; store_guid=' || COALESCE(v.store_guid,'') || '; raw_record_id=' || COALESCE(CAST(v.raw_record_id AS TEXT),'') || '; inode_or_object_id=' || COALESCE(v.inode_num,'') || '; store_id=' || COALESCE(v.store_id,'') AS spotlight_record_locator,
       CASE
         WHEN d.spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'Date is linked to this Spotlight record but represents CoreSpotlight metadata/index update timing unless a separately decoded field supports user activity.'
         WHEN d.spotlight_date_semantic_class IN ('created_date_candidate','modified_date_candidate','accessed_date_candidate','opened_or_used_date_candidate') THEN 'Date is directly linked to this recovered Spotlight value through the same Store-V2 record; validate raw field semantics before reporting as activity.'
         ELSE 'Date is directly linked to this recovered Spotlight value through the same Store-V2 record, but semantic meaning is not yet classified.'
       END AS date_reporting_caution,
       'Each row links one recovered human-readable Spotlight value to one raw Spotlight date candidate from the same Store-V2 record. Use validation locator fields to verify in the parsed raw tables and original store.db.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values v
JOIN date_evidence d ON d.source_id=v.source_id
                    AND d.store_guid=v.store_guid
                    AND d.source_db=v.source_db
                    AND d.inode_num=v.inode_num
                    AND COALESCE(d.store_id,'')=COALESCE(v.store_id,'');

)VSGUI"
    });

    execGuiSql(R"SQL(

DROP VIEW IF EXISTS vw_ios_spotlight_high_value_timeline;
CREATE VIEW vw_ios_spotlight_high_value_timeline AS
WITH base AS (
  SELECT * FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE review_priority IN ('HIGH_HUMAN_REVIEW_VALUE','MEDIUM_HUMAN_REVIEW_VALUE')
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.original_value_length,
       b.readable_text_sample,b.review_priority,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_source_table,
       b.spotlight_date_raw_value,b.spotlight_date_parse_method,b.spotlight_date_type,
       b.spotlight_date_semantic_class,b.date_validation_hint,b.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FILE_PATH_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       CASE
         WHEN b.spotlight_date_semantic_class='metadata_seen_or_index_updated' THEN 'SPOTLIGHT_INDEX_TIME_WITH_VALUE_CONTEXT'
         WHEN b.spotlight_date_semantic_class LIKE '%candidate' THEN 'SPOTLIGHT_ACTIVITY_DATE_CANDIDATE_WITH_VALUE_CONTEXT'
         ELSE 'SPOTLIGHT_VALUE_WITH_UNCLASSIFIED_DATE_CONTEXT'
       END AS investigative_timeline_basis,
       'Spotlight-first high-value timeline. FFS and app database fields are context/corroboration only; the Spotlight value/date fields remain the primary evidence to validate.' AS interpretation_note
FROM base b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_file_reference_review;
CREATE VIEW vw_ios_spotlight_file_reference_review AS
WITH ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,
         matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.readable_text_sample AS spotlight_file_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(f.residency_status,'NO_EXACT_FFS_PATH_LINK') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(CAST(f.matched_size_bytes AS TEXT),'') AS matched_size_bytes,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       CASE WHEN COALESCE(f.residency_status,'')='PRESENT_AS_FILE_IN_FFS' THEN 'SPOTLIGHT_PATH_PRESENT_IN_FFS_INVENTORY'
            WHEN COALESCE(f.residency_status,'')<>'' THEN f.residency_status
            ELSE 'SPOTLIGHT_FILE_REFERENCE_NO_EXACT_FFS_MATCH_IN_CURRENT_LINK_VIEW' END AS file_reference_status,
       'Spotlight/CoreSpotlight file/path reference with date provenance. FFS presence supports current-file existence only and is not proof of use or deletion by itself.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN ffs f ON f.reference_id=b.raw_kv_id
WHERE b.human_text_category='FILE_PATH_OR_ATTACHMENT';

)SQL");

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_url_reference_review;
CREATE VIEW vw_ios_spotlight_url_reference_review AS
WITH vals AS (
  SELECT *, lower(COALESCE(readable_text_sample,'')) AS v
  FROM vw_ios_spotlight_investigative_items_with_dates
  WHERE human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION')
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT v.raw_kv_id,v.raw_record_id,v.source_id,v.store_guid,v.source_db,
       v.spotlight_inode_or_object_id,v.spotlight_store_id,v.parent_inode_num,
       v.spotlight_value_source_field,v.human_text_category,v.readable_text_sample AS spotlight_url_or_web_reference,
       CASE
         WHEN instr(v.v,'https://')>0 THEN substr(v.v,instr(v.v,'https://'),300)
         WHEN instr(v.v,'http://')>0 THEN substr(v.v,instr(v.v,'http://'),300)
         WHEN instr(v.v,'www.')>0 THEN substr(v.v,instr(v.v,'www.'),300)
         ELSE substr(v.v,1,300)
       END AS normalized_url_reference_sample,
       v.spotlight_date_utc,v.spotlight_date_source_field,v.spotlight_date_raw_value,
       v.spotlight_date_parse_method,v.spotlight_date_semantic_class,v.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       COALESCE(a.earliest_record_timestamp_utc,'') AS app_family_earliest_record_timestamp_utc,
       COALESCE(a.latest_record_timestamp_utc,'') AS app_family_latest_record_timestamp_utc,
       'Spotlight/CoreSpotlight URL/web-like reference with date provenance. Browser/app database fields are supporting context and not an exact value match unless separately validated.' AS interpretation_note
FROM vals v
LEFT JOIN app a ON a.candidate_id=v.raw_kv_id;

)SQL");

    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_spotlight_account_contact_reference_review;
CREATE VIEW vw_ios_spotlight_account_contact_reference_review AS
WITH app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,
         parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT b.raw_kv_id,b.raw_record_id,b.source_id,b.store_guid,b.source_db,
       b.spotlight_inode_or_object_id,b.spotlight_store_id,b.parent_inode_num,
       b.spotlight_value_source_field,b.human_text_category,b.readable_text_sample AS spotlight_account_or_contact_reference,
       b.spotlight_date_utc,b.spotlight_date_source_field,b.spotlight_date_raw_value,
       b.spotlight_date_parse_method,b.spotlight_date_semantic_class,b.date_validation_hint,
       COALESCE(a.app_db_link_status,'NO_APP_DB_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.parsed_record_count,0) AS app_family_parsed_record_count,
       'Spotlight/CoreSpotlight account/contact-like reference with date provenance. Treat as a Spotlight value first; app database context is family-level unless exact string matching is later added.' AS interpretation_note
FROM vw_ios_spotlight_investigative_items_with_dates b
LEFT JOIN app a ON a.candidate_id=b.raw_kv_id
WHERE b.human_text_category IN ('EMAIL_OR_ACCOUNT_TEXT','WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE');

DROP VIEW IF EXISTS vw_ios_spotlight_decode_gap_summary;
CREATE VIEW vw_ios_spotlight_decode_gap_summary AS
WITH gaps AS (
  SELECT source_id,store_guid,source_db,decode_gap_status,last_updated_utc
  FROM vw_ios_spotlight_decode_gap_records
)
SELECT g.source_id,g.store_guid,g.source_db,g.decode_gap_status,
       COUNT(*) AS gap_record_count,
       MIN(NULLIF(g.last_updated_utc,'')) AS earliest_gap_last_updated_utc,
       MAX(NULLIF(g.last_updated_utc,'')) AS latest_gap_last_updated_utc,
       COALESCE(dc.raw_record_count,0) AS store_raw_record_count,
       COALESCE(dc.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(dc.human_text_value_count,0) AS human_text_value_count,
       COALESCE(dc.pct_records_with_human_text,'') AS pct_records_with_human_text,
       COALESCE(dc.decode_failures,0) AS native_decode_failures,
       COALESCE(dc.decode_status,'') AS native_decode_status,
       'Summary of Spotlight/CoreSpotlight records parsed at header level but lacking recovered key/value or human-readable text values. This is the primary native parser improvement target list.' AS interpretation_note
FROM gaps g
LEFT JOIN vw_ios_spotlight_decode_coverage_summary dc ON dc.source_id=g.source_id AND dc.store_guid=g.store_guid AND dc.source_db=g.source_db
GROUP BY g.source_id,g.store_guid,g.source_db,g.decode_gap_status;

)SQL");

    execGuiSqlParts({
        R"VSGUI(

DROP VIEW IF EXISTS vw_ios_spotlight_entity_review;
CREATE VIEW vw_ios_spotlight_entity_review AS
WITH base AS (
  SELECT b.*,
         lower(COALESCE(b.readable_text_sample,'')) AS lower_text
  FROM vw_ios_spotlight_investigative_items_with_dates b
  WHERE COALESCE(b.readable_text_sample,'')<>''
), typed AS (
  SELECT b.*,
         CASE
           WHEN b.human_text_category IN ('WEB_OR_URL','MEETING_OR_CONFERENCE','CALENDAR_OR_INVITATION') THEN 'URL_OR_WEB_REFERENCE'
           WHEN b.human_text_category='FILE_PATH_OR_ATTACHMENT' THEN 'FILE_OR_ATTACHMENT_REFERENCE'
           WHEN b.human_text_category='EMAIL_OR_ACCOUNT_TEXT' THEN 'ACCOUNT_OR_EMAIL_REFERENCE'
           WHEN b.human_text_category IN ('WHATSAPP_TEXT_OR_REFERENCE','SIGNAL_TEXT_OR_REFERENCE','TELEGRAM_TEXT_OR_REFERENCE') THEN 'COMMUNICATION_APP_REFERENCE'
           WHEN b.human_text_category LIKE '%MESSAGE%' THEN 'MESSAGE_OR_COMMUNICATION_TEXT'
           ELSE 'OTHER_SPOTLIGHT_TEXT_REFERENCE'
         END AS entity_type
  FROM base b
), normalized AS (
  SELECT t.*,
         CASE
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'https://')>0 THEN substr(t.lower_text,instr(t.lower_text,'https://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'http://')>0 THEN substr(t.lower_text,instr(t.lower_text,'http://'),512)
           WHEN t.entity_type='URL_OR_WEB_REFERENCE' AND instr(t.lower_text,'www.')>0 THEN substr(t.lower_text,instr(t.lower_text,'www.'),512)
           WHEN t.entity_type='FILE_OR_ATTACHMENT_REFERENCE' THEN replace(replace(replace(replace(t.lower_text,'file://',''),'<',''),'>',''),'\\','/')
           ELSE trim(t.lower_text)
         END AS normalized_entity_value
  FROM typed t
), ffs AS (
  SELECT reference_id,residency_status,confidence,matched_file_name,matched_size_bytes,matched_zip_modified_utc,matched_protection_class,matched_app_container,matched_domain
  FROM vw_ios_spotlight_to_ffs_object_links
), app AS (
  SELECT candidate_id,app_db_link_status,database_category,database_name,app_hint,matched_record_category,matched_table_name,parsed_record_count,earliest_record_timestamp_utc,latest_record_timestamp_utc,sample_parsed_value
  FROM vw_ios_spotlight_to_app_db_record_links
)
SELECT n.raw_kv_id,
       n.raw_record_id,
       n.source_id,
       n.store_guid,
       n.source_db,
       n.spotlight_inode_or_object_id,
       n.spotlight_store_id,
       n.parent_inode_num,
       n.entity_type,
       n.human_text_category,
       n.review_priority,
       n.spotlight_value_source_field,
       n.normalized_entity_value,
       n.readable_text_sample,
       n.original_value_length,
       n.spotlight_date_utc,
       n.spotlight_date_source_field,
       n.spotlight_date_raw_value,
       n.spotlight_date_parse_method,
       n.spotlight_date_semantic_class,
       n.date_validation_hint,
       n.date_reporting_caution,
       COALESCE(f.residency_status,'NO_FFS_LINK_CONTEXT') AS ffs_residency_status,
       COALESCE(f.confidence,'') AS ffs_match_confidence,
       COALESCE(f.matched_file_name,'') AS matched_file_name,
       COALESCE(f.matched_zip_modified_utc,'') AS matched_zip_modified_utc,
       COALESCE(f.matched_protection_class,'') AS matched_protection_class,
       COALESCE(f.matched_app_container,'') AS matched_app_container,
       COALESCE(f.matched_domain,'') AS matched_domain,
       COALESCE(a.app_db_link_status,'NO_APP_DB_LINK_CONTEXT') AS app_db_link_status,
       COALESCE(a.database_category,'') AS app_database_category,
       COALESCE(a.database_name,'') AS app_database_name,
       COALESCE(a.app_hint,'') AS app_hint,
       COALESCE(a.matched_record_category,'') AS matched_record_category,
       COALESCE(a.matched_table_name,'') AS matched_table_name,
       COALESCE(a.sample_parsed_value,'') AS sample_app_db_value,
       'raw_key_values.raw_kv_id=' || COALESCE(CAST(n.raw_kv_id AS TEXT),'') || '; raw_records.raw_record_id=' || COALESCE(CAST(n.raw_record_id AS TEXT),'') || '; field_name=' || COALESCE(n.spotlight_value_source_field,'') AS reference_validation_locator,
       'raw_date_candidates.field_name=' || COALESCE(n.spotlight_date_source_field,'') || '; raw_value=' || COALESCE(n.spotlight_date_raw_value,'') || '; parse_method=' || COALESCE(n.spotlight_date_parse_method,'') AS date_validation_locator,
       'Spotlight-first entity view. The entity/value and date columns originate from CoreSpotlight/Spotlight parsed records; app DB and FFS fields are corroborating context only.' AS interpretation_note
FROM normalized n
LEFT JOIN ffs f ON f.reference_id=n.raw_kv_id
LEFT JOIN app a ON a.candidate_id=n.raw_kv_id;

DROP VIEW IF EXISTS vw_ios_spotlight_entity_summary;
CREATE VIEW vw_ios_spotlight_entity_summary AS
SELECT entity_type,
       human_text_category,
       review_priority,
       store_guid,
       source_db,
       spotlight_value_source_field,
       spotlight_date_semantic_class,
       COUNT(*) AS entity_row_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT normalized_entity_value) AS distinct_normalized_entity_count,
       SUM(CASE WHEN ffs_residency_status='PRESENT_AS_FILE_IN_FFS' THEN 1 ELSE 0 END) AS ffs_present_context_count,
       SUM(CASE WHEN app_db_link_status LIKE 'PRESENT%' OR app_db_link_status LIKE '%PRESENT%' THEN 1 ELSE 0 END) AS app_db_present_context_count,
       MIN(NULLIF(spotlight_date_utc,'')) AS earliest_spotlight_date_utc,
       MAX(NULLIF(spotlight_date_utc,'')) AS latest_spotlight_date_utc,
       MIN(substr(normalized_entity_value,1,240)) AS min_sample_entity,
       MAX(substr(normalized_entity_value,1,240)) AS max_sample_entity,
       'Spotlight entity summary. Counts are derived from recovered CoreSpotlight text/probe values and their Spotlight date provenance; app/FFS context is only supporting context.' AS interpretation_note
FROM vw_ios_spotlight_entity_review
GROUP BY entity_type,human_text_category,review_priority,store_guid,source_db,spotlight_value_source_field,spotlight_date_semantic_class;

DROP VIEW IF EXISTS vw_ios_spotlight_native_parser_targets;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_native_parser_targets AS
SELECT source_id,
       store_guid,
       source_db,
       'RECORDS_WITHOUT_RECOVERED_TEXT' AS parser_target_type,
       decode_gap_status AS target_name,
       gap_record_count AS target_count,
       store_raw_record_count AS store_raw_record_count,
       recovered_key_value_count AS recovered_key_value_count,
       human_text_value_count AS human_text_value_count,
       pct_records_with_human_text AS pct_records_with_human_text,
       native_decode_failures AS native_decode_failures,
       CASE WHEN gap_record_count>100000 THEN 'HIGH' WHEN gap_record_count>10000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Improve native CoreSpotlight property/dictionary/value decoding for records that parse at header level but do not yield recovered text/key-value rows.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_decode_gap_summary
UNION ALL
SELECT source_id,
       store_guid,
       source_db,
       'GENERIC_STRING_PROBE_FIELD' AS parser_target_type,
       field_name AS target_name,
       value_row_count AS target_count,
       NULL AS store_raw_record_count,
       value_row_count AS recovered_key_value_count,
       distinct_record_count AS human_text_value_count,
       '' AS pct_records_with_human_text,
       NULL AS native_decode_failures,
       CASE WHEN value_row_count>10000 THEN 'HIGH' WHEN value_row_count>1000 THEN 'MEDIUM' ELSE 'LOW' END AS parser_priority,
       'Map generic __native_core_probe_string_* fields back to real CoreSpotlight property names/types where possible.' AS recommended_next_step,
       interpretation_note
FROM vw_ios_spotlight_field_coverage_summary
WHERE field_decode_status='GENERIC_NATIVE_STRING_PROBE';

DROP VIEW IF EXISTS vw_ios_spotlight_decode_coverage_summary;
CREATE VIEW vw_ios_spotlight_decode_coverage_summary AS
WITH rr AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS raw_record_count,
         SUM(CASE WHEN COALESCE(last_updated_utc,'')<>'' THEN 1 ELSE 0 END) AS records_with_last_updated,
         MIN(NULLIF(last_updated_utc,'')) AS earliest_last_updated_utc,
         MAX(NULLIF(last_updated_utc,'')) AS latest_last_updated_utc
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), kv AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS recovered_key_value_count,
         COUNT(DISTINCT field_name) AS recovered_field_name_count,
         COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS records_with_recovered_values
  FROM raw_key_values
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db
), ht AS (
  SELECT source_id,store_guid,source_db,
         COUNT(*) AS human_text_value_count,
         COUNT(DISTINCT raw_record_id) AS records_with_human_text,
         COUNT(DISTINCT human_text_category) AS human_text_category_count
  FROM vw_ios_spotlight_human_text_values
  GROUP BY source_id,store_guid,source_db
), nd AS (
  SELECT source_id,store_guid,source_db,
         MAX(decode_mode) AS decode_mode,
         MAX(spotlight_version) AS spotlight_version,
         MAX(properties_count) AS native_property_count,
         MAX(categories_count) AS native_category_count,
         MAX(metadata_blocks) AS metadata_blocks,
         MAX(decompressed_blocks) AS decompressed_blocks,
         MAX(failures) AS decode_failures,
         MAX(status) AS decode_status,
         MAX(message) AS decode_message
  FROM native_decode_attempts
  GROUP BY source_id,store_guid,source_db
)
SELECT rr.source_id,rr.store_guid,rr.source_db,
       COALESCE(nd.decode_mode,'') AS decode_mode,
       COALESCE(nd.spotlight_version,0) AS spotlight_version,
       rr.raw_record_count,
       COALESCE(kv.recovered_key_value_count,0) AS recovered_key_value_count,
       COALESCE(kv.recovered_field_name_count,0) AS recovered_field_name_count,
       COALESCE(kv.records_with_recovered_values,0) AS records_with_recovered_values,
       COALESCE(ht.human_text_value_count,0) AS human_text_value_count,
       COALESCE(ht.records_with_human_text,0) AS records_with_human_text,
       COALESCE(ht.human_text_category_count,0) AS human_text_category_count,
       CASE WHEN rr.raw_record_count>0 THEN printf('%.2f', 100.0 * COALESCE(ht.records_with_human_text,0) / rr.raw_record_count) ELSE '0.00' END AS pct_records_with_human_text,
       COALESCE(nd.native_property_count,0) AS native_property_count,
       COALESCE(nd.native_category_count,0) AS native_category_count,
       COALESCE(nd.metadata_blocks,0) AS metadata_blocks,
       COALESCE(nd.decompressed_blocks,0) AS decompressed_blocks,
       COALESCE(nd.decode_failures,0) AS decode_failures,
       COALESCE(nd.decode_status,'NO_NATIVE_DECODE_ATTEMPT_ROW') AS decode_status,
       rr.earliest_last_updated_utc,rr.latest_last_updated_utc,
       CASE WHEN COALESCE(nd.native_property_count,0)=0 THEN 'PROPERTY_DICTIONARY_NOT_DECODED_GENERIC_PROBES_ONLY'
            WHEN COALESCE(kv.recovered_key_value_count,0)=0 THEN 'NO_KEY_VALUES_RECOVERED'
            ELSE 'GENERIC_TEXT_VALUES_RECOVERED' END AS spotlight_decode_interpretation,
       'Spotlight-first coverage view. App/FFS correlation is supporting context; this row measures native CoreSpotlight record/value recovery.' AS interpretation_note
FROM rr
LEFT JOIN kv ON kv.source_id=rr.source_id AND kv.store_guid=rr.store_guid AND kv.source_db=rr.source_db
LEFT JOIN ht ON ht.source_id=rr.source_id AND ht.store_guid=rr.store_guid AND ht.source_db=rr.source_db
LEFT JOIN nd ON nd.source_id=rr.source_id AND nd.store_guid=rr.store_guid AND nd.source_db=rr.source_db;

DROP VIEW IF EXISTS vw_ios_spotlight_field_coverage_summary;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_field_coverage_summary AS
SELECT source_id,store_guid,source_db,field_name,
       COUNT(*) AS value_row_count,
       COUNT(DISTINCT inode_num || ':' || COALESCE(store_id,'')) AS distinct_record_count,
       MIN(LENGTH(COALESCE(field_value,''))) AS min_value_length,
       MAX(LENGTH(COALESCE(field_value,''))) AS max_value_length,
       substr(MIN(COALESCE(field_value,'')),1,1000) AS min_sample_value,
       substr(MAX(COALESCE(field_value,'')),1,1000) AS max_sample_value,
       CASE WHEN field_name LIKE '__native_core_probe_string_%' THEN 'GENERIC_NATIVE_STRING_PROBE'
            WHEN field_name LIKE '__native_%' THEN 'GENERIC_NATIVE_FIELD'
            ELSE 'NAMED_SPOTLIGHT_FIELD' END AS field_decode_status,
       'Field coverage summary from recovered Spotlight key/value rows. Generic probe names indicate values recovered before formal property-name mapping.' AS interpretation_note
FROM raw_key_values
WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  AND COALESCE(field_value,'')<>''
GROUP BY source_id,store_guid,source_db,field_name;

DROP VIEW IF EXISTS vw_ios_spotlight_text_category_summary;
CREATE VIEW vw_ios_spotlight_text_category_summary AS
SELECT human_text_category,review_priority,
       COUNT(*) AS text_value_count,
       COUNT(DISTINCT raw_record_id) AS distinct_spotlight_record_count,
       COUNT(DISTINCT store_guid) AS store_count,
       MIN(original_value_length) AS min_original_value_length,
       MAX(original_value_length) AS max_original_value_length,
       substr(MIN(readable_text_sample),1,1000) AS min_sample_text,
       substr(MAX(readable_text_sample),1,1000) AS max_sample_text,
       'Spotlight recovered text category summary. Categories are triage labels over Spotlight text values, not final CoreSpotlight property names.' AS interpretation_note
FROM vw_ios_spotlight_human_text_values
GROUP BY human_text_category,review_priority;

DROP VIEW IF EXISTS vw_ios_spotlight_record_review;
CREATE VIEW vw_ios_spotlight_record_review AS
WITH text_roll AS (
  SELECT raw_record_id,text_value_count,distinct_text_category_count,human_text_categories,has_high_review_value_text,readable_text_rollup_sample
  FROM vw_ios_spotlight_human_text_rollup
), date_one AS (
  SELECT raw_record_id,
         MAX(spotlight_date_utc) AS spotlight_date_utc,
         MAX(spotlight_date_source_field) AS spotlight_date_source_field,
         MAX(spotlight_date_source_table) AS spotlight_date_source_table,
         MAX(spotlight_date_raw_value) AS spotlight_date_raw_value,
         MAX(spotlight_date_parse_method) AS spotlight_date_parse_method,
         MAX(spotlight_date_type) AS spotlight_date_type,
         MAX(spotlight_date_source_evidence) AS spotlight_date_source_evidence,
         MAX(date_validation_hint) AS date_validation_hint,
         COUNT(*) AS collapsed_date_candidate_count
  FROM vw_ios_spotlight_date_provenance
  GROUP BY raw_record_id
)
SELECT r.raw_record_id,r.source_id,r.store_guid,r.source_db,r.inode_num AS spotlight_inode_or_object_id,r.store_id AS spotlight_store_id,r.parent_inode_num,
       COALESCE(dp.spotlight_date_utc,r.last_updated_utc) AS spotlight_date_utc,
       COALESCE(dp.spotlight_date_source_field,'Last_Updated') AS spotlight_date_source_field,
       COALESCE(dp.spotlight_date_source_table,'raw_records') AS spotlight_date_source_table,
       COALESCE(dp.spotlight_date_raw_value,r.last_updated_raw) AS spotlight_date_raw_value,
       COALESCE(dp.spotlight_date_parse_method,'native_epoch_microseconds') AS spotlight_date_parse_method,
       COALESCE(dp.spotlight_date_type,'metadata_seen_or_index_updated') AS spotlight_date_type,
       COALESCE(dp.spotlight_date_source_evidence,'Last_Updated=' || COALESCE(r.last_updated_raw,'') || ' -> ' || COALESCE(r.last_updated_utc,'')) AS spotlight_date_source_evidence,
       COALESCE(dp.date_validation_hint,'Validate against raw_records.last_updated_raw/last_updated_utc for this Store-V2 record.') AS spotlight_date_validation_hint,
       COALESCE(dp.collapsed_date_candidate_count,0) AS collapsed_date_candidate_count,
       r.last_updated_utc,
       'metadata/index update time - not usage without supporting decoded fields' AS time_interpretation,
       COALESCE(t.text_value_count,0) AS spotlight_text_value_count,
       COALESCE(t.distinct_text_category_count,0) AS spotlight_text_category_count,
       COALESCE(t.human_text_categories,'') AS spotlight_text_categories,
       COALESCE(t.readable_text_rollup_sample,'') AS spotlight_text_rollup_sample,
       0 AS ffs_reference_count,
       0 AS ffs_present_reference_count,
       0 AS ffs_missing_or_unresolved_reference_count,
       0 AS app_db_candidate_count,
       0 AS app_db_present_candidate_count,
       0 AS app_db_unresolved_candidate_count,
       CASE WHEN COALESCE(t.has_high_review_value_text,0)=1 THEN 'HIGH_SPOTLIGHT_TEXT_VALUE'
            WHEN COALESCE(t.text_value_count,0)>0 THEN 'SPOTLIGHT_TEXT_VALUE'
            ELSE 'SPOTLIGHT_RECORD_NO_RECOVERED_TEXT' END AS spotlight_review_priority,
       CASE WHEN COALESCE(t.text_value_count,0)>0 THEN 'TEXT_VALUES_RECOVERED_FROM_SPOTLIGHT'
            ELSE 'NO_TEXT_VALUES_RECOVERED_FOR_RECORD' END AS spotlight_decode_status,
       'Spotlight-first record review. V0_9_21 keeps GUI rows raw_record anchored and avoids broad FFS/app joins; full per-record exports are support/diagnostic-only to prevent long SQL materialization. Use Missing From FFS and object/object-summary views for residency pivots.' AS interpretation_note
FROM raw_records r
LEFT JOIN text_roll t ON t.raw_record_id=r.raw_record_id
LEFT JOIN date_one dp ON dp.raw_record_id=r.raw_record_id
WHERE r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%';

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_summary;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_spotlight_object_inode_summary AS
WITH rec AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count,
         COUNT(DISTINCT COALESCE(parent_inode_num,'')) AS distinct_parent_id_count,
         MIN(last_updated_utc) AS earliest_last_updated_utc,
         MAX(last_updated_utc) AS latest_last_updated_utc,
         MIN(raw_record_id) AS first_raw_record_id,
         MAX(raw_record_id) AS last_raw_record_id
  FROM raw_records
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), kv AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_key_value_rows,
         COUNT(DISTINCT field_name) AS distinct_spotlight_field_count,
         substr(MAX(CASE WHEN field_name='__spotlight_investigator_text_context' THEN field_value ELSE '' END),1,1800) AS spotlight_text_context_sample
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
)
SELECT rec.source_id,rec.store_guid,rec.source_db,rec.spotlight_inode_or_object_id,rec.spotlight_store_id,
       rec.raw_record_count,rec.distinct_parent_id_count,
       COALESCE(kv.raw_key_value_rows,0) AS raw_key_value_rows,
       COALESCE(kv.distinct_spotlight_field_count,0) AS distinct_spotlight_field_count,
       0 AS date_candidate_rows,
       rec.earliest_last_updated_utc,rec.latest_last_updated_utc,
       '' AS earliest_spotlight_date_utc,
       '' AS latest_spotlight_date_utc,
       rec.first_raw_record_id,rec.last_raw_record_id,
       COALESCE(kv.spotlight_text_context_sample,'') AS spotlight_text_context_sample,
       CASE WHEN rec.raw_record_count>1 THEN 'MULTIPLE_SPOTLIGHT_RECORDS_SHARE_OBJECT_ID'
            WHEN COALESCE(kv.raw_key_value_rows,0)>20 THEN 'SINGLE_RECORD_MANY_FIELDS'
            ELSE 'SINGLE_OR_LOW_EXPANSION_OBJECT' END AS object_materialization_status,
       'Object/inode-centric rollup. V0_9_21 normal exports summarize this view because the full per-object listing is support/diagnostic-only.' AS interpretation_note
FROM rec
LEFT JOIN kv ON kv.source_id=rec.source_id AND kv.store_guid=rec.store_guid AND kv.source_db=rec.source_db AND kv.spotlight_inode_or_object_id=rec.spotlight_inode_or_object_id AND kv.spotlight_store_id=rec.spotlight_store_id;

DROP VIEW IF EXISTS vw_ios_spotlight_object_inode_diagnostic_summary;
CREATE VIEW vw_ios_spotlight_object_inode_diagnostic_summary AS
WITH obj AS (
  SELECT source_id,store_guid,source_db,COALESCE(inode_num,'') AS spotlight_inode_or_object_id,COALESCE(store_id,'') AS spotlight_store_id,
         COUNT(*) AS raw_record_count
  FROM raw_records
  WHERE store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%'
  GROUP BY source_id,store_guid,source_db,COALESCE(inode_num,''),COALESCE(store_id,'')
), buckets AS (
  SELECT source_id,store_guid,source_db,
         CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
              WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
              WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
              ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END AS object_record_bucket,
         COUNT(*) AS object_count,
         SUM(raw_record_count) AS raw_record_count,
         MIN(raw_record_count) AS min_records_per_object,
         MAX(raw_record_count) AS max_records_per_object
  FROM obj
  GROUP BY source_id,store_guid,source_db,
           CASE WHEN raw_record_count=1 THEN 'ONE_RECORD_PER_OBJECT'
                WHEN raw_record_count BETWEEN 2 AND 5 THEN 'TWO_TO_FIVE_RECORDS_PER_OBJECT'
                WHEN raw_record_count BETWEEN 6 AND 20 THEN 'SIX_TO_TWENTY_RECORDS_PER_OBJECT'
                ELSE 'MORE_THAN_TWENTY_RECORDS_PER_OBJECT' END
)
SELECT source_id,store_guid,source_db,object_record_bucket,object_count,raw_record_count,min_records_per_object,max_records_per_object,
       'Compact object/inode materialization diagnostic. Use this normal export to decide whether the case should pivot to object-centric aggregation; full per-object rows require support/diagnostic export.' AS interpretation_note
FROM buckets;

DROP VIEW IF EXISTS vw_ios_spotlight_decode_gap_records;
CREATE VIEW vw_ios_spotlight_decode_gap_records AS
SELECT raw_record_id,source_id,store_guid,source_db,inode_num AS spotlight_inode_or_object_id,store_id AS spotlight_store_id,parent_inode_num,last_updated_utc,
       file_name,display_name,full_path,content_type,record_state,
       'NO_KEY_VALUES_OR_TEXT_PROBES_RECOVERED_FOR_SPOTLIGHT_RECORD' AS decode_gap_status,
       'This Spotlight/CoreSpotlight record was parsed at the record/header level but no key/value or human-readable text values were recovered. These rows identify the next native parser decoding target.' AS interpretation_note
FROM raw_records r
WHERE (r.store_guid LIKE 'ios_%' OR r.source_db LIKE '%CoreSpotlight%' OR r.store_path LIKE '%CoreSpotlight%')
  AND NOT EXISTS (
    SELECT 1 FROM raw_key_values kv
    WHERE kv.source_id=r.source_id AND kv.store_guid=r.store_guid AND kv.source_db=r.source_db
      AND kv.inode_num=r.inode_num AND COALESCE(kv.store_id,'')=COALESCE(r.store_id,'')
  );

DROP VIEW IF EXISTS vw_ios_database_residency_candidates;
)VSGUI",
        R"VSGUI(CREATE VIEW vw_ios_database_residency_candidates AS
WITH probes AS (
  SELECT raw_kv_id,source_id,store_guid,source_db,inode_num,store_id,parent_inode_num,field_name,field_value,LOWER(field_value) AS v
  FROM raw_key_values
  WHERE (store_guid LIKE 'ios_%' OR source_db LIKE '%CoreSpotlight%' OR store_path LIKE '%CoreSpotlight%')
    AND COALESCE(field_value,'')<>''
), cats AS (
  SELECT *, CASE
    WHEN v LIKE '%whatsapp%' THEN 'WHATSAPP_RELATED'
    WHEN v LIKE '%signal%' THEN 'SIGNAL_RELATED'
    WHEN v LIKE '%telegram%' THEN 'TELEGRAM_RELATED'
    WHEN v LIKE '%/sms/%' OR v LIKE '%library/sms%' OR v LIKE '%sms.db%' OR v LIKE '%imessage%' OR v LIKE '%com.apple.mobilesms%' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
    WHEN (v LIKE '%/callhistory%' OR v LIKE '%callhistory.storedata%' OR v LIKE '%facetime%' OR v LIKE 'tel:%' OR v LIKE '% tel:%')
         AND v NOT LIKE '%tel.meet%' AND v NOT LIKE '%meet.google%' THEN 'CALL_OR_FACETIME_RELATED'
    WHEN v LIKE '%.ics%' OR v LIKE '%/calendar/%' OR v LIKE '%calendar.google.com%' OR v LIKE '%text/calendar%' OR v LIKE '%vevent%' OR v LIKE '%vcalendar%' OR v LIKE '%webcal:%' OR v LIKE '%invite.ics%' THEN 'CALENDAR_OR_INVITATION_RELATED'
    WHEN v LIKE '%/library/mail/%' OR v LIKE '%attachmentdata%' OR v LIKE '%message/rfc822%' OR v LIKE '%mail.google.com%' OR v LIKE 'from:%' OR v LIKE 'to:%' OR v LIKE 'cc:%' OR v LIKE '%mailto:%' THEN 'MAIL_OR_ACCOUNT_RELATED'
    WHEN v LIKE '%addressbook%' OR v LIKE '%begin:vcard%' OR v LIKE '%vcard%' THEN 'CONTACT_OR_ADDRESS_BOOK_RELATED'
    WHEN v LIKE '%http://%' OR v LIKE '%https://%' OR v LIKE '%www.%' OR v LIKE '%safari%' OR v LIKE '%history%' THEN 'WEB_OR_BROWSER_RELATED'
    ELSE 'OTHER_DB_RESIDENCY_REVIEW'
  END AS object_category
  FROM probes
), db_clean AS (
  SELECT *,
         CASE
           WHEN database_category='APPLE_MESSAGES' THEN 'APPLE_MESSAGES_OR_SMS_RELATED'
           WHEN database_category='CALL_HISTORY' THEN 'CALL_OR_FACETIME_RELATED'
           WHEN database_category='WHATSAPP' THEN 'WHATSAPP_RELATED'
           WHEN database_category='SIGNAL' THEN 'SIGNAL_RELATED'
           WHEN database_category='TELEGRAM' THEN 'TELEGRAM_RELATED'
           WHEN database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT') THEN 'WEB_OR_BROWSER_RELATED'
           WHEN database_category='MAIL' THEN 'MAIL_OR_ACCOUNT_RELATED'
           WHEN database_category='CONTACTS' THEN 'CONTACT_OR_ADDRESS_BOOK_RELATED'
           WHEN database_category='CALENDAR' THEN 'CALENDAR_OR_INVITATION_RELATED'
           ELSE '' END AS app_db_object_category
  FROM ios_app_database_inventory
  WHERE COALESCE(database_category,'')<>''
    AND LOWER(COALESCE(normalized_path,'')) NOT LIKE '%-wal'
    AND LOWER(COALESCE(normalized_path,'')) NOT LIKE '%-shm'
    AND LOWER(COALESCE(database_name,'')) NOT LIKE '%-wal'
    AND LOWER(COALESCE(database_name,'')) NOT LIKE '%-shm'
), db_family AS (
  SELECT source_id,app_db_object_category AS object_category,
         COUNT(DISTINCT ios_db_id) AS matching_database_count,
         MIN(database_name) AS database_name,
         GROUP_CONCAT(DISTINCT database_category) AS database_category,
         MIN(app_hint) AS app_hint,
         MIN(normalized_path) AS candidate_database_path,
         MAX(COALESCE(record_inventory_status,'')) AS record_inventory_status
  FROM db_clean
  WHERE app_db_object_category<>''
  GROUP BY source_id,app_db_object_category
), ri_family AS (
  SELECT d.source_id,d.app_db_object_category AS object_category,
         COUNT(*) AS matching_table_count,
         MIN(ri.record_category) AS matched_record_category,
         MIN(ri.table_name) AS matched_table_name,
         MAX(COALESCE(ri.row_count,0)) AS matched_table_row_count
  FROM ios_app_database_record_inventory ri
  JOIN db_clean d ON d.ios_db_id=ri.ios_db_id
  WHERE d.app_db_object_category<>''
  GROUP BY d.source_id,d.app_db_object_category
), parsed_family AS (
  SELECT d.source_id,d.app_db_object_category AS object_category,
         COUNT(*) AS parsed_record_count,
         MIN(p.record_category) AS parsed_record_category
  FROM ios_app_parsed_records p
  JOIN db_clean d ON d.ios_db_id=p.ios_db_id
  WHERE d.app_db_object_category<>''
  GROUP BY d.source_id,d.app_db_object_category
)
SELECT c.raw_kv_id AS candidate_id,c.source_id,c.store_guid,c.source_db,c.inode_num,c.store_id,c.parent_inode_num,c.field_name,
       c.object_category,substr(c.field_value,1,2000) AS string_value_sample,
       d.database_name,d.database_category,d.app_hint,d.candidate_database_path,d.record_inventory_status,
       COALESCE(pb.parsed_record_category,ri.matched_record_category,'') AS matched_record_category,
       COALESCE(ri.matched_table_name,'') AS matched_table_name,
       COALESCE(ri.matched_table_row_count,0) AS matched_table_row_count,
       CASE WHEN COALESCE(pb.parsed_record_count,0)>0 THEN 'DATABASE_FAMILY_PARSED_RECORDS_AVAILABLE_VALUE_MATCH_NOT_PROVEN'
            WHEN COALESCE(ri.matching_table_count,0)>0 THEN 'DATABASE_FAMILY_TABLE_PRESENT_VALUE_MATCH_NOT_PROVEN'
            WHEN COALESCE(d.matching_database_count,0)>0 THEN 'DATABASE_FAMILY_PRESENT_RECORD_TABLE_NOT_PARSED'
            ELSE 'NO_KNOWN_APP_DATABASE_INVENTORY_MATCH' END AS database_residency_status,
       CASE WHEN COALESCE(pb.parsed_record_count,0)>0
            THEN 'Strict string classification matched a database family with parsed records. This is a lead only; exact Spotlight string-to-row correlation is not yet proven.'
            WHEN COALESCE(d.matching_database_count,0)>0
            THEN 'Strict string classification matched a database family. It does not prove the specific value is present in the database.'
            ELSE 'No matching database family was identified in the current app database inventory.' END AS interpretation_note
FROM cats c
LEFT JOIN db_family d ON d.source_id=c.source_id AND d.object_category=c.object_category
LEFT JOIN ri_family ri ON ri.source_id=c.source_id AND ri.object_category=c.object_category
LEFT JOIN parsed_family pb ON pb.source_id=c.source_id AND pb.object_category=c.object_category
WHERE c.object_category<>'OTHER_DB_RESIDENCY_REVIEW';

)VSGUI"
    });

    // V0_9_6: iOS investigator pivot and unified keyword-search surface views for opened older cases.
    execGuiSql(R"SQL(
DROP VIEW IF EXISTS vw_ios_contact_identity_records;
CREATE VIEW vw_ios_contact_identity_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,
       CASE
         WHEN lower(table_name)='abperson' THEN 'ADDRESSBOOK_PERSON_ROW'
         WHEN lower(table_name)='contacts' THEN 'CONTACTS_CACHE_ROW'
         WHEN lower(table_name) LIKE '%fulltextsearch_content%' THEN 'CONTACT_FULLTEXT_CONTENT'
         WHEN COALESCE(contact_or_participant,'')<>'' THEN 'CONTACT_TEXT_OR_ADDRESS_VALUE'
         WHEN COALESCE(item_identifier,'')<>'' THEN 'CONTACT_IDENTIFIER_VALUE'
         ELSE 'CONTACT_REVIEW_ROW'
       END AS contact_identity_type,
       substr(trim(COALESCE(contact_or_participant,'') || ' ' || COALESCE(title,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'')),1,1200) AS identity_value_sample,
       'Parsed contact/address-book database row or contact cache row. Contact cache/FTS rows can duplicate or tokenize contact data; use database path/table/provenance before reporting.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category='CONTACTS'
  AND lower(table_name) NOT LIKE '%docsize%'
  AND lower(table_name) NOT LIKE '%segdir%'
  AND lower(table_name) NOT LIKE '%segments%'
  AND lower(table_name) NOT LIKE '%_stat%'
  AND (
       lower(table_name) IN ('abperson','contacts','abpersonfulltextsearch_content','abpersonsmartdialerfulltextsearch_content')
       OR COALESCE(contact_or_participant,'')<>'' OR COALESCE(title,'')<>'' OR COALESCE(text_snippet,'')<>'' OR COALESCE(item_identifier,'')<>''
  );

DROP VIEW IF EXISTS vw_ios_contact_identity_summary;
CREATE VIEW vw_ios_contact_identity_summary AS
SELECT database_name,database_normalized_path,table_name,contact_identity_type,parse_status,
       COUNT(*) AS contact_review_row_count,
       COUNT(DISTINCT item_identifier) AS distinct_item_identifier_count,
       SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS rows_with_contact_text,
       SUM(CASE WHEN COALESCE(title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_title,
       SUM(CASE WHEN COALESCE(text_snippet,'')<>'' THEN 1 ELSE 0 END) AS rows_with_text_snippet,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc,
       MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc
FROM vw_ios_contact_identity_records
GROUP BY database_name,database_normalized_path,table_name,contact_identity_type,parse_status
ORDER BY contact_review_row_count DESC,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_web_history_review_records;
CREATE VIEW vw_ios_web_history_review_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       url,title,item_identifier,text_snippet,parse_status,provenance,
       CASE
         WHEN lower(table_name) LIKE '%bookmark%' THEN 'BOOKMARK_OR_SAVED_WEB_ITEM'
         WHEN lower(table_name) LIKE '%history%' THEN 'WEB_HISTORY_OR_VISIT'
         ELSE 'WEB_DATABASE_RECORD'
       END AS web_record_type,
       substr(trim(COALESCE(title,'') || ' ' || COALESCE(url,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'')),1,1600) AS web_review_value_sample,
       'Parsed local web/browser database row. Timestamp interpretation depends on the source table and parser provenance.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT')
   OR lower(app_hint) IN ('safari','chrome','webkit')
   OR lower(database_name) IN ('history.db','safaritabs.db')
ORDER BY record_timestamp_utc DESC,database_name,table_name,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_web_history_review_summary;
CREATE VIEW vw_ios_web_history_review_summary AS
SELECT database_category,app_hint,database_name,table_name,web_record_type,parse_status,
       COUNT(*) AS web_review_row_count,
       SUM(CASE WHEN COALESCE(url,'')<>'' THEN 1 ELSE 0 END) AS rows_with_url,
       SUM(CASE WHEN COALESCE(title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_title,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc,
       MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc,
       MIN(database_normalized_path) AS first_database_path,MAX(database_normalized_path) AS last_database_path
FROM vw_ios_web_history_review_records
GROUP BY database_category,app_hint,database_name,table_name,web_record_type,parse_status
ORDER BY web_review_row_count DESC,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_calendar_review_records;
CREATE VIEW vw_ios_calendar_review_records AS
SELECT ios_app_record_id,source_id,ios_db_id,database_normalized_path,database_name,database_category,app_hint,
       table_name,record_category,source_primary_key,record_timestamp_utc,timestamp_source,
       contact_or_participant,url,title,file_path,item_identifier,text_snippet,parse_status,provenance,
       CASE
         WHEN lower(table_name) LIKE '%attendee%' THEN 'CALENDAR_ATTENDEE_OR_INVITEE'
         WHEN lower(table_name) LIKE '%location%' THEN 'CALENDAR_LOCATION'
         WHEN lower(table_name) LIKE '%attachment%' THEN 'CALENDAR_ATTACHMENT'
         WHEN COALESCE(contact_or_participant,'')<>'' THEN 'CALENDAR_ACCOUNT_OR_INVITEE'
         ELSE 'CALENDAR_EVENT_OR_SUPPORT_ROW'
       END AS calendar_record_type,
       substr(trim(COALESCE(title,'') || ' ' || COALESCE(contact_or_participant,'') || ' ' || COALESCE(url,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'')),1,1600) AS calendar_review_value_sample,
)SQL" R"SQL(       'Parsed calendar database row. Calendar rows may include calendars, attendees, suggestions, attachments, or event-support rows; use table/provenance before reporting.' AS interpretation_note
FROM ios_app_parsed_records
WHERE database_category='CALENDAR'
  AND (COALESCE(title,'')<>'' OR COALESCE(contact_or_participant,'')<>'' OR COALESCE(url,'')<>'' OR COALESCE(text_snippet,'')<>'' OR COALESCE(item_identifier,'')<>'' OR COALESCE(record_timestamp_utc,'')<>'')
ORDER BY record_timestamp_utc DESC,database_name,table_name,ios_app_record_id;

DROP VIEW IF EXISTS vw_ios_calendar_review_summary;
CREATE VIEW vw_ios_calendar_review_summary AS
SELECT database_name,database_normalized_path,table_name,calendar_record_type,parse_status,
       COUNT(*) AS calendar_review_row_count,
       SUM(CASE WHEN COALESCE(title,'')<>'' THEN 1 ELSE 0 END) AS rows_with_title,
       SUM(CASE WHEN COALESCE(contact_or_participant,'')<>'' THEN 1 ELSE 0 END) AS rows_with_contact_or_account,
)SQL" R"SQL(       SUM(CASE WHEN COALESCE(record_timestamp_utc,'')<>'' THEN 1 ELSE 0 END) AS rows_with_timestamp,
       MIN(NULLIF(record_timestamp_utc,'')) AS earliest_record_timestamp_utc,
       MAX(NULLIF(record_timestamp_utc,'')) AS latest_record_timestamp_utc
FROM vw_ios_calendar_review_records
GROUP BY database_name,database_normalized_path,table_name,calendar_record_type,parse_status
ORDER BY calendar_review_row_count DESC,database_name,table_name;

DROP VIEW IF EXISTS vw_ios_investigation_keyword_surface;
CREATE VIEW vw_ios_investigation_keyword_surface AS
SELECT 'CORESPOTLIGHT_TEXT' AS surface_source, source_id, CAST(raw_kv_id AS TEXT) AS source_record_id,
       human_text_category AS review_category, store_guid AS source_container, source_db AS source_location,
       field_name AS field_or_table, '' AS record_timestamp_utc, readable_text_sample AS searchable_value_sample,
       '' AS path_or_url, '' AS contact_or_identity, review_priority AS review_priority,
       'SPOTLIGHT_INDEX_VALUE' AS residency_context, interpretation_note
FROM vw_ios_spotlight_human_text_values
UNION ALL
SELECT 'APP_DATABASE_RECORD' AS surface_source, source_id, CAST(ios_app_record_id AS TEXT) AS source_record_id,
       database_category || ':' || record_category AS review_category, database_name AS source_container, database_normalized_path AS source_location,
       table_name AS field_or_table, record_timestamp_utc,
       substr(trim(COALESCE(title,'') || ' ' || COALESCE(text_snippet,'') || ' ' || COALESCE(item_identifier,'') || ' ' || COALESCE(contact_or_participant,'') || ' ' || COALESCE(url,'') || ' ' || COALESCE(file_path,'')),1,2000) AS searchable_value_sample,
       COALESCE(NULLIF(url,''),file_path) AS path_or_url, contact_or_participant AS contact_or_identity,
       CASE WHEN database_category IN ('APPLE_MESSAGES','WHATSAPP','CALL_HISTORY') THEN 'HIGH_APP_COMMUNICATION_RECORD'
            WHEN database_category IN ('SAFARI_WEB','CHROME_WEB','WEBKIT','MAIL','CALENDAR','CONTACTS') THEN 'MEDIUM_HIGH_APP_RECORD'
            ELSE 'APP_DATABASE_RECORD' END AS review_priority,
       'APP_DATABASE_RECORD_PRESENT_IN_ACQUIRED_DATABASE' AS residency_context,
       'Parsed app database value. This indicates the value came from the acquired/staged database listed, not necessarily from Spotlight.' AS interpretation_note
FROM ios_app_parsed_records
WHERE trim(COALESCE(title,'') || COALESCE(text_snippet,'') || COALESCE(item_identifier,'') || COALESCE(contact_or_participant,'') || COALESCE(url,'') || COALESCE(file_path,''))<>''
UNION ALL
SELECT 'FFS_HIGH_VALUE_PATH' AS surface_source, source_id, CAST(ios_file_id AS TEXT) AS source_record_id,
       COALESCE(NULLIF(app_container_hint,''),domain_hint) AS review_category, file_name AS source_container, normalized_path AS source_location,
       extension AS field_or_table, zip_modified_utc AS record_timestamp_utc, substr(normalized_path,1,2000) AS searchable_value_sample,
       normalized_path AS path_or_url, '' AS contact_or_identity,
       'HIGH_VALUE_FFS_PATH' AS review_priority,
       'PATH_PRESENT_IN_FFS_ZIP_INVENTORY' AS residency_context,
       'FFS path inventory row. Presence means the path was enumerated in the ZIP; absence from this filtered view does not mean absence from the ZIP.' AS interpretation_note
FROM ios_ffs_file_inventory
WHERE lower(normalized_path) LIKE '%sms%' OR lower(normalized_path) LIKE '%message%' OR lower(normalized_path) LIKE '%whatsapp%'
   OR lower(normalized_path) LIKE '%callhistory%' OR lower(normalized_path) LIKE '%facetime%' OR lower(normalized_path) LIKE '%addressbook%'
   OR lower(normalized_path) LIKE '%contacts%' OR lower(normalized_path) LIKE '%calendar%' OR lower(normalized_path) LIKE '%safari%'
   OR lower(normalized_path) LIKE '%history.db%' OR lower(normalized_path) LIKE '%mail%' OR lower(normalized_path) LIKE '%keychain%'
   OR lower(normalized_path) LIKE '%attachment%' OR lower(normalized_path) LIKE '%documents%' OR lower(normalized_path) LIKE '%downloads%'
UNION ALL
SELECT 'APP_DATABASE_INVENTORY' AS surface_source, source_id, CAST(ios_db_id AS TEXT) AS source_record_id,
       database_category AS review_category, database_name AS source_container, normalized_path AS source_location,
       app_hint AS field_or_table, zip_modified_utc AS record_timestamp_utc, substr(normalized_path,1,2000) AS searchable_value_sample,
       normalized_path AS path_or_url, '' AS contact_or_identity,
       'APP_DATABASE_DISCOVERY' AS review_priority,
       COALESCE(record_inventory_status,parse_status) AS residency_context,
       'App database inventory row. Use record inventory and parsed-record views to confirm whether rows were parsed.' AS interpretation_note
FROM ios_app_database_inventory;


)SQL");

}

class ReadOnlyDb {
public:
    explicit ReadOnlyDb(const std::wstring& path) {
        const std::string p = narrow(path);
        if (sqlite3_open_v2(p.c_str(), &db_, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK) {
            sqlite3_busy_timeout(db_, 30000);
            sqlite3_exec(db_, "PRAGMA temp_store=MEMORY; PRAGMA cache_size=-65536;", nullptr, nullptr, nullptr);
            ensureGuiReviewViews(db_);
            return;
        }
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        if (sqlite3_open_v2(p.c_str(), &db_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            std::string msg = db_ ? sqlite3_errmsg(db_) : "unknown";
            if (db_) sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("Unable to open case database: " + msg);
        }
        sqlite3_busy_timeout(db_, 30000);
        sqlite3_exec(db_, "PRAGMA temp_store=MEMORY; PRAGMA cache_size=-65536;", nullptr, nullptr, nullptr);
    }
    ~ReadOnlyDb() { if (db_) sqlite3_close(db_); }
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

bool viewVisibleForCurrentInvestigationTab(const ViewSpec& v) {
    if (gIosReviewMode) return isIosReviewView(v) && isIosPrimarySpotlightReviewView(v);
    return !isIosReviewView(v) && !isIosOnlySharedReviewView(v);
}

void populateViewList(sqlite3* db = nullptr) {
    if (!gReviewView) return;
    SendMessageW(gReviewView, LB_RESETCONTENT, 0, 0);
    gVisibleViews.clear();
    std::vector<size_t> candidates;
    for (size_t i = 0; i < views().size(); ++i) {
        if (!viewVisibleForCurrentInvestigationTab(views()[i])) continue;
        if (!db || sqliteObjectExists(db, views()[i].tableName)) candidates.push_back(i);
    }
    if (candidates.empty()) {
        for (size_t i = 0; i < views().size(); ++i) {
            if (viewVisibleForCurrentInvestigationTab(views()[i])) candidates.push_back(i);
        }
    }
    if (gIosReviewMode) {
        std::stable_sort(candidates.begin(), candidates.end(), [](size_t a, size_t b) {
            const int ra = iosPrimarySpotlightReviewRank(views()[a]);
            const int rb = iosPrimarySpotlightReviewRank(views()[b]);
            if (ra != rb) return ra < rb;
            return a < b;
        });
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
}

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
        } else {
            populateReviewListFromResult(*result);
            setReviewSummary(result->summary);
        }
    } catch (const std::exception& ex) {
        clearListColumns();
        gCurrentRowArtifactIds.clear();
        gCurrentHasNext = false;
        setReviewSummary(L"ERROR applying review page result: " + widen(ex.what()));
    }
    setReviewLoadingState(false);
    delete result;
}

void loadReviewPage() {
    if (gOpenedCaseDb.empty()) {
        setReviewSummary(L"Open an existing VestigantSpotlight.case.sqlite file first.");
        return;
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
    UpdateWindow(gReviewSummary);
    UpdateWindow(gList);

    std::thread([requestId, owner, dbPath, v, viewIndex, requestedPage, ps, search, capturedFilterValue, sql, bindPatterns, checkedCountAtRequest]() {
        auto* result = new ReviewPageResult();
        result->requestId = requestId;
        result->viewIndex = viewIndex;
        result->page = requestedPage;
        result->pageSize = ps;
        const ULONGLONG startedMs = GetTickCount64();
        try {
            ReadOnlyDb db(dbPath);
            sqlite3_stmt* st = nullptr;
            if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &st, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.get()));
            int bind = 1;
            for (const std::string& pattern : bindPatterns) sqlite3_bind_text(st, bind++, pattern.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, bind++, ps + 1);
            sqlite3_bind_int64(st, bind++, static_cast<sqlite3_int64>(requestedPage) * ps);

            int row = 0;
            while (true) {
                if (requestId != gReviewRequestSeq.load()) { sqlite3_finalize(st); delete result; return; }
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
            if (requestId != gReviewRequestSeq.load()) { delete result; return; }
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
    }).detach();
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
    wchar_t buf[4096]{};
    ListView_GetItemText(list, row, col, buf, 4096);
    return buf;
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
    postProgress(1);
    postStatus(L"Ingest running: initializing options.");
    postLog(L"Starting native C++ Spotlight workflow...");
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
        monitor = std::thread([statusPath, progressPath, rootProgressPath, &monitorDone]() {
            std::wstring lastStatus;
            std::wstring lastProgress;
            while (!monitorDone.load()) {
                const std::wstring currentStatus = lastNonEmptyLine(statusPath);
                if (!currentStatus.empty() && currentStatus != lastStatus) {
                    lastStatus = currentStatus;
                    postStatus(L"Ingest status: " + currentStatus);
                }
                std::wstring currentProgress = lastNonEmptyLine(progressPath);
                if (currentProgress.empty()) currentProgress = lastNonEmptyLine(rootProgressPath);
                if (!currentProgress.empty() && currentProgress != lastProgress) {
                    lastProgress = currentProgress;
                    std::wstring stage, message;
                    int pct = parseProgressPercent(currentProgress, stage, message);
                    if (pct >= 0) {
                        postProgress(pct);
                        std::wstring status = L"Ingest running: " + std::to_wstring(pct) + L"%";
                        if (!stage.empty()) status += L" - " + stageDisplayName(stage);
                        if (!message.empty()) status += L" - " + message;
                        postStatus(status);
                    }
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
        opt.preserveEvidence = SendMessageW(gNoPreserve, BM_GETCHECK, 0, 0) != BST_CHECKED;
        opt.decodeCoreNativeValues = SendMessageW(gCoreDecode, BM_GETCHECK, 0, 0) == BST_CHECKED;
        opt.experimentalFullNativeValues = SendMessageW(gFullNative, BM_GETCHECK, 0, 0) == BST_CHECKED;
        opt.maxNativeRecords = parseSizeBox(gMaxRecords, 0);
        opt.maxNativeBlocks = parseSizeBox(gMaxBlocks, 0);
        opt.verbose = SendMessageW(gVerbose, BM_GETCHECK, 0, 0) == BST_CHECKED;
        int exportProfile = static_cast<int>(SendMessageW(gExportProfile, CB_GETCURSEL, 0, 0));
        opt.exportProfile = exportProfile == 1 ? "investigator" : exportProfile == 2 ? "diagnostics" : exportProfile == 3 ? "full" : "minimal";
        int prof = static_cast<int>(SendMessageW(gProfile, CB_GETCURSEL, 0, 0));
        int mode = static_cast<int>(SendMessageW(gMode, CB_GETCURSEL, 0, 0));
        opt.profile = prof == 1 ? "macos" : prof == 2 ? "ios" : "auto";
        opt.mode = mode == 1 ? "diagnostics" : mode == 2 ? "discover" : mode == 3 ? "self-test" : "run";
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
        if (opt.experimentalFullNativeValues) postLog(L"Full native metadata parsing is enabled. Leave record/block limits blank for full ingest, or set limits for diagnostics/testing.");
        else postLog(L"Stable native header/core parsing is enabled. Full native metadata parsing is available through the checkbox above.");
        postStatus(L"Ingest running: parser/enrichment/export workflow active. See log and status line for stages.");
        auto rr = runApplication(opt);
        for (const auto& m : rr.messages) postLog(widen(m));
        std::ostringstream os;
        os << "Complete: store_groups=" << rr.storeCount << " database_candidates=" << rr.databaseCandidateCount << " selected_databases=" << rr.selectedParserDatabaseCount << " artifacts=" << rr.artifactCount << " usage=" << rr.usageCount << " orphan/deleted candidates=" << rr.orphanCandidateCount;
        postLog(widen(os.str()));
        postProgress(100);
        postStatus(L"Ingest complete: " + widen(os.str()));
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

void showTab(int index) {
    if (index == 1) gIosReviewMode = false;
    else if (index == 2) gIosReviewMode = true;
    for (HWND h : gProcessControls) ShowWindow(h, index == 0 ? SW_SHOW : SW_HIDE);
    const bool showSharedReview = (index == 1) || (index == 2 && gIosReviewMode);
    for (HWND h : gReviewControls) ShowWindow(h, showSharedReview ? SW_SHOW : SW_HIDE);
    for (HWND h : gIosControls) ShowWindow(h, (index == 2 && !gIosReviewMode) ? SW_SHOW : SW_HIDE);
    for (HWND h : gTagControls) ShowWindow(h, index == 3 ? SW_SHOW : SW_HIDE);
    if (index == 1 || index == 2) populateViewListForCurrentContextNoThrow();
    if (index == 3) refreshTagList();
}

void createProcessControls(HWND hwnd) {
    const int y0 = 58;
    addProcess(CreateWindowW(L"STATIC", L"Case Information", WS_CHILD | WS_VISIBLE, 16, y0, 720, 24, hwnd, nullptr, gInst, nullptr));
    labelP(hwnd, L"Case Name", 16, y0 + 34, 120, 22); gCaseName = editP(hwnd, 140, y0 + 30, 330, 26, L"Spotlight Case"); SetWindowLongPtrW(gCaseName, GWLP_ID, ID_CASE_NAME_EDIT);
    labelP(hwnd, L"Case Number", 490, y0 + 34, 120, 22); gCaseNumber = editP(hwnd, 612, y0 + 30, 220, 26); SetWindowLongPtrW(gCaseNumber, GWLP_ID, ID_CASE_NUMBER_EDIT);
    labelP(hwnd, L"Investigator", 16, y0 + 70, 120, 22); gInvestigator = editP(hwnd, 140, y0 + 66, 330, 26); SetWindowLongPtrW(gInvestigator, GWLP_ID, ID_INVESTIGATOR_EDIT);
    labelP(hwnd, L"Company", 490, y0 + 70, 120, 22); gCompany = editP(hwnd, 612, y0 + 66, 220, 26); SetWindowLongPtrW(gCompany, GWLP_ID, ID_COMPANY_EDIT);
    labelP(hwnd, L"Case Location", 16, y0 + 106, 120, 22); gOut = editP(hwnd, 140, y0 + 102, 692, 26); SetWindowLongPtrW(gOut, GWLP_ID, ID_CASE_LOCATION_EDIT); gBrowseOut = buttonP(hwnd, L"Browse", ID_BROWSE_OUT, 842, y0 + 102, 90, 28);
    labelP(hwnd, L"Case Database", 16, y0 + 142, 120, 22); gCaseDbPath = editP(hwnd, 140, y0 + 138, 650, 26); SetWindowLongPtrW(gCaseDbPath, GWLP_ID, ID_CASE_DB_EDIT); gBrowseCase = buttonP(hwnd, L"Browse", ID_BROWSE_CASE, 800, y0 + 138, 90, 28); gOpenCase = openCaseButtonP(hwnd, 898, y0 + 136, 96, 32);
    gSaveCaseInfo = buttonP(hwnd, L"Save Case Info", ID_SAVE_CASE_INFO, 140, y0 + 172, 140, 28);
    gCaseAutosaveStatus = addProcess(CreateWindowW(L"STATIC", L"Autosave ready", WS_CHILD | WS_VISIBLE | SS_LEFT, 292, y0 + 176, 260, 22, hwnd, nullptr, gInst, nullptr));

    addProcess(CreateWindowW(L"STATIC", L"Build / Processing", WS_CHILD | WS_VISIBLE, 16, y0 + 210, 720, 24, hwnd, nullptr, gInst, nullptr));
    labelP(hwnd, L"Source type", 16, y0 + 246, 90, 22); gSourceType = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 106, y0 + 242, 180, 160, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SOURCE_TYPE)), gInst, nullptr));
    for (const wchar_t* s : {L"Folder", L"ZIP", L"AFF4 (future)", L"Raw IMG/DD (future)"}) SendMessageW(gSourceType, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gSourceType, CB_SETCURSEL, 0, 0);
    labelP(hwnd, L"Raw Spotlight evidence source", 300, y0 + 246, 190, 22); gInput = editP(hwnd, 492, y0 + 242, 380, 26); gBrowseInput = buttonP(hwnd, L"Browse", ID_BROWSE_INPUT, 882, y0 + 242, 100, 28);
    labelP(hwnd, L"Evidence root / iOS cache case", 16, y0 + 282, 220, 22); gEvidenceRoot = editP(hwnd, 236, y0 + 278, 636, 26); gBrowseRoot = buttonP(hwnd, L"Browse", ID_BROWSE_ROOT, 882, y0 + 278, 100, 28);
    labelP(hwnd, L"7z.exe path (optional)", 16, y0 + 318, 210, 22); gSevenZip = editP(hwnd, 236, y0 + 314, 636, 26); gBrowse7z = buttonP(hwnd, L"Browse", ID_BROWSE_7Z, 882, y0 + 314, 100, 28);
    addProcess(CreateWindowW(L"STATIC", L"Container priority: ZIP/folder stable now; AFF4 + APFS image inventory is the priority next path; raw IMG/DD remains secondary. Legacy V7 import is CLI-only.", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, y0 + 352, 966, 24, hwnd, nullptr, gInst, nullptr));

    labelP(hwnd, L"Profile", 16, y0 + 390, 90, 22); gProfile = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 106, y0 + 386, 180, 160, hwnd, nullptr, gInst, nullptr));
    for (const wchar_t* s : {L"Auto", L"Standard macOS", L"iOS/CoreSpotlight"}) SendMessageW(gProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gProfile, CB_SETCURSEL, 0, 0);
    labelP(hwnd, L"Mode", 300, y0 + 390, 56, 22); gMode = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 356, y0 + 386, 270, 140, hwnd, nullptr, gInst, nullptr));
    for (const wchar_t* s : {L"Process Raw Spotlight Evidence", L"Diagnostics / Bounded Native Parse", L"Discover Stores Only", L"Self-test"}) SendMessageW(gMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gMode, CB_SETCURSEL, 0, 0);
    gRun = buttonP(hwnd, L"Build / Process Case", ID_RUN, 740, y0 + 382, 240, 36);

    gNoPreserve = addProcess(CreateWindowW(L"BUTTON", L"Skip preservation (temporary testing only)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 106, y0 + 424, 300, 24, hwnd, nullptr, gInst, nullptr));
    gCoreDecode = addProcess(CreateWindowW(L"BUTTON", L"Enable safe core string/path probe", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 420, y0 + 424, 270, 24, hwnd, nullptr, gInst, nullptr));
    gFullNative = addProcess(CreateWindowW(L"BUTTON", L"Full native metadata values", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 700, y0 + 424, 250, 24, hwnd, nullptr, gInst, nullptr));
    SendMessageW(gFullNative, BM_SETCHECK, BST_CHECKED, 0);
    gVerbose = addProcess(CreateWindowW(L"BUTTON", L"Verbose log", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 106, y0 + 454, 120, 24, hwnd, nullptr, gInst, nullptr));
    SendMessageW(gVerbose, BM_SETCHECK, BST_CHECKED, 0);
    labelP(hwnd, L"Max records (blank=all)", 236, y0 + 458, 150, 22); gMaxRecords = editP(hwnd, 390, y0 + 454, 70, 26, L"");
    labelP(hwnd, L"Blocks/store (blank=all)", 470, y0 + 458, 160, 22); gMaxBlocks = editP(hwnd, 635, y0 + 454, 70, 26, L"");
    labelP(hwnd, L"Export profile", 715, y0 + 458, 100, 22); gExportProfile = addProcess(CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 815, y0 + 454, 154, 130, hwnd, nullptr, gInst, nullptr));
    for (const wchar_t* s : {L"Minimal", L"Investigator", L"Diagnostics", L"Full CSV"}) SendMessageW(gExportProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s)); SendMessageW(gExportProfile, CB_SETCURSEL, 1, 0);

    labelP(hwnd, L"Ingest status", 16, y0 + 494, 120, 22);
    gIngestStatus = addProcess(CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Waiting: choose/create a case location and evidence source, then run ingest. Progress updates appear here and in the larger status log below.", WS_CHILD | WS_VISIBLE | SS_LEFT, 140, y0 + 490, 842, 82, hwnd, nullptr, gInst, nullptr));
    labelP(hwnd, L"Progress", 16, y0 + 586, 120, 22);
    gIngestProgress = addProcess(CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 140, y0 + 584, 842, 30, hwnd, nullptr, gInst, nullptr));
    SendMessageW(gIngestProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(gIngestProgress, PBM_SETPOS, 0, 0);
    gLog = addProcess(CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 16, y0 + 624, 966, 184, hwnd, nullptr, gInst, nullptr));
    appendLog(L"V0_9_6 iOS investigation build: adds reusable iOS cache/intake support for very large FFS ZIP tests. For iOS/CoreSpotlight runs, the Evidence root / iOS cache case box may point to a prior completed case such as Q:\\SpotlightCase\\TestiOS_WhatsApp_V0_9_4 to skip large ZIP re-inventory/extraction.");
    appendLog(L"Default export profile is Investigator; use Full CSV only when needed.");
}

LRESULT CALLBACK ReviewListSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_KEYDOWN && wp == VK_SPACE) {
        std::vector<int> rows = selectedReviewRows();
        if (!rows.empty()) {
            toggleReviewRowsAsBatch(rows);
            if (rows.size() == 1) focusReviewRow(rows.front() + 1, true);
            setReviewSummary(L"Toggled selected row checkmark(s). Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size())));
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
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void createReviewControls(HWND hwnd) {
    const int y0 = 58;
    addReview(CreateWindowW(L"STATIC", L"Investigation Results Grid: choose a platform-scoped review view from the left, then search/filter/sort/export database-backed results.", WS_CHILD | WS_VISIBLE, 16, y0, 960, 22, hwnd, nullptr, gInst, nullptr));
    labelR(hwnd, L"Views", 16, y0 + 32, 220, 22);
    gReviewView = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY, 16, y0 + 58, 230, 610, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_REVIEW_VIEW)), gInst, nullptr));
    SetWindowSubclass(gReviewView, ReviewViewListSubclassProc, 2, 0);
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
    gOpenCaseFolder = buttonR(hwnd, L"Case Folder", ID_OPEN_CASE_FOLDER, 262, y0 + 102, 116, 26);
    gOpenUploadFolder = buttonR(hwnd, L"Upload Folder", ID_OPEN_UPLOAD_FOLDER, 384, y0 + 102, 128, 26);
    gOpenLogsFolder = buttonR(hwnd, L"Logs", ID_OPEN_LOGS_FOLDER, 518, y0 + 102, 76, 26);
    gOpenDashboard = buttonR(hwnd, L"Dashboard", ID_OPEN_DASHBOARD, 732, y0 + 102, 116, 26);
    gOpenReviewIndex = buttonR(hwnd, L"Review Index", ID_OPEN_REVIEW_INDEX, 854, y0 + 102, 126, 26);
    gReviewSummary = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"No case opened. Open a database from the Case Information tab.", WS_CHILD | WS_VISIBLE | SS_LEFT, 262, y0 + 136, 720, 58, hwnd, nullptr, gInst, nullptr));
    gList = addReview(CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, 262, y0 + 206, 720, 462, hwnd, nullptr, gInst, nullptr));
    ListView_SetExtendedListViewStyle(gList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    SetWindowSubclass(gList, ReviewListSubclassProc, 1, 0);
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

    moveIf(gCaseName, 140, y0 + 30, 330, 26);
    moveIf(gCaseNumber, 612, y0 + 30, std::max(220, right - 612 - 150), 26);
    moveIf(gInvestigator, 140, y0 + 66, 330, 26);
    moveIf(gCompany, 612, y0 + 66, std::max(220, right - 612 - 150), 26);
    moveIf(gBrowseOut, rightBrowseX, y0 + 102, browseW, 28);
    moveIf(gOut, 140, y0 + 102, std::max(250, rightBrowseX - 150), 26);
    moveIf(gOpenCase, right - 96, y0 + 136, 96, 32);
    moveIf(gBrowseCase, right - 198, y0 + 138, 90, 28);
    moveIf(gCaseDbPath, 140, y0 + 138, std::max(250, right - 350), 26);
    moveIf(gCaseAutosaveStatus, 292, y0 + 176, right - 308, 22);

    moveIf(gRun, right - 240, y0 + 382, 240, 36);
    moveIf(gBrowseInput, rightBrowseX, y0 + 242, browseW, 28);
    moveIf(gInput, 492, y0 + 242, std::max(220, rightBrowseX - 502), 26);
    moveIf(gBrowseRoot, rightBrowseX, y0 + 278, browseW, 28);
    moveIf(gEvidenceRoot, 236, y0 + 278, std::max(220, rightBrowseX - 246), 26);
    moveIf(gBrowse7z, rightBrowseX, y0 + 314, browseW, 28);
    moveIf(gSevenZip, 236, y0 + 314, std::max(220, rightBrowseX - 246), 26);
    moveIf(gIngestStatus, 140, y0 + 490, right - 140, 82);
    moveIf(gIngestProgress, 140, y0 + 584, right - 140, 30);
    moveIf(gLog, 16, y0 + 624, right - 16, std::max(120, bottom - (y0 + 624)));

    moveIf(gReviewView, 16, y0 + 58, 230, std::max(220, bottom - (y0 + 58) - 20));
    const int reviewX = 262;
    const int reviewW = right - reviewX;
    // Keep the search box from expanding over the Update/Cancel/Rows controls on wide windows.
    moveIf(gSearch, 326, y0 + 30, 250, 26);
    moveIf(gReviewSummary, reviewX, y0 + 136, reviewW, 58);
    moveIf(gList, reviewX, y0 + 206, reviewW, std::max(240, bottom - (y0 + 206) - 20));
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
                else if (srcType == 2) postStatus(L"Source type: AFF4/APFS priority path. This build registers/hashes and creates image-inventory/active-comparison readiness, but does not yet extract APFS files.");
                else postStatus(L"Source type: Raw IMG/DD is secondary. This build registers/hashes and probes partitions, but AFF4/APFS image inventory is prioritized for active comparison.");
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
            if (srcType >= 2) {
                MessageBoxW(hwnd, L"This source type is registered in the roadmap but extraction is not implemented yet. Use Folder or ZIP for the current build. AFF4/APFS support requires container stream enumeration plus APFS filesystem inventory in a later version.", L"Vestigant Spotlight - Source Type Not Implemented", MB_OK | MB_ICONINFORMATION);
                postStatus(L"Waiting: AFF4/raw image extraction is not implemented in this build. Use Folder or ZIP.");
                return 0;
            }
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
        case ID_REVIEW_VIEW: { if (HIWORD(wp) == LBN_SELCHANGE) { gCurrentPage = 0; gSortColumn = -1; gFilterColumn = -1; gFilterValue.clear(); loadReviewPage(); } return 0; }
        case ID_REVIEW_REFRESH: { gCurrentPage = 0; loadReviewPage(); return 0; }
        case ID_REVIEW_CANCEL_LOAD: { ++gReviewRequestSeq; gReviewLoadInProgress = false; setReviewLoadingState(false); setReviewSummary(L"Cancelled current review-page load. Start another view/search when ready."); return 0; }
        case ID_CTX_SORT_ASC: { if (gContextColumn >= 0) { gSortColumn = gContextColumn; gSortDescending = false; gCurrentPage = 0; loadReviewPage(); } return 0; }
        case ID_CTX_SORT_DESC: { if (gContextColumn >= 0) { gSortColumn = gContextColumn; gSortDescending = true; gCurrentPage = 0; loadReviewPage(); } return 0; }
        case ID_CTX_FILTER_SEARCH: { if (gContextColumn >= 0) { gFilterColumn = gContextColumn; gFilterValue = narrow(getText(gSearch)); gCurrentPage = 0; loadReviewPage(); } return 0; }
        case ID_CTX_CLEAR_FILTER: { gFilterColumn = -1; gFilterValue.clear(); gCurrentPage = 0; loadReviewPage(); return 0; }
        case ID_CTX_TOGGLE_CHECK: { if (gContextRow >= 0) toggleReviewRowChecked(gContextRow); setReviewSummary(L"Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size()))); return 0; }
        case ID_CTX_CHECK_SELECTED: { setReviewRowsChecked(selectedReviewRows(), true); setReviewSummary(L"Checked selected row(s). Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size()))); return 0; }
        case ID_CTX_UNCHECK_SELECTED: { setReviewRowsChecked(selectedReviewRows(), false); setReviewSummary(L"Unchecked selected row(s). Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size()))); return 0; }
        case ID_CTX_TOGGLE_SELECTED: { toggleReviewRowsAsBatch(selectedReviewRows()); setReviewSummary(L"Toggled selected row(s). Checked artifacts=" + std::to_wstring(static_cast<unsigned long long>(gCheckedArtifactIds.size()))); return 0; }
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
    case WM_SET_INGEST_STATUS: { auto* s = reinterpret_cast<std::wstring*>(lp); if (s) { setText(gIngestStatus, *s); delete s; } return 0; }
    case WM_SET_INGEST_PROGRESS: { int pct = static_cast<int>(wp); if (pct < 0) pct = 0; if (pct > 100) pct = 100; if (gIngestProgress) SendMessageW(gIngestProgress, PBM_SETPOS, static_cast<WPARAM>(pct), 0); return 0; }
    case WM_REVIEW_PAGE_RESULT: { completeReviewPageLoad(reinterpret_cast<ReviewPageResult*>(lp)); return 0; }
    case WM_SIZE: { layoutControls(hwnd); return 0; }
    case WM_TIMER: { if (wp == ID_AUTOSAVE_TIMER) { autosaveCaseInformation(hwnd); return 0; } break; }
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        if (mmi) { mmi->ptMinTrackSize.x = 1040; mmi->ptMinTrackSize.y = 780; }
        return 0;
    }
    case WM_DESTROY: { KillTimer(hwnd, ID_AUTOSAVE_TIMER); saveCaseInformationCore(true); if (gUiFont) { DeleteObject(gUiFont); gUiFont = nullptr; } PostQuitMessage(0); return 0; }
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
