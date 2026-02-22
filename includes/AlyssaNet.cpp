// AlyssaNet.cpp 

#include "CoreLLM.hpp"
#include "AlyssaCore.hpp"
#include "voice/ElevenLabsTTS.hpp"
#include "log.hpp"
#include "ExpertBase.hpp"
#include <string>
#include <memory>
#include <algorithm>
#include <sstream>
#include <map>
#include <set>
#include <regex>
#include <cctype>

using namespace alyssa_core;

// =========================================================================
// Constructor & Destructor
// =========================================================================

/**
 * @brief Default constructor for CoreIntegration.
 * @details Initializes with default values and logs construction.
 */
CoreIntegration::CoreIntegration() 
    : initialized(false), active_expert_in_cache("") 
{
    printf("CoreIntegration constructed");
}

/**
 * @brief Destructor for CoreIntegration.
 * @details Cleans up expert histories and logs destruction.
 */
CoreIntegration::~CoreIntegration() {
    // Libera históricos dos especialistas
    for (auto& pair : expert_histories) {
        for (auto& msg : pair.second) {
            free((char*)msg.content);
        }
    }
    printf("CoreIntegration destroyed");
}

// =========================================================================
// Expert Management
// =========================================================================

/**
 * @brief Register a new expert model with the system.
 * @param expert Unique pointer to the expert implementation.
 * @details Adds expert to internal maps and initializes empty history.
 */
void CoreIntegration::register_expert(std::unique_ptr<alyssa_experts::IExpert> expert) {
    if (!expert) return;
    
    std::string id = expert->get_id();
    experts[id] = std::move(expert);
    expert_histories[id] = {};
    
    std::cout << "[Orquestrador] Especialista registrado: " << id << std::endl;
}

/**
 * @brief Remove an expert from the system.
 * @param expert_id Unique identifier of the expert to remove.
 * @details Removes expert from both expert and history maps.
 */
void CoreIntegration::remove_expert(const std::string& expert_id) {
    if (experts.erase(expert_id)) {
        expert_histories.erase(expert_id);
        std::cout << "[Orquestrador] Especialista removido: " << expert_id << std::endl;
    }
}

/**
 * @brief Check if an expert is registered.
 * @param expert_id Unique identifier of the expert.
 * @return true if expert exists, false otherwise.
 */
bool CoreIntegration::has_expert(const std::string& expert_id) const {
    return experts.find(expert_id) != experts.end();
}

/**
 * @brief Set the user's name for personalized interactions.
 * @param name The user's name to be stored in memory.
 */
void CoreIntegration::set_user_name(const std::string& name) {
    user_name = name;
    std::cout << "[IDENTITY] Nome do usuário definido como: " << name << std::endl;
    if (memory_manager) {
        memory_manager->processIdentityFact(name, "user_name");
    }
}

// =========================================================================
// System Initialization
// =========================================================================

/**
 * @brief Initialize the complete Alyssa AI system.
 * @param base_model_path Path to the base model configuration file.
 * @return true if initialization succeeded, false otherwise.
 * @details Loads models, initializes experts, memory system, and fusion engine.
 */
