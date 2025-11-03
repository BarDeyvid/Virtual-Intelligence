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

// Insere no SQLite
void insert_memory(sqlite3 *db, const std::string &path) {
    auto data = load_binary(path);
    AlyssaMemHeader header;
    memcpy(&header, data.data(), sizeof(AlyssaMemHeader));

    const void *blob_data = data.data() + sizeof(AlyssaMemHeader);
    size_t blob_size = data.size() - sizeof(AlyssaMemHeader);

    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT INTO memories (timestamp, model_hash, tokens) VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("Erro ao preparar statement SQLite");

    sqlite3_bind_int64(stmt, 1, header.timestamp);
    sqlite3_bind_text(stmt, 2, std::to_string(header.model_hash).c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, blob_data, blob_size, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        throw std::runtime_error("Erro ao inserir memória");

    sqlite3_finalize(stmt);
    std::cout << "💾 Memória inserida no banco com sucesso.\n";
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
            model_hash TEXT,
            tokens BLOB
        );
    )SQL";

    char *errMsg = nullptr;
    if (sqlite3_exec(db, create_sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Erro ao criar tabela: " << errMsg << "\n";
        sqlite3_free(errMsg);
    }

    insert_memory(db, "alyssa.mem");

    sqlite3_close(db);
    return 0;
}
