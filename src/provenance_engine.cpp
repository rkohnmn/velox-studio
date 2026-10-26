#include "velox/provenance_engine.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>

namespace velox {

namespace {

std::string trimStr(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) sv.remove_suffix(1);
    const std::string_view prefixes[] = {
        "https://doi.org/", "http://doi.org/", "https://dx.doi.org/", "http://dx.doi.org/",
        "doi:", "DOI:", "arXiv:", "arxiv:", "https://ror.org/", "PMID:", "pmid:"
    };
    for (std::string_view pfx : prefixes) {
        if (sv.starts_with(pfx)) {
            sv.remove_prefix(pfx.size());
            break;
        }
    }
    return std::string(sv);
}

} // namespace

bool ProvenanceEngine::validateDoiSyntax(std::string_view doi) noexcept {
    static const std::regex re(R"(^10\.\d{4,9}/[-._;()/:A-Za-z0-9]+$)");
    std::string s = trimStr(doi);
    return std::regex_match(s, re);
}

bool ProvenanceEngine::validatePmidSyntax(std::string_view pmid) noexcept {
    static const std::regex re(R"(^\d{1,8}$)");
    std::string s = trimStr(pmid);
    return std::regex_match(s, re);
}

bool ProvenanceEngine::validatePmcidSyntax(std::string_view pmcid) noexcept {
    static const std::regex re(R"(^PMC\d+$)");
    std::string s = trimStr(pmcid);
    return std::regex_match(s, re);
}

bool ProvenanceEngine::validateArxivSyntax(std::string_view arxiv_id) noexcept {
    static const std::regex modern_re(R"(^\d{4}\.\d{4,5}(v\d+)?$)");
    static const std::regex legacy_re(R"(^[a-z\-]+(\.[A-Z]{2})?/\d{7}(v\d+)?$)");
    std::string s = trimStr(arxiv_id);
    return std::regex_match(s, modern_re) || std::regex_match(s, legacy_re);
}

bool ProvenanceEngine::validateIsbn10(std::string_view isbn) noexcept {
    std::string cleaned;
    for (char c : isbn) {
        if (c != '-' && c != ' ') cleaned.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    if (cleaned.size() != 10) return false;
    int total = 0;
    for (int i = 0; i < 10; ++i) {
        int val = 0;
        if (cleaned[i] == 'X' && i == 9) val = 10;
        else if (std::isdigit(static_cast<unsigned char>(cleaned[i]))) val = cleaned[i] - '0';
        else return false;
        total += (10 - i) * val;
    }
    return (total % 11) == 0;
}

bool ProvenanceEngine::validateIsbn13(std::string_view isbn) noexcept {
    std::string cleaned;
    for (char c : isbn) {
        if (c != '-' && c != ' ') cleaned.push_back(c);
    }
    if (cleaned.size() != 13) return false;
    int total = 0;
    for (int i = 0; i < 13; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(cleaned[i]))) return false;
        int weight = (i % 2 == 0) ? 1 : 3;
        total += weight * (cleaned[i] - '0');
    }
    return (total % 10) == 0;
}

bool ProvenanceEngine::validateIsbn(std::string_view isbn) noexcept {
    std::string cleaned;
    for (char c : isbn) {
        if (c != '-' && c != ' ') cleaned.push_back(c);
    }
    if (cleaned.size() == 10) return validateIsbn10(cleaned);
    if (cleaned.size() == 13) return validateIsbn13(cleaned);
    return false;
}

bool ProvenanceEngine::validateIssnSyntax(std::string_view issn) noexcept {
    static const std::regex re(R"(^\d{4}-\d{3}[\dX]$)");
    std::string s = trimStr(issn);
    if (!std::regex_match(s, re)) return false;
    // Mod-11 check digit verification
    std::string digits;
    for (char c : s) {
        if (c != '-') digits.push_back(c);
    }
    int sum = 0;
    for (int i = 0; i < 7; ++i) {
        sum += (8 - i) * (digits[i] - '0');
    }
    int check = (digits[7] == 'X') ? 10 : (digits[7] - '0');
    return ((sum + check) % 11) == 0;
}

bool ProvenanceEngine::validateRorSyntax(std::string_view ror) noexcept {
    static const std::regex re(R"(^0[0-9a-hjkmnp-tv-z]{6}[0-9]{2}$)");
    std::string s = trimStr(ror);
    return std::regex_match(s, re);
}

