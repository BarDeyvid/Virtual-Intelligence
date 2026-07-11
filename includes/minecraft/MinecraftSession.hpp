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

    // "esperar" is the shortest, least-committal valid completion in the
    // grammar — observed the model collapsing into picking it every single
    // tick regardless of prompt content, a known degenerate-attractor
    // failure mode. Static "don't repeat yourself" wording in the
    // role_instruction is easy for a small model to ignore; escalating
    // pressure directly in the prompt after a few consecutive picks is a
    // more reliable forcing function. Reset to 0 whenever any other verb
    // is chosen.
    int consecutive_esperar_count = 0;
};

} // namespace alyssa_minecraft
