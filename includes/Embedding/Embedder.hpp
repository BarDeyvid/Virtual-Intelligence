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

class Embedder {
public:
    struct Config {
        std::string model_path = "models/ggml-model.gguf";
        int embedding_dimension = 4096;
        
        int32_t seed = -1;
        int32_t n_ctx = 8192;
        int32_t n_batch = 8192;
        int32_t n_threads = 0;
        int32_t n_gpu_layers = 0;
        bool use_mlock = false;
        bool use_mmap = true;
        bool f16_kv = true;
        int embd_normalize = 2;
    };

    Embedder();
    explicit Embedder(const std::string& config_path);
    ~Embedder();

    bool initialize();
    bool initialize(const std::string& config_path);
    
    std::vector<float> generate_embedding(const std::string& text);
    std::vector<std::vector<float>> generate_embeddings(const std::vector<std::string>& texts);
    void clear_embedding_cache();
    
    static double cosine_similarity(const std::vector<float>& a, const std::vector<float>& b);
    static double dot_product(const std::vector<float>& a, const std::vector<float>& b);
    static double magnitude(const std::vector<float>& a);
    
    std::vector<std::tuple<double, int, std::string>> semantic_search(
        const std::string& query, 
        const std::vector<std::string>& documents,
        int top_k = 5
    );

    bool is_initialized() const { return initialized; }
    Config get_config() const { return config; }
    int get_embedding_dimension() const;

private:
    Config config;
    bool initialized = false;
    std::string config_path = "config/embedder_config.json";
    
    // Membros Llama.cpp
    std::shared_ptr<llama_model> model;
    std::shared_ptr<llama_context> context;
    const llama_vocab* vocab_ptr = nullptr; // ADD THIS LINE
    llama_batch batch;
    
    int n_embd = 0;
    enum llama_pooling_type pooling_type;

    // Métodos privados
    bool config_load(const std::string& config_path);
    bool save_config(const std::string& config_path);
    void create_default_config();

    std::vector<llama_token> tokenize(const std::string& text, bool add_special_tokens) const;
    void batch_add_seq_internal(llama_batch & batch_, const std::vector<int32_t> & tokens, llama_seq_id seq_id);
    void batch_decode_internal(llama_batch & batch_, float * output, int n_embd_, int embd_norm);
    std::vector<float> extract_embedding_from_json(const json& response_data);
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response);
};

#endif // EMBEDDER_H