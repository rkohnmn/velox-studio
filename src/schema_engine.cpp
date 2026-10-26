#include "velox/schema_engine.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace velox {

namespace {

std::string toLower(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string trim(std::string_view sv) {
    while (!sv.empty() && (std::isspace(static_cast<unsigned char>(sv.front())) || sv.front() == '`' || sv.front() == '"')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (std::isspace(static_cast<unsigned char>(sv.back())) || sv.back() == ',' || sv.back() == '`' || sv.back() == '"')) {
        sv.remove_suffix(1);
    }
    return std::string(sv);
}

bool hasUpperCase(std::string_view sv) {
    for (char c : sv) {
        if (std::isupper(static_cast<unsigned char>(c))) return true;
    }
    return false;
}

} // namespace


std::string SchemaEngine::sanitizeIdentifier(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '_') {
            out.push_back(c);
        }
    }
    if (out.empty()) return "unnamed_identifier";
    if (std::isdigit(static_cast<unsigned char>(out.front()))) {
        out.insert(out.begin(), '_');
    }
    return out;
}

SchemaAnalysisReport SchemaEngine::analyzeDdl(std::string_view sql_ddl) {
    SchemaAnalysisReport report;

    std::string content(sql_ddl);
    std::istringstream iss(content);
    std::string line;
    SqlTable current_table;
    bool in_table = false;

    while (std::getline(iss, line)) {
        std::string trimmed_line = trim(line);
        if (trimmed_line.empty() || trimmed_line.starts_with("--")) continue;

        std::string lower = toLower(trimmed_line);

        if (lower.starts_with("create table")) {
            in_table = true;
            current_table = SqlTable{};
            // Extract table name before '('
            std::size_t paren = trimmed_line.find('(');
            std::string header = (paren != std::string::npos) ? trimmed_line.substr(0, paren) : trimmed_line;
            std::istringstream hss(header);
            std::string tok, last_tok;
            while (hss >> tok) {
                if (toLower(tok) != "create" && toLower(tok) != "table" &&
                    toLower(tok) != "if" && toLower(tok) != "not" && toLower(tok) != "exists") {
                    last_tok = trim(tok);
                }
            }
            current_table.name = last_tok;
            continue;
        }

        if (in_table) {
            if (trimmed_line.starts_with(")")) {
                report.tables.push_back(current_table);
                in_table = false;
                continue;
            }

            if (lower.starts_with("primary key") || lower.starts_with("constraint") || lower.starts_with("foreign key")) {
                if (lower.find("foreign key") != std::string::npos) {
                    std::size_t lp = trimmed_line.find('(');
                    std::size_t rp = trimmed_line.find(')', lp == std::string::npos ? 0 : lp);
                    std::size_t ref_pos = lower.find("references");
                    if (lp != std::string::npos && rp != std::string::npos && ref_pos != std::string::npos) {
                        std::string fk_col = trim(trimmed_line.substr(lp + 1, rp - lp - 1));
                        std::string ref_part = trim(trimmed_line.substr(ref_pos + 10));
                        std::size_t rlp = ref_part.find('(');
                        std::size_t rrp = ref_part.find(')', rlp == std::string::npos ? 0 : rlp);
                        std::string target_tbl = (rlp != std::string::npos) ? trim(ref_part.substr(0, rlp)) : trim(ref_part);
                        std::string target_col = (rlp != std::string::npos && rrp != std::string::npos)
                                                     ? trim(ref_part.substr(rlp + 1, rrp - rlp - 1))
                                                     : "id";
                        for (auto& col : current_table.columns) {
                            if (col.name == fk_col) {
                                col.is_foreign_key = true;
                                col.fk_target_table = target_tbl;
                                col.fk_target_column = target_col;
                            }
                        }
                    }
                }
                continue;
            }

            std::istringstream lss(trimmed_line);
            SqlColumn col;
            lss >> col.name >> col.raw_type;
            col.name = trim(col.name);
            col.raw_type = trim(col.raw_type);

            if (lower.find("primary key") != std::string::npos) {
                col.is_primary_key = true;
                col.is_not_null = true;
            }
            if (lower.find("not null") != std::string::npos) {
                col.is_not_null = true;
            }
            if (lower.find("unique") != std::string::npos) {
                col.is_unique = true;
            }
            if (lower.find("references") != std::string::npos) {
                col.is_foreign_key = true;
                std::size_t ref_pos = lower.find("references");
                std::string ref_part = trim(trimmed_line.substr(ref_pos + 10));
                std::size_t lp = ref_part.find('(');
                std::size_t rp = ref_part.find(')', lp == std::string::npos ? 0 : lp);
                col.fk_target_table = (lp != std::string::npos) ? trim(ref_part.substr(0, lp)) : trim(ref_part);
                col.fk_target_column = (lp != std::string::npos && rp != std::string::npos)
                                           ? trim(ref_part.substr(lp + 1, rp - lp - 1))
                                           : "id";
            }
            if (!col.name.empty()) {
                current_table.columns.push_back(std::move(col));
            }
        } else if (lower.starts_with("create index") || lower.starts_with("create unique index")) {
            std::size_t on_pos = lower.find(" on ");
            if (on_pos != std::string::npos) {
                std::string after_on = trim(trimmed_line.substr(on_pos + 4));
                std::size_t lp = after_on.find('(');
                std::size_t rp = after_on.find(')', lp == std::string::npos ? 0 : lp);
                if (lp != std::string::npos && rp != std::string::npos) {
                    std::string tbl_name = trim(after_on.substr(0, lp));
                    std::string cols_str = trim(after_on.substr(lp + 1, rp - lp - 1));
                    for (auto& tbl : report.tables) {
                        if (tbl.name == tbl_name) {
                            std::istringstream css(cols_str);
                            std::string cname;
                            while (std::getline(css, cname, ',')) {
                                tbl.indexed_columns.push_back(trim(cname));
                            }
                        }
                    }
                }
            }
        }
    }

    // Evaluate normalization, constraints, naming, and index gaps
    for (const auto& tbl : report.tables) {
        bool has_pk = false;
        int numbered_cols = 0;
        std::unordered_set<std::string> indexed_set(tbl.indexed_columns.begin(), tbl.indexed_columns.end());

        if (hasUpperCase(tbl.name)) {
            report.findings.push_back({
                "WARNING", "NAMING", tbl.name,
                "Table name uses mixed case ('" + tbl.name + "').",
                "Standardize on lower_snake_case plural table names to avoid quoting traps."
            });
            report.normalization_score -= 4;
        }

        for (const auto& col : tbl.columns) {
            if (col.is_primary_key) has_pk = true;

            // Check 1NF repeating groups (e.g., phone1, phone2, tag_1, tag_2)
            if (!col.name.empty() && std::isdigit(static_cast<unsigned char>(col.name.back()))) {
                ++numbered_cols;
            }

            // Check floating-point money/price anti-pattern
            std::string lcol = toLower(col.name);
            std::string ltype = toLower(col.raw_type);
            if ((lcol.find("price") != std::string::npos || lcol.find("amount") != std::string::npos ||
                 lcol.find("total") != std::string::npos || lcol.find("balance") != std::string::npos) &&
                (ltype.find("float") != std::string::npos || ltype.find("double") != std::string::npos ||
                 ltype.find("real") != std::string::npos)) {
                report.findings.push_back({
                    "CRITICAL", "DATA_TYPE", tbl.name,
                    "Monetary column '" + col.name + "' uses imprecise IEEE-754 floating point ('" + col.raw_type + "').",
                    "Change '" + col.name + "' to NUMERIC(12, 2) or BIGINT integer cents."
                });
                report.normalization_score -= 12;
            }

            // Check unindexed foreign keys
            bool looks_like_fk = col.is_foreign_key ||
                                 (lcol.ends_with("_id") && !col.is_primary_key);
            if (looks_like_fk && indexed_set.find(col.name) == indexed_set.end()) {
                report.findings.push_back({
                    "CRITICAL", "INDEX_GAP", tbl.name,
                    "Foreign key '" + col.name + "' lacks a B-tree index (causes JOIN/DELETE table locks).",
                    "CREATE INDEX idx_" + toLower(tbl.name) + "_" + col.name + " ON " + tbl.name + "(" + col.name + ");"
                });
                report.recommended_indexes.push_back(
                    "CREATE INDEX idx_" + toLower(tbl.name) + "_" + col.name + " ON " + tbl.name + "(" + col.name + ");");
                report.normalization_score -= 10;
            }
        }

        if (!has_pk) {
            report.findings.push_back({
                "CRITICAL", "1NF", tbl.name,
                "Table '" + tbl.name + "' has no PRIMARY KEY constraint (violates 1NF entity identity).",
                "Add a surrogate BIGINT GENERATED ALWAYS AS IDENTITY or UUID PRIMARY KEY."
            });
            report.normalization_score -= 15;
        }

        if (numbered_cols >= 2) {
            report.findings.push_back({
                "WARNING", "1NF", tbl.name,
                "Detected numbered repeating column attributes in '" + tbl.name + "' (violates 1NF atomicity).",
                "Extract repeating numbered columns into a normalized child junction table."
            });
            report.normalization_score -= 10;
        }
    }

    report.normalization_score = std::clamp(report.normalization_score, 0, 100);

    // Build Mermaid ERD
    std::ostringstream erd;
    erd << "erDiagram\n";
    for (const auto& tbl : report.tables) {
        for (const auto& col : tbl.columns) {
            if (col.is_foreign_key && !col.fk_target_table.empty()) {
                erd << "    " << col.fk_target_table << " ||--o{ " << tbl.name
                    << " : \"" << col.name << "\"\n";
            }
        }
    }
    for (const auto& tbl : report.tables) {
        erd << "    " << tbl.name << " {\n";
        for (const auto& col : tbl.columns) {
            std::string badge = col.is_primary_key ? " PK" : (col.is_foreign_key ? " FK" : "");
            erd << "        " << col.raw_type << " " << col.name << badge << "\n";
        }
        erd << "    }\n";
    }
    report.mermaid_erd = erd.str();
    return report;
}

