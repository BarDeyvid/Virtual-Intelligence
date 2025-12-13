// AlyssaNet.cpp

#include "CoreLLM.hpp"
#include "AlyssaCore.hpp"
#include "voice/ElevenLabsTTS.hpp"
#include "log.hpp"
#include <string.h>
#include "pc_metrics_reader.cpp"

using namespace alyssa_core;
logging::Logger logg;

// =========================================================================
// Construtor & Destrutor
// =========================================================================

CoreIntegration::CoreIntegration() : initialized(false), active_expert_in_cache("") {
    logg.info("CoreIntegration constructed");
}

CoreIntegration::~CoreIntegration() {
    // Libera os históricos de chat
    for (auto& pair : expert_histories) {
        for (auto& msg : pair.second) {
            free((char*)msg.content);
        }
    }
    // Libera os LoRAs cacheados
    for (auto& pair : lora_cache) {
        if (pair.second) {
            // TODO: Adicionar a função de liberação do LoRA
            llama_adapter_lora_free(pair.second); 
        }
    }
    logg.info("CoreIntegration destroyed – freeing resources");
}

void CoreIntegration::set_user_name(const std::string& name) {
    user_name = name;
    std::cout << "[IDENTITY] Nome do usuário definido como: " << name << std::endl;
    if (memory_manager) {
        memory_manager->processIdentityFact(name, "user_name");
    }
}

void CoreIntegration::clear_kv_cache() {
    if (core_instance) {
        llama_memory_seq_rm(llama_get_memory(core_instance->get_context()), 0, -1, -1);
        active_expert_in_cache = "";
        std::cout << "[Orquestrador]: KV Cache limpo." << std::endl;
    }
}

bool CoreIntegration::initialize(const std::string& base_model_path) {
    if (initialized) return true;

    llama_log_set([](enum ggml_log_level level, const char * text, void * /* user_data */) { // to shut the llama.cpp bitchy ass up
        if (level >= GGML_LOG_LEVEL_ERROR) {
            fprintf(stderr, "%s", text);
        }
    }, nullptr);

    try {

        embedder = std::make_shared<Embedder>("config/embedder_config.json");
        if (!embedder->initialize()) {
             std::cerr << "Falha ao inicializar o Embedder" << std::endl;
             return false;
        }

        // 2. Crie o Core
        core_instance = std::make_unique<alyssa_core::AlyssaCore>(base_model_path);
        
        // 3. Passe o Embedder existente para o Fusion Engine
        fusion_engine = std::make_unique<alyssa_fusion::WeightedFusion>(*embedder);
        
        // 4. Passe o MESMO Embedder para o Memory Manager
        memory_manager = std::make_unique<alyssa_memory::AlyssaMemoryManager>("../alyssa_advanced_memory.db", embedder);
        std::cout << "Sistema de Memória de Longo Prazo (LTM) inicializado." << std::endl;
        
        llama_model* shared_model = core_instance->get_model();

        // 4. Carrega TODAS as configurações dos especialistas
        AllModelConfigs configs = load_config();
        if (configs.empty()) {
            throw std::runtime_error("Falha ao carregar ConfigsLLM.json");
        }

        // 5. Popula os mapas de especialistas
        for (const auto& cfg : configs) {
            std::cout << "Configurando especialista: " << cfg.id << std::endl;
            
            // Salva a configuração
            expert_configs[cfg.id] = cfg;
            
            // Inicializa histórico vazio
            expert_histories[cfg.id] = {};

            // Pré-carrega o LoRA
            if (cfg.usa_LoRA && !cfg.lora_path.empty()) {
                lora_cache[cfg.id] = llama_adapter_lora_init(shared_model, cfg.lora_path.c_str());
                if (!lora_cache[cfg.id]) {
                    throw std::runtime_error("Falha ao carregar LoRA: " + cfg.lora_path);
                }
                std::cout << "  -> LoRA pré-carregado: " << cfg.lora_path << std::endl;
            } else {
                lora_cache[cfg.id] = nullptr;
            }
        }

        initialized = true;
        std::cout << "CoreIntegration (MoE + Weighted Fusion) inicializado com sucesso!" << std::endl;
        return initialized;

    } catch (const std::exception& e) {
        fprintf(stderr, "ERRO CRÍTICO na Inicialização: %s\n", e.what());
        core_instance = nullptr;
        return false;
    }
}

