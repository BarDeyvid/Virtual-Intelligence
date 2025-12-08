#include "llama.h"
#include <iostream>

int main() {
    std::cout << "Iniciando teste da lib llama.cpp..." << std::endl;

    // Caminho do modelo GGUF (ajuste conforme o local do seu arquivo)
    const char *model_path = "llama.cpp/models/Llama-3.2-3B-Instruct-uncensored-Q5_K_M.gguf";

    // Configuração mínima do contexto
    llama_model_params model_params = llama_model_default_params();
    llama_context_params ctx_params = llama_context_default_params();

    // Carrega o modelo
    llama_model *model = llama_model_load_from_file(model_path, model_params);
    if (!model) {
        std::cerr << "❌ Erro ao carregar o modelo: " << model_path << std::endl;
        return 1;
    }
    std::cout << " Modelo carregado com sucesso!" << std::endl;

    // Cria contexto
    llama_context *ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        std::cerr << "❌ Erro ao criar contexto!" << std::endl;
        llama_model_free(model);
        return 1;
    }
    std::cout << " Contexto criado!" << std::endl;

    // Teste básico: imprime o número de tokens do vocabulário
    const llama_vocab* vocab = llama_model_get_vocab(model);
    int n_vocab = llama_vocab_n_tokens(vocab);
    std::cout << "📖 Vocabulário: " << n_vocab << " tokens." << std::endl;

    // Limpeza
    llama_free(ctx);
    llama_model_free(model);

    std::cout << "🧹 Liberação concluída. Tudo funcionando!" << std::endl;
    return 0;
}
