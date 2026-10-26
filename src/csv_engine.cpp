#include "velox/csv_engine.hpp"
#include "velox/sha256.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unordered_set>

namespace velox {

namespace {

constexpr double kPow10[19] = {
    1.0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18
};

inline std::string_view trimView(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '"')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '"')) {
        sv.remove_suffix(1);
    }
    return sv;
}

inline bool fastParseDouble(std::string_view sv, double& out) noexcept {
    sv = trimView(sv);
    if (sv.empty()) return false;

    const char* p = sv.data();
    const char* end = p + sv.size();
    bool neg = false;
    if (*p == '-') {
        neg = true;
        ++p;
    } else if (*p == '+') {
        ++p;
    }
    if (p == end) return false;

    // Fast reject if first char after sign is neither digit nor '.'
    if ((*p < '0' || *p > '9') && *p != '.') return false;

    uint64_t int_part = 0;
    int int_digits = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        int_part = int_part * 10 + static_cast<uint64_t>(*p - '0');
        ++p;
        ++int_digits;
    }

    double val = static_cast<double>(int_part);
    int frac_digits = 0;
    if (p < end && *p == '.') {
        ++p;
        uint64_t frac_part = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            if (frac_digits < 18) {
                frac_part = frac_part * 10 + static_cast<uint64_t>(*p - '0');
                ++frac_digits;
            }
            ++p;
        }
        val += static_cast<double>(frac_part) / kPow10[frac_digits];
    }

    int total_digits = int_digits + frac_digits;
    if (total_digits > 0 && p == end && total_digits <= 18) {
        out = neg ? -val : val;
        return true;
    }

    // Only invoke strtod fallback if scientific notation ('e'/'E') is present!
    // This prevents ISO-8601 dates like "2026-10-01" from triggering strtod.
    if (p < end && (*p == 'e' || *p == 'E')) {
        char buf[64];
        if (sv.size() >= sizeof(buf)) return false;
        std::memcpy(buf, sv.data(), sv.size());
        buf[sv.size()] = '\0';
        char* str_end = nullptr;
        double v = std::strtod(buf, &str_end);
        if (str_end == buf + sv.size() && std::isfinite(v)) {
            out = v;
            return true;
        }
    }
    return false;
}

inline bool looksLikeIsoDate(std::string_view sv) noexcept {
    sv = trimView(sv);
    if (sv.size() < 8 || sv.size() > 32) return false;
    if (sv.size() >= 10 &&
        sv[0] >= '1' && sv[0] <= '2' &&
        sv[1] >= '0' && sv[1] <= '9' &&
        sv[2] >= '0' && sv[2] <= '9' &&
        sv[3] >= '0' && sv[3] <= '9' &&
        (sv[4] == '-' || sv[4] == '/') &&
        sv[5] >= '0' && sv[5] <= '1' &&
        sv[6] >= '0' && sv[6] <= '9') {
        return true;
    }
    return false;
}

inline double computeQuantileSorted(const std::vector<double>& sorted, double q) noexcept {
    if (sorted.empty()) return 0.0;
    if (sorted.size() == 1) return sorted.front();
    double pos = q * static_cast<double>(sorted.size() - 1);
    std::size_t idx = static_cast<std::size_t>(pos);
    double frac = pos - static_cast<double>(idx);
    if (idx + 1 < sorted.size()) {
        return sorted[idx] * (1.0 - frac) + sorted[idx + 1] * frac;
    }
    return sorted[idx];
}

void splitLineCells(std::string_view line, char delimiter, std::vector<std::string_view>& out) {
    out.clear();
    const char* ptr = line.data();
    const char* end = ptr + line.size();
    const char* cell_start = ptr;
    bool in_quotes = false;

    while (ptr < end) {
        char c = *ptr;
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == delimiter && !in_quotes) {
            out.emplace_back(cell_start, static_cast<std::size_t>(ptr - cell_start));
            cell_start = ptr + 1;
        }
        ++ptr;
    }
    out.emplace_back(cell_start, static_cast<std::size_t>(end - cell_start));
}

} // namespace

char CsvEngine::detectDelimiter(std::string_view sample) noexcept {
    std::size_t newline_pos = sample.find('\n');
    std::string_view first_line = (newline_pos == std::string_view::npos)
                                      ? sample
                                      : sample.substr(0, newline_pos);
    std::size_t commas = 0, tabs = 0, pipes = 0, semis = 0;
    for (char c : first_line) {
        if (c == ',') ++commas;
        else if (c == '\t') ++tabs;
        else if (c == '|') ++pipes;
        else if (c == ';') ++semis;
    }
    if (tabs >= commas && tabs >= pipes && tabs >= semis && tabs > 0) return '\t';
    if (pipes >= commas && pipes >= tabs && pipes >= semis && pipes > 0) return '|';
    if (semis > commas && semis > 0) return ';';
    return ',';
}

