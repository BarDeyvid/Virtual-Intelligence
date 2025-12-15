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

/**
 * @struct EmotionalAnalyzerConfig
 * @brief Configuration structure for emotional analyzer settings
 * 
 * This struct contains configuration parameters for the emotional analysis system,
 * including emotion weights, scoring parameters, and conversation weighting.
 */
struct EmotionalAnalyzerConfig {
    /**
     * @struct EmotionWeight
     * @brief Pair structure for emotion-weight combinations
     */
    struct EmotionWeight {
        std::string emotion;  ///< Emotion name
        double weight;        ///< Weight value for the emotion
    };

    std::vector<EmotionWeight> default_weights = {  ///< Default emotion weights
        {"alegria", 1.0}, {"tristeza", 1.0}, {"raiva", 1.0}, 
        {"medo", 1.0}, {"surpresa", 0.8}, {"confiança", 0.9},
        {"antecipacao", 0.7}, {"desgosto", 0.9}
    };

    std::vector<std::string> emotion_categories = {  ///< Available emotion categories
        "alegria", "tristeza", "raiva", "medo", 
        "surpresa", "confiança", "antecipacao", "desgosto"
    };

    double lexicon_word_weight = 0.1;       ///< Weight for lexicon-based word matching
    double intesifier_boost = 0.15;         ///< Boost value for intensifier words
    double negation_penalty = 0.1;          ///< Penalty for negation words
    double exclamation_boost = 0.05;        ///< Boost for exclamation marks
    double question_boost = 0.08;           ///< Boost for question marks
    double period_frustation_boost = 0.05;  ///< Boost for multiple periods (frustration)
    double min_confidence_threshold = 0.1;  ///< Minimum confidence threshold for emotion detection

    double user_input_weight = 0.7;         ///< Weight for user input in conversation analysis
    double ai_response_weight = 0.3;        ///< Weight for AI response in conversation analysis
};

/**
 * @struct MemorySearchConfig
 * @brief Configuration structure for memory search settings
 * 
 * Contains parameters for hybrid search (text + semantic) and result boosting.
 */
struct MemorySearchConfig {
    int default_top_k = 5;                  ///< Default number of top results to return
    double text_weight = 0.4;               ///< Weight for textual similarity in hybrid search
    double semantic_weight = 0.6;           ///< Weight for semantic similarity in hybrid search
    double intention_boost_weight = 0.3;    ///< Boost weight for intention matching
    double emotional_boost_weight = 0.2;    ///< Boost weight for emotional matching
    double min_similarity_score = 0.1;      ///< Minimum similarity score threshold

    /**
     * @struct BoostConfig
     * @brief Nested structure for boosting configuration
     */
    struct BoostConfig {
        double active_intention_multiplier = 0.3;  ///< Multiplier for active intentions
        double emotional_match_multiplier = 0.2;   ///< Multiplier for emotional matches
        double recent_memory_boost = 0.1;          ///< Boost for recent memories
    } boost_config;
};


// ============================================================================
// Estruturas de Dados
// ============================================================================

/**
 * @struct EmotionalAnalysis
 * @brief Structure for emotional analysis results of text
 * 
 * Contains vector representation, dominant emotion, confidence score,
 * and individual emotion scores.
 */
struct EmotionalAnalysis {
    std::vector<float> emotional_vector;            ///< Vector representation of emotional state
    std::string dominant_emotion;                   ///< Dominant emotion detected
    double confidence;                              ///< Confidence score of the analysis
    std::unordered_map<std::string, double> emotion_scores;  ///< Individual emotion scores
};

/**
 * @struct EmotionalState
 * @brief Structure for representing emotional state with timestamp
 * 
 * Tracks current emotional state with intensity and timestamp.
 */
struct EmotionalState {
    std::string name;       ///< Name of the emotional state
    double intensity;       ///< Intensity of the emotion (0.0-1.0)
    uint64_t timestamp;     ///< Timestamp of when this state was recorded
    
    /**
     * @brief Constructor for EmotionalState
     * @param n Emotion name (default: "neutral")
     * @param i Emotion intensity (default: 0.5)
     */
    EmotionalState(const std::string& n = "neutral", double i = 0.5);
};

