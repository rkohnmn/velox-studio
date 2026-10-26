#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace velox {

enum class PidType {
    DOI,
    PMID,
    PMCID,
    ARXIV,
    ISBN,
    ISSN,
    ROR,
    UNKNOWN
};

struct CitationCandidate {
    std::string raw_identifier;
    std::string claimed_title;
    std::string claimed_author;
    int claimed_year = 0;
    std::string claimed_venue;
    std::string asserted_claim;
    std::string source_excerpt;
    std::string source_tier = "primary";          // "primary" | "secondary" | "tertiary"
    std::string semantic_entailment = "entailment"; // "entailment" | "neutral" | "contradiction"
    bool registry_resolved = true;
    bool metadata_title_match = true;
    bool metadata_author_match = true;
    bool metadata_year_match = true;
    bool metadata_venue_match = true;
    bool is_retracted = false;
    bool has_archival_snapshot = true;
};

struct CitationAuditOutcome {
    PidType pid_type = PidType::UNKNOWN;
    bool syntax_valid = false;
    double metadata_parity_score = 0.0;
    double trust_score = 0.0;
    bool fatal_retraction = false;
    std::string failure_taxonomy = "NONE"; // NONE | Type I | Type II | Type III | Type IV | Type V
    std::string verdict;
};

class ProvenanceEngine {
public:
    static PidType classifyIdentifier(std::string_view raw) noexcept;
    static std::string pidTypeToString(PidType t);

    static bool validateDoiSyntax(std::string_view doi) noexcept;
    static bool validatePmidSyntax(std::string_view pmid) noexcept;
    static bool validatePmcidSyntax(std::string_view pmcid) noexcept;
    static bool validateArxivSyntax(std::string_view arxiv_id) noexcept;
    static bool validateIsbn10(std::string_view isbn) noexcept;
    static bool validateIsbn13(std::string_view isbn) noexcept;
    static bool validateIsbn(std::string_view isbn) noexcept;
    static bool validateIssnSyntax(std::string_view issn) noexcept;
    static bool validateRorSyntax(std::string_view ror) noexcept;

    static CitationAuditOutcome auditCitation(const CitationCandidate& c);
};

} // namespace velox
