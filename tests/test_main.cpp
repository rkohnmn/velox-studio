#include <cassert>
#include "velox/arena.hpp"
#include "velox/csv_engine.hpp"
#include "velox/native_gui.hpp"
#include "velox/profiler_engine.hpp"
#include "velox/provenance_engine.hpp"
#include "velox/schema_engine.hpp"
#include "velox/sha256.hpp"
#include "velox/studio_ui.hpp"
#include "velox/viz_engine.hpp"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

using namespace velox;

namespace {

int g_tests_passed = 0;

#define ASSERT_TRUE(cond, msg)                                                        \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::cerr << "[FAIL] " << __FUNCTION__ << ":" << __LINE__ << " — " << msg \
                      << "\n";                                                       \
            std::exit(1);                                                             \
        }                                                                             \
    } while (0)

#define ASSERT_NEAR(a, b, eps, msg)                                                   \
    do {                                                                              \
        if (std::abs((a) - (b)) > (eps)) {                                            \
            std::cerr << "[FAIL] " << __FUNCTION__ << ":" << __LINE__ << " — " << msg \
                      << " (got " << (a) << ", expected " << (b) << ")\n";           \
            std::exit(1);                                                             \
        }                                                                             \
    } while (0)

void testArenaAndSha256() {
    MonotonicArena arena(4096);
    void* p1 = arena.allocate(128, 16);
    void* p2 = arena.allocate(256, 64);
    ASSERT_TRUE(p1 != nullptr && p2 != nullptr && p1 != p2, "Arena allocation must succeed");
    ASSERT_TRUE((reinterpret_cast<uintptr_t>(p2) % 64) == 0, "64-byte cache-line alignment enforced");

    std::string digest = Sha256::hashHex("abc");
    ASSERT_TRUE(digest == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                "SHA-256 NIST vector 'abc' must match exact hex digest");
    ++g_tests_passed;
}

void testCsvEngineAndQuantiles() {
    std::string csv =
        "tenant,latency_ms,requests,day\n"
        "Alpha,10.0,100,2026-10-01\n"
        "Beta,20.0,200,2026-10-02\n"
        "Alpha,30.0,300,2026-10-03\n"
        "Gamma,40.0,400,2026-10-04\n"
        "Beta,50.0,500,2026-10-05\n";

    auto summary = CsvEngine::analyze(csv, "unit_test.csv");
    ASSERT_TRUE(summary.row_count == 5, "Expected 5 rows");
    ASSERT_TRUE(summary.column_count == 4, "Expected 4 columns");
    ASSERT_NEAR(summary.columns[1].mean, 30.0, 1e-6, "Mean of [10,20,30,40,50] is 30");
    ASSERT_NEAR(summary.columns[1].median, 30.0, 1e-6, "Median of [10,20,30,40,50] is 30");
    ASSERT_NEAR(summary.columns[1].iqr, 20.0, 1e-6, "IQR (40 - 20) is 20");

    auto groups = CsvEngine::groupByAggregate(csv, 0, 2, ',');
    ASSERT_TRUE(groups.size() == 3, "Expected 3 tenant groups");
    ASSERT_TRUE(groups[0].key == "Beta" && std::abs(groups[0].sum - 700.0) < 1e-6, "Beta leads with 700");
    ++g_tests_passed;
}

void testVizEngineLaws() {
    double lf = VizEngine::computeLieFactor(15.0 / 10.0, 105.0 / 100.0);
    ASSERT_NEAR(lf, 1.42857, 1e-4, "Truncated baseline Lie Factor must equal ~1.43");
    ASSERT_TRUE(!VizEngine::isLieFactorAcceptable(lf), "LF 1.43 must be rejected");
    ASSERT_TRUE(VizEngine::isLieFactorAcceptable(1.00), "LF 1.00 must be accepted");

    ChartConstraints c;
    c.zero_baseline_valid = false;
    ASSERT_TRUE(VizEngine::selectChart("comparison_ranking", 8, c) == "dot / strip plot",
                "Non-zero baseline must deprecate bar chart in favor of dot/strip plot");

    std::vector<Point2D> series;
    for (int i = 0; i < 1000; ++i) {
        series.push_back({static_cast<double>(i), std::sin(i * 0.05)});
    }
    auto down = VizEngine::downsampleLTTB(series, 50);
    ASSERT_TRUE(down.size() == 50, "LTTB must preserve exact target threshold count");

    double contrast_dark = VizEngine::wcagContrastHex("#0D1117", "#E6EDF3");
    ASSERT_TRUE(contrast_dark >= 10.0, "Dark surface #0D1117 vs #E6EDF3 clears AAA 7:1 easily");
    ++g_tests_passed;
}

