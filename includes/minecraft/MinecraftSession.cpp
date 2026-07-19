// MinecraftSession.cpp
#include "minecraft/MinecraftSession.hpp"

#include <sstream>
#include <fstream>
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

    // gameplayModel é lazy (3GB de E2B só quando o Minecraft realmente liga).
    // brain_mtx segura o load contra um turno de chat em andamento.
    {
        std::lock_guard<std::mutex> brain_lock(brain_mtx);
        if (!core.ensure_gameplay_expert()) {
            std::cerr << "[MinecraftSession] gameplayModel indisponível "
                         "(config ausente ou load falhou)." << std::endl;
            return false;
        }
    }

    if (!bridge.connect(bridge_host, bridge_port)) {
        std::cerr << "[MinecraftSession] Não foi possível conectar ao sidecar em "
                  << bridge_host << ":" << bridge_port
                  << " (rode `node index.js` dentro de minecraft-bridge/ primeiro)." << std::endl;
        return false;
    }
    bridge_host_ = bridge_host;
    bridge_port_ = bridge_port;

    load_goals(); // modo goal-driven (config/gameplay_goals.json), se existir

    running = true;
    worker = std::thread(&MinecraftSession::tick_loop, this, tick_interval_ms);
    std::cout << "[MinecraftSession] Loop iniciado (tick a cada " << tick_interval_ms << "ms)." << std::endl;
    GameplayLog::instance().log("session_start",
        {{"host", bridge_host}, {"port", bridge_port}, {"tick_interval_ms", tick_interval_ms},
         {"config", read_gameplay_config_snapshot()}});
    return true;
}

