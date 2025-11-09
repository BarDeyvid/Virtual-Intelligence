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
#include "llama.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <memory> // Para smart pointers

// --- Constants ---
constexpr int VTIME_DIM = 2;
constexpr size_t MAX_TOKENS = 4096;

// --- Classe Utils Melhorada ---
class Utils {
public:
    static std::vector<float> getTimeStamp() {
        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        // Normalizar para ciclo de 24 horas
        double hours = std::fmod(ts / 3600.0, 24.0);
        double radians = hours * (M_PI / 12.0); // 2π/24 = π/12
        
        return { 
            static_cast<float>(std::sin(radians)), 
            static_cast<float>(std::cos(radians)) 
        };
    }
    
    static std::string formatTimestamp(uint64_t timestamp) {
        std::time_t time = static_cast<std::time_t>(timestamp);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        return std::string(buffer);
    }
};

// --- Classe AlyssaInterprets Melhorada ---
class AlyssaInterprets {
private:
    int ctx;
    llama_model *model;
    llama_context *model_ctx;
    const llama_vocab *vocab;

public:
    AlyssaInterprets(const char *model_path, const std::string& save_path, int ctx_val)
        : ctx(ctx_val), model(nullptr), model_ctx(nullptr), vocab(nullptr) 
    {
        llama_model_params model_params = llama_model_default_params();
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = ctx;
        
        std::cout << "Carregando modelo: " << model_path << std::endl;
        
        this->model = llama_model_load_from_file(model_path, model_params);
        if (!model) {
            throw std::runtime_error("❌ Falha ao carregar modelo: " + std::string(model_path));
        }

        this->vocab = llama_model_get_vocab(model); 
        this->model_ctx = llama_init_from_model(model, ctx_params); 
        if (!model_ctx) {
            llama_model_free(model);
            throw std::runtime_error("❌ Falha ao criar contexto.");
        }
    }

    // Delete copy constructor and assignment operator
    AlyssaInterprets(const AlyssaInterprets&) = delete;
    AlyssaInterprets& operator=(const AlyssaInterprets&) = delete;

    ~AlyssaInterprets() {
        if (model_ctx) {
            llama_free(model_ctx);
        }
        if (model) {
            llama_model_free(model);
        }
        std::cout << "AlyssaInterprets finalizado e recursos liberados.\n";
    }

    uint64_t ModelHash() const {
        std::vector<char> desc_vef(ctx);
        llama_model_desc(model, desc_vef.data(), desc_vef.size());
        uint64_t model_hash = std::hash<std::string>{}(std::string(desc_vef.begin(), desc_vef.end()));
        std::cout << "\nModel Hash: 0x" << std::hex << model_hash << std::dec << "\n";
        return model_hash;
    }
    
    std::vector<llama_token> Tokenizer(const std::string& input, bool verbose = true) const {
        std::vector<float> v_time = Utils::getTimeStamp();
        std::string processed_input = "[TIME:" + std::to_string(v_time[0]) + "," + 
                                    std::to_string(v_time[1]) + "] " + input;
        
        std::vector<llama_token> tokens(MAX_TOKENS);
        int n = llama_tokenize(vocab, processed_input.c_str(), processed_input.size(), 
                              tokens.data(), tokens.size(), true, true);
        if (n < 0) {
            throw std::runtime_error("Falha na tokenização (retornou < 0)");
        }
        tokens.resize(n);
        
        if (verbose) {
            std::cout << "\nTokenização concluída (" << n << " tokens):\n";
            for (auto t : tokens) {
                char buf[64];
                int len = llama_token_to_piece(vocab, t, buf, sizeof(buf), 0, true);
                if (len >= 0) {
                    std::string piece(buf, len);
                    std::cout << "Token ID: " << std::setw(6) << t << " | Token Piece: " << piece << "\n";
                }
            }
        }
        return tokens;
    }

    int32_t token_to_piece(llama_token t, char *piece_buf, size_t buf_size) const {
        return llama_token_to_piece(vocab, t, piece_buf, buf_size, 0, true);
    }
    
    // Método para decodificar tokens para texto
    std::string tokens_to_text(const std::vector<llama_token>& tokens) const {
        std::string result;
        for (llama_token t : tokens) {
            char piece_buf[256];
            int len = token_to_piece(t, piece_buf, sizeof(piece_buf));
            if (len > 0) {
                result.append(piece_buf, len);
            }
        }
        return result;
    }
};

struct MemoryMatch {
    int id;
    double similarity;
    uint64_t timestamp;
    uint32_t n_tokens;
    uint32_t emo_dim;
    std::string preview_text; // Adicionar preview do texto
};

class SqliteHandler {
private:
    sqlite3 *db;
    
