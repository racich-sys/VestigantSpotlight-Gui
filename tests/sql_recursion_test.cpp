#include "sql_recursion_test.h"
#include "db/case_db.h"
#include <filesystem>
#include <iostream>
#include <string>

namespace vestigant::spotlight {
bool runRecursiveCteSafetySmokeTest(const fs::path& out) {
    fs::path dbPath = out / "recursive_cte_safety" / "recursive_cte_safety.case.sqlite";
    std::error_code ec;
    fs::create_directories(dbPath.parent_path(), ec);
    fs::remove(dbPath, ec);
    try {
        CaseDatabase db;
        db.open(dbPath);
        db.exec("PRAGMA temp_store=MEMORY;");
        db.exec(R"SQL(
CREATE TABLE test_inode_nodes(
  artifact_id INTEGER PRIMARY KEY,
  source_id TEXT NOT NULL,
  store_guid TEXT NOT NULL,
  inode_num INTEGER NOT NULL,
  parent_inode_num INTEGER NOT NULL,
  file_name TEXT NOT NULL
);
CREATE INDEX idx_test_inode_nodes_key ON test_inode_nodes(source_id, store_guid, inode_num);
CREATE INDEX idx_test_inode_nodes_parent ON test_inode_nodes(source_id, store_guid, parent_inode_num);
)SQL");
        db.begin();
        auto ins = db.prepare("INSERT INTO test_inode_nodes(artifact_id,source_id,store_guid,inode_num,parent_inode_num,file_name) VALUES(?,?,?,?,?,?)");
        const long long rowCount = 100000;
        for (long long i = 0; i < rowCount; ++i) {
            const long long inode = 2 + i;
            const long long parent = (i == 0) ? 0 : (inode - 1);
            ins.bind(1, i + 1);
            ins.bind(2, "s");
            ins.bind(3, "g");
            ins.bind(4, inode);
            ins.bind(5, parent);
            ins.bind(6, std::string("node_") + std::to_string(inode));
            ins.stepDone();
            ins.reset();
        }
        db.commit();
        auto q = db.prepare(R"SQL(
WITH RECURSIVE inode_tree(source_id,store_guid,artifact_id,current_inode,current_parent,reconstructed_path,depth,visited) AS (
  SELECT source_id,store_guid,artifact_id,inode_num,parent_inode_num,file_name,0,'|' || inode_num || '|'
  FROM test_inode_nodes
  WHERE source_id='s' AND store_guid='g' AND inode_num=2
  UNION ALL
  SELECT c.source_id,c.store_guid,c.artifact_id,c.inode_num,c.parent_inode_num,
         inode_tree.reconstructed_path || '/' || c.file_name,
         inode_tree.depth + 1,
         inode_tree.visited || c.inode_num || '|'
  FROM test_inode_nodes c
  JOIN inode_tree
    ON c.source_id=inode_tree.source_id
   AND c.store_guid=inode_tree.store_guid
   AND c.parent_inode_num=inode_tree.current_inode
  WHERE inode_tree.depth < 20
    AND instr(inode_tree.visited, '|' || c.inode_num || '|')=0
)
SELECT COUNT(*), MAX(depth), MAX(length(reconstructed_path)) FROM inode_tree;
)SQL");
        if (!q.stepRow()) return false;
        const long long producedRows = q.colInt64(0);
        const long long maxDepth = q.colInt64(1);
        const long long maxPathLen = q.colInt64(2);
        if (producedRows != 21) return false;
        if (maxDepth != 20) return false;
        if (maxPathLen <= 0) return false;
    } catch (const std::exception& ex) {
        std::cerr << "Recursive CTE safety smoke test failed: " << ex.what() << "\n";
        return false;
    }
    return true;
}

} // namespace vestigant::spotlight
