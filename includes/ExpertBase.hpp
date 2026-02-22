// ExpertBase.hpp
#pragma once
#include "IExpert.hpp"
#include "AlyssaMemoryHandler.hpp"
#include "pc_metrics_reader.cpp"
#include <memory>
#include <regex>

namespace alyssa_experts {
    
    /**
     * @class ExpertBase
     * @brief Base implementation of IExpert interface with common functionality.
     * 
     * This class provides a concrete implementation of the IExpert interface
     * with standard functionality for model loading, inference, and response parsing.
     * Specialized experts can inherit from this class to override specific behavior.
     */
    class ExpertBase : public IExpert {
    protected:
        SimpleModelConfig config;                          ///< Expert configuration
        std::vector<llama_chat_message> history;           ///< Conversation history
        llama_adapter_lora* lora;                          ///< LoRA adapter (if used)
        std::string expert_id;                             ///< Unique expert identifier

        /**
         * @brief Parse structured signal from expert's raw response.
         * @param raw_response Raw text response from expert.
         * @param expert_id Expert identifier for format-specific parsing.
         * @return Parsed structured signal string.
         * @details Different experts use different output formats:
         *          - emotionalModel: [SINAL] type [CONFIANÇA] score [JUSTIFICATIVA] text
         *          - analyticalModel: [SINAL] type [CONFIANÇA] score [PADRÃO] pattern
         *          - memoryModel: [FATO] fact [CONFIANÇA] score [CONTEXTO] context
         */
        std::string parse_expert_signal(const std::string& raw_response, const std::string& expert_id) {
            std::string signal;
            
            // Padrões de parsing para cada especialista
            std::regex pattern;
            
            if (expert_id == "emotionalModel") {
                pattern = std::regex(R"(\[SINAL\]\s*(\w+)\s*\[CONFIANÇA\]\s*(\d+\.?\d*)\s*\[JUSTIFICATIVA\]\s*(.+))");
            } else if (expert_id == "analyticalModel") {
                pattern = std::regex(R"(\[SINAL\]\s*(\w+)\s*\[CONFIANÇA\]\s*(\d+\.?\d*)\s*\[PADRÃO\]\s*(.+))");
            } else if (expert_id == "memoryModel") {
                pattern = std::regex(R"(\[FATO\]\s*(\w+)\s*\[CONFIANÇA\]\s*(\d+\.?\d*)\s*\[CONTEXTO\]\s*(.+))");
            } else {
                // Fallback para outros especialistas
                pattern = std::regex(R"(\[SINAL\].+)");
            }
            
            std::smatch matches;
            if (std::regex_search(raw_response, matches, pattern) && matches.size() >= 2) {
                signal = matches[0];
            } else {
                // Se não seguir o formato, retorna sinal de erro
                signal = "[ERRO] Formato inválido. Use: [SINAL] tipo [CONFIANÇA] 0.00-1.00 [CONTEXTO] info";
            }
            
            return signal;
        }

    public:
        /**
         * @brief Constructor with configuration.
         * @param cfg Expert configuration structure.
         */
        ExpertBase(const SimpleModelConfig& cfg) 
            : config(cfg), lora(nullptr), expert_id(cfg.id) 
        {
        }
        
        /**
         * @brief Destructor with resource cleanup.
         */
        ~ExpertBase() override {
            clear_history();
            if (lora) {
                llama_adapter_lora_free(lora);
            }
        }

        /**
         * @brief Get expert's unique identifier.
         * @return Expert ID string.
         */
        const std::string& get_id() const override { return expert_id; }
        
        /**
         * @brief Get expert's configuration.
         * @return Reference to expert's SimpleModelConfig.
         */
        const SimpleModelConfig& get_config() const override { return config; }
        
        /**
         * @brief Get expert's conversation history.
         * @return Const reference to conversation history vector.
         */
        const std::vector<llama_chat_message>& get_history() const override { return history; }
        
        /**
         * @brief Clear expert's conversation history.
         * @details Frees allocated memory for message content.
         */
        void clear_history() override {
            for (auto& msg : history) {
                free((char*)msg.content);
            }
            history.clear();
        }

        /**
         * @brief Initialize the expert with a shared model.
         * @param shared_model Pointer to shared llama model for LoRA adaptation.
         * @return true if initialization succeeded, false otherwise.
         */
        bool initialize(llama_model* shared_model) override {
            if (config.usa_LoRA && !config.lora_path.empty()) {
                lora = llama_adapter_lora_init(shared_model, config.lora_path.c_str());
                if (!lora) {
                    std::cerr << "Falha ao carregar LoRA: " << config.lora_path << std::endl;
                    return false;
                }
                std::cout << "LoRA carregado para " << expert_id << ": " << config.lora_path << std::endl;
            }
            return true;
        }