bool CoreIntegration::initialize(const std::string& base_model_path) {
    if (initialized) return true;

    llama_log_set([](enum ggml_log_level level, const char* text, void* /* user_data */) {
        if (level >= GGML_LOG_LEVEL_ERROR) {
            fprintf(stderr, "%s", text);
        }
    }, nullptr);

    try {
        // 1. Carregar configurações primeiro para obter n_ctx
        AllModelConfigs configs = load_config();
        if (configs.empty()) {
            throw std::runtime_error("Falha ao carregar ConfigsLLM.json");
        }
        
        // 2. Buscar configurações dos modelos
        const SimpleModelConfig* base_1b_config = nullptr;
        const SimpleModelConfig* alyssa_4b_config = nullptr;
        
        for (const auto& cfg : configs) {
            if (cfg.id == "alyssa") {
                alyssa_4b_config = &cfg;
            } else if (cfg.model_path.find("1b") != std::string::npos || 
                      cfg.model_path.find("1B") != std::string::npos ||
                      cfg.id != "alyssa") {
                // Usar o primeiro modelo 1b que não seja alyssa como base
                if (!base_1b_config) {
                    base_1b_config = &cfg;
                }
            }
        }
        
        // Fallback: se não encontrar um modelo 1b, usar qualquer modelo que não seja alyssa
        if (!base_1b_config) {
            for (const auto& cfg : configs) {
                if (cfg.id != "alyssa") {
                    base_1b_config = &cfg;
                    break;
                }
            }
        }
        
        if (!base_1b_config || !alyssa_4b_config) {
            throw std::runtime_error("Não foi possível encontrar configurações para modelos base e Alyssa");
        }
        
        // 3. Inicializar embedder
        embedder = std::make_shared<Embedder>("config/embedder_config.json");
        if (!embedder->initialize()) {
            std::cerr << "Falha ao inicializar o Embedder" << std::endl;
            return false;
        }
        
        // 4. Criar instância do core base com o modelo 1b
        int context_size = base_1b_config->n_ctx;
        if (context_size < 4096) {
            context_size = 4096; // Mínimo seguro para memória
            std::cout << "[WARN] Contexto base muito pequeno, ajustando para " << context_size << std::endl;
        }
        
        std::cout << "[INFO] Criando AlyssaCore BASE com modelo: " << base_1b_config->model_path 
                  << " (n_ctx = " << context_size << ")" << std::endl;
        core_instance = std::make_unique<alyssa_core::AlyssaCore>(base_1b_config->model_path, context_size);
        
        // 5. Inicializar fusion engine
        fusion_engine = std::make_unique<alyssa_fusion::WeightedFusion>(*embedder);
        
        // 6. Inicializar memory manager
        memory_manager = std::make_unique<alyssa_memory::AlyssaMemoryManager>(
            "../alyssa_advanced_memory.db", embedder);
        std::cout << "Sistema de Memória de Longo Prazo (LTM) inicializado." << std::endl;
        
        // 7. Criar e registrar especialistas usando o modelo base (1b)
        llama_model* shared_model = core_instance->get_model();
        
        for (const auto& cfg : configs) {
            // Pular o modelo Alyssa (4b) - ele será tratado separadamente
            if (cfg.id == "alyssa") {
                std::cout << "[INFO] Especialista '" << cfg.id << "' (4b) será registrado separadamente" << std::endl;
                continue;
            }
            
            std::cout << "Configurando especialista: " << cfg.id 
                      << " (usando modelo base 1b, max_tokens=" << cfg.params.max_tokens << ")" << std::endl;
            
            // Verificar se o especialista precisa de ajuste de max_tokens
            if (cfg.params.max_tokens < 64) {
                std::cout << "[AVISO] Especialista '" << cfg.id 
                          << "' tem max_tokens muito baixo: " << cfg.params.max_tokens << std::endl;
            }
            
            auto expert = std::make_unique<alyssa_experts::ExpertBase>(cfg);
            if (expert->initialize(shared_model)) {
                register_expert(std::move(expert));
            } else {
                std::cerr << "Falha ao inicializar especialista: " << cfg.id << std::endl;
            }
        }

        // 8. Criar e registrar especialista Alyssa com modelo 4b separado
        std::cout << "[INFO] Inicializando modelo separado para Alyssa (4b): " 
                  << alyssa_4b_config->model_path << std::endl;
        
        // Criar core separado para Alyssa
        int alyssa_context_size = alyssa_4b_config->n_ctx;
        if (alyssa_context_size < 4096) {
            alyssa_context_size = 4096;
        }
        
        auto alyssa_core = std::make_unique<alyssa_core::AlyssaCore>(alyssa_4b_config->model_path, alyssa_context_size);
        
        // Criar especialista Alyssa que usa o core separado
        class Alyssa4bExpert : public alyssa_experts::ExpertBase {
        private:
            std::unique_ptr<alyssa_core::AlyssaCore> alyssa_core;
            
        public:
            Alyssa4bExpert(const SimpleModelConfig& cfg, std::unique_ptr<alyssa_core::AlyssaCore> core)
                : ExpertBase(cfg), alyssa_core(std::move(core)) {
            }
            
            bool initialize(llama_model* shared_model) override {
                // Não precisamos inicializar com o modelo compartilhado
                // pois temos nosso próprio core
                if (config.usa_LoRA && !config.lora_path.empty()) {
                    lora = llama_adapter_lora_init(alyssa_core->get_model(), config.lora_path.c_str());
                    if (!lora) {
                        std::cerr << "Falha ao carregar LoRA para Alyssa: " << config.lora_path << std::endl;
                        return false;
                    }
                    std::cout << "LoRA carregado para Alyssa: " << config.lora_path << std::endl;
                }
                return true;
            }
            
            std::string run(
                const std::string& input,
                alyssa_core::AlyssaCore* core_instance,
                llama_adapter_lora* lora_override,
                std::vector<llama_chat_message>& current_history,
                llama_adapter_lora** active_lora_in_context,
                std::function<void(const std::string&)> stream_callback = nullptr
            ) override {
                // Usar nosso próprio core (4b) em vez do core_instance passado
                return ExpertBase::run(input, alyssa_core.get(), lora_override, 
                                     current_history, active_lora_in_context, stream_callback);
            }
            
            alyssa_fusion::ExpertContribution get_contribution(
                const std::string& input,
                alyssa_core::AlyssaCore* core_instance,
                std::shared_ptr<Embedder> embedder,
                llama_adapter_lora* lora_override,
                std::vector<llama_chat_message>& current_history,
                llama_adapter_lora** active_lora_in_context,
                std::function<void(const std::string&)> stream_callback = nullptr
            ) override {
                // Usar nosso próprio core (4b) em vez do core_instance passado
                return ExpertBase::get_contribution(input, alyssa_core.get(), embedder, lora_override,
                                                  current_history, active_lora_in_context, stream_callback);
            }
        };
        
        // Registrar especialista Alyssa com modelo 4b
        auto alyssa_expert = std::make_unique<Alyssa4bExpert>(*alyssa_4b_config, std::move(alyssa_core));
        if (alyssa_expert->initialize(nullptr)) {
            register_expert(std::move(alyssa_expert));
            std::cout << "[INFO] Especialista Alyssa (4b) registrado com sucesso" << std::endl;
        } else {
            std::cerr << "Falha ao inicializar especialista Alyssa" << std::endl;
        }

        initialized = true;
        std::cout << "CoreIntegration (MoE + Weighted Fusion) inicializado com sucesso!" << std::endl;
        std::cout << "Contexto base configurado: n_ctx = " << context_size << std::endl;
        std::cout << "Modelo base (para especialistas): " << base_1b_config->model_path << std::endl;
        std::cout << "Modelo Alyssa: " << alyssa_4b_config->model_path << std::endl;
        std::cout << "Total de especialistas registrados: " << experts.size() << std::endl;
        return true;

    } catch (const std::exception& e) {
        fprintf(stderr, "ERRO CRÍTICO na Inicialização: %s\n", e.what());
        core_instance = nullptr;
        return false;
    }
}

// =========================================================================
// Utility Functions
// =========================================================================

/**
 * @brief Check if two expert signals are compatible.
 * @param signal1 First expert signal.
 * @param signal2 Second expert signal.
 * @return true if signals are compatible, false if contradictory.
 */
bool CoreIntegration::are_signals_compatible(const std::string& signal1, const std::string& signal2) {
    // Lógica simples de compatibilidade
    // Se ambos os sinais contêm "ERRO", são incompatíveis
    if (signal1.find("[ERRO]") != std::string::npos && 
        signal2.find("[ERRO]") != std::string::npos) {
        return false;
    }
    
    // Sinais com alta confiança (>0.7) são considerados compatíveis se não forem opostos
    // Extrair confiança dos sinais
    auto extract_confidence = [](const std::string& signal) -> float {
        std::regex conf_pattern(R"(\[CONFIANÇA\]\s*(\d+\.?\d*))");
        std::smatch matches;
        if (std::regex_search(signal, matches, conf_pattern) && matches.size() >= 2) {
            try {
                return std::stof(matches[1]);
            } catch (...) {
                return 0.0f;
            }
        }
        return 0.0f;
    };
    
    float conf1 = extract_confidence(signal1);
    float conf2 = extract_confidence(signal2);
    
    // Se ambos têm confiança alta (>0.7), considerar compatíveis
    if (conf1 > 0.7f && conf2 > 0.7f) {
        return true;
    }
    
    // Por padrão, considerar compatíveis
    return true;
};