    #pragma pack(push, 1)
    struct AlyssaMemHeader {
        uint64_t timestamp;
        uint64_t model_hash;
        uint32_t n_tokens;
        uint32_t emo_dim;
    };
    #pragma pack(pop)

    // Função auxiliar privada: Cálculo da Similaridade de Cosseno
    double calculate_cosine_similarity(const std::vector<float>& v1, const std::vector<float>& v2) const {
        if (v1.size() != v2.size() || v1.empty()) {
            return 0.0; 
        }

        double dot_product = 0.0;
        double norm_v1 = 0.0;
        double norm_v2 = 0.0;

        for (size_t i = 0; i < v1.size(); ++i) {
            dot_product += static_cast<double>(v1[i]) * static_cast<double>(v2[i]);
            norm_v1 += static_cast<double>(v1[i]) * static_cast<double>(v1[i]);
            norm_v2 += static_cast<double>(v2[i]) * static_cast<double>(v2[i]);
        }

        if (norm_v1 == 0.0 || norm_v2 == 0.0) {
            return 0.0;
        }

        return dot_product / (std::sqrt(norm_v1) * std::sqrt(norm_v2));
    }

    void create_table_if_not_exists() {
        const char *create_sql = R"SQL(
            CREATE TABLE IF NOT EXISTS memories (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp INTEGER,
                model_hash INTEGER, 
                n_tokens INTEGER,
                emo_dim INTEGER,
                tokens BLOB
            );
        )SQL";
        
