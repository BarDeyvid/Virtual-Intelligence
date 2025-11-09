#include <filesystem>
#include <sqlite3.h>
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
#include "includes/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// --- Embedder Integration ---
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
        int embedding_dimension = 4096;
    };

    Embedder() = default;
    
    explicit Embedder(const std::string& config_path) {
        initialize(config_path);
    }
    
    ~Embedder() {
        stop_llama_server();
    }

    bool initialize(const std::string& config_path = "config/embedder_config.json") {
        if (!load_config(config_path)) {
            std::cerr << "Failed to load embedder config, using defaults" << std::endl;
            create_default_config();
        }
        
        if (config.auto_start_server) {
            if (!start_llama_server()) {
                std::cerr << "Failed to start embedding server" << std::endl;
                return false;
            }
        }
        
        initialized = true;
        std::cout << "Embedder initialized successfully" << std::endl;
        return true;
    }
    
    std::vector<float> generate_embedding(const std::string& text) {
        if (!initialized) {
            throw std::runtime_error("Embedder not initialized");
        }
        
        return generate_embedding_impl(text);
    }
    
    std::vector<std::vector<float>> generate_embeddings(const std::vector<std::string>& texts) {
        std::vector<std::vector<float>> embeddings;
        for (const auto& text : texts) {
            embeddings.push_back(generate_embedding(text));
        }
        return embeddings;
    }
    
    static double cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) {
            throw std::invalid_argument("Vectors must have same dimension");
        }
        
        double dot = 0.0, mag_a = 0.0, mag_b = 0.0;
        for (size_t i = 0; i < a.size(); ++i) {
            dot += a[i] * b[i];
            mag_a += a[i] * a[i];
            mag_b += b[i] * b[i];
        }
        
        if (mag_a == 0 || mag_b == 0) return 0.0;
        return dot / (std::sqrt(mag_a) * std::sqrt(mag_b));
    }
    
    static double dot_product(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) {
            throw std::invalid_argument("Vectors must have same dimension");
        }
        
        double result = 0.0;
        for (size_t i = 0; i < a.size(); ++i) {
            result += a[i] * b[i];
        }
        return result;
    }
    
    static double magnitude(const std::vector<float>& a) {
        double result = 0.0;
        for (const auto& val : a) {
            result += val * val;
        }
        return std::sqrt(result);
    }
    
    std::vector<std::tuple<double, int, std::string>> semantic_search(
        const std::string& query, 
        const std::vector<std::string>& documents,
        int top_k = 5
    ) {
        auto query_embedding = generate_embedding(query);
        std::vector<std::tuple<double, int, std::string>> results;
        
        for (size_t i = 0; i < documents.size(); ++i) {
            try {
                auto doc_embedding = generate_embedding(documents[i]);
                double similarity = cosine_similarity(query_embedding, doc_embedding);
                results.emplace_back(similarity, i, documents[i]);
            } catch (const std::exception& e) {
                std::cerr << "Error generating embedding for document " << i << ": " << e.what() << std::endl;
            }
        }
        
        std::sort(results.begin(), results.end(), 
            [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });
        
        if (results.size() > top_k) {
            results.resize(top_k);
        }
        
        return results;
    }

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

    bool load_config(const std::string& config_path) {
        try {
            if (!fs::exists(config_path)) {
                return false;
            }
            
            std::ifstream file(config_path);
            json j;
            file >> j;
            
            if (j.contains("host")) config.host = j["host"];
            if (j.contains("port")) config.port = j["port"];
            if (j.contains("endpoint")) config.endpoint = j["endpoint"];
            if (j.contains("model_path")) config.model_path = j["model_path"];
            if (j.contains("server_executable")) config.server_executable = j["server_executable"];
            if (j.contains("auto_start_server")) config.auto_start_server = j["auto_start_server"];
            if (j.contains("server_timeout")) config.server_timeout = j["server_timeout"];
            if (j.contains("request_timeout")) config.request_timeout = j["request_timeout"];
            if (j.contains("embedding_dimension")) config.embedding_dimension = j["embedding_dimension"];
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error loading config: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool save_config(const std::string& config_path) {
        try {
            fs::create_directories(fs::path(config_path).parent_path());
            
            json j;
            j["host"] = config.host;
            j["port"] = config.port;
            j["endpoint"] = config.endpoint;
            j["model_path"] = config.model_path;
            j["server_executable"] = config.server_executable;
            j["auto_start_server"] = config.auto_start_server;
            j["server_timeout"] = config.server_timeout;
            j["request_timeout"] = config.request_timeout;
            j["embedding_dimension"] = config.embedding_dimension;
            
            std::ofstream file(config_path);
            file << j.dump(4);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error saving config: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool start_llama_server() {
        if (config.model_path.empty()) {
            std::cerr << "No model path specified for embedding server" << std::endl;
            return false;
        }
        
        if (!fs::exists(config.server_executable)) {
            std::cerr << "Server executable not found: " << config.server_executable << std::endl;
            return false;
        }
        
        server_thread = std::make_unique<std::thread>([this]() { server_process(); });
        
        return wait_for_server_ready(config.server_timeout);
    }
    
    void stop_llama_server() {
        if (server_thread && server_thread->joinable()) {
            // In a real implementation, we'd send a shutdown signal to the server
            server_thread->detach(); // For demo purposes
        }
    }
    
    void server_process() {
        std::string command = config.server_executable + " -m " + config.model_path + 
                             " --host " + config.host + " --port " + std::to_string(config.port) +
                             " --embedding";
        
        std::cout << "Starting embedding server: " << command << std::endl;
        int result = std::system(command.c_str());
        if (result != 0) {
            std::cerr << "Embedding server exited with error: " << result << std::endl;
        }
    }
    
    bool wait_for_server_ready(int timeout_seconds = 30) {
        auto start = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(timeout_seconds)) {
            if (check_server_ready()) {
                server_started = true;
                return true;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        return false;
    }
    
    bool check_server_ready() {
        CURL* curl = curl_easy_init();
        if (!curl) return false;
        
        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, get_server_url().c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        return res == CURLE_OK;
    }
    
    std::vector<float> generate_embedding_impl(const std::string& text) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
        
        json request;
        request["content"] = text;
        
        std::string request_str = request.dump();
        std::string response;
        
        curl_easy_setopt(curl, CURLOPT_URL, get_server_url().c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, config.request_timeout);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, 
            curl_slist_append(nullptr, "Content-Type: application/json"));
        
        CURLcode res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("HTTP request failed: " + std::string(curl_easy_strerror(res)));
        }
        
        curl_easy_cleanup(curl);
        
        return extract_embedding_from_json(json::parse(response));
    }
    
    std::vector<float> extract_embedding_from_json(const json& response_data) {
        if (response_data.contains("embedding") && response_data["embedding"].is_array()) {
            return response_data["embedding"].get<std::vector<float>>();
        }
        throw std::runtime_error("Invalid embedding response format");
    }
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
        size_t total_size = size * nmemb;
        response->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
    
    void create_default_config() {
        config.host = "localhost";
        config.port = 8080;
        config.endpoint = "/embedding";
        config.model_path = "models/embedding_model.bin";
        config.server_executable = "./llama-server";
        config.auto_start_server = true;
        config.server_timeout = 30;
        config.request_timeout = 30;
        config.embedding_dimension = 4096;
        
        save_config("config/embedder_config.json");
    }
};

// --- Constantes e Utilitários Melhorados ---
constexpr int VTIME_DIM = 2;
constexpr size_t MAX_TOKENS = 4096;
constexpr double DECAY_PER_HOUR = 0.01;
constexpr double MIN_IMPORTANCE = 0.3;
constexpr double HARD_LOCK_IMPORTANCE = 0.9;

class Utils {
public:
    static std::vector<float> getTimeStamp() {
        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        double hours = std::fmod(ts / 3600.0, 24.0);
        double radians = hours * (M_PI / 12.0);
        
        return { 
            static_cast<float>(std::sin(radians)), 
            static_cast<float>(std::cos(radians)) 
        };
    }
    
    static std::string getCurrentISOTime() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
        return std::string(buffer);
    }
    
    static double sigmoid(double x) {
        return 1.0 / (1.0 + std::exp(-x));
    }
    
    static double normalizeImportance(double raw_importance) {
        return std::max(0.0, std::min(1.0, raw_importance));
    }
};

