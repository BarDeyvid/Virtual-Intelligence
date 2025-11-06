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

AllModelConfigs load_config() {
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

    public:
        AlyssaCore(const std::string& base_model_path) : model(nullptr), vocab(nullptr) {
            mParams = llama_model_default_params();
            mParams.n_gpu_layers = -1; // Usar GPU para todas as camadas

            model = llama_model_load_from_file(base_model_path.c_str(), mParams);
            if (!model) {
                throw std::runtime_error("Falha ao carregar o modelo BASE: " + base_model_path);
            }
            std::cout << "Modelo BASE carregado com sucesso: " << base_model_path << std::endl;
            
            vocab = llama_model_get_vocab(model);
        }

        ~AlyssaCore() {
            if (model) {
                llama_model_free(model);
                std::cout << "Modelo BASE liberado." << std::endl;
            }
        }

        // Getters para os especialistas usarem
        llama_model* get_model() { return model; }
        const llama_vocab* get_vocab() { return vocab; }
    };
}