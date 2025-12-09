#include "AlyssaCore.hpp"
#include "WeightedFusion/WeightedFusion.hpp"
#include "voice/ElevenLabsTTS.hpp"
#include "AlyssaMemoryHandler.hpp"
#include <memory>
#include <map>
#include <vector>

class CoreIntegration {
private:
    std::unique_ptr<alyssa_core::AlyssaCore> core_instance;
    std::unique_ptr<alyssa_fusion::WeightedFusion> fusion_engine;
    std::shared_ptr<Embedder> embedder;

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

    void set_user_name(const std::string& name);
    std::string user_name = "";
    
    bool initialize(const std::string& base_model_path);
    std::string think(const std::string& input, ElevenLabsTTS& tts);
    
    //  Métodos com Weighted Fusion
    std::string think_with_fusion(const std::string& input, ElevenLabsTTS& tts);
    std::string think_with_fusion_ttsless(const std::string& input); 
    std::string generate_fused_input(const std::string& original_input, const std::vector<alyssa_fusion::ExpertContribution>& contributions, const std::string& emotion);
    void log_source_awareness(const std::string& source, const std::string& message);

private:
    std::string run_expert(const std::string& expert_id, 
                          const std::string& input,
                          bool use_tts, 
                          ElevenLabsTTS* tts = nullptr);
    
    //  Método para executar múltiplos especialistas e fundir
    std::vector<alyssa_fusion::ExpertContribution> run_expert_committee(
        const std::vector<std::string>& expert_ids,
        const std::string& input);
    
    void manage_dynamic_history(const std::string& expert_id, 
                               std::vector<llama_chat_message>& history);
    
    size_t calculate_history_limit(const std::string& expert_id);

    void clear_kv_cache();
    void act(const std::string& command);
    void reflect();
    
public:
    void run_interactive_loop();
};