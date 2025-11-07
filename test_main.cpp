#include "CoreLLM.hpp" 
#include "llama.h"
#include <iostream>
#include <string>

int main() {
    // Configuração de Log e Backends
    llama_log_set(nullptr, nullptr); 
    ggml_backend_load_all(); 

    CoreIntegration alyssa_brain;

    std::cout << "Inicializando CoreIntegration..." << std::endl;
    
    // 1. Chama a Inicialização com o caminho do modelo BASE (Argumento Mínimo)
    if (!alyssa_brain.initialize("models/gemma-3-1b-it-q4_0.gguf")) {
        std::cerr << "Falha Crítica ao inicializar o CoreIntegration. Encerrando." << std::endl;
        return 1;
    }
    
    // 2. Loop de Interação para a API Externa (STT/TTS)
    // O método run_interactive_loop() é implementado em CoreIntegration.cpp
    alyssa_brain.run_interactive_loop();
    
    // O destrutor ~CoreIntegration() será chamado, liberando todos os recursos.
    return 0;
}