// Embedder.hpp
#ifndef EMBEDDER_H
#define EMBEDDER_H

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <tuple>
#include <stdexcept>
#include <curl/curl.h>
#include <fstream>
#include <memory>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class Embedder {
public:
    struct Config {
        std::string host = "localhost";
        int port = 8080;
        std::string endpoint = "/embedding";
        std::string model_path;
        std::string server_executable = "./server";
        bool auto_start_server = true;
        int server_timeout = 30;
        int request_timeout = 30;
        int embedding_dimension = 4096; // padrão para muitos modelos
    };

    Embedder();
    explicit Embedder(const std::string& config_path);
    ~Embedder();

    bool initialize();
    bool initialize(const std::string& config_path);
    
    std::vector<float> generate_embedding(const std::string& text);
    std::vector<std::vector<float>> generate_embeddings(const std::vector<std::string>& texts);
    
    // Funções de similaridade estáticas
    static double cosine_similarity(const std::vector<float>& a, const std::vector<float>& b);
    static double dot_product(const std::vector<float>& a, const std::vector<float>& b);
    static double magnitude(const std::vector<float>& a);
    
    // Busca semântica
    std::vector<std::tuple<double, int, std::string>> semantic_search(
        const std::string& query, 
        const std::vector<std::string>& documents,
        int top_k = 5
    );

    // Getters
    bool is_initialized() const { return initialized; }
    std::string get_server_url() const { 
        return "http://" + config.host + ":" + std::to_string(config.port) + config.endpoint; 
    }
    Config get_config() const { return config; }

private:
    Config config;
    bool initialized = false;
    bool server_started = false;
    std::unique_ptr<std::thread> server_thread;
    std::string config_path = "config/embedder_config.json";

    // Métodos privados
    bool load_config(const std::string& config_path);
    bool save_config(const std::string& config_path);
    bool start_llama_server();
    void stop_llama_server();
    void server_process();
    bool wait_for_server_ready(int timeout_seconds = 30);
    
    std::vector<float> generate_embedding_impl(const std::string& text);
    std::vector<float> extract_embedding_from_json(const json& response_data);
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response);
    
    void create_default_config();
};

#endif // EMBEDDER_H