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
 * @namespace alyssa_experts
 * @brief Namespace containing expert model interfaces for the Alyssa AI system.
 */
namespace alyssa_experts {
    
    /**
     * @class IExpert
     * @brief Interface for specialized expert models in the Mixture of Experts architecture.
     * 
     * This interface defines the contract that all expert models must implement.
     * Experts are specialized AI models focused on specific domains (emotional analysis,
     * memory retrieval, creative thinking, etc.) that contribute to the overall system.
     */
    class IExpert {
    public:
        /**
         * @brief Virtual destructor for proper cleanup.
         */
        virtual ~IExpert() = default;

        // =========================================================================
        // Initialization
        // =========================================================================
        
        /**
         * @brief Initialize the expert with a shared model.
         * @param shared_model Pointer to shared llama model for LoRA adaptation.
         * @return true if initialization succeeded, false otherwise.
         * @details Loads LoRA adapters if configured and prepares expert for inference.
         */
        virtual bool initialize(llama_model* shared_model) = 0;

        // =========================================================================
        // Core Execution
        // =========================================================================
        
        /**
         * @brief Main execution method for expert inference.
         * @param input Text input for the expert.
         * @param core_instance Pointer to AlyssaCore instance for generation.
         * @param lora_override Optional LoRA adapter override (nullptr for default).
         * @param history Conversation history for context.
         * @param active_lora_in_context Output parameter for active LoRA adapter.
         * @param stream_callback Optional callback for streaming output tokens.
         * @return Expert's response as string.
         */
        virtual std::string run(
            const std::string& input,
            alyssa_core::AlyssaCore* core_instance,
            llama_adapter_lora* lora_override,
            std::vector<llama_chat_message>& history,
            llama_adapter_lora** active_lora_in_context,
            std::function<void(const std::string&)> stream_callback = nullptr
        ) = 0;

        /**
         * @brief Get structured contribution for weighted fusion.
         * @param input Text input for the expert.
         * @param core_instance Pointer to AlyssaCore instance.
         * @param embedder Shared pointer to embedding generator.
         * @param lora_override Optional LoRA adapter override.
         * @param history Conversation history for context.
         * @param active_lora_in_context Output parameter for active LoRA adapter.
         * @param stream_callback Optional callback for streaming.
         * @return Structured contribution with embedding for fusion.
         */
        virtual alyssa_fusion::ExpertContribution get_contribution(
            const std::string& input,
            alyssa_core::AlyssaCore* core_instance,
            std::shared_ptr<Embedder> embedder,
            llama_adapter_lora* lora_override,
            std::vector<llama_chat_message>& history,
            llama_adapter_lora** active_lora_in_context,
            std::function<void(const std::string&)> stream_callback = nullptr
        ) = 0;

        // =========================================================================
        // Accessor Methods
        // =========================================================================
        
        /**
         * @brief Get expert's unique identifier.
         * @return Expert ID string.
         */
        virtual const std::string& get_id() const = 0;
        
        /**
         * @brief Get expert's configuration.
         * @return Reference to expert's SimpleModelConfig.
         */
        virtual const SimpleModelConfig& get_config() const = 0;
        
        /**
         * @brief Get expert's conversation history.
         * @return Const reference to conversation history vector.
         */
        virtual const std::vector<llama_chat_message>& get_history() const = 0;
        
        /**
         * @brief Clear expert's conversation history.
         * @details Frees allocated memory for message content.
         */
        virtual void clear_history() = 0;
    };
}