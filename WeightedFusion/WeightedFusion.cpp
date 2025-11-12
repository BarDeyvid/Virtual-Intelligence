#include "WeightedFusion.hpp"
#include <algorithm>
#include <numeric>

namespace alyssa_fusion {

// 🔽 NOVO: Construtor agora inicializa as afinidades
WeightedFusion::WeightedFusion(Embedder& embedder_ref) : embedder(embedder_ref) {
    std::cout << "[Fusion] Inicializando afinidades dos especialistas..." << std::endl;
    embedder.initialize();
    try {
        // (Usei os IDs do seu log: emotionalModel, memoryModel, introspectiveModel, alyssa)
        expert_affinity_embeddings["emotionalModel"] = embedder.generate_embedding("emoção, sentimento, feliz, triste, raiva, como você se sente");
        expert_affinity_embeddings["memoryModel"] = embedder.generate_embedding("memória, lembrar, o que aconteceu antes, passado, recordar");
        expert_affinity_embeddings["introspectiveModel"] = embedder.generate_embedding("por que, como funciona, analisar, pensar, filosofia, significado");
        expert_affinity_embeddings["alyssa"] = embedder.generate_embedding("conversa geral, olá, como vai, o que você acha, quem é você");
        std::cout << "[Fusion] Afinidades carregadas com sucesso." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "AVISO: Falha ao inicializar afinidades do Fusion: " << e.what() << std::endl;
    }
}


// 🔹 A. Rule-based Fusion (Com pesos mais "agressivos")
std::map<std::string, double> WeightedFusion::calculate_rule_based_weights(
    const std::string& input, 
    const std::vector<std::string>& available_experts) {
    
    std::map<std::string, double> weights;
    std::string lower_input = input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), 
    [](unsigned char c){ return std::tolower(c); }
);
    
    for (const auto& expert : available_experts) {
        weights[expert] = 1.0; // Peso base 1.0 (antes do softmax)
    }
    
    // Regras (valores maiores dão mais "força" ao softmax)
    if (lower_input.find("feliz") != std::string::npos ||
        lower_input.find("triste") != std::string::npos ||
        lower_input.find("raiva") != std::string::npos ||
        lower_input.find("medo") != std::string::npos ||
        lower_input.find("sinto") != std::string::npos || // Adicionado
        lower_input.find("odeio") != std::string::npos) {
        weights["emotionalModel"] += 5.0; // 0.6 é muito pouco, vamos usar 5.0
    }
    
    if (lower_input.find("lembr") != std::string::npos ||
        lower_input.find("memória") != std::string::npos ||
        lower_input.find("passado") != std::string::npos ||
        lower_input.find("o que falamos") != std::string::npos) { // Adicionado
        weights["memoryModel"] += 5.0;
    }
    
    if (lower_input.find("por que") != std::string::npos || // "porque" -> "por que"
        lower_input.find("como funciona") != std::string::npos ||
        lower_input.find("pensa") != std::string::npos || // "pensar" -> "pensa"
        lower_input.find("analisar") != std::string::npos) {
        weights["introspectiveModel"] += 5.0;
    }
    
    // Regras para o especialista 'alyssa' (geral)
    if (lower_input.find("oi") != std::string::npos ||
        lower_input.find("olá") != std::string::npos ||
        lower_input.find("bom dia") != std::string::npos ||
        lower_input.find("tchau") != std::string::npos) {
        weights["alyssa"] += 3.0;
    }
    
    // Normalização via softmax (agora os pesos serão bem distintos)
    double sum = 0.0;
    for (const auto& w : weights) sum += exp(w.second);
    for (auto& w : weights) w.second = exp(w.second) / sum;
    
    return weights;
}

