#include "velox/studio_ui.hpp"

#include <iomanip>
#include <sstream>

namespace velox {

std::string StudioUi::renderTerminalBento(const DatasetSummary& ds,
                                          const PreFlightAuditResult& audit,
                                          const SchemaAnalysisReport& schema,
                                          const ProfileReport& prof,
                                          GuiTab active_tab) {
    (void)active_tab;
    std::ostringstream out;
    out << "\n"
        << "+==============================================================================+\n"
        << "|  || || || ||   velox       //  MADE FOR THE FAST PATH (C++20 NATIVE)         |\n"
        << "|  || / || / || ||   _ S T U D I O   Zero-Copy mmap | ImGui+Photon | LieFactor 1.0 |\n"
        << "|  || || / || [#]  Scanmark: 4 Data Lanes + #35F28B Emerald Terminal Sample      |\n"
        << "+==============================================================================+\n"
        << "| [BENTO 1: MMAP COLUMNAR ENGINE]      | [BENTO 2: PERCEPTUAL VIZ AUDIT]       |\n"
        << "| Source  : " << std::left << std::setw(26) << ds.source_name.substr(0, 26)
        << " | Audit Score : " << std::right << std::setw(2) << audit.score_out_of_15 << " / 15 ("
        << (audit.passed_all_blocking ? "PASS" : "FAIL") << ")          |\n"
        << "| Rows    : " << std::left << std::setw(26) << ds.row_count
        << " | Lie Factor  : " << std::fixed << std::setprecision(3) << std::setw(6) << audit.lie_factor
        << " [0.95..1.05]   |\n"
        << "| Columns : " << std::left << std::setw(26) << ds.column_count
        << " | Text WCAG   : " << std::fixed << std::setprecision(1) << std::setw(5) << audit.min_text_contrast
        << ":1 (AAA >= 7:1)  |\n"
        << "| Speed   : " << std::fixed << std::setprecision(2) << std::setw(8) << ds.parse_time_ms << " ms ("
        << std::setw(7) << ds.throughput_mb_s << " MB/s)   | Mark WCAG   : "
        << std::fixed << std::setprecision(1) << std::setw(5) << audit.min_mark_contrast << ":1 (AA  >= 3:1)  |\n"
        << "| SHA-256 : " << ds.sha256_checksum.substr(0, 24) << ".. | Engine      : Native C++20 Vector GUI |\n"
        << "+--------------------------------------+---------------------------------------+\n"
        << "| [BENTO 3: DATABASE DDL & INDEXING]   | [BENTO 4: P99 & N+1 TRACE PROFILER]   |\n"
        << "| Tables Parsed    : " << std::left << std::setw(17) << schema.tables.size()
        << " | Spans Analyzed : " << std::left << std::setw(20) << prof.total_spans << " |\n"
        << "| Norm. Health     : " << std::left << std::setw(3) << schema.normalization_score << "/100           "
        << " | Latency P50/P99: " << std::fixed << std::setprecision(1) << prof.overall_p50_ms << " / "
        << prof.overall_p99_ms << " ms       |\n"
        << "| Index Gaps Found : " << std::left << std::setw(17) << schema.recommended_indexes.size()
        << " | N+1 Waterfalls : " << std::left << std::setw(20) << prof.n_plus_one_alerts.size() << " |\n"
        << "+==============================================================================+\n";

    if (!ds.columns.empty()) {
        out << "\n  [COLUMN TELEMETRY — TABULAR NUMERAL ALIGNMENT]\n"
            << "  " << std::left << std::setw(18) << "Column"
            << std::setw(20) << "Inferred Type"
            << std::right << std::setw(10) << "Mean"
            << std::setw(10) << "Median"
            << std::setw(10) << "IQR"
            << std::setw(10) << "P99" << "\n"
            << "  ------------------------------------------------------------------------------\n";
        for (const auto& col : ds.columns) {
            out << "  " << std::left << std::setw(18) << col.name.substr(0, 17)
                << std::setw(20) << CsvEngine::columnTypeToString(col.type);
            if (col.type == ColumnType::Numeric || col.type == ColumnType::Integer) {
                out << std::right << std::fixed << std::setprecision(2)
                    << std::setw(10) << col.mean
                    << std::setw(10) << col.median
                    << std::setw(10) << col.iqr
                    << std::setw(10) << col.p99 << "\n";
            } else {
                out << std::right << std::setw(10) << "-"
                    << std::setw(10) << "-"
                    << std::setw(10) << "-"
                    << std::setw(10) << (std::to_string(col.distinct_estimate) + " uniq") << "\n";
            }
        }
    }
    return out.str();
}

} // namespace velox
