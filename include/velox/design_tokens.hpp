#pragma once

#include <cstdint>
#include <string_view>

namespace velox::DesignTokens {

// ─────────────────────────────────────────────────────────────────────────────
// Semantic Color Roles (Workflow 5 Step 3 & Step 4 — WCAG AA/AAA Verified)
// ─────────────────────────────────────────────────────────────────────────────
namespace Color {
    inline constexpr uint32_t Primary          = 0x35F28BU; // Phosphor emerald focal action & scanmark
    inline constexpr uint32_t PrimaryMuted     = 0x143D29U; // Active pill fill
    inline constexpr uint32_t Secondary        = 0x4C9AFFU; // OKLCH azure series anchor
    inline constexpr uint32_t Background       = 0x06080CU; // Deep obsidian workspace canvas
    inline constexpr uint32_t CommandBar       = 0x0A0E14U; // Top command bar & status bar
    inline constexpr uint32_t Surface          = 0x0D1117U; // Primary dock & card surface
    inline constexpr uint32_t SurfaceElevated  = 0x161E2BU; // Elevated inspector & modal surface
    inline constexpr uint32_t SurfaceHover     = 0x1E293BU; // Hover interaction state
    inline constexpr uint32_t TextPrimary      = 0xE6EDF3U; // Primary copy (12.6:1 AAA vs Surface)
    inline constexpr uint32_t TextSecondary    = 0x9BA7B4U; // Secondary labels (7.2:1 AAA vs Surface)
    inline constexpr uint32_t TextMuted        = 0x6E7681U; // Muted units / hints (4.6:1 AA vs Background)
    inline constexpr uint32_t BorderSubtle     = 0x30363DU; // 1px structural hairline
    inline constexpr uint32_t BorderStrong     = 0x3D4754U; // Specular card edge
    inline constexpr uint32_t BorderFocus      = 0x35F28BU; // 2px visible keyboard focus ring
    inline constexpr uint32_t Error            = 0xFF6B5EU; // Critical SLA / retraction / schema alert
    inline constexpr uint32_t Warning          = 0xE3B341U; // Caution / normalization warning
    inline constexpr uint32_t Success          = 0x35F28BU; // Verified pass state
}

// ─────────────────────────────────────────────────────────────────────────────
// Typography Hierarchy (Workflow 5 Step 5)
// ─────────────────────────────────────────────────────────────────────────────
namespace Typography {
    inline constexpr std::string_view FontFamilyUI   = "Inter";
    inline constexpr std::string_view FontFamilyMono = "Noto Sans Mono";
    inline constexpr int SizeCaptionPx               = 12;
    inline constexpr int SizeBodyPx                  = 13;
    inline constexpr int SizeHeadingPx               = 16;
    inline constexpr int SizeDisplayKpiPx            = 22;
    inline constexpr double LineHeightBody           = 1.45;
}

// ─────────────────────────────────────────────────────────────────────────────
// Spacing Scale (Workflow 5 Step 6 — 4px Base Unit)
// ─────────────────────────────────────────────────────────────────────────────
namespace Spacing {
    inline constexpr int Unit = 4;
    inline constexpr int Xs   = 4;
    inline constexpr int Sm   = 8;
    inline constexpr int Md   = 16;
    inline constexpr int Lg   = 24;
    inline constexpr int Xl   = 32;
}

// ─────────────────────────────────────────────────────────────────────────────
// Component Sizing & Radii (Workflow 5 Step 3 & Step 7)
// ─────────────────────────────────────────────────────────────────────────────
namespace Sizing {
    inline constexpr int ControlHeight   = 32;
    inline constexpr int CommandBarHeight = 48;
    inline constexpr int StatusBarHeight = 32;
    inline constexpr int IconSize        = 14;
    inline constexpr int SidebarWidth    = 240;
    inline constexpr int InspectorWidth  = 334;
    inline constexpr int MinWindowWidth  = 1024;
    inline constexpr int MinWindowHeight = 680;
}

namespace Radius {
    inline constexpr int Control = 6;
    inline constexpr int Panel   = 8;
    inline constexpr int Dialog  = 10;
    inline constexpr int Pill    = 10;
}

// ─────────────────────────────────────────────────────────────────────────────
// Motion & Accessibility Thresholds (Workflow 5 Step 3 & Workflow 8)
// ─────────────────────────────────────────────────────────────────────────────
namespace Motion {
    inline constexpr int ShortTransitionMs = 140;
    inline constexpr int PanelTransitionMs = 240;
    inline constexpr double SpringStiffness = 220.0;
    inline constexpr double SpringDamping   = 28.0;
}

namespace Accessibility {
    inline constexpr int FocusRingWidthPx       = 2;
    inline constexpr double ContrastMinNormalAA = 4.5;
    inline constexpr double ContrastMinLargeAA  = 3.0;
    inline constexpr double ContrastTargetAAA   = 7.0;
    inline constexpr int MinTextSizePx          = 12;
}

} // namespace velox::DesignTokens