// 🔹 B. Feature-based Fusion (🔽 NOVA LÓGICA DE AFINIDADE)
std::map<std::string, double> WeightedFusion::calculate_feature_based_weights(
    const std::string& input,
    const std::vector<ExpertContribution>& expert_responses) {
    
    std::map<std::string, double> weights;

    // Fallback se afinidades não foram carregadas
    if (expert_affinity_embeddings.empty()) {
        double equal_weight = 1.0 / expert_responses.size();
        for (const auto& c : expert_responses) weights[c.expert_id] = equal_weight;
        return weights;
    }

    try {
        auto input_embedding = embedder.generate_embedding(input);
        
        for (const auto& contribution : expert_responses) {
            const std::string& expert_id = contribution.expert_id;
            
            if (expert_affinity_embeddings.count(expert_id)) {
                // Compara o INPUT com a ESPECIALIDADE (Afinidade)
                double sim = calculate_semantic_similarity(input_embedding, expert_affinity_embeddings.at(expert_id));
                weights[expert_id] = sim;
            } else {
                weights[expert_id] = 0.1; // Penaliza especialista sem afinidade
            }
        }
        
        // Softmax para normalizar e acentuar
        double sum = 0.0;
        for (const auto& w : weights) sum += exp(w.second);
        if (sum == 0.0) sum = 1.0; 
        for (auto& w : weights) w.second = exp(w.second) / sum;
        
    } catch (const std::exception& e) {
        // Fallback em caso de erro no embedding
        double equal_weight = 1.0 / expert_responses.size();
        for (const auto& c : expert_responses) weights[c.expert_id] = equal_weight;
    }
    return weights;
}

// 🔹 C. Hybrid Fusion (🔽 RENOMEADO e com LÓGICA DE MULTIPLICAÇÃO)
std::map<std::string, double> WeightedFusion::calculate_hybrid_weights(
    const std::string& input,
    const std::vector<ExpertContribution>& expert_responses,
    const std::string& current_emotion) {
    
    std::map<std::string, double> final_weights;

    // 1. Obter pesos das Regras (Baseado em keywords)
    std::vector<std::string> expert_ids;
    for(const auto& c : expert_responses) expert_ids.push_back(c.expert_id);
    auto rule_weights = calculate_rule_based_weights(input, expert_ids);
    
    // 2. Obter pesos da Afinidade (Baseado em semântica)
    auto affinity_weights = calculate_feature_based_weights(input, expert_responses);
    
    // 3. Combinar usando Multiplicação (Produto de Especialistas)
    // Isso amplifica fortemente os especialistas que pontuam bem em AMBAS as métricas
    double total_weight = 0.0;
    for (const auto& expert_id : expert_ids) {
        // Usamos 0.01 como "piso" para evitar zerar a pontuação
        double rule_w = rule_weights.count(expert_id) ? rule_weights[expert_id] : 0.01;
        double affinity_w = affinity_weights.count(expert_id) ? affinity_weights[expert_id] : 0.01;
        
        // A "mágica": multiplicar em vez de somar
        double combined_score = rule_w * affinity_w; 
        
        // Bônus/Penalidade Emocional (Exemplo)
        if (current_emotion == "analytical" && expert_id == "introspectiveModel") {
            combined_score *= 1.5; // Bônus de 50%
        } else if (current_emotion == "happy" && expert_id == "emotionalModel") {
            combined_score *= 1.3;
        }
        
        final_weights[expert_id] = combined_score;
        total_weight += combined_score;
    }
    
    // 4. Normalização final (para que somem 1.0)
    if (total_weight > 0.0) {
        for (auto& w : final_weights) {
            w.second /= total_weight;
        }
    } else {
        // Fallback
        for (const auto& expert_id : expert_ids) final_weights[expert_id] = 1.0 / expert_ids.size();
    }
    
    return final_weights;
}

// 🧠 Método principal de fusão (🔽 ATUALIZADO para chamar a função híbrida)
std::string WeightedFusion::fuse_responses(
    const std::string& input,
    const std::vector<ExpertContribution>& contributions,
    const std::string& current_emotion) {
    
    if (contributions.empty()) return "Nenhum especialista respondeu.";
    if (contributions.size() == 1) return contributions[0].response;
    
    // Calcula pesos usando a NOVA fusão híbrida
    auto weights = calculate_hybrid_weights(input, contributions, current_emotion);
    
    // Para debug - mostra os pesos calculados (agora devem ser bem diferentes)
    std::cout << "\n[Weighted Fusion] Pesos calculados (Híbrido):\n";
    for (const auto& w : weights) {
        std::cout << "  " << w.first << ": " << std::fixed << std::setprecision(3) << w.second << "\n";
    }
    
    // Estratégia: seleciona a resposta do especialista com maior peso
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
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), 
    [](unsigned char c){ return std::tolower(c); }
);
    
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