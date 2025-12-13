#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "llama.h"
#include "AlyssaCore.hpp"
#include "WeightedFusion/WeightedFusion.hpp"
#include "AlyssaMemoryHandler.hpp"

/**
 * @brief Namespace da nova arquitetura
 */
namespace alyssa_experts {
    class IExpert {
    public:
        virtual ~IExpert() = default;

        // Inicialização Padrão
        virtual bool initialize(llama_model* shared_model) = 0;

        // Método principal para obter a resposta
        virtual std::string run(
            const std::string& input,
            alyssa_core::AlyssaCore* core_instance,
            llama_adapter_lora* lora_override,
            std::vector<llama_chat_message>& history,
            llama_adapter_lora** active_lora_in_context,
            std::function<void(const std::string&)> stream_callback = nullptr
        ) = 0;

        // Obter contribuição para fusão ponderada
        virtual alyssa_fusion::ExpertContribution get_contribution(
            const std::string& input,
            alyssa_core::AlyssaCore* core_instance,
            std::shared_ptr<Embedder> embedder,
            llama_adapter_lora* lora_override,
            std::vector<llama_chat_message>& history,
            llama_adapter_lora** active_lora_in_context,
            std::function<void(const std::string&)> stream_callback = nullptr
        ) = 0;

        virtual const std::string& get_id() const = 0;
        virtual const SimpleModelConfig& get_config() const = 0;
        virtual const std::vector<llama_chat_message>& get_history() const = 0;
        virtual void clear_history() = 0;
    };
}