// --- Estruturas de Dados para o Sistema Avançado ---
struct EmotionalState {
    std::string name;
    double intensity;
    uint64_t timestamp;
    
    EmotionalState(const std::string& n = "neutral", double i = 0.5) 
        : name(n), intensity(i) {
        timestamp = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
    }
};

struct Intention {
    int id;
    std::string description;
    std::string type;
    std::string trigger;
    bool active;
    double motivation;
    std::string created_at;
    
    Intention(const std::string& desc, const std::string& t, const std::string& trig = "")
        : description(desc), type(t), trigger(trig), active(true), motivation(0.5) {
        created_at = Utils::getCurrentISOTime();
    }
};

struct MemoryLink {
    int source_id;
    int target_id;
    double weight;
    std::string type;
    
    MemoryLink(int src, int tgt, double w = 1.0, const std::string& t = "association")
        : source_id(src), target_id(tgt), weight(w), type(t) {}
};

struct Reflection {
    int memory_id;
    std::string type;
    std::string content;
    std::string created_at;
    
    Reflection(int mem_id, const std::string& t, const std::string& c)
        : memory_id(mem_id), type(t), content(c) {
        created_at = Utils::getCurrentISOTime();
    }
};

// --- Classe Principal do Sistema de Memória Avançado COM EMBEDDINGS ---
class AdvancedMemorySystem {
private:
    sqlite3* db;
    EmotionalState current_emotional_state;
    std::vector<Intention> active_intentions;
    std::map<std::string, double> emotion_weights;
    std::unique_ptr<Embedder> embedder;
    
