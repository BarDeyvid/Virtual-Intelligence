#include "AlyssaCore.hpp"
#include "WeightedFusion/WeightedFusion.hpp"
#include "voice/TTSBase.hpp"
#include "AlyssaMemoryHandler.hpp"
#include "IExpert.hpp"
#include "EndocrineSystem.hpp"
#include "ToolExecutor.hpp"
#include "PersonalityCore.hpp"
#include "PresenceDetector.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <map>
#include <vector>
#include <unordered_map>
#include "vision/VisionManager.hpp"
#include "vision/VisionSnapshot.hpp"

/**
 * @class CoreIntegration
 * @brief Main orchestration class for the Alyssa AI system using Mixture of Experts (MoE) architecture.
 * 
 * This class integrates multiple specialized AI models (experts) with weighted fusion capabilities,
 * memory management, and text-to-speech functionality. It serves as the central controller for
 * processing user inputs through different expert models and generating coherent responses.
 * 
 * Key features:
 * - Mixture of Experts (MoE) architecture with specialized models
 * - Weighted fusion of expert contributions
 * - Long-term memory integration
 * - Context switching between experts
 * - TTS (Text-to-Speech) integration
 * - Dynamic history management
 */
class CoreIntegration {
private:
    std::unique_ptr<alyssa_core::AlyssaCore> core_instance;           ///< Base model instance (1B)

    /// @brief Pool of extra contexts on the shared 1B model for parallel expert
    ///        execution (Phase 3.2). Declared AFTER core_instance on purpose:
    ///        members are destroyed in reverse order, so the pool contexts are
    ///        freed before the model they borrow from.
    std::vector<std::unique_ptr<alyssa_core::AlyssaCore>> expert_context_pool;

    /// Config do gameplayModel guardada no boot; o load real (E2B, ~3GB de
    /// VRAM) só acontece em ensure_gameplay_expert() no primeiro /mc start.
    std::unique_ptr<SimpleModelConfig> deferred_gameplay_config;
    int gameplay_fallback_n_batch = 512; ///< n_batch do 1B p/ fallback do gameplay

    // --- Router adaptativo (docs/plano-router-e-voz-gameplay.md, seção A) ---
    bool router_adaptive = false;   ///< "router_mode":"adaptive" no ConfigsLLM.json
    std::string router_grammar;     ///< config/grammars/router.gbnf carregada no boot

    /**
     * @brief Pre-pass de roteamento: classifica o input numa rota de processamento.
     * @details Roda no 1B compartilhado (core_instance) com grammar — ~100ms,
     *          sem tocar no core/histórico da Alyssa. v1 deliberadamente no
     *          1B: se logs/router_decisions.jsonl mostrar roteamento ruim,
     *          upgrade pro E2B é trocar o core da chamada.
     * @return "direto" | "emocional" | "analitico" | "criativo" | "memoria"
     *         | "comite" (fallback seguro em qualquer erro).
     */
    std::string run_router_prepass(const std::string& input);

    /**
     * @brief Resposta em MODO DIRETO: personalidade+tools+input, sem comitê.
     * @details Compartilhado entre o router (rota direto/memoria) e o
     *          fallback de coerência baixa do comitê (que já existia).
     */
    std::string generate_direct_response(const std::string& respond_to,
                                         const std::string& raw_input,
                                         bool use_tts, ITTS* tts);

    std::unique_ptr<alyssa_fusion::WeightedFusion> fusion_engine;    ///< Weighted fusion engine
    std::shared_ptr<Embedder> embedder;                             ///< Embedding generator for semantic analysis
    std::unique_ptr<alyssa_memory::AlyssaMemoryManager> memory_manager; ///< Long-term memory manager
    std::unique_ptr<alyssa_endocrine::EndocrineSystem> endocrine_system; ///< Hormonal system for behavioral modulation
    std::unique_ptr<alyssa_tools::ToolExecutor> tool_executor;       ///< Registry-driven tool system (Phase 1)
    alyssa_personality::Personality personality;                     ///< Static personality profile (Phase 2.1)
    std::filesystem::file_time_type personality_mtime{};             ///< For hot-reload of personality.json
    std::unique_ptr<alyssa_vision::PresenceDetector> presence_detector; ///< Webcam presence (night-shift)

    /// @brief Map of registered expert models with their unique IDs
    std::unordered_map<std::string, std::unique_ptr<alyssa_experts::IExpert>> experts;
    
    /// @brief Conversation history for each expert
    std::unordered_map<std::string, std::vector<llama_chat_message>> expert_histories;
    