// Método para gerar um prompt fusionado para alyssa
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

    // Adiciona contexto introspectivo (se existir)
    for (const auto& contrib : contributions) {
        if (contrib.expert_id == "introspectiveModel") {
            fused_prompt += "\n[INTROSPECÇÃO] (de " + contrib.source + "): " + contrib.response;
        }
    }

    // Adiciona contexto emocional (se existir)
    for (const auto& contrib : contributions) {
        if (contrib.expert_id == "emotionalModel") {
            fused_prompt += "\n[EMOÇÃO DETECTADA] (de " + contrib.source + "): " + contrib.response;
        }
    }

    // Adiciona contexto social (se existir)
    for (const auto& contrib : contributions) {
        if (contrib.expert_id == "socialModel") {
            fused_prompt += "\n[SOBRE SOCIEDADE] (de " + contrib.source + "): " + contrib.response;
        }
    }

    // Adiciona contexto de memória (se houver)
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



//  Think com Weighted Fusion
std::string CoreIntegration::think_with_fusion(const std::string& input, ElevenLabsTTS& tts) {
    if (!initialized || !core_instance || !fusion_engine) {
        return "Erro: Sistema não inicializado corretamente.";
    }

    std::cout << "\n[Weighted Fusion] Processando input: " << input << std::endl;

    // =========================================================
    //  RECUPERAR MEMÓRIAS E AUMENTAR O INPUT 
    // =========================================================

    // Convert history vector to a format compatible with the expert committee

    std::string memory_context = "";
    std::string augmented_input = input; // Começa como o input original

    if (memory_manager) {
        auto memories = memory_manager->getHybridMemories(input); 

        if (!memories.empty()) {
            memory_context = "\n[CONTEXTO RELEVANTE DE LTM]\n";
            for (const auto& mem : memories) {
                // Formato: User: [conteudo] | Alyssa: [contexto] (Emoção: [emocao])
                memory_context += "- " + mem.content + " (Emoção: " + mem.emotion + ")\n";
            }
            memory_context += "[FIM CONTEXTO LTM]\n";

            std::cout << "🧠 " << memory_context; 

            // O input que vai para os especialistas é o prompt aumentado
            augmented_input = memory_context + input;
        }
    }

    // 1. Executa comitê de especialistas, usando o INPUT AUMENTADO
    std::vector<std::string> expert_committee = {
        "introspectiveModel", 
        "emotionalModel", 
        "socialModel",
        "alyssa"        
    };

    // Altera a chamada para usar 'augmented_input'
    auto contributions = run_expert_committee(expert_committee, augmented_input);

    // 2. Aplica Weighted Fusion
    std::string emotion = fusion_engine->detect_emotion_from_input(input);
    std::string fused_input = generate_fused_input(input, contributions, emotion);
    
    // 3. Executa alyssa com o input fusionado (AGORA COM TTS ATIVADO)
    std::string final_response = run_expert("alyssa", fused_input, true, &tts);

    // 4. Sintetiza áudio da resposta final
    printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", final_response.c_str());
    tts.synthesizeAndPlay(final_response);

    // =========================================================
    // ⬇ SALVAR A INTERAÇÃO COMPLETA NA MEMÓRIA ⬇
    // =========================================================
    if (memory_manager) {
        // Salva o input ORIGINAL do usuário e a resposta final de Alyssa
        memory_manager->processInteraction(
            input,          
            final_response  
        );
        std::cout << "\n Interação salva na LTM." << std::endl;
    }

    return final_response;
}

