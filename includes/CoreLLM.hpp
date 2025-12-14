#include "AlyssaCore.hpp"
#include "WeightedFusion/WeightedFusion.hpp"
#include "voice/ElevenLabsTTS.hpp"
#include "AlyssaMemoryHandler.hpp"
#include "IExpert.hpp"
#include <memory>
#include <map>
#include <vector>
#include <unordered_map>

class CoreIntegration {
private:
    std::unique_ptr<alyssa_core::AlyssaCore> core_instance;
    std::unique_ptr<alyssa_fusion::WeightedFusion> fusion_engine;
    std::shared_ptr<Embedder> embedder;
    std::unique_ptr<alyssa_memory::AlyssaMemoryManager> memory_manager;
    
    // Especialistas gerenciados via interface
    std::unordered_map<std::string, std::unique_ptr<alyssa_experts::IExpert>> experts;
    std::unordered_map<std::string, std::vector<llama_chat_message>> expert_histories;
    
    std::string active_expert_in_cache;
    bool initialized;

public:
    CoreIntegration();
    ~CoreIntegration();

    void set_user_name(const std::string& name);
    std::string user_name = "";
    
    bool initialize(const std::string& base_model_path);
    
    // Métodos principais
    std::string think(const std::string& input, ElevenLabsTTS& tts);
    std::string think_with_fusion(const std::string& input, ElevenLabsTTS& tts);
    std::string think_with_fusion_ttsless(const std::string& input);
    
    // Gerenciamento de especialistas
    void register_expert(std::unique_ptr<alyssa_experts::IExpert> expert);
    void remove_expert(const std::string& expert_id);
    bool has_expert(const std::string& expert_id) const;
    bool validate_context_size(const std::string& prompt, const std::string& expert_id);
    std::string detect_emotion_with_heuristics(const std::string& input);

    float calculate_committee_coherence(const std::vector<alyssa_fusion::ExpertContribution>& contributions);
    bool should_store_in_memory(const std::string& input, const std::string& response);

    // Utilitários
    std::string generate_fused_input(
        const std::string& original_input,
        const std::vector<alyssa_fusion::ExpertContribution>& contributions,
        const std::string& emotion
    );
    
    void log_source_awareness(const std::string& source, const std::string& message);

    void clear_all_cache() {
        clear_kv_cache();
        for (auto& pair : expert_histories) {
            for (auto& msg : pair.second) {
                free((char*)msg.content);
            }
            pair.second.clear();
        }
        std::cout << "[Orquestrador] Todos os caches limpos." << std::endl;
    }

private:
    // Execução de especialistas
    std::string run_expert(
        const std::string& expert_id,
        const std::string& input,
        bool use_tts = false,
        ElevenLabsTTS* tts = nullptr
    );

    bool are_signals_compatible(const std::string& signal1, const std::string& signal2);
    float calculate_string_similarity(const std::string& str1, const std::string& str2);
    bool is_small_talk(const std::string& input);
    
    // Comitê de especialistas para fusão
    std::vector<alyssa_fusion::ExpertContribution> run_expert_committee(
        const std::vector<std::string>& expert_ids,
        const std::string& input
    );
    
    // Gerenciamento de histórico e cache
    void manage_dynamic_history(
        const std::string& expert_id,
        std::vector<llama_chat_message>& history
    );
    
    size_t calculate_history_limit(const std::string& expert_id);
    void clear_kv_cache();
    void switch_expert_context(const std::string& new_expert_id);
    
    // Métodos de reflexão e ação
    void act(const std::string& command);
    void reflect();

public:
    void run_interactive_loop();
};