/**
 * @struct MemoryId
 * @brief Strongly-typed ID structure for memory identification
 * 
 * Provides type safety for memory IDs with validation methods.
 */
struct MemoryId {
    int value;  ///< Integer value of the memory ID
    
    /**
     * @brief Constructor for MemoryId
     * @param v Integer value for the ID
     */
    explicit constexpr MemoryId(int v) : value(v) {}
    
    /// Equality operator
    bool operator==(const MemoryId& other) const {return value == other.value;}
    /// Inequality operator
    bool operator!=(const MemoryId& other) const {return value != other.value;}
    /// Conversion operator to int
    operator int() const {return value;}
    
    /**
     * @brief Get invalid MemoryId instance
     * @return MemoryId with value -1
     */
    static constexpr MemoryId invalid() {return MemoryId(-1);}
    
    /**
     * @brief Check if MemoryId is valid
     * @return true if value > 0, false otherwise
     */
    bool isValid() const {return value > 0;}
};

/**
 * @struct IntentionId
 * @brief Strongly-typed ID structure for intention identification
 * 
 * Provides type safety for intention IDs with validation methods.
 */
struct IntentionId {
    int value;  ///< Integer value of the intention ID
    
    /**
     * @brief Constructor for IntentionId
     * @param v Integer value for the ID
     */
    explicit constexpr IntentionId(int v) : value(v) {}
    
    /// Equality operator
    bool operator==(const IntentionId& other) const {return value == other.value;}
    /// Inequality operator
    bool operator!=(const IntentionId& other) const {return value != other.value;}
    /// Conversion operator to int
    operator int() const {return value;}
    
    /**
     * @brief Get invalid IntentionId instance
     * @return IntentionId with value -1
     */
    static constexpr IntentionId invalid() {return IntentionId(-1);}
    
    /**
     * @brief Check if IntentionId is valid
     * @return true if value > 0, false otherwise
     */
    bool isValid() const {return value > 0;}
};

/**
 * @struct Intention
 * @brief Structure for representing system intentions
 * 
 * Represents goals or purposes that guide the AI's behavior.
 */
struct Intention {
    int id;                     ///< Unique identifier for the intention
    std::string description;    ///< Text description of the intention
    std::string type;           ///< Type/category of intention
    std::string trigger;        ///< What triggered this intention
    bool active;                ///< Whether the intention is currently active
    double motivation;          ///< Motivation level (0.0-1.0)
    std::string created_at;     ///< Timestamp of creation
    
    /**
     * @brief Constructor for Intention
     * @param desc Description text
     * @param t Type/category
     * @param trig Trigger text (optional)
     */
    Intention(const std::string& desc, const std::string& t, const std::string& trig = "");
};

/**
 * @struct MemoryLink
 * @brief Structure for links between memories
 * 
 * Represents associative connections between different memories.
 */
struct MemoryLink {
    MemoryId source_id;     ///< Source memory ID
    MemoryId target_id;     ///< Target memory ID
    double weight;          ///< Strength of the connection (0.0-1.0)
    std::string type;       ///< Type of link (e.g., "association")
    
    /**
     * @brief Constructor for MemoryLink
     * @param src Source memory ID
     * @param tgt Target memory ID
     * @param w Link weight (default: 1.0)
     * @param t Link type (default: "association")
     */
    MemoryLink(MemoryId src, MemoryId tgt, double w = 1.0, const std::string& t = "association");
};

/**
 * @struct Reflection
 * @brief Structure for system-generated reflections
 * 
 * Represents insights or observations generated by the system about memories.
 */
struct Reflection {
    MemoryId memory_id;     ///< ID of the memory being reflected upon
    std::string type;       ///< Type of reflection
    std::string content;    ///< Content of the reflection
    std::string created_at; ///< Timestamp of creation
    
    /**
     * @brief Constructor for Reflection
     * @param mem_id Memory ID
     * @param t Reflection type
     * @param c Reflection content
     */
    Reflection(MemoryId mem_id, const std::string& t, const std::string& c);
};

// ============================================================================
// Classe IScoreCalculator
// ============================================================================

