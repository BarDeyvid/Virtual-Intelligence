// MinecraftSession.hpp
#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include "minecraft/MinecraftBridge.hpp"
#include "minecraft/ActionExecutor.hpp"

// AlyssaNet.hpp has no include guard and is meant to be included exactly once
// per translation unit (every existing .cpp includes it directly) — pulling
// it in transitively here causes a "class redefinition" in any .cpp that
// also includes AlyssaNet.hpp itself. Forward-declare instead; the .cpp
// includes it since it needs the full definition to call run_gameplay_tick().
class CoreIntegration;

namespace alyssa_minecraft {

/**
 * @class MinecraftSession
 * @brief Ties CoreIntegration, MinecraftBridge and ActionExecutor into a
 *        running tick loop: read world state -> ask gameplayModel for one
 *        action -> execute it -> feed the outcome back into the endocrine
 *        system -> repeat.
 *
 * Mirrors the ProactivityEngine's threading convention in Alyssa_CLI.cpp:
 * CoreIntegration is not thread-safe, so every tick takes the caller-owned
 * brain_mtx with try_lock and simply skips acting (but still drains sidecar
 * events) if the brain is busy with a chat turn. A slow Minecraft tick must
 * never stall chat, and a chat turn must never stall Minecraft indefinitely.
 */
class MinecraftSession {
public:
    /// brain_mtx must outlive this session — pass the same mutex the CLI
    /// already wraps every other CoreIntegration call in (g_state.brain_mtx).
    MinecraftSession(CoreIntegration& core, std::mutex& brain_mtx);
    ~MinecraftSession();

    MinecraftSession(const MinecraftSession&) = delete;
    MinecraftSession& operator=(const MinecraftSession&) = delete;

    /// Connects to the minecraft-bridge sidecar and starts the tick thread.
    /// Returns false without starting anything if the sidecar isn't reachable
    /// (e.g. `node index.js` isn't running yet).
    bool start(const std::string& bridge_host = "127.0.0.1",
               int bridge_port = 8765,
               int tick_interval_ms = 3000);

    /// Stops the tick thread and disconnects from the sidecar. Safe to call
    /// even if start() was never called or already failed.
    void stop();

    bool is_running() const { return running; }

    /**
     * @brief Enunciado de voz do jogador pro PRÓXIMO tick (16kHz mono float).
     * @details Slot de 1: comando falado é perecível — um novo substitui o
     *          antigo em vez de enfileirar. O tick anexa
     *          "[VOZ DO JOGADOR] <__media__>" ao prompt e roda o caminho de
     *          áudio (run_gameplay_tick_audio). Thread-safe (voice_mtx);
     *          chamado do on_segment do VoicePipeline (modo vad_only).
     */
    void set_pending_voice(std::vector<float> audio_16k);

private:
    void tick_loop(int tick_interval_ms);

    /// Builds the world-state prompt text and, alongside it, a label ("B1",
    /// "E1"...) -> coordinate map for every nearby block/entity mentioned —
    /// gameplayModel targets those labels instead of raw coordinates (see
    /// gameplay_action.gbnf) — plus an entity label -> name map ("atacar"
    /// targets by name, not coordinate). ActionExecutor::execute() resolves
    /// both back.
    std::string build_prompt(const nlohmann::json& world_state, LabelMap& out_labels,
                              EntityLabelMap& out_entity_names) const;

    /// Forwards any "chat" events (in-game messages from other players, never
    /// the bot's own — the sidecar already filters those) into Alyssa's real
    /// conversational brain and speaks the reply back via the "falar" action.
    /// Uses a blocking lock, not try_lock: skipping a gameplay tick is a
    /// no-op, but silently dropping a chat reply is not.
    void handle_chat_events(const std::vector<nlohmann::json>& events);

    CoreIntegration& core;
    std::mutex& brain_mtx;
    MinecraftBridge bridge;
    ActionExecutor action_executor;
    std::atomic<bool> running{false};
    std::thread worker;

    // Remembered from start() so tick_loop can reconnect after the bridge
    // marks itself disconnected (dead sidecar OR just a slow action that
    // outran MinecraftBridge's read timeout — see read_line's doc comment).
    // Without this, a single hiccup went unrecovered for the rest of the
    // session: every following tick silently got an empty world state
    // forever, with nothing ever calling connect() again.
    std::string bridge_host_;
    int bridge_port_ = 8765;

    // Monotonic tick id, stamped into every GameplayLog entry of the tick
    // (GameplayLog::set_tick) so tools/analyze_gameplay.js can correlate
    // prompt/signal/result lines without timestamp guessing.
    long long tick_counter = 0;

