#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace velox {

struct SqlColumn {
    std::string name;
    std::string raw_type;
    bool is_primary_key = false;
    bool is_not_null = false;
    bool is_unique = false;
    bool is_foreign_key = false;
    std::string fk_target_table;
    std::string fk_target_column;
};

struct SqlTable {
    std::string name;
    std::vector<SqlColumn> columns;
    std::vector<std::string> indexed_columns;
};

struct SchemaFinding {
    std::string severity; // "CRITICAL" | "WARNING" | "INFO"
    std::string category; // "1NF" | "2NF/3NF" | "INDEX_GAP" | "DATA_TYPE" | "NAMING"
    std::string table;
    std::string message;
    std::string recommendation;
};

struct MigrationPlan {
    std::string phase1_expand_sql;
    std::string phase2_backfill_sql;
    std::string phase3_transition_note;
    std::string phase4_contract_sql;
    std::string rollback_down_sql;
};

struct SchemaAnalysisReport {
    std::vector<SqlTable> tables;
    std::vector<SchemaFinding> findings;
    std::vector<std::string> recommended_indexes;
    std::string mermaid_erd;
    int normalization_score = 100;
};

class SchemaEngine {
public:
    static std::string sanitizeIdentifier(std::string_view raw);
    static SchemaAnalysisReport analyzeDdl(std::string_view sql_ddl);
    static MigrationPlan generateZeroDowntimeMigration(const std::string& table_name,
                                                       const std::string& old_column,
                                                       const std::string& new_column,
                                                       const std::string& new_type,
                                                       const std::string& backfill_expr);
};

} // namespace velox
