// MinecraftSession.cpp
#include "minecraft/MinecraftSession.hpp"

#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include "AlyssaNet.hpp"
#include "minecraft/GameplayLog.hpp"

namespace alyssa_minecraft {

namespace {

alyssa_endocrine::EndocrineSystem& require_endocrine(CoreIntegration& core) {
    auto* endocrine = core.get_endocrine_system();
    if (!endocrine) {
        throw std::runtime_error(
            "MinecraftSession: CoreIntegration::initialize() precisa rodar antes "
            "(EndocrineSystem ainda não existe).");
    }
    return *endocrine;
}

} // namespace

MinecraftSession::MinecraftSession(CoreIntegration& core_ref, std::mutex& brain_mtx_ref)
    : core(core_ref),
      brain_mtx(brain_mtx_ref),
      bridge(),
      action_executor(bridge, require_endocrine(core_ref)) {}

MinecraftSession::~MinecraftSession() {
    stop();
}

bool MinecraftSession::start(const std::string& bridge_host, int bridge_port, int tick_interval_ms) {
    if (running) return true;

    if (!bridge.connect(bridge_host, bridge_port)) {
        std::cerr << "[MinecraftSession] Não foi possível conectar ao sidecar em "
                  << bridge_host << ":" << bridge_port
                  << " (rode `node index.js` dentro de minecraft-bridge/ primeiro)." << std::endl;
        return false;
    }

    running = true;
    worker = std::thread(&MinecraftSession::tick_loop, this, tick_interval_ms);
    std::cout << "[MinecraftSession] Loop iniciado (tick a cada " << tick_interval_ms << "ms)." << std::endl;
    GameplayLog::instance().log("session_start",
        {{"host", bridge_host}, {"port", bridge_port}, {"tick_interval_ms", tick_interval_ms}});
    return true;
}

void MinecraftSession::stop() {
    if (!running) return;
    GameplayLog::instance().log("session_stop");
    running = false;
    if (worker.joinable()) {
        worker.join();
    }
    bridge.disconnect();
    std::cout << "[MinecraftSession] Loop parado." << std::endl;
}

std::string MinecraftSession::build_prompt(const nlohmann::json& world_state, LabelMap& out_labels,
                                            EntityLabelMap& out_entity_names) const {
    out_labels.clear();
    out_entity_names.clear();
    std::ostringstream out;
    out << "[ESTADO DO JOGO]\n";

    // Rounded to integers deliberately: the raw float position (many decimal
    // digits) is what the model was seen mashing into fake coordinates
    // (position "-9.9278" -> invented target digits "99278"). Integers are
    // both what the grammar accepts and much easier for a small model to
    // read cleanly.
    auto pos = world_state.value("position", nlohmann::json::object());
    int pos_x = static_cast<int>(std::lround(pos.value("x", 0.0)));
    int pos_y = static_cast<int>(std::lround(pos.value("y", 0.0)));
    int pos_z = static_cast<int>(std::lround(pos.value("z", 0.0)));
    out << "Posição: (" << pos_x << ", " << pos_y << ", " << pos_z << ")\n";
    out << "Vida: " << world_state.value("health", 0.0) << "/20  Fome: "
        << world_state.value("food", 0.0) << "/20\n";

    out << "Inventário: ";
    auto inventory = world_state.value("inventory", nlohmann::json::array());
    if (inventory.empty()) {
        out << "vazio";
    } else {
        bool first = true;
        for (const auto& item : inventory) {
            if (!first) out << ", ";
            out << item.value("count", 0) << "x " << item.value("name", "?");
            first = false;
        }
    }
    out << "\n";

    // Blocks/entities get a short label (B1, E1...) instead of the model
    // having to copy/compute raw X/Y/Z — see gameplay_action.gbnf for why.
    out << "Blocos próximos (use o rótulo para mover/minerar/colocar): ";
    auto blocks = world_state.value("nearby_blocks", nlohmann::json::array());
    size_t block_count = 0;
    for (const auto& block : blocks) {
        // Kept small deliberately: clear_kv_cache() runs every tick (see
        // run_gameplay_tick's docs), so this whole prompt gets reprocessed
        // from scratch every ~few seconds — fewer prefill tokens here is a
        // direct, real latency win, not just cosmetic.
        if (block_count >= 5) break;
        if (block_count > 0) out << ", ";
        std::string label = "B" + std::to_string(block_count + 1);
        int bx = block.value("x", 0), by = block.value("y", 0), bz = block.value("z", 0);
        out << label << "=" << block.value("name", "?") << "(" << bx << "," << by << "," << bz << ")";
        out_labels[label] = {bx, by, bz};
        block_count++;
    }
    if (block_count == 0) out << "nenhum visível";
    out << "\n";

    out << "Entidades próximas (use o rótulo para mover/atacar): ";
    auto entities = world_state.value("nearby_entities", nlohmann::json::array());
    size_t entity_count = 0;
    for (const auto& entity : entities) {
        if (entity_count >= 3) break;
        if (entity_count > 0) out << ", ";
        std::string label = "E" + std::to_string(entity_count + 1);
        int ex = entity.value("x", 0), ey = entity.value("y", 0), ez = entity.value("z", 0);
        out << label << "=" << entity.value("name", "?") << " a "
            << std::fixed << std::setprecision(1) << entity.value("distance", 0.0)
            << " blocos (" << ex << "," << ey << "," << ez << ")";
        out_labels[label] = {ex, ey, ez};
        out_entity_names[label] = entity.value("name", "?");
        entity_count++;
    }
    if (entity_count == 0) out << "nenhuma";
    out << "\n";

    return out.str();
}

void MinecraftSession::tick_loop(int tick_interval_ms) {
    while (running) {
        auto tick_start = std::chrono::steady_clock::now();

        // A single bad tick (bad JSON, a regex edge case, an inference error)
        // must not take down the whole app via std::terminate() — this thread
        // has no exception handler above it, and neither does main().
        GameplayLog::instance().log("tick_start");
        try {
            nlohmann::json world_state = bridge.get_world_state();
            GameplayLog::instance().log("world_state", {{"data", world_state}});

            if (!world_state.empty()) {
                std::unique_lock<std::mutex> brain_lock(brain_mtx, std::try_to_lock);
                if (brain_lock.owns_lock()) {
                    LabelMap labels;
                    EntityLabelMap entity_names;
                    std::string prompt = build_prompt(world_state, labels, entity_names);

                    if (active_directive_ticks_left > 0) {
                        prompt += "[INSTRUÇÃO DO JOGADOR] " + active_directive + "\n";
                        if (--active_directive_ticks_left == 0) {
                            active_directive.clear();
                        }
                    }

                    // Escalating pressure after repeated "esperar" — see the
                    // member's doc comment in the header for why a static
                    // instruction alone isn't reliable enough here.
                    if (consecutive_esperar_count >= 2) {
                        prompt += "[AVISO] Você escolheu esperar " +
                            std::to_string(consecutive_esperar_count) +
                            " vezes seguidas. Escolha uma ação real agora (mover, minerar, atacar) — não esperar de novo.\n";
                    }
                    GameplayLog::instance().log("prompt", {{"text", prompt}});

                    std::string signal = core.run_gameplay_tick(prompt);
                    GameplayLog::instance().log("signal", {{"text", signal}});

                    if (signal.find("[AÇÃO] esperar") != std::string::npos) {
                        ++consecutive_esperar_count;
                    } else {
                        consecutive_esperar_count = 0;
                    }

                    action_executor.execute(signal, labels, entity_names);
                } else {
                    std::cout << "[MinecraftSession] cérebro ocupado (chat em andamento); "
                                 "pulando ação deste tick." << std::endl;
                    GameplayLog::instance().log("tick_skipped", {{"reason", "brain_busy"}});
                }
            }

            // Drena eventos (dano/morte/chat) mesmo em ticks sem ação nova.
            auto events = action_executor.process_pending_events();
            handle_chat_events(events);
        } catch (const std::exception& e) {
            std::cerr << "[MinecraftSession] Tick falhou, seguindo para o próximo: "
                      << e.what() << std::endl;
            GameplayLog::instance().log("tick_error", {{"message", e.what()}});
        }

        auto elapsed = std::chrono::steady_clock::now() - tick_start;
        auto remaining = std::chrono::milliseconds(tick_interval_ms) - elapsed;
        if (remaining.count() > 0) {
            std::this_thread::sleep_for(remaining);
        }
    }
}

namespace {
// Vanilla Minecraft caps chat messages around 256 chars; leave headroom for
// the "<Alyssa> " prefix the server adds.
constexpr size_t kMaxChatReplyLength = 240;
}

void MinecraftSession::handle_chat_events(const std::vector<nlohmann::json>& events) {
    for (const auto& event : events) {
        if (event.value("event", "") != "chat") continue;

        auto data = event.value("data", nlohmann::json::object());
        std::string username = data.value("username", "");
        std::string message = data.value("message", "");
        if (message.empty()) continue;

        // Also hand it to gameplayModel as a standing directive for the next
        // few ticks — Alyssa's chat reply alone can't make the bot actually
        // do anything, and gameplayModel doesn't otherwise see chat at all.
        // Left for the (1B, not very discerning) model to judge relevance;
        // it's told in its role_instruction to ignore it if it doesn't apply.
        active_directive = message;
        active_directive_ticks_left = 5;

        try {
            // CoreIntegration::is_small_talk() (AlyssaNet.cpp:999) routes any
            // message under 30 chars with no question word through a fast
            // path that skips the whole expert committee and asks for a
            // generic, context-free reply — Minecraft chat is short and
            // casual by nature, so nearly everything sent in-game would trip
            // that shortcut and get the same bland "hey, what's up" regardless
            // of content (confirmed: "me bate ai" got the exact same fast-path
            // reply shape as "ei"). Wrapping with real context both pushes the
            // combined string past that length threshold — routing through
            // the full committee instead — and gives Alyssa actual grounding
            // that this is an in-game message from a named player, rather
            // than an isolated one-liner.
            std::string grounded_message = "[Minecraft] " + username +
                " disse no chat do jogo, enquanto jogávamos juntos: \"" + message + "\"";

            std::string reply;
            {
                std::lock_guard<std::mutex> brain_lock(brain_mtx);
                reply = core.think_with_fusion_ttsless(grounded_message);
            }
            if (reply.size() > kMaxChatReplyLength) {
                reply = reply.substr(0, kMaxChatReplyLength - 3) + "...";
            }
            GameplayLog::instance().log("chat_reply", {{"from", username}, {"message", message}, {"reply", reply}});
            bridge.send_action("falar", {reply});
        } catch (const std::exception& e) {
            std::cerr << "[MinecraftSession] Falha ao responder chat de " << username
                      << ": " << e.what() << std::endl;
            GameplayLog::instance().log("chat_reply_error", {{"from", username}, {"message", e.what()}});
        }
    }
}

} // namespace alyssa_minecraft
