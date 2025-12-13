// AlyssaNet.cpp - Versão Refatorada
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

using namespace alyssa_core;
logging::Logger logg;

// =========================================================================
// Construtor & Destrutor
// =========================================================================

CoreIntegration::CoreIntegration() 
    : initialized(false), active_expert_in_cache("") 
{
    logg.info("CoreIntegration constructed");
}

CoreIntegration::~CoreIntegration() {
    // Libera históricos dos especialistas
    for (auto& pair : expert_histories) {
        for (auto& msg : pair.second) {
            free((char*)msg.content);
        }
    }
    logg.info("CoreIntegration destroyed");
}

// =========================================================================
// Gerenciamento de Especialistas
// =========================================================================

void CoreIntegration::register_expert(std::unique_ptr<alyssa_experts::IExpert> expert) {
    if (!expert) return;
    
    std::string id = expert->get_id();
    experts[id] = std::move(expert);
    expert_histories[id] = {};
    
    std::cout << "[Orquestrador] Especialista registrado: " << id << std::endl;
}

void CoreIntegration::remove_expert(const std::string& expert_id) {
    if (experts.erase(expert_id)) {
        expert_histories.erase(expert_id);
        std::cout << "[Orquestrador] Especialista removido: " << expert_id << std::endl;
    }
}

bool CoreIntegration::has_expert(const std::string& expert_id) const {
    return experts.find(expert_id) != experts.end();
}

void CoreIntegration::set_user_name(const std::string& name) {
    user_name = name;
    std::cout << "[IDENTITY] Nome do usuário definido como: " << name << std::endl;
    if (memory_manager) {
        memory_manager->processIdentityFact(name, "user_name");
    }
}

// =========================================================================
// Inicialização do Sistema
// =========================================================================

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
        
        // 2. Buscar configuração do modelo base (alyssa) - usar modelo diferente
        const SimpleModelConfig* base_config = nullptr;
        const SimpleModelConfig* alyssa_config = nullptr;
        
        for (const auto& cfg : configs) {
            if (cfg.id == "alyssa") {
                alyssa_config = &cfg;
            }
            // Procurar um modelo base que não seja um dos especialistas
            if (cfg.id != "emotionalModel" && 
                cfg.id != "introspectiveModel" && 
                cfg.id != "socialModel" &&
                cfg.id != "analyticalModel" &&
                cfg.id != "creativeModel" &&
                cfg.id != "memoryModel") {
                base_config = &cfg;
            }
        }
        
        // Fallback: usar alyssa como base se não encontrar outro
        if (!base_config && alyssa_config) {
            base_config = alyssa_config;
            std::cout << "[INFO] Usando modelo Alyssa como base" << std::endl;
        }
        
        if (!base_config) {
            throw std::runtime_error("Nenhuma configuração de modelo base encontrada");
        }
        
        // 3. Inicializar embedder
        embedder = std::make_shared<Embedder>("config/embedder_config.json");
        if (!embedder->initialize()) {
            std::cerr << "Falha ao inicializar o Embedder" << std::endl;
            return false;
        }
        
        // 4. Criar instância do core com n_ctx do config (usar pelo menos 4096)
        int context_size = base_config->n_ctx;
        if (context_size < 4096) {
            context_size = 4096; // Mínimo seguro para memória
            std::cout << "[WARN] Contexto muito pequeno, ajustando para " << context_size << std::endl;
        }
        
        std::cout << "[INFO] Criando AlyssaCore com modelo: " << base_config->model_path 
                  << " (n_ctx = " << context_size << ")" << std::endl;
        core_instance = std::make_unique<alyssa_core::AlyssaCore>(base_config->model_path, context_size);
        
        // 5. Inicializar fusion engine
        fusion_engine = std::make_unique<alyssa_fusion::WeightedFusion>(*embedder);
        
        // 6. Inicializar memory manager
        memory_manager = std::make_unique<alyssa_memory::AlyssaMemoryManager>(
            "../alyssa_advanced_memory.db", embedder);
        std::cout << "Sistema de Memória de Longo Prazo (LTM) inicializado." << std::endl;
        
        // 7. Criar e registrar especialistas - IGNORAR modelo base dos especialistas
        llama_model* shared_model = core_instance->get_model();
        
        for (const auto& cfg : configs) {
            // Pular o modelo base (já carregado)
            if (cfg.id == base_config->id) {
                std::cout << "[INFO] Modelo base '" << cfg.id << "' já carregado, pulando especialista" << std::endl;
                continue;
            }
            
            std::cout << "Configurando especialista: " << cfg.id 
                      << " (usando modelo compartilhado, max_tokens=" << cfg.params.max_tokens << ")" << std::endl;
            
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

        // 8. Verificar se alyssa foi registrada como especialista
        if (!has_expert("alyssa") && alyssa_config) {
            std::cout << "[INFO] Registrando Alyssa como especialista com modelo: " 
                      << alyssa_config->model_path << std::endl;
            
            auto alyssa_expert = std::make_unique<alyssa_experts::ExpertBase>(*alyssa_config);
            if (alyssa_expert->initialize(shared_model)) {
                register_expert(std::move(alyssa_expert));
            }
        }

        initialized = true;
        std::cout << "CoreIntegration (MoE + Weighted Fusion) inicializado com sucesso!" << std::endl;
        std::cout << "Contexto base configurado: n_ctx = " << context_size << std::endl;
        std::cout << "Modelo base: " << base_config->model_path << std::endl;
        std::cout << "Total de especialistas registrados: " << experts.size() << std::endl;
        return true;

    } catch (const std::exception& e) {
        fprintf(stderr, "ERRO CRÍTICO na Inicialização: %s\n", e.what());
        core_instance = nullptr;
        return false;
    }
}

