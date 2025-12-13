//AlyssaMemoryHandler.hpp
#pragma once

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
#include "EmotionLexiconLoader.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Data Struct Settings
// ============================================================================

struct EmotionalAnalyzerConfig {
    struct EmotionWeight {
        std::string emotion;
        double weight;
    };

    std::vector<EmotionWeight> default_weights = {
        {"alegria", 1.0}, {"tristeza", 1.0}, {"raiva", 1.0}, 
        {"medo", 1.0}, {"surpresa", 0.8}, {"confiança", 0.9},
        {"antecipacao", 0.7}, {"desgosto", 0.9}
    };

    std::vector<std::string> emotion_categories = {
        "alegria", "tristeza", "raiva", "medo", 
        "surpresa", "confiança", "antecipacao", "desgosto"
    };

    double lexicon_word_weight = 0.1;
    double intesifier_boost = 0.15;
    double negation_penalty = 0.1;
    double exclamation_boost = 0.05;
    double question_boost = 0.08;
    double period_frustation_boost = 0.05;
    double min_confidence_threshold = 0.1;

    double user_input_weight = 0.7;
    double ai_response_weight = 0.3;

};

struct MemorySearchConfig {
    int default_top_k = 5;
    double text_weight = 0.4;
    double semantic_weight = 0.6;
    double intention_boost_weight = 0.3;
    double emotional_boost_weight = 0.2;
    double min_similarity_score = 0.1;

    struct BoostConfig {
        double active_intention_multiplier = 0.3;
        double emotional_match_multiplier = 0.2;
        double recent_memory_boost = 0.1;
    } boost_config;
};


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
 * @brief Estrutura para ID's das memorias
 */
    
struct MemoryId {
    int value;
    
    explicit constexpr MemoryId(int v) : value(v) {}
    
    bool operator==(const MemoryId& other) const {return value == other.value;}
    bool operator!=(const MemoryId& other) const {return value != other.value;}
    operator int() const {return value;}
    
    static constexpr MemoryId invalid() {return MemoryId(-1);}
    bool isValid() const {return value > 0;}
};

/**
 * @brief Estrutura para ID's das intencoes
 */

struct IntentionId {
    int value;
    
    explicit constexpr IntentionId(int v) : value(v) {}
    
    bool operator==(const IntentionId& other) const {return value == other.value;}
    bool operator!=(const IntentionId& other) const {return value != other.value;}
    operator int() const {return value;}
    
    static constexpr IntentionId invalid() {return IntentionId(-1);}
    bool isValid() const {return value > 0;}
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
    MemoryId source_id;
    MemoryId target_id;
    double weight;
    std::string type;
    
    MemoryLink(MemoryId src, MemoryId tgt, double w = 1.0, const std::string& t = "association");
};

/**
 * @brief Estrutura para reflexões geradas pelo sistema
 */
struct Reflection {
    MemoryId memory_id;
    std::string type;
    std::string content;
    std::string created_at;
    
    Reflection(MemoryId mem_id, const std::string& t, const std::string& c);
};

// ============================================================================
// Classe IScoreCalculator
// ============================================================================
class IScoreCalculator {
    public:
        virtual ~IScoreCalculator() = default;
        virtual void calculate(const std::string& text, std::unordered_map<std::string, double>& scores) = 0;
};

class LexiconScoreCalculator : public IScoreCalculator {
    private:
        const std::unordered_map<std::string, std::vector<std::string>>& lexicons_;
        const std::unordered_map<std::string, double>& weights_;

    public:
        LexiconScoreCalculator(
            const std::unordered_map<std::string, std::vector<std::string>>& lexicons,
            const std::unordered_map<std::string, double>& weights)
            : lexicons_(lexicons), weights_(weights) {}
        
            void calculate(const std::string& text,
                            std::unordered_map<std::string, double>& scores) override;
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
    std::vector<std::unique_ptr<IScoreCalculator>> score_calculators_;
    