/**
 * @class IScoreCalculator
 * @brief Interface for emotion score calculation
 * 
 * Abstract base class defining the interface for different emotion scoring strategies.
 */
class IScoreCalculator {
public:
    /**
     * @brief Virtual destructor for proper polymorphism
     */
    virtual ~IScoreCalculator() = default;
    
    /**
     * @brief Calculate emotion scores for given text
     * @param text Input text to analyze
     * @param scores Output map of emotion scores (will be filled)
     */
    virtual void calculate(const std::string& text, std::unordered_map<std::string, double>& scores) = 0;
};

/**
 * @class LexiconScoreCalculator
 * @brief Lexicon-based emotion score calculator
 * 
 * Calculates emotion scores based on word matching against emotion lexicons.
 */
class LexiconScoreCalculator : public IScoreCalculator {
private:
    const std::unordered_map<std::string, std::vector<std::string>>& lexicons_;  ///< Emotion lexicons
    const std::unordered_map<std::string, double>& weights_;                     ///< Emotion weights

public:
    /**
     * @brief Constructor for LexiconScoreCalculator
     * @param lexicons Reference to emotion lexicons
     * @param weights Reference to emotion weights
     */
    LexiconScoreCalculator(
        const std::unordered_map<std::string, std::vector<std::string>>& lexicons,
        const std::unordered_map<std::string, double>& weights)
        : lexicons_(lexicons), weights_(weights) {}
    
    /**
     * @brief Calculate emotion scores using lexicon matching
     * @param text Input text to analyze
     * @param scores Output map of emotion scores (will be filled)
     */
    void calculate(const std::string& text,
                   std::unordered_map<std::string, double>& scores) override;
};

// ============================================================================
// Classe EmotionalAnalyzer
// ============================================================================

/**
 * @class EmotionalAnalyzer
 * @brief Emotional analyzer for Portuguese text based on lexicon
 * 
 * Performs emotional analysis on text using lexicon matching, pattern analysis,
 * and punctuation analysis to detect and quantify emotions.
 */
class EmotionalAnalyzer {
private:
    std::unordered_map<std::string, std::vector<std::string>> emotion_lexicons;  ///< Emotion word lexicons
    std::unordered_map<std::string, double> emotion_weights;                     ///< Emotion category weights
    std::vector<std::string> emotion_categories;                                 ///< Available emotion categories
    std::vector<std::unique_ptr<IScoreCalculator>> score_calculators_;           ///< Score calculation strategies
    
    EmotionalAnalyzerConfig config_;  ///< Configuration settings

    /**
     * @brief Set up score calculators based on configuration
     */
    void setupScoreCalculators();
    
    /**
     * @brief Initialize emotion lexicons from loader
     */
    void initializeEmotionLexicons();
    
    /**
     * @brief Analyze text using lexicon matching
     * @param text Input text to analyze
     * @param scores Output scores (will be modified)
     */
    void analyzeWithLexicon(const std::string& text, std::unordered_map<std::string, double>& scores) const;
    
    /**
     * @brief Analyze patterns like intensifiers and negations
     * @param text Input text to analyze
     * @param scores Output scores (will be modified)
     */
    void analyzePatterns(const std::string& text, std::unordered_map<std::string, double>& scores) const;
    
    /**
     * @brief Analyze punctuation for emotional cues
     * @param text Input text to analyze
     * @param scores Output scores (will be modified)
     */
    void analyzePunctuation(const std::string& text, std::unordered_map<std::string, double>& scores) const;
    
    /**
     * @brief Normalize emotion scores to vector format
     * @param scores Raw emotion scores
     * @return Normalized vector of emotion scores
     */
    std::vector<float> normalizeScores(const std::unordered_map<std::string, double>& scores) const;
    
    /**
     * @brief Find dominant emotion from scores
     * @param scores Emotion scores
     * @return Name of dominant emotion
     */
    std::string findDominantEmotion(const std::unordered_map<std::string, double>& scores) const;
    
