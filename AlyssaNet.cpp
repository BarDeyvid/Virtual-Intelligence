// #include "includes/AlyssaNet.h"
#include "llama.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept> // Para std::runtime_error
#include "includes/json.hpp"
#include <fstream>

using json = nlohmann::json;

struct SimpleModelParameters {
    double temperature = 0.0;
    double top_p = 0.0;
    int max_tokens = 0;
};

struct SimpleModelConfig {
    std::string id;
    std::string model_path;
    std::string system_prompt;
    bool usa_LoRA;
    std::string lora_path;
    SimpleModelParameters params;
};

using AllModelConfigs = std::vector<SimpleModelConfig>;

AllModelConfigs load_config(const std::string& filepath) {
    AllModelConfigs configs;
    const std::string& filepath = "models/gemma-3-270m-it-F16.gguf";
    // 1. Abrir o arquivo
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERRO: Não foi possível abrir o arquivo de configuração: " << filepath << std::endl;
        return configs; // Retorna vazio em caso de erro
    }

    try {
        // 2. Fazer o parse do JSON
        json j = json::parse(file);

        // 3. Iterar sobre o array "models"
        if (j.contains("models") && j["models"].is_array()) {
            for (const auto& model_json : j["models"]) {
                SimpleModelConfig config;

                // Extrair campos de nível superior
                if (model_json.contains("id")) {
                    config.id = model_json["id"].get<std::string>();
                }
                if (model_json.contains("model_path")) {
                    config.model_path = model_json["model_path"].get<std::string>();
                }
                if (model_json.contains("system_prompt")) {
                    config.system_prompt = model_json["system_prompt"].get<std::string>();
                }

                // Extrair campos aninhados "parametros"
                if (model_json.contains("parametros") && model_json["parametros"].is_object()) {
                    const auto& params_json = model_json["parametros"];
                    
                    if (params_json.contains("temperatura")) {
                        config.params.temperature = params_json["temperatura"].get<double>();
                    }
                    if (params_json.contains("top_p")) {
                        config.params.top_p = params_json["top_p"].get<double>();
                    }
                    if (params_json.contains("max_tokens")) {
                        config.params.max_tokens = params_json["max_tokens"].get<int>();
                    }
                }

                configs.push_back(config);
            }
        }
    } catch (const json::parse_error& e) {
        std::cerr << "ERRO de Parsing JSON: " << e.what() << std::endl;
    } catch (const json::exception& e) {
        std::cerr << "ERRO JSON (Campo ausente/Tipo errado?): " << e.what() << std::endl;
    }

    return configs;
}

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
        llama_context * ctx = nullptr; 
        llama_sampler * smpl = nullptr;
        std::string system_prompt = "";

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

        void set_system_prompt(const std::string& prompt) {
            system_prompt = prompt;
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

        std::string generate_raw(const std::string & prompt) {
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
        
        // O método principal que recebe o input e retorna a resposta
        std::string generate_response(const std::string& user_input, 
                                    std::vector<llama_chat_message>& history) {
            
            // 1. ADICIONA A NOVA MENSAGEM DO USUÁRIO AO HISTÓRICO
            history.push_back({"user", strdup(user_input.c_str())});

            // 2. MONTA A LISTA COMPLETA DE MENSAGENS (System + Histórico)
            std::vector<llama_chat_message> messages_to_template;
            
            // Adiciona o System Prompt no início
            if (!system_prompt.empty()) {
                messages_to_template.push_back({"system", strdup(system_prompt.c_str())}); 
                // NOTA: Esta memória também deve ser liberada (strdup)
            }
            
            // Adiciona o histórico da conversa
            messages_to_template.insert(messages_to_template.end(), history.begin(), history.end());

            // 3. APLICA O TEMPLATE E GERA O PROMPT
            std::vector<char> formatted(llama_n_ctx(ctx));
            const char * tmpl = llama_model_chat_template(model, nullptr);

            int len = llama_chat_apply_template(
                tmpl, messages_to_template.data(), messages_to_template.size(), true, formatted.data(), formatted.size()
            );
            
            // Libera o System Prompt alocado com strdup
            if (!system_prompt.empty()) {
                free((char*)messages_to_template[0].content);
            }

            if (len < 0 || len > (int)formatted.size()) {
                 if (len > (int)formatted.size()) {
                    formatted.resize(len);
                    len = llama_chat_apply_template(
                        tmpl, messages_to_template.data(), messages_to_template.size(), true, formatted.data(), formatted.size()
                    );
                 }
                 if (len < 0) {
                     fprintf(stderr, "Falha ao aplicar template de chat\n");
                     return "Erro ao processar a conversa.";
                 }
            }
            
            std::string prompt(formatted.begin(), formatted.begin() + len);
            
            // 4. CHAMA A GERAÇÃO DE BAIXO NÍVEL
            std::string response = generate_raw(prompt); 

            // 5. ADICIONA A RESPOSTA DO ASSISTENTE AO HISTÓRICO
            history.push_back({"assistant", strdup(response.c_str())});

            // 6. RETORNA A RESPOSTA
            return response;
        }

        // --- Getters para permitir que DebugLLM use os recursos ---
        llama_sampler * get_sampler() const { return smpl; }
        llama_context * get_context() const { return const_cast<llama_context *>(ctx); }
        llama_model * get_model() const { return model; }
        const std::string& get_system_prompt() const { return system_prompt; }
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

int main() {
    // Configuração de Log
    llama_log_set([](enum ggml_log_level level, const char * text, void *) {
        if (level >= GGML_LOG_LEVEL_ERROR)
            fprintf(stderr, "%s", text);
    }, nullptr);

    // Carrega backends
    ggml_backend_load_all();

   try {
        SocialLLM social("models/gemma-3-270m-it-F16.gguf"); 
        
        // Histórico da conversa (deve ser gerenciado pela função de chat)
        std::vector<llama_chat_message> chat_history; 

        // Simulação de um loop de interação:
        std::cout << "AlyssaNet carregada. Digite 'sair' para encerrar.\n";

        std::string user_input;
        while (true) {
            printf("\033[32m> \033[0m"); 
            std::getline(std::cin, user_input);

            if (user_input == "sair" || user_input.empty()) break;

            // Chame o novo método da SocialLLM
            printf("\033[33m"); 
            std::string response = social.generate_response(user_input, chat_history);
            printf("\n\033[0m");
        }
        
        // Lembre-se de liberar os conteúdos de chat_history aqui!
        for (auto& msg : chat_history) {
            free((char*)msg.content);
        }

    } catch (const std::exception& e) {
        fprintf(stderr, "Erro Crítico: %s\n", e.what());
        return 1;
    }
    return 0;
}