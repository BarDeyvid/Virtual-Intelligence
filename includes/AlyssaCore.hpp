// AlyssaCore.hpp
#pragma once
#include "llama.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include "json.hpp"
#include <fstream>
#include <cstdio>
#include <cstring>
#include <vector>
#include <functional>
#pragma warning(disable: 4244 4267 4458)
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
    std::string role_instruction;
    bool usa_LoRA;
    std::string lora_path;
    SimpleModelParameters params;
};

using AllModelConfigs = std::vector<SimpleModelConfig>;

inline AllModelConfigs load_config() {
    AllModelConfigs configs;
    const std::string& filepath = "config/ConfigsLLM.json";
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
                if (model_json.contains("usa_LoRA")) {
                    config.usa_LoRA = model_json["usa_LoRA"].get<bool>();
                }
                if (model_json.contains("lora_path")) {
                    config.lora_path = model_json["lora_path"].get<std::string>();
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

namespace alyssa_core {

    class AlyssaCore {
    private:
        llama_model* model;
        const llama_vocab* vocab;
        llama_model_params mParams;

        // O CONTEXTO ÚNICO E COMPARTILHADO
        llama_context* ctx;
        int n_ctx = 2048; // Ou carregue do config

    public:
        AlyssaCore(const std::string& base_model_path) 
            : model(nullptr), vocab(nullptr), ctx(nullptr) 
        {
            mParams = llama_model_default_params();
            mParams.n_gpu_layers = -1; 

            model = llama_model_load_from_file(base_model_path.c_str(), mParams);
            if (!model) {
                throw std::runtime_error("Falha ao carregar o modelo BASE: " + base_model_path);
            }
            std::cout << "Modelo BASE carregado com sucesso: " << base_model_path << std::endl;
            
            vocab = llama_model_get_vocab(model);

            // CRIAR O CONTEXTO ÚNICO
            llama_context_params ctx_params = llama_context_default_params();
            ctx_params.n_ctx = n_ctx;
            ctx_params.n_batch = n_ctx;

            ctx = llama_init_from_model(model, ctx_params);
            if (!ctx) {
                throw std::runtime_error("AlyssaCore: Falha ao criar contexto ÚNICO.");
            }
            std::cout << "Contexto ÚNICO criado." << std::endl;
        }

        ~AlyssaCore() {
            // Libera TUDO
            if (ctx) llama_free(ctx);
            if (model) llama_model_free(model);
            std::cout << "Modelo BASE e Contexto ÚNICO liberados." << std::endl;
        }

        // Getters para o Orquestrador
        llama_model* get_model() { return model; }
        const llama_vocab* get_vocab() { return vocab; }
        llama_context* get_context() { return ctx; }
        int get_n_ctx() { return n_ctx; }


        // Lógica de geração movida para cá
        std::string generate_raw(
            const std::string & prompt,
            const SimpleModelParameters& params,
            llama_adapter_lora* lora, // LoRA a ser aplicado
            std::function<void(const std::string& piece)> stream_callback
        ) {
            std::string response;
            int n_prompt_tokens_total;

            // 1. APLICAR O LoRA (se fornecido)
            if (lora != nullptr) {
                int err = llama_set_adapter_lora(ctx, lora, 1.0);
                if (err != 0) {
                    std::cerr << "AlyssaLLM: Falha ao aplicar LoRA" << std::endl;
                } else {
                    std::cout << "Adaptador LoRA aplicado." << std::endl;
                }
            }

            // 2. CRIAR O SAMPLER (com parâmetros do especialista)
            llama_sampler* smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
            float top_p = (params.top_p > 0.0) ? params.top_p : 0.05f;
            llama_sampler_chain_add(smpl, llama_sampler_init_min_p(top_p, 1));
            float temp = (params.temperature > 0.0) ? params.temperature : 0.8f;
            llama_sampler_chain_add(smpl, llama_sampler_init_temp(temp));
            llama_sampler_chain_add(smpl, llama_sampler_init_penalties(64, 1.3f, 0.0f, 0.0f));
            llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
            
            // 3. Verifica se o contexto já tem tokens
            const bool is_first_run = llama_memory_seq_pos_max(llama_get_memory(ctx), 0) == -1;

            // 4. Obtém o número de tokens que JÁ ESTÃO no KV cache.
            const int n_cached = is_first_run ? 0 : llama_memory_seq_pos_max(llama_get_memory(ctx), 0) + 1;

            // 5. Tokeniza o prompt COMPLETO
            n_prompt_tokens_total = llama_tokenize( 
                vocab, prompt.c_str(), prompt.size(), NULL, 0, is_first_run, true
            );

            // Inverte o sinal para obter o número total de tokens
            if (n_prompt_tokens_total < 0) {
                n_prompt_tokens_total = -n_prompt_tokens_total;
            } else if (n_prompt_tokens_total == 0) {
                return ""; 
            }

            std::vector<llama_token> prompt_tokens(n_prompt_tokens_total);
            if (llama_tokenize(vocab, prompt.c_str(), prompt.size(),
                            prompt_tokens.data(), prompt_tokens.size(),
                            is_first_run, true) < 0) {
                throw std::runtime_error("Falha ao tokenizar o prompt\n");
            }

            // 6. Calcula quantos tokens são REALMENTE NOVOS
            const int n_new_tokens = prompt_tokens.size() - n_cached;
            if (n_new_tokens < 0) {
                // Dessincronização do KV Cache
                std::cerr << "AVISO: Detectada dessincronização do KV Cache. Limpando cache." << std::endl;
                llama_memory_seq_rm(llama_get_memory(ctx), 0, -1, -1);
                // Recursivamente tenta novamente com cache limpo
                return generate_raw(prompt, params, lora, stream_callback);
            }

            // 7. Prepara um batch APENAS com os tokens NOVOS
            llama_batch batch = llama_batch_get_one(
                prompt_tokens.data() + n_cached, 
                n_new_tokens                     
            );

            llama_token new_token_id;

            // Loop principal de geração
            while (true) {
                int n_ctx_total = llama_n_ctx(ctx);
                int n_ctx_used  = llama_memory_seq_pos_max(llama_get_memory(ctx), 0) + 1;
                
                if (n_ctx_used + batch.n_tokens > n_ctx_total) {
                    fprintf(stderr, "Tamanho do contexto excedido\n");
                    throw std::runtime_error("Tamanho do contexto excedido\n");
                }

                int ret = llama_decode(ctx, batch);
                if (ret != 0) throw std::runtime_error("Falha ao decodificar");

                new_token_id = llama_sampler_sample(smpl, ctx, -1);

                if (llama_vocab_is_eog(vocab, new_token_id)) break;

                char buf[256];
                int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
                if (n < 0) throw std::runtime_error("Falha ao converter token\n");

                std::string piece(buf, n);
                printf("%s", piece.c_str());
                fflush(stdout);
                response += piece;

                if (stream_callback) {
                    stream_callback(piece);
                }

                batch = llama_batch_get_one(&new_token_id, 1);
            }

            llama_sampler_free(smpl);
            return response;
        }
    };
}