        const char *create_index = R"SQL(
            CREATE INDEX IF NOT EXISTS idx_timestamp ON memories(timestamp);
            CREATE INDEX IF NOT EXISTS idx_model_hash ON memories(model_hash);
        )SQL";

        char *errMsg = nullptr;
        if (sqlite3_exec(db, create_sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string err = "Erro ao criar tabela: ";
            err += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(err);
        }
        if (sqlite3_exec(db, create_index, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string err = "Erro ao criar index: ";
            err += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(err);
        }
    }

public:
    SqliteHandler(const std::string &db_path) : db(nullptr) {
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string errMsg = "Erro ao abrir SQLite DB: ";
            errMsg += sqlite3_errmsg(db);
            sqlite3_close(db); 
            throw std::runtime_error(errMsg);
        }
        std::cout << "Memoria (SQLite) aberta: " << db_path << std::endl;
        
        create_table_if_not_exists(); 
    }

    // Delete copy constructor and assignment operator
    SqliteHandler(const SqliteHandler&) = delete;
    SqliteHandler& operator=(const SqliteHandler&) = delete;

    ~SqliteHandler() {
        if (db) {
            sqlite3_close(db);
            std::cout << "Memoria (SQLite) fechada!" << std::endl;
        }
    }

    bool save_memory_to_sqlite(const std::vector<llama_token> &tokens,
                              const std::vector<float> &v_emo,
                              const std::vector<float> &v_time,
                              uint64_t model_hash) {
        
        if (v_time.size() != VTIME_DIM) {
            throw std::runtime_error("Dimensão de v_time deve ser " + std::to_string(VTIME_DIM));
        }

        AlyssaMemHeader header;
        header.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        header.model_hash = model_hash;
        header.n_tokens = tokens.size();
        header.emo_dim = v_emo.size();

        size_t tokens_size_bytes = tokens.size() * sizeof(llama_token);
        size_t emo_size_bytes = v_emo.size() * sizeof(float);
        size_t vtime_size_bytes = v_time.size() * sizeof(float);

        std::vector<uint8_t> blob_data(tokens_size_bytes + emo_size_bytes + vtime_size_bytes);
        
        uint8_t *current_ptr = blob_data.data();
        std::memcpy(current_ptr, tokens.data(), tokens_size_bytes);
        current_ptr += tokens_size_bytes;
        std::memcpy(current_ptr, v_emo.data(), emo_size_bytes);
        current_ptr += emo_size_bytes;
        std::memcpy(current_ptr, v_time.data(), vtime_size_bytes);
        
        const char *sql = "INSERT INTO memories (timestamp, model_hash, n_tokens, emo_dim, tokens) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Erro ao preparar statement SQLite: " + 
                                   std::string(sqlite3_errmsg(db)));
        }
        
        // Use scope guard para garantir que o statement seja finalizado
        auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(stmt, sqlite3_finalize);
        
        sqlite3_bind_int64(stmt, 1, header.timestamp);
        sqlite3_bind_int64(stmt, 2, header.model_hash);
        sqlite3_bind_int(stmt, 3, header.n_tokens);
        sqlite3_bind_int(stmt, 4, header.emo_dim);
        sqlite3_bind_blob(stmt, 5, blob_data.data(), blob_data.size(), SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            throw std::runtime_error("Erro ao Inserir Memoria: " + 
                                   std::string(sqlite3_errmsg(db)));
        }
        
        std::cout << "Memoria inserida no SQLITE (Tokens: " << header.n_tokens 
                  << ", Emo Dim: " << header.emo_dim << ")\n";
        return true;
    }

    std::vector<MemoryMatch> find_similar_memories(const std::vector<float>& v_emo_query, 
                                                  int top_k,
                                                  AlyssaInterprets* interpreter = nullptr) {
        std::vector<MemoryMatch> matches;
        if (v_emo_query.empty()) {
            std::cerr << "Erro: Vetor emocional de busca esta vazio.\n";
            return matches;
        }

        const char *sql = "SELECT id, timestamp, n_tokens, emo_dim, tokens FROM memories;";
        sqlite3_stmt *stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Erro ao preparar busca por similaridade: " << sqlite3_errmsg(db) << "\n";
            return matches;
        }

        auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(stmt, sqlite3_finalize);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            uint64_t timestamp = sqlite3_column_int64(stmt, 1);
            int n_tokens_stored = sqlite3_column_int(stmt, 2);
            int emo_dim_stored = sqlite3_column_int(stmt, 3);
            const uint8_t *blob_data = (const uint8_t*)sqlite3_column_blob(stmt, 4);

            if (emo_dim_stored != v_emo_query.size()) {
                continue; // Silenciosamente ignora dimensões incompatíveis
            }

            size_t tokens_size_bytes = n_tokens_stored * sizeof(llama_token);
            size_t emo_size_bytes = emo_dim_stored * sizeof(float);

            const uint8_t *emo_ptr = blob_data + tokens_size_bytes;

            std::vector<float> v_emo_stored(emo_dim_stored);
            std::memcpy(v_emo_stored.data(), emo_ptr, emo_size_bytes);

            double similarity = calculate_cosine_similarity(v_emo_query, v_emo_stored);

            MemoryMatch match{
                id,
                similarity,
                timestamp,
                static_cast<uint32_t>(n_tokens_stored),
                static_cast<uint32_t>(emo_dim_stored),
                ""
            };

            // Se temos um interpreter, decodificar preview do texto
            if (interpreter && n_tokens_stored > 0) {
                const uint8_t *tokens_ptr = blob_data;
                std::vector<llama_token> preview_tokens(std::min(n_tokens_stored, 10)); // Primeiros 10 tokens
                std::memcpy(preview_tokens.data(), tokens_ptr, 
                           preview_tokens.size() * sizeof(llama_token));
                match.preview_text = interpreter->tokens_to_text(preview_tokens) + "...";
            }

            matches.push_back(match);
        }

        // Ordena os resultados por similaridade (maior para menor)
        std::sort(matches.begin(), matches.end(), [](const MemoryMatch& a, const MemoryMatch& b) {
            return a.similarity > b.similarity;
        });

        // Retorna top_k resultados
        if (matches.size() > static_cast<size_t>(top_k)) {
            matches.resize(top_k);
        }

        return matches;
    }

    bool load_memory_from_sqlite(int id, 
                                AlyssaInterprets &interpreter,
                                std::vector<llama_token> &tokens,
                                std::vector<float> &v_emo,
                                std::vector<float> &v_time) {
        const char *sql = "SELECT timestamp, model_hash, n_tokens, emo_dim, tokens FROM memories WHERE id = ?;";
        sqlite3_stmt *stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Erro ao preparar statement SQLite: " << sqlite3_errmsg(db) << "\n";
            return false;
        }
        
        auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(stmt, sqlite3_finalize);
        
        AlyssaMemHeader header_out;
        sqlite3_bind_int(stmt, 1, id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            header_out.timestamp = sqlite3_column_int64(stmt, 0);
            header_out.model_hash = sqlite3_column_int64(stmt, 1);
            header_out.n_tokens = sqlite3_column_int(stmt, 2);
            header_out.emo_dim = sqlite3_column_int(stmt, 3);
            const uint8_t *blob_data = (const uint8_t*)sqlite3_column_blob(stmt, 4);
            int blob_size = sqlite3_column_bytes(stmt, 4);

            size_t tokens_size_bytes = header_out.n_tokens * sizeof(llama_token);
            size_t emo_size_bytes = header_out.emo_dim * sizeof(float);
            size_t vtime_size_bytes = VTIME_DIM * sizeof(float);

            if (blob_size != tokens_size_bytes + emo_size_bytes + vtime_size_bytes) {
                std::cerr << "Erro de Desserializacao: Tamanho do BLOB inconsistente\n";
                return false;
            }

            tokens.resize(header_out.n_tokens);
            v_emo.resize(header_out.emo_dim);
            v_time.resize(VTIME_DIM);

            const uint8_t *current_ptr = blob_data;
            std::memcpy(tokens.data(), current_ptr, tokens_size_bytes);
            current_ptr += tokens_size_bytes;
            std::memcpy(v_emo.data(), current_ptr, emo_size_bytes);
            current_ptr += emo_size_bytes;
            std::memcpy(v_time.data(), current_ptr, vtime_size_bytes);

            std::cout << "\n----------------------\n";
            std::cout << "Memoria carregada (ID: " << id << ")\n";
            std::cout << "Timestamp: " << Utils::formatTimestamp(header_out.timestamp) << "\n";
            std::cout << "Texto: " << interpreter.tokens_to_text(tokens) << "\n";
            std::cout << "----------------------\n";

            return true;
        } else {
            std::cout << "Memoria com ID " << id << " nao encontrada.\n";
        }
        
        return false;
    }

    // Método para estatísticas do banco
    void print_stats() const {
        const char *sql = "SELECT COUNT(*) as total, AVG(n_tokens) as avg_tokens FROM memories;";
        sqlite3_stmt *stmt = nullptr;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(stmt, sqlite3_finalize);
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int total = sqlite3_column_int(stmt, 0);
                double avg_tokens = sqlite3_column_double(stmt, 1);
                std::cout << "\n=== Estatisticas do Banco ===\n";
                std::cout << "Total de memorias: " << total << "\n";
                std::cout << "Media de tokens: " << avg_tokens << "\n";
            }
        }
    }
};

