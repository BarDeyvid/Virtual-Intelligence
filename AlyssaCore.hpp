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
            llama_adapter_lora* lora // LoRA a ser aplicado
        ) {
            std::string response;

            bool usa_LoRA = false;
            std::string lora_path = "";

            // 1. APLICAR O LoRA (ou desativar um anterior)
            // O Orquestrador nos diz qual LoRA usar (pode ser nullptr)
            if (usa_LoRA && !lora_path.empty()) {
                llama_adapter_lora *lora = llama_adapter_lora_init(model, lora_path.c_str());

                if (!lora) {
                    throw std::runtime_error("AlyssaLLM: Falha ao inicializar LoRA para: " + lora_path);
                }

                int err = llama_set_adapter_lora(
                    ctx, // Aplica ao nosso contexto específico
                    lora,
                    1.0); 
                        
                if (err != 0) {
                    llama_free(ctx); 
                    throw std::runtime_error("AlyssaLLM: Falha ao aplicar LoRA" );
                }
                std::cout << "Adaptador LoRA aplicado: " << std::endl;
            } else if (usa_LoRA) {
                 std::cerr << "AVISO: usa_LoRA é true, mas lora_path está vazio. LoRA não aplicado para " << std::endl;
            }


            // 2. CRIAR O SAMPLER (com parâmetros do especialista)
            // Temos que recriá-lo ou reconfigurá-lo a cada chamada
            llama_sampler* smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
            double top_p = (params.top_p > 0.0) ? params.top_p : 0.05f;
            llama_sampler_chain_add(smpl, llama_sampler_init_min_p(top_p, 1));
            double temp = (params.temperature > 0.0) ? params.temperature : 0.8f;
            llama_sampler_chain_add(smpl, llama_sampler_init_temp(temp));
            llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
            
            // 3. Verifica se o contexto já tem tokens
            const bool is_first_run = llama_memory_seq_pos_max(llama_get_memory(ctx), 0) == -1;

            // 4. Obtém o número de tokens que JÁ ESTÃO no KV cache.
            const int n_cached = is_first_run ? 0 : llama_memory_seq_pos_max(llama_get_memory(ctx), 0) + 1;

            // 5. Tokeniza o prompt COMPLETO
            const int n_prompt_tokens_total = -llama_tokenize(
                vocab, prompt.c_str(), prompt.size(), NULL, 0, is_first_run, true
            );
            std::vector<llama_token> prompt_tokens(n_prompt_tokens_total);
            if (llama_tokenize(vocab, prompt.c_str(), prompt.size(),
                            prompt_tokens.data(), prompt_tokens.size(),
                            is_first_run, true) < 0) {
                throw std::runtime_error("Falha ao tokenizar o prompt\n");
            }

            // 6. Calcula quantos tokens são REALMENTE NOVOS
            const int n_new_tokens = prompt_tokens.size() - n_cached;
            if (n_new_tokens < 0) {
                // Se isso acontecer, o histórico do especialista está dessincronizado
                // com o KV Cache. Precisamos limpar o cache.
                std::cerr << "AVISO: Detectada dessincronização do KV Cache. Limpando cache." << std::endl;
                llama_memory_seq_rm(llama_get_memory(ctx), 0, -1, -1); // Limpa tudo
                // E tentamos novamente como se fosse a primeira vez
                return generate_raw(prompt, params, lora);
            }

            // 7. Prepara um batch APENAS com os tokens NOVOS
            llama_batch batch = llama_batch_get_one(
                prompt_tokens.data() + n_cached, 
                n_new_tokens                     
            );

            // [FIM DA LÓGICA STATEFUL]
            
            llama_token new_token_id;

            // Loop principal de geração
            while (true) {
                int n_ctx_total = llama_n_ctx(ctx);
                int n_ctx_used  = llama_memory_seq_pos_max(llama_get_memory(ctx), 0) + 1;
                
                if (n_ctx_used + batch.n_tokens > n_ctx_total) {
                    fprintf(stderr, "Tamanho do contexto excedido\n");
                    // TODO: Implementar evicção de KV cache (llama_kv_cache_seq_rm)
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

                batch = llama_batch_get_one(&new_token_id, 1);
            }

            // Libera o sampler desta chamada
            llama_sampler_free(smpl);

            return response;
        }
    };
}