    std::string active_expert_in_cache;    ///< Currently active expert in KV cache
    bool initialized;                      ///< System initialization status

public:
    /**
     * @brief Default constructor.
     * @details Initializes the CoreIntegration system with default values.
     */
    CoreIntegration();
    
    /**
     * @brief Destructor.
     * @details Cleans up all allocated resources and expert histories.
     */
    ~CoreIntegration();

    /**
     * @brief Set the user's name for personalized interactions.
     * @param name The user's name to be stored in memory.
     */
    void set_user_name(const std::string& name);
    
    std::string user_name = ""; ///< Current user's name for personalization
    
    /**
     * @brief Initialize the complete Alyssa AI system.
     * @param base_model_path Path to the base model configuration file.
     * @return true if initialization succeeded, false otherwise.
     * @details Loads models, initializes experts, memory system, and fusion engine.
     */
    bool initialize(const std::string& base_model_path);
    
    // =========================================================================
    // Main Processing Methods
    // =========================================================================
    
    /**
     * @brief Process user input with weighted fusion and TTS.
     * @param input User's text input.
     * @param tts TTS backend (ElevenLabs, Kokoro, ...) for voice synthesis.
     * @return Fused response from multiple experts.
     */
    std::string think_with_fusion(const std::string& input, ITTS& tts);

    /**
     * @brief Process user input with weighted fusion without TTS.
     * @param input User's text input.
     * @return Fused response from multiple experts.
     */
    std::string think_with_fusion_ttsless(const std::string& input);

    /**
     * @brief Generate a spontaneous message without user input (Phase 2.2).
     *
     * Lightweight path for the ProactivityEngine: skips the MoE committee
     * (it's small talk initiated by Alyssa, not a response to analyze) and
     * skips LTM storage (no memory pollution with synthetic prompts).
     * Personality and hormonal context are still injected, and tool calls
     * in the output are resolved.
     *
     * @param reason PT-BR instruction seed from ProactivityEngine's trigger.
     * @return Short proactive message, or empty string when not initialized.
     */
    std::string generate_proactive_message(const std::string& reason);

    /**
     * @brief One-line snapshot of "now" injected into every Alyssa prompt.
     * @details Awareness passiva: relógio local, janela em foco, presença
     *          (snapshot do stream — nunca abre a webcam aqui) e CPU/RAM.
     *          Fonte indisponível fica de fora da linha em vez de falhar.
     *          Público também para o alyssad expor no `status` do protocolo.
     *          Não é thread-safe contra si mesma (reader de CPU com estado):
     *          chame apenas com o cérebro ocioso ou de dentro de um turno.
     * @return Linha "[AMBIENTE] ..." terminada em \n, pronta pra concatenar.
     */
    std::string build_ambient_context();

    /**
     * @brief Sink externo de streaming da resposta (evento `token` do alyssad).
     * @details Recebe cada pedaço gerado pelo expert "alyssa" (fast path,
     *          fusão e follow-ups de tool call — nunca o comitê). Independe
     *          de TTS: os dois sinks coexistem no mesmo callback interno.
     *          Chamado da thread de inferência; o consumidor cuida da própria
     *          sincronização. nullptr (default) = desligado.
     */
    std::function<void(const std::string&)> on_response_chunk;

    // =========================================================================
    // Expert Management Methods
    // =========================================================================
    
    /**
     * @brief Register a new expert model with the system.
     * @param expert Unique pointer to the expert implementation.
     */
    void register_expert(std::unique_ptr<alyssa_experts::IExpert> expert);
    
    /**
     * @brief Remove an expert from the system.
     * @param expert_id Unique identifier of the expert to remove.
     */
    void remove_expert(const std::string& expert_id);
    
    /**
     * @brief Check if an expert is registered.
     * @param expert_id Unique identifier of the expert.
     * @return true if expert exists, false otherwise.
     */
    bool has_expert(const std::string& expert_id) const;
    
    /**
     * @brief Validate if input fits within expert's context window.
     * @param prompt Input text to validate.
     * @param expert_id Expert identifier.
     * @return true if context size is sufficient, false otherwise.
     */
    bool validate_context_size(const std::string& prompt, const std::string& expert_id);
    
    /**
     * @brief Detect emotional content in input using heuristics.
     * @param input Text to analyze for emotional content.
     * @return Detected emotion as string (e.g., "neutralidade", "curiosidade").
     */
    std::string detect_emotion_with_heuristics(const std::string& input);

    /**
     * @brief Calculate coherence metric for expert committee responses.
     * @param contributions Vector of contributions from different experts.
     * @return Coherence score between 0.0 (incoherent) and 1.0 (fully coherent).
     */
    float calculate_committee_coherence(const std::vector<alyssa_fusion::ExpertContribution>& contributions);
    
