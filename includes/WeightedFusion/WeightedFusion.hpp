// WeightedFusion.hpp
#pragma once
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <cmath>
#include "Embedding/Embedder.hpp"

namespace alyssa_fusion {

struct ExpertContribution {
    std::string expert_id;
    double weight;
    std::string response;
    std::vector<float> embedding;
};

class WeightedFusion {
private:
    Embedder& embedder;
    
    // Configurações de ponderação
    double emotion_weight_base = 0.3;
    double context_similarity_weight = 0.4;
    double historical_relevance_weight = 0.3;
    
    // Cache de similaridades
    std::map<std::string, double> expert_affinities;

public:
    WeightedFusion(Embedder& embedder_ref) : embedder(embedder_ref) {}
    
    // 🔹 A. Rule-based Fusion (heurístico simples)
    std::map<std::string, double> calculate_rule_based_weights(
        const std::string& input, 
        const std::vector<std::string>& available_experts);
    
    // 🔹 B. Feature-based Fusion (similaridade vetorial)
    std::map<std::string, double> calculate_feature_based_weights(
        const std::string& input,
        const std::vector<ExpertContribution>& expert_responses);
    
    // 🔹 C. Neural Fusion (rede neural leve)
    std::map<std::string, double> calculate_neural_weights(
        const std::string& input,
        const std::vector<ExpertContribution>& expert_responses,
        const std::string& current_emotion);
    
    // Método principal que orquestra a fusão
    std::string fuse_responses(
        const std::string& input,
        const std::vector<ExpertContribution>& contributions,
        const std::string& current_emotion = "");
    
    // Utilitários
    double calculate_semantic_similarity(const std::vector<float>& emb1, const std::vector<float>& emb2);
    std::string detect_emotion_from_input(const std::string& input);
    std::string extract_keywords(const std::string& text);
};

} // namespace alyssa_fusion