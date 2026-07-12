// ActionExecutor.cpp
#include "minecraft/ActionExecutor.hpp"
#include "minecraft/GameplayLog.hpp"

#include <regex>
#include <sstream>
#include <iostream>
#include <cstdio>

namespace alyssa_minecraft {

using json = nlohmann::json;

namespace {

std::vector<std::string> split_args(const std::string& args_str) {
    std::vector<std::string> args;
    std::istringstream stream(args_str);
    std::string token;
    while (stream >> token) {
        args.push_back(token);
    }
    return args;
}

// The GBNF grammar bounds every field's length now, so a legitimate signal
// is always short — but this is defense in depth, not trust in the model:
// regex-matching an unbounded string is a real risk (a stuck/misbehaving
// generation could hand us kilobytes of garbage), so cap it hard before the
// regex ever sees it rather than relying solely on grammar-side guarantees.
constexpr size_t kMaxSignalLength = 1000;

// Replaces any C0/DEL control byte with a visible \xNN escape so a raw ESC
// (observed once in model output) can never be printed straight into a
// terminal/log and be interpreted as part of an escape sequence.
std::string sanitize_for_display(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        if (c < 0x20 || c == 0x7F) {
            char buf[5];
            std::snprintf(buf, sizeof(buf), "\\x%02X", c);
            out += buf;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

// Verbs whose first argument is a label (per gameplay_action.gbnf's "label"
// rule) rather than a literal value, and how many trailing args follow the
// label unchanged (colocar's block name).
struct LabeledVerb { const char* verb; size_t trailing_args; };
constexpr LabeledVerb kLabeledVerbs[] = {
    {"mover", 0},
    {"minerar", 0},
    {"colocar", 1},
};

std::vector<std::string> to_string_vec(const std::array<int, 3>& coord) {
    return {std::to_string(coord[0]), std::to_string(coord[1]), std::to_string(coord[2])};
}

} // namespace

ActionExecutor::ActionExecutor(MinecraftBridge& bridge_ref, alyssa_endocrine::EndocrineSystem& endocrine_ref)
    : bridge(bridge_ref), endocrine(endocrine_ref) {}

json ActionExecutor::execute(const std::string& gameplay_signal_raw, const LabelMap& labels,
                              const EntityLabelMap& entity_names) {
    static const std::regex pattern(
        R"(\[AÇÃO\]\s*(\w+)\s*(.*?)\s*\[CONFIANÇA\]\s*(\d+\.?\d*)\s*\[CONTEXTO\]\s*(.+))");

    const std::string gameplay_signal = gameplay_signal_raw.size() > kMaxSignalLength
        ? gameplay_signal_raw.substr(0, kMaxSignalLength)
        : gameplay_signal_raw;

    std::smatch matches;
    if (!std::regex_search(gameplay_signal, matches, pattern) || matches.size() < 5) {
        std::cerr << "[ActionExecutor] Sinal fora do formato esperado, ignorando: "
                  << sanitize_for_display(gameplay_signal) << std::endl;
        GameplayLog::instance().log("signal_malformed", {{"raw", gameplay_signal}});
        return json{{"ok", false}, {"message", "signal did not match the gameplay grammar"}};
    }

    const std::string verb = matches[1];
    std::vector<std::string> args = split_args(matches[2]);
    const std::string confidence = matches[3];
    const std::string context = matches[4];

    // mover/minerar/colocar carry a label (B1, E2...) in args[0] instead of
    // raw coordinates — resolve it to real integer coordinates here, before
    // anything reaches the sidecar (which still only understands numbers).
    for (const auto& labeled : kLabeledVerbs) {
        if (verb != labeled.verb) continue;
        if (args.empty()) break;

        auto it = labels.find(args[0]);
        if (it == labels.end()) {
            std::cerr << "[ActionExecutor] Label desconhecido '" << sanitize_for_display(args[0])
                      << "' para ação '" << verb << "', ignorando." << std::endl;
            GameplayLog::instance().log("unknown_label", {{"verb", verb}, {"label", args[0]}});
            return json{{"ok", false}, {"message", "unknown label: " + args[0]}};
        }

        std::vector<std::string> resolved = to_string_vec(it->second);
        for (size_t i = 1; i <= labeled.trailing_args && i < args.size(); ++i) {
            resolved.push_back(args[i]);
        }
        args = std::move(resolved);
        break;
    }

    // "atacar" targets by name, not coordinate — resolve an entity label
    // (E1...) to the name bot.attack() actually needs. Falls back to the raw
    // arg unchanged if it's not a known label, so a model that writes the
    // name directly (e.g. "atacar rabbit") still works exactly as before.
    if (verb == "atacar" && !args.empty()) {
        if (auto it = entity_names.find(args[0]); it != entity_names.end()) {
            args[0] = it->second;
        }
    }

    GameplayLog::instance().log("action_parsed",
        {{"verb", verb}, {"args", args}, {"confidence", confidence}, {"context", context}});

    json result = bridge.send_action(verb, args);
    const bool ok = result.value("ok", false);
    GameplayLog::instance().log("action_result", result);

    // Small, per-action endocrine nudge — not meant to dominate the profile,
    // just to make repeated success/failure feel like something over time.
    if (ok) {
        endocrine.trigger_reward_response(0.15);
    } else {
        endocrine.trigger_stress_response(0.1);
        std::cerr << "[ActionExecutor] Ação '" << sanitize_for_display(verb) << "' falhou ("
                  << sanitize_for_display(context) << "): "
                  << result.value("message", "") << std::endl;
    }

    return result;
}

std::vector<json> ActionExecutor::process_pending_events() {
    auto events = bridge.drain_events();
    for (const auto& event : events) {
        const std::string kind = event.value("event", "");
        const json data = event.value("data", json::object());
        GameplayLog::instance().log("sidecar_event", {{"kind", kind}, {"data", data}});

        if (kind == "death") {
            endocrine.trigger_stress_response(0.9);
        } else if (kind == "damage") {
            double amount = data.value("amount", 0.0);
            endocrine.trigger_stress_response(std::min(1.0, amount / 20.0));
        } else if (kind == "chat") {
            endocrine.trigger_social_response(0.3);
        } else if (kind == "reflex") {
            // Reflexo disparou = ameaça/urgência real — mas o loop de reflexo
            // roda a 250ms e um combate dispara VÁRIOS eventos por segundo;
            // sem rate-limit o cortisol crava em ~0.96 em segundos (visto ao
            // vivo em 2026-07-12). Um nudge a cada 5s no máximo.
            static auto last_reflex_nudge = std::chrono::steady_clock::time_point{};
            auto now = std::chrono::steady_clock::now();
            if (now - last_reflex_nudge > std::chrono::seconds(5)) {
                last_reflex_nudge = now;
                endocrine.trigger_stress_response(0.15);
            }
        }
    }
    return events;
}

} // namespace alyssa_minecraft
