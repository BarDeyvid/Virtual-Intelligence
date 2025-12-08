//AlyssaMemoryHandler.hpp
#ifndef ALYSSA_MEMORY_SYSTEM_HPP
#define ALYSSA_MEMORY_SYSTEM_HPP

#include <filesystem>
#include "sqlite3.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <memory>
#include <ctime>
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <unordered_map>
#include <numeric>
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Estruturas de Dados
// ============================================================================

/**
 * @brief Estrutura para análise emocional de texto
 */
struct EmotionalAnalysis {
    std::vector<float> emotional_vector;
    std::string dominant_emotion;
    double confidence;
    std::unordered_map<std::string, double> emotion_scores;
};

/**
 * @brief Estrutura para representar estado emocional
 */
struct EmotionalState {
    std::string name;
    double intensity;
    uint64_t timestamp;
    
    EmotionalState(const std::string& n = "neutral", double i = 0.5);
};

/**
 * @brief Estrutura para representar intenções do sistema
 */
struct Intention {
    int id;
    std::string description;
    std::string type;
    std::string trigger;
    bool active;
    double motivation;
    std::string created_at;
    
    Intention(const std::string& desc, const std::string& t, const std::string& trig = "");
};

/**
 * @brief Estrutura para vínculos entre memórias
 */
struct MemoryLink {
    int source_id;
    int target_id;
    double weight;
    std::string type;
    
    MemoryLink(int src, int tgt, double w = 1.0, const std::string& t = "association");
};

/**
 * @brief Estrutura para reflexões geradas pelo sistema
 */
struct Reflection {
    int memory_id;
    std::string type;
    std::string content;
    std::string created_at;
    
    Reflection(int mem_id, const std::string& t, const std::string& c);
};

// ============================================================================
// Classe EmotionalAnalyzer
// ============================================================================

/**
 * @brief Analisador emocional baseado em lexicon para texto em português
 */
class EmotionalAnalyzer {
private:
    std::unordered_map<std::string, std::vector<std::string>> emotion_lexicons;
    std::unordered_map<std::string, double> emotion_weights;
    std::vector<std::string> emotion_categories;

    void initializeEmotionLexicons();
    void analyzeWithLexicon(const std::string& text, std::unordered_map<std::string, double>& scores);
    void analyzePatterns(const std::string& text, std::unordered_map<std::string, double>& scores);
    void analyzePunctuation(const std::string& text, std::unordered_map<std::string, double>& scores);
    std::vector<float> normalizeScores(const std::unordered_map<std::string, double>& scores);
    std::string findDominantEmotion(const std::unordered_map<std::string, double>& scores);
    double calculateConfidence(const std::unordered_map<std::string, double>& scores);

public:
    EmotionalAnalyzer();
    
    EmotionalAnalysis analyzeText(const std::string& text);
    EmotionalAnalysis analyzeConversation(const std::string& user_input, const std::string& ai_response);
};

// ============================================================================
// Embedding
// ============================================================================

class Embedder;

// ============================================================================
// Classe Utils
// ============================================================================

/**
 * @brief Utilitários para o sistema de memória
 */
class Utils {
public:
    static std::vector<float> getTimeStamp();
    static std::string getCurrentISOTime();
    static double sigmoid(double x);
    static double normalizeImportance(double raw_importance);
};

// ============================================================================
// Classe AdvancedMemorySystem
// ============================================================================

/**
 * @brief Sistema avançado de memória com suporte a embeddings vetoriais e análise emocional
 */
class AdvancedMemorySystem {
private:
    sqlite3* db;
    EmotionalState current_emotional_state;
    std::vector<Intention> active_intentions;
    std::map<std::string, double> emotion_weights;
    std::shared_ptr<Embedder> embedder;
    std::unique_ptr<EmotionalAnalyzer> emotional_analyzer;
    
    std::unordered_map<std::string, std::pair<std::string, std::string>> emotion_to_intention = {
        {"tristeza", {"Refletir sobre sentimentos difíceis", "emocional"}},
        {"alegria", {"Manter momentos positivos", "emocional"}},
        {"raiva", {"Redirecionar frustrações", "emocional"}},
        {"medo", {"Buscar segurança", "emocional"}},
        {"surpresa", {"Explorar novidade", "emocional"}},
        {"confiança", {"Apoiar outros", "emocional"}},
        {"antecipacao", {"Planejar próximos passos", "planejamento"}}
    };