    // Console-spam gate: print the "sidecar desconectado, reconectando"
    // message only on the connected->disconnected transition, not on every
    // 3s retry of a long outage.
    bool reconnect_notified = false;

    // Alyssa (chat) and gameplayModel (the autonomous actor) otherwise share
    // no state — a chat request like "go for a walk" would only ever reach
    // the chat brain, which has no idea it's embodied in Minecraft at all.
    // handle_chat_events() sets these; tick_loop() folds the directive into
    // gameplayModel's prompt for a few ticks so it can actually act on it,
    // then lets it expire back to normal autonomous behavior. Both are only
    // ever touched from the single worker thread (tick_loop calls
    // handle_chat_events synchronously), so no locking needed here.
    std::string active_directive;
    int active_directive_ticks_left = 0;

    // Voz do jogador (plano B4): escrito pela thread do VAD via
    // set_pending_voice, consumido (swap) pelo tick_loop.
    std::mutex voice_mtx;
    std::vector<float> pending_voice;

    // Última reação do reflexo do sidecar (plano C1) — vira "[REFLEXO] ..."
    // no prompt do tick seguinte. Só a thread do worker toca.
    std::string pending_reflex_note;

    // ---- Modo goal-driven (config/gameplay_goals.json) ----
    // Progressão de aprendizado: um objetivo por vez no prompt, conclusão
    // checada pelo inventário a cada tick. Só a thread do worker toca.
    struct GameplayGoal {
        std::string id;
        std::string objetivo;
        std::string done_item;
        int done_count = 1;
        bool suffix_match = false;
    };
    std::vector<GameplayGoal> goals;
    size_t goal_idx = 0;

    /// Carrega config/gameplay_goals.json (ausente/enabled:false = sem goals).
    void load_goals();

    /// Lê de ConfigsLLM.json os hiperparâmetros do gameplayModel (temperatura,
    /// top_p, grammar on/off...) pro registro session_start — é o que permite
    /// comparar runs A/B por configuração no tools/analyze_gameplay.js.
    static nlohmann::json read_gameplay_config_snapshot();

    /// Conta itens do inventário que casam com o objetivo (exact/suffix).
    static int count_in_inventory(const nlohmann::json& world_state, const GameplayGoal& g);

    /// Avança objetivos satisfeitos (comemora via 'falar' + loga goal_done).
    void advance_goals(const nlohmann::json& world_state);

    // "esperar" is the shortest, least-committal valid completion in the
    // grammar — observed the model collapsing into picking it every single
    // tick regardless of prompt content, a known degenerate-attractor
    // failure mode. Static "don't repeat yourself" wording in the
    // role_instruction is easy for a small model to ignore; escalating
    // pressure directly in the prompt after a few consecutive picks is a
    // more reliable forcing function. Reset to 0 whenever any other verb
    // is chosen.
    int consecutive_esperar_count = 0;

    // "Obsessive spirit" watcher (per user request 2026-07-12): generalizes
    // the esperar-escalation idea above to ANY action — if the exact same
    // verb+target fails for the exact same reason across consecutive
    // attempts, nudge her to stop blindly retrying instead of waiting for a
    // human to notice the log and point it out (which is how every "colocar
    // against an occupied spot" / "craftar wrong wood species" loop got
    // caught so far). Reset on success or as soon as she tries something
    // different. Only the worker thread touches this.
    std::string last_failure_signature;
    std::string last_failure_reason;
    int consecutive_failure_count = 0;

    // Feedback direto: o resultado da última ação entra no prompt de TODO
    // tick ("[ÚLTIMA AÇÃO] minerar ... → FALHOU: ..."). Sem isso o modelo
    // decide completamente às cegas — na run noturna de 2026-07-16 ele
    // repetiu a mesma mineração impossível 2283 vezes sem nunca "saber" que
    // tinha falhado (o [AVISO] só aparecia depois de 2+ repetições, e sem o
    // porquê da falha).
    std::string last_action_note;

    // Ban determinístico (escalação além do [AVISO]): 3 falhas idênticas
    // seguidas => a assinatura exata fica proibida por N ticks (o
    // ActionExecutor recusa sem chamar o sidecar). Prompt sozinho provou
    // não bastar pro 1B sair do loop (391 loops na run noturna).
    std::string ban_signature;
    int ban_ticks_left = 0;

    /// Feeds one action_executor.execute() result into the repeat-failure
    /// tracker above.
    void note_action_outcome(const nlohmann::json& result);
};

} // namespace alyssa_minecraft
