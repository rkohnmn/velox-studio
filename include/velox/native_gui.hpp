#pragma once

#include "velox/animator.hpp"
#include "velox/brand_assets.hpp"
#include "velox/csv_engine.hpp"
#include "velox/design_tokens.hpp"
#include "velox/font_atlas.hpp"
#include "velox/modern_cpp_systems.hpp"
#include "velox/profiler_engine.hpp"
#include "velox/provenance_engine.hpp"
#include "velox/schema_engine.hpp"
#include "velox/ux_research_engine.hpp"
#include "velox/viz_engine.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace velox {

struct Color32 {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    constexpr uint32_t toARGB() const noexcept {
        return (static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(b);
    }

    static constexpr Color32 fromHex(uint32_t rgb, uint8_t alpha = 255) noexcept {
        return {
            static_cast<uint8_t>((rgb >> 16) & 0xFFU),
            static_cast<uint8_t>((rgb >> 8) & 0xFFU),
            static_cast<uint8_t>(rgb & 0xFFU),
            alpha
        };
    }

    constexpr Color32 withAlpha(uint8_t new_a) const noexcept {
        return {r, g, b, new_a};
    }
};

namespace ThemePalette {
    inline constexpr Color32 Background      = Color32::fromHex(DesignTokens::Color::Background);
    inline constexpr Color32 CommandBarBg    = Color32::fromHex(DesignTokens::Color::CommandBar);
    inline constexpr Color32 DockPaneBg      = Color32::fromHex(DesignTokens::Color::Surface);
    inline constexpr Color32 Surface         = Color32::fromHex(0x111722U);
    inline constexpr Color32 SurfaceElevated = Color32::fromHex(DesignTokens::Color::SurfaceElevated);
    inline constexpr Color32 SurfaceHover    = Color32::fromHex(DesignTokens::Color::SurfaceHover);
    inline constexpr Color32 Border          = Color32::fromHex(DesignTokens::Color::BorderSubtle);
    inline constexpr Color32 BorderHighlight = Color32::fromHex(DesignTokens::Color::BorderStrong);
    inline constexpr Color32 Primary         = Color32::fromHex(DesignTokens::Color::Primary);
    inline constexpr Color32 PrimaryMuted    = Color32::fromHex(DesignTokens::Color::PrimaryMuted);
    inline constexpr Color32 Secondary       = Color32::fromHex(DesignTokens::Color::Secondary);
    inline constexpr Color32 AccentPurple    = Color32::fromHex(0xB084F5U);
    inline constexpr Color32 AccentCyan      = Color32::fromHex(0x39C5CFU);
    inline constexpr Color32 TextPrimary     = Color32::fromHex(DesignTokens::Color::TextPrimary);
    inline constexpr Color32 TextSecondary   = Color32::fromHex(DesignTokens::Color::TextSecondary);
    inline constexpr Color32 TextMuted       = Color32::fromHex(DesignTokens::Color::TextMuted);
    inline constexpr Color32 Error           = Color32::fromHex(DesignTokens::Color::Error);
    inline constexpr Color32 Warning         = Color32::fromHex(DesignTokens::Color::Warning);
}

enum class GuiTab : int {
    OverviewBento = 0,
    PerceptualViz = 1,
    SchemaAndErd  = 2,
    TraceProfiler = 3,
    Provenance    = 4
};

struct UiRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    constexpr bool contains(int px, int py) const noexcept {
        return px >= x && px < (x + w) && py >= y && py < (y + h);
    }
};

struct GuiState {
    GuiTab active_tab = GuiTab::OverviewBento;
    int selected_nav_item = 4;
    int sort_column_index = 4;
    bool sort_descending = true;
    int mouse_x = -1;
    int mouse_y = -1;
    bool mouse_down = false;
    int hovered_bar_index = -1;
    double kde_bandwidth_slider = 0.42;
    double sla_budget_ms_slider = 100.0;
    uint32_t audit_run_count = 1;
    SpringAnimator tab_indicator_x{296.0, 260.0, 30.0};
    SpringAnimator bar_growth_factor{1.0, 200.0, 26.0};
    CommandHistory command_history{};
    bool reduced_motion = false;
    float dpi_scale = 1.0f;
    bool running = true;
    uint64_t frame_count = 0;
    bool command_palette_open = false;
    std::string command_palette_input = "";
    std::string status_message = "";
    bool file_dialog_requested = false;
};

