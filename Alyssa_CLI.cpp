// test_main_cli.cpp

#include "CoreLLM.hpp" 
#include "llama.h"
#include <iostream>
#include <string>
#include <thread>
#include "includes/log.hpp"

/**
         * @brief Initialize Log Callback.
         * @param level Level of Logging, just like any other logger.
         * @param text The text of the logging.
         * @param user_data IDK what the user_data is for, Microsoft Vibes.
         * @return `void`.
         */
void log_callback(ggml_log_level level, const char * text, void * user_data) {
    (void)level;
    (void)user_data;
    fputs(text, stderr);
    fflush(stderr);
}

int main() {
    try {      
        llama_log_set(log_callback, nullptr);
        ggml_backend_load_all(); 

        Log::init("alyssa_cli.log");

        auto& logger = Log::getLogger();

        CoreIntegration alyssa_brain;
        logger->debug("Inicializando CoreIntegration Teste CLI...");
        alyssa_brain.set_user_name("Deyvid");
        // 1. Chama a Inicialização com o caminho do modelo BASE
        if (!alyssa_brain.initialize("models/gemma-3-4b-it-q4_0.gguf")) {
            logger->critical("Falha Crítica ao inicializar o CoreIntegration. Encerrando.");
            return 1;
        }
        logger->debug("CoreIntegration Inicializado com sucesso!");

        while (true) {
            std::cout << "\n\033[32m> \033[0m";
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "sair" || input == "exit") {
                std::cout << "Encerrando..." << std::endl;
                break;
            }
            
            if (input.empty()) continue;
            
            //  Usa Weighted Fusion
            std::string alyssa_response = alyssa_brain.think_with_fusion_ttsless(input);
            std::cout << "\n\033[36m[ALYSSA]: \033[0m" << alyssa_response << std::endl;
        }
    } catch (const std::exception& e) {
        auto& logger = Log::getLogger();
        logger->critical("\n[ERRO FATAL] " + std::string(e.what()));
        return 1;
    } catch (...) {
                auto& logger = Log::getLogger();
        logger->critical("\n[ERRO FATAL] Erro desconhecido não capturado.");
        return 1;
    } 
    return 0;
}