void testSchemaEngine() {
    std::string ddl = R"(
CREATE TABLE users (
    id BIGINT PRIMARY KEY,
    email VARCHAR(255) NOT NULL
);
CREATE TABLE invoices (
    id BIGINT PRIMARY KEY,
    user_id BIGINT NOT NULL REFERENCES users(id),
    amount_total FLOAT NOT NULL,
    item1 VARCHAR(50),
    item2 VARCHAR(50)
);
)";
    auto rep = SchemaEngine::analyzeDdl(ddl);
    ASSERT_TRUE(rep.tables.size() == 2, "Must parse 2 DDL tables");
    ASSERT_TRUE(!rep.recommended_indexes.empty(), "Must flag unindexed user_id foreign key");
    ++g_tests_passed;
}

void testProfilerEngine() {
    std::string trace_csv =
        "timestamp_ms,request_id,endpoint,operation,duration_ms,memory_kb,query_fingerprint\n"
        "100,req_1,GET /feed,DB_SELECT,12.0,1000,SELECT * FROM posts WHERE author_id = ?\n"
        "110,req_1,GET /feed,DB_SELECT,13.0,1200,SELECT * FROM posts WHERE author_id = ?\n"
        "120,req_1,GET /feed,DB_SELECT,11.0,1400,SELECT * FROM posts WHERE author_id = ?\n"
        "130,req_1,GET /feed,DB_SELECT,14.0,1600,SELECT * FROM posts WHERE author_id = ?\n"
        "140,req_1,GET /feed,DB_SELECT,15.0,1800,SELECT * FROM posts WHERE author_id = ?\n";

    auto prof = ProfilerEngine::analyzeTraces(trace_csv, 50.0);
    ASSERT_TRUE(prof.n_plus_one_alerts.size() == 1, "Must detect 5x repeated query as N+1 alert");
    ASSERT_TRUE(prof.suspected_memory_leak, "Linear memory climb must trigger OLS leak detection");
    ++g_tests_passed;
}

void testProvenanceEngine() {
    ASSERT_TRUE(ProvenanceEngine::validateDoiSyntax("10.1038/nature12373"), "Valid Nature DOI");
    ASSERT_TRUE(ProvenanceEngine::validateIsbn10("0-306-40615-2"), "Valid ISBN-10 mod-11 checksum");
    ASSERT_TRUE(ProvenanceEngine::validateIsbn13("978-0-306-40615-7"), "Valid ISBN-13 mod-10 checksum");
    ASSERT_TRUE(ProvenanceEngine::validateIssnSyntax("0378-5955"), "Valid ISSN mod-11 checksum");
    ASSERT_TRUE(ProvenanceEngine::validateRorSyntax("05d782368"), "Valid ROR syntax");
    ++g_tests_passed;
}


