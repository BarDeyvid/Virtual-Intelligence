#include "llama.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

//
// 🧠 Exemplo interativo de uso da API llama.cpp em C++
// Permite carregar um modelo, digitar prompts e gerar respostas
// GPT gerou os comentários em português brasileiro

// ============================================================================
// Função auxiliar: mostra instruções de uso no terminal
// ============================================================================
static void print_usage(int, char ** argv) {
    printf("\nUso do programa:\n");
    printf("    %s -m modelo.gguf [-c contexto] [-ngl camadas_gpu]\n\n", argv[0]);
}

// ============================================================================
// Função principal
// ============================================================================
int main(int argc, char ** argv) {

    // Caminho do modelo e parâmetros básicos
    std::string model_path;
    int ngl   = 99;      // número de camadas GPU
    int n_ctx = 2048;    // tamanho do contexto

    // ------------------------------------------------------------------------
    // 🔧 Leitura dos argumentos de linha de comando
    // ------------------------------------------------------------------------
    for (int i = 1; i < argc; i++) {
        try {
            if (strcmp(argv[i], "-m") == 0) {
                if (i + 1 < argc) model_path = argv[++i];
                else { print_usage(argc, argv); return 1; }

            } else if (strcmp(argv[i], "-c") == 0) {
                if (i + 1 < argc) n_ctx = std::stoi(argv[++i]);
                else { print_usage(argc, argv); return 1; }

            } else if (strcmp(argv[i], "-ngl") == 0) {
                if (i + 1 < argc) ngl = std::stoi(argv[++i]);
                else { print_usage(argc, argv); return 1; }

            } else {
                print_usage(argc, argv);
                return 1;
            }

        } catch (std::exception & e) {
            fprintf(stderr, "erro: %s\n", e.what());
            print_usage(argc, argv);
            return 1;
        }
    }

    if (model_path.empty()) {
        print_usage(argc, argv);
        return 1;
    }

    // ------------------------------------------------------------------------
    // 🧱 Configura logs (mostra apenas erros)
    // ------------------------------------------------------------------------
    llama_log_set([](enum ggml_log_level level, const char * text, void *) {
        if (level >= GGML_LOG_LEVEL_ERROR)
            fprintf(stderr, "%s", text);
    }, nullptr);

    // Carrega backends (GPU, CPU, etc)
    ggml_backend_load_all();

    // ------------------------------------------------------------------------
    // 📦 Inicializa o modelo
    // ------------------------------------------------------------------------
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = ngl;

    llama_model * model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!model) {
        fprintf(stderr, "%s: erro ao carregar modelo\n", __func__);
        return 1;
    }

    const llama_vocab * vocab = llama_model_get_vocab(model);

    // ------------------------------------------------------------------------
    // 🧮 Cria o contexto de execução
    // ------------------------------------------------------------------------
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx   = n_ctx;
    ctx_params.n_batch = n_ctx;

    llama_context * ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        fprintf(stderr, "%s: erro ao criar contexto\n", __func__);
        return 1;
    }

    // ------------------------------------------------------------------------
    // 🎲 Cria o amostrador (controla a geração)
    // ------------------------------------------------------------------------
    llama_sampler * smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_min_p(0.05f, 1)); // corte de probabilidade mínima
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.8f));       // temperatura
    llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // ------------------------------------------------------------------------
    // ✨ Função lambda para gerar respostas com base num prompt
    // ------------------------------------------------------------------------
    auto generate = [&](const std::string & prompt) {
        std::string response;

        // Verifica se é a primeira execução
        const bool is_first = llama_memory_seq_pos_max(llama_get_memory(ctx), 0) == -1;

        // Tokeniza o prompt
        const int n_prompt_tokens = -llama_tokenize(
            vocab, prompt.c_str(), prompt.size(), NULL, 0, is_first, true
        );

        std::vector<llama_token> prompt_tokens(n_prompt_tokens);
        if (llama_tokenize(vocab, prompt.c_str(), prompt.size(),
                           prompt_tokens.data(), prompt_tokens.size(),
                           is_first, true) < 0) {
            GGML_ABORT("Falha ao tokenizar o prompt\n");
        }

        // Prepara um batch com os tokens
        llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
        llama_token new_token_id;

        // Loop principal de geração
        while (true) {
            // Garante que o contexto não foi ultrapassado
            int n_ctx_total = llama_n_ctx(ctx);
            int n_ctx_used  = llama_memory_seq_pos_max(llama_get_memory(ctx), 0) + 1;
            if (n_ctx_used + batch.n_tokens > n_ctx_total) {
                printf("\033[0m\n");
                fprintf(stderr, "Tamanho do contexto excedido\n");
                exit(0);
            }

            // Decodifica os tokens atuais
            int ret = llama_decode(ctx, batch);
            if (ret != 0) GGML_ABORT("Falha ao decodificar, ret = %d\n", ret);

            // Amostra o próximo token
            new_token_id = llama_sampler_sample(smpl, ctx, -1);

            // Verifica fim de geração
            if (llama_vocab_is_eog(vocab, new_token_id)) break;

            // Converte o token para texto
            char buf[256];
            int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
            if (n < 0) GGML_ABORT("Falha ao converter token\n");

            std::string piece(buf, n);
            printf("%s", piece.c_str());
            fflush(stdout);
            response += piece;

            // Prepara o próximo batch
            batch = llama_batch_get_one(&new_token_id, 1);
        }

        return response;
    };

    // ------------------------------------------------------------------------
    // 💬 Loop principal de interação (chat)
    // ------------------------------------------------------------------------
    std::vector<llama_chat_message> messages;
    std::vector<char> formatted(llama_n_ctx(ctx));
    int prev_len = 0;

    while (true) {
        printf("\033[32m> \033[0m"); // prompt visual verde
        std::string user;
        std::getline(std::cin, user);

        // Sai se o usuário der Enter vazio
        if (user.empty()) break;

        const char * tmpl = llama_model_chat_template(model, nullptr);

        // Adiciona a mensagem do usuário e aplica o template do chat
        messages.push_back({"user", strdup(user.c_str())});
        int new_len = llama_chat_apply_template(
            tmpl, messages.data(), messages.size(), true, formatted.data(), formatted.size()
        );

        // Ajusta buffer se for pequeno demais
        if (new_len > (int)formatted.size()) {
            formatted.resize(new_len);
            new_len = llama_chat_apply_template(
                tmpl, messages.data(), messages.size(), true, formatted.data(), formatted.size()
            );
        }

        if (new_len < 0) {
            fprintf(stderr, "Falha ao aplicar template de chat\n");
            return 1;
        }

        // Pega somente o texto novo (diferente da conversa anterior)
        std::string prompt(formatted.begin() + prev_len, formatted.begin() + new_len);

        // Gera a resposta
        printf("\033[33m"); // amarelo
        std::string response = generate(prompt);
        printf("\n\033[0m");

        // Salva a resposta no histórico
        messages.push_back({"assistant", strdup(response.c_str())});
        prev_len = llama_chat_apply_template(
            tmpl, messages.data(), messages.size(), false, nullptr, 0
        );

        if (prev_len < 0) {
            fprintf(stderr, "Falha ao aplicar template de chat\n");
            return 1;
        }
    }

    // ------------------------------------------------------------------------
    // 🧹 Liberação de recursos
    // ------------------------------------------------------------------------
    for (auto & msg : messages) {
        free(const_cast<char *>(msg.content));
    }
    llama_sampler_free(smpl);
    llama_free(ctx);
    llama_model_free(model);

    return 0;
}