    void initializeDatabase();
    void loadCurrentState();
    void saveCurrentState();
    void loadActiveIntentions();
    void storeEmbedding(int memory_id, const std::vector<float>& embedding);
    void checkEmotionalAutoActivation();
    void saveReflection(int memory_id, const std::string& type, const std::string& content);
    double calculateIntentionBoost(const std::string& content);
    double calculateEmotionalBoost(const std::string& memory_emotion);
    double calculateAutomaticImportance(const std::string& content, const EmotionalAnalysis& analysis);

public:
    // Estruturas para resultados de busca
    struct ContextualMemory {
        int id;
        std::string content;
        double relevance_score;
        std::string emotion;
    };
    
    struct SemanticMemory {
        int memory_id;
        std::string content;
        double similarity_score;
        std::string emotion;
        double importance;
    };
    
    struct HybridMemoryResult {
        int memory_id;
        std::string content;
        double text_score;
        double semantic_score;
        double combined_score;
        std::string emotion;
    };

    AdvancedMemorySystem(const std::string& db_path, std::shared_ptr<Embedder> embedder_ref);
    ~AdvancedMemorySystem();

    // Gerenciamento de estado emocional
    void setEmotionalState(const std::string& emotion, double intensity);
    EmotionalState getCurrentEmotionalState() const;
    
    // Gerenciamento de memórias
    int storeMemoryWithEmotionalAnalysis(const std::string& content, const std::string& context = "");
    void applyMemoryDecay();
    
    // Sistema de intenções
    void activateIntention(const std::string& description, const std::string& type, 
                          const std::string& trigger = "", double motivation_boost = 0.0);
    void deactivateIntention(int intention_id);
    std::vector<Intention> getActiveIntentions() const;
    
    // Sistema de reflexões
    void generateReflections();
    
    // Sistema de vínculos
    void createMemoryLink(int source_id, int target_id, double weight = 1.0, 
                         const std::string& link_type = "association");
    std::vector<MemoryLink> getMemoryLinks(int memory_id);
    
    // Sistema de busca
    std::vector<ContextualMemory> searchContextualMemories(const std::string& query, int top_k = 5);
    std::vector<SemanticMemory> semanticSearch(const std::string& query, int top_k = 5);
    std::vector<SemanticMemory> semanticSearchWithEmbedding(const std::vector<float>& query_embedding, int top_k = 5);
    std::vector<HybridMemoryResult> hybridSearch(const std::string& query, int top_k = 5);
    
    // Sistema de embeddings
    bool hasEmbedder() const;
    bool generateAndStoreEmbedding(int memory_id, const std::string& content);
    
    // Análise emocional automática
    EmotionalAnalysis analyzeEmotionalContent(const std::string& text);
    EmotionalAnalysis analyzeConversationEmotions(const std::string& user_input, const std::string& ai_response);
    std::vector<float> getAutoEmotionalVector(const std::string& text);
    
    // Monitoramento e debug
    void printSystemStatus();
    void demonstrateSemanticSearch(const std::string& query);
};

// ============================================================================
// Classe AlyssaMemoryManager
// ============================================================================

/**
 * @brief Gerenciador de alto nível para integração com sistemas de IA conversacional
 */
class AlyssaMemoryManager {
private:
    std::unique_ptr<AdvancedMemorySystem> memory_system;
    
    void printEmotionalAnalysis(const EmotionalAnalysis& analysis, const std::string& text);
    void analyzeInputForIntentions(const std::string& input);

public:
    AlyssaMemoryManager(const std::string& db_path, std::shared_ptr<Embedder> embedder_ref);    
    // Processamento de interações
    void processInteraction(const std::string& user_input, const std::string& ai_response);
    void processInteraction(const std::string& user_input, 
                          const std::string& ai_response,
                          const std::vector<float>& emotional_vector);
    
    // Sistema de recuperação
    std::vector<AdvancedMemorySystem::ContextualMemory> getRelevantMemories(const std::string& context);
    std::vector<AdvancedMemorySystem::SemanticMemory> getSemanticMemories(const std::string& context);
    std::vector<AdvancedMemorySystem::HybridMemoryResult> getHybridMemories(const std::string& context);
    
    // Controle de objetivos
    void setCurrentGoal(const std::string& goal, const std::string& type = "learning");
    void processIdentityFact(const std::string& fact_value, const std::string& fact_type);
};

// ============================================================================
// Constantes
// ============================================================================

constexpr int VTIME_DIM = 2;
constexpr size_t MAX_TOKENS = 4096;
constexpr double DECAY_PER_HOUR = 0.01;
constexpr double MIN_IMPORTANCE = 0.3;
constexpr double HARD_LOCK_IMPORTANCE = 0.9;

#endif // ALYSSA_MEMORY_SYSTEM_HPP