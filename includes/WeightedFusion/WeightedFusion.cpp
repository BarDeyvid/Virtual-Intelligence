#include "WeightedFusion/WeightedFusion.hpp"
#include <iomanip>
#include <iostream>

namespace alyssa_fusion {

/**
 * @brief Calculates rule-based weights for expert contributions.
 *
 * Assigns initial minimum weights to all available experts and adjusts them
 * based on specific keywords found in the input. Used as the gating network
 * for Top-K expert selection before any inference runs.
 */
std::map<std::string, double> WeightedFusion::calculate_rule_based_weights(
    const std::string& input,
    const std::vector<std::string>& available_experts)
{
    std::map<std::string, double> weights;
    std::string lower_input = input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);

    // Initialize base weights
    for (const auto& expert : available_experts) {
        weights[expert] = 0.1;
    }

    // emotionalModel
    if (lower_input.find("feliz") != std::string::npos ||
        lower_input.find("triste") != std::string::npos ||
        lower_input.find("raiva") != std::string::npos ||
        lower_input.find("medo") != std::string::npos ||
        lower_input.find("amo") != std::string::npos ||
        lower_input.find("odeio") != std::string::npos) {
        weights["emotionalModel"] += 0.6;
    }

    // memoryModel
    if (lower_input.find("lembr") != std::string::npos ||
        lower_input.find("mem\u00f3ria") != std::string::npos ||
        lower_input.find("passado") != std::string::npos ||
        lower_input.find("antes") != std::string::npos) {
        weights["memoryModel"] += 0.5;
    }

    // introspectiveModel
    if (lower_input.find("porque") != std::string::npos ||
        lower_input.find("como funciona") != std::string::npos ||
        lower_input.find("pensar") != std::string::npos ||
        lower_input.find("analisar") != std::string::npos) {
        weights["introspectiveModel"] += 0.5;
    }

    // socialModel
    if (lower_input.find("oi") != std::string::npos ||
        lower_input.find("ol\u00e1") != std::string::npos ||
        lower_input.find("bom dia") != std::string::npos ||
        lower_input.find("tchau") != std::string::npos) {
        weights["socialModel"] += 0.4;
    }

    // Softmax normalisation
    double sum = 0.0;
    for (const auto& w : weights) sum += std::exp(w.second);
    for (auto& w : weights)      w.second = std::exp(w.second) / sum;

    return weights;
}

/**
 * @brief Cosine similarity between two float embeddings.
 */
double WeightedFusion::calculate_semantic_similarity(
    const std::vector<float>& emb1,
    const std::vector<float>& emb2)
{
    if (emb1.size() != emb2.size() || emb1.empty()) return 0.0;

    double dot_product = 0.0, norm1 = 0.0, norm2 = 0.0;
    for (size_t i = 0; i < emb1.size(); ++i) {
        dot_product += emb1[i] * emb2[i];
        norm1 += emb1[i] * emb1[i];
        norm2 += emb2[i] * emb2[i];
    }

    if (norm1 == 0.0 || norm2 == 0.0) return 0.0;
    return dot_product / (std::sqrt(norm1) * std::sqrt(norm2));
}

/**
 * @brief Heuristic emotion detection from input keywords.
 */
std::string WeightedFusion::detect_emotion_from_input(const std::string& input)
{
    std::string lower_input = input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);

    const std::map<std::string, std::vector<std::string>> emotion_keywords = {
        {"happy",      {"feliz", "alegre", "amo", "adorei", "legal"}},
        {"sad",        {"triste", "infeliz", "deprimido", "choro", "sofro"}},
        {"analytical", {"porque", "como", "explique", "analise", "entenda"}}
    };

    std::map<std::string, int> scores;
    for (const auto& [emotion, keywords] : emotion_keywords) {
        for (const auto& kw : keywords) {
            if (lower_input.find(kw) != std::string::npos) {
                scores[emotion]++;
            }
        }
    }

    if (scores.empty()) return "neutral";

    return std::max_element(scores.begin(), scores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; })->first;
}

/**
 * @brief Keyword extraction placeholder.
 */
std::string WeightedFusion::extract_keywords(const std::string& text)
{
    return text;
}

} // namespace alyssa_fusion
