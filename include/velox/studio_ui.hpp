#pragma once

#include "velox/csv_engine.hpp"
#include "velox/native_gui.hpp"
#include "velox/profiler_engine.hpp"
#include "velox/provenance_engine.hpp"
#include "velox/schema_engine.hpp"
#include "velox/viz_engine.hpp"

#include <string>

namespace velox {

class StudioUi {
public:
    /**
     * Renders a high-density Braun T1000 / Linear-inspired 24-bit TrueColor
     * ANSI/Unicode Bento Grid Terminal UI directly to stdout or string buffer.
     * Zero HTML, zero browser dependencies.
     */
    static std::string renderTerminalBento(const DatasetSummary& ds,
                                           const PreFlightAuditResult& audit,
                                           const SchemaAnalysisReport& schema,
                                           const ProfileReport& prof,
                                           GuiTab active_tab = GuiTab::OverviewBento);
};

} // namespace velox