    EmotionalAnalyzerConfig config_;

    void setupScoreCalculators();
    void initializeEmotionLexicons();
    void analyzeWithLexicon(const std::string& text, std::unordered_map<std::string, double>& scores) const;
    void analyzePatterns(const std::string& text, std::unordered_map<std::string, double>& scores) const;
    void analyzePunctuation(const std::string& text, std::unordered_map<std::string, double>& scores) const;
    std::vector<float> normalizeScores(const std::unordered_map<std::string, double>& scores) const;
    std::string findDominantEmotion(const std::unordered_map<std::string, double>& scores) const;
    double calculateConfidence(const std::unordered_map<std::string, double>& scores) const;

public:
    explicit EmotionalAnalyzer(const EmotionalAnalyzerConfig& config = {});
    
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
// Classe TextNormalizer
// ============================================================================

class TextNormalizer {
    public:
        static std::string toLowerCase(const std::string& text);        
        static std::string removeExtraSpaces(const std::string& text);
        static std::string normalizePunctuation(const std::string& text);
        static std::string normalizePortugueseAccents(const std::string& text);
        static std::vector<std::string> extractWords(const std::string& text);

    private:
        static const std::unordered_map<char, char> accent_map;
};

// ============================================================================
// Classe SQLStatementBuilder
// ============================================================================
class SQLStatementBuilder {
public:
    static std::string createMemoryInsert(const std::string& content, 
                                         const std::string& context,
                                         const std::string& emotion,
                                         double importance,
                                         uint64_t timestamp,
                                         const std::string& vtime);
    
    static std::string createEmbeddingInsert(MemoryId memory_id,
                                            const std::vector<float>& embedding,
                                            const std::string& created_at);
    
    static std::string createSemanticSearchQuery();
    static std::string createContextualSearchQuery(const std::string& pattern, int limit);
    
private:
    static std::string escapeSQLString(const std::string& input);
};

namespace alyssa_memory {
    class SQLiteWrapper {
        private:
            sqlite3* db_ = nullptr;
            
        public:
            explicit SQLiteWrapper(const std::string& db_path) {
                int rc = sqlite3_open_v2(db_path.c_str(), &db_, 
                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
                if (rc != SQLITE_OK) {
                    throw std::runtime_error("Cannot open database: " + 
                        std::string(sqlite3_errmsg(db_)));
                }
            }
            
            ~SQLiteWrapper() {
                if (db_) {
                    sqlite3_close(db_);
                }
            }
            
            // Delete copy operations
            SQLiteWrapper(const SQLiteWrapper&) = delete;
            SQLiteWrapper& operator=(const SQLiteWrapper&) = delete;
            
            // Allow move
            SQLiteWrapper(SQLiteWrapper&& other) noexcept : db_(other.db_) {
                other.db_ = nullptr;
            };
            
            sqlite3* get() const { return db_; };
            operator sqlite3*() const { return db_; };
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
        
        // TODO: Make A Neural Network in ONNX for better results
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
        
        void linkMemoryToIntention(int memory_id, int intention_id);
        
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
            void createMemoryLink(MemoryId source_id, MemoryId target_id, double weight = 1.0, 
                const std::string& link_type = "association");
                std::vector<MemoryLink> getMemoryLinks(MemoryId memory_id);
                
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
                MemorySearchConfig search_config_;
                
                void printEmotionalAnalysis(const EmotionalAnalysis& analysis, const std::string& text);
                void analyzeInputForIntentions(const std::string& input);
            
            public:
                void setSearchConfig(const MemorySearchConfig& config) { search_config_ = config; }
                const MemorySearchConfig& getSearchConfig() const { return search_config_; }

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
                
                EmotionalState getCurrentEmotionalState() const;
                std::vector<Intention> getActiveIntentions() const;
                
                int storeMemoryWithEmotionalAnalysis(const std::string& content, const std::string& context);
                void linkMemoryToIntention(int memory_id, int intention_id);
                
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
        }