    std::unordered_map<std::string, std::pair<std::string, std::string>> emotion_to_intention = {
        {"tristeza", {"Refletir sobre sentimentos difíceis", "emocional"}},
        {"alegria", {"Manter momentos positivos", "emocional"}},
        {"raiva", {"Redirecionar frustrações", "emocional"}},
        {"medo", {"Buscar segurança", "emocional"}},
        {"surpresa", {"Explorar novidade", "emocional"}},
        {"confiança", {"Apoiar outros", "emocional"}},
        {"antecipacao", {"Planejar próximos passos", "planejamento"}}
    };

public:
    AdvancedMemorySystem(const std::string& db_path, bool init_embedder = true) : db(nullptr) {
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
        }
        initializeDatabase();
        loadCurrentState();
        
        emotion_weights = {
            {"alegria", 0.8}, {"tristeza", 0.6}, {"raiva", 0.7}, 
            {"medo", 0.5}, {"surpresa", 0.4}, {"confiança", 0.9}
        };
        
        // Inicializar embedder
        if (init_embedder) {
            try {
                embedder = std::make_unique<Embedder>();
                if (!embedder->initialize()) {
                    std::cerr << "Embedder initialization failed, continuing without semantic search" << std::endl;
                } else {
                    std::cout << "Embedder integrated successfully" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Embedder initialization error: " << e.what() << std::endl;
            }
        }
        
        std::cout << "Sistema de Memória Avançado Inicializado\n";
    }
    
    ~AdvancedMemorySystem() {
        if (db) {
            saveCurrentState();
            sqlite3_close(db);
        }
    }

private:
    void initializeDatabase() {
        const char* tables[] = {
            // Tabela de estados emocionais
            R"(
            CREATE TABLE IF NOT EXISTS emotional_states (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                intensity REAL DEFAULT 0.5,
                timestamp INTEGER
            );
            )",
            
            // Tabela de intenções
            R"(
            CREATE TABLE IF NOT EXISTS intentions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                description TEXT NOT NULL,
                type TEXT NOT NULL,
                trigger TEXT,
                active BOOLEAN DEFAULT 1,
                motivation REAL DEFAULT 0.5,
                created_at TEXT
            );
            )",
            
            // Tabela de decadência de memórias
            R"(
            CREATE TABLE IF NOT EXISTS memory_decay (
                memory_id INTEGER PRIMARY KEY,
                last_access INTEGER,
                current_value REAL,
                decaying_since INTEGER,
                FOREIGN KEY(memory_id) REFERENCES memories(id)
            );
            )",
            
            // Tabela de reflexões
            R"(
            CREATE TABLE IF NOT EXISTS reflections (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                memory_id INTEGER,
                type TEXT,
                content TEXT,
                created_at TEXT,
                FOREIGN KEY(memory_id) REFERENCES memories(id)
            );
            )",
            
            // Tabela de vínculos entre memórias
            R"(
            CREATE TABLE IF NOT EXISTS memory_links (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                source_id INTEGER,
                target_id INTEGER,
                weight REAL DEFAULT 1.0,
                type TEXT,
                FOREIGN KEY(source_id) REFERENCES memories(id),
                FOREIGN KEY(target_id) REFERENCES memories(id)
            );
            )",
            
            // NOVA: Tabela de embeddings vetoriais
            R"(
            CREATE TABLE IF NOT EXISTS memory_embeddings (
                memory_id INTEGER PRIMARY KEY,
                embedding BLOB,
                embedding_dimension INTEGER,
                created_at TEXT,
                FOREIGN KEY(memory_id) REFERENCES memories(id)
            );
            )"
        };
        
        char* errMsg = nullptr;
        for (const char* table_sql : tables) {
            if (sqlite3_exec(db, table_sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                std::string error = "SQL error: ";
                error += errMsg;
                sqlite3_free(errMsg);
                throw std::runtime_error(error);
            }
        }
    }
    
    void loadCurrentState() {
        const char* sql = "SELECT name, intensity FROM emotional_states ORDER BY timestamp DESC LIMIT 1";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                current_emotional_state.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                current_emotional_state.intensity = sqlite3_column_double(stmt, 1);
            }
            sqlite3_finalize(stmt);
        }
        
        loadActiveIntentions();
    }
    
    void saveCurrentState() {
        const char* sql = "INSERT INTO emotional_states (name, intensity, timestamp) VALUES (?, ?, ?)";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, current_emotional_state.name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 2, current_emotional_state.intensity);
            sqlite3_bind_int64(stmt, 3, current_emotional_state.timestamp);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    
    void loadActiveIntentions() {
        active_intentions.clear();
        const char* sql = "SELECT id, description, type, trigger, motivation, created_at FROM intentions WHERE active = 1";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Intention intention(
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))
                );
                intention.id = sqlite3_column_int(stmt, 0);
                intention.motivation = sqlite3_column_double(stmt, 4);
                intention.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                active_intentions.push_back(intention);
            }
            sqlite3_finalize(stmt);
        }
    }

