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

// --- Classe Utils Melhorada ---
// Não há necessidade de uma instância se ela só tem métodos utilitários.
// Usar 'static' é mais eficiente.
class Utils {
public:
    // Não precisamos de construtor ou destrutor se a classe for puramente estática
    
    static std::vector<float> getTimeStamp() {
        // Timestamp cíclico
        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        std::vector<float> v_time = { std::sin(ts / 3600.0f), std::cos(ts / 3600.0f) };
        return v_time;
    };
};

// --- Classe AlyssaInterprets Melhorada ---
// Adicionado gerenciamento de recursos RAII (Destrutor) e tratamento de exceções.
class AlyssaInterprets { 
    int ctx;
    llama_model *model; // Mover para membro da classe para gerenciar o ciclo de vida
    llama_context *model_ctx;
    const llama_vocab *vocab;
    llama_model_params model_params;
    llama_context_params ctx_params;

public:
    // O 'save_path' não estava sendo usado, mas mantive na assinatura
    AlyssaInterprets(const char *model_path, const std::string save_path, int ctx_val)
        : ctx(ctx_val), model(nullptr), model_ctx(nullptr), 
          model_params(llama_model_default_params()), 
          ctx_params(llama_context_default_params())
    {
        ctx_params.n_ctx = ctx;
        std::cout << "Carregando modelo: " << model_path << std::endl;
        
        this->model = llama_model_load_from_file(model_path, model_params);
        if (!model) {
            // MUDANÇA: Usar exceções é o padrão C++. 'exit()' é ruim.
            throw std::runtime_error("❌ Falha ao carregar modelo.");
        }

        this->vocab = llama_model_get_vocab(model); 
        this->model_ctx = llama_init_from_model(model, ctx_params); 
        // Duplicata Removida
        if (!model_ctx) {
            // Se a criação do contexto falhar, devemos liberar o modelo antes de sair.
            llama_model_free(model); // Limpa o recurso alocado
            throw std::runtime_error("❌ Falha ao criar contexto.");
        }
    };

    // Destrutor para RAII: Isso garante que os recursos da LLaMA sejam liberados quando o objeto sair de escopo.
    ~AlyssaInterprets() {
        if (model_ctx) {
            llama_free(model_ctx);
        }
        if (model) {
            llama_model_free(model);
        }
        std::cout << "AlyssaInterprets finalizado e recursos liberados.\n";
    }

    // MUDANÇA: Agora usa o 'this->model'
    uint64_t ModelHash() {
        std::vector<char> desc_vef(ctx);
        llama_model_desc(model, desc_vef.data(), desc_vef.size());
        uint64_t model_hash = std::hash<std::string>{} (std::string(desc_vef.begin(), desc_vef.end()));
        std::cout << "\n Model Hash: 0x" << std::hex << model_hash << std::dec << "\n";
        return model_hash;
    };
    
