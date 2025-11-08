// CoreLLM.hpp
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include "llama.h"
#include "AlyssaCore.hpp" 
#include "voice/PiperTTS.hpp" 

namespace alyssa_core {
    class AlyssaCore;
}

class PiperTTS;

class CoreIntegration {
private:
    // O ÚNICO MOTOR
    std::unique_ptr<alyssa_core::AlyssaCore> core_instance;

    // MAPAS DE ESPECIALISTAS
    // Mapeia "expert_id" (ex: "socialModel") para sua configuração
    std::map<std::string, SimpleModelConfig> expert_configs;
    
    // Mapeia "expert_id" para seu histórico de chat
    std::map<std::string, std::vector<llama_chat_message>> expert_histories;

    // Mapeia "expert_id" para seu adaptador LoRA pré-carregado (ou nullptr)
    std::map<std::string, llama_adapter_lora*> lora_cache;
    
    bool initialized = false;

    // Novo método privado para executar um especialista
    std::string run_expert(const std::string& expert_id, const std::string& input, PiperTTS& tts);

    // Método para limpar o KV Cache quando trocamos de especialista
    void clear_kv_cache();

    // Contador para saber qual especialista está no cache
    std::string active_expert_in_cache;

public:
    CoreIntegration();
    ~CoreIntegration();

    bool initialize(const std::string& model_path);
    std::string think(const std::string& input, PiperTTS& tts); // (Interface pública inalterada)
    void act(const std::string& command);
    void reflect();
    void run_interactive_loop();
};