public:
    // === MÉTODOS DE EMBEDDING E BUSCA SEMÂNTICA ===
    
    bool hasEmbedder() const {
        return embedder != nullptr && embedder->is_initialized();
    }
    
    // Gerar embedding para texto e salvar no banco
    bool generateAndStoreEmbedding(int memory_id, const std::string& content) {
        if (!hasEmbedder()) {
            return false;
        }
        
        try {
            auto embedding = embedder->generate_embedding(content);
            storeEmbedding(memory_id, embedding);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error generating embedding: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Busca semântica usando embeddings
    struct SemanticMemory {
        int memory_id;
        std::string content;
        double similarity_score;
        std::string emotion;
        double importance;
    };
    
    std::vector<SemanticMemory> semanticSearch(const std::string& query, int top_k = 5) {
        std::vector<SemanticMemory> results;
        
        if (!hasEmbedder()) {
            std::cerr << "Embedder not available for semantic search" << std::endl;
            return results;
        }
        
        try {
            auto query_embedding = embedder->generate_embedding(query);
            return semanticSearchWithEmbedding(query_embedding, top_k);
        } catch (const std::exception& e) {
            std::cerr << "Error in semantic search: " << e.what() << std::endl;
            return results;
        }
    }
    
    std::vector<SemanticMemory> semanticSearchWithEmbedding(const std::vector<float>& query_embedding, int top_k = 5) {
        std::vector<SemanticMemory> results;
        
        const char* sql = R"(
            SELECT m.id, m.conteudo, m.emocao, m.importancia, me.embedding
            FROM memories m
            JOIN memory_embeddings me ON m.id = me.memory_id
            WHERE me.embedding IS NOT NULL
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                SemanticMemory mem;
                mem.memory_id = sqlite3_column_int(stmt, 0);
                mem.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                mem.emotion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                mem.importance = sqlite3_column_double(stmt, 3);
                
                // Extrair embedding do BLOB
                const void* blob_data = sqlite3_column_blob(stmt, 4);
                int blob_size = sqlite3_column_bytes(stmt, 4);
                std::vector<float> memory_embedding(blob_size / sizeof(float));
                memcpy(memory_embedding.data(), blob_data, blob_size);
                
                // Calcular similaridade
                mem.similarity_score = Embedder::cosine_similarity(query_embedding, memory_embedding);
                
                // Aplicar boosting contextual
                mem.similarity_score += calculateIntentionBoost(mem.content);
                mem.similarity_score += calculateEmotionalBoost(mem.emotion);
                
                results.push_back(mem);
            }
            sqlite3_finalize(stmt);
        }
        
        // Ordenar por similaridade
        std::sort(results.begin(), results.end(), 
            [](const SemanticMemory& a, const SemanticMemory& b) {
                return a.similarity_score > b.similarity_score;
            });
        
        if (results.size() > top_k) {
            results.resize(top_k);
        }
        
        return results;
    }
    
    // Busca híbrida: combina busca textual e semântica
    struct HybridMemoryResult {
        int memory_id;
        std::string content;
        double text_score;
        double semantic_score;
        double combined_score;
        std::string emotion;
    };
    
    std::vector<HybridMemoryResult> hybridSearch(const std::string& query, int top_k = 5) {
        std::vector<HybridMemoryResult> results;
        
        // Busca textual tradicional
        auto text_results = searchContextualMemories(query, top_k * 2);
        
        // Busca semântica
        auto semantic_results = semanticSearch(query, top_k * 2);
        
        // Combinar resultados
        std::unordered_map<int, HybridMemoryResult> combined;
        
        // Adicionar resultados textuais
        for (const auto& text_mem : text_results) {
            HybridMemoryResult result;
            result.memory_id = text_mem.id;
            result.content = text_mem.content;
            result.text_score = text_mem.relevance_score;
            result.semantic_score = 0.0;
            result.emotion = text_mem.emotion;
            combined[text_mem.id] = result;
        }
        
        // Adicionar/atualizar com resultados semânticos
        for (const auto& semantic_mem : semantic_results) {
            if (combined.find(semantic_mem.memory_id) != combined.end()) {
                combined[semantic_mem.memory_id].semantic_score = semantic_mem.similarity_score;
            } else {
                HybridMemoryResult result;
                result.memory_id = semantic_mem.memory_id;
                result.content = semantic_mem.content;
                result.text_score = 0.0;
                result.semantic_score = semantic_mem.similarity_score;
                result.emotion = semantic_mem.emotion;
                combined[semantic_mem.memory_id] = result;
            }
        }
        
        // Calcular scores combinados e coletar
        for (auto& [id, result] : combined) {
            result.combined_score = (result.text_score * 0.4) + (result.semantic_score * 0.6);
            results.push_back(result);
        }
        
        // Ordenar por score combinado
        std::sort(results.begin(), results.end(),
            [](const HybridMemoryResult& a, const HybridMemoryResult& b) {
                return a.combined_score > b.combined_score;
            });
        
        if (results.size() > top_k) {
            results.resize(top_k);
        }
        
        return results;
    }