//  Think com Weighted Fusion sem TTS
std::string CoreIntegration::think_with_fusion_ttsless(const std::string& input) {
    if (!initialized || !core_instance || !fusion_engine) {
        return "Erro: Sistema não inicializado corretamente.";
    }

    std::cout << "\n[Weighted Fusion TTS-less] Processando input: " << input << std::endl;

    std::string memory_context = "";
    std::string augmented_input = input; // Começa como o input original

    if (memory_manager) {
        auto memories = memory_manager->getHybridMemories(input); 

        if (!memories.empty()) {
            memory_context = "\n[CONTEXTO RELEVANTE DE LTM]\n";
            for (const auto& mem : memories) {
                // Formato: User: [conteudo] | Alyssa: [contexto] (Emoção: [emocao])
                memory_context += "- " + mem.content + " (Emoção: " + mem.emotion + ")\n";
            }
            memory_context += "[FIM CONTEXTO LTM]\n";

            std::cout << "🧠 " << memory_context; 

            // O input que vai para os especialistas é o prompt aumentado
            augmented_input = memory_context + input;
        }
    }

    // 1. Executa comitê de especialistas, usando o INPUT AUMENTADO
    std::vector<std::string> expert_committee = {
        "introspectiveModel", 
        "emotionalModel", 
        "socialModel",
        "alyssa"        
    };

    // Altera a chamada para usar 'augmented_input'
    auto contributions = run_expert_committee(expert_committee, augmented_input); 

    // 2. Detecta emoção
    std::string emotion = fusion_engine->detect_emotion_from_input(input);

    // 3. Gera input fusionado para alyssa
    std::string fused_input = generate_fused_input(input, contributions, emotion);

    // 4. Executa alyssa com o input fusionado
    std::string final_response = run_expert("alyssa", fused_input, false, nullptr);

    // 3. Apenas exibe a resposta final (sem sintetizar áudio)
    printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", final_response.c_str());

    // =========================================================
    //  SALVAR A INTERAÇÃO COMPLETA NA MEMÓRIA 
    // =========================================================

    if (memory_manager) {
        // Salva o input ORIGINAL do usuário e a resposta final de Alyssa
        memory_manager->processInteraction(
            input,          
            final_response  
        );
        std::cout << "\n Interação salva na LTM." << std::endl;
    }

    return final_response;
}

void CoreIntegration::log_source_awareness(const std::string& source, const std::string& message) {
    std::cout << "[SOURCE AWARENESS] " << source << " diz: " << message << std::endl;
}

//  Executa múltiplos especialistas e coleta contribuições
std::vector<alyssa_fusion::ExpertContribution> 
CoreIntegration::run_expert_committee(const std::vector<std::string>& expert_ids,
                                     const std::string& input) {
    
    std::vector<alyssa_fusion::ExpertContribution> contributions;
    
    for (const auto& expert_id : expert_ids) {
        if (expert_configs.find(expert_id) == expert_configs.end()) {
            std::cerr << "Especialista não encontrado: " << expert_id << std::endl;
            continue;
        }
        
        try {
            // Executa especialista sem TTS
            std::string response = run_expert(expert_id, input, false, nullptr);
            
            // Cria contribuição
            alyssa_fusion::ExpertContribution contrib;
            contrib.expert_id = expert_id;
            contrib.response = response;
            
            std::cout << "[Comitê] " << expert_id << " respondeu: " 
                    << (response.length() > 50 ? response.substr(0, 50) + "..." : response) 
                    << " (de " << contrib.source << ")" << std::endl;

            
            // Calcula embedding da resposta
            if (embedder) {
                try {
                    contrib.embedding = embedder->generate_embedding(response);
                } catch (const std::exception& e) {
                    std::cerr << "Erro ao calcular embedding para " << expert_id << ": " << e.what() << std::endl;
                }
            }
            
            if (expert_id == "emotionalModel") {
                contrib.source = "subconscious";
            } else if (expert_id == "introspectiveModel") {
                contrib.source = "subconscious";
            } else if (expert_id == "socialModel") {
                contrib.source = "subconscious";
            } else if (expert_id == "memoryModel") {
                contrib.source = "memory";
            } else {
                contrib.source = "Deyvid";
            }
            
            contributions.push_back(contrib);
            
            std::cout << "[Comitê] " << expert_id << " respondeu: " 
                      << (response.length() > 50 ? response.substr(0, 50) + "..." : response) 
                      << std::endl;
                      
        } catch (const std::exception& e) {
            std::cerr << "Erro executando " << expert_id << ": " << e.what() << std::endl;
        }
    }
    
    return contributions;
}


