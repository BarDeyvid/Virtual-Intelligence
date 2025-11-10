#pragma once
#include "includes/AlyssaCore.hpp"
#include "includes/WeightedFusion/WeightedFusion.hpp"
#include "includes/voice/PiperTTS.hpp"
#include "includes/voice/ElevenLabsTTS.hpp"
#include "includes/AlyssaMemoryHandler.hpp"
#include <memory>
#include <map>
#include <vector>

class CoreIntegration {
private:
    std::unique_ptr<alyssa_core::AlyssaCore> core_instance;
    std::unique_ptr<alyssa_fusion::WeightedFusion> fusion_engine;
    std::unique_ptr<Embedder> embedder;

    std::unique_ptr<AlyssaMemoryManager> memory_manager;
    
    // Mapas de configuração e estado
    std::map<std::string, SimpleModelConfig> expert_configs;
    std::map<std::string, std::vector<llama_chat_message>> expert_histories;
    std::map<std::string, llama_adapter_lora*> lora_cache;
    
    std::string active_expert_in_cache;
    bool initialized;

public:
    CoreIntegration();
    ~CoreIntegration();
    
    bool initialize(const std::string& base_model_path);
    std::string think(const std::string& input, PiperTTS& tts);
    
    // 🆕 Método com Weighted Fusion
    std::string think_with_fusion(const std::string& input, ElevenLabsTTS& tts);
    
private:
    std::string run_expert(const std::string& expert_id, 
                          const std::string& input,
                          bool use_tts, 
                          PiperTTS* tts = nullptr);
    
    // 🆕 Método para executar múltiplos especialistas e fundir
    std::vector<alyssa_fusion::ExpertContribution> run_expert_committee(
        const std::vector<std::string>& expert_ids,
        const std::string& input);
    
    void clear_kv_cache();
    void act(const std::string& command);
    void reflect();
    
public:
    void run_interactive_loop();
};