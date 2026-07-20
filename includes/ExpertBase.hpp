// ExpertBase.hpp
#pragma once
#include "IExpert.hpp"
#include "AlyssaMemoryHandler.hpp"
#include "pc_metrics_reader.hpp"
#include <algorithm>
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
        int base_max_tokens_ = -1;                         ///< max_tokens original do config (gate F2 escala sobre ele)

        /**
         * @brief Parse structured signal from expert's raw response (FLEXIBLE VERSION).
         * @param raw_response Raw text response from expert.
         * @param expert_id Expert identifier for format-specific parsing.
         * @return Parsed structured signal string (returns response if format fails gracefully).
         * @details Attempts multiple parsing strategies with graceful degradation:
         *          1. Try strict format-specific regex patterns
         *          2. Try flexible generic patterns ([WORD] format)
         *          3. Accept raw response as valid contribution
         */
        std::string parse_expert_signal(const std::string& raw_response, const std::string& expert_id) {
            if (raw_response.empty()) {
                return "[VAZIO] Sem resposta do especialista";
            }
            
            std::string signal;
            std::smatch matches;
            
            // ===== ESTRATÉGIA 1: Padrões estritamente formatados por tipo =====
            
            if (expert_id == "emotionalModel") {
                std::regex pattern(R"(\[SINAL\]\s*(\w+)\s*\[CONFIANÇA\]\s*(\d+\.?\d*)\s*\[(?:JUSTIFICATIVA|CONTEXTO)\]\s*(.+))");
                if (std::regex_search(raw_response, matches, pattern) && matches.size() >= 2) {
                    return matches[0];
                }
            } else if (expert_id == "analyticalModel") {
                std::regex pattern(R"(\[SINAL\]\s*(\w+)\s*\[CONFIANÇA\]\s*(\d+\.?\d*)\s*\[(?:PADRÃO|ANÁLISE|CONTEXTO)\]\s*(.+))");
                if (std::regex_search(raw_response, matches, pattern) && matches.size() >= 2) {
                    return matches[0];
                }
            } else if (expert_id == "memoryModel") {
                std::regex pattern(R"(\[FATO\]\s*(.+?)\s*\[CONFIANÇA\]\s*(\d+\.?\d*)\s*\[CONTEXTO\]\s*(.+))");
                if (std::regex_search(raw_response, matches, pattern) && matches.size() >= 2) {
                    return matches[0];
                }
            } else if (expert_id == "introspectiveModel") {
                std::regex pattern(R"(\[(?:INTROSPECÇÃO|REFLEXÃO|INSIGHT)\]\s*(.+))");
                if (std::regex_search(raw_response, matches, pattern) && matches.size() >= 2) {
                    return matches[0];
                }
            } else if (expert_id == "gameplayModel") {
                // Formato imposto pela grammar GBNF (config/grammars/gameplay_action.gbnf);
                // sem fallback genérico aqui — uma ação malformada não deve chegar ao ActionExecutor.
                std::regex pattern(R"(\[AÇÃO\]\s*(\w+)\s*(.*?)\s*\[CONFIANÇA\]\s*(\d+\.?\d*)\s*\[CONTEXTO\]\s*(.+))");
                if (std::regex_search(raw_response, matches, pattern) && matches.size() >= 2) {
                    return matches[0];
                }
                return "[AÇÃO] esperar [CONFIANÇA] 0 [CONTEXTO] resposta malformada, aguardando";
            }
            
            // ===== ESTRATÉGIA 2: Padrões genéricos flexíveis =====
            
            // Procura por qualquer [LABEL] estrutura
            std::regex generic_pattern(R"(\[[\w\s]+\].+)");
            if (std::regex_search(raw_response, matches, generic_pattern)) {
                return matches[0];
            }
            
            // Procura por confiança/score mesmo sem [LABEL]
            std::regex confidence_pattern(R"((?:confiança|confidence|score|score:|prob|probabilidade)[\s:]*(\d+\.?\d*))");
            if (std::regex_search(raw_response, matches, confidence_pattern)) {
                // Tem alguma estrutura de confiança
                std::string score = matches[1];
                return "[SINAL] " + expert_id + " [CONFIANÇA] " + score + " [RESPOSTA] " + raw_response;
            }
            
            // ===== ESTRATÉGIA 3: Aceitar resposta bruta com fallback gracioso =====
            
            // Se nada funcionar, retornar a resposta bruta mas alertar
            if (raw_response.length() > 200) {
                // Truncar respostas muito longas
                return raw_response.substr(0, 200) + "...";
            }
            
            return raw_response;  // Aceitar como está - graceful degradation
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
            free_chat_history(history);
        }

        /**
         * @brief Gate hormonal de tamanho de resposta (v2/F2).
         * @details Captura o max_tokens do config na primeira chamada e escala
         *          sempre sobre ESSE original (nunca composto). Piso de 24
         *          tokens: mesmo de saco cheio ela responde uma frase.
         */
        void set_max_tokens_scale(double scale) override {
            if (base_max_tokens_ < 0) base_max_tokens_ = config.params.max_tokens;
            scale = std::clamp(scale, 0.25, 1.0);
            config.params.max_tokens =
                std::max(24, static_cast<int>(base_max_tokens_ * scale));
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
         * @brief Formata mensagens no turn format do Gemma 4 (arch "gemma4").
         * @details Espelha o Jinja embutido no GGUF para o caso texto-puro sem
         *          tools: system/user/model entre "<|turn>role\n" e "<turn|>\n",
         *          terminando com o generation prompt "<|turn>model\n". Os
         *          marcadores são tokens especiais — o tokenize precisa rodar
         *          com parse_special=true (generate_raw já roda). Sem BOS aqui
         *          (o tokenizer adiciona). Usado porque a API C de template do
         *          llama.cpp não reconhece o Jinja do Gemma 4.
         */
        static std::string format_gemma4_prompt(const std::vector<llama_chat_message>& msgs) {
            std::string out;
            for (const auto& m : msgs) {
                std::string role = (m.role && *m.role) ? m.role : "user";
                if (role == "assistant") role = "model";
                out += "<|turn>" + role + "\n";
                if (m.content) out += m.content;
                out += "<turn|>\n";
            }
            out += "<|turn>model\n";
            return out;
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
            push_chat_message(current_history, "user", expert_input_with_role);

            // 2. Monta template com métricas do sistema
            std::vector<llama_chat_message> messages_to_template;
            int system_prompt_index = -1;
            
            std::string external = pcmetrics.get_simple_metrics_text();
            std::string combined_system_prompt = config.system_prompt;
            
            if (!external.empty()) {
                combined_system_prompt += "\n[HARDWARE: Temp=60C (Normal/Frio), Max=130C. Uso=5.7% (Ocioso)]:\n" + external;
            }
            
            if (!combined_system_prompt.empty()) {
                system_prompt_index = messages_to_template.size();
                push_chat_message(messages_to_template, "system", combined_system_prompt);
            }
            
            // Adiciona histórico da conversa
            messages_to_template.insert(messages_to_template.end(), 
                                      current_history.begin(), current_history.end());

            // 3. Aplica template
            const char* tmpl = llama_model_chat_template(core_instance->get_model(), nullptr);
            std::string prompt;

            if (tmpl && std::string(tmpl).find("<|turn>") != std::string::npos) {
                // Gemma 4: o GGUF embute um Jinja de 16KB (tools/thinking) que
                // a API C llama_chat_apply_template não reconhece (ela só faz
                // sniffing de templates conhecidos) → len<0 e o turno inteiro
                // morria em "Erro ao processar template". Formato real do
                // template: "<|turn>role\n...<turn|>\n", assistant vira
                // "model", generation prompt é "<|turn>model\n". BOS fica de
                // fora: o tokenize do generate_raw roda com add_special=true.
                prompt = format_gemma4_prompt(messages_to_template);
            } else {
                std::vector<char> formatted(core_instance->get_n_ctx() * 2); // Buffer maior
                int len = llama_chat_apply_template(
                    tmpl, messages_to_template.data(), messages_to_template.size(),
                    true, formatted.data(), formatted.size()
                );

                if (len < 0) {
                    // Tentar com buffer maior
                    formatted.resize(-len);
                    len = llama_chat_apply_template(
                        tmpl, messages_to_template.data(), messages_to_template.size(),
                        true, formatted.data(), formatted.size()
                    );
                }

                if (len >= 0) {
                    prompt.assign(formatted.begin(), formatted.begin() + len);
                }
            }

            // Free the system prompt message we pushed into messages_to_template
            if (system_prompt_index != -1) {
                free(const_cast<char*>(messages_to_template[system_prompt_index].content));
            }

            if (prompt.empty()) {
                return "Erro ao processar template de conversa.";
            }

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
            push_chat_message(current_history, "assistant", response);

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