// =========================================================================
// 🧠 Método Think (Orquestração)
// =========================================================================

std::string CoreIntegration::think(const std::string& input, ElevenLabsTTS& tts) {
    if (!initialized || !core_instance) {
        return "Erro: O CoreIntegration não foi inicializado corretamente.";
    }

    // Maintain conversation history
    static std::vector<std::pair<std::string, std::string>> conversation_history;

    // Add new input to the conversation history
    conversation_history.emplace_back("user", input);

    // --- LÓGICA MoE ---
    
    // 1. Chamar "emotionModel" para analisar o input
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

    // Add AI response to the conversation history
    conversation_history.emplace_back("assistant", final_response);

    return final_response;
}



// =========================================================================
// Método run_expert
// =========================================================================

std::string CoreIntegration::run_expert(const std::string& expert_id, 
                                        const std::string& input,
                                        bool use_tts, 
                                        ElevenLabsTTS* tts) {

    // 1. Verifica se o especialista existe
    if (expert_configs.find(expert_id) == expert_configs.end()) {
        throw std::runtime_error("Especialista desconhecido: " + expert_id);
    }
    PCMetricsReader pcmetrics;

    // 2. PEGA A CONFIG, HISTÓRICO E LORA
    SimpleModelConfig& config = expert_configs.at(expert_id);
    std::vector<llama_chat_message>& history = expert_histories.at(expert_id);
    llama_adapter_lora* lora = lora_cache.at(expert_id);

    // 3. CONTROLE DE KV CACHE
    // Se o especialista que queremos usar NÃO é o que está no cache,
    // devemos limpar o cache para evitar misturar estados.
    if (active_expert_in_cache != expert_id) {
        std::cout << "\n[Orquestrador]: Trocando especialista de '" << active_expert_in_cache 
                  << "' para '" << expert_id << "'. Limpando KV Cache E Histórico Local.\n";
        clear_kv_cache();
        active_expert_in_cache = expert_id;
        
        // NOVO: Limpa o histórico local do novo especialista para que a primeira execução não reprocessse um prompt enorme.
        for (auto& msg : history) {
            free((char*)msg.content);
        }
        history.clear();
    }

    // 4. ADICIONA A NOVA MENSAGEM DO USUÁRIO AO HISTÓRICO
    std::string expert_input_with_role = "";
    if (!config.role_instruction.empty()) {
        expert_input_with_role = "[ROLE]: " + config.role_instruction + "\n" + input;
    } else {
        expert_input_with_role = input;
    }

    history.push_back({"user", strdup(expert_input_with_role.c_str())});

    // 5. MONTA O TEMPLATE
    std::vector<llama_chat_message> messages_to_template;
    int system_prompt_index = -1;
    
    std::string external = pcmetrics.get_simple_metrics_text();
    
    // Combinar system_prompt com métricas
    std::string combined_system_prompt = config.system_prompt;
    if (!external.empty()) {
        combined_system_prompt += "\n[CONTEXTO DO SISTEMA - MÉTRICAS DO PC]:\n" + external;
    }
    
    // Adiciona o System Prompt COM MÉTRICAS
    if (!combined_system_prompt.empty()) {
        system_prompt_index = messages_to_template.size();
        messages_to_template.push_back({"system", strdup(combined_system_prompt.c_str())});
    }
    
    // Adiciona o histórico da conversa
    messages_to_template.insert(messages_to_template.end(), history.begin(), history.end());

    // 6. APLICA O TEMPLATE E GERA O PROMPT
    std::vector<char> formatted(core_instance->get_n_ctx());
    const char * tmpl = llama_model_chat_template(core_instance->get_model(), nullptr);

    int len = llama_chat_apply_template(
        tmpl, messages_to_template.data(), messages_to_template.size(), true, formatted.data(), formatted.size()
    );
    
    // Libera OBRIGATORIAMENTE o System Prompt alocado com strdup
    if (system_prompt_index != -1) {
        free((char*)messages_to_template[system_prompt_index].content);
    }

    // (Lógica de resize, se necessário)
    if (len < 0 || len > (int)formatted.size()) {
        if (len > (int)formatted.size()) {
            formatted.resize(len);
            len = llama_chat_apply_template(
                tmpl, messages_to_template.data(), messages_to_template.size(), true, formatted.data(), formatted.size()
            );
        }
        if (len < 0) {
            fprintf(stderr, "Falha ao aplicar template de chat\n");
            return "Erro ao processar a conversa.";
        }
    }
    
    std::string prompt(formatted.begin(), formatted.begin() + len);

    std::string sentence_buffer;
    if (use_tts) {
        auto stream_callback = [&](const std::string& piece) {
            sentence_buffer += piece;
            
            // Quebra a resposta por pontuação (simples)
            size_t punctuation_pos = sentence_buffer.find_first_of(".!?");
            
            if (punctuation_pos != std::string::npos) {
                // Extrai a sentença
                std::string sentence = sentence_buffer.substr(0, punctuation_pos + 1);
                // Remove a sentença do buffer
                sentence_buffer = sentence_buffer.substr(punctuation_pos + 1);
                
                // Limpa espaços em branco do início
                sentence.erase(0, sentence.find_first_not_of(" \t\n\r"));
                
                if (!sentence.empty()) {
                    tts->synthesizeAndPlay(sentence); 
                }
            }
        };
        // 7. CHAMA A GERAÇÃO DE BAIXO NÍVEL (com o callback)
        std::string response = core_instance->generate_raw(
            prompt,
            config.params, 
            lora, 
            stream_callback // <--- PASSA O CALLBACK
        ); 
    
        // Toca qualquer texto que sobrou no buffer
        if (!sentence_buffer.empty()) {
            sentence_buffer.erase(0, sentence_buffer.find_first_not_of(" \t\n\r"));
            if (!sentence_buffer.empty()) {
                tts->synthesizeAndPlay(sentence_buffer);
            }
        }
        // 8. ADICIONA A RESPOSTA (completa) AO HISTÓRICO
        history.push_back({"assistant", strdup(response.c_str())});
        manage_dynamic_history(expert_id, history);

        return response;
    } else {
        std::string response = core_instance->generate_raw(
            prompt,
            config.params, 
            lora, 
            nullptr
        ); 
        // 8. ADICIONA A RESPOSTA (completa) AO HISTÓRICO
        history.push_back({"assistant", strdup(response.c_str())});
    
        return response;
    }
}

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
    else if (expert_id == "creativeModel") {
        limit += 10; 
    }

    if (limit > 50) limit = 50;
    if (limit < 10) limit = 10;

    return limit;
}

void CoreIntegration::manage_dynamic_history(const std::string& expert_id, 
                                           std::vector<llama_chat_message>& history) {
    
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
// Métodos auxiliares (act, reflect, loop)
// =========================================================================

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
        
        // think(user_input, nullptr); // 'think' agora cuida da orquestração
    }
}