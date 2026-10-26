#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace velox {

struct EndpointLatencySummary {
    std::string endpoint;
    std::size_t request_count = 0;
    double p50_ms = 0.0;
    double p95_ms = 0.0;
    double p99_ms = 0.0;
    double max_ms = 0.0;
    bool exceeds_sla = false;
};

struct NPlusOneDetection {
    std::string request_id;
    std::string endpoint;
    std::string query_fingerprint;
    std::size_t execution_count = 0;
    double cumulative_latency_ms = 0.0;
};

struct ProfileReport {
    std::size_t total_spans = 0;
    double overall_p50_ms = 0.0;
    double overall_p95_ms = 0.0;
    double overall_p99_ms = 0.0;
    double memory_growth_kb_per_sec = 0.0;
    double memory_regression_r2 = 0.0;
    bool suspected_memory_leak = false;
    std::vector<EndpointLatencySummary> endpoints;
    std::vector<NPlusOneDetection> n_plus_one_alerts;
    std::string ascii_flamegraph;
};

class ProfilerEngine {
public:
    /**
     * Parses CSV trace spans with columns:
     * timestamp_ms,request_id,endpoint,operation,duration_ms,memory_kb,query_fingerprint
     */
    static ProfileReport analyzeTraces(std::string_view trace_csv, double sla_p99_ms = 100.0);
};

} // namespace velox
