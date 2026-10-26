#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace velox {

/**
 * Normalized User Research Record (Workflow 1 Step 1)
 */
struct ResearchParticipantRecord {
    std::string participant_id;
    std::string usage_frequency;     // "daily" | "weekly" | "occasional"
    std::string primary_task;
    std::string platform;            // "Linux X11" | "Windows Desktop" | "macOS"
    int technical_proficiency = 5;   // 1..10
    std::string pain_point;
    std::string evidence_tag;        // [GOAL], [PAIN], [BEHAVIOR], [CONTEXT], [QUOTE], [ERROR], [ACCESSIBILITY], [PERFORMANCE], [HYPOTHESIS]
    std::string verbatim_quote;
    bool requires_keyboard_only = false;
    bool prefers_reduced_motion = false;
};

/**
 * Behavioral Archetype / Persona (Workflow 1 Step 2 & Step 3)
 */
struct UserPersona {
    std::string archetype_label;     // "Power User", "Professional Operator", "Technical Specialist", "Occasional User"
    std::size_t participant_count = 0;
    std::size_t total_sample_size = 0;
    std::string primary_goals;
    std::string key_frustrations;
    std::string native_cpp_implication;
    std::string confidence_level;    // "Directional" | "Well-supported" | "Exploratory"
};

/**
 * Native Desktop Workflow Stage & Prioritization (Workflow 2 Step 3 & Step 4)
 * Priority = Frequency * Severity * UserImpact * Confidence (1..5 scale each, max 625)
 */
struct JourneyStageOpportunity {
    std::string stage_name;          // "Launch", "Open/Import", "Inspect", "Modify", "Validate", "Save/Export"
    std::string pain_point;
    int frequency = 1;               // 1..5
    int severity = 1;                // 1..5
    int user_impact = 1;             // 1..5
    int confidence = 1;              // 1..5
    int heuristic_priority = 1;      // F * S * I * C
    int implementation_cost = 1;     // Kept separate per Workflow 2 Step 4 (1..5)
    std::string native_design_fix;
};

/**
 * Section 10.4 Heuristic AI-Style & Visual/Written Quality Audit Result
 */
struct QualityAuditBreakdown {
    double written_warning_score = 0.0; // 0..100 (lower = fewer warning signs)
    double visual_warning_score = 0.0;  // 0..100 (lower = fewer warning signs)
    std::string written_band;
    std::string visual_band;
};

struct UxResearchSynthesisReport {
    std::size_t total_participants = 0;
    std::size_t keyboard_only_count = 0;
    std::size_t reduced_motion_count = 0;
    std::vector<UserPersona> personas;
    std::vector<JourneyStageOpportunity> prioritized_journey;
    QualityAuditBreakdown quality_audit;
};

class UxResearchEngine {
public:
    /**
     * Workflow 2 Step 4: Priority = Frequency * Severity * UserImpact * Confidence
     */
    static constexpr int computeOpportunityPriority(int frequency,
                                                    int severity,
                                                    int user_impact,
                                                    int confidence) noexcept {
        return frequency * severity * user_impact * confidence;
    }

    /**
     * Workflow 10.4: Weighted 0-100 Heuristic Warning Score
     * Written weights: 25% lexical, 25% variation, 25% factuality, 25% structure (each 0..10 warning level)
     * Visual weights:  35% geometry, 30% fine-detail, 35% text/semantic accuracy (each 0..10 warning level)
     */
    static QualityAuditBreakdown computeQualityAuditScore(double lexical_warn_0_10,
                                                          double variation_warn_0_10,
                                                          double factuality_warn_0_10,
                                                          double structure_warn_0_10,
                                                          double geometry_warn_0_10,
                                                          double detail_warn_0_10,
                                                          double semantic_text_warn_0_10);

    static std::string interpretWarningBand(double score_0_100);

    static UxResearchSynthesisReport analyzeRecordsCsv(std::string_view csv_content);
};

} // namespace velox