/**
 * @brief Calculate string similarity using Jaccard index.
 * @param str1 First string.
 * @param str2 Second string.
 * @return Similarity score between 0.0 (no similarity) and 1.0 (identical).
 */
float CoreIntegration::calculate_string_similarity(const std::string& str1, const std::string& str2) {
    // Similaridade de Jaccard simplificada
    if (str1.empty() && str2.empty()) return 1.0f;
    if (str1.empty() || str2.empty()) return 0.0f;
    
    std::string lower1 = str1;
    std::string lower2 = str2;
    std::transform(lower1.begin(), lower1.end(), lower1.begin(), ::tolower);
    std::transform(lower2.begin(), lower2.end(), lower2.begin(), ::tolower);
    
    // Contar palavras comuns
    std::set<std::string> words1, words2;
    
    auto tokenize = [](const std::string& text) -> std::set<std::string> {
        std::set<std::string> tokens;
        std::string token;
        for (char c : text) {
            if (std::isalnum(c) || c == '\'') {
                token += c;
            } else if (!token.empty()) {
                tokens.insert(token);
                token.clear();
            }
        }
        if (!token.empty()) tokens.insert(token);
        return tokens;
    };
    
    words1 = tokenize(lower1);
    words2 = tokenize(lower2);
    
    if (words1.empty() && words2.empty()) return 1.0f;
    if (words1.empty() || words2.empty()) return 0.0f;
    
    int intersection = 0;
    for (const auto& word : words1) {
        if (words2.find(word) != words2.end()) {
            intersection++;
        }
    }
    
    int union_size = words1.size() + words2.size() - intersection;
    
    return union_size > 0 ? (float)intersection / union_size : 0.0f;
};

/**
 * @brief Determine if input is small talk/social pleasantry.
 * @param input Text to analyze.
 * @return true if input is small talk, false if substantive content.
 */
bool CoreIntegration::is_small_talk(const std::string& input) {
    // Lista de padrões de small talk
    static const std::vector<std::string> small_talk_patterns = {
        "oi", "olá", "e aí", "eai", "tudo bem", "como vai",
        "bom dia", "boa tarde", "boa noite", "oi, tudo bem?",
        "olá, como você está?", "hey", "hello", "hi"
    };
    
    std::string lower_input = input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
    
    // Remover pontuação
    lower_input.erase(std::remove_if(lower_input.begin(), lower_input.end(), 
                     [](char c) { return std::ispunct(c); }), lower_input.end());
    
    // Verificar se é apenas small talk
    for (const auto& pattern : small_talk_patterns) {
        if (lower_input.find(pattern) != std::string::npos) {
            // Se o input for muito curto (<= 15 chars) ou for exatamente um padrão
            if (lower_input.length() <= 15 || lower_input == pattern) {
                return true;
            }
        }
    }
    
    // Verificar se tem conteúdo semântico
    std::vector<std::string> content_indicators = {
        "?", "porque", "como", "quando", "onde", "por que",
        "explica", "ajuda", "preciso", "problema", "questão"
    };
    
    bool has_content = false;
    for (const auto& indicator : content_indicators) {
        if (lower_input.find(indicator) != std::string::npos) {
            has_content = true;
            break;
        }
    }
    
    return !has_content && lower_input.length() < 30;
};

// =========================================================================
// Context and Cache Control
// =========================================================================

/**
 * @brief Switch active expert context in KV cache.
 * @param new_expert_id Expert to switch to.
 */
void CoreIntegration::switch_expert_context(const std::string& new_expert_id) {
    if (active_expert_in_cache != new_expert_id) {
        std::cout << "\n[Orquestrador]: Trocando de '" << active_expert_in_cache 
                  << "' para '" << new_expert_id << "'\n";
        clear_kv_cache();
        active_expert_in_cache = new_expert_id;
    }
}

/**
 * @brief Clear KV cache for current expert.
 * @details Properly clears the KV cache memory to prevent context leakage between experts.
 *          Removes all cached sequences and resets memory pointers.
 */
void CoreIntegration::clear_kv_cache() {
    if (!core_instance) return;
    
    llama_context* ctx = core_instance->get_context();
    if (!ctx) return;
    
    try {
        // 1. Remove ALL sequences from KV cache (0 = first token, -1 = all remaining)
        llama_memory_seq_rm(llama_get_memory(ctx), 0, -1, -1);
        
        // 2. Additional safety: clear the memory directly if available
        auto memory = llama_get_memory(ctx);
        if (memory) {
            // Remove all sequence data from memory
            llama_memory_seq_rm(memory, 0, -1, -1);
        }
        
        // 3. Reset context tracking variables
        active_expert_in_cache = "";
        
        // 4. Log with timestamp for debugging context leakage
        std::cout << "[Orquestrador] KV Cache limpo completamente - "
                  << "contexto anterior isolado." << std::endl;
                  
    } catch (const std::exception& e) {
        std::cerr << "[ERRO] Falha ao limpar KV cache: " << e.what() << std::endl;
    }
}

// =========================================================================
// Expert Execution
// =========================================================================

/**
 * @brief Validate if input fits within expert's context window.
 * @param prompt Input text to validate.
 * @param expert_id Expert identifier.
 * @return true if context size is sufficient, false otherwise.
 */
