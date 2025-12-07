// test_main.cpp
#include "includes/CoreLLM.hpp" 
#include "llama.h"
#include <iostream>
#include <string>
#include "voice/ElevenLabsTTS.hpp"
#include "voice/VoicePipeline.hpp" 

int main() {
    // Configuração de Log e Backends
    llama_log_set(nullptr, nullptr); 
    ggml_backend_load_all(); 

    CoreIntegration alyssa_brain;

    std::cout << "Inicializando CoreIntegration..." << std::endl;
    
    // 1. Chama a Inicialização com o caminho do modelo BASE
    if (!alyssa_brain.initialize("models/gemma-3-1b-it-q4_0.gguf")) {
        std::cerr << "Falha Crítica ao inic${workspaceFolder}/llama.cpp/includeializar o CoreIntegration. Encerrando." << std::endl;
        return 1;
    }
    std::cout << "CoreIntegration Inicializado...";
    
    ElevenLabsTTS tts;

    VoicePipeline stt("models/ggml-large-v3.bin");

    if (!stt.start()) {
        std::cerr << "Falha ao inciar Pipeline TTS." << std::endl;
        return 1;
    }
    std::string user_input;


    while (true) {
        printf("\033[32m> \033[0m");
        if (stt.get_last_result(user_input)) {
            std::cout << " [Transcrição]: " << user_input << std::endl;
            stt.pause(); 
            
            // 🆕 Usa Weighted Fusion em vez de think simples
            std::string alyssa_response = alyssa_brain.think_with_fusion(user_input, tts);
            
            stt.resume();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 0;
}