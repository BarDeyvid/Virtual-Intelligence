#include "voice/VoicePipeline.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

// (Você precisará de uma forma de detectar Ctrl+C, 
// mas para este exemplo, apenas rodamos por um tempo)
#include <csignal>
std::atomic<bool> g_app_running = true;
void signal_handler(int signum) {
    g_app_running = false;
}

int main() {
    // Configura o handler para Ctrl+C
    signal(SIGINT, signal_handler);
    std::cout.setf(std::ios::unitbuf); // Saída não bufferizada

    const std::string model_path = "models/ggml-medium.en.bin"; 
    
    try {
        // 1. Configura as opções (opcional)
        VoicePipeline::Options options;
        options.language = "pt";
        options.n_threads = 4;
        options.vad_silence_ms = 1200; // 1.2s de silêncio

        // 2. Cria a pipeline
        VoicePipeline pipeline(model_path, options);

        // 3. Inicia a pipeline
        if (!pipeline.start()) {
            std::cerr << "Falha ao iniciar a pipeline." << std::endl;
            return 1;
        }

        std::cout << "Pressione Ctrl+C para sair." << std::endl;

        // 4. Loop principal da aplicação (apenas consome resultados)
        while (g_app_running) {
            std::string result;
            
            // Verifica se há um novo resultado
            if (pipeline.get_last_result(result)) {
                std::cout << "🗣️ [TRANSCRIÇÃO]: " << result << std::endl;
            }

            // Não bloqueie a thread principal, apenas durma um pouco
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // 5. Para a pipeline (o destrutor também faria isso, 
        // mas é uma boa prática ser explícito)
        std::cout << "\nEncerrando..." << std::endl;
        pipeline.stop();

    } catch (const std::exception& e) {
        std::cerr << "ERRO CRÍTICO: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Programa principal encerrado." << std::endl;
    return 0;
}