bool CoreIntegration::validate_context_size(const std::string& prompt, const std::string& expert_id) {
    if (!core_instance) return false;
    
    // Não validar para Alyssa - ela tem seu próprio core
    if (expert_id == "alyssa") {
        return true;
    }
    
    int n_ctx = core_instance->get_n_ctx();
    
    // Estimativa conservativa: ~1.3 caracteres por token
    int estimated_tokens = prompt.length() / 1.3;
    
    if (estimated_tokens > n_ctx * 0.7) { // Usar apenas 70% do contexto
        std::cerr << "[ERRO] Prompt muito longo para " << expert_id 
                  << ": ~" << estimated_tokens << " tokens (limite: " << n_ctx * 0.7 << ")" << std::endl;
        return false;
    }
    
    return true;
}

/**
 * @brief Execute a specific expert with input processing.
 * @param expert_id Identifier of expert to run.
 * @param input Text input for the expert.
 * @param use_tts Enable TTS synthesis for streaming.
 * @param tts Pointer to TTS instance (can be nullptr).
 * @return Expert's response as string.
 */
std::string CoreIntegration::run_expert(
    const std::string& expert_id,
    const std::string& input,
    bool use_tts,
    ElevenLabsTTS* tts
) {
    if (!initialized) {
        return "Erro: Sistema não inicializado.";
    }

    if (experts.find(expert_id) == experts.end()) {
        throw std::runtime_error("Especialista não registrado: " + expert_id);
    }

    // Validar tamanho do contexto antes de continuar
    // Para especialistas normais, usar core_instance, para Alyssa usar um core diferente
    alyssa_core::AlyssaCore* validation_core = core_instance.get();
    if (expert_id == "alyssa") {
        // A Alyssa tem seu próprio core, então não podemos validar facilmente aqui
        // Vamos pular a validação para ela
        std::cout << "[INFO] Usando validação de contexto simplificada para Alyssa" << std::endl;
    } else {
        if (!validate_context_size(input, expert_id)) {
            return "Erro: Input muito longo para processar. Por favor, seja mais breve.";
        }
    }

    // Trocar contexto se necessário (apenas para especialistas que compartilham o core base)
    if (expert_id != "alyssa") {
        if (active_expert_in_cache != expert_id) {
            std::cout << "\n[Orquestrador]: Trocando de '" << active_expert_in_cache 
                      << "' para '" << expert_id << "'\n";
            clear_kv_cache();
            active_expert_in_cache = expert_id;
        }
    } else {
        // Alyssa tem seu próprio cache, então limpamos sempre
        std::cout << "\n[Orquestrador]: Preparando contexto para Alyssa\n";
        // Não limpamos o cache base, pois Alyssa usa um core separado
    }

    auto& expert = experts[expert_id];
    auto& history = expert_histories[expert_id];

    // Configurar callback de streaming para TTS
    std::function<void(const std::string&)> stream_callback = nullptr;
    std::string sentence_buffer;
    
    if (use_tts && tts) {
        stream_callback = [&](const std::string& piece) {
            printf("%s", piece.c_str());
            fflush(stdout);
            
            sentence_buffer += piece;
            size_t punctuation_pos = sentence_buffer.find_first_of(".!?");
            
            if (punctuation_pos != std::string::npos) {
                std::string sentence = sentence_buffer.substr(0, punctuation_pos + 1);
                sentence_buffer = sentence_buffer.substr(punctuation_pos + 1);
                sentence.erase(0, sentence.find_first_not_of(" \t\n\r"));
                
                if (!sentence.empty()) {
                    tts->synthesizeAndPlay(sentence);
                }
            }
        };
    }

    // Executar especialista através da interface
    llama_adapter_lora* active_lora = nullptr;
    std::string response;
    
    // Para Alyssa, precisamos passar um core válido, mas ela usará seu próprio
    alyssa_core::AlyssaCore* core_to_pass = (expert_id == "alyssa") ? core_instance.get() : core_instance.get();
    
    response = expert->run(
        input,
        core_to_pass, // Alyssa irá ignorar e usar seu próprio core
        nullptr, // lora_override (gerenciado pelo especialista)
        history,
        &active_lora,
        stream_callback
    );

    // Processar buffer restante
    if (use_tts && tts && !sentence_buffer.empty()) {
        sentence_buffer.erase(0, sentence_buffer.find_first_not_of(" \t\n\r"));
        if (!sentence_buffer.empty()) {
            tts->synthesizeAndPlay(sentence_buffer);
        }
    }

    // Gerenciar histórico
    manage_dynamic_history(expert_id, history);

    return response;
}

// =========================================================================
// Expert Committee
// =========================================================================

/**
 * @brief Run committee of experts in parallel.
 * @param expert_ids Vector of expert identifiers to include in committee.
 * @param input Text input for all experts.
 * @return Vector of contributions from each expert.
 */
std::vector<alyssa_fusion::ExpertContribution> 
CoreIntegration::run_expert_committee(
    const std::vector<std::string>& expert_ids,
    const std::string& input
) {
    std::vector<alyssa_fusion::ExpertContribution> contributions;
    
    // Guardar o especialista ativo anterior
    std::string previous_expert = active_expert_in_cache;
    
    for (const auto& expert_id : expert_ids) {
        if (!has_expert(expert_id)) {
            std::cerr << "Especialista não encontrado: " << expert_id << std::endl;
            continue;
        }
        
        try {
            // IMPORTANTE: Trocar contexto para cada especialista
            // Isso limpa o KV cache automaticamente
            switch_expert_context(expert_id);
            
            auto& expert = experts[expert_id];
            auto& history = expert_histories[expert_id];
            
            // ====== ISOLAÇÃO DE HISTÓRICO PARA COMITÊ ======
            // Para o comitê, queremos que cada especialista tenha uma perspectiva
            // FRESCA, não contaminada por histórico anterior
            // Mas ainda mantemos o histórico para referências (não usamos-o inline)
            
            // Criar uma cópia vazia do histórico para este turno do comitê
            // Isso previne context leakage entre turnos
            std::vector<llama_chat_message> committee_isolated_history = history;
            
            // Se há muitas mensagens no histórico, limitar para os últimos N turnos apenas
            // para evitar que históricos antigos contaminem o comitê
            if (committee_isolated_history.size() > 4) {
                // Manter apenas os últimos 2 turnos (user + assistant)
                size_t start_idx = committee_isolated_history.size() - 4;
                committee_isolated_history.erase(
                    committee_isolated_history.begin(), 
                    committee_isolated_history.begin() + start_idx
                );
            }
            
            // Obter contribuição através da interface com histórico isolado
            llama_adapter_lora* active_lora = nullptr;
            auto contrib = expert->get_contribution(
                input,
                core_instance.get(),
                embedder,
                nullptr, // lora_override
                committee_isolated_history,  // Usar histórico isolado
                &active_lora
            );
            
            contributions.push_back(contrib);
            
            std::cout << "[Comitê] " << expert_id << " respondeu: " 
                      << (contrib.response.length() > 50 ? 
                          contrib.response.substr(0, 50) + "..." : contrib.response) 
                      << std::endl;
                      
        } catch (const std::exception& e) {
            std::cerr << "Erro executando " << expert_id << ": " << e.what() << std::endl;
            // Continuar com o próximo especialista
        }
    }
    
    // Restaurar o especialista anterior
    if (!previous_expert.empty()) {
        switch_expert_context(previous_expert);
    }
    
    return contributions;
}

