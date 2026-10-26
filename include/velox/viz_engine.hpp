#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace velox {

/**
 * Cleveland-McGill Hierarchy of Elementary Perceptual Tasks (data-visualization.md §1.1)
 */
enum class PerceptualChannel {
    PositionCommonScale = 1,
    PositionNonAligned = 2,
    Length = 3,
    DirectionSlope = 4,
    Angle = 5,
    Area = 6,
    Volume = 7,
    Curvature = 8,
    ShadingSaturation = 9,
    CategoricalHue = 10
};

struct ChartConstraints {
    bool temporal = false;
    bool zero_baseline_valid = true;
    bool hierarchical = false;
    bool single_snapshot = false;
    bool suspect_multimodal = false;
    bool conserves_flow = false;
    bool piecewise_constant = false;
    bool composition_over_time = false;
    bool gestalt_only = false;
    bool raw_counts = false;
    bool rate_or_ratio = false;
    bool landmass_bias_critical = false;
    bool temporal_path = false;
    std::size_t sample_n = 100;
    std::size_t dimensions = 2;
    std::size_t series_count = 1;
    double span_years = 1.0;
};

struct OklchColor {
    double L = 0.65; // 0.0 - 1.0
    double C = 0.15; // 0.0 - 0.4
    double H = 250.0; // 0.0 - 360.0
    std::string toCssOklch() const;
    std::string toHex() const;
    double relativeLuminance() const;
};

struct DirectLabel {
    std::string text;
    double y = 0.0;
    std::string color_hex;
};

struct HistogramBin {
    double lower = 0.0;
    double upper = 0.0;
    std::size_t count = 0;
};

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct PreFlightAuditResult {
    bool passed_all_blocking = true;
    std::size_t score_out_of_15 = 15;
    double lie_factor = 1.0;
    double min_text_contrast = 12.6;
    double min_mark_contrast = 4.8;
    std::string recommended_engine;
    std::vector<std::pair<std::string, bool>> checklist;
};

class VizEngine {
public:
    // §2.0 Algorithmic Chart Dispatch
    static std::string selectChart(const std::string& purpose,
                                   std::size_t cardinality,
                                   const ChartConstraints& constraints);

    // §1.4 Mathematical Lie Factor & Perceptual Integrity
    static double computeLieFactor(double visual_effect_ratio, double data_effect_ratio) noexcept;
    static bool isLieFactorAcceptable(double lf) noexcept {
        return lf >= 0.95 && lf <= 1.05;
    }
    static double proportionalRadius(double value, double k = 1.0) noexcept;
    static std::vector<double> indexSeriesTo100(const std::vector<double>& values);
    static double bankTo45AspectHeight(const std::vector<Point2D>& points, double width);

    // §2.3 Distributions & Density (Freedman-Diaconis & Silverman)
    static double freedmanDiaconisBinWidth(const std::vector<double>& values, double iqr) noexcept;
    static std::vector<HistogramBin> computeHistogram(const std::vector<double>& values, double iqr);
    static double silvermanBandwidth(double std_dev, double iqr, std::size_t n) noexcept;

    // §5.2 Rendering Engine Dispatch & LTTB Perceptual Downsampling
    static std::string selectRenderingEngine(std::size_t mark_count);
    static std::vector<Point2D> downsampleLTTB(const std::vector<Point2D>& data, std::size_t threshold);

    // §3.2 - §3.5 OKLCH Color Science & WCAG 2.1 Contrast
    static std::vector<OklchColor> buildSequentialRamp(std::size_t steps,
                                                       double hue = 250.0,
                                                       double chroma = 0.12,
                                                       double l_min = 0.22,
                                                       double l_max = 0.92);
    static std::vector<OklchColor> buildCategoricalPalette(std::size_t count,
                                                           double lightness = 0.68,
                                                           double chroma = 0.15,
                                                           double start_hue = 255.0);
    static double wcagContrastRatio(const OklchColor& a, const OklchColor& b) noexcept;
    static double wcagContrastHex(const std::string& hex1, const std::string& hex2) noexcept;

    // §4.2 Direct Label Decollision Solver
    static std::vector<DirectLabel> decollideLabels(std::vector<DirectLabel> labels,
                                                    double min_gap = 14.0,
                                                    std::size_t max_iterations = 64);

    // §7 15-Point Pre-Flight Audit
    static PreFlightAuditResult runPreFlightAudit(double lie_factor,
                                                  bool zero_baseline,
                                                  PerceptualChannel primary_channel,
                                                  std::size_t series_count,
                                                  bool uses_direct_labels,
                                                  bool has_dual_axis,
                                                  std::size_t mark_count,
                                                  double text_contrast = 12.6,
                                                  double mark_contrast = 4.8);

    // §6.2 Production Vega-Lite v5 Spec & Self-Contained SVG Generation
    static std::string generateVegaLiteSpec(const std::string& headline_title,
                                            const std::string& subtitle,
                                            const std::vector<std::pair<std::string, double>>& category_totals);

    static std::string renderHorizontalBarSvg(const std::string& headline_title,
                                              const std::string& subtitle,
                                              const std::vector<std::pair<std::string, double>>& bars);
};

} // namespace velox
