// #include "includes/AlyssaNet.h"
#include "llama.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept> // Para std::runtime_error

class CoreIntegration {
    //=================================================================================
    // Coordenação geral, síntese semântica e coerência cognitiva
    // Ativo continuamente
    //=================================================================================
};

class EmotionLLM {
    //=================================================================================
    // Interpretação afetiva, regulação emocional e empatia contextual
    // Ativação sob contexto emocional
    //=================================================================================
};

class MemoryLLM {
    //=================================================================================
    // Acesso, compressão e recuperação de experiências
    // Ativo em consultas e recordações
    //=================================================================================
};

class ActionLLM {
    //=================================================================================
    // Planejamento de tarefas e controle de execução física
    // Ativo em contextos motores
    //=================================================================================
};

class SocialLLM {
    //=================================================================================
    // Processamento de interações humanas e relações interpessoais
    // Ativo em contextos sociais
    //=================================================================================

    private: 
        std::string model_path; // Será inicializado no construtor
        int ngl = -1; // Numero camadas da gpu. -1 = Todas
        int n_ctx = 2048; // Tamanho do contexto
        
        // Membros do modelo e contexto
        llama_model_params mParams = llama_model_default_params();
        llama_model * model = nullptr; 
        const llama_vocab * vocab = nullptr; 
        const llama_context * ctx = nullptr; 
        llama_sampler * smpl = nullptr;

    public:
        // Construtor: Inicializa tudo
        SocialLLM(const std::string& path) : model_path(path) {
            // 1. Configurações
            mParams.n_gpu_layers = ngl;

            // 2. Carrega o modelo
            model = llama_model_load_from_file(model_path.c_str(), mParams);

            if (!model) {
                fprintf(stderr, "%s: erro ao carregar modelo a partir de %s\n", __func__, model_path.c_str());
                throw std::runtime_error("Falha ao carregar o modelo LLM."); 
            }
            std::cout << "Modelo LLM carregado com sucesso!\n";
            
            vocab = llama_model_get_vocab(model);
            
            // 3. Criar contexto de execução
            llama_context_params ctx_params = llama_context_default_params();
            ctx_params.n_ctx = n_ctx;
            ctx_params.n_batch = n_ctx;

            ctx = llama_init_from_model(model, ctx_params);
            if (!ctx) {
                fprintf(stderr, "%s: erro ao criar contexto\n", __func__);
                llama_model_free(model); // Limpa o modelo antes de falhar
                throw std::runtime_error("Falha ao carregar o contexto."); 
            } 
            std::cout << "Contexto Carregado com sucesso\n";

            // 4. Criar o amostrador
            smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
            llama_sampler_chain_add(smpl, llama_sampler_init_min_p(0.05f, 1));
            llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.8f));
            llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
        }

        // Destrutor: Para liberar recursos
        ~SocialLLM() {
            if (smpl) {
                llama_sampler_free(smpl);
            }
            if (ctx) {
                llama_free(const_cast<llama_context *>(ctx)); // Se for a função correta
                std::cout << "Arrumar free ctx\n";
            }
            if (model) {
                llama_model_free(model); 
            }
        }
        
        // --- Getters para permitir que DebugLLM use os recursos ---
        llama_sampler * get_sampler() const { return smpl; }
        llama_context * get_context() const { return const_cast<llama_context *>(ctx); }
        llama_model * get_model() const { return model; }
        const llama_vocab * get_vocab() const { return vocab; }
    };
    
class IntrospectionLLM {
    //=================================================================================
    // Análise interna, autocorreção e evolução comportamental
    // Ativo em estados de reflexão
    //=================================================================================
};

class CentralMemory{
    //=================================================================================
    // Armazenamento de embeddings, vetores de contexto e registros cognitivos
    // Compartilhado entre todos os módulos
    //=================================================================================
};

class DebugLLM {
    private: 
        const llama_vocab * vocab = nullptr;
        llama_context * ctx = nullptr;
        llama_model * model = nullptr; 
        llama_sampler * smpl = nullptr;