PidType ProvenanceEngine::classifyIdentifier(std::string_view raw) noexcept {
    std::string s = trimStr(raw);
    if (validateDoiSyntax(s)) return PidType::DOI;
    if (validatePmcidSyntax(s)) return PidType::PMCID;
    if (validateArxivSyntax(s)) return PidType::ARXIV;
    if (validateIssnSyntax(s)) return PidType::ISSN;
    if (validateRorSyntax(s)) return PidType::ROR;
    std::string cleaned;
    for (char c : s) {
        if (c != '-' && c != ' ') cleaned.push_back(c);
    }
    if ((cleaned.size() == 10 || cleaned.size() == 13) && validateIsbn(s)) {
        return PidType::ISBN;
    }
    if (validatePmidSyntax(s)) return PidType::PMID;
    return PidType::UNKNOWN;
}

std::string ProvenanceEngine::pidTypeToString(PidType t) {
    switch (t) {
        case PidType::DOI: return "DOI";
        case PidType::PMID: return "PMID";
        case PidType::PMCID: return "PMCID";
        case PidType::ARXIV: return "arXiv";
        case PidType::ISBN: return "ISBN";
        case PidType::ISSN: return "ISSN";
        case PidType::ROR: return "ROR";
        case PidType::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

CitationAuditOutcome ProvenanceEngine::auditCitation(const CitationCandidate& c) {
    CitationAuditOutcome out;
    out.pid_type = classifyIdentifier(c.raw_identifier);
    out.syntax_valid = (out.pid_type != PidType::UNKNOWN);
    out.fatal_retraction = c.is_retracted;

    double s_syntax = out.syntax_valid ? 1.0 : 0.0;
    double s_resolved = (out.syntax_valid && c.registry_resolved) ? 1.0 : 0.0;

    if (s_resolved > 0.0) {
        int matches = (c.metadata_title_match ? 1 : 0) +
                      (c.metadata_author_match ? 1 : 0) +
                      (c.metadata_year_match ? 1 : 0) +
                      (c.metadata_venue_match ? 1 : 0);
        out.metadata_parity_score = static_cast<double>(matches) / 4.0;
    } else {
        out.metadata_parity_score = 0.0;
    }

    double s_not_retracted = c.is_retracted ? 0.0 : 1.0;

    double s_entailment = 0.3;
    if (c.semantic_entailment == "entailment") s_entailment = 1.0;
    else if (c.semantic_entailment == "neutral") s_entailment = 0.4;
    else if (c.semantic_entailment == "contradiction") s_entailment = 0.0;

    double s_tier = 0.3;
    if (c.source_tier == "primary") s_tier = 1.0;
    else if (c.source_tier == "secondary") s_tier = 0.5;
    else if (c.source_tier == "tertiary") s_tier = 0.15;

    double s_archived = c.has_archival_snapshot ? 1.0 : 0.0;

    // Weights per citation-provenance-verification.md §6
    double weighted = 0.10 * s_syntax +
                      0.15 * s_resolved +
                      0.20 * out.metadata_parity_score +
                      0.25 * s_not_retracted +
                      0.20 * s_entailment +
                      0.05 * s_tier +
                      0.05 * s_archived;

    if (c.is_retracted) {
        weighted = std::min(weighted, 0.05);
    }
    out.trust_score = std::round(weighted * 10000.0) / 10000.0;

    // Classify failure mode (Types I-V)
    if (c.is_retracted) {
        out.failure_taxonomy = "Type IV (Zombie / Retracted Citation)";
        out.verdict = "FATAL - retracted source cited as active evidence. Block release.";
    } else if (!out.syntax_valid || !c.registry_resolved) {
        out.failure_taxonomy = "Type I (Pure Synthetic Hallucination)";
        out.verdict = "FAIL - identifier failed resolution; delete citation.";
    } else if (out.metadata_parity_score < 0.50) {
        out.failure_taxonomy = "Type II (Chimera Attribution)";
        out.verdict = "FAIL - real identifier recombined with mismatched title/author/venue.";
    } else if (c.semantic_entailment == "contradiction" || c.semantic_entailment == "neutral") {
        out.failure_taxonomy = "Type III (Semantic Misattribution / Paper-Dropping)";
        out.verdict = (c.semantic_entailment == "contradiction")
                          ? "FATAL - source excerpt contradicts the draft assertion."
                          : "CAUTION - neutral entailment; source does not establish specific claim.";
    } else if (c.source_tier == "tertiary") {
        out.failure_taxonomy = "Type V (Circular / Echo Attribution)";
        out.verdict = "CAUTION - tertiary source; trace to primary empirical study.";
    } else if (out.trust_score >= 0.85) {
        out.failure_taxonomy = "NONE";
        out.verdict = "PASS - citation is well-provenanced.";
    } else if (out.trust_score >= 0.60) {
        out.failure_taxonomy = "NONE";
        out.verdict = "CAUTION - resolve flagged components before release.";
    } else {
        out.failure_taxonomy = "Type II/III Composite";
        out.verdict = "FAIL - insufficient provenance to publish this citation.";
    }

    return out;
}

} // namespace velox
