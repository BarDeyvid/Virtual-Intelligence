#include <filesystem>
#include <sqlite3.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring> // Para memcpy
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include "llama.h"
#include <cmath>
#include <stdexcept> // Para std::runtime_error
#include <algorithm> // Para std::sort

// --- Classe Utils Melhorada ---
class Utils {
public:
    static std::vector<float> getTimeStamp() {
        // Timestamp cíclico (hora do dia em formato seno/cosseno)
        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        std::vector<float> v_time = { std::sin(ts / 3600.0f), std::cos(ts / 3600.0f) };
        return v_time;
    };
};

// --- Classe AlyssaInterprets Melhorada ---
class AlyssaInterprets { 
    int ctx;
    llama_model *model;
    llama_context *model_ctx;
    const llama_vocab *vocab;
    llama_model_params model_params;
    llama_context_params ctx_params;

public:
    AlyssaInterprets(const char *model_path, const std::string save_path, int ctx_val)
        : ctx(ctx_val), model(nullptr), model_ctx(nullptr), 
          model_params(llama_model_default_params()), 
          ctx_params(llama_context_default_params())
    {
        ctx_params.n_ctx = ctx;
        std::cout << "Carregando modelo: " << model_path << std::endl;
        
        this->model = llama_model_load_from_file(model_path, model_params);
        if (!model) {
            throw std::runtime_error("❌ Falha ao carregar modelo.");
        }

        this->vocab = llama_model_get_vocab(model); 
        this->model_ctx = llama_init_from_model(model, ctx_params); 
        if (!model_ctx) {
            llama_model_free(model);
            throw std::runtime_error("❌ Falha ao criar contexto.");
        }
    };

    ~AlyssaInterprets() {
        if (model_ctx) {
            llama_free(model_ctx);
        }
        if (model) {
            llama_model_free(model);
        }
        std::cout << "AlyssaInterprets finalizado e recursos liberados.\n";
    }

    uint64_t ModelHash() {
        std::vector<char> desc_vef(ctx);
        llama_model_desc(model, desc_vef.data(), desc_vef.size());
        uint64_t model_hash = std::hash<std::string>{} (std::string(desc_vef.begin(), desc_vef.end()));
        std::cout << "\n Model Hash: 0x" << std::hex << model_hash << std::dec << "\n";
        return model_hash;
    };
    
    std::vector<llama_token> Tokenizer(std::string input) {
        std::vector<float> v_time = Utils::getTimeStamp();
        input = "[TIME:" + std::to_string(v_time[0]) + "," + std::to_string(v_time[1]) + "] " + input;
        
        std::vector<llama_token> tokens(4096);
        int n = llama_tokenize(vocab, input.c_str(), input.size(), tokens.data(), tokens.size(), true, true);
        if (n < 0) {
            throw std::runtime_error("Falha na tokenização (retornou < 0)");
        }
        tokens.resize(n);
        
        std::cout << "\nTokenização concluída (" << n << " tokens):\n";
        for (auto t : tokens) {
            char buf[64];
            int len = llama_token_to_piece(vocab, t, buf, sizeof(buf), 0, true);
            if (len >= 0) {
                std::string piece(buf, len);
                std::cout << "Token ID: " << std::setw(6) << t << " | Token Piece: " << piece << "\n";
            }
        }
        return tokens;
    }

    int32_t token_to_piece(llama_token t, char *piece_buf, size_t buf_size) {
        return llama_token_to_piece(vocab, t, piece_buf, buf_size, 0, true);
    }
};

// Struct para retornar as memórias mais relevantes
struct MemoryMatch {
    int id;
    double similarity;
    uint64_t timestamp;
    uint32_t n_tokens;
    uint32_t emo_dim;
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
    sqlite3_stmt *stmt;