private:
    void storeEmbedding(int memory_id, const std::vector<float>& embedding) {
        const char* sql = R"(
            INSERT OR REPLACE INTO memory_embeddings (memory_id, embedding, embedding_dimension, created_at)
            VALUES (?, ?, ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            // Converter vector<float> para BLOB
            sqlite3_bind_int(stmt, 1, memory_id);
            sqlite3_bind_blob(stmt, 2, embedding.data(), embedding.size() * sizeof(float), SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, static_cast<int>(embedding.size()));
            sqlite3_bind_text(stmt, 4, Utils::getCurrentISOTime().c_str(), -1, SQLITE_STATIC);
            
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

public:
    // === INTERFACE HIGH-LEVEL PARA INTEGRAÇÃO ===
    
    void setEmotionalState(const std::string& emotion, double intensity) {
        current_emotional_state.name = emotion;
        current_emotional_state.intensity = Utils::normalizeImportance(intensity);
        current_emotional_state.timestamp = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        
        std::cout << "Estado Emocional Atualizado: " << emotion 
                  << " (intensidade: " << intensity << ")\n";
        
        checkEmotionalAutoActivation();
    }
    
    EmotionalState getCurrentEmotionalState() const {
        return current_emotional_state;
    }
    
    void applyMemoryDecay() {
        const char* sql = R"(
            UPDATE memory_decay 
            SET current_value = MAX(?, current_value - ?),
                last_access = ?
            WHERE current_value > ?
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            
            sqlite3_bind_double(stmt, 1, MIN_IMPORTANCE);
            sqlite3_bind_double(stmt, 2, DECAY_PER_HOUR);
            sqlite3_bind_int64(stmt, 3, now);
            sqlite3_bind_double(stmt, 4, MIN_IMPORTANCE);
            
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::cerr << "Erro ao aplicar decadência: " << sqlite3_errmsg(db) << "\n";
            } else {
                std::cout << "Decadência de memórias aplicada\n";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    void activateIntention(const std::string& description, const std::string& type, 
                          const std::string& trigger = "", double motivation_boost = 0.0) {
        for (const auto& intention : active_intentions) {
            if (intention.description == description && intention.active) {
                std::cout << "Intenção já está ativa: " << description << "\n";
                return;
            }
        }
        
        const char* sql = R"(
            INSERT INTO intentions (description, type, trigger, active, motivation, created_at)
            VALUES (?, ?, ?, 1, ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, description.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, trigger.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 4, 0.5 + motivation_boost);
            sqlite3_bind_text(stmt, 5, Utils::getCurrentISOTime().c_str(), -1, SQLITE_STATIC);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                loadActiveIntentions();
                std::cout << "Nova Intenção Ativada: " << description << "\n";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    void deactivateIntention(int intention_id) {
        const char* sql = "UPDATE intentions SET active = 0 WHERE id = ?";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, intention_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            active_intentions.erase(
                std::remove_if(active_intentions.begin(), active_intentions.end(),
                    [intention_id](const Intention& i) { return i.id == intention_id; }),
                active_intentions.end()
            );
            
            std::cout << "Intenção Desativada: ID " << intention_id << "\n";
        }
    }
    
    std::vector<Intention> getActiveIntentions() const {
        return active_intentions;
    }
    
    void generateReflections() {
        const char* sql = R"(
            SELECT m.id, m.conteudo, m.emocao, m.importancia 
            FROM memories m
            WHERE m.timestamp >= ? AND m.importancia >= 0.5
            ORDER BY m.timestamp DESC
        )";
        
        sqlite3_stmt* stmt;
        auto six_hours_ago = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now() - std::chrono::hours(6));
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, six_hours_ago);
            
            std::unordered_map<std::string, int> emotion_counts;
            std::unordered_map<std::string, int> memory_ids;
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string emotion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                int memory_id = sqlite3_column_int(stmt, 0);
                
                emotion_counts[emotion]++;
                if (memory_ids.find(emotion) == memory_ids.end()) {
                    memory_ids[emotion] = memory_id;
                }
            }
            sqlite3_finalize(stmt);
            
            for (const auto& [emotion, count] : emotion_counts) {
                if (count >= 3) {
                    std::string reflection_content = 
                        "Notei que me senti muito " + emotion + 
                        " recentemente. Talvez isso signifique algo importante.";
                    
                    saveReflection(memory_ids[emotion], "emotional_pattern", reflection_content);
                    std::cout << "Reflexão Gerada: " << reflection_content << "\n";
                }
            }
        }
    }
    
    void createMemoryLink(int source_id, int target_id, double weight = 1.0, 
                         const std::string& link_type = "association") {
        const char* sql = R"(
            INSERT INTO memory_links (source_id, target_id, weight, type)
            VALUES (?, ?, ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, source_id);
            sqlite3_bind_int(stmt, 2, target_id);
            sqlite3_bind_double(stmt, 3, weight);
            sqlite3_bind_text(stmt, 4, link_type.c_str(), -1, SQLITE_STATIC);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                std::cout << "Vínculo criado: " << source_id << " → " << target_id 
                          << " (" << link_type << ")\n";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    std::vector<MemoryLink> getMemoryLinks(int memory_id) {
        std::vector<MemoryLink> links;
        const char* sql = R"(
            SELECT source_id, target_id, weight, type 
            FROM memory_links 
            WHERE source_id = ? OR target_id = ?
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, memory_id);
            sqlite3_bind_int(stmt, 2, memory_id);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                MemoryLink link(
                    sqlite3_column_int(stmt, 0),
                    sqlite3_column_int(stmt, 1),
                    sqlite3_column_double(stmt, 2),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))
                );
                links.push_back(link);
            }
            sqlite3_finalize(stmt);
        }
        return links;
    }
    
    // Busca Contextual com Intenções (original)
    struct ContextualMemory {
        int id;
        std::string content;
        double relevance_score;
        std::string emotion;
    };
    
    std::vector<ContextualMemory> searchContextualMemories(const std::string& query, 
                                                          int top_k = 5) {
        std::vector<ContextualMemory> results;
        
        const char* sql = R"(
            SELECT id, conteudo, emocao, importancia 
            FROM memories 
            WHERE conteudo LIKE ? OR contexto LIKE ?
            ORDER BY importancia DESC 
            LIMIT ?
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            std::string like_query = "%" + query + "%";
            sqlite3_bind_text(stmt, 1, like_query.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, like_query.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, top_k * 2);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                ContextualMemory mem;
                mem.id = sqlite3_column_int(stmt, 0);
                mem.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                mem.emotion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                mem.relevance_score = sqlite3_column_double(stmt, 3);
                
                mem.relevance_score += calculateIntentionBoost(mem.content);
                mem.relevance_score += calculateEmotionalBoost(mem.emotion);
                
                results.push_back(mem);
            }
            sqlite3_finalize(stmt);
        }
        
        std::sort(results.begin(), results.end(), 
            [](const ContextualMemory& a, const ContextualMemory& b) {
                return a.relevance_score > b.relevance_score;
            });
        
        if (results.size() > top_k) {
            results.resize(top_k);
        }
        
        return results;
    }

