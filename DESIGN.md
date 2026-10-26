---
color.primary: "#35F28B"
color.secondary: "#4C9AFF"
color.surface: "#0D1117"
color.surfaceElevated: "#161B22"
color.background: "#000000"
color.text: "#E6EDF3"
color.textMuted: "#8B949E"
color.border: "#30363D"
color.error: "#FF6B5E"
color.warning: "#E3B341"
color.success: "{color.primary}"
type.headline.fontFamily: "Inter"
type.headline.fontSize: 28px
type.headline.fontWeight: 700
type.headline.lineHeight: 1.15
type.headline.letterSpacing: "-0.025em"
type.body.fontFamily: "Inter"
type.body.fontSize: 15px
type.body.fontWeight: 400
type.body.lineHeight: 1.55
type.mono.fontFamily: "JetBrains Mono"
type.mono.fontSize: 13px
type.mono.fontWeight: 500
type.mono.lineHeight: 1.45
type.mono.fontFeature: "tnum 1, lnum 1"
spacing.unit: 8px
radius.sm: 4px
radius.md: 8px
radius.lg: 12px
shadow.sm: "0 1px 2px rgba(0,0,0,0.45)"
shadow.md: "0 8px 24px rgba(0,0,0,0.65)"
---

## Overview

![Velox Studio Identity](assets/branding/velox-studio-logo-dark.svg)

**Tagline:** *Made for the fast path. A visual system for native data, profiling, and precise measurement.*

The **Velox Studio Scanmark** (`assets/branding/velox-studio-mark.svg`, `velox-studio-icon.svg`) consists of four parallel `#E6EDF3` columnar data lanes intersected by a diagonal zero-copy fast-path aperture, terminating in a single `#35F28B` phosphor-emerald sample square on a `#0D1117` charcoal tile bordered by `#30363D`.

Velox Studio is modeled on the tactile precision of a **1980s Braun T1000 shortwave telemetry receiver designed by Dieter Rams**, cross-pollinated with the high-density dark engineering surfaces of **Linear** and **Tracy Profiler**. Every visual element serves rapid quantitative scanning at 3 AM without retinal halation: deep carbon-black backgrounds (`#000000` and `#0D1117`), crisp 1px structural hairlines (`#30363D`), and high-salience phosphor-emerald (`#35F28B`) reserved strictly for primary interactive affordances and Rank-1/Rank-3 perceptual data marks.

## Colors

- **`color.background` (`#000000`)**: Pure OLED black structural canvas for the outer viewport and hero negative space.
- **`color.surface` (`#0D1117` / `oklch(16% 0.01 260)`)**: Primary Bento card surface, deliberately slightly lifted from pure black to prevent contrast halation against high-luminance typography while maintaining a **13.4:1 WCAG AAA** contrast ratio with `color.text`.
- **`color.surfaceElevated` (`#161B22` / `oklch(21% 0.01 260)`)**: Nested KPI readout modules, interactive hover states, and command tooltips.
- **`color.primary` (`#35F28B` / `oklch(84% 0.21 152)`)**: Phosphor-emerald focal accent used for the primary CTA, active focus rings, and the primary data series in single-metric charts. Roughly 85% of the interface remains neutral grayscale so the 15% carrying `color.primary` achieves immediate preattentive pop-out (within 200ms).
- **`color.secondary` (`#4C9AFF` / `oklch(68% 0.15 255)`)**: Secondary series anchor in OKLCH categorical palettes, engineered for deuteranopia and protanopia safety alongside `color.primary`.
- **`color.text` (`#E6EDF3`) & `color.textMuted` (`#8B949E`)**: Primary copy and secondary axis/footnote annotations; both clear WCAG 2.1 AA/AAA thresholds (`>= 4.5:1` for labels, `>= 7.0:1` for fine print).
- **`color.error` (`#FF6B5E`) & `color.warning` (`#E3B341`)**: Semantic alert channels reserved exclusively for P99 SLA breaches, N+1 query alerts, Lie Factor violations (`LF < 0.95` or `LF > 1.05`), and Type IV retracted citation halts.

