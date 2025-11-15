// AlyssaNet.cpp

#include "CoreLLM.hpp"
#include "AlyssaCore.hpp"
#include "voice/ElevenLabsTTS.hpp"

using namespace alyssa_core;

// =========================================================================
// Construtor & Destrutor
// =========================================================================

CoreIntegration::CoreIntegration() : initialized(false), active_expert_in_cache("") {}

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
        memory_manager = std::make_unique<AlyssaMemoryManager>("alyssa_advanced_memory.db", embedder);

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

        // (A inicialização da LTM foi movida para o topo)
        // memory_manager = std::make_unique<AlyssaMemoryManager>("alyssa_advanced_memory.db", true);
        // std::cout << "Sistema de Memória de Longo Prazo (LTM) inicializado." << std::endl;

        initialized = true;
        std::cout << "CoreIntegration (MoE + Weighted Fusion) inicializado com sucesso!" << std::endl;
        return initialized;

    } catch (const std::exception& e) {
        fprintf(stderr, "ERRO CRÍTICO na Inicialização: %s\n", e.what());
        core_instance = nullptr;
        return false;
    }
}

// 🆕 Think com Weighted Fusion
std::string CoreIntegration::think_with_fusion(const std::string& input, ElevenLabsTTS& tts) {
    if (!initialized || !core_instance || !fusion_engine) {
        return "Erro: Sistema não inicializado corretamente.";
    }

    std::cout << "\n[Weighted Fusion] Processando input: " << input << std::endl;

    // =========================================================
    // ⬇️ RECUPERAR MEMÓRIAS E AUMENTAR O INPUT ⬇️
    // =========================================================
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
        "emotionalModel", 
        "memoryModel", 
        "introspectiveModel",
        "alyssa"  // especialista geral
    };
    
    // Altera a chamada para usar 'augmented_input'
    auto contributions = run_expert_committee(expert_committee, augmented_input); 
    
    // 2. Aplica Weighted Fusion
    std::string emotion = fusion_engine->detect_emotion_from_input(input);
    std::string final_response = fusion_engine->fuse_responses(input, contributions, emotion);
    
    // 3. Sintetiza áudio da resposta final
    printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", final_response.c_str());
    tts.synthesizeAndPlay(final_response);
    
    // =========================================================
    // ⬇️ SALVAR A INTERAÇÃO COMPLETA NA MEMÓRIA ⬇️
    // =========================================================
    if (memory_manager) {
        // Salva o input ORIGINAL do usuário e a resposta final de Alyssa
        memory_manager->processInteraction(
            input,          
            final_response  
        );
        std::cout << "\n✅ Interação salva na LTM." << std::endl;
    }

    return final_response;
}

// 🆕 Think com Weighted Fusion sem TTS
std::string CoreIntegration::think_with_fusion_ttsless(const std::string& input) {
    if (!initialized || !core_instance || !fusion_engine) {
        return "Erro: Sistema não inicializado corretamente.";
    }

    std::cout << "\n[Weighted Fusion TTS-less] Processando input: " << input << std::endl;

    // =========================================================
    // ⬇️ RECUPERAR MEMÓRIAS E AUMENTAR O INPUT ⬇️
    // =========================================================
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
        "emotionalModel", 
        "memoryModel", 
        "introspectiveModel",
        "alyssa"  // especialista geral
    };
    
    // Altera a chamada para usar 'augmented_input'
    auto contributions = run_expert_committee(expert_committee, augmented_input); 
    
    // 2. Aplica Weighted Fusion
    std::string emotion = fusion_engine->detect_emotion_from_input(input);
    std::string final_response = fusion_engine->fuse_responses(input, contributions, emotion);
    
    // 3. Apenas exibe a resposta final (sem sintetizar áudio)
    printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", final_response.c_str());
    
    // =========================================================
    // ⬇️ SALVAR A INTERAÇÃO COMPLETA NA MEMÓRIA ⬇️
    // =========================================================
    if (memory_manager) {
        // Salva o input ORIGINAL do usuário e a resposta final de Alyssa
        memory_manager->processInteraction(
            input,          
            final_response  
        );
        std::cout << "\n✅ Interação salva na LTM." << std::endl;
    }

    return final_response;
}

// 🆕 Executa múltiplos especialistas e coleta contribuições
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
            
            // Calcula embedding da resposta
            if (embedder) {
                try {
                    contrib.embedding = embedder->generate_embedding(response);
                } catch (const std::exception& e) {
                    std::cerr << "Erro ao calcular embedding para " << expert_id << ": " << e.what() << std::endl;
                }
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

    // --- LÓGICA MoE ---
    
    // Exemplo de lógica MoE (futuro):
    // 1. Chamar "emotionModel" para analisar o input
    std::string emotion = run_expert("emotionalModel", input, false, &tts);
    // 2. Chamar "memoryModel" para buscar contexto
    std::string context = run_expert("memoryModel", input + " [EMOÇÃO]: " + emotion, false, &tts);
    // 3. Chamar "alyssa" para a resposta final
    std::string final_input = "[CONTEXTO]: " + context + " [INPUT]: " + input;

    printf("\033[33m[ALYSSA FINAL]: \033[0m"); 
    std::string final_response = run_expert("alyssa", final_input, true, &tts);
    printf("\n\033[0m");

    return final_response;
}


// =========================================================================
// 🛠️ Método run_expert (O novo centro lógico)
// =========================================================================

std::string CoreIntegration::run_expert(const std::string& expert_id, 
                                        const std::string& input,
                                        bool use_tts, 
                                        ElevenLabsTTS* tts) {

    // 1. Verifica se o especialista existe
    if (expert_configs.find(expert_id) == expert_configs.end()) {
        throw std::runtime_error("Especialista desconhecido: " + expert_id);
    }

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

    // 4. ADICIONA A NOVA MENSAGEM DO USUÁRIO AO HISTÓRICO (do especialista)
    std::string expert_input_with_role = "";
    if (!config.role_instruction.empty()) {
        expert_input_with_role = "[ROLE]: " + config.role_instruction + "\n" + input;
    } else {
        expert_input_with_role = input;
    }

    history.push_back({"user", _strdup(expert_input_with_role.c_str())}); // <--- ALTERADO

    // 5. MONTA O TEMPLATE (Corrigindo o bug do AlyssaLLM.hpp)
    std::vector<llama_chat_message> messages_to_template;
    int system_prompt_index = -1; // <--- NOVO: Armazena o índice do system prompt
    
    // Adiciona o System Prompt (temporariamente)
    if (!config.system_prompt.empty()) {
        system_prompt_index = messages_to_template.size(); // O system será o primeiro/único neste momento
        messages_to_template.push_back({"system", _strdup(config.system_prompt.c_str())});
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
        history.push_back({"assistant", _strdup(response.c_str())});
                                                                                            // TODO: Fazer isso daqui se tornar responsivo
        const size_t MAX_HISTORY = 20; // 10 turnos (20 mensagens: 10 user + 10 assistant)
        while (history.size() > MAX_HISTORY) {
            // Remove o par de mensagens mais antigo
            
            // A mensagem é sempre alocada com _strdup/malloc. Precisamos liberá-la.
            free((char*)history.front().content);
            history.erase(history.begin());
        }

        return response;
    } else {
        std::string response = core_instance->generate_raw(
            prompt, 
            config.params, 
            lora, 
            nullptr
        ); 
        // 8. ADICIONA A RESPOSTA (completa) AO HISTÓRICO
        history.push_back({"assistant", _strdup(response.c_str())});
    
        return response;
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