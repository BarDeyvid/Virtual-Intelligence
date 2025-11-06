// SocialLLM.hpp
#pragma once
#include "llama.h"
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cstdio>
#include "json.hpp"
#include "AlyssaCore.hpp"

using json = nlohmann::json;

namespace internal {

    class SocialLLM {
    private:
        // Ponteiros compartilhados (NÃO deletar no destrutor)
        llama_model* model;
        const llama_vocab* vocab;

        // Recursos próprios (DELETAR no destrutor)
        llama_context* ctx;
        llama_sampler* smpl;
        
        SimpleModelConfig config; 
        std::string system_prompt;
        std::vector<llama_chat_message> history; 
        int n_ctx = 2048;

    public:
        // Construtor MODIFICADO
        SocialLLM(llama_model* shared_model, const llama_vocab* shared_vocab, const std::string& expert_id)
            : model(shared_model), vocab(shared_vocab), ctx(nullptr), smpl(nullptr) 
        {
            if (!model && !vocab) {
                throw std::runtime_error("SocialLLM: Modelo ou Vocabular nulo recebido.");
            }

            // 1. Carrega TODAS as configurações e busca a específica.
            AllModelConfigs configs = load_config(); // Chama a global de AlyssaCore
            bool found = false;
            for (const auto& cfg : configs) {
                if (cfg.id == expert_id) {
                    this->config = cfg;
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("SocialLLM: Configuração do especialista com ID '" + expert_id + "' não encontrada no JSON.");
            }
            
            // 2. Configurações internas a partir da 'config' carregada.
            this->system_prompt = this->config.system_prompt;

            // 3. Criar contexto de execução
            llama_context_params ctx_params = llama_context_default_params();
            ctx_params.n_ctx = n_ctx;
            ctx_params.n_batch = n_ctx;

            ctx = llama_init_from_model(model, ctx_params);
            if (!ctx) {
                throw std::runtime_error("SocialLLM: Falha ao criar contexto.");
            }
            std::cout << "Contexto [" << expert_id << "] criado." << std::endl;

            // 4. APLICAR O "Fine-Tuning" (LoRA) - USANDO config.usa_LoRA e config.lora_path
            if (this->config.usa_LoRA && !this->config.lora_path.empty()) {
                llama_adapter_lora *lora = llama_adapter_lora_init(model, this->config.lora_path.c_str());

                if (!lora) {
                    throw std::runtime_error("SocialLLM: Falha ao inicializar LoRA para: " + this->config.lora_path);
                }

                int err = llama_set_adapter_lora(
                    ctx, // Aplica ao nosso contexto específico
                    lora,
                    1.0); 
                        
                if (err != 0) {
                    llama_free(ctx); 
                    throw std::runtime_error("SocialLLM: Falha ao aplicar LoRA: " + this->config.lora_path);
                }
                std::cout << "Adaptador LoRA [" << expert_id << "] aplicado: " << this->config.lora_path << std::endl;
            } else if (this->config.usa_LoRA) {
                 std::cerr << "AVISO: usa_LoRA é true, mas lora_path está vazio. LoRA não aplicado para " << expert_id << "." << std::endl;
            }

            // 5. Criar o amostrador (com parâmetros do config.params)
            smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
            
            // Aplica os parâmetros de sampling (top_p, temperature)
            double top_p = (this->config.params.top_p > 0.0) ? this->config.params.top_p : 0.05f;
            llama_sampler_chain_add(smpl, llama_sampler_init_min_p(top_p, 1));
            
            double temp = (this->config.params.temperature > 0.0) ? this->config.params.temperature : 0.8f;
            llama_sampler_chain_add(smpl, llama_sampler_init_temp(temp));
            
            llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
        }

        // Destrutor (inalterado)
        ~SocialLLM() {
            // Libera APENAS seus próprios recursos
            if (smpl) llama_sampler_free(smpl);
            if (ctx) llama_free(ctx);
            
            // Libera o histórico de chat interno
            for (auto& msg : history) {
                free((char*)msg.content);
            }
            std::cout << "Recursos [SocialLLM] liberados." << std::endl;
        }

        void set_system_prompt(const std::string& prompt) {
                system_prompt = prompt;
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
            std::string generate_response(const std::string& user_input) {
                
                // 1. ADICIONA A NOVA MENSAGEM DO USUÁRIO AO HISTÓRICO
                this->history.push_back({"user", strdup(user_input.c_str())});

                // 2. MONTA A LISTA COMPLETA DE MENSAGENS (System + Histórico)
                std::vector<llama_chat_message> messages_to_template;
                
                // Adiciona o System Prompt no início
                if (!system_prompt.empty()) {
                    messages_to_template.push_back({"system", strdup(system_prompt.c_str())}); 
                    // NOTA: Esta memória também deve ser liberada (strdup)
                }
                
                // Adiciona o histórico da conversa
                messages_to_template.insert(messages_to_template.end(), this->history.begin(), this->history.end());

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
                this->history.push_back({"assistant", strdup(response.c_str())});

                // 6. RETORNA A RESPOSTA
                return response;
            }
    };
}