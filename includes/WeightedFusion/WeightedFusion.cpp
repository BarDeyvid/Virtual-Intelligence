#include "WeightedFusion/WeightedFusion.hpp"
#include <algorithm>
#include <numeric>

namespace alyssa_fusion {

// 🔹 A. Rule-based Fusion
std::map<std::string, double> WeightedFusion::calculate_rule_based_weights(
    const std::string& input, 
    const std::vector<std::string>& available_experts) {
    
    std::map<std::string, double> weights;
    std::string lower_input = input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
    
    // Inicializa pesos base
    for (const auto& expert : available_experts) {
        weights[expert] = 0.1; // peso mínimo
    }
    
    // Regras para emotionalModel
    if (lower_input.find("feliz") != std::string::npos ||
        lower_input.find("triste") != std::string::npos ||
        lower_input.find("raiva") != std::string::npos ||
        lower_input.find("medo") != std::string::npos ||
        lower_input.find("amo") != std::string::npos ||
        lower_input.find("odeio") != std::string::npos) {
        weights["emotionalModel"] += 0.6;
    }
    
    // Regras para memoryModel
    if (lower_input.find("lembr") != std::string::npos ||
        lower_input.find("memória") != std::string::npos ||
        lower_input.find("passado") != std::string::npos ||
        lower_input.find("antes") != std::string::npos) {
        weights["memoryModel"] += 0.5;
    }
    
    // Regras para introspectiveModel
    if (lower_input.find("porque") != std::string::npos ||
        lower_input.find("como funciona") != std::string::npos ||
        lower_input.find("pensar") != std::string::npos ||
        lower_input.find("analisar") != std::string::npos) {
        weights["introspectiveModel"] += 0.5;
    }
    
    // Regras para socialModel
    if (lower_input.find("oi") != std::string::npos ||
        lower_input.find("olá") != std::string::npos ||
        lower_input.find("bom dia") != std::string::npos ||
        lower_input.find("tchau") != std::string::npos) {
        weights["socialModel"] += 0.4;
    }
    
    // Normalização via softmax
    double sum = 0.0;
    for (const auto& w : weights) sum += exp(w.second);
    for (auto& w : weights) w.second = exp(w.second) / sum;
    
    return weights;
}

// 🔹 B. Feature-based Fusion
std::map<std::string, double> WeightedFusion::calculate_feature_based_weights(
    const std::string& input,
    const std::vector<ExpertContribution>& expert_responses) {
    
    std::map<std::string, double> weights;
    
    try {
        // Embedding do input
        auto input_embedding = embedder.generate_embedding(input);
        
        // Calcula similaridade para cada resposta de especialista
        std::vector<double> similarities;
        for (const auto& contribution : expert_responses) {
            double sim = calculate_semantic_similarity(input_embedding, contribution.embedding);
            similarities.push_back(sim);
            weights[contribution.expert_id] = sim;
        }
        
        // Softmax nas similaridades
        double sum = 0.0;
        for (double sim : similarities) sum += exp(sim);
        for (auto& w : weights) w.second = exp(w.second) / sum;
        
    } catch (const std::exception& e) {
        // Fallback: pesos iguais em caso de erro
        double equal_weight = 1.0 / expert_responses.size();
        for (const auto& contribution : expert_responses) {
            weights[contribution.expert_id] = equal_weight;
        }
    }
    
    return weights;
}

// 🔹 C. Neural Fusion (implementação simplificada)
std::map<std::string, double> WeightedFusion::calculate_neural_weights(
    const std::string& input,
    const std::vector<ExpertContribution>& expert_responses,
    const std::string& current_emotion) {
    
    // Esta é uma implementação simplificada
    // Na prática, você teria um modelo neural leve aqui
    
    std::map<std::string, double> weights;
    
    // Combina abordagens A e B com fatores adicionais
    auto rule_weights = calculate_rule_based_weights(input, {});
    auto feature_weights = calculate_feature_based_weights(input, expert_responses);
    
    // Fator emocional
    double emotion_factor = 0.0;
    if (current_emotion == "happy") emotion_factor = 0.1;
    else if (current_emotion == "sad") emotion_factor = 0.15;
    else if (current_emotion == "analytical") emotion_factor = 0.2;
    
    // Combinação ponderada
    for (const auto& contribution : expert_responses) {
        double rule_w = rule_weights.count(contribution.expert_id) ? 
                       rule_weights[contribution.expert_id] : 0.1;
        double feature_w = feature_weights.count(contribution.expert_id) ? 
                          feature_weights[contribution.expert_id] : 0.1;
        
        weights[contribution.expert_id] = 
            (rule_w * 0.4) + (feature_w * 0.4) + (emotion_factor * 0.2);
    }
    
    // Normalização final
    double sum = 0.0;
    for (const auto& w : weights) sum += w.second;
    for (auto& w : weights) w.second /= sum;
    
    return weights;
}

// 🧠 Método principal de fusão
std::string WeightedFusion::fuse_responses(
    const std::string& input,
    const std::vector<ExpertContribution>& contributions,
    const std::string& current_emotion) {
    
    if (contributions.empty()) return "Nenhum especialista respondeu.";
    if (contributions.size() == 1) return contributions[0].response;
    
    // Calcula pesos usando fusão neural
    auto weights = calculate_neural_weights(input, contributions, current_emotion);
    
    // Para debug - mostra os pesos calculados
    std::cout << "\n[Weighted Fusion] Pesos calculados:\n";
    for (const auto& w : weights) {
        std::cout << "  " << w.first << ": " << std::fixed << std::setprecision(3) << w.second << "\n";
    }
    
    // Estratégia: seleciona a resposta do especialista com maior peso
    // (Futuramente pode-se fazer fusão mais sofisticada dos embeddings)
    std::string best_expert;
    double best_weight = -1.0;
    
    for (const auto& contribution : contributions) {
        if (weights[contribution.expert_id] > best_weight) {
            best_weight = weights[contribution.expert_id];
            best_expert = contribution.expert_id;
        }
    }
    
    // Encontra a resposta do melhor especialista
    for (const auto& contribution : contributions) {
        if (contribution.expert_id == best_expert) {
            std::cout << "[Weighted Fusion] Selecionado: " << best_expert 
                      << " (peso: " << best_weight << ")\n";
            return contribution.response;
        }
    }
    
    return contributions[0].response; // fallback
}

// Utilitários
double WeightedFusion::calculate_semantic_similarity(
    const std::vector<float>& emb1, 
    const std::vector<float>& emb2) {
    
    if (emb1.size() != emb2.size() || emb1.empty()) return 0.0;
    
    double dot_product = 0.0, norm1 = 0.0, norm2 = 0.0;
    for (size_t i = 0; i < emb1.size(); ++i) {
        dot_product += emb1[i] * emb2[i];
        norm1 += emb1[i] * emb1[i];
        norm2 += emb2[i] * emb2[i];
    }
    
    if (norm1 == 0.0 || norm2 == 0.0) return 0.0;
    return dot_product / (sqrt(norm1) * sqrt(norm2));
}

std::string WeightedFusion::detect_emotion_from_input(const std::string& input) {
    std::string lower_input = input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
    
    if (lower_input.find("feliz") != std::string::npos || 
        lower_input.find("alegre") != std::string::npos ||
        lower_input.find("amo") != std::string::npos) {
        return "happy";
    } else if (lower_input.find("triste") != std::string::npos ||
               lower_input.find("triste") != std::string::npos) {
        return "sad";
    } else if (lower_input.find("porque") != std::string::npos ||
               lower_input.find("como") != std::string::npos) {
        return "analytical";
    }
    
    return "neutral";
}

std::string WeightedFusion::extract_keywords(const std::string& text) {
    // Implementação simplificada de extração de keywords
    // Na prática, usaríamos um algoritmo mais sofisticado
    return text; // placeholder
}

} // namespace alyssa_fusion