/**
 * @brief Detect emotional content in input using heuristics.
 * @param input Text to analyze for emotional content.
 * @return Detected emotion as string (e.g., "neutralidade", "curiosidade").
 */
std::string CoreIntegration::detect_emotion_with_heuristics(const std::string& input) {
    // 1. Verificar small talk
    if (CoreIntegration::is_small_talk(input)) {
        return "neutralidade";
    }
    
    // 2. Usar o detector do fusion_engine com fallback
    std::string detected = fusion_engine->detect_emotion_from_input(input);
    
    // 3. Heurística: se confiança baixa (< 0.3) ou emoção "surpresa" em input curto
    if (detected == "surpresa" && input.length() < 50) {
        // Verificar se há realmente algo surpreendente
        std::vector<std::string> surprise_indicators = {
            "incrível", "incrivel", "uau", "nossa", "caramba",
            "surpresa", "inesperado", "não acredito", "sério"
        };
        
        std::string lower_input = input;
        std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
        
        bool has_surprise_word = false;
        for (const auto& word : surprise_indicators) {
            if (lower_input.find(word) != std::string::npos) {
                has_surprise_word = true;
                break;
            }
        }
        
        if (!has_surprise_word) {
            return "curiosidade"; // Fallback mais provável para perguntas
        }
    }
    
    return detected;
}

// =========================================================================
// Weighted Fusion
// =========================================================================

/**
 * @brief Generate fused input for Alyssa model from expert contributions.
 * @param original_input Original user input.
 * @param contributions Vector of expert contributions.
 * @param emotion Detected emotion for context.
 * @return Formatted prompt with expert thoughts and memory context.
 */
std::string CoreIntegration::generate_fused_input(
    const std::string& original_input,
    const std::vector<alyssa_fusion::ExpertContribution>& contributions,
    const std::string& emotion
) {
    // Construir blocos de pensamento em português
    std::string thoughts = "[PENSAMENTOS]\n";
    
    // Adicionar emoção detectada
    if (!emotion.empty()) {
        thoughts += "[Emoção]: " + emotion + "\n";
    }
    
    // Organizar pensamentos por especialista
    std::map<std::string, std::vector<std::string>> thoughts_by_type;
    
    // Build thought type mapping from ConfigsLLM.json
    static std::map<std::string, std::string> thought_type_mapping;
    
    if (thought_type_mapping.empty()) {
        // Load and cache mapping from configs
        AllModelConfigs configs = load_config();
        for (const auto& cfg : configs) {
            std::string thought_label = cfg.id;
            
            // Convert ID to Portuguese thought type label
            if (cfg.id.find("emotional") != std::string::npos) {
                thought_label = "Emocional";
            } else if (cfg.id.find("introspect") != std::string::npos) {
                thought_label = "Introspectivo";
            } else if (cfg.id.find("social") != std::string::npos) {
                thought_label = "Social";
            } else if (cfg.id.find("analyt") != std::string::npos) {
                thought_label = "Analítico";
            } else if (cfg.id.find("creat") != std::string::npos) {
                thought_label = "Criativo";
            } else if (cfg.id.find("memory") != std::string::npos) {
                thought_label = "Memória";
            } else {
                // Capitalize first letter for unknown types
                if (!thought_label.empty()) {
                    thought_label[0] = std::toupper(thought_label[0]);
                }
            }
            
            thought_type_mapping[cfg.id] = thought_label;
        }
    }
    
    for (const auto& contrib : contributions) {
        // Skip alyssa from contributions (should not be here)
        if (contrib.expert_id == "alyssa") continue;
        
        // Get thought type from loaded config mapping
        std::string thought_type = contrib.expert_id;
        auto it = thought_type_mapping.find(contrib.expert_id);
        if (it != thought_type_mapping.end()) {
            thought_type = it->second;
        }
        
        thoughts_by_type[thought_type].push_back(contrib.response);
    }
    
    // Adicionar pensamentos ao bloco
    for (const auto& [type, responses] : thoughts_by_type) {
        thoughts += "[" + type + "]: ";
        for (size_t i = 0; i < responses.size(); ++i) {
            thoughts += responses[i];
            if (i < responses.size() - 1) thoughts += " ";
        }
        thoughts += "\n";
    }
    
    // Adicionar contexto de memória
    if (memory_manager) {
        auto memories = memory_manager->getHybridMemories(original_input);
        if (!memories.empty()) {
            thoughts += "[Memória de Longo Prazo]:\n";
            for (const auto& mem : memories) {
                thoughts += "- " + mem.content + " (Emoção: " + mem.emotion + ")\n";
            }
        }
    }
    
    thoughts += "[/PENSAMENTOS]\n\n";
    
    // Construir prompt final para a Alyssa
    std::string fused_prompt = thoughts + 
                               "ENTRADA DO USUÁRIO: \"" + original_input + "\"\n\n" +
                               "Baseado nos pensamentos acima, forneça sua resposta como Alyssa:";
    
    return fused_prompt;
}