private:
    void checkEmotionalAutoActivation() {
        auto it = emotion_to_intention.find(current_emotional_state.name);
        if (it != emotion_to_intention.end() && current_emotional_state.intensity > 0.7) {
            const auto& [description, type] = it->second;
            
            bool already_active = false;
            for (const auto& intention : active_intentions) {
                if (intention.description == description) {
                    already_active = true;
                    break;
                }
            }
            
            if (!already_active) {
                std::string trigger = "emoção: " + current_emotional_state.name;
                activateIntention(description, type, trigger, 0.3);
                std::cout << "Auto-ativação por emoção: " << description << "\n";
            }
        }
    }
    
    void saveReflection(int memory_id, const std::string& type, const std::string& content) {
        const char* sql = R"(
            INSERT INTO reflections (memory_id, type, content, created_at)
            VALUES (?, ?, ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, memory_id);
            sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, Utils::getCurrentISOTime().c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    
    double calculateIntentionBoost(const std::string& content) {
        double boost = 0.0;
        for (const auto& intention : active_intentions) {
            if (content.find(intention.description) != std::string::npos) {
                boost += 0.3 * intention.motivation;
            }
        }
        return std::min(boost, 0.5);
    }
    
    double calculateEmotionalBoost(const std::string& memory_emotion) {
        if (memory_emotion == current_emotional_state.name) {
            return 0.2 * current_emotional_state.intensity;
        }
        return 0.0;
    }

public:
    // === MÉTODOS DE MONITORAMENTO E DEBUG ===
    
    void printSystemStatus() {
        std::cout << "\n=== STATUS DO SISTEMA DE MEMÓRIA ===\n";
        std::cout << "Estado Emocional: " << current_emotional_state.name 
                  << " (intensidade: " << current_emotional_state.intensity << ")\n";
        std::cout << "Intenções Ativas: " << active_intentions.size() << "\n";
        
        for (const auto& intention : active_intentions) {
            std::cout << "   • " << intention.description 
                      << " [" << intention.type << "]"
                      << " (motivação: " << intention.motivation << ")\n";
        }
        
        // Contar memórias
        const char* sql = "SELECT COUNT(*) FROM memories";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                std::cout << "Total de Memórias: " << sqlite3_column_int(stmt, 0) << "\n";
            }
            sqlite3_finalize(stmt);
        }
        
        // Status do embedder
        std::cout << "Embedder: " << (hasEmbedder() ? "Ativo" : "Inativo") << "\n";
        
        std::cout << "====================================\n\n";
    }
    
    // Novo método para demonstrar busca semântica
    void demonstrateSemanticSearch(const std::string& query) {
        if (!hasEmbedder()) {
            std::cout << " Embedder não disponível para busca semântica\n";
            return;
        }
        
        std::cout << "\n🔍 DEMONSTRAÇÃO DE BUSCA SEMÂNTICA\n";
        std::cout << "Query: \"" << query << "\"\n";
        
        auto results = semanticSearch(query, 3);
        
        if (results.empty()) {
            std::cout << "Nenhum resultado encontrado.\n";
            return;
        }
        
        std::cout << "Top " << results.size() << " resultados:\n";
        for (const auto& mem : results) {
            std::cout << std::fixed << std::setprecision(3);
            std::cout << "[" << mem.similarity_score << "] ";
            std::cout << "ID " << mem.memory_id << ": " << mem.content.substr(0, 80);
            if (mem.content.length() > 80) std::cout << "...";
            std::cout << " [" << mem.emotion << "]\n";
        }
        std::cout << "----------------------------------------\n";
    }
};