// =========================================================================
// Controle de Contexto e Cache
// =========================================================================

void CoreIntegration::switch_expert_context(const std::string& new_expert_id) {
    if (active_expert_in_cache != new_expert_id) {
        std::cout << "\n[Orquestrador]: Trocando de '" << active_expert_in_cache 
                  << "' para '" << new_expert_id << "'\n";
        clear_kv_cache();
        active_expert_in_cache = new_expert_id;
    }
}

void CoreIntegration::clear_kv_cache() {
    if (core_instance) {
        llama_memory_seq_rm(llama_get_memory(core_instance->get_context()), 0, -1, -1);
        active_expert_in_cache = "";
        std::cout << "[Orquestrador]: KV Cache limpo." << std::endl;
    }
}

// =========================================================================
// Execução de Especialistas
// =========================================================================

bool CoreIntegration::validate_context_size(const std::string& prompt, const std::string& expert_id) {
    if (!core_instance) return false;
    
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

std::string CoreIntegration::run_expert(
    const std::string& expert_id,
    const std::string& input,
    bool use_tts,
    ElevenLabsTTS* tts
) {
    if (!initialized || !core_instance) {
        return "Erro: Sistema não inicializado.";
    }

    if (experts.find(expert_id) == experts.end()) {
        throw std::runtime_error("Especialista não registrado: " + expert_id);
    }

    // Validar tamanho do contexto antes de continuar
    if (!validate_context_size(input, expert_id)) {
        return "Erro: Input muito longo para processar. Por favor, seja mais breve.";
    }

    // Trocar contexto se necessário
    if (active_expert_in_cache != expert_id) {
        std::cout << "\n[Orquestrador]: Trocando de '" << active_expert_in_cache 
                  << "' para '" << expert_id << "'\n";
        clear_kv_cache();
        active_expert_in_cache = expert_id;
    }

    switch_expert_context(expert_id);

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
    std::string response = expert->run(
        input,
        core_instance.get(),
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
// Comitê de Especialistas
// =========================================================================

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
            switch_expert_context(expert_id);
            
            auto& expert = experts[expert_id];
            auto& history = expert_histories[expert_id];
            
            // Obter contribuição através da interface
            llama_adapter_lora* active_lora = nullptr;
            auto contrib = expert->get_contribution(
                input,
                core_instance.get(),
                embedder,
                nullptr, // lora_override
                history,
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

// =========================================================================
// Weighted Fusion
// =========================================================================

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
    
    for (const auto& contrib : contributions) {
        // Ignorar alyssa das contribuições (ela não deve estar aqui)
        if (contrib.expert_id == "alyssa") continue;
        
        // Mapear tipos de pensamento
        std::string thought_type;
        if (contrib.expert_id == "emotionalModel") thought_type = "Emocional";
        else if (contrib.expert_id == "introspectiveModel") thought_type = "Introspectivo";
        else if (contrib.expert_id == "socialModel") thought_type = "Social";
        else if (contrib.expert_id == "analyticalModel") thought_type = "Analítico";
        else if (contrib.expert_id == "creativeModel") thought_type = "Criativo";
        else if (contrib.expert_id == "memoryModel") thought_type = "Memória";
        else thought_type = contrib.expert_id;
        
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

std::string CoreIntegration::think_with_fusion(const std::string& input, ElevenLabsTTS& tts) {
    if (!initialized || !core_instance || !fusion_engine) {
        return "Erro: Sistema não inicializado corretamente.";
    }

    std::cout << "\n[Weighted Fusion] Processando input: " << input << std::endl;

    // Recuperar memórias e aumentar o input
    std::string memory_context = "";
    std::string augmented_input = input;

    if (memory_manager) {
        auto memories = memory_manager->getHybridMemories(input);
        if (!memories.empty()) {
            memory_context = "\n[CONTEXTO RELEVANTE DE LTM]\n";
            for (const auto& mem : memories) {
                memory_context += "- " + mem.content + " (Emoção: " + mem.emotion + ")\n";
            }
            memory_context += "[FIM CONTEXTO LTM]\n";
            std::cout << "🧠 " << memory_context;
            augmented_input = memory_context + input;
        }
    }

    // 1. Executa comitê de especialistas
    std::vector<std::string> expert_committee = {
        "introspectiveModel", 
        "emotionalModel", 
        "socialModel",
        "alyssa"        
    };

    auto contributions = run_expert_committee(expert_committee, augmented_input);

    // 2. Aplica Weighted Fusion
    std::string emotion = fusion_engine->detect_emotion_from_input(input);
    std::string fused_input = generate_fused_input(input, contributions, emotion);
    
    // 3. Executa alyssa com o input fusionado (com TTS)
    std::string final_response = run_expert("alyssa", fused_input, true, &tts);

    // 4. Sintetiza áudio da resposta final
    printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", final_response.c_str());
    tts.synthesizeAndPlay(final_response);

    // Salvar interação na memória
    if (memory_manager) {
        memory_manager->processInteraction(input, final_response);
        std::cout << "\n Interação salva na LTM." << std::endl;
    }

    return final_response;
}

std::string CoreIntegration::think_with_fusion_ttsless(const std::string& input) {
    if (!initialized || !core_instance || !fusion_engine) {
        return "Erro: Sistema não inicializado corretamente.";
    }

    std::cout << "\n[Weighted Fusion TTS-less] Processando input: " << input << std::endl;

    std::string memory_context = "";
    std::string augmented_input = input;

    if (memory_manager) {
        auto memories = memory_manager->getHybridMemories(input);
        if (!memories.empty()) {
            memory_context = "\n[CONTEXTO RELEVANTE DE LTM]\n";
            for (const auto& mem : memories) {
                memory_context += "- " + mem.content + " (Emoção: " + mem.emotion + ")\n";
            }
            memory_context += "[FIM CONTEXTO LTM]\n";
            std::cout << "🧠 " << memory_context;
            augmented_input = memory_context + input;
        }
    }

    // 1. Executa APENAS os especialistas de pensamento (não incluir alyssa)
    std::vector<std::string> expert_committee = {
        "introspectiveModel", 
        "emotionalModel", 
        "socialModel",
        "analyticalModel",
        "creativeModel",
        "memoryModel"
    };

    auto contributions = run_expert_committee(expert_committee, augmented_input);

    // 2. Detecta emoção
    std::string emotion = fusion_engine->detect_emotion_from_input(input);

    // 3. Gera input fusionado para alyssa
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
// Think Original (Orquestração)
// =========================================================================

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
// Gerenciamento de Histórico
// =========================================================================

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
// Métodos Auxiliares
// =========================================================================

void CoreIntegration::log_source_awareness(const std::string& source, const std::string& message) {
    std::cout << "[SOURCE AWARENESS] " << source << " diz: " << message << std::endl;
}

void CoreIntegration::act(const std::string& command) {
    std::cout << "[ACTION]: Comando recebido: " << command << std::endl;
    // Aqui você poderia chamar: run_expert("actionModel", command);
}

void CoreIntegration::reflect() {
    std::cout << "[REFLECTION]: Iniciando ciclo de reflexão..." << std::endl;
    // Aqui você poderia chamar: run_expert("introspectiveModel", "Resuma o dia.");
}

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