MigrationPlan SchemaEngine::generateZeroDowntimeMigration(const std::string& raw_table_name,
                                                          const std::string& raw_old_column,
                                                          const std::string& raw_new_column,
                                                          const std::string& raw_new_type,
                                                          const std::string& backfill_expr) {
    const std::string table_name = sanitizeIdentifier(raw_table_name);
    const std::string old_column = sanitizeIdentifier(raw_old_column);
    const std::string new_column = sanitizeIdentifier(raw_new_column);
    std::string new_type;
    for (char c : raw_new_type) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '_' || c == '(' || c == ')' || c == ',' || c == ' ') {
            new_type.push_back(c);
        }
    }
    if (new_type.empty()) new_type = "BIGINT";
    MigrationPlan plan;
    plan.phase1_expand_sql =
        "-- PHASE 1 (EXPAND): Add nullable column without table rewrite lock\n"
        "ALTER TABLE " + table_name + " ADD COLUMN " + new_column + " " + new_type + ";\n"
        "CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_" + table_name + "_" + new_column +
        " ON " + table_name + "(" + new_column + ");";

    plan.phase2_backfill_sql =
        "-- PHASE 2 (BACKFILL): Batched 5,000-row update to avoid long-held row locks\n"
        "UPDATE " + table_name + " SET " + new_column + " = " + backfill_expr + "\n"
        "WHERE id IN (SELECT id FROM " + table_name + " WHERE " + new_column + " IS NULL LIMIT 5000);";

    plan.phase3_transition_note =
        "PHASE 3 (TRANSITION): Deploy application dual-write + cutover reads from '" +
        old_column + "' to '" + new_column + "'. Verify pg_stat_activity shows zero reads on '" + old_column + "'.";

    plan.phase4_contract_sql =
        "-- PHASE 4 (CONTRACT): Enforce NOT NULL via NOT VALID check constraint then drop legacy column\n"
        "ALTER TABLE " + table_name + " ADD CONSTRAINT chk_" + new_column + "_not_null CHECK (" +
        new_column + " IS NOT NULL) NOT VALID;\n"
        "ALTER TABLE " + table_name + " VALIDATE CONSTRAINT chk_" + new_column + "_not_null;\n"
        "ALTER TABLE " + table_name + " DROP COLUMN " + old_column + ";";

    plan.rollback_down_sql =
        "-- ROLLBACK (DOWN.SQL): Safe reversal prior to Phase 4 contract step\n"
        "DROP INDEX CONCURRENTLY IF EXISTS idx_" + table_name + "_" + new_column + ";\n"
        "ALTER TABLE " + table_name + " DROP COLUMN IF EXISTS " + new_column + ";";

    return plan;
}

} // namespace velox