// --- Exemplo de Integração Atualizado ---
class AlyssaMemoryManager {
private:
    std::unique_ptr<AdvancedMemorySystem> memory_system;
    
public:
    AlyssaMemoryManager(const std::string& db_path, bool enable_embedder = true) {
        memory_system = std::make_unique<AdvancedMemorySystem>(db_path, enable_embedder);
    }
    
    void processInteraction(const std::string& user_input, 
                          const std::string& ai_response,
                          const std::vector<float>& emotional_vector) {
        std::string dominant_emotion = "neutral";
        double max_intensity = 0.0;
        
        std::vector<std::string> emotions = {"alegria", "tristeza", "raiva", "medo", "surpresa"};
        for (size_t i = 0; i < emotional_vector.size() && i < emotions.size(); ++i) {
            if (emotional_vector[i] > max_intensity) {
                max_intensity = emotional_vector[i];
                dominant_emotion = emotions[i];
            }
        }
        
        memory_system->setEmotionalState(dominant_emotion, max_intensity);
        analyzeInputForIntentions(user_input);
        
        static int interaction_count = 0;
        if (++interaction_count % 10 == 0) {
            memory_system->applyMemoryDecay();
            memory_system->generateReflections();
        }
        
        if (interaction_count % 5 == 0) {
            memory_system->printSystemStatus();
        }
        
        // Demonstração de busca semântica em algumas interações
        if (interaction_count % 7 == 0) {
            memory_system->demonstrateSemanticSearch(user_input);
        }
    }
    
