// AlyssaNet.cpp 

#include "AlyssaNet.hpp"
#include "AlyssaCore.hpp"
#include "voice/TTSBase.hpp"
#include "log.hpp"
#include "ExpertBase.hpp"
#include "pc_metrics_reader.hpp"
#include "UserPrefs.hpp"
#include "InputController.hpp"
#include <string>
#include <memory>
#include <algorithm>
#include <sstream>
#include <map>
#include <set>
#include <regex>
#include <cctype>
#include <future>
#include <chrono>
#include <random>
#include <ctime>
#include "vision/VisionManager.hpp"
#include "ProactivityEngine.hpp"

#include <curl/curl.h>
#include <opencv2/opencv.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace alyssa_core;

// =========================================================================
// Constructor & Destructor
// =========================================================================

/**
 * @brief Default constructor for CoreIntegration.
 * @details Initializes with default values and logs construction.
 */
CoreIntegration::CoreIntegration() 
    : initialized(false), active_expert_in_cache(""),
      endocrine_system(std::make_unique<alyssa_endocrine::EndocrineSystem>())
{
    printf("CoreIntegration constructed");
}

/**
 * @brief Destructor for CoreIntegration.
 * @details Cleans up expert histories and logs destruction.
 */
CoreIntegration::~CoreIntegration() {
    for (auto& pair : expert_histories) {
        free_chat_history(pair.second);
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

namespace {

// Expert com core próprio (não usa o modelo base 1b compartilhado).
// Batizada de "Alyssa4bExpert" originalmente, mas nada aqui é específico da
// Alyssa — reaproveitada para gameplayModel (E2B) também. Vive em file scope
// (e não dentro de initialize()) porque ensure_gameplay_expert() também
// instancia uma no load lazy do Minecraft.
class DedicatedCoreExpert : public alyssa_experts::ExpertBase {
private:
    std::unique_ptr<alyssa_core::AlyssaCore> own_core;

public:
    DedicatedCoreExpert(const SimpleModelConfig& cfg, std::unique_ptr<alyssa_core::AlyssaCore> core)
        : ExpertBase(cfg), own_core(std::move(core)) {
    }

    bool initialize(llama_model* shared_model) override {
        // Não precisamos inicializar com o modelo compartilhado
        // pois temos nosso próprio core
        if (config.usa_LoRA && !config.lora_path.empty()) {
            lora = llama_adapter_lora_init(own_core->get_model(), config.lora_path.c_str());
            if (!lora) {
                std::cerr << "Falha ao carregar LoRA para " << config.id << ": " << config.lora_path << std::endl;
                return false;
            }
            std::cout << "LoRA carregado para " << config.id << ": " << config.lora_path << std::endl;
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
        // Usar nosso próprio core em vez do core_instance passado
        return ExpertBase::run(input, own_core.get(), lora_override,
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
        // Usar nosso próprio core em vez do core_instance passado
        return ExpertBase::get_contribution(input, own_core.get(), embedder, lora_override,
                                          current_history, active_lora_in_context, stream_callback);
    }

    void clear_own_kv_cache() override {
        if (own_core) own_core->clear_kv();
    }

    alyssa_core::AlyssaCore* dedicated_core() override { return own_core.get(); }
};

} // namespace

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

        // 1.5 Router adaptativo (docs/plano-router-e-voz-gameplay.md, A):
        // flag fora do array "models" + grammar carregada uma vez. Qualquer
        // falha degrada pro comportamento antigo (comitê sempre).
        try {
            std::ifstream cfg_file("config/ConfigsLLM.json");
            nlohmann::json j = nlohmann::json::parse(cfg_file);
            router_adaptive = (j.value("router_mode", "committee") == "adaptive");
        } catch (const std::exception&) {
            router_adaptive = false;
        }
        if (router_adaptive) {
            std::ifstream gf("config/grammars/router.gbnf");
            if (gf.is_open()) {
                std::stringstream ss;
                ss << gf.rdbuf();
                router_grammar = ss.str();
            }
            if (router_grammar.empty()) {
                std::cout << "[Router] grammar ausente — voltando pro modo comitê" << std::endl;
                router_adaptive = false;
            } else {
                std::cout << "[Router] Modo adaptativo LIGADO (pre-pass no 1B)" << std::endl;
            }
        }

        // 2. Buscar configurações dos modelos
        const SimpleModelConfig* base_1b_config = nullptr;
        const SimpleModelConfig* alyssa_4b_config = nullptr;
        const SimpleModelConfig* gameplay_config = nullptr;

        for (const auto& cfg : configs) {
            if (cfg.id == "gameplayModel") {
                gameplay_config = &cfg;
            }
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

        // 4.1. Pool de contextos para execução paralela dos experts (Fase 3.2).
        // Os contextos compartilham os pesos do modelo 1B (sem duplicar RAM/VRAM);
        // cada um roda em sua própria thread. Tamanho = TOP_K do gating.
        // Falha na criação (ex.: sem memória) não é fatal: cai no modo sequencial.
        constexpr int EXPERT_POOL_SIZE = 3;
        try {
            for (int i = 0; i < EXPERT_POOL_SIZE; ++i) {
                expert_context_pool.push_back(std::make_unique<alyssa_core::AlyssaCore>(
                    core_instance->get_model(), context_size, base_1b_config->n_batch));
            }
            std::cout << "[Parallel] Pool de " << expert_context_pool.size()
                      << " contextos criado para experts em paralelo" << std::endl;
        } catch (const std::exception& e) {
            expert_context_pool.clear();
            std::cerr << "[Parallel] Falha ao criar pool de contextos (" << e.what()
                      << "). Mantendo execução sequencial." << std::endl;
        }

        // 5. Inicializar fusion engine
        fusion_engine = std::make_unique<alyssa_fusion::WeightedFusion>(*embedder);

        // 5.1. Inicializar sistema de tools (registry-driven, Fase 1)
        tool_executor = std::make_unique<alyssa_tools::ToolExecutor>();
        if (tool_executor->load_registry("config/tools_registry.json")) {
            tool_executor->register_default_handlers();
            register_builtin_tools();
        }
        // Registro ausente/malformado não é fatal: Alyssa funciona sem tools.

        // 5.2. Carregar perfil de personalidade (Fase 2.1; ausência não é fatal)
        personality = alyssa_personality::load_personality("config/personality.json");
        {
            std::error_code ec;
            personality_mtime = std::filesystem::last_write_time("config/personality.json", ec);
        }

        // 5.3. Detector de presença via webcam (cascade ausente = desativado)
        presence_detector = std::make_unique<alyssa_vision::PresenceDetector>();
        if (!presence_detector->is_ready()) {
            presence_detector.reset(); // sem cascade, sem presença — sistema segue normal
        }

        // 5.4. Humor do dia: offsets pequenos e determinísticos (hash da data)
        // nos hormônios — a Alyssa "acorda" um pouco diferente a cada dia.
        if (endocrine_system) {
            std::time_t t = std::time(nullptr);
            std::tm local_tm{};
#ifdef _WIN32
            localtime_s(&local_tm, &t);
#else
            localtime_r(&t, &local_tm);
#endif
            unsigned seed = static_cast<unsigned>(
                (local_tm.tm_year + 1900) * 10000 + (local_tm.tm_mon + 1) * 100 + local_tm.tm_mday);
            std::mt19937 rng(seed);
            std::uniform_real_distribution<double> offset(-0.08, 0.08);

            for (const char* hormone : {"cortisol", "dopamine", "oxytocin", "serotonin", "adrenaline"}) {
                double level = endocrine_system->get_hormone_level(hormone) + offset(rng);
                endocrine_system->set_hormone_level(hormone, std::clamp(level, 0.0, 1.0));
            }
            std::cout << "[Humor do Dia] Baseline hormonal do dia aplicado (seed " << seed << "):\n"
                      << endocrine_system->get_hormone_profile().to_string() << std::endl;
        }

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
            // Pular gameplayModel (E2B) - também tem core próprio, registrado
            // separadamente depois da Alyssa (ver passo 9 abaixo).
            if (cfg.id == "gameplayModel") {
                std::cout << "[INFO] Especialista '" << cfg.id << "' (E2B) será registrado separadamente" << std::endl;
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
        
        // Fase 4.3: se o modelo 4B falhar ao carregar (arquivo ausente, sem
        // VRAM...), Alyssa roda degradada sobre o modelo base 1B já carregado
        // (contexto extra, sem duplicar pesos) em vez de derrubar o sistema.
        std::unique_ptr<alyssa_core::AlyssaCore> alyssa_core;
        try {
            alyssa_core = std::make_unique<alyssa_core::AlyssaCore>(alyssa_4b_config->model_path, alyssa_context_size);
        } catch (const std::exception& e) {
            std::cerr << "[Fallback] Falha ao carregar modelo Alyssa 4B (" << e.what()
                      << "). Usando modelo base 1B como fallback — qualidade reduzida." << std::endl;
            int fallback_ctx = std::min(alyssa_context_size, 8192);
            alyssa_core = std::make_unique<alyssa_core::AlyssaCore>(
                core_instance->get_model(), fallback_ctx, base_1b_config->n_batch);
        }
        
        // Registrar especialista Alyssa com modelo 4b
        auto alyssa_expert = std::make_unique<DedicatedCoreExpert>(*alyssa_4b_config, std::move(alyssa_core));
        if (alyssa_expert->initialize(nullptr)) {
            register_expert(std::move(alyssa_expert));
            std::cout << "[INFO] Especialista Alyssa (4b) registrado com sucesso" << std::endl;
        } else {
            std::cerr << "Falha ao inicializar especialista Alyssa" << std::endl;
        }

        // 9. gameplayModel (Minecraft, PoC Fase 6+): modelo próprio (E2B).
        // LAZY: só guarda a config aqui — os ~3GB do E2B só entram na VRAM
        // no primeiro "/mc start" (ensure_gameplay_expert). Na maioria das
        // sessões o Minecraft nunca liga, e esses 3GB fazem falta pro
        // scheduler (é a diferença entre o Whisper caber HOT ou virar JIT).
        if (gameplay_config) {
            deferred_gameplay_config = std::make_unique<SimpleModelConfig>(*gameplay_config);
            gameplay_fallback_n_batch = base_1b_config->n_batch;
            std::cout << "[INFO] gameplayModel (E2B) registrado para load lazy "
                         "(carrega no primeiro /mc start)" << std::endl;
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

bool CoreIntegration::ensure_gameplay_expert() {
    if (experts.count("gameplayModel")) return true; // já carregado
    if (!initialized || !deferred_gameplay_config) return false;

    const SimpleModelConfig& cfg = *deferred_gameplay_config;
    std::cout << "[INFO] Carregando gameplayModel (lazy): " << cfg.model_path << std::endl;

    int gameplay_context_size = cfg.n_ctx;
    if (gameplay_context_size < 4096) gameplay_context_size = 4096;

    // Mesmo fallback degradado da Alyssa 4B: se o E2B falhar ao carregar,
    // cai para o 1b compartilhado em vez de derrubar o sistema inteiro.
    std::unique_ptr<alyssa_core::AlyssaCore> gameplay_core;
    try {
        gameplay_core = std::make_unique<alyssa_core::AlyssaCore>(
            cfg.model_path, gameplay_context_size);
    } catch (const std::exception& e) {
        std::cerr << "[Fallback] Falha ao carregar modelo do gameplayModel (" << e.what()
                  << "). Usando modelo base 1B como fallback — qualidade reduzida." << std::endl;
        int fallback_ctx = std::min(gameplay_context_size, 8192);
        try {
            gameplay_core = std::make_unique<alyssa_core::AlyssaCore>(
                core_instance->get_model(), fallback_ctx, gameplay_fallback_n_batch);
        } catch (const std::exception& e2) {
            std::cerr << "[MinecraftSession] Fallback 1B também falhou: " << e2.what() << std::endl;
            return false;
        }
    }

    auto gameplay_expert = std::make_unique<DedicatedCoreExpert>(cfg, std::move(gameplay_core));
    if (!gameplay_expert->initialize(nullptr)) {
        std::cerr << "Falha ao inicializar especialista gameplayModel" << std::endl;
        return false;
    }
    register_expert(std::move(gameplay_expert));
    std::cout << "[INFO] Especialista gameplayModel (E2B) registrado com sucesso" << std::endl;
    return true;
}

std::string CoreIntegration::run_gameplay_tick_audio(
    const std::string& world_state_prompt_with_marker,
    const std::vector<float>& audio_16k)
{
    auto it = experts.find("gameplayModel");
    if (it == experts.end()) return "";
    alyssa_core::AlyssaCore* core = it->second->dedicated_core();
    if (!core) return "";

    if (!core->audio_ready() &&
        !core->init_audio("models/mmproj-gemma-4-E2B-it-Q8_0.gguf")) {
        return "";
    }

    clear_kv_cache();
    it->second->clear_own_kv_cache();
    free_chat_history(expert_histories["gameplayModel"]);

    // Prompt manual no formato do run(): [ROLE] + estado do mundo (que já
    // inclui a linha "[VOZ DO JOGADOR] <__media__>"), envelopado no turn
    // format do Gemma 4. Sem histórico: cada tick é stateless.
    const SimpleModelConfig& cfg = it->second->get_config();
    std::string user_text = cfg.role_instruction.empty()
        ? world_state_prompt_with_marker
        : "[ROLE]: " + cfg.role_instruction + "\n" + world_state_prompt_with_marker;

    std::vector<llama_chat_message> msgs;
    msgs.push_back({"system", cfg.system_prompt.c_str()});
    msgs.push_back({"user", user_text.c_str()});
    std::string prompt = alyssa_experts::ExpertBase::format_gemma4_prompt(msgs);

    return core->generate_with_audio(prompt, audio_16k, cfg.params, nullptr);
}

std::string CoreIntegration::run_router_prepass(const std::string& input) {
    if (!core_instance || router_grammar.empty()) return "comite";

    auto t0 = std::chrono::steady_clock::now();

    // Template gemma3 do 1B montado na mão (prompt de turno único, sem
    // histórico — roteamento é stateless de propósito).
    std::string prompt =
        "<start_of_turn>user\n"
        "Você é o roteador da Alyssa. Classifique a mensagem do usuário na rota "
        "que produz a melhor resposta:\n"
        "- direto: pergunta ou papo comum que a Alyssa responde bem sozinha\n"
        "- emocional: carga emocional forte (desabafo, tristeza, briga, euforia)\n"
        "- analitico: raciocínio técnico ou decisão complexa\n"
        "- criativo: pedido de criação (história, ideia, brainstorm)\n"
        "- memoria: referência a conversas ou fatos passados (\"lembra\", \"aquilo que te falei\")\n"
        "- comite: ambíguo ou pesado, precisa de várias perspectivas\n\n"
        "Mensagem: \"" + input + "\"<end_of_turn>\n"
        "<start_of_turn>model\n";

    SimpleModelParameters params;
    params.temperature  = 0.1;
    params.max_tokens   = 12;
    params.timeout_ms   = 5000; // roteador travado não pode travar o turno
    params.grammar      = router_grammar;
    params.grammar_root = "root";

    std::string route = "comite";
    try {
        clear_kv_cache(); // core do 1B limpo (e tracking de expert resetado)
        std::string raw = core_instance->generate_raw(prompt, params, nullptr, nullptr);
        clear_kv_cache(); // não deixa o prompt do roteador pro próximo expert

        // "[ROTA] nome" → nome (a grammar garante o shape; isto é cinto)
        size_t pos = raw.find("[ROTA] ");
        if (pos != std::string::npos) {
            route = raw.substr(pos + 7);
            route.erase(route.find_last_not_of(" \n\r\t") + 1);
        }
    } catch (const std::exception& e) {
        std::cerr << "[Router] pre-pass falhou (" << e.what() << ") — indo de comitê" << std::endl;
        route = "comite";
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    std::cout << "[Router] rota=" << route << " (" << ms << "ms)" << std::endl;

    // Auditoria: cada decisão vira uma linha de JSONL (pra avaliar se o 1B
    // roteia bem ou se precisa do upgrade pro E2B).
    try {
        std::filesystem::create_directories("logs");
        std::ofstream log("logs/router_decisions.jsonl", std::ios::app);
        nlohmann::json line = {
            {"ts", static_cast<long long>(std::time(nullptr))},
            {"route", route},
            {"router_ms", ms},
            {"input_len", input.size()},
        };
        log << line.dump() << "\n";
    } catch (const std::exception&) { /* log é best-effort */ }

    return route;
}

std::string CoreIntegration::generate_direct_response(const std::string& respond_to,
                                                      const std::string& raw_input,
                                                      bool use_tts, ITTS* tts) {
    // Modo direto também precisa de personalidade e ferramentas — perguntas
    // curtas ("lista os arquivos?") caem muito neste caminho.
    std::string direct_personality = alyssa_personality::generate_personality_context(
        personality,
        endocrine_system ? &endocrine_system->get_hormone_profile() : nullptr);
    std::string direct_tools = tool_executor ? tool_executor->get_tools_prompt() : "";

    std::string direct_prompt = direct_personality + direct_tools + build_ambient_context() +
                                "[MODO DIRETO] Responda ao usuário: " + respond_to;
    std::string direct_resp = run_expert("alyssa", direct_prompt, use_tts, tts, &raw_input);
    direct_resp = resolve_tool_calls(direct_resp, use_tts, tts);
    printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", direct_resp.c_str());
    if (memory_manager && should_store_in_memory(raw_input, direct_resp))
        memory_manager->processInteraction(raw_input, direct_resp);
    log_interaction_for_dataset(raw_input, direct_resp, "direct");
    clear_kv_cache();
    return direct_resp;
}

// =========================================================================
// Tool System (Phase 1): built-in handlers with heavy dependencies
// =========================================================================

namespace {

size_t curl_write_to_string(void* contents, size_t size, size_t nmemb, void* userp) {
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

/// Remove tags HTML e decodifica as entidades mais comuns (parser cru de PoC).
std::string strip_html(const std::string& html) {
    std::string text;
    bool in_tag = false;
    for (char c : html) {
        if (c == '<') in_tag = true;
        else if (c == '>') in_tag = false;
        else if (!in_tag) text += c;
    }
    static const std::pair<const char*, const char*> entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&#x27;", "'"}, {"&#39;", "'"}, {"&nbsp;", " "}
    };
    for (const auto& [from, to] : entities) {
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, strlen(from), to);
        }
    }
    return text;
}

/// Só permite nomes de arquivo simples (sem path, sem shell metachars).
bool is_safe_filename(const std::string& name) {
    if (name.empty() || name.size() > 128) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != '-') {
            return false;
        }
    }
    return name.find("..") == std::string::npos;
}

} // namespace

void CoreIntegration::register_builtin_tools() {
    if (!tool_executor) return;

    // --- system_metrics: reaproveita o leitor já existente ---
    tool_executor->register_handler("system_metrics",
        [](const std::map<std::string, std::string>&) -> std::string {
            PCMetricsReader reader;
            return reader.get_simple_metrics_text();
        });

    // --- screenshot: GDI no Windows, grim no Linux, salvo via OpenCV ---
    tool_executor->register_handler("screenshot",
        [](const std::map<std::string, std::string>& args) -> std::string {
            std::string filename = "screenshot.png";
            auto it = args.find("filename");
            if (it != args.end() && !it->second.empty()) filename = it->second;
            if (!is_safe_filename(filename)) {
                return "ERRO: nome de arquivo inválido (use apenas letras, números, '.', '_', '-'): " + filename;
            }

#ifdef _WIN32
            HDC screen_dc = GetDC(nullptr);
            if (!screen_dc) return "ERRO: falha ao acessar a tela (GetDC)";

            int width  = GetSystemMetrics(SM_CXSCREEN);
            int height = GetSystemMetrics(SM_CYSCREEN);

            HDC mem_dc = CreateCompatibleDC(screen_dc);
            HBITMAP bitmap = CreateCompatibleBitmap(screen_dc, width, height);
            HGDIOBJ old_obj = SelectObject(mem_dc, bitmap);
            BitBlt(mem_dc, 0, 0, width, height, screen_dc, 0, 0, SRCCOPY);

            BITMAPINFOHEADER bi{};
            bi.biSize = sizeof(BITMAPINFOHEADER);
            bi.biWidth = width;
            bi.biHeight = -height; // top-down
            bi.biPlanes = 1;
            bi.biBitCount = 32;
            bi.biCompression = BI_RGB;

            cv::Mat frame(height, width, CV_8UC4);
            int rows = GetDIBits(mem_dc, bitmap, 0, height, frame.data,
                                 reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

            SelectObject(mem_dc, old_obj);
            DeleteObject(bitmap);
            DeleteDC(mem_dc);
            ReleaseDC(nullptr, screen_dc);

            if (rows == 0) return "ERRO: falha ao capturar pixels da tela (GetDIBits)";

            cv::Mat bgr;
            cv::cvtColor(frame, bgr, cv::COLOR_BGRA2BGR);
            if (!cv::imwrite(filename, bgr)) {
                return "ERRO: falha ao salvar imagem em " + filename;
            }
            return "Screenshot salvo em '" + filename + "' (" +
                   std::to_string(width) + "x" + std::to_string(height) + ")";
#else
            // Linux (Hyprland): delega ao grim, como o resto do pipeline de visão
            std::string cmd = "grim " + filename;
            if (std::system(cmd.c_str()) != 0) {
                return "ERRO: falha ao executar grim (instalado?)";
            }
            return "Screenshot salvo em '" + filename + "'";
#endif
        });

    // --- webcam_check / webcam_photo: presença e foto via webcam ---
    tool_executor->register_handler("webcam_check",
        [this](const std::map<std::string, std::string>&) -> std::string {
            if (!presence_detector) {
                return "ERRO: webcam/detecção facial indisponível neste sistema";
            }
            auto result = presence_detector->check();
            if (!result.available) {
                return "ERRO: " + result.error;
            }
            std::string desc;
            if (result.present) {
                desc = result.face_count == 1
                     ? "1 pessoa na frente do computador"
                     : std::to_string(result.face_count) + " pessoas na frente do computador";
            } else {
                desc = "ninguém visível na frente do computador";
            }
            desc += " (luminosidade do ambiente: " +
                    std::to_string(static_cast<int>(result.brightness)) + "/255" +
                    (result.brightness < 40 ? ", ambiente escuro" : "") + ")";
            return desc;
        });

    tool_executor->register_handler("webcam_photo",
        [this](const std::map<std::string, std::string>& args) -> std::string {
            if (!presence_detector) {
                return "ERRO: webcam indisponível neste sistema";
            }
            std::string filename = "webcam.png";
            auto it = args.find("filename");
            if (it != args.end() && !it->second.empty()) filename = it->second;
            if (!is_safe_filename(filename)) {
                return "ERRO: nome de arquivo inválido (use apenas letras, números, '.', '_', '-'): " + filename;
            }
            return presence_detector->capture_to_file(filename);
        });

    // --- controle de teclado/mouse: as "mãos" da Alyssa (companion mode).
    //     Remover as entradas do tools_registry.json desativa tudo. ---
    tool_executor->register_handler("screen_info",
        [](const std::map<std::string, std::string>&) {
            return alyssa_input::InputController::screen_info();
        });

    tool_executor->register_handler("mouse_move",
        [](const std::map<std::string, std::string>& args) -> std::string {
            int x = 0, y = 0;
            try {
                x = std::stoi(args.at("x"));
                y = std::stoi(args.at("y"));
            } catch (...) {
                return "ERRO: x e y precisam ser números inteiros (pixels da tela)";
            }
            return alyssa_input::InputController::move_mouse(x, y);
        });

    tool_executor->register_handler("mouse_click",
        [](const std::map<std::string, std::string>& args) -> std::string {
            std::string button = "left";
            auto it = args.find("button");
            if (it != args.end() && !it->second.empty()) button = it->second;
            return alyssa_input::InputController::click(button);
        });

    tool_executor->register_handler("keyboard_type",
        [](const std::map<std::string, std::string>& args) {
            return alyssa_input::InputController::type_text(args.at("text"));
        });

    tool_executor->register_handler("keyboard_key",
        [](const std::map<std::string, std::string>& args) {
            return alyssa_input::InputController::press_key(args.at("key"));
        });


        tool_executor->register_handler("doomscroll",
        [](const std::map<std::string, std::string>&) -> std::string {
            const char* sites[] = {
                "https://www.reddit.com/r/all/",
                "https://en.wikipedia.org/wiki/Special:Random",
                "https://www.youtube.com/",
                "https://www.instagram.com/reels/"
            };
            std::string url = sites[rand() % (sizeof(sites)/sizeof(sites[0]))];
            #ifdef _WIN32
                std::string cmd = "start \"\" \"" + url + "\"";
            #else
                std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
            #endif
            if (std::system(cmd.c_str()) != 0) {
                return "ERRO: falha ao abrir o navegador";
            }
            return "Aberto aleatoriamente: " + url;
        });

    

    // --- open_url: abre um site no navegador padrão (uso: lazer da Alyssa
    //     quando o usuário sai, ou pedido explícito) ---
    tool_executor->register_handler("open_url",
        [](const std::map<std::string, std::string>& args) -> std::string {
            const std::string& url = args.at("url");

            // Allowlist rígida: só http(s) e charset de URL sem metacaracteres
            // de shell (a string vai para std::system).
            if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
                return "ERRO: só URLs http:// ou https:// são permitidas";
            }
            if (url.size() > 300) return "ERRO: URL longa demais";
            // Charset conservador: sem %, !, aspas ou parênteses — a string
            // passa pelo cmd/shell e esses caracteres têm significado lá.
            for (char c : url) {
                bool ok = std::isalnum(static_cast<unsigned char>(c)) ||
                          std::string(":/.?&=_+#~-").find(c) != std::string::npos;
                if (!ok) return std::string("ERRO: caractere não permitido na URL: '") + c + "'";
            }

        #ifdef _WIN32
            std::string cmd = "start \"\" \"" + url + "\"";
        #elif __APPLE__
            std::string cmd = "open \"" + url + "\"";
        #else
            std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
        #endif
            if (std::system(cmd.c_str()) != 0) {
                return "ERRO: falha ao abrir o navegador";
            }
            return "Aberto no navegador: " + url;
        });

    // --- web_search: DuckDuckGo HTML + extração crua dos títulos/snippets ---
    tool_executor->register_handler("web_search",
        [](const std::map<std::string, std::string>& args) -> std::string {
            const std::string& query = args.at("query");

            CURL* curl = curl_easy_init();
            if (!curl) return "ERRO: falha ao inicializar cURL";

            char* escaped = curl_easy_escape(curl, query.c_str(), static_cast<int>(query.size()));
            std::string url = "https://html.duckduckgo.com/html/?q=" + std::string(escaped ? escaped : "");
            if (escaped) curl_free(escaped);

            std::string body;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_to_string);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (AlyssaNet)");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
            // Mesmo esquema do ElevenLabsTTS: verificação SSL desativada (PoC)
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                return std::string("ERRO: requisição falhou: ") + curl_easy_strerror(res);
            }

            // Extração crua: âncoras de resultado do DDG (class="result__a")
            std::string results;
            int count = 0;
            size_t pos = 0;
            while (count < 3 && (pos = body.find("result__a", pos)) != std::string::npos) {
                size_t title_start = body.find('>', pos);
                size_t title_end   = body.find("</a>", title_start);
                if (title_start == std::string::npos || title_end == std::string::npos) break;

                std::string title = strip_html(body.substr(title_start + 1, title_end - title_start - 1));

                std::string snippet;
                size_t snip_pos = body.find("result__snippet", title_end);
                if (snip_pos != std::string::npos && snip_pos < body.find("result__a", title_end)) {
                    size_t snip_start = body.find('>', snip_pos);
                    size_t snip_end   = body.find("</a>", snip_start);
                    if (snip_end == std::string::npos) snip_end = body.find("</td>", snip_start);
                    if (snip_start != std::string::npos && snip_end != std::string::npos) {
                        snippet = strip_html(body.substr(snip_start + 1, snip_end - snip_start - 1));
                        if (snippet.size() > 250) snippet = snippet.substr(0, 250) + "...";
                    }
                }

                if (!title.empty()) {
                    results += std::to_string(count + 1) + ". " + title + "\n";
                    if (!snippet.empty()) results += "   " + snippet + "\n";
                    ++count;
                }
                pos = title_end;
            }

            if (results.empty()) return "Nenhum resultado encontrado para: " + query;
            return results;
        });
    }

// =========================================================================
// Memory Compression & Resilience helpers (Phase 4)
// =========================================================================

void CoreIntegration::log_interaction_for_dataset(const std::string& input,
                                                  const std::string& response,
                                                  const std::string& mode) {
    try {
        nlohmann::json entry = {
            {"timestamp", std::time(nullptr)},
            {"input", input},
            {"response", response},
            {"mode", mode}
        };
        if (endocrine_system) {
            entry["emotional_state"] =
                endocrine_system->get_hormone_profile().get_emotional_state();
        }
        std::ofstream file("training_data.jsonl", std::ios::app);
        if (file.is_open()) {
            file << entry.dump() << "\n";
        }
    } catch (...) {
        // dataset é best-effort: nunca derruba um turno
    }
}

void CoreIntegration::maybe_reload_personality() {
    std::error_code ec;
    auto mtime = std::filesystem::last_write_time("config/personality.json", ec);
    if (ec) return; // arquivo sumiu/inacessível: mantém o perfil em memória

    if (mtime != personality_mtime) {
        std::cout << "[Personality] Hot-reload: personality.json mudou, recarregando..." << std::endl;
        auto reloaded = alyssa_personality::load_personality("config/personality.json");
        if (reloaded.loaded) {
            personality = reloaded;
        } // malformado: mantém o perfil anterior em vez de degradar
        personality_mtime = mtime;
    }
}

std::string CoreIntegration::summarize_history_chunk(const std::string& text) {
    alyssa_core::AlyssaCore* summarizer = !expert_context_pool.empty()
        ? expert_context_pool[0].get()
        : core_instance.get();
    if (!summarizer || text.empty()) return "";

    SimpleModelParameters params;
    params.temperature = 0.3;
    params.top_p = 0.8;
    params.max_tokens = 96;
    params.timeout_ms = 15000; // resumo não pode travar o turno

    std::string prompt =
        "Resuma a conversa abaixo em no máximo 3 frases, preservando fatos, nomes, "
        "preferências e decisões importantes. Responda APENAS com o resumo, sem introdução.\n\n" + text;

    // Pool: clear_kv direto. core_instance: clear_kv_cache() do orquestrador,
    // que também zera o tracking active_expert_in_cache.
    auto clean = [this, summarizer]() {
        if (!expert_context_pool.empty()) summarizer->clear_kv();
        else clear_kv_cache();
    };

    try {
        clean();
        std::string summary = summarizer->generate_raw(prompt, params, nullptr, nullptr);
        clean();

        summary.erase(0, summary.find_first_not_of(" \n\r\t"));
        summary.erase(summary.find_last_not_of(" \n\r\t") + 1);
        return summary;
    } catch (const std::exception& e) {
        std::cerr << "[Memory Cycle] Falha ao resumir histórico: " << e.what() << std::endl;
        return "";
    }
}

std::string CoreIntegration::generate_fallback_response(const std::string& prompt) {
    std::cout << "[Fallback] Resposta da Alyssa vazia/falhou. Tentando modelo base 1B..." << std::endl;

    alyssa_core::AlyssaCore* fallback_core = !expert_context_pool.empty()
        ? expert_context_pool[0].get()
        : core_instance.get();
    if (!fallback_core) {
        return "Desculpa, deu ruim aqui no meu processamento. Tenta de novo?";
    }

    SimpleModelParameters params;
    params.temperature = 0.7;
    params.top_p = 0.9;
    params.max_tokens = 160;
    params.timeout_ms = 15000;

    auto clean = [this, fallback_core]() {
        if (!expert_context_pool.empty()) fallback_core->clear_kv();
        else clear_kv_cache();
    };

    try {
        clean();
        std::string response = fallback_core->generate_raw(prompt, params, nullptr, nullptr);
        clean();

        response.erase(0, response.find_first_not_of(" \n\r\t"));
        response.erase(response.find_last_not_of(" \n\r\t") + 1);

        if (response.empty()) {
            return "Desculpa, deu ruim aqui no meu processamento. Tenta de novo?";
        }
        std::cout << "[Fallback] Modelo base 1B respondeu (evento logado para debug)." << std::endl;
        return response;
    } catch (const std::exception& e) {
        std::cerr << "[Fallback] Modelo base também falhou: " << e.what() << std::endl;
        return "Desculpa, deu ruim aqui no meu processamento. Tenta de novo?";
    }
}

std::string CoreIntegration::build_ambient_context() {
    std::ostringstream amb;

    // Relógio local: dia da semana + hora
    std::time_t now = std::time(nullptr);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    static const char* dias[] = {"domingo", "segunda", "terça", "quarta",
                                 "quinta", "sexta", "sábado"};
    char hhmm[6];
    std::snprintf(hhmm, sizeof(hhmm), "%02d:%02d", tm_buf.tm_hour, tm_buf.tm_min);
    amb << dias[tm_buf.tm_wday] << ", " << hhmm;

    // Janela em foco: o que o usuário está fazendo agora
#ifdef _WIN32
    if (HWND fg = GetForegroundWindow()) {
        wchar_t wtitle[256];
        int wlen = GetWindowTextW(fg, wtitle, 256);
        if (wlen > 0) {
            char title[512];
            int len = WideCharToMultiByte(CP_UTF8, 0, wtitle, wlen,
                                          title, sizeof(title) - 1, nullptr, nullptr);
            if (len > 0) {
                title[len] = '\0';
                amb << " · janela ativa: \"" << title << "\"";
            }
        }
    }
#endif

    // Presença: só o snapshot do stream — check() bloqueia 1-2s e não entra aqui
    if (presence_detector && presence_detector->is_streaming()) {
        auto p = presence_detector->latest();
        if (p.available) {
            amb << " · usuário " << (p.present ? "presente" : "ausente");
            if (p.brightness < 60.0) amb << " (sala escura)";
        }
    }

    // CPU/RAM: reader estático para o delta de CPU funcionar entre turnos.
    // Seguro: todos os chamadores rodam serializados sob brain_mtx.
    static PCMetricsReader ambient_metrics;
    float cpu = ambient_metrics.get_cpu_usage();
    float ram = ambient_metrics.get_ram_usage();
    if (cpu >= 0.0f && ram >= 0.0f) {
        amb << " · CPU " << static_cast<int>(cpu + 0.5f)
            << "% · RAM " << static_cast<int>(ram + 0.5f) << "%";
    }

    return "[AMBIENTE] " + amb.str() + "\n";
}

std::string CoreIntegration::generate_proactive_message(const std::string& reason) {
    if (!initialized || !core_instance) return "";

    maybe_reload_personality();

    std::cout << "\n[Proactivity] Gerando mensagem espontânea: "
              << (reason.length() > 60 ? reason.substr(0, 60) + "..." : reason) << std::endl;

    // Metabolismo leve: tempo passou desde a última interação
    if (endocrine_system) {
        endocrine_system->apply_metabolism(0.02);
    }

    std::string personality_context = alyssa_personality::generate_personality_context(
        personality,
        endocrine_system ? &endocrine_system->get_hormone_profile() : nullptr);

    std::string hormonal_context = "";
    if (endocrine_system) {
        hormonal_context = endocrine_system->generate_hormonal_system_context() + "\n";
    }

    std::string tools_context = tool_executor ? tool_executor->get_tools_prompt() : "";

    // Gostos aprendidos: o lazer e as iniciativas dela consideram o que ela
    // já sabe sobre o usuário ("abrir algo que ela aprendeu que eu gosto")
    std::string prefs_line = alyssa_prefs::render_preferences_line();
    if (!prefs_line.empty()) prefs_line = "[MEMÓRIA DE GOSTOS] " + prefs_line + "\n\n";

    std::string prompt = personality_context + hormonal_context + build_ambient_context() +
        tools_context + prefs_line +
        "[INICIATIVA PRÓPRIA] " + reason + "\n\n"
        "Escreva UMA mensagem curta e espontânea, como quem manda mensagem no chat "
        "sem ter sido chamada. Não cumprimente como se fosse a primeira conversa do dia "
        "e não faça referência a estas instruções.";

    if (reason.find("explorar") != std::string::npos ||
        reason.find("ausente") != std::string::npos ||
        reason.find("distrair") != std::string::npos) {
        prompt += "\nVocê está entediada/curiosa - sinta-se à vontade para usar ferramentas (como abrir um site ou buscar algo) como parte da sua iniciativa. Use [TOOL_CALL] se quiser fazer algo externo antes de responder.";
    }

    std::string proactive_entry = "[iniciativa própria] " + reason;
    std::string response = run_expert("alyssa", prompt, false, nullptr, &proactive_entry);
    response = resolve_tool_calls(response, false, nullptr);

    // Mesmo strip de [RESPOSTA] do fluxo principal
    size_t s = response.find("[RESPOSTA]");
    size_t e = response.find("[/RESPOSTA]");
    if (s != std::string::npos && e != std::string::npos) {
        response = response.substr(s + 10, e - s - 10);
        response.erase(0, response.find_first_not_of(" \n\r\t"));
        response.erase(response.find_last_not_of(" \n\r\t") + 1);
    }

    clear_kv_cache();
    return response;
}

std::string CoreIntegration::resolve_tool_calls(std::string response, bool use_tts, ITTS* tts) {
    if (!tool_executor) return response;

    const int max_rounds = tool_executor->max_rounds();
    const int max_calls  = tool_executor->max_calls_per_round();

    for (int round = 0; round < max_rounds; ++round) {
        auto calls = alyssa_tools::ToolExecutor::parse_tool_calls(response);
        if (calls.empty()) break;

        if (static_cast<int>(calls.size()) > max_calls) {
            std::cout << "[Tools] " << calls.size() << " chamadas no turno; limitando a "
                      << max_calls << std::endl;
            calls.resize(max_calls);
        }

        bool any_failed = false;
        std::string results_block = "[RESULTADOS_FERRAMENTAS]\n";
        for (const auto& call : calls) {
            std::cout << "[Tools] Alyssa chamou: " << call.name << std::endl;
            alyssa_tools::ToolResult result = tool_executor->execute(call);
            if (!result.success) any_failed = true;
            results_block += "- " + call.name + ": " + result.output + "\n";
        }
        results_block += "[/RESULTADOS_FERRAMENTAS]\n\n";

        // Falha com dica no erro → instrução explícita de retry corrigido.
        // Modelo pequeno não infere sozinho que pode tentar de novo.
        std::string followup;
        if (any_failed && round + 1 < max_rounds) {
            followup = results_block +
                "Uma ferramenta FALHOU. Leia a dica na mensagem de erro, corrija os parâmetros "
                "e chame de novo AGORA com [TOOL_CALL] nome(param=valor_corrigido) [/TOOL_CALL]. "
                "Escreva APENAS o bloco corrigido. Se não der pra corrigir, explique em uma frase.";
        } else {
            followup = results_block +
                "Responda ao usuário como Alyssa usando os DADOS CONCRETOS dos resultados "
                "(nomes de arquivos, números, valores) — nunca diga vagamente que 'tem um monte de coisa'. "
                "Se a lista for longa, cite uns 5-8 itens interessantes e o total. "
                "Não mencione o formato interno das ferramentas. "
                "Só use [TOOL_CALL] novamente se realmente precisar de outra ferramenta.";
        }

        // No histórico fica só o bloco de resultados (dados que ela citou),
        // não a instrução boilerplate de como responder.
        response = run_expert("alyssa", followup, use_tts, tts, &results_block);
    }

    // Se depois da última rodada ainda sobrou tool call, remove do texto final
    return alyssa_tools::ToolExecutor::strip_tool_calls(response);
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

    // ANTES do strip de pontuação: o indicador "?" abaixo nunca casava
    // porque a interrogação já tinha sido removida — "bom dia, dormiu bem?"
    // caía no fast path e ganhava resposta genérica que ignorava a pergunta.
    const bool has_question_mark = lower_input.find('?') != std::string::npos;

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
    
    // Verificar se tem conteúdo semântico. Cumprimento puro com "?" já
    // retornou true lá em cima ("tudo bem?"); chegar aqui com pergunta
    // significa pergunta de verdade — merece o comitê.
    std::vector<std::string> content_indicators = {
        "porque", "como", "quando", "onde", "por que",
        "explica", "ajuda", "preciso", "problema", "questão"
    };

    bool has_content = has_question_mark;
    for (const auto& indicator : content_indicators) {
        if (has_content) break;
        if (lower_input.find(indicator) != std::string::npos) {
            has_content = true;
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
    ITTS* tts,
    const std::string* history_user_text
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
        // Alyssa tem seu próprio core: limpa o KV DELE a cada turno. O run()
        // re-prefila o prompt completo (system + histórico + input) sempre,
        // então KV do turno anterior é puro veneno: posições continuam de
        // onde pararam, o contexto acumula prompts duplicados (respostas
        // fora de persona) até estourar o n_ctx — "Falha ao decodificar
        // prompt chunk" permanente a partir daí. Mesmo contrato do
        // run_gameplay_tick e dos slots do pool (clear_kv antes do decode).
        std::cout << "\n[Orquestrador]: Preparando contexto para Alyssa\n";
        experts[expert_id]->clear_own_kv_cache();
    }

    auto& expert = experts[expert_id];
    auto& history = expert_histories[expert_id];

    // Configurar callback de streaming: TTS (fatia por sentença) e/ou sink
    // externo (evento `token` do alyssad). O sink externo só recebe a fala
    // da própria Alyssa — o comitê não passa por run_expert.
    std::function<void(const std::string&)> stream_callback = nullptr;
    std::string sentence_buffer;

    const bool stream_tts      = use_tts && tts;
    const bool stream_external = on_response_chunk && expert_id == "alyssa";

    if (stream_tts || stream_external) {
        stream_callback = [&](const std::string& piece) {
            if (stream_external) on_response_chunk(piece);
            if (!stream_tts) return;

            printf("%s", piece.c_str());
            fflush(stdout);

            sentence_buffer += piece;

            // Fatia por sentença COMPLETA: pontuação final seguida de espaço.
            // Cortar em qualquer '.' picotava a fala — "..." virava três
            // fronteiras e sobravam fragmentos de uma palavra ("Hotel", "é")
            // falados isolados. Regras:
            //   - fronteira = [.!?] com whitespace logo depois (o próximo
            //     token de um "..." ainda pode ser outro ponto);
            //   - sentença mínima de 12 bytes: fragmento curto ("É.") junta
            //     com a próxima em vez de virar um utterance próprio.
            constexpr size_t MIN_SENTENCE_BYTES = 12;
            for (;;) {
                size_t cut = std::string::npos;
                for (size_t i = 0; i + 1 < sentence_buffer.size(); ++i) {
                    const char c = sentence_buffer[i];
                    if ((c == '.' || c == '!' || c == '?') &&
                        std::isspace(static_cast<unsigned char>(sentence_buffer[i + 1])) &&
                        i + 1 >= MIN_SENTENCE_BYTES) {
                        cut = i;
                        break;
                    }
                }
                if (cut == std::string::npos) break;

                std::string sentence = sentence_buffer.substr(0, cut + 1);
                sentence_buffer.erase(0, cut + 1);
                // Trim de espaços e de pontuação órfã herdada da sentença
                // anterior (restos de "..." etc.).
                sentence.erase(0, sentence.find_first_not_of(" \t\n\r.!?"));
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

    // A 4B às vezes imita as anotações internas do prompt e fecha a resposta
    // com "(Emoção: sarcasmo)". É andaime vazando: some daqui pra não ir pra
    // tela, pro histórico nem pra memória SQLite (que re-alimentaria o vício).
    // O TTS pode já ter falado a sentença — a contenção de voz é a regra na
    // persona; isto aqui é a rede de segurança do texto.
    if (expert_id == "alyssa") {
        static const std::regex emotion_note("\\s*\\(\\s*Emoção\\s*:[^)]*\\)\\s*\\.?");
        response = std::regex_replace(response, emotion_note, "");
    }

    // Emagrece a entrada de histórico: o run() arquivou o `input` inteiro
    // (andaime de personalidade/pensamentos/tools) como mensagem do usuário;
    // fica só a fala real. Layout garantido pelo run(): [-2]=user, [-1]=assistant.
    if (history_user_text && history.size() >= 2) {
        auto& user_msg = history[history.size() - 2];
        if (user_msg.role && std::string(user_msg.role) == "user") {
            free(const_cast<char*>(user_msg.content));
            user_msg.content = strdup(history_user_text->c_str());
        }
    }

    // Gerenciar histórico
    manage_dynamic_history(expert_id, history);

    return response;
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
    
    // Sort by weight descending so the most relevant thought lands first in the prompt
    std::vector<alyssa_fusion::ExpertContribution> sorted_contributions = contributions;
    std::sort(sorted_contributions.begin(), sorted_contributions.end(),
              [](const auto& a, const auto& b) { return a.weight > b.weight; });

    for (const auto& contrib : sorted_contributions) {
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
    
    // =====================================================================
    // 🧬 INJECT HORMONAL STATE INTO SYSTEM CONTEXT
    // =====================================================================
    
    std::string hormonal_context = "";
    if (endocrine_system) {
        hormonal_context = endocrine_system->generate_hormonal_system_context();
        hormonal_context += "\n";
        
        std::cout << "\n[Hormonal Injection] " << hormonal_context << std::endl;
    }
    
    // Bloco compacto de ferramentas disponíveis (Fase 1: descrições curtas,
    // não o registro inteiro, para não explodir o contexto)
    std::string tools_context = "";
    if (tool_executor) {
        tools_context = tool_executor->get_tools_prompt();
        if (!tools_context.empty()) tools_context += "\n";
    }

    // Bloco de personalidade com estado atual modulado pelos hormônios (Fase 2.1)
    std::string personality_context = alyssa_personality::generate_personality_context(
        personality,
        endocrine_system ? &endocrine_system->get_hormone_profile() : nullptr);
    if (!personality_context.empty()) personality_context += "\n";

    // Construir prompt final para a Alyssa. Tools ficam por último antes da
    // entrada: modelos pequenos dão mais atenção ao fim do prompt.
    std::string fused_prompt = personality_context +
                               hormonal_context +
                               build_ambient_context() +
                               thoughts +
                               tools_context +
                               "ENTRADA DO USUÁRIO: \"" + original_input + "\"\n\n" +
                               "Baseado nos pensamentos acima e seu estado hormonal atual, forneça sua resposta como Alyssa:";

    return fused_prompt;
}

// =========================================================================
// Core Fusion Engine (shared by both TTS and TTS-less paths)
// =========================================================================

/**
 * @brief Shared implementation for all think_with_fusion variants.
 *
 * Runs rule-based Top-K gating, executes only the selected expert subset,
 * builds the fused prompt, calls Alyssa for the final answer, and handles
 * endocrine updates and memory storage.  TTS streaming is enabled when
 * use_tts=true and tts != nullptr.
 */
std::string CoreIntegration::think_with_fusion_core(
    const std::string& input,
    bool use_tts,
    ITTS* tts
) {
    if (!initialized || !core_instance || !fusion_engine) {
        return "Erro: Sistema não inicializado corretamente.";
    }

    // Hot-reload de personalidade: tuning sem reiniciar
    maybe_reload_personality();

    // =========================================================================
    // 1. ENDOCRINE: metabolism tick
    // =========================================================================
    if (endocrine_system) {
        endocrine_system->apply_metabolism(0.05);
        std::cout << endocrine_system->get_hormone_profile().to_string() << std::endl;
    }

    std::cout << "\n[Weighted Fusion] Processando input: " << input << std::endl;

    // =========================================================================
    // 1.5. FAST PATH: small talk pula o comitê inteiro (latência ~1 chamada 4B)
    // =========================================================================
    if (is_small_talk(input)) {
        std::cout << "[Fast Path] Small talk detectado — comitê pulado.\n";
        auto fast_start = std::chrono::steady_clock::now();

        std::string fast_personality = alyssa_personality::generate_personality_context(
            personality,
            endocrine_system ? &endocrine_system->get_hormone_profile() : nullptr);
        std::string fast_tools = tool_executor ? tool_executor->get_tools_prompt() : "";

        std::string fast_prompt = fast_personality + fast_tools + build_ambient_context() +
            "[CONVERSA CASUAL] Responda curto e natural, como Alyssa: " + input;

        std::string fast_resp = run_expert("alyssa", fast_prompt, use_tts, tts, &input);
        fast_resp = resolve_tool_calls(fast_resp, use_tts, tts);

        auto fast_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - fast_start).count();
        std::cout << "[Fast Path] Resposta em " << fast_ms << "ms\n";
        printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", fast_resp.c_str());

        log_interaction_for_dataset(input, fast_resp, "small_talk");
        clear_kv_cache();
        return fast_resp;
    }

    // =========================================================================
    // 2. MEMORY CONTEXT AUGMENTATION (isolated, max 3 memories)
    // =========================================================================
    std::string augmented_input = input;
    AllModelConfigs configs = load_config();

    if (memory_manager) {
        auto memories = memory_manager->getHybridMemories(input);
        std::vector<typename decltype(memories)::value_type> filtered;
        for (size_t i = 0; i < memories.size() && filtered.size() < 3; ++i) {
            const auto& m = memories[i];
            bool relevant = (m.content.find("?") != std::string::npos || m.content.length() > 20)
                         && (m.emotion.find("erro") == std::string::npos);
            if (relevant) filtered.push_back(m);
        }
        if (!filtered.empty()) {
            std::string mem_ctx = "\n[CONTEXTO DE MEMÓRIA ANTERIOR - ISOLADO PARA ESTE TURNO]\n";
            for (const auto& m : filtered) {
                std::string c = m.content.length() > 150 ? m.content.substr(0, 150) + "..." : m.content;
                mem_ctx += "- " + c + "\n";
            }
            mem_ctx += "[FIM CONTEXTO ISOLADO]\n";
            augmented_input = mem_ctx + input;
            std::cout << "[Memory Context] Injetando " << filtered.size() << " memória(s)" << std::endl;
        }
    }

    // =========================================================================
    // 2.5 ROUTER ADAPTATIVO (docs/plano-router-e-voz-gameplay.md, seção A):
    // um pre-pass de ~100ms no 1B decide se o turno paga o comitê (~1s+).
    // "direto"/"memoria" respondem já; rota de expert único pula o gating.
    // =========================================================================
    std::string forced_expert;
    if (router_adaptive) {
        const std::string route = run_router_prepass(input);
        if (route == "direto") {
            return generate_direct_response(input, input, use_tts, tts);
        } else if (route == "memoria") {
            // As memórias relevantes já foram injetadas na seção 2.
            return generate_direct_response(augmented_input, input, use_tts, tts);
        } else if (route == "emocional") {
            forced_expert = "emotionalModel";
        } else if (route == "analitico") {
            forced_expert = "analyticalModel";
        } else if (route == "criativo") {
            forced_expert = "creativeModel";
        }
        // "comite" (ou rota desconhecida): fluxo completo abaixo.
    }

    // =========================================================================
    // 3. RULE-BASED GATING + Top-K selection (k=3, threshold=0.15)
    // =========================================================================
    std::vector<std::string> available_experts;
    for (const auto& cfg : configs) {
        if (cfg.id != "alyssa") available_experts.push_back(cfg.id);
    }

    constexpr int    TOP_K     = 3;
    constexpr double THRESHOLD = 0.15;

    std::map<std::string, double> gating_weights;
    std::set<std::string> active_experts;

    if (!forced_expert.empty() && has_expert(forced_expert)) {
        // Router escolheu o especialista: comitê de UM, peso cheio.
        active_experts.insert(forced_expert);
        gating_weights[forced_expert] = 1.0;
        std::cout << "[Router] Expert único pelo router: " << forced_expert << "\n";
    } else {
        gating_weights =
            fusion_engine->calculate_rule_based_weights(augmented_input, available_experts);

        std::vector<std::pair<std::string, double>> sorted_w(gating_weights.begin(), gating_weights.end());
        std::sort(sorted_w.begin(), sorted_w.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        for (int i = 0; i < static_cast<int>(sorted_w.size()); ++i) {
            if (i < TOP_K && sorted_w[i].second >= THRESHOLD) {
                active_experts.insert(sorted_w[i].first);
                std::cout << "[MoE Gating] Expert ATIVADO: " << sorted_w[i].first
                          << " (Peso: " << sorted_w[i].second << ")\n";
            } else {
                gating_weights[sorted_w[i].first] = 0.0;
            }
        }
    }

    // =========================================================================
    // 4. CONDITIONAL EXPERT EXECUTION (only the selected subset)
    //    Fase 3.2: paralelo via pool de contextos; sequencial como fallback.
    // =========================================================================
    std::vector<alyssa_fusion::ExpertContribution> contributions;

    // Lista de selecionados na ordem das configs (determinística)
    std::vector<const SimpleModelConfig*> selected;
    for (const auto& cfg : configs) {
        if (cfg.id == "alyssa") continue;
        if (active_experts.find(cfg.id) != active_experts.end()) selected.push_back(&cfg);
    }

    auto committee_start = std::chrono::steady_clock::now();
    const bool run_parallel = expert_context_pool.size() >= 2 && selected.size() >= 2;

    if (run_parallel) {
        // Processa em lotes do tamanho do pool: cada task usa um contexto
        // exclusivo do lote, então não há dois decodes no mesmo contexto.
        const size_t pool_size = expert_context_pool.size();

        for (size_t base = 0; base < selected.size(); base += pool_size) {
            size_t batch_n = std::min(pool_size, selected.size() - base);
            std::vector<std::future<alyssa_fusion::ExpertContribution>> futures;

            for (size_t i = 0; i < batch_n; ++i) {
                const SimpleModelConfig* cfg = selected[base + i];
                alyssa_core::AlyssaCore* slot_core = expert_context_pool[i].get();
                alyssa_experts::IExpert* expert_ptr = experts[cfg->id].get();
                double weight = gating_weights[cfg->id];

                // Cópia isolada do histórico feita na thread principal (o mapa
                // expert_histories não é tocado pelas workers)
                std::vector<llama_chat_message> isolated_history = expert_histories[cfg->id];
                if (isolated_history.size() > 4) {
                    isolated_history.erase(isolated_history.begin(),
                                           isolated_history.begin() + (isolated_history.size() - 4));
                }

                std::cout << "[MoE Execution] Rodando " << cfg->id << " (paralelo, slot "
                          << i << ")...\n";

                futures.push_back(std::async(std::launch::async,
                    [this, expert_ptr, slot_core, weight,
                     isolated_history, augmented_input]() mutable {
                        slot_core->clear_kv(); // contexto reutilizado entre turnos

                        size_t base_size = isolated_history.size();
                        llama_adapter_lora* active_lora = nullptr;
                        auto contrib = expert_ptr->get_contribution(
                            augmented_input, slot_core, embedder, nullptr,
                            isolated_history, &active_lora
                        );
                        contrib.weight = weight;

                        // Libera as mensagens strdup'adas que o run() anexou à cópia
                        for (size_t k = base_size; k < isolated_history.size(); ++k) {
                            free(const_cast<char*>(isolated_history[k].content));
                        }
                        return contrib;
                    }));
            }

            for (auto& f : futures) {
                auto contrib = f.get();
                std::cout << "[Comitê] " << contrib.expert_id << " respondeu: "
                          << (contrib.response.length() > 50
                              ? contrib.response.substr(0, 50) + "..."
                              : contrib.response) << std::endl;
                contributions.push_back(std::move(contrib));
            }
        }
    } else {
        // Caminho sequencial original (pool indisponível ou 1 expert só)
        for (const SimpleModelConfig* cfg_ptr : selected) {
            const auto& cfg = *cfg_ptr;
            std::cout << "[MoE Execution] Rodando " << cfg.id << "...\n";
            switch_expert_context(cfg.id);

            auto& expert  = experts[cfg.id];
            auto& history = expert_histories[cfg.id];

            std::vector<llama_chat_message> isolated_history = history;
            if (isolated_history.size() > 4) {
                isolated_history.erase(isolated_history.begin(),
                                       isolated_history.begin() + (isolated_history.size() - 4));
            }

            llama_adapter_lora* active_lora = nullptr;
            auto contrib = expert->get_contribution(
                augmented_input, core_instance.get(), embedder, nullptr,
                isolated_history, &active_lora
            );
            contrib.weight = gating_weights[cfg.id];
            contributions.push_back(contrib);

            std::cout << "[Comitê] " << cfg.id << " respondeu: "
                      << (contrib.response.length() > 50
                          ? contrib.response.substr(0, 50) + "..."
                          : contrib.response) << std::endl;
        }
    }

    auto committee_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - committee_start).count();
    std::cout << "[MoE Execution] Comitê de " << selected.size() << " expert(s) concluído em "
              << committee_ms << "ms (" << (run_parallel ? "paralelo" : "sequencial")
              << ")" << std::endl;

    // Fallback: no expert cleared the gate
    if (contributions.empty()) {
        std::cout << "[MoE Gating] Nenhum expert atingiu o threshold. Fallback: socialModel\n";
        if (has_expert("socialModel")) {
            switch_expert_context("socialModel");
            auto& expert  = experts["socialModel"];
            auto& history = expert_histories["socialModel"];
            llama_adapter_lora* al = nullptr;
            auto contrib = expert->get_contribution(
                augmented_input, core_instance.get(), embedder, nullptr, history, &al);
            contrib.weight = 1.0;
            contributions.push_back(contrib);
        }
    }

    // =========================================================================
    // 5. ENDOCRINE: update from expert signals
    // =========================================================================
    if (endocrine_system && !contributions.empty()) {
        std::vector<std::string> signals;
        for (const auto& c : contributions) signals.push_back(c.response);
        endocrine_system->update_hormone_levels(signals);
        std::cout << "\n[Endocrine Update] Hormônios atualizados após comitê:\n"
                  << endocrine_system->get_hormone_profile().to_string() << std::endl;
    }

    // =========================================================================
    // 6. COHERENCE CHECK
    // =========================================================================
    float coherence = calculate_committee_coherence(contributions);
    if (coherence < 0.3f) {
        std::cout << "[AVISO] Coerência do comitê baixa (" << coherence
                  << "). Gerando resposta direta." << std::endl;
        return generate_direct_response(input, input, use_tts, tts);
    }

    // =========================================================================
    // 7. GENERATE FUSED PROMPT & CALL ALYSSA
    // =========================================================================
    std::string emotion     = detect_emotion_with_heuristics(input);
    std::string fused_input = generate_fused_input(input, contributions, emotion);
    std::string final_response = run_expert("alyssa", fused_input, use_tts, tts, &input);

    // Fase 4.3: resposta vazia/erro do 4B → última cartada com o modelo base 1B
    std::string trimmed = final_response;
    trimmed.erase(0, trimmed.find_first_not_of(" \n\r\t"));
    if (trimmed.empty() || trimmed.rfind("Erro:", 0) == 0) {
        final_response = generate_fallback_response(fused_input);
    }

    // Resolver [TOOL_CALL] antes de qualquer pós-processamento (Fase 1.2)
    final_response = resolve_tool_calls(final_response, use_tts, tts);

    printf("\033[36m[RESPOSTA FINAL]: \033[0m%s\n", final_response.c_str());

    // Strip optional [RESPOSTA] tags
    size_t s = final_response.find("[RESPOSTA]");
    size_t e = final_response.find("[/RESPOSTA]");
    if (s != std::string::npos && e != std::string::npos) {
        final_response = final_response.substr(s + 10, e - s - 10);
        final_response.erase(0, final_response.find_first_not_of(" \n\r\t"));
        final_response.erase(final_response.find_last_not_of(" \n\r\t") + 1);
    }

    // =========================================================================
    // 8. MEMORY & CLEANUP
    // =========================================================================
    if (memory_manager) {
        if (should_store_in_memory(input, final_response)) {
            memory_manager->processInteraction(input, final_response);
            std::cout << "\n Interação salva na LTM." << std::endl;
        } else {
            std::cout << "\n Small talk/ruído não salvo na LTM." << std::endl;
        }
    }

    if (endocrine_system) {
        std::cout << "\n[Turn End] Limpando caches para próximo turno.\n"
                  << "  Estado final: "
                  << endocrine_system->get_hormone_profile().get_emotional_state() << std::endl;
    }
    log_interaction_for_dataset(input, final_response, "fusion");
    clear_kv_cache();
    return final_response;
}

/**
 * @brief TTS-enabled wrapper over think_with_fusion_core.
 */
std::string CoreIntegration::think_with_fusion(const std::string& input, ITTS& tts)
{
    return think_with_fusion_core(input, true, &tts);
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
 * @return Always true — see comment below for why the filter was disabled.
 */
bool CoreIntegration::should_store_in_memory(const std::string&, const std::string&) {
    // Disabled per user request 2026-07-12. Root cause of "important stuff
    // getting filtered out": criterion 3 below (Jaccard word-overlap
    // similarity against the last 10 interactions) was meant to catch
    // literal repeats, but MinecraftSession::handle_chat_events wraps every
    // relayed player message in the same ~300-char grounding boilerplate
    // ("Isso é só um pedido que você está repassando pro seu 'corpo'...").
    // Since that shared boilerplate dominates the word set, distinct player
    // instructions (including an actual crafting-recipe hint) scored >0.8
    // similarity against each other and got silently dropped as noise —
    // observed live losing real content, not small talk. Rather than
    // special-case the Minecraft path, storing everything is simpler and
    // was the explicit call made here.
    return true;
};

/**
 * @brief TTS-less wrapper over think_with_fusion_core.
 */
std::string CoreIntegration::think_with_fusion_ttsless(const std::string& input)
{
    return think_with_fusion_core(input, false, nullptr);
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

    // Fase 4.2: resumo rolante. Se já existe um [RESUMO] na frente do
    // histórico (de compressões anteriores), ele entra no novo resumo em vez
    // de ser re-arquivado — um único bloco compacto carrega toda a história.
    static const char* SUMMARY_PREFIX = "[RESUMO DA CONVERSA ANTERIOR]";
    std::string previous_summary = "";
    if (!history.empty() &&
        std::string(history.front().content).rfind(SUMMARY_PREFIX, 0) == 0) {
        previous_summary = history.front().content;
        free(const_cast<char*>(history.front().content));
        history.erase(history.begin());
    }

    int messages_to_archive = 8;
    std::string archived_content = "";

    for (int i = 0; i < messages_to_archive && !history.empty(); ++i) {
        const auto& msg = history.front();
        archived_content  += "[" + std::string(msg.role) + "]: " + msg.content + "\n";
        free(const_cast<char*>(msg.content));
        history.erase(history.begin());
    }

    // 1. Arquiva o conteúdo bruto na LTM (comportamento original)
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
        std::cout << "[LTM] Memória bruta arquivada (ID: " << mem_id << ")\n";
    }

    // 2. Gera o resumo rolante (resumo anterior + chunk arquivado) e reinjeta
    //    no início do histórico — o contexto da conversa sobrevive à compressão.
    std::string to_summarize = previous_summary.empty()
        ? archived_content
        : previous_summary + "\n" + archived_content;

    std::string summary = summarize_history_chunk(to_summarize);
    if (!summary.empty()) {
        std::string summary_msg = std::string(SUMMARY_PREFIX) + " " + summary;
        // Role "user" por compatibilidade: templates como o do Gemma não
        // aceitam "system" no meio da conversa.
        history.insert(history.begin(), {"user", strdup(summary_msg.c_str())});

        if (memory_manager) {
            memory_manager->storeMemoryWithEmotionalAnalysis(
                summary, "conversation_summary | expert:" + expert_id);
        }
        std::cout << "[Memory Cycle] Resumo rolante atualizado ("
                  << summary.length() << " chars): "
                  << (summary.length() > 60 ? summary.substr(0, 60) + "..." : summary) << "\n";
    } else if (!previous_summary.empty()) {
        // Resumo falhou: devolve o resumo antigo em vez de perder tudo
        history.insert(history.begin(), {"user", strdup(previous_summary.c_str())});
        std::cout << "[Memory Cycle] Resumo novo falhou; mantendo resumo anterior.\n";
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
        think_with_fusion_ttsless(user_input);
    }
}

void CoreIntegration::start_vision_pipeline() {
    if (!vision_manager_) {
        // UM dono da webcam por vez: o PresenceDetector abre a câmera 0 no
        // stream always-on dos CLIs — abrir uma SEGUNDA captura no mesmo
        // device congela/derruba o driver no Windows (crash de 2026-07-12).
        // O VisionManager assume a câmera; presença degrada pro que o
        // snapshot fornece (face_detected) enquanto a visão estiver ligada.
        if (presence_detector && presence_detector->is_streaming()) {
            std::cout << "[Vision] Parando o stream de presença — o VisionManager "
                         "assume a câmera." << std::endl;
            presence_detector->stop_stream();
        }

        vision_manager_ = std::make_unique<alyssa_vision::VisionManager>(0);
        // Set embedder for face recognition
        if (auto* fr = vision_manager_->get_face_recognizer()) {
            fr->set_embedder(embedder);
        }
        // Set callback for snapshots
        vision_manager_->start([this](const alyssa_vision::VisionSnapshot& snap) {
            this->on_vision_snapshot(snap);
        });
    }
}

bool CoreIntegration::vision_pipeline_running() const {
    return vision_manager_ && vision_manager_->is_running();
}

void CoreIntegration::stop_vision_pipeline() {
    if (vision_manager_) {
        vision_manager_->stop();
    }
}

const alyssa_vision::VisionSnapshot& CoreIntegration::get_latest_vision() const {
    std::lock_guard<std::mutex> lock(vision_mtx_);
    return latest_vision_;
}

void CoreIntegration::on_vision_snapshot(const alyssa_vision::VisionSnapshot& snap) {
    // Update stored snapshot
    {
        std::lock_guard<std::mutex> lock(vision_mtx_);
        latest_vision_ = snap;
    }

    // Update endocrine system based on visual cues
    if (endocrine_system) {
        endocrine_system->update_from_vision(snap);
    }

    // Feed to proactivity engine (if we have access to it)
    // We need to store a reference or callback; we'll add a method in CoreIntegration to set proactivity engine.
    // For simplicity, we'll expose a callback that can be set by the UI.
    // We'll implement a member std::function<void(const VisionSnapshot&)> on_vision_callback_;
    if (on_vision_callback_) {
        on_vision_callback_(snap);
    }
}

void CoreIntegration::set_vision_callback(std::function<void(const alyssa_vision::VisionSnapshot&)> cb) {
    on_vision_callback_ = std::move(cb);
}