    /**
     * @brief Calculate confidence score for analysis
     * @param scores Emotion scores
     * @return Confidence value (0.0-1.0)
     */
    double calculateConfidence(const std::unordered_map<std::string, double>& scores) const;

public:
    /**
     * @brief Constructor for EmotionalAnalyzer
     * @param config Configuration settings (optional)
     */
    explicit EmotionalAnalyzer(const EmotionalAnalyzerConfig& config = {});
    
    /**
     * @brief Analyze emotional content of text
     * @param text Input text to analyze
     * @return EmotionalAnalysis structure with results
     */
    EmotionalAnalysis analyzeText(const std::string& text);
    
    /**
     * @brief Analyze emotional content of conversation
     * @param user_input User input text
     * @param ai_response AI response text
     * @return Combined emotional analysis
     */
    EmotionalAnalysis analyzeConversation(const std::string& user_input, const std::string& ai_response);
};

// ============================================================================
// Embedding
// ============================================================================

// Forward declaration of Embedder class
class Embedder;

// ============================================================================
// Classe Utils
// ============================================================================

/**
 * @class Utils
 * @brief Utility functions for memory system
 * 
 * Provides common utility functions used throughout the memory system.
 */
class Utils {
public:
    /**
     * @brief Get timestamp as cyclical vector (for time encoding)
     * @return Vector with sine/cosine representation of current hour
     */
    static std::vector<float> getTimeStamp();
    
    /**
     * @brief Get current time in ISO 8601 format
     * @return ISO timestamp string
     */
    static std::string getCurrentISOTime();
    
    /**
     * @brief Sigmoid activation function
     * @param x Input value
     * @return Sigmoid of x
     */
    static double sigmoid(double x);
    
    /**
     * @brief Normalize importance score to [0, 1] range
     * @param raw_importance Raw importance value
     * @return Normalized importance (clamped to 0.0-1.0)
     */
    static double normalizeImportance(double raw_importance);
};

// ============================================================================
// Classe TextNormalizer
// ============================================================================

/**
 * @class TextNormalizer
 * @brief Text normalization utilities
 * 
 * Provides functions for normalizing text including case conversion,
 * accent removal, and punctuation normalization.
 */
class TextNormalizer {
public:
    /**
     * @brief Convert text to lowercase
     * @param text Input text
     * @return Lowercase version of text
     */
    static std::string toLowerCase(const std::string& text);        
    
    /**
     * @brief Remove extra whitespace from text
     * @param text Input text
     * @return Text with single spaces between words
     */
    static std::string removeExtraSpaces(const std::string& text);
    
    /**
     * @brief Normalize punctuation spacing
     * @param text Input text
     * @return Text with normalized punctuation spacing
     */
    static std::string normalizePunctuation(const std::string& text);
    
    /**
     * @brief Remove Portuguese accents from text
     * @param text Input text
     * @return Text with accents converted to base characters
     */
    static std::string normalizePortugueseAccents(const std::string& text);
    
    /**
     * @brief Extract words from text with normalization
     * @param text Input text
     * @return Vector of extracted words
     */
    static std::vector<std::string> extractWords(const std::string& text);

private:
    static const std::unordered_map<char, char> accent_map;  ///< Map for accent conversion
};

// ============================================================================
// Classe SQLStatementBuilder
// ============================================================================

/**
 * @class SQLStatementBuilder
 * @brief SQL statement generation utilities
 * 
 * Provides static methods for creating SQL statements with proper escaping.
 */
class SQLStatementBuilder {
public:
    /**
     * @brief Create SQL INSERT statement for memories
     * @param content Memory content
     * @param context Memory context
     * @param emotion Detected emotion
     * @param importance Importance score
     * @param timestamp Unix timestamp
     * @param vtime Vector time representation
     * @return SQL INSERT statement string
     */
    static std::string createMemoryInsert(const std::string& content, 
                                         const std::string& context,
                                         const std::string& emotion,
                                         double importance,
                                         uint64_t timestamp,
                                         const std::string& vtime);
    
    /**
     * @brief Create SQL INSERT statement for embeddings
     * @param memory_id Memory ID
     * @param embedding Vector embedding
     * @param created_at Creation timestamp
     * @return SQL INSERT statement string
     */
    static std::string createEmbeddingInsert(MemoryId memory_id,
                                            const std::vector<float>& embedding,
                                            const std::string& created_at);
    
