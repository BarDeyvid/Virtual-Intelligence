// AlyssaCore.hpp
#pragma once
#include "llama.h"
#include <string>
#include <iostream>
#include <stdexcept>

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