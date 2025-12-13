#ifndef EMBEDDER_H
#define EMBEDDER_H

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <tuple>
#include <stdexcept>
#include <fstream>
#include <memory>
#include <filesystem>
#include <thread>
#include <cstring>
#include "json.hpp"

// Includes para llama.cpp
#include "llama.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

/**
 * @class Embedder
 * @brief Main class for generating text embeddings using llama.cpp models.
 * 
 * This class provides functionality to load embedding models, generate embeddings
 * for text inputs, compute similarity metrics, and perform semantic search.
 * It supports various configuration options and batch processing for efficiency.
 */
class Embedder {
public:
    /**
     * @struct Config
     * @brief Configuration structure for Embedder initialization.
     */
    struct Config {
        std::string model_path = "models/ggml-model.gguf";  ///< Path to the model file
        int embedding_dimension = 4096;                     ///< Dimension of output embeddings
        
        int32_t seed = -1;              ///< Random seed (-1 for random)
        int32_t n_ctx = 8192;           ///< Context window size
        int32_t n_batch = 8192;         ///< Batch size for processing
        int32_t n_threads = 0;          ///< Number of threads (0 = auto)
        int32_t n_gpu_layers = 0;       ///< Number of GPU layers to use
        bool use_mlock = false;         ///< Lock model in memory
        bool use_mmap = true;           ///< Use memory mapping
        bool f16_kv = true;             ///< Use FP16 for KV cache
        int embd_normalize = 2;         ///< Embedding normalization mode (0=none, 1=L2, 2=scaled)
    };

    /**
     * @brief Default constructor.
     * Initializes llama.cpp backend.
     */
    Embedder();
    
    /**
     * @brief Constructor with configuration file path.
     * @param config_path Path to JSON configuration file.
     */
    explicit Embedder(const std::string& config_path);
    
    /**
     * @brief Destructor.
     * Cleans up llama.cpp resources.
     */
    ~Embedder();

    /**
     * @brief Initialize the embedder with default config path.
     * @return true if initialization successful, false otherwise.
     */
    bool initialize();
    
    /**
     * @brief Initialize the embedder with specified config file.
     * @param config_path Path to configuration file.
     * @return true if initialization successful, false otherwise.
     */
    bool initialize(const std::string& config_path);
    
    /**
     * @brief Generate embedding for a single text.
     * @param text Input text to embed.
     * @return Vector of floats representing the embedding.
     * @throws std::runtime_error if embedder not initialized.
     */
    std::vector<float> generate_embedding(const std::string& text);
    
    /**
     * @brief Generate embeddings for multiple texts.
     * @param texts Vector of input texts.
     * @return Vector of embedding vectors.
     * @throws std::runtime_error if embedder not initialized.
     */
    std::vector<std::vector<float>> generate_embeddings(const std::vector<std::string>& texts);
    
    /**
     * @brief Clear the embedding cache.
     * Useful for freeing memory between batches.
     */
    void clear_embedding_cache();
    
    /**
     * @brief Compute cosine similarity between two embeddings.
     * @param a First embedding vector.
     * @param b Second embedding vector.
     * @return Cosine similarity score between -1 and 1.
     */
    static double cosine_similarity(const std::vector<float>& a, const std::vector<float>& b);
    
    /**
     * @brief Compute dot product of two embeddings.
     * @param a First embedding vector.
     * @param b Second embedding vector.
     * @return Dot product value.
     */
    static double dot_product(const std::vector<float>& a, const std::vector<float>& b);
    
    /**
     * @brief Compute magnitude (L2 norm) of an embedding.
     * @param a Embedding vector.
     * @return Magnitude of the vector.
     */
    static double magnitude(const std::vector<float>& a);
    
    /**
     * @brief Perform semantic search on documents.
     * @param query Search query text.
     * @param documents Vector of document texts to search through.
     * @param top_k Number of top results to return (default: 5).
     * @return Vector of tuples (similarity_score, document_index, document_text).
     * @throws std::runtime_error if embedder not initialized.
     */
    std::vector<std::tuple<double, int, std::string>> semantic_search(
        const std::string& query, 
        const std::vector<std::string>& documents,
        int top_k = 5
    );

    /**
     * @brief Check if embedder is initialized.
     * @return true if initialized, false otherwise.
     */
    bool is_initialized() const { return initialized; }
    
    /**
     * @brief Get current configuration.
     * @return Config structure with current settings.
     */
    Config get_config() const { return config; }
    
    /**
     * @brief Get embedding dimension.
     * @return Dimension of embeddings, or 0 if not initialized.
     */
    int get_embedding_dimension() const;

private:
    Config config;                      ///< Configuration settings
    bool initialized = false;           ///< Initialization flag
    std::string config_path = "config/embedder_config.json";  ///< Path to config file
    
    // Llama.cpp members
    std::shared_ptr<llama_model> model; ///< Shared pointer to llama model
    std::shared_ptr<llama_context> context;  ///< Shared pointer to llama context
    const llama_vocab* vocab_ptr = nullptr;  ///< Pointer to model vocabulary
    llama_batch batch;                 ///< Batch for token processing
    
    int n_embd = 0;                    ///< Embedding dimension from model
    enum llama_pooling_type pooling_type;  ///< Pooling type for embeddings

    /**
     * @brief Load configuration from JSON file.
     * @param config_path Path to configuration file.
     * @return true if load successful, false otherwise.
     */
    bool config_load(const std::string& config_path);
    
    /**
     * @brief Save current configuration to JSON file.
     * @param config_path Path to save configuration.
     * @return true if save successful, false otherwise.
     */
    bool save_config(const std::string& config_path);
    
    /**
     * @brief Create default configuration file.
     */
    void create_default_config();

    /**
     * @brief Tokenize text using model vocabulary.
     * @param text Input text to tokenize.
     * @param add_special_tokens Whether to add special tokens.
     * @return Vector of token IDs.
     */
    std::vector<llama_token> tokenize(const std::string& text, bool add_special_tokens) const;
    
    /**
     * @brief Add sequence to batch for processing.
     * @param batch_ Reference to llama batch.
     * @param tokens Vector of token IDs.
     * @param seq_id Sequence ID for batch.
     */
    void batch_add_seq_internal(llama_batch & batch_, const std::vector<int32_t> & tokens, llama_seq_id seq_id);
    
    /**
     * @brief Decode batch and extract embeddings.
     * @param batch_ Reference to llama batch.
     * @param output Pointer to output array.
     * @param n_embd_ Embedding dimension.
     * @param embd_norm Normalization mode.
     */
    void batch_decode_internal(llama_batch & batch_, float * output, int n_embd_, int embd_norm);
    
    /**
     * @brief Extract embedding vector from JSON response.
     * @param response_data JSON data containing embedding.
     * @return Vector of floats representing embedding.
     * @throws std::runtime_error if JSON format not recognized.
     */
    std::vector<float> extract_embedding_from_json(const json& response_data);
    
    /**
     * @brief Callback function for writing HTTP response data.
     * @param contents Pointer to response data.
     * @param size Size of each element.
     * @param nmemb Number of elements.
     * @param response String to append data to.
     * @return Total size written.
     */
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response);
};

#endif // EMBEDDER_H