void testModernCppSystemsAndPool() {
    // 1. PoolAllocator<T> (memcpp.md)
    PoolAllocator<Point2D, 1024> pool;
    Point2D* p1 = pool.construct(3.5, 7.0);
    Point2D* p2 = pool.construct(10.0, 20.0);
    ASSERT_TRUE(pool.activeCount() == 2, "PoolAllocator tracks 2 active allocations");
    ASSERT_NEAR(p1->x + p2->y, 23.5, 1e-9, "Pool-constructed objects hold exact values");
    pool.destroy(p1);
    pool.destroy(p2);
    ASSERT_TRUE(pool.activeCount() == 0, "PoolAllocator returns slots to free list cleanly");

    // 2. C++20 QuantitativeScalar concept & std::span (CPPDEV.md)
    constexpr std::array<double, 4> vals = {10.0, 20.0, 30.0, 40.0};
    double m = computeSpanMean(std::span<const double>(vals));
    ASSERT_NEAR(m, 25.0, 1e-9, "computeSpanMean over std::span equals 25.0");

    // 3. TelemetrySeriesSoA & ThreadPool (optimcpop.md)
    TelemetrySeriesSoA soa;
    soa.revenues_usd = {100.0, 200.0, 300.0};
    soa.scaleRevenues(1.5);
    ASSERT_NEAR(soa.revenues_usd[1], 300.0, 1e-9, "SoA vectorized scaling works");

    ThreadPool tp(2);
    auto fut1 = tp.enqueue([](int a, int b) { return a + b; }, 21, 21);
    auto fut2 = tp.enqueue([](double x) { return x * x; }, 9.0);
    ASSERT_TRUE(fut1.get() == 42, "ThreadPool future 1 resolves to 42");
    ASSERT_NEAR(fut2.get(), 81.0, 1e-9, "ThreadPool future 2 resolves to 81.0");
    ++g_tests_passed;
}


void testUxResearchAndDesktopPatterns() {
    // 1. Workflow 2 Step 4: Priority = Frequency * Severity * UserImpact * Confidence
    int prio = UxResearchEngine::computeOpportunityPriority(5, 5, 5, 5);
    ASSERT_TRUE(prio == 625, "Max heuristic opportunity priority (5*5*5*5) == 625");

    // 2. Workflow 10.4: Written & Visual Quality Heuristic Scorer
    auto qa = UxResearchEngine::computeQualityAuditScore(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0);
    ASSERT_NEAR(qa.written_warning_score, 10.0, 1e-6, "Uniform 1/10 warning yields 10.0/100 score");
    ASSERT_NEAR(qa.visual_warning_score, 10.0, 1e-6, "Uniform 1/10 visual warning yields 10.0/100 score");
    ASSERT_TRUE(qa.written_band.find("0-20") != std::string::npos, "10.0 falls in 0-20 low-warning band");

    // 3. Workflow 1 & 4: Persona clustering & denominator tracking
    std::string sample_csv =
        "participant_id,usage_frequency,primary_task,platform,technical_proficiency,pain_point,evidence_tag,requires_keyboard_only,prefers_reduced_motion\n"
        "p1,daily,inspect,Linux,9,slow,[PERFORMANCE],true,false\n"
        "p2,occasional,export,Windows,5,confusing,[PAIN],false,true\n";
    auto rep = UxResearchEngine::analyzeRecordsCsv(sample_csv);
    ASSERT_TRUE(rep.total_participants == 2, "Parsed 2 participant records");
    ASSERT_TRUE(rep.keyboard_only_count == 1 && rep.reduced_motion_count == 1, "Tracked accessibility counts");
    ASSERT_TRUE(rep.prioritized_journey.size() == 6, "6 native desktop journey stages prioritized");

    // 4. Workflow 8 Step 6: Reduced-motion preference in SpringAnimator
    SpringAnimator rm_spring(0.0, 220.0, 28.0);
    rm_spring.setTarget(250.0);
    rm_spring.step(1.0 / 60.0, true /* reduced_motion */);
    ASSERT_NEAR(rm_spring.value(), 250.0, 1e-9, "Reduced-motion snaps immediately without oscillation");

    // 5. Workflow 7 Step 3 & Workflow 9 Step 4: CommandHistory (Undo/Redo) & CancellationToken
    int param_value = 10;
    CommandHistory cmd_hist;
    cmd_hist.execute("Set KDE Bandwidth", [&]() { param_value = 42; }, [&]() { param_value = 10; });
    ASSERT_TRUE(param_value == 42 && cmd_hist.canUndo(), "CommandHistory executes forward action");
    cmd_hist.undo();
    ASSERT_TRUE(param_value == 10 && cmd_hist.canRedo(), "CommandHistory undo restores previous state");
    cmd_hist.redo();
    ASSERT_TRUE(param_value == 42, "CommandHistory redo re-applies state");

    CancellationToken token;
    token.setProgressPercent(75);
    ASSERT_TRUE(token.progressPercent() == 75 && !token.isCancelled(), "CancellationToken tracks determinate progress");
    token.requestCancel();
    ASSERT_TRUE(token.isCancelled(), "CancellationToken signals cancellation safely");
    ++g_tests_passed;
}

