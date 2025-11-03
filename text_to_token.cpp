#include "llama.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <random>
#include <chrono>
#include <filesystem>

// 🔹 Função utilitária para salvar tokens em arquivo binário
void save_tokens(const std::vector<llama_token> &tokens, const std::string &path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Erro ao abrir arquivo para salvar tokens.");
    out.write(reinterpret_cast<const char *>(tokens.data()), tokens.size() * sizeof(llama_token));
    std::cout << "💾 Tokens salvos em: " << path << " (" << tokens.size() << " tokens)\n";
}

// 🔹 Função utilitária para carregar tokens do arquivo binário
std::vector<llama_token> load_tokens(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Erro ao abrir arquivo para ler tokens.");
    std::vector<llama_token> tokens(
        std::filesystem::file_size(path) / sizeof(llama_token)
    );
    in.read(reinterpret_cast<char *>(tokens.data()), tokens.size() * sizeof(llama_token));
    std::cout << "📂 Tokens recarregados: " << tokens.size() << " tokens\n";
    return tokens;
}

// 🔹 Gera vetor emocional aleatório
std::vector<float> generate_emotion_vector(int dim = 8) {
    std::vector<float> v(dim);
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (float &x : v) x = dist(rng);
    return v;
}

int main(int argc, char **argv) {
    const char *model_path = "models/gemma-3-270m-it-F16.gguf"; // Modelo 270M em F16 para prototipagem rapida
    std::string save_path = "tokens.bin";

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

    // 🔹 Entrada do usuário
    std::cout << "\n🗨️  Digite uma frase para tokenizar:\n> ";
    std::string input;
    std::getline(std::cin, input);

    // Tokenização
    std::vector<llama_token> tokens(4096);
    int n = llama_tokenize(vocab, input.c_str(), input.size(), tokens.data(), tokens.size(), true, true);
    if (n < 0) {
        std::cerr << "❌ Erro na tokenização.\n";
        llama_free(ctx);
        llama_model_free(model);
        return 1;
    }
    tokens.resize(n);

    std::cout << "\n✅ Tokenização concluída (" << n << " tokens):\n";
    for (auto t : tokens) {
        char buf[64];
        int len = llama_token_to_piece(vocab, t, buf, sizeof(buf), 0, true);
        std::string piece(buf, len);
        std::cout << "Token ID: " << std::setw(6) << t
                  << " | Token Piece: " << piece << "\n";
    }

    // 🔹 Salvar tokens
    save_tokens(tokens, save_path);

    // 🔹 Recarregar tokens e reconstruir texto
    auto loaded = load_tokens(save_path);
    std::string reconstructed;
    for (auto t : loaded) {
        char buf[64];
        int len = llama_token_to_piece(vocab, t, buf, sizeof(buf), 0, true);
        reconstructed.append(buf, len);
    }

    std::cout << "\n🔁 Texto reconstruído: \033[36m" << reconstructed << "\033[0m\n";

    // 🔹 Gera vetor emocional fake
    auto emo = generate_emotion_vector(8);
    std::cout << "\n💫 Vetor emocional (8D): [";
    for (size_t i = 0; i < emo.size(); ++i) {
        std::cout << std::fixed << std::setprecision(3) << emo[i];
        if (i < emo.size() - 1) std::cout << ", ";
    }
    std::cout << "]\n";

    // 🔹 ID do modelo
    int64_t model_id = llama_model_meta_count(model);
    std::cout << "\n🔗 Meta Count do modelo: 0x" << std::hex << model_id << std::dec << "\n";

    // 🔹 Limpeza
    llama_free(ctx);
    llama_model_free(model);
    std::cout << "\n🧹 Finalizado com sucesso!\n";
    return 0;
}
