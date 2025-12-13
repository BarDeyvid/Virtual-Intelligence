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
        // 1. Inicializar embedder
        embedder = std::make_shared<Embedder>("config/embedder_config.json");
        if (!embedder->initialize()) {
            std::cerr << "Falha ao inicializar o Embedder" << std::endl;
            return false;
        }

        // 2. Criar instância do core
        core_instance = std::make_unique<alyssa_core::AlyssaCore>(base_model_path);
        
        // 3. Inicializar fusion engine
        fusion_engine = std::make_unique<alyssa_fusion::WeightedFusion>(*embedder);
        
        // 4. Inicializar memory manager
        memory_manager = std::make_unique<alyssa_memory::AlyssaMemoryManager>(
            "../alyssa_advanced_memory.db", embedder);
        std::cout << "Sistema de Memória de Longo Prazo (LTM) inicializado." << std::endl;
        
        // 5. Carregar configurações e criar especialistas base
        AllModelConfigs configs = load_config();
        if (configs.empty()) {
            throw std::runtime_error("Falha ao carregar ConfigsLLM.json");
        }

        // 6. Criar e registrar especialistas
        llama_model* shared_model = core_instance->get_model();
        
        for (const auto& cfg : configs) {
            std::cout << "Configurando especialista: " << cfg.id << std::endl;
            
            auto expert = std::make_unique<alyssa_experts::ExpertBase>(cfg);
            if (expert->initialize(shared_model)) {
                register_expert(std::move(expert));
            } else {
                std::cerr << "Falha ao inicializar especialista: " << cfg.id << std::endl;
            }
        }

        initialized = true;
        std::cout << "CoreIntegration (MoE + Weighted Fusion) inicializado com sucesso!" << std::endl;
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
    std::string fused_prompt = original_input;

    // Adiciona contexto emocional
    if (!emotion.empty()) {
        fused_prompt += "\n[EMOÇÃO]: " + emotion;
    }

    // Adiciona contexto introspectivo
    for (const auto& contrib : contributions) {
        if (contrib.expert_id == "introspectiveModel") {
            fused_prompt += "\n[INTROSPECÇÃO] (de " + contrib.source + "): " + contrib.response;
        }
    }

    // Adiciona contexto emocional
    for (const auto& contrib : contributions) {
        if (contrib.expert_id == "emotionalModel") {
            fused_prompt += "\n[EMOÇÃO DETECTADA] (de " + contrib.source + "): " + contrib.response;
        }
    }

    // Adiciona contexto social
    for (const auto& contrib : contributions) {
        if (contrib.expert_id == "socialModel") {
            fused_prompt += "\n[SOBRE SOCIEDADE] (de " + contrib.source + "): " + contrib.response;
        }
    }

    // Adiciona contexto de memória
    if (memory_manager) {
        auto memories = memory_manager->getHybridMemories(original_input);
        if (!memories.empty()) {
            fused_prompt += "\n[CONTEXTO RELEVANTE DE LTM]\n";
            for (const auto& mem : memories) {
                fused_prompt += "- " + mem.content + " (Emoção: " + mem.emotion + ")\n";
            }
            fused_prompt += "[FIM CONTEXTO LTM]\n";
        }
    }

    if (!user_name.empty()) {
        fused_prompt = "Você é " + user_name + ".\n\n" + fused_prompt;
    }

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

    // 1. Executa comitê de especialistas
    std::vector<std::string> expert_committee = {
        "introspectiveModel", 
        "emotionalModel", 
        "socialModel",
        "alyssa"        
    };

    auto contributions = run_expert_committee(expert_committee, augmented_input);

    // 2. Detecta emoção
    std::string emotion = fusion_engine->detect_emotion_from_input(input);

    // 3. Gera input fusionado para alyssa
    std::string fused_input = generate_fused_input(input, contributions, emotion);

    // 4. Executa alyssa com o input fusionado (sem TTS)
    std::string final_response = run_expert("alyssa", fused_input, false, nullptr);

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
    size_t limit = 20;

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