#include "CoreLLM.hpp" 
#include "llama.h"
#include <iostream>
#include <string>
#include "voice/PiperTTS.hpp"
#include "voice/VoicePipeline.hpp"

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
    std::cout << "CoreIntegration Inicializado...";
    
    PiperTTS tts("models/en_US-ljspeech-high.onnx", "models/en_US-ljspeech-high.onnx.json", "piper/libpiper/build/espeak_ng-install/share/espeak-ng-data");

    VoicePipeline stt("models/ggml-large-v3-turbo.bin");

    if (!stt.start()) {
        std::cerr << "Falha ao inciar Pipeline TTS." << std::endl;
        return 1;
    }
    std::string user_input;


    while (true) {
        printf("\033[32m> \033[0m");
        if (stt.get_last_result(user_input)) {
            std::cout << " [Trasncricao]: " << user_input << std::endl;
            std::string alyssa_response = alyssa_brain.think(user_input);
            tts.synthesizeAndPlay(alyssa_response);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    }
    return 0;
}