    /**
     * @brief Create SQL query for semantic search
     * @return SQL SELECT statement for semantic search
     */
    static std::string createSemanticSearchQuery();
    
    /**
     * @brief Create SQL query for contextual search
     * @param pattern Search pattern for LIKE operator
     * @param limit Result limit
     * @return SQL SELECT statement for contextual search
     */
    static std::string createContextualSearchQuery(const std::string& pattern, int limit);
    
private:
    /**
     * @brief Escape SQL string to prevent injection
     * @param input Input string
     * @return Escaped SQL string
     */
    static std::string escapeSQLString(const std::string& input);
};

namespace alyssa_memory {
    /**
     * @class SQLiteWrapper
     * @brief RAII wrapper for SQLite3 database connection
     * 
     * Manages SQLite database connection lifecycle with proper cleanup.
     */
    class SQLiteWrapper {
    private:
        sqlite3* db_ = nullptr;  ///< SQLite database handle
        
    public:
        /**
         * @brief Constructor with database path
         * @param db_path Path to SQLite database file
         * @throws std::runtime_error if database cannot be opened
         */
        explicit SQLiteWrapper(const std::string& db_path) {
            int rc = sqlite3_open_v2(db_path.c_str(), &db_, 
                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
            if (rc != SQLITE_OK) {
                throw std::runtime_error("Cannot open database: " + 
                    std::string(sqlite3_errmsg(db_)));
            }
        }
        
        /**
         * @brief Destructor - closes database connection
         */
        ~SQLiteWrapper() {
            if (db_) {
                sqlite3_close(db_);
            }
        }
        
        // Delete copy operations
        SQLiteWrapper(const SQLiteWrapper&) = delete;
        SQLiteWrapper& operator=(const SQLiteWrapper&) = delete;
        
        /**
         * @brief Move constructor
         * @param other SQLiteWrapper to move from
         */
        SQLiteWrapper(SQLiteWrapper&& other) noexcept : db_(other.db_) {
            other.db_ = nullptr;
        };
        
        /**
         * @brief Get raw SQLite database pointer
         * @return sqlite3* pointer
         */
        sqlite3* get() const { return db_; };
        
        /**
         * @brief Conversion operator to sqlite3*
         * @return sqlite3* pointer
         */
        operator sqlite3*() const { return db_; };
    };
    
    // ============================================================================
    // Classe AdvancedMemorySystem
    // ============================================================================
    
    /**
     * @class AdvancedMemorySystem
     * @brief Advanced memory system with vector embeddings and emotional analysis
     * 
     * Core system for storing, retrieving, and analyzing memories with emotional
     * context and semantic search capabilities.
     */
    class AdvancedMemorySystem {
    private:
        sqlite3* db;                                ///< SQLite database connection
        EmotionalState current_emotional_state;     ///< Current emotional state
        std::vector<Intention> active_intentions;   ///< Active intentions
        std::map<std::string, double> emotion_weights;  ///< Emotion weighting factors
        std::shared_ptr<Embedder> embedder;         ///< Embedding generator
        std::unique_ptr<EmotionalAnalyzer> emotional_analyzer;  ///< Emotional analyzer
        
        // TODO: Make A Neural Network in ONNX for better results
        /**
         * @brief Mapping from emotions to corresponding intentions
         */
        std::unordered_map<std::string, std::pair<std::string, std::string>> emotion_to_intention = {
            {"tristeza", {"Refletir sobre sentimentos difíceis", "emocional"}},
            {"alegria", {"Manter momentos positivos", "emocional"}},
            {"raiva", {"Redirecionar frustrações", "emocional"}},
            {"medo", {"Buscar segurança", "emocional"}},
            {"surpresa", {"Explorar novidade", "emocional"}},
            {"confiança", {"Apoiar outros", "emocional"}},
            {"antecipacao", {"Planejar próximos passos", "planejamento"}}
        };
        
        /**
         * @brief Initialize database tables
         * @throws std::runtime_error if table creation fails
         */
        void initializeDatabase();
        
