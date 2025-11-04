#include <sqlite3.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include "llama.h"

// Estrutura do header AlyssaMemHeader
#pragma pack(push, 1)
struct AlyssaMemHeader {
    uint64_t timestamp;
    uint64_t model_hash;
    uint32_t n_tokens;
    uint32_t emo_dim;
};
#pragma pack(pop)

// Carrega um .mem para memória
std::vector<uint8_t> load_binary(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Falha ao abrir .mem");
    std::vector<uint8_t> data(std::filesystem::file_size(path));
    in.read(reinterpret_cast<char*>(data.data()), data.size());
    return data;
}

void insert_memory(sqlite3 *db, const std::string &path) {
    auto data = load_binary(path);
    AlyssaMemHeader header;
    memcpy(&header, data.data(), sizeof(AlyssaMemHeader));

    const void *blob_data = data.data() + sizeof(AlyssaMemHeader);
    size_t blob_size = data.size() - sizeof(AlyssaMemHeader);

    sqlite3_stmt *stmt;
    // Inclui n_tokens e emo_dim (4 colunas agora)
    const char *sql =
        "INSERT INTO memories (timestamp, model_hash, n_tokens, emo_dim, tokens) VALUES (?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("Erro ao preparar statement SQLite");

    // 1. Timestamp
    sqlite3_bind_int64(stmt, 1, header.timestamp);
    // 2. Model Hash
    sqlite3_bind_int64(stmt, 2, header.model_hash);
    // 3. n_tokens 
    sqlite3_bind_int(stmt, 3, header.n_tokens);
    // 4. emo_dim 
    sqlite3_bind_int(stmt, 4, header.emo_dim);
    // 5. Tokens (BLOB)
    sqlite3_bind_blob(stmt, 5, blob_data, blob_size, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        throw std::runtime_error("Erro ao inserir memória");

    sqlite3_finalize(stmt);
    std::cout << "💾 Memória inserida no banco com sucesso (Tokens: " << header.n_tokens << ", Emo Dim: " << header.emo_dim << ").\n";
}

void select_memory(sqlite3 *db, int id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT timestamp, model_hash, tokens FROM memories WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("Erro ao preparar statement SQLite");

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t timestamp = sqlite3_column_int64(stmt, 0);
        const unsigned char *model_hash = sqlite3_column_text(stmt, 1);
        const void *tokens = sqlite3_column_blob(stmt, 2);
        int token_size = sqlite3_column_bytes(stmt, 2);

        std::cout << "Timestamp: " << timestamp << "\n";
        std::cout << "Model Hash: " << model_hash << "\n";
        std::cout << "Token Size: " << token_size << " bytes\n";
    } else {
        std::cout << "Memória com ID " << id << " não encontrada.\n";
    }

    sqlite3_finalize(stmt);
}


int main() {
    sqlite3 *db;
    if (sqlite3_open("alyssa_memories.db", &db))
        throw std::runtime_error("Erro ao abrir banco SQLite");

    // Cria tabela se não existir
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

    char *errMsg = nullptr;
    if (sqlite3_exec(db, create_sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Erro ao criar tabela: " << errMsg << "\n";
        sqlite3_free(errMsg);
    }

    insert_memory(db, "alyssa.mem");

    select_memory(db, 1);

    sqlite3_close(db);
    return 0;
}