## Typography

- **Headline & UI Hierarchy (`type.headline`, `type.body`)**: `Inter` (with system geometric sans fallback) provides low-contrast apertures optimized for rapid horizontal and vertical scanning. Chart titles are always written as **takeaway assertions** ("APAC Surpasses EMEA by 24%"), never passive field labels ("Revenue by Region").
- **Numeric & Telemetry Hierarchy (`type.mono`)**: `JetBrains Mono` with mandatory `font-variant-numeric: tabular-nums lining-nums` (`"tnum" 1, "lnum" 1`) across all table columns, P50/P95/P99 latency readouts, SHA-256 digests, and chart axis ticks so decimal points and digits align on an immutable vertical grid.

## Layout

- **Base Spacing Unit**: `8px` (`{spacing.unit}`), scaling strictly in multiples (`8px`, `16px`, `24px`, `32px`, `48px`, `64px`).
- **Modular Asymmetric Bento Grid**: Analytical workbenches use a 12-column CSS Grid (`max-width: 1320px`, `gap: 16px`) that pairs 4-column KPI summary cards (`col-4`) with 8-column perceptual SVG graphics (`col-8`) and 6-column diagnostic tables (`col-6`). Responsive breakpoints collapse cleanly at `1024px`, `768px`, and `390px` with zero horizontal overflow.

## Elevation & Depth

- **Z-Index Scale**: `0` (base canvas and Bento cards), `10` (floating chart tooltips, offset 10px from cursor), `20` (sticky frosted-glass navigation header with `backdrop-filter: blur(10px)`), `30` (modal dialogs and command palette).
- **Border-First Depth**: On dark telemetry surfaces, crisp `1px solid {color.border}` hairlines define spatial boundaries far more cleanly than heavy drop shadows; `{shadow.sm}` and `{shadow.md}` are reserved for floating overlays.

## Shapes

- **Scaled Corner Radii**: Radius scales proportionally with component footprint: `{radius.sm}` (`4px`) for inline badges, buttons, and histogram bars; `{radius.md}` (`8px`) for Bento cards and SVG chart viewports; `{radius.lg}` (`12px`) for top-level modal containers.

## Components

### Buttons
- **Primary Button**: Background `{color.primary}`, text `#000000` (weight `600`), radius `{radius.sm}`, padding `10px 20px`. Hover translates `-1px` on Y via compositor-friendly `transform` over `140ms ease-out`. Focus displays a `2px solid {color.primary}` ring with `3px` offset.
- **Secondary Button**: Background `{color.surface}`, text `{color.text}`, border `1px solid {color.border}`, radius `{radius.sm}`.

### Cards
- **Bento Telemetry Card**: Background `{color.surface}`, border `1px solid {color.border}`, radius `{radius.md}`, padding `20px`. Includes a top header row with left-aligned title (`15px`, weight `600`) and right-aligned monospace status badge.

### Inputs & Tables
- **Data Tables**: Full-width collapse, header cells in uppercase `11.5px` `{color.textMuted}`, text columns left-aligned, quantitative columns right-aligned with `.tabular-nums`.

### Navigation
- **Sticky Topbar**: Height `52px`, horizontal flex layout, live status indicator dot (`10px` circle in `{color.primary}`), and keyboard-accessible section jump links.

## Do's and Don'ts

- **Do**: Enforce a true zero baseline on every length-encoded bar or column chart, and switch to a dot plot or indexed-to-100 line chart whenever a non-zero range is analytically required.
- **Do**: Respect `prefers-reduced-motion: reduce` by collapsing all transition durations to `0.01ms`.
- **Don't**: Introduce blue-to-purple SaaS gradients, glowing decorative orbs, 3D beveled bars, or detached legends when series count `N <= 6`.
- **Don't**: Use proportional numerals in any table cell, latency counter, or chart axis tick.
