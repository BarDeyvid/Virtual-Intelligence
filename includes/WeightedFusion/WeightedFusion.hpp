#pragma once
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <cmath>
#include "Embedding/Embedder.hpp"
#include <onnxruntime/onnxruntime_cxx_api.h>

namespace alyssa_fusion {

struct ExpertContribution {
    std::string expert_id;
    double weight;
    std::string response;
    std::vector<float> embedding;
    std::string source;
};

class WeightedFusion {
private:
    Embedder& embedder;
    
    // ONNX Runtime Members
    Ort::Env env;
    Ort::Session session;
    
    // Configurações de ponderação
    double emotion_weight_base = 0.3;
    double context_similarity_weight = 0.4;
    double historical_relevance_weight = 0.3;
    
    // Cache de similaridades
    std::map<std::string, double> expert_affinities;

public:
    WeightedFusion(Embedder& embedder_ref) 
        : embedder(embedder_ref),
          env(ORT_LOGGING_LEVEL_WARNING, "AlyssaFusion"),
          session(nullptr) { 
          
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1); 
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

        session = Ort::Session(env, "fusion_router.onnx", session_options);
    }   
     
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