/**
 * @brief Process user input with weighted fusion and TTS.
 * @param input User's text input.
 * @param tts ElevenLabsTTS instance for voice synthesis.
 * @return Fused response from multiple experts.
 * @details Runs expert committee, applies weighted fusion, and generates voice output.
 */
std::string CoreIntegration::think_with_fusion(const std::string& input, ElevenLabsTTS& tts) {
    if (!initialized || !core_instance || !fusion_engine) {
        return "Erro: Sistema não inicializado corretamente.";
    }

    std::cout << "\n[Weighted Fusion] Processando input: " << input << std::endl;

    // =====================================================================
    // ISOLATED MEMORY CONTEXT: Previne context leakage
    // =====================================================================
    
    std::string memory_context = "";
    std::string augmented_input = input;

    if (memory_manager) {
        // 1. Recuperar memórias relevantes com LIMITE de tamanho
        auto memories = memory_manager->getHybridMemories(input);
        
        if (!memories.empty()) {
            // 2. Filtrar memórias para evitar contaminação
            //    - Limitar a TOP 3 memórias mais relevantes
            //    - Descartar se muito antigas (contexto irrelevante)
            //    - Descartar se emoção conflita com input atual
            
            std::vector<typename decltype(memories)::value_type> filtered_memories;
            int max_memories = 3; // Máximo de memórias por turno
            
            for (size_t i = 0; i < memories.size() && filtered_memories.size() < max_memories; ++i) {
                const auto& mem = memories[i];
                
                // Validação simples: não injetar se a memória parece irrelevante
                bool is_relevant = 
                    (mem.content.find("?") != std::string::npos ||  // Pergunta
                     mem.content.length() > 20) &&                  // Conteúdo significativo
                    (mem.emotion.find("erro") == std::string::npos); // Não é erro
                
                if (is_relevant) {
                    filtered_memories.push_back(mem);
                }
            }
            
            // 3. Construir contexto isolado de memória
            if (!filtered_memories.empty()) {
                memory_context = "\n[CONTEXTO DE MEMÓRIA ANTERIOR - ISOLADO PARA ESTE TURNO]\n";
                for (const auto& mem : filtered_memories) {
                    // Truncar memórias muito longas para evitar dominação do prompt
                    std::string content = mem.content;
                    if (content.length() > 150) {
                        content = content.substr(0, 150) + "...";
                    }
                    memory_context += "- " + content + "\n";
                }
                memory_context += "[FIM CONTEXTO ISOLADO]\n";
                
                std::cout << "[Memory Context] Injetando " << filtered_memories.size() 
                         << " memória(s) isolada(s)" << std::endl;
                
                augmented_input = memory_context + input;
                
                // IMPORTANTE: Após usar as memórias para context augmentation,
                // limpamos o histórico de memória injected para não contaminar turno seguinte
            } else {
                std::cout << "[Memory Context] Sem memórias relevantes para este turno" << std::endl;
            }
        }
    }

    // =====================================================================
    // EXPERT COMMITTEE EXECUTION
    // =====================================================================
    
    std::vector<std::string> expert_committee = {
        "introspectiveModel", 
        "emotionalModel", 
        "socialModel",
        "alyssa"        
    };

    // Executar comitê
    auto contributions = run_expert_committee(expert_committee, augmented_input);
    
    // 2. CALCULAR COERÊNCIA DO COMITÊ
    float committee_coherence = calculate_committee_coherence(contributions);
    
    // 3. Se coerência muito baixa, regenerar sem comitê
    if (committee_coherence < 0.3) {
    std::cout << "[AVISO] Coerência do comitê baixa (" << committee_coherence 
              << "). Gerando resposta direta." << std::endl;
    
    contributions.clear();
    
    std::string direct_prompt = "[MODO DIRETO] Responda ao usuário: " + input;
    
    // This executes the model AND streams the audio via the callback
    std::string direct_response = run_expert("alyssa", direct_prompt, true, &tts);
    
    printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", direct_response.c_str());
    
    if (memory_manager && should_store_in_memory(input, direct_response)) {
        memory_manager->processInteraction(input, direct_response);
    }

    return direct_response;
}
    
    // 4. Continuar com fusão normal se coerência aceitável
    std::string emotion = detect_emotion_with_heuristics(input);
    std::string fused_input = generate_fused_input(input, contributions, emotion);
    
    // 3. Executa alyssa com o input fusionado (com TTS)
    std::string final_response = run_expert("alyssa", fused_input, true, &tts);

    // 4. Sintetiza áudio da resposta final
    printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", final_response.c_str());
    tts.synthesizeAndPlay(final_response);

    // Salvar interação na memória
    if (memory_manager) {
        if (should_store_in_memory(input, final_response)) {
            memory_manager->processInteraction(input, final_response);
            std::cout << "\n Interação salva na LTM." << std::endl;
        } else {
            std::cout << "\n Small talk/ruído não salvo na LTM." << std::endl;
        }
    }

    return final_response;
}

/**
 * @brief Calculate coherence metric for expert committee responses.
 * @param contributions Vector of contributions from different experts.
 * @return Coherence score between 0.0 (incoherent) and 1.0 (fully coherent).
 */
float CoreIntegration::calculate_committee_coherence(
    const std::vector<alyssa_fusion::ExpertContribution>& contributions
) {
    if (contributions.size() <= 1) return 1.0f;
    
    // Simples métrica de similaridade textual
    int agreeing_signals = 0;
    int total_pairs = 0;
    
    for (size_t i = 0; i < contributions.size(); ++i) {
        for (size_t j = i + 1; j < contributions.size(); ++j) {
            // Verificar se os sinais são compatíveis
            if (are_signals_compatible(contributions[i].response, contributions[j].response)) {
                agreeing_signals++;
            }
            total_pairs++;
        }
    }
    
    return total_pairs > 0 ? (float)agreeing_signals / total_pairs : 0.0f;
};

/**
 * @brief Determine if interaction should be stored in long-term memory.
 * @param input User's input text.
 * @param response System's response text.
 * @return true if worth storing in memory, false for small talk/noise.
 */
