#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace velox {

enum class ColumnType {
    Integer,
    Numeric,
    Temporal,
    Categorical,
    Identifier
};

struct ColumnStats {
    std::string name;
    ColumnType type = ColumnType::Categorical;
    std::size_t count = 0;
    std::size_t null_count = 0;
    std::size_t distinct_estimate = 0;

    // Numeric summary
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double std_dev = 0.0;
    double q25 = 0.0;
    double median = 0.0;
    double q75 = 0.0;
    double iqr = 0.0;
    double p90 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;

    // Sampled/collected values for downstream visualization
    std::vector<double> numeric_values;
    std::unordered_map<std::string, std::size_t> category_counts;
};

struct DatasetSummary {
    std::string source_name;
    std::string sha256_checksum;
    std::size_t byte_size = 0;
    std::size_t row_count = 0;
    std::size_t column_count = 0;
    double parse_time_ms = 0.0;
    double throughput_mb_s = 0.0;
    char delimiter = ',';
    std::vector<ColumnStats> columns;
};

struct GroupByResultRow {
    std::string key;
    std::size_t count = 0;
    double sum = 0.0;
    double mean = 0.0;
    double min = 0.0;
    double max = 0.0;
};

class CsvEngine {
public:
    static char detectDelimiter(std::string_view sample) noexcept;
    static DatasetSummary analyze(std::string_view content,
                                  const std::string& source_name = "memory_buffer",
                                  char delimiter = '\0');

    static std::vector<GroupByResultRow> groupByAggregate(
        std::string_view content,
        std::size_t key_col_idx,
        std::size_t val_col_idx,
        char delimiter = ',');

    static std::string columnTypeToString(ColumnType t);
};

} // namespace velox
