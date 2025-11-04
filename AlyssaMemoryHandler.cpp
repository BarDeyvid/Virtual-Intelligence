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
        
        llama_context *ctx_handle = llama_init_from_model(model, ctx_params); 
        if (!ctx_handle) {
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
    // Headers para o formato de memória Alyssa
    #pragma pack(push, 1)
    struct AlyssaMemHeader {
        uint64_t timestamp;     // segundos desde epoch
        uint64_t model_hash;    // hash do modelo GGUF
        uint32_t n_tokens;      // número de tokens armazenados
        uint32_t emo_dim;       // dimensão do vetor emocional
    };
    #pragma pack(pop)
    sqlite3_stmt *stmt;

public:
    // MUDANÇA: O construtor agora abre o banco de dados SQLite.
    SqliteHandler(const std::string &db_path) : db(nullptr), stmt(nullptr) {
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string errMsg = "Erro ao abrir SQLite DB: ";
            errMsg += sqlite3_errmsg(db);
            sqlite3_close(db); // sqlite3_close() pode ser chamado em um handle com falha
            throw std::runtime_error(errMsg);
        }
        std::cout << "Memoria (SQLite) aberta: " << db_path << std::endl;
    }

    // MUDANÇA: O destrutor agora fecha o banco de dados SQLite.
    ~SqliteHandler() {
        // Finaliza qualquer statement pendente
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        // Fecha o banco de dados
        if (db) {
            sqlite3_close(db);
            std::cout << "Memoria (SQLite) fechada!" << std::endl;
        }
    }

    void retornar(int id) {
        std::cout << "Return Of Value \n" << id;
    };

    // MUDANÇA: Agora aceita AlyssaInterprets& para evitar recarregar o modelo.
    bool load_memory_from_sqlite(int id, 
                                 AlyssaInterprets &interpreter, // Passa por referência
                                 std::vector<llama_token> &tokens,
                                 std::vector<float> &v_emo,
                                 std::vector<float> &v_time) {
        const char *sql = "SELECT timestamp, model_hash, n_tokens, emo_dim, tokens FROM memories WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Erro ao preparar statement SQLite: " << sqlite3_errmsg(db) << "\n";
            return false;
        }
        
        AlyssaMemHeader header_out;
        // MUDANÇA: Removida a criação da 'interpreter'. Estamos usando a que foi passada.

        sqlite3_bind_int(stmt, 1, id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            // 1. Recuperar dados do Header
            header_out.timestamp = sqlite3_column_int64(stmt, 0);
            header_out.model_hash = sqlite3_column_int64(stmt, 1);
            header_out.n_tokens = sqlite3_column_int(stmt, 2);
            header_out.emo_dim = sqlite3_column_int(stmt, 3);

            // 2. Recuperar o BLOB (tokens, emo, v_time) - Na coluna 4
            const uint8_t *blob_data = (const uint8_t*)sqlite3_column_blob(stmt, 4);
            int blob_size = sqlite3_column_bytes(stmt, 4);
            
            const int VTIME_DIM = 2;

            // 3. Deserializar os vetores
            // *** CORREÇÃO DE BUG CRÍTICO AQUI ***
            // Era: header_out.n_tokens = sizeof(llama_token) (Isso estava errado, o idiota esqueceu da logica)
            // Agora: n_tokens * sizeof(llama_token)
            int tokens_size_bytes = header_out.n_tokens * sizeof(llama_token);
            int emo_size_bytes = header_out.emo_dim * sizeof(float);
            int vtime_size_bytes = VTIME_DIM * sizeof(float);

            // Verificacao de tamanho
            if (blob_size != tokens_size_bytes + emo_size_bytes + vtime_size_bytes) {
                std::cerr << "Erro de Desserializacao: Tamanho do BLOB (" << blob_size << " bytes) inconsistente com o Header (" << (tokens_size_bytes + emo_size_bytes + vtime_size_bytes) << " bytes).\n";
                sqlite3_finalize(stmt);
                stmt = nullptr; // Evita double-finalize no destrutor
                return false;
            }

            tokens.resize(header_out.n_tokens);
            v_emo.resize(header_out.emo_dim);
            v_time.resize(VTIME_DIM);

            // Copiar Tokens
            const uint8_t *current_ptr = blob_data;
            std::memcpy(tokens.data(), current_ptr, tokens_size_bytes);

            // Copiar Emocoes
            current_ptr += tokens_size_bytes;
            std::memcpy(v_emo.data(), current_ptr, emo_size_bytes);

            // Copiar V_Time
            current_ptr += emo_size_bytes;
            std::memcpy(v_time.data(), current_ptr, vtime_size_bytes);

            // --- Bloco de Debug ---
            // MUDANÇA: Corrigido para usar 'tokens' em vez de 'tok2' (que estava vazio)
            if (!tokens.empty()) {
                std::cout << "\n----------------------\n";
                std::cout << "Decodificacao para debug\n";
                
                int n_ids = tokens.size() < 10 ? tokens.size() : 10;
                std::cout << "  - IDs (Primeiros " << n_ids << "): ";
                for (size_t i = 0; i < n_ids; i++) {
                    std::cout << tokens[i] << " ";
                }
                std::cout << "\n";

                std::string decoded_text;
                decoded_text.reserve(tokens.size() * 10); 

                for (llama_token t: tokens) {
                    char piece_buf[256];
                    // MUDANÇA: Chamando a função corrigida com o tamanho do buffer
                    int len = interpreter.token_to_piece(t, piece_buf, sizeof(piece_buf));
                    if (len > 0) {
                        decoded_text.append(piece_buf, len);
                    }
                }

                std:: cout << "     - Texto Completo (" << tokens.size() << " tokens):\n";
                std:: cout << "------------------------------------------\n";
                std:: cout << decoded_text << "\n";
                std:: cout << "------------------------------------------\n";
            } else {
                std::cout << "\n Nenhum token carregado.\n";
            }
            // =============================================================
            std::cout << "\n Header .mem:\n";
            std::cout << "  Timestamp: " << header_out.timestamp << "\n";
            std::cout << "  Model Hash: 0x" << std::hex << header_out.model_hash << std::dec << "\n";
            std::cout << "  Tokens: " << header_out.n_tokens << "\n";
            std::cout << "  Emocao Dim: " << header_out.emo_dim << "\n";

            sqlite3_finalize(stmt);
            stmt = nullptr; // Evita double-finalize
            std::cout << "Memoria carregada do SQLite (ID: " << id << ", Tokens: " << header_out.n_tokens << ", Emo Dim:" << header_out.emo_dim << ")\n";
            return true;
        } else {
            std::cout << "Memoria com ID " << id << " nao encontrada.\n";
        }
        
        sqlite3_finalize(stmt);
        stmt = nullptr; // Evita double-finalize
        return false;
    };
};

// --- Main Melhorada ---
// Adicionado try...catch para lidar com as exceções lançadas
int main() {
    try {
        const char *model_path = "models/gemma-3-270m-it-F16.gguf"; 
        std::string save_path = "alyssa.mem";
        
        // MUDANÇA: O construtor agora pode falhar, por isso está no try...catch
        SqliteHandler mem("alyssa_memories.db");
        
        // MUDANÇA: O construtor agora pode falhar
        AlyssaInterprets interpreter(model_path, save_path, 512);
        
        interpreter.Tokenizer("Ola");
        std::cout << "\n" << "\n";

        std::vector<llama_token> tok2;
        std::vector<float> emo2, vtime2;

        // MUDANÇA: Passando a 'interpreter' existente
        mem.load_memory_from_sqlite(1, interpreter, tok2, emo2, vtime2);

    } catch (const std::exception &e) {
        // Captura qualquer std::runtime_error lançado pelos construtores
        std::cerr << "Uma excecao fatal ocorreu: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
} // Destrutores de 'interpreter' e 'mem' são chamados aqui, liberando recursos.