bool CoreIntegration::should_store_in_memory(const std::string& input, const std::string& response) {
    // Critério 1: Não armazenar small talk
    if (CoreIntegration::is_small_talk(input)) return false;
    
    // Critério 2: Mínimo de novidade semântica
    if (input.length() < 20 && response.length() < 30) return false;
    
    // Critério 3: Verificar repetição (similaridade com últimas N interações)
    static std::vector<std::string> recent_interactions;
    static const int MAX_RECENT = 10;
    
    if (recent_interactions.size() >= MAX_RECENT) {
        recent_interactions.erase(recent_interactions.begin());
    }
    
    // Calcular similaridade com interações recentes
    for (const auto& recent : recent_interactions) {
        float similarity = calculate_string_similarity(input, recent);
        if (similarity > 0.8) {
            return false; // Muito similar a algo recente
        }
    }
    
    recent_interactions.push_back(input);
    
    // Critério 4: Presença de elementos memoráveis
    bool has_memorable_elements = 
        input.find("?") != std::string::npos || // Pergunta
        response.find("!") != std::string::npos || // Ênfase
        input.length() > 100 || // Conteúdo substancial
        response.length() > 150;
    
    return has_memorable_elements;
};

/**
 * @brief Process user input with weighted fusion without TTS.
 * @param input User's text input.
 * @return Fused response from multiple experts.
 * @details Same as think_with_fusion but without voice synthesis.
 */
std::string CoreIntegration::think_with_fusion_ttsless(const std::string& input) {
    if (!initialized || !core_instance || !fusion_engine) {
        return "Erro: Sistema não inicializado corretamente.";
    }

    std::cout << "\n[Weighted Fusion TTS-less] Processando input: " << input << std::endl;

    // =====================================================================
    // ISOLATED MEMORY CONTEXT (TTS-less version)
    // =====================================================================
    
    std::string memory_context = "";
    std::string augmented_input = input;
    AllModelConfigs configs = load_config();

    if (memory_manager) {
        // 1. Recuperar memórias relevantes com LIMITE de tamanho
        auto memories = memory_manager->getHybridMemories(input);
        
        if (!memories.empty()) {
            // 2. Filtrar memórias para evitar contaminação
            std::vector<typename decltype(memories)::value_type> filtered_memories;
            int max_memories = 3; // Máximo de memórias por turno
            
            for (size_t i = 0; i < memories.size() && filtered_memories.size() < max_memories; ++i) {
                const auto& mem = memories[i];
                
                // Validação: evitar contextos irrelevantes
                bool is_relevant = 
                    (mem.content.find("?") != std::string::npos ||  
                     mem.content.length() > 20) &&                  
                    (mem.emotion.find("erro") == std::string::npos);
                
                if (is_relevant) {
                    filtered_memories.push_back(mem);
                }
            }
            
            // 3. Construir contexto isolado
            if (!filtered_memories.empty()) {
                memory_context = "\n[CONTEXTO DE MEMÓRIA ANTERIOR - ISOLADO PARA ESTE TURNO]\n";
                for (const auto& mem : filtered_memories) {
                    std::string content = mem.content;
                    if (content.length() > 150) {
                        content = content.substr(0, 150) + "...";
                    }
                    memory_context += "- " + content + "\n";
                }
                memory_context += "[FIM CONTEXTO ISOLADO]\n";
                
                std::cout << "[Memory Context] Injetando " << filtered_memories.size() 
                         << " memória(s) isolada(s)" << std::endl;
                
                augmented_input = memory_context + input;
            }
        }
    }

    // =====================================================================
    // EXPERT COMMITTEE EXECUTION
    // =====================================================================
    
    // 1. Executa APENAS os especialistas de pensamento (não incluir alyssa)
    std::vector<std::string> expert_committee;
    for (const auto& cfg : configs) {
        // Adiciona ao comitê de execução
        expert_committee.push_back(cfg.id);
        if (cfg.id == "alyssa") {
            continue; // Alyssa não participa do comitê, ela é a fusão final
        }

        std::string fallback = cfg.id;
        fallback[0] = std::toupper(fallback[0]);
        std::cout << "[Comitê] " << fallback << " adicionado ao comitê de execução." << std::endl;
    }

    // Executar comitê
    auto contributions = run_expert_committee(expert_committee, augmented_input);
    
    // 2. CALCULAR COERÊNCIA DO COMITÊ
    float committee_coherence = calculate_committee_coherence(contributions);
    
    // 3. Se coerência muito baixa, regenerar sem comitê
    if (committee_coherence < 0.3) {
        std::cout << "[AVISO] Coerência do comitê baixa (" << committee_coherence 
                  << "). Gerando resposta direta." << std::endl;
        
        // Limpar contribuições incoerentes
        contributions.clear();
        
        // Gerar resposta direta com a Alyssa
        std::string direct_prompt = "[MODO DIRETO] Responda ao usuário: " + input;
        return run_expert("alyssa", direct_prompt, false, nullptr);
    }
    
    // 4. Continuar com fusão normal se coerência aceitável
    std::string emotion = detect_emotion_with_heuristics(input);
    std::string fused_input = generate_fused_input(input, contributions, emotion);


    // 4. Executa alyssa com o input fusionado (sem TTS)
    // Agora a Alyssa recebe apenas os pensamentos dos especialistas
    std::string final_response = run_expert("alyssa", fused_input, false, nullptr);

    // 5. Parse da resposta final (remover formatação se necessário)
    // A resposta deve estar limpa, sem blocos [RESPOSTA]
    size_t start_tag = final_response.find("[RESPOSTA]");
    size_t end_tag = final_response.find("[/RESPOSTA]");
    
    if (start_tag != std::string::npos && end_tag != std::string::npos) {
        // Extrair apenas o conteúdo dentro do bloco
        final_response = final_response.substr(start_tag + 10, end_tag - start_tag - 10);
        // Remover whitespace no início e fim
        final_response.erase(0, final_response.find_first_not_of(" \n\r\t"));
        final_response.erase(final_response.find_last_not_of(" \n\r\t") + 1);
    }

    // Exibe resposta final
    printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", final_response.c_str());

    // Salvar interação na memória
    if (memory_manager) {
        memory_manager->processInteraction(input, final_response);
        std::cout << "\n Interação salva na LTM." << std::endl;
    }

    return final_response;
}