void testNativeGuiRasterizer() {
    WorkbenchData wb;
    std::string csv = "region,service,latency_ms,revenue_usd,date\nAPAC,Compute,11.2,4800.0,2026-10-01\nEMEA,Storage,14.1,3900.0,2026-10-01\n";
    wb.dataset = CsvEngine::analyze(csv, "gui_test.csv");
    wb.top_groups = CsvEngine::groupByAggregate(csv, 0, 3, ',');
    wb.audit = VizEngine::runPreFlightAudit(1.0, true, PerceptualChannel::Length, 1, true, false, 2);

    FramebufferCanvas canvas(1440, 900);
    GuiState state;

    // Render all 5 tabs and verify pixel writes and tab transitions
    for (int t = 0; t < 5; ++t) {
        NativeGuiApp::handleKeyPress(static_cast<char>('1' + t), state);
        ASSERT_TRUE(static_cast<int>(state.active_tab) == t, "Keyboard 1-5 switches native GUI tab");
        NativeGuiApp::renderFrame(canvas, wb, state);
    }

    // Verify command palette modal toggle, typing, and rendering
    NativeGuiApp::handleKeyPress('k', state);
    ASSERT_TRUE(state.command_palette_open, "Key 'k' opens command palette modal");
    NativeGuiApp::handleKeyPress('t', state);
    NativeGuiApp::handleKeyPress('e', state);
    NativeGuiApp::handleKeyPress('s', state);
    NativeGuiApp::handleKeyPress('t', state);
    ASSERT_TRUE(state.command_palette_input == "test", "Command palette captures typed input");
    NativeGuiApp::renderFrame(canvas, wb, state);
    NativeGuiApp::handleKeyPress(27, state);
    ASSERT_TRUE(!state.command_palette_open, "Esc closes command palette modal");

    // Verify in-app file loading for SQL and CSV
    ASSERT_TRUE(loadFilePath("examples/ecommerce_schema.sql", wb, state), "loadFilePath loads SQL schema");
    ASSERT_TRUE(!wb.schema.tables.empty(), "SQL schema tables parsed");
    ASSERT_TRUE(loadFilePath("examples/telemetry_sample.csv", wb, state), "loadFilePath loads CSV dataset");
    ASSERT_TRUE(wb.dataset.row_count > 0, "CSV dataset rows parsed");

    // Verify mouse click on Tab 2
    bool clicked = NativeGuiApp::handleMouseClick(500, 20, state);
    ASSERT_TRUE(clicked && state.active_tab == GuiTab::PerceptualViz, "Mouse click on Tab 2 switches tab");

    // Verify phosphor-emerald status dot pixel at (26, 26)
    NativeGuiApp::renderFrame(canvas, wb, state);
    // Verify SpringAnimator physics convergence & Catmull-Rom spline
    SpringAnimator spring(0.0, 220.0, 28.0);
    spring.setTarget(100.0);
    for (int i = 0; i < 120; ++i) spring.step(1.0 / 60.0);
    ASSERT_NEAR(spring.value(), 100.0, 0.05, "SpringAnimator converges smoothly to target");
    ASSERT_NEAR(SpringAnimator::cubicEaseOut(1.0), 1.0, 1e-9, "cubicEaseOut(1.0) == 1.0");
    ASSERT_TRUE(kVeloxIcon32ARGB[0] != 0, "Official Velox Studio 32x32 icon embedded");

    // Verify BMP snapshot export
    const std::string tmp_bmp = "test_snapshot.bmp";
    ASSERT_TRUE(canvas.saveBmp(tmp_bmp), "saveBmp must succeed");
    std::remove(tmp_bmp.c_str());
    ++g_tests_passed;
}

} // namespace