    /**
     * @brief Determine if interaction should be stored in long-term memory.
     * @param input User's input text.
     * @param response System's response text.
     * @return true if worth storing in memory, false for small talk/noise.
     */
    bool should_store_in_memory(const std::string& input, const std::string& response);

    // =========================================================================
    // Utility Methods
    // =========================================================================
    
    /**
     * @brief Generate fused input for Alyssa model from expert contributions.
     * @param original_input Original user input.
     * @param contributions Vector of expert contributions.
     * @param emotion Detected emotion for context.
     * @return Formatted prompt with expert thoughts and memory context.
     */
    std::string generate_fused_input(
        const std::string& original_input,
        const std::vector<alyssa_fusion::ExpertContribution>& contributions,
        const std::string& emotion
    );
    
    /**
     * @brief Log source awareness information.
     * @param source Source identifier (expert ID).
     * @param message Message to log.
     */
    void log_source_awareness(const std::string& source, const std::string& message);

    /**
     * @brief Presence detector getter (CLI proactivity thread uses it).
     * @return Pointer, or nullptr when webcam/cascade unavailable.
     */
    alyssa_vision::PresenceDetector* get_presence_detector() {
        return presence_detector.get();
    }

    /**
     * @brief Current personality state line for UI display (Phase 5.3).
     * @return Ex: "energia alta, de bom humor, respondona, madrugada".
     */
    std::string get_current_personality_state() const {
        std::string state = alyssa_personality::derive_current_state(
            personality,
            endocrine_system ? &endocrine_system->get_hormone_profile() : nullptr);
        std::string period = alyssa_personality::period_of_day(
            alyssa_personality::current_local_hour());
        return state.empty() ? period : state + ", " + period;
    }

    /**
     * @brief Reload personality.json when its mtime changed (hot-reload).
     * @details Called at the start of each turn — tuning de personalidade
     *          sem reiniciar (e sem recarregar modelo nenhum).
     */
    void maybe_reload_personality();

    /**
     * @brief Tool executor getter (for UI inspection of the call log).
     * @return Pointer to ToolExecutor (nullptr if not initialized).
     */
    alyssa_tools::ToolExecutor* get_tool_executor() {
        return tool_executor.get();
    }

    /**
     * @brief Endocrine Getter
     */
    alyssa_endocrine::EndocrineSystem* get_endocrine_system() {
        return endocrine_system.get(); // Ou apenas o nome do seu membro interno
    }

    /**
     * @brief Runs one gameplayModel inference tick (MinecraftSession).
     * @param world_state_prompt Distilled world-state text (position, health,
     *        inventory, nearby blocks/entities — see MinecraftSession::build_prompt).
     * @return Raw grammar-constrained action signal, e.g.
     *         "[AÇÃO] mover 10 64 -3 [CONFIANÇA] 0.8 [CONTEXTO] indo minerar".
     * @details Thin public wrapper: run_expert() itself stays private so chat
     *          experts can't be invoked from outside CoreIntegration's own flow.
     *
     *          run_expert() only clears the KV cache when active_expert_in_cache
     *          changes (see its "Trocar contexto" block) — calling the SAME
     *          expert every tick never trips that check, so the cache grows by
     *          the full cumulative prompt on every call until llama_decode()
     *          fails once n_ctx is exceeded. Each tick's action doesn't need
     *          cross-tick KV continuity anyway, so force the clear every time.
     *
     *          IMPORTANT: run_expert() builds the prompt from
     *          expert_histories[expert_id] (this map), NOT from the IExpert
     *          object's own internal history member — IExpert::clear_history()
     *          clears that other, unused vector. Clearing the wrong one here
     *          previously let the real history grow forever until the prompt
     *          itself exceeded n_batch and crashed with
     *          "GGML_ASSERT(n_tokens_all <= cparams.n_batch) failed".
     *
     *          gameplayModel now runs on its own dedicated core (E2B), not
     *          the shared 1B core_instance — clear_kv_cache() only clears the
     *          latter, so it's a no-op for gameplayModel's actual KV cache.
     *          clear_own_kv_cache() (IExpert.hpp) targets whichever core the
     *          expert really uses; kept the clear_kv_cache() call too since
     *          it also resets active_expert_in_cache tracking for the
     *          shared-core experts, harmless here either way.
     */
    std::string run_gameplay_tick(const std::string& world_state_prompt) {
        clear_kv_cache();
        if (auto it = experts.find("gameplayModel"); it != experts.end()) {
            it->second->clear_own_kv_cache();
        }
        free_chat_history(expert_histories["gameplayModel"]);
        return run_expert("gameplayModel", world_state_prompt);
    }

