#include "WeightedFusion/WeightedFusion.hpp"
#include <iomanip>
#include <iostream>

namespace alyssa_fusion {

// 🔹 A. Rule-based Fusion
/**
 * @brief Calculates rule-based weights for expert contributions.
 * 
 * Assigns initial minimum weights to all available experts and adjusts them based on specific keywords found in the input.
 * 
 * @param input The user's input string.
 * @param available_experts List of available experts.
 * @return A map from expert IDs to their corresponding weights.
 */
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
/**
 * @brief Calculates feature-based weights for expert contributions.
 * 
 * Computes the similarity between the input and each expert's response embedding, then applies softmax normalization to these similarities.
 * 
 * @param input The user's input string.
 * @param expert_responses Vector of ExpertContribution objects containing responses from various experts.
 * @return A map from expert IDs to their corresponding weights.
 */
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
/**
 * @brief Calculates neural-based weights for expert contributions using an ONNX model.
 * 
 * Generates embeddings for the input and each expert's response, runs inference through an ONNX model to determine weights, and applies emotion adjustments if specified.
 * 
 * @param input The user's input string.
 * @param expert_responses Vector of ExpertContribution objects containing responses from various experts.
 * @param current_emotion Current emotional context (e.g., "happy", "sad").
 * @return A map from expert IDs to their corresponding weights.
 */
std::map<std::string, double> WeightedFusion::calculate_neural_weights(
    const std::string& input,
    const std::vector<ExpertContribution>& expert_responses,
    const std::string& current_emotion) {

    std::map<std::string, double> weights;

    // 1. Generate Embedding
    // Ensure this returns std::vector<float> of size 384 (matching your model)
    std::vector<float> input_embedding = embedder.generate_embedding(input);

    if (input_embedding.empty()) {
        std::cerr << "[WeightedFusion] Error: Empty embedding generated.\n";
        return weights; // Return empty or fallback
    }

    // 2. Prepare ONNX Input
    // Shape: [Batch_Size, Dimensions] -> [1, 768]
    std::vector<int64_t> input_shape = {1, (int64_t)input_embedding.size()};
    
    // Memory Info for the tensor
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    
    // Create Input Tensor
    // Note: We pass the data pointer directly. 
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, 
        input_embedding.data(), 
        input_embedding.size(), 
        input_shape.data(), 
        input_shape.size()
    );

    // 3. Setup Input/Output Names
    const char* input_names[] = {"input_embedding"};
    const char* output_names[] = {"expert_weights"};

    try {
        // 4. Run Inference
        auto output_tensors = session.Run(
            Ort::RunOptions{nullptr}, 
            input_names, 
            &input_tensor, 
            1, // Number of inputs
            output_names, 
            1  // Number of outputs
        );

        float* float_weights = output_tensors[0].GetTensorMutableData<float>();
        
        for (size_t i = 0; i < expert_responses.size(); ++i) {
            weights[expert_responses[i].expert_id] = static_cast<double>(float_weights[i]);
        }

        if (current_emotion == "happy" || current_emotion == "sad") {
            weights["emotionalModel"] *= 1.2; 
        } else if (current_emotion == "analytical") {
            weights["introspectiveModel"] *= 1.2;
        }
        
        double sum = 0.0;
        for (const auto& pair : weights) sum += pair.second;
        if (sum > 0) {
            for (auto& pair : weights) weights[pair.first] = pair.second / sum;
        }

    } catch (const Ort::Exception& e) {
        std::cerr << "[WeightedFusion] ONNX Runtime Error: " << e.what() << "\n";
        return calculate_rule_based_weights(input, {}); 
    }

    return weights;
}

// 🧠 Método principal de fusão
/**
 * @brief Fuses multiple expert responses into a single output.
 * 
 * Uses neural-based weights to determine the most relevant expert response and returns it. If no contributions are available, returns a default message.
 * The EndocrineSystem influences weight distribution based on hormonal state.
 * 
 * @param input The user's input string.
 * @param contributions Vector of ExpertContribution objects containing responses from various experts.
 * @param endocrine The EndocrineSystem instance for hormonal modulation of weights.
 * @param current_emotion Current emotional context (optional).
 * @return The fused response as a single string.
 */
