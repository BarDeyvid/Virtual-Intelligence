// AlyssaNet.cpp

#include "CoreLLM.hpp"
#include "AlyssaCore.hpp"
#include "voice/PiperTTS.hpp"

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

    try {
        // 1. Inicializa o Core (que cria o modelo E o contexto)
        core_instance = std::make_unique<AlyssaCore>(base_model_path);
        
        llama_model* shared_model = core_instance->get_model();

        // 2. Carrega TODAS as configurações
        AllModelConfigs configs = load_config();
        if (configs.empty()) {
            throw std::runtime_error("Falha ao carregar ConfigsLLM.json");
        }

        // 3. Popula os mapas de especialistas
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
        std::cout << "CoreIntegration (MoE) inicializado com sucesso!" << std::endl;
        return true;

    } catch (const std::exception& e) {
        fprintf(stderr, "ERRO CRÍTICO na Inicialização: %s\n", e.what());
        core_instance = nullptr; 
        return false;
    }
}

// =========================================================================
// 🧠 Método Think (Orquestração)
// =========================================================================

std::string CoreIntegration::think(const std::string& input, PiperTTS& tts) {
    if (!initialized || !core_instance) {
        return "Erro: O CoreIntegration não foi inicializado corretamente.";
    }

    // --- LÓGICA MoE ---
    // Aqui você decidiria qual especialista chamar.
    // Por enquanto, vamos chamar "alyssa" como padrão.
    
    // Exemplo de lógica MoE (futuro):
    // 1. Chamar "emotionModel" para analisar o input
    std::string emotion = run_expert("emotionalModel", input, false, tts);
    // 2. Chamar "memoryModel" para buscar contexto
    std::string context = run_expert("memoryModel", input + " [EMOÇÃO]: " + emotion, false, tts);
    // 3. Chamar "alyssa" para a resposta final
    std::string final_input = "[CONTEXTO]: " + context + " [INPUT]: " + input;

    printf("\033[33m[ALYSSA FINAL]: \033[0m"); 
    std::string final_response = run_expert("alyssa", final_input, true, tts);
    printf("\n\033[0m");

    return final_response;
}


// =========================================================================
// 🛠️ Método run_expert (O novo centro lógico)
// =========================================================================

std::string CoreIntegration::run_expert(const std::string& expert_id, 
                                        const std::string& input,
                                        bool use_tts, 
                                        PiperTTS& tts) {

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
                  << "' para '" << expert_id << "'. Limpando KV Cache.\n";
        clear_kv_cache();
        active_expert_in_cache = expert_id;
    }

    // 4. ADICIONA A NOVA MENSAGEM DO USUÁRIO AO HISTÓRICO (do especialista)
    history.push_back({"user", strdup(input.c_str())});

    // 5. MONTA O TEMPLATE (Corrigindo o bug do AlyssaLLM.hpp)
    std::vector<llama_chat_message> messages_to_template;
    
    // Adiciona o System Prompt (temporariamente)
    if (!config.system_prompt.empty()) {
        messages_to_template.push_back({"system", strdup(config.system_prompt.c_str())});
    }
    
    // Adiciona o histórico da conversa
    messages_to_template.insert(messages_to_template.end(), history.begin(), history.end());

    // 6. APLICA O TEMPLATE E GERA O PROMPT
    std::vector<char> formatted(core_instance->get_n_ctx());
    const char * tmpl = llama_model_chat_template(core_instance->get_model(), nullptr);

    int len = llama_chat_apply_template(
        tmpl, messages_to_template.data(), messages_to_template.size(), true, formatted.data(), formatted.size()
    );
    
    // Libera o System Prompt alocado com strdup
    if (!config.system_prompt.empty()) {
        free((char*)messages_to_template[0].content);
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
                    tts.synthesizeAndPlay(sentence); 
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
                tts.synthesizeAndPlay(sentence_buffer);
            }
        }
        // 8. ADICIONA A RESPOSTA (completa) AO HISTÓRICO
        history.push_back({"assistant", strdup(response.c_str())});
    
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