// --- Main Melhorada ---
int main() {
    llama_backend_init(); 
    
    try {
        const char *model_path = "models/gemma-3-270m-it-F16.gguf"; 
        
        SqliteHandler mem("alyssa_memories.db"); 
        AlyssaInterprets interpreter(model_path, "alyssa.mem", 512); 
        
        // --- Setup de Memórias para Teste ---
        std::vector<llama_token> tokens_base = interpreter.Tokenizer("Eu gosto de programar em C++. Fui dormir tarde hoje.", false);
        uint64_t hash_exemplo = interpreter.ModelHash();

        // Memórias de exemplo com diferentes emoções
        std::vector<std::pair<std::string, std::vector<float>>> memorias_exemplo = {
            {"Eu gosto de programar em C++. Fui dormir tarde hoje.", {0.9f, 0.2f, -0.1f}},
            {"O dia foi estressante e tive problemas no trabalho.", {-0.8f, 0.1f, 0.4f}},
            {"Estudei sobre algoritmos de busca por 3 horas.", {0.1f, 0.1f, 0.1f}},
            {"Que dia maravilhoso! Concluí meu projeto com sucesso!", {0.95f, 0.8f, 0.3f}},
            {"Me sinto cansado e desanimado hoje.", {-0.7f, -0.5f, 0.2f}}
        };

        for (const auto& [texto, emocao] : memorias_exemplo) {
            auto tokens = interpreter.Tokenizer(texto, false);
            mem.save_memory_to_sqlite(tokens, emocao, Utils::getTimeStamp(), hash_exemplo);
        }

        // --- Testando a Busca Contextual ---
        std::cout << "\n\n=== Teste de Busca Contextual (Top 3) ===\n";
        
        std::vector<float> v_emo_busca = {0.95f, 0.0f, 0.0f}; // Busca por positividade
        int top_k = 3;
        
        std::vector<MemoryMatch> resultados = mem.find_similar_memories(v_emo_busca, top_k, &interpreter);

        std::cout << "\nResultado da Busca (v_emo_busca={0.95, 0.0, 0.0}):\n";
        std::cout << std::left << std::setw(5) << "ID" 
                  << std::setw(15) << "Similaridade" 
                  << std::setw(10) << "Tokens" 
                  << std::setw(40) << "Preview"
                  << "Timestamp\n";
        std::cout << "----------------------------------------------------------------------------\n";

        for (const auto& match : resultados) {
            std::cout << std::left 
                      << std::setw(5) << match.id
                      << std::setw(15) << std::fixed << std::setprecision(4) << match.similarity
                      << std::setw(10) << match.n_tokens
                      << std::setw(40) << (match.preview_text.empty() ? "N/A" : match.preview_text.substr(0, 35))
                      << Utils::formatTimestamp(match.timestamp) << "\n";
        }
        std::cout << "============================================================================\n";

        // --- Teste de Carregamento ---
        std::cout << "\n--- Testando Carregar do SQLite (ID 1) ---\n";
        std::vector<llama_token> tok_loaded;
        std::vector<float> emo_loaded, vtime_loaded;

        if (mem.load_memory_from_sqlite(1, interpreter, tok_loaded, emo_loaded, vtime_loaded)) {
            std::cout << "Carregamento bem sucedido!\n";
        }

        // Mostrar estatísticas
        mem.print_stats();

    } catch (const std::exception &e) {
        std::cerr << "Uma excecao fatal ocorreu: " << e.what() << std::endl;
        llama_backend_free();
        return EXIT_FAILURE;
    }

    llama_backend_free();
    return 0;
}