std::string WeightedFusion::fuse_responses(
    const std::string& input,
    const std::vector<ExpertContribution>& contributions,
    const alyssa_endocrine::EndocrineSystem& endocrine,
    const std::string& current_emotion) {
    
    if (contributions.empty()) return "Nenhum especialista respondeu.";
    if (contributions.size() == 1) return contributions[0].response;
    
    // Calcula pesos usando fusão neural
    auto weights = calculate_neural_weights(input, contributions, current_emotion);
    
    // 🧬 INFLUÊNCIA HORMONAL: Aplica multiplicadores baseado em hormônios
    std::cout << "\n[Endocrine Modulation] Aplicando influência hormonal aos pesos...\n";
    for (auto& contribution : const_cast<std::vector<ExpertContribution>&>(contributions)) {
        double hormone_multiplier = endocrine.get_expert_weight_multiplier(contribution.expert_id);
        
        if (weights.find(contribution.expert_id) != weights.end()) {
            weights[contribution.expert_id] *= hormone_multiplier;
            std::cout << "  " << contribution.expert_id << ": ×" 
                     << std::fixed << std::setprecision(3) << hormone_multiplier << "\n";
        }
    }
    
    // Renormalizar pesos após modulação hormonal
    double sum = 0.0;
    for (const auto& w : weights) sum += w.second;
    if (sum > 0.0) {
        for (auto& w : weights) w.second /= sum;
    }
    
    // Para debug - mostra os pesos calculados
    std::cout << "\n[Weighted Fusion] Pesos finais (após hormônios):\n";
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
/**
 * @brief Calculates the semantic similarity between two embeddings.
 * 
 * Computes the cosine similarity between two vector embeddings.
 * 
 * @param emb1 The first embedding as a vector of floats.
 * @param emb2 The second embedding as a vector of floats.
 * @return The similarity score (0 to 1).
 */
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

/**
 * @brief Detects the emotion from the user's input.
 * 
 * Analyzes the input string for specific keywords related to emotions and returns the detected emotion.
 * 
 * @param input The user's input string.
 * @return Detected emotion as a string (e.g., "happy", "sad").
 */
std::string WeightedFusion::detect_emotion_from_input(const std::string& input) {
    std::string lower_input = input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
    
    // Emotion keyword mapping
    const std::map<std::string, std::vector<std::string>> emotion_keywords = {
        {"happy", {"feliz", "alegre", "amo", "adorei", "legal"}},
        {"sad", {"triste", "infeliz", "deprimido", "choro", "sofro"}},
        {"analytical", {"porque", "como", "explique", "analise", "entenda"}}
    };
    
    // Count keyword matches per emotion
    std::map<std::string, int> emotion_scores;
    for (const auto& [emotion, keywords] : emotion_keywords) {
        for (const auto& keyword : keywords) {
            if (lower_input.find(keyword) != std::string::npos) {
                emotion_scores[emotion]++;
            }
        }
    }
    
    // Return emotion with highest score
    if (emotion_scores.empty()) {
        return "neutral";
    }
    
    auto best_emotion = std::max_element(
        emotion_scores.begin(), emotion_scores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; }
    );
    
    return best_emotion->first;
}

/**
 * @brief Extracts keywords from the given text.
 * 
 * Placeholder function for extracting keywords. In practice, this should implement a more sophisticated keyword extraction algorithm.
 * 
 * @param text The input string to extract keywords from.
 * @return The extracted keywords as a string (placeholder).
 */
std::string WeightedFusion::extract_keywords(const std::string& text) {
    // Implementação simplificada de extração de keywords
    // Na prática, usaríamos um algoritmo mais sofisticado
    return text; // placeholder
}

} // namespace alyssa_fusion
