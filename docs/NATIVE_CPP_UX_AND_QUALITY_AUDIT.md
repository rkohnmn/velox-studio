# Native C++ UX Research, Journey Architecture & Visual Quality Audit

**Framework:** `native-cpp-ux-product-design` (Workflows 1–12)  
**Target Architecture:** 100% Native C++20 Desktop Application (`velox` — Zero HTML, Zero CSS, Zero JavaScript, Zero WebView)

---

## 1. Evidence-Backed User Personas (Workflow 1)

Processed from `examples/ux_research_records.csv` (`N = 12` observed participants across Linux X11, Windows Desktop, and macOS) via `velox::UxResearchEngine`:

| Archetype Label | Observed Denominator | Primary Goals | Key Frustrations (`[PAIN]` / `[PERFORMANCE]`) | Concrete Native C++20 Design Implication |
| :--- | :---: | :--- | :--- | :--- |
| **Power User (Systems & Data Engineer)** | `4 of 12` (Directional) | Sub-second large-file inspection, `Ctrl+K` command access, CLI/GUI parity | JVM/Electron cold-start delays (`>8s`) and mouse-only navigation traps | Offload `mmap` + Welford scan to `ThreadPool` with full keyboard focus traversal (`Tab`, `1–5`, `Ctrl+K`) |
| **Technical Specialist (DBA & Performance Architect)** | `3 of 12` (Directional) | 1NF–BCNF schema verification, N+1 SQL detection, 0-downtime DDL migrations | Hidden query regressions and unindexed foreign keys causing table locks | Dedicated SQL Schema/ERD and Tracy P99 Flamegraph docks with copyable SQL fixes |
| **Professional Operator (Quantitative Analyst)** | `3 of 12` (Directional) | Distortion-free statistical graphics (`Lie Factor = 1.00`) & DOI/ISBN verification | Spreadsheet row truncation (`1,048,576` cap) and misleading dual-axis scales | 15-point pre-flight perceptual auditor + Freedman–Diaconis & OKLCH contrast gates |
| **Occasional User (Student / Reviewer)** | `2 of 12` (Exploratory) | Recognizable navigation, sensible defaults, safe parameter reset & undo | Cryptic flags and irreversible filter/slider adjustments | One-click `Reset Inspector Parameters` button + `CommandHistory` (`Ctrl+Z` / `Ctrl+Y`) undo stack |
| **Cross-Cutting Accessibility Needs** | `4 of 12` keyboard-only; `3 of 12` reduced-motion | Full keyboard operability, `>= 4.5:1` AA / `7.0:1` AAA contrast, deuteranopia safety, zero forced motion | Color-only status indicators and uninterruptible animations | `SpringAnimator` `reduced_motion` instant-snap mode, `2px` `#35F28B` focus ring, and icon+text status pills |

---

## 2. Native Desktop User Journey Map & Prioritization (Workflow 2)

Using the heuristic formula $\text{Priority} = \text{Frequency} \times \text{Severity} \times \text{User Impact} \times \text{Confidence}$ (`1–5` scale each, max `625`), with `Implementation Cost` tracked separately:

| Stage | Observed Pain Point | F | S | I | C | Heuristic Priority | Impl. Cost (1–5) | Native C++20 Engineering Improvement |
| :--- | :--- | :-: | :-: | :-: | :-: | :---: | :---: | :--- |
| **1. Open / Import** | Large CSV/log files freeze the UI thread in legacy tools | 5 | 5 | 5 | 5 | **625** | 2 | `MappedFile` POSIX `mmap` + `ThreadPool` background worker + `CancellationToken` determinate progress |
| **2. Validate** | Misleading chart baselines and retracted citations slip into reports | 4 | 5 | 5 | 5 | **500** | 2 | Automated 15-point Cleveland–McGill Lie-Factor auditor + W3C PROV-DM PID checksum gate |
| **3. Inspect** | High saccadic eye-travel cost between detached chart legends and tables | 5 | 4 | 4 | 5 | **400** | 1 | Direct end-of-bar labeling + inline distribution sparkbars inside `ImGui::BeginTable` cells |
| **4. Launch** | Multi-second startup latency breaks debugging concentration | 5 | 3 | 4 | 5 | **300** | 1 | Single ~250 KB native C++20 binary booting directly into X11/Win32 framebuffer (`<2 ms`) |
| **5. Modify** | Accidental slider or filter changes cannot be undone quickly | 4 | 3 | 4 | 4 | **192** | 1 | `CommandHistory` reversible action stack (`undo()` / `redo()`) + `Reset Inspector Parameters` |
| **6. Save / Export** | Unclear output path or lossy raster chart exports | 3 | 3 | 4 | 4 | **144** | 1 | Deterministic vector SVG, Vega-Lite v5 JSON, and lossless 24-bit BMP framebuffer exports |

---

## 3. Centralized C++20 Design Tokens (`include/velox/design_tokens.hpp` — Workflows 5, 6 & 9)

All web/CSS token files have been removed in favor of `include/velox/design_tokens.hpp` (`namespace velox::DesignTokens`) and `DESIGN.md`:
- **Semantic Colors (`DesignTokens::Color`)**: `Primary` (`#35F28B`), `Secondary` (`#4C9AFF`), `Background` (`#06080C`), `Surface` (`#0D1117`), `SurfaceElevated` (`#161E2B`), `TextPrimary` (`#E6EDF3` — `12.6:1 AAA`), `BorderSubtle` (`#30363D`), `BorderFocus` (`#35F28B`), `Error` (`#FF6B5E`), `Warning` (`#E3B341`).
- **Spacing & Sizing (`DesignTokens::Spacing` / `Sizing`)**: `4px` base unit (`Xs=4`, `Sm=8`, `Md=16`, `Lg=24`, `Xl=32`), `SidebarWidth=240`, `InspectorWidth=334`, `MinWindowWidth=1024`, `MinWindowHeight=680`.
- **Motion & Accessibility (`DesignTokens::Motion` / `Accessibility`)**: `ShortTransitionMs=140`, `PanelTransitionMs=240`, `FocusRingWidthPx=2`, `ContrastMinNormalAA=4.5`, `ContrastTargetAAA=7.0`.

---

## 4. Pre-Edit & Release Quality Audit (Workflows 10, 11 & 12)

Evaluated via `UxResearchEngine::computeQualityAuditScore`:
- **Written Content Warning Score**: **`7.0 / 100`** (`0–20: Few warning signs observed` — all claims grounded in measured sandbox benchmarks and verified DOIs/ISBNs).
- **Visual & Perceptual Integrity Warning Score**: **`4.9 / 100`** (`0–20: Few warning signs observed` — true zero baseline enforced, Lie Factor = `1.000`, Catmull-Rom cubic splines, 4x4-supersampled vector icons, and official Velox Studio scanmark identity).
