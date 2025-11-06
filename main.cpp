// main.cpp
#include "llama.h"
#include "includes/CoreLLM.hpp"
#include "includes/internal/SocialLLM.hpp"
#include "includes/internal/EmotionLLM.hpp"
#include "includes/internal/IntrospectionLLM.hpp"
#include "includes/internal/MemoryLLM.hpp"
#include "AlyssaCore.hpp"

int main() {
    // Configuração de Log e Backends (como no seu original)
    llama_log_set([](enum ggml_log_level level, const char * text, void *) {
        if (level >= GGML_LOG_LEVEL_ERROR)
            fprintf(stderr, "%s", text);
    }, nullptr);
    ggml_backend_load_all();

   try {
        // 1. Carrega o MODELO BASE (pesado) uma única vez
        alyssa_core::AlyssaCore core("models/gemma-3-1b-it-q4_0.gguf"); // <-- Modelo BASE

        // 2. Cria os ESPECIALISTAS, que agora CARREGA SUA PRÓPRIA CONFIG
        internal::SocialLLM social_expert(
            core.get_model(), 
            core.get_vocab(), 
            "socialModel" // <-- ID do especialista no JSON
        );

        internal::EmotionLLM emotion_expert (
            core.get_model(),
            core.get_vocab(),
            "emotionalModel"  // <-- ID do especialista no JSON
        );

        internal::IntrospectionLLM introspection_expert (
            core.get_model(),
            core.get_vocab(),
            "introspectiveModel"  // <-- ID do especialista no JSON
        );

        internal::MemoryLLM memory_expert (
            core.get_model(),
            core.get_vocab(),
            "memoryModel"  // <-- ID do especialista no JSON
        );

        // 3. Loop de Interação (usando o especialista social)
        std::cout << "AlyssaNet (Social) carregada. Digite 'sair' para encerrar.\n";
        std::string user_input;

        while (true) {
            printf("\033[32m> \033[0m"); 
            std::getline(std::cin, user_input);
            if (user_input == "sair" || user_input.empty()) break;

            printf("\033[33m[Social]: \033[0m"); 
            
            std::string social_response = social_expert.generate_response(user_input);
            
            printf("\n\033[0m");

            printf("\033[33m[Emocional]: \033[0m"); 
            
            std::string emotion_response = emotion_expert.generate_response(user_input);
            
            printf("\n\033[0m");

            printf("\033[33m[Introspective]: \033[0m"); 
            
            std::string introspective_response = introspection_expert.generate_response(user_input);
            
            printf("\n\033[0m");

            printf("\033[33m[Memory]: \033[0m"); 
            
            std::string memory_response = memory_expert.generate_response(user_input);
            
            printf("\n\033[0m");
        }
        
    } catch (const std::exception& e) {
        fprintf(stderr, "Erro Crítico: %s\n", e.what());
        return 1;
    }
    return 0;
}