    public:
        DebugLLM(llama_sampler * smpl_ptr, llama_context * ctx_ptr, 
                 llama_model * model_ptr, const llama_vocab * vocab_ptr) 
            : vocab(vocab_ptr), ctx(ctx_ptr), model(model_ptr), smpl(smpl_ptr) {
        }

        std::string generate(const std::string & prompt) {
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
                // Usando std::runtime_error ao invés de GGML_ABORT/exit para ser mais C++
                throw std::runtime_error("Falha ao tokenizar o prompt\n");
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
                    // return vazio ou throw ao invés de exit(0)
                    throw std::runtime_error("Tamanho do contexto excedido\n");
                }

                // Decodifica os tokens atuais
                int ret = llama_decode(ctx, batch);
                if (ret != 0) throw std::runtime_error("Falha ao decodificar");

                // Amostra o próximo token
                new_token_id = llama_sampler_sample(smpl, ctx, -1);

                // Verifica fim de geração
                if (llama_vocab_is_eog(vocab, new_token_id)) break;

                // Converte o token para texto
                char buf[256];
                int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
                if (n < 0) throw std::runtime_error("Falha ao converter token\n");

                std::string piece(buf, n);
                printf("%s", piece.c_str());
                fflush(stdout);
                response += piece;

                // Prepara o próximo batch
                batch = llama_batch_get_one(&new_token_id, 1);
            }

            return response;
        }

        void Inference() {
            std::vector<llama_chat_message> messages;
            std::vector<char> formatted(llama_n_ctx(ctx));
            int prev_len = 0;

            while (true) {
                printf("\033[32m> \033[0m"); 
                std::string user;
                std::getline(std::cin, user);

                if (user.empty()) break;

                const char * tmpl = llama_model_chat_template(model, nullptr);

                messages.push_back({"user", strdup(user.c_str())});
                int new_len = llama_chat_apply_template(
                    tmpl, messages.data(), messages.size(), true, formatted.data(), formatted.size()
                );

                if (new_len > (int)formatted.size()) {
                    formatted.resize(new_len);
                    new_len = llama_chat_apply_template(
                        tmpl, messages.data(), messages.size(), true, formatted.data(), formatted.size()
                    );
                }

                if (new_len < 0) {
                    fprintf(stderr, "Falha ao aplicar template de chat\n");
                    continue;
                }

                std::string prompt(formatted.begin() + prev_len, formatted.begin() + new_len);

                printf("\033[33m"); 
                std::string response = generate(prompt); // Chama a função generate corrigida
                printf("\n\033[0m");

                messages.push_back({"assistant", strdup(response.c_str())});
                prev_len = llama_chat_apply_template(
                    tmpl, messages.data(), messages.size(), false, nullptr, 0
                );

                if (prev_len < 0) {
                    fprintf(stderr, "Falha ao aplicar template de chat\n");
                }
            }
            // Não se esqueça de liberar a memória alocada por strdup em 'messages'
            for (auto& msg : messages) {
                free((char*)msg.content);
            }
        }

        // Destrutor: Não precisa liberar nada, pois os ponteiros são "emprestados" de SocialLLM
        ~DebugLLM() {
        }
};

int main() {
    // Configuração de Log
    llama_log_set([](enum ggml_log_level level, const char * text, void *) {
        if (level >= GGML_LOG_LEVEL_ERROR)
            fprintf(stderr, "%s", text);
    }, nullptr);

    // Carrega backends
    ggml_backend_load_all();

    try {
        // 1. Instanciação CORRIGIDA de SocialLLM
        // Note que o caminho do modelo foi definido na classe SocialLLM, mas é mais robusto passá-lo aqui. Menos Erros = Mais Cabelo
        SocialLLM social("models/gemma-3-270m-it-F16.gguf"); 

        // 2. Instanciação CORRIGIDA de DebugLLM usando os Getters
        DebugLLM debug(
            social.get_sampler(), 
            social.get_context(), 
            social.get_model(), 
            social.get_vocab()
        );

        // Chamada de inferência para iniciar o loop de chat
        debug.Inference();

    } catch (const std::exception& e) {
        // Captura exceções para um shutdown limpo
        fprintf(stderr, "Erro Crítico: %s\n", e.what());
        return 1;
    }

    return 0;
}