std::string CsvEngine::columnTypeToString(ColumnType t) {
    switch (t) {
        case ColumnType::Integer: return "INTEGER";
        case ColumnType::Numeric: return "NUMERIC (FLOAT64)";
        case ColumnType::Temporal: return "TEMPORAL (ISO-8601)";
        case ColumnType::Categorical: return "CATEGORICAL";
        case ColumnType::Identifier: return "IDENTIFIER";
    }
    return "UNKNOWN";
}

DatasetSummary CsvEngine::analyze(std::string_view content,
                                  const std::string& source_name,
                                  char delimiter) {
    DatasetSummary summary;
    summary.source_name = source_name;
    summary.byte_size = content.size();
    summary.sha256_checksum = Sha256::hashHex(content);

    if (content.empty()) return summary;

    auto t0 = std::chrono::steady_clock::now();
    summary.delimiter = (delimiter == '\0') ? detectDelimiter(content) : delimiter;

    const char* ptr = content.data();
    const char* end = ptr + content.size();

    const char* nl = static_cast<const char*>(std::memchr(ptr, '\n', static_cast<std::size_t>(end - ptr)));
    std::string_view header_line = nl
        ? std::string_view(ptr, static_cast<std::size_t>(nl - ptr))
        : std::string_view(ptr, static_cast<std::size_t>(end - ptr));
    if (!header_line.empty() && header_line.back() == '\r') {
        header_line.remove_suffix(1);
    }

    std::vector<std::string_view> cells;
    cells.reserve(32);
    splitLineCells(header_line, summary.delimiter, cells);

    summary.column_count = cells.size();
    summary.columns.resize(summary.column_count);

    struct ColAccumulator {
        std::size_t numeric_hits = 0;
        std::size_t int_hits = 0;
        std::size_t date_hits = 0;
        double mean = 0.0;
        double m2 = 0.0;
        double min_val = std::numeric_limits<double>::infinity();
        double max_val = -std::numeric_limits<double>::infinity();
    };
    std::vector<ColAccumulator> acc(summary.column_count);

    for (std::size_t i = 0; i < summary.column_count; ++i) {
        summary.columns[i].name = std::string(trimView(cells[i]));
        if (summary.columns[i].name.empty()) {
            summary.columns[i].name = "col_" + std::to_string(i);
        }
    }

    ptr = nl ? (nl + 1) : end;

    while (ptr < end) {
        const char* line_end = static_cast<const char*>(
            std::memchr(ptr, '\n', static_cast<std::size_t>(end - ptr)));
        if (!line_end) line_end = end;

        std::string_view line(ptr, static_cast<std::size_t>(line_end - ptr));
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        ptr = (line_end < end) ? (line_end + 1) : end;

        if (line.empty()) continue;

        splitLineCells(line, summary.delimiter, cells);
        ++summary.row_count;

        std::size_t ncols = std::min(cells.size(), summary.column_count);
        for (std::size_t c = 0; c < ncols; ++c) {
            std::string_view raw = trimView(cells[c]);
            auto& col = summary.columns[c];
            auto& ca = acc[c];

            if (raw.empty() || raw == "NULL" || raw == "null" || raw == "NA" || raw == "NaN") {
                ++col.null_count;
                continue;
            }

            ++col.count;
            double v = 0.0;
            if (fastParseDouble(raw, v)) {
                ++ca.numeric_hits;
                if (std::floor(v) == v && raw.find('.') == std::string_view::npos) {
                    ++ca.int_hits;
                }
                double delta = v - ca.mean;
                ca.mean += delta / static_cast<double>(ca.numeric_hits);
                double delta2 = v - ca.mean;
                ca.m2 += delta * delta2;
                if (v < ca.min_val) ca.min_val = v;
                if (v > ca.max_val) ca.max_val = v;

                if (col.numeric_values.size() < 16384 && ((ca.numeric_hits & 15) == 1 || summary.byte_size < 65536)) {
                    col.numeric_values.push_back(v);
                }
            } else {
                if (looksLikeIsoDate(raw)) {
                    ++ca.date_hits;
                }
                if (col.category_counts.size() < 64 && summary.row_count <= 2048) {
                    col.category_counts[std::string(raw)]++;
                }
            }
        }
    }

    for (std::size_t c = 0; c < summary.column_count; ++c) {
        auto& col = summary.columns[c];
        auto& ca = acc[c];

        if (col.count == 0) {
            col.type = ColumnType::Categorical;
            continue;
        }

        double num_ratio = static_cast<double>(ca.numeric_hits) / static_cast<double>(col.count);
        double date_ratio = static_cast<double>(ca.date_hits) / static_cast<double>(col.count);

        if (num_ratio >= 0.85) {
            col.type = (ca.int_hits == ca.numeric_hits) ? ColumnType::Integer : ColumnType::Numeric;
            col.min = ca.min_val;
            col.max = ca.max_val;
            col.mean = ca.mean;
            col.std_dev = (ca.numeric_hits > 1)
                              ? std::sqrt(ca.m2 / static_cast<double>(ca.numeric_hits - 1))
                              : 0.0;

            std::vector<double> sorted = col.numeric_values;
            std::sort(sorted.begin(), sorted.end());
            col.q25 = computeQuantileSorted(sorted, 0.25);
            col.median = computeQuantileSorted(sorted, 0.50);
            col.q75 = computeQuantileSorted(sorted, 0.75);
            col.iqr = col.q75 - col.q25;
            col.p90 = computeQuantileSorted(sorted, 0.90);
            col.p95 = computeQuantileSorted(sorted, 0.95);
            col.p99 = computeQuantileSorted(sorted, 0.99);
            col.distinct_estimate = sorted.size();
        } else if (date_ratio >= 0.80) {
            col.type = ColumnType::Temporal;
            col.distinct_estimate = col.category_counts.size();
        } else {
            col.distinct_estimate = col.category_counts.size();
            if (col.distinct_estimate == col.count && col.count > 20) {
                col.type = ColumnType::Identifier;
            } else {
                col.type = ColumnType::Categorical;
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    summary.parse_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double mb = static_cast<double>(summary.byte_size) / (1024.0 * 1024.0);
    summary.throughput_mb_s = (summary.parse_time_ms > 0.0)
                                  ? (mb / (summary.parse_time_ms / 1000.0))
                                  : 0.0;
    return summary;
}

std::vector<GroupByResultRow> CsvEngine::groupByAggregate(
    std::string_view content,
    std::size_t key_col_idx,
    std::size_t val_col_idx,
    char delimiter) {
    struct FlatSlot {
        std::string_view key{};
        std::size_t count = 0;
        double sum = 0.0;
        double min = std::numeric_limits<double>::infinity();
        double max = -std::numeric_limits<double>::infinity();
        bool occupied = false;
    };
    alignas(64) FlatSlot table[128]{};

    const char* ptr = content.data();
    const char* end = ptr + content.size();

    const char* nl = static_cast<const char*>(std::memchr(ptr, '\n', static_cast<std::size_t>(end - ptr)));
    ptr = nl ? (nl + 1) : end;

    std::size_t max_needed_col = std::max(key_col_idx, val_col_idx);

    while (ptr < end) {
        const char* line_end = static_cast<const char*>(
            std::memchr(ptr, '\n', static_cast<std::size_t>(end - ptr)));
        if (!line_end) line_end = end;

        const char* row_end = (line_end > ptr && *(line_end - 1) == '\r') ? (line_end - 1) : line_end;
        const char* cur = ptr;
        ptr = (line_end < end) ? (line_end + 1) : end;
        if (cur >= row_end) continue;

        std::string_view key_sv{};
        std::string_view val_sv{};
        bool found_key = false;
        bool found_val = false;

        std::size_t col_idx = 0;
        const char* cell_start = cur;
        bool in_quotes = false;
        while (cur <= row_end) {
            if (cur == row_end || (*cur == delimiter && !in_quotes)) {
                if (col_idx == key_col_idx) {
                    key_sv = std::string_view(cell_start, static_cast<std::size_t>(cur - cell_start));
                    found_key = true;
                }
                if (col_idx == val_col_idx) {
                    val_sv = std::string_view(cell_start, static_cast<std::size_t>(cur - cell_start));
                    found_val = true;
                }
                if (col_idx >= max_needed_col) break;
                ++col_idx;
                cell_start = cur + 1;
            } else if (*cur == '"') {
                in_quotes = !in_quotes;
            }
            ++cur;
        }

        if (!found_key || !found_val) continue;

        std::string_view key = trimView(key_sv);
        double val = 0.0;
        if (!fastParseDouble(val_sv, val)) continue;

        uint64_t h = 14695981039346656037ULL;
        for (char c : key) {
            h ^= static_cast<uint8_t>(c);
            h *= 1099511628211ULL;
        }
        std::size_t slot = static_cast<std::size_t>(h & 127ULL);
        for (std::size_t probe = 0; probe < 128; ++probe) {
            auto& s = table[(slot + probe) & 127ULL];
            if (!s.occupied) {
                s.occupied = true;
                s.key = key;
                s.count = 1;
                s.sum = val;
                s.min = val;
                s.max = val;
                break;
            }
            if (s.key == key) {
                ++s.count;
                s.sum += val;
                if (val < s.min) s.min = val;
                if (val > s.max) s.max = val;
                break;
            }
        }
    }

    std::vector<GroupByResultRow> result;
    for (const auto& s : table) {
        if (!s.occupied) continue;
        GroupByResultRow row;
        row.key = std::string(s.key);
        row.count = s.count;
        row.sum = s.sum;
        row.mean = (s.count > 0) ? (s.sum / static_cast<double>(s.count)) : 0.0;
        row.min = (s.count > 0) ? s.min : 0.0;
        row.max = (s.count > 0) ? s.max : 0.0;
        result.push_back(std::move(row));
    }

    std::sort(result.begin(), result.end(), [](const GroupByResultRow& a, const GroupByResultRow& b) {
        return a.sum > b.sum;
    });
    return result;
}

} // namespace velox