    // Função auxiliar privada: Cálculo da Similaridade de Cosseno
    double calculate_cosine_similarity(const std::vector<float>& v1, const std::vector<float>& v2) {
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
    SqliteHandler(const std::string &db_path) : db(nullptr), stmt(nullptr) {
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

    ~SqliteHandler() {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        if (db) {
            sqlite3_close(db);
            std::cout << "Memoria (SQLite) fechada!" << std::endl;
        }
    }

    // Função para salvar memória no SQLite
    bool save_memory_to_sqlite(const std::vector<llama_token> &tokens,
                               const std::vector<float> &v_emo,
                               const std::vector<float> &v_time,
                               uint64_t model_hash) {
        
        AlyssaMemHeader header;
        header.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        header.model_hash = model_hash;
        header.n_tokens = tokens.size();
        header.emo_dim = v_emo.size();

        const int VTIME_DIM = 2; 
        size_t tokens_size_bytes = tokens.size() * sizeof(llama_token);
        size_t emo_size_bytes = v_emo.size() * sizeof(float);
        size_t vtime_size_bytes = v_time.size() * sizeof(float);

        if (v_time.size() != VTIME_DIM) {
             std::cerr << "Alerta: v_time.size() (" << v_time.size() << ") e diferente de VTIME_DIM (2)\n";
        }

        std::vector<uint8_t> blob_data(tokens_size_bytes + emo_size_bytes + vtime_size_bytes);
        
        uint8_t *current_ptr = blob_data.data();
        std::memcpy(current_ptr, tokens.data(), tokens_size_bytes);
        current_ptr += tokens_size_bytes;
        std::memcpy(current_ptr, v_emo.data(), emo_size_bytes);
        current_ptr += emo_size_bytes;
        std::memcpy(current_ptr, v_time.data(), vtime_size_bytes);
        
        const char *sql = "INSERT INTO memories (timestamp, model_hash, n_tokens, emo_dim, tokens) VALUES (?, ?, ?, ?, ?);";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::string err = "Erro ao preparar statement SQLite: ";
            err += sqlite3_errmsg(db);
            throw std::runtime_error(err);
        }
        
        sqlite3_bind_int64(stmt, 1, header.timestamp);
        sqlite3_bind_int64(stmt, 2, header.model_hash);
        sqlite3_bind_int(stmt, 3, header.n_tokens);
        sqlite3_bind_int(stmt, 4, header.emo_dim);
        sqlite3_bind_blob(stmt, 5, blob_data.data(), blob_data.size(), SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::string err = "Erro ao Inserir Memoria: ";
            err += sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            stmt = nullptr;
            throw std::runtime_error(err);
        }
        
        std::cout << "Memoria inserida no SQLITE (Tokens: " << header.n_tokens << ", Emo Dim: " << header.emo_dim << ")\n";
        sqlite3_finalize(stmt);
        stmt = nullptr; 
        return true;
    };

    // Busca contextual por similaridade emocional
    std::vector<MemoryMatch> find_similar_memories(const std::vector<float>& v_emo_query, int top_k) {
        std::vector<MemoryMatch> matches;
        if (v_emo_query.empty()) {
            std::cerr << "Erro: Vetor emocional de busca esta vazio.\n";
            return matches;
        }

        // Seleciona as colunas necessárias para o cálculo e identificação
        const char *sql = "SELECT id, timestamp, n_tokens, emo_dim, tokens FROM memories;";
        sqlite3_stmt *select_stmt = nullptr;

        if (sqlite3_prepare_v2(db, sql, -1, &select_stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Erro ao preparar busca por similaridade: " << sqlite3_errmsg(db) << "\n";
            return matches;
        }

        // Itera sobre todos os registros
        while (sqlite3_step(select_stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(select_stmt, 0);
            uint64_t timestamp = sqlite3_column_int64(select_stmt, 1);
            int n_tokens_stored = sqlite3_column_int(select_stmt, 2);
            int emo_dim_stored = sqlite3_column_int(select_stmt, 3);
            const uint8_t *blob_data = (const uint8_t*)sqlite3_column_blob(select_stmt, 4);

            if (emo_dim_stored != v_emo_query.size()) {
                std::cerr << "Aviso: Memória ID " << id << " ignorada (Dimensoes incompativeis: " << emo_dim_stored << " != " << v_emo_query.size() << ").\n";
                continue;
            }

            // 1. Calcular offsets para a seção v_emo dentro do BLOB
            size_t tokens_size_bytes = n_tokens_stored * sizeof(llama_token);
            size_t emo_size_bytes = emo_dim_stored * sizeof(float);

            // 2. Ponteiro para o início do vetor emocional armazenado
            const uint8_t *emo_ptr = blob_data + tokens_size_bytes;

            // 3. Copia o vetor emocional do BLOB para um vetor C++ temporário
            std::vector<float> v_emo_stored(emo_dim_stored);
            std::memcpy(v_emo_stored.data(), emo_ptr, emo_size_bytes);

            // 4. Calcula a similaridade
            double similarity = calculate_cosine_similarity(v_emo_query, v_emo_stored);

            // 5. Armazena o resultado
            matches.push_back({
                id,
                similarity,
                timestamp,
                static_cast<uint32_t>(n_tokens_stored),
                static_cast<uint32_t>(emo_dim_stored)
            });
        }

        sqlite3_finalize(select_stmt);

        // Ordena os resultados por similaridade (maior para menor)
        std::sort(matches.begin(), matches.end(), [](const MemoryMatch& a, const MemoryMatch& b) {
            return a.similarity > b.similarity;
        });

        // Retorna top_k resultados
        if (matches.size() > top_k) {
            matches.resize(top_k);
        }

        return matches;
    }

    bool save_memory_to_file(const std::string &path,
                             const std::vector<llama_token> &tokens,
                             const std::vector<float> &v_emo,
                             const std::vector<float> &v_time,
                             uint64_t model_hash) {
        
        AlyssaMemHeader header;
        header.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        header.model_hash = model_hash;
        header.n_tokens = tokens.size();
        header.emo_dim = v_emo.size();

        std::ofstream out(path, std::ios::binary);
        if (!out) {
            std::cerr << "Falha ao abrir arquivo para escrita: " << path << "\n";
            return false;
        }

        out.write(reinterpret_cast<char*>(&header), sizeof(header));
        out.write(reinterpret_cast<const char*>(tokens.data()), tokens.size() * sizeof(llama_token));
        out.write(reinterpret_cast<const char*>(v_emo.data()), v_emo.size() * sizeof(float));
        out.write(reinterpret_cast<const char*>(v_time.data()), v_time.size() * sizeof(float));
        out.close();

        std::cout << "Memoria salva em ARQUIVO: " << path << "\n";
        return true;
    }

    bool load_memory_from_sqlite(int id, 
                                 AlyssaInterprets &interpreter,
                                 std::vector<llama_token> &tokens,
                                 std::vector<float> &v_emo,
                                 std::vector<float> &v_time) {
        const char *sql = "SELECT timestamp, model_hash, n_tokens, emo_dim, tokens FROM memories WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Erro ao preparar statement SQLite: " << sqlite3_errmsg(db) << "\n";
            return false;
        }
        
        AlyssaMemHeader header_out;
        sqlite3_bind_int(stmt, 1, id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            header_out.timestamp = sqlite3_column_int64(stmt, 0);
            header_out.model_hash = sqlite3_column_int64(stmt, 1);
            header_out.n_tokens = sqlite3_column_int(stmt, 2);
            header_out.emo_dim = sqlite3_column_int(stmt, 3);
            const uint8_t *blob_data = (const uint8_t*)sqlite3_column_blob(stmt, 4);
            int blob_size = sqlite3_column_bytes(stmt, 4);
            const int VTIME_DIM = 2;
            int tokens_size_bytes = header_out.n_tokens * sizeof(llama_token);
            int emo_size_bytes = header_out.emo_dim * sizeof(float);
            int vtime_size_bytes = VTIME_DIM * sizeof(float);

            if (blob_size != tokens_size_bytes + emo_size_bytes + vtime_size_bytes) {
                std::cerr << "Erro de Desserializacao: Tamanho do BLOB (" << blob_size << " bytes) inconsistente com o Header (" << (tokens_size_bytes + emo_size_bytes + vtime_size_bytes) << " bytes).\n";
                sqlite3_finalize(stmt);
                stmt = nullptr;
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

            // --- Bloco de Debug ---
            if (!tokens.empty()) {
                std::cout << "\n----------------------\n";
                std::cout << "Decodificacao para debug (ID: " << id << ")\n";
                std::string decoded_text;
                for (llama_token t: tokens) {
                    char piece_buf[256];
                    int len = interpreter.token_to_piece(t, piece_buf, sizeof(piece_buf));
                    if (len > 0) decoded_text.append(piece_buf, len);
                }
                std:: cout << "     - Texto Completo (" << tokens.size() << " tokens):\n";
                std:: cout << "------------------------------------------\n";
                std:: cout << decoded_text << "\n";
                std:: cout << "------------------------------------------\n";
            }
            // =============================================================

            sqlite3_finalize(stmt);
            stmt = nullptr; 
            std::cout << "Memoria carregada do SQLite (ID: " << id << ")\n";
            return true;
        } else {
            std::cout << "Memoria com ID " << id << " nao encontrada.\n";
        }
        
        sqlite3_finalize(stmt);
        stmt = nullptr;
        return false;
    };
};

// --- Main (Atualizada para testar as novas funções) ---
int main() {
    llama_backend_init(); 
    
    try {
        const char *model_path = "models/gemma-3-270m-it-F16.gguf"; 
        std::string save_path = "alyssa.mem";
        
        SqliteHandler mem("alyssa_memories.db"); 
        // O AlyssaInterprets irá falhar se o arquivo do modelo não existir.
        AlyssaInterprets interpreter(model_path, save_path, 512); 
        
        // --- Setup de Memórias para Teste de Similaridade ---
        std::vector<llama_token> tokens_base = interpreter.Tokenizer("Eu gosto de programar em C++. Fui dormir tarde hoje.");
        uint64_t hash_exemplo = interpreter.ModelHash();

        // 1. Memória 1 (Foco na Positividade/Energia)
        std::vector<float> v_emo_1 = { 0.9f, 0.2f, -0.1f }; // Alta Positividade (Primeira dimensão)
        mem.save_memory_to_sqlite(tokens_base, v_emo_1, Utils::getTimeStamp(), hash_exemplo);
        
        // 2. Memória 2 (Foco na Negatividade/Cansaço)
        std::vector<llama_token> tokens_2 = interpreter.Tokenizer("O dia foi estressante e tive problemas no trabalho.");
        std::vector<float> v_emo_2 = { -0.8f, 0.1f, 0.4f }; // Alta Negatividade
        mem.save_memory_to_sqlite(tokens_2, v_emo_2, Utils::getTimeStamp(), hash_exemplo);

        // 3. Memória 3 (Foco na Neutralidade/Programação)
        std::vector<llama_token> tokens_3 = interpreter.Tokenizer("Estudei sobre algoritmos de busca por 3 horas.");
        std::vector<float> v_emo_3 = { 0.1f, 0.1f, 0.1f }; // Neutro
        mem.save_memory_to_sqlite(tokens_3, v_emo_3, Utils::getTimeStamp(), hash_exemplo);

        // --- Testando a Busca Contextual ---
        std::cout << "\n\n=== Teste de Busca Contextual (Top 2) ===\n";
        
        // Vetor de busca: Alta Positividade, similar ao v_emo_1
        std::vector<float> v_emo_busca = { 0.95f, 0.0f, 0.0f }; 
        int top_k = 2;
        
        std::vector<MemoryMatch> resultados = mem.find_similar_memories(v_emo_busca, top_k);

        std::cout << "\nResultado da Busca (v_emo_busca={0.95, 0.0, 0.0}):\n";
        std::cout << std::left << std::setw(5) << "ID" << std::setw(15) << "Similaridade" << std::setw(10) << "Tokens" << "Emo Dim\n";
        std::cout << "-------------------------------------------\n";

        for (const auto& match : resultados) {
            std::cout << std::left 
                      << std::setw(5) << match.id
                      << std::setw(15) << std::fixed << std::setprecision(4) << match.similarity
                      << std::setw(10) << match.n_tokens
                      << match.emo_dim << "\n";
        }
        std::cout << "===========================================\n";

        // --- Teste de Carregamento (para provar que a ID 1 funciona) ---
        std::cout << "\n--- Testando Carregar do SQLite (ID 1) ---\n";
        std::vector<llama_token> tok_loaded;
        std::vector<float> emo_loaded, vtime_loaded;

        mem.load_memory_from_sqlite(1, interpreter, tok_loaded, emo_loaded, vtime_loaded);


    } catch (const std::exception &e) {
        std::cerr << "Uma excecao fatal ocorreu: " << e.what() << std::endl;
        llama_backend_free();
        return EXIT_FAILURE;
    }

    llama_backend_free();
    return 0;
}