struct WorkbenchData {
    DatasetSummary dataset;
    std::string group_key_col_name = "region";
    std::string group_val_col_name = "revenue_usd";
    std::vector<GroupByResultRow> top_groups;
    PreFlightAuditResult audit;
    SchemaAnalysisReport schema;
    ProfileReport profile;
    UxResearchSynthesisReport ux_report;
    std::vector<std::pair<CitationCandidate, CitationAuditOutcome>> citations;
};

WorkbenchData buildWorkbenchData(const std::string& optional_csv_path = "examples/telemetry_sample.csv",
                                 const std::string& optional_sql_path = "examples/ecommerce_schema.sql",
                                 const std::string& optional_trace_path = "examples/api_traces.csv",
                                 const std::string& optional_ux_path = "examples/ux_research_records.csv");

bool loadFilePath(const std::string& path, WorkbenchData& wb, GuiState& state);

struct ImGuiListClipper {
    int items_count = 0;
    int display_start = 0;
    int display_end = 0;

    constexpr void Begin(int count, int max_visible_rows = 8) noexcept {
        items_count = count;
        display_start = 0;
        display_end = std::min(count, max_visible_rows);
    }
};

class FramebufferCanvas {
public:
    FramebufferCanvas(int width = 1440, int height = 900);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] const uint32_t* pixels() const noexcept { return pixels_.data(); }
    [[nodiscard]] uint32_t* pixelsMut() noexcept { return pixels_.data(); }

    void clear(Color32 c) noexcept;
    void blendPixel(int x, int y, Color32 c) noexcept;
    void fillRect(int x, int y, int w, int h, Color32 c) noexcept;
    void fillVerticalGradient(int x, int y, int w, int h, Color32 top, Color32 bottom) noexcept;

    void drawSoftShadow(int x, int y, int w, int h, int radius, int spread, uint8_t max_alpha = 90) noexcept;
    void fillRoundedRect(int x, int y, int w, int h, int radius, Color32 c) noexcept;
    void fillGradientRoundedRect(int x, int y, int w, int h, int radius, Color32 top, Color32 bottom) noexcept;
    void strokeRoundedRect(int x, int y, int w, int h, int radius, Color32 c, float thickness = 1.0f) noexcept;
    void drawFrostedGlassPanel(int x, int y, int w, int h, int radius, Color32 tint) noexcept;

    void drawSpriteARGB(int x, int y, int w, int h, const uint32_t* argb_data) noexcept;
    void drawVeloxBrandLockup(int x, int y) noexcept;
    void drawUiIcon(int x, int y, UiIconId icon, Color32 c) noexcept;

    void drawHLine(int x, int y, int len, Color32 c, bool dashed = false) noexcept;
    void drawVLine(int x, int y, int len, Color32 c, bool dashed = false) noexcept;
    void drawLineAA(float x0, float y0, float x1, float y1, Color32 c, float thickness = 1.6f) noexcept;
    void drawCircleAA(float cx, float cy, float radius, Color32 fill_col) noexcept;
    void drawArcRingAA(float cx, float cy, float radius, float thickness,
                       float start_deg, float end_deg, Color32 c) noexcept;
    void drawShadedCurveAA(const std::vector<Point2D>& pts,
                           int plot_x, int plot_y, int plot_w, int plot_h,
                           Color32 line_col, Color32 fill_top, Color32 fill_bottom,
                           bool draw_nodes = false) noexcept;

    int measureTextWidth(std::string_view text, FontFace face = FontFace::InterRegular12) const noexcept;
    void drawTextAA(int x, int y, std::string_view text, Color32 c,
                    FontFace face = FontFace::InterRegular12) noexcept;
    void drawTextRightAA(int right_x, int y, std::string_view text, Color32 c,
                         FontFace face = FontFace::InterRegular12) noexcept;
    void drawTextCenteredAA(int center_x, int y, std::string_view text, Color32 c,
                            FontFace face = FontFace::InterRegular12) noexcept;

    void drawText(int x, int y, std::string_view text, Color32 c, int scale = 1) noexcept;
    void drawTextRight(int right_x, int y, std::string_view text, Color32 c, int scale = 1) noexcept;

    bool saveBmp(const std::string& path) const;

private:
    int width_;
    int height_;
    std::vector<uint32_t> pixels_;
};

class NativeGuiApp {
public:
    static void renderFrame(FramebufferCanvas& canvas,
                            const WorkbenchData& data,
                            GuiState& state);

    static bool handleMouseClick(int mx, int my, GuiState& state);
    static void handleKeyPress(char key, GuiState& state);

    static int run(const WorkbenchData& data, bool force_tui = false);

    static bool exportSnapshotBmp(const WorkbenchData& data,
                                  const std::string& bmp_path,
                                  GuiTab tab = GuiTab::OverviewBento);
};

} // namespace velox
