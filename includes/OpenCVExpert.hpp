// ───────────────────────────────────────────────────────
// OpenCVExpert.hpp (modified)
// ───────────────────────────────────────────────────────
#pragma once

#include "IExpert.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/cudaimgproc.hpp>   // optional GPU
#include <iostream>

namespace alyssa_experts {

class OpenCVExpert : public IExpert {
private:
    SimpleModelConfig config;          ///< Loaded from ConfigsLLM.json
    std::vector<llama_chat_message> history;
    llama_adapter_lora* lora = nullptr;   // optional LoRA
    std::string expert_id;

public:
    explicit OpenCVExpert(const SimpleModelConfig& cfg)
        : config(cfg), expert_id(cfg.id) {}

    ~OpenCVExpert() override {
        clear_history();
        if (lora) llama_adapter_lora_free(lora);
    }

    /* ──────────────────────── IExpert ─────────────────────── */
    const std::string& get_id() const override { return expert_id; }
    const SimpleModelConfig& get_config() const override { return config; }
    const std::vector<llama_chat_message>& get_history() const override { return history; }

    void clear_history() override {
        for (auto& m : history) free((char*)m.content);
        history.clear();
    }

    bool initialize(llama_model* shared_model) override {
        // If a LoRA path is provided, load it.
        if (!config.lora_path.empty()) {
            lora = llama_adapter_lora_init(shared_model, config.lora_path.c_str());
            if (!lora) std::cerr << "[OpenCVExpert] LoRA init failed\n";
        }
        return true;
    }

    /* ──────────────────────── Core execution ─────────────────────── */
    std::string run(
        const std::string& input,
        alyssa_core::AlyssaCore* core_instance,
        llama_adapter_lora* lora_override,
        std::vector<llama_chat_message>& current_history,
        llama_adapter_lora** active_lora_in_context,
        std::function<void(const std::string&)> stream_callback = nullptr
    ) override;

    alyssa_fusion::ExpertContribution get_contribution(
        const std::string& input,
        alyssa_core::AlyssaCore* core_instance,
        std::shared_ptr<Embedder> embedder,
        llama_adapter_lora* lora_override,
        std::vector<llama_chat_message>& current_history,
        llama_adapter_lora** active_lora_in_context,
        std::function<void(const std::string&)> stream_callback = nullptr
    ) override;
private:
    /* Helper: convert raw OpenCV output to a signal for fusion */
    std::string parse_expert_signal(const std::string& raw, const std::string&) const {
        return "[SIGNAL] " + raw;   // simple tag – weighted‑fusion knows to ignore it
    }
};

}   // namespace alyssa_experts