    /**
     * @brief Tick de gameplay com VOZ: o prompt contém "<__media__>" que vira
     *        o enunciado do jogador (16kHz float). ASR+decisão numa inferência.
     * @details Caminho do plano B (docs/plano-router-e-voz-gameplay.md): monta
     *          o prompt no turn format do Gemma 4 na mão (system + user com
     *          marcador), grammar do gameplay idem tick normal. mmproj carrega
     *          lazy no primeiro uso (~557MB). Retorna "" se áudio indisponível
     *          (chamador cai no tick de texto).
     */
    std::string run_gameplay_tick_audio(const std::string& world_state_prompt_with_marker,
                                        const std::vector<float>& audio_16k);

    /**
     * @brief Carrega o gameplayModel (E2B, ~3GB) se ainda não estiver na VRAM.
     * @return true se o expert está registrado ao retornar.
     * @details O load é LAZY: initialize() só guarda a config. Chamado pelo
     *          MinecraftSession::start() — na maioria das sessões o Minecraft
     *          nunca liga e os 3GB ficam livres pro scheduler. Mexe no mapa
     *          de experts: chame com o cérebro ocioso (segurando brain_mtx),
     *          nunca durante um think_with_fusion*.
     */
    bool ensure_gameplay_expert();

    /**
     * @brief Footprint em VRAM do modelo base (LLMResident, scheduler de VRAM).
     * @return Bytes dos pesos do modelo 1B compartilhado, ou 0 se não inicializado.
     * @details Cobre só os pesos do modelo base — o 4B vive dentro do expert
     *          "alyssa" e o KV cache não entra na conta. Os números completos
     *          (contexto, 4B, Whisper) estão em plano_scheduler/BASELINE.md;
     *          se precisar do total real, some a partir de lá.
     */
    std::size_t llm_vram_footprint_bytes() {
        if (!core_instance) return 0;
        llama_model* m = core_instance->get_model();
        return m ? static_cast<std::size_t>(llama_model_size(m)) : 0;
    }

    /**
     * @brief Clear all caches including KV cache and expert histories.
     */
    void clear_all_cache() {
        clear_kv_cache();
        for (auto& pair : expert_histories) {
            free_chat_history(pair.second);
        }
        std::cout << "[Orquestrador] Todos os caches limpos." << std::endl;
    }

    // Vision pipeline
    void start_vision_pipeline();
    void stop_vision_pipeline();
    /// true = VisionManager está com a câmera (presença NÃO deve religar o stream)
    bool vision_pipeline_running() const;
    const alyssa_vision::VisionSnapshot& get_latest_vision() const;

    void set_vision_callback(std::function<void(const alyssa_vision::VisionSnapshot&)> cb);



private:
    // =========================================================================
    // Core Fusion Implementation
    // =========================================================================

    /**
     * @brief Shared processing core for all think_with_fusion variants.
     *
     * Handles: endocrine tick, memory augmentation, rule-based Top-K gating,
     * conditional expert execution, coherence check, fused prompt generation,
     * Alyssa inference, memory storage, and KV cache cleanup.
     *
     * @param input   Raw user input.
     * @param use_tts Whether to stream tokens to TTS.
     * @param tts     Pointer to TTS instance (nullptr when use_tts=false).
     * @return Final response string.
     */
    std::string think_with_fusion_core(
        const std::string& input,
        bool use_tts,
        ITTS* tts
    );

    // =========================================================================
    // Tool System (Phase 1)
    // =========================================================================

    /**
     * @brief Register tool handlers that need heavy deps (OpenCV, CURL, metrics).
     * @details Generic filesystem/time handlers come from
     *          ToolExecutor::register_default_handlers().
     */
    void register_builtin_tools();

    /**
     * @brief Resolve [TOOL_CALL] blocks emitted by Alyssa.
     *
     * Parses tool calls from the response, executes them, feeds the results
     * back to the model, and repeats up to max_rounds (registry setting) to
     * avoid infinite loops. Any leftover tool-call block is stripped from the
     * final text.
     *
     * @param response Raw Alyssa output (may contain tool calls).
     * @param use_tts  Whether follow-up inference streams to TTS.
     * @param tts      TTS instance (nullptr when use_tts=false).
     * @return Final response with tool results incorporated.
     */
    std::string resolve_tool_calls(std::string response, bool use_tts, ITTS* tts);

    // =========================================================================
    // Memory Compression & Resilience (Phase 4)
    // =========================================================================