    std::vector<llama_token> Tokenizer(std::string input) {
        // MUDANÇA: Usando o método estático
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
            // MUDANÇA: Passando o tamanho do buffer corretamente
            int len = llama_token_to_piece(vocab, t, buf, sizeof(buf), 0, true);
            if (len >= 0) {
                std::string piece(buf, len);
                std::cout << "Token ID: " << std::setw(6) << t << " | Token Piece: " << piece << "\n";
            }
        }
        return tokens;
    }

    // MUDANÇA: Corrigido o bug 'sizeof(char*)'. 
    int32_t token_to_piece(llama_token t, char *piece_buf, size_t buf_size) {
        return llama_token_to_piece(vocab, t, piece_buf, buf_size, 0, true);
    }
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

    // Função privada para garantir que a tabela exista
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
            )SQL"; // 

        char *errMsg = nullptr;
        if (sqlite3_exec(db, create_sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string err = "Erro ao criar tabela: ";
            err += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(err); // Lança exceção
        }
        if (sqlite3_exec(db, create_index, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string err = "Erro ao criar index: ";
            err += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(err); // Lança exceção
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
        
        // Garante que a tabela exista assim que o handler for criado
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

    // Sua função, está perfeita.
    std::vector<uint8_t> load_binary(const std::string &path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("Falha ao abrir .mem: " + path);
        
        // Use std::filesystem::file_size para segurança
        size_t file_size = std::filesystem::file_size(path);
        std::vector<uint8_t> data(file_size);
        
        in.read(reinterpret_cast<char*>(data.data()), data.size());
        return data;
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


    // MUDANÇA: Função 'save_memory_to_sqlite' refatorada
    bool save_memory_to_sqlite(const std::vector<llama_token> &tokens,
                               const std::vector<float> &v_emo,
                               const std::vector<float> &v_time,
                               uint64_t model_hash) {
        
        AlyssaMemHeader header;
        header.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        header.model_hash = model_hash;
        header.n_tokens = tokens.size();
        header.emo_dim = v_emo.size();

        // 1. Em vez de salvar em disco, criamos o BLOB em um vetor na memória
        const int VTIME_DIM = 2; 
        size_t tokens_size_bytes = tokens.size() * sizeof(llama_token);
        size_t emo_size_bytes = v_emo.size() * sizeof(float);
        size_t vtime_size_bytes = v_time.size() * sizeof(float); // Mais seguro que usar VTIME_DIM

        if (v_time.size() != VTIME_DIM) {
             std::cerr << "Alerta: v_time.size() (" << v_time.size() << ") e diferente de VTIME_DIM (2)\n";
        }

        std::vector<uint8_t> blob_data(tokens_size_bytes + emo_size_bytes + vtime_size_bytes);
        
        // 2. Copiamos os dados para o vetor (nosso BLOB)
        uint8_t *current_ptr = blob_data.data();
        std::memcpy(current_ptr, tokens.data(), tokens_size_bytes);
        current_ptr += tokens_size_bytes;
        std::memcpy(current_ptr, v_emo.data(), emo_size_bytes);
        current_ptr += emo_size_bytes;
        std::memcpy(current_ptr, v_time.data(), vtime_size_bytes);
        
        // 3. Agora, inserimos no SQLite
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
        sqlite3_bind_blob(stmt, 5, blob_data.data(), blob_data.size(), SQLITE_STATIC); // Usamos o vetor

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::string err = "Erro ao Inserir Memoria: ";
            err += sqlite3_errmsg(db);
            sqlite3_finalize(stmt); // Limpa antes de lançar
            stmt = nullptr;
            throw std::runtime_error(err);
        }
        
        std::cout << "Memoria inserida no SQLITE (Tokens: " << header.n_tokens << ", Emo Dim: " << header.emo_dim << ")\n";
        sqlite3_finalize(stmt);
        stmt = nullptr; 
        return true;
    };

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
    // Inicializa a biblioteca LLaMA (boa prática)
    llama_backend_init(); 
    
    try {
        const char *model_path = "models/gemma-3-270m-it-F16.gguf"; 
        std::string save_path = "alyssa.mem";
        
        // O SqliteHandler agora criará a tabela com a query SQL corrigida.
        SqliteHandler mem("alyssa_memories.db"); 
        AlyssaInterprets interpreter(model_path, save_path, 512);
        
        std::cout << "\n--- Testando Tokenizer ---\n";
        std::vector<llama_token> tokens_ola = interpreter.Tokenizer("Ola, este e um teste de persistencia de memoria.");
        std::cout << "\n--- Fim do Teste Tokenizer ---\n\n";

        // Vamos criar dados de exemplo para salvar
        std::vector<float> v_emo_exemplo = { 0.1f, 0.8f, -0.2f }; // Emoção: 3 dimensões
        std::vector<float> v_time_exemplo = Utils::getTimeStamp(); // Pega o tempo atual
        uint64_t hash_exemplo = interpreter.ModelHash(); // Pega o hash do modelo

        // --- Testando as funções de salvar ---
        std::cout << "--- Testando Salvar em Arquivo ---\n";
        mem.save_memory_to_file("teste.mem", tokens_ola, v_emo_exemplo, v_time_exemplo, hash_exemplo);
        
        std::cout << "\n--- Testando Salvar em SQLite ---\n";
        mem.save_memory_to_sqlite(tokens_ola, v_emo_exemplo, v_time_exemplo, hash_exemplo);
        
        // --- Testando o carregamento ---
        std::cout << "\n--- Testando Carregar do SQLite (ID 1) ---\n";
        std::vector<llama_token> tok2;
        std::vector<float> emo2, vtime2;

        mem.load_memory_from_sqlite(1, interpreter, tok2, emo2, vtime2);

    } catch (const std::exception &e) {
        std::cerr << "Uma excecao fatal ocorreu: " << e.what() << std::endl;
        llama_backend_free(); // Libera o backend em caso de erro
        return EXIT_FAILURE;
    }

    // Libera a biblioteca LLaMA
    llama_backend_free();
    return 0;
}