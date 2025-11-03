#include "llama.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <random>
#include <chrono>
#include <filesystem>
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

// 🔹 Salvar memória Alyssa (.mem)
void save_memory(const std::string &path,
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
    out.write(reinterpret_cast<char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(tokens.data()), tokens.size() * sizeof(llama_token));
    out.write(reinterpret_cast<const char*>(v_emo.data()), v_emo.size() * sizeof(float));
    out.write(reinterpret_cast<const char*>(v_time.data()), v_time.size() * sizeof(float));
    out.close();

    std::cout << "💾 Memória salva em: " << path << "\n";
}

// 🔹 Carregar memória Alyssa (.mem)
void load_memory(const std::string &path,
                 std::vector<llama_token> &tokens,
                 std::vector<float> &v_emo,
                 std::vector<float> &v_time,
                 AlyssaMemHeader &header_out) {

    std::ifstream in(path, std::ios::binary);
    in.read(reinterpret_cast<char*>(&header_out), sizeof(AlyssaMemHeader));

    tokens.resize(header_out.n_tokens);
    v_emo.resize(header_out.emo_dim);
    v_time.resize(2);

    in.read(reinterpret_cast<char*>(tokens.data()), tokens.size() * sizeof(llama_token));
    in.read(reinterpret_cast<char*>(v_emo.data()), v_emo.size() * sizeof(float));
    in.read(reinterpret_cast<char*>(v_time.data()), v_time.size() * sizeof(float));
    in.close();

    std::cout << "📂 Memória carregada de: " << path << "\n";
}

// 🔹 Gera vetor emocional aleatório
std::vector<float> generate_emotion_vector(int dim = 8) {
    std::vector<float> v(dim);
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (float &x : v) x = dist(rng);
    return v;
}

int main() {
    const char *model_path = "models/gemma-3-270m-it-F16.gguf";
    std::string save_path = "alyssa.mem";

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

    // Gera hash do modelo
    char desc_buf[512];
    llama_model_desc(model, desc_buf, sizeof(desc_buf));
    uint64_t model_hash = std::hash<std::string>{}(std::string(desc_buf));
    std::cout << "\n🔗 Modelo hash: 0x" << std::hex << model_hash << std::dec << "\n";

    // Timestamp cíclico
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::vector<float> v_time = { std::sin(ts / 3600.0f), std::cos(ts / 3600.0f) };

    // Entrada do usuário
    std::cout << "\n🗨️  Digite uma frase para tokenizar:\n> ";
    std::string input;
    std::getline(std::cin, input);
    input = "[TIME:" + std::to_string(v_time[0]) + "," + std::to_string(v_time[1]) + "] " + input;

    // Tokenização
    std::vector<llama_token> tokens(4096);
    int n = llama_tokenize(vocab, input.c_str(), input.size(), tokens.data(), tokens.size(), true, true);
    tokens.resize(n);

    std::cout << "\n✅ Tokenização concluída (" << n << " tokens):\n";
    for (auto t : tokens) {
        char buf[64];
        int len = llama_token_to_piece(vocab, t, buf, sizeof(buf), 0, true);
        std::string piece(buf, len);
        std::cout << "Token ID: " << std::setw(6) << t << " | Token Piece: " << piece << "\n";
    }

    auto emo = generate_emotion_vector(8);

    // Salva .mem
    save_memory(save_path, tokens, emo, v_time, model_hash);

    // Carrega e imprime .mem
    AlyssaMemHeader header;
    std::vector<llama_token> tok2;
    std::vector<float> emo2, vtime2;
    load_memory(save_path, tok2, emo2, vtime2, header);

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
