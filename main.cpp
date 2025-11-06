// main.cpp
#include "llama.h"
#include "AlyssaCore.h"
#include "SocialLLM.h"
// #include "EmotionLLM.h" // Você criaria este arquivo similar ao SocialLLM

int main() {
    // Configuração de Log e Backends (como no seu original)
    llama_log_set([](enum ggml_log_level level, const char * text, void *) {
        if (level >= GGML_LOG_LEVEL_ERROR)
            fprintf(stderr, "%s", text);
    }, nullptr);
    ggml_backend_load_all();

   try {
        // 1. Carrega o MODELO BASE (pesado) uma única vez
        AlyssaCore core("models/gemma-3-270m-it-F16.gguf"); // <-- Modelo BASE

        // 2. Cria os ESPECIALISTAS passando o modelo e seus LoRAs
        SocialLLM social_expert(
            core.get_model(), 
            core.get_vocab(), 
            "models/lora_social_adapter.gguf" // <-- Fine-tune SOCIAL
        );
        social_expert.set_system_prompt("Você é um assistente social e comunicativo.");

        // Exemplo de como você criaria outro especialista
        // EmotionLLM emotion_expert(
        //     core.get_model(), 
        //     core.get_vocab(),
        //     "models/lora_emotion_adapter.gguf" // <-- Fine-tune EMOÇÃO
        // );
        // emotion_expert.set_system_prompt("Você é um assistente empático.");

        // 3. Loop de Interação (usando o especialista social)
        std::cout << "AlyssaNet (Social) carregada. Digite 'sair' para encerrar.\n";
        std::string user_input;

        while (true) {
            printf("\033[32m> \033[0m"); 
            std::getline(std::cin, user_input);
            if (user_input == "sair" || user_input.empty()) break;

            printf("\033[33m[Social]: \033[0m"); 
            
            // Chama a resposta (assumindo que 'history' agora é interno)
            std::string response = social_expert.generate_response(user_input);
            
            printf("\n\033[0m");
        }
        
        // Os destrutores de social_expert e core são chamados automaticamente,
        // liberando tudo na ordem correta (primeiro o contexto, depois o modelo).

    } catch (const std::exception& e) {
        fprintf(stderr, "Erro Crítico: %s\n", e.what());
        return 1;
    }
    return 0;
}