        /**
         * @brief Load current emotional state from database
         */
        void loadCurrentState();
        
        /**
         * @brief Save current emotional state to database
         */
        void saveCurrentState();
        
        /**
         * @brief Load active intentions from database
         */
        void loadActiveIntentions();
        
        /**
         * @brief Store embedding vector for memory
         * @param memory_id Memory ID
         * @param embedding Vector embedding
         */
        void storeEmbedding(int memory_id, const std::vector<float>& embedding);
        
        /**
         * @brief Check for automatic intention activation based on emotions
         */
        void checkEmotionalAutoActivation();
        
        /**
         * @brief Save reflection to database
         * @param memory_id Memory ID being reflected upon
         * @param type Type of reflection
         * @param content Reflection content
         */
        void saveReflection(int memory_id, const std::string& type, const std::string& content);
        
        /**
         * @brief Calculate intention boost for memory relevance
         * @param content Memory content
         * @return Boost score (0.0-0.5)
         */
        double calculateIntentionBoost(const std::string& content);
        
        /**
         * @brief Calculate emotional boost for memory relevance
         * @param memory_emotion Emotion associated with memory
         * @return Boost score based on emotional match
         */
        double calculateEmotionalBoost(const std::string& memory_emotion);
        
        /**
         * @brief Calculate automatic importance score for memory
         * @param content Memory content
         * @param analysis Emotional analysis results
         * @return Importance score (0.0-1.0)
         */
        double calculateAutomaticImportance(const std::string& content, const EmotionalAnalysis& analysis);
        
    public:
        /**
         * @struct ContextualMemory
         * @brief Result structure for contextual memory search
         */
        struct ContextualMemory {
            int id;                 ///< Memory ID
            std::string content;    ///< Memory content
            double relevance_score; ///< Relevance score
            std::string emotion;    ///< Associated emotion
        };
        
        /**
         * @struct SemanticMemory
         * @brief Result structure for semantic memory search
         */
        struct SemanticMemory {
            int memory_id;          ///< Memory ID
            std::string content;    ///< Memory content
            double similarity_score; ///< Semantic similarity score
            std::string emotion;    ///< Associated emotion
            double importance;      ///< Memory importance
        };
        
        /**
         * @struct HybridMemoryResult
         * @brief Result structure for hybrid (text + semantic) search
         */
        struct HybridMemoryResult {
            int memory_id;          ///< Memory ID
            std::string content;    ///< Memory content
            double text_score;      ///< Text similarity score
            double semantic_score;  ///< Semantic similarity score
            double combined_score;  ///< Combined hybrid score
            std::string emotion;    ///< Associated emotion
        };
        
        /**
         * @brief Constructor for AdvancedMemorySystem
         * @param db_path Path to SQLite database file
         * @param embedder_ref Shared pointer to Embedder instance
         * @throws std::runtime_error if database cannot be opened
         */
        AdvancedMemorySystem(const std::string& db_path, std::shared_ptr<Embedder> embedder_ref);
        
        /**
         * @brief Destructor - saves state and closes database
         */
        ~AdvancedMemorySystem();
        
        /**
         * @brief Set current emotional state
         * @param emotion Emotion name
         * @param intensity Emotion intensity (0.0-1.0)
         */
        void setEmotionalState(const std::string& emotion, double intensity);
        
        /**
         * @brief Get current emotional state
         * @return Current EmotionalState structure
         */
        EmotionalState getCurrentEmotionalState() const;
        
        /**
         * @brief Link memory to intention
         * @param memory_id Memory ID
         * @param intention_id Intention ID
         */
        void linkMemoryToIntention(int memory_id, int intention_id);
        
        /**
         * @brief Store memory with automatic emotional analysis
         * @param content Memory content
         * @param context Memory context (optional)
         * @return Memory ID if successful, -1 on error
         */
        int storeMemoryWithEmotionalAnalysis(const std::string& content, const std::string& context = "");
        
        /**
         * @brief Apply memory decay over time
         */
        void applyMemoryDecay();
        