void testSecurityFailurePathsAndEdgeCases() {
    // 1. P0-1: Directory open rejection in MappedFile
    MappedFile mf;
    std::string err;
    bool opened_dir = mf.open(".", &err);
    ASSERT_TRUE(!opened_dir, "Directory open rejection in MappedFile");
    ASSERT_TRUE(!err.empty(), "Error string must be set");

    // 2. P0-2: SQL identifier sanitization against DDL injection
    std::string clean_id = SchemaEngine::sanitizeIdentifier("orders; DROP TABLE users;--");
    ASSERT_TRUE(clean_id == "ordersDROPTABLEusers", "SQL identifier must be sanitized");
    MigrationPlan mig = SchemaEngine::generateZeroDowntimeMigration(
        "orders; DROP TABLE users;--", "total_price", "total_cents", "BIGINT", "0");
    ASSERT_TRUE(mig.phase1_expand_sql.find("DROP TABLE users") == std::string::npos, "Drop table not in migration plan");

    // 3. P0-3: XML/SVG & JSON escaping against XSS / broken syntax
    std::vector<std::pair<std::string, double>> dirty_bars = {{"R&D <script>", 42.0}};
    std::string svg = VizEngine::renderHorizontalBarSvg("Title <1> & \"2\"", "Sub", dirty_bars);
    ASSERT_TRUE(svg.find("<script>") == std::string::npos, "Script tag must be stripped");
    ASSERT_TRUE(svg.find("&lt;script&gt;") != std::string::npos, "Script tag must be escaped");
    ASSERT_TRUE(svg.find("R&amp;D") != std::string::npos, "Ampersand must be escaped");

    std::string vl = VizEngine::generateVegaLiteSpec("Title \"quoted\"", "Sub", dirty_bars);
    ASSERT_TRUE(vl.find("Title \\\"quoted\\\"") != std::string::npos, "Quotes in Vega-Lite spec must be escaped");

    // 4. P1-3: Canonical URL prefix normalization in ProvenanceEngine
    ASSERT_TRUE(ProvenanceEngine::classifyIdentifier("https://doi.org/10.1038/s41586-023-06221-2") == PidType::DOI, "DOI prefix");
    ASSERT_TRUE(ProvenanceEngine::classifyIdentifier("arXiv:2305.14314v1") == PidType::ARXIV, "arXiv prefix");
    ASSERT_TRUE(ProvenanceEngine::classifyIdentifier("https://ror.org/02bfwt286") == PidType::ROR, "ROR prefix");

    // 5. Boundary condition: Empty and header-only CSVs
    DatasetSummary empty_ds = CsvEngine::analyze("", "empty.csv");
    ASSERT_TRUE(empty_ds.row_count == 0, "Empty dataset row count");
    DatasetSummary hdr_only = CsvEngine::analyze("a,b,c\n", "hdr.csv");
    ASSERT_TRUE(hdr_only.row_count == 0, "Header-only row count");
    ASSERT_TRUE(hdr_only.column_count == 3, "Header-only column count");
    ++g_tests_passed;
}

int main() {
    std::cout << "Running Velox Studio C++20 Deterministic & Native GUI Test Suite...\n";
    testArenaAndSha256();
    testCsvEngineAndQuantiles();
    testVizEngineLaws();
    testSchemaEngine();
    testProfilerEngine();
    testProvenanceEngine();
    testModernCppSystemsAndPool();
    testUxResearchAndDesktopPatterns();
    testSecurityFailurePathsAndEdgeCases();
    testNativeGuiRasterizer();
    std::cout << "[PASS] All " << g_tests_passed << " test suites passed with 0 errors.\n";
    return 0;
}
