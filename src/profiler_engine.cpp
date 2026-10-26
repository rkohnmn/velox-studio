#include "velox/profiler_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_map>

namespace velox {

namespace {

std::string trimCell(std::string_view sv) {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\r' || sv.front() == '\t')) sv.remove_prefix(1);
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\r' || sv.back() == '\t')) sv.remove_suffix(1);
    return std::string(sv);
}

double sortedQuantile(const std::vector<double>& v, double q) noexcept {
    if (v.empty()) return 0.0;
    double pos = q * static_cast<double>(v.size() - 1);
    std::size_t idx = static_cast<std::size_t>(pos);
    double frac = pos - static_cast<double>(idx);
    if (idx + 1 < v.size()) return v[idx] * (1.0 - frac) + v[idx + 1] * frac;
    return v[idx];
}

} // namespace

ProfileReport ProfilerEngine::analyzeTraces(std::string_view trace_csv, double sla_p99_ms) {
    ProfileReport report;

    std::istringstream iss{std::string(trace_csv)};
    std::string line;
    bool first = true;

    std::vector<double> all_durations;
    std::unordered_map<std::string, std::vector<double>> endpoint_durations;
    std::unordered_map<std::string, double> operation_totals;

    struct QueryKey {
        std::string req_id;
        std::string endpoint;
        std::string fingerprint;
        bool operator==(const QueryKey& o) const {
            return req_id == o.req_id && endpoint == o.endpoint && fingerprint == o.fingerprint;
        }
    };
    struct QueryKeyHash {
        std::size_t operator()(const QueryKey& k) const {
            return std::hash<std::string>{}(k.req_id + "|" + k.endpoint + "|" + k.fingerprint);
        }
    };
    std::unordered_map<QueryKey, std::pair<std::size_t, double>, QueryKeyHash> query_counts;

    std::vector<double> time_sec;
    std::vector<double> mem_kb;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        if (first) {
            first = false;
            if (line.find("timestamp") != std::string::npos) continue;
        }

        std::vector<std::string> cols;
        std::istringstream lss(line);
        std::string cell;
        while (std::getline(lss, cell, ',')) {
            cols.push_back(trimCell(cell));
        }
        if (cols.size() < 6) continue;

        double ts_ms = std::strtod(cols[0].c_str(), nullptr);
        std::string req_id = cols[1];
        std::string endpoint = cols[2];
        std::string op = cols[3];
        double dur_ms = std::strtod(cols[4].c_str(), nullptr);
        double m_kb = std::strtod(cols[5].c_str(), nullptr);
        std::string q_fp = (cols.size() >= 7) ? cols[6] : "";

        ++report.total_spans;
        all_durations.push_back(dur_ms);
        endpoint_durations[endpoint].push_back(dur_ms);
        operation_totals[endpoint + " :: " + op] += dur_ms;

        time_sec.push_back(ts_ms / 1000.0);
        mem_kb.push_back(m_kb);

        if (!q_fp.empty() && q_fp != "none" && q_fp != "-") {
            auto& entry = query_counts[{req_id, endpoint, q_fp}];
            entry.first += 1;
            entry.second += dur_ms;
        }
    }

    std::sort(all_durations.begin(), all_durations.end());
    report.overall_p50_ms = sortedQuantile(all_durations, 0.50);
    report.overall_p95_ms = sortedQuantile(all_durations, 0.95);
    report.overall_p99_ms = sortedQuantile(all_durations, 0.99);

    for (auto& [ep, durs] : endpoint_durations) {
        std::sort(durs.begin(), durs.end());
        EndpointLatencySummary s;
        s.endpoint = ep;
        s.request_count = durs.size();
        s.p50_ms = sortedQuantile(durs, 0.50);
        s.p95_ms = sortedQuantile(durs, 0.95);
        s.p99_ms = sortedQuantile(durs, 0.99);
        s.max_ms = durs.empty() ? 0.0 : durs.back();
        s.exceeds_sla = (s.p99_ms > sla_p99_ms);
        report.endpoints.push_back(std::move(s));
    }

    std::sort(report.endpoints.begin(), report.endpoints.end(),
              [](const EndpointLatencySummary& a, const EndpointLatencySummary& b) {
                  return a.p99_ms > b.p99_ms;
              });

    // N+1 query detection (threshold: >= 5 identical query fingerprints in a single request)
    for (const auto& [k, v] : query_counts) {
        if (v.first >= 5) {
            report.n_plus_one_alerts.push_back({
                k.req_id, k.endpoint, k.fingerprint, v.first, v.second
            });
        }
    }
    std::sort(report.n_plus_one_alerts.begin(), report.n_plus_one_alerts.end(),
              [](const NPlusOneDetection& a, const NPlusOneDetection& b) {
                  return a.execution_count > b.execution_count;
              });

    // OLS linear regression for heap memory leak detection
    if (time_sec.size() >= 4) {
        double n = static_cast<double>(time_sec.size());
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0, sum_yy = 0.0;
        double t0 = time_sec.front();
        for (std::size_t i = 0; i < time_sec.size(); ++i) {
            double x = time_sec[i] - t0;
            double y = mem_kb[i];
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_xx += x * x;
            sum_yy += y * y;
        }
        double denom = n * sum_xx - sum_x * sum_x;
        if (std::abs(denom) > 1e-9) {
            double slope = (n * sum_xy - sum_x * sum_y) / denom;
            report.memory_growth_kb_per_sec = slope;
            double r_num = (n * sum_xy - sum_x * sum_y);
            double r_den = std::sqrt((n * sum_xx - sum_x * sum_x) * (n * sum_yy - sum_y * sum_y));
            double r = (std::abs(r_den) > 1e-9) ? (r_num / r_den) : 0.0;
            report.memory_regression_r2 = r * r;
            report.suspected_memory_leak = (slope > 64.0 && report.memory_regression_r2 > 0.75);
        }
    }

    // Build ASCII/Unicode Flamegraph
    std::vector<std::pair<std::string, double>> ops(operation_totals.begin(), operation_totals.end());
    std::sort(ops.begin(), ops.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    double max_op = ops.empty() ? 1.0 : std::max(1.0, ops.front().second);

    std::ostringstream fg;
    for (const auto& [op_name, total_ms] : ops) {
        int bars = std::max(1, static_cast<int>(std::round((total_ms / max_op) * 36.0)));
        fg << "  " << std::left << std::setw(38) << op_name << " | ";
        for (int b = 0; b < bars; ++b) fg << "#";
        for (int b = bars; b < 36; ++b) fg << ".";
        fg << " | " << std::right << std::setw(8) << std::fixed << std::setprecision(2) << total_ms << " ms\n";
    }
    report.ascii_flamegraph = fg.str();
    return report;
}

} // namespace velox
