// test_main.cpp
#include "CoreLLM.hpp" 
#include "llama.h"
#include <iostream>
#include <string>
// #include "voice/PiperTTS.hpp"
#include "voice/ElevenLabsTTS.hpp"
#include "voice/VoicePipeline.hpp"

void log_callback(ggml_log_level level, const char * text, void * user_data) {
    (void)level;
    (void)user_data;
    fputs(text, stderr); // Imprime a mensagem de log no stderr
    fflush(stderr);
}

int main() {
    try { // <--- Adicione o try
        
        llama_log_set(log_callback, nullptr); // Use o callback de log
        ggml_backend_load_all(); 

        CoreIntegration alyssa_brain;
        std::cout << "Inicializando CoreIntegration..." << std::endl;
    
        // 1. Chama a Inicialização com o caminho do modelo BASE
        if (!alyssa_brain.initialize("models/gemma-3-1b-it-q4_0.gguf")) {
            std::cerr << "Falha Crítica ao inicializar o CoreIntegration. Encerrando." << std::endl;
            return 1;
        }
        std::cout << "CoreIntegration Inicializado...";
        
        // PiperTTS tts("models/en_US-ljspeech-high.onnx", "models/en_US-ljspeech-high.onnx.json", "piper/libpiper/build/espeak_ng-install/share/espeak-ng-data");

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
                
                //  Usa Weighted Fusion em vez de think simples
                std::string alyssa_response = alyssa_brain.think_with_fusion(user_input, tts);
                
                stt.resume();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
       }

    } catch (const std::exception& e) { // <--- Adicione o catch
        std::cerr << "\n[ERRO FATAL] Exceção não capturada: " << e.what() << std::endl;
        return 1;
    } catch (...) { // <--- Pega qualquer outra coisa
        std::cerr << "\n[ERRO FATAL] Erro desconhecido não capturado." << std::endl;
        return 1;
    }
    return 0;
}