#pragma once
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <cmath>
#include "Embedding/Embedder.hpp"
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <algorithm>
#include <numeric>

namespace alyssa_fusion {

/**
 * @struct ExpertContribution
 * @brief Represents the contribution of an expert with its response and metadata.
 */
struct ExpertContribution {
    std::string expert_id;      /**< Unique identifier for the expert */
    double weight;             /**< Weight assigned to the expert's response */
    std::string response;       /**< The expert's response */
    std::vector<float> embedding; /**< Embedding of the expert's response */
    std::string source;         /**< Source or context of the response */
};

/**
 * @class WeightedFusion
 * @brief A class that performs weighted fusion of expert responses based on various criteria.
 */
class WeightedFusion {
private:
    Embedder& embedder;
    
    // ONNX Runtime Members
    Ort::Env env;
    Ort::Session session;
    
    // Configurações de ponderação
    double emotion_weight_base = 0.3;              /**< Base weight for emotional context */
    double context_similarity_weight = 0.4;        /**< Weight for similarity in context */
    double historical_relevance_weight = 0.3;      /**< Weight for historical relevance */
    
    // Cache de similaridades
    std::map<std::string, double> expert_affinities;

public:
    /**
     * @brief Constructor for the WeightedFusion class.
     * 
     * Initializes the embedder and ONNX session.
     * 
     * @param embedder_ref Reference to the Embedder instance used for generating embeddings.
     */
    WeightedFusion(Embedder& embedder_ref) 
        : embedder(embedder_ref),
          env(ORT_LOGGING_LEVEL_WARNING, "AlyssaFusion"),
          session(nullptr) { 
          
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1); 
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

        session = Ort::Session(env, "fusion_router.onnx", session_options);
    }   

    /**
     * @brief Calculates rule-based weights for expert contributions.
     * 
     * Assigns initial minimum weights to all available experts and adjusts them based on specific keywords found in the input.
     * 
     * @param input The user's input string.
     * @param available_experts List of available experts.
     * @return A map from expert IDs to their corresponding weights.
     */
    std::map<std::string, double> calculate_rule_based_weights(
        const std::string& input, 
        const std::vector<std::string>& available_experts);
    
    /**
     * @brief Calculates feature-based weights for expert contributions.
     * 
     * Computes the similarity between the input and each expert's response embedding, then applies softmax normalization to these similarities.
     * 
     * @param input The user's input string.
     * @param expert_responses Vector of ExpertContribution objects containing responses from various experts.
     * @return A map from expert IDs to their corresponding weights.
     */
    std::map<std::string, double> calculate_feature_based_weights(
        const std::string& input,
        const std::vector<ExpertContribution>& expert_responses);
    
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
    std::map<std::string, double> calculate_neural_weights(
        const std::string& input,
        const std::vector<ExpertContribution>& expert_responses,
        const std::string& current_emotion);
    
    /**
     * @brief Fuses multiple expert responses into a single output.
     * 
     * Uses neural-based weights to determine the most relevant expert response and returns it. If no contributions are available, returns a default message.
     * 
     * @param input The user's input string.
     * @param contributions Vector of ExpertContribution objects containing responses from various experts.
     * @param current_emotion Current emotional context (optional).
     * @return The fused response as a single string.
     */
    std::string fuse_responses(
        const std::string& input,
        const std::vector<ExpertContribution>& contributions,
        const std::string& current_emotion = "");
    
    /**
     * @brief Calculates the semantic similarity between two embeddings.
     * 
     * Computes the cosine similarity between two vector embeddings.
     * 
     * @param emb1 The first embedding as a vector of floats.
     * @param emb2 The second embedding as a vector of floats.
     * @return The similarity score (0 to 1).
     */
    double calculate_semantic_similarity(const std::vector<float>& emb1, const std::vector<float>& emb2);
    
    /**
     * @brief Detects the emotion from the user's input.
     * 
     * Analyzes the input string for specific keywords related to emotions and returns the detected emotion.
     * 
     * @param input The user's input string.
     * @return Detected emotion as a string (e.g., "happy", "sad").
     */
    std::string detect_emotion_from_input(const std::string& input);
    
    /**
     * @brief Extracts keywords from the given text.
     * 
     * Placeholder function for extracting keywords. In practice, this should implement a more sophisticated keyword extraction algorithm.
     * 
     * @param text The input string to extract keywords from.
     * @return The extracted keywords as a string (placeholder).
     */
    std::string extract_keywords(const std::string& text);
};

} // namespace alyssa_fusion