        /**
         * @brief Activate new intention
         * @param description Intention description
         * @param type Intention type/category
         * @param trigger Trigger text (optional)
         * @param motivation_boost Additional motivation boost (optional)
         */
        void activateIntention(const std::string& description, const std::string& type, 
            const std::string& trigger = "", double motivation_boost = 0.0);
        
        /**
         * @brief Deactivate intention by ID
         * @param intention_id Intention ID to deactivate
         */
        void deactivateIntention(int intention_id);
        
        /**
         * @brief Get active intentions
         * @return Vector of active Intention structures
         */
        std::vector<Intention> getActiveIntentions() const;
        
        /**
         * @brief Generate reflections based on recent memories
         */
        void generateReflections();
        
        /**
         * @brief Create link between memories
         * @param source_id Source memory ID
         * @param target_id Target memory ID
         * @param weight Link weight (default: 1.0)
         * @param link_type Link type (default: "association")
         */
        void createMemoryLink(MemoryId source_id, MemoryId target_id, double weight = 1.0, 
            const std::string& link_type = "association");
        
        /**
         * @brief Get links associated with memory
         * @param memory_id Memory ID
         * @return Vector of MemoryLink structures
         */
        std::vector<MemoryLink> getMemoryLinks(MemoryId memory_id);
        
        /**
         * @brief Search memories contextually (text-based)
         * @param query Search query text
         * @param top_k Number of results to return (default: 5)
         * @return Vector of ContextualMemory results
         */
        std::vector<ContextualMemory> searchContextualMemories(const std::string& query, int top_k = 5);
        
        /**
         * @brief Search memories semantically (embedding-based)
         * @param query Search query text
         * @param top_k Number of results to return (default: 5)
         * @return Vector of SemanticMemory results
         */
        std::vector<SemanticMemory> semanticSearch(const std::string& query, int top_k = 5);
        
        /**
         * @brief Search memories with pre-computed embedding
         * @param query_embedding Query embedding vector
         * @param top_k Number of results to return (default: 5)
         * @return Vector of SemanticMemory results
         */
        std::vector<SemanticMemory> semanticSearchWithEmbedding(const std::vector<float>& query_embedding, int top_k = 5);
        
        /**
         * @brief Hybrid search (text + semantic)
         * @param query Search query text
         * @param top_k Number of results to return (default: 5)
         * @return Vector of HybridMemoryResult structures
         */
        std::vector<HybridMemoryResult> hybridSearch(const std::string& query, int top_k = 5);
        
        /**
         * @brief Check if embedder is available
         * @return true if embedder is initialized and ready
         */
        bool hasEmbedder() const;
        
        /**
         * @brief Generate and store embedding for memory
         * @param memory_id Memory ID
         * @param content Memory content to embed
         * @return true if successful, false otherwise
         */
        bool generateAndStoreEmbedding(int memory_id, const std::string& content);
        
        /**
         * @brief Analyze emotional content of text
         * @param text Text to analyze
         * @return EmotionalAnalysis structure with results
         */
        EmotionalAnalysis analyzeEmotionalContent(const std::string& text);
        
        /**
         * @brief Analyze emotions in conversation
         * @param user_input User input text
         * @param ai_response AI response text
         * @return Combined emotional analysis
         */
        EmotionalAnalysis analyzeConversationEmotions(const std::string& user_input, const std::string& ai_response);
        
        /**
         * @brief Get automatic emotional vector for text
         * @param text Text to analyze
         * @return Emotional vector (normalized)
         */
        std::vector<float> getAutoEmotionalVector(const std::string& text);
        
        /**
         * @brief Print system status to console
         */
        void printSystemStatus();
        
        /**
         * @brief Demonstrate semantic search with debug output
         * @param query Search query for demonstration
         */
        void demonstrateSemanticSearch(const std::string& query);
    };
    
    // ============================================================================
    // Classe AlyssaMemoryManager
    // ============================================================================
    
    /**
     * @class AlyssaMemoryManager
     * @brief High-level manager for conversational AI memory integration
     * 
     * Provides simplified interface for integrating memory system with
     * conversational AI, handling emotional analysis, intention management,
     * and memory retrieval.
     */
    class AlyssaMemoryManager {
    private:
        std::unique_ptr<AdvancedMemorySystem> memory_system;  ///< Underlying memory system
        MemorySearchConfig search_config_;                     ///< Search configuration
        
