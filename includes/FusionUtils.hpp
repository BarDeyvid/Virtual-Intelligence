#pragma once
// FusionUtils.hpp
// Pure, model-free utility functions extracted from CoreIntegration.
// These have no dependencies on llama.cpp, ONNX, or any heavy library,
// making them fast to unit-test without loading any model.

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <set>
#include <map>
#include "WeightedFusion/WeightedFusion.hpp"

namespace alyssa_utils {

// =============================================================================
// is_small_talk
// =============================================================================

/**
 * @brief Returns true if the input is a short social pleasantry with no
 *        substantive content (greetings, farewells, etc.).
 */
inline bool is_small_talk(const std::string& input) {
    static const std::vector<std::string> patterns = {
        "oi", "olá", "e aí", "eai", "tudo bem", "como vai",
        "bom dia", "boa tarde", "boa noite", "oi, tudo bem?",
        "olá, como você está?", "hey", "hello", "hi"
    };

    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    lower.erase(std::remove_if(lower.begin(), lower.end(),
                               [](char c) { return std::ispunct(c); }),
                lower.end());

    for (const auto& p : patterns) {
        if (lower.find(p) != std::string::npos) {
            if (lower.length() <= 15 || lower == p) return true;
        }
    }

    // 'explique' is the imperative conjugation of 'explicar', same root as 'explica'
    static const std::vector<std::string> content_indicators = {
        "?", "porque", "como", "quando", "onde", "por que",
        "expl", "ajuda", "preciso", "problema", "questão"
    };
    for (const auto& ind : content_indicators) {
        if (lower.find(ind) != std::string::npos) return false;
    }
    return lower.length() < 30;
}

// =============================================================================
// calculate_string_similarity  (Jaccard on word tokens)
// =============================================================================

/**
 * @brief Returns a Jaccard similarity score in [0, 1] between two strings.
 */
inline float calculate_string_similarity(const std::string& s1, const std::string& s2) {
    if (s1.empty() && s2.empty()) return 1.0f;
    if (s1.empty() || s2.empty()) return 0.0f;

    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };
    auto tokenize = [](const std::string& text) {
        std::set<std::string> tokens;
        std::string tok;
        for (char c : text) {
            if (std::isalnum(c) || c == '\'') tok += c;
            else if (!tok.empty()) { tokens.insert(tok); tok.clear(); }
        }
        if (!tok.empty()) tokens.insert(tok);
        return tokens;
    };

    auto w1 = tokenize(lower(s1));
    auto w2 = tokenize(lower(s2));
    if (w1.empty() && w2.empty()) return 1.0f;
    if (w1.empty() || w2.empty()) return 0.0f;

    int inter = 0;
    for (const auto& w : w1) if (w2.count(w)) ++inter;
    int uni = (int)(w1.size() + w2.size()) - inter;
    return uni > 0 ? (float)inter / uni : 0.0f;
}

// =============================================================================
// calculate_history_limit
// =============================================================================

/**
 * @brief Returns the maximum number of messages to keep in an expert's history.
 *        Does not require a live memory manager — uses a fixed emotional intensity.
 *
 * @param expert_id        Expert identifier string.
 * @param emotion_intensity Normalised intensity in [0, 1] (default 0.5 = neutral).
 */
inline size_t calculate_history_limit(const std::string& expert_id,
                                      float emotion_intensity = 0.5f) {
    size_t limit = 150;

    if (emotion_intensity > 0.7f)      limit += 10;
    else if (emotion_intensity < 0.2f) limit -= 5;

    if (expert_id == "introspectiveModel")      limit += 15;
    else if (expert_id == "socialModel")        limit += 5;
    else if (expert_id == "creativeModel")      limit += 10;

    if (limit > 50) limit = 50;
    if (limit < 10) limit = 10;
    return limit;
}

// =============================================================================
// calculate_committee_coherence
// =============================================================================

/**
 * @brief Returns a coherence score in [0, 1] for a set of expert contributions.
 *        Score = fraction of unique pairs whose responses are signal-compatible
 *        (i.e. neither contains "[ERRO]").
 */
inline float calculate_committee_coherence(
    const std::vector<alyssa_fusion::ExpertContribution>& contributions)
{
    if (contributions.size() <= 1) return 1.0f;

    auto compatible = [](const std::string& a, const std::string& b) {
        // Signals are incompatible only if both carry an explicit error tag
        return !(a.find("[ERRO]") != std::string::npos &&
                 b.find("[ERRO]") != std::string::npos);
    };

    int agree = 0, total = 0;
    for (size_t i = 0; i < contributions.size(); ++i) {
        for (size_t j = i + 1; j < contributions.size(); ++j) {
            if (compatible(contributions[i].response, contributions[j].response)) ++agree;
            ++total;
        }
    }
    return total > 0 ? (float)agree / total : 0.0f;
}

// =============================================================================
// apply_topk_gating  (pure version of the gating logic in think_with_fusion_core)
// =============================================================================

/**
 * @brief Applies Top-K threshold filtering to a weight map.
 *
 * Returns the set of expert IDs that pass (weight >= threshold AND rank < top_k).
 * Zeros out the weight for every expert that doesn't pass.
 *
 * @param weights   In/out weight map (expert_id -> weight).
 * @param top_k     Maximum number of experts to activate.
 * @param threshold Minimum weight an expert must have to be activated.
 * @return Set of activated expert IDs.
 */
inline std::set<std::string> apply_topk_gating(
    std::map<std::string, double>& weights,
    int top_k     = 3,
    double threshold = 0.15)
{
    std::vector<std::pair<std::string, double>> sorted(weights.begin(), weights.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::set<std::string> active;
    for (int i = 0; i < static_cast<int>(sorted.size()); ++i) {
        if (i < top_k && sorted[i].second >= threshold) {
            active.insert(sorted[i].first);
        } else {
            weights[sorted[i].first] = 0.0;
        }
    }
    return active;
}

} // namespace alyssa_utils