nlohmann::json MinecraftSession::read_gameplay_config_snapshot() {
    // Re-reads ConfigsLLM.json instead of asking CoreIntegration: the loaded
    // SimpleModelConfig isn't exposed, and for A/B-comparing runs what
    // matters is exactly what the config file said when the session started
    // (temperatura/top_p/grammar on-off are THE knobs the analysis is meant
    // to compare across runs — see the telemetry plan discussed 2026-07-12).
    nlohmann::json snapshot = nlohmann::json::object();
    std::ifstream f("config/ConfigsLLM.json");
    if (!f.is_open()) return snapshot;
    try {
        nlohmann::json j = nlohmann::json::parse(f);
        for (const auto& m : j.value("models", nlohmann::json::array())) {
            if (m.value("id", "") != "gameplayModel") continue;
            snapshot["model_path"] = m.value("model_path", "");
            auto params = m.value("parametros", nlohmann::json::object());
            snapshot["temperatura"] = params.value("temperatura", 0.0);
            snapshot["top_p"] = params.value("top_p", 0.0);
            snapshot["max_tokens"] = params.value("max_tokens", 0);
            snapshot["grammar"] = params.contains("grammar_file");
            break;
        }
    } catch (const std::exception&) {
        // Config unreadable = empty snapshot; never worth failing start() over.
    }
    return snapshot;
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

    // Subsolo profundo = beco sem saída pra maioria dos objetivos (sem
    // madeira, sem comida, sem mesa) e o modelo não infere isso sozinho:
    // na run de 14h de 2026-07-17 ela ficou ~13h numa caverna moendo
    // falhas de mineração porque nada no prompt dizia "sai daí". A dica só
    // aparece no subsolo (~10 tokens) e aponta pro verbo 'subir'.
    if (pos_y < 50) {
        out << "[SITUAÇÃO] Você está no subsolo (y=" << pos_y
            << "). Madeira, comida e mesa de trabalho só existem na superfície — "
               "se o objetivo precisar delas, use a ação 'subir'.\n";
    }

    // Dia/noite decide se mob hostil é ameaça plausível; o sidecar sempre
    // mandou time_of_day/is_raining, só nunca chegava ao modelo. Uma linha
    // curta (~6 tokens) — cabe no orçamento de prefill do tick.
    long tod = static_cast<long>(world_state.value("time_of_day", 0.0));
    const char* phase = (tod < 12000) ? "dia"
                      : (tod < 13000) ? "anoitecendo"
                      : (tod < 23000) ? "noite (mobs hostis aparecem)"
                                       : "amanhecendo";
    out << "Hora do jogo: " << phase;
    if (world_state.value("is_raining", false)) out << " · chovendo";
    out << "\n";

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

void MinecraftSession::set_pending_voice(std::vector<float> audio_16k) {
    std::lock_guard<std::mutex> lk(voice_mtx);
    pending_voice = std::move(audio_16k);
}

void MinecraftSession::load_goals() {
    goals.clear();
    goal_idx = 0;
    std::ifstream f("config/gameplay_goals.json");
    if (!f.is_open()) return;
    try {
        nlohmann::json j = nlohmann::json::parse(f);
        if (!j.value("enabled", false)) return;
        for (const auto& g : j.value("goals", nlohmann::json::array())) {
            GameplayGoal goal;
            goal.id           = g.value("id", "?");
            goal.objetivo     = g.value("objetivo", "");
            goal.done_item    = g.value("done_item", "");
            goal.done_count   = g.value("done_count", 1);
            goal.suffix_match = (g.value("match", "exact") == "suffix");
            if (!goal.objetivo.empty() && !goal.done_item.empty()) goals.push_back(goal);
        }
    } catch (const std::exception& e) {
        std::cerr << "[Goals] gameplay_goals.json inválido: " << e.what() << std::endl;
        goals.clear();
    }
    if (!goals.empty()) {
        std::cout << "[Goals] Modo goal-driven: " << goals.size()
                  << " objetivos carregados." << std::endl;
    }
}

int MinecraftSession::count_in_inventory(const nlohmann::json& world_state,
                                         const GameplayGoal& g) {
    int total = 0;
    for (const auto& item : world_state.value("inventory", nlohmann::json::array())) {
        const std::string name = item.value("name", "");
        const bool match = g.suffix_match
            ? (name.size() >= g.done_item.size() &&
               name.compare(name.size() - g.done_item.size(),
                            g.done_item.size(), g.done_item) == 0)
            : (name == g.done_item);
        if (match) total += item.value("count", 0);
    }
    return total;
}

void MinecraftSession::advance_goals(const nlohmann::json& world_state) {
    // Loop (não if): no boot, objetivos já satisfeitos pelo inventário atual
    // são pulados em cascata — retomar uma sessão não repete o tutorial.
    while (goal_idx < goals.size() &&
           count_in_inventory(world_state, goals[goal_idx]) >= goals[goal_idx].done_count) {
        const auto& done = goals[goal_idx];
        std::cout << "[Goals] Objetivo concluído: " << done.id << std::endl;
        GameplayLog::instance().log("goal_done", {{"id", done.id}});
        goal_idx++;

        std::string msg = (goal_idx < goals.size())
            ? "Consegui: " + done.id + "! Próximo objetivo: " + goals[goal_idx].id
            : "Terminei todos os meus objetivos! Tô jogando livre agora.";
        bridge.send_action("falar", {msg});
    }
}

void MinecraftSession::note_action_outcome(const nlohmann::json& result) {
    // Signature = verb + resolved args (e.g. "colocar -5 72 183 crafting_table")
    // — ActionExecutor echoes these back precisely so a retry against the
    // exact same target is recognizable without re-parsing the raw signal.
    std::ostringstream sig;
    sig << result.value("verb", "?");
    for (const auto& arg : result.value("args", nlohmann::json::array())) {
        if (arg.is_string()) sig << ' ' << arg.get<std::string>();
    }
    std::string signature = sig.str();

    const bool ok = result.value("ok", false);
    std::string message = result.value("message", "");
    if (message.size() > 120) message = message.substr(0, 117) + "...";
    last_action_note = signature + (ok ? " → OK" : " → FALHOU: " + message);

    if (ok) {
        consecutive_failure_count = 0;
        last_failure_signature.clear();
        return;
    }

    // Recusa de ação banida: já é a consequência do loop, não evidência
    // nova de falha — contá-la re-armaria o ban indefinidamente.
    if (result.value("banned", false)) return;

    if (signature == last_failure_signature) {
        ++consecutive_failure_count;
    } else {
        consecutive_failure_count = 1;
        last_failure_signature = signature;
    }
    last_failure_reason = result.value("message", "");

    // Escalação determinística: [AVISO] no prompt (>=2 falhas) provou não
    // bastar — na run noturna o 1B repetiu o mesmo alvo 21x. Na 3ª falha
    // idêntica a assinatura fica proibida por 10 ticks no ActionExecutor.
    if (consecutive_failure_count >= 3) {
        ban_signature = signature;
        ban_ticks_left = 10;
        GameplayLog::instance().log("action_ban_set",
            {{"signature", signature}, {"ticks", ban_ticks_left}});
    }
}

void MinecraftSession::tick_loop(int tick_interval_ms) {
    while (running) {
        auto tick_start = std::chrono::steady_clock::now();

        // A single bad tick (bad JSON, a regex edge case, an inference error)
        // must not take down the whole app via std::terminate() — this thread
        // has no exception handler above it, and neither does main().
        GameplayLog::instance().set_tick(++tick_counter);
        try {
            // MinecraftBridge marks itself disconnected both on a real dead
            // sidecar AND on a merely-slow action that outran its read
            // timeout (see MinecraftBridge::read_line's doc comment) — either
            // way, nothing else ever retries the TCP connect, so without this
            // the session goes dark (empty world state) for good after one
            // hiccup. Retried once per tick; a refused/unreachable connect()
            // fails fast, so this doesn't stall the loop.
            bool reconnect_failed = false;
            if (!bridge.is_connected()) {
                if (!reconnect_notified) {
                    std::cerr << "[MinecraftSession] Sidecar desconectado; tentando reconectar em "
                              << bridge_host_ << ":" << bridge_port_
                              << " (a cada tick, silenciosamente)..." << std::endl;
                    reconnect_notified = true;
                }
                if (bridge.connect(bridge_host_, bridge_port_)) {
                    std::cout << "[MinecraftSession] Reconectado ao sidecar." << std::endl;
                    GameplayLog::instance().log("bridge_reconnected");
                    reconnect_notified = false;
                } else {
                    reconnect_failed = true;
                }
            }

            nlohmann::json world_state = bridge.get_world_state();

            if (world_state.empty()) {
                // ONE entry per idle tick, nothing else. An outage used to
                // log alternating tick_start/world_state lines — a period-2
                // pattern the repeat-collapser can't touch, since it only
                // folds IMMEDIATELY consecutive identical lines (170 idle
                // ticks = 340 log lines observed live 2026-07-12). A single
                // byte-identical line per idle tick collapses into one
                // "repeated: N" marker covering the whole outage.
                GameplayLog::instance().log("tick_idle",
                    {{"reconnect_failed", reconnect_failed}});
            } else {
                // Goal recorded per tick so the analyzer can attribute every
                // action/latency/failure to the goal that was active at the
                // time (per-goal timing needs this, goal_done alone doesn't
                // say when the attempt started).
                GameplayLog::instance().log("tick_start",
                    goal_idx < goals.size()
                        ? nlohmann::json{{"goal", goals[goal_idx].id}}
                        : nlohmann::json::object());
                GameplayLog::instance().log("world_state", {{"data", world_state}});

                advance_goals(world_state); // checa/comemora/avança objetivos

                std::unique_lock<std::mutex> brain_lock(brain_mtx, std::try_to_lock);
                if (brain_lock.owns_lock()) {
                    LabelMap labels;
                    EntityLabelMap entity_names;
                    std::string prompt = build_prompt(world_state, labels, entity_names);

                    // Objetivo atual em TODO tick: é o que dá direção ao
                    // comportamento (e mata o "cavar terra por cavar").
                    if (goal_idx < goals.size()) {
                        prompt += "[OBJETIVO ATUAL] " + goals[goal_idx].objetivo + "\n";
                    }

                    // Feedback da última ação em TODO tick — ver o doc do
                    // membro last_action_note no header (sem isso o modelo
                    // nunca fica sabendo que a ação anterior falhou nem porquê).
                    if (!last_action_note.empty()) {
                        prompt += "[ÚLTIMA AÇÃO] " + last_action_note + "\n";
                    }

                    if (active_directive_ticks_left > 0) {
                        prompt += "[INSTRUÇÃO DO JOGADOR] " + active_directive + "\n";
                        if (--active_directive_ticks_left == 0) {
                            active_directive.clear();
                        }
                    }

                    // Reflexo do sidecar (plano C1): o estrategista precisa
                    // saber que o "corpo" já reagiu sozinho, senão decide às
                    // cegas por cima da fuga/ataque automático.
                    if (!pending_reflex_note.empty()) {
                        prompt += "[REFLEXO] Reação automática recente: " +
                                  pending_reflex_note + "\n";
                        pending_reflex_note.clear();
                    }

                    // Escalating pressure after repeated "esperar" — see the
                    // member's doc comment in the header for why a static
                    // instruction alone isn't reliable enough here.
                    if (consecutive_esperar_count >= 2) {
                        prompt += "[AVISO] Você escolheu esperar " +
                            std::to_string(consecutive_esperar_count) +
                            " vezes seguidas. Escolha uma ação real agora (mover, minerar, atacar) — não esperar de novo.\n";
                    }

                    // "Obsessive spirit" watcher — same idea as the esperar
                    // escalation above, generalized to any action: the exact
                    // same verb+target failing for the exact same reason
                    // repeatedly (e.g. "colocar" against a spot that's
                    // occupied, "craftar" the wrong wood species) is a
                    // pattern a human reading the log spots instantly but
                    // she has no memory of her own last failure to react to.
                    if (consecutive_failure_count >= 2) {
                        prompt += "[AVISO] Você tentou '" + last_failure_signature + "' " +
                            std::to_string(consecutive_failure_count) +
                            " vezes seguidas e sempre falhou pelo mesmo motivo (" + last_failure_reason +
                            "). Pare de repetir exatamente a mesma coisa — tente um alvo ou uma ação diferente.\n";
                    }
                    // Voz do jogador (plano B4): consome o slot e tenta o
                    // caminho de áudio; falha (sem mmproj etc.) degrada pro
                    // tick de texto normal sem a linha de voz.
                    std::vector<float> voice;
                    {
                        std::lock_guard<std::mutex> vlk(voice_mtx);
                        voice.swap(pending_voice);
                    }

                    std::string signal;
                    if (!voice.empty()) {
                        // Instrução dominante: sem isso o modelo ouvia e
                        // seguia cavando terra como se nada tivesse sido dito.
                        std::string voiced_prompt = prompt +
                            "[VOZ DO JOGADOR] O jogador acabou de FALAR com você pelo microfone "
                            "(áudio anexo). Se for um comando de jogo, a sua ação DEVE obedecê-lo "
                            "AGORA — ignore qualquer outro plano. Se não for comando, responda "
                            "com 'falar'. <__media__>\n";
                        GameplayLog::instance().log("prompt",
                            {{"text", voiced_prompt}, {"voice_samples", voice.size()}});
                        signal = core.run_gameplay_tick_audio(voiced_prompt, voice);
                        if (signal.empty()) {
                            std::cerr << "[MinecraftSession] Caminho de voz indisponível; "
                                         "tick segue sem áudio." << std::endl;
                        } else {
                            // No console: é o feedback que diz se ela OBEDECEU
                            // (o GameplayLog guarda, mas ninguém olha ao vivo).
                            std::cout << "[Gaia ouviu] " << signal << std::endl;
                        }
                    }
                    if (signal.empty()) {
                        GameplayLog::instance().log("prompt", {{"text", prompt}});
                        signal = core.run_gameplay_tick(prompt);
                    }
                    GameplayLog::instance().log("signal", {{"text", signal}});

                    if (signal.find("[AÇÃO] esperar") != std::string::npos) {
                        ++consecutive_esperar_count;
                    } else {
                        consecutive_esperar_count = 0;
                    }

                    note_action_outcome(action_executor.execute(signal, labels, entity_names,
                        ban_ticks_left > 0 ? ban_signature : std::string()));
                    if (ban_ticks_left > 0 && --ban_ticks_left == 0) {
                        ban_signature.clear();
                    }
                } else {
                    std::cout << "[MinecraftSession] cérebro ocupado (chat em andamento); "
                                 "pulando ação deste tick." << std::endl;
                    GameplayLog::instance().log("tick_skipped", {{"reason", "brain_busy"}});
                }
            }

            // Drena eventos (dano/morte/chat/reflexo) mesmo em ticks sem ação.
            auto events = action_executor.process_pending_events();
            handle_chat_events(events);
            for (const auto& ev : events) {
                if (ev.value("event", "") == "reflex") {
                    auto d = ev.value("data", nlohmann::json::object());
                    pending_reflex_note =
                        d.value("action", "?") + " — " + d.value("reason", "");
                }
            }
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
            // Explicitly NOT grounded in whether the action succeeded: this
            // reply comes from the chat persona (think_with_fusion_ttsless),
            // which has no visibility into gameplayModel's actual execution —
            // the directive below only takes effect over the NEXT few ticks.
            // Without this caveat, she confidently claims things she hasn't
            // done yet ("Feito! Mesa no chão!" while still gathering wood
            // ticks later) — observed live 2026-07-12, reads as her lying.
            std::string grounded_message = "[Minecraft] " + username +
                " disse no chat do jogo, enquanto jogávamos juntos: \"" + message + "\". "
                "Isso é só um pedido que você está repassando pro seu 'corpo' no jogo "
                "(gameplayModel) tentar nos próximos segundos — você NÃO sabe ainda se vai "
                "dar certo. Responda reconhecendo o pedido (tipo 'beleza, vou tentar' ou "
                "'deixa comigo'). NUNCA diga que já fez, já terminou ou já colocou algo — "
                "você não tem como confirmar isso agora.";

            std::string reply;
            {
                std::lock_guard<std::mutex> brain_lock(brain_mtx);
                reply = core.think_with_fusion_ttsless(grounded_message);
            }
            if (reply.size() > kMaxChatReplyLength) {
                reply.resize(kMaxChatReplyLength - 3);
                // substr/resize cuts by byte count, not by character — Alyssa's
                // replies often end in a multi-byte UTF-8 emoji, and slicing
                // one in half left a dangling partial codepoint that made
                // GameplayLog's json.dump() throw ("invalid UTF-8 byte") live
                // on 2026-07-12, silently dropping that reply to the player.
                // Trim back any orphaned trailing bytes: continuation bytes
                // (10xxxxxx) first, then a lead byte (11xxxxxx) left with none
                // of its continuations.
                auto is_continuation = [](unsigned char c) { return (c & 0xC0) == 0x80; };
                auto is_lead = [](unsigned char c) { return (c & 0xC0) == 0xC0; };
                while (!reply.empty() && is_continuation(static_cast<unsigned char>(reply.back()))) {
                    reply.pop_back();
                }
                if (!reply.empty() && is_lead(static_cast<unsigned char>(reply.back()))) {
                    reply.pop_back();
                }
                reply += "...";
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