        /**
         * @brief Print emotional analysis results to console
         * @param analysis EmotionalAnalysis results
         * @param text Original text analyzed
         */
        void printEmotionalAnalysis(const EmotionalAnalysis& analysis, const std::string& text);
        
        /**
         * @brief Analyze input text for intention triggers
         * @param input User input text
         */
        void analyzeInputForIntentions(const std::string& input);
    
    public:
        /**
         * @brief Set search configuration
         * @param config MemorySearchConfig structure
         */
        void setSearchConfig(const MemorySearchConfig& config) { search_config_ = config; }
        
        /**
         * @brief Get current search configuration
         * @return Current MemorySearchConfig
         */
        const MemorySearchConfig& getSearchConfig() const { return search_config_; }

        /**
         * @brief Constructor for AlyssaMemoryManager
         * @param db_path Path to SQLite database file
         * @param embedder_ref Shared pointer to Embedder instance
         */
        AlyssaMemoryManager(const std::string& db_path, std::shared_ptr<Embedder> embedder_ref);    
        
        /**
         * @brief Process interaction with automatic emotional analysis
         * @param user_input User input text
         * @param ai_response AI response text
         */
        void processInteraction(const std::string& user_input, const std::string& ai_response);
        
        /**
         * @brief Process interaction with provided emotional vector (legacy)
         * @param user_input User input text
         * @param ai_response AI response text
         * @param emotional_vector Pre-computed emotional vector
         */
        void processInteraction(const std::string& user_input, 
                                const std::string& ai_response,
                                const std::vector<float>& emotional_vector);
        
        /**
         * @brief Get relevant memories for context
         * @param context Context string for search
         * @return Vector of ContextualMemory results
         */
        std::vector<AdvancedMemorySystem::ContextualMemory> getRelevantMemories(const std::string& context);
        
        /**
         * @brief Get semantic memories for context
         * @param context Context string for search
         * @return Vector of SemanticMemory results
         */
        std::vector<AdvancedMemorySystem::SemanticMemory> getSemanticMemories(const std::string& context);
        
        /**
         * @brief Get hybrid search results for context
         * @param context Context string for search
         * @return Vector of HybridMemoryResult structures
         */
        std::vector<AdvancedMemorySystem::HybridMemoryResult> getHybridMemories(const std::string& context);
        
        /**
         * @brief Get current emotional state
         * @return Current EmotionalState structure
         */
        EmotionalState getCurrentEmotionalState() const;
        
        /**
         * @brief Get active intentions
         * @return Vector of active Intention structures
         */
        std::vector<Intention> getActiveIntentions() const;
        
        /**
         * @brief Store memory with emotional analysis
         * @param content Memory content
         * @param context Memory context
         * @return Memory ID if successful, -1 on error
         */
        int storeMemoryWithEmotionalAnalysis(const std::string& content, const std::string& context);
        
        /**
         * @brief Link memory to intention
         * @param memory_id Memory ID
         * @param intention_id Intention ID
         */
        void linkMemoryToIntention(int memory_id, int intention_id);
        
        /**
         * @brief Set current goal/intention
         * @param goal Goal description
         * @param type Goal type (default: "learning")
         */
        void setCurrentGoal(const std::string& goal, const std::string& type = "learning");
        
        /**
         * @brief Process identity fact for user modeling
         * @param fact_value Fact value
         * @param fact_type Fact type/category
         */
        void processIdentityFact(const std::string& fact_value, const std::string& fact_type);
    };
    
    // ============================================================================
    // Constantes
    // ============================================================================
    
    constexpr int VTIME_DIM = 2;                 ///< Dimension of vector time representation
    constexpr size_t MAX_TOKENS = 38216;         ///< Maximum tokens for embedding
    constexpr double DECAY_PER_HOUR = 0.01;      ///< Memory decay rate per hour
    constexpr double MIN_IMPORTANCE = 0.3;       ///< Minimum importance threshold
    constexpr double HARD_LOCK_IMPORTANCE = 0.9; ///< Importance level for hard-locked memories
}