    std::vector<AdvancedMemorySystem::ContextualMemory> getRelevantMemories(const std::string& context) {
        return memory_system->searchContextualMemories(context, 3);
    }
    
    std::vector<AdvancedMemorySystem::SemanticMemory> getSemanticMemories(const std::string& context) {
        return memory_system->semanticSearch(context, 3);
    }
    
    std::vector<AdvancedMemorySystem::HybridMemoryResult> getHybridMemories(const std::string& context) {
        return memory_system->hybridSearch(context, 3);
    }
    
    void setCurrentGoal(const std::string& goal, const std::string& type = "learning") {
        memory_system->activateIntention(goal, type, "user_defined", 0.5);
    }

private:
    void analyzeInputForIntentions(const std::string& input) {
        std::string lower_input = input;
        std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
        
        std::vector<std::pair<std::string, std::pair<std::string, std::string>>> triggers = {
            {"aprender", {"Aprender novo tópico", "aprendizado"}},
            {"como funciona", {"Entender mecanismos", "curiosidade"}},
            {"problema", {"Resolver desafio", "resolução"}},
            {"lembrar", {"Recuperar informação", "memória"}}
        };
        
        for (const auto& [keyword, intention] : triggers) {
            if (lower_input.find(keyword) != std::string::npos) {
                memory_system->activateIntention(intention.first, intention.second, keyword, 0.2);
                break;
            }
        }
    }
};

// --- Exemplo de Uso Atualizado ---
int main() {
    try {
        AlyssaMemoryManager memory_manager("alyssa_advanced_memory.db", true);
        
        std::vector<std::pair<std::string, std::string>> interactions = {
            {"Olá Alyssa, como você está?", "Estou bem, obrigada! E você?"},
            {"Estou aprendendo sobre inteligência artificial", "Que interessante! IA é um tópico fascinante."},
            {"Tive um problema com meu código hoje", "Sinto muito ouvir isso. Posso ajudar a resolver?"},
            {"Você consegue lembrar das nossas conversas anteriores?", "Sim, mantenho registro das nossas interações!"},
            {"Explique sobre machine learning e redes neurais", "São técnicas importantes de IA para reconhecimento de padrões."}
        };
        
        std::vector<std::vector<float>> emotional_vectors = {
            {0.8, 0.1, 0.0, 0.0, 0.3},
            {0.6, 0.0, 0.0, 0.0, 0.8},
            {0.1, 0.3, 0.6, 0.1, 0.0},
            {0.7, 0.0, 0.0, 0.0, 0.5},
            {0.5, 0.0, 0.0, 0.0, 0.9}
        };
        
        for (size_t i = 0; i < interactions.size(); ++i) {
            std::cout << "\n=== Interação " << (i + 1) << " ===\n";
            std::cout << "Usuário: " << interactions[i].first << "\n";
            std::cout << "Alyssa: " << interactions[i].second << "\n";
            
            memory_manager.processInteraction(
                interactions[i].first, 
                interactions[i].second,
                emotional_vectors[i]
            );
            
            // Testar diferentes tipos de busca
            if (i == 2) {
                auto semantic_results = memory_manager.getSemanticMemories("problema código");
                if (!semantic_results.empty()) {
                    std::cout << "Busca Semântica: " << semantic_results.size() << " resultados encontrados\n";
                }
            }
        }
        
        memory_manager.setCurrentGoal("Melhorar compreensão de emoções humanas", "desenvolvimento");
        
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}