    /**
     * @brief Summarize an archived history chunk with the base 1B model (Phase 4.2).
     * @param text Concatenated messages (and previous rolling summary, if any).
     * @return Short summary, or empty string on failure (compression then
     *         degrades to the old drop-only behavior).
     */
    std::string summarize_history_chunk(const std::string& text);

    /**
     * @brief Last-resort response using the base 1B model (Phase 4.3).
     * @details Called when Alyssa's 4B answer comes back empty/failed.
     * @param prompt The already-fused prompt.
     * @return Response from the base model, or canned apology on total failure.
     */
    std::string generate_fallback_response(const std::string& prompt);

    /**
     * @brief Append one interaction to training_data.jsonl (LoRA groundwork).
     * @details Cada linha: {timestamp, input, response, mode, emotional_state}.
     *          O dataset cresce sozinho a cada conversa — quando chegar a hora
     *          de fine-tunar o Gemma, a matéria-prima já existe.
     */
    void log_interaction_for_dataset(const std::string& input,
                                     const std::string& response,
                                     const std::string& mode);

    // =========================================================================
    // Expert Execution Methods
    // =========================================================================

    /**
     * @brief Execute a specific expert with input processing.
     * @param expert_id Identifier of expert to run.
     * @param input Text input for the expert.
     * @param use_tts Enable TTS synthesis for streaming.
     * @param tts Pointer to TTS instance (can be nullptr).
     * @return Expert's response as string.
     */
    /**
     * @param history_user_text Se não-nulo, substitui o `input` na entrada
     *        de histórico do usuário deste turno. O prompt completo
     *        (personalidade+pensamentos+tools+ambiente) é andaime do turno:
     *        arquivado inteiro, re-templata a cada turno seguinte até o
     *        prompt estourar o n_ctx — e vaza instrução nas respostas.
     *        Aqui vai só o que o usuário disse de fato.
     */
    std::string run_expert(
        const std::string& expert_id,
        const std::string& input,
        bool use_tts = false,
        ITTS* tts = nullptr,
        const std::string* history_user_text = nullptr
    );

    /**
     * @brief Check if two expert signals are compatible.
     * @param signal1 First expert signal.
     * @param signal2 Second expert signal.
     * @return true if signals are compatible, false if contradictory.
     */
    bool are_signals_compatible(const std::string& signal1, const std::string& signal2);

    /**
     * @brief Determine if input is small talk/social pleasantry.
     * @param input Text to analyze.
     * @return true if input is small talk, false if substantive content.
     */
    bool is_small_talk(const std::string& input);
    
    
    // =========================================================================
    // History and Cache Management
    // =========================================================================
    
    /**
     * @brief Manage expert history with dynamic size limits.
     * @param expert_id Expert identifier.
     * @param history Reference to expert's conversation history.
     * @details Archives old messages to LTM when history exceeds limit.
     */
    void manage_dynamic_history(
        const std::string& expert_id,
        std::vector<llama_chat_message>& history
    );
    
    /**
     * @brief Calculate dynamic history limit based on expert and emotional state.
     * @param expert_id Expert identifier.
     * @return Maximum number of messages to keep in history.
     */
    size_t calculate_history_limit(const std::string& expert_id);
    
    /**
     * @brief Clear KV cache for current expert.
     */
    void clear_kv_cache();
    
    /**
     * @brief Switch active expert context in KV cache.
     * @param new_expert_id Expert to switch to.
     */
    void switch_expert_context(const std::string& new_expert_id);
    
    // =========================================================================
    // Reflection and Action Methods
    // =========================================================================
    
    /**
     * @brief Execute an action command.
     * @param command Action command to execute.
     */
    void act(const std::string& command);
    
    /**
     * @brief Perform system reflection/self-analysis.
     */
    void reflect();
    
    /**
     * @brief Get reference to the EndocrineSystem for hormonal state access.
     * @return Pointer to EndocrineSystem (nullptr if not initialized)
     */
    alyssa_endocrine::EndocrineSystem* get_endocrine_system() const {
        return endocrine_system.get();
    }

    std::unique_ptr<alyssa_vision::VisionManager> vision_manager_;
    alyssa_vision::VisionSnapshot latest_vision_;
    mutable std::mutex vision_mtx_;
    void on_vision_snapshot(const alyssa_vision::VisionSnapshot& snap);

    std::function<void(const alyssa_vision::VisionSnapshot&)> on_vision_callback_;

public:
    /**
     * @brief Run interactive command-line interface.
     * @details Continuously processes user input until exit command.
     */
    void run_interactive_loop();
};