        /**
         * @brief Main execution method for expert inference.
         * @param input Text input for the expert.
         * @param core_instance Pointer to AlyssaCore instance for generation.
         * @param lora_override Optional LoRA adapter override (nullptr for default).
         * @param current_history Conversation history for context.
         * @param active_lora_in_context Output parameter for active LoRA adapter.
         * @param stream_callback Optional callback for streaming output tokens.
         * @return Expert's response as string.
         */
        std::string run(
            const std::string& input,
            alyssa_core::AlyssaCore* core_instance,
            llama_adapter_lora* lora_override,
            std::vector<llama_chat_message>& current_history,
            llama_adapter_lora** active_lora_in_context,
            std::function<void(const std::string&)> stream_callback = nullptr
        ) override {
            PCMetricsReader pcmetrics;

            // 1. Adiciona mensagem do usuário ao histórico
            std::string expert_input_with_role = "";
            if (!config.role_instruction.empty()) {
                expert_input_with_role = "[ROLE]: " + config.role_instruction + "\n" + input;
            } else {
                expert_input_with_role = input;
            }
            
            // IMPORTANTE: Verificar se precisamos alocar memória para a string
            char* user_msg = strdup(expert_input_with_role.c_str());
            current_history.push_back({"user", user_msg});

            // 2. Monta template com métricas do sistema
            std::vector<llama_chat_message> messages_to_template;
            int system_prompt_index = -1;
            
            std::string external = pcmetrics.get_simple_metrics_text();
            std::string combined_system_prompt = config.system_prompt;
            
            if (!external.empty()) {
                combined_system_prompt += "\n[CONTEXTO DO SISTEMA - MÉTRICAS DO PC]:\n" + external;
            }
            
            if (!combined_system_prompt.empty()) {
                system_prompt_index = messages_to_template.size();
                char* sys_msg = strdup(combined_system_prompt.c_str());
                messages_to_template.push_back({"system", sys_msg});
            }
            
            // Adiciona histórico da conversa
            messages_to_template.insert(messages_to_template.end(), 
                                      current_history.begin(), current_history.end());

            // 3. Aplica template
            std::vector<char> formatted(core_instance->get_n_ctx() * 2); // Buffer maior
            const char* tmpl = llama_model_chat_template(core_instance->get_model(), nullptr);

            int len = llama_chat_apply_template(
                tmpl, messages_to_template.data(), messages_to_template.size(), 
                true, formatted.data(), formatted.size()
            );

            // Limpa alocação do system prompt (se existir)
            if (system_prompt_index != -1) {
                free((char*)messages_to_template[system_prompt_index].content);
            }

            if (len < 0) {
                // Tentar com buffer maior
                formatted.resize(-len);
                len = llama_chat_apply_template(
                    tmpl, messages_to_template.data(), messages_to_template.size(),
                    true, formatted.data(), formatted.size()
                );
            }

            if (len < 0) {
                return "Erro ao processar template de conversa.";
            }

            std::string prompt(formatted.begin(), formatted.begin() + len);

            // 4. Executa geração
            // llama_adapter_lora** final_lora = (lora_override != nullptr) ? lora_override : lora;
            
            // Atualizar o ponteiro ativo de LoRA
            // if (active_lora_in_context != nullptr) {
            //     *active_lora_in_context = final_lora;
            // }
            
            std::string response = core_instance->generate_raw(
                prompt,
                config.params,
                nullptr, // Usar LoRA padrão do especialista (lora) - desabilitado para evitar conflitos
                stream_callback
            );

            // 5. Adiciona resposta ao histórico
            char* assistant_msg = strdup(response.c_str());
            current_history.push_back({"assistant", assistant_msg});

            return response;
        }

        /**
         * @brief Get structured contribution for weighted fusion.
         * @param input Text input for the expert.
         * @param core_instance Pointer to AlyssaCore instance.
         * @param embedder Shared pointer to embedding generator.
         * @param lora_override Optional LoRA adapter override.
         * @param current_history Conversation history for context.
         * @param active_lora_in_context Output parameter for active LoRA adapter.
         * @param stream_callback Optional callback for streaming.
         * @return Structured contribution with embedding for fusion.
         */
        alyssa_fusion::ExpertContribution get_contribution(
            const std::string& input,
            alyssa_core::AlyssaCore* core_instance,
            std::shared_ptr<Embedder> embedder,
            llama_adapter_lora* lora_override,
            std::vector<llama_chat_message>& current_history,
            llama_adapter_lora** active_lora_in_context,
            std::function<void(const std::string&)> stream_callback = nullptr
        ) override {
            alyssa_fusion::ExpertContribution contrib;
            contrib.expert_id = expert_id;
            
            // Executa geração
            std::string raw_response = run(input, core_instance, lora_override, 
                                        current_history, active_lora_in_context, stream_callback);
            
            // PARSE DA RESPOSTA EM SINAL ESTRUTURADO
            contrib.response = parse_expert_signal(raw_response, expert_id);
            
            // Remover atribuição de identidades do usuário
            contrib.source = expert_id; // Usar apenas o ID, sem interpretação
            
            if (embedder) {
                try {
                    contrib.embedding = embedder->generate_embedding(contrib.response);
                } catch (const std::exception& e) {
                    std::cerr << "Erro ao calcular embedding para " << expert_id 
                            << ": " << e.what() << std::endl;
                }
            }
            
            return contrib;
        }
    };
}