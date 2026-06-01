#include "parsers/v7_output_importer.h"
#include "core/csv.h"
#include "core/path_utils.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace vestigant::spotlight {
namespace {

std::string q(const std::string& s) { std::string o="'"; for(char c:s) o += (c=='\'' ? "''" : std::string(1,c)); o += "'"; return o; }

bool completeCsvRecord(const std::string& rec) {
    bool inQuotes=false;
    for (std::size_t i=0;i<rec.size();++i) if (rec[i]=='"') { if (inQuotes && i+1<rec.size() && rec[i+1]=='"') { ++i; continue; } inQuotes=!inQuotes; }
    return !inQuotes;
}
std::string trimRecord(std::string s) { if(!s.empty()&&s.back()=='\n') s.pop_back(); if(!s.empty()&&s.back()=='\r') s.pop_back(); return s; }

class CsvStream {
public:
    explicit CsvStream(const fs::path& p): file_(p), in_(p, std::ios::binary) {
        if(!in_) throw std::runtime_error("Unable to open V7 CSV: "+pathString(p));
        std::string rec; if(readRec(rec)) { headers_=csvParseLine(trimRecord(rec)); if(!headers_.empty()&&headers_[0].size()>=3&&(unsigned char)headers_[0][0]==0xEF) headers_[0].erase(0,3); }
    }
    bool next(Row& r) { std::string rec; if(!readRec(rec)) return false; auto f=csvParseLine(trimRecord(rec)); r.clear(); for(std::size_t i=0;i<headers_.size();++i) r[headers_[i]]= i<f.size()?f[i]:""; return true; }
private:
    bool readRec(std::string& out) { out.clear(); std::string line; while(std::getline(in_, line)) { out += line; out += '\n'; if(completeCsvRecord(out)) return true; } return !out.empty(); }
    fs::path file_; std::ifstream in_; std::vector<std::string> headers_;
};

std::vector<fs::path> findFiles(const fs::path& root, const std::string& prefix, const std::string& suffix) {
    std::vector<fs::path> out; std::error_code ec; if(!fs::exists(root,ec)) return out;
    for(fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it!=end; it.increment(ec)) {
        if(ec){ec.clear();continue;} if(!it->is_regular_file(ec)) continue; auto n=toLower(it->path().filename().string());
        if(n.rfind(toLower(prefix),0)==0 && n.size()>=suffix.size() && n.substr(n.size()-suffix.size())==toLower(suffix)) out.push_back(it->path());
    }
    std::sort(out.begin(), out.end()); return out;
}

std::vector<fs::path> findSuffix(const fs::path& root, const std::string& suffix) {
    std::vector<fs::path> out; std::error_code ec; if(!fs::exists(root,ec)) return out;
    for(fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it!=end; it.increment(ec)) {
        if(ec){ec.clear();continue;} if(!it->is_regular_file(ec)) continue; auto n=toLower(it->path().filename().string());
        if(n.size()>=suffix.size() && n.substr(n.size()-suffix.size())==toLower(suffix)) out.push_back(it->path());
    }
    std::sort(out.begin(), out.end()); return out;
}

std::string dbRole(const std::string& sourceDb, const std::string& fileName) {
    auto s=toLower(sourceDb+" "+fileName); if(s.find("dotstore")!=std::string::npos||s.find(".store.db")!=std::string::npos) return "dotstore"; if(s.find("store.db")!=std::string::npos||s.find("_store_")!=std::string::npos) return "store"; return "unknown";
}

std::string guidFromName(const std::string& name) { return name.size()>=36 ? name.substr(0,36) : std::string(); }

void ensureSchema(CaseDatabase& db) {
    db.exec(R"SQL(
CREATE TABLE IF NOT EXISTS v7_import_sessions (v7_session_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, v7_output_path TEXT, imported_utc TEXT, kv_rows INTEGER, date_candidate_rows INTEGER, parsed_date_rows INTEGER, fullpath_rows INTEGER, files_imported INTEGER);
CREATE TABLE IF NOT EXISTS v7_record_key_values (v7_kv_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, import_file TEXT, import_row INTEGER, store_guid TEXT, store_path TEXT, source_db TEXT, source_db_role TEXT, inode_num TEXT, store_id TEXT, parent_inode_num TEXT, full_path TEXT, record_state TEXT, field_name TEXT, field_value TEXT);
CREATE TABLE IF NOT EXISTS v7_date_candidates (v7_date_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, import_file TEXT, import_row INTEGER, store_guid TEXT, store_path TEXT, source_db TEXT, source_db_role TEXT, inode_num TEXT, store_id TEXT, field_name TEXT, field_value TEXT, parsed_utc TEXT, parse_method TEXT);
CREATE TABLE IF NOT EXISTS v7_parsed_date_values (v7_parsed_date_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, import_file TEXT, import_row INTEGER, store_guid TEXT, store_path TEXT, source_db TEXT, source_db_role TEXT, inode_num TEXT, store_id TEXT, field_name TEXT, field_value TEXT, parsed_utc TEXT, parse_method TEXT);
CREATE TABLE IF NOT EXISTS v7_decoder_fullpaths (v7_fullpath_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, import_file TEXT, import_row INTEGER, store_guid TEXT, source_db_role TEXT, inode_num TEXT, full_path TEXT);
CREATE TABLE IF NOT EXISTS v7_field_inventory (v7_field_inventory_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, field_name TEXT, row_count INTEGER, populated_count INTEGER, sample_value TEXT);
CREATE TABLE IF NOT EXISTS native_v7_comparison (comparison_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id TEXT, metric_name TEXT, native_value INTEGER, v7_value INTEGER, delta_value INTEGER, created_utc TEXT);
CREATE INDEX IF NOT EXISTS idx_v7_kv_field ON v7_record_key_values(source_id, field_name);
CREATE INDEX IF NOT EXISTS idx_v7_kv_inode_field ON v7_record_key_values(source_id, store_guid, inode_num, field_name);
CREATE INDEX IF NOT EXISTS idx_v7_dates_inode_field ON v7_date_candidates(source_id, store_guid, inode_num, field_name);
CREATE INDEX IF NOT EXISTS idx_v7_fullpaths_inode ON v7_decoder_fullpaths(source_id, store_guid, inode_num);
)SQL");
}

void bindCommon(SqlStatement& st, const std::string& sourceId, const fs::path& f, std::size_t row, const Row& r) {
    const auto sourceDb=get(r,"source_db");
    st.bind(1,sourceId); st.bind(2,pathString(f)); st.bind(3,(long long)row); st.bind(4,get(r,"store_guid")); st.bind(5,get(r,"store_path")); st.bind(6,sourceDb); st.bind(7,dbRole(sourceDb,f.filename().string())); st.bind(8,get(r,"inode_num")); st.bind(9,get(r,"store_id"));
}

std::size_t importKv(CaseDatabase& db, const std::string& sourceId, const fs::path& f) {
    auto st=db.prepare("INSERT INTO v7_record_key_values(source_id,import_file,import_row,store_guid,store_path,source_db,source_db_role,inode_num,store_id,parent_inode_num,full_path,record_state,field_name,field_value) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    CsvStream csv(f); Row r; std::size_t row=0;
    while(csv.next(r)){++row; bindCommon(st,sourceId,f,row,r); st.bind(10,get(r,"parent_inode_num")); st.bind(11,get(r,"full_path")); st.bind(12,get(r,"record_state")); st.bind(13,get(r,"field_name")); st.bind(14,get(r,"field_value")); st.stepDone(); st.reset();}
    return row;
}

std::size_t importDate(CaseDatabase& db, const std::string& sourceId, const fs::path& f, const std::string& table) {
    auto st=db.prepare("INSERT INTO "+table+"(source_id,import_file,import_row,store_guid,store_path,source_db,source_db_role,inode_num,store_id,field_name,field_value,parsed_utc,parse_method) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
    CsvStream csv(f); Row r; std::size_t row=0;
    while(csv.next(r)){++row; bindCommon(st,sourceId,f,row,r); st.bind(10,get(r,"field_name")); st.bind(11,get(r,"field_value")); st.bind(12,get(r,"parsed_utc")); st.bind(13,get(r,"parse_method")); st.stepDone(); st.reset();}
    return row;
}

std::size_t importFullpaths(CaseDatabase& db, const std::string& sourceId, const fs::path& f) {
    std::ifstream in(f,std::ios::binary); if(!in) throw std::runtime_error("Unable to open V7 fullpaths TSV: "+pathString(f));
    auto st=db.prepare("INSERT INTO v7_decoder_fullpaths(source_id,import_file,import_row,store_guid,source_db_role,inode_num,full_path) VALUES(?,?,?,?,?,?,?)");
    std::string line; std::getline(in,line); std::size_t row=0; const auto fn=f.filename().string();
    while(std::getline(in,line)){ if(!line.empty()&&line.back()=='\r') line.pop_back(); auto tab=line.find('\t'); if(tab==std::string::npos) continue; ++row; st.bind(1,sourceId); st.bind(2,pathString(f)); st.bind(3,(long long)row); st.bind(4,guidFromName(fn)); st.bind(5,dbRole("",fn)); st.bind(6,line.substr(0,tab)); st.bind(7,line.substr(tab+1)); st.stepDone(); st.reset(); }
    return row;
}

long long scalar(CaseDatabase& db, const std::string& sql) { auto st=db.prepare(sql); return st.stepRow()?st.colInt64(0):0; }
void metric(CaseDatabase& db, const std::string& sourceId, const std::string& name, long long nativeValue, long long v7Value) { auto st=db.prepare("INSERT INTO native_v7_comparison(source_id,metric_name,native_value,v7_value,delta_value,created_utc) VALUES(?,?,?,?,?,?)"); st.bind(1,sourceId); st.bind(2,name); st.bind(3,nativeValue); st.bind(4,v7Value); st.bind(5,nativeValue-v7Value); st.bind(6,nowUtc()); st.stepDone(); }

void rebuildComparison(CaseDatabase& db, const std::string& sourceId) {
    db.exec("DELETE FROM v7_field_inventory WHERE source_id="+q(sourceId));
    db.exec("DELETE FROM native_v7_comparison WHERE source_id="+q(sourceId));
    db.exec("INSERT INTO v7_field_inventory(source_id,field_name,row_count,populated_count,sample_value) SELECT source_id,field_name,COUNT(*),SUM(CASE WHEN COALESCE(field_value,'')<>'' THEN 1 ELSE 0 END),MIN(CASE WHEN COALESCE(field_value,'')<>'' THEN field_value ELSE NULL END) FROM v7_record_key_values WHERE source_id="+q(sourceId)+" GROUP BY source_id,field_name");
    metric(db,sourceId,"raw_records",scalar(db,"SELECT COUNT(*) FROM raw_records WHERE source_id="+q(sourceId)),0);
    metric(db,sourceId,"key_values",scalar(db,"SELECT COUNT(*) FROM raw_key_values WHERE source_id="+q(sourceId)),scalar(db,"SELECT COUNT(*) FROM v7_record_key_values WHERE source_id="+q(sourceId)));
    metric(db,sourceId,"date_candidates",scalar(db,"SELECT COUNT(*) FROM raw_date_candidates WHERE source_id="+q(sourceId)),scalar(db,"SELECT COUNT(*) FROM v7_date_candidates WHERE source_id="+q(sourceId)));
    metric(db,sourceId,"parsed_date_values",0,scalar(db,"SELECT COUNT(*) FROM v7_parsed_date_values WHERE source_id="+q(sourceId)));
    metric(db,sourceId,"fullpaths",0,scalar(db,"SELECT COUNT(*) FROM v7_decoder_fullpaths WHERE source_id="+q(sourceId)));
    metric(db,sourceId,"distinct_v7_fields",scalar(db,"SELECT COUNT(*) FROM field_inventory WHERE source_id="+q(sourceId)),scalar(db,"SELECT COUNT(*) FROM v7_field_inventory WHERE source_id="+q(sourceId)));
}

} // namespace

V7ImportCounts V7OutputImporter::importDirectory(const fs::path& v7OutputPath, const std::string& sourceId, CaseDatabase& db, Logger& log) const {
    V7ImportCounts c; if(v7OutputPath.empty()) return c; std::error_code ec; if(!fs::exists(v7OutputPath,ec)) throw std::runtime_error("V7 output path does not exist: "+pathString(v7OutputPath));
    ensureSchema(db); log.info("Importing V7 parser output for comparison: "+pathString(v7OutputPath)); db.begin();
    try {
        for(const auto& f: findFiles(v7OutputPath,"record_key_values",".csv")){ c.kvRows += importKv(db,sourceId,f); ++c.filesImported; log.info("Imported V7 KV: "+pathString(f)); }
        for(const auto& f: findFiles(v7OutputPath,"date_field_candidates",".csv")){ c.dateCandidateRows += importDate(db,sourceId,f,"v7_date_candidates"); ++c.filesImported; log.info("Imported V7 date candidates: "+pathString(f)); }
        for(const auto& f: findFiles(v7OutputPath,"parsed_date_values",".csv")){ c.parsedDateRows += importDate(db,sourceId,f,"v7_parsed_date_values"); ++c.filesImported; log.info("Imported V7 parsed dates: "+pathString(f)); }
        for(const auto& f: findSuffix(v7OutputPath,"_fullpaths.tsv")){ c.fullpathRows += importFullpaths(db,sourceId,f); ++c.filesImported; log.info("Imported V7 fullpaths: "+pathString(f)); }
        rebuildComparison(db, sourceId);
        auto st=db.prepare("INSERT INTO v7_import_sessions(source_id,v7_output_path,imported_utc,kv_rows,date_candidate_rows,parsed_date_rows,fullpath_rows,files_imported) VALUES(?,?,?,?,?,?,?,?)");
        st.bind(1,sourceId); st.bind(2,pathString(v7OutputPath)); st.bind(3,nowUtc()); st.bind(4,(long long)c.kvRows); st.bind(5,(long long)c.dateCandidateRows); st.bind(6,(long long)c.parsedDateRows); st.bind(7,(long long)c.fullpathRows); st.bind(8,(long long)c.filesImported); st.stepDone();
        db.commit();
    } catch(...) { db.rollbackNoThrow(); throw; }
    log.info("V7 import complete: kv="+std::to_string(c.kvRows)+" date_candidates="+std::to_string(c.dateCandidateRows)+" parsed_dates="+std::to_string(c.parsedDateRows)+" fullpaths="+std::to_string(c.fullpathRows)+" files="+std::to_string(c.filesImported));
    return c;
}

} // namespace vestigant::spotlight
