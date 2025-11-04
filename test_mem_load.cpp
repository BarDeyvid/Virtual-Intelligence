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

// Headers para o formato de memória Alyssa
#pragma pack(push, 1)
struct AlyssaMemHeader {
    uint64_t timestamp;     // segundos desde epoch
    uint64_t model_hash;    // hash do modelo GGUF
    uint32_t n_tokens;      // número de tokens armazenados
    uint32_t emo_dim;       // dimensão do vetor emocional
};
#pragma pack(pop)

bool load_memory_from_sqlite(sqlite3 *db, int id,
                             std::vector<llama_token> &tokens,
                             std::vector<float> &v_emo,
                             std::vector<float> &v_time,
                             AlyssaMemHeader &header_out) {

    sqlite3_stmt *stmt;
    // 📢 ATUALIZADO: A query SQL agora seleciona n_tokens e emo_dim
    const char *sql = "SELECT timestamp, model_hash, n_tokens, emo_dim, tokens FROM memories WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "❌ Erro ao preparar statement SQLite: " << sqlite3_errmsg(db) << "\n";
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // 1. Recuperar dados do Header a partir das colunas (nova ordem)
        header_out.timestamp = sqlite3_column_int64(stmt, 0);
        // Converte a hash de TEXT (string) de volta para uint64_t
        header_out.model_hash = sqlite3_column_int64(stmt, 1);
        
        // NOVO: Lê n_tokens e emo_dim diretamente do DB
        header_out.n_tokens = sqlite3_column_int(stmt, 2);
        header_out.emo_dim = sqlite3_column_int(stmt, 3);
        
        // 2. Recuperar o BLOB (tokens, emo, v_time) - Agora na coluna 4
        const uint8_t *blob_data = (const uint8_t*)sqlite3_column_blob(stmt, 4);
        int blob_size = sqlite3_column_bytes(stmt, 4);

        // VTime é o único fixo (sin/cos de timestamp)
        const int VTIME_DIM = 2;
        
        // 3. Deserializar os vetores usando os tamanhos lidos do DB
        int tokens_size_bytes = header_out.n_tokens * sizeof(llama_token);
        int emo_size_bytes = header_out.emo_dim * sizeof(float);
        int vtime_size_bytes = VTIME_DIM * sizeof(float);
        
        // Verificação de tamanho (melhora a robustez)
        if (blob_size != tokens_size_bytes + emo_size_bytes + vtime_size_bytes) {
            std::cerr << "❌ Erro de Desserialização: Tamanho do BLOB (" << blob_size << " bytes) inconsistente com o Header (" << tokens_size_bytes + emo_size_bytes + vtime_size_bytes << " bytes).\n";
            sqlite3_finalize(stmt);
            return false;
        }

        tokens.resize(header_out.n_tokens);
        v_emo.resize(header_out.emo_dim);
        v_time.resize(VTIME_DIM);

        // Copiar Tokens
        const uint8_t *current_ptr = blob_data;
        std::memcpy(tokens.data(), current_ptr, tokens_size_bytes);

        // Copiar Emoções
        current_ptr += tokens_size_bytes;
        std::memcpy(v_emo.data(), current_ptr, emo_size_bytes);

        // Copiar V_Time
        current_ptr += emo_size_bytes;
        std::memcpy(v_time.data(), current_ptr, vtime_size_bytes);

        sqlite3_finalize(stmt);
        std::cout << "📂 Memória carregada do SQLite (ID: " << id << ", Tokens: " << header_out.n_tokens << ", Emo Dim: " << header_out.emo_dim << ")\n";
        return true;
    } else {
        std::cout << "❌ Memória com ID " << id << " não encontrada.\n";
    }

    sqlite3_finalize(stmt);
    return false;
}

int main() {
    const char *model_path = "models/gemma-3-270m-it-F16.gguf";
    std::string save_path = "alyssa_memories.db";

    llama_model_params model_params = llama_model_default_params();
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 512;

    std::cout << "🧠 Carregando modelo: " << model_path << std::endl;
    llama_model *model = llama_model_load_from_file(model_path, model_params);
    if (!model) {
        std::cerr << "❌ Falha ao carregar modelo.\n";
        return 1;
    }

    const llama_vocab *vocab = llama_model_get_vocab(model);
    llama_context *ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        std::cerr << "❌ Falha ao criar contexto.\n";
        llama_model_free(model);
        return 1;
    }

    sqlite3 *db;
    if (sqlite3_open("alyssa_memories.db", &db)) {
        std::cerr << "❌ Erro ao abrir banco SQLite\n";
        return 1;
    }

    // Carrega do SQLite
    AlyssaMemHeader header;
    std::vector<llama_token> tok2;
    std::vector<float> emo2, vtime2;
    if (load_memory_from_sqlite(db, 2, tok2, emo2, vtime2, header)) {
        std::cout << "✅ Memória carregada do SQLite com sucesso.\n";
    } else {
        std::cerr << "❌ Falha ao carregar memória do SQLite.\n";
    }
    
    char buf[256];
    
    // ==========================================================
    // NOVO BLOCO DE DECODIFICAÇÃO COMPLETA PARA DEBUG
    // ==========================================================
    if (!tok2.empty()) {
        std::cout << "\n----------------------------------------\n";
        std::cout << "🧩 DECODIFICAÇÃO PARA DEBUG\n";
        
        // 1. Imprime os primeiros 10 IDs de tokens
        int n_ids = tok2.size() < 10 ? tok2.size() : 10;
        std::cout << "   - IDs (Primeiros " << n_ids << "): ";
        for (size_t i = 0; i < n_ids; i++) {
            std::cout << tok2[i] << " ";
        }
        std::cout << "\n";

        // 2. Constrói o texto completo decodificado
        std::string decoded_text;
        decoded_text.reserve(tok2.size() * 10); // Pré-aloca espaço estimado
        
        for (llama_token t : tok2) {
            // Buffer temporário para o pedaço do token
            char piece_buf[256];
            // Decodifica o token para bytes. 'len' é o número de bytes escritos.
            int len = llama_token_to_piece(vocab, t, piece_buf, sizeof(piece_buf), 0, true); 
            
            // Adiciona o pedaço do token à string final
            if (len > 0) {
                decoded_text.append(piece_buf, len);
            }
        }
        
        // 3. Imprime o texto completo
        std::cout << "   - Texto Completo (" << tok2.size() << " tokens):\n";
        std::cout << "----------------------------------------\n";
        std::cout << decoded_text << "\n";
        std::cout << "----------------------------------------\n";

    } else {
        std::cout << "\n🧩 Nenhum token carregado.\n";
    }
    // ==========================================================

    std::cout << "\n🧩 Header .mem:\n";
    std::cout << "  Timestamp: " << header.timestamp << "\n";
    std::cout << "  Model Hash: 0x" << std::hex << header.model_hash << std::dec << "\n";
    std::cout << "  Tokens: " << header.n_tokens << "\n";
    std::cout << "  Emoção Dim: " << header.emo_dim << "\n";

    llama_free(ctx);
    llama_model_free(model);
    std::cout << "\n🧹 Finalizado com sucesso!\n";
    return 0;
}