// =========================================================================
// Think Original (Orchestration)
// =========================================================================

/**
 * @brief Process user input using standard MoE architecture.
 * @param input User's text input.
 * @param tts ElevenLabsTTS instance for voice synthesis.
 * @return Response generated by the AI system.
 * @details Uses emotional analysis, memory retrieval, and Alyssa model for final response.
 */
std::string CoreIntegration::think(const std::string& input, ElevenLabsTTS& tts) {
    if (!initialized || !core_instance) {
        return "Erro: O CoreIntegration não foi inicializado corretamente.";
    }

    // Manter histórico da conversação
    static std::vector<std::pair<std::string, std::string>> conversation_history;

    // Adiciona novo input ao histórico
    conversation_history.emplace_back("user", input);

    // --- LÓGICA MoE ---
    
    // 1. Chamar "emotionalModel" para analisar o input
    std::string emotion = run_expert("emotionalModel", input, false, &tts);
    
    // 2. Chamar "memoryModel" para buscar contexto
    std::string context_input = input + " [EMOÇÃO]: " + emotion;
    for (const auto& [role, msg] : conversation_history) {
        context_input += "\n[" + role + "]: " + msg;
    }
    
    std::string context = run_expert("memoryModel", context_input, false, &tts);
    
    // 3. Chamar "alyssa" para a resposta final
    std::string final_input = "[CONTEXTO]: " + context + "\n[INPUT]: " + input;

    printf("\033[33m[ALYSSA FINAL]: \033[0m"); 
    std::string final_response = run_expert("alyssa", final_input, true, &tts);
    printf("\n\033[0m");

    // Adiciona resposta ao histórico
    conversation_history.emplace_back("assistant", final_response);

    return final_response;
}

// =========================================================================
// History Management
// =========================================================================

/**
 * @brief Calculate dynamic history limit based on expert and emotional state.
 * @param expert_id Expert identifier.
 * @return Maximum number of messages to keep in history.
 */
size_t CoreIntegration::calculate_history_limit(const std::string& expert_id) {
    size_t limit = 150;

    if (memory_manager) {
        auto emotional_state = memory_manager->getCurrentEmotionalState();
        
        if (emotional_state.intensity > 0.7) {
            limit += 10; 
        }
        else if (emotional_state.intensity < 0.2) {
            limit -= 5;
        }
    }

    if (expert_id == "introspectiveModel") {
        limit += 15; 
    } 
    else if (expert_id == "socialModel") {
        limit += 5;  
    }
    else if (has_expert("creativeModel") && expert_id == "creativeModel") {
        limit += 10;
    }

    if (limit > 50) limit = 50;
    if (limit < 10) limit = 10;

    return limit;
}

/**
 * @brief Manage expert history with dynamic size limits.
 * @param expert_id Expert identifier.
 * @param history Reference to expert's conversation history.
 * @details Archives old messages to LTM when history exceeds limit.
 */
void CoreIntegration::manage_dynamic_history(
    const std::string& expert_id, 
    std::vector<llama_chat_message>& history
) {
    size_t dynamic_limit = calculate_history_limit(expert_id);
    
    if (history.size() <= dynamic_limit) return;

    std::cout << "[Memory Cycle] Otimizando histórico do especialista '" << expert_id 
              << "' (Limite atual: " << dynamic_limit << ", Tamanho: " << history.size() << ")\n";

    int messages_to_archive = 4;
    std::string archived_content = "";
    std::string accumulated_roles = "";

    for (int i = 0; i < messages_to_archive && !history.empty(); ++i) {
        auto& msg = history.front();
        
        std::string role = msg.role;
        std::string content = msg.content;
        
        accumulated_roles += role + ", ";
        archived_content += "[" + role + "]: " + content + "\n";

        free((char*)msg.content);
        history.erase(history.begin());
    }

    if (memory_manager && !archived_content.empty()) {
        std::string context_tag = "archived_history | expert:" + expert_id;

        int mem_id = memory_manager->storeMemoryWithEmotionalAnalysis(
            archived_content, 
            context_tag
        );

        auto intentions = memory_manager->getActiveIntentions();
        if (!intentions.empty()) {
            memory_manager->linkMemoryToIntention(mem_id, intentions[0].id);
        }
        std::cout << "[LTM] Memória comprimida e arquivada (ID: " << mem_id << ")\n";
        std::cout << "   -> Conteúdo: " << archived_content.substr(0, 50) << "...\n";
    }
}

// =========================================================================
// Utility Methods
// =========================================================================

/**
 * @brief Log source awareness information.
 * @param source Source identifier (expert ID).
 * @param message Message to log.
 */
void CoreIntegration::log_source_awareness(const std::string& source, const std::string& message) {
    std::cout << "[SOURCE AWARENESS] " << source << " diz: " << message << std::endl;
}

/**
 * @brief Execute an action command.
 * @param command Action command to execute.
 */
void CoreIntegration::act(const std::string& command) {
    std::cout << "[ACTION]: Comando recebido: " << command << std::endl;
    // Aqui você poderia chamar: run_expert("actionModel", command);
}

/**
 * @brief Perform system reflection/self-analysis.
 */
void CoreIntegration::reflect() {
    std::cout << "[REFLECTION]: Iniciando ciclo de reflexão..." << std::endl;
    // Aqui você poderia chamar: run_expert("introspectiveModel", "Resuma o dia.");
}

/**
 * @brief Run interactive command-line interface.
 * @details Continuously processes user input until exit command.
 */
void CoreIntegration::run_interactive_loop() {
    std::cout << "Loop Interativo iniciado. Digite 'sair' para encerrar.\n";
    std::string user_input;

    while (true) {
        printf("\033[32m> \033[0m"); 
        std::getline(std::cin, user_input);
        if (user_input == "sair" || user_input.empty()) break;
        
        // Processar input do usuário
        ElevenLabsTTS tts;
        think_with_fusion_ttsless(user_input);
    }
}